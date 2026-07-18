#ifndef POECRAFT_BESTIARY_H
#define POECRAFT_BESTIARY_H

#include <stdint.h>

#include "poecraft/api.h"
#include "poecraft/item_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The approved B1 Imprint recipe has four repeated price-key entries. */
#define PC_BESTIARY_MAX_COST_KEYS 4

typedef enum pc_bestiary_operation {
    PC_BESTIARY_OPERATION_CREATE = 0,
    PC_BESTIARY_OPERATION_RESTORE = 1
} pc_bestiary_operation;

typedef enum pc_bestiary_refusal {
    PC_BESTIARY_REFUSAL_NONE = 0,
    PC_BESTIARY_REFUSAL_UNKNOWN_ACTION = 1,
    PC_BESTIARY_REFUSAL_REQUIRES_MAGIC_ITEM = 2,
    PC_BESTIARY_REFUSAL_ITEM_CORRUPTED = 3,
    PC_BESTIARY_REFUSAL_ITEM_MIRRORED = 4,
    PC_BESTIARY_REFUSAL_CHECKPOINT_ALREADY_EXISTS = 5,
    PC_BESTIARY_REFUSAL_CHECKPOINT_MISSING = 6,
    PC_BESTIARY_REFUSAL_CHECKPOINT_BOUND_TO_DIFFERENT_ITEM = 7
} pc_bestiary_refusal;

typedef enum pc_bestiary_checkpoint_requirement {
    PC_BESTIARY_CHECKPOINT_ABSENT = 0,
    PC_BESTIARY_CHECKPOINT_PRESENT = 1
} pc_bestiary_checkpoint_requirement;

typedef enum pc_bestiary_checkpoint_effect {
    PC_BESTIARY_CHECKPOINT_CREATE = 0,
    PC_BESTIARY_CHECKPOINT_CONSUME = 1
} pc_bestiary_checkpoint_effect;

/*
 * One live item plus an optional saved checkpoint bound to its stable identity.
 * This compound state deliberately leaves pc_item_state unchanged and does not
 * model the checkpoint as a second live item.
 */
typedef struct pc_bestiary_craft_state {
    uint32_t struct_size;
    uint32_t abi_version;
    pc_item_state item;
    uint64_t live_item_identity;
    int32_t checkpoint_present;
    uint64_t checkpoint_bound_identity;
    pc_item_state checkpoint;
} pc_bestiary_craft_state;

typedef struct pc_bestiary_action_info {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t action_index;
    uint32_t global_action_id;
    uint32_t global_recipe_id;
    const char* action_id;
    const char* recipe_id;
    const char* family_display_name;
    const char* display_name;
    int32_t operation; /* pc_bestiary_operation */
    int32_t transition_kind; /* 0: deterministic */
    int32_t emulator_available;
    int32_t calculator_available;
    int32_t strategy_builder_available;
    int32_t solver_available;
    int32_t checkpoint_requirement;
    int32_t checkpoint_effect;
    int32_t identity_requirement; /* 0: current item, 1: same item */
    uint32_t cost_key_count;
    const char* cost_keys[PC_BESTIARY_MAX_COST_KEYS];
} pc_bestiary_action_info;

typedef struct pc_bestiary_action_request {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* action_id;
} pc_bestiary_action_request;

typedef struct pc_bestiary_action_result {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* action_id;
    int32_t applied;
    int32_t refusal; /* pc_bestiary_refusal */
    const char* refusal_key;
    const char* refusal_reason;
    uint32_t cost_key_count;
    const char* cost_keys[PC_BESTIARY_MAX_COST_KEYS];
    uint32_t consumed_price_key_count;
    const char* consumed_price_keys[PC_BESTIARY_MAX_COST_KEYS];
    uint32_t output_item_count;
    uint32_t output_checkpoint_count;
    uint32_t consumed_checkpoint_count;
    int32_t checkpoint_present;
} pc_bestiary_action_result;

typedef struct pc_bestiary_calculation {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t outcome_count; /* one deterministic outcome */
    double probability;
    pc_bestiary_craft_state successor;
    pc_bestiary_action_result result;
} pc_bestiary_calculation;

/* Engine-owned presentation for the one specific B1 solver option. */
typedef struct pc_bestiary_solver_option_info {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t option_index;
    const char* option_id;
    const char* display_name;
    const char* goal_restriction_key;
    const char* goal_restriction;
    int32_t goal_rarity; /* pc_rarity */
    int32_t requires_complete_goal;
    uint32_t min_program_actions;
    uint32_t max_program_actions;
} pc_bestiary_solver_option_info;

pc_result pc_bestiary_state_init(
    const pc_item_state* item,
    uint64_t live_item_identity,
    pc_bestiary_craft_state* out_state,
    pc_error_info* out_error);

pc_result pc_bestiary_get_action_count(
    pc_data_handle data,
    uint32_t* out_count,
    pc_error_info* out_error);

pc_result pc_bestiary_get_action_info(
    pc_data_handle data,
    uint32_t action_index,
    pc_bestiary_action_info* out_info,
    pc_error_info* out_error);

pc_result pc_bestiary_find_action(
    pc_data_handle data,
    const char* action_id,
    uint32_t* out_index,
    pc_error_info* out_error);

/*
 * Apply commits the compound successor only on success. Every refusal leaves
 * the live item, checkpoint, and consumption unchanged.
 */
pc_result pc_bestiary_apply_action(
    pc_data_handle data,
    pc_bestiary_craft_state* state,
    const pc_bestiary_action_request* request,
    pc_bestiary_action_result* out_result,
    pc_error_info* out_error);

/* Deterministic, non-mutating calculation through the same native transition. */
pc_result pc_bestiary_calculate_action(
    pc_data_handle data,
    const pc_bestiary_craft_state* state,
    const pc_bestiary_action_request* request,
    pc_bestiary_calculation* out_calculation,
    pc_error_info* out_error);

pc_result pc_bestiary_get_solver_option_count(
    pc_data_handle data,
    uint32_t* out_count,
    pc_error_info* out_error);

pc_result pc_bestiary_get_solver_option_info(
    pc_data_handle data,
    uint32_t option_index,
    pc_bestiary_solver_option_info* out_info,
    pc_error_info* out_error);

#ifdef __cplusplus
}
#endif

#endif
