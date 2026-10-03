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

/* Approved simulation-only independent cached requests, version 1. Apply pays
 * one Lock and reserves the finite supported normalized request table without
 * choosing an actual currency. Observe chooses/reveals an applicable entry,
 * free and stable across repeated observations and checkpoint restoration.
 * No exact cross-currency game law or adaptive solver authority is claimed.
 * Reservations are persisted as full outcomes, never RNG seed/state. */
pc_result pc_hinekora_lock_apply(pc_action_context_handle context,
    pc_item_state* item, pc_hinekora_lock_handle* out_lock,
    pc_error_info* out_error);
pc_result pc_hinekora_lock_observe(pc_hinekora_lock_handle lock,
    const pc_item_state* item, const pc_action_request* currency,
    pc_item_state* out_preview, pc_action_result* out_result,
    pc_error_info* out_error);
/* Legacy fixed-request creation/checkpoints remain supported below. */
/* Fixed-currency foresight for the existing native item projection. One live
 * foresight per action context; it is bound to this caller-owned item address,
 * session and complete currency request. Creation spends one Lock and reserves
 * one native outcome, marking only PC_ITEM_FORESEEN on the input. Ordinary
 * Calculator/solver/strategy ingress refuses this information state; copies
 * retain the marker without inheriting the original identity-bound Lock.
 * Inapplicable requests refuse
 * before sampling/payment. Veiled currencies and pending Unveil offers refuse
 * until their visible/hidden disclosure and portable checkpoint contract is approved.
 * No cross-currency joint law or solver model is claimed.
 * Keep the item alive while the context retains its identity, or call
 * release_item before ending its lifetime. Handle destruction is memory
 * cleanup, not an in-game decline/refresh. A second Lock refuses until an
 * item modification or successful currency use (including equal visible
 * output), even if the old handle was destroyed or merely invalidated. */
pc_result pc_hinekora_lock_create(pc_action_context_handle context,
    pc_item_state* item, const pc_action_request* currency,
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
/* Native lifetime cleanup, not an item action/decline. Call before this item's
 * storage ends while its context survives (even after handle destruction).
 * Detaches the ended address so allocator reuse cannot inherit its identity.
 * Leaves the old value's marker intact: a surviving marked value stays
 * unavailable until explicit checkpoint restoration. No draw or payment. */
void pc_hinekora_lock_release_item(pc_action_context_handle context,
    const pc_item_state* item);
/* Versioned emulator checkpoint, not a probability/reachability certificate.
 * Fixed checkpoints contain the selected outcome; independent checkpoints
 * contain the complete reserved domain, including unseen outcomes. Neither
 * exports an RNG seed/state. Snapshot
 * identity pins runtime hashes, base/level, full real item fields and request.
 * Export retains a declined/inactivated no-refresh tombstone. Restore is an
 * explicit history/import replacement: it rebinds this paid state without
 * sampling or payment, atomically replacing any previous context foresight.
 * Callers must keep the old item alive until replacement completes.
 * snapshot_item validates runtime/session identity and returns exact stored
 * current storage for transport reconstruction; ordinary crafting still needs
 * restore. A marker without a matching checkpoint remains unavailable. */
pc_result pc_hinekora_lock_export(pc_hinekora_lock_handle lock,
    const pc_item_state* item, char* buffer, size_t capacity,
    size_t* out_length, pc_error_info* out_error);
pc_result pc_hinekora_lock_snapshot_item(pc_action_context_handle context,
    const char* snapshot_json, size_t size, pc_item_state* out_item,
    pc_error_info* out_error);
/* currency may be null only for an independent checkpoint with no selection. */
pc_result pc_hinekora_lock_restore(pc_action_context_handle context,
    pc_item_state* item, const pc_action_request* currency,
    const char* snapshot_json, size_t size,
    pc_hinekora_lock_handle* out_lock, pc_error_info* out_error);
#ifdef __cplusplus
}
#endif
#endif
