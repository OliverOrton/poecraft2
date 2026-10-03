import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {ClientMessage, WorkerMessage, SimulationOptions} from "../src/app/engine-protocol";
import {pinStrategyFeeder, type StrategyDocument} from "../src/app/strategy-model";

const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
const transport: EngineTransport = {postMessage: (message: ClientMessage, transfer?: Transferable[]) => worker.postMessage(message, (transfer ?? []) as unknown as TransferListItem[]),
    onMessage: handler => worker.on("message", (message: WorkerMessage) => handler(message)), terminate: () => void worker.terminate()};
const client = new EngineClient(transport);
const root = new URL("../../../data/compiled/current/", import.meta.url);
const read = (name: string) => readFileSync(new URL(name, root), "utf8");
const bundle = new TextEncoder().encode(`{"manifest":${read("manifest.json")},"strings":${read("strings.json")},"game_data":${read("game-data.json")}}`);
const base = "Metadata/Items/Armours/BodyArmours/BodyInt17";
const child: StrategyDocument = {version: "v1", name: "Paid child", description: "", start_node_id: "start",
    base_state: {base_key: base, item_level: 86, rarity: "normal"},
    output_contracts: [{id: "magic", base_key: base, predicate: {type: "rarity_is", rarity: "magic"}}],
    nodes: [{id: "start", kind: "start", position: {x: 0, y: 0}}, {id: "craft", kind: "operation", operation: {type: "transmute", params: {}}, position: {x: 1, y: 0}},
        {id: "end", kind: "terminal", terminal: "success", position: {x: 2, y: 0}}],
    edges: [{id: "begin", from: "start", to: "craft", priority: 0}, {id: "done", from: "craft", to: "end", priority: 0}]};
const parent: StrategyDocument = {...structuredClone(child), name: "Parent", output_contracts: [], resources: [pinStrategyFeeder("feeder", {id: "saved-child", name: "Child", strategy: child, revision: "r1", createdAt: 1}, "magic")]};
parent.nodes[1].operation = {type: "invoke_feeder", params: {resource_id: "feeder"}};
try {
    await client.whenReady();
    const data = await client.loadData(bundle);
    const session = await client.createSession(data, base, 86);
    const economy = await client.loadEconomy({version: "v1", id: "feeder-fixed-quotes", prices: {"resource:feeder": 7, transmute: 2}});
    const execute = async (document: StrategyDocument, options: SimulationOptions) => {
        const strategy = await client.compileStrategy(session, document);
        const simulator = await client.createSimulator(session, strategy, economy);
        try { return await client.runStrategy(simulator, {seed: 42, max_actions_per_run: 10, retained_trace_count: 2, max_trace_entries: 30, ...options}); }
        finally { await client.closeSimulator(simulator); await client.closeStrategy(strategy); }
    };
    const result = await execute(parent, {target_runs: 1000});
    assert.equal(result.summary.success_count, 1000);
    assert.equal(result.summary.total_actions, 2000);
    assert.equal(result.summary.known_total_cost, 9000);
    assert.equal(result.summary.cost_status, "complete");
    const output = result.traces[0].entries.at(-1)!.resources![0];
    assert.equal(output.base_key, base); assert.equal(output.item_level, 86); assert.equal(output.acquisitions, 1);
    assert.equal(output.feeder!.output_accepted, true); assert.equal(output.feeder!.known_cost, 9);
    assert.equal(output.feeder!.revision, "r1");
    assert.equal(result.action_distribution.reduce((sum, row) => sum + row.count, 0), 2000);
    assert.ok(result.sampled_accounting.materials.some(row => row.price_key === "resource:feeder" && row.count === 1000));
    assert.ok(result.sampled_accounting.materials.some(row => row.price_key === "transmute" && row.count === 1000));
    const capped = await execute(parent, {target_runs: 1, max_cost_per_run: 7});
    assert.equal(capped.summary.cost_limit_count, 1); assert.equal(capped.summary.known_total_cost, 7);
    const failure = structuredClone(parent);
    const pinned = JSON.parse(failure.resources![0].feeder!.document_json) as StrategyDocument;
    pinned.output_contracts![0].predicate = {type: "rarity_is", rarity: "rare"};
    failure.resources![0].feeder!.document_json = JSON.stringify(pinned);
    const mismatched = await execute(failure, {target_runs: 1});
    assert.equal(mismatched.summary.success_count, 0); assert.equal(mismatched.summary.known_total_cost, 9);
    assert.equal(mismatched.traces[0].entries.at(-1)!.resources![0].feeder!.output_accepted, false);
    await assert.rejects(client.strategyEvaluate(session, parent), /inventory\/control identity/);
    await client.closeEconomy(economy); await client.closeSession(session);
    console.log("WASM worker feeder cost, output predicate, limits, resources and exact-refusal checks passed.");
} finally { client.destroy(); }
