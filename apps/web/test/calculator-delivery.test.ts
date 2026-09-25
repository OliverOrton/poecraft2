import assert from "node:assert/strict";
import { parseHTML } from "linkedom";
import type { SolveProgress } from "../src/app/engine-protocol";
import type { SolverGoal, SolveOptions } from "../src/app/engine-protocol";
import type { EngineClient } from "../src/app/engine-client";
import {
    createCalculatorDeliveryTrace,
    retainCalculatorProgress,
    retainCalculatorWorkerMetrics,
    type CalculatorDeliveryTrace,
} from "../src/app/solve-workspace";
import { pinEconomy, setPrice } from "../src/app/workspace/prices";
import { finishVerifiedCalculatorProbe } from "./calculator-delivery-probe-control";

const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.assign(globalThis, {
    window: dom.window,
    document: dom.document,
    HTMLElement: dom.HTMLElement,
    customElements: dom.customElements,
});
const { PcCalculator } = await import("../src/app/components/pc-calculator");
const calculator = new PcCalculator();
calculator.innerHTML = '<div class="pc-calc-solve-panel"></div>';
const access = calculator as unknown as {
    renderSolvePanel(): void;
    solveProgress: SolveProgress;
    solveProgressExport: CalculatorDeliveryTrace;
};
access.solveProgress = {
    phase_owner: "discovery",
    trace: {
        current: {
            active_work_owner: "ordinary_search",
            candidate_source: "fixture",
            candidate_stage: "verified",
            missing_continuations: 0,
            numerical_generation: 1,
            reforge_source: "outer_calculator",
        },
        events: [],
    },
} as unknown as SolveProgress;
setPrice("chaos", 100);
const request = {
    run_id: "fixture-run",
    submitted_at_utc: "2026-09-15T00:00:00Z",
    base_path: "fixture-base",
    item_level: 86,
    goal_envelope: { version: "v1" as const, slots: [{ family_mod_key: "goal", min_tier: 1 }] },
    solve_options: { solve_profile: "calculator_product_v1" as const },
    bounded_finish_after_ms: 240000,
    economy: pinEconomy(["chaos"]),
    identity: { abi_version: 2, source_revision: null, runtime_wasm_sha256: null,
        data_hash: null, data_hash_source: "unavailable" },
};
access.solveProgressExport = createCalculatorDeliveryTrace(request);
retainCalculatorProgress(access.solveProgressExport, access.solveProgress);
request.goal_envelope.slots[0].min_tier = 9;
request.economy.snapshot.prices.chaos = 999;
const downloads: Blob[] = [];
URL.createObjectURL = (blob: Blob): string => {
    downloads.push(blob);
    return "blob:fixture";
};
URL.revokeObjectURL = (): void => {};

for (let render = 0; render < 3; render += 1) {
    access.renderSolvePanel();
    const button = calculator.querySelector<HTMLButtonElement>("[data-progress-export]");
    assert.ok(button && !button.disabled);
    button.click();
    assert.equal(downloads.length, render + 1,
        "each newly rendered export button must have exactly one working listener");
    const exported = JSON.parse(await downloads.at(-1)!.text());
    assert.equal(exported.request.goal_envelope.slots[0].min_tier, 1);
    assert.equal(exported.request.economy.snapshot.prices.chaos, 100);
    assert.equal(exported.observations.length, 1);
}
// Partial traces survive cancellation/error, even before the first native trace.
for (const status of ["preparing", "cancelled", "error", "completed"] as const) {
    access.solveProgressExport.status = status;
    access.solveProgress = null as unknown as SolveProgress;
    access.renderSolvePanel();
    calculator.querySelector<HTMLButtonElement>("[data-progress-export]")!.click();
    assert.equal(JSON.parse(await downloads.at(-1)!.text()).status, status);
}
const bounded = createCalculatorDeliveryTrace(request);
for (let sequence = 1; sequence <= 4100; sequence += 1) {
    retainCalculatorProgress(bounded, { lifecycle_sequence: sequence } as SolveProgress);
}
assert.equal(bounded.observations.length, 4096);
assert.equal(bounded.observations_omitted, 4);
assert.equal(bounded.observations[0].lifecycle_sequence, 5);
const history = bounded.observations;
retainCalculatorWorkerMetrics(bounded, {
    progress_observations: [{ lifecycle_sequence: 9000 } as SolveProgress],
    progress_observations_omitted: 7,
} as Parameters<typeof retainCalculatorWorkerMetrics>[1]);
assert.strictEqual(bounded.observations, history);
assert.ok(!("progress_observations" in bounded.worker!));
assert.equal(bounded.worker?.progress_observations_omitted, 7);

// Exercise startSolve itself: mutate the form and prices while its first await
// is pending, then verify the actual native request and retained error/cancel
// export still describe the submission. Native mechanics are not mocked here
// as qualification; this tests the UI's ownership and transport boundary.
for (const fail of [false, true]) {
    const live = new PcCalculator();
    live.innerHTML = '<div class="pc-calc-solve-panel"></div>';
    const fields = live as unknown as {
        startSolve(): Promise<void>;
        client: EngineClient;
        solver: number; item: number; session: number;
        base: string; itemLevel: number;
        slots: Array<{ familyModKey: string; minTier: number }>;
        pickerActions: unknown[];
        solveAbsoluteGapTarget: number;
        solveProgressExport: CalculatorDeliveryTrace;
    };
    const actions = [{ id: "chaos", family: "basic", cost_keys: ["chaos"] }];
    const goals: SolverGoal[] = [];
    const released: number[] = [];
    Object.assign(fields, { solver: 9, item: 7, session: 1, base: "submitted-base",
        itemLevel: 86, slots: [{ familyModKey: "original-goal", minTier: 1 }],
        pickerActions: actions, solveAbsoluteGapTarget: 2 });
    setPrice("chaos", 100);
    fields.client = {
        getAbiVersion: () => 2,
        cloneItem: async (item: number) => {
            assert.equal(item, 7);
            fields.base = "later-base";
            fields.item = 999;
            fields.slots[0].familyModKey = "later-goal";
            fields.solveAbsoluteGapTarget = 999;
            setPrice("chaos", 700);
            return 17;
        },
        exportItem: async (item: number) => { assert.equal(item, 17); return { fixture: "submitted-item" }; },
        openSolver: async (session: number, goal: SolverGoal) => {
            assert.equal(session, 1); goals.push(structuredClone(goal)); return 77;
        },
        solverActions: async () => actions,
        closeSolver: async () => {},
        loadEconomy: async (economy: { prices: Record<string, number> }) => {
            assert.equal(economy.prices.chaos, 100); return 88;
        },
        closeEconomy: async () => {},
        closeItem: async (item: number) => { released.push(item); },
        solverSolve: async (_solver: number, item: number, _economy: number, options: SolveOptions,
            execution: { boundedFinishAfterMs: number }) => {
            assert.equal(item, 17);
            assert.equal(options.max_absolute_optimality_gap, 2);
            assert.equal(execution.boundedFinishAfterMs, 240000,
                "unattended Calculator requests retain the ordinary four-minute finish");
            if (fail) throw new Error("fixture worker failure");
            return { cancelled: true };
        },
    } as unknown as EngineClient;
    await fields.startSolve();
    assert.equal(goals.length, 2);
    assert.deepEqual(goals[0].slots[0], { family_mod_key: "original-goal", min_tier: 1 });
    assert.deepEqual(goals[1].slots[0], { family_mod_key: "original-goal", min_tier: 1 });
    assert.deepEqual(goals[1].actions, ["chaos"]);
    assert.equal(fields.solveProgressExport.request.base_path, "submitted-base");
    assert.deepEqual(fields.solveProgressExport.resolved.start_item, { fixture: "submitted-item" });
    assert.equal(fields.solveProgressExport.status, fail ? "error" : "cancelled");
    assert.deepEqual(released, [17]);
    live.querySelector<HTMLButtonElement>("[data-progress-export]")!.click();
    assert.equal(JSON.parse(await downloads.at(-1)!.text()).request.base_path, "submitted-base");
}
console.log("  ok - Calculator DOM export, frozen request, partial states and bounded history");

const finderRequest = {
    ...request,
    solve_options: {solver_mode: "strategy_finder" as const},
};
const finderTrace = createCalculatorDeliveryTrace(finderRequest);
finderRequest.solve_options.solver_mode = "current" as "strategy_finder";
assert.equal(finderTrace.request.solve_options.solver_mode, "strategy_finder");
assert.equal(JSON.parse(JSON.stringify(finderTrace)).request.solve_options.solver_mode,
    "strategy_finder");

const controls = calculator as unknown as {
    solveRunning: boolean; solveFinish: () => void; solveFinishRequested: boolean;
    solveAbort: AbortController; solveProgress: SolveProgress;
};
let intents = 0;
controls.solveRunning = true; controls.solveFinishRequested = false;
controls.solveFinish = () => { ++intents; };
controls.solveAbort = new AbortController();
controls.solveProgress = {expanded_states: 1, discovered_states: 1,
    frontier_states: 0, sweeps: 0, focused_round: 0, state_action_rows: 0,
    transition_entries: 0, reforge_work: 0, refinement_states: 0, refinement_classes: 0,
    certification_discovered_pairs: 0,
    phase: "expanding", phase_owner: "fixture", trace: {current: {
        active_work_owner: "fixture", candidate_source: "fixture", candidate_stage: "verified", reforge_source: "outer_calculator",
        verified_artifact_available: false}, events: []}} as unknown as SolveProgress;
access.renderSolvePanel();
assert.equal(calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="finish"]')!.disabled, true);
controls.solveProgress.trace!.current.verified_artifact_available = true;
access.renderSolvePanel();
const finishButton = calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="finish"]')!;
assert.equal(finishButton.disabled, false);
for (let observation = 0; observation < 3; ++observation) {
    access.renderSolvePanel();
    assert.equal(finishVerifiedCalculatorProbe("default_finish", calculator), false);
    assert.equal(intents, 0, "unattended probe never clicks an enabled Finish button");
    assert.equal(controls.solveFinishRequested, false);
    assert.equal(controls.solveAbort.signal.aborted, false);
}
assert.equal(finishVerifiedCalculatorProbe("finish", calculator), true);
finishButton.click();
assert.equal(intents, 1);
assert.match(calculator.querySelector('[data-solve-cmd="finish"]')!.textContent!, /Finishing/);
assert.equal(controls.solveAbort.signal.aborted, false);
calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="cancel"]')!.click();
assert.equal(controls.solveAbort.signal.aborted, true);
controls.solveRunning = false;
access.renderSolvePanel();
const modeSelect = calculator.querySelector<HTMLSelectElement>('[data-solve-mode]')!;
modeSelect.querySelector('option[value="current"]')!.removeAttribute("selected");
modeSelect.querySelector('option[value="strategy_finder"]')!.setAttribute("selected", "");
modeSelect.dispatchEvent(new dom.window.Event("change"));
assert.equal(calculator.querySelector<HTMLSelectElement>('[data-solve-mode]')!.value,
    "strategy_finder");
assert.equal(calculator.querySelector<HTMLInputElement>('[data-solve-target="absolute"]')!.disabled,
    true);
