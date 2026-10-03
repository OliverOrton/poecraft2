#pragma once
#include "recombination.hpp"
#include <optional>
#include <functional>

namespace poecraft {
// Fully specified item acquisition: expected total cost of obtaining this exact
// represented output, including bases, mandatory crafting, failures and retries.
// The caller supplies and owns this quote; it is not a native feeder certificate.
struct RecombAcquisition {
    std::string id, source_kind, quote_identity;
    pc_item_state item{};
    double total_cost_chaos = 0;
    bool cost_complete = false;
};
struct RecombSolverRequest {
    std::shared_ptr<const SessionImpl> session;
    std::string goal_set_json, price_identity;
    std::vector<RecombAcquisition> acquisitions;
    std::vector<pc_item_state> initial_items;
    std::vector<double> initial_item_costs;
    std::optional<double> recombination_cost_chaos;
    bool recombination_cost_complete = false;
    unsigned max_items = 64, max_states = 256, max_policy_iterations = 32;
    std::uint64_t max_work = 20000000;
    std::function<bool()> cancelled;
};
enum class RecombDecisionKind { Acquire, Recombine, Discard };
struct RecombPolicyDecision {
    unsigned state = 0;
    RecombDecisionKind kind = RecombDecisionKind::Acquire;
    unsigned acquisition = 0, input_a = 0, input_b = 0;
    std::vector<std::pair<unsigned, double>> outcomes;
};
struct RecombSolverResult {
    std::string model_id = kRandomRecombModel, price_identity;
    bool game_odds_estimated = true, global_optimality_claim = false;
    bool search_converged = false, cost_complete = false;
    double expected_cost_chaos = 0, entry_cost_chaos = 0;
    double expected_recombinations = 0, expected_discards = 0;
    std::vector<double> expected_acquisitions;
    std::vector<pc_item_state> items;
    std::vector<std::vector<unsigned>> inventories;
    std::vector<bool> terminal;
    std::vector<RecombPolicyDecision> policy;
    std::vector<std::string> exclusions;
    unsigned initial_state = 0, policy_iterations = 0;
    std::uint64_t work_spent = 0;
};
// Read-only bounded policy proposal, fixed-policy numerical evaluation of the
// declared estimated mechanics/economic inputs. No global lower or exact closure.
RecombSolverResult solve_random_recomb_inventory(const RecombSolverRequest&);
}
