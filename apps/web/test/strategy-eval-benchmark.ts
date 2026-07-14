/*
 * Opt-in Phase C.1 benchmark. It drives the same Node/WASM worker path as the
 * web smoke tests, excludes session construction, warms each graph, validates
 * exact fields independently of timing, and compares progress callbacks with
 * the same stepped path running callback-suppressed.
 *
 * Run with: npm run benchmark:strategy-eval
 */

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, EngineTransport } from "../src/app/engine-client";
import type {
    ClientMessage,
    StrategyEvalResult,
    WorkerMessage,
} from "../src/app/engine-protocol";

const REPO_ROOT = new URL("../../../", import.meta.url);
const ARTIFACT_DIR = new URL("data/compiled/current/", REPO_ROOT);
const BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";

interface BenchmarkCase {
    name: string;
    familyKey: string;
    expectedActions: number;
    baselineMs: number;
}

const CASES: BenchmarkCase[] = [
    {
        name: "T1 flat Energy Shield",
        familyKey: "LocalIncreasedEnergyShield11",
        expectedActions: 81.7014428412,
        baselineMs: 1434,
    },
    {
        name: "T1 hybrid Energy Shield/Life",
        familyKey: "LocalBaseEnergyShieldAndLife4_",
        expectedActions: 162.4028856824,
        baselineMs: 3510,
    },
];

function readText(url: URL): string {
    return readFileSync(fileURLToPath(url), "utf8");
}

function bundle(): Uint8Array {
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
        onError: (handler) => worker.on("error", handler),
        terminate: () => void worker.terminate(),
    };
    return { client: new EngineClient(transport), worker };
}

function graph(familyKey: string): Record<string, unknown> {
    return {
        version: "v1",
        name: "Pinned exact Alteration loop",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: 86,
            rarity: "normal",
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "transmute",
                kind: "operation",
                operation: { type: "transmute", params: {} },
            },
            {
                id: "alteration",
                kind: "operation",
                operation: { type: "alteration", params: {} },
            },
            {
                id: "regal",
                kind: "operation",
                operation: { type: "regal", params: {} },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "transmute",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "transmute_hit",
                from: "transmute",
                to: "regal",
                priority: 0,
                condition: {
                    type: "has_mod_family",
                    family_mod_key: familyKey,
                    min_tier: 1,
                },
            },
            {
                id: "transmute_miss",
                from: "transmute",
                to: "alteration",
                priority: 999,
                is_default: true,
            },
            {
                id: "alteration_hit",
                from: "alteration",
                to: "regal",
                priority: 0,
                condition: {
                    type: "has_mod_family",
                    family_mod_key: familyKey,
                    min_tier: 1,
                },
            },
            {
                id: "regal_done",
                from: "regal",
                to: "success",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "repeat",
                from: "alteration",
                to: "alteration",
                priority: 999,
                is_default: true,
            },
        ],
    };
}

function median(values: number[]): number {
    const sorted = [...values].sort((a, b) => a - b);
    return sorted[Math.floor(sorted.length / 2)];
}

function assertExact(result: StrategyEvalResult, benchmark: BenchmarkCase): void {
    assert.equal(result.converged, true);
    assert.ok(Math.abs(result.terminals.success - 1) < 1e-10);
    assert.ok(
        Math.abs(result.expected_actions - benchmark.expectedActions) < 1e-9,
        `${benchmark.name}: expected ${benchmark.expectedActions}, got ${result.expected_actions}`,
    );
    assert.equal(result.sweeps, 0, "pinned loops must avoid iterative fallback");
    const alteration = result.expected_consumption.find(
        (entry) => entry.key === "alteration",
    );
    assert.ok(alteration);
    assert.ok(
        Math.abs(alteration.quantity - (benchmark.expectedActions - 2)) < 1e-9,
    );
    const transmute = result.expected_consumption.find(
        (entry) => entry.key === "transmute",
    );
    const regal = result.expected_consumption.find(
        (entry) => entry.key === "regal",
    );
    assert.ok(transmute && Math.abs(transmute.quantity - 1) < 1e-10);
    assert.ok(regal && Math.abs(regal.quantity - 1) < 1e-10);
    const transmuteHit = result.edges.find(
        (edge) => edge.id === "transmute_hit",
    );
    const alterationHit = result.edges.find(
        (edge) => edge.id === "alteration_hit",
    );
    const repeat = result.edges.find((edge) => edge.id === "repeat");
    assert.ok(transmuteHit && alterationHit);
    assert.ok(
        Math.abs(
            transmuteHit.expected_traversals +
                alterationHit.expected_traversals -
                1,
        ) < 1e-10,
    );
    assert.ok(
        repeat &&
            Math.abs(
                repeat.expected_traversals -
                    (benchmark.expectedActions -
                        2 -
                        alterationHit.expected_traversals),
            ) < 1e-9,
    );
}

async function timed(
    client: EngineClient,
    session: number,
    benchmark: BenchmarkCase,
    reportProgress: boolean,
): Promise<{ elapsedMs: number; result: StrategyEvalResult }> {
    const started = performance.now();
    const result = await client.strategyEvaluate(
        session,
        graph(benchmark.familyKey),
        undefined,
        reportProgress ? { onProgress: () => {} } : undefined,
    );
    return { elapsedMs: performance.now() - started, result };
}

const { client, worker } = spawnClient();
await client.whenReady();
const data = await client.loadData(bundle());
const session = await client.createSession(data, BASE, 86);
try {
    const report: Array<Record<string, number | string>> = [];
    for (const benchmark of CASES) {
        for (let warm = 0; warm < 2; warm += 1) {
            assertExact((await timed(client, session, benchmark, false)).result, benchmark);
        }
        const suppressed: number[] = [];
        const reported: number[] = [];
        for (let sample = 0; sample < 7; sample += 1) {
            const withoutCallbacks = await timed(client, session, benchmark, false);
            assertExact(withoutCallbacks.result, benchmark);
            suppressed.push(withoutCallbacks.elapsedMs);
            const withCallbacks = await timed(client, session, benchmark, true);
            assertExact(withCallbacks.result, benchmark);
            reported.push(withCallbacks.elapsedMs);
        }
        const suppressedMedian = median(suppressed);
        const reportedMedian = median(reported);
        const overheadMs = reportedMedian - suppressedMedian;
        const allowedOverheadMs = Math.max(2, suppressedMedian * 0.02);
        const speedup = benchmark.baselineMs / suppressedMedian;
        assert.ok(
            speedup >= 5,
            `${benchmark.name} speedup ${speedup.toFixed(2)}x missed 5x`,
        );
        assert.ok(
            overheadMs <= allowedOverheadMs,
            `${benchmark.name} callback overhead ${overheadMs.toFixed(2)} ms exceeded ${allowedOverheadMs.toFixed(2)} ms`,
        );
        report.push({
            name: benchmark.name,
            baseline_ms: benchmark.baselineMs,
            stepped_median_ms: Number(suppressedMedian.toFixed(3)),
            progress_median_ms: Number(reportedMedian.toFixed(3)),
            progress_overhead_ms: Number(overheadMs.toFixed(3)),
            speedup_x: Number(speedup.toFixed(2)),
            expected_actions: benchmark.expectedActions,
        });
    }
    console.log(JSON.stringify({ strategy_eval_benchmark: report }, null, 2));
} finally {
    await client.closeSession(session);
    await client.closeData(data);
    await worker.terminate();
}
