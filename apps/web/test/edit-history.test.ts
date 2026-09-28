import assert from "node:assert/strict";
import { EditHistory } from "../src/app/edit-history";

const history = new EditHistory<{mods: string[]}>();
const initial = {mods: [] as string[]};
history.reset(initial);
initial.mods.push("outside mutation");
history.record({mods: ["life"]});
history.record({mods: ["life", "resistance"]});
assert.equal(history.canUndo, true);
assert.equal(history.canRedo, false);
assert.deepEqual(history.go(0), {mods: []});
assert.equal(history.canRedo, true);
const restored = history.go(1)!;
restored.mods.push("outside mutation");
assert.deepEqual(history.at(1), {mods: ["life"]});
history.record({mods: ["life", "mana"]});
assert.equal(history.canRedo, false);
assert.equal(history.length, 3);
assert.equal(history.go(-1), undefined);
assert.equal(history.go(100), undefined);
assert.equal(history.cursor, 2);

// Redo and the selected history position survive draft persistence.
history.go(1);
const recovered = new EditHistory<{mods: string[]}>();
recovered.restore(JSON.parse(JSON.stringify(history.export())), {mods: ["life"]});
assert.equal(recovered.cursor, 1);
assert.equal(recovered.canRedo, true);
assert.deepEqual(recovered.go(2), {mods: ["life", "mana"]});
// Legacy or mismatched drafts keep the actual current item as their new baseline.
recovered.restore(undefined, {mods: ["old draft"]});
assert.equal(recovered.canUndo, false);
recovered.restore(history.export(), {mods: ["different draft"]});
assert.equal(recovered.length, 1);
assert.deepEqual(recovered.at(0), {mods: ["different draft"]});

// A text-edit group is a single step and retains the state preceding the edit.
const labels = new EditHistory<string>();
labels.reset("original");
labels.record("n");
labels.record("new", true);
assert.equal(labels.length, 2);
assert.equal(labels.go(0), "original");
assert.equal(labels.go(1), "new");
labels.record("new");
assert.equal(labels.length, 2);

const bounded = new EditHistory<number>(3);
bounded.reset(0);
for (let i = 1; i <= 5; i++) bounded.record(i);
assert.deepEqual(bounded.export(), {entries: [3, 4, 5], cursor: 2});
const bytes = new EditHistory<string>(100, 32);
bytes.reset("small");
bytes.record("a single large current document must survive even if it exceeds the budget");
assert.equal(bytes.length, 1);
assert.equal(bytes.canUndo, false);
console.log("Edit history: snapshots, branching, recovery, grouping and limits passed");
