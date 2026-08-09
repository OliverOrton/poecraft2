import assert from "node:assert/strict";

import type { SolveSummary } from "../src/app/engine-protocol";
import {
    certifiedFactorLabel,
    shouldCompileSolvePolicy,
    calculatorSolveOptions,
    solveResultMarkup,
    solveTerminationDetail,
} from "../src/app/solver-result-presentation";

{
    const summary = solveSummary({
        converged: false,
        policy_available: true,
        policy_status: "bounded_near_optimal",
        termination: "target_gap",
        lower_bound: 100,
        upper_bound: 110,
        evaluated_policy_cost: 109,
        absolute_optimality_gap: 10,
        relative_optimality_gap: 0.1,
        requested_relative_optimality_gap: 0.1,
        target_met: true,
        target_fired: "relative",
    });
    assert.equal(shouldCompileSolvePolicy(summary), true);
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["chaos", "bench:<unsafe>"],
        excludedActions: 3,
        economyLabel: "Settlers & pinned",
        terminationDetail: solveTerminationDetail(summary, null),
        hasCompiledStrategy: true,
        busy: false,
        verification: null,
        telemetry: {
            incremental_action_envelope: {
                remaining_action_envelope: 2,
            },
        },
    });
    assert.match(markup, /data-policy-status="bounded_near_optimal"/);
    assert.match(markup, /Returned policy expected cost/);
    assert.match(markup, /Optimal-cost lower bound/);
    assert.match(markup, /Certified policy upper bound/);
    assert.match(markup, /Absolute optimality gap/);
    assert.match(markup, /Certified multiplicative factor/);
    assert.match(markup, /Certified within 1\.10x of optimal\./);
    assert.match(markup, /At most 10% more expensive than optimal\./);
    assert.match(markup, /Relative-gap criterion fired/);
    assert.match(markup, /Settlers &amp; pinned/);
    assert.match(markup, /bench:&lt;unsafe&gt;/);
    assert.match(markup, /Open in Strategy Board/);
    assert.match(markup, /Verify 10,000 runs/);
    assert.match(markup, /Product goal-relevant action scope/);
    assert.match(markup, /Zero-progress outcomes retry through destructive reforges only/);
    assert.match(markup, /Admitted priced families: Bench 1 \/ Currency \/ structural 1/);
    assert.match(markup, /Explicitly deferred: Veiled crafting/);
    assert.match(markup, /2 action obligations remained unresolved/);
    assert.doesNotMatch(markup, /is 10% suboptimal/i);
    assert.doesNotMatch(markup, /upper bound[^<]*optimum/i);
    assert.doesNotMatch(markup, /Did not converge/);
    console.log("  ok - bounded target results use the approved certificate DOM");
}

{
    assert.deepEqual(calculatorSolveOptions(5, 10), {
        goal_progress_gated_reforges: true,
        max_absolute_optimality_gap: 5,
        max_relative_optimality_gap: 0.1,
    });
    assert.deepEqual(calculatorSolveOptions(0, -1), {
        goal_progress_gated_reforges: true,
    });
    assert.deepEqual(calculatorSolveOptions(Number.NaN, 0), {
        goal_progress_gated_reforges: true,
    });
    console.log("  ok - Calculator solves always opt into gated reforges and map optional targets");
}

{
    const summary = solveSummary({
        converged: false,
        policy_available: true,
        policy_status: "bounded_feasible",
        termination: "refused_resource_cap",
        stop_cause: "transition_cap",
        cap_hit_mask: 2,
        lower_bound: 40,
        upper_bound: 200,
        evaluated_policy_cost: 175,
        absolute_optimality_gap: 160,
        relative_optimality_gap: 4,
    });
    const detail = solveTerminationDetail(summary, {
        optimization: { cap_hits: ["max_transitions"] },
    });
    assert.match(detail, /resource cap \(max_transitions\)/);
    assert.match(detail, /returned executable policy remains bounded/);
    assert.equal(shouldCompileSolvePolicy(summary), true);
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["chaos"],
        excludedActions: 0,
        economyLabel: "Test economy",
        terminationDetail: detail,
        hasCompiledStrategy: true,
        busy: false,
        verification: null,
    });
    assert.match(markup, /Bounded feasible policy/);
    assert.match(markup, /Resource cap reached/);
    assert.match(markup, /Transition cap/);
    assert.match(markup, /Certified within 5\.00x of optimal\./);
    console.log("  ok - capped bounded policies stay executable and non-exact");
}

{
    const summary = solveSummary({
        converged: false,
        policy_available: false,
        policy_status: "none",
        termination: "refused_resource_cap",
        stop_cause: "state_cap",
        cap_hit_mask: 1,
        lower_bound: 25,
    });
    assert.equal(shouldCompileSolvePolicy(summary), false);
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: [],
        excludedActions: 0,
        economyLabel: null,
        terminationDetail: solveTerminationDetail(summary, null),
        hasCompiledStrategy: false,
        busy: false,
        verification: null,
    });
    assert.match(markup, /No executable policy certificate was returned/);
    assert.match(markup, /before it could certify an executable policy/i);
    assert.match(markup, /No policy returned/);
    assert.match(markup, /Not certified/);
    assert.match(markup, /No economy selected/);
    assert.match(markup, /State cap/);
    assert.doesNotMatch(markup, /Open in Strategy Board/);
    assert.doesNotMatch(markup, />Unavailable</);
    console.log("  ok - capped no-policy results retain their stopping cause");
}

{
    const summary = solveSummary({
        policy_available: false,
        policy_status: "none",
        termination: "refused_resource_cap",
        stop_cause: "memory_cap",
    });
    const detail = solveTerminationDetail(summary, {
        policy_refinement: {
            core_policy: {
                candidate_present: true,
                status: "exact",
            },
            direct_certification: {
                status: "resource_cap",
                resource_cap: "max_discovered_states",
                failure_reason: "solver exceeded max_discovered_states (200000)",
            },
            strict_lift: {
                status: "resource_cap",
                resource_cap: "max_estimated_memory_bytes",
                failure_reason: "replay-backed closed partition reached memory cap",
            },
            publication: {
                status: "none",
            },
        },
    });
    assert.match(detail, /^Core policy found;/);
    assert.match(detail, /direct certification reached resource cap \(max_discovered_states\)/);
    assert.match(detail, /strict refinement reached resource cap \(max_estimated_memory_bytes\)/);
    assert.match(detail, /No executable policy was published/);
    assert.doesNotMatch(detail, /solver found no policy/i);
    console.log("  ok - retained core candidates report the certification stage that stopped publication");
}

{
    const summary = solveSummary({
        converged: false,
        policy_available: true,
        policy_status: "bounded_feasible",
        termination: "exact_closed",
        stop_cause: "exact_closed",
        start_value: 125,
        lower_bound: 0,
        upper_bound: 125,
        evaluated_policy_cost: 125,
        absolute_optimality_gap: 125,
        relative_optimality_gap: null,
    });
    const detail = solveTerminationDetail(summary, null);
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["regal"],
        excludedActions: 0,
        economyLabel: "Test economy",
        terminationDetail: detail,
        hasCompiledStrategy: true,
        busy: false,
        verification: null,
    });
    assert.match(markup, /Bounded feasible policy/);
    assert.match(markup, /Coarse discovery closed/);
    assert.match(detail, /exact state refinement/i);
    assert.doesNotMatch(markup, /Exact proof closed/);
    assert.doesNotMatch(markup, /Exact optimum certified/);
    console.log("  ok - refined bounded policies do not inherit a coarse exactness claim");
}

{
    const summary = solveSummary({
        converged: true,
        policy_available: true,
        policy_status: "exact",
        termination: "exact_closed",
        start_value: 230.26738656962243,
        lower_bound: 230.26738656962243,
        upper_bound: 230.26738656962243,
        evaluated_policy_cost: 230.26738656962243,
        absolute_optimality_gap: 0,
        relative_optimality_gap: 0,
    });
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["chaos"],
        excludedActions: 0,
        economyLabel: "Test economy",
        terminationDetail: solveTerminationDetail(summary, null),
        hasCompiledStrategy: true,
        busy: false,
        verification: null,
    });
    assert.match(markup, /Exact optimal policy/);
    assert.match(markup, /Exact optimum certified\./);
    assert.match(markup, /Exact proof closed/);
    assert.equal(certifiedFactorLabel(0.104), "1.11x");
    console.log("  ok - exact results remain exact and factors round conservatively");
}

function solveSummary(overrides: Partial<SolveSummary>): SolveSummary {
    return {
        converged: false,
        start_state: 0,
        start_value: null,
        expanded_states: 123,
        sweeps: 4,
        residual: 0.001,
        skipped_actions: 0,
        stop_cause: "none",
        cap_hit_mask: 0,
        registry_actions: 11,
        candidate_actions: 8,
        evaluator_supported_actions: 7,
        supported_priced_actions: 6,
        skipped_missing_price_actions: 1,
        skipped_unsupported_actions: 1,
        policy_available: false,
        policy_status: "none",
        termination: "none",
        lower_bound: null,
        upper_bound: null,
        evaluated_policy_cost: null,
        absolute_optimality_gap: null,
        relative_optimality_gap: null,
        requested_absolute_optimality_gap: 0,
        requested_relative_optimality_gap: 0,
        target_met: false,
        target_fired: "none",
        ...overrides,
    };
}
