#ifndef POECRAFT_RECOMBINATION_H
#define POECRAFT_RECOMBINATION_H
#include "poecraft/multi_item.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PC_RECOMBINATION_PAIR_VERSION 1u
typedef struct pc_recombination_pair* pc_recombination_pair_handle;

/* Current Random only. Inputs are snapshotted with their distinct workspace
 * identities and interpreting sessions. No RNG draw or mutation occurs here.
 * Model v1 estimates spawn-proportional selection and preserves selected tiers
 * and recorded rolls, without unverified upgrade bonuses. */
pc_result pc_recombination_pair_create(const pc_craft_resource* input_a,
    const pc_craft_resource* input_b, uint32_t pair_version,
    pc_recombination_pair_handle* out_pair, pc_error_info* out_error);
void pc_recombination_pair_destroy(pc_recombination_pair_handle pair);

/* Returns an independently owned session handle. Caller destroys it with
 * pc_session_destroy. Carrier 0/1 maps each result to its actual output base
 * and averaged level, including retained incoming identities below roll level. */
pc_result pc_recombination_pair_output_session(pc_recombination_pair_handle pair,
    uint32_t carrier, pc_session_handle* out_session, pc_error_info* out_error);

/* Query-required-count JSON contract, length excludes trailing NUL. Read only;
 * outcomes contain canonical selected occurrences and native full output items.
 * Each output session must be obtained with the preceding mapping API.
 * Native shared-goal observers consume these outcomes in their own sessions;
 * there is no automatic solver or rolled-stat/defence-percentile goal scope. */
pc_result pc_recombination_pair_calculate_json(pc_recombination_pair_handle pair,
    char* buffer, size_t buffer_size, size_t* out_length, pc_error_info* out_error);

typedef struct pc_recombination_result {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t pair_version;
    uint32_t carrier;
    const char* model_id; /* static */
    pc_multi_item_result transaction;
    pc_session_handle output_session; /* independently owned on success */
    pc_item_state output_item;
    uint32_t gold_cost_complete; /* 0 means unknown, never a zero quote */
    uint32_t dust_cost_complete;
} pc_recombination_result;

/* Pass all inventory resources relevant to collision checking (up to the
 * native fixed capacity). Identities and item addresses must be distinct.
 * Both pinned inputs are consumed; the new output is returned separately in
 * its own session. Successful receipts store before/after for Undo/Redo; replay
 * does not sample again. Stale/replay/alias/output-ID collision errors preserve
 * every input, RNG and result buffer. Gold/dust remain explicitly incomplete.
 * Receipt identity strings are borrowed from resources/output_identity. */
pc_result pc_recombination_pair_apply(pc_recombination_pair_handle pair,
    pc_action_context_handle context, pc_craft_resource* resources,
    uint32_t resource_count, const char* output_identity,
    pc_recombination_result* out_result, pc_error_info* out_error);
#ifdef __cplusplus
}
#endif
#endif
