#pragma once

#include "solver_executable_fragment.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace poecraft {
namespace solver {
namespace fragment_v1 {

inline constexpr const char* kCleanOneGoalRenewalCaseV1 =
    "clean_one_goal_transmute_scour_renewal_v1";
inline constexpr const char* kCleanOneGoalRenewalBaseV1 =
    "Metadata/Items/Armours/BodyArmours/BodyInt17";
inline constexpr const char* kCleanOneGoalRenewalGoalModV1 =
    "LocalIncreasedEnergyShield11";

struct EngineBackedRenewalFixtureV1 {
    ExecutableFragmentIRV1 ir;
    LeafVerificationContextV1 context;
    std::shared_ptr<const ExactPrimitiveOracleV1> oracle;
    double exact_terminal_probability = 0.0;
    double exact_goal_plus_junk_probability = 0.0;
    double exact_other_nonterminal_probability = 0.0;
    std::uint64_t transmute_physical_outcomes = 0;
    std::string base_identity;
    std::string goal_identity;
    std::string forward_reference_identity;
};

struct EngineBackedRenewalBuildResultV1 {
    std::optional<EngineBackedRenewalFixtureV1> fixture;
    std::string refusal;

    bool ok() const { return fixture.has_value(); }
};

EngineBackedRenewalBuildResultV1
build_clean_one_goal_transmute_scour_renewal_v1(
    const std::string& compiled_artifact_directory);

struct IndependentFragmentEvaluationV1 {
    CanonicalIdentityV1 candidate_identity;
    std::uint64_t strategy_json_bytes = 0;
    std::uint64_t compiled_nodes = 0;
    std::uint64_t compiled_edges = 0;
    bool converged = false;
    bool proper = false;
    bool cost_complete = false;
    bool cost_reconciled = false;
    double success_probability = 0.0;
    double failure_probability = 0.0;
    double stop_probability = 0.0;
    double action_not_applied_probability = 0.0;
    double no_matching_edge_probability = 0.0;
    double unresolved_probability = 0.0;
    double expected_actions = 0.0;
    std::map<std::string, double> expected_consumption;
    double total_expected_cost = 0.0;
    double maximum_mass_error = 0.0;
    double forward_maximum_delta = 0.0;
};

struct EngineBackedFragmentEvaluationResultV1 {
    std::optional<FlattenedFragmentCandidateV1> candidate;
    std::optional<IndependentFragmentEvaluationV1> evaluation;
    std::string refusal;

    bool ok() const {
        return candidate.has_value() && evaluation.has_value();
    }
};

struct EngineBackedFragmentEvaluationLimitsV1 {
    std::uint32_t max_states = 1000000;
    std::uint32_t max_pairs = 5000000;
    std::uint64_t max_transitions = 20000000;
    std::uint64_t max_owned_bytes = 1024ull * 1024ull * 1024ull;
};

class EngineBackedFragmentEvaluatorV1 {
public:
    EngineBackedFragmentEvaluationResultV1 evaluate(
        const FlattenedFragmentCandidateV1& candidate,
        const std::string& compiled_artifact_directory,
        const std::string& economy_json = {},
        const EngineBackedFragmentEvaluationLimitsV1& limits = {}) const;
};

} // namespace fragment_v1
} // namespace solver
} // namespace poecraft
