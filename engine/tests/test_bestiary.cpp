#include "tests.hpp"

#include "../src/engine_internal.hpp"
#include "../src/handles_internal.hpp"
#include "poecraft/bestiary.h"
#include "poecraft/item_state.h"

#include <cstring>
#include <memory>

using namespace poecraft;

namespace {

DataImpl contract_data() {
    DataImpl data;
    BestiaryRecipeDescriptor recipe;
    recipe.global_recipe_id = 10;
    recipe.id = "bestiary:imprint";
    recipe.display_name = "Imprint";
    recipe.emulator_available = true;
    recipe.calculator_available = true;
    recipe.strategy_builder_available = true;
    recipe.solver_available = true;
    data.bestiary_recipes = {recipe};
    BestiaryActionDescriptor create;
    create.global_action_id = 20;
    create.global_recipe_id = 10;
    create.id = "bestiary:imprint";
    create.display_name = "Create Imprint";
    create.operation = BestiaryOperationKind::Create;
    create.rarity_mask = 1u << PC_RARITY_MAGIC;
    create.forbidden_item_flags = PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED;
    create.checkpoint_requirement = BestiaryCheckpointRequirement::Absent;
    create.checkpoint_effect = BestiaryCheckpointEffect::Create;
    create.identity_requirement = BestiaryIdentityRequirement::CurrentItem;
    create.cost_keys = {
        "beast:craicic-croaker", "beast:rare", "beast:rare", "beast:rare"};
    BestiaryActionDescriptor restore;
    restore.global_action_id = 21;
    restore.global_recipe_id = 10;
    restore.id = "bestiary:restore_imprint";
    restore.display_name = "Restore Imprint";
    restore.operation = BestiaryOperationKind::Restore;
    restore.rarity_mask = 0x7;
    restore.forbidden_item_flags = PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED;
    restore.checkpoint_requirement = BestiaryCheckpointRequirement::Present;
    restore.checkpoint_effect = BestiaryCheckpointEffect::Consume;
    restore.identity_requirement = BestiaryIdentityRequirement::SameItem;
    data.bestiary_actions = {create, restore};
    data.bestiary_recipe_by_id.emplace("bestiary:imprint", 0);
    data.bestiary_action_by_id.emplace("bestiary:imprint", 0);
    data.bestiary_action_by_id.emplace("bestiary:restore_imprint", 1);
    data.count_bestiary_actions = 2;
    return data;
}

bool same_item(const pc_item_state& left, const pc_item_state& right) {
    return std::memcmp(&left, &right, sizeof(pc_item_state)) == 0;
}

BestiaryCraftState representative_state() {
    BestiaryCraftState state;
    state.live_item_identity = 101;
    pc_item_clear(&state.item);
    state.item.rarity = PC_RARITY_MAGIC;
    state.item.quality = 29;
    state.item.item_flags = PC_ITEM_SPLIT | PC_ITEM_SYNTHESISED;
    state.item.generic_influence_bits = 3;
    state.item.searing_exarch_tier = 2;
    state.item.eater_of_worlds_tier = 1;
    state.item.socket_count = 6;
    state.item.socket_colors[0] = 1;
    state.item.socket_colors[1] = 2;
    state.item.link_mask = 31;
    pc_mod_slot* prefix = nullptr;
    pc_item_add_mod(
        &state.item, PC_SIDE_PREFIX, 7, 11,
        PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_FRACTURED, &prefix);
    prefix->roll_count = 2;
    prefix->rolls[0] = 17;
    prefix->rolls[1] = 23;
    state.item.implicit_count = 1;
    state.item.implicits[0].mod_id = 9;
    state.item.implicits[0].flags = PC_MOD_SLOT_SYNTH;
    state.item.implicits[0].roll_count = 1;
    state.item.implicits[0].rolls[0] = 41;
    state.item.enchantment_count = 1;
    state.item.enchantments[0].mod_id = 12;
    state.item.enchantments[0].roll_count = 1;
    state.item.enchantments[0].rolls[0] = 8;
    return state;
}

} // namespace

void run_bestiary_tests() {
    const DataImpl data = contract_data();

    BestiaryCraftState state = representative_state();
    const pc_item_state original = state.item;
    const BestiaryCalculation calculated =
        calculate_bestiary_action(data, state, 0);
    PC_CHECK(calculated.outcome.applied);
    PC_CHECK(calculated.outcome.consumed_price_keys.size() == 4);
    PC_CHECK(same_item(calculated.successor.item, original));
    PC_CHECK(calculated.successor.checkpoint_active);
    PC_CHECK(same_item(calculated.successor.checkpoint, original));
    PC_CHECK(calculated.successor.checkpoint_bound_identity == 101);

    const BestiaryActionOutcome created =
        apply_bestiary_action(data, state, 0);
    PC_CHECK(created.applied);
    PC_CHECK(same_item(state.item, calculated.successor.item));
    PC_CHECK(same_item(state.checkpoint, calculated.successor.checkpoint));
    PC_CHECK(state.checkpoint_active);

    const BestiaryCraftState before_second = state;
    const BestiaryActionOutcome second =
        apply_bestiary_action(data, state, 0);
    PC_CHECK(!second.applied);
    PC_CHECK(second.refusal == BestiaryRefusalReason::CheckpointAlreadyExists);
    PC_CHECK(same_item(state.item, before_second.item));
    PC_CHECK(same_item(state.checkpoint, before_second.checkpoint));

    state.item.rarity = PC_RARITY_RARE;
    state.item.quality = 1;
    state.item.prefix_count = 0;
    state.item.implicit_count = 0;
    const BestiaryCalculation restore_calc =
        calculate_bestiary_action(data, state, 1);
    const BestiaryActionOutcome restored =
        apply_bestiary_action(data, state, 1);
    PC_CHECK(restored.applied);
    PC_CHECK(restored.consumed_price_keys.empty());
    PC_CHECK(restored.consumed_checkpoint_count == 1);
    PC_CHECK(same_item(state.item, original));
    PC_CHECK(!state.checkpoint_active);
    PC_CHECK(same_item(state.item, restore_calc.successor.item));

    BestiaryCraftState identical = representative_state();
    PC_CHECK(apply_bestiary_action(data, identical, 0).applied);
    PC_CHECK(apply_bestiary_action(data, identical, 1).applied);
    PC_CHECK(!identical.checkpoint_active);
    PC_CHECK(same_item(identical.item, original));

    BestiaryCraftState wrong_rarity = representative_state();
    wrong_rarity.item.rarity = PC_RARITY_RARE;
    const BestiaryCraftState wrong_before = wrong_rarity;
    const auto wrong = apply_bestiary_action(data, wrong_rarity, 0);
    PC_CHECK(!wrong.applied);
    PC_CHECK(wrong.refusal == BestiaryRefusalReason::RequiresMagicItem);
    PC_CHECK(same_item(wrong_rarity.item, wrong_before.item));
    PC_CHECK(!wrong_rarity.checkpoint_active);

    for (const std::uint8_t flag : {static_cast<std::uint8_t>(PC_ITEM_CORRUPTED),
                                    static_cast<std::uint8_t>(PC_ITEM_MIRRORED)}) {
        BestiaryCraftState forbidden = representative_state();
        forbidden.item.item_flags |= flag;
        const auto outcome = apply_bestiary_action(data, forbidden, 0);
        PC_CHECK(!outcome.applied);
        PC_CHECK(outcome.consumed_price_keys.empty());
        PC_CHECK(!forbidden.checkpoint_active);
    }

    BestiaryCraftState missing = representative_state();
    const auto missing_result = apply_bestiary_action(data, missing, 1);
    PC_CHECK(!missing_result.applied);
    PC_CHECK(missing_result.refusal == BestiaryRefusalReason::CheckpointMissing);

    BestiaryCraftState different = representative_state();
    PC_CHECK(apply_bestiary_action(data, different, 0).applied);
    different.live_item_identity = 202;
    const pc_item_state changed_item = different.item;
    const pc_item_state saved = different.checkpoint;
    const auto different_result = apply_bestiary_action(data, different, 1);
    PC_CHECK(!different_result.applied);
    PC_CHECK(different_result.refusal ==
             BestiaryRefusalReason::CheckpointBoundToDifferentItem);
    PC_CHECK(same_item(different.item, changed_item));
    PC_CHECK(same_item(different.checkpoint, saved));
    PC_CHECK(different.checkpoint_active);
    auto public_data = std::make_shared<DataImpl>(contract_data());
    pc_data public_handle{public_data};
    pc_error_info error;
    pc_error_info_init(&error);
    std::uint32_t action_count = 0;
    PC_CHECK(pc_bestiary_get_action_count(
                 &public_handle, &action_count, &error) == PC_RESULT_OK);
    PC_CHECK(action_count == 2);
    pc_bestiary_action_info action_info{};
    PC_CHECK(pc_bestiary_get_action_info(
                 &public_handle, 0, &action_info, &error) == PC_RESULT_OK);
    PC_CHECK(std::strcmp(action_info.action_id, "bestiary:imprint") == 0);
    PC_CHECK(std::strcmp(action_info.recipe_id, "bestiary:imprint") == 0);
    PC_CHECK(std::strcmp(action_info.family_display_name, "Imprint") == 0);
    PC_CHECK(action_info.cost_key_count == 4);
    PC_CHECK(std::strcmp(action_info.cost_keys[0],
                         "beast:craicic-croaker") == 0);
    PC_CHECK(std::strcmp(action_info.cost_keys[1], "beast:rare") == 0);
    PC_CHECK(std::strcmp(action_info.cost_keys[2], "beast:rare") == 0);
    PC_CHECK(std::strcmp(action_info.cost_keys[3], "beast:rare") == 0);

    pc_bestiary_solver_option_info option_info{};
    PC_CHECK(pc_bestiary_get_solver_option_info(
                 &public_handle, 0, &option_info, &error) == PC_RESULT_OK);
    PC_CHECK(std::strcmp(option_info.option_id, "imprint_retry") == 0);
    PC_CHECK(std::strcmp(
                 option_info.checkpoint_restriction_key,
                 "magic_checkpoint_carrier") == 0);
    PC_CHECK(option_info.checkpoint_rarity == PC_RARITY_MAGIC);
    PC_CHECK(option_info.automatic_discovery == 1);
    PC_CHECK(option_info.state_local_discovery == 1);
    PC_CHECK(option_info.resource_bounded == 1);

    const BestiaryCraftState public_source = representative_state();
    pc_bestiary_craft_state public_state{};
    PC_CHECK(pc_bestiary_state_init(
                 &public_source.item, public_source.live_item_identity,
                 &public_state, &error) == PC_RESULT_OK);
    pc_bestiary_action_request create_request{
        sizeof(pc_bestiary_action_request), PC_ABI_VERSION,
        "bestiary:imprint"};
    pc_bestiary_calculation public_calc{};
    PC_CHECK(pc_bestiary_calculate_action(
                 &public_handle, &public_state, &create_request,
                 &public_calc, &error) == PC_RESULT_OK);
    PC_CHECK(public_calc.outcome_count == 1);
    PC_CHECK(public_calc.probability == 1.0);
    PC_CHECK(public_calc.result.applied == 1);
    PC_CHECK(public_calc.result.consumed_price_key_count == 4);
    PC_CHECK(public_calc.successor.checkpoint_present == 1);
    PC_CHECK(public_state.checkpoint_present == 0);

    pc_bestiary_action_result public_result{};
    PC_CHECK(pc_bestiary_apply_action(
                 &public_handle, &public_state, &create_request,
                 &public_result, &error) == PC_RESULT_OK);
    PC_CHECK(public_result.applied == 1);
    PC_CHECK(public_state.checkpoint_present == 1);
    const pc_bestiary_craft_state public_before_refusal = public_state;
    PC_CHECK(pc_bestiary_apply_action(
                 &public_handle, &public_state, &create_request,
                 &public_result, &error) == PC_RESULT_OK);
    PC_CHECK(public_result.applied == 0);
    PC_CHECK(public_result.refusal ==
             PC_BESTIARY_REFUSAL_CHECKPOINT_ALREADY_EXISTS);
    PC_CHECK(std::strcmp(public_result.refusal_key,
                         "checkpoint_already_exists") == 0);
    PC_CHECK(public_result.consumed_price_key_count == 0);
    PC_CHECK(std::memcmp(&public_state, &public_before_refusal,
                         sizeof(public_state)) == 0);
}
