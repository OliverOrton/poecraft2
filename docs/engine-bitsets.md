# Engine Bitset Plan

**Status:** implemented one-item engine reference. Recombinator masks and
two-item universe notes are reserved future substrate, not active work.

## Purpose

The native engine should not carry rich mod objects through hot crafting loops. It should operate on dense session-local mod IDs, compact arrays, and bitsets.

This document maps the old `poeCraft` Python mod-pool behavior to the bitsets the new engine should use. [mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md) is authoritative for vocabulary, source-field meaning, and action-specific filtering/weight rules.

See [item-state-flow.md](item-state-flow.md) for the compact item state that action masks are derived from, and [weight-calculation-flow.md](weight-calculation-flow.md) for how candidate masks become weighted roll tables.

The old flow was:

```text
ModRegistry.get_mods_for_base(base)
  -> scan all global Mod objects
  -> create base-specific prefix/suffix lists

CraftingEngine._create_mod_pool(item, action)
  -> scan the base lists again
  -> filter by item state, effective tag signature, fossils, influence, metamods
  -> build weighted ModPool
```

The new flow should be:

```text
build session mod universe
  -> assign dense mod IDs 0..N-1
  -> precompute masks and hot arrays

build weighted action candidate pool
  -> combine masks
  -> compute or reuse weights
  -> sample from compact weighted table
```

## Sparse And Unused Bits

Some unused bits are absolutely fine.

There are three different cases:

1. Padding bits at the end of a bitset word.
2. Empty masks for mechanics that do not apply to the current base.
3. Extra session mods included for uncommon mechanics such as recombinators.

All three are acceptable. The rule is that every result must be intersected with the session's valid universe:

```text
result &= session_universe_mask
```

For bitsets stored as 64-bit words, the final word will usually contain padding bits beyond `N`. These bits should be zeroed when masks are created and ignored by iteration.

For base-specific mechanics, it is fine if a helmet session has empty cluster-jewel masks, or a bow session has empty eldritch implicit masks. Empty masks are cheap and make code paths simpler.

For recombinators, it is also fine to include a slightly larger session universe. Bitsets are small compared with object-heavy structures. If a session has 2,000 mods, one bitset is 250 bytes. If it has 10,000 mods, one bitset is 1,250 bytes. Correctness and simple uniform representation matter more than shaving a few no-op bits.

## What A Bitset Means

A bitset is conceptually a string of `1` and `0` values, with one bit per session mod ID.

Example with eight session mods:

```text
session mod IDs:  0 1 2 3 4 5 6 7
life classification-tag mask: 0 1 0 0 1 1 0 0
prefix mask:      1 1 0 1 0 1 0 0
```

The `life classification-tag mask` means mods `1`, `4`, and `5` have the `life` classification tag. Spawn-weight selector, generation-weight selector, classification, and added tags remain separate dimensions.

In C this should not be stored as a text string. Store it as fixed-width words, usually `uint64_t[]`, and operate on 64 mods at a time:

```text
candidate_words[i] = prefix_words[i] & life_tag_words[i] & normal_roll_words[i]
```

The important thing is that bit position `k` always refers to `session_mod_id == k`.

Use one dense mod-ID universe with many independent masks. Prefixes, suffixes, crafted mods, influences, each required tag channel, and special mechanics get independent bitsets over the same IDs. Do not create separate prefix and suffix ID spaces: that would complicate group blocking, item slots, global-key lookup, and actions that sample across both sides.

## Dense Session Mod IDs

Every session should build a local mod universe:

```text
session_mod_id: 0..N-1
```

`session_mod_id` maps to the global compiled data:

```text
session_global_mod_id[N]
```

All engine masks are bitsets over `session_mod_id`, not global RePoE IDs.

The normal session universe should include:

- mods that can spawn on the selected base at the selected item level
- all influence-specific mods for that base/item level that supported actions can make reachable
- crafted mods available to that item class
- base implicits
- essence-only mods reachable by essences
- fossil added/forced mods reachable by supported fossils
- corrupted implicits, if supported for the base
- eldritch implicits, if supported for the base
- veiled template mods and unveil outcome mods

When recombinators are enabled, the session universe may also include:

- mods currently present on input item A
- mods currently present on input item B
- transfer-only mods that are not normal rolls
- special recombinator-only state mods, if needed

Do not create a separate rich-object path for recombinators unless profiling or correctness proves it necessary.

Prefer a dedicated recombination session built from both input items over including recombination-only transfer mods in every normal crafting session.

## Core Per-Mod Arrays

Bitsets answer "which mods?" questions. Per-mod arrays answer "what is this mod?" questions.

Required hot arrays:

```text
global_mod_id[N]
group_id[N]
required_level[N]
generation_type[N]
domain_id[N]
flags[N]
influence_id[N]
spawn_weight_offset[N]
spawn_weight_count[N]
generation_weight_offset[N]
generation_weight_count[N]
implicit_tag_offset[N]
implicit_tag_count[N]
stat_offset[N]
stat_count[N]
```

`flags[N]` should include common booleans:

- crafted
- essence_only
- metamod
- cluster_notable
- veiled_template
- unveiled
- fractured_transfer_allowed, if recombinator rules need it
- split_transfer_allowed, if recombinator rules need it
- corrupted_implicit
- eldritch_implicit
- delve

Prefix/suffix should be masks, not only flags, because action filtering constantly intersects on affix side.

`required_level[N]` is retained for mechanics that care about the tier level after eligibility is already decided, such as Sanctified Fossil weighting, and for debugging/diagnostics. Normal action-pool filtering should not re-check item level if the session universe was built from a base pool already filtered by item level.

## Base/Session Universe Masks

These masks replace the old `Mods.prefixes` and `Mods.suffixes` lists without claiming that every session-reachable row is currently rollable.

```text
session_universe_mask
base_explicit_universe_mask
prefix_mask
suffix_mask
implicit_mask
crafted_mask
essence_only_mask
normal_random_roll_mask
bench_craftable_mask
delve_added_mask
delve_forced_mask
veiled_template_mask
unveiled_mask
corrupted_implicit_mask
eldritch_implicit_mask
```

`normal_random_roll_mask` should exclude things that are not valid ordinary random rolls:

```text
normal_random_roll_mask =
    base_explicit_universe_mask
  & ~(crafted_mask | essence_only_mask | implicit_mask)
  & ~(veiled_template_mask | unveiled_mask)
  & ~(delve_added_mask | delve_forced_mask)
  & ~(corrupted_implicit_mask | eldritch_implicit_mask)
```

Some mechanics add back special mods explicitly, such as fossil added mods or essence guaranteed mods.

`base_explicit_universe_mask` contains prefix/suffix rows associated with the selected base and item level, including influence-specific rows reachable after supported influence changes even when their spawn weight is zero for the current effective tag signature. It should not include crafted bench mods, essence-only mods, implicits, veiled outcome mods, corrupted implicits, eldritch implicits, or recombination-only transfer mods.

Current positive weight is represented by `positive_spawn_weight_mask[tag_signature_id]` or `positive_base_weight_mask[tag_signature_id]`, not by membership in `base_explicit_universe_mask`.

## Domain And Mechanic Masks

The old registry did domain filtering differently for normal items, jewels, abyss jewels, and cluster jewels.

Represent these as masks:

```text
domain_item_mask
domain_misc_mask
domain_abyss_jewel_mask
domain_affliction_jewel_mask
domain_crafted_mask
domain_delve_mask
```

The selected base decides which domains are legal for the session. Once the session universe is built, action code should rarely care about raw domains except for special mechanics.

Cluster jewel masks:

```text
cluster_jewel_mod_mask
cluster_notable_mask
cluster_socket_mod_mask
cluster_enchant_allowed_mask[tag_signature_id]
```

For non-cluster bases, these can be empty.

## Tag-Channel Masks

The old code used several distinct tag channels. Keep them separate.

Spawn-weight selector tags:

```text
spawn_tag_mask[tag_id]
```

A mod is in this mask if its ordered `spawn_weights` contain `tag_id`.

Generation-weight selector tags:

```text
generation_tag_mask[tag_id]
```

A mod is in this mask if its ordered `generation_weights` contain `tag_id`.

Classification tags (source field `implicit_tags`):

```text
implicit_tag_mask[tag_id]
```

A mod is in this mask if its `implicit_tags` contain `tag_id`.

This distinction matters:

- normal roll eligibility uses spawn weights
- fossil multipliers use implicit tags
- harvest targeting uses implicit tags
- cannot-roll attack/caster uses implicit tags

It is not necessary to allocate a physical mask for every global tag in every session. The session can keep a `tag_id -> mask_index` table and materialize only tags that exist in the session or are referenced by supported actions. But allocating a few empty masks is fine if it keeps the first implementation simpler.

## Item-Level Filtering

The old Python engine checked item level during action-pool construction:

```text
mod.required_level <= item.ilvl
```

The new engine should move that check earlier. A session is built for a selected base and item level, so the base/session mod universe should already exclude tiers above that item level.

```text
session_universe = mods_for_base_and_ilvl(base, ilvl) + special reachable mods
```

That means no `ilvl_allowed_mask` is needed in normal hot-loop action formulas. If the user changes item level, rebuild the session or rebuild the base pool. Do not keep one broad all-level session and ask every action to filter item level again.

Keep `required_level` in the per-mod arrays because some mechanics still use it after eligibility, especially Sanctified Fossil level weighting.

## Group Masks

The old engine excludes mods whose `group` already exists on the item.

Precompute:

```text
group_mask[group_id]
```

At runtime:

```text
current_group_block_mask = OR(group_mask[group] for each current explicit mod)
```

Then action filtering uses:

```text
pool &= ~current_group_block_mask
```

For UI family grouping, keep the old family concept separately:

```text
family_id = group_id + stat_signature_id
```

Family grouping is useful for display and tier grouping. It is not the same as the exclusivity group used for blocking. Strategy predicates continue to target groups unless an explicitly named family predicate is added.

RePoE tiers are separate mods. A life tier, for example, is a different mod row from another life tier. Those tiers should occupy the same exclusivity group/bucket, so once one tier is present, `current_group_block_mask` prevents every other tier in that bucket from rolling on the same item.

## Influence Masks

The old registry detected influence from spawn weights and stored internal influence names:

- `shaper`
- `elder`
- `crusader`
- `adjudicator`
- `basilisk`
- `eyrie`

Use:

```text
influence_none_mask
influence_shaper_mask
influence_elder_mask
influence_crusader_mask
influence_adjudicator_mask
influence_basilisk_mask
influence_eyrie_mask
```

For normal rolling:

```text
influence_allowed_mask =
    influence_none_mask
  | masks_for_influences_currently_on_item
```

All reachable influence masks exist in the session even when the starting item has no influence. Adding influence changes the active masks and effective tag signature; it does not rebuild the mod-ID universe. Weight arrays for newly encountered tag signatures may be created lazily in the worker-local action context. Immutable `SessionData` is never mutated by this cache fill.

For conqueror exalt style actions:

```text
influence_allowed_mask = mask_for_requested_influence
```

The effective tag signature still matters for final weight calculation because influence changes the selector tags active on the item.

## Metamod Masks

The old engine filters cannot-roll metamods using `implicit_tags`.

Precompute:

```text
attack_tag_mask = implicit_tag_mask[attack]
caster_tag_mask = implicit_tag_mask[caster]
```

At runtime:

```text
metamod_block_mask = empty
if cannot_roll_attack:
    metamod_block_mask |= attack_tag_mask
if cannot_roll_caster:
    metamod_block_mask |= caster_tag_mask
```

Then:

```text
pool &= ~metamod_block_mask
```

Prefixes-locked and suffixes-locked are not mod-pool filters for adding mods. They matter for removal and reroll actions, where they preserve one side of the item.

## Fossil Masks

The old fossil behavior has four parts:

- positive implicit-tag weight multipliers
- negative implicit-tag weight multipliers or blocks
- added mods
- forced mods

For each fossil set used in a craft, derive:

```text
fossil_block_mask
fossil_touched_mask
fossil_added_mask
fossil_forced_mask
fossil_multiplier_table[N]
sanctified_flag
```

Filtering:

```text
pool |= fossil_added_mask_for_affix
pool &= ~fossil_block_mask
```

Weighting:

```text
weight *= fossil_multiplier_table[mod]
if sanctified:
    weight *= sanctified_level_multiplier[mod]
```

Forced mods are not sampled from the normal pool. They are applied first, then their groups become blocked for later random rolls.

## Essence Masks

Essences guarantee a mod by item class.

Store:

```text
essence_guaranteed_mod_id[essence_id][item_class_id]
essence_usable_mask[item_class_id]
```

The guaranteed mod may be `essence_only`, so it must be present in the session universe even if excluded from `normal_random_roll_mask`.

Essence flow:

```text
add guaranteed mod if its group is not already blocked
current_group_block_mask |= group_mask[guaranteed.group]
roll remaining mods from normal_random_roll_mask
```

## Harvest Masks

Harvest target tags use `implicit_tags`.

For reforge or augment with tag:

```text
harvest_target_mask = implicit_tag_mask[tag]
```

Guaranteed tag pool:

```text
pool =
    (prefix_mask | suffix_mask)
  & normal_random_roll_mask
  & harvest_target_mask
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_notable_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Sample this guaranteed candidate set with `active_spawn_weight[tag_signature_id]`; generation multipliers do not apply. Prefix and suffix rows compete in one combined spawn-weighted distribution. After the guaranteed mod is added, normal random rolls proceed with its group blocked.

## Veiled Masks

The old code separates:

- generic veiled placeholder mods
- actual unveiled outcome mods

Masks:

```text
veiled_prefix_template_mask
veiled_suffix_template_mask
unveiled_prefix_mask
unveiled_suffix_mask
unveiled_named_syndicate_mask
unveiled_generic_mask
```

Unveil option pool:

```text
pool =
  unveiled_affix_mask
  & unveiled_generic_mask
  & positive_unveil_weight_mask[base_tag_signature_id]
  & ~current_group_block_mask
```

Then sample without replacement.

## Eldritch And Corrupted Implicit Masks

Eldritch implicits are only valid on helmets, body armours, gloves, and boots.

Masks:

```text
eldritch_searing_mask
eldritch_eater_mask
eldritch_tier_allowed_mask[influence_type][tier]
corrupted_implicit_mask
```

For bases where these mechanics cannot apply, the masks can exist and be empty.

## Recombination Masks

Recombinators are the reason the session universe may need to include mods that are not normal rolls on the selected target base.

Do not fall back to rich objects by default. Add recombinator masks:

```text
input_a_mod_mask
input_b_mod_mask
input_either_mod_mask
recomb_transferable_mask
recomb_target_base_compatible_mask
recomb_protected_mask
recomb_blocked_mask
recomb_special_state_mask
```

Candidate pool:

```text
pool =
    input_either_mod_mask
  & recomb_transferable_mask
  & recomb_target_base_compatible_mask
  & ~recomb_blocked_mask
```

Then apply group conflict rules:

```text
pool &= ~current_group_block_mask
```

Some recombinator behavior may care about metadata that normal rolling ignores, such as source item side, fractured/synthesized/split/corrupted state, or transfer restrictions. Store that as item-state arrays or recombinator-specific flags, not as Python-style mod objects.

The session universe should include any input mods even when they are not spawnable on the current target base. The normal random-roll masks will still exclude them, while recombinator masks can include them.

## Effective Tag Signature Masks

The old engine repeatedly computed:

```text
base tags + cluster enchant tag + influence tags
```

The new engine should intern that set as:

```text
tag_signature_id
```

For each tag signature, cache in the worker-local action context:

```text
positive_spawn_weight_mask[tag_signature_id]
positive_base_weight_mask[tag_signature_id]
active_spawn_weight[tag_signature_id][N]
active_generation_multiplier[tag_signature_id][N]
base_roll_weight[tag_signature_id][N]
```

This preserves RePoE first-match weight order without scanning each mod's ordered weight rows every time an action pool is built. `positive_spawn_weight_mask` means the raw spawn weight is above zero. `positive_base_weight_mask` means spawn and generation weights have already produced a positive normal explicit-roll weight.

## Concrete Mask Inventory From Old poeCraft

The old project effectively had four sources of mod eligibility:

1. Normal explicit mods from `ModRegistry.get_mods_for_base`.
2. Special explicit registries such as crafted, essence-only, delve, veiled, and unveiled mods.
3. Implicit registries such as base implicits, corrupted implicits, and eldritch implicits.
4. Dynamic item state such as current groups, fractured mods, crafted mods on the item, and prefix/suffix locks.

The new engine should make those sources visible as separate masks and small lookup tables. Do not hide them inside one broad "all mods" pool.

### Static Session Masks

These are built once for a base, item level, and supported mechanic set.

```text
session_universe_mask
```

Contains every dense session mod ID. This is the outer bound for all masks. It may include recombination input mods and special transfer-only mods that no normal action can roll.

```text
base_explicit_universe_mask
```

Contains the prefix/suffix explicit universe associated with the selected base and item level. It includes reachable influence rows even when they have zero current spawn weight. It is filtered for source domain, base/item-class compatibility, item level, and stable cluster/base constraints, but current effective-tag, influence-state, metamod, group, and action legality are applied by separate masks. It excludes essence-only, crafted, implicit, veiled outcome, corrupted implicit, eldritch implicit, and recombination-only mods.

```text
normal_random_roll_mask
```

Contains the mods that old `_add_random_mod` could use for ordinary rolling before action-specific filters. This drives transmute, augment, alteration, regal, alchemy, chaos, exalt, the random part of essence crafts, the random part of harvest reforges, and eldritch chaos side rerolls.

```text
prefix_mask
suffix_mask
```

Contain generation-type prefix and suffix mods. These apply across normal explicit mods, crafted mods, veiled templates, unveiled mods, and any special explicit mod that still has an affix side.

```text
domain_item_mask
domain_misc_mask
domain_abyss_jewel_mask
domain_affliction_jewel_mask
domain_crafted_mask
domain_delve_mask
```

Preserve the old registry distinction between ordinary items, regular jewels, abyss jewels, cluster jewels, crafted mods, and delve mods. These are mostly ingest/session-build masks; hot crafting actions should usually consume the already-derived mechanic masks instead.

```text
influence_none_mask
influence_shaper_mask
influence_elder_mask
influence_crusader_mask
influence_adjudicator_mask
influence_basilisk_mask
influence_eyrie_mask
```

Contain mods detected from influence spawn-weight tags. Normal influenced items use `influence_none_mask | item_influence_masks`; conqueror-style exalts use only the requested influence mask.

```text
crafted_mask
bench_craftable_mask
metamod_mask
metamod_prefixes_locked_mask
metamod_suffixes_locked_mask
metamod_no_attack_mask
metamod_no_caster_mask
metamod_multimod_mask
```

`crafted_mask` contains every crafted-domain explicit mod. `bench_craftable_mask` contains only the mods available through the bench for the selected item class. The metamod masks let item state quickly discover whether the current crafted mods enable prefix lock, suffix lock, cannot roll attack, cannot roll caster, or multimod behavior.

```text
essence_only_mask
essence_guaranteed_mask
```

`essence_only_mask` contains mods that should never appear in normal random rolls. `essence_guaranteed_mask` contains all guaranteed essence mods reachable for this session. The actual craft should use the direct essence lookup table described below, then block that mod's group before rolling the remaining random mods.

```text
delve_added_mask
delve_forced_mask
fossil_sell_price_mask
```

Contain fossil special mods. `delve_added_mask` is unioned into fossil pools for the matching fossil set. `delve_forced_mask` is applied before random fossil rolling. `fossil_sell_price_mask` covers the Gilded Fossil implicit/sell-price special case.

```text
implicit_mask
base_implicit_mask
corrupted_implicit_mask
eldritch_implicit_mask
eldritch_searing_mask
eldritch_eater_mask
eldritch_tier_mask[influence_type][tier]
```

Keep explicit affix logic separate from implicit logic. Eldritch ember/ichor actions use the eldritch masks and tier masks. Bloodstained Fossil uses corrupted implicit masks.

```text
veiled_template_mask
veiled_prefix_template_mask
veiled_suffix_template_mask
unveiled_mask
unveiled_prefix_mask
unveiled_suffix_mask
unveiled_generic_mask
unveiled_named_syndicate_mask
```

Veiled exalts and veiled chaos add placeholder/template mods. Unveil consumes the placeholder and offers actual unveiled outcome mods. The old engine excluded named syndicate outcomes from generic unveil pools, so keep `unveiled_generic_mask` separate from `unveiled_named_syndicate_mask`.

```text
spawn_tag_mask[tag_id]
generation_tag_mask[tag_id]
implicit_tag_mask[tag_id]
```

Keep the three tag dimensions separate. Spawn-weight selector tags answer "what is this mod's active spawn weight for this effective tag signature?" Generation-weight selector tags answer "what generation multiplier applies?" Classification tags answer "does this mod count as attack, caster, life, fire, resistance, etc. for fossil, Harvest, and metamod targeting?"

```text
group_mask[group_id]
family_mask[family_id]
```

Groups enforce cannot-roll-together behavior. Families are for UI tier grouping. RePoE tiers are separate mods but should usually share the same `group_id`, so the group mask blocks sibling tiers once one tier exists on the item.

```text
required_level_eq_mask[level]
```

This is not an item-level eligibility mask. Item level should already be baked into session creation. Keep this only for mechanics that compare tiers after the pool is legal, especially Harvest resistance conversion, which requires the replacement mod to have the same `required_level` as the source mod.

### Dynamic Item-State Masks

These are rebuilt or updated as the item changes.

```text
current_explicit_mask
current_prefix_mask
current_suffix_mask
current_group_block_mask
current_fractured_mask
current_crafted_mask
current_veiled_template_mask
current_eldritch_implicit_mask
```

These replace repeated scans over the current item. `current_group_block_mask` is the most important one for rolling because it blocks every mod in a group that already exists on the item.

```text
removable_mask
reroll_preserve_mask
reroll_remove_mask
prefix_lock_preserve_mask
suffix_lock_preserve_mask
```

These are not catalog masks; they are action-state masks. Annul and harvest augment use `removable_mask`. Chaos, alteration, alchemy, essence, harvest reforge, and fossil crafts use preserve/remove masks to respect fractured mods and, when applicable, prefix/suffix locks.

### Action-Derived Masks

These are built from static masks plus current item state.

```text
affix_open_mask = prefix_mask or suffix_mask
```

Selected by open prefix/suffix slots and by the action. Eldritch exalts may force one side based on dominant eldritch influence.

```text
influence_allowed_mask =
    influence_none_mask
  | masks_for_influences_currently_on_item
```

Normal rolling uses the item influence set. Conqueror exalts replace this with the requested influence mask.

```text
metamod_block_mask =
    (cannot_roll_attack ? implicit_tag_mask[attack] : empty)
  | (cannot_roll_caster ? implicit_tag_mask[caster] : empty)
```

Cannot-roll attack/caster blocks by implicit tags in the old engine. Prefix/suffix locks are reroll/removal preservation rules, not add-pool filters.

```text
harvest_target_mask = implicit_tag_mask[harvest_tag]
```

Harvest reforge and augment guarantee a mod from this target mask before continuing with normal random rolling or removing a different mod.

```text
resistance_conversion_source_mask =
    implicit_tag_mask[resistance]
  & implicit_tag_mask[from_resistance_tag]
```

```text
resistance_conversion_replacement_mask =
    implicit_tag_mask[resistance]
  & implicit_tag_mask[to_resistance_tag]
  & ~implicit_tag_mask[from_resistance_tag]
  & same_affix_side_as_source
  & required_level_eq_mask[source.required_level]
  & normal_random_roll_mask
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Harvest resistance conversion samples this derived candidate set with `active_spawn_weight[tag_signature_id]`; generation multipliers do not apply. It has stricter requirements than generic Harvest targeting, so it deserves its own derived mask.

```text
fossil_block_mask
fossil_added_mask
fossil_forced_mask
fossil_touched_mask
```

These are derived per fossil set. Fossils are one of the places where masks and weight tables must work together: the masks decide inclusion/blocking, while the multiplier table changes final weights.

### Side Tables Beside Masks

Not everything should be a mask. These old behaviors are better represented as compact tables:

```text
active_spawn_weight[tag_signature_id][mod_id]
active_generation_multiplier[tag_signature_id][mod_id]
```

These preserve RePoE first-match weight behavior and avoid re-scanning ordered weight rows.

```text
essence_guaranteed_mod_id[essence_id][item_class_id]
bench_mod_id[bench_action_id]
fossil_multiplier[fossil_set_id][mod_id]
sanctified_level_multiplier[mod_id]
unveil_option_weight[tag_signature_id][mod_id]
eldritch_implicit_weight[influence_type][tier][mod_id]
corrupted_implicit_weight[tag_signature_id][mod_id]
```

These should stay as lookups because the action needs a specific selected mod, a selected fossil set, or a weighted table, not just membership. The masks still bound the candidate set before these tables are used.

### Old Action Mapping

Basic add/reroll actions use:

```text
normal_random_roll_mask
prefix_mask / suffix_mask
positive_base_weight_mask[tag_signature_id]
influence_allowed_mask
current_group_block_mask
metamod_block_mask
```

Bench crafting uses:

```text
bench_craftable_mask
crafted_mask
current_group_block_mask
current_crafted_mask
metamod_multimod state
```

Essence crafting uses:

```text
essence_guaranteed_mod_id
essence_guaranteed_mask
normal_random_roll_mask
current_group_block_mask after the guaranteed mod is added
```

Fossil crafting uses:

```text
normal_random_roll_mask
delve_added_mask
delve_forced_mask
fossil_block_mask
fossil_multiplier table
sanctified_level_multiplier
```

Harvest uses:

```text
implicit_tag_mask[harvest_tag]
normal_random_roll_mask
resistance conversion derived masks when converting resistance mods
removable_mask for augment's remove-different-mod step
```

Veiled actions use:

```text
veiled_prefix_template_mask / veiled_suffix_template_mask
unveiled_prefix_mask / unveiled_suffix_mask
unveiled_generic_mask
current_veiled_template_mask
current_group_block_mask excluding the placeholder being replaced
```

Eldritch actions use:

```text
eldritch_searing_mask / eldritch_eater_mask
eldritch_tier_mask
normal_random_roll_mask for eldritch exalt/chaos explicit-affix changes
current_eldritch_implicit_mask
```

Recombinators should use their own transfer masks over the same session mod IDs:

```text
input_a_mod_mask
input_b_mod_mask
input_either_mod_mask
recomb_transferable_mask
recomb_target_base_compatible_mask
recomb_blocked_mask
```

This lets recombinators see mods that are irrelevant to normal rolling without making normal rolling slower or less correct.

## Common Pool Formulas

Normal prefix/suffix add:

```text
pool =
    affix_mask
  & normal_random_roll_mask
  & positive_base_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_notable_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Conqueror exalt:

```text
pool =
    affix_mask
  & normal_random_roll_mask
  & positive_base_weight_mask[tag_signature_id]
  & requested_influence_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Fossil roll:

```text
pool =
    affix_mask
  & (normal_random_roll_mask | fossil_added_mask)
  & positive_base_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & ~current_group_block_mask
  & ~fossil_block_mask
```

Harvest guaranteed tag:

```text
pool =
    (prefix_mask | suffix_mask)
  & normal_random_roll_mask
  & implicit_tag_mask[harvest_tag]
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_notable_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Harvest samples this set using `active_spawn_weight[tag_signature_id]`, not `base_roll_weight`. Generation multipliers are not part of Harvest-targeted selection.

Bench craft:

```text
candidate = bit_for_mod[bench_mod_id]
legal =
    candidate
  & bench_craftable_mask
  & ~current_group_block_mask
```

Recombinator:

```text
pool =
    input_either_mod_mask
  & recomb_transferable_mask
  & recomb_target_base_compatible_mask
  & ~current_group_block_mask
  & ~recomb_blocked_mask
```

## Allocation Guidance

Start with a simple mask set, even if a few masks are empty for many bases.

Memory impact is modest: each mask costs `ceil(N / 64) * 8` bytes. At 10,000 session mods, a prefix or suffix mask is only about 1.25 KB. Per-tag-signature weight arrays are larger than masks, so create uncommon influence/signature weight arrays lazily in each worker context rather than splitting the ID universe or mutating shared sessions.

Good fixed masks:

- prefix/suffix
- normal random roll
- crafted
- essence-only
- domain masks
- influence masks
- metamod attack/caster masks
- special mechanic masks

Good sparse/on-demand masks:

- tag masks
- group masks
- family masks
- fossil-set masks
- tag-signature masks
- recombinator input masks

This keeps the common engine code simple while avoiding huge tables for tags or groups that never appear in the session.

## Invariants

The engine should enforce these invariants:

- Every bitset has the same word length for a session.
- Prefix, suffix, and mechanic masks share one session mod-ID universe.
- Padding bits outside `0..N-1` are zeroed or ignored.
- Public iteration over a bitset must never return IDs outside `0..N-1`.
- Action pools are always intersected with `session_universe_mask`.
- `normal_random_roll_mask` never includes crafted, essence-only, implicit, veiled, unveiled, delve-only, or transfer-only mods.
- Recombination masks may include mods that normal random rolls cannot use.
- Weight arrays are valid only for a specific `tag_signature_id` and action context.
- Group blocking uses `group_id`, not UI family ID.
- UI family grouping uses `group_id + stat_signature_id`.
- Harvest-targeted candidate sets use `positive_spawn_weight_mask` and `active_spawn_weight`, never generation multipliers.

The guiding principle: a bit being present should mean "this mod satisfies this specific rule dimension," not "this mod is globally craftable."
