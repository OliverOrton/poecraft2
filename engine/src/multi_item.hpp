#pragma once
#include "engine_internal.hpp"

namespace poecraft {
struct CraftResource {
    std::string identity;
    std::string role;
    std::shared_ptr<const SessionImpl> session;
    pc_item_state item{};
};
struct ResourceChange {
    std::string identity;
    int effect = 0;
    CraftResource before;
    CraftResource after;
};
struct CraftTransaction {
    std::vector<ResourceChange> changes;
    std::vector<std::string> consumed_price_keys;
};

/* Role-independent atomic commit supports retained/changed/consumed resources
 * and new identities. No acquisition or automatic replacement is implicit. */
void commit_craft_transaction(std::vector<CraftResource>& resources,
                              const CraftTransaction& transaction);
CraftTransaction prepare_multi_item_craft(ActionContextImpl& context,
    const std::string& action, const std::vector<CraftResource>& resources);

/* Sample the established structural Awakener law after transaction preflight.
 * Numerical roll distributions are not yet represented by this engine. */
pc_item_state awaken_item(ActionContextImpl& context,
    const SessionImpl& donor_session, const pc_item_state& donor,
    const pc_item_state& receiver);
}
