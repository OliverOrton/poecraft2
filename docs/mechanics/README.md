# Mechanics

**Status: current stable mechanic authority.**

Parent: [Documentation](../README.md)

Verified against code: 2026-07-19 @ d5e38e3

Verification scope: native action enums and mutation paths, C ABI request
parsing, exact single-action calculation, solver registry/options/compiler,
WASM facade, worker protocol, and the Emulator, Calculator, and Strategy
Builder source surfaces. No external mechanic research was used.

## Scope

This area records only behavior that is implemented in poecraft2 or stated in
an existing dated Oliver ruling. It is the permanent mechanic authority for
the project; historical plans remain evidence of when a ruling was made but
have no current sequencing authority.

“Supported” is surface-specific. A native action may be fully implemented even
when the product exposes it through an indirect picker, the solver can expose a
compound option that is not a primitive action, and the Strategy Builder can
execute vocabulary that its visual palette does not make convenient to author.

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
| 14 | `unveil` | [Veiled crafting](veiled-crafting.md) | yes | registry | panel | panel | dropdown |
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

All 26 primitives first pass through the native craftability guard: corrupted
or mirrored items refuse the action. Family files record the additional
legality and transition rules.

## Vocabulary Outside `pc_action_type`

These implemented operations are intentionally outside the 26-value C enum:

| Operation | Native/product role | Emulator | Solver | Calculator | Strategy Builder |
| --- | --- | --- | --- | --- | --- |
| `restart` | synthetic fresh-base transition priced by `base` | no | registered synthetic action | visible basic-panel action | compiler/simulator support; absent from visual operation dropdown |
| `condition_check_only` | mutation-free router operation | no | emitted as routing structure, not a priced primitive | no direct action | operation dropdown and simulator support |
| `bestiary:imprint` | deterministic checkpoint creation | panel | automatic Imprint option dependency, not an ordinary registry row | panel and dedicated exact Bestiary calculation | operation dropdown and simulator support |
| `bestiary:restore_imprint` | deterministic checkpoint restore | panel | automatic Imprint option dependency, not an ordinary registry row | panel and dedicated exact Bestiary calculation | operation dropdown and simulator support |

Bestiary details and its two explicitly unsupported recipe IDs are in
[Bestiary Imprint](bestiary-imprint.md). Conditions, fixed options, and exact
strategy-evaluation limits are in
[Strategy and solver vocabulary](strategy-and-solver-vocabulary.md).

## Complete Parameterized Solver Vocabulary

The registry builds the following stable action IDs. Session filtering can omit
an ID when its data is unavailable or the action cannot be legal for that
session.

- fixed IDs: all non-parameterized primitive IDs in the table above, plus
  `restart`;
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
- `influence_exalt:<influence-name>`; the checked artifact exposes
  `adjudicator`, `basilisk`, `crusader`, `elder`, `eyrie`, and `shaper`;
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
- [Strategy and solver vocabulary](strategy-and-solver-vocabulary.md) —
  synthetic operations, conditions, solver IDs, and compound option support.

## Known Cross-Surface Boundaries

- The Emulator and Calculator intentionally filter corruption-only Essences
  from their catalog, while the native data/session path does not retain that
  flag. Corruption-only Essence behavior is not implemented.
- The visual Strategy Builder exposes fewer condition leaf types than the JSON
  compiler/simulator accepts.
- The exact single-action calculator supports every primitive. Whole-graph
  exact strategy evaluation supports `mod_count`, `mod_family_count`,
  `has_unveil_option`, and authored Unveil selection; the two Bestiary
  operation IDs remain on their separate stateful calculation path rather
  than ordinary evaluator actions.
- Bench is a real native action even though neither product surface has a
  dedicated bench craft-panel tab.
- Solver-generated `restart` nodes compile and simulate, but `restart` is not
  present in the visual Strategy Builder operation dropdown.

Open mechanic questions are kept in the family files that own them. The
current code-inspection audit found questions about double-side-lock Scour,
raw normal-item Annulment, corruption-only Essence rejection, and whether the
implemented partial fossil special effects are the intended permanent contract.
