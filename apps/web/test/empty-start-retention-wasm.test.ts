// Explicit integration qualification, not part of the routine web suite.
// npx tsx test/empty-start-retention-wasm.test.ts [--four-goal] [--output <receipt.json>]
// The fixture's 60-second finish and 90-second watchdog include preparation.
// Run in an isolated process with an outer timeout for noncooperative native work.
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { createEngineBindings } from "../src/app/engine-wasm";
import type { SolveOptions, SolverGoal } from "../src/app/engine-protocol";

const root = new URL("../../../", import.meta.url);
const read = (path: string) => JSON.parse(readFileSync(new URL(path, root), "utf8"));
const fourGoal = process.argv.includes("--four-goal");
const fixturePath = `fixtures/solver-quality-ladder/v1/cases/conquest-lamellar-allflame-clean-${fourGoal ? 4 : 5}-goal-product8.json`;
const fixture = read(fixturePath);
assert.equal(fixture.start.mods.length, 0);
assert.equal(fixture.goal.min_satisfied_slots, fourGoal ? 4 : 5);
const expectedLower = fourGoal ? 198.8334996747695 : 405.3694021063399;
const snapshot = read(fixture.economy.snapshot_path);
assert.equal(snapshot.id, fixture.economy.id);
assert.equal(snapshot.metadata.content_sha256, fixture.economy.content_sha256);
assert.equal(snapshot.metadata.source_cutoff_at_utc, fixture.economy.source_cutoff_at_utc);
assert.equal(fixture.economy.fallback_price, null);
const prices = { ...snapshot.prices, ...fixture.economy.manual_overrides };
const engine = await createEngineBindings();
const data = engine.loadData(new TextEncoder().encode(JSON.stringify({
    manifest: read("data/compiled/current/manifest.json"),
    strings: read("data/compiled/current/strings.json"),
    game_data: read("data/compiled/current/game-data.json"),
})));
const session = engine.createSession(data, fixture.session.base_metadata_path, fixture.session.item_level);
const economy = engine.loadEconomy({ ...snapshot, prices });
const item = engine.createItem(session, fixture.start.rarity, fixture.start.with_implicits);
let solver: number | undefined;
const began = performance.now();
const elapsed = () => performance.now() - began;
const receipt: Record<string, unknown> = { fixture: fixturePath, input: fixture, status: "running" };
try {
    // Ask the native registry for the same priced product envelope as the benchmark.
    const envelope = engine.openSolver(session, fixture.product_action_envelope.envelope_goal);
    let actions: string[];
    try {
        actions = engine.solverActions(envelope)
            .filter(action => action.cost_keys.every(key => Object.hasOwn(prices, key)))
            .map(action => action.id);
    } finally {
        engine.closeSolver(envelope);
    }
    assert.ok(actions.length > 0);
    receipt.product_action_ids = actions;
    const goal: SolverGoal = { ...fixture.goal, actions };
    solver = engine.openSolver(session, goal);
    const { solve_step_work_items, worker_step_ms, cancel_ack_ms, ...caps } = fixture.caps;
    void worker_step_ms; void cancel_ack_ms;
    engine.beginSolverSolve(solver, item, economy, caps as SolveOptions);
    let requested = false;
    let done = false;
    while (!done) {
        assert.ok(elapsed() < fixture.watchdog_seconds * 1000, "fixture watchdog expired");
        if (!requested && elapsed() >= fixture.requested_bounded_finish_seconds * 1000) {
            engine.requestSolverSolveBoundedFinish(solver);
            requested = true;
            receipt.finish_requested_ms = elapsed();
        }
        done = engine.stepSolverSolve(solver, solve_step_work_items).done;
    }
    const summary = engine.finishSolverSolve(solver);
    receipt.solve_complete_ms = elapsed();
    receipt.summary = summary;
    const telemetry = engine.solverTelemetry(solver) as unknown as {
        timings_ns: unknown;
        incremental_action_envelope: unknown;
        policy_refinement: {
            strict_lift: { status: string; global_lower_bound_closed: boolean };
            publication: unknown;
        };
        carrier_bound_attribution: { proof_pattern_manager: { patterns: Array<{
            id: string; converged: boolean; start_contribution: number | null; fallback_reason: string;
        }> } };
    };
    receipt.timings_ns = telemetry.timings_ns;
    receipt.incremental_action_envelope = telemetry.incremental_action_envelope;
    receipt.strict_lift = telemetry.policy_refinement.strict_lift;
    receipt.publication = telemetry.policy_refinement.publication;
    const pattern = telemetry.carrier_bound_attribution.proof_pattern_manager.patterns
        .find(entry => entry.id === "native_retention");
    receipt.pattern = pattern;
    assert.ok(pattern?.converged, pattern?.fallback_reason);
    assert.ok(Math.abs((pattern.start_contribution ?? 0) - expectedLower) < 1e-7);
    assert.ok((summary.lower_bound ?? 0) + 1e-7 >= pattern.start_contribution!);
    assert.ok(summary.policy_available, "bounded result must have an executable policy");
    const graph = engine.solverCompileStrategy(solver);
    receipt.strategy_sha256 = createHash("sha256").update(graph).digest("hex");
    const strategy = engine.compileStrategy(session, graph);
    try {
        const v = fixture.verification;
        const evaluation = engine.beginStrategyEvaluation(strategy, {
            max_states: v.exact_max_states, max_pairs: v.exact_max_pairs,
            max_transitions: v.exact_max_transitions, max_owned_bytes: v.exact_max_owned_bytes,
        }, economy);
        try {
            let evaluated = false;
            while (!evaluated) {
                assert.ok(elapsed() < fixture.watchdog_seconds * 1000, "evaluation watchdog expired");
                evaluated = engine.stepStrategyEvaluation(evaluation, solve_step_work_items).done;
            }
            const result = engine.finishStrategyEvaluation(evaluation);
            receipt.exact_evaluation = result;
            const totals = result.accounting.totals.per_invocation;
            assert.equal(result.converged, true);
            assert.equal(totals.cost_complete, true);
            assert.ok(Math.abs(result.terminals.success - 1) < 1e-9);
            for (const key of ["failure", "stop", "action_not_applied", "no_matching_edge", "unresolved"] as const) {
                assert.equal(result.terminals[key], 0);
            }
            assert.ok(summary.upper_bound !== null && totals.total_expected_cost !== null);
            assert.ok(Math.abs(totals.total_expected_cost - summary.upper_bound) <=
                v.exact_cost_absolute_tolerance + v.exact_cost_relative_tolerance * Math.abs(summary.upper_bound));
            // Frozen against B6 with the same retained root lower and finish
            // request. Assert useful continuation integration after evaluating
            // the actual graph, so a failed gate still retains its true cost.
            if (fourGoal) {
                assert.equal(telemetry.policy_refinement.strict_lift.status, "requested_bounded_finish");
                assert.equal(telemetry.policy_refinement.strict_lift.global_lower_bound_closed, false);
                assert.ok(summary.upper_bound <= 5218.040949685988 + 1e-7,
                    "bounded strict finish lost the verified four-goal incumbent");
            } else {
                assert.ok(summary.upper_bound <= 16997812.199227553 * 0.8,
                    "empty-start continuation misses the 20% policy improvement gate");
            }
        } finally {
            engine.closeStrategyEvaluation(evaluation);
        }
    } finally {
        engine.closeStrategy(strategy);
    }
    engine.abandonSolverSolve(solver);
    engine.closeSolver(solver);
    solver = undefined;

    // Abandon a separately scoped setup with control memory and check release.
    // Native focused fixtures own retention-domain refusal assertions.
    solver = engine.openSolver(session, goal);
    engine.beginSolverSolve(solver, item, economy, {
        ...caps, consider_imprint_programs: true,
    } as SolveOptions);
    engine.abandonSolverSolve(solver);
    receipt.status = "passed";
} catch (error) {
    receipt.status = "failed";
    receipt.error = String(error);
    throw error;
} finally {
    if (solver !== undefined) {
        engine.abandonSolverSolve(solver);
        engine.closeSolver(solver);
    }
    engine.closeItem(item);
    engine.closeEconomy(economy);
    engine.closeSession(session);
    engine.closeData(data);
    receipt.total_ms = elapsed();
    receipt.memory_after_cleanup = engine.memoryStats();
    const outputIndex = process.argv.indexOf("--output");
    if (outputIndex >= 0) writeFileSync(process.argv[outputIndex + 1], JSON.stringify(receipt, null, 2) + "\n");
    console.log(JSON.stringify({ status: receipt.status, summary: receipt.summary,
        total_ms: receipt.total_ms, memory_after_cleanup: receipt.memory_after_cleanup }));
    assert.equal(engine.memoryStats().live_handles, 0);
}
