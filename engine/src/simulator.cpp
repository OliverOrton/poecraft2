#include "engine_internal.hpp"
#include "json.hpp"
#include "poecraft/session.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace poecraft {

namespace {

using json::Type;
using json::Value;

[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error("strategy: " + message);
}

const Value& require_object_member(
    const Value& object,
    const char* key,
    Type type) {
    const Value* value = object.find(key);
    if (value == nullptr || value->type != type) {
        invalid(std::string("expected ") + key);
    }
    return *value;
}

std::string string_member(
    const Value& object,
    const char* key,
    const std::string& fallback = std::string()) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::String) {
        invalid(std::string(key) + " must be a string");
    }
    return value->string;
}

int int_member(const Value& object, const char* key, int fallback) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::Number || !std::isfinite(value->number)) {
        invalid(std::string(key) + " must be a number");
    }
    return static_cast<int>(value->number);
}

bool bool_member(const Value& object, const char* key, bool fallback) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::Bool) {
        invalid(std::string(key) + " must be a bool");
    }
    return value->boolean;
}

int rarity_from_name(const std::string& name) {
    if (name == "normal") return PC_RARITY_NORMAL;
    if (name == "magic") return PC_RARITY_MAGIC;
    if (name == "rare") return PC_RARITY_RARE;
    invalid("unknown rarity: " + name);
}

std::uint8_t side_cap(const SessionImpl& session, const pc_item_state& item) {
    if (item.rarity == PC_RARITY_MAGIC) return 1;
    if (item.rarity == PC_RARITY_RARE) return session.rare_affix_cap;
    return 0;
}

void add_start_mod(
    const SessionImpl& session,
    pc_item_state& item,
    const Value& value,
    int side) {
    std::string key;
    bool fractured = false;
    bool crafted = false;
    if (value.type == Type::String) {
        key = value.string;
    } else if (value.type == Type::Object) {
        key = string_member(value, "mod_key");
        if (key.empty()) key = string_member(value, "key");
        fractured = bool_member(value, "fractured", false);
        crafted = bool_member(value, "crafted", false);
    } else {
        invalid("base_state explicit mods must be strings or objects");
    }
    if (key.empty()) invalid("base_state explicit mod requires mod_key");

    const DataImpl& data = *session.data;
    const auto pos_it = data.mod_pos_by_key.find(key);
    if (pos_it == data.mod_pos_by_key.end()) {
        invalid("base_state mod key not found: " + key);
    }
    const std::uint32_t global_id = data.mod_global_ids[pos_it->second];
    const auto session_it = session.session_id_by_global_id.find(global_id);
    if (session_it == session.session_id_by_global_id.end()) {
        invalid("base_state mod is not in the session: " + key);
    }
    const std::uint32_t mod_id = session_it->second;
    if (session.gen_type[mod_id] != side) {
        invalid("base_state mod is on the wrong affix side: " + key);
    }
    std::uint8_t flags = 0;
    if (fractured) flags |= PC_MOD_SLOT_FRACTURED;
    if (crafted) flags |= PC_MOD_SLOT_CRAFTED;
    if (pc_item_add_mod(
            &item,
            side,
            mod_id,
            static_cast<std::uint16_t>(session.primary_group[mod_id]),
            flags,
            nullptr) != PC_RESULT_OK) {
        invalid("base_state has too many explicit mods");
    }
}

pc_item_state parse_start_item(
    const SessionImpl& session,
    const Value& base_state) {
    const DataImpl& data = *session.data;
    const std::string expected_base =
        data.string_at(data.base_metadata_path_sid[session.base_index]);
    const std::string base_key = string_member(base_state, "base_key");
    if (base_key.empty() || base_key != expected_base) {
        invalid("base_state.base_key does not match the compile session");
    }
    const int item_level =
        int_member(base_state, "item_level", static_cast<int>(session.item_level));
    if (item_level != static_cast<int>(session.item_level)) {
        invalid("base_state.item_level does not match the compile session");
    }

    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = static_cast<std::uint8_t>(
        rarity_from_name(string_member(base_state, "rarity", "normal")));

    if (bool_member(base_state, "with_implicits", true)) {
        if (session.base_implicit_mod_ids.size() > PC_MAX_IMPLICITS) {
            invalid("base implicit count exceeds item capacity");
        }
        for (std::uint32_t mod_id : session.base_implicit_mod_ids) {
            pc_mod_slot& slot = item.implicits[item.implicit_count++];
            slot.mod_id = mod_id;
            slot.group_id =
                static_cast<std::uint16_t>(session.primary_group[mod_id]);
        }
    }

    item.quality =
        static_cast<std::uint8_t>(int_member(base_state, "quality", 0));
    item.item_flags =
        static_cast<std::uint8_t>(int_member(base_state, "item_flags", 0));
    if (bool_member(base_state, "corrupted", false)) {
        item.item_flags |= PC_ITEM_CORRUPTED;
    }
    if (bool_member(base_state, "mirrored", false)) {
        item.item_flags |= PC_ITEM_MIRRORED;
    }
    if (bool_member(base_state, "split", false)) {
        item.item_flags |= PC_ITEM_SPLIT;
    }
    if (bool_member(base_state, "synthesised", false)) {
        item.item_flags |= PC_ITEM_SYNTHESISED;
    }
    item.generic_influence_bits = static_cast<std::uint8_t>(
        int_member(base_state, "generic_influence_bits", 0));

    for (const auto& spec :
         (base_state.find("prefixes") != nullptr
              ? require_object_member(base_state, "prefixes", Type::Array).array
              : json::Array{})) {
        add_start_mod(session, item, spec, PC_SIDE_PREFIX);
    }
    for (const auto& spec :
         (base_state.find("suffixes") != nullptr
              ? require_object_member(base_state, "suffixes", Type::Array).array
              : json::Array{})) {
        add_start_mod(session, item, spec, PC_SIDE_SUFFIX);
    }
    return item;
}

CompiledCondition compile_condition(
    const SessionImpl& session,
    const Value& value,
    int depth = 0) {
    if (depth > 32) invalid("condition nesting is too deep");
    if (value.type != Type::Object) invalid("condition must be an object");
    const std::string type = string_member(value, "type");
    if (type.empty()) invalid("condition requires type");

    const DataImpl& data = *session.data;
    CompiledCondition out;
    if (type == "always") {
        out.kind = ConditionKind::Always;
        return out;
    }
    if (type == "has_mod_group") {
        const std::string group = string_member(value, "group");
        const auto it = data.group_id_by_key.find(group);
        if (it == data.group_id_by_key.end()) {
            invalid("unknown mod group: " + group);
        }
        out.kind = ConditionKind::HasModGroup;
        out.group_id = it->second;
        return out;
    }
    if (type == "has_mod_family") {
        std::string key = string_member(value, "family_mod_key");
        if (key.empty()) key = string_member(value, "mod_key");
        if (key.empty()) {
            invalid("has_mod_family requires family_mod_key");
        }
        const auto pos_it = data.mod_pos_by_key.find(key);
        if (pos_it == data.mod_pos_by_key.end()) {
            invalid("unknown modifier family key: " + key);
        }
        const std::uint32_t global_id = data.mod_global_ids[pos_it->second];
        const auto session_it =
            session.session_id_by_global_id.find(global_id);
        if (session_it == session.session_id_by_global_id.end()) {
            invalid("modifier family is not in the session: " + key);
        }
        const std::uint32_t mod_id = session_it->second;
        out.kind = ConditionKind::HasModFamily;
        out.family_id = session.family_id[mod_id];
        out.min_value = int_member(value, "min_tier", 0);
        if (bool_member(value, "fractured", false)) {
            out.required_flags |= PC_MOD_SLOT_FRACTURED;
        }
        if (out.min_value < 0) {
            invalid("has_mod_family min_tier must be non-negative");
        }
        return out;
    }
    if (type == "rarity_is") {
        std::string rarity = string_member(value, "rarity");
        if (rarity.empty()) rarity = string_member(value, "value");
        out.kind = ConditionKind::RarityIs;
        out.min_value = rarity_from_name(rarity);
        out.max_value = out.min_value;
        return out;
    }

    auto compile_range = [&](ConditionKind kind, int default_max) {
        out.kind = kind;
        out.min_value = int_member(
            value,
            "min",
            int_member(value, "value", int_member(value, "count", 0)));
        out.max_value = int_member(value, "max", default_max);
        if (out.min_value < 0 || out.max_value < out.min_value) {
            invalid("condition count range is invalid");
        }
    };
    if (type == "open_prefix_count") {
        compile_range(ConditionKind::OpenPrefixCount, PC_MAX_PREFIXES);
        return out;
    }
    if (type == "open_suffix_count") {
        compile_range(ConditionKind::OpenSuffixCount, PC_MAX_SUFFIXES);
        return out;
    }
    if (type == "prefix_count_range") {
        compile_range(ConditionKind::PrefixCountRange, PC_MAX_PREFIXES);
        return out;
    }
    if (type == "suffix_count_range") {
        compile_range(ConditionKind::SuffixCountRange, PC_MAX_SUFFIXES);
        return out;
    }

    const Value* children = value.find("conditions");
    if (children == nullptr) children = value.find("children");
    if (type == "all" || type == "all_of" || type == "any" ||
        type == "any_of" || type == "not" || type == "at_least") {
        if (children == nullptr || children->type != Type::Array) {
            invalid("composite condition requires conditions array");
        }
        if (children->array.size() > 1024) {
            invalid("composite condition has too many children");
        }
        if (type == "all" || type == "all_of") out.kind = ConditionKind::All;
        if (type == "any" || type == "any_of") out.kind = ConditionKind::Any;
        if (type == "not") out.kind = ConditionKind::Not;
        if (type == "at_least") {
            out.kind = ConditionKind::AtLeast;
            out.min_value = int_member(value, "count", 1);
        }
        for (const auto& child : children->array) {
            out.children.push_back(
                compile_condition(session, child, depth + 1));
        }
        if (out.kind == ConditionKind::Not && out.children.size() != 1) {
            invalid("not condition requires exactly one child");
        }
        if (out.kind == ConditionKind::AtLeast &&
            (out.min_value < 0 ||
             out.min_value > static_cast<int>(out.children.size()))) {
            invalid("at_least count is outside the child range");
        }
        return out;
    }
    invalid("unknown condition type: " + type);
}

bool action_type_from_name(const std::string& name, ActionType& out) {
    static const std::pair<const char*, ActionType> table[] = {
        {"transmute", ActionType::Transmute},
        {"augment", ActionType::Augment},
        {"alteration", ActionType::Alteration},
        {"regal", ActionType::Regal},
        {"alchemy", ActionType::Alchemy},
        {"chaos", ActionType::Chaos},
        {"exalt", ActionType::Exalt},
        {"annul", ActionType::Annul},
        {"scour", ActionType::Scour},
        {"essence", ActionType::Essence},
        {"fossil", ActionType::Fossil},
        {"bench", ActionType::Bench},
        {"veiled_chaos", ActionType::VeiledChaos},
        {"veiled_exalt", ActionType::VeiledExalt},
        {"unveil", ActionType::Unveil},
        {"harvest_reforge", ActionType::HarvestReforge},
        {"harvest_augment", ActionType::HarvestAugment},
        {"harvest_resist", ActionType::HarvestResist},
        {"eldritch_ember", ActionType::EldritchEmber},
        {"eldritch_ichor", ActionType::EldritchIchor},
        {"eldritch_exalt", ActionType::EldritchExalt},
        {"eldritch_chaos", ActionType::EldritchChaos},
        {"eldritch_annul", ActionType::EldritchAnnul},
        {"influence_exalt", ActionType::InfluenceExalt},
        {"fracture", ActionType::Fracture},
    };
    for (const auto& entry : table) {
        if (name == entry.first) {
            out = entry.second;
            return true;
        }
    }
    return false;
}

void compile_operation(
    const SessionImpl& session,
    const Value& operation,
    StrategyNode& node) {
    const std::string type = string_member(operation, "type");
    if (type == "condition_check_only") {
        node.kind = StrategyNodeKind::Router;
        node.action_type = -1;
        return;
    }
    if (type == "restart") {
        /* Buy a fresh base and keep crafting: no engine action runs; the
         * run loop resets the item in place. */
        node.action_type = kStrategyRestartOperation;
        node.price_keys = {"base"};
        return;
    }
    ActionType action_type;
    if (!action_type_from_name(type, action_type)) {
        invalid("unknown operation type: " + type);
    }
    node.action.type = action_type;
    node.action_type = static_cast<int>(action_type);
    node.price_keys = {type};

    const Value* params = operation.find("params");
    const Value& source =
        params != nullptr && params->type == Type::Object ? *params : operation;
    const DataImpl& data = *session.data;
    if (action_type == ActionType::Essence) {
        std::string key = string_member(source, "essence_key");
        if (key.empty()) key = string_member(source, "essence");
        if (key.empty()) invalid("essence operation requires essence_key");
        const auto it = data.essence_by_key.find(key);
        if (it == data.essence_by_key.end()) {
            invalid("unknown essence key: " + key);
        }
        node.action.essence_index = it->second;
        node.price_keys = {"essence:" + key};
    } else if (action_type == ActionType::Fossil) {
        const Value* fossils = source.find("fossils");
        if (fossils == nullptr || fossils->type != Type::Array ||
            fossils->array.empty() ||
            fossils->array.size() > PC_MAX_FOSSILS_PER_ACTION) {
            invalid("fossil operation requires 1-4 fossils");
        }
        std::vector<std::string> keys;
        for (const auto& entry : fossils->array) {
            if (entry.type != Type::String) {
                invalid("fossil keys must be strings");
            }
            const auto it = data.fossil_by_key.find(entry.string);
            if (it == data.fossil_by_key.end()) {
                invalid("unknown fossil key: " + entry.string);
            }
            node.action.fossil_indices.push_back(it->second);
            keys.push_back(entry.string);
        }
        std::sort(node.action.fossil_indices.begin(),
                  node.action.fossil_indices.end());
        node.action.fossil_indices.erase(
            std::unique(node.action.fossil_indices.begin(),
                        node.action.fossil_indices.end()),
            node.action.fossil_indices.end());
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        node.price_keys.clear();
        for (const auto& key : keys) {
            node.price_keys.push_back("fossil:" + key);
        }
        node.price_keys.push_back(
            "resonator:" + std::to_string(keys.size()));
    } else if (action_type == ActionType::Bench ||
               action_type == ActionType::Unveil) {
        const std::string key = string_member(source, "mod_key");
        if (key.empty()) invalid(type + " operation requires mod_key");
        const auto pos = data.mod_pos_by_key.find(key);
        if (pos == data.mod_pos_by_key.end())
            invalid("unknown mod key: " + key);
        const auto session_mod = session.session_id_by_global_id.find(
            data.mod_global_ids[pos->second]);
        if (session_mod == session.session_id_by_global_id.end())
            invalid("mod is unavailable in this session: " + key);
        node.action.mod_id = session_mod->second;
        node.price_keys = action_type == ActionType::Bench
                              ? std::vector<std::string>{"bench:" + key}
                              : std::vector<std::string>{"unveil"};
    } else if (action_type == ActionType::HarvestReforge ||
               action_type == ActionType::HarvestAugment) {
        std::string tag = string_member(source, "target_tag");
        if (tag.empty()) tag = string_member(source, "tag");
        const auto it = data.tag_id_by_name.find(tag);
        if (it == data.tag_id_by_name.end())
            invalid("unknown Harvest tag: " + tag);
        node.action.target_tag_id = it->second;
        node.price_keys = {type + ":" + tag};
    } else if (action_type == ActionType::HarvestResist) {
        const std::string source_tag = string_member(source, "source_tag");
        const std::string target_tag = string_member(source, "target_tag");
        const auto from = data.tag_id_by_name.find(source_tag);
        const auto to = data.tag_id_by_name.find(target_tag);
        if (from == data.tag_id_by_name.end() ||
            to == data.tag_id_by_name.end())
            invalid("unknown Harvest resistance tag");
        node.action.source_tag_id = from->second;
        node.action.target_tag_id = to->second;
        node.price_keys = {"harvest_resist"};
    } else if (action_type == ActionType::EldritchEmber ||
               action_type == ActionType::EldritchIchor) {
        node.action.tier =
            static_cast<std::uint32_t>(int_member(source, "tier", 0));
        if (node.action.tier < 1 || node.action.tier > 4)
            invalid("Eldritch tier must be 1-4");
        node.price_keys = {type + ":" + std::to_string(node.action.tier)};
    } else if (action_type == ActionType::InfluenceExalt) {
        const std::string influence = string_member(source, "influence");
        const auto it = data.influence_code_by_name.find(influence);
        if (it == data.influence_code_by_name.end() || it->second <= 0)
            invalid("unknown influence: " + influence);
        node.action.influence_code = it->second;
        node.price_keys = {"influence_exalt:" + influence};
    }
}

bool has_group(
    const SessionImpl& session,
    const pc_item_state& item,
    std::uint32_t group_id) {
    auto side_has = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t mod_id = slots[i].mod_id;
            if (mod_id >= session.mod_count) continue;
            for (std::uint32_t p = session.group_offsets[mod_id];
                 p < session.group_offsets[mod_id + 1]; ++p) {
                if (session.group_ids[p] == group_id) return true;
            }
        }
        return false;
    };
    return side_has(item.prefixes, item.prefix_count) ||
           side_has(item.suffixes, item.suffix_count);
}

bool has_family(
    const SessionImpl& session,
    const pc_item_state& item,
    std::uint32_t family_id,
    int min_tier,
    std::uint8_t required_flags) {
    auto side_has = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t mod_id = slots[i].mod_id;
            if (mod_id >= session.mod_count ||
                session.family_id[mod_id] != family_id) {
                continue;
            }
            if ((slots[i].flags & required_flags) != required_flags) continue;
            const std::uint32_t tier = session.family_tier_index[mod_id];
            if (min_tier == 0 ||
                (tier != 0 && tier <= static_cast<std::uint32_t>(min_tier))) {
                return true;
            }
        }
        return false;
    };
    return side_has(item.prefixes, item.prefix_count) ||
           side_has(item.suffixes, item.suffix_count);
}

bool evaluate_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item) {
    switch (condition.kind) {
    case ConditionKind::Always:
        return true;
    case ConditionKind::HasModGroup:
        return has_group(session, item, condition.group_id);
    case ConditionKind::HasModFamily:
        return has_family(
            session,
            item,
            condition.family_id,
            condition.min_value,
            condition.required_flags);
    case ConditionKind::RarityIs:
        return item.rarity == condition.min_value;
    case ConditionKind::OpenPrefixCount: {
        const int open =
            std::max(0, static_cast<int>(side_cap(session, item)) -
                            static_cast<int>(item.prefix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::OpenSuffixCount: {
        const int open =
            std::max(0, static_cast<int>(side_cap(session, item)) -
                            static_cast<int>(item.suffix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::PrefixCountRange:
        return item.prefix_count >= condition.min_value &&
               item.prefix_count <= condition.max_value;
    case ConditionKind::SuffixCountRange:
        return item.suffix_count >= condition.min_value &&
               item.suffix_count <= condition.max_value;
    case ConditionKind::All:
        return std::all_of(
            condition.children.begin(),
            condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_condition(child, session, item);
            });
    case ConditionKind::Any:
        return std::any_of(
            condition.children.begin(),
            condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_condition(child, session, item);
            });
    case ConditionKind::Not:
        return !evaluate_condition(condition.children.front(), session, item);
    case ConditionKind::AtLeast: {
        int matches = 0;
        for (const auto& child : condition.children) {
            if (evaluate_condition(child, session, item)) ++matches;
        }
        return matches >= condition.min_value;
    }
    }
    return false;
}

const StrategyEdge* select_edge(
    const StrategyNode& node,
    const SessionImpl& session,
    const pc_item_state& item) {
    const StrategyEdge* fallback = nullptr;
    for (const auto& edge : node.edges) {
        if (edge.is_default) {
            fallback = &edge;
        } else if (evaluate_condition(edge.condition, session, item)) {
            return &edge;
        }
    }
    return fallback;
}

bool options_equal(
    const SimulationOptionsInternal& a,
    const SimulationOptionsInternal& b) {
    return a.target_runs == b.target_runs && a.seed == b.seed &&
           a.max_actions_per_run == b.max_actions_per_run &&
           a.max_graph_steps_per_run == b.max_graph_steps_per_run &&
           a.max_cost_per_run == b.max_cost_per_run &&
           a.retained_trace_count == b.retained_trace_count &&
           a.max_trace_entries == b.max_trace_entries &&
           a.retained_success_count == b.retained_success_count &&
           a.retained_failure_count == b.retained_failure_count;
}

void append_trace(
    RetainedTrace* trace,
    const SimulationOptionsInternal& options,
    TraceEntryInternal entry,
    bool final_entry = false) {
    if (trace == nullptr || options.max_trace_entries == 0) return;
    if (trace->entries.size() < options.max_trace_entries) {
        trace->entries.push_back(std::move(entry));
    } else if (final_entry) {
        trace->entries.back() = std::move(entry);
    }
}

const char* failure_default_detail(int reason) {
    switch (reason) {
    case PC_SIM_FAILURE_TERMINAL: return "failure terminal";
    case PC_SIM_FAILURE_ACTION_LIMIT: return "action limit reached";
    case PC_SIM_FAILURE_COST_LIMIT: return "cost limit reached";
    case PC_SIM_FAILURE_STEP_LIMIT: return "graph step limit reached";
    case PC_SIM_FAILURE_NO_MATCHING_EDGE: return "no matching or default edge";
    case PC_SIM_FAILURE_ACTION_NOT_APPLIED: return "action was not applicable";
    case PC_SIM_FAILURE_MISSING_PRICE: return "cost limit requires a missing price";
    default: return "";
    }
}

bool action_needs_rollback(ActionType type) {
    // These reforges validate before mutation and always report applied once
    // entered; an empty pool simply yields fewer mods. Avoid copying the full
    // item state for their overwhelmingly common success path.
    switch (type) {
    case ActionType::Transmute:
    case ActionType::Alteration:
    case ActionType::Alchemy:
    case ActionType::Chaos:
        return false;
    default:
        return true;
    }
}

void aggregate_failure(
    SimulatorImpl& simulator,
    int reason,
    const std::string& node_id,
    const std::string& detail) {
    for (auto& entry : simulator.failure_summaries) {
        if (entry.failure_reason == reason && entry.node_id == node_id &&
            entry.detail == detail) {
            ++entry.count;
            return;
        }
    }
    simulator.failure_summaries.push_back(
        {reason, node_id, detail, 1});
}

struct RunResult {
    int terminal_kind = PC_TERMINAL_FAILURE;
    int failure_reason = PC_SIM_FAILURE_NONE;
    std::string terminal_node_id;
    std::string detail;
    std::uint64_t actions = 0;
    double known_cost = 0.0;
    bool cost_complete = true;
    pc_item_state item{};
};

RunResult run_one(SimulatorImpl& simulator, RetainedTrace* trace) {
    const StrategyImpl& strategy = *simulator.strategy;
    const SessionImpl& session = *simulator.session;
    const SimulationOptionsInternal& options = simulator.options;
    const std::uint64_t derived_steps =
        static_cast<std::uint64_t>(options.max_actions_per_run) * 4u +
        static_cast<std::uint64_t>(strategy.nodes.size()) * 4u + 16u;
    const std::uint64_t max_steps =
        options.max_graph_steps_per_run != 0
            ? options.max_graph_steps_per_run
            : std::min<std::uint64_t>(
                  derived_steps, std::numeric_limits<std::uint32_t>::max());

    RunResult result;
    result.item = strategy.start_item;
    std::uint32_t node_index = strategy.start_node;
    std::uint64_t graph_steps = 0;
    bool run_missing_price = false;

    auto finish_failure = [&](int reason,
                              const StrategyNode& node,
                              const std::string& detail,
                              int terminal_kind = PC_TERMINAL_FAILURE) {
        result.terminal_kind = terminal_kind;
        result.failure_reason = reason;
        result.terminal_node_id = node.id;
        result.detail = detail.empty() ? failure_default_detail(reason) : detail;
        if (trace != nullptr && options.max_trace_entries != 0) {
            TraceEntryInternal entry;
            entry.step_index = static_cast<std::uint32_t>(graph_steps);
            entry.node_id = node.id;
            entry.node_kind = static_cast<int>(node.kind);
            entry.action_type = node.action_type;
            entry.cumulative_actions = result.actions;
            entry.known_cumulative_cost = result.known_cost;
            entry.cost_complete = result.cost_complete;
            entry.terminal_kind = terminal_kind;
            entry.failure_reason = reason;
            entry.item = result.item;
            append_trace(trace, options, std::move(entry), true);
        }
    };

    while (true) {
        const StrategyNode& node = strategy.nodes[node_index];
        ++graph_steps;
        if (graph_steps > max_steps) {
            finish_failure(PC_SIM_FAILURE_STEP_LIMIT, node, "");
            break;
        }

        if (node.kind == StrategyNodeKind::Terminal) {
            result.terminal_kind = node.terminal_kind;
            result.terminal_node_id = node.id;
            if (node.terminal_kind == PC_TERMINAL_FAILURE) {
                result.failure_reason = PC_SIM_FAILURE_TERMINAL;
                result.detail =
                    node.reason.empty() ? "failure terminal" : node.reason;
            } else if (node.terminal_kind == PC_TERMINAL_STOP) {
                result.failure_reason = PC_SIM_FAILURE_TERMINAL;
                result.detail = node.reason.empty() ? "stop terminal" : node.reason;
            }
            if (trace != nullptr && options.max_trace_entries != 0) {
                TraceEntryInternal entry;
                entry.step_index =
                    static_cast<std::uint32_t>(graph_steps - 1);
                entry.node_id = node.id;
                entry.node_kind = static_cast<int>(node.kind);
                entry.cumulative_actions = result.actions;
                entry.known_cumulative_cost = result.known_cost;
                entry.cost_complete = result.cost_complete;
                entry.terminal_kind = node.terminal_kind;
                entry.failure_reason = result.failure_reason;
                entry.item = result.item;
                append_trace(trace, options, std::move(entry), true);
            }
            break;
        }

        bool applied = false;
        if (node.kind == StrategyNodeKind::Operation) {
            if (result.actions >= options.max_actions_per_run) {
                finish_failure(PC_SIM_FAILURE_ACTION_LIMIT, node, "");
                break;
            }

            bool price_known = simulator.economy == nullptr;
            double price = 0.0;
            std::vector<std::string> missing_keys;
            if (simulator.economy != nullptr) {
                price_known = true;
                for (const auto& price_key : node.price_keys) {
                    const auto price_it =
                        simulator.economy->prices.find(price_key);
                    if (price_it != simulator.economy->prices.end()) {
                        price += price_it->second;
                    } else {
                        price_known = false;
                        missing_keys.push_back(price_key);
                    }
                }
            }

            if (!price_known && options.max_cost_per_run > 0.0) {
                for (const auto& price_key : missing_keys) {
                    ++simulator.missing_prices[price_key];
                }
                result.cost_complete = false;
                run_missing_price = true;
                ++simulator.summary.missing_price_action_count;
                finish_failure(PC_SIM_FAILURE_MISSING_PRICE, node, "");
                break;
            }
            if (price_known && simulator.economy != nullptr &&
                options.max_cost_per_run > 0.0 &&
                result.known_cost + price > options.max_cost_per_run) {
                finish_failure(PC_SIM_FAILURE_COST_LIMIT, node, "");
                break;
            }

            ActionOutcome outcome;
            if (node.action_type == kStrategyRestartOperation) {
                const int removed =
                    result.item.prefix_count + result.item.suffix_count;
                pc_item_clear(&result.item);
                if (session.base_implicit_mod_ids.size() <=
                    PC_MAX_IMPLICITS) {
                    for (std::uint32_t mod_id :
                         session.base_implicit_mod_ids) {
                        pc_mod_slot& slot =
                            result.item.implicits[
                                result.item.implicit_count++];
                        slot.mod_id = mod_id;
                        slot.group_id = static_cast<std::uint16_t>(
                            session.primary_group[mod_id]);
                    }
                }
                outcome.applied = true;
                outcome.removed = removed;
            } else {
                pc_item_state rollback;
                const bool needs_rollback =
                    action_needs_rollback(node.action.type);
                if (needs_rollback) rollback = result.item;
                outcome = apply_action(*simulator.context, &result.item,
                                       node.action);
                if (!outcome.applied && needs_rollback) {
                    result.item = rollback;
                }
            }
            ++result.actions;
            ++simulator.action_counts[node_index];
            if (!outcome.applied) {
                finish_failure(PC_SIM_FAILURE_ACTION_NOT_APPLIED, node, "");
                break;
            }

            applied = true;
            if (price_known && simulator.economy != nullptr) {
                result.known_cost += price;
                ++simulator.summary.costed_action_count;
            } else if (simulator.economy != nullptr) {
                for (const auto& price_key : missing_keys) {
                    ++simulator.missing_prices[price_key];
                }
                result.cost_complete = false;
                run_missing_price = true;
                ++simulator.summary.missing_price_action_count;
            }
        }

        const StrategyEdge* edge = select_edge(node, session, result.item);
        if (trace != nullptr && options.max_trace_entries != 0) {
            TraceEntryInternal entry;
            entry.step_index = static_cast<std::uint32_t>(graph_steps - 1);
            entry.node_id = node.id;
            entry.node_kind = static_cast<int>(node.kind);
            entry.action_type = node.action_type;
            entry.action_applied = applied;
            entry.matched_edge_id = edge != nullptr ? edge->id : "";
            entry.cumulative_actions = result.actions;
            entry.known_cumulative_cost = result.known_cost;
            entry.cost_complete = result.cost_complete;
            entry.item = result.item;
            append_trace(trace, options, std::move(entry));
        }
        if (edge == nullptr) {
            finish_failure(PC_SIM_FAILURE_NO_MATCHING_EDGE, node, "");
            break;
        }
        node_index = edge->target;
    }

    if (run_missing_price) {
        ++simulator.summary.missing_price_run_count;
    }
    return result;
}

} // namespace

std::shared_ptr<StrategyImpl> compile_strategy_json(
    std::shared_ptr<const SessionImpl> session,
    const char* strategy_json,
    std::size_t strategy_json_size) {
    if (session == nullptr || strategy_json == nullptr) {
        invalid("null compile input");
    }
    Value root = json::Parser(strategy_json, strategy_json_size).parse();
    if (root.type != Type::Object) invalid("root must be an object");
    if (string_member(root, "version") != "v1") {
        invalid("version must be v1");
    }

    auto strategy = std::make_shared<StrategyImpl>();
    strategy->session = std::move(session);
    strategy->name = string_member(root, "name");
    strategy->start_item = parse_start_item(
        *strategy->session,
        require_object_member(root, "base_state", Type::Object));

    const Value& nodes = require_object_member(root, "nodes", Type::Array);
    if (nodes.array.empty()) invalid("nodes must not be empty");
    if (nodes.array.size() > 10000) invalid("node count exceeds safety limit");
    strategy->nodes.reserve(nodes.array.size());
    for (const auto& node_value : nodes.array) {
        if (node_value.type != Type::Object) invalid("node must be an object");
        StrategyNode node;
        node.id = string_member(node_value, "id");
        if (node.id.empty()) invalid("node requires a non-empty id");
        if (strategy->node_by_id.count(node.id) != 0) {
            invalid("duplicate node id: " + node.id);
        }
        const std::string kind = string_member(node_value, "kind");
        if (kind == "start") {
            node.kind = StrategyNodeKind::Start;
        } else if (kind == "operation") {
            node.kind = StrategyNodeKind::Operation;
            compile_operation(
                *strategy->session,
                require_object_member(
                    node_value, "operation", Type::Object),
                node);
        } else if (kind == "router") {
            node.kind = StrategyNodeKind::Router;
        } else if (kind == "terminal") {
            node.kind = StrategyNodeKind::Terminal;
            const std::string terminal = string_member(node_value, "terminal");
            if (terminal == "success") {
                node.terminal_kind = PC_TERMINAL_SUCCESS;
            } else if (terminal == "failure") {
                node.terminal_kind = PC_TERMINAL_FAILURE;
            } else if (terminal == "stop") {
                node.terminal_kind = PC_TERMINAL_STOP;
            } else {
                invalid("unknown terminal kind: " + terminal);
            }
            node.reason = string_member(node_value, "reason");
        } else {
            invalid("unknown node kind: " + kind);
        }
        strategy->node_by_id.emplace(
            node.id, static_cast<std::uint32_t>(strategy->nodes.size()));
        strategy->nodes.push_back(std::move(node));
    }

    const std::string start_id = string_member(root, "start_node_id");
    const auto start_it = strategy->node_by_id.find(start_id);
    if (start_it == strategy->node_by_id.end() ||
        strategy->nodes[start_it->second].kind != StrategyNodeKind::Start) {
        invalid("start_node_id must reference a start node");
    }
    strategy->start_node = start_it->second;
    std::size_t start_count = 0;
    for (const auto& node : strategy->nodes) {
        if (node.kind == StrategyNodeKind::Start) ++start_count;
    }
    if (start_count != 1) invalid("strategy must contain exactly one start node");

    const Value& edges = require_object_member(root, "edges", Type::Array);
    if (edges.array.size() > 50000) invalid("edge count exceeds safety limit");
    std::unordered_map<std::string, bool> edge_ids;
    std::vector<std::uint32_t> default_counts(strategy->nodes.size(), 0);
    std::uint32_t source_order = 0;
    for (const auto& edge_value : edges.array) {
        if (edge_value.type != Type::Object) invalid("edge must be an object");
        StrategyEdge edge;
        edge.id = string_member(edge_value, "id");
        if (edge.id.empty() || edge_ids.count(edge.id) != 0) {
            invalid("edge ids must be non-empty and unique");
        }
        edge_ids.emplace(edge.id, true);
        const std::string from = string_member(edge_value, "from");
        const std::string to = string_member(edge_value, "to");
        const auto from_it = strategy->node_by_id.find(from);
        const auto to_it = strategy->node_by_id.find(to);
        if (from_it == strategy->node_by_id.end() ||
            to_it == strategy->node_by_id.end()) {
            invalid("edge endpoint does not reference a node");
        }
        if (strategy->nodes[from_it->second].kind ==
            StrategyNodeKind::Terminal) {
            invalid("terminal nodes cannot have outgoing edges");
        }
        edge.target = to_it->second;
        edge.priority = int_member(edge_value, "priority", 0);
        edge.is_default = bool_member(edge_value, "is_default", false);
        edge.source_order = source_order++;
        if (edge.is_default) {
            if (++default_counts[from_it->second] > 1) {
                invalid("a node may have at most one default edge");
            }
            edge.condition.kind = ConditionKind::Always;
        } else {
            const Value* condition = edge_value.find("condition");
            edge.condition =
                condition != nullptr
                    ? compile_condition(*strategy->session, *condition)
                    : CompiledCondition{};
        }
        strategy->nodes[from_it->second].edges.push_back(std::move(edge));
    }

    for (auto& node : strategy->nodes) {
        std::stable_sort(
            node.edges.begin(),
            node.edges.end(),
            [](const StrategyEdge& a, const StrategyEdge& b) {
                if (a.priority != b.priority) return a.priority < b.priority;
                return a.source_order < b.source_order;
            });
    }
    return strategy;
}

std::shared_ptr<EconomyImpl> load_economy_json(
    const char* economy_json,
    std::size_t economy_json_size) {
    if (economy_json == nullptr) {
        throw std::runtime_error("economy: null JSON");
    }
    Value root = json::Parser(economy_json, economy_json_size).parse();
    if (root.type != Type::Object) {
        throw std::runtime_error("economy: root must be an object");
    }
    const Value* version = root.find("version");
    if (version != nullptr &&
        (version->type != Type::String || version->string != "v1")) {
        throw std::runtime_error("economy: version must be v1");
    }
    const Value* prices = root.find("prices");
    if (prices == nullptr) prices = &root;
    if (prices->type != Type::Object) {
        throw std::runtime_error("economy: prices must be an object");
    }

    auto economy = std::make_shared<EconomyImpl>();
    const Value* id = root.find("id");
    if (id != nullptr && id->type == Type::String) economy->id = id->string;
    for (const auto& member : prices->object) {
        if (member.first == "version" || member.first == "id" ||
            member.first == "prices") {
            continue;
        }
        if (member.second.type != Type::Number ||
            !std::isfinite(member.second.number) ||
            member.second.number < 0.0) {
            throw std::runtime_error(
                "economy: price must be a finite non-negative number: " +
                member.first);
        }
        economy->prices[member.first] = member.second.number;
    }
    return economy;
}

void run_simulator_chunk(
    SimulatorImpl& simulator,
    const SimulationOptionsInternal& options,
    std::uint32_t max_completed_runs) {
    if (!simulator.options_set) {
        simulator.options = options;
        simulator.options_set = true;
        simulator.context = std::make_unique<ActionContextImpl>(options.seed);
        simulator.context->session = simulator.session;
        simulator.context->capture_action_trace = false;
    } else if (!options_equal(simulator.options, options)) {
        throw std::runtime_error(
            "simulation options changed after the first chunk");
    }

    const std::uint64_t remaining =
        simulator.options.target_runs - simulator.summary.completed_runs;
    const std::uint64_t run_count =
        std::min<std::uint64_t>(remaining, max_completed_runs);
    for (std::uint64_t i = 0; i < run_count; ++i) {
        RetainedTrace trace;
        RetainedTrace* trace_ptr =
            simulator.traces.size() < simulator.options.retained_trace_count
                ? &trace
                : nullptr;
        RunResult result = run_one(simulator, trace_ptr);
        if (trace_ptr != nullptr) {
            simulator.traces.push_back(std::move(trace));
        }

        ++simulator.summary.completed_runs;
        simulator.summary.total_actions += result.actions;
        simulator.summary.known_total_cost += result.known_cost;
        if (result.terminal_kind == PC_TERMINAL_SUCCESS) {
            ++simulator.summary.success_count;
        } else if (result.terminal_kind == PC_TERMINAL_STOP) {
            ++simulator.summary.stop_count;
        } else {
            ++simulator.summary.failure_count;
        }
        switch (result.failure_reason) {
        case PC_SIM_FAILURE_ACTION_LIMIT:
            ++simulator.summary.action_limit_count;
            break;
        case PC_SIM_FAILURE_COST_LIMIT:
            ++simulator.summary.cost_limit_count;
            break;
        case PC_SIM_FAILURE_STEP_LIMIT:
            ++simulator.summary.step_limit_count;
            break;
        case PC_SIM_FAILURE_NO_MATCHING_EDGE:
            ++simulator.summary.no_matching_edge_count;
            break;
        case PC_SIM_FAILURE_ACTION_NOT_APPLIED:
            ++simulator.summary.action_not_applied_count;
            break;
        default:
            break;
        }
        if (result.terminal_kind != PC_TERMINAL_SUCCESS) {
            aggregate_failure(
                simulator,
                result.failure_reason,
                result.terminal_node_id,
                result.detail);
        }

        const bool retain_success =
            result.terminal_kind == PC_TERMINAL_SUCCESS &&
            simulator.success_examples.size() <
                simulator.options.retained_success_count;
        const bool retain_failure =
            result.terminal_kind != PC_TERMINAL_SUCCESS &&
            simulator.failure_examples.size() <
                simulator.options.retained_failure_count;
        if (retain_success || retain_failure) {
            SimulationExampleInternal example;
            example.terminal_kind = result.terminal_kind;
            example.failure_reason = result.failure_reason;
            example.terminal_node_id = result.terminal_node_id;
            example.action_count = result.actions;
            example.known_total_cost = result.known_cost;
            example.cost_complete = result.cost_complete;
            example.item = result.item;
            if (retain_success) {
                simulator.success_examples.push_back(std::move(example));
            } else {
                simulator.failure_examples.push_back(std::move(example));
            }
        }
    }
}

} // namespace poecraft
