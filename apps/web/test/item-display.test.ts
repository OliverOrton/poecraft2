import assert from "node:assert/strict";

import {
    influenceLabels,
    placeStableSlots,
    visibleModTags,
} from "../src/app/item-display";
import type { Catalog } from "../src/app/engine-protocol";
import {
    buildGenericInfluenceCatalog,
    buildInfluenceExaltCatalog,
} from "../src/app/influence-presentation";

assert.deepEqual(
    visibleModTags([
        "life",
        "flat_life_regen",
        "resource",
        "attribute",
        "life",
    ]),
    ["life", "attribute"],
);

interface TestMod {
    id: number;
}

const idOf = (mod: TestMod): number => mod.id;
const initial = placeStableSlots([{ id: 10 }, { id: 20 }], 3, [], idOf);
assert.deepEqual(initial.ids, [10, 20, undefined]);

const afterRemove = placeStableSlots([{ id: 20 }], 3, initial.ids, idOf);
assert.deepEqual(afterRemove.ids, [undefined, 20, undefined]);

const afterAdd = placeStableSlots(
    [{ id: 20 }, { id: 30 }],
    3,
    afterRemove.ids,
    idOf,
);
assert.deepEqual(afterAdd.ids, [30, 20, undefined]);

const afterReroll = placeStableSlots(
    [{ id: 40 }, { id: 50 }],
    3,
    afterAdd.ids,
    idOf,
);
assert.deepEqual(afterReroll.ids, [40, 50, undefined]);

interface TargetMod {
    familyKey: string;
}

const targetIdOf = (mod: TargetMod): string => mod.familyKey;
const initialTargets = placeStableSlots(
    [{ familyKey: "life" }, { familyKey: "defence" }],
    3,
    [],
    targetIdOf,
);
assert.deepEqual(initialTargets.ids, ["life", "defence", undefined]);

const editedTargets = placeStableSlots(
    [{ familyKey: "defence" }, { familyKey: "resistance" }],
    3,
    initialTargets.ids,
    targetIdOf,
);
assert.deepEqual(editedTargets.ids, ["resistance", "defence", undefined]);

const influenceCatalog: Catalog = {
    groupKeyById: [],
    groupNameById: [],
    essences: [],
    fossils: [],
    bench: [],
    harvestTags: [],
    influences: [
        { key: "crusader", code: 3, name: "Crusader" },
        { key: "warlord", code: 1, name: "Warlord" },
        { key: "redeemer", code: 5, name: "Redeemer" },
        { key: "hunter", code: 2, name: "Hunter" },
    ],
    genericInfluences: [
        { key: "adjudicator", code: 1, name: "Warlord" },
        { key: "basilisk", code: 2, name: "Hunter" },
        { key: "crusader", code: 3, name: "Crusader" },
        { key: "elder", code: 4, name: "Elder" },
        { key: "eyrie", code: 5, name: "Redeemer" },
        { key: "shaper", code: 6, name: "Shaper" },
    ],
};
assert.deepEqual(
    influenceLabels((1 << 0) | (1 << 3) | (1 << 5), 0, 0, influenceCatalog),
    ["Warlord", "Elder", "Shaper"],
    "generic influence display remains complete when currency choices narrow",
);

const derivedGenericInfluences = buildGenericInfluenceCatalog({
    adjudicator: 11,
    basilisk: 12,
    crusader: 13,
    elder: 14,
    eyrie: 15,
    none: 0,
    shaper: 16,
});
assert.deepEqual(
    derivedGenericInfluences.map((entry) => entry.name),
    ["Shaper", "Elder", "Crusader", "Warlord", "Redeemer", "Hunter"],
    "generic influence catalog order matches modifier pickers and pools",
);
assert.deepEqual(
    buildInfluenceExaltCatalog(derivedGenericInfluences).map((entry) => [
        entry.key,
        entry.code,
    ]),
    [
        ["shaper", 16],
        ["elder", 14],
        ["crusader", 13],
        ["warlord", 11],
        ["redeemer", 15],
        ["hunter", 12],
    ],
    "currency choices derive numeric codes from the compiled generic catalog",
);

console.log("  ok - concrete and target slots keep stable presentation identities");

// Exercise the rendered pool: equal family ids from different sources must not
// collapse, and enchantment membership/on-item state must remain independent.
const { parseHTML } = await import("linkedom");
const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
Object.assign(globalThis, {
    window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent,
});
const { PcModPool } = await import("../src/app/components/pc-mod-pool");
const { readItemCard } = await import("../src/app/item-preview");
const { renderToStaticMarkup } = await import("react-dom/server");
const { createElement } = await import("react");
const { ItemCard } = await import("../src/app/components/pc-mod-list");
const pool = new PcModPool();
const mods = [
    [4, "base_implicit"], [8, "implicit:corrupted"],
    [9, "implicit:searing_exarch"], [9, "implicit:eater_of_worlds"],
    [12, "retained:heist_enchantment"], [12, "retained:labyrinth_enchantment"],
    [12, "retained:harvest_enchantment"], [12, "retained:blight_enchantment"],
].map(([reach, via], id) => ({
    session_mod_id: id, global_mod_id: id, key: `mod-${id}`, generation_type: -1,
    reach_kind: Number(reach), reach_influence: -1, reach_via: String(via),
    primary_group_id: 1, family_id: 1, family_tier_index: 1,
    required_level: 1, group_display_name: "Shared group", text_lines: [`Modifier ${id}`], classification_tags: [],
}));
pool.setModel({mods, item: {
    rarity: "rare", prefixOnItem: new Set(), suffixOnItem: new Set(),
    implicitOnItem: new Set([1]), enchantmentOnItem: new Set([4]),
    fracturedPrefixOnItem: new Set(), fracturedSuffixOnItem: new Set(), groupOnItem: new Set(),
    maxPrefix: 3, maxSuffix: 3,
}, poolWeights: new Map(), pool: null});
const openSections = () => pool.querySelectorAll<HTMLButtonElement>('[data-section][aria-expanded="false"]').forEach(button => button.click());
pool.setActiveTab("implicit");
openSections();
assert.equal(pool.querySelectorAll(".pc-mod-family").length, 4);
assert.equal(pool.querySelectorAll(".pc-mod-family.is-on-item").length, 1);
assert.match(pool.textContent!, /Vaal Implicits/);
assert.match(pool.textContent!, /Eldritch: Searing Exarch Implicits/);
assert.match(pool.textContent!, /Eldritch: Eater of Worlds Implicits/);
assert.doesNotMatch(pool.textContent!, /Modifier [4-7]/);
pool.setActiveTab("enchantment");
openSections();
assert.equal(pool.querySelectorAll(".pc-mod-family").length, 4);
assert.equal(pool.querySelectorAll(".pc-mod-family.is-on-item").length, 1);
for (const source of ["Heist", "Labyrinth", "Harvest", "Blight"]) assert.match(pool.textContent!, new RegExp(`${source} Enchantments`));
assert.doesNotMatch(pool.textContent!, /Modifier [0-3]/);
pool.querySelector<HTMLButtonElement>(".pc-mod-family-header")!.click();
assert.equal(pool.querySelector<HTMLButtonElement>(".pc-mod-tier-btn")!.disabled, true);

// Saved native flags reach the shared card, and a later uncorrupted state clears
// both the label and border class (including when other item flags remain set).
const client = {
    createSession: async () => 1, importItem: async () => 2,
    itemInfo: async () => ({rarity: "rare", item_flags: 5, memory_strands: 0, lifecycle: 0,
        prefix_mod_ids: [], suffix_mod_ids: [], implicit_mod_ids: [], enchantment_mod_ids: [],
        fractured_prefix_mod_ids: [], fractured_suffix_mod_ids: [], max_prefix: 3, max_suffix: 3}),
    closeItem: async () => {}, closeSession: async () => {},
} as unknown as import("../src/app/engine-client").EngineClient;
const card = await readItemCard(client, 1, null,
    {base: "base", itemLevel: 86, rarity: "rare", state: {}}, "Example item");
assert.equal(card.itemFlags, 5);
const corruptedCard = renderToStaticMarkup(createElement(ItemCard, {model: card}));
assert.match(corruptedCard, /is-corrupted/);
assert.match(corruptedCard, />Corrupted</);
const clearedCard = renderToStaticMarkup(createElement(ItemCard, {model: {...card, itemFlags: 4}}));
assert.doesNotMatch(clearedCard, /is-corrupted|>Corrupted</);
console.log("  ok - modifier sources stay distinct and native corrupted flags reach item cards");

// Both item contexts expose their implicit editing path. A pending Unveil
// keeps the same property values visible while disabling mutations.
const editableCard = {...card, properties: {influences: [{key: "shaper", name: "Shaper", code: 6}], influenceBits: 32, corrupted: true}};
const editableMarkup = renderToStaticMarkup(createElement(ItemCard, {model: editableCard}));
assert.doesNotMatch(editableMarkup, /Add implicit|data-add-mod-side="implicit"/);
assert.match(editableMarkup, /aria-label="Shaper" checked/);
const lockedMarkup = renderToStaticMarkup(createElement(ItemCard, {model: {...editableCard, readOnly: true}}));
assert.doesNotMatch(lockedMarkup, /data-add-mod-side|Remove modifier/);
assert.match(lockedMarkup, /fieldset disabled/);
const targetMarkup = renderToStaticMarkup(createElement(ItemCard, {model: {
    kind: "target", baseName: "Vaal Regalia", itemLevel: 86, rarity: "rare", maxPrefix: 3, maxSuffix: 3,
    prefixes: [], suffixes: [], otherRequirements: [], properties: {influences: [], corrupted: true},
    implicits: [{key: "chosen-implicit", textLines: ["Required corruption"], probabilityLabel: "2%"}],
    implicitInfluences: ["Searing Exarch"],
}}));
assert.match(targetMarkup, /data-target-implicit="chosen-implicit"/);
assert.match(targetMarkup, /Remove implicit requirement/);
assert.match(targetMarkup, /aria-label="Any influence" checked/);
assert.match(targetMarkup, /is-corrupted/);
assert.match(targetMarkup, /Searing Exarch/);
pool.setInteractionMode("goal");
pool.setSelectedImplicits(["mod-1"]);
pool.setActiveTab("implicit");
openSections();
assert.equal(pool.querySelectorAll(".pc-mod-family.is-selected").length, 1);
let selectedImplicit = "";
pool.addEventListener("craft-mod", event => { selectedImplicit = (event as CustomEvent).detail.key; });
const selectedFamily = pool.querySelector(".pc-mod-family.is-selected")!;
selectedFamily.querySelector<HTMLButtonElement>(".pc-mod-family-header")!.click();
const selectedTier = pool.querySelector<HTMLButtonElement>('[data-mod-key="mod-1"] button')!;
assert.ok(!selectedTier.disabled);
selectedTier.click();
assert.equal(selectedImplicit, "mod-1");
console.log("  ok - implicit goals and item properties are editable and respect the Unveil lock");
