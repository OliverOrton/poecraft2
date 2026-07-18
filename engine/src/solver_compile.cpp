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
 * (docs/solver/crafting-solver-plan.md, Policy To Strategy Graph).
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
    /* Cost annotations are presentation metadata. Fixed micro-chaos
     * precision keeps native/WASM documents byte-stable across harmless
     * last-bit differences in dense fixed-policy evaluation. */
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
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
    std::map<std::uint32_t, std::uint32_t> representative_by_family;
    pc_bitset_for_each(
        junk.member_mask.data(), session.words, [&](std::size_t bit) {
            const std::uint32_t mod = static_cast<std::uint32_t>(bit);
            representative_by_family.emplace(session.family_id[mod], mod);
        });
    bool exact_family_cover = !representative_by_family.empty();
    for (std::uint32_t mod = 0;
         exact_family_cover && mod < session.mod_count; ++mod) {
        const bool family_selected =
            representative_by_family.count(session.family_id[mod]) != 0;
        const bool class_selected =
            pc_bitset_test(junk.member_mask.data(), mod);
        if (family_selected != class_selected) exact_family_cover = false;
    }

    std::string out = exact_family_cover
                          ? "{\"type\":\"mod_family_count\",\"family_mod_keys\":["
                          : "{\"type\":\"mod_count\",\"mod_keys\":[";
    bool first = true;
    const auto append_key = [&](const std::uint32_t mod) {
            const std::string key =
                mod_key_of(session, mod);
            if (key.empty()) {
                gap("junk class member " + std::to_string(mod) +
                    " has no stable modifier key");
            }
            if (!first) out += ',';
            first = false;
            out += "\"" + json_escape(key) + "\"";
    };
    if (exact_family_cover) {
        for (const auto& [unused_family, representative] :
             representative_by_family) {
            (void)unused_family;
            append_key(representative);
        }
    } else {
        pc_bitset_for_each(
            junk.member_mask.data(), session.words, [&](std::size_t bit) {
                append_key(static_cast<std::uint32_t>(bit));
            });
    }
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

std::string abstract_state_condition(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const std::vector<SlotVocabulary>& vocabulary,
    const AbstractState& state) {
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
    /* Item flags establish that a metamod is active. When that ordinary
     * crafted affix is fractured, distinguish the exact carrier identity so
     * compiled routing agrees with the native transition. */
    const DataImpl& data = *session.data;
    const std::pair<std::uint32_t, int> metamod_flags[] = {
        {kFlagMultimod, data.metamod_multimod_code},
        {kFlagNoAttack, data.metamod_no_attack_code},
        {kFlagNoCaster, data.metamod_no_caster_code},
        {kFlagPrefixesLocked, data.metamod_prefixes_locked_code},
        {kFlagSuffixesLocked, data.metamod_suffixes_locked_code},
    };
    for (const auto& [flag, code] : metamod_flags) {
        if ((state.flags & flag) == 0 || code < 0) continue;
        std::uint32_t representative = kNoId;
        for (const std::uint32_t mod : session.bench_mod_ids) {
            if (mod < session.metamod_type.size() &&
                session.metamod_type[mod] == code) {
                representative = mod;
                break;
            }
        }
        if (representative == kNoId) {
            gap("active metamod has no stable session modifier");
        }
        const std::string member =
            "{\"type\":\"has_mod_family\",\"family_mod_key\":\"" +
            json_escape(mod_key_of(session, representative)) +
            "\",\"min_tier\":0}";
        parts.push_back(with_slot_flags(member, false, true));
        const std::string fractured =
            with_slot_flags(member, true, false);
        parts.push_back((state.fractured_metamod_flags & flag) != 0
                            ? fractured
                            : not_of(fractured));
    }
    const std::string veiled_prefix = item_flag_condition("veiled_prefix");
    const std::string veiled_suffix = item_flag_condition("veiled_suffix");
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
            session, layout.junk_classes[i], state.crafted_junk_counts[i],
            PC_MOD_SLOT_CRAFTED));
        parts.push_back(mod_count_condition(
            session, layout.junk_classes[i],
            state.fractured_crafted_junk_counts[i],
            PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED));
    }
    return all_of(parts);
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
    CalcContext& calc,
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

    /* Abstract-state ids follow transition discovery order, which is allowed
     * to differ between native and WASM. Canonicalize the compile order and
     * emitted node ids by the exact state predicate so policy compression
     * produces the same document for the same solved policy. */
    std::map<std::uint32_t, std::string> state_conditions;
    for (const std::uint32_t state_id : compiled_states) {
        state_conditions.emplace(
            state_id,
            abstract_state_condition(
                session, layout, vocabulary, calc.state(state_id)));
    }
    std::sort(
        compiled_states.begin(), compiled_states.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            const std::string& left_condition = state_conditions.at(left);
            const std::string& right_condition = state_conditions.at(right);
            if (left_condition != right_condition) {
                return left_condition < right_condition;
            }
            if (result.policy[left].index != result.policy[right].index) {
                return result.policy[left].index < result.policy[right].index;
            }
            const std::string left_cost = number(result.values[left]);
            const std::string right_cost = number(result.values[right]);
            if (left_cost != right_cost) return left_cost < right_cost;
            return left < right;
        });
    std::vector<std::uint32_t> canonical_state_ids(
        result.values.size(), kNoId);
    for (std::uint32_t i = 0; i < compiled_states.size(); ++i) {
        canonical_state_ids[compiled_states[i]] = i;
    }
    const auto state_node = [&](const std::uint32_t state_id) {
        if (state_id >= canonical_state_ids.size() ||
            canonical_state_ids[state_id] == kNoId) {
            gap("policy node requested for a non-working state");
        }
        return "s" + std::to_string(canonical_state_ids[state_id]);
    };

    /* Exact policy-region compression. States may share one emitted
     * continuation only when the selected program is state-independent and
     * the serialized expected-cost annotation is identical. Observation-
     * owned and state-local retry options remain singleton regions so their
     * concrete routing recipes cannot be conflated. */
    std::map<std::string, std::vector<std::uint32_t>> leaders_by_key;
    std::map<std::uint32_t, std::vector<std::uint32_t>> states_by_leader;
    std::vector<std::uint32_t> emitted_states;
    for (const std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool primitive_unveil =
            planner.kind == PlannerOperatorKind::Primitive &&
            calc.registry().actions[planner.primitive_action].params.type ==
                ActionType::Unveil;
        const bool state_local_option =
            planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::FracturePrepare ||
             planner.option_kind == FixedOptionKind::ImprintRetry);
        std::uint32_t leader = state_id;
        if (!primitive_unveil && !state_local_option) {
            const std::string key =
                std::to_string(result.policy[state_id].index) + ":" +
                number(result.values[state_id]);
            std::vector<std::uint32_t>& leaders = leaders_by_key[key];
            if (leaders.empty() ||
                states_by_leader[leaders.back()].size() >= 8) {
                leaders.push_back(state_id);
                emitted_states.push_back(state_id);
            }
            leader = leaders.back();
        } else {
            emitted_states.push_back(state_id);
        }
        states_by_leader[leader].push_back(state_id);
    }
    std::map<std::uint32_t, OptionKernel> compiled_option_kernels;
    for (const std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        if (planner.kind != PlannerOperatorKind::FixedOption ||
            (planner.option_kind != FixedOptionKind::Renewal &&
             planner.option_kind != FixedOptionKind::ProtectedRepeat &&
             planner.option_kind != FixedOptionKind::FracturePrepare &&
             planner.option_kind != FixedOptionKind::ImprintRetry)) {
            continue;
        }
        const OptionKernel& kernel = calc.option_kernel(
            state_id, result.policy[state_id].index);
        if (!kernel.supported || !kernel.legal) {
            gap("selected S7.4 option has no legal expansion kernel");
        }
        compiled_option_kernels.emplace(state_id, kernel);
    }
    std::uint32_t node_count = 4; /* start, router, goal, offpolicy */
    const auto check_node_cap = [&]() {
        if (node_count > result.options.max_compiled_nodes) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_nodes";
            gap("compiled policy exceeded max_compiled_nodes (" +
                std::to_string(result.options.max_compiled_nodes) + ")");
        }
    };
    check_node_cap();

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
    for (std::uint32_t state_id : emitted_states) {
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
            json += ",{\"id\":\"";
            json += state_node(state_id);
            json += "\",\"kind\":\"router\"}";
            ++node_count;
            check_node_cap();
            for (std::size_t option = 0;
                 option < result.unveil_preferences[state_id].size();
                 ++option) {
                const std::uint32_t mod_id =
                    result.unveil_preferences[state_id][option];
                json += ",{\"id\":\"";
                json += state_node(state_id);
                json += "_u" + std::to_string(option);
                json += "\",\"kind\":\"operation\",\"expected_cost\":";
                json += number(result.values[state_id]);
                json += ",\"operation\":{\"type\":\"unveil\",\"mod_key\":\"";
                json += json_escape(mod_key_of(session, mod_id));
                json += "\"}}";
                ++node_count;
                check_node_cap();
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::ImprintRetry) {
            if (planner.primitive_program.empty()) {
                gap("imprint retry option has no attempt program");
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "\",\"kind\":\"operation\",\"expected_cost\":" +
                    number(result.values[state_id]) +
                    ",\"operation\":{\"type\":\"bestiary:imprint\"}}";
            ++node_count;
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                json += ",{\"id\":\"" + state_node(state_id) + "_o" +
                        std::to_string(step) +
                        "\",\"kind\":\"operation\",\"operation\":" +
                        operation_json(
                            session, calc.registry().actions.at(
                                         planner.primitive_program[step])) +
                        "}";
                ++node_count;
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_imprint_route\",\"kind\":\"router\"}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_restore\",\"kind\":\"operation\",\"operation\":"
                    "{\"type\":\"bestiary:restore_imprint\"}}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat)) {
            const bool observed =
                !planner.primitive_program.empty() &&
                calc.registry()
                        .actions.at(planner.primitive_program.back())
                        .params.type == ActionType::Unveil;
            const std::size_t primitive_steps =
                planner.primitive_program.size() - (observed ? 1u : 0u);
            if (primitive_steps == 0) {
                gap("renewal option has no executable rolling step");
            }
            for (std::size_t step = 0; step < primitive_steps; ++step) {
                json += ",{\"id\":\"" + state_node(state_id);
                if (step > 0) json += "_o" + std::to_string(step);
                json += "\",\"kind\":\"operation\"";
                if (step == 0) {
                    json += ",\"expected_cost\":" +
                            number(result.values[state_id]);
                }
                json += ",\"operation\":" + operation_json(
                    session, calc.registry().actions.at(
                                 planner.primitive_program[step])) + "}";
                ++node_count;
                check_node_cap();
            }
            if (observed) {
                const auto& preferences =
                    result.option_unveil_preferences.at(state_id);
                if (preferences.empty()) {
                    gap("observed renewal has no Unveil preferences");
                }
                json += ",{\"id\":\"" + state_node(state_id) +
                        "_observe\",\"kind\":\"router\"}";
                ++node_count;
                check_node_cap();
                for (std::size_t observation = 0;
                     observation < preferences.size(); ++observation) {
                    json += ",{\"id\":\"" + state_node(state_id) +
                            "_obs" + std::to_string(observation) +
                            "\",\"kind\":\"router\"}";
                    ++node_count;
                    check_node_cap();
                    for (std::size_t choice = 0;
                         choice < preferences[observation].choices.size();
                         ++choice) {
                        json += ",{\"id\":\"" +
                                state_node(state_id) + "_obs" +
                                std::to_string(observation) + "_u" +
                                std::to_string(choice) +
                                "\",\"kind\":\"operation\",\"operation\":";
                        ActionDescriptor unveil =
                            calc.registry().actions.at(
                                planner.primitive_program.back());
                        unveil.params.mod_id =
                            preferences[observation].choices[choice].mod_id;
                        json += operation_json(session, unveil) + "}";
                        ++node_count;
                        check_node_cap();
                    }
                }
            } else {
                json += ",{\"id\":\"" + state_node(state_id) +
                        "_retry\",\"kind\":\"router\"}";
                ++node_count;
                check_node_cap();
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::FracturePrepare) {
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            if (kernel.entry_continues) {
                json += ",{\"id\":\"" + state_node(state_id) +
                        "\",\"kind\":\"operation\",\"expected_cost\":" +
                        number(result.values[state_id]) +
                        ",\"operation\":" + operation_json(
                            session, calc.registry().actions.at(
                                         planner.conditional_action)) + "}";
                ++node_count;
                check_node_cap();
                continue;
            }
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                json += ",{\"id\":\"" + state_node(state_id);
                if (step > 0) json += "_o" + std::to_string(step);
                json += "\",\"kind\":\"operation\"";
                if (step == 0) {
                    json += ",\"expected_cost\":" +
                            number(result.values[state_id]);
                }
                json += ",\"operation\":" + operation_json(
                    session, calc.registry().actions.at(
                                 planner.primitive_program[step])) + "}";
                ++node_count;
                check_node_cap();
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture_route\",\"kind\":\"router\"}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture\",\"kind\":\"operation\",\"operation\":" +
                    operation_json(
                        session, calc.registry().actions.at(
                                     planner.conditional_action)) + "}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        const std::vector<std::uint32_t>& program =
            planner.primitive_program;
        if (program.empty()) {
            gap("planner operator " + planner.id + " has no primitive program");
        }
        for (std::size_t step = 0; step < program.size(); ++step) {
            json += ",{\"id\":\"";
            json += state_node(state_id);
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
            check_node_cap();
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
        if (edge_counter > result.options.max_compiled_edges) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_edges";
            gap("compiled policy exceeded max_compiled_edges (" +
                std::to_string(result.options.max_compiled_edges) + ")");
        }
        if (json.size() > result.options.max_strategy_json_bytes) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("compiled policy exceeded max_strategy_json_bytes (" +
                std::to_string(result.options.max_strategy_json_bytes) +
                ")");
        }
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

    for (const std::uint32_t leader : emitted_states) {
        for (const std::uint32_t state_id : states_by_leader.at(leader)) {
            edge(
                "router", state_node(leader), 1,
                state_conditions.at(state_id),
                false);
        }
    }
    edge("router", "offpolicy", 2, "", true);
    for (std::uint32_t state_id : emitted_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool unveil =
            planner.kind == PlannerOperatorKind::Primitive &&
            calc.registry().actions[planner.primitive_action].params.type ==
                ActionType::Unveil;
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::ImprintRetry) {
            const std::string base = state_node(state_id);
            edge(base, base + "_o0", 0, "", true);
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::string from =
                    base + "_o" + std::to_string(step);
                const std::string to =
                    step + 1 < planner.primitive_program.size()
                        ? base + "_o" + std::to_string(step + 1)
                        : base + "_imprint_route";
                edge(from, to, 0, "", true);
            }
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            const std::string route = base + "_imprint_route";
            for (std::size_t i = 0; i < kernel.retry_states.size(); ++i) {
                edge(
                    route, base + "_restore", static_cast<int>(i),
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(kernel.retry_states[i])),
                    false);
            }
            edge(
                route, "router",
                static_cast<int>(kernel.retry_states.size()), "", true);
            edge(base + "_restore", base, 0, "", true);
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat)) {
            const bool observed =
                !planner.primitive_program.empty() &&
                calc.registry()
                        .actions.at(planner.primitive_program.back())
                        .params.type == ActionType::Unveil;
            const std::size_t primitive_steps =
                planner.primitive_program.size() - (observed ? 1u : 0u);
            for (std::size_t step = 0; step < primitive_steps; ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to;
                if (step + 1 < primitive_steps) {
                    to = state_node(state_id) + "_o" +
                         std::to_string(step + 1);
                } else {
                    to = state_node(state_id) +
                         (observed ? "_observe" : "_retry");
                }
                edge(from, to, 0, "", true);
            }
            if (observed) {
                const auto& preferences =
                    result.option_unveil_preferences.at(state_id);
                const std::string dispatcher =
                    state_node(state_id) + "_observe";
                for (std::size_t observation = 0;
                     observation < preferences.size(); ++observation) {
                    const std::string offer =
                        state_node(state_id) + "_obs" +
                        std::to_string(observation);
                    edge(
                        dispatcher, offer,
                        static_cast<int>(observation),
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(preferences[observation]
                                           .observation_state)),
                        false);
                    for (std::size_t choice = 0;
                         choice < preferences[observation].choices.size();
                         ++choice) {
                        const ObservedUnveilChoice& selected =
                            preferences[observation].choices[choice];
                        const std::string operation =
                            offer + "_u" + std::to_string(choice);
                        edge(
                            offer, operation, static_cast<int>(choice),
                            "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                                json_escape(mod_key_of(
                                    session, selected.mod_id)) +
                                "\"}",
                            false);
                        edge(
                            operation,
                            selected.successor_state == state_id
                                ? state_node(state_id)
                                : "router",
                            0, "", true);
                    }
                    edge(
                        offer, "offpolicy",
                        static_cast<int>(
                            preferences[observation].choices.size()),
                        "", true);
                }
                /* No-offer and other expressible stopped paths are exits,
                 * not fabricated option failures. */
                edge(
                    dispatcher, "router",
                    static_cast<int>(preferences.size()), "", true);
            } else {
                const OptionKernel& kernel =
                    compiled_option_kernels.at(state_id);
                const std::string retry =
                    state_node(state_id) + "_retry";
                for (std::size_t i = 0; i < kernel.retry_states.size(); ++i) {
                    edge(
                        retry, state_node(state_id),
                        static_cast<int>(i),
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(kernel.retry_states[i])),
                        false);
                }
                edge(
                    retry, "router",
                    static_cast<int>(kernel.retry_states.size()), "", true);
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::FracturePrepare) {
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            if (kernel.entry_continues) {
                edge(state_node(state_id), "router", 0, "", true);
                continue;
            }
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to =
                    step + 1 < planner.primitive_program.size()
                        ? state_node(state_id) + "_o" +
                              std::to_string(step + 1)
                        : state_node(state_id) +
                              "_fracture_route";
                edge(from, to, 0, "", true);
            }
            const std::string route =
                state_node(state_id) + "_fracture_route";
            int priority = 0;
            for (const std::uint32_t continuation :
                 kernel.continuation_states) {
                edge(
                    route,
                    state_node(state_id) + "_fracture",
                    priority++,
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(continuation)),
                    false);
            }
            for (const std::uint32_t retry_state : kernel.retry_states) {
                edge(
                    route, state_node(state_id), priority++,
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(retry_state)),
                    false);
            }
            edge(route, "router", priority, "", true);
            edge(
                state_node(state_id) + "_fracture", "router",
                0, "", true);
            continue;
        }
        if (!unveil) {
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to = "router";
                if (step + 1 < planner.primitive_program.size()) {
                    to = state_node(state_id) + "_o" +
                         std::to_string(step + 1);
                }
                edge(from, to, 0, "", true);
            }
            continue;
        }
        const auto& preferences = result.unveil_preferences[state_id];
        for (std::size_t option = 0; option < preferences.size(); ++option) {
            const std::string operation =
                state_node(state_id) + "_u" +
                std::to_string(option);
            const std::string condition =
                "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                json_escape(mod_key_of(session, preferences[option])) +
                "\"}";
            edge(state_node(state_id), operation,
                 static_cast<int>(option), condition, false);
            edge(operation, "router", 0, "", true);
        }
        edge(state_node(state_id), "offpolicy",
             static_cast<int>(preferences.size()), "", true);
    }
    json += "]}";
    if (json.size() > result.options.max_strategy_json_bytes) {
        if (telemetry != nullptr) telemetry->cap_hit = "max_strategy_json_bytes";
        gap("compiled policy exceeded max_strategy_json_bytes (" +
            std::to_string(result.options.max_strategy_json_bytes) + ")");
    }
    if (telemetry != nullptr) {
        telemetry->working_states = static_cast<std::uint32_t>(
            compiled_states.size());
        telemetry->policy_regions = static_cast<std::uint32_t>(
            emitted_states.size());
        telemetry->nodes = node_count;
        telemetry->edges = edge_counter;
        telemetry->strategy_json_bytes = json.size();
    }
    return json;
}

} // namespace solver
} // namespace poecraft
