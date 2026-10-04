#pragma once
#include "engine_internal.hpp"
#include <functional>

namespace poecraft {
// Qualified configured-cluster actions; unrelated mechanics remain explicit.
inline bool cluster_currency_qualified(ActionType type) {
    switch (type) {
    case ActionType::Transmute: case ActionType::Alteration:
    case ActionType::Augment: case ActionType::Regal: case ActionType::Exalt:
    case ActionType::FoulbornAugment: case ActionType::FoulbornRegal:
    case ActionType::FoulbornExalt: case ActionType::Annul: case ActionType::Scour:
    case ActionType::Alchemy: case ActionType::Chaos: case ActionType::Fossil:
    case ActionType::HarvestReforge: case ActionType::HarvestAugment:
    case ActionType::HarvestResist: case ActionType::RemoveCraftedModifiers: return true;
    default: return false;
    }
}

// Full represented removal laws share the sampled native legality/transformations.
// No abstract materialization or output conditioning; recorded rolls survive.
ActionOutcome visit_full_item_removal_outcomes(ActionContextImpl&,
    const pc_item_state&, const ActionParameters&,
    const std::function<void(const pc_item_state&, long double)>&);

// Concrete cluster outcomes rebuild the native pool after every draw; only
// completed outcomes may be projected into Calculator terminal observations.
ActionOutcome visit_cluster_currency_outcomes(ActionContextImpl&,
    const pc_item_state&, const ActionParameters&,
    const std::function<void(const pc_item_state&, long double)>&,
    std::uint64_t max_work = 2000000,
    const std::function<std::uint32_t(const pc_item_state&)>& terminal_observation = {},
    const std::function<void(std::uint64_t)>& require_scratch_bytes = {});

// Shared preparation for sampled execution and single-action calculation.
struct DominanceChoice {
    int side;
    std::uint8_t index;
    std::uint32_t upgrade;
};
std::vector<DominanceChoice> dominance_choices(
    const SessionImpl&, const pc_item_state&);
pc_item_state dominance_result(const SessionImpl&, const pc_item_state&,
    const DominanceChoice& upgrade, const DominanceChoice& remove);

struct AwakenerChoices {
    std::vector<std::uint32_t> donor, receiver;
};
AwakenerChoices awakener_choices(const SessionImpl& receiver_session,
    const SessionImpl& donor_session, const pc_item_state& donor,
    const pc_item_state& receiver);
pc_item_state awakener_base(const SessionImpl&, const pc_item_state& donor,
    const pc_item_state& receiver, std::uint32_t donor_mod,
    std::uint32_t receiver_mod);

std::vector<std::pair<std::uint32_t, std::uint64_t>> vaal_implicit_weights(
    const SessionImpl&);
pc_item_state vaal_implicit_result(const SessionImpl&, const pc_item_state&,
    std::uint32_t mod, std::uint32_t removed);
std::vector<std::pair<std::uint32_t, std::uint64_t>> eldritch_implicit_weights(
    const SessionImpl&, bool searing, std::uint32_t tier);
bool set_eldritch_implicit(const SessionImpl&, pc_item_state&,
    bool searing, std::uint32_t tier, std::uint32_t mod);
std::vector<std::pair<pc_item_state, long double>> fossil_implicit_outcomes(
    const SessionImpl&, const pc_item_state&, const std::vector<std::uint32_t>& fossils);
}
