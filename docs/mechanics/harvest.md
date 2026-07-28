# Harvest

**Status: current implemented mechanic reference using the owner-approved allowlist.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-28 @ active

Verification scope: checked-in recipe manifest, generated native allowlist,
sampled and exact native transitions, solver registry, and Emulator and
Calculator controls. No live or external source was consulted.

## Scope

This family owns `harvest_reforge`, `harvest_augment`, and `harvest_resist`.
Available parameter values come only from the checked-in
[Harvest recipe manifest](../../fixtures/economy/harvest-recipes-v1.json), not
from arbitrary session tags.

The exact allowlist is:

- reforge: `fire`, `cold`, `lightning`, `physical`, `life`, `defences`,
  `chaos`, `attack`, `caster`, `speed`, `critical`, `minion`, `elemental`,
  `attribute`, `mana`, and `drop`;
- augment: `fire`, `cold`, `lightning`, `physical`, `life`, `defences`,
  `chaos`, `attack`, `caster`, `speed`, and `critical`; and
- resistance conversion: the six ordered source/target pairs among `fire`,
  `cold`, and `lightning`.

## Implemented Behavior

`harvest_reforge:<tag>` requires a rare, non-corrupted, non-mirrored item. It
preserves fractured affixes and locked sides, adds one guaranteed first
modifier from the target-tagged ordinary naturally rollable pool, then fills
from the ordinary pool toward a random four-to-six-mod rare result. Cannot
Roll Attack/Caster filters apply to both the guaranteed and filler pools.

`harvest_augment:<tag>` requires a magic or rare, non-corrupted,
non-mirrored item with no generic influence and no Eldritch implicit tier. It
adds one target-tagged modifier from the ordinary naturally rollable pool, then
samples uniformly from every other non-fractured affix on unlocked sides and
removes one. If there is no other removable affix, the added modifier remains.
The add step obeys capacity, group conflicts, and Cannot Roll filters.

`harvest_resist:<source>:<target>` requires magic or rare rarity. It selects an
eligible non-fractured source modifier on an unlocked side that carries both
the `resistance` tag and the requested source-element tag. It removes that
modifier and chooses an ordinary naturally rollable replacement on the same
side with the same required level, the `resistance` and target-element tags,
and without the source-element tag. If a selected source has no replacement,
it is restored and another distinct source modifier ID may be attempted. The
action refuses when no complete conversion is possible.

For all three operations, “ordinary naturally rollable” means positive spawn
weight, positive ordinary generation weight, membership in the requested
target-tag classification, and the ordinary final roll weight. Zero ordinary
generation weight always excludes a modifier. Harvest does not replace its
generation percentage with `100` or revive a zero-generation modifier.

The resistance registry ID contains both source and target. Its economy cost
key is target-specific: `harvest_resist:<target>`.

## Dated Oliver Rulings

- **2026-07-28:** Harvest reforge, augment, and resistance conversion all use
  the target-restricted ordinary naturally rollable pool. Both spawn and
  ordinary generation weights must be positive, and selection uses ordinary
  final roll weight. A zero-generation modifier remains unavailable.
- **2026-07-15:** the checked-in current-core-game recipe manifest is the
  owner-approved engine/web allowlist and price mapping. Arbitrary session tags
  must not become actions; the canonical key is `defences`; resistance prices
  depend on the target element. This is recorded in the archived
  [economy plan](../archive/2026-07-15-economy/plan.md).

The native solver registry also records the owner ruling that Harvest augment
is add-then-remove. The inspected source does not attach a date to that ruling,
so this reference does not invent one.

## Engine Coverage And Code Pointers

- `fixtures/economy/harvest-recipes-v1.json` — approved action and recipe
  allowlist.
- `scripts/generate-harvest-crafts.py` and
  `engine/src/harvest_crafts.generated.hpp.in` —
  build-time native arrays derived from the fixture.
- `engine/src/actions_basic.cpp` — sampled reforge, augment, and resistance
  conversion.
- `engine/src/session_builder.cpp` — shared targeted-natural pool
  construction.
- `engine/src/solver_registry.cpp` — exact action IDs, legality, and cost keys.
- `engine/src/solver_reforge.cpp` — exact guaranteed-first reforge distribution.
- `engine/src/solver_calc.cpp` — exact add-then-remove and resistance
  conversion distributions.
- `apps/web/src/app/harvest-crafts.ts` — product options generated from the
  same allowlist vocabulary.

## Emulator Support

The Harvest panel exposes the approved reforge and augment tags and all six
resistance conversions. It sends only the selected target tag, or source and
target pair, to the native engine; TypeScript does not recreate the outcome
rules.

## Solver Support

Every allowlisted action whose required tag pool is nonempty for the selected
session can receive a registry row. All three mechanic types have exact
single-action evaluation. The registry treats target tags as action parameters
and resistance source/target tags as state discriminators.

Harvest reforge can be used by a protected-side option because it respects
metamods. Harvest augment and resistance conversion are exact single-slot
special actions rather than renewal programs.

## Calculator Support

The Calculator Harvest panel exposes the same allowlist. It resolves the
parameterized registry ID, calculates exact outcomes, and derives prices from
the versioned recipe manifest.

## Explicitly Unsupported Behavior

- Arbitrary tags outside the enumerated manifest do not become solver-registry
  or product Harvest actions.
- The C ABI and strategy parser enforce the allowlist for reforge and augment,
  but currently do not enforce the six-pair allowlist for `harvest_resist`.
  A raw caller can therefore submit other known, distinct tag names; this
  backend exposure is not an approved additional Harvest recipe.
- Harvest augment is unavailable on generic-influenced or Eldritch-implicit
  items under the current native and solver contract.
- No Harvest operation beyond the three named primitive types and their
  allowlisted parameters is implemented.

## Open Questions Requiring Oliver

- Should the C ABI and strategy compiler reject every `harvest_resist` pair
  outside the six owner-approved fire/cold/lightning conversions, matching the
  solver registry and product controls?
- The existing add-then-remove owner ruling lacks a recorded date. The date is
  a provenance gap; this reference does not infer one.
