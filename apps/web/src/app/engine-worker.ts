/*
 * Engine worker: owns the WASM module and runs all engine work off the UI
 * thread. It speaks the message protocol in engine-protocol.ts and runs in both
 * a browser Web Worker and Node's worker_threads (used by the headless test).
 *
 * Native graph simulations are run in bounded chunks. Between chunks the
 * worker yields to the event loop so queued cancel/progress messages are
 * delivered, keeping the UI responsive and cancellation prompt.
 */

import type { EngineBindings } from "./engine-wasm";
import {
    Catalog,
    CatalogEntry,
    ClientMessage,
    CraftAction,
    EngineError,
    SimulationOptions,
    SimulationProgress,
    SolverGoal,
    StrategyEvalOptions,
    StrategyResult,
    WorkerMessage,
} from "./engine-protocol";

const DEFAULT_CHUNK_SIZE = 1000;

let bindings: EngineBindings;
let post: (message: WorkerMessage) => void = () => {};
const cancelled = new Set<number>();

// Raw bundle bytes retained until a catalog is distilled from them, then the
// compact catalog is cached and the bytes are dropped to reclaim memory.
const dataBundles = new Map<number, Uint8Array>();
const catalogCache = new Map<number, Catalog>();

interface BundleShape {
    strings: { strings: string[]; string_id_base?: number };
    game_data: {
        groups: { key_string_ids: number[]; display_name_string_ids: number[] };
        essences: {
            key_string_ids: number[];
            name_string_ids: number[];
            is_corruption_only?: number[];
        };
        fossils: { key_string_ids: number[]; name_string_ids: number[] };
        mods: {
            global_mod_ids: number[];
            key_string_ids: number[];
            text_line_offsets: number[];
            text_line_string_ids: number[];
        };
        bench_options: { global_mod_ids: number[] };
        tags: { global_tag_ids: number[]; name_string_ids: number[] };
    };
}

/**
 * Distil the UI-authoring catalog from the compiled data bundle. Pure JS over
 * the same JSON the engine loaded — no WASM call, so it works without rebuilding
 * the engine. Mod groups are returned indexed by group id (matching the
 * `primary_group_id` mods report); essences and fossils are filtered to named,
 * craftable rows and sorted for display.
 */
function buildCatalog(bundle: Uint8Array): Catalog {
    const json = JSON.parse(new TextDecoder().decode(bundle)) as BundleShape;
    const strings = json.strings.strings;
    const base = json.strings.string_id_base ?? 0;
    const s = (id: number): string => strings[id - base] ?? "";
    const g = json.game_data;

    const groupKeyById = g.groups.key_string_ids.map(s);
    const groupNameById = g.groups.display_name_string_ids.map(s);

    const essences: CatalogEntry[] = [];
    for (let i = 0; i < g.essences.key_string_ids.length; i += 1) {
        if (g.essences.is_corruption_only?.[i]) continue;
        const name = s(g.essences.name_string_ids[i]);
        if (!name) continue;
        essences.push({ key: s(g.essences.key_string_ids[i]), name });
    }
    const fossils: CatalogEntry[] = [];
    for (let i = 0; i < g.fossils.key_string_ids.length; i += 1) {
        const name = s(g.fossils.name_string_ids[i]);
        if (!name) continue;
        fossils.push({ key: s(g.fossils.key_string_ids[i]), name });
    }
    essences.sort((a, b) => a.name.localeCompare(b.name));
    fossils.sort((a, b) => a.name.localeCompare(b.name));
    const modIndex = new Map(
        g.mods.global_mod_ids.map((id, index) => [id, index]),
    );
    const bench: CatalogEntry[] = [];
    const seenBench = new Set<string>();
    for (const globalId of g.bench_options.global_mod_ids) {
        if (globalId < 0) continue;
        const index = modIndex.get(globalId);
        if (index === undefined) continue;
        const key = s(g.mods.key_string_ids[index]);
        if (!key || seenBench.has(key)) continue;
        seenBench.add(key);
        const lineOffset = g.mods.text_line_offsets[index];
        const lineEnd = g.mods.text_line_offsets[index + 1];
        const name =
            lineEnd > lineOffset
                ? s(g.mods.text_line_string_ids[lineOffset])
                : key;
        bench.push({ key, name: name || key });
    }
    bench.sort((a, b) => a.name.localeCompare(b.name));
    const tagNames = new Set(g.tags.name_string_ids.map(s));
    const harvestTags = [
        "attack",
        "caster",
        "life",
        "defences",
        "physical",
        "fire",
        "cold",
        "lightning",
        "chaos",
        "speed",
        "critical",
    ]
        .filter((key) => tagNames.has(key))
        .map((key) => ({
            key,
            name: key.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase()),
        }));
    const influences = [
        "shaper",
        "elder",
        "crusader",
        "adjudicator",
        "basilisk",
        "eyrie",
    ].map((key) => ({
        key,
        code: {
            adjudicator: 1,
            basilisk: 2,
            crusader: 3,
            elder: 4,
            eyrie: 5,
            shaper: 6,
        }[key],
        name: {
            adjudicator: "Warlord",
            basilisk: "Redeemer",
            eyrie: "Hunter",
        }[key] ?? key.replace(/\b\w/g, (c) => c.toUpperCase()),
    }));
    return {
        groupKeyById,
        groupNameById,
        essences,
        fossils,
        bench,
        harvestTags,
        influences,
    };
}

function catalogFor(data: number): Catalog {
    const cached = catalogCache.get(data);
    if (cached) return cached;
    const bundle = dataBundles.get(data);
    if (!bundle) {
        throw new EngineError(1, "catalog unavailable: data not loaded");
    }
    const catalog = buildCatalog(bundle);
    catalogCache.set(data, catalog);
    dataBundles.delete(data);
    return catalog;
}

const yieldToEventLoop = (() => {
    if (typeof MessageChannel !== "undefined") {
        const channel = new MessageChannel();
        const pending: Array<() => void> = [];
        channel.port1.onmessage = () => {
            pending.shift()?.();
        };
        return (): Promise<void> =>
            new Promise((resolve) => {
                pending.push(resolve);
                channel.port2.postMessage(undefined);
            });
    }
    return (): Promise<void> =>
        new Promise((resolve) => setTimeout(resolve, 0));
})();

const yieldToTimerTask = (): Promise<void> =>
    new Promise((resolve) => setTimeout(resolve, 0));

async function runStrategy(
    id: number,
    params: Record<string, unknown>,
): Promise<StrategyResult> {
    const simulator = params.simulator as number;
    const options = params.options as SimulationOptions;
    let chunkSize = (params.chunkSize as number) || DEFAULT_CHUNK_SIZE;
    let yieldCount = 0;
    const runChunk = (): SimulationProgress => {
        const started = performance.now();
        const progress = bindings.runSimulatorChunk(
            simulator,
            options,
            chunkSize,
        );
        const elapsedMs = Math.max(0.1, performance.now() - started);
        const scale = Math.min(4, Math.max(0.5, 16 / elapsedMs));
        chunkSize = Math.min(
            10_000,
            Math.max(1, Math.round(chunkSize * scale)),
        );
        return progress;
    };

    if (cancelled.has(id)) {
        const progress = bindings.runSimulatorChunk(simulator, options, 0);
        return {
            cancelled: true,
            progress,
            ...bindings.simulatorResult(simulator),
        };
    }
    let progress = runChunk();
    post({
        kind: "progress",
        id,
        done: progress.completed_runs,
        total: progress.target_runs,
    });
    while (!progress.finished) {
        // MessageChannel is the low-latency yield, but some worker runtimes can
        // repeatedly service its private queue before incoming cancel messages.
        // Periodically use a timer task to guarantee external messages a turn.
        ++yieldCount;
        await (yieldCount % 4 === 0
            ? yieldToTimerTask()
            : yieldToEventLoop());
        if (cancelled.has(id)) {
            return {
                cancelled: true,
                progress,
                ...bindings.simulatorResult(simulator),
            };
        }
        progress = runChunk();
        post({
            kind: "progress",
            id,
            done: progress.completed_runs,
            total: progress.target_runs,
        });
    }
    return {
        cancelled: false,
        progress,
        ...bindings.simulatorResult(simulator),
    };
}

async function dispatch(
    id: number,
    method: string,
    params: Record<string, unknown>,
): Promise<unknown> {
    switch (method) {
        case "loadData": {
            const bundle = params.bundle as Uint8Array;
            const data = bindings.loadData(bundle);
            dataBundles.set(data, bundle);
            return { data };
        }
        case "catalog":
            return { catalog: catalogFor(params.data as number) };
        case "dataSummary":
            return bindings.dataSummary(params.data as number);
        case "listBases":
            return { bases: bindings.listBases(params.data as number) };
        case "closeData":
            bindings.closeData(params.data as number);
            dataBundles.delete(params.data as number);
            catalogCache.delete(params.data as number);
            return {};
        case "createSession":
            return {
                session: bindings.createSession(
                    params.data as number,
                    params.base as string,
                    (params.itemLevel as number) ?? 0,
                ),
            };
        case "closeSession":
            bindings.closeSession(params.session as number);
            return {};
        case "modCount":
            return { count: bindings.modCount(params.session as number) };
        case "modInfo":
            return bindings.modInfo(
                params.session as number,
                params.modId as number,
            );
        case "createContext":
            return {
                context: bindings.createContext(
                    params.session as number,
                    (params.seed as number) ?? 0,
                ),
            };
        case "closeContext":
            bindings.closeContext(params.context as number);
            return {};
        case "memoryStats":
            return bindings.memoryStats();
        case "createItem":
            return {
                item: bindings.createItem(
                    params.session as number,
                    (params.rarity as string) ?? "normal",
                    (params.withImplicits as boolean) ?? true,
                ),
            };
        case "cloneItem":
            return { item: bindings.cloneItem(params.item as number) };
        case "closeItem":
            bindings.closeItem(params.item as number);
            return {};
        case "itemInfo":
            return bindings.itemInfo(
                params.item as number,
                (params.session as number) ?? 0,
            );
        case "exportItem":
            return { state: bindings.exportItem(params.item as number) };
        case "importItem":
            return { item: bindings.importItem(params.state) };
        case "addMod":
            bindings.addMod(params.item as number, params.session as number, {
                key: params.key as string,
                side: params.side as string | undefined,
                fractured: params.fractured as boolean | undefined,
            });
            return {};
        case "removeMod":
            bindings.removeMod(params.item as number, {
                modId: params.modId as number,
                side: params.side as "prefix" | "suffix",
            });
            return {};
        case "setModFractured":
            bindings.setModFractured(params.item as number, {
                modId: params.modId as number,
                side: params.side as "prefix" | "suffix",
            });
            return {};
        case "apply":
            return {
                result: bindings.apply(
                    params.context as number,
                    params.item as number,
                    params.action as CraftAction,
                ),
            };
        case "debugPool":
            return bindings.debugPool(
                params.context as number,
                params.item as number,
                {
                    action: params.action as CraftAction,
                    side: params.side as string | undefined,
                    include_rejected: params.includeRejected as boolean | undefined,
                },
            );
        case "compileStrategy":
            return {
                strategy: bindings.compileStrategy(
                    params.session as number,
                    params.strategy,
                ),
            };
        case "closeStrategy":
            bindings.closeStrategy(params.strategy as number);
            return {};
        case "strategyEvaluate": {
            const strategy = bindings.compileStrategy(
                params.session as number,
                params.strategy,
            );
            try {
                return bindings.strategyEvaluate(
                    strategy,
                    params.options as StrategyEvalOptions | undefined,
                );
            } finally {
                bindings.closeStrategy(strategy);
            }
        }
        case "loadEconomy":
            return { economy: bindings.loadEconomy(params.economy) };
        case "closeEconomy":
            bindings.closeEconomy(params.economy as number);
            return {};
        case "createSimulator":
            return {
                simulator: bindings.createSimulator(
                    params.session as number,
                    params.strategy as number,
                    params.economy as number | undefined,
                ),
            };
        case "closeSimulator":
            bindings.closeSimulator(params.simulator as number);
            return {};
        case "runStrategy":
            return runStrategy(id, params);
        case "openSolver":
            return {
                solver: bindings.openSolver(
                    params.session as number,
                    params.goal as SolverGoal,
                ),
            };
        case "closeSolver":
            bindings.closeSolver(params.solver as number);
            return {};
        case "solverActions": {
            let actions = bindings.solverActions(params.solver as number);
            if (params.omitFossilCombos) {
                // Multi-fossil loadout ids are "fossil:<key>+<key>..." with
                // sorted keys (solver_registry.cpp); keep only the singles.
                actions = actions.filter(
                    (action) =>
                        !action.id.startsWith("fossil:") ||
                        !action.id.includes("+"),
                );
            }
            return { actions };
        }
        case "solverCalc":
            return bindings.solverCalc(
                params.solver as number,
                params.item as number,
                params.action as string,
            );
        case "solverSolve":
            return bindings.solverSolve(
                params.solver as number,
                params.item as number,
                params.economy as number,
                params.options as
                    | { epsilon?: number; max_states?: number; max_sweeps?: number }
                    | undefined,
            );
        case "solverStateValue":
            return bindings.solverStateValue(
                params.solver as number,
                params.state as number,
            );
        case "solverProject":
            return {
                state: bindings.solverProject(
                    params.solver as number,
                    params.item as number,
                ),
            };
        case "solverCompileStrategy":
            return {
                strategy: bindings.solverCompileStrategy(
                    params.solver as number,
                ),
            };
        case "solverLog":
            return { log: bindings.solverLog(params.solver as number) };
        default:
            throw new EngineError(1, `unknown method: ${method}`);
    }
}

async function handle(message: ClientMessage): Promise<void> {
    if (message.kind === "cancel") {
        cancelled.add(message.id);
        return;
    }
    if (message.kind !== "request") {
        return;
    }
    const { id, method, params } = message;
    try {
        const result = await dispatch(id, method, params);
        post({ kind: "response", id, ok: true, result });
    } catch (error) {
        const info =
            error instanceof EngineError
                ? { code: error.code, message: error.message }
                : { code: -1, message: String(error) };
        post({ kind: "response", id, ok: false, error: info });
    } finally {
        cancelled.delete(id);
    }
}

async function main(): Promise<void> {
    const isBrowserWorker =
        typeof (globalThis as { postMessage?: unknown }).postMessage ===
            "function" &&
        typeof (globalThis as { document?: unknown }).document === "undefined";
    if (
        isBrowserWorker &&
        typeof (globalThis as { WorkerGlobalScope?: unknown })
            .WorkerGlobalScope === "undefined"
    ) {
        // Some embedded Chromium shells omit the WorkerGlobalScope constructor
        // even though the module is running in a real worker. Emscripten uses
        // this marker to choose its fetch-based worker loader.
        (
            globalThis as { WorkerGlobalScope?: unknown }
        ).WorkerGlobalScope = Object;
    }
    const { createEngineBindings } = await import("./engine-wasm");
    bindings = await createEngineBindings();
    if (isBrowserWorker) {
        const scope = globalThis as unknown as {
            postMessage: (message: unknown) => void;
            onmessage: ((event: { data: ClientMessage }) => void) | null;
        };
        post = (message) => scope.postMessage(message);
        scope.onmessage = (event) => {
            void handle(event.data);
        };
    } else {
        const { parentPort } = await import(
            /* @vite-ignore */ "node:worker_threads"
        );
        if (!parentPort) {
            throw new Error("engine-worker started without a parent port");
        }
        post = (message) => parentPort.postMessage(message);
        parentPort.on("message", (data: ClientMessage) => {
            void handle(data);
        });
    }
    post({ kind: "ready", abiVersion: bindings.abiVersion() });
}

void main();
