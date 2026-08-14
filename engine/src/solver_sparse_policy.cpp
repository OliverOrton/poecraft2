#include "solver_sparse_policy.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace poecraft {
namespace solver {
namespace solve_detail {
namespace {

std::uint64_t saturated_scratch_add(
        const std::uint64_t left,
        const std::uint64_t right) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return left + right;
}

std::uint64_t saturated_scratch_product(
        const std::size_t count,
        const std::size_t width) {
    if (width != 0 &&
        count > std::numeric_limits<std::uint64_t>::max() / width) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(count) *
           static_cast<std::uint64_t>(width);
}

bool finite_wide(const WideFloat value) {
    return std::isfinite(value.high) &&
           std::isfinite(value.low) &&
           std::isfinite(value.value());
}

WideFloat wide_dot(
        const std::vector<WideFloat>& left,
        const std::vector<WideFloat>& right) {
    WideFloat value = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        value += left[i] * right[i];
    }
    return value;
}

double wide_norm(const std::vector<WideFloat>& values) {
    const double squared = wide_dot(values, values).value();
    if (!std::isfinite(squared)) {
        return std::numeric_limits<double>::infinity();
    }
    return std::sqrt(std::max(0.0, squared));
}

constexpr double kFixedPolicyRelativeTolerance = 1e-18;

} // namespace

std::uint64_t append_sparse_policy_row(
        SolveTransitionCache& graph,
        std::vector<PricedSparseRow>& priced_rows,
        const SparsePolicyRowInput& input,
        const std::optional<SharedSparseTransitionSpan> shared_transitions) {
    if (input.owner_state == kNoId) {
        throw std::invalid_argument(
            "sparse policy row has no owner state");
    }
    if (priced_rows.size() != graph.rows.size()) {
        throw std::logic_error(
            "sparse policy rows and prices are misaligned");
    }
    if (input.owner_state >= graph.state_rows.size()) {
        graph.state_rows.resize(input.owner_state + 1);
    }
    graph.discovered_states = std::max(
        graph.discovered_states, input.owner_state + 1);

    SparseRow row;
    row.owner_state = input.owner_state;
    row.admitted = input.admitted;
    row.transition_offset = shared_transitions.has_value()
                                ? shared_transitions->offset
                                : graph.successors.size();
    if (shared_transitions.has_value() &&
        (shared_transitions->count != input.transitions.size() ||
         shared_transitions->offset > graph.successors.size() ||
         shared_transitions->count >
             graph.successors.size() - shared_transitions->offset)) {
        throw std::invalid_argument(
            "shared sparse transition span does not match the row");
    }
    for (std::size_t index = 0; index < input.transitions.size(); ++index) {
        const SparsePolicyTransitionInput& transition =
            input.transitions[index];
        if (transition.successor == kNoId) {
            throw std::invalid_argument(
                "sparse policy transition has no successor");
        }
        if (shared_transitions.has_value()) {
            const std::uint64_t offset =
                shared_transitions->offset + index;
            if (graph.successors.at(offset) != transition.successor ||
                graph.probabilities.at(offset) != transition.probability) {
                throw std::invalid_argument(
                    "shared sparse transition span changed its exact row");
            }
        } else {
            graph.successors.push_back(transition.successor);
            graph.probabilities.push_back(transition.probability);
        }
        if (transition.successor == input.owner_state) {
            row.self_probability += transition.probability;
        }
    }
    row.transition_count = static_cast<std::uint32_t>(
        input.transitions.size());
    row.embedded_self_probability = row.self_probability;
    row.self_probability_embedded =
        row.embedded_self_probability > 0.0;

    row.choice_offset = graph.choices.size();
    for (const SparsePolicyChoiceInput& choice : input.choices) {
        SparseChoiceGroup stored;
        stored.successor_offset =
            graph.choice_successors.size();
        stored.probability = choice.probability;
        stored.has_self = choice.has_self;
        for (const std::uint32_t successor : choice.successors) {
            if (successor == kNoId) {
                throw std::invalid_argument(
                    "sparse policy choice has no successor");
            }
            if (successor == input.owner_state) {
                stored.has_self = true;
            } else {
                graph.choice_successors.push_back(successor);
            }
        }
        stored.successor_count = static_cast<std::uint32_t>(
            graph.choice_successors.size() -
            stored.successor_offset);
        graph.choices.push_back(stored);
    }
    row.choice_count = static_cast<std::uint32_t>(
        graph.choices.size() - row.choice_offset);
    if (row.self_probability > 0.0) {
        ++graph.algebraic_self_loops;
    }
    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
        if (graph.choices[row.choice_offset + i].has_self) {
            ++graph.algebraic_self_loops;
        }
    }

    const std::uint64_t row_index = graph.rows.size();
    StateRowSpan& owner_rows =
        graph.state_rows[input.owner_state];
    if (owner_rows.count == 0) {
        owner_rows.offset = row_index;
    } else {
        graph.rows.at(owner_rows.tail).next_owner_row =
            row_index;
    }
    owner_rows.tail = row_index;
    ++owner_rows.count;
    graph.rows.push_back(row);

    PricedSparseRow priced;
    priced.operator_index = input.operator_index;
    priced.cost = input.cost;
    priced_rows.push_back(priced);
    return row_index;
}

template <typename ValueAt>
double evaluate_sparse_policy_row_impl(
        const SolveTransitionCache& graph,
        const std::vector<PricedSparseRow>& priced_rows,
        const std::size_t row_index,
        std::uint32_t& transition_work,
        ValueAt&& value_at,
        const std::optional<SparsePolicyCachedTransitionValue>
            cached_transitions) {
    const SparseRow& row = graph.rows.at(row_index);
    double constant = priced_rows.at(row_index).cost;
    transition_work = 0;
    if (!std::isfinite(constant) || constant < 0.0) {
        return kInfinity;
    }
    const auto finite_or_infinity = [](const double value) {
        return std::isfinite(value) ? value : kInfinity;
    };
    if (cached_transitions.has_value() &&
        row.choice_count == 0 &&
        row.transition_count >= 1024) {
        const double self_value = finite_or_infinity(
            value_at(row.owner_state));
        const bool infinite_self =
            row.embedded_self_probability > 0.0 &&
            self_value == kInfinity;
        const std::uint32_t non_self_infinite =
            cached_transitions->infinite_count -
            (infinite_self ? 1u : 0u);
        transition_work += row.transition_count;
        if (non_self_infinite != 0) return kInfinity;
        constant += cached_transitions->finite_sum;
        if (!infinite_self) {
            constant -=
                row.embedded_self_probability * self_value;
        }
    } else {
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            if (graph.successors.at(offset) == row.owner_state) {
                continue;
            }
            const double value = finite_or_infinity(
                value_at(graph.successors.at(offset)));
            ++transition_work;
            if (value == kInfinity) return kInfinity;
            constant += graph.probabilities.at(offset) * value;
        }
    }

    struct SelfChoice {
        double alternate = kInfinity;
        std::uint32_t alternate_state = kNoId;
        double probability = 0.0;
    };
    std::vector<SelfChoice> self_choices;
    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
        const SparseChoiceGroup& group =
            graph.choices.at(row.choice_offset + i);
        double best = kInfinity;
        std::uint32_t best_state = kNoId;
        for (std::uint32_t s = 0; s < group.successor_count; ++s) {
            const std::uint32_t candidate =
                graph.choice_successors.at(
                    group.successor_offset + s);
            const double candidate_value = finite_or_infinity(
                value_at(candidate));
            if (sparse_policy_choice_precedes(
                    candidate_value, candidate,
                    best, best_state)) {
                best = candidate_value;
                best_state = candidate;
            }
        }
        transition_work += group.successor_count;
        if (group.has_self) {
            self_choices.push_back(
                {best, best_state, group.probability});
        } else {
            if (best == kInfinity) return kInfinity;
            constant += group.probability * best;
        }
    }

    /*
     * Solve the row-local fixed point exactly:
     *
     * x = constant + p_self*x + sum g*min(x, alternate_g)
     */
    std::sort(
        self_choices.begin(), self_choices.end(),
        [](const auto& left, const auto& right) {
            return sparse_policy_choice_precedes(
                left.alternate,
                left.alternate_state,
                right.alternate,
                right.alternate_state);
        });
    double loop_probability = row.self_probability;
    for (const SelfChoice& choice : self_choices) {
        loop_probability += choice.probability;
    }
    std::size_t fixed_choices = 0;
    while (true) {
        const double denominator = sparse_policy_exit_probability(
            WideFloat{loop_probability});
        const double value = denominator > 0.0
                                 ? constant / denominator
                                 : kInfinity;
        if (fixed_choices >= self_choices.size() ||
            sparse_policy_choice_precedes(
                value, row.owner_state,
                self_choices[fixed_choices].alternate,
                self_choices[fixed_choices].alternate_state)) {
            return finite_or_infinity(value);
        }
        const SelfChoice& fixed =
            self_choices[fixed_choices++];
        const double alternate = fixed.alternate;
        const double probability = fixed.probability;
        if (!std::isfinite(alternate)) return kInfinity;
        constant += probability * alternate;
        loop_probability -= probability;
    }
}

double evaluate_sparse_policy_row(
        const SolveTransitionCache& graph,
        const std::vector<PricedSparseRow>& priced_rows,
        const std::vector<double>& values,
        const std::size_t row_index,
        std::uint32_t& transition_work,
        const std::optional<SparsePolicyCachedTransitionValue>
            cached_transitions) {
    return evaluate_sparse_policy_row_impl(
        graph, priced_rows, row_index, transition_work,
        [&](const std::uint32_t state) {
            return values.at(state);
        },
        cached_transitions);
}

double evaluate_sparse_policy_row_with_accessor(
        const SolveTransitionCache& graph,
        const std::vector<PricedSparseRow>& priced_rows,
        const std::size_t row_index,
        std::uint32_t& transition_work,
        const SparsePolicyStateValueAccessor value_at,
        const void* const value_context) {
    if (value_at == nullptr) {
        throw std::invalid_argument(
            "sparse policy row value accessor is null");
    }
    return evaluate_sparse_policy_row_impl(
        graph, priced_rows, row_index, transition_work,
        [&](const std::uint32_t state) {
            return value_at(value_context, state);
        },
        std::nullopt);
}

double sparse_policy_exit_probability(
        const WideFloat self_probability) {
    const WideFloat complement =
        WideFloat{1.0} - self_probability;
    const double value = complement.value();
    return value > 0.0 ? value : 0.0;
}

bool sparse_policy_choice_precedes(
        const double candidate_value,
        const std::uint32_t candidate,
        const double incumbent_value,
        const std::uint32_t incumbent) {
    if (candidate == kNoId) return false;
    if (incumbent == kNoId) return true;
    if (candidate_value < incumbent_value) return true;
    if (incumbent_value < candidate_value) return false;
    return candidate < incumbent;
}

std::uint32_t select_sparse_policy_choice_successor(
        const SolveTransitionCache& graph,
        const SparseChoiceGroup& choice,
        const std::uint32_t owner_state,
        const std::vector<double>& values) {
    std::uint32_t selected =
        choice.has_self ? owner_state : kNoId;
    double selected_value =
        choice.has_self ? values.at(owner_state) : kInfinity;
    for (std::uint32_t successor = 0;
         successor < choice.successor_count; ++successor) {
        const std::uint32_t candidate =
            graph.choice_successors.at(
                choice.successor_offset + successor);
        const double candidate_value = values.at(candidate);
        if (sparse_policy_choice_precedes(
                candidate_value, candidate,
                selected_value, selected)) {
            selected = candidate;
            selected_value = candidate_value;
        }
    }
    return selected;
}

bool sparse_policy_row_precedes(
        const double candidate_value,
        const std::uint64_t candidate_row,
        const double incumbent_value,
        const std::uint64_t incumbent_row) {
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    if (candidate_row == no_row) return false;
    if (!std::isfinite(candidate_value)) return false;
    if (incumbent_row == no_row) return true;
    if (candidate_value < incumbent_value) return true;
    if (incumbent_value < candidate_value) return false;
    return candidate_row < incumbent_row;
}

bool sparse_policy_value_improvement_exceeds_tolerance(
        const double candidate_value,
        const double incumbent_value,
        const double tolerance) {
    return candidate_value <
        incumbent_value - std::max(0.0, tolerance);
}

SparsePolicyReplacementDecision sparse_policy_replacement_decision(
        const double candidate_value,
        const std::uint64_t candidate_row,
        const double incumbent_value,
        const std::uint64_t incumbent_row,
        const double stability_tolerance) {
    if (!sparse_policy_row_precedes(
            candidate_value, candidate_row,
            incumbent_value, incumbent_row)) {
        return SparsePolicyReplacementDecision::Unchanged;
    }
    if (candidate_value == incumbent_value) {
        return SparsePolicyReplacementDecision::Replace;
    }
    return sparse_policy_value_improvement_exceeds_tolerance(
               candidate_value,
               incumbent_value,
               stability_tolerance)
        ? SparsePolicyReplacementDecision::Replace
        : SparsePolicyReplacementDecision::SuppressedStrictImprovement;
}

bool advance_sparse_policy_components(
        const SparsePolicyTarjanView& view,
        SparsePolicyComponentWorkspace& workspace,
        const std::uint32_t max_work_items) {
    const std::size_t state_count = view.active.size();
    if (workspace.components_ready) return true;
    if (workspace.tarjan_index.empty()) {
        workspace.tarjan_index.assign(state_count, kNoId);
        workspace.tarjan_lowlink.assign(state_count, kNoId);
        workspace.tarjan_on_stack.assign(state_count, 0);
    }
    const auto is_representative =
        [&](const std::uint32_t state) {
            return view.representative.empty() ||
                   view.representative.at(state) == state;
        };
    const auto push = [&](const std::uint32_t state) {
        workspace.tarjan_index[state] =
            workspace.tarjan_lowlink[state] =
                workspace.tarjan_next_index++;
        workspace.tarjan_stack.push_back(state);
        workspace.tarjan_on_stack[state] = 1;
        workspace.tarjan_dfs.push_back({state, 0});
    };

    std::uint32_t work = 0;
    while (work < max_work_items) {
        if (workspace.tarjan_dfs.empty()) {
            while (workspace.tarjan_root_cursor <
                       view.active_states.size() &&
                   work < max_work_items) {
                const std::uint32_t root =
                    view.active_states[
                        workspace.tarjan_root_cursor++];
                ++work;
                if (!view.active.at(root) ||
                    view.terminal.at(root) ||
                    view.policy_rows.at(root) ==
                        std::numeric_limits<std::uint64_t>::max() ||
                    !is_representative(root) ||
                    workspace.tarjan_index[root] != kNoId) {
                    continue;
                }
                push(root);
                break;
            }
            if (workspace.tarjan_dfs.empty()) break;
        }
        PolicyTarjanFrame& frame = workspace.tarjan_dfs.back();
        const PolicyRow& row = view.rows.at(frame.state);
        if (frame.next_edge < row.edge_count) {
            const std::uint32_t target = view.edges.at(
                row.edge_offset + frame.next_edge++).target;
            ++work;
            if (target >= state_count ||
                !view.active.at(target) ||
                view.terminal.at(target) ||
                view.policy_rows.at(target) ==
                    std::numeric_limits<std::uint64_t>::max()) {
                continue;
            }
            if (workspace.tarjan_index[target] == kNoId) {
                push(target);
            } else if (workspace.tarjan_on_stack[target]) {
                workspace.tarjan_lowlink[frame.state] = std::min(
                    workspace.tarjan_lowlink[frame.state],
                    workspace.tarjan_index[target]);
            }
            continue;
        }
        const std::uint32_t completed = frame.state;
        workspace.tarjan_dfs.pop_back();
        ++work;
        if (!workspace.tarjan_dfs.empty()) {
            const std::uint32_t parent =
                workspace.tarjan_dfs.back().state;
            workspace.tarjan_lowlink[parent] = std::min(
                workspace.tarjan_lowlink[parent],
                workspace.tarjan_lowlink[completed]);
        }
        if (workspace.tarjan_lowlink[completed] ==
            workspace.tarjan_index[completed]) {
            workspace.components.emplace_back();
            while (true) {
                const std::uint32_t member =
                    workspace.tarjan_stack.back();
                workspace.tarjan_stack.pop_back();
                workspace.tarjan_on_stack[member] = 0;
                workspace.components.back().push_back(member);
                if (member == completed) break;
            }
            std::sort(
                workspace.components.back().begin(),
                workspace.components.back().end());
        }
    }

    if (workspace.tarjan_root_cursor >=
            view.active_states.size() &&
        workspace.tarjan_dfs.empty()) {
        workspace.component_by_state.assign(state_count, kNoId);
        for (std::uint32_t component = 0;
             component < workspace.components.size(); ++component) {
            for (const std::uint32_t state :
                 workspace.components[component]) {
                workspace.component_by_state[state] = component;
            }
        }
        workspace.local.assign(state_count, -1);
        workspace.components_ready = true;
    }
    return workspace.components_ready;
}

std::uint64_t sparse_policy_component_scratch_bytes(
        const std::size_t component_size,
        const bool caller_retains_incomplete_result) {
    std::uint64_t bytes = saturated_scratch_product(
        component_size, sizeof(double));
    if (component_size <= kDensePolicyComponentLimit) {
        if (component_size ==
            std::numeric_limits<std::size_t>::max()) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        const std::uint64_t matrix_elements =
            saturated_scratch_product(
                component_size, component_size + 1);
        bytes = saturated_scratch_add(
            bytes,
            matrix_elements ==
                    std::numeric_limits<std::uint64_t>::max()
                ? matrix_elements
                : saturated_scratch_product(
                      static_cast<std::size_t>(matrix_elements),
                      sizeof(WideFloat)));
        return saturated_scratch_add(
            bytes,
            saturated_scratch_product(
                component_size, sizeof(WideFloat)));
    }

    /* A resumed sparse work unit allocates its fresh b/x/r/r0/p/v/s/t
     * vectors before it moves the identically sized retained vectors out of
     * SparsePolicyResume. Account for that overlap, the retained member list
     * and object, the fresh return object's result.values, and (when declared)
     * the caller's cleared result.values capacity from the prior incomplete
     * unit. This is deliberately the worst resumed-call peak so every caller
     * gets a safe bound without having to mirror allocation order. */
    if (caller_retains_incomplete_result) {
        bytes = saturated_scratch_add(
            bytes,
            saturated_scratch_product(
                component_size, sizeof(double)));
    }
    const std::uint64_t one_wide_set = saturated_scratch_product(
        component_size, 8 * sizeof(WideFloat));
    bytes = saturated_scratch_add(bytes, one_wide_set);
    bytes = saturated_scratch_add(bytes, one_wide_set);
    bytes = saturated_scratch_add(
        bytes,
        saturated_scratch_product(
            component_size, sizeof(std::uint32_t)));
    return saturated_scratch_add(bytes, sizeof(SparsePolicyResume));
}

SparsePolicyComponentResult advance_sparse_policy_component(
        const SparsePolicyComponentView& view,
        std::unique_ptr<SparsePolicyResume>& resume,
        std::vector<WideFloat>* exact_values) {
    SparsePolicyComponentResult result;
    if (exact_values != nullptr) exact_values->clear();
    const std::size_t n = view.members.size();
    result.values.assign(n, 0.0);

    if (n <= kDensePolicyComponentLimit) {
        const std::size_t stride = n + 1;
        std::vector<WideFloat> matrix(n * stride, 0.0);
        const auto cell = [&](const std::size_t row,
                              const std::size_t column) -> WideFloat& {
            return matrix[row * stride + column];
        };
        for (std::size_t i = 0; i < n; ++i) {
            cell(i, i) = 1.0;
            cell(i, n) = view.rhs.at(i);
            const PolicyRow& row =
                view.rows.at(view.members[i]);
            for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                const PolicyEdge& edge =
                    view.edges.at(row.edge_offset + e);
                if (view.component_by_state.at(edge.target) ==
                    view.component) {
                    cell(
                        i,
                        static_cast<std::size_t>(
                            view.local_by_state.at(edge.target))) -=
                        WideFloat{edge.probability};
                }
            }
        }
        for (std::size_t column = 0; column < n; ++column) {
            std::size_t pivot = column;
            for (std::size_t row = column + 1; row < n; ++row) {
                if (std::fabs(cell(row, column).value()) >
                    std::fabs(cell(pivot, column).value())) {
                    pivot = row;
                }
            }
            if (!finite_wide(cell(pivot, column))) {
                result.status =
                    SparsePolicyComponentStatus::NumericFailure;
                return result;
            }
            if (cell(pivot, column).high == 0.0 &&
                cell(pivot, column).low == 0.0) {
                result.status =
                    SparsePolicyComponentStatus::Singular;
                return result;
            }
            if (pivot != column) {
                for (std::size_t k = column; k <= n; ++k) {
                    std::swap(cell(pivot, k), cell(column, k));
                }
            }
            for (std::size_t row = column + 1; row < n; ++row) {
                const WideFloat factor =
                    cell(row, column) / cell(column, column);
                if (!finite_wide(factor)) {
                    result.status =
                        SparsePolicyComponentStatus::NumericFailure;
                    return result;
                }
                if (factor.high == 0.0 && factor.low == 0.0) continue;
                for (std::size_t k = column; k <= n; ++k) {
                    cell(row, k) -= factor * cell(column, k);
                }
            }
        }
        std::vector<WideFloat> solved_wide(n, 0.0);
        for (std::size_t back = n; back-- > 0;) {
            WideFloat value = cell(back, n);
            for (std::size_t column = back + 1;
                 column < n; ++column) {
                value -= cell(back, column) * solved_wide[column];
            }
            solved_wide[back] = value / cell(back, back);
            if (!finite_wide(solved_wide[back])) {
                result.status =
                    SparsePolicyComponentStatus::NumericFailure;
                result.values.clear();
                return result;
            }
        }

        WideFloat residual_squared = 0.0;
        WideFloat rhs_squared = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const WideFloat dense_rhs = view.rhs.at(i);
            rhs_squared += dense_rhs * dense_rhs;
            WideFloat expected = dense_rhs;
            const PolicyRow& row =
                view.rows.at(view.members[i]);
            for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                const PolicyEdge& edge =
                    view.edges.at(row.edge_offset + e);
                if (view.component_by_state.at(edge.target) !=
                    view.component) {
                    continue;
                }
                expected += WideFloat{edge.probability} *
                            solved_wide[static_cast<std::size_t>(
                                view.local_by_state.at(edge.target))];
            }
            const WideFloat residual = solved_wide[i] - expected;
            residual_squared += residual * residual;
        }
        const double rhs_squared_value = rhs_squared.value();
        const double residual_squared_value = residual_squared.value();
        const double rhs_norm = std::isfinite(rhs_squared_value)
                                    ? std::sqrt(std::max(
                                          0.0, rhs_squared_value))
                                    : std::numeric_limits<double>::infinity();
        const double tolerance =
            kFixedPolicyRelativeTolerance *
            std::max(1.0, rhs_norm);
        const double checked_residual =
            std::isfinite(residual_squared_value)
                ? std::sqrt(std::max(0.0, residual_squared_value))
                : std::numeric_limits<double>::infinity();
        if (!std::isfinite(checked_residual) ||
            checked_residual > tolerance) {
            result.status =
                SparsePolicyComponentStatus::NumericFailure;
            result.values.clear();
            return result;
        }
        for (std::size_t i = 0; i < n; ++i) {
            result.values[i] = solved_wide[i].value();
            if (!std::isfinite(result.values[i])) {
                result.status =
                    SparsePolicyComponentStatus::NumericFailure;
                result.values.clear();
                return result;
            }
        }
        if (exact_values != nullptr) {
            *exact_values = std::move(solved_wide);
        }
        resume.reset();
        return result;
    }

    std::vector<WideFloat> b(n);
    for (std::size_t i = 0; i < n; ++i) {
        b[i] = view.rhs.at(i);
    }
    const bool can_resume =
        resume != nullptr &&
        resume->members == view.members &&
        resume->b == b;
    std::vector<WideFloat> x(n), r(n), r0(n), p(n, 0.0),
        v(n, 0.0), s(n), t(n);
    WideFloat rho_previous = 1.0;
    WideFloat alpha = 1.0;
    WideFloat omega = 1.0;
    std::uint32_t resumed_iterations = 0;
    SparsePolicySolveMode mode = SparsePolicySolveMode::BiCGSTAB;
    double last_true_residual =
        std::numeric_limits<double>::infinity();
    std::uint8_t true_residual_stagnation = 0;
    if (can_resume) {
        x = std::move(resume->x);
        r = std::move(resume->r);
        r0 = std::move(resume->r0);
        p = std::move(resume->p);
        v = std::move(resume->v);
        s = std::move(resume->s);
        t = std::move(resume->t);
        rho_previous = resume->rho_previous;
        alpha = resume->alpha;
        omega = resume->omega;
        resumed_iterations = resume->iterations;
        mode = resume->mode;
        last_true_residual = resume->last_true_residual;
        true_residual_stagnation =
            resume->true_residual_stagnation;
        resume.reset();
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            const double previous =
                view.previous_values.at(view.members[i]);
            x[i] = std::isfinite(previous) && previous >= 0.0 &&
                           previous < kValueCeiling
                       ? previous
                       : 0.0;
        }
    }

    const auto multiply = [&](
        const std::vector<WideFloat>& input,
        std::vector<WideFloat>& output) {
        for (std::size_t i = 0; i < n; ++i) {
            WideFloat internal_sum = 0.0;
            const PolicyRow& row =
                view.rows.at(view.members[i]);
            for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                const PolicyEdge& edge =
                    view.edges.at(row.edge_offset + e);
                if (view.component_by_state.at(edge.target) ==
                    view.component) {
                    const WideFloat product =
                        edge.probability *
                        input[static_cast<std::size_t>(
                            view.local_by_state.at(edge.target))];
                    internal_sum = internal_sum + product;
                }
            }
            output[i] = input[i] - internal_sum;
        }
    };
    const auto true_residual = [&](std::vector<WideFloat>& output) {
        multiply(x, t);
        for (std::size_t i = 0; i < n; ++i) {
            output[i] = b[i] - t[i];
        }
        return wide_norm(output);
    };
    const auto reset_bicgstab = [&]() {
        r0 = r;
        std::fill(p.begin(), p.end(), 0.0);
        std::fill(v.begin(), v.end(), 0.0);
        rho_previous = 1.0;
        alpha = 1.0;
        omega = 1.0;
    };

    const double tolerance =
        kFixedPolicyRelativeTolerance * std::max(1.0, wide_norm(b));
    if (!can_resume) {
        last_true_residual = true_residual(r);
        r0 = r;
    }
    bool converged = last_true_residual <= tolerance;
    const std::size_t max_iterations = view.max_iterations;
    constexpr std::size_t kIterationsPerWorkUnit = 4;
    const std::size_t remaining_iterations =
        resumed_iterations < max_iterations
            ? max_iterations - resumed_iterations
            : 0;
    const std::size_t work_unit_iterations = std::min(
        remaining_iterations, kIterationsPerWorkUnit);
    std::size_t iterations = 0;
    bool bicgstab_breakdown = false;
    bool bicgstab_candidate_converged = false;

    if (!converged &&
        mode == SparsePolicySolveMode::BiCGSTAB) {
        for (; iterations < work_unit_iterations; ++iterations) {
            /* Count a failed Krylov step against the same bounded work budget
             * as a completed step; otherwise an exact breakdown could resume
             * forever without advancing the declared iteration limit. */
            const WideFloat rho = wide_dot(r0, r);
            if (rho.value() == 0.0 || omega.value() == 0.0 ||
                !finite_wide(rho) || !finite_wide(omega) ||
                !finite_wide(rho_previous) || !finite_wide(alpha)) {
                bicgstab_breakdown = true;
                ++iterations;
                break;
            }
            const WideFloat beta =
                (rho / rho_previous) * (alpha / omega);
            if (!finite_wide(beta)) {
                bicgstab_breakdown = true;
                ++iterations;
                break;
            }
            for (std::size_t i = 0; i < n; ++i) {
                p[i] = r[i] + beta * (p[i] - omega * v[i]);
                if (!finite_wide(p[i])) bicgstab_breakdown = true;
            }
            if (bicgstab_breakdown) {
                ++iterations;
                break;
            }
            multiply(p, v);
            const WideFloat denominator = wide_dot(r0, v);
            if (denominator.value() == 0.0 ||
                !finite_wide(denominator)) {
                bicgstab_breakdown = true;
                ++iterations;
                break;
            }
            alpha = rho / denominator;
            if (!finite_wide(alpha)) {
                bicgstab_breakdown = true;
                ++iterations;
                break;
            }
            for (std::size_t i = 0; i < n; ++i) {
                s[i] = r[i] - alpha * v[i];
                if (!finite_wide(s[i])) bicgstab_breakdown = true;
            }
            if (bicgstab_breakdown) {
                ++iterations;
                break;
            }
            if (wide_norm(s) <= tolerance) {
                for (std::size_t i = 0; i < n; ++i) {
                    x[i] += alpha * p[i];
                    if (!finite_wide(x[i])) bicgstab_breakdown = true;
                }
                bicgstab_candidate_converged = true;
                ++iterations;
                break;
            }
            multiply(s, t);
            const WideFloat tt = wide_dot(t, t);
            if (tt.value() == 0.0 || !finite_wide(tt)) {
                bicgstab_breakdown = true;
                ++iterations;
                break;
            }
            omega = wide_dot(t, s) / tt;
            if (!finite_wide(omega) || omega.value() == 0.0) {
                bicgstab_breakdown = true;
                ++iterations;
                break;
            }
            for (std::size_t i = 0; i < n; ++i) {
                x[i] += alpha * p[i] + omega * s[i];
                r[i] = s[i] - omega * t[i];
                if (!finite_wide(x[i]) || !finite_wide(r[i])) {
                    bicgstab_breakdown = true;
                }
            }
            rho_previous = rho;
            if (bicgstab_breakdown || wide_norm(r) <= tolerance) {
                ++iterations;
                break;
            }
        }

        if (bicgstab_breakdown) {
            /* Gauss-Seidel converges from any finite seed for the proper
             * substochastic SCC systems admitted by policy properness. Keep
             * every still-finite accelerated coordinate and deterministically
             * replace only contaminated coordinates. */
            for (WideFloat& value : x) {
                if (!finite_wide(value)) value = 0.0;
            }
            last_true_residual = true_residual(r);
            converged = last_true_residual <= tolerance;
            mode = SparsePolicySolveMode::GaussSeidel;
            true_residual_stagnation = 0;
        } else {
            const double checked_residual = true_residual(s);
            converged = checked_residual <= tolerance;
            if (!converged) {
                constexpr double kMinimumRelativeProgress = 1e-6;
                const bool progressed =
                    std::isfinite(checked_residual) &&
                    checked_residual <
                        last_true_residual *
                            (1.0 - kMinimumRelativeProgress);
                true_residual_stagnation = progressed
                    ? 0
                    : static_cast<std::uint8_t>(std::min<unsigned int>(
                          std::numeric_limits<std::uint8_t>::max(),
                          static_cast<unsigned int>(
                              true_residual_stagnation) +
                              1));
                last_true_residual = checked_residual;
                if (!std::isfinite(checked_residual) ||
                    true_residual_stagnation >= 2) {
                    for (WideFloat& value : x) {
                        if (!finite_wide(value)) value = 0.0;
                    }
                    last_true_residual = true_residual(r);
                    mode = SparsePolicySolveMode::GaussSeidel;
                    true_residual_stagnation = 0;
                } else if (bicgstab_candidate_converged) {
                    /* The recursive residual was optimistic. Restart from the
                     * freshly measured residual instead of resuming a stale
                     * Krylov recurrence. */
                    r = s;
                    reset_bicgstab();
                }
            } else {
                last_true_residual = checked_residual;
            }
        }

        /* A proper substochastic component can keep making tiny Krylov
         * progress without ever tripping the stagnation detector. Bound that
         * acceleration phase by the component dimension, then continue with
         * the deterministic Gauss-Seidel authority under the same public
         * iteration budget. This is a mode switch, not an additional cap. */
        const std::size_t bicgstab_iteration_limit =
            std::min<std::size_t>(
                max_iterations,
                std::max<std::size_t>(32, 2 * n));
        if (!converged &&
            mode == SparsePolicySolveMode::BiCGSTAB &&
            resumed_iterations + iterations >=
                bicgstab_iteration_limit) {
            last_true_residual = true_residual(r);
            converged = last_true_residual <= tolerance;
            mode = SparsePolicySolveMode::GaussSeidel;
            true_residual_stagnation = 0;
        }
    }

    if (!converged &&
        mode == SparsePolicySolveMode::GaussSeidel) {
        for (; iterations < work_unit_iterations; ++iterations) {
            bool finite_sweep = true;
            for (std::size_t i = 0; i < n; ++i) {
                WideFloat numerator = b[i];
                WideFloat diagonal = 1.0;
                const PolicyRow& row =
                    view.rows.at(view.members[i]);
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge =
                        view.edges.at(row.edge_offset + e);
                    if (view.component_by_state.at(edge.target) !=
                        view.component) {
                        continue;
                    }
                    const std::size_t successor =
                        static_cast<std::size_t>(
                            view.local_by_state.at(edge.target));
                    if (successor == i) {
                        diagonal -= WideFloat{edge.probability};
                    } else {
                        numerator += edge.probability * x[successor];
                    }
                }
                if (!(diagonal.value() > 0.0) ||
                    !finite_wide(diagonal) ||
                    !finite_wide(numerator)) {
                    finite_sweep = false;
                    break;
                }
                const WideFloat updated = numerator / diagonal;
                if (!finite_wide(updated)) {
                    finite_sweep = false;
                    break;
                }
                x[i] = updated;
            }
            if (!finite_sweep) {
                for (WideFloat& value : x) {
                    if (!finite_wide(value)) value = 0.0;
                }
            }
        }
        last_true_residual = true_residual(r);
        converged = last_true_residual <= tolerance;
    }

    result.iterations =
        static_cast<std::uint32_t>(iterations);
    result.total_iterations =
        resumed_iterations + result.iterations;
    if (!converged && result.total_iterations < max_iterations) {
        resume = std::make_unique<SparsePolicyResume>();
        resume->members = view.members;
        resume->b = std::move(b);
        resume->x = std::move(x);
        resume->r = std::move(r);
        resume->r0 = std::move(r0);
        resume->p = std::move(p);
        resume->v = std::move(v);
        resume->s = std::move(s);
        resume->t = std::move(t);
        resume->rho_previous = rho_previous;
        resume->alpha = alpha;
        resume->omega = omega;
        resume->iterations = result.total_iterations;
        resume->mode = mode;
        resume->last_true_residual = last_true_residual;
        resume->true_residual_stagnation =
            true_residual_stagnation;
        result.status =
            SparsePolicyComponentStatus::Incomplete;
        result.values.clear();
        return result;
    }
    resume.reset();
    if (!converged) {
        result.status =
            SparsePolicyComponentStatus::DidNotConverge;
        result.values.clear();
        return result;
    }
    for (std::size_t i = 0; i < n; ++i) {
        result.values[i] = x[i].value();
    }
    if (exact_values != nullptr) {
        *exact_values = std::move(x);
    }
    return result;
}

} // namespace solve_detail
} // namespace solver
} // namespace poecraft
