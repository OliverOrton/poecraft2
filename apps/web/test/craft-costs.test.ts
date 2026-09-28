import assert from "node:assert/strict";
import { addCraftSpend, emptyCraftSpend } from "../src/app/craft-costs";
import { EditHistory } from "../src/app/edit-history";
import { harvestMaterials } from "../src/app/harvest-crafts";

const original = emptyCraftSpend();
const imprint = addCraftSpend(original, ["beast:craicic-croaker", "beast:rare", "beast:rare", "beast:rare"]);
assert.deepEqual(original.counts, {});
assert.equal(imprint.counts["beast:rare"], 3);
assert.deepEqual(addCraftSpend(imprint, []), imprint, "Refused and free actions preserve spend");
assert.equal(addCraftSpend(imprint, undefined).untracked, true);

const history = new EditHistory<ReturnType<typeof emptyCraftSpend>>(3);
let spend = emptyCraftSpend();
history.reset(spend);
for (let i = 0; i < 105; i++) { spend = addCraftSpend(spend, ["chaos"]); history.record(spend); }
assert.equal(history.length, 3);
assert.equal(history.at(0)!.counts.chaos, 103, "Trimming Undo must retain earlier expenditure");
const reloaded = new EditHistory<typeof spend>(3);
reloaded.restore(JSON.parse(JSON.stringify(history.export())), spend);
const prior = reloaded.go(1)!;
assert.equal(prior.counts.chaos, 104);
reloaded.record(addCraftSpend(prior, ["annul"]));
assert.deepEqual(reloaded.at(2)!.counts, {chaos: 104, annul: 1});
assert.equal(reloaded.canRedo, false);

for (const [tag, rancour] of [["minion", 3], ["elemental", 1], ["attribute", 2], ["mana", 2], ["drop", 1]] as const) {
    assert.equal(harvestMaterials(`harvest_reforge:${tag}`).find(part => part.key === "lifeforce:rancour")?.quantity, rancour);
}
assert.deepEqual(harvestMaterials("harvest_reforge:fire").map(({key, quantity}) => ({key, quantity})), [{key: "lifeforce:wild", quantity: 50}]);
assert.equal(harvestMaterials("harvest_augment:life").find(part => part.key === "lifeforce:sacred")?.quantity, 1);
assert.deepEqual(harvestMaterials("harvest_resist:cold").map(({key, quantity}) => ({key, quantity})), [{key: "lifeforce:vivid", quantity: 500}]);
console.log("Craft spend: repeated quantities, unknown consumption, history trimming/branching and Harvest material recipes passed");
