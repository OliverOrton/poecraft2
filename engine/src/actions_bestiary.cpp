#include "engine_internal.hpp"

#include "poecraft/item_state.h"

namespace poecraft {

namespace {

BestiaryActionOutcome refused(BestiaryRefusalReason reason,
                              const BestiaryCraftState& state) {
    BestiaryActionOutcome outcome;
    outcome.refusal = reason;
    outcome.output_checkpoint_count = state.checkpoint_active ? 1 : 0;
    return outcome;
}

BestiaryRefusalReason common_refusal(
    const BestiaryActionDescriptor& action,
    const BestiaryCraftState& state) {
    if ((action.forbidden_item_flags & PC_ITEM_CORRUPTED) != 0 &&
        (state.item.item_flags & PC_ITEM_CORRUPTED) != 0) {
        return BestiaryRefusalReason::ItemCorrupted;
    }
    if ((action.forbidden_item_flags & PC_ITEM_MIRRORED) != 0 &&
        (state.item.item_flags & PC_ITEM_MIRRORED) != 0) {
        return BestiaryRefusalReason::ItemMirrored;
    }
    const std::uint8_t rarity_bit =
        state.item.rarity < 8 ? static_cast<std::uint8_t>(1u << state.item.rarity)
                              : 0;
    if ((action.rarity_mask & rarity_bit) == 0) {
        return BestiaryRefusalReason::RequiresMagicItem;
    }
    if (action.checkpoint_requirement ==
            BestiaryCheckpointRequirement::Absent &&
        state.checkpoint_active) {
        return BestiaryRefusalReason::CheckpointAlreadyExists;
    }
    if (action.checkpoint_requirement ==
            BestiaryCheckpointRequirement::Present &&
        !state.checkpoint_active) {
        return BestiaryRefusalReason::CheckpointMissing;
    }
    if (action.identity_requirement ==
            BestiaryIdentityRequirement::SameItem &&
        state.checkpoint_bound_identity != state.live_item_identity) {
        return BestiaryRefusalReason::CheckpointBoundToDifferentItem;
    }
    return BestiaryRefusalReason::None;
}

} // namespace

BestiaryActionOutcome apply_bestiary_action(
    const DataImpl& data,
    BestiaryCraftState& state,
    const std::uint32_t action_index) {
    if (action_index >= data.bestiary_actions.size()) {
        return refused(BestiaryRefusalReason::UnknownAction, state);
    }
    const BestiaryActionDescriptor& action =
        data.bestiary_actions[action_index];
    const BestiaryRefusalReason reason = common_refusal(action, state);
    if (reason != BestiaryRefusalReason::None) {
        return refused(reason, state);
    }

    BestiaryCraftState next = state;
    BestiaryActionOutcome outcome;
    outcome.applied = true;
    outcome.consumed_price_keys = action.cost_keys;
    if (action.checkpoint_effect == BestiaryCheckpointEffect::Create) {
        next.checkpoint = next.item;
        next.checkpoint_active = true;
        next.checkpoint_bound_identity = next.live_item_identity;
        outcome.output_checkpoint_count = 1;
    } else {
        next.item = next.checkpoint;
        next.checkpoint_active = false;
        next.checkpoint_bound_identity = 0;
        next.checkpoint = {};
        outcome.output_checkpoint_count = 0;
        outcome.consumed_checkpoint_count = 1;
    }
    state = next;
    return outcome;
}

BestiaryCalculation calculate_bestiary_action(
    const DataImpl& data,
    const BestiaryCraftState& state,
    const std::uint32_t action_index) {
    BestiaryCalculation calculation;
    calculation.successor = state;
    calculation.outcome = apply_bestiary_action(
        data, calculation.successor, action_index);
    return calculation;
}

} // namespace poecraft
