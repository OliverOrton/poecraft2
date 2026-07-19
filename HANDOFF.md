# Session Handoff - S8.4R.3 Automatic Imprint Discovery Is Next

Updated 2026-07-18 after completing S8.4R.2. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4, S8.0-S8.4, and S8.4R.1-R2 are complete. **S8.4R.3 is the sole next
implementation boundary.** Do not begin browser transfer/lifetime work (R4),
verification truth work (R5), integrated acceptance (R6), or S8.5 in the R3
chunk.

Historical S8.0-S8.4 evidence remains immutable. R1/R2 regression evidence is
separate under
[fixtures/solver-regressions/s8.4r/v1](fixtures/solver-regressions/s8.4r/v1/).
No unbounded real solve, real-case compilation, verification run, 10,000-run
sample, full acceptance suite, rendered review, or R3+ implementation was
performed during R2.

## What R2 Delivered

### Lazy carrier-local automatic candidates

Automatic permanent bench, temporary blocker, protected-metamod, Multimod, and
Fracture options are no longer materialized as one global cross product during
`CalcContext` construction. Construction retains only explicit primitive and
manual fixed-option candidates. On expansion, the current carrier supplies a
bounded plausibility filter; candidates that require exact evaluation run in a
transient strict local abstraction.

Price-key availability can skip a candidate before expensive exact work, but
price values never participate in generation or admission. Complete exact
outcome and state-local resource kernels deduplicate before a planner operator,
dependency, or shared successor is admitted. Dependency-only primitives remain
non-selectable product actions. Exact rejected, collapsed, missing-price, and
resource-deferred dispositions continue through the bounded R1 diagnostic
path.

Admitted state-local option kernels are retained after ordinary evaluator-cache
release because the existing primitive compiler needs their exact setup,
retry, cleanup, recovery, and exit routes. Those kernels, the state-local maps,
the dynamic operator/dependency sets, and the price-identity map are included
in the existing selected owned-byte estimate and cap. Transient local action,
transition, and reforge work is merged into the R1 telemetry and uses the same
configured caps.

### Bounded before/after evidence

R2 adds derived bounded ordinary and advanced product cases beside the pinned
Conquest/Mirage case. The compact evidence is
[r2-before-after-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r2-before-after-summary.json).
All three runs construct and expand at most the start state, compile nothing,
and verify nothing.

| Case | Candidate operators | Shared dependencies | Junk classes | Discovered states | Selected owned bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Conquest before → after | 1,785 → 17 | 147 → 0 | 44 → 13 | 70,000 → 29,069 | 39,092,675 → 16,763,739 |
| Ordinary before → after | 1,318 → 7 | 153 → 0 | 45 → 14 | 13,887 → 13,887 | 13,849,401 → 12,970,069 |
| Advanced before → after | 1,773 → 5 | 159 → 0 | 45 → 13 | 17,954 → 17,954 | 15,213,407 → 14,034,697 |

The after Conquest run expanded one state without hitting the 70,000 shared
discovered-state cap. Its remaining expensive automatic local kernel was
truthfully deferred at the existing local resource boundary. Ordinary and
advanced first-expansion row/transition counts stayed at `4 / 13,886` and
`4 / 17,953` respectively.

### Preserved S8.3 and R1 contracts

Focused S8.3 tests retain the analytic temporary-blocker boundary `4`,
protected boundary `23`, and Fracture boundary `23.75`, including primitive
compiled execution. Direct ready-carrier Fracture pricing uses the exact local
resource vector, so an unused preparation identity cannot affect its price
boundary. Exact-kernel-neutral automatic programs no longer survive as planner
operators.

R1 diagnostic sample budgets, counters, byte/output caps, native memory
telemetry, and the pinned runner remain in force. No C ABI or strategy
vocabulary changed, so release WASM was not rebuilt in this intermediate
checkpoint.

## Focused Validation Completed

No full acceptance suite was run.

- `powershell -File scripts/build.ps1` — passed.
- `poecraft_engine_tests.exe --solver-s8-3-only` — 137 checks, 0 failures.
- `poecraft_engine_tests.exe --solver-solve-only` — 96 checks, 0 failures;
  the optional artifact sub-suite was not requested and reported skipped.
- The three-case S8.4R corpus validated.
- Pinned Conquest/Mirage, ordinary, and advanced cases completed under
  `bounded_first_expansion` with compilation and verification disabled.

## Exact Next Boundary: S8.4R.3 Only

Correct the Imprint option drift and build automatic Imprint discovery on the
new bounded state-local substrate:

- magic rarity is required when the checkpoint is created, not for the final
  solve goal;
- discover useful bounded exact attempt programs only at reachable magic
  carriers where native checkpoint creation is legal;
- discover intermediate exits automatically and let them continue through
  ordinary Bellman values; non-exits restore and retry exactly;
- deduplicate complete attempt/restore kernels before admission and report any
  internal depth/work boundary as a resource boundary, never mechanic
  invalidity;
- remove the user-authored attempt program and false final-magic/complete-goal
  restriction across native metadata, bindings, types, Calculator, tests, and
  docs; and
- add the non-magic-final-goal compiled strategy fixture, while deferring its
  final 10,000-run verification to R6.

Mechanic ambiguity is decided by Oliver. Do not research or infer PoE rules.

## Deferred Boundaries And Gotchas

- R4 owns giant strategy transfer, structured clones, solved-handle lifetime,
  and rebuild-on-reprice behavior.
- R5 owns verification terminal/off-policy truth, confidence, cost semantics,
  and compiler-emitted evaluator vocabulary such as `mod_count`.
- R6 alone runs exact real product solves, required 10,000-run compiled-policy
  verifications, release-WASM rebuild, and the complete non-visual
  acceptance/evidence pass.
- B1.5 remains waived/deferred, not complete. Do not silently backfill it.
- Prefix-to-Suffix and Suffix-to-Prefix beastcrafts remain parked and absent.
- Large S8.0 strategies and projections are immutable historical evidence, not
  normal product inputs.
