import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {parseHTML} from "linkedom";
import {readClusterCatalog, validClusterSelection, clusterEnchantmentText} from "../src/app/cluster-configuration";
import {supportedBasePickerBases} from "../src/app/base-picker-model";
import {itemSnapshotCluster} from "../src/app/workspace/persistence";
import {readItemCard} from "../src/app/item-preview";
import type {EngineClient} from "../src/app/engine-client";
import type {BaseInfo, ClusterConfiguration} from "../src/app/engine-protocol";

const root = new URL("../../../data/compiled/current/", import.meta.url);
const bundle = new TextEncoder().encode(JSON.stringify({strings: JSON.parse(readFileSync(new URL("strings.json", root), "utf8")),
    game_data: JSON.parse(readFileSync(new URL("game-data.json", root), "utf8"))}));
const catalogs = readClusterCatalog(bundle);
assert.equal(Object.keys(catalogs).length, 3);
assert.equal(Object.values(catalogs).reduce((n, c) => n + c.passives.length, 0), 55);
const path = "Metadata/Items/Jewels/JewelPassiveTreeExpansionSmall";
const catalog = catalogs[path];
const attack = catalog.passives.find(p => p.key === "affliction_chance_to_block_attack_damage")!;
const spell = catalog.passives.find(p => p.key === "affliction_chance_to_block_spell_damage")!;
assert.equal(attack.tag, spell.tag);
assert.notEqual(attack.key, spell.key);
assert.notDeepEqual(attack.text, spell.text);
const configuration = {passiveKey: attack.key, passiveCount: 2};
assert.equal(validClusterSelection(catalog, configuration), true);
for (const config of [{...configuration, passiveCount: 2.5}, {...configuration, passiveCount: 4}, {...configuration, passiveKey: attack.tag}])
    assert.equal(validClusterSelection(catalog, config), false);
for (const passive of catalog.passives.filter(p => p.tag.startsWith("old_do_not_use_")))
    assert.equal(validClusterSelection(catalog, {passiveKey: passive.key, passiveCount: 2}), false);
const base: BaseInfo = {path, name: "Small Cluster Jewel", item_class_key: "Jewel", drop_level: 1, support: 1, cluster: catalog};
assert.deepEqual(supportedBasePickerBases([base]), []);
assert.deepEqual(supportedBasePickerBases([base], true), [base]);
const state = {cluster: {passive_key: attack.key, passive_count: 2}};
assert.deepEqual(itemSnapshotCluster({state}), configuration);
assert.deepEqual(itemSnapshotCluster(JSON.parse(JSON.stringify({state, cluster: configuration}))), configuration);

// Shared previews must reopen the saved configuration, including state-only records.
const calls: unknown[][] = [];
const client = {createSession: async (...args: unknown[]) => { calls.push(args); return 1; },
    importItem: async () => 2, itemInfo: async () => ({prefix_mod_ids: [], suffix_mod_ids: [], implicit_mod_ids: [],
        fractured_prefix_mod_ids: [], fractured_suffix_mod_ids: [], max_prefix: 2, max_suffix: 2}),
    closeItem: async () => {}, closeSession: async () => {}} as unknown as EngineClient;
await readItemCard(client, 3, null, {base: path, itemLevel: 84, state}, "Saved cluster");
assert.deepEqual(calls, [[3, path, 84, configuration]]);

const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
Object.assign(globalThis, {window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent, BroadcastChannel: undefined});
globalThis.fetch = async () => new Response(readFileSync(new URL("../public/game-assets/catalog.json", import.meta.url)));
const {PcBasePicker} = await import("../src/app/components/pc-base-picker");
const picker = new PcBasePicker();
picker.setAttribute("allow-clusters", "");
picker.setBases([base]);
let selected: unknown;
picker.addEventListener("confirm", e => {selected = (e as CustomEvent).detail;});
for (const config of [undefined, {...configuration, passiveCount: 2.5}, {...configuration, passiveKey: attack.tag}]) {
    picker.setSelection(path, 84, config);
    assert.equal(picker.querySelector<HTMLButtonElement>(".pc-bp-confirm")!.disabled, true);
}
const selectedConfig: ClusterConfiguration = {passiveKey: spell.key, passiveCount: 3};
picker.setSelection(path, 84, selectedConfig);
assert.match(picker.textContent!, /Small passive type/);
const confirm = picker.querySelector<HTMLButtonElement>(".pc-bp-confirm")!;
assert.equal(confirm.disabled, false);
confirm.click();
assert.deepEqual(selected, {base: path, itemLevel: 84, cluster: selectedConfig});
console.log("  ok - cluster catalog identity, picker validation and saved preview transport");


// Undo between two configurations of the same base must reopen the saved
// native session before interpreting its dense modifier IDs.
const {PcEmulator} = await import("../src/app/components/pc-emulator");
const {setWorkspace} = await import("../src/app/workspace/registry");
const {EditHistory} = await import("../src/app/edit-history");
const emulator = new PcEmulator();
const history = new EditHistory<any>();
const previousFrame = {snapshot: {base: path, itemLevel: 84, cluster: configuration, state},
    entry: {action: "Opened", applied: true, added: 0, removed: 0}};
const currentFrame = {...previousFrame, snapshot: {...previousFrame.snapshot, cluster: selectedConfig,
    state: {cluster: {passive_key: spell.key, passive_count: 3}}}};
history.reset(previousFrame); history.record(currentFrame);
const undoCalls: unknown[][] = [];
const access = emulator as unknown as {restoreHistory(i: number): Promise<void>; cluster?: ClusterConfiguration; session: number; item: number};
setWorkspace({notifyDirty: () => {}} as any);
Object.assign(access, {base: path, itemLevel: 84, cluster: selectedConfig, dataId: 1, item: 2, session: 3, context: 4,
    undoHistory: history, modCache: [], syncControls: () => {}, refresh: async () => {}, persist: async () => {},
    client: {createSession: async (...args: unknown[]) => {undoCalls.push(["session", ...args]); return 10;},
        createContext: async () => 11, modCount: async () => 0,
        importItem: async (...args: unknown[]) => {undoCalls.push(["import", ...args]); return 12;}, itemInfo: async () => ({}),
        closeItem: async () => {}, closeSession: async () => {}, closeContext: async () => {}}});
await access.restoreHistory(0);
assert.deepEqual(undoCalls, [["session", 1, path, 84, configuration], ["import", state, 10]]);
assert.deepEqual(access.cluster, configuration);
assert.equal(access.session, 10); assert.equal(access.item, 12);
assert.equal(history.cursor, 0);
setWorkspace(null);
console.log("  ok - Emulator Undo recreates the saved cluster configuration of the same base");

assert.deepEqual(clusterEnchantmentText({passive_count: 2, jewel_socket_count: 0, passive_text: ["Added Small Passive Skills grant: 4% increased maximum Life"]}),
    ["Adds 2 Passive Skills", "0 Added Passive Skills are Jewel Sockets", "Added Small Passive Skills grant: 4% increased maximum Life"]);
assert.deepEqual(clusterEnchantmentText(null), []);
const {createStrategyFromItemSnapshot, strategyClusterConfiguration} = await import("../src/app/strategy-model");
const document = createStrategyFromItemSnapshot({base:path,itemLevel:84,state}, () => undefined);
assert.deepEqual(document.base_state.cluster,{passive_key:attack.key,passive_count:2});
assert.deepEqual(strategyClusterConfiguration(JSON.parse(JSON.stringify(document))),configuration);
console.log("  ok - native fixed enchantments and configured authored strategy transport");
