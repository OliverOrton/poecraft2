#pragma once

#include "solver_solve_types.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace poecraft {
namespace solver {
namespace solve_detail {

struct SparsePolicyCachedTransitionValue {
    std::uint32_t infinite_count = 0;
    double finite_sum = 0.0;
};

struct SparsePolicyTransitionInput {
    std::uint32_t successor = kNoId;
    double probability = 0.0;
};

struct SparsePolicyChoiceInput {
    double probability = 0.0;
    bool has_self = false;
    std::vector<std::uint32_t> successors;
};

struct SparsePolicyRowInput {
    std::uint32_t owner_state = kNoId;
    std::uint32_t operator_index = kNoId;
    double cost = kInfinity;
    bool admitted = true;
    std::vector<SparsePolicyTransitionInput> transitions;
    std::vector<SparsePolicyChoiceInput> choices;
};

struct SharedSparseTransitionSpan {
    std::uint64_t offset = 0;
    std::uint32_t count = 0;
};

/*
 * Materialize a precomputed exact probability row into the same index-stable
 * sparse arenas consumed by the broad solver. The caller remains responsible
 * for semantic admission, resource caps, and interning every successor.
 */
std::uint64_t append_sparse_policy_row(
    SolveTransitionCache& graph,
    std::vector<PricedSparseRow>& priced_rows,
    const SparsePolicyRowInput& input,
    std::optional<SharedSparseTransitionSpan> shared_transitions =
        std::nullopt);

/*
 * Evaluate one sparse Bellman row, including row-local self loops and
 * observer-choice fixed points. A caller may supply the broad solver's
 * incremental transition aggregate; exact local refinement normally omits it.
 */
double evaluate_sparse_policy_row(
    const SolveTransitionCache& graph,
    const std::vector<PricedSparseRow>& priced_rows,
    const std::vector<double>& values,
    std::size_t row_index,
    std::uint32_t& transition_work,
    std::optional<SparsePolicyCachedTransitionValue>
        cached_transitions = std::nullopt);

/* Exact stored-double complement used whenever algebraic self mass is removed
 * from a policy row. Any representable positive exit remains positive; only a
 * nonpositive complement denotes a closed/invalid row. */
double sparse_policy_exit_probability(WideFloat self_probability);

/*
 * One transitive total order for observed choices. Expected value is primary
 * and the stable successor id breaks exact ties. Bellman selection, fixed-row
 * evaluation, finalization, and compiler recipes must all use this authority;
 * pairwise epsilon ties are intentionally excluded because they cannot form a
 * consistent preference order for arbitrary offer subsets.
 */
bool sparse_policy_choice_precedes(
    double candidate_value,
    std::uint32_t candidate,
    double incumbent_value,
    std::uint32_t incumbent);

std::uint32_t select_sparse_policy_choice_successor(
    const SolveTransitionCache& graph,
    const SparseChoiceGroup& choice,
    std::uint32_t owner_state,
    const std::vector<double>& values);

struct SparsePolicyRowSelection {
    double value = kInfinity;
    std::uint64_t row =
        std::numeric_limits<std::uint64_t>::max();
    std::uint32_t transition_work = 0;
    std::uint64_t evaluated_rows = 0;
};

/*
 * Shared deterministic Howard row selection. Row iteration order remains the
 * graph's stable owner chain; epsilon ties keep the first admitted row.
 */
template <typename RowEligible, typename RowEvaluator>
SparsePolicyRowSelection select_sparse_policy_row(
        const SolveTransitionCache& graph,
        const std::uint32_t state,
        const double epsilon,
        RowEligible&& eligible,
        RowEvaluator&& evaluate) {
    SparsePolicyRowSelection selected;
    for (const std::uint64_t row : state_row_indices(graph, state)) {
        if (!eligible(row)) continue;
        std::uint32_t row_work = 0;
        const double value = evaluate(row, row_work);
        selected.transition_work += row_work;
        ++selected.evaluated_rows;
        if (value < selected.value - epsilon) {
            selected.value = value;
            selected.row = row;
        }
    }
    return selected;
}

struct SparsePolicyTarjanView {
    const std::vector<std::uint32_t>& active_states;
    const std::vector<std::uint8_t>& active;
    const std::vector<std::uint8_t>& terminal;
    const std::vector<std::uint64_t>& policy_rows;
    const std::vector<std::uint32_t>& representative;
    const std::vector<PolicyRow>& rows;
    const std::vector<PolicyEdge>& edges;
};

/*
 * Advance deterministic Tarjan discovery by at most max_work_items. Returns
 * true once every active policy state has a component assignment.
 */
bool advance_sparse_policy_components(
    const SparsePolicyTarjanView& view,
    SparsePolicyComponentWorkspace& workspace,
    std::uint32_t max_work_items);

enum class SparsePolicyComponentStatus : std::uint8_t {
    Complete,
    Incomplete,
    Singular,
    DidNotConverge,
    NumericFailure,
};

struct SparsePolicyComponentView {
    const std::vector<std::uint32_t>& members;
    std::uint32_t component = kNoId;
    const std::vector<std::uint32_t>& component_by_state;
    const std::vector<std::int32_t>& local_by_state;
    const std::vector<PolicyRow>& rows;
    const std::vector<PolicyEdge>& edges;
    const std::vector<double>& rhs;
    const std::vector<double>& previous_values;
    /* Caller-owned product limit. BiCGSTAB iterations and fallback
     * Gauss-Seidel sweeps consume the same deterministic counter. */
    std::uint32_t max_iterations;
};

struct SparsePolicyComponentResult {
    SparsePolicyComponentStatus status =
        SparsePolicyComponentStatus::Complete;
    std::vector<double> values;
    std::uint32_t iterations = 0;
    std::uint32_t total_iterations = 0;
};

/* Conservative peak allocation inside one component work unit. Sparse
 * components include the fresh result.values allocation and the overlap
 * between a retained resume and the fresh work vectors allocated before that
 * resume is consumed. Callers whose cooperative loop retains the cleared
 * result.values capacity across work units declare that ownership explicitly;
 * broad SolveWork destroys its per-call result and therefore passes false. */
std::uint64_t sparse_policy_component_scratch_bytes(
    std::size_t component_size,
    bool caller_retains_incomplete_result);

/*
 * Advance one fixed-policy component through the same WideFloat dense or
 * resumed BiCGSTAB/Gauss-Seidel solve used by production SolveWork. Large
 * components retain the historical four-iteration cooperative work unit and
 * stop at the caller-declared component iteration limit.
 */
SparsePolicyComponentResult advance_sparse_policy_component(
    const SparsePolicyComponentView& view,
    std::unique_ptr<SparsePolicyResume>& resume,
    std::vector<WideFloat>* exact_values = nullptr);

} // namespace solve_detail
} // namespace solver
} // namespace poecraft
