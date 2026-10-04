import assert from "node:assert/strict";
import {parseHTML} from "linkedom";
import type {Catalog, ModInfo} from "../src/app/engine-protocol";
import {createBlankStrategy, type StrategyDocument} from "../src/app/strategy-model";

const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
Object.assign(globalThis, {window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent, BroadcastChannel: undefined});
const {PcStrategyEditor} = await import("../src/app/components/pc-strategy-editor");
const catalog: Catalog = {groupKeyById: ["family"], groupNameById: ["Family"],
    essences: [], fossils: [], bench: [], harvestTags: [], influences: []};
const mod = (key: string): ModInfo => ({session_mod_id: 0, global_mod_id: 0, key,
    generation_type: 0, reach_kind: 0, reach_influence: 0, reach_via: "",
    primary_group_id: 0, family_id: 0, required_level: 1, group_display_name: "Family",
    family_tier_index: 1, text_lines: ["+1 to maximum Life"], classification_tags: []});
interface Access {
    client: unknown; catalog: Catalog; engineReady: boolean; strategy: StrategyDocument;
    disposed: boolean; modifierOptions: Array<{value: string}>; modifierBaseKey: string;
    modifierLoading: boolean; ensureModifiers(): Promise<void>;
}
const calls: unknown[][] = [], closed: number[] = [], releases: Array<() => void> = [];
const editor = new PcStrategyEditor() as unknown as Access;
const strategy = createBlankStrategy("Metadata/Items/Rings/Ring1", 80);
strategy.resources = [{id: "a", base_state: structuredClone(strategy.base_state), acquisition_price_key: "a"},
    {id: "b", base_state: structuredClone(strategy.base_state), acquisition_price_key: "b"}];
const resourcesBefore = structuredClone(strategy.resources);
Object.assign(editor, {catalog, engineReady: true, strategy, client: {
    createSession: async (...args: unknown[]) => {
        const id = calls.push(args);
        await new Promise<void>(resolve => {releases.push(resolve);});
        return id;
    }, modCount: async () => 1, modInfo: async (session: number) => mod(`mod-${session}`),
    closeSession: async (session: number) => {closed.push(session);},
}});
const settle = () => new Promise<void>(resolve => setImmediate(resolve));

// Stable ordinary requests must publish and cache once, rather than reject
// every result and recursively start another request.
const ordinary = editor.ensureModifiers();
releases[0](); await ordinary;
assert.deepEqual(editor.modifierOptions.map(option => option.value), ["mod-1"]);
await editor.ensureModifiers();
assert.equal(calls.length, 1);
assert.deepEqual(closed, [1]);

// Changing only the cluster configuration while native work is in flight
// must drop the old result and fetch the newest configuration once.
strategy.base_state.cluster = {passive_key: "first", passive_count: 2};
const stale = editor.ensureModifiers();
strategy.base_state.cluster = {passive_key: "second", passive_count: 3};
await editor.ensureModifiers(); // The current request still owns the loading flag.
releases[1](); await stale;
assert.equal(calls.length, 3);
assert.deepEqual(editor.modifierOptions.map(option => option.value), ["mod-1"]);
assert.deepEqual(calls[1][3], {passiveKey: "first", passiveCount: 2});
assert.deepEqual(calls[2][3], {passiveKey: "second", passiveCount: 3});
releases[2](); await settle();
assert.deepEqual(editor.modifierOptions.map(option => option.value), ["mod-3"]);
assert.equal(editor.modifierLoading, false);
await editor.ensureModifiers(); assert.equal(calls.length, 3);
assert.deepEqual(closed, [1, 2, 3]);
assert.deepEqual(strategy.resources, resourcesBefore, "Modifier cache work must not change A/B resources");

// Disposal suppresses both publication and the replacement request while
// still closing the native session acquired by the in-flight request.
strategy.base_state.item_level++;
const disposed = editor.ensureModifiers();
editor.disposed = true;
strategy.base_state.cluster.passive_count = 2;
releases[3](); await disposed; await settle();
assert.equal(calls.length, 4);
assert.deepEqual(editor.modifierOptions.map(option => option.value), ["mod-3"]);
assert.deepEqual(closed, [1, 2, 3, 4]);
console.log("Strategy modifier cache accepts stable results, rejects stale cluster results and closes disposed work.");
