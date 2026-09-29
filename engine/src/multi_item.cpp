#include "multi_item.hpp"
#include "handles_internal.hpp"
#include "poecraft/multi_item.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <stdexcept>

namespace poecraft {
namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::invalid_argument(reason); }
void validate_resources(const std::vector<CraftResource>& resources) {
    std::set<std::string> ids, roles;
    for (const auto& resource : resources) {
        require(!resource.identity.empty() && ids.insert(resource.identity).second,
                "Multi-item resources require distinct nonempty identities; self-donation is invalid");
        require(resource.session != nullptr, "Resource session is missing");
        require(resource.item.memory_strands <= 100 && resource.item.lifecycle <= PC_ITEM_DESTROYED,
                "Resource has invalid memory or lifecycle state");
        if (!resource.role.empty()) require(roles.insert(resource.role).second, "Duplicate input role");
    }
}
}

void commit_craft_transaction(std::vector<CraftResource>& resources,
                              const CraftTransaction& transaction) {
    validate_resources(resources);
    auto next = resources;
    std::set<std::string> changed;
    for (const auto& change : transaction.changes) {
        require(changed.insert(change.identity).second, "Transaction repeats a resource identity");
        auto it = std::find_if(next.begin(), next.end(), [&](const auto& r) { return r.identity == change.identity; });
        require(change.after.identity == change.identity, "Transaction changed an item's identity");
        if (change.effect == PC_RESOURCE_CREATED) {
            require(it == next.end(), "Created resource identity already exists");
            next.push_back(change.after);
        } else {
            require(it != next.end(), "Transaction resource is missing");
            require(change.before.identity == change.identity && it->role == change.before.role &&
                    it->session == change.before.session &&
                    std::memcmp(&it->item, &change.before.item, sizeof(pc_item_state)) == 0,
                    "Transaction is stale; an input changed before commit");
            require(change.effect == PC_RESOURCE_RETAINED || change.effect == PC_RESOURCE_CHANGED ||
                    change.effect == PC_RESOURCE_CONSUMED, "Unknown resource effect");
            if (change.effect == PC_RESOURCE_CONSUMED)
                require(it->item.lifecycle == PC_ITEM_LIVE && change.after.item.lifecycle == PC_ITEM_CONSUMED,
                        "Consumed resource must change from live to absent");
            if (change.effect == PC_RESOURCE_RETAINED)
                require(it->session == change.after.session &&
                        std::memcmp(&it->item, &change.after.item, sizeof(pc_item_state)) == 0,
                        "Retained resources cannot change item state");
            *it = change.after;
        }
    }
    validate_resources(next);
    resources.swap(next);
}

CraftTransaction prepare_multi_item_craft(ActionContextImpl& context,
        const std::string& action, const std::vector<CraftResource>& resources) {
    validate_resources(resources);
    require(action == "awakener", "Unknown multi-item craft");
    require(resources.size() == 2, "Awakener requires exactly donor and receiver roles");
    const CraftResource* donor = nullptr;
    const CraftResource* receiver = nullptr;
    for (const auto& r : resources) {
        if (r.role == "donor") donor = &r;
        if (r.role == "receiver") receiver = &r;
    }
    require(donor && receiver, "Awakener requires donor and receiver roles");
    require(context.session == receiver->session, "Action context must belong to the receiver session");
    const auto& a = *donor->session->data;
    const auto& b = *receiver->session->data;
    require((donor->session->data == receiver->session->data) ||
            (!a.artifact_game_data_hash.empty() && a.artifact_game_data_hash == b.artifact_game_data_hash &&
             a.artifact_strings_hash == b.artifact_strings_hash), "Resource data identities are incompatible");
    require(a.base_item_class_id[donor->session->base_index] == b.base_item_class_id[receiver->session->base_index],
            "Awakener input item classes differ");
    const auto saved_rng = context.rng;
    try {
        CraftTransaction result;
        auto consumed = *donor;
        consumed.item.lifecycle = PC_ITEM_CONSUMED;
        auto updated = *receiver;
        updated.item = awaken_item(context, *donor->session, donor->item, receiver->item);
        result.changes = {{donor->identity, PC_RESOURCE_CONSUMED, *donor, consumed},
                          {receiver->identity, PC_RESOURCE_CHANGED, *receiver, updated}};
        result.consumed_price_keys = {"awakener"};
        return result;
    } catch (...) { context.rng = saved_rng; throw; }
}
}

pc_result pc_multi_item_apply(pc_action_context_handle context, const char* action,
        pc_craft_resource* resources, uint32_t count,
        pc_multi_item_result* result, pc_error_info* error) {
    const auto fail = [&](const char* reason, pc_result code) {
        if (error) { pc_error_info_init(error); error->code = code;
            std::snprintf(error->message, sizeof(error->message), "%s", reason); }
        return code;
    };
    if (!context || !action || !resources || !result || !count || count > PC_MAX_CRAFT_RESOURCES)
        return fail("Invalid multi-item arguments", PC_RESULT_INVALID_ARGUMENT);
    const auto saved_rng = context->impl->rng;
    try {
        std::vector<poecraft::CraftResource> inputs;
        std::set<pc_item_state*> pointers;
        for (uint32_t i = 0; i < count; ++i) {
            const auto& r = resources[i];
            if (!r.identity || !r.role || !r.session || !r.item || !pointers.insert(r.item).second)
                throw std::invalid_argument("Null or aliased multi-item input");
            inputs.push_back({r.identity, r.role, r.session->impl, *r.item});
        }
        auto transaction = poecraft::prepare_multi_item_craft(*context->impl, action, inputs);
        poecraft::commit_craft_transaction(inputs, transaction);
        pc_multi_item_result next{};
        next.struct_size = sizeof(next); next.abi_version = PC_ABI_VERSION;
        next.resource_count = count; next.consumed_price_key = "awakener";
        for (uint32_t i = 0; i < count; ++i) {
            auto& change = next.resources[i];
            change.identity = resources[i].identity;
            change.before = *resources[i].item;
            change.after = inputs[i].item;
            change.effect = change.after.lifecycle == PC_ITEM_CONSUMED ? PC_RESOURCE_CONSUMED : PC_RESOURCE_CHANGED;
        }
        for (uint32_t i = 0; i < count; ++i) *resources[i].item = next.resources[i].after;
        *result = next;
        if (error) pc_error_info_init(error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        context->impl->rng = saved_rng;
        return fail(ex.what(), PC_RESULT_UNSUPPORTED_FEATURE);
    }
}
