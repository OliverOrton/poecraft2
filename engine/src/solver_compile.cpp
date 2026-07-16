#include "solver_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "poecraft/bitset.h"

/*
 * Solver S5: compile a policy into the ordinary strategy graph format
 * (docs/crafting-solver-plan.md, Policy To Strategy Graph).
 *
 * Shape: start -> master router. The router's prioritized edges are, in
 * order, the goal test (success terminal), one membership test per
 * policy-reachable state (to that state's primitive operation or fixed-option
 * primitive chain, which routes back to the router), and a default edge to a
 * failure terminal so any
 * off-policy item — abstraction drift, vocabulary mismatch — fails loudly
 * instead of silently rerouting. The first operation node for each state
 * carries its expected remaining cost as an "expected_cost" annotation
 * (ignored by the strategy compiler, consumed by the editor/board).
 */
namespace poecraft {
namespace solver {

namespace {

[[noreturn]] void gap(const std::string& message) {
    throw std::runtime_error("policy compile: " + message);
}

std::string json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

std::string number(double value) {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    return buffer;
}

const char* rarity_name(std::uint8_t rarity) {
    switch (rarity) {
    case PC_RARITY_NORMAL:
        return "normal";
    case PC_RARITY_MAGIC:
        return "magic";
    default:
        return "rare";
    }
}

std::string all_of(const std::vector<std::string>& parts) {
    if (parts.size() == 1) return parts.front();
    std::string out = "{\"type\":\"all\",\"conditions\":[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += ',';
        out += parts[i];
    }
    out += "]}";
    return out;
}

std::string any_of(const std::vector<std::string>& parts) {
    if (parts.size() == 1) return parts.front();
    std::string out = "{\"type\":\"any\",\"conditions\":[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += ',';
        out += parts[i];
    }
    out += "]}";
    return out;
}

std::string at_least(std::size_t count,
                     const std::vector<std::string>& parts) {
    std::string out = "{\"type\":\"at_least\",\"count\":" +
                      std::to_string(count) + ",\"conditions\":[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += ',';
        out += parts[i];
    }
    out += "]}";
    return out;
}

std::string not_of(const std::string& part) {
    return "{\"type\":\"not\",\"conditions\":[" + part + "]}";
}

std::string rarity_condition(std::uint8_t rarity) {
    return std::string("{\"type\":\"rarity_is\",\"rarity\":\"") +
           rarity_name(rarity) + "\"}";
}

std::string count_condition(const char* type, std::uint8_t count) {
    return std::string("{\"type\":\"") + type +
           "\",\"min\":" + std::to_string(count) +
           ",\"max\":" + std::to_string(count) + "}";
}

std::string item_flag_condition(const char* flag) {
    return std::string("{\"type\":\"item_flag\",\"flag\":\"") +
           flag + "\"}";
}

std::string eldritch_tier_condition(
    const char* side,
    std::uint8_t tier) {
    return std::string("{\"type\":\"eldritch_tier\",\"side\":\"") +
           side + "\",\"min\":" + std::to_string(tier) +
           ",\"max\":" + std::to_string(tier) + "}";
}

/* Per-slot condition builders resolved once up front. */
struct SlotVocabulary {
    std::string member;     /* any-tier membership */
    std::string satisfied;  /* membership at the required tier */
};

std::string mod_key_of(const SessionImpl& session, std::uint32_t mod_id) {
    const DataImpl& data = *session.data;
    return data.string_at(data.mod_key_sid[session.global_index[mod_id]]);
}

std::string mod_count_condition(
    const SessionImpl& session,
    const JunkClass& junk,
    std::uint8_t count,
    std::uint8_t required_flags = 0) {
    std::string out = "{\"type\":\"mod_count\",\"mod_keys\":[";
    bool first = true;
    pc_bitset_for_each(
        junk.member_mask.data(), session.words, [&](std::size_t bit) {
            const std::string key =
                mod_key_of(session, static_cast<std::uint32_t>(bit));
            if (key.empty()) {
                gap("junk class member " + std::to_string(bit) +
                    " has no stable modifier key");
            }
            if (!first) out += ',';
            first = false;
            out += "\"" + json_escape(key) + "\"";
        });
    if (first) gap("junk class has no members");
    out += "]";
    if ((required_flags & PC_MOD_SLOT_FRACTURED) != 0) {
        out += ",\"fractured\":true";
    }
    if ((required_flags & PC_MOD_SLOT_CRAFTED) != 0) {
        out += ",\"crafted\":true";
    }
    out += ",\"min\":" + std::to_string(count) +
           ",\"max\":" + std::to_string(count) + "}";
    return out;
}

std::string with_slot_flags(
    std::string condition,
    bool fractured,
    bool crafted) {
    if (!fractured && !crafted) return condition;
    if (condition.empty() || condition.back() != '}') {
        gap("slot condition is not a JSON object");
    }
    condition.pop_back();
    if (fractured) condition += ",\"fractured\":true";
    if (crafted) condition += ",\"crafted\":true";
    condition += '}';
    return condition;
}

std::uint32_t first_bit(const std::vector<std::uint64_t>& mask,
                        std::size_t words) {
    std::uint32_t found = kNoId;
    pc_bitset_for_each(mask.data(), words, [&](std::size_t bit) {
        if (found == kNoId) found = static_cast<std::uint32_t>(bit);
    });
    return found;
}

SlotVocabulary slot_vocabulary(const SessionImpl& session,
                               const ResolvedGoalSlot& slot,
                               std::size_t slot_index) {
    const DataImpl& data = *session.data;
    SlotVocabulary vocabulary;
    if (slot.spec.family_id != kNoId) {
        const std::uint32_t representative =
            first_bit(slot.satisfying_mask, session.words);
        const std::string key = mod_key_of(session, representative);
        if (key.empty()) {
            gap("goal slot " + std::to_string(slot_index) +
                " has no stable modifier key");
        }
        vocabulary.member =
            "{\"type\":\"has_mod_family\",\"family_mod_key\":\"" +
            json_escape(key) + "\",\"min_tier\":0}";
        vocabulary.satisfied =
            "{\"type\":\"has_mod_family\",\"family_mod_key\":\"" +
            json_escape(key) + "\",\"min_tier\":" +
            std::to_string(slot.spec.min_tier) + "}";
    } else {
        const std::string key =
            data.string_at(data.group_key_sids[slot.spec.group_id]);
        if (key.empty()) {
            gap("goal slot " + std::to_string(slot_index) +
                " group has no stable key");
        }
        vocabulary.member = "{\"type\":\"has_mod_group\",\"group\":\"" +
                            json_escape(key) + "\",\"min_tier\":0}";
        vocabulary.satisfied =
            "{\"type\":\"has_mod_group\",\"group\":\"" +
            json_escape(key) + "\",\"min_tier\":" +
            std::to_string(slot.spec.min_tier) + "}";
    }
    return vocabulary;
}

std::string operation_json(const SessionImpl& session,
                           const ActionDescriptor& action) {
    const DataImpl& data = *session.data;
    if (action.synthetic) return "{\"type\":\"restart\"}";
    switch (action.params.type) {
    case ActionType::Essence:
        return "{\"type\":\"essence\",\"essence_key\":\"" +
               json_escape(data.string_at(
                   data.essence_key_sids[action.params.essence_index])) +
               "\"}";
    case ActionType::Fossil: {
        std::string out = "{\"type\":\"fossil\",\"fossils\":[";
        for (std::size_t i = 0; i < action.params.fossil_indices.size();
             ++i) {
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
    default:
        /* Basic currency and simple actions: id == operation name. */
        return "{\"type\":\"" + json_escape(action.id) + "\"}";
    }
}

} // namespace

std::string compile_policy_strategy_json(
    const CalcContext& calc,
    const SolveResult& result,
    const std::string& name,
    PolicyCompilationTelemetry* telemetry) {
    const SessionImpl& session = calc.session();
    const DataImpl& data = *session.data;
    const AbstractLayout& layout = calc.layout();

    if (result.start_state == kNoId ||
        result.start_state >= result.values.size()) {
        gap("solve result has no start state");
    }

    std::vector<SlotVocabulary> vocabulary;
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        vocabulary.push_back(
            slot_vocabulary(session, layout.slots[i], i));
    }

    /* Collect and validate the policy-reachable working states. */
    std::vector<std::uint32_t> compiled_states;
    for (std::uint32_t state_id = 0; state_id < result.values.size();
         ++state_id) {
        if (!result.policy_reachable[state_id] ||
            result.goal_states[state_id]) {
            continue;
        }
        const AbstractState& state = calc.state(state_id);
        if (result.policy[state_id] == kNoId) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has no action");
        }
        if (result.policy[state_id] >= calc.operators().size()) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has an unknown planner operator");
        }
        const PlannerOperator& selected =
            calc.operators().at(result.policy[state_id]);
        if (result.policy[state_id].kind != selected.kind) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has a mismatched planner-operator tag");
        }
        compiled_states.push_back(state_id);
    }
    if (compiled_states.empty()) gap("policy reaches no working states");
    std::uint32_t node_count = 4; /* start, router, goal, offpolicy */

    const AbstractState& start = calc.state(result.start_state);
    pc_item_state start_item;
    if (!calc.materialize(result.start_state, start_item)) {
        gap("start state cannot be materialized exactly");
    }

    /* --- emit --------------------------------------------------------------- */
    std::string json = "{\"version\":\"v1\",\"name\":\"";
    json += json_escape(name);
    json += "\",\"base_state\":{\"base_key\":\"";
    json += json_escape(
        data.string_at(data.base_metadata_path_sid[session.base_index]));
    json += "\",\"item_level\":";
    json += std::to_string(session.item_level);
    json += ",\"rarity\":\"";
    json += rarity_name(start.rarity);
    std::uint32_t item_flags = 0;
    if (start.flags & kFlagCorrupted) item_flags |= PC_ITEM_CORRUPTED;
    if (start.flags & kFlagMirrored) item_flags |= PC_ITEM_MIRRORED;
    if (start.flags & kFlagSplit) item_flags |= PC_ITEM_SPLIT;
    if (start.flags & kFlagSynthesised) item_flags |= PC_ITEM_SYNTHESISED;
    json += "\",\"item_flags\":" + std::to_string(item_flags);
    json += ",\"generic_influence_bits\":" +
            std::to_string(start.influence_bits);
    json += ",\"searing_exarch_tier\":" +
            std::to_string(start.searing_exarch_tier);
    json += ",\"eater_of_worlds_tier\":" +
            std::to_string(start.eater_of_worlds_tier);
    const auto start_mods = [&](const char* name, const pc_mod_slot* slots,
                                std::uint8_t count) {
        json += ",\"";
        json += name;
        json += "\":[";
        for (std::uint8_t i = 0; i < count; ++i) {
            if (i > 0) json += ',';
            json += "{\"mod_key\":\"";
            json += json_escape(mod_key_of(session, slots[i].mod_id));
            json += "\"";
            if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) != 0) {
                json += ",\"fractured\":true";
            }
            if ((slots[i].flags & PC_MOD_SLOT_CRAFTED) != 0) {
                json += ",\"crafted\":true";
            }
            json += '}';
        }
        json += ']';
    };
    start_mods("prefixes", start_item.prefixes, start_item.prefix_count);
    start_mods("suffixes", start_item.suffixes, start_item.suffix_count);
    json += "},\"start_node_id\":\"start\",\"nodes\":[";
    json += "{\"id\":\"start\",\"kind\":\"start\"},";
    json += "{\"id\":\"router\",\"kind\":\"router\"},";
    json +=
        "{\"id\":\"goal\",\"kind\":\"terminal\",\"terminal\":\"success\"},";
    json += "{\"id\":\"offpolicy\",\"kind\":\"terminal\",\"terminal\":"
            "\"failure\",\"reason\":\"item left the policy-reachable "
            "state set\"}";
    for (std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool unveil =
            planner.kind == PlannerOperatorKind::Primitive &&
            calc.registry().actions[planner.primitive_action].params.type ==
                ActionType::Unveil;
        if (unveil) {
            if (state_id >= result.unveil_preferences.size() ||
                result.unveil_preferences[state_id].empty()) {
                gap("unveil state " + std::to_string(state_id) +
                    " has no resolved option preference");
            }
            json += ",{\"id\":\"s";
            json += std::to_string(state_id);
            json += "\",\"kind\":\"router\"}";
            ++node_count;
            for (std::size_t option = 0;
                 option < result.unveil_preferences[state_id].size();
                 ++option) {
                const std::uint32_t mod_id =
                    result.unveil_preferences[state_id][option];
                json += ",{\"id\":\"s";
                json += std::to_string(state_id);
                json += "_u" + std::to_string(option);
                json += "\",\"kind\":\"operation\",\"expected_cost\":";
                json += number(result.values[state_id]);
                json += ",\"operation\":{\"type\":\"unveil\",\"mod_key\":\"";
                json += json_escape(mod_key_of(session, mod_id));
                json += "\"}}";
                ++node_count;
            }
            continue;
        }
        const std::vector<std::uint32_t>& program =
            planner.primitive_program;
        if (program.empty()) {
            gap("planner operator " + planner.id + " has no primitive program");
        }
        for (std::size_t step = 0; step < program.size(); ++step) {
            json += ",{\"id\":\"s";
            json += std::to_string(state_id);
            if (step > 0) json += "_o" + std::to_string(step);
            json += "\",\"kind\":\"operation\"";
            if (step == 0) {
                json += ",\"expected_cost\":";
                json += number(result.values[state_id]);
            }
            json += ",\"operation\":";
            json += operation_json(
                session, calc.registry().actions.at(program[step]));
            json += "}";
            ++node_count;
        }
    }
    json += "],\"edges\":[";

    std::uint32_t edge_counter = 0;
    const auto edge = [&](const std::string& from, const std::string& to,
                          int priority, const std::string& condition,
                          bool is_default) {
        if (edge_counter > 0) json += ',';
        json += "{\"id\":\"e";
        json += std::to_string(edge_counter++);
        json += "\",\"from\":\"";
        json += from;
        json += "\",\"to\":\"";
        json += to;
        json += "\",\"priority\":";
        json += std::to_string(priority);
        if (is_default) {
            json += ",\"is_default\":true";
        } else {
            json += ",\"condition\":";
            json += condition;
        }
        json += "}";
    };

    edge("start", "router", 0, "", true);

    /* Goal first: the configured number of satisfied slots at the finished
     * rarity succeeds regardless of junk. */
    {
        std::vector<std::string> parts{rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            satisfied.push_back(vocabulary[i].satisfied);
        }
        parts.push_back(
            calc.goal().required_satisfied_slots() == satisfied.size()
                ? all_of(satisfied)
                : at_least(calc.goal().required_satisfied_slots(),
                           satisfied));
        edge("router", "goal", 0, all_of(parts), false);
    }

    for (std::uint32_t state_id : compiled_states) {
        const AbstractState& state = calc.state(state_id);
        std::vector<std::string> parts{rarity_condition(state.rarity)};
        parts.push_back(
            count_condition("prefix_count_range", state.prefix_count));
        parts.push_back(
            count_condition("suffix_count_range", state.suffix_count));
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const auto status =
                static_cast<GoalSlotStatus>(state.slot_status[i]);
            const bool fractured =
                (state.fractured_goal_mask & (1u << i)) != 0;
            const bool crafted =
                (state.crafted_goal_mask & (1u << i)) != 0;
            switch (status) {
            case GoalSlotStatus::Satisfied:
                parts.push_back(vocabulary[i].satisfied);
                parts.push_back(
                    fractured
                        ? with_slot_flags(vocabulary[i].member, true, false)
                        : not_of(with_slot_flags(
                              vocabulary[i].member, true, false)));
                parts.push_back(
                    crafted
                        ? with_slot_flags(vocabulary[i].member, false, true)
                        : not_of(with_slot_flags(
                              vocabulary[i].member, false, true)));
                break;
            case GoalSlotStatus::PresentBelowTier:
                parts.push_back(vocabulary[i].member);
                parts.push_back(not_of(vocabulary[i].satisfied));
                parts.push_back(
                    fractured
                        ? with_slot_flags(vocabulary[i].member, true, false)
                        : not_of(with_slot_flags(
                              vocabulary[i].member, true, false)));
                parts.push_back(
                    crafted
                        ? with_slot_flags(vocabulary[i].member, false, true)
                        : not_of(with_slot_flags(
                              vocabulary[i].member, false, true)));
                break;
            case GoalSlotStatus::Absent:
                parts.push_back(not_of(vocabulary[i].member));
                break;
            }
        }
        static const std::pair<std::uint32_t, const char*> flag_conditions[] = {
            {kFlagCorrupted, "corrupted"},
            {kFlagMirrored, "mirrored"},
            {kFlagSplit, "split"},
            {kFlagSynthesised, "synthesised"},
            {kFlagFractured, "fractured"},
            {kFlagCraftedMod, "crafted"},
            {kFlagVeiledMod, "veiled"},
            {kFlagMultimod, "multimod"},
            {kFlagNoAttack, "no_attack"},
            {kFlagNoCaster, "no_caster"},
            {kFlagPrefixesLocked, "prefixes_locked"},
            {kFlagSuffixesLocked, "suffixes_locked"},
            {kFlagInfluenced, "influenced"},
            {kFlagEldritchImplicit, "eldritch_implicit"},
        };
        for (const auto& [flag, name] : flag_conditions) {
            const std::string condition = item_flag_condition(name);
            parts.push_back((state.flags & flag) != 0
                                ? condition
                                : not_of(condition));
        }
        const std::string veiled_prefix =
            item_flag_condition("veiled_prefix");
        const std::string veiled_suffix =
            item_flag_condition("veiled_suffix");
        parts.push_back(state.veiled_side == PC_SIDE_PREFIX
                            ? veiled_prefix
                            : not_of(veiled_prefix));
        parts.push_back(state.veiled_side == PC_SIDE_SUFFIX
                            ? veiled_suffix
                            : not_of(veiled_suffix));
        parts.push_back(
            "{\"type\":\"influence_bits\",\"value\":" +
            std::to_string(state.influence_bits) + "}");
        parts.push_back(eldritch_tier_condition(
            "searing", state.searing_exarch_tier));
        parts.push_back(eldritch_tier_condition(
            "eater", state.eater_of_worlds_tier));
        for (std::size_t i = 0; i < layout.junk_classes.size(); ++i) {
            parts.push_back(mod_count_condition(
                session, layout.junk_classes[i], state.junk_counts[i]));
            parts.push_back(mod_count_condition(
                session, layout.junk_classes[i],
                state.fractured_junk_counts[i], PC_MOD_SLOT_FRACTURED));
            parts.push_back(mod_count_condition(
                session, layout.junk_classes[i],
                state.crafted_junk_counts[i], PC_MOD_SLOT_CRAFTED));
            parts.push_back(mod_count_condition(
                session, layout.junk_classes[i],
                state.fractured_crafted_junk_counts[i],
                PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED));
        }
        edge("router", "s" + std::to_string(state_id), 1, all_of(parts),
             false);
    }
    edge("router", "offpolicy", 2, "", true);
    for (std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool unveil =
            planner.kind == PlannerOperatorKind::Primitive &&
            calc.registry().actions[planner.primitive_action].params.type ==
                ActionType::Unveil;
        if (!unveil) {
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = "s" + std::to_string(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to = "router";
                if (step + 1 < planner.primitive_program.size()) {
                    to = "s" + std::to_string(state_id) + "_o" +
                         std::to_string(step + 1);
                }
                edge(from, to, 0, "", true);
            }
            continue;
        }
        const auto& preferences = result.unveil_preferences[state_id];
        for (std::size_t option = 0; option < preferences.size(); ++option) {
            const std::string operation =
                "s" + std::to_string(state_id) + "_u" +
                std::to_string(option);
            const std::string condition =
                "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                json_escape(mod_key_of(session, preferences[option])) +
                "\"}";
            edge("s" + std::to_string(state_id), operation,
                 static_cast<int>(option), condition, false);
            edge(operation, "router", 0, "", true);
        }
        edge("s" + std::to_string(state_id), "offpolicy",
             static_cast<int>(preferences.size()), "", true);
    }
    json += "]}";
    if (telemetry != nullptr) {
        telemetry->working_states = static_cast<std::uint32_t>(
            compiled_states.size());
        telemetry->nodes = node_count;
        telemetry->edges = edge_counter;
        telemetry->strategy_json_bytes = json.size();
    }
    return json;
}

} // namespace solver
} // namespace poecraft
