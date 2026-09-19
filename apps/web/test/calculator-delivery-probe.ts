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

const root = fileURLToPath(new URL("../../../", import.meta.url));
const [caseId, output, control = "finish"] = process.argv.slice(2);
assert.ok(caseId && output && ["finish", "cancel_setup"].includes(control));
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
let data = 0, session = 0, item = 0, solver = 0;
let nativeGraph: unknown = null;
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
    const compile = client.solverCompileStrategy.bind(client);
    client.solverCompileStrategy = async (...args) => { nativeGraph = await compile(...args); return nativeGraph as Awaited<ReturnType<typeof compile>>; };
    let intent = false;
    const render = fields.renderSolvePanel.bind(calculator);
    fields.renderSolvePanel = () => {
        render();
        if (intent) return;
        const button = calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="finish"]');
        if (control === "finish" && button && !button.disabled) {
            intent = true; button.click();
        } else if (control === "cancel_setup" && fields.solveDeliveryStage === "worker_solve_requested") {
            intent = true;
            setTimeout(() => {
                fields.solveUiMilestones.push({stage: "cancel_intent", ui_elapsed_ms: performance.now()-fields.solveUiStartedAt});
                calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="cancel"]')!.click();
            }, 100);
        }
    };
    await fields.startSolve();
    const trace = fields.solveProgressExport;
    const graphText = nativeGraph === null ? null : typeof nativeGraph === "string" ? nativeGraph : JSON.stringify(nativeGraph);
    const report = {case_id: caseId, control, environment: "linkedom actual Calculator + node-worker_threads WASM",
        visual_review: "not performed", request: trace?.request, resolved: trace?.resolved,
        trace, solve_summary: fields.solveSummary, error: fields.solveError,
        usable_strategy: !!fields.solvedStrategy, graph_sha256: graphText === null ? null : createHash("sha256").update(graphText).digest("hex"),
        graph: nativeGraph};
    writeFileSync(output, JSON.stringify(report, null, 2)+"\n");
    console.log(JSON.stringify({case_id: caseId, control, status: trace?.status, usable: report.usable_strategy,
        error: report.error, ui_milestones: trace?.ui_milestones}));
    assert.equal(trace?.status, control === "finish" ? "completed" : "cancelled");
    assert.equal(report.usable_strategy, control === "finish");
} finally {
    if (solver) await client.closeSolver(solver);
    if (item) await client.closeItem(item);
    if (session) await client.closeSession(session);
    if (data) await client.closeData(data);
    await worker.terminate();
}
