/*
 * Engine worker: owns the WASM module and runs all engine work off the UI
 * thread. It speaks the message protocol in engine-protocol.ts and runs in both
 * a browser Web Worker and Node's worker_threads (used by the headless test).
 *
 * Native graph simulations are run in bounded chunks. Between chunks the
 * worker yields to the event loop so queued cancel/progress messages are
 * delivered, keeping the UI responsive and cancellation prompt.
 */

import { createEngineBindings, EngineBindings } from "./engine-wasm";
import {
    Catalog,
    CatalogEntry,
    ClientMessage,
    CraftAction,
    EngineError,
    SimulationOptions,
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
    return { groupKeyById, groupNameById, essences, fossils };
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

const yieldToEventLoop = (): Promise<void> =>
    new Promise((resolve) => setTimeout(resolve, 0));

async function runStrategy(
    id: number,
    params: Record<string, unknown>,
): Promise<StrategyResult> {
    const simulator = params.simulator as number;
    const options = params.options as SimulationOptions;
    const chunkSize = (params.chunkSize as number) || DEFAULT_CHUNK_SIZE;

    if (cancelled.has(id)) {
        const progress = bindings.runSimulatorChunk(simulator, options, 0);
        return {
            cancelled: true,
            progress,
            ...bindings.simulatorResult(simulator),
        };
    }
    let progress = bindings.runSimulatorChunk(simulator, options, chunkSize);
    post({
        kind: "progress",
        id,
        done: progress.completed_runs,
        total: progress.target_runs,
    });
    while (!progress.finished) {
        // Yield so queued cancel messages are processed before the next chunk.
        await yieldToEventLoop();
        if (cancelled.has(id)) {
            return {
                cancelled: true,
                progress,
                ...bindings.simulatorResult(simulator),
            };
        }
        progress = bindings.runSimulatorChunk(simulator, options, chunkSize);
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
    bindings = await createEngineBindings();
    const isBrowserWorker =
        typeof (globalThis as { WorkerGlobalScope?: unknown }).WorkerGlobalScope !==
        "undefined";
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
