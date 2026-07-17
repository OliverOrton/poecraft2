# Session Handoff - B1.2 Native Actions And Exact Calculation Is Next

Updated 2026-07-17 after B1.1. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0 and B1.1 are complete. The sole selected recipe remains
`bestiary:imprint`, classified as checkpoint/restore. Prefix to Suffix and
Suffix to Prefix remain explicit unsupported B1 rows and have no actions.
The authoritative mechanic files remain under
[fixtures/bestiary/v1](fixtures/bestiary/v1/).

Canonical SQLite schema version 2 now contains manifest, recipe, beast-input,
action, and action-cost tables. Artifact schema version 4 compiles three
recipe descriptors (one selected, two unsupported) and exactly two actions:

- `bestiary:imprint`: magic-only; corrupted/mirrored forbidden; checkpoint
  must be absent; create one checkpoint; cost one
  `beast:craicic-chimeral` plus three `beast:rare` keys.
- `bestiary:restore_imprint`: any rarity; corrupted/mirrored forbidden;
  same-item checkpoint must be present; consume it; zero beast cost.

The economy catalog maps `craicic-chimeral` through the existing Beast stash
provider to `beast:craicic-chimeral`. `beast:rare` is deliberately manual-only,
not zero-cost. The canonical local rebuild hash is
`93c97d879e11b2022fc272b4d51c6336c3656151cd758ec1c2427d8f74bfc615`.

## Exact Next Boundary

Implement **B1.2 only - Native Actions And Exact Calculation**:

1. Add explicit compound craft state: one live `pc_item_state`, stable live
   item identity, and at most one saved full-state checkpoint bound to that
   identity. This is not a second live item and must not enter
   `pc_item_state`.
2. Execute the two compiled native descriptors exactly. Creation snapshots
   the entire value-copyable item without changing it. Restoration overwrites
   the full mutable state and consumes the checkpoint, including when the
   state was already identical.
3. Return stable refusal reasons for wrong rarity, corrupted, mirrored,
   checkpoint-present, checkpoint-missing, and different-item binding. Every
   refusal preserves both item and checkpoint and consumes no cost.
4. Add the deterministic exact-calculation path over the same compound state
   and prove direct action/calculation agreement with the approved focused
   outcomes.

Stop after the B1.2 checkpoint. Do not add strategy vocabulary, simulator
checkpoint execution, solver options, bindings, workspace UI, or begin B1.3.
Rewrite this handoff so B1.3 is the sole exact next boundary.

## Gotchas

- The compiled descriptors, not display strings, own legality and costs.
- Preserve all fields of `pc_item_state`; do not reconstruct only explicit
  modifiers.
- A different live identity must refuse restoration even if item bytes match.
- Creation with an existing checkpoint refuses without replacing it.
- Restoration is free, single-use, and an identical-state restore still
  applies and consumes the checkpoint.
- No online mechanic research or inference. No routine full suite, WASM/web
  rebuild, rendered review, or 10,000-run verification in B1.2.

## Later Sequence

B1.3 adds the specific exact Imprint retry solver option and ordinary strategy
execution. B1.4 carries bindings and product surfaces; B1.5 is acceptance. S8
follows only after B1 is accepted.
