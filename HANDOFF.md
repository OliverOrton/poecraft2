# Session Handoff - B1.3 Solver And Strategy Integration Is Next

Updated 2026-07-17 after B1.2. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.2 are complete. The only selected recipe is the checkpoint/restore
`bestiary:imprint`; Prefix to Suffix and Suffix to Prefix remain explicit
unsupported recipe rows with no actions.

Schema-v2 SQLite and artifact-schema-v4 data provide two descriptors with the
approved stable ids, legality, checkpoint effects, and cost vectors. The
economy maps Craicic Chimeral through the Beast provider and leaves the three
rare beasts as an explicit manual price key.

Native `BestiaryCraftState` contains one live `pc_item_state`, a stable live
identity, and at most one saved full-state checkpoint bound to that identity.
`apply_bestiary_action` implements atomic creation/restoration and stable
refusal reasons. `calculate_bestiary_action` copies the compound state and
uses that same transition, giving deterministic action/calculation parity.
No checkpoint was added to `pc_item_state` and no second live item exists.

## Exact Next Boundary

Implement **B1.3 only - Solver And Strategy Integration**:

1. Extend strategy operation compilation/execution for exactly
   `bestiary:imprint` and `bestiary:restore_imprint`. Each simulator run owns
   compound checkpoint state; restart discards the old bound checkpoint.
2. Use the descriptor-provided repeated creation cost and zero restoration
   cost. Inapplicable operations retain ordinary action-not-applied failure
   behavior.
3. Add one specific fixed solver option, `imprint_retry`, through the existing
   S7 operator contract. Its user-selected ordinary action program runs from a
   magic entry state; each attempt creates an Imprint, executes the exact
   program, exits on the configured goal threshold, or restores the entry
   checkpoint and retries. Do not add a generic macro language.
4. Keep raw create/restore out of the ordinary one-item solver primitives.
   Only the fixed option may collapse failed attempt outcomes back to the
   exact entry state because native restore guarantees it.
5. Compile a selected Imprint policy into ordinary strategy nodes containing
   explicit create, attempt, route, restore, and retry operations. Prove a
   small exact solve compiles and executes without unsupported or unmatched
   routes.

Stop after the B1.3 checkpoint. Do not add public bindings, C ABI product
surfaces, WASM rebuilds, workspace UI, or begin B1.4. Rewrite this handoff so
B1.4 is the sole exact next boundary.

## Gotchas

- The option pays one Craicic Chimeral and three rare beasts on every attempt;
  restoration is free.
- The attempt program contains existing exact ordinary solver actions only.
- Success can leave the active checkpoint attached to the live item. Retry
  must restore and consume it before creating the next one.
- Simulator routers still inspect the live item only; the generated option
  controls checkpoint lifecycle structurally and needs no generic checkpoint
  condition language.
- Do not expose the parked conversions or infer their behavior.
- No routine full test suite, WASM/web rebuild, rendered review, or 10,000-run
  verification in B1.3.

## Later Sequence

B1.4 carries C ABI/Python/WASM bindings and product workspace surfaces. B1.5
is final Bestiary acceptance. S8 begins only after B1 is accepted.
