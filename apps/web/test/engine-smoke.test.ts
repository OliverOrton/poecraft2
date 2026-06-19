/*
 * Headless acceptance test for the Phase 8 WASM/worker runtime.
 *
 * Runs the engine compiled to WebAssembly inside a real Node worker_threads
 * worker, driven through the same EngineClient the browser UI uses. It covers
 * the acceptance gate:
 *   - load data, create a session/item, apply an action, query a debug pool;
 *   - WASM matches the native engine on the canonical spec pool fixtures
 *     (same rule-fixture check);
 *   - a long strategy run reports progress and cancels promptly (UI stays
 *     responsive).
 *
 * Run with: npm test  (tsx test/engine-smoke.test.ts)
 */

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, EngineTransport } from "../src/app/engine-client";
import type { ClientMessage, WorkerMessage } from "../src/app/engine-protocol";

const REPO_ROOT = new URL("../../../", import.meta.url);
const ARTIFACT_DIR = new URL("data/compiled/current/", REPO_ROOT);
const FIXTURES = new URL("fixtures/spec/session-pools/", REPO_ROOT);
const BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";
const ITEM_LEVEL = 86;

function readText(url: URL): string {
    return readFileSync(fileURLToPath(url), "utf8");
}

function buildBundle(): Uint8Array {
    const manifest = readText(new URL("manifest.json", ARTIFACT_DIR));
    const strings = readText(new URL("strings.json", ARTIFACT_DIR));
    const gameData = readText(new URL("game-data.json", ARTIFACT_DIR));
    const bundle = `{"manifest":${manifest},"strings":${strings},"game_data":${gameData}}`;
    return new TextEncoder().encode(bundle);
}

function readFixture(name: string): { summary: Record<string, number> } {
    return JSON.parse(readText(new URL(name, FIXTURES)));
}

function spawnClient(): { client: EngineClient; worker: Worker } {
    // A .mjs bootstrap registers tsx's loader, then imports the .ts worker.
    const worker = new Worker(
        new URL("./worker-bootstrap.mjs", import.meta.url),
    );
    worker.on("error", (error) => {
        console.error("worker error:", error);
        process.exitCode = 1;
    });
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

// --- tiny async test runner -------------------------------------------------

const tests: Array<{ name: string; fn: () => Promise<void> }> = [];
function test(name: string, fn: () => Promise<void>): void {
    tests.push({ name, fn });
}

// --- shared session ---------------------------------------------------------

let client: EngineClient;
let worker: Worker;
let dataId: number;
let sessionId: number;
let contextId: number;

function repeatStrategy(): Record<string, unknown> {
    return {
        version: "v1",
        name: "Chaos until three prefixes",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: ITEM_LEVEL,
            rarity: "rare",
        },
        nodes: [
            { id: "start", kind: "start" },
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
                to: "chaos",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "done",
                from: "chaos",
                to: "success",
                priority: 0,
                condition: {
                    type: "prefix_count_range",
                    min: 3,
                    max: 3,
                },
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

test("API shape: ABI version is current", async () => {
    assert.equal(client.getAbiVersion(), 1);
});

test("load data through the memory bundle path", async () => {
    dataId = await client.loadData(buildBundle());
    const summary = await client.dataSummary(dataId);
    assert.equal(summary.complete_dataset, true);
    assert.ok((summary.base_item_count as number) > 0);
    assert.ok((summary.mod_count as number) > 0);
});

test("create a session and an item", async () => {
    sessionId = await client.createSession(dataId, BASE, ITEM_LEVEL);
    contextId = await client.createContext(sessionId, 1);
    assert.ok((await client.modCount(sessionId)) > 0);
    const item = await client.createItem(sessionId, { rarity: "rare" });
    const info = await client.itemInfo(item);
    assert.equal(info.rarity, "rare");
    await client.closeItem(item);
});

test("apply an action mutates the item per the rules", async () => {
    const item = await client.createItem(sessionId, {
        rarity: "normal",
        withImplicits: true,
    });
    const result = await client.apply(contextId, item, { type: "alchemy" });
    assert.equal(result.applied, true);
    assert.ok(result.added > 0);
    const info = await client.itemInfo(item);
    assert.equal(info.rarity, "rare");
    await client.closeItem(item);
});

test("WASM debug pool matches the native spec fixtures", async () => {
    const item = await client.createItem(sessionId, {
        rarity: "rare",
        withImplicits: false,
    });
    for (const [name, side] of [
        ["vaal-regalia-ilvl-86-normal-prefix.json", "prefix"],
        ["vaal-regalia-ilvl-86-normal-suffix.json", "suffix"],
    ] as const) {
        const fixture = readFixture(name);
        const pool = await client.debugPool(contextId, item, {
            action: { type: "chaos" },
            side,
        });
        assert.equal(pool.summary.candidate_count, fixture.summary.total_count);
        assert.equal(
            pool.summary.combined_total_weight,
            fixture.summary.combined_total_weight,
        );
    }
    const combined = readFixture("vaal-regalia-ilvl-86-alchemy-combined.json");
    const pool = await client.debugPool(contextId, item, {
        action: { type: "alchemy" },
    });
    assert.equal(pool.summary.candidate_count, combined.summary.total_count);
    assert.equal(
        pool.summary.prefix_total_weight,
        combined.summary.prefix_total_weight,
    );
    assert.equal(
        pool.summary.suffix_total_weight,
        combined.summary.suffix_total_weight,
    );
    assert.equal(
        pool.summary.combined_total_weight,
        combined.summary.combined_total_weight,
    );
    await client.closeItem(item);
});

test("native strategy run reports progress, traces, and economy cost", async () => {
    const strategy = await client.compileStrategy(sessionId, repeatStrategy());
    const economy = await client.loadEconomy({
        version: "v1",
        prices: { chaos: 2 },
    });
    const simulator = await client.createSimulator(
        sessionId,
        strategy,
        economy,
    );
    const progress: number[] = [];
    const result = await client.runStrategy(
        simulator,
        {
            target_runs: 2000,
            seed: 42,
            max_actions_per_run: 100,
            retained_trace_count: 3,
            retained_success_count: 2,
        },
        { chunkSize: 500, onProgress: (p) => progress.push(p.done) },
    );
    assert.equal(result.cancelled, false);
    assert.equal(result.progress.completed_runs, 2000);
    assert.equal(result.summary.success_count, 2000);
    assert.ok(result.summary.total_actions > 2000);
    assert.equal(result.summary.cost_status, "complete");
    assert.equal(result.traces.length, 3);
    assert.equal(result.traces[0].entries[0].matched_edge_id, "begin");
    assert.equal(result.examples.success.length, 2);
    assert.ok(progress.length >= 2, "expected multiple progress callbacks");
    assert.equal(progress.at(-1), 2000);
    await client.closeSimulator(simulator);
    await client.closeEconomy(economy);
    await client.closeStrategy(strategy);
});

test("long strategy run cancels promptly via AbortSignal", async () => {
    const strategy = await client.compileStrategy(sessionId, repeatStrategy());
    const simulator = await client.createSimulator(sessionId, strategy);
    const controller = new AbortController();
    const result = await client.runStrategy(
        simulator,
        {
            target_runs: 5_000_000,
            seed: 7,
            max_actions_per_run: 100,
            retained_trace_count: 1,
        },
        {
            chunkSize: 250,
            signal: controller.signal,
            onProgress: () => controller.abort(),
        },
    );
    assert.equal(result.cancelled, true);
    assert.ok(
        result.progress.completed_runs < 5_000_000,
        "cancelled run should stop early",
    );
    assert.ok(
        result.progress.completed_runs > 0,
        "at least one chunk should have run",
    );
    await client.closeSimulator(simulator);
    await client.closeStrategy(strategy);
});

test("an already-aborted strategy run performs no simulations", async () => {
    const strategy = await client.compileStrategy(sessionId, repeatStrategy());
    const simulator = await client.createSimulator(sessionId, strategy);
    const controller = new AbortController();
    controller.abort();
    const result = await client.runStrategy(
        simulator,
        {
            target_runs: 1000,
            max_actions_per_run: 100,
        },
        { signal: controller.signal },
    );
    assert.equal(result.cancelled, true);
    assert.equal(result.progress.completed_runs, 0);
    assert.equal(result.summary.completed_runs, 0);
    await client.closeSimulator(simulator);
    await client.closeStrategy(strategy);
});

// Wire the shared client into the runner before executing.
{
    const spawned = spawnClient();
    client = spawned.client;
    worker = spawned.worker;
}

await client.whenReady();
{
    let passed = 0;
    try {
        for (const { name, fn } of tests) {
            await fn();
            console.log(`  ok - ${name}`);
            passed += 1;
        }
        console.log(`\n${passed}/${tests.length} passed`);
    } catch (error) {
        console.error(`\nFAILED: ${error instanceof Error ? error.stack : error}`);
        process.exitCode = 1;
    } finally {
        client.dispose();
        await worker.terminate();
    }
}
