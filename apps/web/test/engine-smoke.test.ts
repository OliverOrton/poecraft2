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
import { EngineError } from "../src/app/engine-protocol";
import type {
    ClientMessage,
    ModInfo,
    SolveProgress,
    WorkerMessage,
} from "../src/app/engine-protocol";
import { prepareSolverStrategy } from "../src/app/solve-workspace";
import { validateStrategy } from "../src/app/strategy-model";

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

function evaluatorStrategy(
    operation: Record<string, unknown> = { type: "chaos", params: {} },
): Record<string, unknown> {
    return {
        version: "v1",
        name: "Evaluate one reforge",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: ITEM_LEVEL,
            rarity: "rare",
        },
        nodes: [
            { id: "start", kind: "start" },
            { id: "reforge", kind: "operation", operation },
            { id: "success", kind: "terminal", terminal: "success" },
            { id: "failure", kind: "terminal", terminal: "failure" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "reforge",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "three-prefixes",
                from: "reforge",
                to: "success",
                priority: 0,
                condition: { type: "prefix_count_range", min: 3, max: 3 },
            },
            {
                id: "otherwise",
                from: "reforge",
                to: "failure",
                priority: 999,
                is_default: true,
            },
        ],
    };
}

function alterationLoopStrategy(familyKey: string): Record<string, unknown> {
    return {
        version: "v1",
        name: "Exact Alteration loop",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: ITEM_LEVEL,
            rarity: "magic",
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "alteration",
                kind: "operation",
                operation: { type: "alteration", params: {} },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "alteration",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "hit",
                from: "alteration",
                to: "success",
                priority: 0,
                condition: {
                    type: "has_mod_family",
                    family_mod_key: familyKey,
                    min_tier: 1,
                },
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

function fallbackPhaseStrategy(
    familyKey: string,
    carrier: ModInfo,
): Record<string, unknown> {
    const hit = {
        type: "has_mod_family",
        family_mod_key: familyKey,
        min_tier: 1,
    };
    return {
        version: "v1",
        name: "Two-distribution fallback loop",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: ITEM_LEVEL,
            rarity: "magic",
            [carrier.generation_type === 0 ? "prefixes" : "suffixes"]: [
                {
                    mod_key: carrier.key,
                    fractured: true,
                },
            ],
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "alteration_a",
                kind: "operation",
                operation: { type: "alteration", params: {} },
            },
            {
                id: "alteration_b",
                kind: "operation",
                operation: { type: "alteration", params: {} },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "alteration_a",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "a_hit",
                from: "alteration_a",
                to: "success",
                priority: 0,
                condition: hit,
            },
            {
                id: "a_miss",
                from: "alteration_a",
                to: "alteration_b",
                priority: 999,
                is_default: true,
            },
            {
                id: "b_hit",
                from: "alteration_b",
                to: "success",
                priority: 0,
                condition: hit,
            },
            {
                id: "b_miss",
                from: "alteration_b",
                to: "alteration_a",
                priority: 999,
                is_default: true,
            },
        ],
    };
}

async function lowWeightAlterationMod(): Promise<ModInfo> {
    const item = await client.createItem(sessionId, {
        rarity: "magic",
        withImplicits: false,
    });
    try {
        const pool = await client.debugPool(contextId, item, {
            action: { type: "alteration" },
            side: "both",
        });
        const candidate = pool.entries
            .filter((entry) => entry.accepted && entry.final_weight > 0)
            .sort(
                (a, b) =>
                    a.final_weight - b.final_weight ||
                    a.session_mod_id - b.session_mod_id,
            )[0];
        assert.ok(candidate, "expected an Alteration pool candidate");
        return client.modInfo(sessionId, candidate.session_mod_id);
    } finally {
        await client.closeItem(item);
    }
}

async function lowWeightAlterationFamily(): Promise<string> {
    return (await lowWeightAlterationMod()).key;
}

async function strictAlterationCarrier(target: ModInfo): Promise<ModInfo> {
    const count = await client.modCount(sessionId);
    for (let id = 0; id < count; id += 1) {
        const candidate = await client.modInfo(sessionId, id);
        if (
            candidate.reach_kind === 0 &&
            candidate.required_level <= ITEM_LEVEL &&
            candidate.generation_type === 1 - target.generation_type &&
            candidate.family_id !== target.family_id &&
            candidate.primary_group_id !== target.primary_group_id
        ) {
            return candidate;
        }
    }
    throw new Error("expected a distinct opposite-side Alteration carrier");
}

function familyTierStrategy(
    familyKey: string,
    itemModKey: string,
    generationType: number,
    minTier: number,
    itemFractured = false,
    requireFractured = false,
): Record<string, unknown> {
    return {
        version: "v1",
        name: "Family tier condition",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: ITEM_LEVEL,
            rarity: "rare",
            [generationType === 0 ? "prefixes" : "suffixes"]: [
                {
                    mod_key: itemModKey,
                    ...(itemFractured ? { fractured: true } : {}),
                },
            ],
        },
        nodes: [
            { id: "start", kind: "start" },
            { id: "success", kind: "terminal", terminal: "success" },
            { id: "failure", kind: "terminal", terminal: "failure" },
        ],
        edges: [
            {
                id: "match",
                from: "start",
                to: "success",
                priority: 0,
                condition: {
                    type: "has_mod_family",
                    family_mod_key: familyKey,
                    min_tier: minTier,
                    ...(requireFractured ? { fractured: true } : {}),
                },
            },
            {
                id: "fallback",
                from: "start",
                to: "failure",
                priority: 999,
                is_default: true,
            },
        ],
    };
}

function phase13Strategy(): Record<string, unknown> {
    return {
        version: "v1",
        name: "Harvest once",
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: ITEM_LEVEL,
            rarity: "rare",
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "harvest",
                kind: "operation",
                operation: {
                    type: "harvest_reforge",
                    params: { target_tag: "life" },
                },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            { id: "begin", from: "start", to: "harvest" },
            { id: "done", from: "harvest", to: "success" },
        ],
    };
}

test("API shape: ABI version is current", async () => {
    assert.equal(client.getAbiVersion(), 2);
});

test("load data through the memory bundle path", async () => {
    dataId = await client.loadData(buildBundle());
    const summary = await client.dataSummary(dataId);
    assert.equal(summary.complete_dataset, true);
    assert.ok((summary.base_item_count as number) > 0);
    assert.ok((summary.mod_count as number) > 0);
    const bases = await client.listBases(dataId);
    const base = bases.find((entry) => entry.path === BASE);
    assert.equal(base?.support, 0);
    assert.equal(base?.drop_level, 68);
});

test("create a session and an item", async () => {
    sessionId = await client.createSession(dataId, BASE, ITEM_LEVEL);
    contextId = await client.createContext(sessionId, 1);
    assert.ok((await client.modCount(sessionId)) > 0);
    const firstMod = await client.modInfo(sessionId, 0);
    assert.equal(typeof firstMod.family_id, "number");
    assert.ok(firstMod.family_tier_index > 0);
    assert.ok(Array.isArray(firstMod.text_lines));
    assert.ok(Array.isArray(firstMod.classification_tags));
    const item = await client.createItem(sessionId, { rarity: "rare" });
    const info = await client.itemInfo(item);
    assert.equal(info.rarity, "rare");
    await client.closeItem(item);
});

test("emulator editing can add and remove an exact explicit mod", async () => {
    const count = await client.modCount(sessionId);
    let explicit: ModInfo | null = null;
    for (let id = 0; id < count; id += 1) {
        const mod = await client.modInfo(sessionId, id);
        if (mod.generation_type === 0 || mod.generation_type === 1) {
            explicit = mod;
            break;
        }
    }
    assert.ok(explicit);
    const side = explicit.generation_type === 0 ? "prefix" : "suffix";
    const item = await client.createItem(sessionId, {
        rarity: "rare",
        withImplicits: false,
    });
    await client.addMod(item, sessionId, { key: explicit.key, side });
    let info = await client.itemInfo(item);
    assert.ok(
        (info[`${side}_mod_ids`] as number[]).includes(explicit.session_mod_id),
    );
    await client.setModFractured(item, {
        modId: explicit.session_mod_id,
        side,
    });
    info = await client.itemInfo(item);
    assert.ok(
        (info[`fractured_${side}_mod_ids`] as number[]).includes(
            explicit.session_mod_id,
        ),
    );
    await client.removeMod(item, {
        modId: explicit.session_mod_id,
        side,
    });
    info = await client.itemInfo(item);
    assert.ok(
        !(info[`${side}_mod_ids`] as number[]).includes(explicit.session_mod_id),
    );
    await client.closeItem(item);
});

test("Fracturing Orb fractures one random explicit modifier", async () => {
    const item = await client.createItem(sessionId, {
        rarity: "normal",
        withImplicits: false,
    });
    const alchemy = await client.apply(contextId, item, { type: "alchemy" });
    assert.equal(alchemy.applied, true);
    const before = await client.itemInfo(item);
    const beforeIds = [
        ...(before.prefix_mod_ids as number[]),
        ...(before.suffix_mod_ids as number[]),
    ];
    assert.ok(beforeIds.length >= 4);

    const fracture = await client.apply(contextId, item, { type: "fracture" });
    assert.deepEqual(fracture, { applied: true, added: 0, removed: 0 });
    const after = await client.itemInfo(item);
    const afterIds = [
        ...(after.prefix_mod_ids as number[]),
        ...(after.suffix_mod_ids as number[]),
    ];
    const fracturedIds = [
        ...(after.fractured_prefix_mod_ids as number[]),
        ...(after.fractured_suffix_mod_ids as number[]),
    ];
    assert.deepEqual(afterIds.sort((a, b) => a - b), beforeIds.sort((a, b) => a - b));
    assert.equal(fracturedIds.length, 1);
    assert.equal(
        (await client.apply(contextId, item, { type: "fracture" })).applied,
        false,
    );
    await client.closeItem(item);
});

test("catalog exposes mod groups, essences, and fossils as usable keys", async () => {
    const catalog = await client.catalog(dataId);
    assert.ok(catalog.groupKeyById.length > 0);
    assert.ok(catalog.essences.length > 0);
    assert.ok(catalog.fossils.length > 0);
    assert.ok(catalog.bench.length > 0);
    assert.ok(catalog.harvestTags.some((entry) => entry.key === "life"));
    assert.equal(
        catalog.harvestTags.find((entry) => entry.key === "defences")?.code,
        3,
    );
    assert.equal(
        catalog.harvestTags.find((entry) => entry.key === "elemental")?.code,
        1,
    );
    assert.ok(!catalog.harvestTags.some((entry) => entry.key === "resistance"));
    assert.ok(catalog.influences.some((entry) => entry.key === "crusader"));
    assert.equal(
        catalog.influences.find((entry) => entry.key === "shaper")?.code,
        6,
    );
    assert.equal(
        catalog.influences.find((entry) => entry.key === "adjudicator")?.code,
        1,
    );
    assert.ok(catalog.essences.every((entry) => entry.key && entry.name));
    assert.ok(catalog.fossils.every((entry) => entry.key && entry.name));

    // The mod-group dropdown stores groupKeyById[mod.primary_group_id] into a
    // has_mod_group condition. Prove that key actually resolves in the native
    // condition compiler — the round trip the UI depends on.
    const mod = await client.modInfo(sessionId, 0);
    const groupKey = catalog.groupKeyById[mod.primary_group_id];
    assert.ok(groupKey, "session mod maps to a catalog group key");

    const strategy = repeatStrategy();
    (strategy.edges as Array<Record<string, unknown>>)[1].condition = {
        type: "has_mod_group",
        group: groupKey,
    };
    const compiled = await client.compileStrategy(sessionId, strategy);
    assert.ok(compiled > 0);
    await client.closeStrategy(compiled);
});

test("WASM Fossil and Essence transitions ignore metamods but keep fractures", async () => {
    const catalog = await client.catalog(dataId);
    const fossil = catalog.fossils.find((entry) =>
        entry.key.endsWith("CurrencyDelveCraftingRandom"),
    );
    const essence = catalog.essences[0];
    assert.ok(fossil);
    assert.ok(essence);

    const lockKeys = [
        "StrMasterItemGenerationCannotChangePrefixes",
        "DexMasterItemGenerationCannotChangeSuffixes",
    ];
    const assertRenewalRemovesLocks = async (
        action: Parameters<typeof client.apply>[2],
    ): Promise<void> => {
        const item = await client.createItem(sessionId, {
            rarity: "rare",
            withImplicits: false,
        });
        try {
            for (const key of lockKeys) {
                await client.addMod(item, sessionId, { key });
            }
            const before = await client.itemInfo(item, sessionId);
            const lockIds = [
                ...(before.prefix_mod_ids as number[]),
                ...(before.suffix_mod_ids as number[]),
            ];
            assert.equal(lockIds.length, 2);
            assert.equal((await client.apply(contextId, item, action)).applied, true);
            const after = await client.itemInfo(item, sessionId);
            const live = new Set([
                ...(after.prefix_mod_ids as number[]),
                ...(after.suffix_mod_ids as number[]),
            ]);
            for (const id of lockIds) assert.equal(live.has(id), false);
        } finally {
            await client.closeItem(item);
        }
    };
    await assertRenewalRemovesLocks({
        type: "fossil",
        fossils: [fossil.key],
    });
    await assertRenewalRemovesLocks({
        type: "essence",
        essence: essence.key,
    });

    for (const action of [
        { type: "fossil" as const, fossils: [fossil.key] },
        { type: "essence" as const, essence: essence.key },
    ]) {
        const item = await client.createItem(sessionId, {
            rarity: "rare",
            withImplicits: false,
        });
        try {
            await client.addMod(item, sessionId, {
                key: "LocalIncreasedEnergyShield11",
                fractured: true,
            });
            const before = await client.itemInfo(item, sessionId);
            const fracturedId = [
                ...(before.fractured_prefix_mod_ids as number[]),
                ...(before.fractured_suffix_mod_ids as number[]),
            ][0];
            assert.notEqual(fracturedId, undefined);
            assert.equal((await client.apply(contextId, item, action)).applied, true);
            const after = await client.itemInfo(item, sessionId);
            assert.ok(
                [
                    ...(after.fractured_prefix_mod_ids as number[]),
                    ...(after.fractured_suffix_mod_ids as number[]),
                ].includes(fracturedId),
            );
        } finally {
            await client.closeItem(item);
        }
    }

    const chaosItem = await client.createItem(sessionId, {
        rarity: "rare",
        withImplicits: false,
    });
    try {
        for (const key of lockKeys) {
            await client.addMod(chaosItem, sessionId, { key });
        }
        const before = await client.itemInfo(chaosItem, sessionId);
        const lockIds = new Set([
            ...(before.prefix_mod_ids as number[]),
            ...(before.suffix_mod_ids as number[]),
        ]);
        assert.equal((await client.apply(contextId, chaosItem, { type: "chaos" })).applied, true);
        const after = await client.itemInfo(chaosItem, sessionId);
        const live = new Set([
            ...(after.prefix_mod_ids as number[]),
            ...(after.suffix_mod_ids as number[]),
        ]);
        for (const id of lockIds) assert.equal(live.has(id), true);
    } finally {
        await client.closeItem(chaosItem);
    }
});

test("Phase 13 mechanics run through the WASM worker boundary", async () => {
    let item = await client.createItem(sessionId, { rarity: "rare" });
    const bench = await client.apply(contextId, item, {
        type: "bench",
        mod_key: "StrMasterItemGenerationCannotChangePrefixes",
    });
    assert.equal(bench.applied, true);
    const veiled = await client.apply(contextId, item, {
        type: "veiled_exalt",
    });
    assert.equal(veiled.applied, true);
    let info = await client.itemInfo(item, sessionId);
    const options = info.veiled_option_mod_ids as number[];
    assert.equal(options.length, 3);
    const choice = await client.modInfo(sessionId, options[0]);
    assert.equal(
        (
            await client.apply(contextId, item, {
                type: "unveil",
                mod_key: choice.key,
            })
        ).applied,
        true,
    );
    await client.closeItem(item);

    item = await client.createItem(sessionId, { rarity: "rare" });
    assert.equal(
        (
            await client.apply(contextId, item, {
                type: "harvest_reforge",
                target_tag: "life",
            })
        ).applied,
        true,
    );
    await client.closeItem(item);

    item = await client.createItem(sessionId, { rarity: "rare" });
    assert.equal(
        (
            await client.apply(contextId, item, {
                type: "eldritch_ember",
                tier: 1,
            })
        ).applied,
        true,
    );
    info = await client.itemInfo(item, sessionId);
    assert.equal(info.searing_exarch_tier, 1);
    assert.ok((info.implicit_mod_ids as number[]).length > 0);
    await client.closeItem(item);

    item = await client.createItem(sessionId, { rarity: "rare" });
    assert.equal(
        (
            await client.apply(contextId, item, {
                type: "influence_exalt",
                influence: "crusader",
            })
        ).applied,
        true,
    );
    info = await client.itemInfo(item, sessionId);
    assert.notEqual(info.generic_influence_bits, 0);
    await client.closeItem(item);
});

test("Phase 13 strategy operations run natively through WASM", async () => {
    const strategy = await client.compileStrategy(sessionId, phase13Strategy());
    const simulator = await client.createSimulator(sessionId, strategy);
    const result = await client.runStrategy(simulator, {
        target_runs: 5,
        seed: 7,
    });
    assert.equal(result.summary.success_count, 5);
    assert.equal(result.summary.total_actions, 5);
    await client.closeSimulator(simulator);
    await client.closeStrategy(strategy);
});

test("exact strategy evaluation agrees with a 5k-run simulation", async () => {
    const document = evaluatorStrategy();
    const economy = {
        version: "v1",
        id: "wasm-s8.4-accounting",
        prices: { chaos: 2 },
    };
    const reviewProjection = {
        schema_version: "solver_review_projection_v1",
        raw_strategy: { execution_authority: "raw_strategy_only" },
        sections: [
            {
                id: "setup",
                label: "Setup",
                role: "setup",
                raw_references: [
                    { node_id: "start" },
                    { edge_id: "begin" },
                ],
            },
            {
                id: "rolling",
                label: "One reforge",
                role: "rolling",
                raw_references: [
                    { node_id: "reforge" },
                    { edge_id: "three-prefixes" },
                    { edge_id: "otherwise" },
                ],
            },
            {
                id: "success",
                label: "Success",
                role: "finishing",
                raw_references: [{ node_id: "success" }],
            },
            {
                id: "failure",
                label: "Failure",
                role: "recovery",
                raw_references: [{ node_id: "failure" }],
            },
        ],
    };
    const exact = await client.strategyEvaluate(
        sessionId,
        document,
        { top_classes_per_node: 4, include_success_normalized: true },
        { economy, reviewProjection },
    );
    const terminalTotal =
        exact.terminals.success +
        exact.terminals.failure +
        exact.terminals.stop +
        exact.terminals.action_not_applied +
        exact.terminals.no_matching_edge +
        exact.terminals.unresolved;
    assert.ok(
        Math.abs(terminalTotal - 1) < 1e-3,
        `terminal probabilities sum to ${terminalTotal}`,
    );
    assert.ok(exact.edges.some((edge) => edge.expected_traversals > 0));
    assert.ok(exact.expected_actions > 0);
    assert.ok(
        exact.expected_consumption.some(
            (entry) => entry.key === "chaos" && entry.quantity > 0,
        ),
    );
    assert.equal(exact.accounting.version, "s8.4_v1");
    assert.equal(exact.accounting.pricing.status, "complete");
    assert.equal(exact.accounting.pricing.economy_id, "wasm-s8.4-accounting");
    assert.equal(exact.accounting.totals.per_invocation.expected_actions, 1);
    assert.equal(exact.accounting.totals.per_invocation.total_expected_cost, 2);
    assert.equal(exact.accounting.actions.per_invocation.length, 1);
    assert.equal(exact.accounting.actions.per_invocation[0].id, "chaos");
    assert.equal(exact.accounting.actions.per_invocation[0].expected_visits, 1);
    assert.deepEqual(exact.accounting.materials.per_invocation, [
        {
            price_key: "chaos",
            expected_quantity: 1,
            price_status: "priced",
            unit_price: 2,
            cost_contribution: 2,
        },
    ]);
    assert.ok(exact.accounting.totals.success_normalized);
    assert.equal(exact.accounting.review_sections.enabled, true);
    assert.equal(
        exact.accounting.review_sections.items.reduce(
            (sum, section) => sum + section.per_invocation.expected_actions,
            0,
        ),
        1,
    );
    assert.deepEqual(exact.accounting.reconciliation, {
        action_descriptor_visits_difference: 0,
        action_descriptor_applied_difference: 0,
        node_operation_visits_difference: 0,
        material_quantity_differences: { chaos: 0 },
        cost_dot_product_difference: 0,
        section_actions_difference: 0,
        section_material_differences: { chaos: 0 },
    });

    const strategy = await client.compileStrategy(sessionId, document);
    const simulator = await client.createSimulator(sessionId, strategy);
    const run = await client.runStrategy(simulator, {
        target_runs: 5000,
        seed: 20260714,
    });
    const empiricalSuccess = run.summary.success_count / 5000;
    assert.ok(
        Math.abs(empiricalSuccess - exact.terminals.success) < 0.03,
        `empirical success ${empiricalSuccess} vs exact ${exact.terminals.success}`,
    );
    assert.ok(
        Math.abs(run.summary.total_actions / 5000 - exact.expected_actions) <
            0.03,
    );
    assert.equal(run.sampled_accounting.evidence_source, "simulator_sample");
    assert.equal(run.sampled_accounting.sample_count, 5000);
    assert.equal(run.sampled_accounting.seed, 20260714);
    assert.equal(run.sampled_accounting.actions[0].action_id, "chaos");
    assert.equal(
        run.sampled_accounting.actions[0].count,
        run.summary.total_actions,
    );
    assert.equal(run.sampled_accounting.materials[0].price_key, "chaos");
    assert.equal(
        run.sampled_accounting.materials[0].count,
        run.summary.total_actions,
    );
    await client.closeSimulator(simulator);
    await client.closeStrategy(strategy);
});

test("S6 Phase 4 exact evaluation supports veiled reforges", async () => {
    const exact = await client.strategyEvaluate(
        sessionId,
        evaluatorStrategy({ type: "veiled_chaos", params: {} }),
    );
    const terminalTotal =
        exact.terminals.success +
        exact.terminals.failure +
        exact.terminals.stop +
        exact.terminals.action_not_applied +
        exact.terminals.no_matching_edge +
        exact.terminals.unresolved;
    assert.ok(Math.abs(terminalTotal - 1) < 1e-3);
    assert.ok(exact.expected_actions > 0);
});

test("S6 Phase 4 condition vocabulary compiles through WASM", async () => {
    const mod = await client.modInfo(sessionId, 0);
    const catalog = await client.catalog(dataId);
    const group = catalog.groupKeyById[mod.primary_group_id];
    assert.ok(group);
    const document = repeatStrategy();
    (document.edges as Array<Record<string, unknown>>)[0].condition = {
        type: "any",
        conditions: [
            { type: "has_mod_group", group, min_tier: 1 },
            { type: "mod_count", mod_keys: [mod.key], min: 0, max: 1 },
            { type: "item_flag", flag: "prefixes_locked" },
            { type: "influence_bits", value: 0 },
            { type: "eldritch_tier", side: "searing", min: 0, max: 4 },
            { type: "has_unveil_option", mod_key: mod.key },
        ],
    };
    const strategy = await client.compileStrategy(sessionId, document);
    assert.ok(strategy > 0);
    await client.closeStrategy(strategy);
});

test("exact evaluation reports capacity without double-prefixing", async () => {
    let caught: unknown;
    try {
        await client.strategyEvaluate(sessionId, evaluatorStrategy(), {
            max_transitions: 1,
        });
    } catch (error) {
        caught = error;
    }
    assert.ok(caught instanceof EngineError);
    assert.equal(caught.code, 8);
    assert.match(caught.detail, /max_transitions/);
    assert.doesNotMatch(caught.detail, /poecraft engine error/);
    assert.equal(
        caught.message.match(/poecraft engine error 8/g)?.length,
        1,
    );
    assert.doesNotMatch(caught.message, /bad_alloc/);
});

test("stepped exact evaluation reports ordered progress", async () => {
    const family = await lowWeightAlterationFamily();
    const progress: Array<{
        phase: string;
        discovered: number;
        solved: number;
    }> = [];
    const result = await client.strategyEvaluate(
        sessionId,
        alterationLoopStrategy(family),
        undefined,
        {
            chunkSize: 1,
            onProgress: (event) =>
                progress.push({
                    phase: event.phase,
                    discovered: event.discovered_pairs,
                    solved: event.solved_sccs,
                }),
        },
    );
    assert.equal(result.converged, true);
    assert.ok(progress.length >= 2, "expected at least two progress events");
    assert.equal(progress[0].phase, "discovery");
    assert.equal(progress.at(-1)?.phase, "done");
    for (let i = 1; i < progress.length; i += 1) {
        assert.ok(
            progress[i].discovered >= progress[i - 1].discovered,
            "discovered pair count must not go backwards",
        );
        assert.ok(
            progress[i].solved >= progress[i - 1].solved,
            "solved SCC count must not go backwards",
        );
    }
});

test("exact evaluation cancellation is prompt and leaks no handles", async () => {
    const family = await lowWeightAlterationFamily();
    const graph = alterationLoopStrategy(family);
    const baseline = await client.memoryStats();
    assert.ok(baseline.wasm_memory_bytes > 0);
    assert.ok(baseline.native_live_owned_bytes > 0);
    assert.ok(
        baseline.native_peak_owned_bytes >= baseline.native_live_owned_bytes,
    );
    assert.equal(
        baseline.scope,
        "facade_registries_plus_solver_and_evaluator_owned_allocations",
    );
    for (let attempt = 0; attempt < 6; attempt += 1) {
        const controller = new AbortController();
        const started = performance.now();
        await assert.rejects(
            client.strategyEvaluate(sessionId, graph, undefined, {
                chunkSize: 1,
                signal: controller.signal,
                onProgress: () => controller.abort(),
            }),
            /cancelled/,
        );
        assert.ok(
            performance.now() - started < 1000,
            "cancelled evaluation should abandon promptly",
        );
    }
    const after = await client.memoryStats();
    assert.equal(after.live_handles, baseline.live_handles);
    assert.ok(after.native_live_owned_bytes > 0);
    assert.ok(after.native_peak_owned_bytes >= after.native_live_owned_bytes);
    assert.ok(after.native_serialized_output_bytes >= 0);
});

test("fallback-capable evaluation completes or cancels promptly", async () => {
    const target = await lowWeightAlterationMod();
    const carrier = await strictAlterationCarrier(target);
    const controller = new AbortController();
    const phases: string[] = [];
    let fallbackAt = 0;
    let completedExactly = false;
    let completionSummary: Record<string, unknown> | null = null;
    let caught: unknown;
    try {
        const result = await client.strategyEvaluate(
            sessionId,
            fallbackPhaseStrategy(target.key, carrier),
            { epsilon: 1e-30, max_sweeps: 100_000 },
            {
                signal: controller.signal,
                onProgress: (event) => {
                    phases.push(event.phase);
                    if (event.phase === "fallback" && fallbackAt === 0) {
                        fallbackAt = performance.now();
                        controller.abort();
                    }
                },
            },
        );
        completedExactly =
            result.converged &&
            result.residual_mass === 0 &&
            Math.abs(result.terminals.success - 1) < 1e-9;
        completionSummary = {
            converged: result.converged,
            residual_mass: result.residual_mass,
            terminals: result.terminals,
        };
    } catch (error) {
        caught = error;
    }
    if (fallbackAt > 0) {
        assert.match(String(caught), /cancelled/);
        assert.ok(phases.includes("fallback"), "expected fallback progress");
        assert.ok(
            performance.now() - fallbackAt < 1000,
            "fallback cancellation should abandon promptly",
        );
    } else {
        assert.equal(caught, undefined);
        assert.ok(
            completedExactly,
            `direct exact evaluation should complete: ${
                JSON.stringify(completionSummary)
            }`,
        );
    }
});

test("a burst of obsolete evaluations leaves only the newest graph", async () => {
    const family = await lowWeightAlterationFamily();
    const controllerA = new AbortController();
    const controllerB = new AbortController();
    const first = client.strategyEvaluate(
        sessionId,
        alterationLoopStrategy(family),
        undefined,
        { chunkSize: 1, signal: controllerA.signal },
    );
    controllerA.abort();
    const second = client.strategyEvaluate(
        sessionId,
        alterationLoopStrategy(family),
        undefined,
        { chunkSize: 1, signal: controllerB.signal },
    );
    controllerB.abort();
    const newest = client.strategyEvaluate(sessionId, evaluatorStrategy());
    const settled = await Promise.allSettled([first, second, newest]);
    assert.equal(settled[0].status, "rejected");
    assert.equal(settled[1].status, "rejected");
    assert.equal(settled[2].status, "fulfilled");
    if (settled[2].status === "fulfilled") {
        assert.equal(settled[2].value.converged, true);
        assert.equal(settled[2].value.expected_actions, 1);
    }
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
    assert.equal(result.action_distribution.length, 1);
    assert.equal(
        result.action_distribution[0].count,
        result.summary.total_actions,
    );
    assert.equal(result.traces.length, 3);
    assert.equal(result.traces[0].entries[0].matched_edge_id, "begin");
    assert.equal(result.examples.success.length, 2);
    assert.ok(progress.length >= 2, "expected multiple progress callbacks");
    assert.equal(progress.at(-1), 2000);
    await client.closeSimulator(simulator);
    await client.closeEconomy(economy);

    const otherEconomy = await client.loadEconomy({
        version: "v1",
        id: "fixture-other-league",
        prices: { chaos: 5 },
    });
    const otherSimulator = await client.createSimulator(
        sessionId,
        strategy,
        otherEconomy,
    );
    const other = await client.runStrategy(otherSimulator, {
        target_runs: 2000,
        seed: 42,
        max_actions_per_run: 100,
        retained_trace_count: 3,
        retained_success_count: 2,
    });
    assert.equal(other.summary.success_count, result.summary.success_count);
    assert.equal(other.summary.total_actions, result.summary.total_actions);
    assert.deepEqual(other.action_distribution, result.action_distribution);
    assert.ok(
        Math.abs(
            other.summary.known_total_cost -
                result.summary.known_total_cost * 2.5,
        ) < 1e-6,
        "league prices change costs without changing transitions",
    );
    await client.closeSimulator(otherSimulator);
    await client.closeEconomy(otherEconomy);
    await client.closeStrategy(strategy);
});

test("published-size economy metadata loads without exhausting the WASM stack", async () => {
    const keys = Array.from(
        { length: 1800 },
        (_, index) => `missing:fixture-key-${index.toString().padStart(4, "0")}`,
    );
    const economy = await client.loadEconomy({
        version: "v1",
        id: "economy:large-metadata-fixture",
        metadata: {
            schema_version: 1,
            missing_keys: keys,
            low_confidence_keys: keys.slice(0, 500),
        },
        prices: { chaos: 1, alteration: 0.2 },
    });
    await client.closeEconomy(economy);
});

test("modifier family minimum tier executes in the native WASM simulator", async () => {
    const count = await client.modCount(sessionId);
    const mods = await Promise.all(
        Array.from({ length: count }, (_, id) => client.modInfo(sessionId, id)),
    );
    const tierTwo = mods.find(
        (mod) =>
            (mod.generation_type === 0 || mod.generation_type === 1) &&
            mod.family_tier_index === 2,
    );
    assert.ok(tierTwo, "expected a display family with a second tier");
    const tierOne = mods.find(
        (mod) =>
            mod.family_id === tierTwo.family_id &&
            mod.family_tier_index === 1,
    );
    assert.ok(tierOne, "expected the selected family's first tier");

    for (const [threshold, expectedSuccess] of [
        [2, 1],
        [1, 0],
    ] as const) {
        const strategy = await client.compileStrategy(
            sessionId,
            familyTierStrategy(
                tierOne.key,
                tierTwo.key,
                tierTwo.generation_type,
                threshold,
            ),
        );
        const simulator = await client.createSimulator(sessionId, strategy);
        const result = await client.runStrategy(simulator, {
            target_runs: 1,
            retained_trace_count: 1,
        });
        assert.equal(result.summary.success_count, expectedSuccess);
        await client.closeSimulator(simulator);
        await client.closeStrategy(strategy);
    }

    for (const [itemFractured, expectedSuccess] of [
        [false, 0],
        [true, 1],
    ] as const) {
        const strategy = await client.compileStrategy(
            sessionId,
            familyTierStrategy(
                tierOne.key,
                tierTwo.key,
                tierTwo.generation_type,
                2,
                itemFractured,
                true,
            ),
        );
        const simulator = await client.createSimulator(sessionId, strategy);
        const result = await client.runStrategy(simulator, {
            target_runs: 1,
            retained_trace_count: 1,
        });
        assert.equal(result.summary.success_count, expectedSuccess);
        await client.closeSimulator(simulator);
        await client.closeStrategy(strategy);
    }
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

test("solver runs in the browser runtime: odds, solve, compiled policy", async () => {
    // Goal: the family of the first prefix in the live normal pool.
    const item = await client.createItem(sessionId, {
        rarity: "rare",
        withImplicits: false,
    });
    const pool = await client.debugPool(contextId, item, {
        action: { type: "exalt" },
        side: "prefix",
    });
    assert.ok(pool.entries.length > 0);
    const goalMod = await client.modInfo(
        sessionId,
        pool.entries[0].session_mod_id,
    );

    const solver = await client.openSolver(sessionId, {
        version: "v1",
        rarity: "rare",
        slots: [{ family_mod_key: goalMod.key, min_tier: 0 }],
        actions: [
            "transmute", "augment", "alteration", "regal", "alchemy",
            "chaos", "exalt", "annul", "scour", "restart",
        ],
    });

    const actions = await client.solverActions(solver);
    assert.equal(actions.length, 10);
    const restart = actions.find((action) => action.id === "restart");
    assert.ok(restart?.synthetic);
    assert.deepEqual(restart?.cost_keys, ["base"]);
    await assert.rejects(
        client.solverCompileStrategy(solver),
        /solve|policy|available/i,
        "raw strategy transfer must preserve native compile errors",
    );

    // Calculator: exact exalt odds before you click.
    const odds = await client.solverCalc(solver, item, "exalt");
    assert.equal(odds.supported, true);
    assert.equal(odds.legal, true);
    const total = odds.outcomes.reduce(
        (sum, outcome) => sum + outcome.probability,
        0,
    );
    assert.ok(Math.abs(total - 1) < 1e-9);
    assert.ok(odds.slot_satisfied[0] > 0 && odds.slot_satisfied[0] < 1);
    assert.ok(
        Math.abs(odds.success_probability - odds.slot_satisfied[0]) < 1e-9,
    );

    // The combined threshold is engine-owned, including finished rarity.
    const suffixPool = await client.debugPool(contextId, item, {
        action: { type: "exalt" },
        side: "suffix",
    });
    const suffixGoalMod = await client.modInfo(
        sessionId,
        suffixPool.entries[0].session_mod_id,
    );
    const partialSolver = await client.openSolver(sessionId, {
        version: "v1",
        rarity: "rare",
        min_satisfied_slots: 1,
        slots: [
            { family_mod_key: goalMod.key, min_tier: 0 },
            { family_mod_key: suffixGoalMod.key, min_tier: 0 },
        ],
        actions: ["exalt"],
    });
    const partialOdds = await client.solverCalc(partialSolver, item, "exalt");
    const summedPartialSuccess = partialOdds.outcomes.reduce(
        (sum, outcome) =>
            outcome.rarity === 2 &&
            outcome.slots.slice(0, 2).filter((status) => status === 2)
                .length >= 1
                ? sum + outcome.probability
                : sum,
        0,
    );
    assert.ok(
        Math.abs(partialOdds.success_probability - summedPartialSuccess) <
            1e-9,
    );
    // A multi-slot solve emits honest stepped progress and can be abandoned
    // promptly before policy extraction.
    const economySpec = {
        version: "v1",
        prices: {
            transmute: 0.1, augment: 0.5, alteration: 0.2, regal: 1.0,
            alchemy: 0.5, chaos: 1.0, exalt: 20.0, annul: 3.0,
            scour: 0.5, base: 5.0,
        },
    };
    const economy = await client.loadEconomy(economySpec);
    const controller = new AbortController();
    const cancelProgress: SolveProgress[] = [];
    const cancelStarted = performance.now();
    const cancelledSolve = await client.solverSolve(
        partialSolver,
        item,
        economy,
        undefined,
        {
            chunkSize: 1,
            signal: controller.signal,
            onProgress: (progress) => {
                cancelProgress.push(progress);
                if (cancelProgress.length >= 2) controller.abort();
            },
        },
    );
    assert.equal(cancelledSolve.cancelled, true);
    assert.ok(cancelledSolve.worker.step_count > 0);
    assert.ok(cancelledSolve.worker.max_step_ms > 0);
    assert.ok(cancelProgress.length >= 2);
    assert.ok(
        performance.now() - cancelStarted < 5000,
        "cancelled solve should abandon promptly",
    );
    await client.closeSolver(partialSolver);

    // Solve, then verify the compiled policy through the simulator.
    const solveProgress: SolveProgress[] = [];
    const solve = await client.solverSolve(solver, item, economy, undefined, {
        onProgress: (progress) => solveProgress.push(progress),
    });
    assert.equal(solve.cancelled, false);
    if (solve.cancelled) assert.fail("completed solve was cancelled");
    assert.ok(solveProgress.length >= 2);
    assert.ok(
        solveProgress.some((progress) => progress.phase === "expanding"),
    );
    assert.ok(
        solveProgress.some((progress) => progress.phase === "iterating") ||
            solve.residual === 0,
        "solve should expose iteration or finish with a zero exactness gap",
    );
    assert.equal(solveProgress.at(-1)?.phase, "done");
    assert.equal(solve.converged, true);
    assert.ok(solve.start_value !== null && solve.start_value > 0);
    assert.equal(solve.policy_available, true);
    assert.equal(solve.policy_status, "exact");
    assert.equal(solve.termination, "exact_closed");
    assert.equal(solve.stop_cause, "exact_closed");
    assert.equal(solve.cap_hit_mask, 0);
    assert.ok(solve.registry_actions > solve.candidate_actions);
    assert.equal(solve.candidate_actions, 10);
    assert.equal(solve.evaluator_supported_actions, 10);
    assert.equal(solve.supported_priced_actions, 10);
    assert.equal(solve.skipped_missing_price_actions, 0);
    assert.equal(solve.skipped_unsupported_actions, 0);
    assert.equal(solve.lower_bound, solve.start_value);
    assert.equal(solve.upper_bound, solve.start_value);
    assert.equal(solve.evaluated_policy_cost, solve.start_value);
    assert.equal(solve.absolute_optimality_gap, 0);
    assert.equal(solve.relative_optimality_gap, 0);
    assert.equal(solve.target_met, false);
    assert.equal(solve.target_fired, "none");
    assert.equal(solveProgress.at(-1)?.lower_bound, solve.start_value);
    assert.equal(solveProgress.at(-1)?.upper_bound, solve.start_value);
    assert.equal(solve.skipped_actions, 0);
    assert.ok(solve.worker.step_count > 0);
    assert.ok(solve.worker.max_step_ms > 0);

    const solveTelemetry = await client.solverTelemetry(solver);
    assert.equal(solveTelemetry.version, "solver_telemetry_v1");
    assert.equal(
        (solveTelemetry.actions as Record<string, unknown>).candidate,
        10,
    );
    assert.equal(
        (solveTelemetry.optimization as Record<string, unknown>).status,
        "exact_abstract",
    );

    const startState = await client.solverProject(solver, item);
    assert.equal(startState, solve.start_state);
    const startValue = await client.solverStateValue(solver, startState);
    assert.equal(startValue.value, solve.start_value);
    assert.ok(startValue.action !== null);

    const log = await client.solverLog(solver);
    assert.ok(log.split("\n").filter(Boolean).length === solve.expanded_states);

    const transferStarted = performance.now();
    const compiled = await client.solverCompileStrategy(solver);
    const transferMs = performance.now() - transferStarted;
    const compiledTelemetry = await client.solverTelemetry(solver);
    const compilation = compiledTelemetry.compilation as Record<string, unknown>;
    assert.equal(
        compilation.available,
        true,
    );
    assert.ok((compilation.strategy_json_bytes as number) > 0);
    assert.ok(Number.isFinite(transferMs));
    console.log(
        `    solver strategy transfer: ${(compilation.strategy_json_bytes as number).toLocaleString()} bytes in ${transferMs.toFixed(2)} ms`,
    );
    const prepared = prepareSolverStrategy(compiled);
    assert.ok(
        prepared.nodes.every(
            (node) =>
                Number.isFinite(node.position.x) &&
                Number.isFinite(node.position.y),
        ),
        "solver policies receive board positions before opening",
    );
    assert.ok(
        prepared.nodes.some(
            (node) =>
                node.kind === "operation" &&
                typeof node.expected_cost === "number",
        ),
        "operation nodes preserve native expected_cost annotations",
    );
    assert.deepEqual(
        validateStrategy(prepared).filter((issue) => issue.severity === "error"),
        [],
        "the prepared policy is Strategy Board-valid",
    );
    const reloaded = JSON.parse(
        JSON.stringify(prepared),
    ) as typeof prepared;
    assert.deepEqual(
        validateStrategy(reloaded).filter((issue) => issue.severity === "error"),
        [],
        "the compiled policy survives workspace-style JSON save/reload",
    );
    const exact = await client.strategyEvaluate(
        sessionId,
        reloaded,
        undefined,
        { economy: economySpec },
    );
    assert.equal(exact.converged, true);
    assert.ok(exact.terminals.success > 1 - 1e-9);
    assert.ok(exact.terminals.failure < 1e-12);
    assert.ok(exact.terminals.action_not_applied < 1e-12);
    assert.ok(exact.terminals.no_matching_edge < 1e-12);
    assert.ok(exact.terminals.unresolved < 1e-12);
    assert.equal(exact.accounting.totals.per_invocation.cost_complete, true);
    const exactCost =
        exact.accounting.totals.per_invocation.total_expected_cost;
    assert.ok(exactCost !== null);
    assert.ok(
        Math.abs(exactCost! - solve.evaluated_policy_cost!) < 1e-7,
        `exact compiled cost ${exactCost} vs solver policy cost ${solve.evaluated_policy_cost}`,
    );
    const strategy = await client.compileStrategy(sessionId, reloaded);
    const simulator = await client.createSimulator(
        sessionId,
        strategy,
        economy,
    );
    const run = await client.runStrategy(simulator, {
        target_runs: 5000,
        seed: 20260707,
        max_actions_per_run: 100000,
    });
    assert.equal(run.cancelled, false);
    assert.equal(run.summary.completed_runs, 5000);
    assert.equal(run.summary.success_count, 5000);
    const empirical = run.summary.known_total_cost / 5000;
    assert.ok(
        Math.abs(empirical - solve.start_value) < 0.5,
        `empirical ${empirical} vs V(start) ${solve.start_value}`,
    );

    await client.closeSimulator(simulator);
    await client.closeStrategy(strategy);
    await client.closeEconomy(economy);
    const beforeSolverClose = await client.memoryStats();
    await client.closeSolver(solver);
    const afterSolverClose = await client.memoryStats();
    assert.equal(
        afterSolverClose.live_handles,
        beforeSolverClose.live_handles - 1,
        "closing the scoped solver must release exactly one facade handle",
    );
    assert.ok(
        afterSolverClose.native_live_owned_bytes <
            beforeSolverClose.native_live_owned_bytes,
        "closing the scoped solver must release its selected live ownership",
    );
    assert.ok(
        afterSolverClose.wasm_memory_bytes >= beforeSolverClose.wasm_memory_bytes,
        "WASM linear-memory high-water is not expected to shrink after close",
    );
    console.log(
        `    solver close: handles ${beforeSolverClose.live_handles} -> ${afterSolverClose.live_handles}; live bytes ${beforeSolverClose.native_live_owned_bytes.toLocaleString()} -> ${afterSolverClose.native_live_owned_bytes.toLocaleString()}; peak ${afterSolverClose.native_peak_owned_bytes.toLocaleString()}; WASM high-water ${beforeSolverClose.wasm_memory_bytes.toLocaleString()} -> ${afterSolverClose.wasm_memory_bytes.toLocaleString()}; max solve step ${solve.worker.max_step_ms.toFixed(2)} ms`,
    );
    await client.closeItem(item);
});

test("calculator picker: full-registry solver, filtered actions, fossil combos", async () => {
    const item = await client.createItem(sessionId, {
        rarity: "rare",
        withImplicits: false,
    });
    const pool = await client.debugPool(contextId, item, {
        action: { type: "exalt" },
        side: "prefix",
    });
    const goalMod = await client.modInfo(
        sessionId,
        pool.entries[0].session_mod_id,
    );

    // No `actions` subset: the Calculator tab keeps the full registry open.
    const solver = await client.openSolver(sessionId, {
        version: "v1",
        rarity: "rare",
        slots: [{ family_mod_key: goalMod.key, min_tier: 0 }],
    });

    const actions = await client.solverActions(solver, {
        omitFossilCombos: true,
    });
    assert.ok(actions.some((action) => action.id === "chaos"));
    assert.ok(actions.some((action) => action.id === "restart"));
    assert.ok(
        !actions.some(
            (action) =>
                action.id.startsWith("fossil:") && action.id.includes("+"),
        ),
        "picker list must not contain multi-fossil combos",
    );
    const singles = actions.filter((action) =>
        action.id.startsWith("fossil:"),
    );
    assert.ok(singles.length >= 2, "expected single-fossil actions");
    const essence = actions.find((action) => action.id.startsWith("essence:"));
    const chaos = actions.find((action) => action.id === "chaos");
    assert.ok(essence);
    assert.ok(chaos);
    for (const action of [singles[0], essence]) {
        assert.equal(action.preservation.destructive_renewal, true);
        assert.equal(action.preservation.preserves_fractured_affixes, true);
        assert.deepEqual(action.preservation.protection, {
            prefix_lock: false,
            suffix_lock: false,
            cannot_roll_attack: false,
            cannot_roll_caster: false,
        });
        assert.ok(action.preservation.can_destroy.includes("satisfied_goal_subset"));
        assert.ok(action.preservation.can_preserve.includes("fractured_state"));
    }
    assert.deepEqual(chaos.preservation.protection, {
        prefix_lock: true,
        suffix_lock: true,
        cannot_roll_attack: true,
        cannot_roll_caster: true,
    });
    const resistanceConversions = actions.filter((action) =>
        action.id.startsWith("harvest_resist:"),
    );
    assert.equal(resistanceConversions.length, 6);
    for (const action of resistanceConversions) {
        const target = action.id.split(":").at(-1);
        assert.deepEqual(action.cost_keys, [`harvest_resist:${target}`]);
    }

    // The tab reconstructs combo ids from single-fossil keys (sorted, joined
    // with "+", per solver_registry.cpp) — prove the round trip calculates.
    const keys = singles
        .slice(0, 2)
        .map((action) => action.id.slice("fossil:".length))
        .sort();
    const odds = await client.solverCalc(
        solver,
        item,
        `fossil:${keys.join("+")}`,
    );
    assert.equal(odds.supported, true);
    assert.equal(odds.legal, true);
    // Reforge evaluators carry a ≥99.5% coverage budget, and the WASM facade
    // rounds each probability to six decimals, so the sum is only near 1.
    const total = odds.outcomes.reduce(
        (sum, outcome) => sum + outcome.probability,
        0,
    );
    assert.ok(
        total > 0.995 && total < 1.001,
        `probabilities sum to ${total}`,
    );

    const focusedSolver = await client.openSolver(sessionId, {
        version: "v1",
        rarity: "rare",
        slots: [
            { family_mod_key: "LocalIncreasedEnergyShield11", min_tier: 1 },
            { family_mod_key: "ColdResist8", min_tier: 1 },
        ],
        action_mode: "goal_relevant",
        fossil_mode: "goal_relevant",
    });
    const focusedActions = await client.solverActions(focusedSolver);
    const focusedFossils = focusedActions.filter((action) =>
        action.id.startsWith("fossil:"),
    );
    assert.ok(focusedFossils.length > 0 && focusedFossils.length < 500);
    assert.deepEqual(
        Array.from(
            new Set(
                focusedFossils.map(
                    (action) => action.id.split("+").length,
                ),
            ),
        ).sort(),
        [1, 2, 3, 4],
        "product envelope must retain useful fossil leaders at every socket count",
    );
    assert.ok(
        focusedFossils.some(
            (action) =>
                action.id ===
                "fossil:Metadata/Items/Currency/CurrencyDelveCraftingCold+Metadata/Items/Currency/CurrencyDelveCraftingDefences",
        ),
        "focused ES/cold goal must retain the useful Frigid + Dense loadout",
    );
    for (const deferred of [
        "bench:StrMasterItemGenerationCannotChangePrefixes",
        "bench:DexMasterItemGenerationCannotChangeSuffixes",
        "bench:StrIntMasterItemGenerationCanHaveMultipleCraftedMods",
        "bench:IntMasterItemGenerationCannotRollAttackAffixes",
        "bench:StrDexMasterItemGenerationCannotRollCasterAffixes",
        "remove_crafted_modifiers",
    ]) {
        assert.ok(
            !focusedActions.some((action) => action.id === deferred),
            `${deferred} waits for its bounded S7.4 assembly route`,
        );
    }
    const primitiveFracture = focusedActions.find(
        (action) => action.id === "fracture",
    );
    assert.ok(
        primitiveFracture,
        "goal-relevant product mode exposes primitive Fracture directly",
    );
    assert.deepEqual(primitiveFracture.cost_keys, ["fracture"]);
    await client.closeSolver(focusedSolver);

    await client.closeSolver(solver);
    await client.closeItem(item);
});

test("automatic S8.3 bench selects and executes through WASM", async () => {
    const item = await client.createItem(sessionId, {
        rarity: "rare",
        withImplicits: false,
    });
    for (const key of [
        "LocalIncreasedEnergyShield11",
        "LocalIncreasedEnergyShieldPercent8",
        "LocalIncreasedEnergyShieldPercentAndStunRecovery6",
    ]) {
        await client.addMod(item, sessionId, { key });
    }
    const solver = await client.openSolver(sessionId, {
        version: "v1",
        rarity: "rare",
        slots: [
            {
                family_mod_key: "EinharMasterColdResist3__",
                min_tier: 3,
            },
        ],
        actions: ["restart"],
        automatic_candidates: true,
    });
    const economy = await client.loadEconomy({
        version: "v1",
        prices: {
            base: 100,
            "bench:EinharMasterColdResist3__": 0.5,
        },
    });
    const solve = await client.solverSolve(solver, item, economy, {
        max_states: 25000,
        max_discovered_states: 50000,
        max_expanded_states: 25000,
        max_state_action_rows: 250000,
        max_transitions: 5000000,
        max_reforge_work: 5000000,
    });
    assert.equal(solve.cancelled, false);
    if (solve.cancelled) assert.fail("automatic solve was cancelled");
    const telemetry = await client.solverTelemetry(solver);
    const control = (telemetry.action_control as Record<string, unknown>)
        .automatic_candidates as Record<string, unknown>;
    assert.equal(
        solve.policy_available,
        true,
        `automatic bench solve returned no executable policy: ${JSON.stringify(solve)}`,
    );
    assert.ok(
        solve.policy_status === "exact" ||
            solve.policy_status === "bounded_near_optimal" ||
            solve.policy_status === "bounded_feasible",
        `automatic bench solve returned an invalid policy status: ${JSON.stringify(solve)}`,
    );
    assert.equal(control.enabled, true);
    assert.ok((control.operators as number) > 0);
    const rows = control.rows as Record<string, number>;
    assert.ok(rows.selected > 0);

    const compiled = await client.solverCompileStrategy(solver);
    const prepared = prepareSolverStrategy(compiled);
    assert.ok(
        prepared.nodes.some(
            (node) =>
                node.kind === "operation" &&
                node.operation?.type === "bench",
        ),
        "automatic permanent bench should compile as the deterministic finish",
    );
    const strategy = await client.compileStrategy(sessionId, prepared);
    const simulator = await client.createSimulator(
        sessionId,
        strategy,
        economy,
    );
    const run = await client.runStrategy(simulator, {
        target_runs: 64,
        seed: 2026071803,
        max_actions_per_run: 1000,
    });
    assert.equal(run.cancelled, false);
    assert.equal(run.summary.completed_runs, 64);
    assert.equal(run.summary.success_count, 64);
    assert.equal(run.summary.action_not_applied_count, 0);
    assert.equal(run.summary.no_matching_edge_count, 0);

    await client.closeSimulator(simulator);
    await client.closeStrategy(strategy);
    await client.closeEconomy(economy);
    await client.closeSolver(solver);
    await client.closeItem(item);
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
