// Finite contracts through the real source-matched WASM worker.
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {CalculatorGoalSet, ClientMessage, WorkerMessage} from "../src/app/engine-protocol";
const root = new URL("../../../", import.meta.url);
const lock = JSON.parse(readFileSync(new URL("apps/web/runtime.lock.json", root), "utf8"));
const artifact = new URL(lock.runtime_directory + "/", root);
const text = (name: string) => readFileSync(new URL(name, artifact), "utf8");
const bundle = new TextEncoder().encode(`{"manifest":${text("manifest.json")},"strings":${text("strings.json")},"game_data":${text("game-data.json")}}`);
const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
const transport: EngineTransport = {
    postMessage: (message: ClientMessage, transfer?: Transferable[]) => worker.postMessage(message, (transfer ?? []) as unknown as TransferListItem[]),
    onMessage: handler => worker.on("message", handler), terminate: () => void worker.terminate(),
};
const client = new EngineClient(transport);
const sessions: number[] = [], items: number[] = [], contexts: number[] = [], pairs: number[] = [];
const close = (a: number, b: number) => Math.abs(a - b) < 1e-12;
try {
    await client.whenReady(); const data = await client.loadData(bundle);
    const aSession = await client.createSession(data, "Metadata/Items/Rings/Ring1", 60); sessions.push(aSession);
    const bSession = await client.createSession(data, "Metadata/Items/Rings/Ring2", 86); sessions.push(bSession);
    const a = await client.createItem(aSession, {rarity: "rare", withImplicits: true}); items.push(a);
    const b = await client.createItem(bSession, {rarity: "rare", withImplicits: true}); items.push(b);
    const before = [await client.exportItem(a, aSession), await client.exportItem(b, bSession)];
    const resources = [{identity: "input-a", role: "a", session: aSession, item: a},
        {identity: "input-b", role: "b", session: bSession, item: b}] as const;
    const pair = await client.openRecombinationPair({resources: [resources[0], resources[1]]}); pairs.push(pair);
    const goal: CalculatorGoalSet = {version: "calculator_goal_set_v1", actions: [], goals: [
        {id: "one", goal: {version: "v1", rarity: "rare", slots: [], allow_extra_modifiers: true}},
        {id: "two", goal: {version: "v1", rarity: "rare", slots: [], allow_extra_modifiers: true}},
    ]};
    const calc = await client.recombinationCalculate(pair, goal);
    assert.equal(calc.game_odds_estimated, true); assert.equal(calc.model_projection_exact, true);
    assert.equal(calc.cost_complete, false); assert.equal(calc.gold_cost, null); assert.equal(calc.dust_cost, null);
    assert.equal(calc.any_goal_probability, 1);
    assert.deepEqual(calc.goal_results?.map(value => value.success_probability), [1, 1]);
    assert.equal(new Set(calc.outcomes.map(value => value.state)).size, calc.outcomes.length);
    for (const carrier of [0, 1]) assert.ok(close(calc.outcomes.filter(row => row.carrier === carrier).reduce((sum, row) => sum + row.probability, 0), .5));
    assert.ok(calc.outcomes.every(row => row.item_level === 75 && row.matched_goal_ids?.length === 2));
    assert.deepEqual([await client.exportItem(a, aSession), await client.exportItem(b, bSession)], before);
    await assert.rejects(client.recombinationCalculate(pair, {...goal, actions: ["chaos"]}), /strategy actions/);
    await assert.rejects(client.openRecombinationPair({resources: [resources[0], {...resources[1], item: a, session: aSession}]}), /aliased/);
    const context = await client.createContext(aSession, 37); contexts.push(context);
    await assert.rejects(client.recombinationApply(pair, context, {resources: [...resources], output_identity: "input-a"}), /identity|collid/);
    assert.deepEqual([await client.exportItem(a, aSession), await client.exportItem(b, bSession)], before);
    const c = await client.importItem(before[0], aSession); items.push(c);
    const d = await client.importItem(before[1], bSession); items.push(d);
    const controlResources = [{...resources[0], identity: "control-a", item: c}, {...resources[1], identity: "control-b", item: d}];
    const controlPair = await client.openRecombinationPair({resources: [controlResources[0], controlResources[1]]}); pairs.push(controlPair);
    const controlContext = await client.createContext(aSession, 37); contexts.push(controlContext);
    const applied = await client.recombinationApply(pair, context, {resources: [...resources], output_identity: "output"});
    items.push(applied.output_item); sessions.push(applied.output_session);
    const control = await client.recombinationApply(controlPair, controlContext, {resources: controlResources, output_identity: "control-output"});
    items.push(control.output_item); sessions.push(control.output_session);
    assert.equal(applied.carrier, control.carrier, "refused collision preserves RNG");
    assert.equal(applied.item_level, 75); assert.equal(applied.cost_complete, false);
    assert.equal(applied.gold_cost, null); assert.equal(applied.dust_cost, null);
    assert.equal(applied.resources.length, 3);
    assert.deepEqual(applied.resources.slice(0, 2).map(resource => (resource.after as {lifecycle: number}).lifecycle), [1, 1]);
    const output = await client.exportItem(applied.output_item, applied.output_session);
    assert.equal((output as {item_level: number}).item_level, 75);
    assert.deepEqual(output, await client.exportItem(control.output_item, control.output_session));
    await assert.rejects(client.recombinationApply(pair, context, {resources: [...resources], output_identity: "replay"}), /stale|live|consumed|replay/);
    // Redo imports the stored output in its own interpreting session; it does
    // not re-run the pair law or consume a new RNG draw.
    const redo = await client.importItem(applied.resources[2].after, applied.output_session); items.push(redo);
    assert.deepEqual(await client.exportItem(redo, applied.output_session), output);
    console.log("Random recombination native pair/goal/atomic Apply worker contracts passed");
} finally {
    for (const pair of pairs) await client.closeRecombinationPair(pair);
    for (const item of items) await client.closeItem(item);
    for (const context of contexts) await client.closeContext(context);
    for (const session of sessions) await client.closeSession(session);
    client.dispose(); await worker.terminate();
}
