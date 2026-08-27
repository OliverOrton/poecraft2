import assert from "node:assert/strict";

import {
    estimatedActionSpendPerSuccess,
    expectedAttempts,
    formatChaosValue,
    formatExpectedAttempts,
    formatProbabilityExact,
    formatRawProbability,
    presentSampledSuccess,
    priceSourceLabel,
    presentExpectedConsumption,
} from "../src/app/odds-presentation";

const probability = 0.084922;
assert.equal(formatProbabilityExact(probability), "8.4922%");
assert.equal(formatRawProbability(probability), "0.084922");
assert.ok(Math.abs(expectedAttempts(probability) - 11.775511646) < 1e-9);
assert.equal(formatExpectedAttempts(probability), "11.7755");
assert.equal(
    formatChaosValue(estimatedActionSpendPerSuccess(1, probability)),
    "11.7755c",
);

assert.equal(formatProbabilityExact(0.000018), "0.0018%");
assert.equal(formatProbabilityExact(1e-12), "1e-10%");
assert.equal(formatRawProbability(1e-12), "1e-12");
assert.equal(formatProbabilityExact(1), "100%");
assert.equal(formatExpectedAttempts(0), "∞");
assert.equal(formatChaosValue(Number.POSITIVE_INFINITY), "∞");

const priced = presentExpectedConsumption(
    [
        { key: "alteration", quantity: 79.7014428 },
        { key: "regal", quantity: 1 },
    ],
    (key) => (key === "alteration" ? 0.2 : undefined),
);
assert.equal(priced.complete, false);
assert.deepEqual(priced.missingKeys, ["regal"]);
assert.ok(Math.abs(priced.total - 15.94028856) < 1e-9);
assert.match(priced.rowsHtml, /79\.7014 × alteration/);
assert.match(priced.rowsHtml, /Missing price/);
assert.equal(priceSourceLabel("owner_default"), "Owner default");

const zeroHitSample = presentSampledSuccess(0, 1000)!;
assert.equal(zeroHitSample.rate, "0%");
assert.notEqual(zeroHitSample.interval95, "0%–0%");
assert.match(zeroHitSample.interval95, /^0%–0\.38/);
const rareHitSample = presentSampledSuccess(1, 10000)!;
assert.equal(rareHitSample.rate, "0.01%");
assert.match(rareHitSample.interval95, /–/);

console.log("  ok - odds precision and per-success cost stay explicit");
