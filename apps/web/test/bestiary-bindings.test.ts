import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, type EngineTransport } from "../src/app/engine-client";
import type {
    ClientMessage,
    ModInfo,
    SolverGoal,
    WorkerMessage,
} from "../src/app/engine-protocol";
import { operationLabel } from "../src/app/strategy-model";

const REPO_ROOT = new URL("../../../", import.meta.url);
const ARTIFACT_DIR = new URL("data/compiled/current/", REPO_ROOT);
const BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";

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

function oneOperationStrategy(operation: string): Record<string, unknown> {
    return {
        version: "v1",
        name: operation,
        start_node_id: "start",
        base_state: {
            base_key: BASE,
            item_level: 86,
            rarity: "magic",
        },
        nodes: [
            { id: "start", kind: "start" },
            {
                id: "operation",
                kind: "operation",
                operation: { type: operation, params: {} },
            },
            { id: "success", kind: "terminal", terminal: "success" },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "operation",
                priority: 0,
                condition: { type: "always" },
            },
            {
                id: "done",
                from: "operation",
                to: "success",
                priority: 0,
                is_default: true,
            },
        ],
    };
}

const spawned = spawnClient();
const { client, worker } = spawned;

try {
    await client.whenReady();
    const data = await client.loadData(buildBundle());
    const presentation = await client.bestiaryPresentation(data);
    assert.deepEqual(
        presentation.actions.map((action) => action.id),
        ["bestiary:imprint", "bestiary:restore_imprint"],
    );
    assert.deepEqual(presentation.actions[0]?.cost_keys, [
        "beast:craicic-chimeral",
        "beast:rare",
        "beast:rare",
        "beast:rare",
    ]);
    assert.deepEqual(presentation.actions[1]?.cost_keys, []);
    assert.deepEqual(
        presentation.solver_options.map((option) => ({
            id: option.id,
            restriction: option.goal_restriction_key,
            rarity: option.goal_rarity,
            complete: option.requires_complete_goal,
            bounds: [option.min_program_actions, option.max_program_actions],
        })),
        [
            {
                id: "imprint_retry",
                restriction: "complete_magic_item_goal",
                rarity: "magic",
                complete: true,
                bounds: [1, 3],
            },
        ],
    );
    assert.equal(
        presentation.actions.some((action) => action.id.includes("prefix")),
        false,
    );

    const session = await client.createSession(data, BASE, 86);
    const context = await client.createContext(session, 17);
    const item = await client.createItem(session, {
        rarity: "magic",
        withImplicits: false,
    });
    assert.equal((await client.itemInfo(item)).checkpoint_present, false);

    const calculation = await client.bestiaryCalculate(
        data,
        item,
        "bestiary:imprint",
    );
    assert.equal(calculation.deterministic, true);
    assert.equal(calculation.probability, 1);
    assert.equal(calculation.result.applied, true);
    assert.deepEqual(
        calculation.result.consumed_price_keys,
        presentation.actions[0]?.cost_keys,
    );
    assert.equal(
        calculation.successor.bestiary.checkpoint_present,
        true,
    );
    assert.equal((await client.itemInfo(item)).checkpoint_present, false);

    const created = await client.bestiaryApply(data, item, "bestiary:imprint");
    assert.equal(created.applied, true);
    assert.equal(created.output_item_count, 1);
    assert.equal(created.output_checkpoint_count, 1);
    assert.equal(created.checkpoint_present, true);
    assert.equal((await client.itemInfo(item)).checkpoint_present, true);

    const refusedCreate = await client.bestiaryApply(
        data,
        item,
        "bestiary:imprint",
    );
    assert.equal(refusedCreate.applied, false);
    assert.equal(refusedCreate.refusal_key, "checkpoint_already_exists");
    assert.deepEqual(refusedCreate.consumed_price_keys, []);
    assert.equal(refusedCreate.checkpoint_present, true);

    const exported = await client.exportItem(item);
    const imported = await client.importItem(exported);
    assert.equal((await client.itemInfo(imported)).checkpoint_present, true);
    const importedRestore = await client.bestiaryApply(
        data,
        imported,
        "bestiary:restore_imprint",
    );
    assert.equal(importedRestore.applied, true);
    assert.equal(importedRestore.consumed_checkpoint_count, 1);
    assert.deepEqual(importedRestore.cost_keys, []);
    assert.deepEqual(importedRestore.consumed_price_keys, []);

    await client.apply(context, item, { type: "alteration" });
    const alteredInfo = await client.itemInfo(item);
    assert.ok(
        (alteredInfo.prefix_mod_ids as number[]).length +
            (alteredInfo.suffix_mod_ids as number[]).length >
            0,
    );
    const restored = await client.bestiaryApply(
        data,
        item,
        "bestiary:restore_imprint",
    );
    assert.equal(restored.applied, true);
    assert.equal(restored.consumed_checkpoint_count, 1);
    assert.equal(restored.checkpoint_present, false);
    const restoredInfo = await client.itemInfo(item);
    assert.equal(
        (restoredInfo.prefix_mod_ids as number[]).length +
            (restoredInfo.suffix_mod_ids as number[]).length,
        0,
    );

    await client.bestiaryApply(data, item, "bestiary:imprint");
    const identicalRestore = await client.bestiaryApply(
        data,
        item,
        "bestiary:restore_imprint",
    );
    assert.equal(identicalRestore.applied, true);
    assert.equal(identicalRestore.consumed_checkpoint_count, 1);
    const missingRestore = await client.bestiaryApply(
        data,
        item,
        "bestiary:restore_imprint",
    );
    assert.equal(missingRestore.applied, false);
    assert.equal(missingRestore.refusal_key, "checkpoint_missing");
    assert.deepEqual(missingRestore.consumed_price_keys, []);

    await client.bestiaryApply(data, item, "bestiary:imprint");
    assert.equal((await client.itemInfo(item)).checkpoint_present, true);
    await client.closeItem(item);
    const restarted = await client.createItem(session, {
        rarity: "magic",
        withImplicits: false,
    });
    assert.equal((await client.itemInfo(restarted)).checkpoint_present, false);

    assert.equal(
        operationLabel({ type: "bestiary:imprint", params: {} }),
        "Create Imprint",
    );
    assert.equal(
        operationLabel({
            type: "bestiary:restore_imprint",
            params: {},
        }),
        "Restore Imprint",
    );
    for (const operation of [
        "bestiary:imprint",
        "bestiary:restore_imprint",
    ]) {
        const strategy = await client.compileStrategy(
            session,
            oneOperationStrategy(operation),
        );
        await client.closeStrategy(strategy);
    }

    const modCount = await client.modCount(session);
    let goalMod: ModInfo | undefined;
    for (let id = 0; id < modCount; id += 1) {
        const candidate = await client.modInfo(session, id);
        if (
            candidate.generation_type === 0 || candidate.generation_type === 1
        ) {
            goalMod = candidate;
            break;
        }
    }
    assert.ok(goalMod);
    const goal: SolverGoal = {
        version: "v1",
        rarity: "magic",
        min_satisfied_slots: 1,
        slots: [
            {
                family_mod_key: goalMod.key,
                min_tier: 0,
            },
        ],
        actions: ["alteration"],
        options: [
            {
                type: "imprint_retry",
                actions: ["alteration"],
                until: { goal_slots: [0], min_satisfied: 1 },
            },
        ],
    };
    const solver = await client.openSolver(session, goal);
    assert.equal(
        (await client.solverActions(solver)).some((action) =>
            action.id.startsWith("bestiary:"),
        ),
        false,
    );
    await client.closeSolver(solver);

    await client.closeItem(restarted);
    await client.closeItem(imported);
    await client.closeContext(context);
    await client.closeSession(session);
    await client.closeData(data);
    console.log("ok - B1.4 Bestiary bindings and workspace contracts");
} finally {
    client.dispose();
    await worker.terminate();
}
