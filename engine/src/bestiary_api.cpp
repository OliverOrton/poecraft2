#include "poecraft/bestiary.h"

#include "engine_internal.hpp"
#include "handles_internal.hpp"

#include <cstdio>
#include <cstring>

namespace {

void set_public_error(pc_error_info* error, pc_result code,
                      const char* message) {
    if (error == nullptr) return;
    error->struct_size = static_cast<std::uint32_t>(sizeof(pc_error_info));
    error->abi_version = PC_ABI_VERSION;
    error->code = static_cast<std::int32_t>(code);
    std::snprintf(error->message, sizeof(error->message), "%s",
                  message ? message : "");
}

void clear_public_error(pc_error_info* error) {
    set_public_error(error, PC_RESULT_OK, "");
}

bool input_struct_ok(std::uint32_t struct_size, std::uint32_t abi_version,
                     std::size_t minimum_size, pc_error_info* error) {
    if (abi_version != PC_ABI_VERSION) {
        set_public_error(error, PC_RESULT_INVALID_ARGUMENT,
                         "incompatible struct abi_version");
        return false;
    }
    if (struct_size < minimum_size) {
        set_public_error(error, PC_RESULT_INVALID_ARGUMENT,
                         "input struct too small");
        return false;
    }
    return true;
}

const poecraft::BestiaryRecipeDescriptor* recipe_for(
    const poecraft::DataImpl& data,
    const poecraft::BestiaryActionDescriptor& action) {
    for (const auto& recipe : data.bestiary_recipes) {
        if (recipe.global_recipe_id == action.global_recipe_id) return &recipe;
    }
    return nullptr;
}

const char* refusal_key(poecraft::BestiaryRefusalReason reason) {
    switch (reason) {
    case poecraft::BestiaryRefusalReason::None: return "none";
    case poecraft::BestiaryRefusalReason::UnknownAction:
        return "unknown_action";
    case poecraft::BestiaryRefusalReason::RequiresMagicItem:
        return "requires_magic_item";
    case poecraft::BestiaryRefusalReason::ItemCorrupted:
        return "item_corrupted";
    case poecraft::BestiaryRefusalReason::ItemMirrored:
        return "item_mirrored";
    case poecraft::BestiaryRefusalReason::CheckpointAlreadyExists:
        return "checkpoint_already_exists";
    case poecraft::BestiaryRefusalReason::CheckpointMissing:
        return "checkpoint_missing";
    case poecraft::BestiaryRefusalReason::CheckpointBoundToDifferentItem:
        return "checkpoint_bound_to_different_item";
    }
    return "unknown_action";
}

const char* refusal_reason(poecraft::BestiaryRefusalReason reason) {
    switch (reason) {
    case poecraft::BestiaryRefusalReason::None: return "";
    case poecraft::BestiaryRefusalReason::UnknownAction:
        return "The Bestiary action is unknown.";
    case poecraft::BestiaryRefusalReason::RequiresMagicItem:
        return "Imprint creation requires a magic item.";
    case poecraft::BestiaryRefusalReason::ItemCorrupted:
        return "The live item is corrupted.";
    case poecraft::BestiaryRefusalReason::ItemMirrored:
        return "The live item is mirrored.";
    case poecraft::BestiaryRefusalReason::CheckpointAlreadyExists:
        return "The live item already has an active Imprint checkpoint.";
    case poecraft::BestiaryRefusalReason::CheckpointMissing:
        return "The live item has no active Imprint checkpoint.";
    case poecraft::BestiaryRefusalReason::CheckpointBoundToDifferentItem:
        return "The Imprint checkpoint is bound to a different live item.";
    }
    return "The Bestiary action is unknown.";
}

poecraft::BestiaryCraftState to_internal(
    const pc_bestiary_craft_state& state) {
    poecraft::BestiaryCraftState result;
    result.item = state.item;
    result.live_item_identity = state.live_item_identity;
    result.checkpoint_active = state.checkpoint_present != 0;
    result.checkpoint_bound_identity = state.checkpoint_bound_identity;
    result.checkpoint = state.checkpoint;
    return result;
}

void from_internal(const poecraft::BestiaryCraftState& source,
                   pc_bestiary_craft_state& target) {
    std::memset(&target, 0, sizeof(target));
    target.struct_size = sizeof(target);
    target.abi_version = PC_ABI_VERSION;
    target.item = source.item;
    target.live_item_identity = source.live_item_identity;
    target.checkpoint_present = source.checkpoint_active ? 1 : 0;
    target.checkpoint_bound_identity = source.checkpoint_bound_identity;
    target.checkpoint = source.checkpoint;
}

bool action_costs_fit(const poecraft::BestiaryActionDescriptor& action,
                      pc_error_info* error) {
    if (action.cost_keys.size() <= PC_BESTIARY_MAX_COST_KEYS) return true;
    set_public_error(error, PC_RESULT_CAPACITY_EXCEEDED,
                     "Bestiary action cost vector exceeds public capacity");
    return false;
}

void fill_result(const poecraft::BestiaryActionDescriptor& action,
                 const poecraft::BestiaryActionOutcome& outcome,
                 const poecraft::BestiaryCraftState& successor,
                 pc_bestiary_action_result& result) {
    std::memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = PC_ABI_VERSION;
    result.action_id = action.id.c_str();
    result.applied = outcome.applied ? 1 : 0;
    result.refusal = static_cast<std::int32_t>(outcome.refusal);
    result.refusal_key = refusal_key(outcome.refusal);
    result.refusal_reason = refusal_reason(outcome.refusal);
    result.cost_key_count = static_cast<std::uint32_t>(action.cost_keys.size());
    for (std::size_t i = 0; i < action.cost_keys.size(); ++i) {
        result.cost_keys[i] = action.cost_keys[i].c_str();
    }
    result.consumed_price_key_count =
        static_cast<std::uint32_t>(outcome.consumed_price_keys.size());
    for (std::size_t i = 0; i < outcome.consumed_price_keys.size(); ++i) {
        result.consumed_price_keys[i] = action.cost_keys[i].c_str();
    }
    result.output_item_count = outcome.output_item_count;
    result.output_checkpoint_count = outcome.output_checkpoint_count;
    result.consumed_checkpoint_count = outcome.consumed_checkpoint_count;
    result.checkpoint_present = successor.checkpoint_active ? 1 : 0;
}

pc_result resolve_action(pc_data_handle data,
                         const pc_bestiary_action_request* request,
                         std::uint32_t& out_index,
                         pc_error_info* error) {
    if (data == nullptr || request == nullptr || request->action_id == nullptr) {
        set_public_error(error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!input_struct_ok(request->struct_size, request->abi_version,
                         sizeof(pc_bestiary_action_request), error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto found = data->impl->bestiary_action_by_id.find(request->action_id);
    if (found == data->impl->bestiary_action_by_id.end()) {
        set_public_error(error, PC_RESULT_NOT_FOUND,
                         "Bestiary action id not found");
        return PC_RESULT_NOT_FOUND;
    }
    out_index = found->second;
    if (!action_costs_fit(data->impl->bestiary_actions.at(out_index), error)) {
        return PC_RESULT_CAPACITY_EXCEEDED;
    }
    return PC_RESULT_OK;
}

} // namespace

extern "C" {

pc_result pc_bestiary_state_init(
    const pc_item_state* item,
    const std::uint64_t live_item_identity,
    pc_bestiary_craft_state* out_state,
    pc_error_info* out_error) {
    if (item == nullptr || out_state == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    poecraft::BestiaryCraftState state;
    state.item = *item;
    state.live_item_identity = live_item_identity;
    from_internal(state, *out_state);
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_get_action_count(
    pc_data_handle data,
    std::uint32_t* out_count,
    pc_error_info* out_error) {
    if (data == nullptr || out_count == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_count = static_cast<std::uint32_t>(data->impl->bestiary_actions.size());
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_get_action_info(
    pc_data_handle data,
    const std::uint32_t action_index,
    pc_bestiary_action_info* out_info,
    pc_error_info* out_error) {
    if (data == nullptr || out_info == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (action_index >= data->impl->bestiary_actions.size()) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                         "Bestiary action index out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& action = data->impl->bestiary_actions[action_index];
    if (!action_costs_fit(action, out_error)) {
        return PC_RESULT_CAPACITY_EXCEEDED;
    }
    const auto* recipe = recipe_for(*data->impl, action);
    if (recipe == nullptr) {
        set_public_error(out_error, PC_RESULT_INTERNAL_ERROR,
                         "Bestiary action recipe is missing");
        return PC_RESULT_INTERNAL_ERROR;
    }
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->struct_size = sizeof(*out_info);
    out_info->abi_version = PC_ABI_VERSION;
    out_info->action_index = action_index;
    out_info->global_action_id = action.global_action_id;
    out_info->global_recipe_id = action.global_recipe_id;
    out_info->action_id = action.id.c_str();
    out_info->recipe_id = recipe->id.c_str();
    out_info->family_display_name = recipe->display_name.c_str();
    out_info->display_name = action.display_name.c_str();
    out_info->operation = static_cast<std::int32_t>(action.operation);
    out_info->transition_kind = action.transition;
    out_info->emulator_available = recipe->emulator_available ? 1 : 0;
    out_info->calculator_available = recipe->calculator_available ? 1 : 0;
    out_info->strategy_builder_available =
        recipe->strategy_builder_available ? 1 : 0;
    out_info->solver_available = recipe->solver_available ? 1 : 0;
    out_info->checkpoint_requirement =
        static_cast<std::int32_t>(action.checkpoint_requirement);
    out_info->checkpoint_effect =
        static_cast<std::int32_t>(action.checkpoint_effect);
    out_info->identity_requirement =
        static_cast<std::int32_t>(action.identity_requirement);
    out_info->cost_key_count =
        static_cast<std::uint32_t>(action.cost_keys.size());
    for (std::size_t i = 0; i < action.cost_keys.size(); ++i) {
        out_info->cost_keys[i] = action.cost_keys[i].c_str();
    }
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_find_action(
    pc_data_handle data,
    const char* action_id,
    std::uint32_t* out_index,
    pc_error_info* out_error) {
    if (data == nullptr || action_id == nullptr || out_index == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto found = data->impl->bestiary_action_by_id.find(action_id);
    if (found == data->impl->bestiary_action_by_id.end()) {
        set_public_error(out_error, PC_RESULT_NOT_FOUND,
                         "Bestiary action id not found");
        return PC_RESULT_NOT_FOUND;
    }
    *out_index = found->second;
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_apply_action(
    pc_data_handle data,
    pc_bestiary_craft_state* state,
    const pc_bestiary_action_request* request,
    pc_bestiary_action_result* out_result,
    pc_error_info* out_error) {
    if (state == nullptr || out_result == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!input_struct_ok(state->struct_size, state->abi_version,
                         sizeof(pc_bestiary_craft_state), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::uint32_t action_index = 0;
    const pc_result resolved =
        resolve_action(data, request, action_index, out_error);
    if (resolved != PC_RESULT_OK) return resolved;
    auto internal = to_internal(*state);
    const auto outcome = poecraft::apply_bestiary_action(
        *data->impl, internal, action_index);
    from_internal(internal, *state);
    fill_result(data->impl->bestiary_actions[action_index], outcome, internal,
                *out_result);
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_calculate_action(
    pc_data_handle data,
    const pc_bestiary_craft_state* state,
    const pc_bestiary_action_request* request,
    pc_bestiary_calculation* out_calculation,
    pc_error_info* out_error) {
    if (state == nullptr || out_calculation == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!input_struct_ok(state->struct_size, state->abi_version,
                         sizeof(pc_bestiary_craft_state), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::uint32_t action_index = 0;
    const pc_result resolved =
        resolve_action(data, request, action_index, out_error);
    if (resolved != PC_RESULT_OK) return resolved;
    const auto calculation = poecraft::calculate_bestiary_action(
        *data->impl, to_internal(*state), action_index);
    std::memset(out_calculation, 0, sizeof(*out_calculation));
    out_calculation->struct_size = sizeof(*out_calculation);
    out_calculation->abi_version = PC_ABI_VERSION;
    out_calculation->outcome_count = 1;
    out_calculation->probability = 1.0;
    from_internal(calculation.successor, out_calculation->successor);
    fill_result(data->impl->bestiary_actions[action_index],
                calculation.outcome, calculation.successor,
                out_calculation->result);
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_get_solver_option_count(
    pc_data_handle data,
    std::uint32_t* out_count,
    pc_error_info* out_error) {
    if (data == nullptr || out_count == nullptr) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const bool available =
        data->impl->bestiary_action_by_id.count("bestiary:imprint") != 0 &&
        data->impl->bestiary_action_by_id.count(
            "bestiary:restore_imprint") != 0;
    *out_count = available ? 1u : 0u;
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_bestiary_get_solver_option_info(
    pc_data_handle data,
    const std::uint32_t option_index,
    pc_bestiary_solver_option_info* out_info,
    pc_error_info* out_error) {
    std::uint32_t count = 0;
    const pc_result count_result =
        pc_bestiary_get_solver_option_count(data, &count, out_error);
    if (count_result != PC_RESULT_OK) return count_result;
    if (out_info == nullptr || option_index >= count) {
        set_public_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                         "Bestiary solver option index out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->struct_size = sizeof(*out_info);
    out_info->abi_version = PC_ABI_VERSION;
    out_info->option_index = option_index;
    out_info->option_id = "imprint_retry";
    out_info->display_name = "Automatic Imprint stages";
    out_info->checkpoint_restriction_key = "magic_checkpoint_carrier";
    out_info->checkpoint_restriction =
        "Discovered at reachable magic carriers where native checkpoint "
        "creation is legal; the final goal may have any rarity.";
    out_info->checkpoint_rarity = PC_RARITY_MAGIC;
    out_info->automatic_discovery = 1;
    out_info->state_local_discovery = 1;
    out_info->resource_bounded = 1;
    clear_public_error(out_error);
    return PC_RESULT_OK;
}

} // extern "C"
