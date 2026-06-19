/*
 * EngineClient: the small, stable main-thread handle UI components depend on.
 *
 * It is a thin RPC layer over a worker that runs the WASM engine. It exposes
 * coarse-grained, promise-returning methods and references engine objects by
 * opaque integer handles, so UI code never touches WASM memory or the worker
 * protocol directly. Long strategy runs report progress and honour an
 * AbortSignal for cancellation.
 */

import {
    ActionOutcome,
    AffixSide,
    BaseInfo,
    ClientMessage,
    CraftAction,
    EngineError,
    ModInfo,
    PoolDebug,
    SimulationOptions,
    StrategyResult,
    WorkerMessage,
} from "./engine-protocol";

/** Transport abstraction so the same client drives a browser Worker or a
 * Node worker_threads worker (via a small adapter in the test harness). */
export interface EngineTransport {
    postMessage(message: ClientMessage, transfer?: Transferable[]): void;
    onMessage(handler: (message: WorkerMessage) => void): void;
    terminate?(): void;
}

interface Pending {
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
    onProgress?: (progress: { done: number; total: number }) => void;
    abortCleanup?: () => void;
}

export interface StrategyRunOptions {
    chunkSize?: number;
    onProgress?: (progress: { done: number; total: number }) => void;
    signal?: AbortSignal;
}

export class EngineClient {
    private readonly transport: EngineTransport;
    private readonly pending = new Map<number, Pending>();
    private nextId = 1;
    private abiVersion = 0;
    private readonly ready: Promise<void>;
    private resolveReady!: () => void;

    constructor(transport: EngineTransport) {
        this.transport = transport;
        this.ready = new Promise((resolve) => {
            this.resolveReady = resolve;
        });
        this.transport.onMessage((message) => this.onMessage(message));
    }

    /** Create a client backed by a browser Web Worker loaded from this module's
     * sibling engine-worker. Bundlers (Vite) rewrite the worker URL. */
    static spawn(): EngineClient {
        const worker = new Worker(new URL("./engine-worker.ts", import.meta.url), {
            type: "module",
        });
        return new EngineClient({
            postMessage: (message, transfer) =>
                worker.postMessage(message, transfer ?? []),
            onMessage: (handler) => {
                worker.onmessage = (event: MessageEvent) =>
                    handler(event.data as WorkerMessage);
            },
            terminate: () => worker.terminate(),
        });
    }

    /** Resolves once the worker has initialised the WASM module. */
    whenReady(): Promise<void> {
        return this.ready;
    }

    private onMessage(message: WorkerMessage): void {
        if (message.kind === "ready") {
            this.abiVersion = message.abiVersion;
            this.resolveReady();
            return;
        }
        if (message.kind === "progress") {
            this.pending.get(message.id)?.onProgress?.({
                done: message.done,
                total: message.total,
            });
            return;
        }
        const pending = this.pending.get(message.id);
        if (!pending) {
            return;
        }
        this.pending.delete(message.id);
        pending.abortCleanup?.();
        if (message.ok) {
            pending.resolve(message.result);
        } else {
            pending.reject(
                new EngineError(
                    message.error?.code ?? -1,
                    message.error?.message ?? "unknown error",
                ),
            );
        }
    }

    private async call<T>(
        method: string,
        params: Record<string, unknown>,
        options?: {
            transfer?: Transferable[];
            onProgress?: (progress: { done: number; total: number }) => void;
            signal?: AbortSignal;
        },
    ): Promise<T> {
        await this.ready;
        const id = this.nextId++;
        return new Promise<T>((resolve, reject) => {
            const pending: Pending = {
                resolve: resolve as (value: unknown) => void,
                reject,
                onProgress: options?.onProgress,
            };
            this.pending.set(id, pending);
            const signal = options?.signal;
            if (signal) {
                if (signal.aborted) {
                    this.transport.postMessage({ kind: "cancel", id });
                } else {
                    const onAbort = () =>
                        this.transport.postMessage({ kind: "cancel", id });
                    signal.addEventListener("abort", onAbort, { once: true });
                    pending.abortCleanup = () =>
                        signal.removeEventListener("abort", onAbort);
                }
            }
            this.transport.postMessage(
                { kind: "request", id, method, params },
                options?.transfer,
            );
        });
    }

    getAbiVersion(): number {
        return this.abiVersion;
    }

    async loadData(bundle: Uint8Array): Promise<number> {
        const { data } = await this.call<{ data: number }>(
            "loadData",
            { bundle },
            { transfer: [bundle.buffer] },
        );
        return data;
    }

    dataSummary(data: number): Promise<Record<string, unknown>> {
        return this.call("dataSummary", { data });
    }

    closeData(data: number): Promise<void> {
        return this.call<void>("closeData", { data });
    }

    async listBases(data: number): Promise<BaseInfo[]> {
        const { bases } = await this.call<{ bases: BaseInfo[] }>("listBases", {
            data,
        });
        return bases;
    }

    async createSession(
        data: number,
        base: string,
        itemLevel: number,
    ): Promise<number> {
        const { session } = await this.call<{ session: number }>("createSession", {
            data,
            base,
            itemLevel,
        });
        return session;
    }

    closeSession(session: number): Promise<void> {
        return this.call<void>("closeSession", { session });
    }

    async modCount(session: number): Promise<number> {
        const { count } = await this.call<{ count: number }>("modCount", {
            session,
        });
        return count;
    }

    modInfo(session: number, modId: number): Promise<ModInfo> {
        return this.call("modInfo", { session, modId });
    }

    async createContext(session: number, seed = 0): Promise<number> {
        const { context } = await this.call<{ context: number }>("createContext", {
            session,
            seed,
        });
        return context;
    }

    closeContext(context: number): Promise<void> {
        return this.call<void>("closeContext", { context });
    }

    async createItem(
        session: number,
        options?: { rarity?: string; withImplicits?: boolean },
    ): Promise<number> {
        const { item } = await this.call<{ item: number }>("createItem", {
            session,
            rarity: options?.rarity ?? "normal",
            withImplicits: options?.withImplicits ?? true,
        });
        return item;
    }

    async cloneItem(item: number): Promise<number> {
        const result = await this.call<{ item: number }>("cloneItem", { item });
        return result.item;
    }

    closeItem(item: number): Promise<void> {
        return this.call<void>("closeItem", { item });
    }

    itemInfo(item: number, session = 0): Promise<Record<string, unknown>> {
        return this.call("itemInfo", { item, session });
    }

    async exportItem(item: number): Promise<unknown> {
        const { state } = await this.call<{ state: unknown }>("exportItem", {
            item,
        });
        return state;
    }

    async importItem(state: unknown): Promise<number> {
        const { item } = await this.call<{ item: number }>("importItem", {
            state,
        });
        return item;
    }

    addMod(
        item: number,
        session: number,
        spec: { key: string; side?: AffixSide; fractured?: boolean },
    ): Promise<void> {
        return this.call<void>("addMod", { item, session, ...spec });
    }

    async apply(
        context: number,
        item: number,
        action: CraftAction,
    ): Promise<ActionOutcome> {
        const { result } = await this.call<{ result: ActionOutcome }>("apply", {
            context,
            item,
            action,
        });
        return result;
    }

    debugPool(
        context: number,
        item: number,
        query: { action: CraftAction; side?: AffixSide; includeRejected?: boolean },
    ): Promise<PoolDebug> {
        return this.call("debugPool", {
            context,
            item,
            action: query.action,
            side: query.side,
            includeRejected: query.includeRejected,
        });
    }

    async compileStrategy(session: number, strategy: unknown): Promise<number> {
        const result = await this.call<{ strategy: number }>("compileStrategy", {
            session,
            strategy,
        });
        return result.strategy;
    }

    closeStrategy(strategy: number): Promise<void> {
        return this.call<void>("closeStrategy", { strategy });
    }

    async loadEconomy(economy: unknown): Promise<number> {
        const result = await this.call<{ economy: number }>("loadEconomy", {
            economy,
        });
        return result.economy;
    }

    closeEconomy(economy: number): Promise<void> {
        return this.call<void>("closeEconomy", { economy });
    }

    async createSimulator(
        session: number,
        strategy: number,
        economy?: number,
    ): Promise<number> {
        const result = await this.call<{ simulator: number }>(
            "createSimulator",
            { session, strategy, economy },
        );
        return result.simulator;
    }

    closeSimulator(simulator: number): Promise<void> {
        return this.call<void>("closeSimulator", { simulator });
    }

    /** Run the native compiled strategy in bounded chunks. The worker yields
     * between chunks for progress and AbortSignal cancellation. */
    runStrategy(
        simulator: number,
        simulation: SimulationOptions,
        options?: StrategyRunOptions,
    ): Promise<StrategyResult> {
        return this.call<StrategyResult>(
            "runStrategy",
            { simulator, options: simulation, chunkSize: options?.chunkSize },
            { onProgress: options?.onProgress, signal: options?.signal },
        );
    }

    dispose(): void {
        this.transport.terminate?.();
    }
}
