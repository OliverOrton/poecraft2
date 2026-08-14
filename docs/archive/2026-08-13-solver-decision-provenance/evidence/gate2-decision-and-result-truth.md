# Gate 2 Decision And Result Truth

Date: 2026-08-13

Source checkpoint under review: `c76180f` plus the Gate 2 working diff.

## Classification

- **source-confirmed:** the former incremental and focused upper helpers
  duplicated sparse-row fixed-point arithmetic, including different
  denominator and comparison cutoffs.
- **source-confirmed:** epsilon-based action preference could retain a larger
  finite expected cost, and the same row selector served both canonical
  objective order and policy-iteration stability.
- **source-confirmed:** final exact extraction separately used epsilon/Q/
  variance ordering instead of the canonical row winner.
- **contract-confirmed:** `exact_closed` plus `bounded_feasible` is an intended
  coarse-closure/bounded-refinement result, not itself a defect.
- **source-confirmed and corrected:** synthesis of `exact_closed` from coarse
  `none` or `no_executable_policy` previously relied on a caller comment rather
  than an explicit discovery-closure precondition.
- **already protected and clarified:** an open incremental envelope publishes
  the safe zero floor. The public scalar was sound, but its proof family was
  not explicit in final result telemetry.
- **not localized:** the historical Fossil-to-Chaos request remains
  unavailable. Nothing in this gate is claimed as its cause.

## 2A - Existing provenance is sufficient

No new construction-origin field or per-state hot-path trace was added.
`BoundedPolicyIncumbent::ChoiceSource`, upper-policy provenance samples,
fallback source, complete publication-candidate samples, policy witnesses,
selected action IDs, transition/policy hashes, and final executable-policy
assertions already answer the hardening controls. With no historical request
and no residual unexplained current choice after Gate 1, another always-live
provenance representation would add overhead without a concrete unanswered
question. The Gate 2A timing budget is therefore not activated.

## 2B - One sparse Q authority

`evaluate_sparse_policy_row()` now owns row cost, ordinary and failure mass,
observed-choice selection, exact stored-double self-loop exit, and row-local
choice fixed points. A value-accessor entry point uses the same implementation
for provisional/partial value tables. Incremental admission and focused upper
evaluation now supply only their missing/goal/fallback value semantics.

Focused units cover direct/accessor equality, a partial table with a terminal
outside the local value vector, exact self-loop normalization, choices, and
non-finite rejection. No mismatch was preserved with a new epsilon.

## 2C - Strict objective, stable iteration

The canonical row winner is strict finite expected cost with stable row ID only
for exact equality. Equivalent variant pricing, constructive incumbents,
incremental overlays, strict alternative scheduling, final extraction, and
verified publication follow the same rule. A one-ULP smaller finite row wins
the canonical selector regardless of admission order; exact equality selects
stable identity.

Policy replacement is a separate named decision. A smaller row beyond the
stability tolerance replaces the current row; an exact-value stable-ID change
also replaces it; a strictly smaller row inside the tolerance is recorded as
suppressed. Broad or quotient policy iteration may stop numerically in that
case, but it cannot publish exact closure until a later strict proof reconciles
the canonical winner. Monotone bounded-upper overlays and finite-policy strict
reoptimization use strict improvements directly.

The focused Exalt control exposed and then closed an important tie case:
verified candidates with exactly equal executable cost retain the stronger
strict proof authority, while any representably cheaper verified candidate
wins and remains bounded unless independently closed.

## 2D - Termination authority

All four successful-refinement publication call paths were traced. Existing
cap and target stops retain their orthogonal termination. Globally exact strict
closure supersedes an earlier stop. The legal coarse-close/bounded-refinement
pair remains unchanged.

For coarse `none` or `no_executable_policy`, callers now pass a separately
computed `coarse_discovery_closed` fact covering action-envelope closure,
state/resource caps, focused/full discovery closure, and focused proof closure.
The helper rejects synthesis without that fact. Native tests cover both the
legal mapping and the rejected unearned mapping.

## 2E - Lower-bound authority

`SolveResult` now carries internal lower-bound authority that serializes under
telemetry `policy_result` as:

- `global_lower_bound_certified`;
- `open_incremental_envelope_universal_zero`;
- `closed_incremental_action_envelope`;
- `global_action_relaxation`;
- `exact_policy_closure`; or
- `none`.

The fixed C ABI summary is unchanged. The diagnostic JSON already crosses the
native/WASM facade and remains forward-compatible in TypeScript. Open
incremental work keeps lower zero and reports no closed global proof; closed
work may retain a positive certified lower; exact policy closure is distinct;
and lower authority does not imply `policy_available` or a finite executable
upper.

## Focused validation

All commands used the fresh Release native build from
`powershell -File scripts/build.ps1`:

| Command | Result |
| --- | --- |
| `poecraft_engine_tests.exe --solver-solve-only` | 6,698 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-refinement-only` | 301 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-quotient-proof-only` | 357 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-policy-refinement-only` | 4,893 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-api-only data/compiled/current` | 2,539 checks, 0 failures; Imprint and Eldritch 10,000-run controls pass |
| `git diff --check` | pass |

This is focused Gate 2 validation, not the Gate 5 acceptance pipeline.
