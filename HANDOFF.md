# Session Handoff - S8.4R.4 Browser Transfer And Lifetime Is Next

Updated 2026-07-18 after completing S8.4R.3. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4, S8.0-S8.4, and S8.4R.1-R3 are complete. **S8.4R.4 is the sole next
implementation boundary.** Do not begin verification-truth work (R5),
integrated acceptance (R6), S8.5, or later work in the R4 chunk.

Historical S8.0-S8.4 evidence remains immutable. R1-R3 regression evidence is
separate under
[fixtures/solver-regressions/s8.4r/v1](fixtures/solver-regressions/s8.4r/v1/).
No unbounded real solve, full acceptance suite, 10,000-run verification, or
rendered UI review was performed in R3.

## What R3 Delivered

### Correct Imprint semantics

Magic rarity is enforced only when native checkpoint creation is attempted.
The final solver goal may be rare. An Imprint attempt exits when its discovered
intermediate predicate matches, preserving that actual successor for ordinary
Bellman continuation. Every other exact outcome restores the bound checkpoint
and retries. Checkpoint creation/restoration, one Craicic Chimeral plus three
rare beasts per attempt, retry occupancy, and primitive compilation retain the
existing exact engine paths.

The JSON goal parser now rejects user-authored `imprint_retry` programs and
exits. C ABI metadata, Python/WASM bindings, TypeScript types, Calculator draft
persistence, eligibility, controls, pricing text, tests, and docs describe
automatic state-local discovery instead of a final-rarity or complete-goal
restriction. Missing beast prices defer candidates and are never zero cost.

### Bounded state-local discovery on R2

At each reachable carrier, the transient strict R2 context first asks the
native Bestiary action whether checkpoint creation is legal. It enumerates a
bounded set of goal-relevant primitive programs, executes each exact kernel,
and derives useful exits from goal slots missing at the carrier and satisfied
by actual positive-probability outcomes. Complete create/attempt/restore/exit
kernels and exact resource vectors deduplicate before admission. Only a unique
admitted program's structural primitives are added to the shared solve.

The default program depth/work ceilings are solver search resources, not
mechanic limits. Exhaustion appears in bounded automatic-candidate diagnostics
as `max_imprint_program_depth` or `max_imprint_program_work`. R1 sample, output,
owned-byte, and solve-work caps remain in force; automatic evidence strings are
included in the selected owned-byte estimate.

### Focused rare-final fixture

The fixture
[automatic-imprint-to-rare-focused.json](fixtures/solver-regressions/s8.4r/v1/cases/automatic-imprint-to-rare-focused.json)
starts from a legal magic Vaal Regalia carrier and has a rare two-slot final
goal. The solver automatically selects an exact Augment Imprint stage, exits on
the actual useful magic successor, and continues through ordinary Regal value.
The compiled graph uses the existing create/Augment/route/restore/retry/Regal
primitives.

Its intentionally small deterministic simulation completed 64/64 successes
with zero failure, limit, unapplied, or unmatched routes: 2,230 checkpoint
creates, 2,166 restores, 2,230 Augments, 64 Regals, 2,230 Chimerals, and 6,690
rare beasts. The compact record is
[r3-imprint-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r3-imprint-summary.json).
The required 10,000-run verification belongs to R6.

## Focused Validation Completed

No full acceptance suite was run.

- Native Imprint gate: 42 checks, 0 failures.
- Existing R2/S8.3 state-local gate: 137 checks, 0 failures.
- Native Bestiary gate: 77 checks, 0 failures.
- Python bindings: 15 tests passed.
- Release-WASM Bestiary binding/workspace contract passed.
- `npx tsc --noEmit` passed.
- Release WASM was rebuilt because the solver-option C ABI metadata changed.

## Exact Next Boundary: S8.4R.4 Only

Implement the browser strategy-transfer and lifetime repair already pinned in
the active plan:

- remove giant JSON-inside-JSON transfer and unnecessary full clones;
- preflight or align compilation size caps;
- release the solved native handle and transition closure after successful
  strategy transfer;
- rebuild on repricing rather than retaining the solve closure; and
- add non-visual live-byte lifecycle coverage under the R1 telemetry contract.

Do not add retained-cache mode. Oliver owns rendered and visual review.

## Deferred Boundaries And Gotchas

- R5 owns terminal/off-policy verification truth, confidence, cost semantics,
  and exact evaluator vocabulary such as `mod_count`.
- R6 alone runs exact real product solves, required 10,000-run compiled-policy
  verifications, and the complete non-visual acceptance/evidence pass.
- B1.5 remains waived/deferred, not complete. Do not silently backfill it.
- Prefix-to-Suffix and Suffix-to-Prefix beastcrafts remain parked and absent.
- R1 caps/telemetry and R2 lazy exact-kernel-deduplicated admission are settled;
  do not reopen the resolved eager-cross-product/`bad_alloc` work.
- Large S8.0 strategies and projections are immutable historical evidence, not
  normal product inputs.
