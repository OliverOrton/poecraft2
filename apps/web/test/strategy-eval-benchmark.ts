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

function rareReforgeGraph(): Record<string, unknown> {
    const condition = {
        type: "has_mod_family",
        family_mod_key: "LocalIncreasedEnergyShield11",
        min_tier: 1,
    };
    return {
        version: "v1",
        name: "Pinned exact rare-reforge loop",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: 86,
            rarity: "normal",
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "alchemy",
                kind: "operation",
                operation: { type: "alchemy", params: {} },
            },
            {
                id: "chaos",
                kind: "operation",
                operation: { type: "chaos", params: {} },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "alchemy",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "alchemy_hit",
                from: "alchemy",
                to: "success",
                priority: 0,
                condition,
            },
            {
                id: "alchemy_miss",
                from: "alchemy",
                to: "chaos",
                priority: 999,
                is_default: true,
            },
            {
                id: "chaos_hit",
                from: "chaos",
                to: "success",
                priority: 0,
                condition,
            },
            {
                id: "repeat",
                from: "chaos",
                to: "chaos",
                priority: 999,
                is_default: true,
            },
        ],
    };
}

function alchemyScourGraph(): Record<string, unknown> {
    const condition = {
        type: "all",
        conditions: [
            {
                type: "has_mod_family",
                family_mod_key: "LocalIncreasedEnergyShield11",
                min_tier: 1,
            },
            {
                type: "has_mod_family",
                family_mod_key: "LocalBaseEnergyShieldAndLife4_",
                min_tier: 1,
            },
        ],
    };
    return {
        version: "v1",
        name: "Pinned exact Alchemy-Scour loop",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: 86,
            rarity: "normal",
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "alchemy",
                kind: "operation",
                operation: { type: "alchemy", params: {} },
            },
            {
                id: "scour",
                kind: "operation",
                operation: { type: "scour", params: {} },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "alchemy",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "alchemy_hit",
                from: "alchemy",
                to: "success",
                priority: 0,
                condition,
            },
            {
                id: "alchemy_miss",
                from: "alchemy",
                to: "scour",
                priority: 999,
                is_default: true,
            },
            {
                id: "repeat",
                from: "scour",
                to: "alchemy",
                priority: 0,
                condition: { type: "always" },
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

function assertWideRareReforge(result: StrategyEvalResult): void {
    const expectedActions = 24.91546431794879;
    assert.equal(result.converged, true);
    assert.equal(
        result.sweeps,
        0,
        "the wide rare loop must use the direct solve",
    );
    assert.ok(Math.abs(result.terminals.success - 1) < 1e-10);
    assert.ok(Math.abs(result.expected_actions - expectedActions) < 1e-9);
    const alchemy = result.expected_consumption.find(
        (entry) => entry.key === "alchemy",
    );
    const chaos = result.expected_consumption.find(
        (entry) => entry.key === "chaos",
    );
    assert.ok(alchemy && Math.abs(alchemy.quantity - 1) < 1e-12);
    assert.ok(
        chaos && Math.abs(chaos.quantity - 23.915464317948782) < 1e-9,
    );
    const edge = (id: string): number | undefined =>
        result.edges.find((entry) => entry.id === id)?.expected_traversals;
    assert.ok(
        Math.abs((edge("alchemy_hit") ?? 0) - 0.040135716005084476) <
            1e-10,
    );
    assert.ok(
        Math.abs((edge("alchemy_miss") ?? 0) - 0.959864283994916) < 1e-10,
    );
    assert.ok(
        Math.abs((edge("chaos_hit") ?? 0) - 0.9598642839954707) < 1e-9,
    );
    assert.ok(Math.abs((edge("repeat") ?? 0) - 22.955600033932225) < 1e-9);
}

function assertAlchemyScour(result: StrategyEvalResult): void {
    const expectedActions = 2933.0267080405497;
    assert.equal(result.converged, true);
    assert.equal(
        result.sweeps,
        0,
        "deterministic Scour spokes must contract to a direct singleton solve",
    );
    assert.ok(Math.abs(result.terminals.success - 1) < 1e-10);
    assert.ok(Math.abs(result.expected_actions - expectedActions) < 1e-8);
    const alchemy = result.expected_consumption.find(
        (entry) => entry.key === "alchemy",
    );
    const scour = result.expected_consumption.find(
        (entry) => entry.key === "scour",
    );
    assert.ok(alchemy && scour);
    assert.ok(Math.abs(alchemy.quantity - scour.quantity - 1) < 1e-8);
    assert.ok(
        Math.abs(result.expected_actions - alchemy.quantity - scour.quantity) <
            1e-8,
    );
    const edge = (id: string): number | undefined =>
        result.edges.find((entry) => entry.id === id)?.expected_traversals;
    assert.ok(Math.abs((edge("begin") ?? 0) - 1) < 1e-12);
    assert.ok(Math.abs((edge("alchemy_hit") ?? 0) - 1) < 1e-8);
    assert.ok(Math.abs((edge("alchemy_miss") ?? 0) - scour.quantity) < 1e-8);
    assert.ok(Math.abs((edge("repeat") ?? 0) - scour.quantity) < 1e-8);
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
    const rareStarted = performance.now();
    const rare = await client.strategyEvaluate(session, rareReforgeGraph());
    const rareElapsedMs = performance.now() - rareStarted;
    assertWideRareReforge(rare);
    const alchemyScourStarted = performance.now();
    let alchemyScourDiscoveredPairs = 0;
    let alchemyScourDiscoveryMs = 0;
    const alchemyScour = await client.strategyEvaluate(
        session,
        alchemyScourGraph(),
        undefined,
        {
            onProgress: (progress) => {
                alchemyScourDiscoveredPairs = Math.max(
                    alchemyScourDiscoveredPairs,
                    progress.discovered_pairs,
                );
                if (
                    progress.phase !== "discovery" &&
                    alchemyScourDiscoveryMs === 0
                ) {
                    alchemyScourDiscoveryMs =
                        performance.now() - alchemyScourStarted;
                }
            },
        },
    );
    const alchemyScourElapsedMs = performance.now() - alchemyScourStarted;
    assertAlchemyScour(alchemyScour);
    assert.equal(alchemyScourDiscoveredPairs, 16_607);
    assert.ok(alchemyScourDiscoveryMs > 0);
    assert.ok(
        alchemyScourElapsedMs <= alchemyScourDiscoveryMs * 1.5 + 25,
        `Alchemy-Scour total ${alchemyScourElapsedMs.toFixed(1)} ms exceeded discovery ${alchemyScourDiscoveryMs.toFixed(1)} ms by too much`,
    );
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
    console.log(
        JSON.stringify(
            {
                wide_rare_reforge: {
                    elapsed_ms: Number(rareElapsedMs.toFixed(3)),
                    expected_actions: rare.expected_actions,
                    success: rare.terminals.success,
                },
                alchemy_scour: {
                    elapsed_ms: Number(alchemyScourElapsedMs.toFixed(3)),
                    discovery_ms: Number(alchemyScourDiscoveryMs.toFixed(3)),
                    discovered_pairs: alchemyScourDiscoveredPairs,
                    expected_actions: alchemyScour.expected_actions,
                    success: alchemyScour.terminals.success,
                    sweeps: alchemyScour.sweeps,
                },
                strategy_eval_benchmark: report,
            },
            null,
            2,
        ),
    );
} finally {
    await client.closeSession(session);
    await client.closeData(data);
    await worker.terminate();
}
