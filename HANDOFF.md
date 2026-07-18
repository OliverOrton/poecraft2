# Session Handoff - S8.5 Compact Review And Optional Trim Is Next

Updated 2026-07-18 after S8.4. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4 and S8.0-S8.4 are complete. Oliver waived B1.5 as a separate
acceptance checkpoint; it remains waived/deferred, not complete. Do not
backfill its full suite, 10,000-run Imprint verification, or rendered review.

Historical S8.0 captures and S8.1 projections remain immutable under
[fixtures/solver-baselines/s8.0](fixtures/solver-baselines/s8.0/). S8.2 and
S8.3 evidence also remains unchanged. S8.4 evidence is separate at
[fixtures/solver-baselines/s8.4/evidence.json](fixtures/solver-baselines/s8.4/evidence.json).

## S8.4 Result

The existing exact strategy evaluator remains the sole occupancy/accounting
engine. It still solves node/state occupancy and edge traversals once; its
finalization now adds `accounting.version = "s8.4_v1"`:

- `actions`: stable native descriptor id/display name, expected visits and
  applied counts, price-key quantities/contributions, native classifications,
  and reconciled raw operation-node counts;
- `materials`: stable price key, exact quantity, priced/missing status, unit
  price, and contribution from the same immutable economy passed to native;
- `techniques`: ordinary actions, Restart/base, Fracture preparation/use,
  retry action/count, temporary blocker, permanent bench, Multimod setup and
  finishes, protection setup/reapplication, cleanup/replacement, and
  deterministic finish;
- `review_sections`: optional S8.1 section id/label/role plus its raw node/edge
  references, actions, materials, price totals, edge traversal total, and
  techniques. Every raw node/edge must be covered exactly once, and every edge
  remains owned by its source-node section. Labels and roles are output only;
  they never affect evaluation or execution;
- `reconciliation`: descriptor visits/applied counts to raw operation visits,
  descriptor material vectors to evaluator consumption, price dot product to
  known cost, and non-overlapping section actions/materials to the whole graph.

Primary work is per one strategy invocation and terminal mass remains separate.
When explicitly requested and `0 < P(success) < 1`, native also returns
`per_invocation / P(success)` labelled
`independent_whole_strategy_retries`. The result explicitly says this is not a
conditional-path expectation.

Missing prices never become zero. Native returns incomplete status, the
missing keys, known expected cost, and `null` complete total. The worker passes
the pinned economy snapshot into native and the completed product result keeps
the existing league/snapshot identity and provenance attachment.

Selected S8.3 fixed-option primitive nodes and retry edges receive validated
`accounting_roles` metadata during policy compilation. The strategy parser
accepts only the fixed S8.4 vocabulary. These fields do not enter legality,
transitions, routing, Bellman selection, kernel collapse, or compression. Raw
ordinary strategy documents remain the sole execution authority.

Simulator evidence remains separate. Native now aggregates stable descriptor
attempt counts and applied material counts, while retaining its raw per-node
operation counters. C ABI, Python, WASM, and TypeScript results label these as
`simulator_sample` and include sample count and seed; they are never merged
into exact totals.

## Interfaces And Product

- Native C++ owns all accounting inference and aggregation.
- C ABI evaluator options add an immutable economy handle, copied review
  projection JSON, and opt-in normalization. The old options prefix remains
  accepted. Simulator adds stable descriptor/material aggregate queries and
  reports seed/target runs.
- Python adds `Strategy.evaluate(...)` and `SimulationResult.sampled_accounting`.
- Release WASM carries the same options/results and was rebuilt.
- Worker/TypeScript pass the pinned economy and optional review projection;
  they contain no pricing, retry, setup, cleanup, or grouping rules.
- Strategy Calculator renders native-priced materials, exact descriptor and
  technique totals, optional section totals, and explicitly labelled
  success-normalized work. Re-costing reruns native evaluation.
- Simulator presentation identifies sampled descriptor/material averages with
  run count and seed.

## Analytic Evidence

- Restart: one expected action and one base consumption.
- Geometric Chaos retry: single-attempt `p = 0.383397158397`; exact actions and
  Chaos `2.608261376220`; retry traversals `1.608261376220`; price `2` gives
  exact cost `5.216522752440`; section sum is exact.
- The required exact-versus-sampled comparison used exactly 10,000 runs at
  seed `20260718`: action/material average `2.6069` versus exact
  `2.608261376220`, within the existing fixture tolerance. This run was needed
  to validate the new sampled descriptor/material aggregates.
- Fracture/recovery: success/failure `0.2 / 0.8`; per-invocation actions `3.6`;
  preparation and Fracture `1` each; cleanup Scour and base `0.8` each; exact
  cost `17.8`; review sections `0 + 2 + 1.6 + 0 + 0 = 3.6`.
  Independent-retry normalization is five invocations, `18` actions, cost
  `89`. Omitting the Fracture price leaves explicit incomplete status, known
  cost `7.8`, and no total.
- Bench chain: exact actions `9`, cost `31`, and sections
  `0 + 2 + 4 + 3 + 0 = 9`. It includes one blocker, two protection setups,
  one reapplication/retry, three shared cleanup uses, one Multimod, and two
  distinct deterministic finishing descriptors.

All descriptor/node, material, cost, and section reconciliation differences
are zero in the analytic fixtures.

## Real Cases And Validation

The real comparisons were solved with `--skip-verification`; no unrelated
10,000-run compiled-policy gate was added. With automatic generation disabled:

- `oracle-real-one-mod`: value `8.02014428412885`, states `90`, graph `15/94`,
  SHA-256 `072c6c25b59e34e3edc117dc285e4545175431dcf1efa8218203a278f3e0abad`;
- `oracle-real-two-mod`: value `356.45917785234406`, states `99`, graph
  `19/116`, SHA-256
  `951a4282a48974c4ca4c338cca7e8cdc320af2570a684db70b8fbb3b40e97269`;
- `ordinary-es-bench`: value `741.5018555381404`, states `18,817`, graph
  `3,457/19,437`, SHA-256
  `61038f737ad5c916e314fb1070359daccfd6a0adfc52a0c6c7f62521012ff60e`.

All three strategy byte hashes exactly match S8.3/S8.0. Production build and
final non-visual acceptance details are recorded in the S8.4 evidence file.
The final native suite passed 469,026 checks with zero failures; Python passed
17 binding and 10 S8-baseline tests; typecheck, 27/27 WASM smoke cases, the
strategy model/Calculator, economy, and odds presentation passed. The served
production bundle returned HTTP 200 for HTML, hashed JS/CSS, release WASM,
compiled data, the economy index, and a pinned economy snapshot.
The monolithic web command's archived S7 pre-B1.4 pin remains deliberately
excluded and was not rewritten. No browser visual review or screenshots were
performed. Canonical SQLite and compiled engine data were not hand-edited.

## Exact Next Boundary

Execute **S8.5 only - Compact Review And Optional Empirical Trim**.

Keep focus and trim separate:

1. Focus view hides/collapses low-visit regions using exact or sampled
   visitation and never mutates the exact strategy.
2. Trim creates a separate derived document, records the original hash,
   discovery run count/seed/threshold and removed raw ids, and uses only the
   explicit S8.0 fallback choice (Restart or a trimmed terminal).
3. Add aggregate sampled counters for every visited node and traversed edge;
   retain the S8.4 action/material counters.
4. Evaluate original and derived graphs exactly, then validate the derived
   graph with a separate Simulator sample. Do not reuse discovery runs as
   validation evidence.
5. Report exact success/action/material/cost/graph deltas, sampled confidence,
   and the nonzero upper bound for unseen branches. Label the derived strategy
   `empirically_trimmed` and heuristic even when measured impact is zero.

Do not begin S8.6 acceptance, recombinators, B1.5 backfill, or unrelated
mechanics.

## Preserved Mismatches And Gotchas

- Compiler-only `mod_count` routing remains unsupported by Calculator exact
  evaluation.
- The archived endgame sample remains `0.9942` against `0.995`.
- The historical temporary-blocker solve remains non-converged. Its S8.0
  capture is unchanged; automatic assembly remains separate S8.3 evidence.
- Protected-reforge captures remain abandoned after long runs; do not restart
  them without an explicit later requirement.
- Archived S7 artifact pins predate B1.4.
- B1.5 remains waived/deferred, not complete.
- Fossil and Essence ignoring metamods is Oliver's mechanic authority. Do not
  research or reverse it.
- S8.1 section roles/labels/order/depth and S8.4 accounting roles are
  non-executable metadata. Raw strategy routing remains authoritative.

S8.5 is the sole next boundary.
