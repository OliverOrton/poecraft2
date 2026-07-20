# Fracture

**Status: current implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: native Fracture transition, exact calculator, solver
primitive and fixed-option paths, compiled strategy output, and Emulator and
Calculator controls.

## Scope

This family owns the `fracture` primitive and explains the separate solver
operator named `fracture_prepare`.

## Implemented Behavior

`fracture` requires a rare, non-corrupted, non-mirrored item with:

- at least four explicit affixes;
- no generic influence;
- no synthesised flag; and
- no already-fractured explicit affix.

It samples uniformly across every live prefix and suffix and sets the fractured
flag on the chosen slot. Crafted affixes and metamods are members of that
uniform set; active metamod effects do not change selection. Split state and
Eldritch implicit tiers are not refusal conditions in the native action.

The action changes only the selected slot flag. It does not add or remove an
affix and does not let the caller choose which affix is hit.

## Dated Oliver Rulings

- **2026-07-17:** Fracture should enter the ordinary solver candidate space
  automatically when it can improve the cheapest route.
- **2026-07-18:** Fracturing is typically an early-craft technique: prepare a
  cheap carrier, potentially with several goal modifiers to improve the chance
  of a useful hit, and make a miss cheap to recover. Product planning should
  use the primitive Fracture in the ordinary solve and recover misses through a
  priced Restart/base route rather than retain per-carrier preparation
  closures.

Both are recorded in the archived
[S8/B1 plan](../archive/2026-07-19-bestiary-solver-s8/plan.md). The later ruling
keeps `fracture_prepare` available only for explicit authored solver envelopes.

## Engine Coverage And Code Pointers

- `engine/src/actions_basic.cpp` — `do_fracture` legality and uniform slot
  selection.
- `engine/src/solver_calc.cpp` — exact one-successor-per-explicit-slot
  distribution.
- `engine/src/solver_registry.cpp` — primitive descriptor, relevance, and
  automatic-product admission.
- `engine/src/solver_options.cpp` and `engine/src/solver_api.cpp` — authored
  `fracture_prepare` option support.
- `engine/src/solver_compile.cpp` — compilation of selected primitive/option
  routes to ordinary strategy nodes.
- `apps/web/src/app/components/pc-emulator.ts` and `pc-calculator.ts` — visible
  Fracture controls.

## Emulator Support

The basic-currency panel exposes the real random Fracture action. The Emulator
also permits a scratch-only right-click gesture that marks a specific live mod,
or directly adds a selected pool mod, as fractured. That deterministic gesture
is an item-state editing aid and is not the `fracture` mechanic.

## Solver Support

The primitive has exact calculation support and can compete as an ordinary
priced automatic candidate when goal-relevant. Each distinct explicit-mod
class produces its exact probability mass.

`fracture_prepare` remains an accepted user-authored fixed-option kind for
explicit envelopes. Automatic product mode does not retain that closure; it
uses ordinary preparation primitives, primitive `fracture`, and `restart` for
miss recovery. A Fracture-relevant product solve therefore requires a supplied
manual `base` price.

## Calculator Support

The Calculator basic panel exposes Fracture for exact single-action outcomes.
Its solver readiness path can surface a missing `base` price when automatic
Fracture planning needs Restart recovery.

## Explicitly Unsupported Behavior

- The mechanic cannot target a selected affix.
- It is refused on generic-influenced, synthesised, already-fractured, or
  fewer-than-four-explicit items, and on every corrupted or mirrored item.
- The Emulator’s deterministic scratch gesture is not available to the solver,
  Calculator, C action API, or compiled strategy as a substitute mechanic.

## Open Questions Requiring Oliver

No unresolved Fracture mechanic rule was found in the inspected code and dated
rulings. Remaining archived R3A/R4-R6 work concerns solver scale and
verification, not the primitive transition law.
