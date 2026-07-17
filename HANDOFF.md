# Session Handoff - B1.1 Data, Price, And Registry Substrate Is Next

Updated 2026-07-17 after Oliver approved and closed B1.0. Read
[AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0 is complete. Its sole selected recipe is Imprint. The authoritative
versioned contract and focused expected outcomes are:

- [manifest](fixtures/bestiary/v1/manifest.json)
- [Imprint recipe contract](fixtures/bestiary/v1/recipes/imprint.json)
- [Imprint expected outcomes](fixtures/bestiary/v1/expected-outcomes/imprint.json)

Prefix to Suffix and Suffix to Prefix were removed from the selected set and
are explicitly unsupported in B1. Their incomplete discussion is not mechanic
authority and must not be implemented or inferred.

S7 remains closed. Its final endgame sample is still recorded honestly at
`0.9942` success against the former `0.995` target; no replacement sample was
run and the miss was not relabelled as a numeric pass. The completed S7 record
lives under [docs/archive/2026-07-solver-s7](docs/archive/2026-07-solver-s7/).

## Approved Imprint Contract

- Recipe id: `bestiary:imprint`; classification: checkpoint/restore.
- Creation consumes `beast:craicic-chimeral` once and `beast:rare` three times.
- Any engine-supported magic item is eligible except corrupted or mirrored
  items. Crafted, fractured, influenced, split, synthesised, and implicit state
  is allowed.
- Creation leaves the live item unchanged and creates one exact full mutable
  state checkpoint bound to that same item. Only one active checkpoint is
  allowed per item.
- Restoration is beast-free, restores the entire saved mutable state into the
  same live item, and consumes the checkpoint. Restoring an already-identical
  state still succeeds and consumes it.
- Restoration refuses a missing or different-item checkpoint and refuses when
  the current live item is corrupted or mirrored. Refusal preserves item and
  checkpoint and consumes nothing.
- Emulator, Calculator, Strategy Builder, and solver availability are all
  required.
- The representation is one live item plus an optional saved checkpoint. Do
  not force it into an ordinary one-item action over the live item alone, do
  not introduce a second live item, and do not generalize it into a macro
  language.

## Exact Next Boundary

Implement **B1.1 only - Data, Price, And Registry Substrate** from the approved
contract:

1. Add production manifest loading and validation for the selected and parked
   recipe identities without turning display text or tags into rules.
2. Register `beast:craicic-chimeral` and `beast:rare` through the existing
   economy model and Beast provider surface. Preserve the repeated price-key
   cost vector; do not silently drop the three rare beasts or assume they are
   free.
3. Add native action descriptors for Imprint creation and restoration with the
   approved legality, costs, checkpoint requirement, and state effects.
4. Keep both parked conversion ids explicitly unsupported and out of product
   action envelopes.

Stop after the B1.1 checkpoint. Do not implement the checkpoint mutation,
restoration engine behavior, exact calculation, bindings, workspace surfaces,
strategy execution, or solver support; those begin in B1.2 and later phases.
Rewrite this handoff so B1.2 is the sole exact next boundary.

## B1.1 Gotchas

- SQLite and the compiled artifact remain canonical/derived respectively; do
  not hand-edit either.
- The existing economy provider recognizes the `Beast` stash category but has
  no Beast mappings. Source mappings must be explicit and stable.
- `pc_item_state` is value-copyable, but an Imprint checkpoint is additional
  state with a different lifecycle. A reserved companion-state flag is not a
  license to model the checkpoint as a second live item.
- Creation and restoration are separate deterministic operations. Restoration
  has no beast cost and successful restoration consumes the checkpoint.
- Oliver's fixture is the mechanic authority. Do not research or infer
  additional Bestiary rules.
- Follow milestone test cadence: no routine full suite, WASM rebuild, web test,
  rendered smoke, screenshot, or 10,000-run strategy verification in B1.1.

## Later Sequence And Parked Scope

B1.2-B1.5 carry native mutation/calculation, solver/strategy integration,
bindings/product surfaces, and final acceptance. S8 follows only after B1 is
accepted. Trade leaves, Hinekora's Lock, corruption/tainted/finishing work,
recombinators, accounts/publishing, ML, and ambient Emulator odds remain
outside the current boundary.
