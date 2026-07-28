# True First-Frontier Successor Census Report

**Status: final evidence and decision record.**

Parent: [Milestone archive](README.md)

## Result

The four previously cap-censored first Chaos rows are now measured to exact
completion:

| Case | Side mix | Exact support | With carrier | Reforge work | Selected bytes at action completion |
| --- | --- | ---: | ---: | ---: | ---: |
| full three | SSS | 3,204,323 | 3,204,324 | 151,348,836 | 905,334,214 |
| deep three | PSS | 712,877 | 712,878 | 28,155,816 | 221,414,592 |
| full four | PPPS | 222,580 | 222,581 | 6,797,580 | 58,083,741 |
| deep four | PPSS | 222,580 | 222,581 | 6,797,580 | 58,110,125 |

These are exact collision-checked `AbstractState` counts from the live
primitive-Chaos evaluator, not estimates. Each distribution sums to one within
`7.4e-15`, has zero duplicate successor IDs, and reports the same support
through the evaluator and census paths.

The two complete repetitions of every case produced identical support counts,
probability sums, goal/affix histograms, work, frontier peaks, projection
classes, selected memory, and ordered support/probability hashes.

## What The 200,000-State Prefix Hid

The old prefix covered very different fractions of the real rows:

| Case | States seen before cap | Fraction of support | Work fraction |
| --- | ---: | ---: | ---: |
| full three | 199,952 | 6.24% | 6.70% |
| deep three | 199,960 | 28.05% | 28.95% |
| full four | 199,969 | 89.84% | 90.02% |
| deep four | 199,969 | 89.84% | 90.02% |

The four-mod rows were only modestly over the state cap. The three-mod rows
were not: full-three is sixteen times the cap and exceeds the product's
11,000,000 reforge-work allowance by `13.76x`.

This corrects every ceiling that had used 200,000 as if it were approximately
the true first-row support.

## The Main Structural Finding

The complete support is almost the full Cartesian product of the one-sided
payload projections:

| Case | Prefix classes | Suffix classes | Product | Support density |
| --- | ---: | ---: | ---: | ---: |
| full three | 893 | 3,601 | 3,215,693 | 99.65% |
| deep three | 221 | 3,251 | 718,471 | 99.22% |
| full four | 276 | 817 | 225,492 | 98.71% |
| deep four | 276 | 817 | 225,492 | 98.71% |

The projection keys are collision-checked byte strings retaining one side's
affix count, goal statuses, and exact junk-class counts. They omit cross-side
blocker provenance and are deliberately optimistic diagnostic ceilings, not
Bellman equivalence.

Every prefix, suffix, and goal-status projection class in the complete
support was already present in the 200,000-state prefix. The missing states
are additional cross-side combinations within those known classes.

This matters because the preceding
[action-local side-factorization experiment](../2026-07-27-action-local-side-factorization/README.md)
proved that the probability table is not rank one: sequential rolls couple
the sides through the combined remaining-weight denominator. The present
census adds that support-only factorization is nearly complete but does not
reduce the number of joint continuation values. Under the full admitted
action set, Bellman continuation can distinguish those joint states.

Therefore:

- the state explosion is not primarily duplicate generation;
- a prefix/suffix support representation alone does not compact the exact
  expectation;
- the already rejected simple probability convolution cannot recover it; and
- a future symbolic row would need a new proof over the dense joint
  continuation-value expectation.

No such proof or production architecture is selected.

## Cleanup Ceiling On Complete Support

The complete goal-pattern histogram lets the earlier cleanup ceiling be
recalculated without censoring:

| Case | Two-plus same-side nonterminal states | Support fraction | States that must disappear to fit cap | Shortfall after impossible deletion |
| --- | ---: | ---: | ---: | ---: |
| full three | 64,242 | 2.00% | 3,004,324 | 2,940,082 |
| deep three | 4,190 | 0.59% | 512,878 | 508,688 |
| full four | 21,887 | 9.83% | 22,581 | 694 |
| deep four | 11,194 | 5.03% | 22,581 | 11,387 |

This grants more power than a real cleanup route: every matching nonterminal
state disappears for free, deterministically, before adding any cleanup
successor. Even that impossible operation fails every case. Full-four is
close, but remains 694 states over the cap before accounting for cleanup
costs, failures, legality, or the other states created before/after the row.

Cleanup is not reopened by the complete census.

## Resource Shape

The exact action-local roll frontier peaks at:

- 2,328,144 roll states for full-three;
- 448,500 for deep-three; and
- 133,000 for each four-mod case.

Native selected-allocation peak after diagnostic result construction was:

- 1,174,647,235 bytes for full-three;
- 281,445,020 bytes for deep-three;
- 76,933,894 bytes for full-four; and
- 76,960,543 bytes for deep-four.

The full-three measurement therefore exceeds the current 1 GiB selected-owned
product cap even before a usable closed Bellman graph exists.

Repeated total wall times ranged from 18.88–19.42 seconds for full-three,
3.63–3.67 seconds for deep-three, and below one second for the four-mod cases.
Those figures are GCC/machine-bound and do not survive a compiler or hardware
change. Deterministic work is the portable result.

## Measurement Contract

The environment-gated diagnostic:

1. entered normal solve expansion at the unchanged empty rare start;
2. selected the live registry's primitive `chaos` action;
3. lifted only private one-action state/reforge diagnostic allowances;
4. completed the ordinary exact `OutcomeDistribution`;
5. derived histograms and collision-checked projections from its interned
   successors; and
6. stopped with typed
   `diagnostic_first_reforge_census_complete` before sparse-row retention or
   successor expansion.

A small armour oracle completed with 9,870 successors, matching evaluator and
census counts, probability `1.0000000000000009`, zero duplicate IDs, and
192,384 work.

Frozen identity, case/report hashes, probability hashes, repeat wall/memory
snapshots, and exact composition counts are in the
[tracked summary](../../../fixtures/solver-natural-t1/v1/evidence/true-successor-frontier-census-summary.json).
Raw reports remain local under
`build/true-successor-frontier-census/`.

## Restoration And Acceptance

All measurement-only changes to `solver_internal.hpp`,
`solver_reforge.cpp`, `solver_solve_expand.cpp`, and
`solver_solve_telemetry.cpp` were restored. The final engine source matches
the Gate 0 commit.

No mechanic, goal, condition, action, transition, solver algorithm, product
cap, public ABI, artifact, binding, WASM, web, or product behavior changed.
The measurement build completed successfully; the small oracle and eight
hard-case measurements completed with no timeout or surviving process.

Final acceptance is tracked-evidence JSON parsing, documentation link
integrity, whitespace validation, restored-source verification, and a clean
committed tree. Cross-layer runtime suites would validate no retained
behavior and are not required.

## Decision

Close this milestone without production integration.

The census removes an important uncertainty but does not reveal a small exact
state class to merge. The hard support is a dense joint side product, its
probabilities are already known not to factor simply, and the full
continuation values remain joint under the admitted actions.

Another solver architecture should not begin from support-only side
projection, cleanup compression, a cap increase, or the old 200,000-state
prefix. Reopening compact broad rows requires a new exact argument for the
dense joint expectation or a deliberately narrower product objective.
