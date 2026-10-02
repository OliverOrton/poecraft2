import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {ClientMessage, WorkerMessage, PoolEntry, ClusterConfiguration, CalculatorItemGoal} from "../src/app/engine-protocol";

const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
const transport: EngineTransport = {postMessage: (m: ClientMessage, t?: Transferable[]) => worker.postMessage(m, (t ?? []) as unknown as TransferListItem[]),
    onMessage: h => worker.on("message", (m: WorkerMessage) => h(m)), terminate: () => void worker.terminate()};
const client = new EngineClient(transport);
const root = new URL("../../../data/compiled/current/", import.meta.url);
const read = (name: string) => JSON.parse(readFileSync(new URL(name, root), "utf8"));
const bundle = new TextEncoder().encode(JSON.stringify({manifest: read("manifest.json"), strings: read("strings.json"), game_data: read("game-data.json")}));
const baseRoot = "Metadata/Items/Jewels/JewelPassiveTreeExpansion";
const configurations: Array<[string, ClusterConfiguration]> = [
    ["Small", {passiveKey: "affliction_maximum_life", passiveCount: 2}],
    ["Medium", {passiveKey: "affliction_fire_damage_over_time_multiplier", passiveCount: 4}],
    ["Large", {passiveKey: "affliction_axe_and_sword_damage", passiveCount: 8}],
];
const admitted = (rows: PoolEntry[]) => rows.filter(r => r.accepted && r.final_weight > 0);
const near = (a: number, b: number) => assert.ok(Math.abs(a - b) < 1e-12, `${a} != ${b}`);
try {
    await client.whenReady();
    const data = await client.loadData(bundle);
    const bases = await client.listBases(data);
    assert.equal(bases.filter(b => b.cluster).length, 3);
    for (const [size, configuration] of configurations) {
        for (const level of [1, 68, 84]) {
            const session = await client.createSession(data, baseRoot + size, level, configuration);
            const warm = await client.createContext(session, 17);
            const fresh = await client.createContext(session, 17);
            const item = await client.createItem(session, {rarity: "rare"});
            try {
                const info = await client.itemInfo(item, session);
                assert.equal(info.max_prefix, 2); assert.equal(info.max_suffix, 2);
                assert.equal((info.cluster as {jewel_socket_count: number}).jewel_socket_count, size === "Large" ? 2 : size === "Medium" ? 1 : 0);
                const initial = await client.debugPool(warm, item, {action: {type: "exalt"}});
                assert.deepEqual(initial.entries, (await client.debugPool(fresh, item, {action: {type: "exalt"}})).entries);
                assert.ok(admitted(initial.entries).length > 0);
                for (const row of admitted(initial.entries)) assert.ok((await client.modInfo(session, row.session_mod_id)).required_level <= level);
                await client.addMod(item, session, {key: admitted(initial.entries)[0].key});
                assert.deepEqual((await client.debugPool(warm, item, {action: {type: "exalt"}})).entries,
                    (await client.debugPool(fresh, item, {action: {type: "exalt"}})).entries);
                const snapshot = await client.exportItem(item, session) as Record<string, any>;
                assert.equal(snapshot.cluster.passive_key, configuration.passiveKey);
                assert.equal(snapshot.cluster.passive_count, configuration.passiveCount);
                const restored = await client.importItem(JSON.parse(JSON.stringify(snapshot)), session);
                assert.deepEqual(await client.exportItem(restored, session), snapshot);
                await client.closeItem(restored);
                for (const mutation of [
                    {...snapshot, cluster: {...snapshot.cluster, passive_count: configuration.passiveCount + 0.5}},
                    {...snapshot, cluster: {...snapshot.cluster, item_level: level + 1}},
                    {...snapshot, cluster: {...snapshot.cluster, passive_key: "affliction_chance_to_block"}},
                    {...snapshot, cluster: undefined}, {...snapshot, base_key: undefined},
                    {...snapshot, generic_influence_bits: 1},
                ]) await assert.rejects(client.importItem(mutation, session));
                await client.apply(warm, item, {type: "scour"});
                assert.deepEqual((await client.exportItem(item, session) as Record<string, unknown>).cluster, snapshot.cluster);
                assert.deepEqual((await client.debugPool(warm, item, {action: {type: "exalt"}})).entries, initial.entries);
                await assert.rejects(client.openSolver(session, {rarity: "rare", slots: [{family_mod_key: admitted(initial.entries)[0].key, min_tier: 1}], allow_extra_modifiers: true}), /explicit qualified primitive scope/);
                await assert.rejects(client.apply(warm, item, {type: "vaal"}), /not yet approved/);
            } finally {
                await client.closeItem(item); await client.closeContext(warm); await client.closeContext(fresh); await client.closeSession(session);
            }
        }
    }
    console.log("  ok - WASM configured pools, level gates, caps, cache parity and stable import");
    const attackSession = await client.createSession(data, baseRoot + "Small", 84,
        {passiveKey: "affliction_chance_to_block_attack_damage", passiveCount: 2});
    const spellSession = await client.createSession(data, baseRoot + "Small", 84,
        {passiveKey: "affliction_chance_to_block_spell_damage", passiveCount: 2});
    const attackItem = await client.createItem(attackSession);
    const spellItem = await client.createItem(spellSession);
    const attackState = await client.exportItem(attackItem, attackSession) as Record<string, any>;
    const spellState = await client.exportItem(spellItem, spellSession) as Record<string, any>;
    assert.equal(attackState.cluster.passive_tag, spellState.cluster.passive_tag);
    assert.notDeepEqual(attackState.cluster.passive_stats, spellState.cluster.passive_stats);
    await assert.rejects(client.importItem(attackState, spellSession), /configuration does not match/);
    await client.closeItem(attackItem); await client.closeItem(spellItem);
    await client.closeSession(attackSession); await client.closeSession(spellSession);
    console.log("  ok - WASM shared passive tags cannot substitute for configured identity");

    const session = await client.createSession(data, baseRoot + "Small", 84, configurations[0][1]);
    const context = await client.createContext(session, 0);
    const magic = await client.createItem(session, {rarity: "magic"});
    const normal = await client.createItem(session);
    const first = admitted((await client.debugPool(context, magic, {action: {type: "augment"}})).entries);
    const target = await client.modInfo(session, first.find(r => r.key.includes("AfflictionNotable"))!.session_mod_id);
    const total = first.reduce((s, r) => s + r.final_weight, 0);
    let one = 0, two = 0;
    for (const row of first) {
        const mod = await client.modInfo(session, row.session_mod_id);
        const p = row.final_weight / total;
        if (mod.family_id === target.family_id) {one += p; two += p; continue;}
        const next = await client.cloneItem(magic);
        await client.addMod(next, session, {key: mod.key});
        const second = admitted((await client.debugPool(context, next, {action: {type: "augment"}, side: mod.generation_type === 0 ? "suffix" : "prefix"})).entries);
        let weight = 0, goalWeight = 0;
        for (const r of second) {
            weight += r.final_weight;
            if ((await client.modInfo(session, r.session_mod_id)).family_id === target.family_id) goalWeight += r.final_weight;
        }
        two += p * (weight ? goalWeight / weight : 0);
        await client.closeItem(next);
    }
    const goal: CalculatorItemGoal = {rarity: "magic", slots: [{family_mod_key: target.key, min_tier: 1}], allow_extra_modifiers: true, automatic_candidates: false, actions: ["transmute", "restart"]};
    const calc = await client.openCalcGoal(session, goal);
    const result = await client.currencyCalc(calc, normal, "transmute");
    assert.equal(result.supported, true); assert.equal(result.legal, true);
    near(result.success_probability, (one + two) / 2);
    near(result.outcomes.reduce((s, r) => s + r.probability, 0), 1);
    const restart = await client.currencyCalc(calc, magic, "restart");
    assert.equal(restart.legal, true);
    assert.equal(restart.outcomes.length, 1);
    assert.equal(restart.outcomes[0].rarity, 0);
    assert.equal(restart.outcomes[0].prefixes + restart.outcomes[0].suffixes, 0);
    assert.equal((await client.exportItem(magic, session) as Record<string, any>).cluster.passive_key, configurations[0][1].passiveKey);
    await assert.rejects(client.currencyCalc(calc, normal, "vaal"), /not yet approved/);
    const rareGoal = await client.openCalcGoal(session, {rarity: "rare", slots: [], allow_extra_modifiers: true});
    const actions = await client.solverActions(rareGoal);
    assert.equal(actions.find(a => a.id === "alchemy")?.cluster_support, 7);
    assert.equal(actions.find(a => a.id === "foulborn_exalt")?.cluster_support, 7);
    assert.equal(actions.find(a => a.id === "restart")?.cluster_support, 2);
    const rare = await client.currencyCalc(rareGoal, normal, "alchemy");
    near(rare.outcomes.filter(r => r.prefixes + r.suffixes === 3).reduce((p,r) => p+r.probability,0), .65);
    near(rare.outcomes.filter(r => r.prefixes + r.suffixes === 4).reduce((p,r) => p+r.probability,0), .35);
    await client.closeSolver(rareGoal);
    const exactGoal = {...goal, automatic_candidates: false, actions: ["transmute", "alteration", "augment", "annul"]};
    const exact = await client.openSolver(session, exactGoal);
    const rows = await client.solverCalc(exact, normal, "transmute");
    near(rows.success_probability, result.success_probability);
    const document = {version: "v1", start_node_id: "s", base_state: {base_key: baseRoot+"Small", item_level:84,
        rarity:"normal", with_implicits:false, cluster:{passive_key:configurations[0][1].passiveKey,passive_count:2}},
        nodes:[{id:"s",kind:"start"},{id:"a",kind:"operation",operation:{type:"transmute"}},
            {id:"yes",kind:"terminal",terminal:"success"},{id:"no",kind:"terminal",terminal:"failure"}],
        edges:[{id:"e",from:"s",to:"a"},{id:"h",from:"a",to:"yes",priority:0,
            condition:{type:"has_mod_family",family_mod_key:target.key,min_tier:1}},
            {id:"f",from:"a",to:"no",priority:999,is_default:true}]};
    const evaluated = await client.strategyEvaluate(session,document,{max_states:5000,max_pairs:10000,max_transitions:50000,max_owned_bytes:128*1024*1024});
    near(evaluated.terminals.success, result.success_probability);
    await client.closeSolver(exact);
    await client.closeSolver(calc); await client.closeItem(magic); await client.closeItem(normal); await client.closeContext(context); await client.closeSession(session);
    console.log("  ok - WASM Calculator matches independent native two-draw enumeration and preserves restart identity");
    await client.closeData(data);
} finally {client.dispose(); await worker.terminate();}
