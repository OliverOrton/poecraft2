import assert from "node:assert/strict";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {RecombinationPlannerRequest, RecombinationPlannerResponse, WorkerMessage} from "../src/app/engine-protocol";
import {runRecombinationPlanner, type RecombinationPlannerWorker} from "../src/app/recombination-planner";

function deferred<T>() {
    let resolve!: (value: T) => void;
    const promise = new Promise<T>(r => {resolve = r;});
    return {promise, resolve};
}
const request = (): RecombinationPlannerRequest => ({version: "recombination-planner-request-v2",
    base_key: "fixture", item_level: 80, data_identity: ["data", "source", "game", "strings"],
    model_id: "model", price_identity: "prices", goal_set: {version: "goal", slots: []},
    acquisitions: [], initial_items: [], all_in_attempt_cost_chaos: 1, allow_incomplete_costs: false,
    export_checked: false});
function response(r: RecombinationPlannerRequest): RecombinationPlannerResponse {
    return {result: {version: "random_recombination_inventory_v2", model_id: r.model_id,
        price_identity: r.price_identity, scenario_id: r.scenario?.id ?? null, data_identity: r.data_identity,
        goal_set: r.goal_set, ...(r.scenario ? {prefix_first: [r.scenario.prefix_first_a, r.scenario.prefix_first_b]} : {}),
        cost_complete: true, fully_priced_ranking: true, game_odds_estimated: true,
        global_optimality_claim: false, robust_cost_bound: false, expected_cost_chaos: 1,
        expected_recombinations: 0, acquisitions: []}, checked_export: null};
}
class FakeWorker implements RecombinationPlannerWorker {
    readonly ready = deferred<void>(); readonly loaded = deferred<number>();
    readonly completed = deferred<RecombinationPlannerResponse>(); readonly native = deferred<void>();
    readonly disposed = deferred<void>(); readonly terminated = deferred<void>();
    calls: string[] = []; bytes?: Uint8Array; snapshot?: RecombinationPlannerRequest;
    disposeCount = 0; abi = 2;
    whenReady() {this.calls.push("ready"); return this.ready.promise;}
    getAbiVersion() {return this.abi;}
    loadData(bytes: Uint8Array) {this.calls.push("load"); this.bytes = bytes; return this.loaded.promise;}
    recombinationPlanner(_data: number, r: RecombinationPlannerRequest) {
        this.calls.push("native"); this.snapshot = r; this.native.resolve(); return this.completed.promise;
    }
    dispose() {++this.disposeCount; this.disposed.resolve(); return this.terminated.promise;}
    open() {this.ready.resolve(); this.loaded.resolve(7);}
}
const bundle = new Uint8Array([1, 2, 3]);
const baseOptions = {expectedAbiVersion: 2, requestIdentity: "document/revision/content"};
let created = 0;
const before = new AbortController(); before.abort();
await assert.rejects(runRecombinationPlanner(bundle, request(), {...baseOptions, signal: before.signal,
    createWorker: () => {++created; throw Error("Must not create");}}), {name: "AbortError"});
assert.equal(created, 0);

// Snapshot caller mutations and wait for actual transport termination before delivery.
{
    const fake = new FakeWorker(), r = request();
    const options = {...baseOptions, createWorker: () => fake};
    let delivered = false;
    const run = runRecombinationPlanner(bundle, r, options).then(value => {delivered = true; return value;});
    r.price_identity = "changed"; options.requestIdentity = "changed";
    fake.open(); await fake.native.promise;
    assert.notStrictEqual(fake.bytes!.buffer, bundle.buffer); assert.deepEqual(bundle, new Uint8Array([1, 2, 3]));
    assert.equal(fake.snapshot!.price_identity, "prices");
    fake.completed.resolve(response(fake.snapshot!)); await fake.disposed.promise;
    assert.equal(delivered, false); fake.terminated.resolve();
    const result = await run;
    assert.equal(result.worker.request_identity, baseOptions.requestIdentity);
    assert.equal(JSON.parse(result.worker.request_json).price_identity, "prices");
    assert.equal(result.worker.disposed, true); assert.equal(fake.disposeCount, 1);
}
for (const stage of ["ready", "load", "native"] as const) {
    const fake = new FakeWorker(), controller = new AbortController();
    const run = runRecombinationPlanner(bundle, request(), {...baseOptions, signal: controller.signal, createWorker: () => fake});
    const rejected = assert.rejects(run, {name: "AbortError"});
    if (stage !== "ready") fake.ready.resolve();
    if (stage === "native") {fake.loaded.resolve(7); await fake.native.promise;}
    else await Promise.resolve();
    controller.abort(); await fake.disposed.promise; assert.equal(fake.disposeCount, 1);
    fake.terminated.resolve(); await rejected;
    fake.open(); fake.completed.resolve(response(request()));
    await Promise.resolve(); await Promise.resolve();
    assert.equal(fake.disposeCount, 1, "late replies must not deliver or terminate twice");
    if (stage === "ready") assert.deepEqual(fake.calls, ["ready"]);
}
for (const mismatch of ["abi", "model", "data", "price", "scenario", "order", "goal", "export", "stale"] as const) {
    const fake = new FakeWorker(), r = request();
    r.scenario = {id: "explicit", prefix_first_a: .25, prefix_first_b: .75};
    if (mismatch === "abi") fake.abi = 999;
    const run = runRecombinationPlanner(bundle, r, {...baseOptions, createWorker: () => fake,
        isCurrent: () => mismatch !== "stale"});
    const rejected = assert.rejects(run, /ABI|identity/);
    fake.open();
    if (mismatch !== "abi") {
        await fake.native.promise; const reply = response(fake.snapshot!);
        if (mismatch === "model") reply.result.model_id = "wrong";
        if (mismatch === "data") reply.result.data_identity = ["wrong", "source", "game", "strings"];
        if (mismatch === "price") reply.result.price_identity = "wrong";
        if (mismatch === "scenario") reply.result.scenario_id = "wrong";
        if (mismatch === "order") reply.result.prefix_first = [.5, .5];
        if (mismatch === "goal") reply.result.goal_set = {version: "different"};
        if (mismatch === "export") reply.checked_export = {version: "checked-recombination-builder-v1",
            strategy: {} as never, economy: {}, identity_receipt: {...reply.result, price_identity: "wrong"}};
        fake.completed.resolve(reply);
    }
    await fake.disposed.promise; fake.terminated.resolve(); await rejected;
    if (mismatch === "abi") assert.deepEqual(fake.calls, ["ready"]);
}
{
    const fake = new FakeWorker();
    const run = runRecombinationPlanner(bundle, request(), {...baseOptions, wallTimeMs: 10, createWorker: () => fake});
    const rejected = assert.rejects(run, /wall limit/);
    await fake.disposed.promise; fake.terminated.resolve(); await rejected;
    assert.equal(fake.disposeCount, 1);
}
for (const readOnlyPlanner of [false, true]) {
    let receive!: (m: WorkerMessage) => void; const calls: string[] = [];
    const client = new EngineClient({postMessage: message => {if (message.kind === "request") calls.push(message.method);},
        onMessage: handler => {receive = handler;}}, {readOnlyPlanner});
    receive({kind: "ready", abiVersion: 2}); await client.whenReady();
    if (readOnlyPlanner) await assert.rejects(client.createSession(7, "fixture", 80), /live crafting resources/);
    else await assert.rejects(client.recombinationPlanner(7, request()), /request-owned worker/);
    assert.deepEqual(calls, []); await client.dispose();
}

// Real Worker.terminate while synchronous work cannot service cancel messages.
// This transport witness parks inside its native-call stand-in; it is not a
// game-kernel/model test. The matching-WASM fixture qualifies the facade separately.
{
    const gate = new SharedArrayBuffer(4), entered = new Int32Array(gate), controller = new AbortController();
    const worker = new Worker(`const {parentPort,workerData}=require('node:worker_threads');
        const gate=new Int32Array(workerData);parentPort.postMessage({kind:'ready',abiVersion:2});
        parentPort.on('message',m=>{if(m.kind!=='request')return;
          if(m.method==='loadData')parentPort.postMessage({kind:'response',id:m.id,ok:true,result:{data:7}});
          if(m.method==='recombinationPlanner'){Atomics.store(gate,0,1);parentPort.postMessage({entered:true});
            Atomics.wait(gate,0,1);throw Error('Terminated work must not resume');}});`, {eval: true, workerData: gate});
    let exitObserved = false; worker.once("exit", () => {exitObserved = true;});
    const transport: EngineTransport = {postMessage: (message, transfer) => worker.postMessage(message,
            (transfer ?? []) as unknown as TransferListItem[]),
        onMessage: handler => worker.on("message", message => {
            if (message.entered) controller.abort(); else handler(message);
        }), onError: handler => worker.on("error", handler), terminate: () => worker.terminate()};
    try {
        await assert.rejects(runRecombinationPlanner(bundle, request(), {...baseOptions, signal: controller.signal,
            wallTimeMs: 5000, createWorker: () => new EngineClient(transport, {readOnlyPlanner: true})}), {name: "AbortError"});
        assert.equal(Atomics.load(entered, 0), 1); assert.equal(exitObserved, true);
    } finally {await worker.terminate();}
}
console.log("Planner immutable identity, live-worker refusal, real termination, stale reply and lifetime checks passed");
