#ifndef POECRAFT_HINEKORA_H
#define POECRAFT_HINEKORA_H
#include "poecraft/session.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_hinekora_lock* pc_hinekora_lock_handle;
/* Registry-independent consumption metadata; no solver action admission. */
const char* pc_hinekora_lock_cost_key(void);
int32_t pc_hinekora_lock_currency_supported(int32_t action_type);

/* Fixed-currency foresight for the existing native item projection. One live
 * foresight per action context; it is bound to this caller-owned item address,
 * session and complete currency request. Creation spends one Lock and reserves
 * one native outcome without modifying the item. Inapplicable requests refuse
 * before sampling/payment. No cross-currency joint law or solver model is claimed.
 * Keep the item alive until destroying the handle. Handle destruction is memory
 * cleanup, not an in-game decline/refresh. A second Lock refuses until an
 * item modification or successful currency use (including equal visible
 * output), even if the old handle was destroyed or merely invalidated. */
pc_result pc_hinekora_lock_create(pc_action_context_handle context,
    const pc_item_state* item, const pc_action_request* currency,
    pc_hinekora_lock_handle* out_lock, pc_error_info* out_error);
/* Inspection is free and idempotent. It never changes item or context RNG.
 * Ordinary pc_apply_action of the bound request commits this exact preview.
 * Applying another successful action invalidates it. Different items refuse. */
pc_result pc_hinekora_lock_preview(pc_hinekora_lock_handle lock,
    const pc_item_state* item, pc_item_state* out_preview,
    pc_action_result* out_result, pc_error_info* out_error);
pc_result pc_hinekora_lock_commit(pc_hinekora_lock_handle lock,
    pc_item_state* item, pc_action_result* out_result, pc_error_info* out_error);
pc_result pc_hinekora_lock_status(pc_hinekora_lock_handle lock,
    const pc_item_state* item, int32_t* out_active, pc_error_info* out_error);
/* Call when modifying the raw value-copyable item through an external editor,
 * Bestiary, multi-item operation or import/history replacement. Such writes do
 * not pass an action context; the engine cannot observe arbitrary memory writes.
 * Invalidation is permanent, even if the previous item bytes are restored. */
void pc_hinekora_lock_invalidate(pc_hinekora_lock_handle lock);
void pc_hinekora_lock_destroy(pc_hinekora_lock_handle lock);
#ifdef __cplusplus
}
#endif
#endif
