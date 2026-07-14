import assert from "node:assert/strict";

import { StrategyEvalResult } from "../src/app/engine-protocol";
import {
    buildStrategyBoardAnnotations,
    normalizeStrategyBuilderMode,
    strategyEvalRefusalMarkup,
    strategyStructuralSignature,
} from "../src/app/strategy-eval-presentation";
import { createDefaultStrategy } from "../src/app/strategy-model";
import { StrategyDraftRecord } from "../src/app/workspace/persistence";

{
    const legacy: StrategyDraftRecord = {
        docId: "legacy",
        strategy: null,
        sourceItem: null,
        savedRef: null,
        savedName: null,
        dirty: true,
        updatedAt: 1,
    };
    const calculator: StrategyDraftRecord = {
        ...legacy,
        docId: "calculator",
        builderMode: "calculator",
    };
    assert.equal(normalizeStrategyBuilderMode(legacy.builderMode), "simulator");
    assert.equal(
        normalizeStrategyBuilderMode(calculator.builderMode),
        "calculator",
    );
    console.log("  ok - calculator mode persists and legacy drafts use Simulator");
}

{
    const strategy = createDefaultStrategy();
    const original = strategyStructuralSignature(strategy);

    strategy.nodes[1].position.x += 220;
    strategy.ui = { viewport: { panX: 91, panY: 42, zoom: 0.8 } };
    strategy.nodes[1].name = "Display-only node name";
    strategy.edges[0].label = "Display-only edge label";
    strategy.name = "Display-only strategy name";
    assert.equal(
        strategyStructuralSignature(strategy),
        original,
        "drag, viewport and display labels must not queue an evaluation",
    );

    if (strategy.nodes[1].kind !== "operation") {
        throw new Error("expected the default operation node");
    }
    strategy.nodes[1].operation = { type: "alchemy", params: {} };
    assert.notEqual(
        strategyStructuralSignature(strategy),
        original,
        "operation changes must queue an evaluation",
    );
    console.log("  ok - structural edits rerun evaluation but node drags do not");
}

{
    const strategy = createDefaultStrategy();
    const result = cannedResult();
    const annotations = buildStrategyBoardAnnotations(strategy, result, true);
    assert.equal(annotations.stale, true);
    assert.equal(annotations.nodeBadges.get("chaos")?.label, "2.50×");
    assert.equal(annotations.nodeBadges.get("success")?.label, "75%");
    assert.equal(annotations.edgeLabels.get("begin")?.label, "100%");
    assert.equal(annotations.edgeLabels.get("done")?.label, "30%");
    assert.match(
        annotations.edgeLabels.get("repeat")?.title ?? "",
        /^1\.75 expected traversals/,
    );
    console.log("  ok - canned exact results render node and edge annotations");
}

{
    const refusal = {
        code: 4,
        detail:
            "strategy evaluation unsupported:\n" +
            "- node 'veiled-reforge' operation '<veiled_chaos>' has no exact calculator evaluator",
    };
    const markup = strategyEvalRefusalMarkup(refusal);
    assert.match(
        markup,
        /This strategy uses actions or conditions Calculator mode cannot evaluate exactly yet\./,
    );
    assert.ok(markup.includes("poecraft engine error 4"));
    assert.ok(markup.includes("&lt;veiled_chaos&gt;"));
    assert.ok(!markup.includes("<veiled_chaos>"));
    console.log("  ok - evaluator refusal text is preserved and safely rendered");
}

for (const code of [5, 8]) {
    const markup = strategyEvalRefusalMarkup({
        code,
        detail:
            code === 5
                ? "std::bad_alloc"
                : "strategy evaluation exceeded max_transitions (1)",
    });
    assert.match(
        markup,
        /This strategy is too large to evaluate exactly with the current limits\./,
    );
    assert.doesNotMatch(
        markup,
        /uses actions or conditions Calculator mode cannot evaluate/,
    );
    assert.equal(
        markup.match(new RegExp(`poecraft engine error ${code}`, "g"))?.length,
        1,
    );
}
console.log("  ok - capacity and allocation failures use honest exact-size copy");

function cannedResult(): StrategyEvalResult {
    return {
        version: "v1",
        converged: true,
        sweeps: 19,
        residual_mass: 0,
        terminals: {
            success: 0.75,
            failure: 0.25,
            stop: 0,
            action_not_applied: 0,
            no_matching_edge: 0,
            unresolved: 0,
            by_node: [
                { node_id: "success", kind: "success", p: 0.75 },
                { node_id: "failure", kind: "failure", p: 0.25 },
            ],
        },
        unresolved_by_node: [],
        failures_by_node: [],
        expected_actions: 2.5,
        expected_consumption: [{ key: "chaos", quantity: 2.5 }],
        targets: [{ kind: "group", group_id: 1 }],
        nodes: [
            {
                id: "chaos",
                expected_visits: 2.5,
                classes: [
                    {
                        share: 1,
                        rarity: 2,
                        prefixes: 3,
                        suffixes: 2,
                        flags: 0,
                        blocked: 0,
                        slots: [0],
                    },
                ],
                classes_truncated_share: 0,
            },
        ],
        edges: [
            { id: "begin", expected_traversals: 1 },
            { id: "done", expected_traversals: 0.75 },
            { id: "repeat", expected_traversals: 1.75 },
            { id: "fail", expected_traversals: 0.25 },
        ],
    };
}
