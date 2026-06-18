# Mod Data And Pool Semantics

## Purpose

This is the authoritative vocabulary and filtering specification for modifier data.

Other documents may describe storage layouts, bitsets, or action flows, but they should not redefine what a tag, group, pool, or weight means. If abbreviated wording elsewhere conflicts with this document, this document wins.

The old `poeCraft` implementation is evidence for how the previous simulator behaved. It is not automatically the intended game rule. This document separates:

```text
source semantics:
  what RePoE fields mean

old implementation behavior:
  what the Python project actually did

new intended behavior:
  what poecraft2 should implement
```

## Required Vocabulary

Avoid the bare words `tag`, `pool`, and `eligible` when a more precise term exists.

### Mod Row / Mod Tier

A RePoE mod row is one exact modifier tier identified by a stable global mod key.

```text
IncreasedLife3
LocalPhysicalDamagePercent7
```

Different tiers are different mod rows even when they share an exclusivity group.

### Global Mod Catalog

The global mod catalog contains every normalized mod row known to the selected data artifact:

```text
normal explicit mods
crafted mods
essence-only mods
delve/fossil mods
base/corrupted/eldritch implicits
veiled templates and unveiled outcomes
other supported special mods
```

Catalog membership does not mean a mod is legal for a base, item, or action.

### Base Tags

Base tags come from the selected base item record. They describe the base for ordered spawn/generation-weight matching.

Examples:

```text
default
bow
weapon
two_hand_weapon
helmet
int_armour
```

`default` is normally already present in RePoE base tags. Ingest should normalize and validate that fact rather than depending on callers to add it inconsistently.

### Spawn-Weight Selector Tags

Each mod has an ordered `spawn_weights` list:

```text
[{tag, weight}, ...]
```

The first row whose tag appears in the effective tag signature determines the spawn weight.

```text
first_matching_spawn_weight(mod, effective_tags):
  for row in mod.spawn_weights in source order:
    if row.tag in effective_tags:
      return row.weight
  return 0
```

A positive spawn weight means the mod passes this weight-selector stage for that tag signature. It does not by itself prove that the mod is legal for the action.

### Generation-Weight Selector Tags

Each mod may have an ordered `generation_weights` list. The first matching row supplies a percentage multiplier:

```text
first_matching_generation_pct(mod, effective_tags):
  for row in mod.generation_weights in source order:
    if row.tag in effective_tags:
      return row.weight
  return 100
```

Generation weights modify normal explicit-roll weighting where that action uses the normal formula. They are not a second spawn-permission system and they are not used by Harvest-targeted selection.

### Mod Classification Tags (`implicit_tags`)

Despite the source field name, `implicit_tags` are classification tags on the mod. They do not mean that the mod is an item implicit.

Examples:

```text
attack
caster
life
fire
resistance
unveiled_mod
```

They are used for mechanics such as:

```text
fossil weighting/blocking
Harvest targeting
cannot-roll attack/caster metamods
resistance conversion
unveil classification
```

Call these `classification tags` in prose. Use `implicit_tags` only when naming the source/schema field.

### Added Tags (`adds_tags`)

`adds_tags` describes tags contributed by a mod when that source behavior matters.

The old Python engine loaded this field but did not add it to the effective tag set during normal explicit crafting. Therefore:

- do not silently treat `adds_tags` as base tags, spawn-selector tags, or classification tags;
- do not include them in an effective tag signature by default;
- preserve them in ingest and compiled data;
- activate them only for a mechanic whose behavior has been explicitly verified and documented.

### Effective Tag Signature

The effective tag signature is the exact set consulted by ordered spawn and generation-weight rows for a particular selection context.

For normal explicit rolls:

```text
base tags
+ selected cluster enchant/passive tag, when applicable
+ generic influence selector tags currently on the item
```

Some implicit systems add action-specific selector tags, such as eldritch tier-blocking tags. Those form a separate action-specific signature and must not leak into normal explicit rolling.

Classification tags and `adds_tags` are not part of the normal effective tag signature.

### Generation Type

`generation_type` identifies how a mod occupies or participates in item state:

```text
prefix
suffix
implicit-like source types
special source types
```

For explicit rolling, prefix and suffix are affix sides. They are not separate probability systems.

### Domain

`domain` is a source/category boundary such as:

```text
item
misc
abyss_jewel
affliction_jewel
crafted
delve
```

Domain filtering happens while constructing base/session collections or special mechanic registries. A domain is not a tag.

### Exclusivity Group

The RePoE group is the cannot-coexist bucket used by crafting.

If any current explicit mod has group `G`, every candidate mod in group `G` is blocked.

```text
current_group_block_mask =
  OR(group_mask[group_id] for each current explicit mod)
```

This is the authoritative meaning of `group` in engine rules and strategy predicates such as `has_mod_group`.

### UI Family

A UI family groups rows that should be presented as tiers of the same visible modifier:

```text
family_key = group_key + stat_signature
```

Families are presentation metadata. They are not exclusivity buckets and are not substituted for group predicates.

If family-based strategy predicates are added later, they must be named explicitly as family predicates.

### Influence Classification

The old implementation inferred one influence label from spawn-weight rows when:

```text
default had weight 0
and a recognized influence selector tag had positive weight
```

This was a convenience classification used for action filtering. The new ingest may normalize the same information, but the original ordered spawn rows remain authoritative for final weight calculation.

### Session Mod Universe

The session mod universe contains every mod row that the enabled mechanics may need for the selected base and item level.

It may contain rows that have zero spawn weight for the item’s current influence state, including reachable influence mods and direct/special mechanic mods.

Session membership means `reachable by some enabled mechanic`, not `currently rollable`.

### Base Explicit Universe

`base_explicit_universe_mask` contains prefix/suffix explicit rows associated with the base/session before current action-state filtering.

It can include:

```text
ordinary explicit mods
influence-specific explicit mods reachable after influence changes
```

It excludes direct/special registries such as crafted, essence-only, implicit, veiled outcome, corrupted implicit, eldritch implicit, and transfer-only rows unless a separate mechanic mask adds them.

The name deliberately does not say `spawnable`: some rows in this universe may have zero weight for the current tag signature.

### Normal Random-Roll Universe

`normal_random_roll_mask` is the subset usable by ordinary random explicit rolls before current item/action restrictions.

It excludes:

```text
crafted
essence-only
implicit
veiled/unveiled direct outcomes
delve-only direct/forced mods
corrupted/eldritch implicits
transfer-only mods
```

### Action Candidate Set

The action candidate set is the final set of mod rows legal for one selection step after applying:

```text
action mechanic
open affix sides
current group blocking
current influence restrictions
cluster restrictions
metamod restrictions
special mechanic inclusion/exclusion
positive weight rule for that action
```

### Weighted Candidate Pool

The weighted candidate pool is the compact `(mod_id, weight)` list built from the action candidate set.

Only this final structure is sampled.

## Four Different Meanings Of “Eligible”

Do not write only `eligible`. Use one of:

```text
catalog-included:
  present in normalized global data

session-reachable:
  present in this session universe

action-legal:
  survives structural/current-state action filters

positively weighted:
  the action's weight formula produces weight > 0
```

A row must be both action-legal and positively weighted before it may be sampled.

## Exact Old Base-Collection Behavior

The old `ModRegistry.get_mods_for_base` performed these broad steps:

1. Start with loaded prefix/suffix rows from supported source domains.
2. Apply jewel/abyss/cluster domain restrictions.
3. Apply cluster passive-tag restrictions where relevant.
4. Exclude essence-only rows.
5. Evaluate first-matching spawn weight against base tags.
6. Keep positive-weight ordinary rows.
7. Also keep recognized influence rows when the old influence/base-shape heuristic considered them reachable.
8. Split rows into prefix and suffix lists.
9. Build UI families as `group + stat signature`.

Important limits:

- old base collection was not the final action pool;
- item-level filtering happened later in `_create_mod_pool`;
- influence reachability used a heuristic over tag strings and item-class/base-tag text;
- crafted and special registries were also stored separately.

The new engine should preserve observed source behavior where correct, but should replace string-substring heuristics with normalized, testable ingest data.

## Exact Old Normal Action-Pool Behavior

The old `_create_mod_pool(item, side, ...)`:

1. Chose the base prefix list or base suffix list.
2. Collected groups already present on current prefixes and suffixes.
3. Rebuilt effective selector tags from base, cluster enchant, and influence state.
4. Excluded rows in a current group.
5. Excluded rows above item level.
6. Excluded crafted and essence-only rows.
7. Applied influence filtering.
8. Applied the cluster notable cap.
9. Added fossil-added rows when requested.
10. Applied cannot-roll attack/caster using classification tags.
11. Evaluated spawn weight.
12. Evaluated generation multiplier.
13. Applied standard fossil transforms when requested. Sanctified's special
    level/lucky transform remains deferred and is rejected as unsupported.
14. Removed rows with final weight `<= 0`.
15. Sampled from the resulting weighted list.

This sequence is useful evidence, but poecraft2 moves stable base/item-level work into session construction.

## New Normal Explicit Selection

For a normal explicit selection step:

```text
candidate =
    open_affix_side_mask
  & normal_random_roll_mask
  & positive_base_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Normal weight:

```text
spawn = first_matching_spawn_weight(...)
generation_pct = first_matching_generation_pct(...)
weight = floor(spawn * generation_pct / 100)
```

### Prefix/Suffix Selection

If an action may add either side:

1. Build legal prefix candidates.
2. Build legal suffix candidates.
3. Concatenate both weighted candidate lists, or sample a side proportional to each side’s total weight.
4. Draw one mod from the combined distribution.

Do not choose prefix versus suffix 50/50.

Side selection is fixed only when the game mechanic explicitly fixes it, such as:

```text
augmenting a magic item with only one open legal side
eldritch exalt side determined by dominant eldritch influence
a direct craft whose selected mod has a fixed side
```

For multi-mod reforges, rebuild the combined candidate distribution after every selected mod because slot availability and group blocking changed.

## Harvest-Targeted Selection

Harvest-targeted selection is not the normal explicit weight formula.

For a guaranteed classification tag:

```text
candidate =
    open_prefix_or_suffix_mask
  & normal_random_roll_mask
  & classification_tag_mask[harvest_tag]
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Weight:

```text
weight = active_spawn_weight[tag_signature_id][mod_id]
```

Rules:

- target membership comes from `implicit_tags`;
- cannot-roll attack/caster metamods are respected;
- generation multipliers are not applied;
- prefix and suffix candidates compete in one spawn-weighted distribution;
- the guaranteed mod’s group blocks later selections;
- ordinary filler mods in a Harvest reforge use the normal explicit selection formula.

Harvest resistance conversion is also Harvest-targeted selection:

```text
candidate =
    same_affix_side_as_source
  & normal_random_roll_mask
  & classification_tag_mask[resistance]
  & classification_tag_mask[to_element]
  & ~classification_tag_mask[from_element]
  & required_level_eq_mask[source.required_level]
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Replacement weight is active spawn weight only.

## Fossil Selection

Fossils modify the normal explicit distribution.

```text
candidate =
    open_affix_side_mask
  & (normal_random_roll_mask | fossil_added_mask[fossil_set])
  & positive_base_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
  & ~fossil_block_mask[fossil_set]
```

Weight:

```text
normal base weight
* fossil classification-tag multiplier
* optional Sanctified level multiplier (deferred; currently unsupported)
```

Forced fossil mods are direct additions, not weighted candidates.

## Essence And Bench Selection

Essence guaranteed mods use direct lookup:

```text
essence_guaranteed_mod_id[essence_id][item_class]
```

They bypass random weighted selection, then block their group before ordinary filler rolls.

Bench crafts also use direct lookup and legality checks. They are not part of the normal random pool.

## Unveil And Implicit Systems

Unveil, corrupted implicit, and eldritch implicit selection use their own documented candidate sets and weight arrays.

Do not assume the normal explicit formula applies merely because these source rows also have `spawn_weights`.

## Old Behavior Versus Intended Corrections

| Topic | Old Python behavior | poecraft2 rule |
|---|---|---|
| Item-level filtering | Rechecked during action-pool construction | Baked into session construction; retained only for mechanics that compare levels |
| Prefix/suffix choice during many reforges | Chose an open side 50/50 | Combined weighted prefix/suffix distribution |
| Harvest guaranteed selection | Classification tag + spawn weight; did not respect cannot-roll metamods | Classification tag + spawn weight only; respects cannot-roll metamods |
| Harvest resistance replacement | Old code multiplied spawn by generation weight | Spawn weight only; respects metamods |
| Normal explicit selection | Spawn × generation | Spawn × generation |
| Influence availability | Derived label plus tag-string heuristic | Normalized reachability data plus authoritative ordered spawn rows |
| `adds_tags` | Loaded but not applied to effective tags | Preserved but inactive unless a verified mechanic explicitly uses them |
| Family predicates | UI family existed; strategy conditions checked group | Group remains the normal strategy predicate; family predicates must be explicit if added |

## Required Debug Output

Pool debugging should expose each filtering stage with precise names:

```text
global catalog row
session-reachable: yes/no and reason
normal-random member: yes/no and reason
action-legal: yes/no and first failing rule
active spawn row/tag/weight
active generation row/tag/percentage, or not used by this action
metamod block reason
group block reason
special mechanic multiplier
final weight
```

For combined-side selection, debug output must show:

```text
prefix total weight
suffix total weight
combined total weight
chosen mod and side
```

For Harvest, it must explicitly say:

```text
generation multiplier: not applicable
```

## Naming Rules For Other Docs And Code

Prefer:

```text
base tags
spawn-weight selector tags
generation-weight selector tags
classification tags
added tags
effective tag signature
exclusivity group
UI family
session mod universe
base explicit universe
normal random-roll universe
action candidate set
weighted candidate pool
```

Avoid:

```text
tags                    // without naming the channel
mod pool                // without naming the layer
eligible mods           // without naming the eligibility stage
base_spawnable_mask     // includes currently zero-weight reachable influence rows
family/group            // as if interchangeable
```

## Invariants

- Ordered spawn and generation rows retain source order.
- Spawn, generation, classification, and added tags remain separate data channels.
- Session membership does not imply current positive weight.
- A positive spawn weight does not override group, influence, metamod, slot, or mechanic legality.
- Exclusivity uses group IDs, never UI family IDs.
- Normal explicit rolls use spawn × generation.
- Harvest-targeted draws use spawn weight only and respect metamods.
- Unforced prefix/suffix selection is weight-proportional, never 50/50.
- Direct mechanics such as essence and bench crafting do not enter normal weighted selection.
- Every sampled row is both action-legal and positively weighted under that action’s formula.
