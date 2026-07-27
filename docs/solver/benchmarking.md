# Solver Benchmark Trajectories

**Status: stable contract verified on 2026-07-26 at source commit
`670e9b7`.** Checked against `engine/benchmarks/solver_benchmark.cpp`,
`solver_corpus_runner.py`, `solver_reports.py`, their focused tests, and the
[fresh baseline evidence](../../fixtures/solver-natural-t1/v1/evidence/anytime-trajectory-baseline-summary.json).

Parent: [Solver](README.md)

## Observation contract

A benchmark run is an anytime trajectory, not only its terminal label. The
native benchmark records observable root state after a complete
`pc_solver_solve_step`:

- elapsed wall time and focused phase/round;
- root lower and upper values, absolute and relative gaps, and incumbent kind;
- discovered, expanded, and frontier states;
- state/action rows, transitions, and reforge work;
- live and peak solver-owned bytes; and
- whether the sample raised or decreased the lower value, or lowered or
  increased an incumbent upper value.

The decrease/increase fields are diagnostics, not assertions. Focused graph
growth is not currently specified to make the reported lower value monotone
across rounds. An upper-value comparison is meaningful only when both samples
have executable incumbents.

Samples occur after an observable native step, at focused-round or incumbent
changes, at the configured bounded wall interval, and at completion. A long
blocking native step has no internal checkpoint. Reports and evidence must say
“step-boundary trajectory,” never imply sub-step sampling.

## Frozen future metric semantics

No primary comparison score is selected. If a later analytics milestone
computes normalized gap, it uses the following contract for nonnegative cost:

```text
g(t) = (U(t) - L(t)) / max(U(t), 1 cost unit)
```

Clamp numerical noise to `[0, 1]`. Before an executable incumbent exists,
`g(t) = 1` even if the native value field contains a finite implementation
ceiling. `incumbent_kind != none`, not `isfinite(U)`, establishes an incumbent.
The floor is one unit in the pinned economy’s solver cost unit.

A future integral treats the recorded trajectory as right-continuous and
piecewise constant: `g(0) = 1`, and a sample becomes the known value at its
timestamp until the next sample. At a common fixed horizon:

- exact closure extends with gap zero;
- a completed resource-cap or product-target result extends with its last
  certified gap;
- watchdog expiry is right-censored only when an atomic partial trajectory
  exists; and
- crash, operating-system OOM, invalid bound, cancellation, memory refusal,
  runner error, and a watchdog without a trajectory are distinct failures, not
  censoring.

Relative target time uses `U / L - 1` only when `L > 0` and an executable
incumbent exists. Exact target time requires exact policy/termination status,
not merely a floating-point zero gap.

These semantics define how stored observations may be analyzed later. This
milestone does not implement gap integrals, survival estimates, profiles,
paired bootstrap intervals, or a primary score.

## Durable partial reports

The corpus runner supplies a unique partial-result sidecar path for each
attempt. The native benchmark atomically replaces that sidecar after observable
steps. Atomic replacement prevents the watchdog from leaving a half-written
JSON document.

After watchdog cleanup and the no-survivor check, the runner marks the
observation analyzable only when the sidecar contains the selected case and at
least one bound-trace sample. The ledger remains `watchdog_expired`; the
sidecar is never relabelled as a completed solve. A timeout before the first
completed native step has no trajectory and therefore is not usable as a
censored observation.

Completed native exit code `0` or `2` plus a final report remains a completed
measurement. Exit `2` preserves expectation misses such as resource-cap
results. Known Windows out-of-memory statuses and abnormal process
terminations receive separate runner statuses; unknown native/process failures
remain explicit rather than being guessed into a more specific class.

## Experiment identity

The v2 run ledger pins:

- source commit and dirty paths;
- executable path and SHA-256;
- corpus path, SHA-256, ID, schema, and natural-T1 generator-config SHA-256;
- compiled-artifact manifest SHA-256 and data/string identities;
- machine/OS/processor/logical-CPU identity and Python version;
- worker count, memory budget, exact-evaluation setting, selected corpus roles,
  and role-manifest hash; and
- every selected case’s session, start, goal, caps, economy, action envelope,
  generation metadata, corpus features, and resolved product action IDs.

Resume refuses an existing output directory when its executable, corpus,
artifact, machine, or benchmark configuration differs. A full observation
identity includes the executable hash. In an intentional baseline/candidate
comparison, baseline and candidate executable hashes are recorded as the
treatment; every other comparison control must match.

The natural-T1 v1 corpus assigns whole generator strata to `development`,
`validation`, or `frozen_test` in `evaluation-roles.json`. No stratum is split
across roles. Existing smoke/full/deep acceptance tiers remain unchanged.

## Evidence interpretation

Wall time is machine-, load-, operating-system-, build-, and compiler-bound.
It does not survive a hardware or compiler change. Deterministic work counters
such as expansions, rows, transitions, and reforge work are reported alongside
wall time and remain the first check for algorithmic equivalence.

Adaptive accumulated-gap racing is not implemented. Because pre-incumbent gap
is fixed at its maximum, that rule preferentially eliminates runs that are
slow to find their first incumbent and would select an incumbent-discovery
schedule rather than eventual exactness.
