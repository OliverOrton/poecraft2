# Mechanics

**Status: current stable mechanic authority.**

Parent: [Documentation](../README.md)

Vocabulary/index updated on 2026-09-29 for the 36-value primitive enum and
current currency, Calculator and editor surfaces. The earlier 2026-08-25 audit
at `a1449fa` covered the original 26 actions and solver vocabulary. Family
transition-law pages retain their own verification stamps; this documentation
update does not infer or change a Path of Exile mechanic.

The original full-matrix verification covered native mutation paths, C ABI
request parsing, exact single-action calculation, solver
registry/options/compiler, WASM facade, worker protocol, and the Emulator,
Calculator, and Strategy Builder source surfaces. No external mechanic
research was used.

## Scope

This area records only behavior that is implemented in poecraft2 or stated in
an existing dated Oliver ruling. It is the permanent mechanic authority for
the project; historical plans remain evidence of when a ruling was made but
have no current sequencing authority.

“Supported” is surface-specific. A native action may be fully implemented even
when the product exposes it through an indirect picker, the solver can expose a
compound option that is not a primitive action, and the Strategy Builder can
execute vocabulary that its visual palette does not make convenient to author.

Configured cluster inputs have a separate [native mechanic contract](clusters.md).
Its admitted basic currencies and concrete single-action Calculator scope do
not imply rare reforge or solver-continuation qualification.

## Complete Primitive Coverage

The completeness check started with the ordinal `pc_action_type` enum in
`engine/include/poecraft/session.h`, compared it with `ActionType` in
`engine/src/engine_internal.hpp`, then checked the C ABI parser, simulator
parser, WASM parser, TypeScript `CraftAction` union, exact calculator support,
solver registry, and product controls. Every enum value appears exactly once
below.

Surface labels:

- **panel**: a dedicated visible product control exists;
- **crafted pool**: the Emulator invokes the real bench action by selecting a
  crafted modifier in its modifier pool rather than from a craft panel;
- **registry picker**: the Calculator can select the registry action, but its
  manual craft-panel tabs do not provide a dedicated bench panel;
- **dropdown**: the Strategy Builder operation dropdown accepts the primitive;
- **registry**: the native solver can register the primitive when the selected
  session and request make it legal.

| Ordinal | Action ID | Family | Exact calculator | Solver | Emulator | Calculator | Strategy Builder |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 0 | `transmute` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 1 | `augment` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 2 | `alteration` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 3 | `regal` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 4 | `alchemy` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 5 | `chaos` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 6 | `exalt` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 7 | `annul` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 8 | `scour` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 9 | `essence` | [Essences](essences.md) | yes | parameterized registry | panel | panel | dropdown |
| 10 | `fossil` | [Fossils](fossils.md) | yes | parameterized registry | panel | panel | dropdown |
| 11 | `bench` | [Bench and metamods](bench-and-metamods.md) | yes | parameterized registry | crafted pool | registry picker | dropdown |
| 12 | `veiled_chaos` | [Veiled crafting](veiled-crafting.md) | yes | registry | panel | panel | dropdown |
| 13 | `veiled_exalt` | [Veiled crafting](veiled-crafting.md) | yes | registry | panel | panel | dropdown |
| 14 | `unveil` | [Veiled crafting](veiled-crafting.md) | yes | registry | panel | panel | modifier cards |
| 15 | `harvest_reforge` | [Harvest](harvest.md) | yes | parameterized registry | panel | panel | dropdown |
| 16 | `harvest_augment` | [Harvest](harvest.md) | yes | parameterized registry | panel | panel | dropdown |
| 17 | `harvest_resist` | [Harvest](harvest.md) | yes | parameterized registry | panel | panel | dropdown |
| 18 | `eldritch_ember` | [Eldritch and influence](eldritch-and-influence.md) | yes | tiered registry | panel | panel | dropdown |
| 19 | `eldritch_ichor` | [Eldritch and influence](eldritch-and-influence.md) | yes | tiered registry | panel | panel | dropdown |
| 20 | `eldritch_exalt` | [Eldritch and influence](eldritch-and-influence.md) | yes | registry | panel | panel | dropdown |
| 21 | `eldritch_chaos` | [Eldritch and influence](eldritch-and-influence.md) | yes | registry | panel | panel | dropdown |
| 22 | `eldritch_annul` | [Eldritch and influence](eldritch-and-influence.md) | yes | registry | panel | panel | dropdown |
| 23 | `influence_exalt` | [Eldritch and influence](eldritch-and-influence.md) | yes | parameterized registry | panel | panel | dropdown |
| 24 | `fracture` | [Fracture](fracture.md) | yes | registry | panel | panel | dropdown |
| 25 | `remove_crafted_modifiers` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 26 | `foulborn_augment` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 27 | `foulborn_regal` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 28 | `foulborn_exalt` | [Ordinary currency](ordinary-currency.md) | yes | registry | panel | panel | dropdown |
| 29 | `remembrance` | [Memory](memory-and-corruption.md) | held | no | unavailable control | unavailable control | no |
| 30 | `unravelling` | [Memory](memory-and-corruption.md) | held | no | unavailable control | unavailable control | no |
| 31 | `dominance` | [Influence](eldritch-and-influence.md) | yes, single action | no | panel | panel | dropdown |
| 32 | `tempering` | [Enchantments](memory-and-corruption.md) | held: no public weights | no | unavailable control | unavailable control | no |
| 33 | `tailoring` | [Enchantments](memory-and-corruption.md) | held: no public weights | no | unavailable control | unavailable control | no |
| 34 | `vaal` | [Corruption](memory-and-corruption.md) | ordinary equipment | no | Basic panel | Basic panel | dropdown |
| 35 | `double_corruption` | [Corruption](memory-and-corruption.md) | approved terminal goal projection | no | held Temple control | Temple panel | no |

Enabled primitive crafts pass through the native craftability guard: corrupted
or mirrored items refuse the action. Held laws remain explicit refusals. Manual
item authoring is a separate editor operation. Family files record additional
legality, transition rules and the supported subsets.

## Vocabulary Outside `pc_action_type`

These implemented operations are intentionally outside the 36-value C enum:

| Operation | Native/product role | Emulator | Solver | Calculator | Strategy Builder |
| --- | --- | --- | --- | --- | --- |
| `restart` | synthetic fresh-base transition priced by `base` | no | registered synthetic action | Basic panel | operation dropdown and simulator support |
| `condition_check_only` | mutation-free router operation | no | emitted as routing structure, not a priced primitive | no direct action | operation dropdown and simulator support |
| `bestiary:imprint` | deterministic checkpoint creation | panel | automatic Imprint option dependency, not an ordinary registry row | panel and native compound-state goal calculation | operation dropdown and simulator support |
| `bestiary:restore_imprint` | deterministic checkpoint restore | panel | automatic Imprint option dependency, not an ordinary registry row | panel and native compound-state goal calculation | operation dropdown and simulator support |
| `awakener` | role-based donor/receiver transaction | Stash donor panel | no | read-only retained-pair/refill goal calculation | operation dropdown and simulator support |

Bestiary details and its two explicitly unsupported recipe IDs are in
[Bestiary Imprint](bestiary-imprint.md). Conditions, fixed options, and exact
strategy-evaluation limits are in
[Strategy and solver vocabulary](strategy-and-solver-vocabulary.md).

## Complete Parameterized Solver Vocabulary

The registry builds the following stable action IDs. Session filtering can omit
an ID when its data is unavailable or the action cannot be legal for that
session.

- fixed IDs: non-parameterized primitive IDs marked **registry** in the table
  above, plus `restart`; recognition alone does not register held or
  single-action-only currencies;
- `essence:<metadata-key>`;
- `fossil:<key>` through `fossil:<key1>+<key2>+<key3>+<key4>`, with sorted
  unique fossil keys and cost keys for each fossil plus
  `resonator:<socket-count>`;
- `bench:<mod-key>`;
- `harvest_reforge:<tag>` and `harvest_augment:<tag>` from the approved
  allowlist;
- `harvest_resist:<source>:<target>` for the six ordered fire, cold, and
  lightning conversions; its cost key is `harvest_resist:<target>`;
- `eldritch_ember:1` through `eldritch_ember:4` and
  `eldritch_ichor:1` through `eldritch_ichor:4`;
- `influence_exalt:<currency-influence>` for `crusader`, `hunter`, `redeemer`,
  and `warlord`; the compiled modifier pools separately retain the internal
  `adjudicator`, `basilisk`, `crusader`, `elder`, `eyrie`, and `shaper`
  identities;
- `fracture` and `remove_crafted_modifiers`.

The fixed-option kinds accepted from solver request JSON are
`scour_alchemy`, `eldritch_side_intent`, `protected_side`,
`multimod_finish`, `renewal`, `protected_repeat`, and `fracture_prepare`.
`imprint_retry` is automatic-only and is rejected as user-authored input.
The internal automatic machinery also has a temporary-bench repeat kernel.
These are solver operators that compile to primitive strategy operations; they
are not additional crafting rules.

## Mechanic Families

- [Ordinary currency](ordinary-currency.md) — basic rarity, add, remove,
  reforge, Scour, and crafted-modifier cleanup actions.
- [Essences](essences.md) — guaranteed-mod rare reforges and the
  corruption-only boundary.
- [Fossils](fossils.md) — loadouts, fossil pool weighting, forced/added mods,
  and implemented one-item special effects.
- [Bench and metamods](bench-and-metamods.md) — crafted-mod limits, Multimod,
  side locks, and cannot-roll pool filters.
- [Veiled crafting](veiled-crafting.md) — Veiled Chaos, Veiled Exalt, offer
  generation, and Unveil selection.
- [Harvest](harvest.md) — approved targeted reforge, add/remove augment, and
  resistance-conversion actions.
- [Eldritch and influence](eldritch-and-influence.md) — Eldritch implicits,
  dominance-sensitive explicit currency, and influence exalts.
- [Fracture](fracture.md) — random explicit-mod fracture and its solver role.
- [Bestiary Imprint](bestiary-imprint.md) — checkpoint creation and restore.
- [Memory, enchantments and corruption](memory-and-corruption.md) — held memory
  and enchantment laws, Vaal and the approved double-corruption projection.
- [Strategy and solver vocabulary](strategy-and-solver-vocabulary.md) —
  synthetic operations, conditions, solver IDs, and compound option support.

## Known Cross-Surface Boundaries

- Special Essences use the ordinary item reforge with their canonical
  guaranteed modifier. Their corruption-only flag describes acquisition;
  obtaining/upgrading Essences from trapped monsters is outside this model.
- The visual Strategy Builder exposes fewer condition leaf types than the JSON
  compiler/simulator accepts.
- Exact single-action support follows the table above. All supported Calculator
  actions evaluate one native Input → Goal predicate, including explicit mods,
  implicit keys and selected influence/corruption properties. Extended goals
  remain outside Strategy finder. Whole-graph exact strategy evaluation supports
  `mod_count`, `mod_family_count`, `has_unveil_option`, authored Unveil selection
  and checkpoint-aware Bestiary descriptors within their documented grammar.
- Bench is a real native action even though neither product surface has a
  dedicated bench craft-panel tab.
- `restart` is present in the visual Strategy Builder operation dropdown and
  retains its separate priced fresh-base transition.
- Veiled offers have different internal representations: the Simulator persists
  offers at placeholder acquisition, while the exact solver samples them when
  Unveil is observed. These are distribution-equivalent only within the admitted
  immediate-observation grammar. Post-acquisition blockers remain unsupported
  under Oliver's ruling in [Veiled crafting](veiled-crafting.md).

Open mechanic questions are kept in the family files that own them. The
current code-inspection audit found questions about double-side-lock Scour,
raw normal-item Annulment, and whether the implemented partial fossil special
effects are the intended permanent contract.

## Currency expansion delta

The [currency execution record](../active/2026-09-28-currency-expansion/README.md)
owns the dated validation and release evidence for appended IDs 26–35.
Foulborn is documented in Ordinary currency; Shaper/Elder, Awakener and Dominance
in Eldritch and influence; evidence-held operations and scoped Vaal sampling in
[Memory, enchantments and corruption](memory-and-corruption.md). A recognized
primitive ID is not a claim of exact or automatic support.
