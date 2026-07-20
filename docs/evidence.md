# Evidence

**Status: authoritative index of pinned cases and measured history.** Raw
fixtures and archived reports remain the evidence; this page summarizes and
routes to them rather than duplicating complete reports.

Parent: [Documentation map](README.md)

## Rule And Data Fixtures

- [`fixtures/spec`](../fixtures/spec/) pins small session pools, weights, and
  action invariants used across native and binding checks.
- [Bestiary v1](../fixtures/bestiary/v1/) pins the owner-approved Imprint
  identity, create/restore contract, refusals, price keys, and explicitly
  unsupported conversion recipes.
- [Economy fixtures](../fixtures/economy/) pin price-key identities, Harvest
  recipe vocabulary, provider samples, and the runtime snapshot shape.

These fixtures are mechanic/data evidence. Current explanatory authority lives
in [Mechanics](mechanics/README.md), [Engine](engine/README.md), and
[Economy](economy/README.md).

## Solver S7 Acceptance

The [S7 archive](archive/2026-07-solver-s7/README.md) preserves the final plan,
handoff, and engineering report. Native and release-WASM comparison reports
agreed across the permanent corpus and the complete automated suite passed at
closure. The final endgame simulator sample was `0.9942` against the former
`0.995` target; it remains a disclosed numeric miss, not a rewritten pass.

## S8 Frozen Before-State And Review Contracts

The [S8.0 evidence guide](../fixtures/solver-baselines/s8.0/README.md) indexes
the frozen corpus, raw compiled strategies, baseline results, review-projection
schema/examples, action-accounting schema/examples, and trimming-provenance
contract. It represents 60,000 simulator executions across new captures and the
archived endgame sample while preserving disclosed non-convergence, abandoned
captures, and evaluator gaps.

Later immutable comparison files are:

- [S8.2 preservation control](../fixtures/solver-baselines/s8.2/evidence.json),
  including unchanged exact-policy cases and the price-flip pruning proof;
- [S8.3 automatic candidates](../fixtures/solver-baselines/s8.3/evidence.json),
  including analytic blocker `4`, protected-side `23`, and Fracture `23.75`
  selection boundaries; and
- [S8.4 exact accounting](../fixtures/solver-baselines/s8.4/evidence.json),
  including the 10,000-run seed-`20260718` comparison of exact
  `2.608261376220` versus sampled `2.6069` actions/material units.

## S8.4R Product Regression And Repair Evidence

The [S8.4R evidence guide](../fixtures/solver-regressions/s8.4r/v1/evidence/README.md)
owns commands, artifact identity, runner boundaries, and interpretation.
Selected compact records are:

- [R1 before/after](../fixtures/solver-regressions/s8.4r/v1/evidence/r1-after-summary.json):
  bounded diagnostic retention, finalization caps, and selected live-memory
  telemetry. The pre-repair ordinary product diagnostic discovered 63,479
  states from one expansion in about 30 seconds; it is a regression signature,
  not a successful solve.
- [R2 before/after](../fixtures/solver-regressions/s8.4r/v1/evidence/r2-before-after-summary.json):
  state-local automatic construction reduced Conquest/ordinary/advanced
  candidate counts from `1,785/1,318/1,773` to `17/7/5` without reopening the
  old global cross product.
- [R3 Imprint](../fixtures/solver-regressions/s8.4r/v1/evidence/r3-imprint-summary.json):
  automatic state-local Imprint discovery and the focused deterministic sample;
  the separate required 10,000-run verification was deferred.
- [R3F Fracture](../fixtures/solver-regressions/s8.4r/v1/evidence/r3f-implementation-summary.json):
  primitive Fracture product planning and structural `23.75` evidence. Stopped
  normal-cap attempts did not establish Bellman entry.
- [R3A scaling](../fixtures/solver-regressions/s8.4r/v1/evidence/r3a-carrier-scaling-summary.json):
  the retained-kernel repair path, including the roughly 433 MB 223-carrier
  boundary, about 22.7 MB live selected bytes at the corrected 1,024-state
  boundary, and the final 4,096-state result: 11.55 seconds, 38,613 discovered
  states, 67,055 rows, 539,238 transitions, and about 184.13 MB selected bytes.
  The unchanged normal-cap request did not enter Bellman in the 30-second time
  box. No cap was raised.

## Engine And WASM Evidence Boundaries

The [engine performance archive](archive/2026-06-engine-performance/README.md)
contains point-in-time native hot-path audits and a decision menu. Later Phase
14 and S7 improvements superseded its work-order status.

[WASM](engine/wasm.md) distinguishes source-inspected capability, Node worker
test evidence, native-only measurements, and real-browser unknowns. Native
throughput or memory observations must not be presented as browser/WASM proof.
