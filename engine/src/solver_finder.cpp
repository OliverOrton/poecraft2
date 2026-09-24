#include "solver_finder.hpp"

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace poecraft::solver {
namespace {

bool same_json(const json::Value& left, const json::Value& right) {
    if (left.type != right.type) return false;
    switch (left.type) {
    case json::Type::Null: return true;
    case json::Type::Bool: return left.boolean == right.boolean;
    case json::Type::Number: return left.number == right.number;
    case json::Type::String: return left.string == right.string;
    case json::Type::Array:
        if (left.array.size() != right.array.size()) return false;
        for (std::size_t i = 0; i < left.array.size(); ++i)
            if (!same_json(left.array[i], right.array[i])) return false;
        return true;
    case json::Type::Object:
        if (left.object.size() != right.object.size()) return false;
        for (std::size_t i = 0; i < left.object.size(); ++i)
            if (left.object[i].first != right.object[i].first ||
                !same_json(left.object[i].second, right.object[i].second))
                return false;
        return true;
    }
    return false;
}

std::string text_member(const json::Value& value, const char* key) {
    const json::Value* member = value.find(key);
    return member != nullptr && member->type == json::Type::String
        ? member->string : std::string{};
}

} // namespace

FinderCandidatePreparation prepare_finder_candidate(
    const CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const pc_item_state& original_start,
    const std::string& strategy_json) {
    FinderCandidatePreparation prepared;
    try {
        if (session == nullptr || strategy_json.empty()) {
            prepared.refusal = "finder candidate has no session or graph";
            return prepared;
        }
        prepared.strategy = compile_strategy_json(
            std::move(session), strategy_json.data(), strategy_json.size());
        if (exact_item_state_key(prepared.strategy->start_item) !=
            exact_item_state_key(original_start)) {
            prepared.refusal = "finder candidate changes the original start item";
            prepared.strategy.reset();
            return prepared;
        }
        const json::Value graph = json::Parser(
            strategy_json.data(), strategy_json.size()).parse();
        const std::string goal_text = compile_finder_goal_condition(problem);
        const json::Value trusted_goal = json::Parser(
            goal_text.data(), goal_text.size()).parse();
        std::unordered_set<std::string> successes;
        for (const json::Value& node : graph.at("nodes").as_array()) {
            if (text_member(node, "kind") == "terminal" &&
                text_member(node, "terminal") == "success") {
                successes.insert(text_member(node, "id"));
            }
        }
        std::size_t guarded_ingress = 0;
        for (const json::Value& edge : graph.at("edges").as_array()) {
            if (!successes.contains(text_member(edge, "to"))) continue;
            const json::Value* condition = edge.find("condition");
            if (condition == nullptr ||
                !same_json(*condition, trusted_goal)) {
                prepared.refusal =
                    "finder success ingress is not the original native goal";
                prepared.strategy.reset();
                return prepared;
            }
            ++guarded_ingress;
        }
        if (guarded_ingress == 0) {
            prepared.refusal = "finder candidate has no guarded success ingress";
            prepared.strategy.reset();
            return prepared;
        }
        for (const StrategyNode& node : prepared.strategy->nodes) {
            if (node.kind != StrategyNodeKind::Operation) continue;
            const std::uint32_t action = resolve_strategy_action(
                node, problem.registry());
            if (action == kNoId ||
                std::find(problem.candidates().begin(),
                          problem.candidates().end(), action) ==
                    problem.candidates().end()) {
                prepared.refusal =
                    "finder operation is outside the requested action scope";
                prepared.strategy.reset();
                return prepared;
            }
        }
    } catch (const std::exception& ex) {
        prepared.refusal = ex.what();
        prepared.strategy.reset();
    }
    return prepared;
}

bool finder_evaluation_accepted(const StrategyEvalResult& result) {
    constexpr double tolerance = 1e-9;
    return result.converged && result.cost_complete &&
           std::isfinite(result.total_expected_cost) &&
           result.total_expected_cost >= 0.0 &&
           result.success_probability >= 1.0 - tolerance &&
           result.failure_probability <= tolerance &&
           result.stop_probability <= tolerance &&
           result.action_not_applied_probability <= tolerance &&
           result.no_matching_edge_probability <= tolerance &&
           result.unresolved_probability <= tolerance;
}

} // namespace poecraft::solver
