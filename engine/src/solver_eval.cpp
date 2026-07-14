#include "solver_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "poecraft/bitset.h"

namespace poecraft {
namespace solver {

namespace {

struct TargetEntry {
    GoalSlot slot;
    std::string origin;
};

struct EvalModel {
    std::unique_ptr<CalcContext> calc;
    std::vector<std::uint32_t> action_by_node;
    std::vector<GoalSlot> targets;
};

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
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0) {
            add_gap(
                gaps,
                "edge '" + edge_id +
                    "' condition has_mod_family(fractured=true) cannot be "
                    "represented exactly");
        }
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
            target.origin = edge_id;
            targets.push_back(std::move(target));
        }
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

EvalModel derive_model(const StrategyImpl& strategy) {
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
            true   /* preserve exact group effects between graph actions */);
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
        return state.slot_status[slot] !=
               static_cast<std::uint8_t>(GoalSlotStatus::Absent);
    }
    case ConditionKind::HasModFamily: {
        const std::size_t slot = layout_slot_for(condition, layout);
        if (condition.min_value == 0) {
            return state.slot_status[slot] !=
                   static_cast<std::uint8_t>(GoalSlotStatus::Absent);
        }
        return state.slot_status[slot] ==
               static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
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

StrategyEvalResult evaluate_strategy(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    if (strategy.session == nullptr || strategy.start_node >= strategy.nodes.size()) {
        throw std::invalid_argument("invalid compiled strategy");
    }
    if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0 ||
        options.max_sweeps == 0 || options.max_states == 0) {
        throw std::invalid_argument("invalid strategy evaluation options");
    }

    EvalModel model = derive_model(strategy);
    CalcContext& calc = *model.calc;
    StrategyEvalResult result;
    result.targets = model.targets;

    const std::size_t node_count = strategy.nodes.size();
    std::vector<double> node_visits(node_count, 0.0);
    std::vector<std::map<std::uint32_t, double>> incoming(node_count);
    std::vector<double> terminal_mass(node_count, 0.0);
    std::vector<double> action_not_applied(node_count, 0.0);
    std::vector<double> no_matching_edge(node_count, 0.0);
    std::map<std::string, double> edge_traversals;

    using Wave = std::map<std::uint64_t, double>;
    const auto pair_key = [](std::uint32_t node, std::uint32_t state) {
        return (static_cast<std::uint64_t>(node) << 32) | state;
    };
    const auto pair_node = [](std::uint64_t key) {
        return static_cast<std::uint32_t>(key >> 32);
    };
    const auto pair_state = [](std::uint64_t key) {
        return static_cast<std::uint32_t>(key & 0xffffffffu);
    };

    const std::uint32_t start_state = calc.intern_item(strategy.start_item);
    if (calc.state_count() > options.max_states) {
        throw std::runtime_error("strategy evaluation exceeded max_states");
    }
    Wave current;
    current[pair_key(strategy.start_node, start_state)] = 1.0;
    node_visits[strategy.start_node] = 1.0;
    incoming[strategy.start_node][start_state] = 1.0;

    const auto absorb_terminal = [&](std::uint32_t node_index, double mass) {
        const StrategyNode& node = strategy.nodes[node_index];
        terminal_mass[node_index] += mass;
        if (node.terminal_kind == PC_TERMINAL_SUCCESS) {
            result.success_probability += mass;
        } else if (node.terminal_kind == PC_TERMINAL_FAILURE) {
            result.failure_probability += mass;
        } else {
            result.stop_probability += mass;
        }
    };

    for (std::uint32_t sweep = 0; sweep < options.max_sweeps; ++sweep) {
        Wave next;
        const auto enqueue = [&](std::uint32_t node_index,
                                 std::uint32_t state_id,
                                 double mass) {
            if (mass == 0.0) return;
            node_visits[node_index] += mass;
            incoming[node_index][state_id] += mass;
            if (strategy.nodes[node_index].kind ==
                StrategyNodeKind::Terminal) {
                absorb_terminal(node_index, mass);
            } else {
                next[pair_key(node_index, state_id)] += mass;
            }
        };
        const auto route = [&](std::uint32_t node_index,
                               std::uint32_t state_id,
                               double mass) {
            const StrategyNode& node = strategy.nodes[node_index];
            const StrategyEdge* fallback = nullptr;
            const StrategyEdge* selected = nullptr;
            for (const StrategyEdge& edge : node.edges) {
                if (edge.is_default) {
                    fallback = &edge;
                } else if (evaluate_abstract_condition(
                               edge.condition, calc.session(), calc.layout(),
                               calc.state(state_id))) {
                    selected = &edge;
                    break;
                }
            }
            if (selected == nullptr) selected = fallback;
            if (selected == nullptr) {
                no_matching_edge[node_index] += mass;
                result.no_matching_edge_probability += mass;
                return;
            }
            edge_traversals[selected->id] += mass;
            enqueue(selected->target, state_id, mass);
        };

        for (const auto& [key, mass] : current) {
            const std::uint32_t node_index = pair_node(key);
            const std::uint32_t state_id = pair_state(key);
            const StrategyNode& node = strategy.nodes[node_index];
            if (node.kind == StrategyNodeKind::Terminal) {
                absorb_terminal(node_index, mass);
                continue;
            }
            if (node.kind != StrategyNodeKind::Operation) {
                route(node_index, state_id, mass);
                continue;
            }

            /* The simulator counts an attempted illegal operation as an
             * action, but does not consume its price keys. */
            result.expected_actions += mass;
            const std::uint32_t action_index =
                model.action_by_node[node_index];
            const ActionDescriptor& action =
                calc.registry().actions.at(action_index);
            if (!action_legal(calc.session(), action, calc.state(state_id))) {
                action_not_applied[node_index] += mass;
                result.action_not_applied_probability += mass;
                continue;
            }
            for (const std::string& key_name : action.cost_keys) {
                result.expected_consumption[key_name] += mass;
            }
            const OutcomeDistribution& outcomes =
                calc.outcomes(state_id, action_index);
            if (!outcomes.supported) {
                throw StrategyEvalUnsupported(
                    "strategy evaluation unsupported:\n- node '" + node.id +
                    "' operation '" + action.id +
                    "' has no exact distribution for a reachable state");
            }
            if (calc.state_count() > options.max_states) {
                throw std::runtime_error(
                    "strategy evaluation exceeded max_states");
            }
            double distribution_mass = 0.0;
            for (const OutcomeEntry& outcome : outcomes.entries) {
                distribution_mass += outcome.probability;
                route(
                    node_index, outcome.state,
                    mass * outcome.probability);
            }
            if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                throw std::runtime_error(
                    "strategy evaluation action distribution does not sum "
                    "to one at node '" + node.id + "'");
            }
        }

        double transient_mass = 0.0;
        for (const auto& entry : next) transient_mass += entry.second;
        const double error = std::fabs(
            absorbed_probability(result) + transient_mass - 1.0);
        result.max_mass_conservation_error =
            std::max(result.max_mass_conservation_error, error);
        if (error > 1e-8) {
            throw std::runtime_error(
                "strategy evaluation mass conservation failed");
        }
        result.sweeps = sweep + 1;
        current = std::move(next);
        if (transient_mass < options.epsilon) {
            result.converged = true;
            break;
        }
    }

    std::vector<double> unresolved(node_count, 0.0);
    for (const auto& [key, mass] : current) {
        unresolved[pair_node(key)] += mass;
        result.residual_mass += mass;
    }
    result.unresolved_probability = result.residual_mass;
    if (result.residual_mass < options.epsilon) result.converged = true;
    result.max_mass_conservation_error = std::max(
        result.max_mass_conservation_error,
        std::fabs(absorbed_probability(result) + result.residual_mass - 1.0));

    for (std::size_t i = 0; i < node_count; ++i) {
        const StrategyNode& source = strategy.nodes[i];
        if (source.kind == StrategyNodeKind::Terminal) {
            result.terminal_nodes.push_back(
                {source.id, source.terminal_kind, terminal_mass[i]});
        }
        if (unresolved[i] > 0.0) {
            result.unresolved_by_node.push_back({source.id, unresolved[i]});
        }
        if (action_not_applied[i] > 0.0) {
            result.failures_by_node.push_back(
                {source.id, "action_not_applied", action_not_applied[i]});
        }
        if (no_matching_edge[i] > 0.0) {
            result.failures_by_node.push_back(
                {source.id, "no_matching_edge", no_matching_edge[i]});
        }

        StrategyEvalNode output_node;
        output_node.id = source.id;
        output_node.expected_visits = node_visits[i];
        std::vector<std::pair<std::uint32_t, double>> classes(
            incoming[i].begin(), incoming[i].end());
        std::stable_sort(
            classes.begin(), classes.end(), [](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });
        const std::size_t keep = std::min<std::size_t>(
            options.top_classes_per_node, classes.size());
        for (std::size_t c = 0; c < keep; ++c) {
            output_node.classes.push_back(
                {node_visits[i] == 0.0
                     ? 0.0
                     : classes[c].second / node_visits[i],
                 calc.state(classes[c].first)});
        }
        double truncated = 0.0;
        for (std::size_t c = keep; c < classes.size(); ++c) {
            truncated += classes[c].second;
        }
        output_node.classes_truncated_share =
            node_visits[i] == 0.0 ? 0.0 : truncated / node_visits[i];
        result.nodes.push_back(std::move(output_node));

        for (const StrategyEdge& edge : source.edges) {
            result.edges.push_back(
                {edge.id, edge_traversals[edge.id]});
        }
    }
    return result;
}

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
