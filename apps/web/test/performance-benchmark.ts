import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, EngineTransport } from "../src/app/engine-client";
import type {
    ClientMessage,
    WorkerMessage,
} from "../src/app/engine-protocol";

const REPO_ROOT = new URL("../../../", import.meta.url);
const ARTIFACT_DIR = new URL("data/compiled/current/", REPO_ROOT);
const BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";
const RUNS = Number(process.env.POECRAFT_BENCH_RUNS ?? "100000");
const ACTION_TYPE = process.env.POECRAFT_BENCH_ACTION ?? "chaos";
const PROFILE = process.env.POECRAFT_BENCH_PROFILE ?? "ui";
const ACTIONS_PER_RUN = Math.max(
    1,
    Number(process.env.POECRAFT_BENCH_ACTIONS_PER_RUN ?? "10"),
);

function readText(url: URL): string {
    return readFileSync(fileURLToPath(url), "utf8");
}

function buildBundle(): Uint8Array {
    const manifest = readText(new URL("manifest.json", ARTIFACT_DIR));
    const strings = readText(new URL("strings.json", ARTIFACT_DIR));
    const gameData = readText(new URL("game-data.json", ARTIFACT_DIR));
    return new TextEncoder().encode(
        `{"manifest":${manifest},"strings":${strings},"game_data":${gameData}}`,
    );
}

function spawnClient(): { client: EngineClient; worker: Worker } {
    const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
    const transport: EngineTransport = {
        postMessage: (message: ClientMessage, transfer?: Transferable[]) =>
            worker.postMessage(
                message,
                (transfer ?? []) as unknown as TransferListItem[],
            ),
        onMessage: (handler: (message: WorkerMessage) => void) =>
            worker.on("message", handler),
        terminate: () => void worker.terminate(),
    };
    return { client: new EngineClient(transport), worker };
}

const actionNodes = Array.from({ length: ACTIONS_PER_RUN }, (_, index) => ({
    id: `${ACTION_TYPE}_${index + 1}`,
    kind: "operation",
    operation: { type: ACTION_TYPE, params: {} },
}));
const strategy = {
    version: "v1",
    name: "Phase 14 WASM throughput benchmark",
    start_node_id: "start",
    base_state: {
        base_key: BASE,
        item_level: 86,
        rarity: ACTION_TYPE === "alteration" ? "magic" : "rare",
    },
    nodes: [
        { id: "start", kind: "start" },
        ...actionNodes,
        { id: "success", kind: "terminal", terminal: "success" },
    ],
    edges: [
        {
            id: "begin",
            from: "start",
            to: actionNodes[0].id,
            priority: 0,
            condition: { type: "always" },
        },
        ...actionNodes.map((node, index) => ({
            id: `step_${index + 1}`,
            from: node.id,
            to: actionNodes[index + 1]?.id ?? "success",
            priority: 0,
            condition: { type: "always" },
        })),
    ],
};

const { client, worker } = spawnClient();
let data = 0;
let session = 0;
let compiled = 0;
let simulator = 0;
try {
    const bundle = buildBundle();
    const loadStarted = performance.now();
    data = await client.loadData(bundle);
    const dataLoadMs = performance.now() - loadStarted;

    const sessionStarted = performance.now();
    session = await client.createSession(data, BASE, 86);
    const sessionBuildMs = performance.now() - sessionStarted;
    compiled = await client.compileStrategy(session, strategy);
    simulator = await client.createSimulator(session, compiled);

    const memoryBefore = await client.memoryStats();
    let progressEvents = 0;
    const started = performance.now();
    const result = await client.runStrategy(
        simulator,
        {
            target_runs: RUNS,
            seed: 31,
            retained_trace_count: PROFILE === "ui" ? 10 : 0,
            max_trace_entries: PROFILE === "ui" ? 1_000 : undefined,
            retained_success_count: PROFILE === "ui" ? 5 : 0,
            retained_failure_count: PROFILE === "ui" ? 5 : 0,
        },
        {
            chunkSize: PROFILE === "ui" ? Math.min(500, RUNS) : 5_000,
            onProgress: () => {
                progressEvents += 1;
            },
        },
    );
    const elapsedMs = performance.now() - started;
    const memoryAfter = await client.memoryStats();
    if (result.summary.completed_runs !== RUNS) {
        throw new Error("WASM benchmark did not complete every run");
    }
    if (progressEvents < 2) {
        throw new Error("WASM benchmark did not expose chunked progress");
    }

    const report = {
        schema_version: 1,
        engine: "wasm-worker",
        profile: PROFILE,
        runs: RUNS,
        data_load_ms: Number(dataLoadMs.toFixed(3)),
        session_build_ms: Number(sessionBuildMs.toFixed(3)),
        elapsed_ms: Number(elapsedMs.toFixed(3)),
        runs_per_second: Math.round((RUNS * 1000) / elapsedMs),
        actions_per_second: Math.round(
            (result.summary.total_actions * 1000) / elapsedMs,
        ),
        actions_per_run: ACTIONS_PER_RUN,
        action_type: ACTION_TYPE,
        total_actions: result.summary.total_actions,
        progress_events: progressEvents,
        wasm_memory_bytes_before: memoryBefore.wasm_memory_bytes,
        wasm_memory_bytes_after: memoryAfter.wasm_memory_bytes,
        wasm_memory_growth_bytes:
            memoryAfter.wasm_memory_bytes - memoryBefore.wasm_memory_bytes,
    };
    const payload = `${JSON.stringify(report, null, 2)}\n`;
    const output = process.env.POECRAFT_BENCH_OUTPUT;
    if (output) {
        const target = resolve(output);
        mkdirSync(dirname(target), { recursive: true });
        writeFileSync(target, payload);
    }
    process.stdout.write(payload);
} finally {
    if (simulator) await client.closeSimulator(simulator);
    if (compiled) await client.closeStrategy(compiled);
    if (session) await client.closeSession(session);
    if (data) await client.closeData(data);
    client.dispose();
    await worker.terminate();
}
