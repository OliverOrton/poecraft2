import assert from "node:assert/strict";
import {selectedRuntime} from "../../../scripts/build-data-bundle.mjs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {ClientMessage, WorkerMessage, SimulationOptions} from "../src/app/engine-protocol";
import {pinStrategyFeeder, type StrategyDocument} from "../src/app/strategy-model";

const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
const transport: EngineTransport = {postMessage: (message: ClientMessage, transfer?: Transferable[]) => worker.postMessage(message, (transfer ?? []) as unknown as TransferListItem[]),
    onMessage: handler => worker.on("message", (message: WorkerMessage) => handler(message)), terminate: () => void worker.terminate()};
const client = new EngineClient(transport);
const selected = selectedRuntime();
const bundle = new Uint8Array(selected.bundle);
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
    const recycled = structuredClone(parent);
    recycled.nodes.push(
        {id: "discard", kind: "operation", operation: {type: "discard_resource", params: {resource_id: "current"}}, position: {x: 3, y: 0}},
        {id: "move", kind: "operation", operation: {type: "move_resource", params: {from: "feeder", to: "current"}}, position: {x: 4, y: 0}});
    recycled.edges[1].to = "discard";
    recycled.edges.push({id: "empty", from: "discard", to: "move", priority: 0}, {id: "again", from: "move", to: "craft", priority: 0});
    const twice = await execute(recycled, {target_runs: 1, max_actions_per_run: 6});
    assert.equal(twice.summary.action_limit_count, 1);
    assert.equal(twice.summary.total_actions, 6);
    assert.equal(twice.summary.known_total_cost, 18);
    const entries = twice.traces[0].entries;
    const moved = entries.find(entry => entry.node_id === "move")!;
    assert.equal(moved.resources![0].lifecycle, 1); // consumed after the real move
    assert.equal(moved.resources![0].identity, "feeder/1");
    assert.equal(moved.known_cumulative_cost, 9); // move/discard did not reacquire
    const replacement = entries.at(-1)!.resources![0];
    assert.equal(replacement.identity, "feeder/2");
    assert.equal(replacement.acquisitions, 2);
    assert.equal(replacement.feeder!.known_cost, 9);
    assert.ok(twice.sampled_accounting.materials.some(row => row.price_key === "resource:feeder" && row.count === 2));
    const stopped = structuredClone(parent);
    const stoppedChild = JSON.parse(stopped.resources![0].feeder!.document_json) as StrategyDocument;
    stoppedChild.nodes.at(-1)!.terminal = "failure";
    stopped.resources![0].feeder!.document_json = JSON.stringify(stoppedChild);
    const failed = await execute(stopped, {target_runs: 1});
    assert.equal(failed.summary.success_count, 0);
    assert.equal(failed.summary.known_total_cost, 9);
    assert.equal(failed.traces[0].entries.at(-1)!.resources![0].feeder!.output_accepted, false);
    await assert.rejects(client.strategyEvaluate(session, parent), /inventory\/control identity/);
    // Functional Builder ports delegate selection and atomic Apply to the native
    // pair owner. These cases require a matching freshly built WASM artifact.
    const rareChild = structuredClone(child);
    rareChild.nodes[1].operation!.type = "alchemy";
    rareChild.output_contracts![0].predicate = {type: "rarity_is", rarity: "rare"};
    const pairGraph: StrategyDocument = {version: "v1", name: "Builder pair", description: "", start_node_id: "start",
        base_state: {...child.base_state, rarity: "rare"},
        resources: [pinStrategyFeeder("feeder", {id: "rare-child", name: "Rare child", revision: "r1", createdAt: 1, strategy: rareChild}, "magic"),
            {id: "out", base_state: child.base_state, acquisition_price_key: "resource:out"}],
        nodes: [{id: "start", kind: "start", position: {x: 0, y: 0}},
            {id: "source", kind: "operation", source_only: true, operation: {type: "invoke_feeder", params: {resource_id: "feeder"}}, position: {x: 1, y: 1}},
            {id: "pair", kind: "operation", operation: {type: "recombination", params: {input_a: "current", input_b: "", output: "out"}}, position: {x: 2, y: 0}},
            {id: "end", kind: "terminal", terminal: "success", position: {x: 3, y: 0}}],
        edges: [{id: "entry", from: "start", to: "pair", kind: "control", to_port: "input_a", priority: 0},
            {id: "supply", from: "source", to: "pair", kind: "item", to_port: "input_b", priority: 0},
            {id: "after", from: "pair", to: "end", kind: "control", priority: 0, condition: {type: "rarity_is", rarity: "rare"}}]};
    const pairEconomy = await client.loadEconomy({version: "v1", id: "pair-quotes", prices: {"resource:feeder": 7, alchemy: 2, "resource:donor": 4,
        "recombination:gold": 999, "recombination:dust": 999}});
    const pairRun = async (document: StrategyDocument, options: SimulationOptions) => {
        const compiled = await client.compileStrategy(session, document);
        const simulator = await client.createSimulator(session, compiled, pairEconomy);
        try { return await client.runStrategy(simulator, {seed: 42, max_actions_per_run: 10, retained_trace_count: 2, max_trace_entries: 40, ...options}); }
        finally { await client.closeSimulator(simulator); await client.closeStrategy(compiled); }
    };
    try {
        const paired = await pairRun(pairGraph, {target_runs: 1000});
        assert.equal(paired.summary.success_count, 1000); assert.equal(paired.summary.total_actions, 3000);
        assert.equal(paired.summary.known_total_cost, 9000); assert.equal(paired.summary.cost_status, "incomplete");
        const final = paired.traces[0].entries.at(-1)!;
        assert.equal((final.item as {lifecycle: number}).lifecycle, 1);
        assert.equal(final.resources![0].lifecycle, 1);
        const actual = final.resources![1];
        assert.equal(actual.identity, "recomb/pair/1"); assert.equal(actual.lifecycle, 0); assert.equal(actual.active_output, true);
        assert.equal(actual.recombination!.input_a, "current/1"); assert.equal(actual.recombination!.input_b, "feeder/1");
        assert.equal(actual.recombination!.gold_cost_complete, false); assert.equal(actual.recombination!.dust_cost_complete, false);
        assert.equal(actual.recombination!.game_odds_estimated, true);
        assert.equal(paired.examples.success[0].resources![1].identity, actual.identity);
        assert.ok(paired.missing_prices.some(row => row.key === "recombination:gold"));
        assert.ok(paired.missing_prices.some(row => row.key === "recombination:dust"));
        assert.equal(paired.sampled_accounting.materials.some(row => row.price_key.startsWith("recombination:")), false);
        const unknownCap = await pairRun(pairGraph, {target_runs: 1, max_cost_per_run: 10000});
        assert.equal(unknownCap.summary.missing_price_run_count, 1); assert.equal(unknownCap.summary.total_actions, 0);
        assert.equal(unknownCap.summary.known_total_cost, 0); assert.equal(unknownCap.traces[0].entries.at(-1)!.resources![0].acquisitions, 0);
        const limited = await pairRun(pairGraph, {target_runs: 1, max_actions_per_run: 2});
        assert.equal(limited.summary.action_limit_count, 1); assert.equal(limited.summary.total_actions, 2); assert.equal(limited.summary.known_total_cost, 9);
        assert.equal(limited.traces[0].entries.at(-1)!.resources![0].lifecycle, 0);
        assert.equal(limited.traces[0].entries.at(-1)!.resources![1].lifecycle, 1);
        const loop = structuredClone(pairGraph); loop.edges[2].to = "pair"; loop.edges[2].to_port = "input_a";
        const twicePair = await pairRun(loop, {target_runs: 1, max_actions_per_run: 6});
        assert.equal(twicePair.summary.action_limit_count, 1); assert.equal(twicePair.summary.total_actions, 6); assert.equal(twicePair.summary.known_total_cost, 18);
        const reused = twicePair.traces[0].entries.at(-1)!.resources![1];
        assert.equal(reused.identity, "recomb/pair/2"); assert.equal(reused.recombination!.input_a, "recomb/pair/1");
        assert.equal(reused.recombination!.input_b, "feeder/2"); // another fresh paid child, the actual first result recycled
        const stoppedPair = structuredClone(pairGraph);
        const stoppedRare = JSON.parse(stoppedPair.resources![0].feeder!.document_json) as StrategyDocument;
        stoppedRare.nodes.at(-1)!.terminal = "failure"; stoppedPair.resources![0].feeder!.document_json = JSON.stringify(stoppedRare);
        const childFailed = await pairRun(stoppedPair, {target_runs: 1});
        assert.equal(childFailed.summary.success_count, 0); assert.equal(childFailed.summary.known_total_cost, 9);
        assert.equal(childFailed.traces[0].entries.at(-1)!.resources![0].feeder!.output_accepted, false);
        assert.equal(childFailed.traces[0].entries.at(-1)!.resources![1].lifecycle, 1);
        // Two paid sources on different bases. The output route and resource
        // example must agree with the actual carrier's base, level and item.
        const mixedPair = structuredClone(pairGraph);
        const otherBase = "Metadata/Items/Armours/BodyArmours/BodyInt16";
        const blankRare = structuredClone(rareChild); blankRare.nodes[1].operation = {type: "condition_check_only", params: {}};
        blankRare.base_state.rarity = "rare";
        mixedPair.resources![0] = pinStrategyFeeder("feeder", {id: "blank-child", name: "Blank child", revision: "r1", createdAt: 1, strategy: blankRare}, "magic");
        mixedPair.resources!.push({id: "donor", base_state: {base_key: otherBase, item_level: 60, rarity: "rare"}, acquisition_price_key: "resource:donor"});
        mixedPair.nodes.push({id: "donor", kind: "operation", source_only: true, operation: {type: "acquire_resource", params: {resource_id: "donor"}}, position: {x: 1, y: 2}},
            {id: "other", kind: "terminal", terminal: "success", position: {x: 4, y: 1}});
        mixedPair.edges.push({id: "supply-a", from: "donor", to: "pair", kind: "item", to_port: "input_a", priority: 0},
            {id: "other-base", from: "pair", to: "other", priority: 1, is_default: true});
        mixedPair.edges[2].condition = {type: "base_is", base_key: base};
        const mixed = await pairRun(mixedPair, {target_runs: 1});
        assert.equal(mixed.summary.success_count, 1); assert.equal(mixed.summary.total_actions, 3); assert.equal(mixed.summary.known_total_cost, 11);
        const returned = mixed.examples.success[0].resources!.find(resource => resource.active_output)!;
        assert.equal(returned.item_level, 75); assert.ok([base, otherBase].includes(returned.base_key!));
        assert.equal(mixed.examples.success[0].terminal_node_id, returned.base_key === base ? "end" : "other");
        assert.equal(returned.recombination!.input_a, "donor/1"); assert.equal(returned.recombination!.input_b, "feeder/1");
        assert.equal((returned.item as {rarity: number}).rarity, 2); // Session level is carried by the resource envelope.
        const unsupported = structuredClone(mixedPair);
        unsupported.resources!.at(-1)!.base_state.generic_influence_bits = 1;
        const refused = await pairRun(unsupported, {target_runs: 1});
        assert.equal(refused.summary.success_count, 0);
        assert.equal(refused.traces[0].entries.at(-1)!.resources![0].lifecycle, 0);
        assert.equal(refused.traces[0].entries.at(-1)!.resources![2].lifecycle, 0);
        assert.equal(refused.traces[0].entries.at(-1)!.resources![1].lifecycle, 1);
        await assert.rejects(client.strategyEvaluate(session, pairGraph), /inventory\/control identity/);
    } finally { await client.closeEconomy(pairEconomy); }
    await client.closeEconomy(economy); await client.closeSession(session);
    console.log("WASM worker feeder cost, output predicate, limits, resources and exact-refusal checks passed.");
} finally { client.dispose(); await worker.terminate(); }
