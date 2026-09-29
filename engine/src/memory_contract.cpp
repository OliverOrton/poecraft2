#include "poecraft/session.h"
#include "engine_internal.hpp"

namespace poecraft {
const char* unavailable_currency_reason(ActionType type) {
    switch (type) {
    case ActionType::Dominance:
        return "Dominance is unavailable: canonical T1-to-elevated relationships and selectable-pair probabilities are unresolved";
    case ActionType::Tempering:
    case ActionType::Tailoring:
        return "Heist enchantment crafting is unavailable: current eligible-pool weights and socket consequences are unresolved; retained enchantments are preserved";
    case ActionType::DoubleCorruption:
        return "Double corruption is unavailable: the influenced reforge, socket/link and exceptional-base laws are unresolved";
    default: return nullptr;
    }
}
}

pc_result pc_action_memory_interaction(
        int32_t action, pc_memory_interaction* out) {
    if (!out || action < PC_ACTION_TRANSMUTE || action > PC_ACTION_DOUBLE_CORRUPTION)
        return PC_RESULT_INVALID_ARGUMENT;
    *out = {-1, -1, -1, 0,
        "Memory-strand crafting is unavailable: consumption distribution, tier bias and interaction timing are unresolved"};
    switch (action) {
    case PC_ACTION_FOSSIL:
    case PC_ACTION_HARVEST_REFORGE:
    case PC_ACTION_HARVEST_AUGMENT:
    case PC_ACTION_HARVEST_RESIST:
        out->consumes_strands = 0;
        out->unavailable_reason = "This action does not consume strands, but its interaction with tier bias is unresolved";
        break;
    case PC_ACTION_REMEMBRANCE:
        out->observes_strands = 1;
        out->consumes_strands = 0;
        out->unavailable_reason = "Remembrance is unavailable: the distribution of the replacement strand count is unresolved";
        break;
    case PC_ACTION_UNRAVELLING:
        out->observes_strands = 1;
        out->consumes_strands = 1;
        out->reads_before_consumption = 1;
        out->unavailable_reason = "Unravelling is unavailable: upgrade probabilities and destination-tier law are unresolved";
        break;
    default: break;
    }
    return PC_RESULT_OK;
}
