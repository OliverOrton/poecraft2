#ifndef POECRAFT_API_H
#define POECRAFT_API_H

#include <stddef.h>
#include <stdint.h>

#include "poecraft/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PC_ABI_VERSION 1u

/*
 * Opaque handles. Lifetime/threading rules (see docs/codebase-structure.md):
 *  - data and session handles are immutable after construction and shareable
 *    across threads.
 *  - an action context owns random state and scratch space and belongs to one
 *    worker/thread at a time.
 *  - child handles retain internal references to parent data, so destroying a
 *    parent handle does not dangle a live child.
 *  - every create has one matching null-safe destroy.
 */
typedef struct pc_data* pc_data_handle;
typedef struct pc_session* pc_session_handle;
typedef struct pc_action_context* pc_action_context_handle;
typedef struct pc_strategy* pc_strategy_handle;
typedef struct pc_economy* pc_economy_handle;
typedef struct pc_simulator* pc_simulator_handle;

uint32_t pc_abi_version(void);

/* Initialise a caller-owned error buffer. Safe to pass to any API afterwards. */
void pc_error_info_init(pc_error_info* error);

/*
 * Load the complete runtime artifact.
 *
 * pc_data_load_file reads a manifest.json and its sibling game-data.json and
 * strings.json from the same directory.
 *
 * pc_data_load_memory parses a single bundled JSON document of the shape
 * {"manifest": {...}, "strings": {...}, "game_data": {...}}; this is the path
 * WebAssembly callers use since they cannot read sibling files.
 */
pc_result pc_data_load_file(
    const char* manifest_path,
    pc_data_handle* out_data,
    pc_error_info* out_error);

pc_result pc_data_load_memory(
    const void* bytes,
    size_t byte_count,
    pc_data_handle* out_data,
    pc_error_info* out_error);

void pc_data_destroy(pc_data_handle data);

/* Top-level row counts taken from the loaded manifest. */
typedef struct pc_data_summary {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t artifact_schema_version;
    int32_t complete_dataset; /* 1 when the manifest declares it complete */
    uint32_t string_count;
    uint32_t tag_count;
    uint32_t item_class_count;
    uint32_t base_item_count;
    uint32_t ordinary_session_base_count;
    uint32_t cluster_unsupported_base_count;
    uint32_t unsupported_domain_base_count;
    uint32_t mod_count;
    uint32_t group_count;
} pc_data_summary;

pc_result pc_data_get_summary(
    pc_data_handle data,
    pc_data_summary* out_summary,
    pc_error_info* out_error);

/*
 * Look up a base by its dense index in [0, base_item_count). out_path receives
 * a pointer to the stable metadata path (owned by data, valid until destroy)
 * and out_session_support receives its pc_session_support classification. This
 * lets callers enumerate the dataset and resolve any base by metadata path.
 */
pc_result pc_data_get_base_path(
    pc_data_handle data,
    uint32_t base_index,
    const char** out_path,
    int32_t* out_session_support,
    pc_error_info* out_error);

/* Display name and item-class key for a base, both owned by data. Useful for
 * picker UIs. Either out pointer may be null. */
pc_result pc_data_get_base_display(
    pc_data_handle data,
    uint32_t base_index,
    const char** out_name,
    const char** out_item_class_key,
    pc_error_info* out_error);

/* Canonical base drop-level requirement for picker ordering. A negative value
 * is the artifact's explicit unknown sentinel; callers must not invent a
 * replacement level. */
pc_result pc_data_get_base_drop_level(
    pc_data_handle data,
    uint32_t base_index,
    int32_t* out_drop_level,
    pc_error_info* out_error);

/*
 * Validate provisional fixed capacities against the loaded dataset. The report
 * records the maximum observed value for each capacity so the ABI can be
 * tuned before it is frozen. result is PC_RESULT_CAPACITY_EXCEEDED when any
 * observed value exceeds its compile-time limit; the report is still filled.
 */
typedef struct pc_capacity_report {
    uint32_t struct_size;
    uint32_t abi_version;

    uint32_t max_base_implicits;       /* limit: PC_MAX_IMPLICITS */
    uint32_t max_mod_stats;            /* limit: PC_MAX_ROLL_VALUES */
    uint32_t max_base_tags;            /* informational */
    uint32_t max_mod_groups;           /* informational */
    uint32_t max_essence_mod_links;    /* informational */

    int32_t base_implicits_ok;
    int32_t mod_stats_ok;
} pc_capacity_report;

pc_result pc_data_check_capacities(
    pc_data_handle data,
    pc_capacity_report* out_report,
    pc_error_info* out_error);

/*
 * Write a short human-readable summary of the loaded data into a caller buffer
 * using the query-required-count pattern: out_length always receives the byte
 * length (excluding the trailing NUL) that a full write needs. When the buffer
 * is too small the result is PC_RESULT_BUFFER_TOO_SMALL and the buffer is left
 * untouched.
 */
pc_result pc_data_debug_format(
    pc_data_handle data,
    char* buffer,
    size_t buffer_size,
    size_t* out_length,
    pc_error_info* out_error);

#ifdef __cplusplus
}
#endif

#endif
