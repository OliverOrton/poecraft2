// Direct adapter ownership regression: no feeders, sampled Apply, export,
// discovery of a retry policy or simulator. The native fixture is already admitted.
import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {readFileSync} from "node:fs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {selectedRuntime} from "../../../scripts/build-data-bundle.mjs";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import {EngineError, type RecombinationPlannerRequest, type WorkerMessage} from "../src/app/engine-protocol";
import {prepareRingPlannerFixture} from "./recombination-planner-fixture";
const legacyNegative = process.argv.includes("--expect-legacy-session-conflict");
if (legacyNegative) {
    const sha = (path: string) => createHash("sha256").update(readFileSync(new URL(path, import.meta.url))).digest("hex");
    assert.equal(sha("../../../bindings/wasm/dist/poecraft_engine.wasm"), "c8b4991a0457b671d677f5751eb88f9025b6982a4b2011b3b2321a80dd1038cb");
    assert.equal(sha("../../../bindings/wasm/dist/poecraft_engine.mjs"), "8ec20cf7c0f86cbad4c5d0638e0733da3110610cf02091c0807d4b87186d2b97");
}
const workers: Worker[] = [], exited = new Set<Worker>();
function spawn(readOnlyPlanner: boolean): EngineClient {
    const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url)); workers.push(worker);
    worker.once("exit", () => exited.add(worker));
    const transport: EngineTransport = {postMessage: (message, transfer) => worker.postMessage(message,
        (transfer ?? []) as unknown as TransferListItem[]), onMessage: handler => worker.on("message", (message: WorkerMessage) => handler(message)),
        onError: handler => worker.on("error", handler), terminate: () => worker.terminate()};
    return new EngineClient(transport, {readOnlyPlanner});
}
const selected = selectedRuntime(), bundle = new Uint8Array(selected.bundle);
const live = spawn(false), planner = spawn(true);
try {
    await Promise.all([live.whenReady(), planner.whenReady()]);
    assert.equal(planner.getAbiVersion(), live.getAbiVersion());
    const liveData = await live.loadData(bundle.slice()), plannerData = await planner.loadData(bundle.slice());
    const fixture = await prepareRingPlannerFixture(live, liveData);
    const originalPhysical = await live.exportItem(fixture.full, fixture.session);
    const dataBefore = await planner.dataSummary(plannerData);
    const request: RecombinationPlannerRequest = {version: "recombination-planner-request-v2", base_key: fixture.base, item_level: 80,
        data_identity: [selected.manifest.source.data_hash, selected.manifest.source.source_hash,
            selected.manifest.files["game-data.json"].sha256, selected.manifest.files["strings.json"].sha256],
        model_id: "poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1", price_identity: "fixture-prices",
        goal_set: {version: "calculator_goal_set_v1", actions: [], goals: [{id: "already-owned", goal: {version: "v1", rarity: "rare",
            slots: fixture.selected.map(mod => ({family_mod_key: mod.key, min_tier: mod.family_tier_index}))}}]},
        acquisitions: [{id: "finished", source_kind: "purchase", quote_identity: "quote-finished", item_state: fixture.itemAB,
            total_cost_chaos: 20, cost_complete: true}],
        initial_items: [{item_state: fixture.itemAB, paid_cost_chaos: 3}], all_in_attempt_cost_chaos: 1,
        allow_incomplete_costs: false, limits: {items: 4, states: 8, policy_iterations: 2, work: 1_000_000}, export_checked: false};
    const requestBefore = JSON.stringify(request);
    await assert.rejects(live.recombinationPlanner(liveData, request), /request-owned worker/);
    await assert.rejects(planner.createSession(plannerData, fixture.base, 80), /live crafting resources/);
    // Deliberately reuse a read-only dataset in this API regression. Production
    // coordinator requests still create and terminate one worker per request.
    if (legacyNegative) {
        await assert.rejects(planner.recombinationPlanner(plannerData, request), (error: unknown) => {
            assert.ok(error instanceof EngineError);
            assert.equal(error.detail, "Recombination item contains conflicting physical modifiers"); return true;
        });
    } else {
        const first = await planner.recombinationPlanner(plannerData, request);
        const again = await planner.recombinationPlanner(plannerData, request);
        assert.deepEqual(again, first, "fresh per-call session mappings must not accumulate or leak prior state");
        assert.equal(first.result.expected_cost_chaos, 3); assert.equal(first.result.entry_cost_chaos, 3);
        assert.equal(first.result.expected_recombinations, 0); assert.equal(first.result.acquisitions[0].expected_invocations, 0);
        const differentlyPaid = structuredClone(request); differentlyPaid.initial_items[0].paid_cost_chaos = 11;
        const changed = await planner.recombinationPlanner(plannerData, differentlyPaid);
        assert.equal(changed.result.expected_cost_chaos, 11); assert.equal(changed.result.entry_cost_chaos, 11);
        assert.equal(changed.result.expected_recombinations, 0); assert.equal(changed.result.acquisitions[0].expected_invocations, 0);
    }
    assert.equal(JSON.stringify(request), requestBefore); assert.ok(bundle.byteLength > 0);
    assert.deepEqual(await planner.dataSummary(plannerData), dataBefore, "canonical dataset ownership must remain immutable");
    assert.deepEqual(await live.exportItem(fixture.full, fixture.session), originalPhysical, "planning must not consume or rewrite the live item");
    await planner.closeData(plannerData);
    for (const item of [fixture.blank, fixture.full, fixture.a, fixture.b]) await live.closeItem(item);
    await live.closeContext(fixture.context); await live.closeSession(fixture.session); await live.closeData(liveData);
    console.log(legacyNegative ? "Known old adapter constructor conflict reproduced on admitted owned Ring input; ownership preserved" :
        "Repeated planner calls preserve fresh mappings, immutable data, live physical inputs and explicit paid ownership");
} finally {await live.dispose(); await planner.dispose(); await Promise.all(workers.map(worker => worker.terminate()));}
assert.equal(exited.size, workers.length);
