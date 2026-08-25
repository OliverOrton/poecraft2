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
        policy_status: "bounded_feasible",
        termination: "requested_bounded_finish",
        stop_cause: "requested_bounded_finish",
        lower_bound: 10,
        upper_bound: 2000,
        evaluated_policy_cost: 1900,
        absolute_optimality_gap: 1990,
        relative_optimality_gap: 199,
    });
    assert.equal(shouldCompileSolvePolicy(summary), true);
    assert.match(
        solveTerminationDetail(summary, null),
        /product discovery budget/i,
    );
}

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
        missingPriceKeys: ["currency:<missing>"],
        economyLabel: "Settlers & pinned",
        terminationDetail: solveTerminationDetail(summary, null),
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        considerImprintPrograms: false,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["chaos"],
        busy: false,
        verification: null,
        telemetry: {
            incremental_action_envelope: {
                enabled: true,
                closed: false,
                remaining_action_envelope: 2,
                actions: {
                    unevaluated: 1,
                    evaluating: 0,
                    unresolved: 1,
                },
            },
            action_control: {
                automatic_candidates: {
                    enabled: true,
                    by_kind: {
                        eldritch_side: {
                            eligible: 2,
                            rows: 2,
                            deferred: 1,
                            missing_price: 1,
                        },
                        veiled: {
                            eligible: 1,
                            rows: 1,
                            missing_price: 0,
                        },
                        cannot_roll: {
                            eligible: 1,
                            rows: 0,
                            missing_price: 0,
                        },
                    },
                },
            },
        },
    });
    assert.match(markup, /data-policy-status="bounded_near_optimal"/);
    assert.match(markup, /data-policy-available="true"/);
    assert.match(markup, /data-exact-authority="false"/);
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
    assert.match(markup, /Product goal-relevant, supported priced action scope/);
    assert.match(markup, /Goal-progress-gated reforges enabled: zero-progress outcomes retry through destructive reforges only/);
    assert.match(markup, /Admitted priced primitive families: Bench 1 \/ Currency \/ structural 1/);
    assert.match(markup, /Admitted automatic families: Cannot Roll 1 eligible candidate, 0 completed rows \/ Eldritch Side 2 eligible candidates, 2 completed rows \/ Veiled 1 eligible candidate, 1 completed row/);
    assert.match(markup, /Automatic candidates rejected for missing prices: Eldritch Side 1/);
    assert.match(markup, /Automatic candidates left deferred: Eldritch Side 1/);
    assert.match(markup, /Pre-solve primitive missing-price identity/);
    assert.match(markup, /2 action obligations remained open when the solve stopped \(1 unevaluated, 0 evaluating, 1 unresolved\)/);
    assert.match(markup, /currency:&lt;missing&gt;/);
    assert.match(markup, /Chaos-only policy authority: executable incumbent only/);
    assert.doesNotMatch(markup, /Explicitly deferred: Veiled crafting/);
    assert.doesNotMatch(markup, /is 10% suboptimal/i);
    assert.doesNotMatch(markup, /upper bound[^<]*optimum/i);
    assert.doesNotMatch(markup, /Did not converge/);
    assert.match(markup, /Automatic Imprint checkpoint\/retry programs are excluded by caller scope/);
    assert.match(markup, /data-consider-imprint-programs="false"/);
    console.log("  ok - bounded target results use the approved certificate DOM");
}

{
    assert.deepEqual(calculatorSolveOptions(5, 10), {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: false,
        consider_imprint_programs: false,
        max_policy_refinement_states: 200_000,
        max_absolute_optimality_gap: 5,
        max_relative_optimality_gap: 0.1,
    });
    assert.deepEqual(calculatorSolveOptions(0, -1), {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: false,
        consider_imprint_programs: false,
        max_policy_refinement_states: 200_000,
    });
    assert.deepEqual(calculatorSolveOptions(Number.NaN, 0), {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: false,
        consider_imprint_programs: false,
        max_policy_refinement_states: 200_000,
    });
    assert.deepEqual(calculatorSolveOptions(0, 0, true), {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: true,
        consider_imprint_programs: false,
        max_policy_refinement_states: 200_000,
    });
    assert.deepEqual(calculatorSolveOptions(0, 0, false, false), {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: false,
        consider_imprint_programs: false,
        max_policy_refinement_states: 200_000,
    });
    assert.deepEqual(calculatorSolveOptions(0, 0, false, true), {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: false,
        consider_imprint_programs: true,
        max_policy_refinement_states: 200_000,
    });
    console.log("  ok - Calculator solves use exact operator-major scheduling and map optional targets");
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
        missingPriceKeys: [],
        economyLabel: "Test economy",
        terminationDetail: detail,
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["chaos"],
        busy: false,
        verification: null,
        telemetry: {
            optimization: { cap_hits: ["max_transitions"] },
            incremental_action_envelope: {
                enabled: true,
                closed: false,
                actions: {
                    unevaluated: 3,
                    evaluating: 1,
                    unresolved: 2,
                },
            },
        },
    });
    assert.match(markup, /Bounded feasible policy/);
    assert.match(markup, /Resource cap reached/);
    assert.match(markup, /Transition cap/);
    assert.match(markup, /max_transitions/);
    assert.match(markup, /Certified within 5\.00x of optimal\./);
    assert.match(markup, /6 action obligations remained open/);
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
        missingPriceKeys: [],
        economyLabel: null,
        terminationDetail: solveTerminationDetail(summary, null),
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: false,
        compiledOperationTypes: [],
        busy: false,
        verification: null,
    });
    assert.match(markup, /No executable policy certificate was returned/);
    assert.match(markup, /before it could certify an executable policy/i);
    assert.match(markup, /No executable policy/);
    assert.match(markup, /Numeric search bounds cannot authorize compilation/);
    assert.match(markup, /Unavailable: no executable policy/);
    assert.match(markup, /No economy selected/);
    assert.match(markup, /State cap/);
    assert.doesNotMatch(markup, /Open in Strategy Board/);
    assert.doesNotMatch(markup, />Unavailable</);
    console.log("  ok - capped no-policy results retain their stopping cause");
}

{
    const summary = solveSummary({
        converged: true,
        policy_available: false,
        policy_status: "exact",
        termination: "exact_closed",
        stop_cause: "exact_closed",
        start_value: 42,
        lower_bound: 42,
        upper_bound: 42,
        evaluated_policy_cost: 42,
        absolute_optimality_gap: 0,
        relative_optimality_gap: 0,
    });
    assert.equal(shouldCompileSolvePolicy(summary), false);
    const telemetry = {
        incremental_action_envelope: {
            enabled: true,
            closed: true,
            actions: {
                unevaluated: 0,
                evaluating: 0,
                unresolved: 0,
            },
        },
    };
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["chaos"],
        excludedActions: 0,
        missingPriceKeys: [],
        economyLabel: "Test economy",
        terminationDetail: solveTerminationDetail(summary, telemetry),
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["chaos"],
        busy: false,
        verification: {
            completedRuns: 10_000,
            empiricalCost: 42,
            delta: 0,
        },
        telemetry,
    });
    assert.match(markup, /data-policy-available="false"/);
    assert.match(markup, /data-exact-authority="false"/);
    assert.match(
        markup,
        /data-solve-result="lower-bound">42c/,
    );
    assert.match(
        markup,
        /data-solve-result="upper-bound">Unavailable: no executable policy/,
    );
    assert.match(
        markup,
        /data-solve-result="absolute-gap">Unavailable: no executable policy/,
    );
    assert.match(
        markup,
        /data-solve-result="multiplicative-factor">Unavailable: no executable policy/,
    );
    assert.match(markup, /Closed numeric bounds without executable policy/);
    assert.doesNotMatch(markup, /Exact optimal policy/);
    assert.doesNotMatch(markup, /Exact optimum certified/);
    assert.doesNotMatch(markup, /Open in Strategy Board/);
    assert.doesNotMatch(markup, /10,000-run empirical mean/);
    console.log("  ok - equal numeric bounds cannot create executable-policy or exactness authority");
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
        missingPriceKeys: [],
        economyLabel: "Test economy",
        terminationDetail: detail,
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["regal"],
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
        missingPriceKeys: [],
        economyLabel: "Test economy",
        terminationDetail: solveTerminationDetail(summary, {
            incremental_action_envelope: {
                enabled: true,
                closed: true,
                actions: {
                    unevaluated: 0,
                    evaluating: 0,
                    unresolved: 0,
                },
            },
        }),
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["chaos"],
        busy: false,
        verification: null,
        telemetry: {
            incremental_action_envelope: {
                enabled: true,
                closed: true,
                actions: {
                    unevaluated: 0,
                    evaluating: 0,
                    unresolved: 0,
                },
            },
        },
    });
    assert.match(markup, /Exact optimal policy/);
    assert.match(markup, /Exact optimum certified within the displayed product scope\./);
    assert.match(markup, /Exact proof closed/);
    assert.match(markup, /Chaos-only policy authority: exact within the displayed scope/);
    assert.match(markup, /Action obligations closed/);
    assert.equal(certifiedFactorLabel(0.104), "1.11x");
    console.log("  ok - exact results remain exact and factors round conservatively");
}

{
    const summary = solveSummary({
        converged: true,
        policy_available: true,
        policy_status: "exact",
        termination: "exact_closed",
        stop_cause: "exact_closed",
        lower_bound: 90,
        upper_bound: 90,
        evaluated_policy_cost: 90,
        absolute_optimality_gap: 0,
        relative_optimality_gap: 0,
    });
    const telemetry = {
        incremental_action_envelope: {
            enabled: true,
            closed: false,
            actions: {
                unevaluated: 0,
                evaluating: 0,
                unresolved: 1,
            },
        },
    };
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["chaos"],
        excludedActions: 0,
        missingPriceKeys: [],
        economyLabel: "Test economy",
        terminationDetail: solveTerminationDetail(summary, telemetry),
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["chaos"],
        busy: false,
        verification: null,
        telemetry,
    });
    assert.match(markup, /data-exact-authority="false"/);
    assert.match(markup, /Executable policy; exactness not established/);
    assert.match(markup, /exactness metadata is inconsistent/);
    assert.match(markup, /Unavailable: exactness not established/);
    assert.match(markup, /1 action obligations remained open/);
    assert.match(markup, /Chaos-only policy authority: executable incumbent only/);
    assert.doesNotMatch(markup, /Exact optimal policy/);
    console.log("  ok - an explicitly open action envelope blocks exact presentation");
}

{
    const summary = solveSummary({
        policy_available: true,
        policy_status: "bounded_feasible",
        termination: "numerical_stability",
        stop_cause: "numerical_stability",
        lower_bound: 0,
        upper_bound: 37_279_857.83,
        evaluated_policy_cost: 37_279_857.83,
        absolute_optimality_gap: 37_279_857.83,
    });
    const detail = solveTerminationDetail(summary, null);
    assert.match(detail, /strict comparisons/);
    assert.match(detail, /published as bounded/);
    const markup = solveResultMarkup({
        summary,
        admittedActionIds: ["chaos"],
        excludedActions: 0,
        missingPriceKeys: [],
        economyLabel: "Test economy",
        terminationDetail: detail,
        productActionScope: "goal_relevant",
        goalProgressGatedReforges: true,
        hasCompiledStrategy: true,
        compiledOperationTypes: ["chaos"],
        busy: false,
        verification: null,
        telemetry: null,
    });
    assert.match(markup, /Numerical stability boundary/);
    assert.doesNotMatch(markup, /Exact optimal policy/);
    console.log("  ok - numerical stability stops remain explicitly bounded");
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
