#ifndef POECRAFT_SRC_ENGINE_INTERNAL_HPP
#define POECRAFT_SRC_ENGINE_INTERNAL_HPP

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "poecraft/item_state.h"
#include "poecraft/rng.h"

/*
 * Internal engine representation. Public opaque handles (pc_data, pc_session,
 * pc_action_context) are reinterpret-cast to/from these. DataImpl is immutable
 * after load and shared by shared_ptr so a session/context keeps it alive even
 * if the caller destroys the data handle first.
 */
namespace poecraft {

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

    // base items (parallel arrays, length base_count)
    std::uint32_t base_count = 0;
    std::vector<std::uint32_t> base_global_ids;
    std::vector<std::uint32_t> base_metadata_path_sid;
    std::vector<std::uint32_t> base_name_sid;
    std::vector<std::uint32_t> base_item_class_id;
    std::vector<std::int32_t> base_domain_code;
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
    std::vector<std::uint32_t> mod_group_offsets;  // mod_count + 1
    std::vector<std::uint32_t> mod_group_ids_flat; // all groups per mod
    std::unordered_map<std::uint32_t, std::uint32_t> mod_pos_by_global_id;

    // ordered weight rows (first-match semantics, indexed by offsets)
    std::vector<std::uint32_t> spawn_offsets; // mod_count + 1
    std::vector<std::uint32_t> spawn_tag_ids;
    std::vector<std::int32_t> spawn_weights;
    std::vector<std::uint32_t> gen_offsets; // mod_count + 1
    std::vector<std::uint32_t> gen_tag_ids;
    std::vector<std::int32_t> gen_weights;
    std::vector<std::uint32_t> class_offsets; // mod_count + 1
    std::vector<std::uint32_t> class_tag_ids;

    // stats (capacity informational)
    std::vector<std::uint32_t> stat_offsets; // mod_count + 1

    // bench options needed to collect crafted mods for an item class
    std::vector<std::int32_t> bench_global_mod_ids;
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
    std::vector<std::int32_t> fossil_rolls_lucky;
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

    // enum mappings
    std::vector<std::string> domain_name_by_code;     // reverse domain enum
    std::unordered_map<std::string, int> domain_code_by_name;
    std::vector<std::string> influence_name_by_code;  // reverse influence enum
    int gen_prefix_code = -1;
    int gen_suffix_code = -1;

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
    Fossil = 5
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
    std::vector<std::uint64_t> positive_spawn_weight_mask;
    std::vector<std::uint64_t> positive_base_weight_mask;
    std::unordered_map<std::uint32_t, std::vector<std::uint64_t>> group_masks;
    std::unordered_map<std::uint32_t, std::vector<std::uint64_t>>
        implicit_tag_masks;
    std::vector<std::vector<std::uint64_t>> influence_masks;

    std::vector<std::uint32_t> effective_base_tag_ids; // sorted, for debug/info
    std::vector<std::int64_t> selector_tag_by_influence;

    // Direct-mechanic lookup tables use dense session mod ids.
    std::vector<std::uint32_t> base_implicit_mod_ids;
    std::vector<std::uint32_t> essence_guaranteed_mod_ids;
    std::vector<std::vector<std::uint32_t>> fossil_added_mod_ids;
    std::vector<std::vector<std::uint32_t>> fossil_forced_mod_ids;

    // max affixes per side for a rare item: 2 for (abyss) jewels, else 3.
    std::uint8_t rare_affix_cap = 3;
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
    std::vector<std::uint32_t> fossil_indices;
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

struct PoolCacheKeyHash {
    std::size_t operator()(const PoolCacheKey& key) const;
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
    std::vector<WeightTable> uncommon_weight_tables;
    std::unordered_map<PoolCacheKey, WeightedPool, PoolCacheKeyHash> pool_cache;
    std::vector<std::uint64_t> candidate_mask_scratch;
    std::vector<std::uint64_t> block_mask_scratch;
    std::vector<ActionTraceStage> last_action_trace;
    std::uint64_t pool_cache_hits = 0;
    std::uint64_t pool_cache_misses = 0;

    explicit ActionContextImpl(std::uint64_t seed) : rng(seed) {}
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
    bool* out_cache_hit = nullptr);

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
    Fossil = 10
};

struct ActionParameters {
    ActionType type = ActionType::Transmute;
    std::uint32_t essence_index = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> fossil_indices;
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

} // namespace poecraft

#endif
