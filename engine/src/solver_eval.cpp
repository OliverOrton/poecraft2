#include "solver_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "json.hpp"
#include "poecraft/bitset.h"

namespace poecraft {
namespace solver {

namespace {

using json::Parser;
using json::Type;
using json::Value;

struct ReviewSectionSpec {
    std::string id;
    std::string label;
    std::string role;
    std::vector<std::uint32_t> nodes;
    std::vector<std::string> edges;
};

struct TargetEntry {
    GoalSlot slot;
    std::string origin;
};

struct EvalModel {
    std::unique_ptr<CalcContext> calc;
    std::vector<std::uint32_t> action_by_node;
    std::vector<GoalSlot> targets;
};

enum class EvalAbsorptionKind {
    Terminal,
    ActionNotApplied,
    NoMatchingEdge,
};

struct EvalTransition {
    std::uint32_t target = kNoId;
    double probability = 0.0;
    std::uint32_t edge = kNoId;
    /* When pass-through contraction rewrites this transition, the pair it
     * originally entered (the head of the folded deterministic chain).
     * Flow committed through the transition is credited to that chain. */
    std::uint32_t via = kNoId;
};

struct EvalAbsorption {
    EvalAbsorptionKind kind = EvalAbsorptionKind::Terminal;
    std::uint32_t node = kNoId;
    std::uint32_t state = kNoId;
    double probability = 0.0;
    std::uint32_t edge = kNoId;
};

struct EvalRow {
    std::vector<EvalTransition> transitions;
    std::vector<EvalAbsorption> absorptions;
};

struct EvalPair {
    std::uint32_t node = kNoId;
    std::uint32_t state = kNoId;
    bool operation = false;
    bool consumes = false;
    std::uint32_t action = kNoId;
    std::uint32_t row = kNoId;
};

std::uint64_t eval_pair_key(std::uint32_t node, std::uint32_t state) {
    return (static_cast<std::uint64_t>(node) << 32) | state;
}

void add_gap(std::vector<std::string>& gaps, const std::string& gap) {
    if (std::find(gaps.begin(), gaps.end(), gap) == gaps.end()) {
        gaps.push_back(gap);
    }
}

std::string join_gaps(const std::vector<std::string>& gaps) {
    std::string message = "strategy evaluation unsupported:";
    for (const std::string& gap : gaps) {
        message += "\n- " + gap;
    }
    return message;
}

bool same_action_parameters(
    const ActionParameters& compiled,
    const ActionParameters& descriptor) {
    if (compiled.type != descriptor.type) return false;
    switch (compiled.type) {
    case ActionType::Essence:
        return compiled.essence_index == descriptor.essence_index;
    case ActionType::Fossil:
        return compiled.fossil_indices == descriptor.fossil_indices;
    case ActionType::Bench:
        return compiled.mod_id == descriptor.mod_id;
    case ActionType::Unveil:
        /* The registry's one "unveil" descriptor represents choosing the
         * authored unveil result, which is carried only by the graph node. */
        return true;
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
        return compiled.target_tag_id == descriptor.target_tag_id;
    case ActionType::HarvestResist:
        return compiled.source_tag_id == descriptor.source_tag_id &&
               compiled.target_tag_id == descriptor.target_tag_id;
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
        return compiled.tier == descriptor.tier;
    case ActionType::InfluenceExalt:
        return compiled.influence_code == descriptor.influence_code;
    default:
        return true;
    }
}

void collect_condition_targets(
    const CompiledCondition& condition,
    const std::string& edge_id,
    std::vector<TargetEntry>& targets,
    std::vector<std::string>& gaps) {
    if (condition.kind == ConditionKind::HasModFamily) {
        auto found = std::find_if(
            targets.begin(), targets.end(), [&](const TargetEntry& target) {
                return target.slot.family_id == condition.family_id;
            });
        const std::uint32_t threshold = static_cast<std::uint32_t>(
            std::max(0, condition.min_value));
        if (found == targets.end()) {
            TargetEntry target;
            target.slot.family_id = condition.family_id;
            target.slot.min_tier = threshold;
            target.origin = edge_id;
            targets.push_back(std::move(target));
        } else if (threshold != 0) {
            if (found->slot.min_tier != 0 &&
                found->slot.min_tier != threshold) {
                add_gap(
                    gaps,
                    "edge '" + edge_id + "' and edge '" + found->origin +
                        "' use different non-zero tier thresholds for "
                        "family " + std::to_string(condition.family_id) +
                        "; align the tiers");
            } else {
                found->slot.min_tier = threshold;
            }
        }
    } else if (condition.kind == ConditionKind::HasModGroup) {
        const auto found = std::find_if(
            targets.begin(), targets.end(), [&](const TargetEntry& target) {
                return target.slot.group_id == condition.group_id;
            });
        if (found == targets.end()) {
            TargetEntry target;
            target.slot.group_id = condition.group_id;
            target.slot.min_tier = static_cast<std::uint32_t>(
                std::max(0, condition.min_value));
            target.origin = edge_id;
            targets.push_back(std::move(target));
        } else if (condition.min_value != 0) {
            const std::uint32_t threshold = static_cast<std::uint32_t>(
                condition.min_value);
            if (found->slot.min_tier != 0 &&
                found->slot.min_tier != threshold) {
                add_gap(
                    gaps,
                    "edge '" + edge_id + "' and edge '" + found->origin +
                        "' use different non-zero tier thresholds for "
                        "group " + std::to_string(condition.group_id) +
                        "; align the tiers");
            } else {
                found->slot.min_tier = threshold;
            }
        }
    } else if (condition.kind == ConditionKind::ModCount ||
               condition.kind == ConditionKind::ModFamilyCount) {
        add_gap(
            gaps,
            "edge '" + edge_id +
                "' compiler-only mod_count cannot be represented by "
                "Calculator mode yet");
    } else if (condition.kind == ConditionKind::HasUnveilOption) {
        add_gap(
            gaps,
            "edge '" + edge_id +
                "' unveil-option routing cannot be represented by "
                "Calculator mode yet");
    }
    for (const CompiledCondition& child : condition.children) {
        collect_condition_targets(child, edge_id, targets, gaps);
    }
}

bool target_contains_mod(
    const SessionImpl& session,
    const GoalSlot& target,
    std::uint32_t mod) {
    if (target.family_id != kNoId) {
        return mod < session.family_id.size() &&
               session.family_id[mod] == target.family_id;
    }
    if (target.group_id >= session.group_masks.size() ||
        session.group_masks[target.group_id].empty()) {
        return false;
    }
    return pc_bitset_test(
        session.group_masks[target.group_id].data(), mod);
}

bool targets_overlap(
    const SessionImpl& session,
    const GoalSlot& a,
    const GoalSlot& b) {
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        if (target_contains_mod(session, a, mod) &&
            target_contains_mod(session, b, mod)) {
            return true;
        }
    }
    return false;
}

EvalModel derive_model(
    const StrategyImpl& strategy,
    std::optional<std::uint32_t> state_cap) {
    const auto session = strategy.session;
    ActionRegistry registry = build_action_registry(*session);
    std::vector<std::string> gaps;
    std::vector<std::uint32_t> action_by_node(
        strategy.nodes.size(), kNoId);
    std::vector<std::uint32_t> used_actions;

    for (std::size_t i = 0; i < strategy.nodes.size(); ++i) {
        const StrategyNode& node = strategy.nodes[i];
        if (node.kind != StrategyNodeKind::Operation) continue;
        const std::uint32_t action = resolve_strategy_action(node, registry);
        action_by_node[i] = action;
        if (action == kNoId) {
            add_gap(
                gaps,
                "node '" + node.id +
                    "' operation does not resolve to a calculator action");
            continue;
        }
        const ActionDescriptor& descriptor = registry.actions[action];
        if (descriptor.params.type == ActionType::Unveil) {
            add_gap(
                gaps,
                "node '" + node.id +
                    "' authored unveil selection depends on its concrete "
                    "offered options");
        }
        if (!calc_supports(descriptor)) {
            add_gap(
                gaps,
                "node '" + node.id + "' operation '" + descriptor.id +
                    "' has no exact calculator evaluator");
        }
        if (descriptor.cost_keys != node.price_keys) {
            throw std::runtime_error(
                "strategy evaluation price-key mismatch at node '" +
                node.id + "'");
        }
        if (std::find(used_actions.begin(), used_actions.end(), action) ==
            used_actions.end()) {
            used_actions.push_back(action);
        }
    }

    std::vector<TargetEntry> target_entries;
    for (const StrategyNode& node : strategy.nodes) {
        for (const StrategyEdge& edge : node.edges) {
            if (!edge.is_default) {
                collect_condition_targets(
                    edge.condition, edge.id, target_entries, gaps);
            }
        }
    }
    if (target_entries.size() > kMaxGoalSlots) {
        const std::string offender =
            target_entries[kMaxGoalSlots].origin;
        add_gap(
            gaps,
            "edge '" + offender + "' brings the graph to " +
                std::to_string(target_entries.size()) +
                " distinct condition targets; the exact limit is " +
                std::to_string(kMaxGoalSlots));
    }
    for (std::size_t a = 0; a < target_entries.size(); ++a) {
        for (std::size_t b = a + 1; b < target_entries.size(); ++b) {
            if (targets_overlap(
                    *session, target_entries[a].slot,
                    target_entries[b].slot)) {
                add_gap(
                    gaps,
                    "edge '" + target_entries[a].origin + "' and edge '" +
                        target_entries[b].origin +
                        "' reference overlapping family/group targets; "
                        "align the conditions");
            }
        }
    }
    if (!gaps.empty()) throw StrategyEvalUnsupported(join_gaps(gaps));

    GoalSpec goal;
    for (const TargetEntry& target : target_entries) {
        goal.slots.push_back(target.slot);
    }
    goal.min_satisfied_slots =
        static_cast<std::uint32_t>(goal.slots.size());

    EvalModel model;
    model.action_by_node = std::move(action_by_node);
    model.targets = goal.slots;
    try {
        model.calc = std::make_unique<CalcContext>(
            session, goal, std::move(registry), used_actions,
            true,  /* allow count/rarity-only graphs */
            false, /* no operations must not mean the full registry */
            true,  /* preserve exact group effects between graph actions */
            state_cap);
    } catch (const std::exception& ex) {
        std::string origin;
        for (const TargetEntry& target : target_entries) {
            if (!origin.empty()) origin += ", ";
            origin += "'" + target.origin + "'";
        }
        throw StrategyEvalUnsupported(
            "strategy evaluation unsupported:\n- condition targets on "
            "edge(s) " + origin + " cannot share one exact abstraction: " +
            ex.what());
    }
    return model;
}

EvalModel derive_checked_model(
    const std::shared_ptr<const StrategyImpl>& strategy,
    std::uint32_t max_states) {
    if (strategy == nullptr) {
        throw std::invalid_argument("invalid compiled strategy");
    }
    return derive_model(*strategy, max_states);
}

std::size_t layout_slot_for(
    const CompiledCondition& condition,
    const AbstractLayout& layout) {
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        const GoalSlot& slot = layout.slots[i].spec;
        if (condition.kind == ConditionKind::HasModFamily &&
            slot.family_id == condition.family_id) {
            return i;
        }
        if (condition.kind == ConditionKind::HasModGroup &&
            slot.group_id == condition.group_id) {
            return i;
        }
    }
    throw std::logic_error("compiled evaluation condition target is absent");
}

double absorbed_probability(const StrategyEvalResult& result) {
    return result.success_probability + result.failure_probability +
           result.stop_probability +
           result.action_not_applied_probability +
           result.no_matching_edge_probability;
}

std::string json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                const char* hex = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0xf]);
                out.push_back(hex[c & 0xf]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

void append_number(std::string& out, double value) {
    if (value == 0.0) value = 0.0; /* canonicalize negative zero */
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    out += stream.str();
}

const char* terminal_name(int kind) {
    switch (kind) {
    case PC_TERMINAL_SUCCESS: return "success";
    case PC_TERMINAL_FAILURE: return "failure";
    case PC_TERMINAL_STOP: return "stop";
    default: return "failure";
    }
}

void add_classification(
    std::vector<std::string>& classifications,
    const std::string& value) {
    if (std::find(
            classifications.begin(), classifications.end(), value) ==
        classifications.end()) {
        classifications.push_back(value);
    }
}

std::map<std::string, double> empty_technique_totals() {
    return {
        {"ordinary_crafting_actions", 0.0},
        {"restart_actions", 0.0},
        {"base_consumptions", 0.0},
        {"fracture_preparation_actions", 0.0},
        {"fracture_actions", 0.0},
        {"retry_actions", 0.0},
        {"retry_count", 0.0},
        {"temporary_blocker_applications", 0.0},
        {"permanent_goal_bench_finishes", 0.0},
        {"multimod_setup_actions", 0.0},
        {"multimod_finishing_bench_actions", 0.0},
        {"protection_setup_actions", 0.0},
        {"protection_reapplications", 0.0},
        {"crafted_mod_cleanup_or_replacement_actions", 0.0},
        {"deterministic_finishing_actions", 0.0},
    };
}

void add_role_work(
    std::map<std::string, double>& totals,
    const std::string& role,
    double visits,
    double applied) {
    if (role == "ordinary_crafting") {
        totals["ordinary_crafting_actions"] += visits;
    } else if (role == "restart") {
        totals["restart_actions"] += visits;
        totals["base_consumptions"] += applied;
    } else if (role == "fracture_preparation") {
        totals["fracture_preparation_actions"] += visits;
    } else if (role == "fracture") {
        totals["fracture_actions"] += visits;
    } else if (role == "retry_action") {
        totals["retry_actions"] += visits;
    } else if (role == "retry") {
        totals["retry_count"] += visits;
    } else if (role == "temporary_blocker") {
        totals["temporary_blocker_applications"] += applied;
    } else if (role == "permanent_goal_bench") {
        totals["permanent_goal_bench_finishes"] += applied;
    } else if (role == "multimod_setup") {
        totals["multimod_setup_actions"] += applied;
    } else if (role == "multimod_finish") {
        totals["multimod_finishing_bench_actions"] += applied;
    } else if (role == "protection_setup") {
        totals["protection_setup_actions"] += applied;
    } else if (role == "protection_reapplication") {
        totals["retry_count"] += visits;
        totals["protection_reapplications"] += visits;
    } else if (role == "cleanup_or_replacement") {
        totals["crafted_mod_cleanup_or_replacement_actions"] += applied;
    } else if (role == "deterministic_finish") {
        totals["deterministic_finishing_actions"] += applied;
    }
}

std::vector<StrategyEvalMaterialTotal> price_materials(
    const std::map<std::string, double>& quantities,
    const std::shared_ptr<const EconomyImpl>& economy,
    double& known_cost,
    bool& complete) {
    known_cost = 0.0;
    complete = economy != nullptr;
    std::vector<StrategyEvalMaterialTotal> materials;
    materials.reserve(quantities.size());
    for (const auto& [key, quantity] : quantities) {
        StrategyEvalMaterialTotal material;
        material.price_key = key;
        material.expected_quantity = quantity;
        if (economy != nullptr) {
            const auto found = economy->prices.find(key);
            if (found != economy->prices.end()) {
                material.priced = true;
                material.unit_price = found->second;
                material.cost_contribution = quantity * found->second;
                known_cost += material.cost_contribution;
            } else {
                complete = false;
            }
        }
        materials.push_back(std::move(material));
    }
    return materials;
}

std::vector<ReviewSectionSpec> parse_review_sections(
    const StrategyImpl& strategy,
    const std::string& document) {
    if (document.empty()) return {};
    Value root = Parser(document.data(), document.size()).parse();
    if (root.type != Type::Object) {
        throw std::invalid_argument("review projection root must be an object");
    }
    const Value* schema = root.find("schema_version");
    if (schema == nullptr || schema->type != Type::String ||
        schema->string != "solver_review_projection_v1") {
        throw std::invalid_argument(
            "review projection must use solver_review_projection_v1");
    }
    const Value* raw = root.find("raw_strategy");
    if (raw == nullptr || raw->type != Type::Object) {
        throw std::invalid_argument("review projection requires raw_strategy");
    }
    const Value* authority = raw->find("execution_authority");
    if (authority == nullptr || authority->type != Type::String ||
        authority->string != "raw_strategy_only") {
        throw std::invalid_argument(
            "review projection cannot have execution authority");
    }
    const Value* sections = root.find("sections");
    if (sections == nullptr || sections->type != Type::Array ||
        sections->array.empty()) {
        throw std::invalid_argument("review projection requires sections");
    }

    std::unordered_map<std::string, std::uint32_t> node_by_id;
    std::unordered_set<std::string> edge_ids;
    for (std::uint32_t node = 0; node < strategy.nodes.size(); ++node) {
        node_by_id.emplace(strategy.nodes[node].id, node);
        for (const StrategyEdge& edge : strategy.nodes[node].edges) {
            edge_ids.emplace(edge.id);
        }
    }
    std::unordered_set<std::string> seen_sections;
    std::unordered_set<std::string> seen_nodes;
    std::unordered_set<std::string> seen_edges;
    std::vector<ReviewSectionSpec> result;
    for (const Value& section_value : sections->array) {
        if (section_value.type != Type::Object) {
            throw std::invalid_argument("review section must be an object");
        }
        const auto required_string = [&](const char* key) -> std::string {
            const Value* value = section_value.find(key);
            if (value == nullptr || value->type != Type::String ||
                value->string.empty()) {
                throw std::invalid_argument(
                    std::string("review section requires ") + key);
            }
            return value->string;
        };
        ReviewSectionSpec section;
        section.id = required_string("id");
        section.label = required_string("label");
        section.role = required_string("role");
        if (!seen_sections.emplace(section.id).second) {
            throw std::invalid_argument("duplicate review section id");
        }
        const Value* references = section_value.find("raw_references");
        if (references == nullptr || references->type != Type::Array) {
            throw std::invalid_argument(
                "review section requires raw_references");
        }
        for (const Value& reference : references->array) {
            if (reference.type != Type::Object) {
                throw std::invalid_argument(
                    "review raw reference must be an object");
            }
            const Value* node_id = reference.find("node_id");
            const Value* edge_id = reference.find("edge_id");
            if ((node_id == nullptr) == (edge_id == nullptr)) {
                throw std::invalid_argument(
                    "review reference must name one raw node or edge");
            }
            if (node_id != nullptr) {
                if (node_id->type != Type::String ||
                    !node_by_id.contains(node_id->string) ||
                    !seen_nodes.emplace(node_id->string).second) {
                    throw std::invalid_argument(
                        "review projection has an unresolved or duplicate raw node");
                }
                section.nodes.push_back(node_by_id.at(node_id->string));
            } else {
                if (edge_id->type != Type::String ||
                    !edge_ids.contains(edge_id->string) ||
                    !seen_edges.emplace(edge_id->string).second) {
                    throw std::invalid_argument(
                        "review projection has an unresolved or duplicate raw edge");
                }
                section.edges.push_back(edge_id->string);
            }
        }
        result.push_back(std::move(section));
    }
    if (seen_nodes.size() != strategy.nodes.size() ||
        seen_edges.size() != edge_ids.size()) {
        throw std::invalid_argument(
            "review projection must cover every raw node and edge exactly once");
    }
    std::vector<std::size_t> section_by_node(strategy.nodes.size());
    for (std::size_t section = 0; section < result.size(); ++section) {
        for (const std::uint32_t node : result[section].nodes) {
            section_by_node[node] = section;
        }
    }
    std::unordered_map<std::string, std::size_t> section_by_edge;
    for (std::size_t section = 0; section < result.size(); ++section) {
        for (const std::string& edge : result[section].edges) {
            section_by_edge.emplace(edge, section);
        }
    }
    for (std::size_t source = 0; source < strategy.nodes.size(); ++source) {
        for (const StrategyEdge& edge : strategy.nodes[source].edges) {
            if (section_by_edge.at(edge.id) != section_by_node[source]) {
                throw std::invalid_argument(
                    "review edge must be owned by its source-node section");
            }
        }
    }
    return result;
}

} // namespace

std::uint32_t resolve_strategy_action(
    const StrategyNode& node,
    const ActionRegistry& registry) {
    if (node.kind != StrategyNodeKind::Operation) return kNoId;
    for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
        const ActionDescriptor& descriptor = registry.actions[i];
        if (node.action_type == kStrategyRestartOperation) {
            if (descriptor.synthetic) return i;
            continue;
        }
        if (descriptor.synthetic) continue;
        if (same_action_parameters(node.action, descriptor.params)) return i;
    }
    return kNoId;
}

bool evaluate_abstract_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state) {
    switch (condition.kind) {
    case ConditionKind::Always:
        return true;
    case ConditionKind::HasModGroup: {
        const std::size_t slot = layout_slot_for(condition, layout);
        const bool present = condition.min_value == 0
                                 ? state.slot_status[slot] !=
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Absent)
                                 : state.slot_status[slot] ==
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Satisfied);
        if (!present) return false;
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0 &&
            (state.fractured_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        if ((condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0 &&
            (state.crafted_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        return true;
    }
    case ConditionKind::HasModFamily: {
        const std::size_t slot = layout_slot_for(condition, layout);
        const bool present = condition.min_value == 0
                                 ? state.slot_status[slot] !=
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Absent)
                                 : state.slot_status[slot] ==
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Satisfied);
        if (!present) return false;
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0 &&
            (state.fractured_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        if ((condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0 &&
            (state.crafted_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        return true;
    }
    case ConditionKind::RarityIs:
        return state.rarity == condition.min_value;
    case ConditionKind::OpenPrefixCount: {
        const int open = std::max(
            0, static_cast<int>(rarity_affix_cap(session, state.rarity)) -
                   static_cast<int>(state.prefix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::OpenSuffixCount: {
        const int open = std::max(
            0, static_cast<int>(rarity_affix_cap(session, state.rarity)) -
                   static_cast<int>(state.suffix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::PrefixCountRange:
        return state.prefix_count >= condition.min_value &&
               state.prefix_count <= condition.max_value;
    case ConditionKind::SuffixCountRange:
        return state.suffix_count >= condition.min_value &&
               state.suffix_count <= condition.max_value;
    case ConditionKind::ItemFlag: {
        std::uint32_t flag = 0;
        switch (condition.item_flag) {
        case ItemFlagKind::Corrupted: flag = kFlagCorrupted; break;
        case ItemFlagKind::Mirrored: flag = kFlagMirrored; break;
        case ItemFlagKind::Split: flag = kFlagSplit; break;
        case ItemFlagKind::Synthesised: flag = kFlagSynthesised; break;
        case ItemFlagKind::Fractured: flag = kFlagFractured; break;
        case ItemFlagKind::Crafted: flag = kFlagCraftedMod; break;
        case ItemFlagKind::Veiled: flag = kFlagVeiledMod; break;
        case ItemFlagKind::Multimod: flag = kFlagMultimod; break;
        case ItemFlagKind::NoAttack: flag = kFlagNoAttack; break;
        case ItemFlagKind::NoCaster: flag = kFlagNoCaster; break;
        case ItemFlagKind::PrefixesLocked: flag = kFlagPrefixesLocked; break;
        case ItemFlagKind::SuffixesLocked: flag = kFlagSuffixesLocked; break;
        case ItemFlagKind::Influenced: flag = kFlagInfluenced; break;
        case ItemFlagKind::EldritchImplicit:
            flag = kFlagEldritchImplicit;
            break;
        case ItemFlagKind::VeiledPrefix:
            return state.veiled_side == PC_SIDE_PREFIX;
        case ItemFlagKind::VeiledSuffix:
            return state.veiled_side == PC_SIDE_SUFFIX;
        }
        return (state.flags & flag) != 0;
    }
    case ConditionKind::InfluenceBits:
        return state.influence_bits == condition.min_value;
    case ConditionKind::EldritchTier: {
        const int tier = condition.eldritch_side == 0
                             ? state.searing_exarch_tier
                             : state.eater_of_worlds_tier;
        return tier >= condition.min_value && tier <= condition.max_value;
    }
    case ConditionKind::ModCount:
    case ConditionKind::ModFamilyCount:
    case ConditionKind::HasUnveilOption:
        return false; /* rejected during model derivation */
    case ConditionKind::All:
        return std::all_of(
            condition.children.begin(), condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_abstract_condition(
                    child, session, layout, state);
            });
    case ConditionKind::Any:
        return std::any_of(
            condition.children.begin(), condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_abstract_condition(
                    child, session, layout, state);
            });
    case ConditionKind::Not:
        return !evaluate_abstract_condition(
            condition.children.front(), session, layout, state);
    case ConditionKind::AtLeast: {
        int matches = 0;
        for (const CompiledCondition& child : condition.children) {
            if (evaluate_abstract_condition(child, session, layout, state)) {
                ++matches;
            }
        }
        return matches >= condition.min_value;
    }
    }
    return false;
}

struct StrategyEvalWork::Impl {
    struct FallbackState {
        std::uint32_t component = kNoId;
        std::vector<std::uint32_t> members;
        std::map<std::uint32_t, std::size_t> local_index;
        std::vector<double> wave;
        std::vector<double> visits;
        double input_mass = 0.0;
        double exited_mass = 0.0;
        std::uint32_t sweeps = 0;
    };

    std::shared_ptr<const StrategyImpl> strategy;
    StrategyEvalOptions options;
    EvalModel model;
    std::vector<ReviewSectionSpec> review_sections;
    StrategyEvalResult output;
    StrategyEvalPhase phase = StrategyEvalPhase::Discovery;

    std::map<std::uint64_t, std::uint32_t> pair_by_key;
    std::vector<EvalPair> pairs;
    std::vector<EvalRow> rows;
    std::map<
        std::pair<std::uint32_t, const OutcomeDistribution*>,
        std::uint32_t> row_by_distribution;
    std::uint64_t stored_transitions = 0;
    std::size_t discover_index = 0;
    std::uint32_t start_pair = kNoId;

    std::vector<std::vector<std::uint32_t>> components;
    std::vector<std::uint32_t> component_by_pair;
    std::size_t component_index = 0;
    std::vector<double> external_incoming;
    std::vector<double> pair_visits;
    std::vector<double> unresolved_pair;
    /* Deterministic pass-through contraction (contract_pass_through).
     * A contracted pair keeps its single outgoing transition in
     * chain_next/chain_edge; chain_inflow accumulates the mass that
     * entered it so its visits and edge traversal are settled during
     * finalization. */
    std::vector<std::uint8_t> pair_contracted;
    std::vector<std::uint32_t> chain_next;
    std::vector<std::uint32_t> chain_edge;
    std::vector<std::uint32_t> chain_terminal;
    std::vector<double> chain_inflow;
    std::unique_ptr<FallbackState> fallback;
    std::uint64_t fallback_sweeps = 0;
    bool hard_unresolved = false;

    std::vector<double> terminal_mass;
    std::vector<double> action_not_applied;
    std::vector<double> no_matching_edge;
    std::vector<std::map<std::uint32_t, double>> terminal_incoming;
    std::map<std::string, std::uint32_t> edge_index_by_id;
    std::vector<double> edge_traversals;

    Impl(
        std::shared_ptr<const StrategyImpl> strategy_in,
        const StrategyEvalOptions& options_in)
        : strategy(std::move(strategy_in)),
          options(options_in),
          model(derive_checked_model(strategy, options_in.max_states)),
          review_sections(parse_review_sections(
              *strategy, options_in.review_projection_json)) {
        if (strategy == nullptr || strategy->session == nullptr ||
            strategy->start_node >= strategy->nodes.size()) {
            throw std::invalid_argument("invalid compiled strategy");
        }
        if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0 ||
            options.max_sweeps == 0 || options.max_states == 0 ||
            options.max_pairs == 0 || options.max_transitions == 0) {
            throw std::invalid_argument("invalid strategy evaluation options");
        }
        output.targets = model.targets;
        const std::size_t node_count = strategy->nodes.size();
        terminal_mass.assign(node_count, 0.0);
        action_not_applied.assign(node_count, 0.0);
        no_matching_edge.assign(node_count, 0.0);
        terminal_incoming.resize(node_count);
        for (const StrategyNode& node : strategy->nodes) {
            for (const StrategyEdge& edge : node.edges) {
                const std::uint32_t index =
                    static_cast<std::uint32_t>(edge_traversals.size());
                if (!edge_index_by_id.emplace(edge.id, index).second) {
                    throw std::logic_error(
                        "compiled strategy contains a duplicate edge id");
                }
                edge_traversals.push_back(0.0);
            }
        }

        const std::uint32_t start_state =
            model.calc->intern_item(strategy->start_item);
        ensure_state_limit();
        if (strategy->nodes[strategy->start_node].kind ==
            StrategyNodeKind::Terminal) {
            terminal_mass[strategy->start_node] = 1.0;
            terminal_incoming[strategy->start_node][start_state] = 1.0;
            phase = StrategyEvalPhase::Finalization;
        } else {
            start_pair = intern_pair(strategy->start_node, start_state);
        }
    }

    void ensure_state_limit() const {
        if (model.calc->state_count() > options.max_states) {
            throw std::length_error(
                "strategy evaluation exceeded max_states (" +
                std::to_string(options.max_states) + ")");
        }
    }

    std::uint32_t intern_pair(
        std::uint32_t node,
        std::uint32_t state) {
        const std::uint64_t key = eval_pair_key(node, state);
        const auto found = pair_by_key.find(key);
        if (found != pair_by_key.end()) return found->second;
        if (pairs.size() >= options.max_pairs) {
            throw std::length_error(
                "strategy evaluation exceeded max_pairs (" +
                std::to_string(options.max_pairs) + ")");
        }
        const std::uint32_t id = static_cast<std::uint32_t>(pairs.size());
        pair_by_key.emplace(key, id);
        EvalPair pair;
        pair.node = node;
        pair.state = state;
        pairs.push_back(std::move(pair));
        return id;
    }

    const EvalRow& pair_row(std::uint32_t pair) const {
        return rows.at(pairs.at(pair).row);
    }

    void ensure_transition_budget(std::uint64_t additional) const {
        if (stored_transitions > options.max_transitions ||
            additional > options.max_transitions - stored_transitions) {
            throw std::length_error(
                "strategy evaluation exceeded max_transitions (" +
                std::to_string(options.max_transitions) + ")");
        }
    }

    const StrategyEdge* select_edge(
        const StrategyNode& node,
        std::uint32_t state) const {
        const StrategyEdge* fallback_edge = nullptr;
        for (const StrategyEdge& edge : node.edges) {
            if (edge.is_default) {
                fallback_edge = &edge;
            } else if (evaluate_abstract_condition(
                           edge.condition, model.calc->session(),
                           model.calc->layout(), model.calc->state(state))) {
                return &edge;
            }
        }
        return fallback_edge;
    }

    void expand_pair(std::uint32_t pair_id) {
        const std::uint32_t node_index = pairs.at(pair_id).node;
        const std::uint32_t state_id = pairs.at(pair_id).state;
        const StrategyNode& node = strategy->nodes.at(node_index);
        bool operation = false;
        bool consumes = false;
        std::uint32_t action_index = kNoId;
        const OutcomeDistribution* shared_distribution = nullptr;
        std::map<std::pair<std::uint32_t, std::uint32_t>, double> transitions;
        std::map<
            std::tuple<int, std::uint32_t, std::uint32_t, std::uint32_t>,
            double> absorptions;

        const auto add_transition = [&](const std::pair<
                                            std::uint32_t,
                                            std::uint32_t>& key,
                                        double probability) {
            auto found = transitions.find(key);
            if (found == transitions.end()) {
                ensure_transition_budget(
                    transitions.size() + absorptions.size() + 1);
                transitions.emplace(key, probability);
            } else {
                found->second += probability;
            }
        };
        const auto add_absorption = [&](const std::tuple<
                                            int,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t>& key,
                                        double probability) {
            auto found = absorptions.find(key);
            if (found == absorptions.end()) {
                ensure_transition_budget(
                    transitions.size() + absorptions.size() + 1);
                absorptions.emplace(key, probability);
            } else {
                found->second += probability;
            }
        };

        const auto route = [&](std::uint32_t state, double probability) {
            if (!(probability > 0.0)) return;
            if (!std::isfinite(probability)) {
                throw std::runtime_error(
                    "strategy evaluation found a non-finite transition");
            }
            const StrategyEdge* selected = select_edge(node, state);
            if (selected == nullptr) {
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::NoMatchingEdge),
                     node_index, state, kNoId},
                    probability);
                return;
            }
            const std::uint32_t edge = edge_index_by_id.at(selected->id);
            const StrategyNode& target = strategy->nodes.at(selected->target);
            if (target.kind == StrategyNodeKind::Terminal) {
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::Terminal),
                     selected->target, state, edge},
                    probability);
                return;
            }
            const std::uint32_t target_pair =
                intern_pair(selected->target, state);
            add_transition({target_pair, edge}, probability);
        };

        if (node.kind != StrategyNodeKind::Operation) {
            route(state_id, 1.0);
        } else {
            operation = true;
            action_index = model.action_by_node.at(node_index);
            const ActionDescriptor& action =
                model.calc->registry().actions.at(action_index);
            if (!action_legal(
                    model.calc->session(), action,
                    model.calc->state(state_id))) {
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::ActionNotApplied),
                     node_index, state_id, kNoId},
                    1.0);
            } else {
                consumes = true;
                const OutcomeDistribution& outcomes =
                    model.calc->outcomes(state_id, action_index);
                if (!outcomes.supported) {
                    throw StrategyEvalUnsupported(
                        "strategy evaluation unsupported:\n- node '" +
                        node.id + "' operation '" + action.id +
                        "' has no exact distribution for a reachable state");
                }
                ensure_state_limit();
                const auto shared = row_by_distribution.find(
                    {node_index, &outcomes});
                if (shared != row_by_distribution.end()) {
                    EvalPair& pair = pairs.at(pair_id);
                    pair.operation = operation;
                    pair.consumes = consumes;
                    pair.action = action_index;
                    pair.row = shared->second;
                    return;
                }
                shared_distribution = &outcomes;
                double distribution_mass = 0.0;
                for (const OutcomeEntry& outcome : outcomes.entries) {
                    distribution_mass += outcome.probability;
                    route(outcome.state, outcome.probability);
                }
                if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                    throw std::runtime_error(
                        "strategy evaluation action distribution does not "
                        "sum to one at node '" + node.id + "'");
                }
            }
        }

        EvalPair& pair = pairs.at(pair_id);
        pair.operation = operation;
        pair.consumes = consumes;
        pair.action = action_index;
        EvalRow row;
        row.transitions.reserve(transitions.size());
        for (const auto& [key, probability] : transitions) {
            row.transitions.push_back(
                {key.first, probability, key.second});
        }
        row.absorptions.reserve(absorptions.size());
        for (const auto& [key, probability] : absorptions) {
            row.absorptions.push_back(
                {static_cast<EvalAbsorptionKind>(std::get<0>(key)),
                  std::get<1>(key), std::get<2>(key), probability,
                  std::get<3>(key)});
        }
        double row_mass = 0.0;
        for (const EvalTransition& transition : row.transitions) {
            row_mass += transition.probability;
        }
        for (const EvalAbsorption& absorption : row.absorptions) {
            row_mass += absorption.probability;
        }
        if (std::fabs(row_mass - 1.0) > 1e-9) {
            throw std::runtime_error(
                "strategy evaluation transition row does not sum to one at "
                "node '" + node.id + "'");
        }
        stored_transitions += row.transitions.size() + row.absorptions.size();
        pair.row = static_cast<std::uint32_t>(rows.size());
        rows.push_back(std::move(row));
        if (shared_distribution != nullptr) {
            row_by_distribution.emplace(
                std::make_pair(node_index, shared_distribution), pair.row);
        }
    }

    /* Fold deterministic pass-through pairs — exactly one outgoing
     * transition with probability exactly 1 and no absorptions — out of
     * the transition relation before components are built. Every
     * transition entering such a pair is redirected to the first
     * non-pass-through pair down its chain, so hub-and-spoke loops
     * (reforge ↔ deterministic scour, solver router/restart graphs)
     * become self-loops the closed-form solvers handle instead of
     * multi-thousand-member fallback components. The rewrite is exact:
     * probabilities are only regrouped, never truncated, and the folded
     * pairs' visits and edge traversals are settled from chain_inflow at
     * finalization. Pairs on a purely deterministic cycle are left in
     * place so recurrent classes keep their unresolved treatment. */
    void contract_pass_through() {
        const std::size_t count = pairs.size();
        pair_contracted.assign(count, 0);
        chain_next.assign(count, kNoId);
        chain_edge.assign(count, kNoId);
        chain_terminal.assign(count, kNoId);
        chain_inflow.assign(count, 0.0);
        if (count == 0) return;

        std::vector<std::uint8_t> pass(count, 0);
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            const EvalRow& row = pair_row(pair);
            if (row.absorptions.empty() && row.transitions.size() == 1 &&
                row.transitions.front().probability == 1.0) {
                pass[pair] = 1;
                chain_next[pair] = row.transitions.front().target;
                chain_edge[pair] = row.transitions.front().edge;
            }
        }

        /* Resolve each pair's forward target: itself when it is not a
         * pass-through, otherwise the end of its deterministic chain.
         * state: 0 unvisited, 1 on the current walk, 2 resolved. */
        std::vector<std::uint8_t> state(count, 0);
        std::vector<std::uint32_t> forward(count, kNoId);
        std::vector<std::uint32_t> path;
        bool any_contracted = false;
        for (std::uint32_t root = 0; root < count; ++root) {
            if (state[root] == 2) continue;
            path.clear();
            std::uint32_t cursor = root;
            while (state[cursor] != 2) {
                if (!pass[cursor]) {
                    state[cursor] = 2;
                    forward[cursor] = cursor;
                    break;
                }
                if (state[cursor] == 1) {
                    /* The walk re-entered itself: cursor..path.back()
                     * form a deterministic cycle. Keep those pairs. */
                    std::size_t cycle = path.size();
                    while (path[cycle - 1] != cursor) --cycle;
                    --cycle;
                    for (std::size_t i = cycle; i < path.size(); ++i) {
                        state[path[i]] = 2;
                        forward[path[i]] = path[i];
                        pass[path[i]] = 0;
                    }
                    path.resize(cycle);
                    break;
                }
                state[cursor] = 1;
                path.push_back(cursor);
                cursor = chain_next[cursor];
            }
            for (std::size_t i = path.size(); i-- > 0;) {
                const std::uint32_t pair = path[i];
                state[pair] = 2;
                forward[pair] = forward[chain_next[pair]];
                chain_terminal[pair] = forward[pair];
                pair_contracted[pair] = 1;
                any_contracted = true;
            }
        }
        if (!any_contracted) return;

        std::uint64_t remaining_transitions = 0;
        for (EvalRow& row : rows) {
            bool touched = false;
            for (const EvalTransition& transition : row.transitions) {
                if (pair_contracted[transition.target]) {
                    touched = true;
                    break;
                }
            }
            if (touched) {
                std::map<
                    std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>,
                    double> merged;
                for (const EvalTransition& transition : row.transitions) {
                    const std::uint32_t via =
                        pair_contracted[transition.target]
                            ? transition.target
                            : kNoId;
                    const std::uint32_t target =
                        via == kNoId ? transition.target
                                     : forward[transition.target];
                    merged[{target, transition.edge, via}] +=
                        transition.probability;
                }
                row.transitions.clear();
                row.transitions.reserve(merged.size());
                for (const auto& [key, probability] : merged) {
                    row.transitions.push_back(
                        {std::get<0>(key), probability, std::get<1>(key),
                         std::get<2>(key)});
                }
            }
            remaining_transitions +=
                row.transitions.size() + row.absorptions.size();
        }
        stored_transitions = remaining_transitions;
    }

    /* Settle the mass that flowed through contracted pairs: each visit
     * of a folded chain and its single edge, in chain order. Runs once,
     * at finalization, after every component (or the reference sweep)
     * has committed its flows into chain_inflow. */
    void propagate_chain_inflow() {
        const std::size_t count = pair_contracted.size();
        std::vector<std::uint32_t> indegree(count, 0);
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            if (!pair_contracted[pair]) continue;
            const std::uint32_t next = chain_next[pair];
            if (pair_contracted[next]) ++indegree[next];
        }
        std::vector<std::uint32_t> ready;
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            if (pair_contracted[pair] && indegree[pair] == 0) {
                ready.push_back(pair);
            }
        }
        while (!ready.empty()) {
            const std::uint32_t pair = ready.back();
            ready.pop_back();
            const double inflow = chain_inflow[pair];
            pair_visits[pair] += inflow;
            if (chain_edge[pair] != kNoId) {
                edge_traversals.at(chain_edge[pair]) += inflow;
            }
            const std::uint32_t next = chain_next[pair];
            if (pair_contracted[next]) {
                chain_inflow[next] += inflow;
                if (--indegree[next] == 0) ready.push_back(next);
            }
        }
    }

    void build_components() {
        contract_pass_through();
        const std::size_t count = pairs.size();
        struct Frame {
            std::uint32_t pair = kNoId;
            std::size_t next_transition = 0;
        };
        std::vector<std::uint32_t> index(count, kNoId);
        std::vector<std::uint32_t> lowlink(count, kNoId);
        std::vector<std::uint8_t> on_stack(count, 0);
        std::vector<std::uint32_t> tarjan_stack;
        tarjan_stack.reserve(count);
        std::vector<std::vector<std::uint32_t>> raw_components;
        std::uint32_t next_index = 0;

        const auto push_pair = [&](std::uint32_t pair,
                                   std::vector<Frame>& dfs) {
            index[pair] = next_index;
            lowlink[pair] = next_index;
            ++next_index;
            tarjan_stack.push_back(pair);
            on_stack[pair] = 1;
            dfs.push_back({pair, 0});
        };
        for (std::uint32_t root = 0; root < count; ++root) {
            if (pair_contracted[root] || index[root] != kNoId) continue;
            std::vector<Frame> dfs;
            push_pair(root, dfs);
            while (!dfs.empty()) {
                Frame& frame = dfs.back();
                const auto& transitions = pair_row(frame.pair).transitions;
                if (frame.next_transition < transitions.size()) {
                    const std::uint32_t target =
                        transitions[frame.next_transition++].target;
                    if (index[target] == kNoId) {
                        push_pair(target, dfs);
                    } else if (on_stack[target]) {
                        lowlink[frame.pair] = std::min(
                            lowlink[frame.pair], index[target]);
                    }
                    continue;
                }

                const std::uint32_t completed = frame.pair;
                dfs.pop_back();
                if (!dfs.empty()) {
                    const std::uint32_t parent = dfs.back().pair;
                    lowlink[parent] = std::min(
                        lowlink[parent], lowlink[completed]);
                }
                if (lowlink[completed] == index[completed]) {
                    raw_components.emplace_back();
                    while (true) {
                        const std::uint32_t member = tarjan_stack.back();
                        tarjan_stack.pop_back();
                        on_stack[member] = 0;
                        raw_components.back().push_back(member);
                        if (member == completed) break;
                    }
                    std::sort(
                        raw_components.back().begin(),
                        raw_components.back().end());
                }
            }
        }

        /* Tarjan emits sink components first. Reverse that order so forward
         * mass reaches every component before it is solved, without copying
         * the (potentially dense) edge relation into adjacency lists. */
        components.clear();
        components.reserve(raw_components.size());
        for (auto it = raw_components.rbegin();
             it != raw_components.rend(); ++it) {
            components.push_back(std::move(*it));
        }
        component_by_pair.assign(count, kNoId);
        for (std::uint32_t component = 0;
             component < components.size(); ++component) {
            for (const std::uint32_t pair : components[component]) {
                component_by_pair[pair] = component;
            }
        }

        external_incoming.assign(count, 0.0);
        pair_visits.assign(count, 0.0);
        unresolved_pair.assign(count, 0.0);
        if (start_pair != kNoId) {
            if (pair_contracted[start_pair]) {
                chain_inflow[start_pair] = 1.0;
                external_incoming.at(chain_terminal[start_pair]) = 1.0;
            } else {
                external_incoming[start_pair] = 1.0;
            }
        }
        component_index = 0;
    }

    bool component_has_exit(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members) const {
        for (const std::uint32_t pair : members) {
            const EvalRow& row = pair_row(pair);
            if (!row.absorptions.empty()) return true;
            for (const EvalTransition& transition : row.transitions) {
                if (component_by_pair[transition.target] != component &&
                    transition.probability > 0.0) {
                    return true;
                }
            }
        }
        return false;
    }

    bool dense_solve(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) const {
        const std::size_t n = members.size();
        std::map<std::uint32_t, std::size_t> local;
        for (std::size_t i = 0; i < n; ++i) local[members[i]] = i;
        std::vector<std::vector<long double>> matrix(
            n, std::vector<long double>(n + 1, 0.0L));
        for (std::size_t row = 0; row < n; ++row) {
            matrix[row][row] = 1.0L;
            matrix[row][n] = incoming[row];
        }
        for (std::size_t source = 0; source < n; ++source) {
            for (const EvalTransition& transition :
                 pair_row(members[source]).transitions) {
                if (component_by_pair[transition.target] != component) continue;
                matrix[local.at(transition.target)][source] -=
                    static_cast<long double>(transition.probability);
            }
        }

        long double matrix_scale = 0.0L;
        for (const auto& row : matrix) {
            for (std::size_t col = 0; col < n; ++col) {
                matrix_scale = std::max(matrix_scale, std::fabs(row[col]));
            }
        }
        long double min_pivot = std::numeric_limits<long double>::infinity();
        long double max_pivot = 0.0L;
        for (std::size_t col = 0; col < n; ++col) {
            std::size_t pivot = col;
            long double pivot_abs = std::fabs(matrix[col][col]);
            for (std::size_t row = col + 1; row < n; ++row) {
                const long double candidate = std::fabs(matrix[row][col]);
                if (candidate > pivot_abs) {
                    pivot = row;
                    pivot_abs = candidate;
                }
            }
            if (pivot_abs <= std::max(1e-18L, matrix_scale * 1e-14L)) {
                return false;
            }
            if (pivot != col) std::swap(matrix[pivot], matrix[col]);
            min_pivot = std::min(min_pivot, pivot_abs);
            max_pivot = std::max(max_pivot, pivot_abs);
            for (std::size_t row = col + 1; row < n; ++row) {
                const long double factor = matrix[row][col] / matrix[col][col];
                if (factor == 0.0L) continue;
                for (std::size_t k = col; k <= n; ++k) {
                    matrix[row][k] -= factor * matrix[col][k];
                }
            }
        }
        if (max_pivot > 0.0L && min_pivot / max_pivot < 1e-13L) {
            return false;
        }

        std::vector<long double> solved(n, 0.0L);
        for (std::size_t back = n; back-- > 0;) {
            long double value = matrix[back][n];
            for (std::size_t col = back + 1; col < n; ++col) {
                value -= matrix[back][col] * solved[col];
            }
            solved[back] = value / matrix[back][back];
            if (!std::isfinite(solved[back])) return false;
        }
        long double max_value = 0.0L;
        for (const long double value : solved) {
            max_value = std::max(max_value, std::fabs(value));
        }
        const long double negative_tolerance =
            1e-12L * std::max(1.0L, max_value);
        visits.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (solved[i] < -negative_tolerance) return false;
            visits[i] = static_cast<double>(std::max(0.0L, solved[i]));
            if (!std::isfinite(visits[i])) return false;
        }

        long double max_residual = 0.0L;
        for (std::size_t target = 0; target < n; ++target) {
            long double expected = incoming[target];
            for (std::size_t source = 0; source < n; ++source) {
                for (const EvalTransition& transition :
                     pair_row(members[source]).transitions) {
                    if (transition.target == members[target]) {
                        expected += static_cast<long double>(visits[source]) *
                                    transition.probability;
                    }
                }
            }
            max_residual = std::max(
                max_residual,
                std::fabs(static_cast<long double>(visits[target]) - expected));
        }
        const long double tolerance = std::max(
            1e-12L,
            static_cast<long double>(options.epsilon) *
                std::max(1.0L, max_value) * 10.0L);
        return max_residual <= tolerance;
    }

    bool rank_one_solve(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) const {
        if (members.size() < 2) return false;
        const auto row = [&](std::uint32_t pair) {
            std::map<std::uint32_t, double> probabilities;
            for (const EvalTransition& transition : pair_row(pair).transitions) {
                if (component_by_pair[transition.target] == component) {
                    probabilities[transition.target] += transition.probability;
                }
            }
            return probabilities;
        };
        const std::map<std::uint32_t, double> reference = row(members.front());
        for (std::size_t source = 1; source < members.size(); ++source) {
            if (pairs[members[source]].row == pairs[members.front()].row) {
                continue;
            }
            const auto candidate = row(members[source]);
            if (candidate.size() != reference.size()) return false;
            auto a = reference.begin();
            auto b = candidate.begin();
            for (; a != reference.end(); ++a, ++b) {
                if (a->first != b->first ||
                    std::fabs(a->second - b->second) > 1e-15) {
                    return false;
                }
            }
        }
        double internal_probability = 0.0;
        for (const auto& [target, probability] : reference) {
            (void)target;
            internal_probability += probability;
        }
        const double denominator = 1.0 - internal_probability;
        if (!(denominator > 1e-14)) return false;
        double total_incoming = 0.0;
        for (const double mass : incoming) total_incoming += mass;
        const double total_visits = total_incoming / denominator;
        if (!std::isfinite(total_visits)) return false;
        visits = incoming;
        for (std::size_t target = 0; target < members.size(); ++target) {
            const auto found = reference.find(members[target]);
            if (found != reference.end()) {
                visits[target] += found->second * total_visits;
            }
        }
        return true;
    }

    void add_absorption(const EvalAbsorption& absorption, double mass) {
        if (!(mass > 0.0)) return;
        if (absorption.edge != kNoId) {
            edge_traversals.at(absorption.edge) += mass;
        }
        switch (absorption.kind) {
        case EvalAbsorptionKind::Terminal:
            terminal_mass[absorption.node] += mass;
            terminal_incoming[absorption.node][absorption.state] += mass;
            break;
        case EvalAbsorptionKind::ActionNotApplied:
            action_not_applied[absorption.node] += mass;
            break;
        case EvalAbsorptionKind::NoMatchingEdge:
            no_matching_edge[absorption.node] += mass;
            break;
        }
    }

    void commit_component(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& visits) {
        for (std::size_t i = 0; i < members.size(); ++i) {
            const std::uint32_t pair = members[i];
            const double visit = visits[i];
            pair_visits[pair] += visit;
            const EvalRow& row = pair_row(pair);
            for (const EvalTransition& transition : row.transitions) {
                const double flow = visit * transition.probability;
                if (transition.edge != kNoId) {
                    edge_traversals.at(transition.edge) += flow;
                }
                if (transition.via != kNoId) {
                    chain_inflow.at(transition.via) += flow;
                }
                if (component_by_pair[transition.target] != component) {
                    external_incoming[transition.target] += flow;
                }
            }
            for (const EvalAbsorption& absorption : row.absorptions) {
                add_absorption(
                    absorption, visit * absorption.probability);
            }
        }
    }

    void add_unresolved(
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& mass,
        bool hard) {
        for (std::size_t i = 0; i < members.size(); ++i) {
            unresolved_pair[members[i]] += mass[i];
        }
        hard_unresolved = hard_unresolved || hard;
    }

    void begin_fallback(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming) {
        fallback = std::make_unique<FallbackState>();
        fallback->component = component;
        fallback->members = members;
        fallback->wave = incoming;
        fallback->visits.assign(members.size(), 0.0);
        for (std::size_t i = 0; i < members.size(); ++i) {
            fallback->local_index[members[i]] = i;
            fallback->input_mass += incoming[i];
        }
        phase = StrategyEvalPhase::Fallback;
    }

    void finish_component() {
        ++component_index;
        phase = component_index < components.size()
                    ? StrategyEvalPhase::Solving
                    : StrategyEvalPhase::Finalization;
    }

    void solve_component() {
        if (component_index >= components.size()) {
            phase = StrategyEvalPhase::Finalization;
            return;
        }
        const std::uint32_t component =
            static_cast<std::uint32_t>(component_index);
        const auto& members = components[component_index];
        std::vector<double> incoming;
        incoming.reserve(members.size());
        double input_mass = 0.0;
        for (const std::uint32_t pair : members) {
            incoming.push_back(external_incoming[pair]);
            input_mass += external_incoming[pair];
        }
        if (input_mass == 0.0) {
            commit_component(component, members, incoming);
            finish_component();
            return;
        }
        if (!component_has_exit(component, members)) {
            /* A recurrent class has infinite visit counts. Preserve a finite,
             * useful entry snapshot and attribute all entering probability as
             * unresolved without fabricating repeated traversals. */
            for (std::size_t i = 0; i < members.size(); ++i) {
                pair_visits[members[i]] += incoming[i];
            }
            add_unresolved(members, incoming, true);
            finish_component();
            return;
        }

        std::vector<double> visits;
        bool solved = false;
        if (members.size() == 1) {
            double self_probability = 0.0;
            for (const EvalTransition& transition :
                 pair_row(members.front()).transitions) {
                if (transition.target == members.front()) {
                    self_probability += transition.probability;
                }
            }
            const double denominator = 1.0 - self_probability;
            if (denominator > 1e-14) {
                const double value = incoming.front() / denominator;
                if (std::isfinite(value) && value >= 0.0) {
                    visits = {value};
                    solved = true;
                }
            }
        } else if (rank_one_solve(
                       component, members, incoming, visits)) {
            solved = true;
        } else if (members.size() <= 64) {
            solved = dense_solve(component, members, incoming, visits);
        }
        if (solved) {
            commit_component(component, members, visits);
            finish_component();
        } else {
            begin_fallback(component, members, incoming);
        }
    }

    void run_fallback_batch() {
        FallbackState& state = *fallback;
        constexpr std::uint32_t kBatchSweeps = 32;
        bool finished = false;
        for (std::uint32_t batch = 0;
             batch < kBatchSweeps && state.sweeps < options.max_sweeps;
             ++batch) {
            double wave_mass = 0.0;
            for (std::size_t i = 0; i < state.wave.size(); ++i) {
                state.visits[i] += state.wave[i];
                wave_mass += state.wave[i];
            }
            std::vector<double> next(state.wave.size(), 0.0);
            for (std::size_t source = 0; source < state.members.size(); ++source) {
                for (const EvalTransition& transition :
                     pair_row(state.members[source]).transitions) {
                    if (component_by_pair[transition.target] ==
                        state.component) {
                        next[state.local_index.at(transition.target)] +=
                            state.wave[source] * transition.probability;
                    }
                }
            }
            double residual = 0.0;
            for (const double mass : next) residual += mass;
            state.exited_mass += wave_mass - residual;
            state.wave = std::move(next);
            ++state.sweeps;
            ++fallback_sweeps;
            const double conservation_error = std::fabs(
                state.input_mass - state.exited_mass - residual);
            output.max_mass_conservation_error = std::max(
                output.max_mass_conservation_error, conservation_error);
            if (conservation_error >
                1e-8 * std::max(1.0, state.input_mass)) {
                throw std::runtime_error(
                    "strategy evaluation fallback mass conservation failed");
            }
            if (residual < options.epsilon) {
                finished = true;
                break;
            }
        }
        if (state.sweeps >= options.max_sweeps) finished = true;
        if (!finished) return;

        double residual = 0.0;
        for (const double mass : state.wave) residual += mass;
        commit_component(state.component, state.members, state.visits);
        if (residual > 0.0) {
            add_unresolved(
                state.members, state.wave, residual >= options.epsilon);
        }
        fallback.reset();
        finish_component();
    }

    void finalize() {
        propagate_chain_inflow();
        CalcContext& calc = *model.calc;
        const std::size_t node_count = strategy->nodes.size();
        std::vector<double> node_visits(node_count, 0.0);
        std::vector<double> operation_visits(node_count, 0.0);
        std::vector<double> operation_applied(node_count, 0.0);
        std::vector<double> unresolved_by_node(node_count, 0.0);
        std::vector<std::map<std::uint32_t, double>> incoming(node_count);

        for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
            const EvalPair& record = pairs[pair];
            const double visits = pair_visits[pair];
            node_visits[record.node] += visits;
            incoming[record.node][record.state] += visits;
            unresolved_by_node[record.node] += unresolved_pair[pair];
            output.residual_mass += unresolved_pair[pair];
            if (record.operation) {
                output.expected_actions += visits;
                operation_visits[record.node] += visits;
                if (record.consumes) {
                    operation_applied[record.node] += visits;
                    const ActionDescriptor& action =
                        calc.registry().actions.at(record.action);
                    for (const std::string& key : action.cost_keys) {
                        output.expected_consumption[key] += visits;
                    }
                }
            }
        }
        for (std::size_t node = 0; node < node_count; ++node) {
            node_visits[node] += terminal_mass[node];
            for (const auto& [state, mass] : terminal_incoming[node]) {
                incoming[node][state] += mass;
            }
            const StrategyNode& source = strategy->nodes[node];
            if (source.kind == StrategyNodeKind::Terminal) {
                output.terminal_nodes.push_back(
                    {source.id, source.terminal_kind, terminal_mass[node]});
                if (source.terminal_kind == PC_TERMINAL_SUCCESS) {
                    output.success_probability += terminal_mass[node];
                } else if (source.terminal_kind == PC_TERMINAL_FAILURE) {
                    output.failure_probability += terminal_mass[node];
                } else {
                    output.stop_probability += terminal_mass[node];
                }
            }
            if (unresolved_by_node[node] > 0.0) {
                output.unresolved_by_node.push_back(
                    {source.id, unresolved_by_node[node]});
            }
            if (action_not_applied[node] > 0.0) {
                output.failures_by_node.push_back(
                    {source.id, "action_not_applied",
                     action_not_applied[node]});
                output.action_not_applied_probability +=
                    action_not_applied[node];
            }
            if (no_matching_edge[node] > 0.0) {
                output.failures_by_node.push_back(
                    {source.id, "no_matching_edge",
                     no_matching_edge[node]});
                output.no_matching_edge_probability +=
                    no_matching_edge[node];
            }

            StrategyEvalNode output_node;
            output_node.id = source.id;
            output_node.expected_visits = node_visits[node];
            std::vector<std::pair<std::uint32_t, double>> classes(
                incoming[node].begin(), incoming[node].end());
            std::stable_sort(
                classes.begin(), classes.end(), [](const auto& a, const auto& b) {
                    if (a.second != b.second) return a.second > b.second;
                    return a.first < b.first;
                });
            const std::size_t keep = std::min<std::size_t>(
                options.top_classes_per_node, classes.size());
            for (std::size_t c = 0; c < keep; ++c) {
                output_node.classes.push_back(
                    {node_visits[node] == 0.0
                         ? 0.0
                         : classes[c].second / node_visits[node],
                     calc.state(classes[c].first)});
            }
            double truncated = 0.0;
            for (std::size_t c = keep; c < classes.size(); ++c) {
                truncated += classes[c].second;
            }
            output_node.classes_truncated_share =
                node_visits[node] == 0.0
                    ? 0.0
                    : truncated / node_visits[node];
            output.nodes.push_back(std::move(output_node));

            for (const StrategyEdge& edge : source.edges) {
                output.edges.push_back(
                    {edge.id, edge_traversals.at(edge_index_by_id.at(edge.id))});
            }
        }

        output.technique_totals = empty_technique_totals();
        std::vector<std::map<std::string, double>> node_techniques(
            node_count);
        std::vector<std::vector<std::string>> node_classifications(
            node_count);
        std::map<std::string, StrategyEvalActionTotal> actions_by_id;
        double total_applied_actions = 0.0;
        for (std::size_t node_index = 0; node_index < node_count;
             ++node_index) {
            const StrategyNode& node = strategy->nodes[node_index];
            if (node.kind != StrategyNodeKind::Operation) continue;
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(
                    model.action_by_node.at(node_index));
            StrategyEvalActionTotal& action = actions_by_id[descriptor.id];
            if (action.id.empty()) {
                action.id = descriptor.id;
                action.display_name = descriptor.display_name;
                action.price_keys = descriptor.cost_keys;
            }
            action.expected_visits += operation_visits[node_index];
            action.expected_applied += operation_applied[node_index];
            action.nodes.push_back(
                {node.id, operation_visits[node_index],
                 operation_applied[node_index]});
            total_applied_actions += operation_applied[node_index];

            std::vector<std::string> roles = node.accounting_roles;
            if (descriptor.synthetic) {
                add_classification(roles, "restart");
            } else {
                add_classification(roles, "ordinary_crafting");
            }
            if (descriptor.params.type == ActionType::Fracture) {
                add_classification(roles, "fracture");
            } else if (
                descriptor.params.type == ActionType::RemoveCraftedModifiers) {
                add_classification(roles, "cleanup_or_replacement");
            } else if (descriptor.params.type == ActionType::Bench) {
                const std::uint32_t mod = descriptor.params.mod_id;
                if (mod < calc.session().metamod_type.size()) {
                    const int metamod = calc.session().metamod_type[mod];
                    if (metamod ==
                            calc.session().data->metamod_prefixes_locked_code ||
                        metamod ==
                            calc.session().data->metamod_suffixes_locked_code) {
                        add_classification(roles, "protection_setup");
                    } else if (
                        metamod ==
                        calc.session().data->metamod_multimod_code) {
                        add_classification(roles, "multimod_setup");
                    }
                }
                const bool goal_bench = std::any_of(
                    output.targets.begin(), output.targets.end(),
                    [&](const GoalSlot& target) {
                        return target_contains_mod(
                            calc.session(), target, mod);
                    });
                if (goal_bench) {
                    add_classification(roles, "permanent_goal_bench");
                    add_classification(roles, "deterministic_finish");
                }
            }
            for (const std::string& role : roles) {
                add_classification(
                    node_classifications[node_index], role);
                add_classification(action.classifications, role);
                add_role_work(
                    output.technique_totals, role,
                    operation_visits[node_index],
                    operation_applied[node_index]);
                add_role_work(
                    node_techniques[node_index], role,
                    operation_visits[node_index],
                    operation_applied[node_index]);
            }
        }
        for (const auto& [unused, action] : actions_by_id) {
            (void)unused;
            output.action_totals.push_back(action);
        }

        std::unordered_map<std::string, double> traversal_by_edge;
        for (const StrategyEvalEdge& edge : output.edges) {
            traversal_by_edge.emplace(edge.id, edge.expected_traversals);
        }
        std::unordered_map<std::string, std::map<std::string, double>>
            edge_techniques;
        for (const StrategyNode& node : strategy->nodes) {
            for (const StrategyEdge& edge : node.edges) {
                const double traversals = traversal_by_edge.at(edge.id);
                for (const std::string& role : edge.accounting_roles) {
                    add_role_work(
                        output.technique_totals, role, traversals,
                        traversals);
                    add_role_work(
                        edge_techniques[edge.id], role, traversals,
                        traversals);
                }
            }
        }

        output.pricing_enabled = options.economy != nullptr;
        if (options.economy != nullptr) {
            output.economy_id = options.economy->id;
        }
        output.material_totals = price_materials(
            output.expected_consumption, options.economy,
            output.known_expected_cost, output.cost_complete);
        if (output.cost_complete) {
            output.total_expected_cost = output.known_expected_cost;
        }

        double descriptor_visits = 0.0;
        double descriptor_applied = 0.0;
        std::map<std::string, double> action_materials;
        for (const StrategyEvalActionTotal& action : output.action_totals) {
            descriptor_visits += action.expected_visits;
            descriptor_applied += action.expected_applied;
            for (const std::string& key : action.price_keys) {
                action_materials[key] += action.expected_applied;
            }
        }
        output.action_descriptor_visits_difference =
            descriptor_visits - output.expected_actions;
        output.action_descriptor_applied_difference =
            descriptor_applied - total_applied_actions;
        double operation_visit_sum = 0.0;
        for (const double visits : operation_visits) {
            operation_visit_sum += visits;
        }
        output.node_operation_visits_difference =
            operation_visit_sum - output.expected_actions;
        for (const auto& [key, quantity] : output.expected_consumption) {
            output.material_quantity_differences[key] =
                action_materials[key] - quantity;
        }
        double priced_dot_product = 0.0;
        for (const StrategyEvalMaterialTotal& material :
             output.material_totals) {
            if (material.priced) {
                priced_dot_product += material.expected_quantity *
                                      material.unit_price;
            }
        }
        output.cost_dot_product_difference =
            priced_dot_product - output.known_expected_cost;

        output.review_sections_enabled = !review_sections.empty();
        if (!review_sections.empty()) {
            std::vector<std::size_t> section_by_node(node_count);
            for (std::size_t section = 0; section < review_sections.size();
                 ++section) {
                for (const std::uint32_t node :
                     review_sections[section].nodes) {
                    section_by_node[node] = section;
                }
            }
            std::vector<std::map<std::string, StrategyEvalActionTotal>>
                section_actions(review_sections.size());
            std::vector<std::map<std::string, double>> section_materials(
                review_sections.size());
            output.review_sections.resize(review_sections.size());
            for (std::size_t section = 0; section < review_sections.size();
                 ++section) {
                StrategyEvalReviewSection& target =
                    output.review_sections[section];
                target.id = review_sections[section].id;
                target.label = review_sections[section].label;
                target.role = review_sections[section].role;
                target.raw_edge_ids = review_sections[section].edges;
                target.techniques = empty_technique_totals();
                for (const std::uint32_t node_index :
                     review_sections[section].nodes) {
                    target.raw_node_ids.push_back(
                        strategy->nodes[node_index].id);
                    target.expected_actions += operation_visits[node_index];
                    for (const auto& [key, value] :
                         node_techniques[node_index]) {
                        target.techniques[key] += value;
                    }
                    if (strategy->nodes[node_index].kind !=
                        StrategyNodeKind::Operation) {
                        continue;
                    }
                    const ActionDescriptor& descriptor =
                        calc.registry().actions.at(
                            model.action_by_node.at(node_index));
                    StrategyEvalActionTotal& action =
                        section_actions[section][descriptor.id];
                    if (action.id.empty()) {
                        action.id = descriptor.id;
                        action.display_name = descriptor.display_name;
                        action.price_keys = descriptor.cost_keys;
                    }
                    for (const std::string& role :
                         node_classifications[node_index]) {
                        add_classification(action.classifications, role);
                    }
                    action.expected_visits += operation_visits[node_index];
                    action.expected_applied += operation_applied[node_index];
                    action.nodes.push_back(
                        {strategy->nodes[node_index].id,
                         operation_visits[node_index],
                         operation_applied[node_index]});
                    for (const std::string& key : descriptor.cost_keys) {
                        section_materials[section][key] +=
                            operation_applied[node_index];
                    }
                }
                for (const std::string& edge :
                     review_sections[section].edges) {
                    target.expected_edge_traversals +=
                        traversal_by_edge.at(edge);
                    for (const auto& [key, value] : edge_techniques[edge]) {
                        target.techniques[key] += value;
                    }
                }
                for (auto& [unused, action] : section_actions[section]) {
                    (void)unused;
                    target.actions.push_back(std::move(action));
                }
                target.materials = price_materials(
                    section_materials[section], options.economy,
                    target.known_expected_cost, target.cost_complete);
                if (target.cost_complete) {
                    target.total_expected_cost = target.known_expected_cost;
                }
            }
            double section_actions_sum = 0.0;
            std::map<std::string, double> section_material_sum;
            for (const StrategyEvalReviewSection& section :
                 output.review_sections) {
                section_actions_sum += section.expected_actions;
                for (const StrategyEvalMaterialTotal& material :
                     section.materials) {
                    section_material_sum[material.price_key] +=
                        material.expected_quantity;
                }
            }
            output.section_actions_difference =
                section_actions_sum - output.expected_actions;
            for (const auto& [key, quantity] :
                 output.expected_consumption) {
                output.section_material_differences[key] =
                    section_material_sum[key] - quantity;
            }
        }
        output.success_normalized_enabled =
            options.include_success_normalized &&
            output.success_probability > 0.0 &&
            output.success_probability < 1.0;
        output.unresolved_probability = output.residual_mass;
        output.sweeps = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            fallback_sweeps,
            std::numeric_limits<std::uint32_t>::max()));
        output.converged =
            !hard_unresolved && output.residual_mass < options.epsilon;
        const double conservation_error = std::fabs(
            absorbed_probability(output) + output.residual_mass - 1.0);
        output.max_mass_conservation_error = std::max(
            output.max_mass_conservation_error, conservation_error);
        if (conservation_error > 1e-8) {
            throw std::runtime_error(
                "strategy evaluation mass conservation failed");
        }
        phase = StrategyEvalPhase::Done;
    }

    StrategyEvalResult forward_reference() {
        if (phase == StrategyEvalPhase::Finalization) {
            unresolved_pair.assign(pairs.size(), 0.0);
            pair_visits.assign(pairs.size(), 0.0);
            finalize();
            return output;
        }
        if (discover_index != pairs.size()) {
            throw std::logic_error(
                "strategy evaluation reference requires completed discovery");
        }
        pair_visits.assign(pairs.size(), 0.0);
        unresolved_pair.assign(pairs.size(), 0.0);
        std::vector<double> wave = external_incoming;
        std::uint32_t sweeps = 0;
        for (; sweeps < options.max_sweeps; ++sweeps) {
            std::vector<double> next(pairs.size(), 0.0);
            for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
                const double mass = wave[pair];
                if (!(mass > 0.0)) continue;
                pair_visits[pair] += mass;
                const EvalRow& row = pair_row(static_cast<std::uint32_t>(pair));
                for (const EvalTransition& transition :
                     row.transitions) {
                    const double flow = mass * transition.probability;
                    next[transition.target] += flow;
                    if (transition.edge != kNoId) {
                        edge_traversals.at(transition.edge) += flow;
                    }
                    if (transition.via != kNoId) {
                        chain_inflow.at(transition.via) += flow;
                    }
                }
                for (const EvalAbsorption& absorption :
                     row.absorptions) {
                    add_absorption(
                        absorption, mass * absorption.probability);
                }
            }
            double transient = 0.0;
            for (const double mass : next) transient += mass;
            double absorbed = 0.0;
            for (const double mass : terminal_mass) absorbed += mass;
            for (const double mass : action_not_applied) absorbed += mass;
            for (const double mass : no_matching_edge) absorbed += mass;
            const double error = std::fabs(absorbed + transient - 1.0);
            output.max_mass_conservation_error = std::max(
                output.max_mass_conservation_error, error);
            if (error > 1e-8) {
                throw std::runtime_error(
                    "strategy evaluation reference mass conservation failed");
            }
            wave = std::move(next);
            if (transient < options.epsilon) {
                ++sweeps;
                break;
            }
        }
        double residual = 0.0;
        for (std::size_t pair = 0; pair < wave.size(); ++pair) {
            unresolved_pair[pair] = wave[pair];
            residual += wave[pair];
        }
        hard_unresolved = residual >= options.epsilon;
        fallback_sweeps = sweeps;
        phase = StrategyEvalPhase::Finalization;
        finalize();
        return output;
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining-- > 0 && phase != StrategyEvalPhase::Done) {
            switch (phase) {
            case StrategyEvalPhase::Discovery:
                if (discover_index < pairs.size()) {
                    expand_pair(static_cast<std::uint32_t>(discover_index));
                    ++discover_index;
                } else {
                    build_components();
                    phase = StrategyEvalPhase::Solving;
                }
                break;
            case StrategyEvalPhase::Solving:
                solve_component();
                break;
            case StrategyEvalPhase::Fallback:
                run_fallback_batch();
                break;
            case StrategyEvalPhase::Finalization:
                finalize();
                break;
            case StrategyEvalPhase::Done:
                break;
            }
        }
    }

    StrategyEvalProgress progress() const {
        StrategyEvalProgress value;
        value.phase = phase;
        value.done = phase == StrategyEvalPhase::Done;
        value.discovered_pairs = discover_index;
        value.pending_pairs = pairs.size() - discover_index;
        value.solved_sccs = component_index;
        value.total_sccs = components.size();
        value.fallback_sweeps = fallback_sweeps;
        if (fallback != nullptr) {
            for (const double mass : fallback->wave) value.residual += mass;
        } else {
            for (const double mass : unresolved_pair) value.residual += mass;
        }
        return value;
    }
};

StrategyEvalWork::StrategyEvalWork(
    std::shared_ptr<const StrategyImpl> strategy,
    const StrategyEvalOptions& options)
    : impl_(std::make_unique<Impl>(std::move(strategy), options)) {}

StrategyEvalWork::~StrategyEvalWork() = default;
StrategyEvalWork::StrategyEvalWork(StrategyEvalWork&&) noexcept = default;
StrategyEvalWork& StrategyEvalWork::operator=(StrategyEvalWork&&) noexcept =
    default;

void StrategyEvalWork::step(std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

StrategyEvalProgress StrategyEvalWork::progress() const {
    return impl_->progress();
}

const StrategyEvalResult& StrategyEvalWork::result() const {
    if (impl_->phase != StrategyEvalPhase::Done) {
        throw std::logic_error("strategy evaluation is not finished");
    }
    return impl_->output;
}

StrategyEvalResult evaluate_strategy(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    std::shared_ptr<const StrategyImpl> borrowed(
        &strategy, [](const StrategyImpl*) {});
    StrategyEvalWork work(std::move(borrowed), options);
    while (!work.progress().done) work.step(4096);
    return work.result();
}

StrategyEvalResult evaluate_strategy_forward_reference_for_test(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    std::shared_ptr<const StrategyImpl> borrowed(
        &strategy, [](const StrategyImpl*) {});
    StrategyEvalWork work(std::move(borrowed), options);
    while (work.impl_->phase == StrategyEvalPhase::Discovery) {
        work.impl_->step(1);
    }
    return work.impl_->forward_reference();
}

namespace {

void append_material_totals_json(
    std::string& out,
    const std::vector<StrategyEvalMaterialTotal>& materials,
    double divisor = 1.0) {
    out.push_back('[');
    for (std::size_t i = 0; i < materials.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalMaterialTotal& material = materials[i];
        out += "{\"price_key\":\"" + json_escape(material.price_key) +
               "\",\"expected_quantity\":";
        append_number(out, material.expected_quantity / divisor);
        out += ",\"price_status\":\"";
        out += material.priced ? "priced" : "missing";
        out += "\",\"unit_price\":";
        if (material.priced) {
            append_number(out, material.unit_price);
        } else {
            out += "null";
        }
        out += ",\"cost_contribution\":";
        if (material.priced) {
            append_number(out, material.cost_contribution / divisor);
        } else {
            out += "null";
        }
        out.push_back('}');
    }
    out.push_back(']');
}

void append_techniques_json(
    std::string& out,
    const std::map<std::string, double>& techniques,
    double divisor = 1.0) {
    out.push_back('{');
    std::size_t index = 0;
    for (const auto& [key, value] : techniques) {
        if (index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, value / divisor);
    }
    out.push_back('}');
}

void append_action_totals_json(
    std::string& out,
    const std::vector<StrategyEvalActionTotal>& actions,
    const std::vector<StrategyEvalMaterialTotal>& priced_materials,
    double divisor = 1.0) {
    std::map<std::string, const StrategyEvalMaterialTotal*> price_by_key;
    for (const StrategyEvalMaterialTotal& material : priced_materials) {
        price_by_key.emplace(material.price_key, &material);
    }
    out.push_back('[');
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalActionTotal& action = actions[i];
        out += "{\"id\":\"" + json_escape(action.id) +
               "\",\"display_name\":\"" +
               json_escape(action.display_name) +
               "\",\"expected_visits\":";
        append_number(out, action.expected_visits / divisor);
        out += ",\"expected_applied\":";
        append_number(out, action.expected_applied / divisor);
        out += ",\"classifications\":[";
        for (std::size_t role = 0; role < action.classifications.size();
             ++role) {
            if (role != 0) out.push_back(',');
            out += "\"" + json_escape(action.classifications[role]) + "\"";
        }
        out += "],\"materials\":[";
        std::map<std::string, std::uint32_t> quantities;
        for (const std::string& key : action.price_keys) ++quantities[key];
        std::size_t material_index = 0;
        for (const auto& [key, count] : quantities) {
            if (material_index++ != 0) out.push_back(',');
            const double quantity =
                action.expected_applied * static_cast<double>(count) /
                divisor;
            out += "{\"price_key\":\"" + json_escape(key) +
                   "\",\"expected_quantity\":";
            append_number(out, quantity);
            const auto found = price_by_key.find(key);
            const bool priced =
                found != price_by_key.end() && found->second->priced;
            out += ",\"price_status\":\"";
            out += priced ? "priced" : "missing";
            out += "\",\"unit_price\":";
            if (priced) {
                append_number(out, found->second->unit_price);
            } else {
                out += "null";
            }
            out += ",\"cost_contribution\":";
            if (priced) {
                append_number(out, quantity * found->second->unit_price);
            } else {
                out += "null";
            }
            out.push_back('}');
        }
        out += "],\"raw_nodes\":[";
        for (std::size_t node = 0; node < action.nodes.size(); ++node) {
            if (node != 0) out.push_back(',');
            const StrategyEvalActionNode& entry = action.nodes[node];
            out += "{\"node_id\":\"" + json_escape(entry.node_id) +
                   "\",\"expected_visits\":";
            append_number(out, entry.expected_visits / divisor);
            out += ",\"expected_applied\":";
            append_number(out, entry.expected_applied / divisor);
            out.push_back('}');
        }
        out += "]}";
    }
    out.push_back(']');
}

void append_cost_totals_json(
    std::string& out,
    double expected_actions,
    double known_cost,
    bool complete,
    double total_cost,
    double divisor = 1.0) {
    out += "{\"expected_actions\":";
    append_number(out, expected_actions / divisor);
    out += ",\"known_expected_cost\":";
    append_number(out, known_cost / divisor);
    out += ",\"total_expected_cost\":";
    if (complete) {
        append_number(out, total_cost / divisor);
    } else {
        out += "null";
    }
    out += ",\"cost_complete\":";
    out += complete ? "true" : "false";
    out.push_back('}');
}

} // namespace

std::string serialize_strategy_eval(const StrategyEvalResult& result) {
    std::string out = "{\"version\":\"v1\",\"converged\":";
    out += result.converged ? "true" : "false";
    out += ",\"sweeps\":" + std::to_string(result.sweeps);
    out += ",\"residual_mass\":";
    append_number(out, result.residual_mass);
    out += ",\"terminals\":{";
    out += "\"success\":";
    append_number(out, result.success_probability);
    out += ",\"failure\":";
    append_number(out, result.failure_probability);
    out += ",\"stop\":";
    append_number(out, result.stop_probability);
    out += ",\"action_not_applied\":";
    append_number(out, result.action_not_applied_probability);
    out += ",\"no_matching_edge\":";
    append_number(out, result.no_matching_edge_probability);
    out += ",\"unresolved\":";
    append_number(out, result.unresolved_probability);
    out += ",\"by_node\":[";
    for (std::size_t i = 0; i < result.terminal_nodes.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalTerminalNode& node = result.terminal_nodes[i];
        out += "{\"node_id\":\"" + json_escape(node.node_id) +
               "\",\"kind\":\"" + terminal_name(node.terminal_kind) +
               "\",\"p\":";
        append_number(out, node.probability);
        out += '}';
    }
    out += "]}";

    out += ",\"unresolved_by_node\":[";
    for (std::size_t i = 0; i < result.unresolved_by_node.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalNodeMass& node = result.unresolved_by_node[i];
        out += "{\"node_id\":\"" + json_escape(node.node_id) +
               "\",\"mass\":";
        append_number(out, node.mass);
        out += '}';
    }
    out += ']';

    out += ",\"failures_by_node\":[";
    for (std::size_t i = 0; i < result.failures_by_node.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalFailure& failure = result.failures_by_node[i];
        out += "{\"node_id\":\"" + json_escape(failure.node_id) +
               "\",\"reason\":\"" + json_escape(failure.reason) +
               "\",\"p\":";
        append_number(out, failure.probability);
        out += '}';
    }
    out += ']';

    out += ",\"expected_actions\":";
    append_number(out, result.expected_actions);
    out += ",\"expected_consumption\":[";
    std::size_t consumption_index = 0;
    for (const auto& [key, quantity] : result.expected_consumption) {
        if (consumption_index++ != 0) out += ',';
        out += "{\"key\":\"" + json_escape(key) +
               "\",\"quantity\":";
        append_number(out, quantity);
        out += '}';
    }
    out += ']';

    out += ",\"accounting\":{\"version\":\"s8.4_v1\",\"semantics\":{";
    out += "\"primary\":\"per_strategy_invocation\",";
    out += "\"terminal_mass_separate\":true,";
    out += "\"success_normalized_basis\":";
    if (result.success_normalized_enabled) {
        out += "\"independent_whole_strategy_retries\"";
    } else {
        out += "null";
    }
    out += ",\"success_normalized_is_conditional_path_expectation\":false}";
    out += ",\"pricing\":{\"status\":\"";
    out += !result.pricing_enabled
               ? "disabled"
               : (result.cost_complete ? "complete" : "incomplete");
    out += "\",\"economy_id\":";
    if (result.pricing_enabled) {
        out += "\"" + json_escape(result.economy_id) + "\"";
    } else {
        out += "null";
    }
    out += ",\"missing_price_keys\":[";
    std::size_t missing_price_index = 0;
    for (const StrategyEvalMaterialTotal& material : result.material_totals) {
        if (material.priced) continue;
        if (missing_price_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(material.price_key) + "\"";
    }
    out += "]}";
    out += ",\"totals\":{\"per_invocation\":";
    append_cost_totals_json(
        out, result.expected_actions, result.known_expected_cost,
        result.cost_complete, result.total_expected_cost);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        out += "{\"basis\":\"independent_whole_strategy_retries\",";
        out += "\"success_probability_denominator\":";
        append_number(out, result.success_probability);
        out += ",\"expected_invocations\":";
        append_number(out, 1.0 / result.success_probability);
        out += ",\"work\":";
        append_cost_totals_json(
            out, result.expected_actions, result.known_expected_cost,
            result.cost_complete, result.total_expected_cost,
            result.success_probability);
        out.push_back('}');
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"actions\":{\"per_invocation\":";
    append_action_totals_json(
        out, result.action_totals, result.material_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_action_totals_json(
            out, result.action_totals, result.material_totals,
            result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"materials\":{\"per_invocation\":";
    append_material_totals_json(out, result.material_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_material_totals_json(
            out, result.material_totals, result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"techniques\":{\"per_invocation\":";
    append_techniques_json(out, result.technique_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_techniques_json(
            out, result.technique_totals, result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"review_sections\":{\"enabled\":";
    out += result.review_sections_enabled ? "true" : "false";
    out += ",\"items\":[";
    for (std::size_t i = 0; i < result.review_sections.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalReviewSection& section = result.review_sections[i];
        out += "{\"id\":\"" + json_escape(section.id) +
               "\",\"label\":\"" + json_escape(section.label) +
               "\",\"role\":\"" + json_escape(section.role) +
               "\",\"raw_references\":{\"node_ids\":[";
        for (std::size_t node = 0; node < section.raw_node_ids.size(); ++node) {
            if (node != 0) out.push_back(',');
            out += "\"" + json_escape(section.raw_node_ids[node]) + "\"";
        }
        out += "],\"edge_ids\":[";
        for (std::size_t edge = 0; edge < section.raw_edge_ids.size(); ++edge) {
            if (edge != 0) out.push_back(',');
            out += "\"" + json_escape(section.raw_edge_ids[edge]) + "\"";
        }
        out += "]},\"per_invocation\":";
        append_cost_totals_json(
            out, section.expected_actions, section.known_expected_cost,
            section.cost_complete, section.total_expected_cost);
        out += ",\"expected_edge_traversals\":";
        append_number(out, section.expected_edge_traversals);
        out += ",\"actions\":";
        append_action_totals_json(
            out, section.actions, section.materials);
        out += ",\"materials\":";
        append_material_totals_json(out, section.materials);
        out += ",\"techniques\":";
        append_techniques_json(out, section.techniques);
        out += ",\"success_normalized\":";
        if (result.success_normalized_enabled) {
            out += "{\"work\":";
            append_cost_totals_json(
                out, section.expected_actions, section.known_expected_cost,
                section.cost_complete, section.total_expected_cost,
                result.success_probability);
            out += ",\"expected_edge_traversals\":";
            append_number(
                out, section.expected_edge_traversals /
                         result.success_probability);
            out += ",\"actions\":";
            append_action_totals_json(
                out, section.actions, section.materials,
                result.success_probability);
            out += ",\"materials\":";
            append_material_totals_json(
                out, section.materials, result.success_probability);
            out += ",\"techniques\":";
            append_techniques_json(
                out, section.techniques, result.success_probability);
            out.push_back('}');
        } else {
            out += "null";
        }
        out.push_back('}');
    }
    out += "]}";
    out += ",\"reconciliation\":{\"action_descriptor_visits_difference\":";
    append_number(out, result.action_descriptor_visits_difference);
    out += ",\"action_descriptor_applied_difference\":";
    append_number(out, result.action_descriptor_applied_difference);
    out += ",\"node_operation_visits_difference\":";
    append_number(out, result.node_operation_visits_difference);
    out += ",\"material_quantity_differences\":{";
    std::size_t difference_index = 0;
    for (const auto& [key, difference] :
         result.material_quantity_differences) {
        if (difference_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, difference);
    }
    out += "},\"cost_dot_product_difference\":";
    append_number(out, result.cost_dot_product_difference);
    out += ",\"section_actions_difference\":";
    append_number(out, result.section_actions_difference);
    out += ",\"section_material_differences\":{";
    difference_index = 0;
    for (const auto& [key, difference] :
         result.section_material_differences) {
        if (difference_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, difference);
    }
    out += "}}}";

    out += ",\"targets\":[";
    for (std::size_t i = 0; i < result.targets.size(); ++i) {
        if (i != 0) out += ',';
        const GoalSlot& target = result.targets[i];
        if (target.family_id != kNoId) {
            out += "{\"kind\":\"family\",\"family_id\":" +
                   std::to_string(target.family_id) +
                   ",\"min_tier\":" + std::to_string(target.min_tier) +
                   '}';
        } else {
            out += "{\"kind\":\"group\",\"group_id\":" +
                   std::to_string(target.group_id) + '}';
        }
    }
    out += ']';

    out += ",\"nodes\":[";
    for (std::size_t i = 0; i < result.nodes.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalNode& node = result.nodes[i];
        out += "{\"id\":\"" + json_escape(node.id) +
               "\",\"expected_visits\":";
        append_number(out, node.expected_visits);
        out += ",\"classes\":[";
        for (std::size_t c = 0; c < node.classes.size(); ++c) {
            if (c != 0) out += ',';
            const StrategyEvalClass& entry = node.classes[c];
            const AbstractState& state = entry.state;
            out += "{\"share\":";
            append_number(out, entry.share);
            out += ",\"rarity\":" + std::to_string(state.rarity) +
                   ",\"prefixes\":" +
                   std::to_string(state.prefix_count) +
                   ",\"suffixes\":" +
                   std::to_string(state.suffix_count) +
                   ",\"flags\":" + std::to_string(state.flags) +
                   ",\"blocked\":" +
                   std::to_string(state.blocked_mask) + ",\"slots\":[";
            for (std::size_t slot = 0; slot < result.targets.size(); ++slot) {
                if (slot != 0) out += ',';
                out += std::to_string(state.slot_status[slot]);
            }
            out += "]}";
        }
        out += "],\"classes_truncated_share\":";
        append_number(out, node.classes_truncated_share);
        out += '}';
    }
    out += ']';

    out += ",\"edges\":[";
    for (std::size_t i = 0; i < result.edges.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalEdge& edge = result.edges[i];
        out += "{\"id\":\"" + json_escape(edge.id) +
               "\",\"expected_traversals\":";
        append_number(out, edge.expected_traversals);
        out += '}';
    }
    out += "]}";
    return out;
}

} // namespace solver
} // namespace poecraft
