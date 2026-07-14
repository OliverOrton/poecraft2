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
 * policy-reachable state (to that state's operation node, which routes
 * back to the router), and a default edge to a failure terminal so any
 * off-policy item — abstraction drift, vocabulary mismatch — fails loudly
 * instead of silently rerouting. Every operation node carries its state's
 * expected remaining cost as an "expected_cost" annotation (ignored by
 * the strategy compiler, consumed by the editor/board).
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

/* Per-slot condition builders resolved once up front. */
struct SlotVocabulary {
    std::string member;     /* any-tier membership */
    std::string satisfied;  /* membership at the required tier */
    std::string blocked;    /* a blocking group is occupied */
};

std::string mod_key_of(const SessionImpl& session, std::uint32_t mod_id) {
    const DataImpl& data = *session.data;
    return data.string_at(data.mod_key_sid[session.global_index[mod_id]]);
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
        if (slot.spec.min_tier != 0) {
            gap("goal slot " + std::to_string(slot_index) +
                " is group-identified with a tier threshold; the condition "
                "vocabulary has no group-tier test yet");
        }
        const std::string key =
            data.string_at(data.group_key_sids[slot.spec.group_id]);
        if (key.empty()) {
            gap("goal slot " + std::to_string(slot_index) +
                " group has no stable key");
        }
        vocabulary.member = "{\"type\":\"has_mod_group\",\"group\":\"" +
                            json_escape(key) + "\"}";
        vocabulary.satisfied = vocabulary.member;
    }
    std::vector<std::string> occupied;
    for (std::uint32_t group : slot.blocking_group_ids) {
        const std::string key = data.string_at(data.group_key_sids[group]);
        if (key.empty()) {
            gap("blocking group " + std::to_string(group) +
                " has no stable key");
        }
        occupied.push_back("{\"type\":\"has_mod_group\",\"group\":\"" +
                           json_escape(key) + "\"}");
    }
    vocabulary.blocked = any_of(occupied);
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
    const std::string& name) {
    const SessionImpl& session = calc.session();
    const DataImpl& data = *session.data;
    const AbstractLayout& layout = calc.layout();

    if (!layout.discriminating_tag_ids.empty()) {
        gap("tag-discriminating layouts need junk-class conditions the "
            "strategy vocabulary does not have yet");
    }
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
    std::map<std::string, std::uint32_t> signature_owner;
    for (std::uint32_t state_id = 0; state_id < result.values.size();
         ++state_id) {
        if (!result.policy_reachable[state_id] ||
            result.goal_states[state_id]) {
            continue;
        }
        const AbstractState& state = calc.state(state_id);
        if (state.flags != 0 || state.influence_bits != 0) {
            gap("state " + std::to_string(state_id) +
                " carries mechanic flags the condition vocabulary cannot "
                "test yet");
        }
        if (result.policy[state_id] == kNoId) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has no action");
        }
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const bool blocked = (state.blocked_mask & (1u << i)) != 0;
            const bool present =
                state.slot_status[i] !=
                static_cast<std::uint8_t>(GoalSlotStatus::Absent);
            if (blocked && present) {
                gap("state " + std::to_string(state_id) +
                    " is blocked while the goal is present; not "
                    "expressible yet");
            }
        }
        std::string signature;
        signature += static_cast<char>('0' + state.rarity);
        signature += static_cast<char>('0' + state.prefix_count);
        signature += static_cast<char>('0' + state.suffix_count);
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            signature += static_cast<char>('0' + state.slot_status[i]);
            signature += (state.blocked_mask & (1u << i)) ? 'b' : '.';
        }
        const auto [owner, inserted] =
            signature_owner.emplace(signature, state_id);
        if (!inserted) {
            gap("states " + std::to_string(owner->second) + " and " +
                std::to_string(state_id) +
                " share one expressible signature; junk-class conditions "
                "are required to split them");
        }
        compiled_states.push_back(state_id);
    }
    if (compiled_states.empty()) gap("policy reaches no working states");

    const AbstractState& start = calc.state(result.start_state);

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
    json += "\"},\"start_node_id\":\"start\",\"nodes\":[";
    json += "{\"id\":\"start\",\"kind\":\"start\"},";
    json += "{\"id\":\"router\",\"kind\":\"router\"},";
    json +=
        "{\"id\":\"goal\",\"kind\":\"terminal\",\"terminal\":\"success\"},";
    json += "{\"id\":\"offpolicy\",\"kind\":\"terminal\",\"terminal\":"
            "\"failure\",\"reason\":\"item left the policy-reachable "
            "state set\"}";
    for (std::uint32_t state_id : compiled_states) {
        const ActionDescriptor& action =
            calc.registry().actions[result.policy[state_id]];
        json += ",{\"id\":\"s";
        json += std::to_string(state_id);
        json += "\",\"kind\":\"operation\",\"expected_cost\":";
        json += number(result.values[state_id]);
        json += ",\"operation\":";
        json += operation_json(session, action);
        json += "}";
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
            const bool blocked = (state.blocked_mask & (1u << i)) != 0;
            switch (status) {
            case GoalSlotStatus::Satisfied:
                parts.push_back(vocabulary[i].satisfied);
                break;
            case GoalSlotStatus::PresentBelowTier:
                parts.push_back(vocabulary[i].member);
                parts.push_back(not_of(vocabulary[i].satisfied));
                break;
            case GoalSlotStatus::Absent:
                parts.push_back(not_of(vocabulary[i].member));
                parts.push_back(blocked
                                    ? vocabulary[i].blocked
                                    : not_of(vocabulary[i].blocked));
                break;
            }
        }
        edge("router", "s" + std::to_string(state_id), 1, all_of(parts),
             false);
    }
    edge("router", "offpolicy", 2, "", true);
    for (std::uint32_t state_id : compiled_states) {
        edge("s" + std::to_string(state_id), "router", 0, "", true);
    }
    json += "]}";
    return json;
}

} // namespace solver
} // namespace poecraft
