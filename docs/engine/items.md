# Item state

**Status: implemented engine contract.** This page describes storage and
ownership, not the rules of individual crafts.

Parent: [Engine](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Mechanic behavior belongs in [Mechanics](../mechanics/README.md). Pool and
weight construction are described in [Pools](pools.md) and
[Weights](weights.md).

## Role and ownership

`pc_item_state` is a compact, caller-owned value representing the mutable
state of one item. It contains no pointers or owning allocations, so the
engine can copy a state before an action and commit it only on success.

The selected base, item level, class, base tags, and session-local mod catalog
belong to `pc_session`, not to the item. A slot's `mod_id` is therefore valid
only with the session that produced it. Persistent JSON uses stable mod and
base keys and resolves them when imported.

The C ABI leaves a passed item unchanged when an action fails. Callers own the
item's lifetime; session and action-context handles must remain valid while an
action interprets its IDs.

## Fixed layout

The public layout is declared in `engine/include/poecraft/item_state.h`.

| State | Capacity or representation |
| --- | --- |
| Prefixes | 3 `pc_mod_slot` values |
| Suffixes | 3 `pc_mod_slot` values |
| Implicits | 8 `pc_mod_slot` values |
| Enchantments | 4 `pc_mod_slot` values |
| Numeric rolls per slot | 8 signed 32-bit values |
| Veiled options per slot | 3 session-local mod IDs |
| Sockets | 6 color bytes plus an adjacency link mask |
| Generic influences | One byte of artifact influence-code bits |
| Eldritch influence | Separate Searing Exarch and Eater tier bytes |

Each mod slot stores its dense session mod ID, a cached primary group ID,
flags, optional numeric rolls, and optional unveil state. Slot flags are
fractured, crafted, veiled, eldritch, and synthesised. Item flags are
corrupted, mirrored, split, and synthesised.

These capacities are implementation limits, not general game-rule claims.
The data capacity check and import paths reject unsupported input rather than
silently truncating it.

## Affix capacity

The item-only helpers implement the generic limits: one prefix and suffix for
magic items and three of each for rare items. The crafting path uses the
session's base-dependent cap instead. It sets rare jewels and abyss jewels to
two affixes per side and ordinary supported bases to three.

The item state itself does not decide action legality. Rarity transitions,
locks, fractured preservation, influence behavior, and mechanic-specific
conditions are engine action rules documented under [Mechanics](../mechanics/README.md).

## Mutation and derived state

The public helpers clear an item, add or remove a slot, compact a side, and
find the first fractured or veiled explicit. Empty slots use `PC_MOD_NONE`.
Removal is allowed to move the last live slot into a hole; affix order is not
an engine identity.

Larger derived structures do not live on each item. At action time the
context scans at most six explicit slots to derive facts such as occupied
groups, fractured or crafted slots, affix availability, and active metamods.
All group memberships for a mod come from the session catalog; the cached
primary `group_id` in the slot is not a replacement for those session tables.
Candidate masks and prefix sums stay in reusable action-context scratch.

## Export, import, and cloning

The native item value contains only engine state. The WASM facade adds JSON
export/import around it, translating session-local IDs to stable keys. The
JSON includes rarity, explicit/implicit/enchantment slots, flags, influence,
quality, sockets, links, and the current Bestiary compound checkpoint.

The facade's `pcw_item_clone` copies the `pc_item_state` value but starts the
new handle without that Bestiary checkpoint. Code that needs a compound
checkpoint copy should export and import instead.

## Current boundaries

- The fields for numeric rolls, sockets, links, quality, enchantments, and
  unveil choices are real and participate in import/export. Current crafting
  actions do not populate numeric roll values or implement socket, link, or
  quality mutation.
- Structural simulation therefore cannot evaluate conditions that depend on
  rolled stat totals.
- The engine supports one-item actions. There is no two-item or recombinator
  state model.
- Full catalog-sized masks are context/session data, not embedded in the item.
- Session-local integer IDs are never persistence identities.

## Invariants

- Live counts stay within their fixed capacities, and empty slots contain
  `PC_MOD_NONE`.
- A live slot's mod ID belongs to the interpreting session.
- Failed C ABI actions leave the caller's item unchanged.
- Derived masks and cached pools can always be rebuilt from session data and
  the item value.
- JSON crossing a session or artifact boundary uses stable keys, not dense
  IDs.

Implementation entry points: `engine/include/poecraft/item_state.h`,
`engine/src/item_state.cpp`, `engine/src/actions_basic.cpp`,
`engine/src/actions_bestiary.cpp`, and `bindings/wasm/wasm_api.cpp`.
