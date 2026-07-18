# Session Handoff - S8.4R.2 State-Local Automatic Candidates Is Next

Updated 2026-07-18 after completing S8.4R.1. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4, S8.0-S8.4, and S8.4R.1 are complete. **S8.4R.2 is the sole next
implementation boundary.** Do not begin automatic Imprint discovery (R3),
browser transfer/lifetime work (R4), verification truth work (R5), integrated
acceptance (R6), or S8.5 in the R2 chunk.

Historical S8.0-S8.4 evidence remains immutable. R1 evidence lives separately
under [fixtures/solver-regressions/s8.4r/v1](fixtures/solver-regressions/s8.4r/v1/).
The pre-fix runner build missed its 124-second safe timeout. Per Oliver's
direction, the baseline was recorded honestly as skipped and implementation
continued; no pre-fix or unbounded product solve was launched.

## What R1 Delivered

### Pinned product regression and bounded runner

The new versioned case pins:

- `Metadata/Items/Armours/BodyArmours/BodyStrDex20` (Conquest Lamellar), item
  level 86, empty rare start;
- the four required T1 family keys named in the active plan;
- Mirage economy
  `economy:mirage:9175d37d83d90ab936e572f04c7599afbf18ff6cefc90786a5276da1759cd52f`
  at cutoff `2026-07-15T21:18:29Z`, without overrides or fallback and with the
  missing base price explicit;
- the exact 17 Calculator-envelope primitive/fossil action IDs priced by that
  snapshot; and
- a runner mode that performs construction plus at most the capped first
  expansion. It never compiles, evaluates, or verifies the strategy.

The runner records construction/action pins, work counts, time, report bytes,
process working-set snapshots, selected native live/peak bytes, and solver
estimated live/diagnostic bytes. The compact evidence is
[r1-after-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r1-after-summary.json);
the complete telemetry is
[r1-after-report.json](fixtures/solver-regressions/s8.4r/v1/evidence/r1-after-report.json).

The repeated pinned measurement completed in 154.1663 ms, expanded exactly one
state, reached the explicit 70,000 discovered-state cap with 69,999 frontier
states, stored 11 state/action rows and 55,804 transitions, and performed 19
bounded work units. It retained 16 action-reason samples while reporting 245
omitted events, 9 preservation samples, and 2 automatic-candidate samples. The
telemetry document was 24,756 bytes; selected native memory was 31,252,333 live
and 39,093,816 peak bytes. There were no runner errors. These are containment
measurements, not optimality or performance acceptance.

### Bounded diagnostics and final output

`SolveTransitionCache` no longer owns one full automatic-candidate record for
every option/state. It retains aggregate considered/eligible/rejected/
collapsed/deferred counters plus at most `max_diagnostic_samples` typed samples.
Preservation witnesses, automatic witnesses, skipped-action samples, and action
reason samples use the same explicit sample budget and report totals/omissions.

Solver owned-memory estimates include retained transition samples, finalized
diagnostic strings, result vectors, and retained outputs. Preservation,
automatic-candidate finalization, extraction scratch projections, final result
owners, and telemetry serialization are checked against explicit caps.
`max_telemetry_json_bytes` is enforced before each append, so serialization does
not first create an oversized document and reject it afterward.

Exact strategy evaluation now has `max_owned_bytes` and
`max_output_json_bytes`. Persistent evaluator owners, final accounting results,
important finalization temporaries, and the final JSON builder are bounded and
reported in the result's `memory` object.

### Native and WASM live-memory telemetry

The C ABI exposes `pc_solver_memory_stats` and
`pc_strategy_eval_memory_stats`, returning selected live owned bytes, lifetime
peak bytes, and retained serialized-output bytes. The WASM facade aggregates
facade registries plus solver/evaluator owners through `pcw_memory_stats`.
`EngineClient.memoryStats()` now returns WASM heap high-water, live handle count,
native live/peak/serialized selected bytes, and an explicit scope label.

Release WASM was rebuilt after the ABI/facade changes. Python evaluator options
and TypeScript protocol types carry the new byte caps.

## Focused Validation Completed

No full acceptance suite was run.

- `powershell -File scripts/build.ps1` — passed.
- `poecraft_engine_tests.exe --solver-solve-only` — 96 checks, 0 failures.
- `poecraft_engine_tests.exe --solver-eval-only` — 895 checks, 0 failures. This
  retained the repository-required 10,000-run S8.4 accounting oracle already in
  that focused test group.
- `poecraft_engine_tests.exe --solver-api-only data\compiled\current` — 2,724
  checks, 0 failures.
- `poecraft_engine_tests.exe --solver-s8-3-only` — 226 checks, 0 failures.
- `powershell -File scripts/build-wasm.ps1` — passed and rebuilt release WASM.
- A direct final release-WASM `pcw_memory_stats` smoke reported a 128 MiB heap,
  zero handles, and valid selected live/peak/serialized fields with the explicit
  scope label.
- The intended name-filtered WASM memory smoke uses a custom in-file runner, so
  it executed that one smoke file's 27 non-visual checks; all passed. It was not
  the full web or repository acceptance suite.
- `npx tsc --noEmit` in `apps/web` — passed.
- The pinned corpus validated, then the post-fix case completed under
  `bounded_first_expansion` with verification disabled.

## Exact Next Boundary: S8.4R.2 Only

Replace eager global Fracture/bench/blocker/metamod cross-product generation
with lazy, carrier/state-local admission before dependencies widen the shared
abstraction. Deduplicate by exact complete kernel before admission. Preserve:

- the existing analytic S8.3 price boundaries;
- exact legality, setup/cleanup/recovery/exit evidence;
- dependency-only primitives as non-selectable structural dependencies;
- primitive execution and compiled-strategy vocabulary; and
- the R1 counters, sample budgets, owned-byte/output caps, memory telemetry, and
  pinned Conquest/Mirage bounded runner.

Use the pinned regression plus ordinary/advanced product construction and
bounded solve diagnostics. R2 must reduce candidate/dependency/layout widening
before state expansion. Do not solve the Conquest case without explicit caps,
do not run its 10,000-run verification, and do not run the complete acceptance
suite. Those integrated gates remain R6 work.

## Deferred Boundaries And Gotchas

- R3 owns automatic Imprint stage/program discovery and the correction of the
  false final-magic/complete-goal restriction. Do not mix it into R2.
- R4 owns giant strategy transfer, structured clones, solved-handle lifetime,
  and rebuild-on-reprice behavior.
- R5 owns verification terminal/off-policy truth, confidence, cost semantics,
  and compiler-emitted evaluator vocabulary such as `mod_count`.
- R6 alone runs exact real product solves, required 10,000-run compiled-policy
  verifications, and the complete non-visual acceptance/evidence pass.
- B1.5 remains waived/deferred, not complete. Do not silently backfill it.
- Prefix-to-Suffix and Suffix-to-Prefix beastcrafts remain parked and absent.
- Large S8.0 strategies and projections are immutable historical evidence, not
  normal product inputs.
- Mechanic ambiguity is decided by Oliver. Do not research or infer PoE rules.
