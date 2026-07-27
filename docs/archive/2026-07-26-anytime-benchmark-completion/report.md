# Anytime Benchmark Completion Report

**Status: completed on 2026-07-26.**

Parent: [Anytime Benchmark Completion archive](README.md)

## Result

The reduced Gates 0, 1, 2, and 5 passed.

- `solver_benchmark` accepts an attempt-specific partial-output path and
  atomically replaces a valid single-case JSON report after recorded native
  solve-step boundaries.
- Snapshots expose the current bounds, incumbent kind, work, memory, and
  explicit lower-decrease/upper-increase diagnostics without changing solve
  scheduling or proof behavior.
- The corpus runner validates a sidecar only after watchdog cleanup and the
  no-survivor check. A valid partial remains `watchdog_expired`; it is never
  reported as a completed solve.
- Runner v2 provenance pins source, executable, corpus, natural-T1
  generator-config, artifact, machine, benchmark configuration, and corpus
  role identity. Resume rejects incompatible output directories.
- Complete generator strata are assigned to development, validation, or
  frozen-test roles: 69, 45, and 32 cases respectively.
- Reporter v2 includes completed cases, analyzable partial observations,
  watchdogs without trajectories, and explicit failures separately.
- No gap integral, survival estimate, target/data/performance profile, paired
  bootstrap, primary score, or adaptive racing rule was implemented.

Snapshots are cooperative and occur only after a native
`pc_solver_solve_step` returns. They provide no visibility inside one long
blocking step.

## Fresh Baseline

The tracked
[baseline record](../../../fixtures/solver-natural-t1/v1/evidence/anytime-trajectory-baseline-summary.json)
was produced by running the 14 development-role smoke cases from a clean
`670e9b7` source tree under the new contract. All 14 were completed,
analyzable measurements: two ended `bounded_near_optimal` and twelve ended
`refused_state_cap`. Resource-cap refusal is a completed observation, not
censoring or a crash.

| Measurement | Minimum | Median | Maximum |
| --- | ---: | ---: | ---: |
| Native total wall | 70.15 ms | 106.77 ms | 2,496.58 ms |
| Isolated-process wall | 361.70 ms | 417.77 ms | 2,815.83 ms |
| Expanded states | 1 | 1 | 3,038 |
| Discovered states | 10,759 | 25,000 | 25,000 |
| State/action rows | 1 | 1 | 19,132 |
| Transitions | 1 | 1 | 82,498 |
| Reforge work | 583,205 | 791,000 | 1,530,480 |
| Bound samples | 1 | 1 | 15 |

The fresh one-second watchdog probe killed
`natural-t1-smoke-one-armour-09767fd6f824`, left no survivor, and recovered
four step-boundary samples. Its final recorded point at 897.09 ms had
`L=1.3920075388001607`, `U=12.748000000000005`, 767 expanded / 10,538
discovered states, 4,575 rows, 50,861 transitions, and 1,010,962 reforge-work
units. The reporter counted it as one analyzable administrative censor and
zero completed cases.

Wall time is machine-, load-, operating-system-, build-, and compiler-bound.
These figures do not survive a hardware or compiler change. Deterministic work
counters must accompany later comparisons.

## Acceptance

- `powershell -File scripts/build.ps1` passed after the native snapshot
  implementation. The existing GCC warning in
  `solver_solve_heuristics.cpp` remained; no checkpoint error appeared.
- The affected runner/reporter suite passed: 13 tests in 0.77 seconds.
- The clean smoke baseline completed 14/14 cases with no survivor.
- The real watchdog probe produced a valid partial trajectory and no survivor.
- The Markdown audit checked 912 local targets with zero missing or invalid.
- The full repository acceptance pipeline was intentionally not run. This
  milestone did not touch mechanics, SQLite, the compiled artifact, bindings,
  WASM, or web.

## Remaining Boundaries

The baseline is development-role smoke coverage, not a frozen-test candidate
comparison. It disabled exact compiled-policy evaluation and does not support
an exactness or policy-quality claim. Twelve cases reached their state cap
before producing a policy; that result is preserved rather than hidden.

Trajectory analytics are deferred until a second real candidate provides data
about what discriminates. Frozen evaluation must use common cases, budgets,
resource caps, environment, and stopping policy. The accumulated-gap racing
proposal is rejected, not deferred, because maximum pre-incumbent gap biases
it toward early-incumbent schedules.
