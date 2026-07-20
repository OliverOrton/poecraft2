# Bestiary Imprint

**Status: current owner-approved implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: approved Bestiary fixtures, canonical/compiled recipe
descriptors, native companion-state actions and exact calculation, WASM facade,
automatic solver option, compiled simulator operations, and product controls.

## Scope

This family owns `bestiary:imprint` for checkpoint creation and
`bestiary:restore_imprint` for checkpoint restore. These are compound-state
Bestiary operations, not values in `pc_action_type`.

The authoritative selected-recipe contract is the
[Bestiary v1 manifest](../../fixtures/bestiary/v1/manifest.json),
[Imprint recipe](../../fixtures/bestiary/v1/recipes/imprint.json), and
[expected outcomes](../../fixtures/bestiary/v1/expected-outcomes/imprint.json).

## Implemented Behavior

Create Imprint is deterministic and requires the current live item to be magic,
non-corrupted, and non-mirrored, with no checkpoint already present. Generic or
Eldritch influence, crafted or fractured affixes, split/synthesised state,
implicits, enchantments, quality, sockets, and links do not prevent creation.

Creation stores one full mutable-item-state snapshot in companion Bestiary
state, bound to the same live item identity. The cost vector is one
`beast:craicic-chimeral` plus three `beast:rare`. Only one checkpoint may exist.

Restore Imprint is deterministic and requires a checkpoint bound to the same
live item. The current live item must be non-corrupted and non-mirrored, but it
does not have to remain magic. Restore replaces the entire mutable item state
with the stored snapshot and consumes the checkpoint, including when current
and stored states are identical. Restore has no beast cost.

Every refusal is atomic: item state, checkpoint state, and resources remain
unchanged. A checkpoint cannot be used for a different item identity. The WASM
item-clone operation copies the live `pc_item_state` but intentionally resets
Bestiary companion state, so cloning does not duplicate a checkpoint.

## Dated Oliver Rulings

- **2026-07-17:** Oliver approved the Bestiary v1 contract with Imprint as the
  sole selected recipe, deterministic create/restore behavior, full mutable
  snapshot scope, one checkpoint, same-live-item binding, exact costs, and
  atomic refusals. The fixture files carry
  `approved_owner_2026-07-17`.
- **2026-07-18:** magic rarity is required only at checkpoint creation. An
  automatic Imprint solver route may target a rare final goal; final-goal magic
  is not an eligibility condition. This correction is recorded in the archived
  [S8/B1 plan](../archive/2026-07-19-bestiary-solver-s8/plan.md).

## Engine Coverage And Code Pointers

- `fixtures/bestiary/v1/**` — owner-approved contract and outcome oracle.
- `engine/include/poecraft/bestiary.h` — Bestiary C ABI and companion-state
  surface.
- `engine/src/actions_bestiary.cpp` — create/restore legality, snapshot, and
  atomic transition behavior.
- `engine/src/api.cpp` — dedicated Bestiary calculation and presentation.
- `engine/src/solver_options.cpp`, `solver_api.cpp`, and `solver_compile.cpp` —
  automatic Imprint discovery, exact retry kernel, and primitive graph output.
- `engine/src/simulator.cpp` — create/restore strategy operations.
- `bindings/wasm/wasm_api.cpp` — Bestiary presentation/apply/calculate facade
  and clone checkpoint reset.

## Emulator Support

The Bestiary panel obtains native presentation rows and exposes both Create
Imprint and Restore Imprint. The item and checkpoint handles stay together in
the worker. The UI displays native refusal reasons and current checkpoint
presence rather than recreating legality rules.

## Solver Support

Imprint is not an ordinary flat action-registry primitive. Automatic candidate
generation discovers bounded state-local Imprint attempt programs at reachable
magic carriers, evaluates their exact create/attempt/route/restore retry kernel,
and compiles a selected option to the two Bestiary operations plus ordinary
attempt and routing nodes.

User-authored `imprint_retry` programs and exits are rejected. Automatic-only
discovery may use a renewal attempt program or ordinary add/upgrade actions,
and successful exits continue through the ordinary Bellman solve. The archived
10,000-run Imprint verification remains deferred; that is a verification
boundary, not missing implementation of the create/restore rule.

## Calculator Support

The Calculator Bestiary panel exposes both operations and uses the dedicated
deterministic Bestiary calculation API, including checkpoint state, refusal
reason, resource vector, and consumed-checkpoint count. Automatic Calculator
solves can also admit the Imprint option when state-local discovery finds a
complete useful kernel.

## Explicitly Unsupported Behavior

- `bestiary:prefix_to_suffix` and `bestiary:suffix_to_prefix` are explicit
  unsupported recipe rows and have no action implementation.
- User-authored `imprint_retry` solver options are rejected; Imprint retry is
  automatic-only.
- A checkpoint does not transfer to another item or to a WASM item clone.
- No Bestiary recipe beyond the two Imprint operations is supported.

## Open Questions Requiring Oliver

No unresolved Imprint transition-law question was found in the approved
fixtures or inspected code. The deferred 10,000-run gate is evidence work, not
a mechanic ruling.
