// Explicit, supervised qualification probe. Not part of the fast npm tests.
// Real Calculator DOM, client, worker and WASM; no rendered visual qualification.
import assert from "node:assert/strict";
import { readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";
import { parseHTML } from "linkedom";
import { EngineClient } from "../src/app/engine-client";
import { loadSolverBenchmarkCorpus, materializeSolverBenchmarkEconomy,
    validateCorpusArtifactPins } from "./solver-benchmark-corpus";
import { getPrices, setPrice, setFallbackPrice } from "../src/app/workspace/prices";
import { finishVerifiedCalculatorProbe } from "./calculator-delivery-probe-control";

const root = fileURLToPath(new URL("../../../", import.meta.url));
const [caseId, output, control = "finish", repeatText = "1"] = process.argv.slice(2);
assert.ok(caseId && output && ["finish", "default_finish", "cancel_setup", "cancel_retention", "cancel_compile"].includes(control));
const expectsDelivery = control === "finish" || control === "default_finish";
const repetitions = Number(repeatText);
assert.ok(repetitions === 1 || repetitions === 2);
assert.ok(repetitions === 1 || output.endsWith(".json"));
const corpus = loadSolverBenchmarkCorpus(resolve(root,
    "docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json"));
const spec = corpus.cases.find(c => c.id === caseId)!;
assert.ok(spec?.session && spec.start && spec.goal && spec.product_action_envelope);
const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.assign(globalThis, {window: dom.window, document: dom.document,
    HTMLElement: dom.HTMLElement, customElements: dom.customElements});
const { PcCalculator } = await import("../src/app/components/pc-calculator");
const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
const client = new EngineClient({
    postMessage: (m, transfer) => worker.postMessage(m, (transfer ?? []) as TransferListItem[]),
    onMessage: h => { worker.on("message", h); },
    onError: h => { worker.on("error", h); },
    terminate: () => { void worker.terminate(); },
});
try {
for (let repetition = 0; repetition < repetitions; ++repetition) {
let data = 0, session = 0, item = 0, solver = 0;
let nativeGraph: unknown = null;
let releaseTelemetry: unknown = null;
let releaseTelemetryReadMs = 0;
let controlBoundary: unknown = null;
const solve = client.solverSolve.bind(client);
const compile = client.solverCompileStrategy.bind(client);
client.solverSolve = async (...args) => {
    const result = await solve(...args);
    if (result.cancelled) {
        // Probe-only projection after native release, before Calculator closes
        // the solver handle. Its measured overhead remains in the UI gate.
        const began = performance.now();
        const telemetry = await client.solverTelemetry(args[0]);
        releaseTelemetry = telemetry.abandon_lifecycle ?? null;
        releaseTelemetryReadMs = performance.now() - began;
    }
    return result;
};
try {
    await client.whenReady();
    const artifact = resolve(root, "data/compiled/current");
    const manifest = JSON.parse(readFileSync(resolve(artifact, "manifest.json"), "utf8"));
    validateCorpusArtifactPins(corpus.manifest, manifest, client.getAbiVersion());
    const bundle = {manifest, strings: JSON.parse(readFileSync(resolve(artifact, "strings.json"), "utf8")),
        game_data: JSON.parse(readFileSync(resolve(artifact, "game-data.json"), "utf8"))};
    data = await client.loadData(new TextEncoder().encode(JSON.stringify(bundle)));
    session = await client.createSession(data, spec.session.base_metadata_path, spec.session.item_level);
    item = await client.createItem(session, {rarity: spec.start.rarity, withImplicits: spec.start.with_implicits});
    for (const mod of spec.start.mods) await client.addMod(item, session, {
        key: mod.key, fractured: mod.flags.includes("fractured") || undefined,
        crafted: mod.flags.includes("crafted") || undefined, veiled: mod.flags.includes("veiled") || undefined});
    assert.ok(!spec.start.generic_influence_bits && !spec.start.searing_exarch_tier && !spec.start.eater_of_worlds_tier);
    solver = await client.openSolver(session, spec.product_action_envelope.envelope_goal);
    const actions = await client.solverActions(solver);
    setFallbackPrice(null);
    for (const key of Object.keys(getPrices())) setPrice(key, null);
    const economy = materializeSolverBenchmarkEconomy(spec, root) as {prices: Record<string, number>};
    for (const [key, value] of Object.entries(economy.prices)) setPrice(key, value);
    const calculator = new PcCalculator();
    calculator.innerHTML = '<div class="pc-calc-solve-panel"></div>';
    // Access actual component state only to load the frozen corpus fixture.
    const fields = calculator as any;
    Object.assign(fields, {client, session, item, solver, pickerActions: actions,
        base: spec.session.base_metadata_path, itemLevel: spec.session.item_level,
        goalRarity: spec.goal.rarity, minSatisfiedSlots: spec.goal.min_satisfied_slots ?? spec.goal.slots.length,
        slots: spec.goal.slots.map(s => ({
            ...("family_mod_key" in s ? {familyModKey: s.family_mod_key} : {group: s.group}), minTier: s.min_tier ?? 1})),
        solveAllowEconomicRestart: false, solveConsiderImprintPrograms: false});
    client.solverCompileStrategy = async (...args) => { nativeGraph = await compile(...args); return nativeGraph as Awaited<ReturnType<typeof compile>>; };
    let intent = false;
    const render = fields.renderSolvePanel.bind(calculator);
    fields.renderSolvePanel = () => {
        render();
        if (intent) return;
        if (finishVerifiedCalculatorProbe(control, calculator)) {
            intent = true;
        } else if (control === "cancel_setup" && fields.solveDeliveryStage === "worker_solve_requested") {
            intent = true;
            setTimeout(() => {
                fields.solveUiMilestones.push({stage: "cancel_intent", ui_elapsed_ms: performance.now()-fields.solveUiStartedAt});
                calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="cancel"]')!.click();
            }, 100);
        } else if (control === "cancel_retention" &&
                fields.solveProgress?.trace?.current?.active_work_owner === "retention_setup") {
            intent = true;
            fields.solveUiMilestones.push({stage: "cancel_intent", ui_elapsed_ms: performance.now()-fields.solveUiStartedAt});
            calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="cancel"]')!.click();
        } else if (control === "cancel_compile" &&
                fields.solveProgress?.phase_owner === "compilation" &&
                !fields.solveProgress.trace?.current?.verified_artifact_available &&
                fields.solveProgress.trace?.events?.some((event: any) =>
                    event.kind === "compile_start" && event.reason === "initial_candidate")) {
            // Synchronize on the native candidate boundary; no sleep guesses
            // where a long executing WASM call might be. The new assertion
            // return exposes admitted private scratch before evaluation.
            intent = true;
            controlBoundary = fields.solveProgress;
            fields.solveUiMilestones.push({stage: "cancel_intent", ui_elapsed_ms: performance.now()-fields.solveUiStartedAt});
            calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="cancel"]')!.click();
        }
    };
    let probeError: string | null = null;
    try {
        await fields.startSolve();
    } catch (error) {
        probeError = error instanceof Error ? error.message : String(error);
    }
    const trace = fields.solveProgressExport;
    // Read bounded final diagnostics after actual delivery. This does not add
    // polling or checker work to the measured solve/Finish-to-usable interval.
    const diagnosticsStarted = performance.now();
    const finalTelemetry = expectsDelivery && trace?.status === "completed"
        ? await client.solverTelemetry(solver) : null;
    const finalTelemetryReadMs = performance.now() - diagnosticsStarted;
    const graphText = nativeGraph === null ? null : typeof nativeGraph === "string" ? nativeGraph : JSON.stringify(nativeGraph);
    const report = {case_id: caseId, control, probe_control_intent: intent, repetition, runtime_warm: repetition > 0,
        runtime_versions: process.versions,
        cache_context: "same runtime; fresh native data, session, item and solver handles on each repetition",
        environment: "linkedom actual Calculator + node-worker_threads WASM",
        visual_review: "not performed", request: trace?.request, resolved: trace?.resolved,
        trace, solve_summary: fields.solveSummary, error: fields.solveError,
        usable_strategy: !!fields.solvedStrategy, graph_sha256: graphText === null ? null : createHash("sha256").update(graphText).digest("hex"),
        graph: nativeGraph, probe_error: probeError, release_telemetry: releaseTelemetry,
        release_telemetry_read_ms: releaseTelemetryReadMs, control_boundary: controlBoundary,
        final_telemetry: finalTelemetry, final_telemetry_read_ms: finalTelemetryReadMs};
    writeFileSync(repetition === 0 ? output : output.replace(/\.json$/, ".warm.json"),
        JSON.stringify(report, null, 2)+"\n");
    console.log(JSON.stringify({case_id: caseId, control, status: trace?.status, usable: report.usable_strategy,
        error: report.error, probe_error: probeError, ui_milestones: trace?.ui_milestones}));
    assert.equal(probeError, null);
    assert.equal(trace?.status, expectsDelivery ? "completed" : "cancelled");
    assert.equal(report.usable_strategy, expectsDelivery);
    if (control === "default_finish") {
        assert.equal(intent, false);
        assert.equal(trace.request.bounded_finish_after_ms, 240000);
        assert.ok(!trace.ui_milestones.some((entry: {stage: string}) => entry.stage === "finish_intent"));
    }
    if (control === "cancel_compile") assert.ok(controlBoundary);
} finally {
    if (solver) await client.closeSolver(solver);
    if (item) await client.closeItem(item);
    if (session) await client.closeSession(session);
    if (data) await client.closeData(data);
    client.solverSolve = solve;
    client.solverCompileStrategy = compile;
}
}
} finally {
    await worker.terminate();
}
