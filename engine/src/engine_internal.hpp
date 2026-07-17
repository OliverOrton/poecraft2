#ifndef POECRAFT_SRC_ENGINE_INTERNAL_HPP
#define POECRAFT_SRC_ENGINE_INTERNAL_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "poecraft/item_state.h"
#include "poecraft/rng.h"
#include "poecraft/simulator.h"

/*
 * Internal engine representation. Public opaque handles (pc_data, pc_session,
 * pc_action_context) are reinterpret-cast to/from these. DataImpl is immutable
 * after load and shared by shared_ptr so a session/context keeps it alive even
 * if the caller destroys the data handle first.
 */
namespace poecraft {

enum class BestiaryOperationKind : std::uint8_t {
    Create = 0,
    Restore = 1,
};

enum class BestiaryCheckpointRequirement : std::uint8_t {
    Absent = 0,
    Present = 1,
};

enum class BestiaryCheckpointEffect : std::uint8_t {
    Create = 0,
    Consume = 1,
};

enum class BestiaryIdentityRequirement : std::uint8_t {
    CurrentItem = 0,
    SameItem = 1,
};

struct BestiaryBeastInputDescriptor {
    std::string beast_key;
    std::string display_name;
    std::uint32_t quantity = 0;
    std::string price_key;
};

struct BestiaryRecipeDescriptor {
    std::uint32_t global_recipe_id = 0;
    std::string id;
    std::string display_name;
    std::uint8_t classification = 0;
    std::uint8_t support = 0;
    std::string unsupported_reason;
    bool emulator_available = false;
    bool calculator_available = false;
    bool strategy_builder_available = false;
    bool solver_available = false;
    std::vector<BestiaryBeastInputDescriptor> beast_inputs;
};

struct BestiaryActionDescriptor {
    std::uint32_t global_action_id = 0;
    std::uint32_t global_recipe_id = 0;
    std::uint32_t operation_ordinal = 0;
    std::string id;
    std::string display_name;
    BestiaryOperationKind operation = BestiaryOperationKind::Create;
    std::uint8_t transition = 0;
    std::uint8_t rarity_mask = 0;
    std::uint8_t forbidden_item_flags = 0;
    BestiaryCheckpointRequirement checkpoint_requirement =
        BestiaryCheckpointRequirement::Absent;
    BestiaryCheckpointEffect checkpoint_effect =
        BestiaryCheckpointEffect::Create;
    BestiaryIdentityRequirement identity_requirement =
        BestiaryIdentityRequirement::CurrentItem;
    std::vector<std::string> cost_keys;
};

enum class BestiaryRefusalReason : std::uint8_t {
    None = 0,
    UnknownAction = 1,
    RequiresMagicItem = 2,
    ItemCorrupted = 3,
    ItemMirrored = 4,
    CheckpointAlreadyExists = 5,
    CheckpointMissing = 6,
    CheckpointBoundToDifferentItem = 7,
};

/* One live item plus an optional saved copy. The checkpoint is not a second
 * live item and deliberately does not widen the public pc_item_state ABI. */
struct BestiaryCraftState {
    pc_item_state item{};
    std::uint64_t live_item_identity = 0;
    bool checkpoint_active = false;
    std::uint64_t checkpoint_bound_identity = 0;
    pc_item_state checkpoint{};
};

struct BestiaryActionOutcome {
    bool applied = false;
    BestiaryRefusalReason refusal = BestiaryRefusalReason::None;
    std::vector<std::string> consumed_price_keys;
    std::uint8_t output_item_count = 1;
    std::uint8_t output_checkpoint_count = 0;
    std::uint8_t consumed_checkpoint_count = 0;
};

struct BestiaryCalculation {
    BestiaryCraftState successor{};
    BestiaryActionOutcome outcome{};
};

struct DataImpl {
    std::vector<std::string> strings;

    std::uint32_t artifact_schema_version = 0;
    bool complete_dataset = false;

    // manifest row counts
    std::uint32_t count_tags = 0;
    std::uint32_t count_item_classes = 0;
    std::uint32_t count_base_items = 0;
    std::uint32_t count_mods = 0;
    std::uint32_t count_groups = 0;
    std::uint32_t count_ordinary_bases = 0;
    std::uint32_t count_cluster_bases = 0;
    std::uint32_t count_unsupported_domain_bases = 0;
    std::uint32_t count_bestiary_recipes = 0;
    std::uint32_t count_bestiary_actions = 0;

    // Manifest-backed B1 Bestiary contract. Unsupported recipes are retained
    // in recipes and deliberately have no action descriptors.
    std::vector<BestiaryRecipeDescriptor> bestiary_recipes;
    std::vector<BestiaryActionDescriptor> bestiary_actions;
    std::unordered_map<std::string, std::uint32_t> bestiary_recipe_by_id;
    std::unordered_map<std::string, std::uint32_t> bestiary_action_by_id;

    // base items (parallel arrays, length base_count)
    std::uint32_t base_count = 0;
    std::vector<std::uint32_t> base_global_ids;
    std::vector<std::uint32_t> base_metadata_path_sid;
    std::vector<std::uint32_t> base_name_sid;
    std::vector<std::uint32_t> base_item_class_id;
    std::vector<std::int32_t> base_domain_code;
    std::vector<std::int32_t> base_drop_levels;
    std::vector<std::int32_t> base_session_support;
    std::vector<std::int32_t> base_flags;
    std::vector<std::uint32_t> base_tag_offsets;      // base_count + 1
    std::vector<std::uint32_t> base_implicit_offsets; // base_count + 1
    std::vector<std::int32_t> base_implicit_global_mod_ids;
    std::unordered_map<std::string, std::uint32_t> base_by_path;

    // item classes
    std::uint32_t item_class_count = 0;
    std::vector<std::uint32_t> item_class_global_ids;
    std::vector<std::uint32_t> item_class_key_sid;
    std::unordered_map<std::uint32_t, std::uint32_t> item_class_index_by_id;

    // base item tag links (flat, indexed by base_tag_offsets)
    std::vector<std::uint32_t> base_tag_ids;

    // tag identity
    std::unordered_map<std::string, std::uint32_t> tag_id_by_name;
    std::unordered_map<std::uint32_t, std::string> tag_name_by_id;

    // mods (parallel arrays, length mod_count)
    std::uint32_t mod_count = 0;
    std::vector<std::uint32_t> mod_global_ids;
    std::vector<std::uint32_t> mod_key_sid;
    std::vector<std::int32_t> mod_gen_type_code;
    std::vector<std::int32_t> mod_domain_code;
    std::vector<std::uint32_t> mod_required_level;
    std::vector<std::uint32_t> mod_primary_group;
    std::vector<std::int32_t> mod_flags;
    std::vector<std::int32_t> mod_influence_code;
    std::vector<std::int32_t> mod_metamod_type_code;
    std::vector<std::int32_t> mod_special_kind_code;
    std::vector<std::uint32_t> mod_group_offsets;  // mod_count + 1
    std::vector<std::uint32_t> mod_group_ids_flat; // all groups per mod
    std::unordered_map<std::uint32_t, std::uint32_t> mod_pos_by_global_id;
    std::unordered_map<std::string, std::uint32_t> mod_pos_by_key;

    // exclusivity-group stable keys + human display labels
    std::vector<std::uint32_t> group_key_sids;
    std::vector<std::uint32_t> group_display_name_sids;
    std::unordered_map<std::string, std::uint32_t> group_id_by_key;

    // mod display text lines (one mod -> N pre-translated lines)
    std::vector<std::uint32_t> mod_text_line_offsets;   // mod_count + 1
    std::vector<std::uint32_t> mod_text_line_sids;      // flat
    // stable c_str() pointers into `strings`, indexed by global mod position
    std::vector<std::vector<const char*>> mod_text_line_ptrs;

    // ordered weight rows (first-match semantics, indexed by offsets)
    std::vector<std::uint32_t> spawn_offsets; // mod_count + 1
    std::vector<std::uint32_t> spawn_tag_ids;
    std::vector<std::int32_t> spawn_weights;
    std::vector<std::uint32_t> gen_offsets; // mod_count + 1
    std::vector<std::uint32_t> gen_tag_ids;
    std::vector<std::int32_t> gen_weights;
    std::vector<std::uint32_t> class_offsets; // mod_count + 1
    std::vector<std::uint32_t> class_tag_ids;

    // stats (used for capacity checks and display-family identity)
    std::vector<std::uint32_t> stat_offsets; // mod_count + 1
    std::vector<std::uint32_t> stat_key_sids; // flat

    // bench options
    std::uint32_t bench_count = 0;
    std::vector<std::uint32_t> bench_global_option_ids;
    std::vector<std::int32_t> bench_global_mod_ids;
    std::vector<std::uint32_t> bench_action_kind_sids;
    std::vector<std::uint32_t> bench_action_value_sids;
    std::vector<std::int32_t> bench_tiers;
    std::vector<std::uint32_t> bench_class_offsets;
    std::vector<std::int32_t> bench_class_global_ids;

    // essence direct lookup
    std::uint32_t essence_count = 0;
    std::vector<std::uint32_t> essence_key_sids;
    std::vector<std::int32_t> essence_item_level_restrictions;
    std::vector<std::uint32_t> essence_mod_offsets;
    std::vector<std::uint32_t> essence_class_key_sids;
    std::vector<std::int32_t> essence_linked_global_mod_ids;
    std::unordered_map<std::string, std::uint32_t> essence_by_key;

    // fossil weights and direct added/forced mod links
    std::uint32_t fossil_count = 0;
    std::vector<std::uint32_t> fossil_key_sids;
    std::vector<std::uint32_t> fossil_name_sids;
    std::vector<std::int32_t> fossil_rolls_lucky;
    std::vector<std::int32_t> fossil_mirrors;
    std::vector<std::uint32_t> fossil_weight_offsets;
    std::vector<std::int32_t> fossil_weight_kind_codes;
    std::vector<std::uint32_t> fossil_weight_tag_ids;
    std::vector<std::int32_t> fossil_weight_values;
    std::vector<std::uint32_t> fossil_mod_offsets;
    std::vector<std::int32_t> fossil_mod_kind_codes;
    std::vector<std::int32_t> fossil_linked_global_mod_ids;
    std::unordered_map<std::string, std::uint32_t> fossil_by_key;
    int fossil_weight_negative_code = -1;
    int fossil_weight_positive_code = -1;
    int fossil_mod_added_code = -1;
    int fossil_mod_forced_code = -1;
    int fossil_mod_sell_price_code = -1;

    // enum mappings
    std::vector<std::string> domain_name_by_code;     // reverse domain enum
    std::unordered_map<std::string, int> domain_code_by_name;
    std::vector<std::string> influence_name_by_code;  // reverse influence enum
    std::unordered_map<std::string, int> influence_code_by_name;
    int gen_prefix_code = -1;
    int gen_suffix_code = -1;
    int gen_corrupted_code = -1;
    int gen_searing_implicit_code = -1;
    int gen_eater_implicit_code = -1;
    int metamod_multimod_code = -1;
    int metamod_no_attack_code = -1;
    int metamod_no_caster_code = -1;
    int metamod_prefixes_locked_code = -1;
    int metamod_suffixes_locked_code = -1;
    int special_corrupted_implicit_code = -1;
    int special_eldritch_implicit_code = -1;
    int special_unveiled_code = -1;
    int special_veiled_template_code = -1;

    const std::string& string_at(std::uint32_t sid) const {
        static const std::string empty;
        if (sid >= strings.size()) {
            return empty;
        }
        return strings[sid];
    }

    const std::string& domain_name(std::int32_t code) const {
        static const std::string empty;
        if (code < 0 ||
            static_cast<std::size_t>(code) >= domain_name_by_code.size()) {
            return empty;
        }
        return domain_name_by_code[static_cast<std::size_t>(code)];
    }
};

/* How a session mod entered the universe. */
enum class ReachKind : std::uint8_t {
    Base = 0,
    Influence = 1,
    Crafted = 2,
    Essence = 3,
    BaseImplicit = 4,
    Fossil = 5,
    Veiled = 6,
    Unveiled = 7,
    CorruptedImplicit = 8,
    EldritchImplicit = 9
};

/*
 * Immutable per-session runtime data: the dense mod universe, masks over dense
 * session mod ids, and base-effective-tag-signature weights. Built once at
 * session creation and never mutated (worker action contexts cache uncommon
 * signature weights separately).
 */
struct SessionImpl {
    std::shared_ptr<const DataImpl> data;
    std::uint32_t base_index = 0;
    std::uint32_t item_level = 0;

    // dense universe: session_mod_id (0..mod_count-1) -> DataImpl mod position
    std::uint32_t mod_count = 0;
    std::size_t words = 0;
    std::vector<std::uint32_t> global_index;
    std::vector<std::int8_t> gen_type;         // 0 prefix, 1 suffix, -1 special
    std::vector<std::uint32_t> primary_group;  // groups[0], for display
    std::vector<std::int32_t> flags;
    std::vector<std::int32_t> metamod_type;
    std::vector<std::int32_t> special_kind;
    // full exclusivity-group membership per session mod (multi-group blocking)
    std::vector<std::uint32_t> group_offsets;  // mod_count + 1
    std::vector<std::uint32_t> group_ids;      // flat
    std::vector<std::uint32_t> required_level;
    std::vector<std::int32_t> influence_code;  // -1 none
    std::vector<std::uint8_t> reach_kind;      // ReachKind
    std::vector<std::int32_t> reach_influence; // influence code if via influence
    std::vector<std::string> reach_via;        // "base" or "influence:<name>"
    std::unordered_map<std::uint32_t, std::uint32_t> session_id_by_global_id;

    // base effective-tag-signature weights (the common, eager signature).
    // base_spawn_weight doubles as active_spawn_weight for the base signature.
    std::vector<std::uint32_t> base_spawn_weight;
    std::vector<std::uint32_t> base_gen_pct;
    std::vector<std::uint32_t> base_roll_weight;

    // per-session-mod classification tags ("implicit_tags"), used by Harvest
    // targeting, fossil multipliers, and metamod blocking.
    std::vector<std::uint32_t> class_offsets; // mod_count + 1
    std::vector<std::uint32_t> class_tag_ids; // flat
    // Stable c_str() pointers into DataImpl tag names, indexed by session mod.
    std::vector<std::vector<const char*>> classification_tag_name_ptrs;

    // masks over dense session mod ids
    std::vector<std::uint64_t> universe_mask;
    std::vector<std::uint64_t> prefix_mask;
    std::vector<std::uint64_t> suffix_mask;
    std::vector<std::uint64_t> base_explicit_universe_mask;
    std::vector<std::uint64_t> normal_random_roll_mask;
    std::vector<std::uint64_t> crafted_mask;
    std::vector<std::uint64_t> essence_only_mask;
    std::vector<std::uint64_t> implicit_mask;
    std::vector<std::uint64_t> delve_mask;
    std::vector<std::uint64_t> veiled_template_mask;
    std::vector<std::uint64_t> unveiled_mask;
    std::vector<std::uint64_t> unveiled_generic_mask;
    std::vector<std::uint64_t> corrupted_implicit_mask;
    std::vector<std::uint64_t> eldritch_implicit_mask;
    std::vector<std::uint64_t> eldritch_searing_mask;
    std::vector<std::uint64_t> eldritch_eater_mask;
    std::vector<std::uint64_t> positive_spawn_weight_mask;
    std::vector<std::uint64_t> positive_base_weight_mask;
    // Group/tag ids are dense global ids. Flat outer vectors avoid hash-table
    // lookups in the per-pick path; absent ids have an empty inner mask.
    std::vector<std::vector<std::uint64_t>> group_masks;
    std::vector<std::vector<std::uint64_t>> implicit_tag_masks;
    std::vector<std::vector<std::uint64_t>> influence_masks;

    std::vector<std::uint32_t> effective_base_tag_ids; // sorted, for debug/info
    std::vector<std::int64_t> selector_tag_by_influence;

    // Direct-mechanic lookup tables use dense session mod ids.
    std::vector<std::uint32_t> base_implicit_mod_ids;
    std::vector<std::uint32_t> bench_mod_ids;
    std::uint32_t veiled_prefix_mod_id =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t veiled_suffix_mod_id =
        std::numeric_limits<std::uint32_t>::max();
    std::vector<std::vector<std::uint32_t>> eldritch_searing_tier_mod_ids;
    std::vector<std::vector<std::uint32_t>> eldritch_eater_tier_mod_ids;
    std::vector<std::vector<std::uint32_t>> fossil_sell_price_mod_ids;
    std::vector<std::uint32_t> essence_guaranteed_mod_ids;
    std::vector<std::vector<std::uint32_t>> fossil_added_mod_ids;
    std::vector<std::vector<std::uint32_t>> fossil_forced_mod_ids;

    // max affixes per side for a rare item: 2 for (abyss) jewels, else 3.
    std::uint8_t rare_affix_cap = 3;
    bool eldritch_eligible = false;

    // Display-family identity is primary exclusion group + ordered stat
    // signature + generation side + acquisition source. It is deliberately
    // distinct from the exclusion group used by crafting legality.
    std::vector<std::uint32_t> family_id;
    // Family tier ranks: 1 = highest required_level mod in the family.
    std::vector<std::uint32_t> family_tier_index;
};

/* One weighted candidate. */
struct PoolEntry {
    std::uint32_t session_mod_id;
    std::uint32_t global_mod_id;
    std::uint32_t primary_group;
    std::int8_t gen_type;
    std::uint32_t required_level;
    std::uint32_t spawn_weight;
    std::uint32_t generation_pct;
    std::uint32_t final_weight;
};

struct WeightTable {
    std::vector<std::uint32_t> tag_ids;
    std::vector<std::uint32_t> spawn_weight;
    std::vector<std::uint32_t> generation_pct;
    std::vector<std::uint32_t> base_roll_weight;
    std::vector<std::uint32_t> spawn_tag_id;
    std::vector<std::uint32_t> generation_tag_id;
    std::vector<std::int32_t> spawn_row_ordinal;
    std::vector<std::int32_t> generation_row_ordinal;
    std::vector<std::uint64_t> positive_spawn_mask;
    std::vector<std::uint64_t> positive_base_mask;
};

struct WeightedPool {
    std::vector<PoolEntry> entries;
    std::vector<std::uint64_t> prefix_sums;
    std::uint64_t total_weight = 0;
    std::uint64_t prefix_total_weight = 0;
    std::uint64_t suffix_total_weight = 0;
};

enum class PoolWeightKind : std::uint8_t {
    Normal = 0,
    HarvestSpawnOnly = 1,
    Fossil = 2
};

struct PoolBuildRequest {
    PoolWeightKind weight_kind = PoolWeightKind::Normal;
    int side_filter = -1;
    std::uint32_t target_tag_id = std::numeric_limits<std::uint32_t>::max();
    int influence_only_code = -1;
    std::vector<std::uint32_t> fossil_indices;
};

struct PoolBuildHints {
    std::vector<std::uint64_t>* group_block_mask = nullptr;
    bool block_attack = false;
    bool block_caster = false;
};

struct PoolCacheKey {
    std::vector<std::uint64_t> candidate_mask;
    std::uint32_t tag_signature_id = 0;
    PoolWeightKind weight_kind = PoolWeightKind::Normal;
    std::uint32_t target_tag_id = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> fossil_indices;

    bool operator==(const PoolCacheKey& other) const {
        return candidate_mask == other.candidate_mask &&
               tag_signature_id == other.tag_signature_id &&
               weight_kind == other.weight_kind &&
               target_tag_id == other.target_tag_id &&
               fossil_indices == other.fossil_indices;
    }
};

struct PoolCacheLookup {
    const std::vector<std::uint64_t>* candidate_mask = nullptr;
    std::uint32_t tag_signature_id = 0;
    PoolWeightKind weight_kind = PoolWeightKind::Normal;
    std::uint32_t target_tag_id = std::numeric_limits<std::uint32_t>::max();
    const std::vector<std::uint32_t>* fossil_indices = nullptr;
};

struct PoolCacheKeyHash {
    using is_transparent = void;
    std::size_t operator()(const PoolCacheKey& key) const;
    std::size_t operator()(const PoolCacheLookup& key) const;
};

struct PoolCacheKeyEqual {
    using is_transparent = void;
    bool operator()(
        const PoolCacheKey& a,
        const PoolCacheKey& b) const {
        return a == b;
    }
    bool operator()(
        const PoolCacheKey& a,
        const PoolCacheLookup& b) const;
    bool operator()(
        const PoolCacheLookup& a,
        const PoolCacheKey& b) const {
        return (*this)(b, a);
    }
};

struct RefillPoolCacheKey {
    std::vector<std::uint64_t> group_block_mask;
    std::uint32_t tag_signature_id = 0;
    PoolWeightKind weight_kind = PoolWeightKind::Normal;
    std::uint32_t target_tag_id = std::numeric_limits<std::uint32_t>::max();
    std::uint8_t influence_bits = 0;
    std::int8_t side_filter = -1;
    std::int8_t influence_only_code = -1;
    bool block_attack = false;
    bool block_caster = false;
    std::vector<std::uint32_t> fossil_indices;

    bool operator==(const RefillPoolCacheKey& other) const {
        return group_block_mask == other.group_block_mask &&
               tag_signature_id == other.tag_signature_id &&
               weight_kind == other.weight_kind &&
               target_tag_id == other.target_tag_id &&
               influence_bits == other.influence_bits &&
               side_filter == other.side_filter &&
               influence_only_code == other.influence_only_code &&
               block_attack == other.block_attack &&
               block_caster == other.block_caster &&
               fossil_indices == other.fossil_indices;
    }
};

struct RefillPoolCacheLookup {
    const std::vector<std::uint64_t>* group_block_mask = nullptr;
    std::uint32_t tag_signature_id = 0;
    PoolWeightKind weight_kind = PoolWeightKind::Normal;
    std::uint32_t target_tag_id = std::numeric_limits<std::uint32_t>::max();
    std::uint8_t influence_bits = 0;
    std::int8_t side_filter = -1;
    std::int8_t influence_only_code = -1;
    bool block_attack = false;
    bool block_caster = false;
    const std::vector<std::uint32_t>* fossil_indices = nullptr;
};

struct RefillPoolCacheHash {
    using is_transparent = void;
    std::size_t operator()(const RefillPoolCacheKey& key) const;
    std::size_t operator()(const RefillPoolCacheLookup& key) const;
};

struct RefillPoolCacheEqual {
    using is_transparent = void;
    bool operator()(
        const RefillPoolCacheKey& a,
        const RefillPoolCacheKey& b) const {
        return a == b;
    }
    bool operator()(
        const RefillPoolCacheKey& a,
        const RefillPoolCacheLookup& b) const;
    bool operator()(
        const RefillPoolCacheLookup& a,
        const RefillPoolCacheKey& b) const {
        return (*this)(b, a);
    }
};

struct PoolDebugRow {
    PoolEntry entry{};
    std::uint32_t tag_signature_id = 0;
    std::uint32_t active_spawn_tag_id =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t active_generation_tag_id =
        std::numeric_limits<std::uint32_t>::max();
    std::int32_t active_spawn_row = -1;
    std::int32_t active_generation_row = -1;
    std::uint32_t blocking_group_id =
        std::numeric_limits<std::uint32_t>::max();
    std::int32_t first_failure = 0;
    bool normal_random_member = false;
    bool side_allowed = false;
    bool influence_allowed = false;
    bool group_allowed = false;
    bool mechanic_allowed = false;
    bool positively_weighted = false;
    bool generation_applied = true;
    std::uint32_t special_multiplier_pct = 100;
};

struct ActionTraceStage {
    std::uint32_t stage_index = 0;
    bool direct = false;
    bool cache_hit = false;
    std::uint32_t tag_signature_id = 0;
    PoolWeightKind weight_kind = PoolWeightKind::Normal;
    int side_filter = -1;
    std::uint64_t prefix_total_weight = 0;
    std::uint64_t suffix_total_weight = 0;
    std::uint64_t combined_total_weight = 0;
    std::uint64_t roll = 0;
    std::uint32_t chosen_mod_id = std::numeric_limits<std::uint32_t>::max();
    std::int8_t chosen_side = -1;
};

struct ActionContextImpl {
    std::shared_ptr<const SessionImpl> session;
    Rng rng;
    std::unordered_map<std::string, std::uint32_t> signature_by_key;
    std::array<std::uint32_t, 256> signature_by_influence_bits;
    std::vector<WeightTable> uncommon_weight_tables;
    std::unordered_map<
        PoolCacheKey,
        WeightedPool,
        PoolCacheKeyHash,
        PoolCacheKeyEqual>
        pool_cache;
    std::unordered_map<
        RefillPoolCacheKey,
        const WeightedPool*,
        RefillPoolCacheHash,
        RefillPoolCacheEqual>
        refill_pool_cache;
    const RefillPoolCacheKey* last_refill_pool_key = nullptr;
    const WeightedPool* last_refill_pool = nullptr;
    std::vector<std::uint64_t> candidate_mask_scratch;
    std::vector<std::uint64_t> block_mask_scratch;
    std::vector<std::uint64_t> influence_mask_scratch;
    std::vector<std::uint64_t> empty_group_mask;
    std::vector<std::uint32_t> occupied_groups_scratch;
    std::vector<ActionTraceStage> last_action_trace;
    std::uint64_t pool_cache_hits = 0;
    std::uint64_t pool_cache_misses = 0;
    std::uint64_t candidate_build_ns = 0;
    std::uint64_t weighted_pool_build_ns = 0;
    std::uint64_t sampling_calls = 0;
    std::uint64_t sampling_ns = 0;
    bool perf_timing_enabled = false;
    bool incremental_refill_enabled = true;
    bool capture_action_trace = true;

    explicit ActionContextImpl(std::uint64_t seed) : rng(seed) {
        signature_by_influence_bits.fill(
            std::numeric_limits<std::uint32_t>::max());
        signature_by_influence_bits[0] = 0;
    }
};

/*
 * Parse the three artifact JSON documents into a DataImpl. Throws
 * std::runtime_error with a descriptive message on malformed/incomplete data.
 */
std::shared_ptr<DataImpl> load_data_impl(
    const std::string& manifest_text,
    const std::string& strings_text,
    const std::string& game_data_text);

/* Parse a single bundled JSON document of shape
 * {"manifest":..,"strings":..,"game_data":..} (the WebAssembly memory path). */
std::shared_ptr<DataImpl> load_data_impl_bundle(const std::string& bundle_text);

/*
 * Build the dense session universe and masks for the selected base/item level.
 * The caller has already verified the base is ordinary (non-cluster). Mirrors
 * tools/ingest/poecraft_ingest/engine_selection.py so engine pools match the
 * spec fixtures.
 */
void build_session(SessionImpl& session);

/*
 * Build the normal explicit weighted candidate pool for the current item using
 * the base tag signature. side_filter: -1 both, 0 prefix only, 1 suffix only.
 * Group blocking is derived from the item's live explicit slots. Mirrors the
 * spawn x generation first-match formula with integer truncation.
 */
std::uint32_t intern_item_tag_signature(
    ActionContextImpl& context,
    const pc_item_state* item);

const WeightedPool& get_weighted_pool(
    ActionContextImpl& context,
    const pc_item_state* item,
    const PoolBuildRequest& request,
    bool* out_cache_hit = nullptr,
    const PoolBuildHints* hints = nullptr);

void build_pool_debug_rows(
    ActionContextImpl& context,
    const pc_item_state* item,
    const PoolBuildRequest& request,
    bool include_rejected,
    std::vector<PoolDebugRow>& out_rows,
    WeightedPool* out_summary,
    bool* out_cache_hit);

/* Core crafting actions. Matches the pc_action_type C enum order. */
enum class ActionType : int {
    Transmute = 0,
    Augment = 1,
    Alteration = 2,
    Regal = 3,
    Alchemy = 4,
    Chaos = 5,
    Exalt = 6,
    Annul = 7,
    Scour = 8,
    Essence = 9,
    Fossil = 10,
    Bench = 11,
    VeiledChaos = 12,
    VeiledExalt = 13,
    Unveil = 14,
    HarvestReforge = 15,
    HarvestAugment = 16,
    HarvestResist = 17,
    EldritchEmber = 18,
    EldritchIchor = 19,
    EldritchExalt = 20,
    EldritchChaos = 21,
    EldritchAnnul = 22,
    InfluenceExalt = 23,
    Fracture = 24,
    RemoveCraftedModifiers = 25
};

struct ActionParameters {
    ActionType type = ActionType::Transmute;
    std::uint32_t essence_index = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> fossil_indices;
    std::uint32_t mod_id = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t target_tag_id = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t source_tag_id = std::numeric_limits<std::uint32_t>::max();
    int influence_code = -1;
    std::uint32_t tier = 0;
};

struct ActionOutcome {
    bool applied = false;
    int added = 0;
    int removed = 0;
};

/*
 * Apply one crafting action, mutating *item in place. Sampling uses rng. When
 * the action's preconditions are not met (wrong rarity, no open affix, nothing
 * removable) the item is left unchanged and outcome.applied is false. Callers
 * that need failed-call atomicity apply to a temporary copy (the C ABI does).
 */
ActionOutcome apply_action(
    ActionContextImpl& context,
    pc_item_state* item,
    const ActionParameters& action);

BestiaryActionOutcome apply_bestiary_action(
    const DataImpl& data,
    BestiaryCraftState& state,
    std::uint32_t action_index);

BestiaryCalculation calculate_bestiary_action(
    const DataImpl& data,
    const BestiaryCraftState& state,
    std::uint32_t action_index);

// --- compiled strategy simulator -------------------------------------------

enum class ConditionKind : std::uint8_t {
    Always = 0,
    HasModGroup = 1,
    RarityIs = 2,
    OpenPrefixCount = 3,
    OpenSuffixCount = 4,
    PrefixCountRange = 5,
    SuffixCountRange = 6,
    All = 7,
      Any = 8,
      Not = 9,
      AtLeast = 10,
      HasModFamily = 11,
      ModCount = 12,
      ItemFlag = 13,
      InfluenceBits = 14,
      EldritchTier = 15,
      HasUnveilOption = 16,
      ModFamilyCount = 17
  };

enum class ItemFlagKind : std::uint8_t {
    Corrupted,
    Mirrored,
    Split,
    Synthesised,
    Fractured,
    Crafted,
    Veiled,
    VeiledPrefix,
    VeiledSuffix,
    Multimod,
    NoAttack,
    NoCaster,
    PrefixesLocked,
    SuffixesLocked,
    Influenced,
    EldritchImplicit,
};

struct CompiledCondition {
    ConditionKind kind = ConditionKind::Always;
    std::uint32_t group_id = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t family_id = std::numeric_limits<std::uint32_t>::max();
    std::uint8_t required_flags = 0;
    ItemFlagKind item_flag = ItemFlagKind::Corrupted;
    std::uint8_t eldritch_side = 0; /* 0 searing, 1 eater */
    int min_value = 0;
    int max_value = 0;
    std::vector<std::uint32_t> mod_ids;
    std::vector<std::uint32_t> family_ids;
    std::vector<CompiledCondition> children;
    /* Structurally identical predicates share a per-strategy memo slot. */
    std::uint32_t memo_slot = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t count_memo_slot =
        std::numeric_limits<std::uint32_t>::max();
};

enum class StrategyNodeKind : std::uint8_t {
    Start = PC_STRATEGY_NODE_START,
    Operation = PC_STRATEGY_NODE_OPERATION,
    Router = PC_STRATEGY_NODE_ROUTER,
    Terminal = PC_STRATEGY_NODE_TERMINAL
};

struct StrategyEdge {
    std::string id;
    std::uint32_t target = 0;
    int priority = 0;
    bool is_default = false;
    std::uint32_t source_order = 0;
    CompiledCondition condition;
};

struct StrategyDispatchNode {
    std::uint32_t condition = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t when_true = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t when_false = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t edge = std::numeric_limits<std::uint32_t>::max();
};

struct StrategyDispatchSignature {
    std::vector<std::uint32_t> true_conditions;
    std::uint32_t edge = std::numeric_limits<std::uint32_t>::max();
};

struct StrategyDirectDispatchSignature {
    std::vector<std::int16_t> values;
    std::uint32_t edge = std::numeric_limits<std::uint32_t>::max();
};

/* Operation-node action_type for the synthetic "restart" operation: throw
 * the item away and continue on a fresh base (price key "base"). Outside
 * the pc_action_type range on purpose; apply_action never sees it. */
inline constexpr int kStrategyRestartOperation = 1000;
inline constexpr int kStrategyBestiaryImprintOperation = 1001;
inline constexpr int kStrategyBestiaryRestoreImprintOperation = 1002;

struct StrategyNode {
    std::string id;
    StrategyNodeKind kind = StrategyNodeKind::Start;
    ActionParameters action;
    int action_type = -1;
    std::uint32_t bestiary_action_index =
        std::numeric_limits<std::uint32_t>::max();
    std::vector<std::string> price_keys;
    int terminal_kind = -1;
    std::string reason;
    std::vector<StrategyEdge> edges;
    /* Exact decision DAG for conjunction-only routing nodes. It preserves
     * ordinary edge priority while avoiding a linear scan of hundreds of
     * solver-emitted abstract-state predicates after every primitive action. */
    std::vector<CompiledCondition> dispatch_conditions;
    std::vector<StrategyDispatchNode> dispatch_nodes;
    std::unordered_map<
        std::size_t, std::vector<StrategyDispatchSignature>>
        dispatch_signatures;
    std::vector<CompiledCondition> direct_dispatch_features;
    std::unordered_map<
        std::size_t, std::vector<StrategyDirectDispatchSignature>>
        direct_dispatch_signatures;
};

struct StrategyImpl {
    std::shared_ptr<const SessionImpl> session;
    std::string name;
    pc_item_state start_item{};
    std::uint32_t start_node = 0;
    std::vector<StrategyNode> nodes;
    std::unordered_map<std::string, std::uint32_t> node_by_id;
    std::uint32_t condition_memo_slots = 0;
    std::uint32_t count_memo_slots = 0;
};

/* Simulator-authoritative condition predicate, exposed internally for exact
 * evaluator parity tests. */
bool evaluate_compiled_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item);

struct EconomyImpl {
    std::string id;
    std::unordered_map<std::string, double> prices;
};

struct TraceEntryInternal {
    std::uint32_t step_index = 0;
    std::string node_id;
    int node_kind = PC_STRATEGY_NODE_START;
    int action_type = -1;
    bool action_applied = false;
    std::string matched_edge_id;
    std::uint64_t cumulative_actions = 0;
    double known_cumulative_cost = 0.0;
    bool cost_complete = true;
    int terminal_kind = -1;
    int failure_reason = PC_SIM_FAILURE_NONE;
    pc_item_state item{};
};

struct RetainedTrace {
    std::vector<TraceEntryInternal> entries;
};

struct SimulationExampleInternal {
    int terminal_kind = PC_TERMINAL_FAILURE;
    int failure_reason = PC_SIM_FAILURE_NONE;
    std::string terminal_node_id;
    std::uint64_t action_count = 0;
    double known_total_cost = 0.0;
    bool cost_complete = true;
    pc_item_state item{};
};

struct FailureSummaryInternal {
    int failure_reason = PC_SIM_FAILURE_NONE;
    std::string node_id;
    std::string detail;
    std::uint64_t count = 0;
};

struct SimulationSummaryInternal {
    std::uint64_t completed_runs = 0;
    std::uint64_t success_count = 0;
    std::uint64_t failure_count = 0;
    std::uint64_t stop_count = 0;
    std::uint64_t total_actions = 0;
    std::uint64_t action_limit_count = 0;
    std::uint64_t cost_limit_count = 0;
    std::uint64_t step_limit_count = 0;
    std::uint64_t no_matching_edge_count = 0;
    std::uint64_t action_not_applied_count = 0;
    std::uint64_t missing_price_run_count = 0;
    std::uint64_t costed_action_count = 0;
    std::uint64_t missing_price_action_count = 0;
    double known_total_cost = 0.0;
};

struct SimulationOptionsInternal {
    std::uint64_t target_runs = 0;
    std::uint64_t seed = 0;
    std::uint32_t max_actions_per_run = 0;
    std::uint32_t max_graph_steps_per_run = 0;
    double max_cost_per_run = 0.0;
    std::uint32_t retained_trace_count = 0;
    std::uint32_t max_trace_entries = 0;
    std::uint32_t retained_success_count = 0;
    std::uint32_t retained_failure_count = 0;
};

struct SimulatorImpl {
    std::shared_ptr<const SessionImpl> session;
    std::shared_ptr<const StrategyImpl> strategy;
    std::shared_ptr<const EconomyImpl> economy;
    std::unique_ptr<ActionContextImpl> context;
    bool options_set = false;
    SimulationOptionsInternal options;
    SimulationSummaryInternal summary;
    std::vector<RetainedTrace> traces;
    std::vector<SimulationExampleInternal> success_examples;
    std::vector<SimulationExampleInternal> failure_examples;
    std::vector<FailureSummaryInternal> failure_summaries;
    std::vector<std::uint64_t> action_counts;
    std::unordered_map<std::string, std::uint64_t> missing_prices;
    std::vector<double> node_prices;
    std::vector<std::uint8_t> node_prices_known;
    std::vector<std::vector<std::string>> node_missing_price_keys;
    std::vector<std::uint32_t> condition_cache_generation;
    std::vector<std::uint8_t> condition_cache_values;
    std::vector<std::uint32_t> count_cache_generation;
    std::vector<std::int8_t> count_cache_values;
    std::vector<std::int16_t> direct_dispatch_values_scratch;
    std::vector<std::uint32_t> true_conditions_scratch;
    std::uint32_t current_condition_generation = 0;
};

std::shared_ptr<StrategyImpl> compile_strategy_json(
    std::shared_ptr<const SessionImpl> session,
    const char* strategy_json,
    std::size_t strategy_json_size);

std::shared_ptr<EconomyImpl> load_economy_json(
    const char* economy_json,
    std::size_t economy_json_size);

void prepare_simulator_runtime(SimulatorImpl& simulator);

void run_simulator_chunk(
    SimulatorImpl& simulator,
    const SimulationOptionsInternal& options,
    std::uint32_t max_completed_runs);

} // namespace poecraft

#endif
