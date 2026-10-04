import assert from "node:assert/strict";
import {parseHTML} from "linkedom";
import type {EngineClient} from "../src/app/engine-client";
import type {Catalog, StrategyResult, StrategyTraceEntry} from "../src/app/engine-protocol";
import type {ConcreteModListModel} from "../src/app/components/pc-mod-list";

// DOM/native-client stubs test presentation lifetime and identity admission.
// These are not legal native crafting witnesses or probability/cost evidence.
const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
Object.assign(globalThis, {window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent});
globalThis.fetch = async () => new Response("missing", {status: 404});
const {PcRunTrace} = await import("../src/app/components/pc-run-trace");
const catalog: Catalog = {groupKeyById: [], groupNameById: [], essences: [], fossils: [], bench: [], harvestTags: [],
    influences: [], genericInfluences: [{key: "shaper", name: "Shaper", code: 6}]};
const calls: unknown[][] = [], closedItems: number[] = [], closedSessions: number[] = [];
const releases: Array<() => void> = [], states = new Map<number, Record<string, unknown>>();
let active = 0, maxActive = 0, failRead = false, failClose = false;
const client = {
    createSession: async (...args: unknown[]) => {
        const id = calls.push(args); maxActive = Math.max(maxActive, ++active);
        await new Promise<void>(resolve => releases.push(resolve)); return id;
    },
    importItem: async (state: Record<string, unknown>, session: number) => {states.set(session, state); return session;},
    itemInfo: async (item: number) => ({rarity: "Rare", prefix_mod_ids: [10], suffix_mod_ids: [20],
        implicit_mod_ids: [], enchantment_mod_ids: [], fractured_prefix_mod_ids: [10], fractured_suffix_mod_ids: [],
        item_flags: states.get(item)!.item_flags, memory_strands: states.get(item)!.memory_strands,
        lifecycle: states.get(item)!.lifecycle, generic_influence_bits: 32, searing_exarch_tier: 0, eater_of_worlds_tier: 0,
        max_prefix: 3, max_suffix: 3}),
    modInfo: async (_session: number, id: number) => {
        if (failRead) throw new Error("Native preview read failed");
        return {key: id === 10 ? "retained-prefix-key" : "retained-suffix-key", family_tier_index: 2,
            text_lines: [id === 10 ? "Retained prefix text" : "Retained suffix text"],
            classification_tags: [], reach_kind: id === 20 ? 2 : 0};
    },
    closeItem: async (item: number) => {closedItems.push(item); if (failClose) throw new Error("Item close failed");},
    closeSession: async (session: number) => {closedSessions.push(session); active--;},
} as unknown as EngineClient;
const state = (lifecycle = 0) => ({rarity: 2, item_flags: 16, memory_strands: 4, lifecycle,
    prefixes: [{mod_key: "retained-prefix-key", rolls: [11]}], suffixes: [{mod_key: "retained-suffix-key", rolls: [22]}]});
const entry = (identity: string, base: string, level: number): StrategyTraceEntry => ({step_index: 0, node_id: identity,
    node_kind: 1, action_type: 1100, action_applied: true, matched_edge_id: "exact-edge-id", cumulative_actions: 2,
    known_cumulative_cost: 9, cost_complete: false, terminal_kind: null, failure_reason: 0,
    item: {legacy_current: "must stay separate"}, resources: [{resource_id: 'out"quoted', identity,
        base_key: base, item_level: level, item: state(), lifecycle: 0, memory_strands: 4, acquisitions: 1,
        active_output: true, feeder: {strategy_id: "paid-child", revision: "original-revision", output_contract_id: "original-contract",
            terminal_kind: 0, failure_reason: 0, terminal_node_id: "child-end", detail: "exact receipt", actions: 1,
            known_cost: 9, child_known_cost: 2, acquisition_price_key: "original-price", acquisition_cost_complete: true,
            cost_complete: false, output_accepted: true, child_resources: []}}]});
const result = (entries: StrategyTraceEntry[]) => ({traces: [{entries}], action_distribution: []}) as unknown as StrategyResult;
const trace = new PcRunTrace();
document.body.append(trace);
trace.setItemPreviewContext(client, 77, catalog, []);
const first = entry("physical/first", "native-carrier-a", 60), second = entry("physical/second", "native-carrier-b", 75);
const original = structuredClone([first, second]);
const access = trace as unknown as {previewWork: Promise<void>};
const settle = () => new Promise<void>(resolve => setImmediate(resolve));
const select = (index: number) => trace.querySelector<HTMLButtonElement>(`[data-step="${index}"]`)!.click();
trace.setResult(result([first, second]));
trace.querySelector<HTMLButtonElement>('[data-mode="trace"]')!.click();
await settle(); // Default last step starts its native read.
assert.deepEqual(calls[0], [77, "native-carrier-b", 75, undefined]);
select(0); select(1); // Intermediate queued step must be skipped; only current step may publish.
releases[0](); await settle();
assert.equal(calls.length, 2); assert.deepEqual(calls[1], calls[0]);
assert.deepEqual(closedItems, [1]); assert.deepEqual(closedSessions, [1]);
releases[1](); await access.previewWork;
const wrapper = trace.querySelector<HTMLElement>(".pc-trace-item-preview")!;
assert.equal(wrapper.dataset.resourceId, 'out"quoted'); assert.equal(wrapper.dataset.resourceIdentity, "physical/second");
const card = trace.querySelector("pc-mod-list") as unknown as {model: ConcreteModListModel};
assert.equal(card.model.baseKey, "native-carrier-b"); assert.equal(card.model.itemLevel, 75);
assert.equal(card.model.readOnly, true); assert.deepEqual(card.model.influences, ["Shaper"]);
assert.equal(card.model.prefixes[0].fractured, true); assert.deepEqual(card.model.prefixes[0].rollValues, [11]);
assert.equal(card.model.suffixes[0].crafted, true); assert.deepEqual(card.model.suffixes[0].rollValues, [22]);
assert.equal(card.model.itemFlags, 16); assert.equal(card.model.memoryStrands, 4);
assert.equal(trace.querySelector(".pc-item-fracture-mod, .pc-item-remove-mod, .pc-item-add-mod"), null);
const raw = trace.querySelectorAll(".pc-trace-detail > details > pre");
assert.deepEqual(JSON.parse(raw[0].textContent!), second.resources![0]);
assert.deepEqual(JSON.parse(raw[1].textContent!), second.resources);
assert.equal(trace.querySelector(".pc-trace-detail > dl > div:last-child dd")!.textContent, "9.00 +");
assert.match(trace.textContent!, /Output accepted/); assert.match(trace.textContent!, /Feeder cost incomplete/);
assert.deepEqual([first, second], original, "Reading cards must not mutate trace items, provenance or costs");
assert.equal(maxActive, 1, "Native preview imports are serialized per trace view");

// Empty retained trace is a stated empty view rather than a template fallback.
trace.setResult(result([])); trace.querySelector<HTMLButtonElement>('[data-mode="trace"]')!.click();
assert.match(trace.textContent!, /No retained steps/); assert.equal(calls.length, 2);

// Missing actual carrier data retains raw native envelopes; never use entry.item or a template.
const missing = entry("missing/carrier", "", 75);
trace.setResult(result([missing])); trace.querySelector<HTMLButtonElement>('[data-mode="trace"]')!.click();
await access.previewWork;
assert.equal(calls.length, 2); assert.equal(trace.querySelector("pc-mod-list"), null);
assert.match(trace.querySelector('[role="status"]')!.textContent!, /native base, level, physical identity/);
assert.deepEqual(JSON.parse(trace.querySelector(".pc-trace-detail > details > pre")!.textContent!), missing.resources![0]);

// Read failure and even item-close failure must still attempt session cleanup and retain receipts.
failRead = true; failClose = true;
const failed = entry("physical/rejected", "native-carrier-c", 80);
failed.resources![0].feeder!.output_accepted = false;
trace.setResult(result([failed])); trace.querySelector<HTMLButtonElement>('[data-mode="trace"]')!.click();
await settle(); releases[2](); await access.previewWork;
assert.match(trace.querySelector('[role="alert"]')!.textContent!, /Item close failed/);
assert.match(trace.textContent!, /Output rejected/); assert.equal(trace.querySelector("pc-mod-list"), null);
assert.deepEqual(closedSessions, [1, 2, 3]); assert.equal(active, 0);
assert.deepEqual(JSON.parse(trace.querySelector(".pc-trace-detail > details > pre")!.textContent!), failed.resources![0]);

// Explicit document disposal waits for acquired handles and prevents queued/replacement reads.
failRead = false; failClose = false;
trace.setResult(result([first, second])); trace.querySelector<HTMLButtonElement>('[data-mode="trace"]')!.click();
await settle(); select(0);
let disposed = false;
const disposal = trace.disposeItemPreview().then(() => {disposed = true;});
await settle(); assert.equal(disposed, false);
releases[3](); await disposal;
assert.equal(calls.length, 4); assert.equal(trace.querySelector("pc-mod-list"), null);
assert.deepEqual(closedItems, [1, 2, 3, 4]); assert.deepEqual(closedSessions, [1, 2, 3, 4]);
trace.setItemPreviewContext(client, 77, catalog, []); select(1); await access.previewWork;
assert.equal(calls.length, 4); trace.remove();
console.log("Trace card DOM/client-stub regression: native carrier admission, stale reads, receipts, readonly semantics and cleanup.");
