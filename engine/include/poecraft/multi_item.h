#ifndef POECRAFT_MULTI_ITEM_H
#define POECRAFT_MULTI_ITEM_H
#include "poecraft/session.h"
#ifdef __cplusplus
extern "C" {
#endif

#define PC_MAX_CRAFT_RESOURCES 8
typedef enum pc_resource_effect {
    PC_RESOURCE_RETAINED = 0,
    PC_RESOURCE_CHANGED = 1,
    PC_RESOURCE_CREATED = 2,
    PC_RESOURCE_CONSUMED = 3
} pc_resource_effect;

/* Identity belongs to the workspace, not the native handle. A role names how
 * this request uses a resource; it does not create or acquire that resource. */
typedef struct pc_craft_resource {
    const char* identity;
    const char* role;
    pc_session_handle session;
    pc_item_state* item;
} pc_craft_resource;

typedef struct pc_resource_change {
    const char* identity; /* borrowed from the input request */
    int32_t effect;
    pc_item_state before;
    pc_item_state after;
} pc_resource_change;

typedef struct pc_multi_item_result {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t resource_count;
    pc_resource_change resources[PC_MAX_CRAFT_RESOURCES];
    const char* consumed_price_key; /* static; acquisition is separate */
} pc_multi_item_result;

/* Validates all roles, identities, sessions and mechanics before committing
 * any item mutation. Failure preserves every input and the RNG. Result holds
 * recorded before/after values for application Undo; it is not an Imprint. */
pc_result pc_multi_item_apply(
    pc_action_context_handle context, const char* action,
    pc_craft_resource* resources, uint32_t resource_count,
    pc_multi_item_result* out_result, pc_error_info* out_error);

#ifdef __cplusplus
}
#endif
#endif
