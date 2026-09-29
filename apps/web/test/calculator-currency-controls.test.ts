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
const result: CalcResult = {supported: true, legal: true, success_probability: 0,
    slot_satisfied: [], outcomes: [], vaal_branches: {implicit: 0.25, sockets: 0.25, reforge: 0.25, unchanged: 0.25},
    implicit_outcomes: [{mod: 0, weight: 100, added_probability: 0.05, present_probability: 0.8}]};
access.client = {
    cloneItem: async () => 4, exportItem: async () => ({}), closeItem: async (...args) => {calls.push(["closeItem", ...args]);},
    currencyCalc: async (...args) => {calls.push(["currencyCalc", ...args]); return result;},
    openCalcInspector: async () => 5, closeSolver: async (...args) => {calls.push(["closeSolver", ...args]);},
};
access.renderGoal = () => {};
access.actionId = "vaal";
access.solver = 0; // Branch inspection must also work before a goal is chosen.
await access.recalc();
assert.deepEqual(calls, [["currencyCalc", 5, 4, "vaal"], ["closeItem", 4], ["closeSolver", 5]]);
assert.match(calculator.textContent!, /Vaal outcomes/);
assert.match(calculator.textContent!, /Roll chance/);
assert.match(calculator.textContent!, /Final chance/);
assert.doesNotMatch(calculator.textContent!, /Exact result/);
access.calc = {...result, vaal_branches: undefined,
    double_corruption_branches: {implicit: 0.25, sockets: 0.25, reforge: 0.25, destroyed: 0.25},
    implicit_outcomes: [
        {mod: 0, weight: 100, added_probability: 0.07, present_probability: 0.07},
        {mod: 1, weight: 200, added_probability: 0.13, present_probability: 0.13},
    ], implicit_pairs: [{mods: [0, 1], probability: 0.03}]};
access.mechanicValues.set("corruption-implicit-first", "0");
access.mechanicValues.set("corruption-implicit-second", "1");
access.renderResults();
assert.match(calculator.textContent!, /Two corruption implicits/);
assert.match(calculator.textContent!, /Destroyed/);
assert.match(calculator.textContent!, /3%/);
assert.doesNotMatch(calculator.textContent!, /No change beyond corruption/);
assert.match(calculator.textContent!, /Bricked \(modifiers changed\)/);
assert.match(calculator.textContent!, /excluding conflicting groups/);
const terminalMarkup = access.renderOutcomes({...access.calc, outcomes: ["bricked", "destroyed"].map((terminal, index) => ({
    terminal: terminal as "bricked" | "destroyed", state: -1 - index, probability: 0.25,
    rarity: -1, prefixes: 0, suffixes: 0, flags: 0, blocked: 0, is_goal: false, slots: [],
}))});
assert.match(terminalMarkup, /<td>Bricked<\/td>/);
assert.match(terminalMarkup, /<td>Destroyed<\/td>/);
assert.doesNotMatch(terminalMarkup, /Finished item is not/);
assert.doesNotMatch(terminalMarkup, /0P\/0S/);
assert.deepEqual(calls, [["currencyCalc", 5, 4, "vaal"], ["closeItem", 4], ["closeSolver", 5]],
    "Selecting a pair displays the native joint probability without another calculation");
access.actionId = "double_corruption";
await access.recalc();
assert.deepEqual(calls.slice(-3), [["currencyCalc", 5, 4, "double_corruption"], ["closeItem", 4], ["closeSolver", 5]]);
access.client.currencyCalc = async () => { throw new Error("Unsupported input"); };
await access.recalc();
assert.equal(access.calcError, "Unsupported input");
assert.match(calculator.textContent!, /Unsupported input/);
assert.doesNotMatch(calculator.textContent!, /Vaal outcomes/, "A failed calculation must clear stale odds");
const {loadGameAssets} = await import("../src/app/game-assets");
const {disposeReact} = await import("../src/app/react-host");
await loadGameAssets();
disposeReact(controls);
console.log("  ok - Expanded currency controls calculate native odds and clean up read-only handles");
