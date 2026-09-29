import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { parseHTML } from "linkedom";
import type { CraftAction, Catalog, ModInfo } from "../src/app/engine-protocol";
import type { CraftControlsModel, CraftPanel } from "../src/app/components/pc-craft-controls";
import type { DraftRecord, ItemSnapshot } from "../src/app/workspace/persistence";

const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
Object.assign(globalThis, {window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent, BroadcastChannel: undefined});
globalThis.fetch = async () => new Response(readFileSync(new URL("../public/game-assets/catalog.json", import.meta.url)));

const { PcEmulator } = await import("../src/app/components/pc-emulator");
const { PcCraftControls } = await import("../src/app/components/pc-craft-controls");
const emulator = new PcEmulator();
emulator.innerHTML = '<pc-craft-controls class="pc-advanced-crafts"></pc-craft-controls>';
const catalog: Catalog = {groupKeyById: [], groupNameById: [], essences: [], fossils: [], bench: [], harvestTags: [], influences: []};
const mods: ModInfo[] = ["Life", "Mana", "Energy Shield"].map((name, index) => ({
    session_mod_id: index, global_mod_id: index, key: `unveil-${index}`, generation_type: 0,
    reach_kind: 7, reach_influence: -1, reach_via: "veiled:unveil", primary_group_id: index,
    family_id: index, required_level: 60, group_display_name: name, family_tier_index: 1,
    text_lines: [`+(50-60) to maximum [${name}]`, "A second modifier line"], classification_tags: [],
}));
const access = emulator as unknown as {
    catalog: Catalog; item: number; session: number; modCache: ModInfo[]; veiledOptions: number[];
    busy: boolean; activeCraftPanel: CraftPanel; mechanicValues: Map<string, string>;
    renderMechanicControls(): void; guard(work: () => Promise<void>): Promise<void>;
    renderShell(): void; currentWork: Promise<void> | null;
    persist(): Promise<void>; draftFor(snapshot: ItemSnapshot): DraftRecord;
    restoreUnveilChoice(revealed: boolean): void; save(): Promise<boolean>;
    craftMod(): Promise<void>; fractureMod(): Promise<void>; removeMod(): Promise<void>;
    applyConfiguredAction(action: CraftAction): Promise<void>;
};
Object.assign(access, {catalog, item: 1, session: 2, modCache: mods, veiledOptions: [0, 1, 2], busy: false, activeCraftPanel: "unveil"});
const applied: CraftAction[] = [];
access.applyConfiguredAction = async action => { applied.push(action); };
const drafts: DraftRecord[] = [];
access.persist = async () => { drafts.push(access.draftFor({base: "base", itemLevel: 86, state: {}})); };
let directEdits = 0;
access.craftMod = access.fractureMod = access.removeMod = async () => { directEdits++; };
const button = (selector: string) => {
    const found = emulator.querySelector<HTMLButtonElement>(selector);
    assert.ok(found, selector);
    return found;
};
access.renderShell();
assert.equal(emulator.querySelectorAll(".pc-unveil-hidden").length, 3);
assert.doesNotMatch(emulator.textContent!, /maximum Life/);
assert.equal(emulator.querySelector('select[data-mechanic="unveil"]'), null);
button("[data-unveil-reveal]").click();
await access.currentWork;
assert.deepEqual([...applied], [], "Revealing offers must not execute or charge a craft");
assert.equal(drafts.at(-1)?.unveilRevealed, true, "The pending choice is saved before displaying offers");
assert.deepEqual(access.veiledOptions, [0, 1, 2], "Revealing must preserve the native offer set");
assert.equal(emulator.querySelectorAll(".pc-unveil-choice").length, 3);
assert.match(emulator.textContent!, /50–60 to maximum Life/);
assert.equal(button('[data-config-action="unveil"]').disabled, true, "No default modifier selection");
for (const command of ["change-base", "create", "undo", "redo", "save", "save-as", "duplicate", "strategy", "calculator"]) {
    assert.equal(button(`[data-cmd="${command}"]`).disabled, true, `${command} is locked while choosing`);
}
assert.equal(button('[data-craft-panel="veiled"]').disabled, true);
const controlModel = () => (emulator.querySelector("pc-craft-controls") as unknown as {model: CraftControlsModel}).model;
controlModel().onSimple("chaos");
controlModel().onConfigured("harvest_reforge");
controlModel().onMemoryStrands?.(10);
controlModel().onAwakener?.();
controlModel().onPanel("veiled");
const pool = emulator.querySelector("pc-mod-pool")!;
for (const name of ["craft-mod", "fracture-mod", "remove-mod"]) {
    pool.dispatchEvent(new dom.CustomEvent(name, {detail: {key: "mod", modId: 0, side: "prefix", onItem: true}}));
}
assert.deepEqual([...applied], [], "Currency callbacks cannot bypass the locked UI");
assert.equal(directEdits, 0, "Direct editing events cannot modify a revealed item");
assert.equal(await access.save(), false, "Workspace save cannot export an unfinished choice");
assert.equal(access.activeCraftPanel, "unveil");

const choice = emulator.querySelectorAll<HTMLInputElement>('.pc-unveil-choice input')[1];
choice.checked = true;
choice.dispatchEvent(new dom.Event("click", {bubbles: true}));
assert.equal(access.mechanicValues.get("unveil"), "unveil-1");
assert.equal(button('[data-config-action="unveil"]').disabled, false);
assert.equal(button('[data-cmd="create"]').disabled, true, "Selecting a radio alone does not unlock the item");
button('[data-config-action="unveil"]').click();
await access.currentWork;
assert.deepEqual([...applied], [{type: "unveil", mod_key: "unveil-1"}]);
assert.equal(button('[data-cmd="create"]').disabled, true, "A refused craft with unchanged offers stays locked");
access.applyConfiguredAction = async action => {
    applied.push(action);
    access.veiledOptions = [];
    access.renderMechanicControls();
};
button('[data-config-action="unveil"]').click();
await access.currentWork;
assert.equal(button('[data-cmd="create"]').disabled, false, "Consuming the offers unlocks editing");
assert.equal(button('[data-craft-panel="veiled"]').disabled, false);
assert.equal(access.draftFor({base: "base", itemLevel: 86, state: {}}).unveilRevealed, false);

// Reload reconstructs the native handle but retains the pending choice lock.
access.item = 3;
access.veiledOptions = [0, 1, 2];
access.restoreUnveilChoice(Boolean(drafts.at(-1)?.unveilRevealed));
access.renderMechanicControls();
assert.equal(emulator.querySelectorAll(".pc-unveil-choice").length, 3);
assert.equal(button('[data-cmd="create"]').disabled, true);
assert.equal(access.mechanicValues.has("unveil"), false, "A restored item must not inherit a stale selection");
assert.equal(button('[data-config-action="unveil"]').disabled, true);
access.veiledOptions = [];
access.renderMechanicControls();
assert.match(emulator.textContent!, /no veiled modifier/);
assert.equal(emulator.querySelector("[data-unveil-reveal]"), null);
assert.equal(emulator.querySelector('[data-config-action="unveil"]'), null);

// If saving the lock fails, offers must remain hidden and editing stays usable.
access.veiledOptions = [0, 1, 2];
access.renderMechanicControls();
access.persist = async () => { throw new Error("Draft storage unavailable"); };
button("[data-unveil-reveal]").click();
await access.currentWork?.catch(() => {});
assert.equal(emulator.querySelectorAll(".pc-unveil-hidden").length, 3);
assert.equal(button('[data-cmd="create"]').disabled, false);

const calculatorControls = new PcCraftControls();
const model = (emulator.querySelector("pc-craft-controls") as unknown as {model: CraftControlsModel}).model;
let calculatorAction = "";
calculatorControls.setModel({...model, mode: "calculator", panel: "unveil", onSimple: id => { calculatorAction = id; }});
assert.equal(calculatorControls.querySelectorAll(".pc-unveil-choice").length, 0);
calculatorControls.querySelector<HTMLButtonElement>('[data-select-action="unveil"]')!.click();
assert.equal(calculatorAction, "unveil", "The separate Calculator panel retains its native calculation action");
const { disposeReact } = await import("../src/app/react-host");
const { loadGameAssets } = await import("../src/app/game-assets");
await loadGameAssets();
disposeReact(emulator.querySelector<HTMLElement>("pc-craft-controls")!);
disposeReact(calculatorControls);
console.log("  ok - Unveil locks edits until confirmed, persists across reload and handles refusal/storage failure");
