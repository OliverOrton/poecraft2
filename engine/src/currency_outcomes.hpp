#pragma once
#include "engine_internal.hpp"

namespace poecraft {
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
