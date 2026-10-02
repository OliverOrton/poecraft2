#include "hinekora_internal.hpp"
#include "handles_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

struct pc_hinekora_lock {
    std::shared_ptr<poecraft::HinekoraForesight> impl;
};
namespace {
void error(pc_error_info* out, pc_result code, const char* message) {
    if (!out) return;
    pc_error_info_init(out);
    out->code = code;
    std::snprintf(out->message, sizeof(out->message), "%s", message);
}
void result(const poecraft::ActionOutcome& outcome, pc_action_result* out) {
    *out = {sizeof(*out), PC_ABI_VERSION, outcome.applied ? 1 : 0,
            outcome.added, outcome.removed};
}
template<std::size_t N>
bool same_slots(const pc_mod_slot (&a)[N], const pc_mod_slot (&b)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
        if (a[i].mod_id != b[i].mod_id || a[i].group_id != b[i].group_id ||
            a[i].flags != b[i].flags || a[i].roll_count != b[i].roll_count ||
            a[i].veiled_option_count != b[i].veiled_option_count ||
            a[i].veiled_chosen_mod_id != b[i].veiled_chosen_mod_id ||
            !std::equal(std::begin(a[i].rolls), std::end(a[i].rolls), std::begin(b[i].rolls)) ||
            !std::equal(std::begin(a[i].veiled_option_mod_ids), std::end(a[i].veiled_option_mod_ids), std::begin(b[i].veiled_option_mod_ids))) return false;
    }
    return true;
}
bool same(const pc_item_state& a, const pc_item_state& b) {
    // Every native field is compared exactly, including numeric rolls and
    // metadata. C/C++ padding bytes are not item state and may change on copies.
    return a.rarity == b.rarity && a.quality == b.quality &&
        a.memory_strands == b.memory_strands && a.lifecycle == b.lifecycle &&
        a.item_flags == b.item_flags && a.prefix_count == b.prefix_count &&
        a.suffix_count == b.suffix_count && a.implicit_count == b.implicit_count &&
        a.enchantment_count == b.enchantment_count &&
        same_slots(a.prefixes, b.prefixes) && same_slots(a.suffixes, b.suffixes) &&
        same_slots(a.implicits, b.implicits) && same_slots(a.enchantments, b.enchantments) &&
        a.generic_influence_bits == b.generic_influence_bits &&
        a.searing_exarch_tier == b.searing_exarch_tier &&
        a.eater_of_worlds_tier == b.eater_of_worlds_tier &&
        a.socket_count == b.socket_count && a.link_mask == b.link_mask &&
        std::equal(std::begin(a.socket_colors), std::end(a.socket_colors), std::begin(b.socket_colors));
}
bool current(poecraft::HinekoraForesight& f, const pc_item_state* item) {
    if (f.identity != item) return false;
    if (!same(f.input, *item)) { f.active = false; f.refresh_allowed = true; }
    return f.active;
}
bool same_action(const poecraft::ActionParameters& a,
                 const poecraft::ActionParameters& b) {
    return a.type == b.type && a.essence_index == b.essence_index &&
        a.fossil_indices == b.fossil_indices && a.mod_id == b.mod_id &&
        a.target_tag_id == b.target_tag_id && a.source_tag_id == b.source_tag_id &&
        a.influence_code == b.influence_code && a.tier == b.tier;
}
bool currency(poecraft::ActionType type) {
    using T = poecraft::ActionType;
    switch (type) {
    case T::Transmute: case T::Augment: case T::Alteration: case T::Regal:
    case T::Alchemy: case T::Chaos: case T::Exalt: case T::Annul: case T::Scour:
    case T::Essence: case T::VeiledChaos: case T::VeiledExalt:
    case T::EldritchEmber: case T::EldritchIchor: case T::EldritchExalt:
    case T::EldritchChaos: case T::EldritchAnnul: case T::InfluenceExalt:
    case T::Fracture: case T::FoulbornAugment: case T::FoulbornRegal:
    case T::FoulbornExalt: case T::Dominance: case T::Vaal:
        return true;
    default: return false;
    }
}
pc_result check(pc_hinekora_lock_handle lock, const pc_item_state* item,
                pc_error_info* out_error) {
    if (!lock || !item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock or item");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (lock->impl->identity != item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock belongs to a different item identity");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!current(*lock->impl, item)) {
        error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, "Lock foresight is consumed or invalidated");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
    return PC_RESULT_OK;
}
}
namespace poecraft {
ActionOutcome apply_with_foresight(ActionContextImpl& context,
    pc_item_state* item, const ActionParameters& action) {
    auto& f = context.hinekora_foresight;
    if (f && current(*f, item) && same_action(f->action, action)) {
        *item = f->preview;
        f->active = false;
        f->refresh_allowed = true;
        return f->outcome;
    }
    pc_item_state working = *item;
    auto outcome = apply_action(context, &working, action);
    if (outcome.applied) {
        *item = working;
        if (f && f->identity == item) {
            f->active = false;
            f->refresh_allowed = true;
        }
    }
    return outcome;
}
}
const char* pc_hinekora_lock_cost_key(void) { return "hinekora_lock"; }
int32_t pc_hinekora_lock_currency_supported(int32_t action_type) {
    return currency(static_cast<poecraft::ActionType>(action_type)) ? 1 : 0;
}
pc_result pc_hinekora_lock_create(pc_action_context_handle context,
    const pc_item_state* item, const pc_action_request* request,
    pc_hinekora_lock_handle* out_lock, pc_error_info* out_error) {
    if (out_lock) *out_lock = nullptr;
    if (!context || !item || !request || !out_lock) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock creation argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        poecraft::ActionParameters action;
        auto rc = poecraft::resolve_foresight_request(*context->impl, *request,
                                                     action, out_error);
        if (rc != PC_RESULT_OK) return rc;
        if (!currency(action.type)) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                "Lock fixed-currency preview does not support this operation");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        const auto& previous = context->impl->hinekora_foresight;
        if (previous) {
            if (previous->identity == item && same(previous->input, *item) &&
                !previous->refresh_allowed) {
                error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                    "Modify the item before applying another Lock; decline does not refresh foresight");
                return PC_RESULT_UNSUPPORTED_FEATURE;
            }
            if (previous->identity != item && previous->active) {
                error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                    "One live Lock per context; resolve the existing item first");
                return PC_RESULT_UNSUPPORTED_FEATURE;
            }
        }
        auto foresight = std::make_shared<poecraft::HinekoraForesight>();
        foresight->identity = item;
        foresight->input = *item;
        foresight->preview = *item;
        foresight->action = action;
        // Fresh private caches avoid copying pointers into context pool caches.
        // Reserve exactly the same marginal law as the selected native action.
        poecraft::ActionContextImpl sampled(0);
        sampled.session = context->impl->session;
        sampled.rng = context->impl->rng;
        foresight->outcome = poecraft::apply_action(sampled, &foresight->preview, action);
        if (!foresight->outcome.applied) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                  "Selected currency is inapplicable; no Lock was consumed");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        auto holder = std::make_unique<pc_hinekora_lock>();
        holder->impl = foresight;
        if (previous) previous->active = false;
        context->impl->rng = sampled.rng;
        context->impl->hinekora_foresight = std::move(foresight);
        *out_lock = holder.release();
        error(out_error, PC_RESULT_OK, "");
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
}
pc_result pc_hinekora_lock_preview(pc_hinekora_lock_handle lock,
    const pc_item_state* item, pc_item_state* out_preview,
    pc_action_result* out_result, pc_error_info* out_error) {
    if (!out_preview || !out_result || out_preview == item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock preview requires separate output and result");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    auto rc = check(lock, item, out_error);
    if (rc != PC_RESULT_OK) return rc;
    *out_preview = lock->impl->preview;
    result(lock->impl->outcome, out_result);
    error(out_error, PC_RESULT_OK, "");
    return PC_RESULT_OK;
}
pc_result pc_hinekora_lock_commit(pc_hinekora_lock_handle lock,
    pc_item_state* item, pc_action_result* out_result, pc_error_info* out_error) {
    if (!out_result) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock commit result");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    auto rc = check(lock, item, out_error);
    if (rc != PC_RESULT_OK) return rc;
    *item = lock->impl->preview;
    lock->impl->active = false;
    lock->impl->refresh_allowed = true;
    result(lock->impl->outcome, out_result);
    error(out_error, PC_RESULT_OK, "");
    return PC_RESULT_OK;
}
pc_result pc_hinekora_lock_status(pc_hinekora_lock_handle lock,
    const pc_item_state* item, int32_t* out_active, pc_error_info* out_error) {
    if (!lock || !item || !out_active) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock status argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (lock->impl->identity != item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock belongs to a different item identity");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_active = current(*lock->impl, item) ? 1 : 0;
    error(out_error, PC_RESULT_OK, "");
    return PC_RESULT_OK;
}
void pc_hinekora_lock_invalidate(pc_hinekora_lock_handle lock) {
    if (lock) lock->impl->active = false;
}
void pc_hinekora_lock_destroy(pc_hinekora_lock_handle lock) { delete lock; }
