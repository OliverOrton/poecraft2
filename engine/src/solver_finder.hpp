#pragma once

#include "solver_compile_contracts.hpp"
#include "solver_eval_types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace poecraft::solver {

struct FinderCandidatePreparation {
    std::shared_ptr<StrategyImpl> strategy;
    std::string refusal;

    bool ready() const { return strategy != nullptr && refusal.empty(); }
};

/* This accepts an untrusted complete ordinary graph only when its original
 * item, every success ingress and every operation are bound to the caller's
 * native request. It does not evaluate or certify the graph. */
FinderCandidatePreparation prepare_finder_candidate(
    const CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const pc_item_state& original_start,
    const std::string& strategy_json);

bool finder_evaluation_accepted(const StrategyEvalResult& result);

struct FinderCheckedPolicy {
    std::string strategy_json;
    double expected_cost = 0.0;
    double expected_actions = 0.0;
    double success_probability = 0.0;
    std::uint64_t evaluation_peak_bytes = 0;
};

struct FinderProgress {
    bool done = false;
    bool finish_requested = false;
    std::uint32_t considered = 0;
    std::uint32_t checked = 0;
    std::uint32_t accepted = 0;
    std::uint32_t refused = 0;
    std::uint32_t censored = 0;
    std::uint64_t logical_reforge_work = 0;
    std::uint64_t live_owned_bytes = 0;
    std::uint64_t peak_owned_bytes = 0;
};

/* Peer heuristic policy search. It never creates SolveWork or supplies a
 * lower/exact certificate. One native evaluator is live at most. */
class PolicyFinderWork {
  public:
    PolicyFinderWork(
        CalcContext& problem,
        std::shared_ptr<const SessionImpl> session,
        const pc_item_state& original_start,
        std::unordered_map<std::string, double> prices,
        const SolveOptions& limits);
    ~PolicyFinderWork();
    PolicyFinderWork(const PolicyFinderWork&) = delete;
    PolicyFinderWork& operator=(const PolicyFinderWork&) = delete;

    void step(std::uint32_t max_work_items);
    void request_bounded_finish();
    FinderProgress progress() const;
    const std::optional<FinderCheckedPolicy>& best() const;
    std::string telemetry_json() const;

  private:
    struct Sketch {
        std::vector<std::uint32_t> actions;
        double score = 0.0;
    };
    CalcContext& problem_;
    std::shared_ptr<const SessionImpl> session_;
    pc_item_state original_start_{};
    std::shared_ptr<EconomyImpl> economy_;
    SolveOptions limits_;
    std::vector<Sketch> frontier_;
    std::size_t cursor_ = 0;
    std::shared_ptr<StrategyImpl> checking_strategy_;
    std::unique_ptr<StrategyEvalWork> checker_;
    std::string checking_graph_;
    std::optional<FinderCheckedPolicy> best_;
    FinderProgress counters_;
    std::string last_refusal_;
    bool finish_requested_ = false;
    bool done_ = false;

    void start_next_candidate();
    void complete_active_candidate();
    void charge_active_work();
    std::uint64_t retained_owned_bytes() const;
    void update_peak();
};

} // namespace poecraft::solver
