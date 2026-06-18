#ifndef POECRAFT_SESSION_H
#define POECRAFT_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Session runtime support, mirrors the artifact session_support enum. */
typedef enum pc_session_support {
    PC_SESSION_SUPPORT_ORDINARY = 0,
    PC_SESSION_SUPPORT_CLUSTER_UNSUPPORTED = 1,
    PC_SESSION_SUPPORT_UNSUPPORTED_DOMAIN = 2
} pc_session_support;

/*
 * A session pins one base + item level out of the immutable data set and builds
 * its dense full mod universe, static masks, and base-signature weights.
 * Creating a session for a cluster base returns PC_RESULT_UNSUPPORTED_FEATURE.
 */
typedef struct pc_session_options {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* base_metadata_path; /* stable RePoE metadata path */
    uint32_t item_level;
} pc_session_options;

pc_result pc_session_create(
    pc_data_handle data,
    const pc_session_options* options,
    pc_session_handle* out_session,
    pc_error_info* out_error);

void pc_session_destroy(pc_session_handle session);

/* Resolved identity of a session's base. String fields point into immutable
 * data owned by the session and are valid until the session is destroyed. */
typedef struct pc_base_info {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t global_base_item_id;
    uint32_t item_level;
    int32_t session_support; /* pc_session_support */
    uint32_t tag_count;
    uint32_t implicit_count;
    const char* metadata_path;
    const char* name;
    const char* item_class_key;
    const char* domain;
} pc_base_info;

pc_result pc_session_get_base_info(
    pc_session_handle session,
    pc_base_info* out_info,
    pc_error_info* out_error);

/*
 * An action context owns per-worker mutable state: random number generator and
 * craft scratch. Two contexts may share one immutable session safely. seed
 * makes the RNG reproducible for tests; cross-platform replay is not a
 * compatibility guarantee.
 */
typedef struct pc_action_context_options {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t seed;
} pc_action_context_options;

pc_result pc_action_context_create(
    pc_session_handle session,
    const pc_action_context_options* options,
    pc_action_context_handle* out_context,
    pc_error_info* out_error);

void pc_action_context_destroy(pc_action_context_handle context);

/* Draw the next 64-bit value from the context RNG. Exposed for tests and
 * diagnostics; real actions consume the RNG internally. */
pc_result pc_action_context_next_u64(
    pc_action_context_handle context,
    uint64_t* out_value,
    pc_error_info* out_error);

/* Re-seed the context RNG (test/debug helper). */
pc_result pc_action_context_reseed(
    pc_action_context_handle context,
    uint64_t seed,
    pc_error_info* out_error);

/*
 * Initialise an item for this session's base. options may be null for an empty
 * normal item. Failed calls leave *out_item unchanged.
 */
typedef struct pc_item_init_options {
    uint32_t struct_size;
    uint32_t abi_version;
    uint8_t rarity; /* pc_rarity */
    int32_t with_implicits; /* 1 to populate base implicits */
} pc_item_init_options;

pc_result pc_item_init(
    pc_session_handle session,
    const pc_item_init_options* options,
    pc_item_state* out_item,
    pc_error_info* out_error);

/* Human-readable item dump. Query-required-count buffer pattern (see
 * pc_data_debug_format). */
pc_result pc_item_debug_format(
    pc_session_handle session,
    const pc_item_state* item,
    char* buffer,
    size_t buffer_size,
    size_t* out_length,
    pc_error_info* out_error);

// --- session debug / introspection -----------------------------------------

/* Number of dense session mod ids (0..count-1). */
pc_result pc_session_get_mod_count(
    pc_session_handle session,
    uint32_t* out_count,
    pc_error_info* out_error);

/* Per-session-mod metadata. String fields are owned by the session and valid
 * until it is destroyed. */
typedef struct pc_mod_info {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t session_mod_id;
    uint32_t global_mod_id;
    const char* key;
    int32_t generation_type; /* 0 prefix, 1 suffix */
    int32_t reach_kind;      /* pc_mod_reach_kind */
    int32_t reach_influence; /* influence enum code, or -1 */
    const char* reach_via;   /* "base" or "influence:<name>" */
    uint32_t primary_group_id;
    uint32_t required_level;
} pc_mod_info;

typedef enum pc_mod_reach_kind {
    PC_MOD_REACH_BASE = 0,
    PC_MOD_REACH_INFLUENCE = 1,
    PC_MOD_REACH_CRAFTED = 2,
    PC_MOD_REACH_ESSENCE = 3,
    PC_MOD_REACH_BASE_IMPLICIT = 4,
    PC_MOD_REACH_FOSSIL = 5
} pc_mod_reach_kind;

pc_result pc_session_get_mod_info(
    pc_session_handle session,
    uint32_t session_mod_id,
    pc_mod_info* out_info,
    pc_error_info* out_error);

/* Named static session masks. */
typedef enum pc_session_mask_kind {
    PC_MASK_UNIVERSE = 0,
    PC_MASK_PREFIX = 1,
    PC_MASK_SUFFIX = 2,
    PC_MASK_NORMAL_RANDOM_ROLL = 3,
    PC_MASK_POSITIVE_SPAWN = 4,
    PC_MASK_POSITIVE_BASE = 5,
    PC_MASK_BASE_EXPLICIT_UNIVERSE = 6,
    PC_MASK_CRAFTED = 7,
    PC_MASK_ESSENCE_ONLY = 8,
    PC_MASK_IMPLICIT = 9,
    PC_MASK_DELVE = 10
} pc_session_mask_kind;

/* Dump the session's effective base tag names (query-required-count). String
 * pointers are owned by the session. */
pc_result pc_session_dump_effective_tags(
    pc_session_handle session,
    const char** out_names,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/* Dump every exclusivity group id a session mod belongs to (query-required-
 * count). Multi-group mods return more than one id. */
pc_result pc_session_dump_mod_groups(
    pc_session_handle session,
    uint32_t session_mod_id,
    uint32_t* out_group_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/* Dump a named mask as ascending session mod ids (query-required-count). */
pc_result pc_session_dump_mask(
    pc_session_handle session,
    int mask_kind,
    uint32_t* out_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/* One weighted candidate from a debug pool query. */
typedef struct pc_pool_entry {
    uint32_t session_mod_id;
    uint32_t global_mod_id;
    uint32_t primary_group_id;
    int32_t generation_type;
    uint32_t required_level;
    uint32_t spawn_weight;
    uint32_t generation_multiplier_pct;
    uint32_t final_weight;
} pc_pool_entry;

/*
 * Build the normal explicit weighted candidate pool for the given item using
 * the base tag signature, applying group blocking from the item's live slots.
 * side_filter: -1 both sides, 0 prefix only, 1 suffix only. Uses the
 * query-required-count pattern: out_count always receives the full entry count.
 */
pc_result pc_action_context_debug_pool(
    pc_action_context_handle context,
    const pc_item_state* item,
    int side_filter,
    pc_pool_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/*
 * Build a Harvest-targeted candidate pool restricted to mods carrying the named
 * classification tag, weighted by active spawn weight only (each entry's
 * final_weight == spawn_weight; generation multipliers do not apply).
 * Returns PC_RESULT_NOT_FOUND if the tag name is unknown. side_filter as above.
 */
pc_result pc_action_context_debug_harvest_pool(
    pc_action_context_handle context,
    const pc_item_state* item,
    const char* target_tag,
    int side_filter,
    pc_pool_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/* Dump the session mods carrying a classification tag (implicit_tag_mask),
 * as ascending session mod ids. Returns PC_RESULT_NOT_FOUND for unknown tags. */
pc_result pc_session_dump_implicit_tag(
    pc_session_handle session,
    const char* tag,
    uint32_t* out_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/* Dump one materialized influence mask by artifact influence code. Code 0 is
 * the non-influence mask. generic_influence_bits uses bit (code - 1). */
pc_result pc_session_dump_influence_mask(
    pc_session_handle session,
    int32_t influence_code,
    uint32_t* out_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

// --- crafting actions -------------------------------------------------------

typedef enum pc_action_type {
    PC_ACTION_TRANSMUTE = 0,
    PC_ACTION_AUGMENT = 1,
    PC_ACTION_ALTERATION = 2,
    PC_ACTION_REGAL = 3,
    PC_ACTION_ALCHEMY = 4,
    PC_ACTION_CHAOS = 5,
    PC_ACTION_EXALT = 6,
    PC_ACTION_ANNUL = 7,
    PC_ACTION_SCOUR = 8,
    PC_ACTION_ESSENCE = 9,
    PC_ACTION_FOSSIL = 10
} pc_action_type;

#define PC_MAX_FOSSILS_PER_ACTION 4

typedef struct pc_action_request {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t action_type; /* pc_action_type */
    const char* essence_key; /* stable metadata key for PC_ACTION_ESSENCE */
    uint32_t fossil_count;
    const char* fossil_keys[PC_MAX_FOSSILS_PER_ACTION];
} pc_action_request;

typedef struct pc_action_result {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t applied;   /* 1 if the item changed, 0 if preconditions failed */
    int32_t added;     /* explicit mods added */
    int32_t removed;   /* explicit mods removed */
} pc_action_result;

/*
 * Apply one crafting action to the item using the context's RNG. Sampling and
 * mutation happen on a private copy; the input item is overwritten only when
 * the action succeeds, so a failed/inapplicable action leaves it unchanged
 * (out_result.applied == 0). Returns PC_RESULT_INVALID_ARGUMENT for a bad
 * action type or null arguments.
 */
pc_result pc_apply_action(
    pc_action_context_handle context,
    pc_item_state* item,
    const pc_action_request* request,
    pc_action_result* out_result,
    pc_error_info* out_error);

typedef struct pc_batch_summary {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t item_count;
    uint32_t applied_count;
    uint64_t total_added;
    uint64_t total_removed;
} pc_batch_summary;

/*
 * Apply one parsed action request to a contiguous array of caller-owned items.
 * Each item has the same per-item commit semantics as pc_apply_action. results
 * may be null when only the aggregate summary is needed. The context RNG and
 * caches are reused across the whole batch; the last-action trace describes
 * the final item processed.
 */
pc_result pc_apply_action_batch(
    pc_action_context_handle context,
    pc_item_state* items,
    uint32_t item_count,
    const pc_action_request* request,
    pc_action_result* results,
    pc_batch_summary* out_summary,
    pc_error_info* out_error);

// --- request-shaped rich pool debugging ------------------------------------

typedef enum pc_pool_debug_failure {
    PC_POOL_DEBUG_ACCEPTED = 0,
    PC_POOL_DEBUG_NOT_NORMAL_RANDOM = 1,
    PC_POOL_DEBUG_SIDE_CLOSED = 2,
    PC_POOL_DEBUG_MECHANIC_FILTER = 3,
    PC_POOL_DEBUG_INFLUENCE_FILTER = 4,
    PC_POOL_DEBUG_GROUP_BLOCK = 5,
    PC_POOL_DEBUG_ZERO_WEIGHT = 6
} pc_pool_debug_failure;

typedef struct pc_pool_query_request {
    uint32_t struct_size;
    uint32_t abi_version;
    pc_action_request action;
    int32_t side_filter;     /* -1 both, 0 prefix, 1 suffix */
    int32_t include_rejected; /* non-zero returns every session row */
} pc_pool_query_request;

typedef struct pc_pool_debug_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t session_mod_id;
    uint32_t global_mod_id;
    const char* key;
    int32_t generation_type;
    int32_t reach_kind;
    const char* reach_via;
    uint32_t tag_signature_id;
    int32_t normal_random_member;
    int32_t side_allowed;
    int32_t mechanic_allowed;
    int32_t influence_allowed;
    int32_t group_allowed;
    int32_t positively_weighted;
    int32_t first_failure; /* pc_pool_debug_failure */
    uint32_t blocking_group_id; /* UINT32_MAX when not blocked */
    int32_t active_spawn_row;
    const char* active_spawn_tag;
    uint32_t active_spawn_weight;
    int32_t active_generation_row;
    const char* active_generation_tag;
    uint32_t active_generation_pct;
    int32_t generation_applied;
    uint32_t special_multiplier_pct;
    uint32_t final_weight;
} pc_pool_debug_entry;

typedef struct pc_pool_debug_summary {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t tag_signature_id;
    int32_t cache_hit;
    uint32_t candidate_count;
    uint64_t prefix_total_weight;
    uint64_t suffix_total_weight;
    uint64_t combined_total_weight;
    uint64_t cache_hits;
    uint64_t cache_misses;
} pc_pool_debug_summary;

/*
 * Inspect the exact pool used by an action request. With include_rejected=0,
 * entries contains only the weighted candidate pool. With include_rejected=1,
 * every session row is returned with its first failing stage and active ordered
 * spawn/generation rows. Query-required-count semantics apply.
 */
pc_result pc_debug_pool_query(
    pc_action_context_handle context,
    const pc_item_state* item,
    const pc_pool_query_request* request,
    pc_pool_debug_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_pool_debug_summary* out_summary,
    pc_error_info* out_error);

/* One selection stage from the most recent pc_apply_action call. Direct
 * essence/fossil additions have direct=1 and no random roll totals. */
typedef struct pc_action_trace_stage {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t stage_index;
    int32_t direct;
    int32_t cache_hit;
    uint32_t tag_signature_id;
    int32_t weight_kind; /* internal normal/Harvest/fossil weight context */
    int32_t side_filter;
    uint64_t prefix_total_weight;
    uint64_t suffix_total_weight;
    uint64_t combined_total_weight;
    uint64_t roll;
    uint32_t chosen_session_mod_id;
    int32_t chosen_side;
} pc_action_trace_stage;

/* Query the chosen mod/side and pool totals for every selection stage in the
 * most recent action. A failed/no-op action produces an empty trace. */
pc_result pc_action_context_debug_last_trace(
    pc_action_context_handle context,
    pc_action_trace_stage* stages,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

#ifdef __cplusplus
}
#endif

#endif
