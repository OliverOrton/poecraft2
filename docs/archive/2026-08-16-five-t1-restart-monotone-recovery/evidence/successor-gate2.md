# Successor Gate 2 - Priced Five-T1 Decision Point

**Status: Gate 3 required.**

Source checkpoint: `db8b31e` (`Share product Fracture policy regions`). The
two frozen native witnesses were run once on 2026-08-16 with their declared
prices and caps unchanged. Sampled verification, release WASM, and the full
acceptance pipeline were not run.

## Witness A - Restart-Free Reference

`conquest-lamellar-allflame-five-natural-t1-product` remains sound:

| Evidence | Result |
| --- | ---: |
| publication | bounded selected coarse policy |
| lower / evaluated upper | 0 / 624,800.9519118543 |
| graph | 184 nodes / 666 edges / 482,233 bytes |
| product / certification defaults | 170 Restart / 170 fail-closed |
| paired-default-only | true |
| independent success / off-policy mass | 1 / 0 |
| total / solve wall time | 5,684.80 / 3,507.58 ms |
| largest native solve step | 234.23 ms |
| native evaluator peak | 196,591,966 bytes |

The retained selected candidate is still `verified_retained`; the published
product graph independently matches its exact cost. The representation and
semantics are unchanged from the prior sound boundary, and the native step is
inside 250 ms.

## Witness B - Priced Base

`conquest-lamellar-allflame-five-natural-t1-priced-base-product` still does
not materialize the preferred candidate. Publication remains the independently
evaluated six-node Chaos renewal at 37,279,857.73995944.

| Evidence | Result |
| --- | ---: |
| internal certificate | 2,015 nodes / 4,123 edges / 757 policy regions |
| raw states / pairs | 35,837 / 8,395,474 |
| rows / transitions | 544 / 8,396,650 |
| row payload | 268,692,800 bytes |
| evaluator owned / cap | 1,178,823,076 / 1,050,981,903 bytes |
| refined-pair limit | 1,215,000 |
| refinement reached | no |
| publication graph | 6 nodes / 7 edges / 2,656 bytes |
| lower / evaluated upper | 0 / 37,279,857.73995944 |
| independent success / off-policy mass | 1 / 0 |
| total / solve wall time | 12,903.31 / 12,169.59 ms |
| largest native solve step | 2,715.25 ms |

The direct certificate still stops as
`exact_eval_pair_discovery_memory_cap`. Because refinement never begins,
8,395,474 is a raw-pair count and is not evidence that the refined quotient
exceeds 1,215,000.

## Phase Attribution And Decision

| Exact-evaluator stage | Active wall time |
| --- | ---: |
| model setup | 141.21 ms |
| observation preparation | 25.29 ms |
| pair discovery excluding intern lookup | 3,295.90 ms |
| pair interning | 1,392.97 ms |
| exact kernel work | 90.65 ms |
| pair refinement and later stages | not reached |

Gate 1 does not reduce Witness B's internal certificate: its 2,015-node carrier
is structurally different from the product-local Fracture-heavy four-goal
publication graph. No further compiler-dedup claim is available for this
witness.

The exact pair interning index is used only while discovery is open, yet it is
retained while the closed graph attempts refinement. Source accounting charges
one ordered-map node per raw pair; at 8,395,474 entries this is the smallest
clearly owning disposable structure and exceeds the 127,841,173-byte overage
on its own. Gate 3 will therefore make one lifetime/representation change:
retire collision-safe discovery-only interning indexes after the reachable
graph closes and before refinement. This attacks the measured byte boundary.
There is no proved refined-class count boundary to attack yet.

Raw diagnostic reports are
`build/qualification/successor-gate2-witness-a.json` and
`build/qualification/successor-gate2-witness-b.json`.
