import {EngineClient} from "./engine-client";
import {EngineError, type RecombinationPlannerRequest, type RecombinationPlannerResponse} from "./engine-protocol";

export interface RecombinationPlannerWorker {
    whenReady(): Promise<void>;
    getAbiVersion(): number;
    loadData(bytes: Uint8Array): Promise<number>;
    recombinationPlanner(data: number, request: RecombinationPlannerRequest): Promise<RecombinationPlannerResponse>;
    dispose(): void | Promise<unknown>;
}
export interface RecombinationPlannerRunOptions {
    signal?: AbortSignal;
    expectedAbiVersion: number;
    /** Immutable authored revision/content identity retained with the result. */
    requestIdentity: string;
    isCurrent?: () => boolean;
    wallTimeMs?: number;
    /** Test/Node transport injection; production always creates a fresh worker. */
    createWorker?: () => RecombinationPlannerWorker;
}
export interface RecombinationPlannerRunResult extends RecombinationPlannerResponse {
    worker: {request_identity: string; request_json: string;
        cancellation: "request-owned-worker-termination"; disposed: true};
}
function aborted(): Error {
    const error = new Error("Recombination planning cancelled; no crafting input was consumed");
    error.name = "AbortError"; return error;
}

/** This native solver is synchronous. Its worker belongs to one immutable,
 * read-only request and is actually terminated on abort, timeout and completion.
 * Persistent engine workers/live handles never participate in this lifecycle. */
export function runRecombinationPlanner(bundle: Uint8Array, request: RecombinationPlannerRequest,
    options: RecombinationPlannerRunOptions): Promise<RecombinationPlannerRunResult> {
    if (options.signal?.aborted) return Promise.reject(aborted());
    const wallTime = options.wallTimeMs ?? 180_000;
    if (!Number.isFinite(wallTime) || wallTime < 1 || wallTime > 180_000)
        return Promise.reject(new Error("Planner wall limit must be between 1 and 180000 ms"));
    if (!options.requestIdentity) return Promise.reject(new Error("Planner request identity is required"));
    const {signal, expectedAbiVersion, requestIdentity, isCurrent} = options;
    const requestJson = JSON.stringify(request);
    const snapshot = JSON.parse(requestJson) as RecombinationPlannerRequest;
    // loadData transfers the copy; the caller's frozen bytes stay owned by it.
    const ownedBundle = bundle.slice();
    const client = (options.createWorker ?? (() => EngineClient.spawnRecombinationPlanner()))();
    return new Promise((resolve, reject) => {
        let finished = false;
        const onAbort = () => finish(undefined, aborted());
        const timer = setTimeout(() => finish(undefined, new Error("Recombination planner wall limit reached")), wallTime);
        const finish = (result?: RecombinationPlannerResponse, error?: unknown) => {
            if (finished) return;
            finished = true;
            clearTimeout(timer);
            signal?.removeEventListener("abort", onAbort);
            // Calling dispose immediately interrupts an already-running native
            // call. Node transports can await actual worker termination here.
            let termination: void | Promise<unknown>;
            try { termination = client.dispose(); } catch (failure) { reject(failure); return; }
            Promise.resolve(termination).then(() => {
                if (error !== undefined) { reject(error); return; }
                if (signal?.aborted) { reject(aborted()); return; }
                if (isCurrent && !isCurrent()) {
                    reject(new EngineError(1, "Planner result invalidated by changed request identity")); return;
                }
                resolve({...result!, worker: {request_identity: requestIdentity,
                    request_json: requestJson, cancellation: "request-owned-worker-termination", disposed: true}});
            }, reject);
        };
        signal?.addEventListener("abort", onAbort, {once: true});
        // Protect the race between the first preflight and listener registration.
        if (signal?.aborted) { onAbort(); return; }
        void (async () => {
            await client.whenReady();
            if (finished) return;
            if (client.getAbiVersion() !== expectedAbiVersion)
                throw new EngineError(4, "Planner engine ABI differs from the pinned build");
            const data = await client.loadData(ownedBundle);
            if (finished) return;
            const result = await client.recombinationPlanner(data, snapshot);
            if (finished) return;
            if (result.result.version !== "random_recombination_inventory_v2" ||
                result.result.model_id !== snapshot.model_id || result.result.price_identity !== snapshot.price_identity ||
                result.result.scenario_id !== (snapshot.scenario?.id ?? null) ||
                JSON.stringify(result.result.data_identity) !== JSON.stringify(snapshot.data_identity) ||
                JSON.stringify(result.result.goal_set) !== JSON.stringify(snapshot.goal_set) ||
                (snapshot.scenario && JSON.stringify(result.result.prefix_first) !==
                    JSON.stringify([snapshot.scenario.prefix_first_a, snapshot.scenario.prefix_first_b])) ||
                (result.checked_export && JSON.stringify(result.checked_export.identity_receipt) !== JSON.stringify(result.result)))
                throw new EngineError(1, "Planner response model/data/price/scenario identity differs");
            finish(result);
        })().catch(error => finish(undefined, error));
    });
}
