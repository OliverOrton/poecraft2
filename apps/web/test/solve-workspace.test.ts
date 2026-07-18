import assert from "node:assert/strict";

import type { SolverActionInfo } from "../src/app/engine-protocol";
import {
    prepareSolverStrategy,
    pricedSolverActionIds,
    solvePriceReadiness,
} from "../src/app/solve-workspace";
import { createDefaultStrategy } from "../src/app/strategy-model";

{
    const actions: SolverActionInfo[] = [
        action("chaos", ["chaos"]),
        action("restart", ["base"]),
        action("free-transition", []),
        action("fossil:test", ["fossil:test", "resonator:1"]),
    ];
    const prices = new Map<string, number>([
        ["chaos", 1],
        ["fossil:test", 2],
    ]);
    const readiness = solvePriceReadiness(actions, (key) => prices.get(key));
    assert.equal(readiness.totalActions, 4);
    assert.equal(readiness.pricedActions, 2);
    assert.deepEqual(readiness.costKeys, [
        "base",
        "chaos",
        "fossil:test",
        "resonator:1",
    ]);
    assert.deepEqual(readiness.missingKeys, ["base", "resonator:1"]);
    assert.deepEqual(pricedSolverActionIds(actions, (key) => prices.get(key)), [
        "chaos",
        "free-transition",
    ]);
    console.log("  ok - Solve readiness uses native cost keys and shared prices");
}

{
    const compiled = createDefaultStrategy();
    compiled.nodes[1].expected_cost = 2.9319;
    for (const node of compiled.nodes) {
        delete (node as { position?: { x: number; y: number } }).position;
    }
    const prepared = prepareSolverStrategy(compiled);
    assert.ok(
        prepared.nodes.every(
            (node) =>
                Number.isFinite(node.position.x) &&
                Number.isFinite(node.position.y),
        ),
    );
    assert.equal(prepared.nodes[1].expected_cost, 2.9319);
    assert.notStrictEqual(prepared, compiled);
    console.log("  ok - compiled policies auto-layout without losing solver values");
}

function action(id: string, costKeys: string[]): SolverActionInfo {
    return {
        index: 0,
        id,
        display_name: id,
        transition_kind: 0,
        synthetic: false,
        cost_keys: costKeys,
        preservation: {
            can_preserve: [],
            can_destroy: [],
            can_create: [],
            can_make_unreachable: [],
            destructive_renewal: false,
            preserves_fractured_affixes: false,
            protection: {
                prefix_lock: false,
                suffix_lock: false,
                cannot_roll_attack: false,
                cannot_roll_caster: false,
            },
        },
    };
}
