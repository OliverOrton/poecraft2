import assert from "node:assert/strict";

import type { SolverActionInfo } from "../src/app/engine-protocol";
import {
    incompleteSolveDetail,
    prepareSolverStrategy,
    pricedSolverActionIds,
    solvePriceReadiness,
} from "../src/app/solve-workspace";
import { createDefaultStrategy } from "../src/app/strategy-model";

{
    assert.equal(
        incompleteSolveDetail({
            optimization: {
                full_request_status: "incomplete_resource_cap",
                cap_hits: ["max_transitions"],
            },
        }),
        "The exact solve reached the max_transitions resource boundary before it could produce a complete policy. This is a solver-capacity limit, not a pricing error; the incomplete policy was not compiled.",
    );
    assert.equal(
        incompleteSolveDetail(null),
        "The native optimizer did not produce a complete policy. The incomplete policy was not compiled.",
    );
    console.log("  ok - incomplete solves report their native resource boundary");
}

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
    assert.equal(readiness.missingFractureBasePrice, false);
    assert.deepEqual(pricedSolverActionIds(actions, (key) => prices.get(key)), [
        "chaos",
        "free-transition",
    ]);
    console.log("  ok - Solve readiness uses native cost keys and shared prices");
}

{
    const actions: SolverActionInfo[] = [
        action("restart", ["base"]),
        action("fracture", ["fracture"]),
        action("chaos", ["chaos"]),
    ];
    const prices = new Map<string, number>([
        ["fracture", 25],
        ["chaos", 1],
    ]);
    const missingBase = solvePriceReadiness(actions, (key) => prices.get(key));
    assert.equal(missingBase.missingFractureBasePrice, true);
    prices.set("base", 10);
    const ready = solvePriceReadiness(actions, (key) => prices.get(key));
    assert.equal(ready.missingFractureBasePrice, false);
    console.log("  ok - priced Fracture requires an actionable Restart base price");
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
