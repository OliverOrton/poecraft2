# Session Handoff

**Status: no implementation boundary is active.**

Latest archive:
[Harvest Natural Pools And Shared Exact Reforge Frontier](docs/archive/2026-07-28-harvest-shared-reforge-frontier/README.md)

## Completed

Oliver's 2026-07-28 Harvest ruling is implemented. Harvest reforge, augment,
and resistance conversion share the `TargetedNatural` pool: positive spawn
and ordinary generation weights, requested target tag, and ordinary final
roll weight. The positive-spawn/zero-generation regression covers sampled,
exact, and debug paths.

The shared exact reforge-frontier prototype was measured and restored. After
its canonical bucket projection was corrected, Lucent/full-four and
Jagged/deep-four matched sequential outcomes, probability bits, discovered
states, and deterministic hashes. It did not reduce action-lane work, did not
move the 3,000,000-work product boundary, retained about 47 MiB, and increased
total wall time by 26–33%.

No shared cross-action frontier, frontier telemetry, cap change, work-account
change, or solver semantic change survives. The unrestricted solver and
goal-progress-gated mode retain their prior contracts.

## Current Decision Boundary

Root broad-action competition remains the measured exact-search wall. The
dense structural-DAG replay shape is closed; do not reopen it as caching,
lockstep loop accounting, or a cap increase. Oliver must select the next
solver research or implementation chunk before source work resumes.

Commits remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
