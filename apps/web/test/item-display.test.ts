import assert from "node:assert/strict";

import {
    placeStableSlots,
    visibleModTags,
} from "../src/app/item-display";

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

console.log("  ok - concrete and target slots keep stable presentation identities");
