# Session Handoff - B1.4 Bindings And Workspace Surfaces Is Next

Updated 2026-07-17 after B1.3. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.3 are complete. Only `bestiary:imprint` is selected, classified as
checkpoint/restore. Prefix to Suffix and Suffix to Prefix remain explicit
unsupported B1 recipe rows and have no action descriptors or behavior.

The data path is canonical SQLite schema 2 to derived artifact schema 4.
Recipe/action descriptors preserve approved legality, checkpoint effects, and
the exact repeated price-key vector. Craicic Chimeral resolves through the
existing Beast provider; `beast:rare` is explicit manual-only pricing.

Native `BestiaryCraftState` is one live item plus an optional full-state saved
copy bound to a stable live identity. Creation/restoration and deterministic
calculation share one atomic transition implementation. Strategy execution
supports the exact two action ids and keeps checkpoint state per run; restart
discards the old item and its checkpoint.

Solver support is the specific `imprint_retry` fixed option. It accepts one to
three exact ordinary primitive actions, only for a complete magic-item goal.
Every attempt pays the four beast keys plus program costs. Non-goal outcomes
compile through explicit free restore and retry; goal outcomes terminate with
the active checkpoint still bound to the successful item. Raw create/restore
are not ordinary one-item DP primitives.

Local checkpoint commits:

- `f0c4461` - B1.0 authoritative Imprint contract.
- `534dac4` - B1.1 data, price, and registry substrate.
- `a6adfb7` - B1.2 native checkpoint state and deterministic calculation.
- The B1.3 checkpoint commit is the immediate parent of the next session.

## Exact Next Boundary

Implement **B1.4 only - Bindings And Workspace Surfaces**:

1. Define the public C ABI compound-state/action/calculation surface without
   widening `pc_item_state` or pretending the checkpoint is a second item.
   Carry stable action ids, refusal reasons, costs, checkpoint presence, and
   success output through Python and WASM bindings.
2. Add the selected Bestiary family to the engine-owned shared action
   presentation used by Emulator and Calculator. Strategy Builder emits the
   same ordinary `bestiary:imprint` and `bestiary:restore_imprint` operations
   already accepted by the native strategy compiler.
3. Expose the specific `imprint_retry` solver option and its complete-magic-goal
   restriction honestly. Do not invent a generic macro editor.
4. Rebuild release WASM because the public ABI/binding surface changes, then
   run only the B1.4 binding/web automated checks required by the active plan.

Stop after B1.4. Do not begin B1.5 acceptance or S8. Rewrite this handoff so
B1.5 is the sole exact next boundary.

## Gotchas

- Simulator trace/action-distribution values for the two Bestiary operations
  currently use internal operation codes outside `pc_action_type`; B1.4 must
  give bindings an explicit stable presentation rather than assuming those
  are ordinary public action enum values.
- Refusals preserve item, checkpoint, and costs. Identical-state restore still
  applies and consumes the checkpoint.
- UI has no mechanic authority and must not reimplement legality or checkpoint
  rules in TypeScript.
- `beast:rare` may be missing until the user supplies a manual price; never
  treat that as free.
- Do not expose parked conversions or infer mechanics.
- Oliver owns rendered/visual review; no screenshots or browser visual smoke.

## Validation At B1.3

- Canonical ingest and artifact validation: passed.
- Focused economy Beast fixture: passed.
- Native fallback build: passed.
- Focused Imprint action/calculation tests: 42 checks, 0 failures.
- Solver/compiler layer: 134 checks, 0 failures. The Imprint exact policy
  compiled and simulated with no unsupported, unapplied, or unmatched route.

B1.5 remains final Bestiary acceptance. S8 follows only after B1 closes.
