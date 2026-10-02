# Item state

**Status: implemented engine contract.** This page describes storage and
ownership, not the rules of individual crafts.

Parent: [Engine](README.md)

Verified against code: 2026-07-19 @ d5e38e3

Updated 2026-09-29 for ABI v3 transport, checkpoint-preserving clones and the
session-aware manual editor; focused native and release-WASM checks cover this
follow-up. Earlier storage and helper contracts retain their original scope.

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

Configured ordinary non-unique clusters use the additive
`pc_session_create_cluster` API with a stable base key, item level, passive key
and total added-passive count. `pc_session_cluster_configuration_json` exposes
that immutable identity and canonical passive stats/index metadata. Unconfigured
cluster creation and legacy `old_do_not_use` passive configurations stay refused.
Passive-tree socket/index metadata is distinct from equipment `socket_count`;
no allocation/pathing algorithm or configuration-generation law is inferred.

WASM exports bind the configuration in `cluster`; imports require the same
base, level, passive key and integer count, then author imported explicits through
the native editor to validate domain, caps, groups and dynamic eligibility.
Emulator/Calculator selection, drafts, Stash previews and Undo reopen that
configuration. [The cluster receipt](../active/2026-10-02-clusters-completion/README.md)
owns admitted crafting laws and remaining owner review.

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

## Manual item authoring

`pc_item_edit_json` authors a fixture without spending currency. Stable-key
`add_explicit`, `add_implicit` and `remove_implicit` edits share one atomic native
validation path with rarity, ordinary influence and corruption property changes.
Explicit additions resolve their side and crafted/veiled flags from the session,
accept an optional fracture flag, and enforce session affix caps and all exclusion
groups. Ordinary influences are limited to two and cannot coexist with fractured
affixes or Eldritch tiers. Failed edits leave the item unchanged.

Adding an influenced explicit sets its ordinary influence bit; adding a Vaal
implicit sets the corrupted flag. Eldritch additions set the corresponding side's
tier, preserving its current tier when eligible or using the lowest admitted
currency tier. Removing that implicit clears the side's tier. Removing a Vaal
implicit or ordinary influenced explicit leaves the item property in place.
Properties can be explicitly overridden while authoring a fixture.

Corruption does not block manual authoring, including Calculator input editing.
It still blocks real actions where the crafting contract requires it. The raw
`pc_item_add_mod` helper remains a lower-level fixture API and does not infer these
properties. The frontend adapts this editor; it does not duplicate crafting laws.

## Export, import, and cloning

The native item value contains only engine state. The WASM facade adds JSON
export/import around it, translating session-local IDs to stable keys. The
JSON includes rarity, explicit/implicit/enchantment slots, flags, influence,
quality, sockets, links, and the current Bestiary compound checkpoint.

ABI v3 adds `memory_strands` (integer 0–100; absent imports default to zero)
and `lifecycle` (live=0, consumed=1, destroyed=2). The WASM item-state format is
version 3. Session-aware exports persist base, level, stable mod keys, numeric
rolls and veil keys, including checkpoint slots. Import remaps keys and rejects
unknown mappings, invalid bounds/capacities and mismatched base/level. Dense-only
legacy slots cannot safely cross the corrected catalogue revision: session-aware
import refuses them and requires an export with stable keys from the original
runtime. Empty legacy states retain zero-strand/live defaults.

`pcw_item_clone` preserves the compound checkpoint and rebinds its identity to
the clone. In-game Imprint restoration restores the complete item, including
strands, but never restores a consumed donor. Old native callers with ABI v2
options are rejected; bindings and WASM must be rebuilt together.

## Current boundaries

- The fields for numeric rolls, sockets, links, quality, enchantments, and
  unveil choices are real and participate in import/export. Dominance and Vaal
  populate changed modifier values from canonical ranges. Socket, link and
  quality mutation remain unmodelled; Vaal uses the owner-approved affix projection.
- Structural simulation therefore cannot evaluate conditions that depend on
  rolled stat totals.
- `multi_item.h` exposes named resource roles and retained/changed/created/consumed
  receipts. Internal commit validates unique identities, unchanged before-state,
  compatible sessions/data and atomically swaps all outputs. A two-input/new-output
  fixture qualifies this foundation; no recombination law is implemented.
- Strand-bearing crafting and exact solving refuse unresolved interaction laws.
  Retained enchantments are preserved and displayed separately from implicits.
  Vaal, Dominance and Awakener preserve them without claiming stat-total evaluation;
  other crafting on them and exact effect goals remain unavailable.
- Consumed/destroyed resources cannot be crafted, projected as empty live items,
  or resurrected by in-game Imprint restoration.
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
