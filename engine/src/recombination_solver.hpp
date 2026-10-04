#pragma once
#include "recombination.hpp"
#include <optional>
#include <functional>
#include <map>

namespace poecraft {
struct RecombFeederOutcome { pc_item_state item{}; double probability = 0; };
class RecombCheckedFeeder {
public:
    const std::string& document() const { return document_; }
    const std::string& strategy_id() const { return strategy_id_; }
    const std::string& revision() const { return revision_; }
    const std::string& output_contract_id() const { return output_contract_id_; }
    const std::vector<RecombFeederOutcome>& outcomes() const { return outcomes_; }
    double total_cost() const { return cost_; }
    double child_actions() const { return actions_; }
    double acquisition_cost() const { return acquisition_cost_; }
    const std::shared_ptr<const EconomyImpl>& economy() const { return economy_; }
    const std::shared_ptr<const SessionImpl>& session() const { return session_; }
    const std::map<std::string,double>& materials() const { return materials_; }
private:
    RecombCheckedFeeder() = default;
    std::string document_, strategy_id_, revision_, output_contract_id_;
    std::shared_ptr<const SessionImpl> session_;
    std::shared_ptr<const EconomyImpl> economy_;
    std::vector<RecombFeederOutcome> outcomes_;
    std::map<std::string,double> materials_;
    double cost_ = 0, actions_ = 0, acquisition_cost_ = 0;
    friend std::shared_ptr<const RecombCheckedFeeder> check_recomb_feeder(
        std::shared_ptr<const SessionImpl>, const std::string&, const std::string&,
        const std::string&, const std::string&, double, std::shared_ptr<const EconomyImpl>,
        const std::function<bool()>&);
};
// Successful single-item children only: full removal laws, native routers, no nested inventory.
std::shared_ptr<const RecombCheckedFeeder> check_recomb_feeder(
    std::shared_ptr<const SessionImpl>, const std::string& strategy_id, const std::string& revision,
    const std::string& document_json, const std::string& output_contract_id,
    double paid_start_cost, std::shared_ptr<const EconomyImpl>, const std::function<bool()>& cancelled = {});
// Fully specified item acquisition: expected total cost of obtaining this exact
// represented output, including bases, mandatory crafting, failures and retries.
// The caller supplies and owns this quote; it is not a native feeder certificate.
struct RecombAcquisition {
    std::string id, source_kind, quote_identity;
    pc_item_state item{};
    double total_cost_chaos = 0;
    bool cost_complete = false;
    std::shared_ptr<const RecombCheckedFeeder> checked_feeder;
};
struct RecombSolverRequest {
    std::shared_ptr<const SessionImpl> session;
    std::string goal_set_json, price_identity, model_id = kRandomRecombModel;
    std::optional<RecombScenario> scenario;
    std::vector<RecombAcquisition> acquisitions;
    std::vector<pc_item_state> initial_items;
    std::vector<double> initial_item_costs;
    std::optional<double> recombination_cost_chaos;
    bool recombination_cost_complete = false;
    bool allow_incomplete_costs = false;
    unsigned max_items = 64, max_states = 256, max_policy_iterations = 32;
    std::uint64_t max_work = 20000000;
    std::function<bool()> cancelled;
};
enum class RecombDecisionKind { Acquire, Recombine, Discard, Child };
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
    double expected_recombinations = 0, expected_discards = 0, expected_child_actions = 0;
    std::vector<double> expected_acquisitions;
    std::vector<pc_item_state> items;
    std::vector<std::vector<unsigned>> inventories;
    std::vector<bool> terminal, item_is_goal;
    std::vector<RecombPolicyDecision> policy;
    std::vector<std::string> exclusions;
    unsigned initial_state = 0, policy_iterations = 0;
    std::uint64_t work_spent = 0;
};
// Read-only bounded policy proposal, fixed-policy numerical evaluation of the
// declared estimated mechanics/economic inputs. No global lower or exact closure.
RecombSolverResult solve_random_recomb_inventory(const RecombSolverRequest&);
struct RecombBuilderExport {
    std::string strategy_json, economy_json;
    double checked_cost = 0, checked_recombinations = 0, checked_discards = 0,
        checked_child_actions = 0, checked_builder_actions = 0;
    std::vector<double> checked_acquisitions;
};
// Restricted native export/check. General authored inventory evaluator stays held.
RecombBuilderExport export_recomb_builder_policy(const RecombSolverRequest&, const RecombSolverResult&);
RecombBuilderExport check_recomb_builder_policy(const RecombSolverRequest&, const RecombSolverResult&,
    const std::string& strategy_json);
}
