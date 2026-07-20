# Ordinary Currency

**Status: current implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: native sampled actions, exact single-action calculator,
solver registry/compiler, WASM action parser, and the Emulator and Calculator
basic-currency controls.

## Scope

This family owns `transmute`, `augment`, `alteration`, `regal`, `alchemy`,
`chaos`, `exalt`, `annul`, `scour`, and `remove_crafted_modifiers`.
`restart` is documented with the synthetic solver vocabulary in
[Strategy and solver vocabulary](strategy-and-solver-vocabulary.md).

## Implemented Behavior

All actions refuse a null, corrupted, or mirrored item before mechanic-specific
legality is checked. Random modifiers come from the native session pool, obey
group conflicts and affix capacity, and apply the active Cannot Roll Attack and
Cannot Roll Caster pool filters. Reforge actions in this family preserve
fractured affixes and every affix on a locked side.

- `transmute` requires a normal item, changes it to magic, and rolls one or two
  explicit modifiers.
- `augment` requires a magic item and attempts to add one random explicit
  modifier.
- `alteration` requires a magic item and reforges it to one or two explicit
  modifiers.
- `regal` requires a magic item, changes it to rare, and attempts to add one
  explicit modifier. The rarity upgrade counts as applied even when no modifier
  can be added.
- `alchemy` requires a normal item, changes it to rare, and rolls four, five,
  or six explicit modifiers, capped by the session affix capacity.
- `chaos` requires a rare item and reforges it to four, five, or six explicit
  modifiers, capped by the session affix capacity.
- `exalt` requires a rare item and attempts to add one random explicit
  modifier.
- `annul` samples uniformly from non-fractured affixes on unlocked sides and
  removes one. If both sides are locked or no eligible affix exists, it does
  nothing. The raw native dispatcher does not apply a rarity guard to this
  action.
- `scour` refuses a normal item. With exactly one locked side, it keeps every
  affix on that side and removes the other side. With neither side locked, it
  keeps only fractured affixes. Its post-action rarity is normal when nothing
  remains, magic when only fractures remain without a lock, and rare when a
  lock was active and at least one affix remains.
- `remove_crafted_modifiers` requires magic or rare rarity and removes every
  crafted affix that is not fractured. It does not remove natural affixes or a
  fractured crafted affix.

The currently implemented `scour` branch treats “both sides locked” like
“neither side locked” for preservation: only fractured affixes survive. That
observed code behavior is not promoted here to an Oliver-approved mechanic
rule; it is listed as an open confirmation below.

## Dated Oliver Rulings

- **2026-07-15:** remove-crafted-modifiers is a real primitive and costs one
  Scour. This is recorded in the archived
  [S7 plan](../archive/2026-07-solver-s7/plan.md).
- **2026-07-15:** a tied or absent Eldritch dominance falls back to these
  ordinary Exalt, Chaos, and Annul transitions. The Eldritch side-intent setup
  is separate and must pay for its setup currency. See
  [Eldritch and influence](eldritch-and-influence.md).

## Engine Coverage And Code Pointers

- `engine/include/poecraft/session.h` — C action enum and request structure.
- `engine/src/actions_basic.cpp` — `apply_action`, `reforge`, `do_add_one`,
  `do_annul`, `do_scour`, and `do_remove_crafted_modifiers`.
- `engine/src/engine_internal.hpp` — shared transition facts for ordinary
  renewals.
- `engine/src/api.cpp` — C ABI request parsing and application.
- `engine/src/solver_calc.cpp` and `engine/src/solver_reforge.cpp` — exact
  single-action distributions.
- `engine/src/solver_registry.cpp` — action descriptors and the one-Scour cost
  key for `remove_crafted_modifiers`.
- `bindings/wasm/wasm_api.cpp` — WASM JSON action parsing.

The native C ABI, Python binding, WASM facade, simulator, exact calculator, and
solver all reach the same native action vocabulary.

## Emulator Support

All ten actions are visible in the Emulator’s basic-currency panel. The panel
invokes the native action and displays whether it applied and how many affixes
were added or removed.

The Emulator also has scratch-only direct modifier add/remove/fracture gestures.
Those gestures edit item state and must not be confused with these random
currency transitions.

## Solver Support

All ten actions have registry descriptors and exact calculation support.
Relevant compound solver operators include Scour-then-Alchemy, renewal loops,
protected-side operations, and protected repeats. They compile to the primitive
actions above; they are not additional currency mechanics.

## Calculator Support

All ten actions are visible in the Calculator’s basic-currency panel and use
the exact native single-action evaluator. The Calculator also exposes the
synthetic `restart` action beside them, but Restart is not a `pc_action_type`
value.

## Explicitly Unsupported Behavior

- Ordinary actions on corrupted or mirrored items are refused; no tainted or
  corruption-specific currency is implemented in this family.
- The engine models modifier identities and structural outcomes, not rolled
  numeric stat values.
- Scour-plus-Alchemy is not one opaque native action. It is a solver option that
  executes two real primitives when the Alchemy step is legal.

## Open Questions Requiring Oliver

- When both Prefixes Cannot Be Changed and Suffixes Cannot Be Changed are
  simultaneously present, should Scour preserve both sides, refuse, or keep the
  currently implemented only-fractures behavior?
- Should raw `annul` be legal on any non-corrupted, non-mirrored item that
  happens to carry removable affixes, or should native application enforce the
  magic/rare legality already used by the solver registry?
