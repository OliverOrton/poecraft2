# Fossils

**Status: current implemented mechanic reference with named partial special-effect support.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: compiled Fossil fields, native pool construction and
special effects, exact reforge calculation, solver loadout registry, and the
Emulator and Calculator Fossil controls.

## Scope

This family owns the parameterized `fossil` primitive. A request supplies one
through four Fossil keys. The C parser resolves, sorts, and deduplicates them.
The solver ID is the plus-joined sorted unique loadout, and its cost vector is
one cost key per Fossil plus `resonator:<loadout-size>`.

## Implemented Behavior

Fossil application refuses corrupted or mirrored items, changes any other
rarity to rare, preserves fractured affixes, and ignores every metamod effect.
It does not preserve locked sides and does not apply Cannot Roll Attack/Caster
filters.

For the resolved loadout, native pool construction:

- starts from the ordinary random-roll universe;
- adds the loadout’s compiled “added mod” rows;
- applies all compiled positive and negative classification-tag weight
  multipliers;
- directly inserts the loadout’s compiled forced modifiers, deduplicated by
  modifier ID; and
- fills toward a random four-to-six-mod rare result, capped by the session
  affix capacity.

When a selected Fossil has the compiled `rolls_lucky` flag, the current code
multiplies each candidate’s pool weight by
`100 + max(required_level - 40, 0)` percent. This is implemented candidate
weighting; the engine does not model numeric stat rolls.

After a successful reforge, the native one-item special-effect pass implements:

- a Fossil named `Bloodstained Fossil`: choose one weighted corrupted implicit
  from the session, add it when possible, and then set the corrupted item flag;
- any compiled sell-price implicit links: choose one linked implicit and add
  it when possible; and
- any Fossil whose compiled `mirrors` flag is set: mark the resulting live item
  mirrored.

Those special effects mutate the one live item. No second output object is
created.

## Dated Oliver Rulings

- **2026-07-17:** Fossil and Essence renewals ignore every metamod side lock
  and Cannot Roll pool restriction. Fractured affixes survive independently;
  a fractured metamod may survive but has no effect on the roll. This is
  recorded in the archived
  [S8/B1 plan](../archive/2026-07-19-bestiary-solver-s8/plan.md).

## Engine Coverage And Code Pointers

- `tools/ingest/poecraft_ingest/compiled_data.py` — compiled Fossil flags,
  weights, and linked modifiers.
- `engine/src/data_loader.cpp` and `engine/src/engine_internal.hpp` — Fossil
  fields retained by native `DataImpl`.
- `engine/src/api.cpp` — one-to-four-key validation, sorting, and
  deduplication.
- `engine/src/session_builder.cpp` — added/forced/sell-price session links,
  fixed Fossil multipliers, and the required-level weight multiplier.
- `engine/src/actions_basic.cpp` — sampled reforge and
  `apply_fossil_specials`.
- `engine/src/solver_reforge.cpp` — exact multi-mod reforge distribution.
- `engine/src/solver_registry.cpp` — lazy sorted unique one-to-four-Fossil
  action IDs and resonator cost keys.

## Emulator Support

The Fossil panel lets the user build a loadout from the product catalog,
prevents duplicate selections, limits the selection to four, and applies the
native `fossil` action. The same one-item special effects are visible through
the resulting item state.

## Solver Support

The solver lazily enumerates sorted unique one-to-four-Fossil loadouts and can
filter them for relevance before exact calculation. Exact reforge calculation
uses the same added/forced rows and weight transformations as sampled
application. It projects Bloodstained corruption and the mirroring flag into
abstract item flags, but it does not enumerate the sampled corrupted-implicit
or sell-price-implicit identities. Protected-side options do not admit Fossil
because the owner ruling says it ignores every metamod.

Special flags that affect auxiliary item state are not all represented in the
solver’s abstract goal language; registry presence is not a claim that every
auxiliary special effect can be optimized as a goal.

## Calculator Support

The Calculator Fossil panel builds the same one-to-four selection and resolves
the matching solver registry ID. It shows exact explicit-mod outcome
probabilities and prices the loadout by its component Fossils and resonator.

## Explicitly Unsupported Behavior

- Compiled `changes_quality`, `rolls_white_sockets`, and
  `corrupted_essence_chance` fields are not loaded into the native mechanic and
  have no action behavior.
- The `mirrors` special marks the one output item mirrored; it does not produce
  a second item or copy.
- Exact reforge calculation tracks the implemented corruption/mirroring item
  flags but does not preserve the sampled special-implicit identity as an exact
  outcome dimension. Its Bloodstained flag projection checks only that the
  session has a corrupted-implicit candidate, whereas sampled application sets
  the flag only after that implicit is successfully added.
- Numeric modifier value rolls, including lucky numeric value rolls, are not
  represented.
- Corrupted-Essence generation from a Fossil is not implemented.

## Open Questions Requiring Oliver

- Is the implemented `rolls_lucky` required-level pool-weight multiplier the
  intended permanent contract for this simulator, or should that field remain
  explicitly partial until numeric rolls exist?
- Is marking the single live result mirrored the intended one-item abstraction
  for a mirroring Fossil, or should the mechanic be unsupported until a
  multi-output contract exists?
- Should Bloodstained and sell-price implicit effects remain supported while
  quality, white-socket, and corrupted-Essence effects are unsupported, or is a
  different explicit partial-support boundary required?
- Is flag-only exact projection sufficient for Bloodstained/mirroring Fossil
  outcomes, or must exact calculation also represent the resulting special
  implicit identity before these effects are considered fully supported?
