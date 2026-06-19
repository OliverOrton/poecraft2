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
        case "loadData":
            return { data: bindings.loadData(params.bundle as Uint8Array) };
        case "dataSummary":
            return bindings.dataSummary(params.data as number);
        case "listBases":
            return { bases: bindings.listBases(params.data as number) };
        case "closeData":
            bindings.closeData(params.data as number);
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
