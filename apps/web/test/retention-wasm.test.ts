// Focused, explicitly invoked WASM acceptance; bounded finish, no Simulator.
// Run: npx tsx test/retention-wasm.test.ts
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createEngineBindings } from "../src/app/engine-wasm";
import type { SolverGoal } from "../src/app/engine-protocol";

const root = new URL("../../../", import.meta.url);
const read = (path: string) => readFileSync(new URL(path, root), "utf8");
const engine = await createEngineBindings();
const data = engine.loadData(new TextEncoder().encode(JSON.stringify({
    manifest: JSON.parse(read("data/compiled/current/manifest.json")),
    strings: JSON.parse(read("data/compiled/current/strings.json")),
    game_data: JSON.parse(read("data/compiled/current/game-data.json")),
})));
const session = engine.createSession(data, "Metadata/Items/Armours/BodyArmours/BodyStrDex20", 86);
const evidence = "docs/archive/2026-09-04-free-value-bellman-research/";
const goal = JSON.parse(read(evidence + "native-goal.json")) as SolverGoal;
const economy = engine.loadEconomy(JSON.parse(read(evidence + "native-economy.json")));
const mods = ["LocalIncreasedArmourAndEvasionAndStunRecovery6",
    "LocalBaseArmourAndEvasionRating8", "LocalIncreasedArmourAndEvasion8",
    "ChanceToSuppressSpellsHigh5___"];

try {
    const scenarios = process.argv.includes("--fallback-only") ? [false] : [true, false];
    for (const fractured of scenarios) {
        const item = engine.createItem(session, "rare", false);
        const solver = engine.openSolver(session, goal);
        try {
            mods.forEach((key, index) => engine.addMod(item, session, {
                key, fractured: fractured && index === 3,
            }));
            const began = performance.now();
            // No retention flag supplied: normal WASM solver creation enables it.
            engine.beginSolverSolve(solver, item, economy, {
                solve_profile: "calculator_product_v1",
                max_solver_owned_bytes: 1 << 30,
                full_evidence: true,
            });
            // Proof-pattern attribution is published at finalization, not in
            // the live snapshot. Finish bounded without a discovery benchmark.
            engine.requestSolverSolveBoundedFinish(solver);
            let done = false;
            for (let steps = 0; steps < 20_000 && !done; steps += 1) {
                done = engine.stepSolverSolve(solver, 8).done;
            }
            assert.equal(done, true, "bounded finalization should complete");
            const summary = engine.finishSolverSolve(solver);
            const telemetry = engine.solverTelemetry(solver) as unknown as {
                carrier_bound_attribution: { proof_pattern_manager: { patterns: Array<{
                    id: string; converged: boolean; start_contribution: number | null;
                    fallback_reason: string; solution_sweeps: number;
                }> } };
            };
            const pattern = telemetry.carrier_bound_attribution.proof_pattern_manager.patterns
                .find((entry) => entry.id === "native_retention");
            assert.ok(pattern, "WASM should retain native retention attribution");
            if (fractured) {
                assert.equal(pattern.converged, true, pattern.fallback_reason);
                assert.equal(pattern.fallback_reason, "");
                assert.ok(pattern.start_contribution !== null);
                assert.ok(Math.abs(pattern.start_contribution - 352.31017033879505) < 1e-8);
                assert.ok(Math.abs((summary.lower_bound ?? 0) - 352.31017033879505) < 1e-8);
                assert.ok(pattern.solution_sweeps > 0);
            } else {
                assert.equal(pattern.converged, false);
                assert.equal(pattern.start_contribution, null); // no issued certificate
                assert.match(pattern.fallback_reason, /natural anchored request/);
            }
            console.log(JSON.stringify({ fractured, elapsed_ms: performance.now() - began,
                pattern, lower: summary.lower_bound, memory: engine.memoryStats() }));
        } finally {
            engine.abandonSolverSolve(solver);
            engine.closeSolver(solver);
            engine.closeItem(item);
        }
    }
} finally {
    engine.closeEconomy(economy);
    engine.closeSession(session);
    engine.closeData(data);
}
assert.equal(engine.memoryStats().live_handles, 0);
console.log("Selected WASM retention cases and handle cleanup passed");
