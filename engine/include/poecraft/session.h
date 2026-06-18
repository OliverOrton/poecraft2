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
 * A session pins one base + item level out of the immutable data set. In this
 * phase it resolves the base and exposes its identity; dense session mod
 * universe and masks are built in Phase 5. Creating a session for a cluster
 * base returns PC_RESULT_UNSUPPORTED_FEATURE.
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
    int32_t reach_kind;      /* 0 base, 1 influence */
    int32_t reach_influence; /* influence enum code, or -1 */
    const char* reach_via;   /* "base" or "influence:<name>" */
    uint32_t primary_group_id;
    uint32_t required_level;
} pc_mod_info;

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
    PC_MASK_POSITIVE_BASE = 5
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
    PC_ACTION_SCOUR = 8
} pc_action_type;

typedef struct pc_action_request {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t action_type; /* pc_action_type */
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

#ifdef __cplusplus
}
#endif

#endif
