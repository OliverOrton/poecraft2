# Weight Calculation Flow

## Purpose

The engine should treat mod selection as two separate phases:

1. Build an eligibility mask.
2. Assign weights to the set bits in that mask and sample from them.

Bitsets answer "which mods are legal?" Weight arrays answer "how likely is each legal mod?"

The old `poeCraft` implementation recalculated both phases by scanning Python `Mod` objects. The new engine should preserve verified source semantics while applying the intentional corrections in [mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md), which is authoritative for tag channels, candidate-set meaning, and action-specific weight formulas.

## Old Normal Explicit Behavior To Preserve

The old explicit-affix formula was:

```text
spawn_weight = first matching spawn_weights row, or 0
generation_multiplier = first matching generation_weights row / 100, or 1.0

weight = int(spawn_weight * generation_multiplier)
```

If `weight <= 0`, the mod is not positively weighted for that normal explicit candidate set.

For fossil crafts:

```text
fossil_multiplier = product of all matching fossil positive/negative tag multipliers
weight = int(weight * fossil_multiplier)
```

If any matching fossil negative weight is `0`, the fossil multiplier is `0` and the mod is blocked.

For Sanctified Fossil level weighting, the old code applied:

```text
level_adjust = 1.0 + (mod.required_level - 40) * 0.01
weight = max(1, int(weight * level_adjust))
```

This happens after spawn, generation, and fossil multipliers have already produced a positive weight.

Item level should not be part of normal hot weight calculation in the new engine. The session mod universe should already exclude mods above the selected item level. Keep `required_level` for Sanctified weighting, Harvest resistance conversion, debug output, and diagnostics.

## Effective Tag Signatures

Spawn and generation weights depend on the selection context's effective tag signature.

The old engine built this from:

```text
base item tags
+ "default"
+ cluster enchant/effect tag, when present
+ influence tags, when present
```

Implicit systems that need action-specific selector tags use a separate action-specific signature. Classification tags and `adds_tags` are not inserted into the normal explicit effective tag signature.

The new engine should intern each effective tag set as a `tag_signature_id`.

Example:

```text
tag_signature_id 17 =
    default
    body_armour
    str_armour
    body_armour_shaper
```

Once a tag signature exists, cache the normal explicit weights for it in the worker-local action context. `SessionData` remains immutable and shareable.

## Cached Base Weight Arrays

For each `tag_signature_id`, build these arrays over dense session mod IDs:

```text
active_spawn_weight[tag_signature_id][mod_id]      // uint32, 0 means cannot spawn
active_generation_pct[tag_signature_id][mod_id]    // uint16 or uint32, 100 means 1.0x
base_roll_weight[tag_signature_id][mod_id]         // uint32
positive_spawn_weight_mask[tag_signature_id]       // bitset
positive_base_weight_mask[tag_signature_id]        // bitset
```

Build them like this:

```text
spawn = first_matching_spawn_weight(mod.spawn_weights, tag_signature)
if spawn <= 0:
    base_roll_weight = 0
    clear positive_spawn_weight bit
    clear positive_base_weight bit
    continue

set positive_spawn_weight bit

gen_pct = first_matching_generation_weight(mod.generation_weights, tag_signature)
if no generation row matches:
    gen_pct = 100

weight = (spawn * gen_pct) / 100
if weight <= 0:
    base_roll_weight = 0
    clear positive_base_weight bit
    continue

base_roll_weight = weight
set positive_base_weight bit
```

Use integer math. RePoE generation and fossil multipliers are naturally percent-like values where `100` means unchanged. This avoids floating-point differences across platforms.

For deterministic engine semantics, truncate after the generation multiplier before applying fossil or Sanctified modifiers.

## Normal Explicit Roll Flow

Normal actions include transmute, augment, alteration, regal, alchemy, chaos, exalt, and the random remainder of essence or harvest reforges.

First build the candidate mask:

```text
pool =
    affix_mask
  & normal_random_roll_mask
  & positive_base_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Then build a compact weighted pool:

```text
total_weight = 0
candidate_count = 0

for mod_id in set_bits(pool):
    weight = base_roll_weight[tag_signature_id][mod_id]
    if weight <= 0:
        continue

    candidate_mod_ids[candidate_count] = mod_id
    candidate_weights[candidate_count] = weight
    total_weight += weight
    candidate_count += 1
```

If an action can add either a prefix or a suffix, do not choose the affix side 50/50 unless the game rule says to. Build the legal prefix and suffix candidates and sample across their combined total weights.

After a mod is chosen, update:

```text
current_explicit_mask
current_prefix_mask or current_suffix_mask
current_group_block_mask
open prefix/suffix counts
```

If the action needs to add another random mod, rebuild or incrementally update the candidate mask before the next sample. Group blocking changes after each selected mod.

## Fossil Weight Flow

Fossils are action-specific. They should not change `base_roll_weight`.

For each supported fossil set, build:

```text
fossil_block_mask[fossil_set_id]
fossil_added_mask[fossil_set_id]
fossil_forced_mod_ids[fossil_set_id]
fossil_multiplier_fixed[fossil_set_id][mod_id]
```

`fossil_multiplier_fixed` should represent the product of all matching fossil positive and non-zero negative weights. Use a fixed-point scale, for example:

```text
FIXED_ONE = 1_000_000
```

Then:

```text
pool =
    affix_mask
  & (normal_random_roll_mask | fossil_added_mask[fossil_set_id])
  & positive_base_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & ~current_group_block_mask
  & ~fossil_block_mask[fossil_set_id]
```

Weight each candidate:

```text
weight = base_roll_weight[tag_signature_id][mod_id]
weight = (weight * fossil_multiplier_fixed[fossil_set_id][mod_id]) / FIXED_ONE
if weight <= 0:
    skip

if sanctified:
    sanctified_pct = 100 + (required_level[mod_id] - 40)
    weight = max(1, (weight * sanctified_pct) / 100)
```

Forced fossil mods are not sampled from the weighted pool. Add them first if their side and group are legal, then update `current_group_block_mask` before normal fossil rolling.

Gilded/Bloodstained-style implicit or sell-price behavior should remain outside explicit-affix weighting.

## Harvest Weight Flow

Harvest-targeted selection uses classification-tag membership and active spawn weight only. It does not use the normal spawn × generation formula.

For a guaranteed tag:

```text
pool =
    (prefix_mask | suffix_mask)
  & normal_random_roll_mask
  & implicit_tag_mask[harvest_tag]
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & cluster_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Sample from `active_spawn_weight[tag_signature_id]`. If both affix sides are open, prefix and suffix candidates compete in one combined spawn-weighted distribution.

After the guaranteed mod is added, update group blocking and continue with the normal explicit spawn × generation formula for any filler mods.

Harvest resistance conversion is a special weighted replacement:

```text
pool =
    same_affix_side_as_source
  & normal_random_roll_mask
  & implicit_tag_mask[resistance]
  & implicit_tag_mask[to_resistance_tag]
  & ~implicit_tag_mask[from_resistance_tag]
  & required_level_eq_mask[source.required_level]
  & positive_spawn_weight_mask[tag_signature_id]
  & influence_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Sample with `active_spawn_weight[tag_signature_id]`; generation multipliers do not apply.

## Essence Weight Flow

Essence guaranteed mods bypass weighted sampling.

```text
guaranteed_mod_id = essence_guaranteed_mod_id[essence_id][item_class_id]
```

If the guaranteed mod is legal and its group is not blocked:

```text
add guaranteed mod
current_group_block_mask |= group_mask[group_id[guaranteed_mod_id]]
```

The remaining random mods use the normal explicit roll flow. Essence-only mods stay out of `normal_random_roll_mask`; they enter through the direct guaranteed lookup only.

## Bench Craft Flow

Bench crafting is not weighted.

```text
mod_id = bench_mod_id[bench_action_id]
legal =
    bit_for_mod[mod_id]
  & bench_craftable_mask
  & ~current_group_block_mask
```

Metamod and multimod limits are item-state rules, not weighted sampling rules.

## Veiled And Implicit Weight Flow

Some old registries use spawn weights directly but do not apply generation multipliers.

Use separate weight arrays or small weighted lists for:

```text
unveil_weight[tag_signature_id][mod_id]
eldritch_implicit_weight[influence_type][tier][mod_id]
corrupted_implicit_weight[tag_signature_id][mod_id]
```

Unveiled outcomes:

```text
pool =
    unveiled_affix_mask
  & unveiled_generic_mask
  & positive_unveil_weight_mask[tag_signature_id]
  & ~current_group_block_mask_excluding_placeholder
```

Sample options from `unveil_weight`, usually without replacement.

Eldritch implicits:

```text
pool =
    eldritch_influence_mask
  & eldritch_tier_mask[influence_type][tier]
  & positive_eldritch_weight_mask[influence_type][tier]
```

Sample from the eldritch implicit weight table. Do not mix this with explicit prefix/suffix weighting.

## Sampling Strategy

Start with compact weighted arrays and prefix sums.

```text
candidate_mod_ids[]
prefix_weight_sums[]
total_weight
```

Sampling:

```text
roll = random_int(0, total_weight - 1)
idx = lower_bound(prefix_weight_sums, roll + 1)
chosen_mod_id = candidate_mod_ids[idx]
```

This is simple, deterministic, and fast enough for a first engine. It also makes cache validation easy.

Alias tables can be added later for heavily reused pools:

```text
if weighted_pool_cache_hits > threshold:
    build alias table for O(1) sampling
```

Do not start with alias tables everywhere. Many pools are short-lived because current groups, open affix slots, fossils, influence, metamods, and harvest tags change the candidate mask.

## Weighted Pool Cache

Cache the compact weighted pool after masks and weights have both been applied.

Suggested key:

```text
candidate_mask_hash
tag_signature_id
weight_context_id
```

Where `weight_context_id` captures:

```text
normal
fossil_set_id
sanctified enabled
harvest_spawn_only
unveil context
eldritch implicit context
```

The candidate mask hash already reflects current groups, affix side, metamod blocking, influence, harvest target tags, and open-slot state if those were part of the mask.

Cache value:

```text
candidate_mod_ids[]
candidate_weights[]
prefix_weight_sums[]
total_weight
optional alias table
```

For ML simulation, it is worth prewarming common contexts:

```text
normal prefix
normal suffix
chaos reroll common states
common fossil sets
common harvest tags
```

But correctness should not depend on prewarming. A cache miss should just build the weighted pool from masks and arrays.

## Recommended Implementation Order

1. Implement `tag_signature_id` interning.
2. Implement `base_roll_weight[tag_signature_id][mod_id]`.
3. Implement normal explicit weighted pool building from a candidate mask.
4. Add weighted pool caching with prefix sums.
5. Add fossil multiplier tables and block masks.
6. Add Harvest target/resistance derived masks using active spawn weights only.
7. Add separate unveil and eldritch implicit weight tables.
8. Add optional alias tables only after profiling.

## Invariants

- Eligibility masks decide membership before weights are read.
- A mod with zero final weight must not be sampled.
- Normal explicit actions use `base_roll_weight`.
- Harvest-targeted selection uses `active_spawn_weight` and respects metamod blocking; generation multipliers do not apply.
- Essence guaranteed mods and bench crafts bypass weighted sampling.
- Fossils modify action weights, not cached base weights.
- Prefix/suffix locks preserve or remove existing mods; they do not change add-pool weights.
- If one action adds multiple random mods, update group blocking after each selected mod.
- Use integer or fixed-point math consistently so simulation is deterministic across machines.
