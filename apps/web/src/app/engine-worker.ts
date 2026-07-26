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
    SolveOptions,
    SolveProgress,
    SolverWorkerMetrics,
    SolverGoal,
    SolverSolveResult,
    StrategyEvalOptions,
    StrategyEvalProgress,
    StrategyResult,
    WorkerMessage,
} from "./engine-protocol";
import { buildHarvestCatalog } from "./harvest-crafts";

const DEFAULT_CHUNK_SIZE = 1000;

let bindings: EngineBindings;
let post: (message: WorkerMessage, transfer?: ArrayBuffer[]) => void = () => {};
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
    const harvestTags = buildHarvestCatalog(tagNames);
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

function solveProgressCounts(
    progress: SolveProgress,
): { done: number; total: number } {
    if (progress.phase === "expanding") {
        return {
            done: progress.expanded_states,
            total: progress.expanded_states + 1,
        };
    }
    if (progress.phase === "iterating") {
        return { done: progress.sweeps, total: progress.sweeps + 1 };
    }
    return { done: 1, total: 1 };
}

async function solveSolver(
    id: number,
    params: Record<string, unknown>,
): Promise<SolverSolveResult> {
    const solver = params.solver as number;
    let begun = false;
    // The adaptive loop already caps later solver steps at four work items.
    // Apply that bound to the first call as well: benchmark/native chunk sizes
    // can be much larger, and forwarding one verbatim would create a single
    // long, non-cancellable WASM slice before adaptation has any measurement.
    let workItems = Math.min(
        4,
        Math.max(1, (params.chunkSize as number) || 8),
    );
    let observedPhase: SolveProgress["phase"] = "expanding";
    let emittedProgress = false;
    let lastProgressAt = -Infinity;
    let yieldCount = 0;
    let unyieldedStepMs = 0;
    const worker: SolverWorkerMetrics = {
        step_count: 0,
        yield_count: 0,
        max_step_ms: 0,
        total_step_ms: 0,
    };
    const reportEveryChunk = params.chunkSize !== undefined;
    let progress: SolveProgress = {
        phase: "expanding",
        done: false,
        expanded_states: 0,
        sweeps: 0,
        residual: 1e12,
        start_value_bound: 1e12,
        lower_bound: null,
        upper_bound: null,
        absolute_optimality_gap: null,
        relative_optimality_gap: null,
        focused_round: 0,
        incumbent_kind: "none",
        discovered_states: 0,
        frontier_states: 0,
        state_action_rows: 0,
        transition_entries: 0,
        reforge_work: 0,
        live_owned_bytes: 0,
        peak_owned_bytes: 0,
    };

    try {
        if (cancelled.has(id)) {
            return { cancelled: true, progress, worker };
        }
        bindings.beginSolverSolve(
            solver,
            params.item as number,
            params.economy as number,
            params.options as SolveOptions | undefined,
        );
        begun = true;

        do {
            if (cancelled.has(id)) {
                return { cancelled: true, progress, worker };
            }
            /* S7.2 splits Bellman sweeps into bounded sparse-row units.
             * Expansion and iteration both adapt toward a 12 ms worker slice,
             * so cancellation is serviced inside a large sweep rather than
             * only between whole-table sweeps. */
            const stepWorkItems = workItems;
            const started = performance.now();
            progress = bindings.stepSolverSolve(solver, stepWorkItems);
            const measuredMs = Math.max(0, performance.now() - started);
            const elapsedMs = Math.max(0.1, measuredMs);
            worker.step_count += 1;
            worker.max_step_ms = Math.max(worker.max_step_ms, measuredMs);
            worker.total_step_ms += measuredMs;
            unyieldedStepMs += measuredMs;
            const phaseChanged = progress.phase !== observedPhase;
            if (phaseChanged) {
                workItems = 1;
            } else {
                const scale = Math.min(2, Math.max(0.5, 12 / elapsedMs));
                workItems = Math.min(
                    4,
                    Math.max(1, Math.round(stepWorkItems * scale)),
                );
            }
            observedPhase = progress.phase;

            const now = performance.now();
            if (
                reportEveryChunk ||
                !emittedProgress ||
                phaseChanged ||
                progress.done ||
                now - lastProgressAt >= 100
            ) {
                const counts = solveProgressCounts(progress);
                post({
                    kind: "progress",
                    id,
                    ...counts,
                    solve: progress,
                });
                emittedProgress = true;
                lastProgressAt = now;
            }

            if (
                !progress.done &&
                (params.yieldEveryStep === true || unyieldedStepMs >= 8)
            ) {
                ++yieldCount;
                worker.yield_count = yieldCount;
                unyieldedStepMs = 0;
                // Give an external AbortSignal a timer-task turn immediately.
                await (yieldCount % 2 === 1
                    ? yieldToTimerTask()
                    : yieldToEventLoop());
            }
        } while (!progress.done);

        if (cancelled.has(id)) {
            return { cancelled: true, progress, worker };
        }
        const summary = bindings.finishSolverSolve(solver);
        begun = false;
        return { ...summary, cancelled: false, progress, worker };
    } finally {
        if (begun) bindings.abandonSolverSolve(solver);
    }
}

function evaluationProgressCounts(
    progress: StrategyEvalProgress,
): { done: number; total: number } {
    if (progress.phase === "discovery") {
        return {
            done: progress.discovered_pairs,
            total: progress.discovered_pairs + progress.pending_pairs,
        };
    }
    if (progress.phase === "solving") {
        return {
            done: progress.solved_sccs,
            total: progress.total_sccs,
        };
    }
    if (progress.phase === "fallback") {
        return {
            done: progress.fallback_sweeps,
            total: progress.fallback_sweeps + 1,
        };
    }
    return { done: progress.done ? 1 : 0, total: 1 };
}

async function evaluateStrategy(
    id: number,
    params: Record<string, unknown>,
): Promise<unknown> {
    let strategy = 0;
    let evaluation = 0;
    let economy = 0;
    let workItems = Math.max(1, (params.chunkSize as number) || 16);
    let yieldCount = 0;
    let lastProgressAt = -Infinity;
    let emittedProgress = false;
    let observedPhase: StrategyEvalProgress["phase"] = "discovery";
    let pendingDiscovery: number | undefined;
    const reportProgress = params.reportProgress === true;
    try {
        if (cancelled.has(id)) {
            throw new EngineError(1, "strategy evaluation cancelled");
        }
        strategy = bindings.compileStrategy(
            params.session as number,
            params.strategyJson as Uint8Array,
        );
        if (params.economy !== undefined) {
            economy = bindings.loadEconomy(params.economy);
        }
        evaluation = bindings.beginStrategyEvaluation(
            strategy,
            params.options as StrategyEvalOptions | undefined,
            economy || undefined,
            params.reviewProjection,
        );
        let progress: StrategyEvalProgress;
        do {
            if (cancelled.has(id)) {
                throw new EngineError(1, "strategy evaluation cancelled");
            }
            // Do not let a discovery-calibrated budget spill across the
            // discovery/solve boundary, and enter each SCC one at a time so
            // an iterative fallback is observable before any sweeps run.
            const stepWorkItems =
                observedPhase === "discovery" && pendingDiscovery !== undefined
                    ? Math.min(workItems, Math.max(1, pendingDiscovery))
                    : observedPhase === "solving"
                      ? 1
                      : workItems;
            const started = performance.now();
            progress = bindings.stepStrategyEvaluation(evaluation, stepWorkItems);
            const elapsedMs = Math.max(0.1, performance.now() - started);
            const phaseChanged = progress.phase !== observedPhase;
            if (phaseChanged) {
                // One fallback work item is already 32 native sweeps. Rebase
                // at every phase boundary, then adapt from that phase's cost.
                workItems = 1;
            } else {
                const scale = Math.min(4, Math.max(0.5, 16 / elapsedMs));
                workItems = Math.min(
                    16_384,
                    Math.max(1, Math.round(stepWorkItems * scale)),
                );
            }
            observedPhase = progress.phase;
            pendingDiscovery = progress.pending_pairs;
            const now = performance.now();
            if (
                reportProgress &&
                (!emittedProgress ||
                    phaseChanged ||
                    progress.done ||
                    now - lastProgressAt >= 100)
            ) {
                const counts = evaluationProgressCounts(progress);
                post({
                    kind: "progress",
                    id,
                    ...counts,
                    evaluation: progress,
                });
                emittedProgress = true;
                lastProgressAt = now;
            }
            if (!progress.done) {
                ++yieldCount;
                // Exact evaluations can finish in only a few chunks. Give
                // external abort messages a timer-task turn immediately, then
                // alternate with the lower-latency MessageChannel yield.
                await (yieldCount % 2 === 1
                    ? yieldToTimerTask()
                    : yieldToEventLoop());
            }
        } while (!progress.done);
        if (cancelled.has(id)) {
            throw new EngineError(1, "strategy evaluation cancelled");
        }
        return bindings.finishStrategyEvaluation(evaluation);
    } finally {
        if (evaluation) bindings.closeStrategyEvaluation(evaluation);
        if (economy) bindings.closeEconomy(economy);
        if (strategy) bindings.closeStrategy(strategy);
    }
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
        case "bestiaryPresentation":
            return bindings.bestiaryPresentation(params.data as number);
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
        case "bestiaryApply":
            return {
                result: bindings.bestiaryApply(
                    params.data as number,
                    params.item as number,
                    params.actionId as string,
                ),
            };
        case "bestiaryCalculate":
            return bindings.bestiaryCalculate(
                params.data as number,
                params.item as number,
                params.actionId as string,
            );
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
                    params.strategyJson as Uint8Array,
                ),
            };
        case "closeStrategy":
            bindings.closeStrategy(params.strategy as number);
            return {};
        case "strategyEvaluate": {
            return evaluateStrategy(id, params);
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
            return solveSolver(id, params);
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
                strategyJson: bindings.solverCompileStrategy(
                    params.solver as number,
                ),
            };
        case "solverLog":
            return { log: bindings.solverLog(params.solver as number) };
        case "solverTelemetry":
            return bindings.solverTelemetry(params.solver as number);
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
        const strategyJson =
            method === "solverCompileStrategy" &&
            result !== null &&
            typeof result === "object"
                ? (result as { strategyJson?: unknown }).strategyJson
                : undefined;
        const transfer =
            strategyJson instanceof Uint8Array
                ? [strategyJson.buffer as ArrayBuffer]
                : undefined;
        post({ kind: "response", id, ok: true, result }, transfer);
    } catch (error) {
        const info =
            error instanceof EngineError
                ? { code: error.code, detail: error.detail }
                : {
                      code: -1,
                      detail:
                          error instanceof Error ? error.message : String(error),
                  };
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
            postMessage: (
                message: unknown,
                transfer?: ArrayBuffer[],
            ) => void;
            onmessage: ((event: { data: ClientMessage }) => void) | null;
        };
        post = (message, transfer) =>
            scope.postMessage(message, transfer ?? []);
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
        post = (message, transfer) =>
            parentPort.postMessage(message, transfer ?? []);
        parentPort.on("message", (data: ClientMessage) => {
            void handle(data);
        });
    }
    post({ kind: "ready", abiVersion: bindings.abiVersion() });
}

void main();
