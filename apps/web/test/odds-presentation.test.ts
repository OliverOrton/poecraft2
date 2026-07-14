import assert from "node:assert/strict";

import {
    estimatedActionSpendPerSuccess,
    expectedAttempts,
    formatChaosValue,
    formatExpectedAttempts,
    formatProbabilityExact,
    formatRawProbability,
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
assert.equal(formatProbabilityExact(1), "100%");
assert.equal(formatExpectedAttempts(0), "∞");
assert.equal(formatChaosValue(Number.POSITIVE_INFINITY), "∞");

console.log("  ok - odds precision and per-success cost stay explicit");
