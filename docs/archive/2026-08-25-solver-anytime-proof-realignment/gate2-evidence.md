# Gate 2 — Monotone verified-incumbent ownership

Status: passed on 2026-08-25. This is a behavior-preserving ownership and
progress-truth gate, not final acceptance.

## Retained implementation

One typed `IncumbentPortfolio` now owns the selected output, pending
unverified selected-policy snapshot, retained materialized/verified
candidates, and the independently verified upper observed during cooperative
finalization. Compatibility aliases preserve the Gate 1 call sites and their
solver-owned byte authority until later ownership cleanup removes them.

Every portfolio candidate retains:

- goal, economy, and caller-scope identity;
- action-vocabulary identity and captured prefix size;
- artifact, source generation, target generation, and graph-prefix identity;
- coarse/materialized/compiled/exact-evaluated stage;
- complete captured policy or witness plus compiled artifact provenance; and
- independent certification, evaluation, properness, executability, and
  reconciliation status.

Caller scope hashes the selected candidate actions plus the caller's
goal-progress-gated reforge, automatic Imprint, economic Restart, and explicit
envelope choices. A retained candidate is invalidated if that scope changes.
Append-only graph growth remains compatible only while the candidate's exact
captured prefix is unchanged.

The portfolio remembers the cheapest independently evaluated proper
executable cost monotonically. A worse later verified candidate, an
unverified cheaper estimate, or a later graph epoch cannot increase or erase
that authority. A strictly cheaper verified candidate replaces it. Existing
final publication still compares all compatible retained candidates under the
same exact-cost/properness rule and publishes the cheapest.

## Progress authority

Live progress and telemetry now separate:

1. `candidate_estimate`, source, stage, verification state, and identity;
2. monotone `verified_executable_upper` and portfolio identity;
3. `independent_global_lower`, certification, and proof provenance;
4. `restricted_search_lower` plus whether its action envelope is global; and
5. `exact_closure`, present only after exact policy closure.

`SolveProgress.start_value_bound` remains the selected/coarse candidate.
`SolveProgress.upper_bound` is now only the monotone independently verified
executable upper. The Calculator labels both separately; its live gap uses the
verified upper. Cancellation and `finish()` remain cooperative and
packaging-only.

## Regression authority

The focused native regression constructs a verified 10-Chaos incumbent,
observes a later 14-Chaos verified candidate from a newer graph generation,
then a 3-Chaos unverified coarse candidate. Verified authority remains 10.
A later independently verified 8-Chaos candidate replaces it exactly once.

The final owner four-to-five report is
`build/performance/native-solver-solver-anytime-gate2-scope-owner-case-conquest-lamellar-allflame-partial-five-last-mile-current-v1.json`.
It retained:

- status `bounded_feasible`;
- lower `3.4724500000000003` and evaluated upper
  `2698.8747960143623`;
- graph 215/563;
- proper, completely priced, zero-off-policy exact evaluation; and
- strategy SHA-256
  `2062ec8b3400fd5cb54538ce359b22821e681dfa11049c4cab5a8f696eac5db5`.

Its portfolio reported a materialized but unverified candidate estimate
`12197.277488392701` separately from the cheaper verified upper, two verified
observations, one strictly cheaper replacement, monotonicity true, and caller
scope `135e0cfe6942d537`.

The final 1,000-state fixed-work report is
`build/performance/native-solver-solver-anytime-gate2-scope-fixed-case-conquest-lamellar-allflame-partial-five-capped-diagnostic-v1.json`.
It retained the named refusal, lower `3.4724500000000003`, independently
evaluated upper `59810.953776974464`, graph 51/149, and strategy SHA-256
`12e9b2dd308b9a7994fb906764d0f430603cf8d14e860074927580554ef4beb4`.
Its cheaper `12197.277488392703` materialized estimate remained explicitly
unverified and did not replace the public upper.

The exact dirty three-to-five control retained exact value
`0.03801281249999996` and graph 6/7. Its portfolio snapshot reports the same
value independently as verified upper, global lower, and exact closure, while
the restricted envelope is globally closed.

## Checks

- release native build completed with the portfolio regression compiled;
- final owner, fixed-work refusal, and exact dirty controls passed their
  complete benchmark expectations and independent exact evaluations;
- both bounded strategies retained their Gate 1 hashes and graph census;
- portfolio telemetry is valid JSON and reports nonzero caller-scope identity
  for the bounded product controls;
- TypeScript `npx tsc --noEmit` passed; and
- `git diff --check` passed before checkpointing.

Under the plan's cadence, the complete engine, web, and repository suites and
sampled verification remain deferred to the final gate.

## Gate decision

Gate 2 passes. Verified executable authority is monotone across candidate
sources and graph growth, the public progress fields no longer conflate
coarse estimates with verified uppers, and fixed-work policy/value/graph/
properness/classification authority is unchanged. Gate 3 may migrate the
observational action-envelope lanes into a cooperative scheduler.
