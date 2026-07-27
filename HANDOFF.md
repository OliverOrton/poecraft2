# Session Handoff

**Status: no active implementation boundary.** Oliver must select the next
bounded chunk before implementation resumes.

## Latest Completed Result

[Anytime Benchmark Completion](docs/archive/2026-07-26-anytime-benchmark-completion/README.md)
completed on 2026-07-26 on branch
`codex/anytime-benchmark-completion`.

The milestone retained only its reduced Gates 0, 1, 2, and 5:

1. Future normalized-gap, horizon, exactness, censoring, failure, target, and
   numerical-floor semantics are stable, but no primary score is selected.
2. Runner v2 pins source, executable, corpus, natural-T1 generator config,
   compiled artifact, machine, benchmark configuration, and whole-stratum
   development/validation/frozen-test roles.
3. The native benchmark atomically checkpoints single-case step-boundary
   snapshots. A watchdog-valid partial remains `watchdog_expired` and
   analyzable; completed resource-cap results and explicit failures remain
   separate.
4. Reporter v2 includes incomplete analyzable observations, watchdogs without
   trajectories, and explicit failures in run summaries.

The highest-risk path was exercised against a real native process. A
one-second watchdog recovered four samples from
`natural-t1-smoke-one-armour-09767fd6f824`, counted one administrative censor,
and left no survivor. The snapshot stops at the latest completed native solve
step; there is no sub-step observation.

## Acceptance And Baseline

The native build passed. The affected runner/reporter tests passed 13/13 in
0.77 seconds. The full repository pipeline was intentionally not run because
this scope did not touch mechanics, SQLite, the compiled artifact, bindings,
WASM, or web.

A fresh clean-source development-smoke baseline completed 14/14 cases:
two `bounded_near_optimal` and twelve `refused_state_cap`. Native total wall
had a 106.77 ms median and 2,496.58 ms maximum; isolated-process wall had a
417.77 ms median and 2,815.83 ms maximum. Work ranged up to 3,038 expansions,
25,000 discovered states, 19,132 rows, 82,498 transitions, and 1,530,480
reforge-work units.

Wall figures are machine-, load-, build-, and compiler-bound. They do not
survive a hardware or compiler change. The tracked
[evidence summary](fixtures/solver-natural-t1/v1/evidence/anytime-trajectory-baseline-summary.json)
pins the identities, raw-local hashes, deterministic work, limitations, and
real timeout probe.

## Deferred And Rejected Work

Gap integrals, survival/target summaries, completed-optimum error
decomposition, paired uncertainty, data profiles, and performance profiles
remain deferred in the [solver roadmap](docs/future/solver-roadmap.md) until a
second real candidate exists.

Accumulated-gap racing is rejected, not deferred. Pre-incumbent normalized gap
is maximal, so that rule preferentially culls slow-to-first-incumbent runs and
selects for incumbent timing rather than eventual exactness.

The current-solver path remains CPU-native. GPU work is relevant only to
future ML guidance if Oliver later selects it.

## Repository State

The implementation source boundary is clean commit `670e9b7`. Final archive
and evidence documentation follow it on the same local branch. Nothing was
pushed.

Commits must remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
