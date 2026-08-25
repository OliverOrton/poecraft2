#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {
namespace solve_detail {

/* Only this typed value may cross a proof/pruning boundary. Ordering and
 * executable-planner projections intentionally have no conversion to it. */
struct ProofLowerValue {
    double value = 0.0;
};

enum class ProofPatternKind : std::uint8_t {
    UniversalCover,
    CleanMdp,
    CarrierMdp,
    TerminalDebt,
    StrictClean,
    OperatorLower,
    Count,
};

enum class ProofPatternSolution : std::uint8_t {
    AcyclicDynamicProgram,
    MonotoneSubsolution,
    OneStepFloor,
    ExactSuccessorComposition,
};

struct ProofPatternContract {
    ProofPatternKind kind = ProofPatternKind::UniversalCover;
    std::string_view id;
    std::string_view finite_projection;
    std::string_view covered_action_shapes;
    std::string_view local_fallback;
    std::string_view immediate_price_authority;
    std::string_view optimistic_successor_authority;
    ProofPatternSolution solution =
        ProofPatternSolution::MonotoneSubsolution;
    std::string_view provenance;
    double residual = std::numeric_limits<double>::infinity();
    std::uint64_t solution_sweeps = 0;
    std::uint64_t selected_owner_calls = 0;
    bool converged = false;
};

struct ProofPatternContribution {
    ProofPatternKind owner = ProofPatternKind::UniversalCover;
    ProofLowerValue lower;
    bool available = true;
};

struct ProofPatternSelection {
    ProofLowerValue lower;
    std::uint32_t owner_mask = 0;
    bool used_zero_fallback = false;
};

struct ProofPatternFixtureTransition {
    std::uint32_t successor = 0;
    double probability = 0.0;
};

struct ProofPatternFixtureRow {
    std::uint32_t owner = 0;
    double immediate = 0.0;
    std::vector<ProofPatternFixtureTransition> transitions;
};

inline bool validate_proof_pattern_subsolution_fixture(
        const std::vector<double>& values,
        const std::vector<std::uint8_t>& goal_states,
        const std::vector<ProofPatternFixtureRow>& rows,
        const double epsilon = 1e-12) {
    if (values.size() != goal_states.size()) return false;
    for (std::size_t state = 0; state < values.size(); ++state) {
        if (!std::isfinite(values[state]) || values[state] < 0.0) {
            return false;
        }
        if (goal_states[state] && values[state] != 0.0) return false;
    }
    for (const ProofPatternFixtureRow& row : rows) {
        if (row.owner >= values.size() || !std::isfinite(row.immediate) ||
            row.immediate < 0.0) {
            return false;
        }
        double rhs = row.immediate;
        double probability = 0.0;
        for (const ProofPatternFixtureTransition& transition :
             row.transitions) {
            if (transition.successor >= values.size() ||
                !std::isfinite(transition.probability) ||
                transition.probability < 0.0) {
                return false;
            }
            probability += transition.probability;
            rhs += transition.probability * values[transition.successor];
        }
        if (probability > 1.0 + epsilon) return false;
        const double scale = std::max(
            {1.0, std::abs(values[row.owner]), std::abs(rhs)});
        if (values[row.owner] > rhs + epsilon * scale) return false;
    }
    return true;
}

class ProofPatternManager {
  public:
    ProofPatternManager()
        : contracts{{
              {ProofPatternKind::UniversalCover,
               "universal_cover",
               "exact_goal_mask",
               "all priced goal-reaching primitive shapes",
               "zero on unknown coverage",
               "descriptor resource cost",
               "free setup, perfect preservation, deterministic subset",
               ProofPatternSolution::AcyclicDynamicProgram,
               "probability_free_universal_goal_cover"},
              {ProofPatternKind::CleanMdp,
               "clean_mdp",
               "rarity x exact goal mask x prefix/suffix occupancy",
               "priced clean-carrier primitive shapes",
               "universal cover",
               "descriptor or proved primitive wrapper price",
               "optimistic pool probability and clairvoyant observation",
               ProofPatternSolution::MonotoneSubsolution,
               "clean_goal_progress_relaxation"},
              {ProofPatternKind::CarrierMdp,
               "carrier_mdp",
               "rarity x exact goal mask",
               "priced carrier-legal primitive shapes",
               "unavailable locally",
               "descriptor or proved primitive wrapper price",
               "perfect preservation/free cleanup progress envelope",
               ProofPatternSolution::MonotoneSubsolution,
               "carrier_persistent_progress_relaxation"},
              {ProofPatternKind::TerminalDebt,
               "terminal_debt",
               "exact carrier first-step legality",
               "all priced executable first steps",
               "zero for unknown or free first step",
               "minimum legal first-step price",
               "free continuation and terminal completion",
               ProofPatternSolution::OneStepFloor,
               "carrier_terminal_debt_floor"},
              {ProofPatternKind::StrictClean,
               "strict_clean",
               "strict clean states plus rare goal/occupancy projection",
               "covered registry primitive rows",
               "clean MDP escape",
               "descriptor price",
               "exact covered kernels plus optimistic escape",
               ProofPatternSolution::MonotoneSubsolution,
               "strict_clean_goal_progress_pattern"},
              {ProofPatternKind::OperatorLower,
               "operator_lower",
               "exact source carrier x planner action shape",
               "priced primitive and fixed-program operators",
               "carrier Bellman lower or negative infinity skip",
               "guaranteed first step or exact option quantity",
               "proved survivor union plus universal continuation",
               ProofPatternSolution::ExactSuccessorComposition,
               "operator_immediate_plus_optimistic_successor"},
          }} {
        ProofPatternContract& operator_contract = contract(
            ProofPatternKind::OperatorLower);
        operator_contract.residual = 0.0;
        operator_contract.solution_sweeps = 1;
        operator_contract.converged = true;
    }

    ProofPatternContract& contract(const ProofPatternKind kind) {
        return contracts.at(static_cast<std::size_t>(kind));
    }

    const ProofPatternContract& contract(
            const ProofPatternKind kind) const {
        return contracts.at(static_cast<std::size_t>(kind));
    }

    ProofPatternSelection select_maximum(
            const std::initializer_list<ProofPatternContribution>
                contributions,
            const double ceiling) const {
        ProofPatternSelection selected;
        bool finite = false;
        bool invalid_available = false;
        for (const ProofPatternContribution& contribution :
             contributions) {
            if (!contribution.available) continue;
            if (!std::isfinite(contribution.lower.value) ||
                contribution.lower.value < 0.0 ||
                contribution.lower.value >= ceiling) {
                invalid_available = true;
                continue;
            }
            if (!finite || contribution.lower.value >
                               selected.lower.value) {
                selected.lower = contribution.lower;
                selected.owner_mask =
                    std::uint32_t{1} << static_cast<std::uint32_t>(
                        contribution.owner);
                finite = true;
            } else if (contribution.lower.value ==
                       selected.lower.value) {
                selected.owner_mask |=
                    std::uint32_t{1} << static_cast<std::uint32_t>(
                        contribution.owner);
            }
        }
        if (invalid_available || !finite) {
            selected.lower = {0.0};
            selected.owner_mask = 0;
            selected.used_zero_fallback = true;
            return selected;
        }
        for (std::size_t owner = 0; owner < contracts.size(); ++owner) {
            if ((selected.owner_mask & (std::uint32_t{1} << owner)) != 0) {
                ++contracts[owner].selected_owner_calls;
            }
        }
        return selected;
    }

    mutable std::array<ProofPatternContract,
               static_cast<std::size_t>(ProofPatternKind::Count)>
        contracts;

    /* Pattern-owned runtime storage. Names intentionally match the former
     * SolveWork fields so the ownership refactor cannot perturb arithmetic. */
    std::vector<double> goal_cover_cost;
    std::vector<double> clean_goal_cover_cost;
    std::vector<double> carrier_goal_progress_cost;
    std::vector<std::uint32_t> carrier_unproved_first_step_actions;
    std::vector<std::pair<std::uint32_t, double>>
        carrier_priced_first_step_actions;
    mutable std::vector<std::int8_t>
        carrier_goal_progress_eligibility_cache;
    mutable std::vector<double> carrier_terminal_debt_cache;
    std::vector<double> clean_goal_escape_cost;
    std::vector<std::uint32_t> clean_goal_escape_action;
    std::vector<double> clean_goal_no_exalt_escape_cost;
    std::vector<std::uint32_t> clean_goal_no_exalt_escape_action;
    std::vector<double> strict_clean_goal_cover_cost;
    std::uint32_t strict_clean_goal_cover_state_count = 0;
    bool strict_clean_goal_cover_refresh_needed = false;
    bool goal_cover_cost_ready = false;
};

static_assert(!std::is_convertible_v<ProofPatternSelection, double>);

} // namespace solve_detail
} // namespace solver
} // namespace poecraft
