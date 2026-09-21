// S1: evaluate one saved graph through the existing worker, never run search.
import { readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";
import { EngineClient } from "../../../../apps/web/src/app/engine-client";
import { materializeSolverBenchmarkEconomy } from "../../../../apps/web/test/solver-benchmark-corpus";

async function main() {
const root = resolve(process.cwd(), "../..");
const output = resolve(root, "out/cooperative-setup");
const read = (path: string) => JSON.parse(readFileSync(resolve(root, path), "utf8"));
const spec = read("docs/active/2026-09-09-cross-base-capability-recovery/core/cases/cb02-cross-base-product8-long240.json");
const graphPath = "docs/active/2026-09-14-progress-and-delivery/strategies/conquest-four.strategy.json";
const request = {
    graphPath, graph: read(graphPath), goal: spec.goal, session: spec.session,
    economy: materializeSolverBenchmarkEconomy(spec, root),
    options: { epsilon: 0, max_sweeps: 0, max_states: spec.verification.exact_max_states,
        max_pairs: spec.verification.exact_max_pairs, max_transitions: spec.verification.exact_max_transitions,
        max_owned_bytes: spec.verification.exact_max_owned_bytes, max_output_json_bytes: 67108864 },
};
if (process.argv[2] === "prepare") {
    writeFileSync(resolve(output, "S1-request.json"), JSON.stringify(request));
} else {
    const worker = new Worker(pathToFileURL(resolve(root, "apps/web/test/worker-bootstrap.mjs")));
    const client = new EngineClient({
        postMessage: (message, transfer) => worker.postMessage(message, transfer as any),
        onMessage: (handler) => worker.on("message", handler),
        onError: (handler) => worker.on("error", handler),
        terminate: () => { void worker.terminate(); },
    });
    const started = performance.now();
    let data: number | undefined, session: number | undefined;
    try {
        const manifest = read("data/compiled/current/manifest.json");
        const strings = read("data/compiled/current/strings.json");
        const game_data = read("data/compiled/current/game-data.json");
        data = await client.loadData(new TextEncoder().encode(JSON.stringify({ manifest, strings, game_data })));
        session = await client.createSession(data, spec.session.base_metadata_path, spec.session.item_level);
        const result = await client.strategyEvaluate(session, request.graph, request.options, { economy: request.economy });
        writeFileSync(resolve(output, "S1-wasm-evaluation.json"), JSON.stringify({ result, wall_ms: performance.now() - started }));
    } finally {
        if (session !== undefined) await client.closeSession(session);
        if (data !== undefined) await client.closeData(data);
        await worker.terminate();
    }
}
}
main().catch((error) => { console.error(error); process.exitCode = 1; });
