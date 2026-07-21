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

## Exact Solver State Scaling Q0-Q5

The [solver-scaling v1 guide](../fixtures/solver-scaling/v1/README.md) owns the
pinned manifests, diagnostic cap sweeps, final native reports, reproduction
commands, and native/WASM disclosure. Final acceptance established:

- strict/quotient Chaos value parity within `5.7e-13`, 57,719 exact merges,
  and zero observation mismatches;
- complete two-T1 and three-slot product closure at 189,946 and 169,892 strict
  states, with no unsafe merge under their full admitted action sets;
- exact compiled-strategy evaluation with zero unresolved mass and 10,000
  successful simulator runs for each required product case;
- a 6,391-node, 9,607-edge, 2,667,748-byte exact shared strategy for the
  formerly unrepresentable two-T1 policy; and
- measured smallest sufficient increases to 200,000 state/search limits,
  1,215,000 rows, and 11,000,000 reforge work. Transition, selected-memory,
  compiler, strategy-JSON, and telemetry caps remained unchanged; and
- a green non-visual Node/WASM solve and compile of the worst accepted
  three-slot product case in 142 seconds, with 678 MB selected solver
  ownership and a 1.321 GB WASM heap against the module's 4 GiB maximum.

The product reports are the real all-action acceptance. The Chaos fixture is
only the bounded strict oracle; its compression ratio is not generalized to
the complete product envelopes.

## Exact Solver Action And State Pruning

The 2026-07-21 follow-up kept the Q5 product envelopes, accepted values, and
every resource cap unchanged. Its pinned reports and reproduction commands
are indexed by the
[solver-scaling fixture guide](../fixtures/solver-scaling/v1/README.md).

- The protected-repeat producibility/setup filter reduced the three-slot
  protected admission set from 167,244 to 17,186 candidates and protected
  evaluation time from 17.709 seconds to 0.268 seconds. The 9,191 surviving
  Scour/preparation rows and their exact template reuse remained available.
- An exact constructive goal-finish certificate evaluated the legal
  `bench:EinharMasterColdResist3__` row first. Its executable upper was `3`;
  the strict minimum optimistic lower across the other 31 admitted operators
  was `3.0058720000000001`.
- The accepted three-slot solve therefore fell from 169,892 discovered states,
  56,838 expanded states, and 1,214,860 rows to 2 discovered/expanded states
  and 1 row. The final pinned report measured 1.27 ms of solve time and 179 ms
  end to end including exact compiled evaluation and 10,000-run verification.
- Exact value `3`, the 5-node/5-edge compiled strategy, one-action execution,
  mean cost `3`, 10,000/10,000 successes, and every cap check remained
  unchanged. The first finite constructive upper was recorded at expanded
  state 1.
- The rebuilt WASM module closed the same solve/compile/10,000-run path in
  270 ms, with a 19.55 ms maximum Worker slice and no heap growth from its
  278,396,928-byte starting allocation.
- The natural two-T1 product remained the broad control: no certificate was
  accepted, exact value stayed `230.26738656962243`, 189,946 strict states and
  903,935 rows remained, compilation succeeded, and its 10,000-run
  verification passed. This is product-envelope evidence, not a Chaos-only
  optimization.

The certificate-disabled native oracle test retains the same value and policy,
and a price-bound partial graph is explicitly refused as a price-independent
reprice cache. No approximate state merge or terminal canonicalization was
introduced.

## Real Three-T1 From Scratch Diagnostic

The 2026-07-21
[empty-rare diagnostic](../fixtures/solver-scaling/v1/evidence/real-three-t1-from-scratch-summary.json)
corrects the scope of the seeded value-`3` carrier. With the same three slots,
32-action envelope, prices, exact behavior, and unchanged production caps:

- the first state alone discovered 74,563 states, produced 331,960 retained
  transitions, and consumed 7,863,354 reforge-work units;
- the full bounded run stopped at 200,000 discovered / 55,088 expanded states,
  1,152,570 rows, 9,304,122 reforge work, and 725,411,658 selected-owned bytes;
- strict and quotient state counts were both 200,000 with zero exact merges;
- focused lower reached `5.3503139241737685`, but the executable upper remained
  infinite after 19 rounds, so there is no exact value or policy; and
- temporary-bench synthesis accounted for 659,762 candidates and 5,133,587
  variants, taking 6.853 seconds inside 13.644 seconds of automatic admission.

The run establishes a cap stop, not acceptance. A state-cap increase alone is
not supported because the row and reforge budgets were already nearly spent.

## Engine And WASM Evidence Boundaries

The [engine performance archive](archive/2026-06-engine-performance/README.md)
contains point-in-time native hot-path audits and a decision menu. Later Phase
14 and S7 improvements superseded its work-order status.

[WASM](engine/wasm.md) distinguishes source-inspected capability, Node worker
test evidence, native-only measurements, and real-browser unknowns. Native
throughput or memory observations must not be presented as browser/WASM proof.
