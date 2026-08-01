#pragma once

#include "solver_compile_conditions.hpp"

namespace poecraft {
namespace solver {
namespace {

std::string hex_u64(const std::uint64_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (std::size_t index = 0; index < out.size(); ++index) {
        const std::size_t shift = (out.size() - index - 1) * 4;
        out[index] = digits[(value >> shift) & 0xfu];
    }
    return out;
}

std::string stable_key_json(
        const refinement::StableKey& value) {
    std::string out = "[";
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index != 0) out += ',';
        out += "\"" + hex_u64(value[index]) + "\"";
    }
    return out + "]";
}

std::string sorted_u32_json(
        const std::vector<std::uint32_t>& values) {
    std::string out = "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out += ',';
        out += std::to_string(values[index]);
    }
    return out + "]";
}

refinement::StableKey count_observation_membership(
        const AbstractLayout& layout,
        const std::uint32_t mod) {
    refinement::StableKey bits(
        (layout.count_observations.size() + 63) / 64, 0);
    for (std::size_t observation = 0;
         observation < layout.count_observations.size();
         ++observation) {
        const std::vector<std::uint64_t>& mask =
            layout.count_observations[observation].member_mask;
        if (!mask.empty() && pc_bitset_test(mask.data(), mod)) {
            bits[observation / 64] |=
                std::uint64_t{1} << (observation % 64);
        }
    }
    return bits;
}

std::string observation_signature_condition(
        const SessionImpl& session,
        const AbstractLayout& layout,
        const refinement::ObservationRequirement& input_requirement,
        const refinement::FeatureSignature& input_signature) {
    const refinement::ObservationRequirement requirement =
        refinement::canonical_observation_requirement(
            input_requirement);
    const refinement::FeatureSignature signature =
        refinement::canonical_feature_signature(
            input_signature);
    if (requirement != input_requirement ||
        signature != input_signature) {
        gap(
            "refined policy supplied a non-canonical observation "
            "route");
    }

    std::string out =
        "{\"type\":\"observation_signature\",\"version\":" +
        std::to_string(kObservationSignatureConditionVersion) +
        ",\"requirement\":{\"item_features\":" +
        std::to_string(requirement.item_features) +
        ",\"modifier_tag_ids\":" +
        sorted_u32_json(requirement.modifier_tag_ids) +
        ",\"affix_observations\":[";
    for (std::size_t index = 0;
         index < requirement.affix_observations.size(); ++index) {
        if (index != 0) out += ',';
        const RefinementAffixObservation& observation =
            requirement.affix_observations[index];
        out +=
            "{\"features\":" +
            std::to_string(observation.features) +
            ",\"selector\":{\"required_affix_traits\":" +
            std::to_string(
                observation.selector.required_affix_traits) +
            ",\"forbidden_affix_traits\":" +
            std::to_string(
                observation.selector.forbidden_affix_traits) +
            ",\"required_item_traits\":" +
            std::to_string(
                observation.selector.required_item_traits) +
            ",\"forbidden_item_traits\":" +
            std::to_string(
                observation.selector.forbidden_item_traits) +
            ",\"required_tag_ids\":" +
            sorted_u32_json(
                observation.selector.required_tag_ids) +
            "}}";
    }
    out += "]},\"signature\":[";
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (index != 0) out += ',';
        const refinement::FeatureAtom& atom = signature[index];
        out +=
            "{\"feature\":" +
            std::to_string(
                static_cast<std::uint8_t>(atom.feature)) +
            ",\"subject\":" + std::to_string(atom.subject) +
            ",\"value\":" + stable_key_json(atom.value) + "}";
    }
    out += "],\"goal_status_tier_class_by_mod\":[";
    bool first = true;
    const bool goal_context_required =
        std::any_of(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            [](const RefinementAffixObservation& observation) {
                return (
                    observation.features &
                    refinement_feature(
                        RefinementFeature::
                            GoalStatusTierClass)) != 0;
            });
    if (goal_context_required) {
        for (std::uint32_t mod = 0;
             mod < session.mod_count; ++mod) {
            refinement::StableKey value{
                0u,
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Absent)};
            for (std::size_t slot = 0;
                 slot < layout.slots.size(); ++slot) {
                if (!pc_bitset_test(
                        layout.slots[slot].member_mask.data(),
                        mod)) {
                    continue;
                }
                if (value.front() != 0) {
                    gap(
                        "observation-signature goal context has "
                        "overlapping modifier slots");
                }
                value = {
                    slot + 1,
                    static_cast<std::uint8_t>(
                        pc_bitset_test(
                            layout.slots[slot]
                                .satisfying_mask.data(),
                            mod)
                            ? GoalSlotStatus::Satisfied
                            : GoalSlotStatus::
                                  PresentBelowTier)};
            }
            if (value.front() == 0) continue;
            if (!first) out += ',';
            first = false;
            out +=
                "{\"mod_key\":\"" +
                json_escape(mod_key_of(session, mod)) +
                "\",\"value\":" +
                stable_key_json(value) + "}";
        }
    }
    out +=
        "],\"count_observation_count\":" +
        std::to_string(layout.count_observations.size()) +
        ",\"count_observation_membership_by_mod\":[";
    first = true;
    const bool count_context_required =
        std::any_of(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            [](const RefinementAffixObservation& observation) {
                return (
                    observation.features &
                    refinement_feature(
                        RefinementFeature::
                            CountObservationMembership)) != 0;
            });
    if (count_context_required) {
        for (std::uint32_t mod = 0;
             mod < session.mod_count; ++mod) {
            const refinement::StableKey value =
                count_observation_membership(layout, mod);
            if (std::all_of(
                    value.begin(), value.end(),
                    [](const std::uint64_t word) {
                        return word == 0;
                    })) {
                continue;
            }
            if (!first) out += ',';
            first = false;
            out +=
                "{\"mod_key\":\"" +
                json_escape(mod_key_of(session, mod)) +
                "\",\"value\":" +
                stable_key_json(value) + "}";
        }
    }
    return out + "]}";
}

std::string operation_json(const SessionImpl& session,
                           const ActionDescriptor& action) {
    const DataImpl& data = *session.data;
    if (action.synthetic) return "{\"type\":\"restart\"}";
    switch (action.params.type) {
    case ActionType::Essence:
        if (action.params.essence_index >=
            data.essence_key_sids.size()) {
            gap(
                "essence action has no stable session key");
        }
        return "{\"type\":\"essence\",\"essence_key\":\"" +
               json_escape(data.string_at(
                   data.essence_key_sids[action.params.essence_index])) +
               "\"}";
    case ActionType::Fossil: {
        std::string out = "{\"type\":\"fossil\",\"fossils\":[";
        for (std::size_t i = 0; i < action.params.fossil_indices.size();
             ++i) {
            if (action.params.fossil_indices[i] >=
                data.fossil_key_sids.size()) {
                gap(
                    "fossil action has no stable session key");
            }
            if (i > 0) out += ',';
            out += '"';
            out += json_escape(data.string_at(
                data.fossil_key_sids[action.params.fossil_indices[i]]));
            out += '"';
        }
        return out + "]}";
    }
    case ActionType::Bench:
    case ActionType::Unveil:
        return std::string("{\"type\":\"") +
               (action.params.type == ActionType::Bench ? "bench"
                                                        : "unveil") +
               "\",\"mod_key\":\"" +
               json_escape(mod_key_of(session, action.params.mod_id)) +
               "\"}";
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
        return std::string("{\"type\":\"") +
               (action.params.type == ActionType::HarvestReforge
                    ? "harvest_reforge"
                    : "harvest_augment") +
               "\",\"target_tag\":\"" +
               json_escape(data.tag_name_by_id.at(
                   action.params.target_tag_id)) +
               "\"}";
    case ActionType::HarvestResist:
        return "{\"type\":\"harvest_resist\",\"source_tag\":\"" +
               json_escape(
                   data.tag_name_by_id.at(action.params.source_tag_id)) +
               "\",\"target_tag\":\"" +
               json_escape(
                   data.tag_name_by_id.at(action.params.target_tag_id)) +
               "\"}";
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
        return std::string("{\"type\":\"") +
               (action.params.type == ActionType::EldritchEmber
                    ? "eldritch_ember"
                    : "eldritch_ichor") +
               "\",\"tier\":" + std::to_string(action.params.tier) + "}";
    case ActionType::InfluenceExalt:
        return "{\"type\":\"influence_exalt\",\"influence\":\"" +
               json_escape(data.influence_name_by_code.at(
                   static_cast<std::size_t>(
                       action.params.influence_code))) +
               "\"}";
    case ActionType::Transmute:
        return "{\"type\":\"transmute\"}";
    case ActionType::Augment:
        return "{\"type\":\"augment\"}";
    case ActionType::Alteration:
        return "{\"type\":\"alteration\"}";
    case ActionType::Regal:
        return "{\"type\":\"regal\"}";
    case ActionType::Alchemy:
        return "{\"type\":\"alchemy\"}";
    case ActionType::Chaos:
        return "{\"type\":\"chaos\"}";
    case ActionType::Exalt:
        return "{\"type\":\"exalt\"}";
    case ActionType::Annul:
        return "{\"type\":\"annul\"}";
    case ActionType::Scour:
        return "{\"type\":\"scour\"}";
    case ActionType::VeiledChaos:
        return "{\"type\":\"veiled_chaos\"}";
    case ActionType::VeiledExalt:
        return "{\"type\":\"veiled_exalt\"}";
    case ActionType::EldritchExalt:
        return "{\"type\":\"eldritch_exalt\"}";
    case ActionType::EldritchChaos:
        return "{\"type\":\"eldritch_chaos\"}";
    case ActionType::EldritchAnnul:
        return "{\"type\":\"eldritch_annul\"}";
    case ActionType::Fracture:
        return "{\"type\":\"fracture\"}";
    case ActionType::RemoveCraftedModifiers:
        return "{\"type\":\"remove_crafted_modifiers\"}";
    }
    gap("action type has no strategy operation serializer");
}

std::string accounting_roles_json(
    const PlannerOperator& planner,
    std::uint32_t action_index,
    bool fracture_use = false) {
    std::vector<std::string> roles;
    const auto add = [&](const char* role) {
        if (std::find(roles.begin(), roles.end(), role) == roles.end()) {
            roles.emplace_back(role);
        }
    };
    if (planner.kind == PlannerOperatorKind::FixedOption) {
        switch (planner.option_kind) {
        case FixedOptionKind::FracturePrepare:
            add(fracture_use ? "fracture" : "fracture_preparation");
            if (!fracture_use) add("retry_action");
            break;
        case FixedOptionKind::TemporaryBenchRepeat:
            if (action_index == planner.setup_action) add("temporary_blocker");
            if (action_index == planner.followup_action) add("retry_action");
            if (action_index == planner.cleanup_action) {
                add("cleanup_or_replacement");
            }
            break;
        case FixedOptionKind::ProtectedRepeat:
            if (action_index == planner.setup_action) add("protection_setup");
            if (action_index == planner.followup_action) add("retry_action");
            if (action_index == planner.cleanup_action) {
                add("cleanup_or_replacement");
            }
            break;
        case FixedOptionKind::ProtectedSide:
            if (action_index == planner.setup_action) add("protection_setup");
            break;
        case FixedOptionKind::MultimodFinish:
            if (action_index == planner.setup_action) {
                add("multimod_setup");
            } else {
                add("multimod_finish");
                add("permanent_goal_bench");
                add("deterministic_finish");
            }
            break;
        case FixedOptionKind::Renewal:
            add("retry_action");
            break;
        default:
            break;
        }
    }
    if (planner.automatic_kind == AutomaticCandidateKind::PermanentBench) {
        add("permanent_goal_bench");
        add("deterministic_finish");
    }
    if (roles.empty()) return {};
    std::string out = ",\"accounting_roles\":[";
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (i != 0) out += ',';
        out += "\"" + roles[i] + "\"";
    }
    return out + ']';
}

} // namespace
} // namespace solver
} // namespace poecraft
