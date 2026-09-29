import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {parseHTML} from "linkedom";
import type {CalcResult, Catalog, ModInfo} from "../src/app/engine-protocol";
import type {CraftControlsModel} from "../src/app/components/pc-craft-controls";

const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
Object.assign(globalThis, {window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent, BroadcastChannel: undefined});
globalThis.fetch = async () => new Response(readFileSync(new URL("../public/game-assets/catalog.json", import.meta.url)));
const {PcCalculator} = await import("../src/app/components/pc-calculator");
const {PcCraftControls} = await import("../src/app/components/pc-craft-controls");
const calculator = new PcCalculator();
calculator.innerHTML = '<pc-craft-controls class="pc-advanced-crafts"></pc-craft-controls><div class="pc-calc-output"></div>';
const controls = calculator.querySelector("pc-craft-controls") as InstanceType<typeof PcCraftControls>;
const catalog: Catalog = {groupKeyById: [], groupNameById: [], essences: [], fossils: [], bench: [], harvestTags: [], influences: []};
const access = calculator as unknown as {
    catalog: Catalog; item: number; session: number; solver: number; actionId: string;
    activeCraftPanel: string; calc: CalcResult; calcError: string; modCache: ModInfo[];
    client: Record<string, (...args: unknown[]) => Promise<unknown>>;
    renderActionPanels(): void; renderResults(): void; renderGoal(): void;
    renderOutcomes(result: CalcResult): string;
    actionChanged(): Promise<void>; recalc(): Promise<void>; selectedCostKeys(): string[];
    currentWork: Promise<void> | null;
    mechanicValues: Map<string, string>;
    goalImplicitKeys: string[]; goalInfluenceBits?: number; goalCorrupted?: boolean;
};
Object.assign(access, {catalog, item: 1, session: 2, solver: 3, busy: false});
let selected = "";
access.actionChanged = async () => { selected = access.actionId; };
for (const [panel, action] of [["basic", "vaal"], ["influenced", "dominance"], ["temple", "double_corruption"]]) {
    access.activeCraftPanel = panel;
    access.renderActionPanels();
    const button = controls.querySelector<HTMLButtonElement>(`[data-select-action="${action}"]`)!;
    assert.ok(button && !button.disabled);
    button.click();
    await access.currentWork;
    assert.equal(selected, action);
    assert.deepEqual(access.selectedCostKeys(), [action]);
}
access.activeCraftPanel = "awakener";
access.renderActionPanels();
const model = (controls as unknown as {model: CraftControlsModel}).model;
controls.setModel({...model, donors: [{key: "donor", name: "My donor"}]});
const awakener = [...controls.querySelectorAll<HTMLButtonElement>("button")].find(button => button.textContent?.includes("Calculate Awakener"))!;
assert.ok(awakener && !awakener.disabled);
awakener.click();
await access.currentWork;
assert.equal(selected, "awakener");

const calls: unknown[][] = [];
const result: CalcResult = {supported: true, legal: true, success_probability: 0.03,
    slot_satisfied: [], implicit_satisfied: [0.07, 0.13], outcomes: [],
    double_corruption_branches: {implicit: 0.25, sockets: 0.25, reforge: 0.25, destroyed: 0.25}};
access.client = {
    cloneItem: async () => 4, exportItem: async () => ({}), closeItem: async (...args) => {calls.push(["closeItem", ...args]);},
    currencyCalc: async (...args) => {calls.push(["currencyCalc", ...args]); return result;},
    openCalcGoal: async (...args) => {calls.push(["openCalcGoal", ...args]); return 5;},
    solverActions: async () => [], closeSolver: async (...args) => {calls.push(["closeSolver", ...args]);},
};
access.renderGoal = () => {};
access.goalImplicitKeys = ["implicit-a", "implicit-b"];
access.goalInfluenceBits = 40;
access.goalCorrupted = true;
access.solver = 0;
for (const action of ["double_corruption", "vaal", "chaos", "dominance"]) {
    calls.length = 0;
    access.actionId = action;
    await access.recalc();
    const goal = calls[0][2] as Record<string, unknown>;
    assert.deepEqual(goal.implicit_mod_keys, ["implicit-a", "implicit-b"]);
    assert.equal(goal.influence_bits, 40);
    assert.equal(goal.corrupted, true);
    assert.equal(goal.min_satisfied_slots, undefined, "Implicit-only goals need no fake explicit slot");
    assert.deepEqual(calls.slice(1), [["currencyCalc", 5, 4, action], ["closeItem", 4], ["closeSolver", 5]]);
    assert.match(calculator.textContent!, /3%/);
    assert.doesNotMatch(calculator.textContent!, /Specific implicit pair|Roll chance|Final chance|Add goal modifiers/);
}
const terminalMarkup = access.renderOutcomes({...result, outcomes: ["bricked", "destroyed"].map((terminal, index) => ({
    terminal: terminal as "bricked" | "destroyed", state: -1 - index, probability: 0.25,
    rarity: -1, prefixes: 0, suffixes: 0, flags: 0, blocked: 0, is_goal: false, slots: [],
}))});
assert.match(terminalMarkup, /<td>Bricked<\/td>/);
assert.match(terminalMarkup, /<td>Destroyed<\/td>/);
assert.doesNotMatch(terminalMarkup, /Finished item is not|0P\/0S/);
access.client.currencyCalc = async () => { throw new Error("Unsupported input"); };
await access.recalc();
assert.equal(access.calcError, "Unsupported input");
assert.match(calculator.textContent!, /Unsupported input/);
assert.doesNotMatch(calculator.textContent!, /3%/, "A failed calculation must clear stale odds");
const copyAccess = calculator as unknown as {
    itemRarity: string; itemPrefixes: unknown[]; itemSuffixes: unknown[]; itemImplicits: unknown[];
    itemInfluenceBits: number; itemFlags: number; modKeyToFamily: Map<string, string>;
    slots: unknown[]; goalImplicitKeys: string[]; goalInfluenceBits?: number; goalCorrupted?: boolean;
    goalRarity: string; copyInputToGoal(): Promise<void>; goalChanged(): Promise<void>; selectContext(context: string): void;
};
Object.assign(copyAccess, {itemRarity: "rare", itemPrefixes: [{key: "life", tierIndex: 2}], itemSuffixes: [],
    itemImplicits: [{key: "vaal-implicit"}], itemInfluenceBits: 40, itemFlags: 1,
    modKeyToFamily: new Map([["life", "life-family"]])});
copyAccess.goalChanged = async () => {};
copyAccess.selectContext = () => {};
await copyAccess.copyInputToGoal();
assert.deepEqual(copyAccess.slots, [{familyModKey: "life-family", minTier: 2}]);
assert.deepEqual(copyAccess.goalImplicitKeys, ["vaal-implicit"]);
assert.equal(copyAccess.goalInfluenceBits, 40);
assert.equal(copyAccess.goalCorrupted, true);
assert.equal(copyAccess.goalRarity, "rare");
const editAccess = calculator as unknown as {
    addGoalFromPool(key: string): void;
    addInputMod(key: string, side: "prefix" | "implicit", fractured?: boolean): Promise<void>;
    inputChanged(): Promise<void>;
};
Object.assign(access, {modCache: [
    {key: "vaal-implicit", reach_kind: 8},
    {key: "shaper-mod", reach_kind: 1, reach_influence: 6, family_tier_index: 2},
]});
Object.assign(copyAccess, {slots: [], goalImplicitKeys: [], goalInfluenceBits: undefined, goalCorrupted: false,
    modKeyToFamily: new Map([["shaper-mod", "shaper-family"]])});
editAccess.addGoalFromPool("vaal-implicit");
await access.currentWork;
assert.equal(copyAccess.goalCorrupted, true);
assert.deepEqual(copyAccess.goalImplicitKeys, ["vaal-implicit"]);
editAccess.addGoalFromPool("shaper-mod");
await access.currentWork;
assert.equal(copyAccess.goalInfluenceBits, 32);
assert.deepEqual(copyAccess.slots, [{familyModKey: "shaper-family", minTier: 2}]);
editAccess.addGoalFromPool("vaal-implicit");
await access.currentWork;
assert.deepEqual(copyAccess.goalImplicitKeys, []);
assert.equal(copyAccess.goalCorrupted, true, "Removing a modifier does not undo an authored item property");
calls.length = 0;
access.client.editItem = async (...args) => { calls.push(args); };
editAccess.inputChanged = async () => {};
await editAccess.addInputMod("vaal-implicit", "implicit");
await editAccess.addInputMod("shaper-mod", "prefix");
assert.deepEqual(calls, [[1, 2, {add_implicit: "vaal-implicit"}], [1, 2, {add_explicit: "shaper-mod", fractured: false}]]);
const {loadGameAssets} = await import("../src/app/game-assets");
const {disposeReact} = await import("../src/app/react-host");
await loadGameAssets();
disposeReact(controls);
console.log("  ok - Expanded currency controls calculate native odds and clean up read-only handles");
