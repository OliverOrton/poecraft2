# Essences

**Status: current implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: compiled Essence data, native application and exact
reforge paths, solver registry, WASM facade, and the web catalog and Essence
pickers.

## Scope

This family owns the parameterized `essence` primitive and solver IDs of the
form `essence:<metadata-key>`, including Horror, Hysteria, Insanity and Delirium.
Their `is_corruption_only` flag describes acquisition, not a different item craft.

## Implemented Behavior

For a resolvable Essence key and selected item class, the session stores one
guaranteed modifier. Applying the action:

1. refuses corrupted or mirrored items;
2. enforces the compiled Essence item-level restriction;
3. changes the item to rare;
4. removes every non-fractured explicit affix, including crafted metamods;
5. adds the item-class-specific guaranteed modifier directly; and
6. fills from the ordinary random pool toward a random four-to-six-mod rare
   result, capped by the session affix capacity.

Essence application ignores every active metamod effect. Prefix/suffix locks do
not preserve their sides and Cannot Roll Attack/Caster does not filter either
the guaranteed modifier or filler pool. Fractured affixes survive independently,
including a fractured metamod affix, but that fractured metamod’s effect is
ignored during the roll.

The C action parser resolves the metadata key to the session Essence index. The
solver registry emits `essence:<metadata-key>` only when the selected session
has a resolvable guaranteed modifier.

## Dated Oliver Rulings

- **2026-09-28:** For Horror, Hysteria, Insanity and Delirium, corruption-only
  describes how the Essence is obtained. Using one on an item is the existing
  Essence reforge with its item-class-specific guaranteed modifier. This
  supersedes the former selector/registry exclusion based on that flag.
- **2026-07-17:** Essence and Fossil renewals ignore every metamod side lock and
  Cannot Roll pool restriction. Fractured affixes survive independently; a
  fractured metamod may survive but has no effect on that roll. This correction
  is recorded in the archived
  [S8/B1 plan](../archive/2026-07-19-bestiary-solver-s8/plan.md).

## Engine Coverage And Code Pointers

- `tools/ingest/poecraft_ingest/compiled_data.py` — compiled Essence arrays,
  including `is_corruption_only` in the artifact.
- `engine/src/data_loader.cpp` and `engine/src/engine_internal.hpp` — fields
  loaded into native `DataImpl`, including the validated parallel
  `is_corruption_only` array.
- `engine/src/session_builder.cpp` — item-class guaranteed-mod resolution.
- `engine/src/api.cpp` — `essence_key` request parsing.
- `engine/src/actions_basic.cpp` — sampled guaranteed-mod reforge.
- `engine/src/solver_reforge.cpp` and `engine/src/solver_calc.cpp` — exact
  reforge distribution.
- `engine/src/solver_registry.cpp` — `essence:<metadata-key>` descriptors.
- `apps/web/src/app/engine-worker.ts` — named Essence catalogue presentation.

## Emulator Support

The Essence panel groups named Essence entries by type and tier and sends the
chosen `essence_key` to the native action. Horror, Hysteria, Insanity and Delirium
appear with a Special tier. Their artwork and item-class tooltip text follow
the same canonical joins as other Essences. Remnant of Corruption is not an item
reforge and does not appear as an Essence type.

## Solver Support

The solver registry and exact calculator support the guaranteed-mod
reforge for each resolvable Essence. Product goal filtering
keeps only an Essence whose guaranteed modifier exactly matches a goal
family and records unrelated Essences under a stable filtered reason.
The acquisition flag does not affect admission. The former
`filtered_corruption_only_essence` rejection reason is retired.

Protected-side solver options deliberately do not admit Essence as the
protected renewal, because the dated owner ruling says it ignores every
metamod.

## Calculator Support

The Calculator’s Essence panel uses the same web catalog as the
Emulator and evaluates `essence:<metadata-key>` with the exact native reforge
calculator.

## Explicitly Unsupported Behavior

- Obtaining/upgrading Essences by corrupting trapped monsters is outside this
  one-item crafting model.
- Essence rolls do not preserve or obey any metamod effect.
