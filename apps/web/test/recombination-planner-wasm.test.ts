// Requires a newly built matching WASM facade. It reads the selected frozen
// artifact in memory and never invokes build:data or changes generated metadata.
import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {readFileSync} from "node:fs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {ModInfo, RecombinationPlannerRequest, WorkerMessage} from "../src/app/engine-protocol";
import {runRecombinationPlanner} from "../src/app/recombination-planner";
import type {StrategyDocument} from "../src/app/strategy-model";
const root = new URL("../../../", import.meta.url);
const lock = JSON.parse(readFileSync(new URL("apps/web/runtime.lock.json", root), "utf8"));
const artifact = new URL(lock.runtime_directory + "/", root);
const bytes = (name: string) => readFileSync(new URL(name, artifact));
const hash = (value: Uint8Array) => createHash("sha256").update(value).digest("hex");
const manifestBytes = bytes("manifest.json"), manifest = JSON.parse(manifestBytes.toString("utf8"));
assert.equal(hash(manifestBytes), lock.manifest_sha256);
const game = bytes("game-data.json"), strings = bytes("strings.json");
assert.equal(hash(game), manifest.files["game-data.json"].sha256);
assert.equal(hash(strings), manifest.files["strings.json"].sha256);
const bundle = new TextEncoder().encode(`{"manifest":${manifestBytes.toString("utf8")},"strings":${strings.toString("utf8")},"game_data":${game.toString("utf8")}}`);
const dataIdentity: [string, string, string, string] = [manifest.source.data_hash, manifest.source.source_hash,
    manifest.files["game-data.json"].sha256, manifest.files["strings.json"].sha256];
const ordinaryModel = "poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1";
const advancedModel = "poe1-random-blocking-sensitivity-preserve-tier-roll-no-upgrade-v3-draft1";
const workers: Worker[] = [], exits: Set<Worker> = new Set();
function spawn(readOnlyPlanner: boolean): EngineClient {
    const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url)); workers.push(worker);
    worker.once("exit", () => exits.add(worker));
    const transport: EngineTransport = {postMessage: (message, transfer) => worker.postMessage(message,
            (transfer ?? []) as unknown as TransferListItem[]),
        onMessage: handler => worker.on("message", (message: WorkerMessage) => handler(message)),
        onError: handler => worker.on("error", handler), terminate: () => worker.terminate()};
    return new EngineClient(transport, {readOnlyPlanner});
}
const client = spawn(false), base = "Metadata/Items/Rings/Ring1";
const near = (a: number, b: number) => assert.ok(Math.abs(a - b) < 1e-7, `${a} differs from ${b}`);
try {
    await client.whenReady(); const abi = client.getAbiVersion(), data = await client.loadData(bundle.slice());
    const session = await client.createSession(data, base, 80), context = await client.createContext(session, 62667494);
    const blank = await client.createItem(session, {rarity: "rare", withImplicits: false});
    const pool = await client.debugPool(context, blank, {action: {type: "exalt"}, side: "prefix"});
    // Same finite fixture rule as the native Ring witness: first ordinary,
    // positive-proxy prefixes in session order with full groups compatible.
    // Admission/group checking stays in native editing and pair owners.
    const selected: ModInfo[] = [];
    const full = await client.createItem(session, {rarity: "rare", withImplicits: false});
    for (const candidate of pool.entries.filter(entry => entry.accepted && entry.generation_type === 0 && entry.spawn_weight > 0)
        .sort((a, b) => a.session_mod_id - b.session_mod_id)) {
        const info = await client.modInfo(session, candidate.session_mod_id);
        if (info.reach_kind !== 0) continue;
        try {await client.addMod(full, session, {key: info.key, side: "prefix"});}
        catch {continue;}
        selected.push(info); if (selected.length === 2) break;
    }
    assert.equal(selected.length, 2);
    const a = await client.createItem(session, {rarity: "magic", withImplicits: false});
    const b = await client.createItem(session, {rarity: "magic", withImplicits: false});
    await client.addMod(a, session, {key: selected[0].key, side: "prefix"});
    await client.addMod(b, session, {key: selected[1].key, side: "prefix"});
    const itemA = await client.exportItem(a, session), itemB = await client.exportItem(b, session), itemAB = await client.exportItem(full, session);
    const initialPhysical = [itemA, itemB];
    const child: StrategyDocument = {version: "v1", name: "Checked B", description: "", start_node_id: "start",
        base_state: {base_key: base, item_level: 80, rarity: "magic", with_implicits: false,
            prefixes: [{mod_key: selected[1].key}], suffixes: [], implicits: [], enchantments: []},
        output_contracts: [{id: "complete", base_key: base, predicate: {type: "always"}}],
        nodes: [{id: "start", kind: "start", position: {x: 0, y: 0}},
            {id: "success", kind: "terminal", terminal: "success", position: {x: 1, y: 0}}],
        edges: [{id: "entry", from: "start", to: "success", priority: 0, is_default: true}]};
    // Match all represented quoted rolls rather than just the successful goal.
    child.base_state.prefixes![0].rolls = (itemB as {prefixes: Array<{rolls: number[]}>}).prefixes[0].rolls;
    const request: RecombinationPlannerRequest = {version: "recombination-planner-request-v2", base_key: base, item_level: 80,
        data_identity: dataIdentity, model_id: ordinaryModel, price_identity: "fixture-prices",
        goal_set: {version: "calculator_goal_set_v1", actions: [], goals: [{id: "target", goal: {version: "v1", rarity: "rare",
            slots: selected.map(mod => ({family_mod_key: mod.key, min_tier: mod.family_tier_index}))}}]},
        acquisitions: [{id: "a", source_kind: "purchase", quote_identity: "quote-a", item_state: itemA, total_cost_chaos: 1, cost_complete: true},
            {id: "b", source_kind: "checked_feeder", quote_identity: "quote-b-full-retry-cost", item_state: itemB, total_cost_chaos: 1, cost_complete: true,
                feeder: {strategy_id: "b-child", revision: "revision-1", document_json: JSON.stringify(child),
                    output_contract_id: "complete", paid_start_cost_chaos: 1}},
            {id: "finished", source_kind: "completed_feeder", quote_identity: "quote-finished-multimod-comparison", item_state: itemAB,
                total_cost_chaos: 20, cost_complete: true}],
        initial_items: [], all_in_attempt_cost_chaos: 1, allow_incomplete_costs: false,
        feeder_economy: {version: "v1", id: "fixture-prices", prices: {annul: 2, scour: 3}},
        limits: {items: 64, states: 256, policy_iterations: 32, work: 20_000_000}, export_checked: true};
    const run = (r: RecombinationPlannerRequest, current = true) => runRecombinationPlanner(bundle, r,
        {expectedAbiVersion: abi, requestIdentity: JSON.stringify(r), isCurrent: () => current,
            createWorker: () => spawn(true), wallTimeMs: 180_000});
    const result = await run(request), receipt = result.result, checked = result.checked_export!;
    assert.ok(checked); near(receipt.expected_cost_chaos, 1 + 2 / .333);
    near(receipt.expected_recombinations, 1 / .333);
    near(receipt.acquisitions[0].expected_invocations + receipt.acquisitions[1].expected_invocations, 1 + 1 / .333);
    assert.equal(receipt.acquisitions[2].expected_invocations, 0);
    assert.ok(receipt.expected_cost_chaos < 3 / .333); assert.equal(receipt.acquisitions[1].native_feeder_checked, true);
    assert.equal(receipt.fully_priced_ranking, true); assert.equal(receipt.robust_cost_bound, false);
    near(checked.expected_cost_chaos as number, receipt.expected_cost_chaos);
    assert.deepEqual(checked.identity_receipt, receipt);
    const unverified = structuredClone(request); unverified.acquisitions[1].source_kind = "completed_feeder";
    delete unverified.acquisitions[1].feeder;
    await assert.rejects(run(unverified), /Unchecked feeder/);
    const cheaper = structuredClone(request); cheaper.acquisitions[2].total_cost_chaos = 2; cheaper.export_checked = false;
    const purchased = await run(cheaper); near(purchased.result.expected_cost_chaos, 2);
    assert.equal(purchased.result.expected_recombinations, 0); assert.equal(purchased.result.acquisitions[2].native_feeder_checked, false);
    const incomplete = structuredClone(request); incomplete.all_in_attempt_cost_chaos = null;
    incomplete.allow_incomplete_costs = true; incomplete.export_checked = false;
    const unpriced = await run(incomplete); assert.equal(unpriced.result.cost_complete, false);
    assert.equal(unpriced.result.fully_priced_ranking, false); assert.equal(unpriced.checked_export, null);
    const wrongData = structuredClone(request); wrongData.data_identity[0] = "changed";
    await assert.rejects(run(wrongData), /frozen data identity differs/);
    const wrongRevision = structuredClone(request); wrongRevision.acquisitions[2] = structuredClone(wrongRevision.acquisitions[1]);
    wrongRevision.acquisitions[2].id = "different";
    const changedDoc = structuredClone(child); changedDoc.base_state.rarity = "rare";
    wrongRevision.acquisitions[2].feeder!.document_json = JSON.stringify(changedDoc);
    await assert.rejects(run(wrongRevision), /conflicting documents/);
    await assert.rejects(run(request, false), /changed request identity/);
    for (const chance of [0, .5, 1]) {
        const scenario = structuredClone(request); scenario.model_id = advancedModel; scenario.export_checked = false;
        scenario.scenario = {id: `explicit-${chance}`, prefix_first_a: chance, prefix_first_b: chance};
        const report = await run(scenario);
        assert.equal(report.result.model_id, advancedModel); assert.equal(report.result.scenario_id, scenario.scenario.id);
        assert.equal(report.result.configuration_id, "pooled-exclusive-counts-first-positive-proxy-order-scenario-v1");
        assert.equal(report.result.robust_cost_bound, false); assert.equal(report.checked_export, null);
        near(report.result.expected_cost_chaos, 1 + 2 / .333); // This ordinary fixture is order-insensitive, not an ordering estimate.
    }
    const advancedApply = structuredClone(request); advancedApply.model_id = advancedModel;
    advancedApply.scenario = {id: "explicit-point", prefix_first_a: .5, prefix_first_b: .5};
    await assert.rejects(run(advancedApply), /Analysis-only|analysis-only|Apply/);
    assert.deepEqual([await client.exportItem(a, session), await client.exportItem(b, session)], initialPhysical);
    assert.equal(bundle.byteLength > 0, true, "planner loading must not detach caller bytes");
    assert.ok(workers.slice(1).every(worker => exits.has(worker)), "each read-only worker must exit before delivery");
    // General authored evaluation still refuses inventory graphs. Only the
    // native restricted checker attached to the export supplies its expectation.
    await assert.rejects(client.strategyEvaluate(session, checked.strategy), /inventory\/control identity/);
    const compiled = await client.compileStrategy(session, checked.strategy), economy = await client.loadEconomy(checked.economy);
    const simulator = await client.createSimulator(session, compiled, economy);
    try {
        const trials = await client.runStrategy(simulator, {target_runs: 1000, seed: 62667494,
            max_actions_per_run: 1000, max_graph_steps_per_run: 4096, retained_success_count: 1,
            retained_failure_count: 1, retained_trace_count: 1, max_trace_entries: 256});
        assert.equal(trials.summary.completed_runs, 1000); assert.equal(trials.summary.success_count, 1000);
        assert.equal(trials.summary.known_total_cost, 6826); assert.equal(trials.summary.total_actions, 11739);
        assert.equal(trials.summary.cost_status, "complete"); assert.equal(trials.missing_prices.length, 0);
        console.log(JSON.stringify({witness: "Ring1/80 checked recycling", modifiers: selected.map(mod => mod.key),
            model: receipt.model_id, data_identity: dataIdentity, expected_cost: receipt.expected_cost_chaos,
            seed: 62667494, trials: trials.summary, worker_survivors: workers.slice(1).filter(worker => !exits.has(worker)).length}));
    } finally {await client.closeSimulator(simulator); await client.closeStrategy(compiled); await client.closeEconomy(economy);}
    await client.closeSession(session);
} finally {await client.dispose(); await Promise.all(workers.map(worker => worker.terminate()));}
console.log("Matching WASM planner, complete feeder identity/costs, restricted export and physical recycling checks passed");
