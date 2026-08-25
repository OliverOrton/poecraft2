# Verified Interim Upper Publication Result

**Status: complete and accepted on 2026-08-21.**

The live solver now exposes the cheapest independently verified strict
executable strategy through the existing progress `upper_bound`, even while
alternative-action refinement remains unfinished. The lower bound and exact
classification are unchanged.

## Implementation

`PolicyExactLiftProgress` records a strict upper only after the emitted graph
has passed compilation, exact evaluation, properness, executability, paired-
default, zero-off-policy, cost-completeness, and finite nonnegative-cost
checks. `PolicyExactLiftWork` retains the numeric minimum across competitive
frontier restarts. The parent `SolveWork` then takes the minimum of its current
public upper and that verified strict upper when producing live progress.

No C ABI, WASM JSON, TypeScript type, strategy vocabulary, Bellman comparison,
action-admission rule, or final-result field changed. The coarse
`start_value_bound` deliberately remains coarse diagnostic state; the public
`upper_bound` is the certified executable incumbent.

## Real Four-T1 Result

The checked native primary reproduced the existing 3,419 discovered / 1,207
expanded coarse states, 11,384 rows, 29,135 transitions, 717/717 upper passes,
and the same 17,584 open automatic actions (13,379 unevaluated plus 4,205
unresolved).

At 38,943.05 ms, strict publication lowered the live upper from
`3759.5969190423507` to `3745.7295960574743`. The value stayed monotone for the
remaining 261 seconds and across the repeated approximately 5,820-, 5,924-,
and 5,961-state strict passes. All 465 bound-trace samples contained zero upper
increases. The global lower remained zero.

The five-minute watchdog still fired at 300,102.42 ms. This is therefore a
real incumbent-publication repair, not an exact-closure performance repair.
The browser can now show that a materially better strategy has been found,
but an unfinished solve still cannot return its compiled strategy graph.

Compact checked measurements are recorded in
[native-primary-summary.json](evidence/native-primary-summary.json). The full
diagnostic report remains a derived local artifact under
`build/solver-diagnostics/strict-interim-upper/`.

## Qualification

- Native build: pass.
- Focused native solver suite: 96,174 checks, 0 failures.
- Checked native four-T1 diagnostic: intended live-upper behavior passed;
  exact-closure watchdog still fired as expected.
- Release WASM build: pass, with only the existing tautological-range
  warnings.
- TypeScript `--noEmit`: pass.
- Focused WASM engine smoke: 28/28 pass.
- `git diff --check`: pass before documentation finalization.
- Full repository pipeline and 10,000-run primary verification: not run. The
  primary did not finish and produced no final strategy to verify.

## Next Boundary

Persistent strict frontier growth remains the next solver-performance owner.
The strict refiner repeatedly reconstructs nearly the complete selected
closure when competitive carriers appear. Preserving or incrementally
extending the oracle, partition, and proof state is the route to a result that
finishes and returns the strategy, while the separate state-928 Imprint work
frontier remains open.
