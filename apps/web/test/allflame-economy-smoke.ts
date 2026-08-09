import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, type EngineTransport } from "../src/app/engine-client";
import type { ClientMessage, WorkerMessage } from "../src/app/engine-protocol";
import {
    EconomyService,
    MemoryEconomyCache,
} from "../src/app/workspace/economy-service";

const REPO_ROOT = new URL("../../../", import.meta.url);
const ARTIFACT_DIR = new URL("data/compiled/current/", REPO_ROOT);
const ECONOMY_ROOT = new URL("../public/economy/", import.meta.url);
const BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";
const ITEM_LEVEL = 86;

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

async function localEconomyFetch(input: string | URL): Promise<Response> {
    const url = new URL(String(input));
    const file = url.pathname.endsWith("/league-index.json")
        ? new URL("league-index.json", ECONOMY_ROOT)
        : new URL(`snapshots/${url.pathname.split("/").at(-1)}`, ECONOMY_ROOT);
    try {
        return new Response(readText(file), {
            status: 200,
            headers: { "content-type": "application/json" },
        });
    } catch {
        return new Response("missing", { status: 404 });
    }
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

const economyService = new EconomyService({
    indexUrl: "https://economy.test/league-index.json",
    fetch: localEconomyFetch,
    cache: new MemoryEconomyCache(),
    broadcast: false,
});
await economyService.initialize();
assert.equal(economyService.getState().selectedProfile, "allflame");
const pinned = economyService.pin(["chaos"]);
assert.equal(
    pinned.sourceSnapshotId,
    "economy:allflame:a122cad9494aa3361016b6f9c542e029e7aa1465de6d04bd6b5b150b5d26c485",
);
assert.equal(pinned.snapshot.prices.chaos, 1);

const { client, worker } = spawnClient();
await client.whenReady();
let data = 0;
let session = 0;
let context = 0;
let item = 0;
let solver = 0;
let economy = 0;
try {
    data = await client.loadData(buildBundle());
    session = await client.createSession(data, BASE, ITEM_LEVEL);
    context = await client.createContext(session, 1);
    item = await client.createItem(session, {
        rarity: "rare",
        withImplicits: false,
    });
    const pool = await client.debugPool(context, item, {
        action: { type: "chaos" },
        side: "prefix",
    });
    assert.ok(pool.entries.length > 0);
    const goalMod = await client.modInfo(session, pool.entries[0].session_mod_id);
    solver = await client.openSolver(session, {
        version: "v1",
        rarity: "rare",
        slots: [{ family_mod_key: goalMod.key, min_tier: 0 }],
        actions: ["chaos"],
    });
    const actions = await client.solverActions(solver);
    assert.deepEqual(actions.map((action) => action.id), ["chaos"]);
    assert.deepEqual(actions[0].cost_keys, ["chaos"]);

    economy = await client.loadEconomy(pinned.snapshot);
    const solve = await client.solverSolve(solver, item, economy, {
        max_states: 10_000,
        max_discovered_states: 10_000,
        max_expanded_states: 10_000,
        max_state_action_rows: 10_000,
        max_transitions: 500_000,
        max_reforge_work: 500_000,
        max_solver_owned_bytes: 128 * 1024 * 1024,
    });
    assert.equal(solve.cancelled, false);
    assert.equal(solve.supported_priced_actions, 1);
    assert.equal(solve.skipped_missing_price_actions, 0);
    assert.ok(solve.start_value !== null && solve.start_value > 0);
    console.log(
        `ok - Calculator pinned ${pinned.sourceSnapshotId} and native Solve used chaos=${pinned.snapshot.prices.chaos}`,
    );
} finally {
    if (economy) await client.closeEconomy(economy);
    if (solver) await client.closeSolver(solver);
    if (item) await client.closeItem(item);
    if (context) await client.closeContext(context);
    if (session) await client.closeSession(session);
    if (data) await client.closeData(data);
    client.dispose();
    await worker.terminate();
}
