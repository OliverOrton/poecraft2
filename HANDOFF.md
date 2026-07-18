# Session Handoff - S8.4 Exact Accounting Is Next

Updated 2026-07-18 after S8.3. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4 and S8.0-S8.3 are complete. Oliver waived B1.5 as a separate
acceptance checkpoint; it remains waived/deferred, not complete. Do not
backfill its full suite, 10,000-run Imprint verification, or rendered review.

S8.0 historical captures and S8.1 display-only projections remain immutable
under [fixtures/solver-baselines/s8.0](fixtures/solver-baselines/s8.0/). S8.1
sections, roles, labels, ordering, and solve depth have no executable or solver
authority. S8.2 preservation evidence remains separate under
[fixtures/solver-baselines/s8.2](fixtures/solver-baselines/s8.2/), and the S8.3
automatic-candidate evidence is under
[fixtures/solver-baselines/s8.3](fixtures/solver-baselines/s8.3/).

## S8.3 Result

The native solver now generates carrier-aware candidates when
`action_mode: "goal_relevant"` or `automatic_candidates: true` is requested:

- Fracture preparation/retry enters only for an exact satisfying carrier,
  legal direct or approved preparation path, almost-sure retry, and complete
  recovery and outer exits.
- Permanent bench crafts enter only when the exact deterministic legal
  successor advances a requested group or family/tier goal.
- Temporary ordinary bench blockers enter only when setup is exact, cleanup
  cannot erase a pre-existing crafted carrier, and the full successor kernel
  differs from the relevant single-slot follow-up alone through group conflict
  or occupied prefix/suffix capacity. All exits must advance the target where
  required, clean up, recover, and route completely.
- Protected prefix/suffix options enter only with a satisfied protected-side
  carrier, an unsatisfied opposite-side target, a legal side lock, a follow-up
  that respects and is exactly changed by the lock, and complete reapplication,
  cleanup/replacement, recovery, and exits. Fossil and Essence never qualify
  because their S8.2 transition facts ignore metamods. Existing exact
  Multimod finishes may combine nonconflicting distinct goal benches.

Generation is price-independent and bounded by the goal-relevant registry,
finite supported setup/follow-up vocabulary, exact carrier legality, and the
existing solver caps. Complete candidates with prices compete under the
unchanged minimum-expected-downstream-cost Bellman objective. Missing prices
skip candidates with evidence. Uncertainty is retained unless existing exact
invalidity, equivalence/collapse, strict Restart dominance, or a resource cap
proves another disposition.

Automatic witnesses include candidate kind/id; carrier id/hash and exact
facts; relevant goal mask; legality; baseline and candidate kernel hashes and
mechanism; setup, follow-up, and cleanup primitives; completeness flags; and
selected, included, rejected, collapsed, or deferred reason. Selected options
compile into existing currency, bench, Fracture, cleanup, routing, and Restart
operations. There is no opaque macro, generic program language, or TypeScript
crafting authority.

The additive C-ABI goal/telemetry JSON and TypeScript protocol flag carry the
feature through the worker/WASM boundary without an ABI-version change. The
Python solver surface already passes those JSON documents through the C ABI,
so no Python source change was required. Release WASM was rebuilt.

## Evidence And Validation

- Analytic price flips:
  - temporary blocker boundary `4`: selected at blocker price `2`, value `13`;
    raw Exalt selected at `5`, value `15`; the price-`4` tie retains both rows
    and deterministically selects the lower-variance blocker;
  - prefix-lock boundary `23`: protected Scour selected at `10`, value `14`;
    Restart selected at `30`, value `27`;
  - Fracture boundary `23.75`: automatic Fracture selected at `23`, value
    `124.25`; Restart selected at `24`, value `125`.
- The blocker value/policy matches a pruning-disabled exact control. Automatic
  Fracture matches the manually enumerated S7 option value within `1e-12`.
  Kernel-neutral blockers, influenced Fracture preparation, missing cleanup or
  lock dependencies, and Fossil/Essence protection are refused.
- Compiled native routes completed 64 blocker, 64 protected, and 512 Fracture
  runs with no failure, unapplied action, or unmatched route. Release WASM
  selected and executed an automatic permanent bench for 64/64 successes.
- Final native acceptance passed 468,934 checks. Focused S8.3 and C-ABI gates
  passed 226 and 2,724 checks. WASM engine smoke passed 27/27, TypeScript
  typecheck passed, and the remaining non-visual web tests passed.
- The monolithic `npm test` command still fails only its archived S7 corpus-pin
  assertion because those pins predate the current B1.4 artifact. This is a
  required preserved mismatch, not an S8.3 regression; the historical manifest
  was not rewritten.
- With automatic generation disabled, `oracle-real-one-mod`,
  `oracle-real-two-mod`, and `ordinary-es-bench` retain their S8.0 values,
  states, compiled node/edge counts, and byte-identical raw strategy hashes.
- Benchmark comparisons used `--skip-verification`. No new 10,000-run S8.3
  verification gate was invented. Full native acceptance did execute its
  pre-existing embedded 10,000-run compiler/evaluator/API checks.
- No browser visual review, screenshots, protected-reforge restart, endgame
  recapture, canonical SQLite edit, or compiled-data edit was performed.

## Exact Next Boundary

Execute **S8.4 only - Exact Action And Material Accounting**.

Extend the existing exact strategy evaluator occupancy result with action-
descriptor visits, price-key quantities and priced contributions, and review-
section totals for retries, restarts, blocker setup/cleanup, protection, and
finishing. Keep ordinary totals distinct from explicitly labelled
success-normalized whole-strategy retry totals. Compare exact forecasts with
Simulator averages without merging the evidence sources.

Do not begin S8.5 focus/trimming, S8.6 acceptance, recombinators, B1.5
backfill, or unrelated mechanic work.

## Preserved Mismatches And Gotchas

- Compiler-only `mod_count` routing remains unsupported by Calculator exact
  evaluation.
- The archived endgame sample remains 0.9942 against 0.995.
- The historical temporary-blocker solve remains non-converged. Its S8.0
  capture is unchanged; automatic assembly is evidenced separately in S8.3.
- Protected-reforge captures remain abandoned after long runs; do not restart
  them without an explicit later requirement.
- Archived S7 artifact pins predate B1.4.
- B1.5 remains waived/deferred, not complete.
- Fossil and Essence ignoring metamods is Oliver's mechanic authority. Do not
  research or reverse it.
- Raw ordinary strategies remain the only execution authority. S8.1
  projections and S8.2/S8.3 evidence are comparison/review documents only.

S8.4 is the sole next boundary.
