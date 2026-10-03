// One-off isolated qualification fixture using the existing product worker,
// board preparation, exact evaluator and Simulator. No solver search or seed.
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { Worker, type TransferListItem } from "node:worker_threads";
import { EngineClient } from "../src/app/engine-client";
import { prepareSolverStrategy } from "../src/app/solve-workspace";
import { validateCorpusArtifactPins } from "./solver-benchmark-corpus";

const [graphPath, casePath, artifactPath, mainRoot, output, trialsText = "0", expectedCostText] = process.argv.slice(2);
assert.ok(graphPath && casePath && artifactPath && mainRoot && output);
const trials = Number(trialsText);
assert.ok(trials === 0 || trials === 1000);
const read = (path: string) => JSON.parse(readFileSync(path, "utf8"));
const graphBytes = readFileSync(graphPath);
const spec = read(casePath);
const graph = prepareSolverStrategy(JSON.parse(graphBytes.toString("utf8")));
const snapshot = read(resolve(mainRoot, spec.economy.snapshot_path));
assert.equal(snapshot.metadata.content_sha256, spec.economy.content_sha256);
const economyDocument = {...snapshot, prices: {...snapshot.prices, ...spec.economy.manual_overrides}};
const manifest = read(resolve(artifactPath, "manifest.json"));
const worker = new Worker(new URL("./sol61-worker-bootstrap.mjs", import.meta.url));
const client = new EngineClient({
    postMessage: (message, transfer) => worker.postMessage(message, (transfer ?? []) as TransferListItem[]),
    onMessage: handler => { worker.on("message", handler); },
    onError: handler => { worker.on("error", handler); },
    terminate: () => { void worker.terminate(); },
});
const receipt: Record<string, unknown> = {
    graph_path: graphPath,
    graph_sha256: createHash("sha256").update(graphBytes).digest("hex"),
    case_path: casePath,
    artifact_path: artifactPath,
    economy_identity: spec.economy.content_sha256,
    scope: "ordinary_strategy_execution_diagnostic; no imported native programme provenance or solver incumbent admission",
    trials,
    status: "running",
};
let lastMemory: unknown;
worker.on("message", message => { if (message?.__sol61_memory) lastMemory = message.__sol61_memory; });
let data = 0, session = 0, economy = 0, strategy = 0, simulator = 0;
const began = performance.now();
try {
    await client.whenReady();
    validateCorpusArtifactPins(read(resolve(dirname(casePath), "manifest.json")), manifest, client.getAbiVersion());
    data = await client.loadData(new TextEncoder().encode(JSON.stringify({
        manifest,
        strings: read(resolve(artifactPath, "strings.json")),
        game_data: read(resolve(artifactPath, "game-data.json")),
    })));
    session = await client.createSession(data, spec.session.base_metadata_path, spec.session.item_level);
    economy = await client.loadEconomy(economyDocument);
    strategy = await client.compileStrategy(session, graph);
    receipt.product_board_import = "passed";
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 30000);
    let evaluated;
    try {
        evaluated = await client.strategyEvaluate(session, graph, {
            max_states: 800000, max_pairs: 1215000, max_transitions: 10000000,
            max_owned_bytes: 1073741824, max_output_json_bytes: 16777216,
        }, {economy: economyDocument, chunkSize: 256, signal: controller.signal});
    } finally { clearTimeout(timer); }
    const totals = evaluated.accounting.totals.per_invocation;
    receipt.exact = {
        converged: evaluated.converged,
        terminals: evaluated.terminals,
        expected_actions: totals.expected_actions,
        cost_complete: totals.cost_complete,
        total_expected_cost: totals.total_expected_cost,
        memory: evaluated.memory,
        consumption: evaluated.expected_consumption,
    };
    assert.equal(evaluated.converged, true);
    assert.equal(totals.cost_complete, true);
    if (expectedCostText !== undefined) {
        const expectedCost = Number(expectedCostText);
        assert.ok(Number.isFinite(expectedCost) && expectedCost >= 0);
        assert.ok(typeof totals.total_expected_cost === "number" &&
            Math.abs(totals.total_expected_cost - expectedCost) <= Math.max(1e-8, expectedCost * 1e-9));
        receipt.native_cost_parity = {expected_cost: expectedCost, tolerance_relative: 1e-9, status: "passed"};
    }
    assert.ok(Math.abs(evaluated.terminals.success - 1) < 1e-9);
    for (const key of ["failure", "stop", "action_not_applied", "no_matching_edge", "unresolved"] as const)
        assert.ok(evaluated.terminals[key] <= 1e-9);
    if (trials) {
        simulator = await client.createSimulator(session, strategy, economy);
        const result = await client.runStrategy(simulator, {
            target_runs: 1000, seed: 27092026, max_actions_per_run: 1000000,
            max_graph_steps_per_run: 4000000, max_cost_per_run: 1000000000,
            retained_trace_count: 0, retained_success_count: 0, retained_failure_count: 0,
        }, {chunkSize: 1});
        receipt.simulator = {summary: result.summary, failure_summaries: result.failure_summaries};
        assert.equal(result.cancelled, false);
        assert.equal(result.summary.completed_runs, 1000);
        // These finite caps censor long valid runs of a million-action policy.
        // Count censoring separately; execution failures are never acceptable.
        receipt.simulator_qualification = {
            requested_runs: 1000, max_actions_per_run: 1000000,
            max_graph_steps_per_run: 4000000, max_cost_per_run: 1000000000,
            success_count: result.summary.success_count,
            failures: result.failure_summaries,
            meaning: "finite execution qualification; censored runs do not estimate uncapped policy EV",
        };
        const unexpected = result.failure_summaries.filter(entry =>
            ![2, 3, 4].includes(entry.failure_reason));
        assert.equal(unexpected.length, 0, JSON.stringify(unexpected));
        assert.equal(result.summary.action_not_applied_count, 0);
        assert.equal(result.summary.no_matching_edge_count, 0);
        assert.equal(result.summary.missing_price_run_count, 0);
    }
    receipt.status = "passed";
} catch (error) {
    receipt.status = "failed";
    receipt.error = String(error);
    throw error;
} finally {
    if (simulator) await client.closeSimulator(simulator);
    if (strategy) await client.closeStrategy(strategy);
    if (economy) await client.closeEconomy(economy);
    if (session) await client.closeSession(session);
    if (data) await client.closeData(data);
    receipt.wasm_memory = lastMemory;
    await worker.terminate();
    receipt.wall_ms = performance.now() - began;
    writeFileSync(output, JSON.stringify(receipt, null, 2) + "\n");
    console.log(JSON.stringify({status: receipt.status, wall_ms: receipt.wall_ms, output}));
}
