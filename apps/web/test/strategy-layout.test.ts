import assert from "node:assert/strict";

import {
    autoLayoutStrategy,
    ensureStrategyPositions,
    hasStrategyPositions,
} from "../src/app/strategy-layout";
import type {
    StrategyDocument,
    StrategyEdge,
    StrategyNode,
} from "../src/app/strategy-model";

function chainStrategy(length: number): StrategyDocument {
    const nodes: StrategyNode[] = [];
    const edges: StrategyEdge[] = [];
    nodes.push({ id: "start", kind: "start", name: "Start" } as StrategyNode);
    let previous = "start";
    for (let i = 0; i < length; i += 1) {
        const id = `op-${i}`;
        nodes.push({
            id,
            kind: "operation",
            name: "",
            operation: { type: "chaos", params: {} },
        } as StrategyNode);
        edges.push({
            id: `edge-${i}`,
            from: previous,
            to: id,
            priority: 0,
            condition: { type: "always" },
            label: "",
        });
        previous = id;
    }
    nodes.push({
        id: "done",
        kind: "terminal",
        name: "Success",
        terminal: "success",
        reason: "",
    } as StrategyNode);
    edges.push({
        id: "edge-done",
        from: previous,
        to: "done",
        priority: 0,
        condition: { type: "always" },
        label: "",
    });
    return {
        version: "v1",
        name: "chain",
        description: "",
        start_node_id: "start",
        base_state: { base_key: "Metadata/Items/Test", item_level: 86 },
        nodes,
        edges,
    } as StrategyDocument;
}

{
    // Solver-compiled policies can chain thousands of nodes deep. The layout
    // DFS must be iterative (no call-stack overflow) and the barycenter
    // sweeps near-linear (the old per-column full rescan made deep chains
    // quadratic and froze the page).
    const strategy = chainStrategy(30_000);
    const startedAt = performance.now();
    const applied = ensureStrategyPositions(strategy);
    const elapsed = performance.now() - startedAt;
    assert.equal(applied, true);
    assert.equal(hasStrategyPositions(strategy), true);
    for (const node of strategy.nodes) {
        assert.ok(Number.isFinite(node.position.x));
        assert.ok(Number.isFinite(node.position.y));
    }
    assert.ok(
        elapsed < 10_000,
        `deep-chain layout took ${Math.round(elapsed)}ms; expected well under 10s`,
    );
    console.log(
        `  ok - 30k-node chain lays out iteratively in ${Math.round(elapsed)}ms`,
    );
}

{
    // Policy-shaped graph: wide columns, retry back edges, and self loops.
    const strategy = chainStrategy(2_000);
    for (let i = 40; i < 2_000; i += 40) {
        strategy.edges.push({
            id: `retry-${i}`,
            from: `op-${i}`,
            to: `op-${i - 40}`,
            priority: 1,
            condition: { type: "always" },
            label: "",
            is_default: true,
        } as StrategyEdge);
    }
    for (let i = 20; i < 2_000; i += 100) {
        strategy.edges.push({
            id: `loop-${i}`,
            from: `op-${i}`,
            to: `op-${i}`,
            priority: 2,
            condition: { type: "always" },
            label: "",
        });
    }
    autoLayoutStrategy(strategy);
    assert.equal(hasStrategyPositions(strategy), true);
    const columnX = new Set(strategy.nodes.map((node) => node.position.x));
    assert.ok(columnX.size > 1, "layered layout should produce columns");
    console.log("  ok - cyclic policy-shaped graph lays out with columns");
}

{
    // Layout must remain a no-op when every node already has a position.
    const strategy = chainStrategy(3);
    strategy.nodes.forEach((node, index) => {
        node.position = { x: index * 100, y: 50 };
    });
    assert.equal(ensureStrategyPositions(strategy), false);
    assert.equal(strategy.nodes[1].position.x, 100);
    console.log("  ok - existing positions are preserved");
}
