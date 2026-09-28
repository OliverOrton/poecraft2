import assert from "node:assert/strict";
import { formatModText, modTextLabel, modTextLines } from "../src/app/mod-text";

assert.equal(formatModText("+(10-30) to maximum [Life|Life]"), "+10–30 to maximum Life");
assert.equal(formatModText("(-20--10)% to [Chaos Resistance]"), "-20–-10% to Chaos Resistance");
assert.equal(formatModText("<mod>{(0.5-1.5)% Life regenerated per second} (Local)"), "0.5–1.5% Life regenerated per second (Local)");
assert.deepEqual(modTextLines([" first\\nsecond\r\nthird ", ""]), ["first", "second", "third"]);
assert.equal(modTextLabel(["(5-10)% increased Damage", "(Does not affect Minions)"]), "5–10% increased Damage / (Does not affect Minions)");
assert.equal(formatModText("<img src=x onerror=alert(1)>"), "<img src=x onerror=alert(1)>", "formatting does not grant HTML authority; React and escaped legacy renderers own escaping");
console.log("  ok - modifier markup and ranges format without discarding mechanical qualifiers");
