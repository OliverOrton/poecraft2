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
        ["crusader", 13],
        ["warlord", 11],
        ["redeemer", 15],
        ["hunter", 12],
    ],
    "currency choices derive numeric codes from the compiled generic catalog",
);

console.log("  ok - concrete and target slots keep stable presentation identities");
