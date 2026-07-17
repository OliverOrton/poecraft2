# Item State Flow

**Status: implemented item-state reference.** Future two-item extensions are
explicitly labelled and are not active sequencing.

Modifier vocabulary and weighted-selection rules in this document defer to [mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md).

## Purpose

The old `poeCraft` implementation stored item state as Python objects:

```text
Item
  prefixes: List[ModInstance]
  suffixes: List[ModInstance]
  implicits: List[ModInstance]
  enchantments: List[ModInstance]
  rarity
  corrupted / mirrored / split / synthesised
  quality
  sockets / links
  influences
  searing_exarch_tier / eater_of_worlds_tier
```

That structure was flexible and easy to read, but the new engine should store the same mutable state in compact fixed slots. The item has at most a few live mods, so the item itself should stay small and cheap to copy. Large bitsets and weighted candidate arrays belong in reusable scratch space, not inside every item.

The target split is:

```text
SessionData:
  immutable base/mod data, static masks, weights, lookup tables

ItemState:
  compact mutable truth of one item

CraftScratch:
  temporary masks and candidate arrays derived from SessionData + ItemState
```

## What The Old Item Stored

The old `Item` had immutable session/base fields:

```text
ilvl
base_type
base_tags
item_class
base_properties
enchant_effect
```

Its snapshot code deliberately did not serialize most of those fields because they do not change after item/session creation.

The old mutable fields were:

```text
rarity
prefixes
suffixes
implicits
enchantments
corrupted
mirrored
split
synthesised
quality
sockets
links
influences
searing_exarch_tier
eater_of_worlds_tier
```

Each old `ModInstance` stored:

```text
mod reference
rolled_values
is_fractured
is_eldritch
is_veiled
veiled_options
veiled_chosen
custom_stat_text
```

The new state should preserve these concepts but replace object references and lists with dense IDs, small flags, counts, and fixed arrays.

## Recommended Compact State

Use session-local mod IDs in hot state. A `session_mod_id` maps back to global compiled mod data through `SessionData`.

Session-local IDs are never persistent identities. The canonical `mod.key` from the global compiled data is the stable identity used by saved items, strategy start states, imports/exports, traces that outlive a session, and published representative items. Loading a persistent resource resolves each global mod key into the current session's dense ID.

```c
#define MAX_PREFIXES 3
#define MAX_SUFFIXES 3
#define MAX_EXPLICITS 6
#define MAX_IMPLICITS 8
#define MAX_ENCHANTS 4
#define MAX_ROLL_VALUES 8
#define MAX_VEILED_OPTIONS 3
#define MAX_SOCKETS 6
#define MOD_NONE UINT32_MAX
```

These fixed limits are provisional implementation capacities, not assumed game rules. Validate them against the current RePoE dataset before locking the ABI. Load/import must return an explicit unsupported-capacity error rather than truncating extra rolls, implicits, enchantments, sockets, or options.

```c
typedef enum {
    MOD_SLOT_FRACTURED = 1 << 0,
    MOD_SLOT_CRAFTED   = 1 << 1,
    MOD_SLOT_VEILED    = 1 << 2,
    MOD_SLOT_ELDRITCH  = 1 << 3,
    MOD_SLOT_SYNTH     = 1 << 4,
} ModSlotFlags;

typedef struct {
    uint32_t mod_id;       // dense session mod id, or MOD_NONE
    uint16_t group_id;     // cached from SessionData for fast group blocking
    uint8_t flags;

    uint8_t roll_count;
    int32_t rolls[MAX_ROLL_VALUES];

    uint8_t veiled_option_count;
    uint32_t veiled_option_mod_ids[MAX_VEILED_OPTIONS];
    uint32_t veiled_chosen_mod_id; // MOD_NONE until chosen
} ModSlot;
```

```c
typedef enum {
    ITEM_CORRUPTED   = 1 << 0,
    ITEM_MIRRORED    = 1 << 1,
    ITEM_SPLIT       = 1 << 2,
    ITEM_SYNTHESISED = 1 << 3,
} ItemFlags;

typedef struct {
    uint8_t rarity;       // 0 normal, 1 magic, 2 rare
    uint8_t quality;
    uint8_t item_flags;

    uint8_t prefix_count;
    uint8_t suffix_count;
    uint8_t implicit_count;
    uint8_t enchantment_count;

    ModSlot prefixes[MAX_PREFIXES];
    ModSlot suffixes[MAX_SUFFIXES];
    ModSlot implicits[MAX_IMPLICITS];
    ModSlot enchantments[MAX_ENCHANTS];

    uint8_t generic_influence_bits;
    uint8_t searing_exarch_tier;
    uint8_t eater_of_worlds_tier;

    uint8_t socket_count;
    uint8_t socket_colors[MAX_SOCKETS]; // R/G/B/W encoded as small ints
    uint8_t link_mask;                  // bit i means socket i linked to i+1
} ItemState;
```

`ilvl`, internal `base_id`, `item_class_id`, `base_tags`, `base_properties`, and `enchant_effect` can live in `SessionData` or in a small immutable item header. They do not need to be copied during simulation branches unless the engine supports changing base/item level mid-session.

For standalone saved items, store both:

```text
ItemIdentity:
  base_key (stable RePoE metadata path)
  ilvl
  schema/data version

ItemState:
  mutable state serialized with global mod keys
```

For hot simulation, pass `SessionData` separately and copy only `ItemState`.

Runtime loaders resolve persistent `base_key` to the compiled global/internal `base_id`. Integer base IDs are artifact-local implementation details and must not appear in IndexedDB, JSON exports, account resources, or publications.

## Why Slots Instead Of Item Bitsets

The old engine repeatedly scanned the current item:

```text
item.prefixes + item.suffixes
```

That was fine because there are only up to six explicit mods. The expensive part was scanning all possible mods.

The new engine should keep that same idea:

```text
scan 0..6 current explicit slots -> cheap
scan thousands of possible mods -> avoid with masks
```

Do not store full `current_group_block_mask`, `current_explicit_mask`, or `removable_mask` inside `ItemState` by default. Put those in `CraftScratch` and rebuild them from the few live slots when an action starts.

This keeps branching cheap:

```text
copy ItemState only
reuse CraftScratch per worker/thread
```

If profiling later shows repeated item-state scans matter, add cached derived bits to `ItemState`, but start with the simpler value-object state.

## Craft Scratch

`CraftScratch` should contain temporary data that can be rebuilt at any time:

```text
current_explicit_mask
current_prefix_mask
current_suffix_mask
current_group_block_mask
current_fractured_mask
current_crafted_mask
current_veiled_mask
removable_mask
preserve_mask
remove_mask
metamod_state_bits
influence_allowed_mask
candidate_mask
candidate_mod_ids[]
candidate_weights[]
prefix_weight_sums[]
```

This scratch data depends on the current action. For example, a chaos orb with prefixes locked has a different preserve/remove mask than an exalt.

## Derived State From Slots

The old helpers can be mapped directly.

```text
max_prefix:
  magic -> 1
  rare jewel/abyss jewel -> 2
  rare other -> 3
  normal -> 0
```

```text
max_suffix:
  same as max_prefix
```

```text
can_add_prefix = prefix_count < max_prefix
can_add_suffix = suffix_count < max_suffix
```

```text
has_generic_influence = generic_influence_bits != 0
has_eldritch_influence = searing_exarch_tier > 0 || eater_of_worlds_tier > 0
is_influenced = has_generic_influence || has_eldritch_influence
```

```text
dominant_eldritch:
  searing tier > eater tier -> searing_exarch
  eater tier > searing tier -> eater_of_worlds
  otherwise -> none
```

Metamods can be derived by scanning explicit slots:

```text
prefixes_locked = any slot has metamod_type prefixes_locked
suffixes_locked = any slot has metamod_type suffixes_locked
cannot_roll_attack = any slot has metamod_type no_attack
cannot_roll_caster = any slot has metamod_type no_caster
has_multimod = any slot has metamod_type multimod
crafted_count = count slots with crafted flag
```

Since this is at most six slots, scanning is cheap. `SessionData` should expose:

```text
mod_flags[mod_id]
metamod_type[mod_id]
group_id[mod_id]
generation_type[mod_id]
```

## Slot Operations

Use small helper functions. These replace Python list append/remove/clear.

```text
add_prefix(item, mod_id, roll_mode)
add_suffix(item, mod_id, roll_mode)
remove_prefix_at(item, index)
remove_suffix_at(item, index)
clear_prefixes(item)
clear_suffixes(item)
compact_side(item, side)
find_fractured_slot(item)
find_veiled_slot(item)
```

When adding a mod:

```text
slot.mod_id = mod_id
slot.group_id = SessionData.group_id[mod_id]
slot.flags = flags derived from mod + action
slot.rolls = roll stat values when full-roll detail is enabled
```

Structural simulation may skip numeric roll values for throughput. In that mode, conditions requiring rolled stat totals are unavailable. A later full-roll toggle retains numeric values for stat-total conditions and representative result items.

When removing a mod, move the last slot on that side into the removed slot position and decrement the count. Affix order is not important for simulation correctness. If UI wants stable display order, the UI layer can sort or preserve order separately.

## Reforge And Reroll Flow

The old engine's reforge pattern was:

```text
save locked side, if any
find fractured mod, if any
clear rerolled sides
restore locked side and/or fractured mod
roll new mods
```

The new engine should express that as a preserve pass:

```text
preserve_prefixes = prefixes_locked
preserve_suffixes = suffixes_locked

preserve slots where:
  slot is fractured
  or side is locked

remove every other explicit slot
compact prefix/suffix arrays
rebuild group block mask from preserved slots
roll new mods
```

That is the key rule for chaos, harvest reforge, essence, fossil, alteration, alchemy, and eldritch chaos:

```text
old removed groups do not block new rolls
preserved groups do block new rolls
forced or guaranteed mods added during the action then block later rolls
```

### Chaos / Harvest Reforge

```text
check craftable
check rarity rare
preserve fractured and locked-side slots
remove all other explicit slots
roll target rare mod count
if harvest reforge:
  add guaranteed tag mod first
  update group block mask
roll remaining normal mods
```

### Essence

```text
check craftable
preserve fractured slot
remove all other explicit slots
set rarity rare
add guaranteed essence mod if its group is not blocked
update group block mask
roll remaining normal mods
```

Essence guaranteed mods may be `essence_only`, so they enter through direct lookup, not the normal random-roll pool.

### Fossil

```text
check craftable
preserve fractured slot
remove all other explicit slots
set rarity rare
add forced fossil mods first if legal
update group block mask
roll remaining mods using fossil weights
apply Bloodstained/Gilded implicit effects after explicit rolling
```

### Alteration

```text
check craftable
check rarity magic
preserve fractured slot
remove all other explicit slots
roll 1 or 2 magic mods
```

### Scour

```text
if prefixes locked:
  remove suffixes
  rarity = rare if prefixes remain else normal
elif suffixes locked:
  remove prefixes
  rarity = rare if suffixes remain else normal
else:
  remove all explicit mods except fractured
  rarity = magic if fractured remains else normal
```

## Add-One Flow

The old add-one actions include augment, regal, exalt, veiled exalt, eldritch exalt, and conqueror exalts.

These actions do not clear existing slots. Existing groups block new rolls.

```text
rebuild group block mask from all current explicit slots
derive the legal open affix sides
build one combined weighted candidate pool when either side is allowed
sample weighted mod
append slot
```

Conqueror exalt also mutates influence state:

```text
add generic influence bit
roll only requested influence mod
if no influenced mod can roll:
  remove the influence bit again
  fail
```

Eldritch exalt chooses the side from dominant eldritch influence:

```text
dominant searing -> add prefix
dominant eater -> add suffix
no dominant -> normal exalt-style side choice
```

## Remove Flow

The old removable helper respected prefix/suffix locks and fractured mods:

```text
if prefixes_locked and suffixes_locked:
  removable = empty
elif prefixes_locked:
  removable = suffixes
elif suffixes_locked:
  removable = prefixes
else:
  removable = prefixes + suffixes

if respect_fractured:
  remove fractured slots from removable
```

In the new engine, build `removable_slots[]` in scratch as side/index pairs:

```text
typedef struct {
    uint8_t side;  // prefix or suffix
    uint8_t index;
} SlotRef;
```

Annul samples one slot from this small array and removes it.

Harvest augment does:

```text
add guaranteed tag mod
build removable_slots excluding the newly added slot
remove random removable slot
```

Remove crafted mods does:

```text
remove every explicit slot with crafted flag unless fractured
compact sides
```

## Veiled State

The old engine only allowed one veiled mod at a time.

State needed per slot:

```text
is_veiled
veiled_option_count
veiled_option_mod_ids[3]
veiled_chosen_mod_id
```

Veiled exalt/chaos adds a veiled placeholder slot:

```text
mod_id = VeiledPrefix or VeiledSuffix
slot.flags |= MOD_SLOT_VEILED
```

Unveil options are generated from the unveiled mod pool and stored on the veiled slot. Unveil replaces the placeholder:

```text
find veiled slot
validate chosen option
validate group does not conflict with existing slots excluding placeholder
replace slot.mod_id with chosen unveiled mod id
clear MOD_SLOT_VEILED
clear options or keep chosen for UI history
reroll values for chosen mod
```

This replacement can happen in place because the side does not change.

## Implicits, Eldritch, And Corrupted State

The old item kept implicits separate from prefixes/suffixes. Keep that split.

`implicits[]` can contain:

```text
base implicits
corrupted implicits
eldritch implicits
Gilded Fossil sell-price implicit
```

Use slot flags and mod generation type to distinguish them.

Eldritch ember/ichor flow:

```text
check item class can use eldritch implicits
check no generic influence
set searing/eater tier
remove existing eldritch implicit for that influence
roll new eldritch implicit
append implicit slot with MOD_SLOT_ELDRITCH
```

Bloodstained Fossil flow:

```text
append corrupted implicit
set ITEM_CORRUPTED
```

Fracturing Orb flow:

```text
require rare item with at least 4 explicit modifiers
reject generic influence, synthesised state, or any existing fractured slot
choose uniformly from every explicit slot without consulting metamods
set MOD_SLOT_FRACTURED on the selected slot
```

Eldritch implicits and split state do not block Fracturing Orb.

Blessed Orb rerolls implicit slot values only. Divine rerolls explicit slot values only and skips fractured slots.

## Socket, Link, And Quality State

The old implementation included quality, sockets, and links in the same item object.

For the engine:

```text
quality: uint8_t
socket_count: uint8_t
socket_colors[6]
link_mask: uint8_t
```

`link_mask` is compact:

```text
bit 0 = socket 0 linked to socket 1
bit 1 = socket 1 linked to socket 2
...
```

Largest link group can be derived by scanning `link_mask`.

Quality is mutated directly:

```text
normal item -> +5
magic item -> +2
rare item -> +1
cap at 20 in old implementation
```

Socket and quality systems are mostly separate from explicit mod masks and weights.

## Snapshot And Simulation Copying

The old snapshot serialized mutable state only and rebuilt `ModInstance` objects from mod IDs.

The new simulator can do better:

```text
ItemState child = parent;
apply craft to child;
```

This value copy is cheap if `ItemState` stays small and does not embed bitsets.

For UI undo/redo or saved items, serialize:

```text
base/item identity
rarity
slot arrays with stable global mod keys, rolls, and flags
implicits/enchantments
item flags
quality
sockets/link_mask
influence bits/tiers
```

Do not serialize derived scratch masks.

Undo/redo snapshots that live only inside one open document may use session-local IDs internally, but any snapshot written to IndexedDB, JSON, an account, or a publication must use global keys.

## Relationship To Masks And Weights

Item state drives masks, but it should not store them permanently.

At action start:

```text
scratch.current_group_block_mask = empty
scratch.metamod_state_bits = empty
scratch.crafted_count = 0

for each explicit slot:
  scratch.current_group_block_mask |= group_mask[slot.group_id]
  update metamod bits from SessionData.metamod_type[slot.mod_id]
  update crafted count from slot flags or SessionData.mod_flags
```

For reforge actions, rebuild this scratch state after removals/preservation:

```text
preserve slots
clear rerolled slots
compact
rebuild scratch from remaining slots
roll new mods
```

That is what makes the old "current buckets do not matter after a reforge" behavior work correctly.

## Implemented Historical Order

1. Implement `ItemState`, `ModSlot`, and side helper functions.
2. Implement conversion from old-style/JSON item snapshots to `ItemState`.
3. Implement `build_item_scratch(SessionData, ItemState)`.
4. Implement add-one actions: augment, regal, exalt.
5. Implement remove actions: annul, remove crafted, scour.
6. Implement reforge actions: alteration, chaos, essence, fossil, harvest reforge.
7. Implement veiled placeholder/options/replacement.
8. Implement eldritch implicit and eldritch explicit actions.
9. Add UI serialization and optional display ordering if needed.

## Invariants

- `prefix_count <= max_prefix`.
- `suffix_count <= max_suffix`.
- Empty slots have `mod_id == MOD_NONE`.
- A live explicit slot's `group_id` matches `SessionData.group_id[mod_id]`.
- Fractured slots are never removed by ordinary removal/reroll operations.
- Reforge actions clear removed slots before rebuilding group-block state.
- Existing groups block add-one actions.
- Only preserved groups block reroll actions.
- Veiled option state belongs to the veiled placeholder slot.
- Full bitsets and weighted candidate arrays live in scratch, not in `ItemState`.
