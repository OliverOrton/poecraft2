import assert from "node:assert/strict";
import {createDefaultStrategy, cloneStrategy, pinStrategyFeeder, strategyResourcePorts, strategyNodeConnectors, strategyConnection, strategyRecombinationBindings, strategySourceEntryEdge, validateStrategy, type StrategyDocument} from "../src/app/strategy-model";
import {EditHistory} from "../src/app/edit-history";
import {strategyStructuralSignature} from "../src/app/strategy-eval-presentation";

const child = createDefaultStrategy();
child.solver_policy_scope = "gated_search_with_paid_root_foulborn_salvage_v2";
child.output_contracts = [{id: "ready", base_key: child.base_state.base_key, predicate: {type: "rarity_is", rarity: "rare"}}];
const saved = {id: "saved-child", name: "Child", revision: "r1", createdAt: 1, strategy: child};
const parent = createDefaultStrategy();
parent.resources = [pinStrategyFeeder("feeder", saved, "ready")];
parent.nodes[1].operation = {type: "invoke_feeder", params: {resource_id: "feeder"}};
assert.deepEqual(validateStrategy(parent).filter(issue => issue.severity === "error"), []);
assert.deepEqual(strategyResourcePorts(parent.nodes[1]), {inputs: [], outputs: ["feeder"]});

const frozen = parent.resources[0].feeder!.document_json;
child.base_state.rarity = "normal";
saved.revision = "r2";
assert.equal(parent.resources[0].feeder!.document_json, frozen);
assert.equal(parent.resources[0].feeder!.revision, "r1");
assert.deepEqual(JSON.parse(JSON.stringify(cloneStrategy(parent))).resources, parent.resources);
const history = new EditHistory<StrategyDocument>();
history.reset(parent);
const edited = cloneStrategy(parent);
edited.nodes[1].operation = {type: "move_resource", params: {from: "feeder", to: "current"}};
history.record(edited);
assert.equal(history.go(0)!.resources![0].feeder!.document_json, frozen);
assert.equal(history.go(1)!.nodes[1].operation!.type, "move_resource");
const restored = new EditHistory<StrategyDocument>();
restored.restore(JSON.parse(JSON.stringify(history.export())), edited);
assert.equal(restored.go(0)!.resources![0].feeder!.revision, "r1");

const invalid = cloneStrategy(parent);
invalid.resources![0].feeder!.output_contract_id = "missing";
assert.ok(validateStrategy(invalid).some(issue => issue.message.includes("missing")));
invalid.resources![0].feeder!.document_json = "{";
assert.ok(validateStrategy(invalid).some(issue => issue.severity === "error"));
const cycle = cloneStrategy(parent);
cycle.resources![0].feeder!.document_json = JSON.stringify({...parent, output_contracts: child.output_contracts});
assert.ok(validateStrategy(cycle).some(issue => issue.message.includes("cycle")));
const historical = cloneStrategy(parent);
const older = {...cloneStrategy(parent), output_contracts: child.output_contracts};
historical.resources![0].feeder!.revision = "r2";
historical.resources![0].feeder!.document_json = JSON.stringify(older);
assert.equal(validateStrategy(historical).some(issue => issue.message.includes("cycle")), false);
const conflict = cloneStrategy(parent);
conflict.resources!.push({...cloneStrategy(parent).resources![0], id: "other"});
conflict.resources![1].feeder!.document_json = JSON.stringify(child);
assert.ok(validateStrategy(conflict).some(issue => issue.message.includes("conflicting")));
const selfMove = cloneStrategy(parent);
selfMove.nodes[1].operation = {type: "move_resource", params: {from: "feeder", to: "feeder"}};
assert.ok(validateStrategy(selfMove).some(issue => issue.message.includes("distinct")));
assert.notEqual(strategyStructuralSignature(parent), strategyStructuralSignature(edited));
const recombination = cloneStrategy(parent);
recombination.nodes[1].operation = {type: "recombination", params: {input_a: "feeder", input_b: "current", output: "feeder"}};
assert.equal(validateStrategy(recombination).some(issue => issue.severity === "error"), false);
assert.deepEqual(strategyResourcePorts(recombination.nodes[1]), {inputs: ["feeder", "current"], outputs: ["feeder"]});
assert.throws(() => pinStrategyFeeder("x", saved, "missing"));
console.log("Strategy feeder pinning, output contracts, item ports and history checks passed.");

// Typed supply is persisted separately from sequential execution. A blank path
// gains a visible Start entry in the same history edit; an authored path stays intact.
{
    const graph = cloneStrategy(parent);
    graph.resources!.push({id: "donor", base_state: {...graph.base_state, rarity: "rare"}, acquisition_price_key: "resource:donor"},
        {id: "out", base_state: graph.base_state, acquisition_price_key: "resource:out"});
    graph.nodes = [{id: "start", kind: "start", position: {x: 0, y: 0}},
        {id: "feeder", kind: "operation", source_only: true, operation: {type: "invoke_feeder", params: {resource_id: "feeder"}}, position: {x: 1, y: 0}},
        {id: "donor", kind: "operation", source_only: true, operation: {type: "acquire_resource", params: {resource_id: "donor"}}, position: {x: 1, y: 1}},
        {id: "pair", kind: "operation", operation: {type: "recombination", params: {input_a: "current", input_b: "", output: "out"}}, position: {x: 2, y: 0}},
        {id: "end", kind: "terminal", terminal: "success", position: {x: 3, y: 0}}];
    graph.edges = [];
    const [start, feeder, donor, pair, end] = graph.nodes;
    assert.deepEqual(strategyNodeConnectors(pair).inputs.map(port => port.id), ["input_a", "input_b"]);
    assert.equal(strategyNodeConnectors(pair).outputs.length, 1);
    for (const source of [feeder, donor]) {
        assert.equal(strategyNodeConnectors(source).inputs.length, 0);
        assert.equal(strategyNodeConnectors(source).outputs.length, 1);
        assert.throws(() => strategyConnection(graph, start.id, source.id));
    }
    const supplyA = {id: "supply-a", from: feeder.id, to: pair.id, priority: 0, ...strategyConnection(graph, feeder.id, pair.id, "input_a")};
    graph.edges.push(supplyA);
    const entry = strategySourceEntryEdge(graph, supplyA)!;
    assert.equal(entry.kind, "control"); assert.equal(entry.to_port, "input_a");
    graph.edges.push(entry);
    const supplyB = {id: "supply-b", from: donor.id, to: pair.id, priority: 0, ...strategyConnection(graph, donor.id, pair.id, "input_b")};
    graph.edges.push(supplyB, {id: "done", from: pair.id, to: end.id, priority: 0, ...strategyConnection(graph, pair.id, end.id), condition: {type: "base_is", base_key: graph.base_state.base_key}});
    assert.equal(strategySourceEntryEdge(graph, supplyB), undefined);
    assert.deepEqual(strategyRecombinationBindings(graph, pair), {input_a: "feeder", input_b: "donor", output: "out"});
    assert.deepEqual(strategyResourcePorts(pair, graph), {inputs: ["feeder", "donor"], outputs: ["out"]});
    assert.deepEqual(validateStrategy(graph), []);
    assert.throws(() => strategyConnection(graph, start.id, pair.id, "input_b"));
    assert.throws(() => strategyConnection(graph, feeder.id, end.id));
    const reconnect = cloneStrategy(graph);
    Object.assign(reconnect.edges.find(edge => edge.id === "supply-b")!, strategyConnection(reconnect, donor.id, pair.id, "input_a"));
    assert.ok(validateStrategy(reconnect).some(issue => issue.message.includes("only one supply")));
    const broken = cloneStrategy(graph);
    broken.edges.find(edge => edge.id === "supply-a")!.condition = {type: "rarity_is", rarity: "rare"};
    assert.ok(validateStrategy(broken).some(issue => issue.code === "item-connection"));
    const invalidConnector = cloneStrategy(graph);
    invalidConnector.edges.find(edge => edge.id === "done")!.to_port = "unknown";
    assert.ok(validateStrategy(invalidConnector).some(issue => issue.code === "connector"));
    assert.notEqual(strategyStructuralSignature(graph), strategyStructuralSignature(reconnect));
    const outputEdit = cloneStrategy(graph);
    outputEdit.output_contracts = [{id: "pair-output", resource_id: "out", base_key: graph.base_state.base_key, predicate: {type: "rarity_is", rarity: "rare"}}];
    assert.notEqual(strategyStructuralSignature(graph), strategyStructuralSignature(outputEdit));
    const edits = new EditHistory<StrategyDocument>(); edits.reset(graph); edits.record(outputEdit);
    const imported = new EditHistory<StrategyDocument>(); imported.restore(JSON.parse(JSON.stringify(edits.export())), outputEdit);
    assert.deepEqual(imported.go(0)!.edges, graph.edges);
    assert.equal(imported.go(1)!.output_contracts![0].resource_id, "out");
    assert.deepEqual(JSON.parse(JSON.stringify(graph)), graph);
}
console.log("Builder two-input/one-output connectors, paid supply, reconnection and retained history checks passed.");
