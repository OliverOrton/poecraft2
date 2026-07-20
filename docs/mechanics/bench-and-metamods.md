# Bench And Metamods

**Status: current implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: native bench application and metamod scans, shared action
transition facts, solver registry/options/compiler, and product crafted-mod
surfaces.

## Scope

This family owns the parameterized `bench` primitive, solver IDs of the form
`bench:<mod-key>`, and the implemented effects of crafted Multimod, Prefixes
Cannot Be Changed, Suffixes Cannot Be Changed, Cannot Roll Attack Modifiers,
and Cannot Roll Caster Modifiers. Crafted-modifier cleanup is documented in
[Ordinary currency](ordinary-currency.md).

## Implemented Behavior

`bench` requires a magic or rare, non-corrupted, non-mirrored item and a
session-visible modifier marked bench-craftable for the selected item class.
It adds that modifier directly with the crafted flag, while the shared direct
add path enforces side capacity and modifier-group conflicts.

Crafted-count limits are:

- without Multimod, at most one crafted affix;
- while adding Multimod, or while Multimod is already present, at most three
  total crafted affixes; and
- a non-Multimod craft is refused when any crafted affix already exists and
  Multimod is absent.

The engine discovers metamod state by scanning live explicit affixes. A
fractured crafted metamod still sets the corresponding flag until an action
whose contract explicitly ignores metamods is used.

- Prefixes Cannot Be Changed and Suffixes Cannot Be Changed preserve every
  affix on the named side during ordinary renewals. Removal actions exclude a
  locked side. A metamod on a preserved side is itself preserved.
- Cannot Roll Attack/Caster remove candidate modifiers carrying the respective
  classification tag from ordinary pool construction.
- Essence and Fossil are the owner-defined exceptions: both ignore side locks
  and cannot-roll filters while still preserving fractured affixes.
- `remove_crafted_modifiers` removes all non-fractured crafted affixes in one
  primitive and uses the `scour` cost key.

Applying a new bench craft does not replace an existing crafted affix in place.
Cleanup and recrafting are separate actions.

## Dated Oliver Rulings

- **2026-07-15:** remove-crafted-modifiers is a real primitive costing one
  Scour.
- **2026-07-17:** Essence and Fossil ignore every metamod side lock and
  cannot-roll restriction; a fractured metamod can survive but its effect is
  ignored for those rolls.
- **2026-07-17:** solver action-space work recognizes permanent finishes,
  temporary group/slot blockers, metamod setup, Multimod finishes, and
  cleanup/replacement as distinct useful bench roles.

The first ruling is in the archived
[S7 plan](../archive/2026-07-solver-s7/plan.md);
the latter two are in the archived
[S8/B1 plan](../archive/2026-07-19-bestiary-solver-s8/plan.md).

## Engine Coverage And Code Pointers

- `engine/src/actions_basic.cpp` — `do_bench`, `item_has_metamod`,
  `side_locked`, preservation, removal, and cleanup.
- `engine/src/engine_internal.hpp` — loaded metamod codes and shared
  `ActionTransitionFacts`.
- `engine/src/session_builder.cpp` — bench-craftable session set and metamod
  classification.
- `engine/src/solver_registry.cpp` — `bench:<mod-key>` descriptors, cost
  vectors, flags, and automatic-candidate metadata.
- `engine/src/solver_options.cpp` and `engine/src/solver_compile.cpp` — protected,
  temporary-bench, and Multimod compound kernels and primitive compilation.
- `engine/src/solver_calc.cpp` — exact bench successor and metamod-aware
  calculation.

## Emulator Support

The Emulator has no dedicated Bench craft tab. Its modifier pool groups crafted
mods separately; selecting one in direct-craft mode invokes the real native
`bench` action. Selecting a non-crafted pool row in that scratch surface can
force-add state directly and is not a bench mechanic.

The basic panel exposes `remove_crafted_modifiers` for cleanup.

## Solver Support

The registry includes each session bench option as `bench:<mod-key>` with the
canonical bench cost vector. The exact calculator supports deterministic bench
application and cleanup.

User-authored fixed options include `protected_side`, `protected_repeat`, and
`multimod_finish`. Automatic candidate generation can synthesize a permanent
goal bench, a temporary blocker/repeat, a protected metamod route, or a
Multimod finish when its exact kernel and dependencies are complete. These
options compile to ordinary `bench`, follow-up, cleanup, and routing nodes.

## Calculator Support

The Calculator’s visible craft-panel tabs do not include a Bench panel. Bench
actions are nevertheless present in its solver registry/picker and in solver
results, and their costs and exact deterministic successors are supported.
Crafted modifier goals can also be selected from the modifier-pool goal editor;
goal selection is not itself application of a bench action.

## Explicitly Unsupported Behavior

- There is no opaque “protected craft” native mechanic; solver protected-side
  options apply and pay for each primitive setup, follow-up, reapplication, and
  cleanup.
- There is no in-place bench replacement primitive. Existing crafted affixes
  must be removed through another supported action.
- Essence/Fossil protection by a metamod is explicitly not supported.

## Open Questions Requiring Oliver

- The native Scour path currently keeps only fractured affixes when both side
  locks are present. What should the authoritative double-lock Scour behavior
  be? This is the same open question recorded in
  [Ordinary currency](ordinary-currency.md).
