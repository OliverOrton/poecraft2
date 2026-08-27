import assert from "node:assert/strict";

import { buildCalculatorTargetModel } from "../src/app/calculator-goal-model";
import {
    buildModifierOptions,
    type ModifierFamilyOption,
} from "../src/app/modifier-options";
import type { Catalog, ModInfo } from "../src/app/engine-protocol";
import type { CalculatorGoalSlot } from "../src/app/workspace/persistence";

const options: ModifierFamilyOption[] = [
    {
        value: "life-family",
        label: "+(100–119) to maximum Life",
        side: "prefix",
        sourceKind: "base",
        sourceLabel: "",
        tags: ["life"],
        tiers: [
            { tier: 1, label: "+(100–119) to maximum Life", requiredLevel: 82 },
            { tier: 2, label: "+(90–99) to maximum Life", requiredLevel: 75 },
        ],
    },
    {
        value: "cold-family",
        label: "+(46–48)% to Cold Resistance",
        side: "suffix",
        sourceKind: "base",
        sourceLabel: "",
        tags: ["cold", "resistance"],
        tiers: [
            { tier: 1, label: "+(46–48)% to Cold Resistance", requiredLevel: 84 },
            { tier: 2, label: "+(42–45)% to Cold Resistance", requiredLevel: 72 },
        ],
    },
    {
        value: "defence-family",
        label: "+(121–132)% increased Energy Shield",
        side: "prefix",
        sourceKind: "influence",
        sourceLabel: "Shaper",
        tags: ["defences"],
        tiers: [
            {
                tier: 1,
                label: "+(121–132)% increased Energy Shield",
                requiredLevel: 80,
            },
        ],
    },
];

const build = (
    slots: CalculatorGoalSlot[],
    probabilities?: number[],
) =>
    buildCalculatorTargetModel({
        baseName: "Vaal Regalia",
        itemLevel: 86,
        rarity: "rare",
        slots,
        modifierOptions: options,
        maxPrefix: 3,
        maxSuffix: 3,
        slotProbabilities: probabilities,
        formatProbability: (probability) => `${probability * 100}%`,
        groupLabel: (key) =>
            key === "legacy_group" ? "Recovered legacy group" : key,
    });

const empty = build([]);
assert.equal(empty.kind, "target");
assert.equal(empty.baseName, "Vaal Regalia");
assert.equal(empty.itemLevel, 86);
assert.equal(empty.maxPrefix, 3);
assert.equal(empty.maxSuffix, 3);
assert.deepEqual(empty.prefixes, []);
assert.deepEqual(empty.suffixes, []);

const populated = build(
    [
        { familyModKey: "life-family", minTier: 2 },
        { familyModKey: "cold-family", minTier: 0 },
        { familyModKey: "defence-family", minTier: 1 },
        { group: "legacy_group", minTier: 0 },
    ],
    [0.25, 0.5, 0.75, 1],
);
assert.deepEqual(
    populated.prefixes.map((slot) => slot.familyModKey),
    ["life-family", "defence-family"],
    "prefix requirements preserve their persisted relative order",
);
assert.deepEqual(
    populated.suffixes.map((slot) => slot.familyModKey),
    ["cold-family"],
);
assert.equal(populated.prefixes[0].textLines[0], "+(90–99) to maximum Life");
assert.equal(populated.prefixes[0].probabilityLabel, "25%");
assert.equal(populated.suffixes[0].minTier, 0);
assert.equal(
    populated.suffixes[0].textLines[0],
    "+(46–48)% to Cold Resistance",
);
assert.equal(populated.suffixes[0].probabilityLabel, "50%");
assert.equal(populated.prefixes[1].sourceLabel, "Shaper");
assert.deepEqual(populated.otherRequirements, [
    {
        slotIndex: 3,
        label: "Recovered legacy group",
        minTier: 0,
        probabilityLabel: "100%",
    },
]);

const denseOptions: ModifierFamilyOption[] = Array.from(
    { length: 6 },
    (_, index) => ({
        value: `dense-${index}`,
        label: `Dense family ${index}`,
        side: index < 3 ? "prefix" : "suffix",
        sourceKind: "base",
        sourceLabel: "",
        tags: [],
        tiers: [
            { tier: 1, label: `Dense tier ${index}`, requiredLevel: 1 },
        ],
    }),
);
const dense = buildCalculatorTargetModel({
    baseName: "Vaal Regalia",
    itemLevel: 86,
    rarity: "rare",
    slots: denseOptions.map((option) => ({
        familyModKey: option.value,
        minTier: 1,
    })),
    modifierOptions: denseOptions,
    maxPrefix: 3,
    maxSuffix: 3,
    groupLabel: (key) => key,
});
assert.equal(dense.prefixes.length, 3);
assert.equal(dense.suffixes.length, 3);
assert.equal(dense.otherRequirements.length, 0);

const influenceMod = (
    id: number,
    internalName: string,
    influenceCode: number,
): ModInfo => ({
    session_mod_id: id,
    global_mod_id: id,
    key: `influence-${internalName}`,
    generation_type: 0,
    reach_kind: 1,
    reach_influence: influenceCode,
    reach_via: `influence:${internalName}`,
    primary_group_id: id,
    family_id: id,
    required_level: 1,
    group_display_name: internalName,
    family_tier_index: 1,
    text_lines: [`${internalName} modifier`],
    classification_tags: [],
});
const influenceOptionCatalog: Catalog = {
    groupKeyById: [],
    groupNameById: [],
    essences: [],
    fossils: [],
    bench: [],
    harvestTags: [],
    influences: [
        { key: "warlord", code: 1, name: "Warlord" },
        { key: "hunter", code: 2, name: "Hunter" },
        { key: "redeemer", code: 5, name: "Redeemer" },
    ],
};
const influenceOptions = buildModifierOptions(
    [
        influenceMod(1, "adjudicator", 1),
        influenceMod(2, "basilisk", 2),
        influenceMod(4, "elder", 4),
        influenceMod(5, "eyrie", 5),
        influenceMod(6, "shaper", 6),
    ],
    influenceOptionCatalog,
);
assert.deepEqual(
    new Map(influenceOptions.map((option) => [option.value, option.sourceLabel])),
    new Map([
        ["influence-adjudicator", "Warlord"],
        ["influence-basilisk", "Hunter"],
        ["influence-elder", "Elder"],
        ["influence-eyrie", "Redeemer"],
        ["influence-shaper", "Shaper"],
    ]),
    "generic influence pools keep Elder/Shaper and use canonical public labels",
);
assert.deepEqual(
    influenceOptions.map((option) => option.sourceLabel),
    ["Shaper", "Elder", "Warlord", "Redeemer", "Hunter"],
    "all modifier pickers use the shared generic-influence order",
);

console.log("  ok - Calculator v1 goals adapt to the shared target item model");
