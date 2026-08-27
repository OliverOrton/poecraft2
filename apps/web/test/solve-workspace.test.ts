import assert from "node:assert/strict";

import type { SolverActionInfo } from "../src/app/engine-protocol";
import {
    buildCalculatorSolverGoal,
    incompleteSolveDetail,
    prepareSolverStrategy,
    pricedSolverActionIds,
    solvePriceReadiness,
} from "../src/app/solve-workspace";
import { createDefaultStrategy } from "../src/app/strategy-model";

{
    const fields = {
        rarity: "rare" as const,
        minSatisfiedSlots: 1,
        slots: [{ family_mod_key: "goal-life", min_tier: 1 }],
    };
    const selectedFossil = "fossil:lucent";
    const automaticActions = [action("chaos", ["chaos"])];
    const requestedFossil = action(selectedFossil, [
        "fossil:lucent",
        "resonator:1",
    ]);
    const envelopeActions = (requested: readonly string[] | undefined) =>
        requested?.includes(selectedFossil)
            ? [...automaticActions, requestedFossil]
            : automaticActions;
    const price = (key: string): number | undefined =>
        new Map([
            ["chaos", 1],
            ["fossil:lucent", 2],
            ["resonator:1", 1],
        ]).get(key);

    const fossilEnvelope = buildCalculatorSolverGoal(
        fields,
        "product_envelope",
        selectedFossil,
    );
    const chaosEnvelope = buildCalculatorSolverGoal(
        fields,
        "product_envelope",
        "chaos",
    );
    assert.deepEqual(
        fossilEnvelope,
        chaosEnvelope,
        "the inspected odds action must not change the product envelope",
    );
    const fossilCandidateIds = pricedSolverActionIds(
        envelopeActions(fossilEnvelope.requested_fossil_actions),
        price,
    );
    const chaosCandidateIds = pricedSolverActionIds(
        envelopeActions(chaosEnvelope.requested_fossil_actions),
        price,
    );
    assert.deepEqual(fossilCandidateIds, chaosCandidateIds);
    assert.deepEqual(
        buildCalculatorSolverGoal(
            fields,
            "scoped_solve",
            selectedFossil,
            fossilCandidateIds,
        ),
        buildCalculatorSolverGoal(
            fields,
            "scoped_solve",
            "chaos",
            chaosCandidateIds,
        ),
        "the complete serialized Solve goal must be odds-selection invariant",
    );
    assert.deepEqual(
        buildCalculatorSolverGoal(fields, "odds", selectedFossil)
            .requested_fossil_actions,
        [selectedFossil],
        "the exact odds handle still materializes a selected Fossil",
    );
    assert.deepEqual(
        buildCalculatorSolverGoal(
            fields,
            "product_envelope",
            "chaos",
            undefined,
            ["temporary_bench", "harvest"],
        ).disabled_action_families,
        ["temporary_bench", "harvest"],
    );
    assert.equal(
        buildCalculatorSolverGoal(
            fields,
            "odds",
            "chaos",
            undefined,
            ["temporary_bench"],
        ).disabled_action_families,
        undefined,
        "Solve diagnostics must not narrow exact single-action odds",
    );
    console.log(
        "  ok - Calculator odds selection is isolated from the product Solve envelope",
    );
}

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
    compiled.edges[1].condition = {
        type: "observation_signature",
        version: 1,
        requirement: {
            item_features: 1,
            modifier_tag_ids: [],
            affix_observations: [],
        },
        signature: [
            {
                feature: 0,
                subject: 0,
                value: ["0000000000000002"],
            },
        ],
        goal_status_tier_class_by_mod: [],
        count_observation_count: 0,
        count_observation_membership_by_mod: [],
    };
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
    assert.equal(prepared.edges[1].condition?.type, "observation_signature");
    assert.deepEqual(prepared.edges[1].condition?.signature, [
        {
            feature: 0,
            subject: 0,
            value: ["0000000000000002"],
        },
    ]);
    assert.strictEqual(prepared, compiled);
    console.log(
        "  ok - uniquely transferred policies preserve opaque exact routers and auto-layout without a full clone",
    );
}

function action(id: string, costKeys: string[]): SolverActionInfo {
    return {
        index: 0,
        id,
        display_name: id,
        family: id === "restart"
            ? "restart"
            : id.startsWith("fossil:")
              ? "fossil"
              : id === "fracture"
                ? "fracture"
                : "currency",
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
