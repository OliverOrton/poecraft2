#ifndef POECRAFT_HINEKORA_INTERNAL_HPP
#define POECRAFT_HINEKORA_INTERNAL_HPP
#include "engine_internal.hpp"
#include "poecraft/hinekora.h"
namespace poecraft {
struct HinekoraReservation {
    ActionParameters action;
    pc_item_state preview{};
    ActionOutcome outcome;
};
struct HinekoraForesight {
    std::shared_ptr<const SessionImpl> session;
    pc_item_state* identity = nullptr;
    pc_item_state input{};
    pc_item_state preview{};
    ActionParameters action;
    ActionOutcome outcome;
    bool independent = false;
    bool selected = true;
    std::vector<HinekoraReservation> reservations;
    bool active = true;
    bool refresh_allowed = false;
};
// The generic ABI parser resolves stable keys; mechanics stay in this module.
pc_result resolve_foresight_request(const ActionContextImpl& context,
    const pc_action_request& request, ActionParameters& action,
    pc_error_info* error);
ActionOutcome apply_with_foresight(ActionContextImpl& context,
    pc_item_state* item, const ActionParameters& action);
}
#endif
