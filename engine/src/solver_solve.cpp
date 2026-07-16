#include "solver_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/*
 * Solver S4: value iteration over the reachable abstract state set
 * (docs/crafting-solver-plan.md, DP Solver).
 *
 *   V(goal) = 0
 *   V(s)    = min over legal actions a of cost(a) + sum P(s'|s,a) V(s')
 *
 * Restarting is an ordinary action whose successor is the clean base, so
 * it upper-bounds every value and salvage-versus-restart falls out of the
 * minimization. Reforge-class cycles make this value iteration rather
 * than settling; iteration starts from +infinity and converges because
 * the restart bound pulls every state that can reach the goal to a finite
 * value.
 */
namespace poecraft {
namespace solver {

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();
/* Finite upper-bound initialization (the restart bound makes every
 * goal-connected value finite; genuinely unreachable states stay here).
 * Value iteration descends monotonically from above, so a plain +infinity
 * start would never lift off: a backup is only finite once every
 * successor is, and no such state exists initially. */
constexpr double kValueCeiling = 1e12;

struct PricedOperator {
    std::uint32_t index = kNoId;
    double cost = 0.0;
    std::vector<std::pair<std::string, double>> resource_prices;
};

struct SparseChoiceGroup {
    std::uint64_t successor_offset = 0;
    std::uint32_t successor_count = 0;
    double probability = 0.0;
    bool has_self = false;
};

struct SparseVariant {
    std::uint32_t operator_index = kNoId;
    std::uint64_t quantity_offset = 0;
    std::uint32_t quantity_count = 0;
    std::uint64_t choice_option_offset = 0;
    std::uint32_t choice_option_count = 0;
};

struct SparseRow {
    std::uint64_t variant_offset = 0;
    std::uint32_t variant_count = 0;
    std::uint64_t transition_offset = 0;
    std::uint32_t transition_count = 0;
    double self_probability = 0.0;
    std::uint64_t choice_offset = 0;
    std::uint32_t choice_count = 0;
};

struct StateRowSpan {
    std::uint64_t offset = 0;
    std::uint32_t count = 0;
};

struct PendingSparseRow {
    std::uint32_t state = kNoId;
    std::uint32_t operator_index = kNoId;
    const std::vector<std::pair<std::string, double>>* resources = nullptr;
    const std::vector<OutcomeEntry>* transitions = nullptr;
    const std::vector<OutcomeChoiceGroup>* choices = nullptr;
    const std::vector<OutcomeChoiceOption>* choice_options = nullptr;
};

struct PricedSparseRow {
    std::uint32_t operator_index = kNoId;
    double cost = kInfinity;
    std::uint64_t choice_option_offset = 0;
    std::uint32_t choice_option_count = 0;
};

} // namespace

/* A completed reachable closure is independent of the economy. Equivalent
 * kernels retain all operator/resource variants, so a later solve may change
 * relative prices without rebuilding transitions or reusing a stale action
 * representative. */
struct SolveTransitionCache {
    std::uint32_t start_state = kNoId;
    std::vector<std::uint32_t> operator_indices;
    std::uint32_t max_discovered_states = 0;
    std::uint32_t max_expanded_states = 0;
    std::uint64_t max_state_action_rows = 0;
    std::uint64_t max_transitions = 0;
    std::uint64_t max_reforge_work = 0;
    std::uint32_t discovered_states = 0;
    std::uint32_t expanded_states = 0;
    std::vector<std::uint8_t> expanded;
    std::vector<StateRowSpan> state_rows;
    std::vector<SparseRow> rows;
    std::vector<SparseVariant> variants;
    std::vector<std::uint32_t> row_variant_indices;
    std::vector<double> variant_quantities;
    std::vector<std::uint32_t> successors;
    std::vector<double> probabilities;
    std::vector<SparseChoiceGroup> choices;
    std::vector<std::uint32_t> choice_successors;
    std::vector<OutcomeChoiceOption> choice_options;
    std::uint64_t algebraic_self_loops = 0;
    bool focused_partial = false;

    bool compatible(
        const std::uint32_t requested_start,
        const std::vector<PricedOperator>& priced,
        const SolveOptions& options) const {
        if (start_state != requested_start ||
            max_discovered_states != options.max_discovered_states ||
            max_expanded_states != options.max_expanded_states ||
            max_state_action_rows != options.max_state_action_rows ||
            max_transitions != options.max_transitions ||
            max_reforge_work != options.max_reforge_work ||
            operator_indices.size() != priced.size()) {
            return false;
        }
        for (std::size_t i = 0; i < priced.size(); ++i) {
            if (operator_indices[i] != priced[i].index) return false;
        }
        return true;
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        bytes += operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += state_rows.capacity() * sizeof(StateRowSpan);
        bytes += rows.capacity() * sizeof(SparseRow);
        bytes += variants.capacity() * sizeof(SparseVariant);
        bytes += row_variant_indices.capacity() * sizeof(std::uint32_t);
        bytes += variant_quantities.capacity() * sizeof(double);
        bytes += successors.capacity() * sizeof(std::uint32_t);
        bytes += probabilities.capacity() * sizeof(double);
        bytes += choices.capacity() * sizeof(SparseChoiceGroup);
        bytes += choice_successors.capacity() * sizeof(std::uint32_t);
        bytes += choice_options.capacity() * sizeof(OutcomeChoiceOption);
        return bytes;
    }
};

struct SolveWork::Impl {
    CalcContext& calc;
    const SessionImpl& session;
    SolveOptions options;
    SolveResult result;
    std::vector<PricedOperator> operators;
    std::vector<bool> reported_unsupported;
    std::vector<std::uint8_t> expanded;
    std::vector<std::uint8_t> queued;
    std::deque<std::uint32_t> queue;
    std::uint32_t expanded_count = 0;
    std::uint32_t peak_queue_size = 0;
    std::uint32_t sweeps = 0;
    double residual = kValueCeiling;
    std::shared_ptr<SolveTransitionCache> transition_cache;
    std::vector<PricedSparseRow> priced_rows;
    std::vector<std::int32_t> priced_operator_position;
    std::unordered_map<std::size_t, std::vector<std::uint64_t>>
        kernel_rows_by_hash;
    enum class BackupStage : std::uint8_t { Measure, Apply };
    BackupStage backup_stage = BackupStage::Measure;
    std::uint32_t backup_cursor = 0;
    double measured_residual = kValueCeiling;
    std::vector<std::pair<double, std::uint32_t>> prioritized_states;
    bool backup_active = false;
    bool cache_pending = false;
    std::vector<std::uint64_t> policy_rows;
    bool policy_initialized = false;
    bool policy_iteration_failed = false;
    std::uint64_t peak_policy_scratch_bytes = 0;
    std::vector<std::uint32_t> improper_policy_states;
    bool focused_mode = false;
    bool focus_optimizing = false;
    bool focused_lower_mode = false;
    bool focused_closure_proved = false;
    std::uint32_t next_focus_checkpoint = 32;
    std::uint64_t peak_owned_bytes = 0;
    SolvePhase phase = SolvePhase::Expanding;
    bool consumed = false;

    Impl(
        CalcContext& context,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& solve_options)
        : calc(context), session(context.session()), options(solve_options),
          reported_unsupported(context.operators().size(), false) {
        const auto setup_started = std::chrono::steady_clock::now();
        options.max_expanded_states = std::min(
            options.max_expanded_states, options.max_states);
        calc.reset_solve_telemetry();
        calc.set_solve_resource_caps(
            options.max_discovered_states, options.max_reforge_work);
        result.options = options;
        result.diagnostics.registry_actions = static_cast<std::uint32_t>(
            calc.registry().actions.size());
        result.diagnostics.candidate_actions = static_cast<std::uint32_t>(
            calc.candidates().size());
        const ActionControlSummary& control = calc.action_control();
        result.diagnostics.relevance_reduced_actions =
            control.pruned_outside_goal_relevance +
            control.pruned_outside_envelope;
        result.diagnostics.dependency_actions =
            control.dependency_primitives;
        result.diagnostics.deferred_actions =
            control.deferred_fossil_loadouts;
        result.diagnostics.action_inclusion_reasons.push_back(
            std::string(
                control.explicit_envelope
                    ? "included:explicit_goal_envelope:"
                    : calc.registry().fossil_generation_goal_relevant
                          ? "included:bounded_goal_relevant_envelope:"
                          : "included:conservative_exhaustive_envelope:") +
            std::to_string(control.included_primitives));
        if (control.pruned_outside_envelope != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                "pruned:not_permitted_by_explicit_goal_envelope:" +
                std::to_string(control.pruned_outside_envelope));
        }
        if (control.pruned_outside_goal_relevance != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                "pruned:outside_product_goal_relevance:" +
                std::to_string(
                    control.pruned_outside_goal_relevance));
        }
        if (control.dependency_primitives != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                "included:fixed_option_structural_dependency:" +
                std::to_string(control.dependency_primitives));
        }
        if (control.deferred_fossil_loadouts != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                std::string(
                    calc.registry().fossil_generation_goal_relevant
                        ? "deferred:outside_bounded_goal_relevant_fossil_beam:"
                        : "deferred:lazy_fossil_signature_not_requested:") +
                std::to_string(control.deferred_fossil_loadouts));
        }
        /* Primitive support remains action-registry telemetry. Fixed options
         * are priced and evaluated alongside those primitive wrappers. */
        for (const std::uint32_t index : calc.candidates()) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(index);
            const bool supported = calc_supports(descriptor);
            if (supported) {
                ++result.diagnostics.evaluator_supported_actions;
            }
        }
        for (const std::uint32_t index : calc.candidate_operators()) {
            const PlannerOperator& planner = calc.operators().at(index);
            double cost = 0.0;
            bool priced = true;
            std::vector<std::pair<std::string, double>> resource_prices;
            for (const auto& [key, quantity] :
                 planner.resource_quantities) {
                const auto found = prices.find(key);
                if (found == prices.end()) {
                    priced = false;
                    break;
                }
                cost += quantity * found->second;
                resource_prices.push_back({key, found->second});
            }
            if (!priced) {
                result.diagnostics.skipped_missing_price.push_back(
                    planner.id);
                add_action_reason(
                    "unpriced", planner.id,
                    "missing_one_or_more_resource_prices");
                continue;
            }
            ++result.diagnostics.priced_scanned_actions;
            const bool supported =
                planner.kind == PlannerOperatorKind::FixedOption ||
                calc_supports(calc.registry().actions.at(
                    planner.primitive_action));
            if (!supported) {
                reported_unsupported[index] = true;
                result.diagnostics.skipped_unsupported.push_back(planner.id);
                add_action_reason(
                    "unsupported", planner.id,
                    "no_exact_evaluator_for_requested_primitive");
                continue;
            }
            operators.push_back(
                {index, cost, std::move(resource_prices)});
            ++result.diagnostics.supported_priced_actions;
        }

        result.start_state = calc.intern_item(start_item);
        priced_operator_position.assign(calc.operators().size(), -1);
        for (std::uint32_t i = 0; i < operators.size(); ++i) {
            priced_operator_position[operators[i].index] =
                static_cast<std::int32_t>(i);
        }
        const auto& cached = calc.solve_transition_cache();
        if (cached != nullptr &&
            cached->compatible(result.start_state, operators, options)) {
            transition_cache = cached;
            expanded_count = transition_cache->expanded_states;
            expanded = transition_cache->expanded;
            focused_mode = transition_cache->focused_partial;
            cache_pending = !focused_mode;
            result.diagnostics.transition_cache_reused = true;
        } else {
            transition_cache = std::make_shared<SolveTransitionCache>();
            transition_cache->start_state = result.start_state;
            transition_cache->max_discovered_states =
                options.max_discovered_states;
            transition_cache->max_expanded_states =
                options.max_expanded_states;
            transition_cache->max_state_action_rows =
                options.max_state_action_rows;
            transition_cache->max_transitions = options.max_transitions;
            transition_cache->max_reforge_work = options.max_reforge_work;
            for (const PricedOperator& priced : operators) {
                transition_cache->operator_indices.push_back(priced.index);
            }
            enqueue(result.start_state);
        }
        result.diagnostics.solve_setup_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - setup_started)
                .count());
    }

    void enqueue(const std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_back(state);
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    }

    bool same_kernel(
        const SparseRow& stored,
        const PendingSparseRow& pending) const {
        const auto transition_count = [&]() {
            std::size_t count = 0;
            if (pending.transitions != nullptr) {
                for (const OutcomeEntry& entry : *pending.transitions) {
                    if (entry.state != pending.state) ++count;
                }
            }
            return count;
        }();
        const std::size_t choice_count =
            pending.choices == nullptr ? 0 : pending.choices->size();
        if (stored.transition_count != transition_count ||
            stored.choice_count != choice_count) {
            return false;
        }
        double self_probability = 0.0;
        std::size_t stored_transition = 0;
        if (pending.transitions != nullptr) {
            for (const OutcomeEntry& right : *pending.transitions) {
                if (right.state == pending.state) {
                    self_probability += right.probability;
                    continue;
                }
                const std::uint64_t offset =
                    stored.transition_offset + stored_transition++;
                if (transition_cache->successors.at(offset) != right.state ||
                    transition_cache->probabilities.at(offset) !=
                        right.probability) {
                    return false;
                }
            }
        }
        if (stored.self_probability != self_probability) {
            return false;
        }
        for (std::size_t i = 0; i < choice_count; ++i) {
            const SparseChoiceGroup& left = transition_cache->choices.at(
                stored.choice_offset + i);
            const OutcomeChoiceGroup& right = pending.choices->at(i);
            const bool has_self = std::find(
                right.states.begin(), right.states.end(), pending.state) !=
                right.states.end();
            const std::size_t successor_count =
                right.states.size() - (has_self ? 1u : 0u);
            if (left.probability != right.probability ||
                left.has_self != has_self ||
                left.successor_count != successor_count) {
                return false;
            }
            std::size_t stored_successor = 0;
            for (const std::uint32_t successor : right.states) {
                if (successor == pending.state) continue;
                if (transition_cache->choice_successors.at(
                        left.successor_offset + stored_successor++) !=
                    successor) {
                    return false;
                }
            }
        }
        return true;
    }

    std::size_t kernel_hash(const PendingSparseRow& pending) const {
        const auto mix = [](std::size_t& hash, std::size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) +
                    (hash >> 2);
        };
        std::size_t hash = 2166136261u;
        if (pending.transitions != nullptr) {
            std::size_t non_self = 0;
            double self_probability = 0.0;
            for (const OutcomeEntry& entry : *pending.transitions) {
                if (entry.state == pending.state) {
                    self_probability += entry.probability;
                    continue;
                }
                ++non_self;
                mix(hash, entry.state);
                mix(hash, std::hash<double>{}(entry.probability));
            }
            mix(hash, non_self);
            mix(hash, std::hash<double>{}(self_probability));
        }
        if (pending.choices != nullptr) {
            mix(hash, pending.choices->size());
            for (const OutcomeChoiceGroup& group : *pending.choices) {
                mix(hash, std::hash<double>{}(group.probability));
                const bool has_self = std::find(
                    group.states.begin(), group.states.end(),
                    pending.state) != group.states.end();
                mix(hash, has_self ? 1u : 0u);
                mix(hash, group.states.size() - (has_self ? 1u : 0u));
                for (const std::uint32_t state : group.states) {
                    if (state == pending.state) continue;
                    mix(hash, state);
                }
            }
        }
        return hash;
    }

    void add_action_reason(
        const char* disposition,
        const std::string& action,
        const std::string& reason) {
        result.diagnostics.action_inclusion_reasons.push_back(
            std::string(disposition) + ":" + reason + ":" + action);
    }

    void record_cap(const std::string& name, bool state_cap = false) {
        if (std::find(result.diagnostics.cap_hits.begin(),
                      result.diagnostics.cap_hits.end(), name) ==
            result.diagnostics.cap_hits.end()) {
            result.diagnostics.cap_hits.push_back(name);
        }
        result.diagnostics.resource_cap_hit = true;
        if (state_cap) result.diagnostics.state_cap_hit = true;
    }

    void check_solver_byte_cap() {
        const std::uint64_t current = estimated_owned_bytes();
        peak_owned_bytes = std::max(peak_owned_bytes, current);
        if (current > options.max_solver_owned_bytes) {
            record_cap("max_solver_owned_bytes");
        }
    }

    void append_sparse_row(
        const std::uint32_t state,
        PendingSparseRow pending) {
        static const std::vector<OutcomeEntry> empty_transitions;
        static const std::vector<OutcomeChoiceGroup> empty_choices;
        static const std::vector<OutcomeChoiceOption> empty_choice_options;
        const auto& transitions = pending.transitions == nullptr
                                      ? empty_transitions
                                      : *pending.transitions;
        const auto& choices = pending.choices == nullptr
                                  ? empty_choices
                                  : *pending.choices;
        const auto& choice_options = pending.choice_options == nullptr
                                         ? empty_choice_options
                                         : *pending.choice_options;
        pending.state = state;
        if (state >= transition_cache->state_rows.size()) {
            transition_cache->state_rows.resize(state + 1);
        }
        StateRowSpan& span = transition_cache->state_rows[state];
        SparseRow* equivalent = nullptr;
        for (std::uint32_t i = 0; i < span.count; ++i) {
            SparseRow& stored = transition_cache->rows.at(span.offset + i);
            if (!same_kernel(stored, pending)) continue;
            equivalent = &stored;
            break;
        }

        std::uint64_t transition_count = 0;
        double self_probability = 0.0;
        for (const OutcomeEntry& entry : transitions) {
            if (entry.state == state) {
                self_probability += entry.probability;
            } else {
                ++transition_count;
            }
        }
        for (const OutcomeChoiceGroup& group : choices) {
            for (const std::uint32_t successor : group.states) {
                if (successor != state) ++transition_count;
            }
        }
        if (equivalent == nullptr &&
            transition_cache->rows.size() >= options.max_state_action_rows) {
            throw SolverResourceLimit(
                "max_state_action_rows", options.max_state_action_rows);
        }

        SparseRow* stored_row = equivalent;
        if (stored_row == nullptr) {
            SparseRow row;
            row.variant_offset = transition_cache->row_variant_indices.size();
            row.self_probability = self_probability;
            const std::size_t hash = kernel_hash(pending);
            const SparseRow* shared_kernel = nullptr;
            const auto found = kernel_rows_by_hash.find(hash);
            if (found != kernel_rows_by_hash.end()) {
                for (const std::uint64_t row_index : found->second) {
                    const SparseRow& candidate =
                        transition_cache->rows.at(row_index);
                    if (same_kernel(candidate, pending)) {
                        shared_kernel = &candidate;
                        break;
                    }
                }
            }
            if (shared_kernel != nullptr) {
                row.transition_offset = shared_kernel->transition_offset;
                row.transition_count = shared_kernel->transition_count;
                row.choice_offset = shared_kernel->choice_offset;
                row.choice_count = shared_kernel->choice_count;
            } else {
                if (transition_cache->successors.size() +
                        transition_cache->choice_successors.size() +
                        transition_count > options.max_transitions) {
                    throw SolverResourceLimit(
                        "max_transitions", options.max_transitions);
                }
                row.transition_offset = transition_cache->successors.size();
                for (const OutcomeEntry& entry : transitions) {
                    if (entry.state == state) continue;
                    transition_cache->successors.push_back(entry.state);
                    transition_cache->probabilities.push_back(
                        entry.probability);
                }
                row.transition_count = static_cast<std::uint32_t>(
                    transition_cache->successors.size() -
                    row.transition_offset);
                row.choice_offset = transition_cache->choices.size();
                row.choice_count = static_cast<std::uint32_t>(choices.size());
                for (const OutcomeChoiceGroup& group : choices) {
                    SparseChoiceGroup stored;
                    stored.successor_offset =
                        transition_cache->choice_successors.size();
                    stored.probability = group.probability;
                    for (const std::uint32_t successor : group.states) {
                        if (successor == state) {
                            stored.has_self = true;
                        } else {
                            transition_cache->choice_successors.push_back(
                                successor);
                        }
                    }
                    stored.successor_count = static_cast<std::uint32_t>(
                        transition_cache->choice_successors.size() -
                        stored.successor_offset);
                    transition_cache->choices.push_back(stored);
                }
            }
            if (row.self_probability > 0.0) {
                ++transition_cache->algebraic_self_loops;
            }
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                if (transition_cache->choices[row.choice_offset + i]
                        .has_self) {
                    ++transition_cache->algebraic_self_loops;
                }
            }
            if (span.count == 0) span.offset = transition_cache->rows.size();
            transition_cache->rows.push_back(row);
            stored_row = &transition_cache->rows.back();
            if (shared_kernel == nullptr) {
                kernel_rows_by_hash[hash].push_back(
                    transition_cache->rows.size() - 1);
            }
            ++span.count;
        }

        SparseVariant variant;
        variant.operator_index = pending.operator_index;
        variant.quantity_offset = transition_cache->variant_quantities.size();
        const PricedOperator& priced = operators.at(
            static_cast<std::size_t>(
                priced_operator_position.at(pending.operator_index)));
        for (const auto& [key, unused_price] : priced.resource_prices) {
            (void)unused_price;
            double quantity = 0.0;
            if (pending.resources != nullptr) {
                const auto found = std::find_if(
                    pending.resources->begin(), pending.resources->end(),
                    [&](const auto& resource) {
                        return resource.first == key;
                    });
                if (found != pending.resources->end()) quantity = found->second;
            }
            transition_cache->variant_quantities.push_back(quantity);
        }
        variant.quantity_count = static_cast<std::uint32_t>(
            transition_cache->variant_quantities.size() -
            variant.quantity_offset);
        variant.choice_option_offset = transition_cache->choice_options.size();
        variant.choice_option_count = static_cast<std::uint32_t>(
            choice_options.size());
        transition_cache->choice_options.insert(
            transition_cache->choice_options.end(),
            choice_options.begin(), choice_options.end());
        transition_cache->variants.push_back(variant);
        const std::uint32_t variant_index = static_cast<std::uint32_t>(
            transition_cache->variants.size() - 1);
        if (stored_row->variant_count != 0 &&
            stored_row->variant_offset + stored_row->variant_count !=
                transition_cache->row_variant_indices.size()) {
            const std::uint64_t relocated =
                transition_cache->row_variant_indices.size();
            for (std::uint32_t i = 0; i < stored_row->variant_count; ++i) {
                transition_cache->row_variant_indices.push_back(
                    transition_cache->row_variant_indices.at(
                        stored_row->variant_offset + i));
            }
            stored_row->variant_offset = relocated;
        }
        transition_cache->row_variant_indices.push_back(variant_index);
        ++stored_row->variant_count;

        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();

        if (!focused_mode) {
            for (const OutcomeEntry& entry : transitions) {
                if (entry.state != state) enqueue(entry.state);
            }
            for (const OutcomeChoiceGroup& group : choices) {
                for (const std::uint32_t successor : group.states) {
                    if (successor != state) enqueue(successor);
                }
            }
        }
    }

    void expand_one() {
        const auto started = std::chrono::steady_clock::now();
        const std::uint32_t state = queue.front();
        queue.pop_front();
        if (state >= expanded.size()) expanded.resize(state + 1, 0);
        expanded[state] = 1;
        ++expanded_count;
        if (calc.is_goal_state(calc.state(state))) {
            result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            return;
        }

        try {
            for (const PricedOperator& priced : operators) {
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                PendingSparseRow pending;
                pending.state = state;
                pending.operator_index = priced.index;
                pending.resources = &planner.resource_quantities;
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    const OptionKernel& kernel =
                        calc.option_kernel(state, priced.index);
                    if (!kernel.supported) {
                        if (!reported_unsupported[priced.index]) {
                            reported_unsupported[priced.index] = true;
                            result.diagnostics.skipped_unsupported.push_back(
                                planner.id);
                            add_action_reason(
                                "unsupported", planner.id,
                                "fixed_option_kernel_unavailable");
                        }
                        continue;
                    }
                    if (!kernel.legal) continue;
                    pending.transitions = &kernel.exits;
                    pending.choices =
                        &kernel.observation_choice_groups;
                    pending.choice_options =
                        &kernel.observation_choice_options;
                    pending.resources = &kernel.expected_resources;
                } else {
                    const std::uint32_t action_index =
                        planner.primitive_action;
                    if (!action_legal(
                            session, calc.registry().actions[action_index],
                            calc.state(state))) {
                        continue;
                    }
                    const OutcomeDistribution& distribution =
                        calc.outcomes(state, action_index);
                    if (!distribution.supported) {
                        if (!reported_unsupported[priced.index]) {
                            reported_unsupported[priced.index] = true;
                            result.diagnostics.skipped_unsupported.push_back(
                                planner.id);
                            add_action_reason(
                                "unsupported", planner.id,
                                "exact_evaluator_unavailable");
                        }
                        continue;
                    }
                    if (distribution.choice_groups.empty()) {
                        pending.transitions = &distribution.entries;
                    } else {
                        pending.choices = &distribution.choice_groups;
                        pending.choice_options =
                            &distribution.choice_options;
                    }
                }
                try {
                    append_sparse_row(state, std::move(pending));
                } catch (...) {
                    if (planner.kind == PlannerOperatorKind::FixedOption) {
                        calc.release_option_kernel(state, priced.index);
                    } else {
                        calc.release_outcome(
                            state, planner.primitive_action);
                    }
                    throw;
                }
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    calc.release_option_kernel(state, priced.index);
                } else {
                    calc.release_outcome(state, planner.primitive_action);
                }
            }
        } catch (const SolverResourceLimit& limit) {
            record_cap(
                limit.cap_name(),
                limit.cap_name() == "max_discovered_states");
        }
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        if (!result.diagnostics.resource_cap_hit &&
            expanded_count % 64 == 0) {
            check_solver_byte_cap();
        }
    }

    void prepare_priced_rows() {
        priced_rows.assign(transition_cache->rows.size(), {});
        for (std::size_t row_index = 0;
             row_index < transition_cache->rows.size(); ++row_index) {
            const SparseRow& row = transition_cache->rows[row_index];
            PricedSparseRow& selected = priced_rows[row_index];
            std::vector<std::pair<double, std::uint32_t>> variants;
            variants.reserve(row.variant_count);
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant = transition_cache->variants.at(
                    transition_cache->row_variant_indices.at(
                        row.variant_offset + i));
                const std::int32_t priced_position =
                    priced_operator_position.at(variant.operator_index);
                if (priced_position < 0) continue;
                const PricedOperator& priced =
                    operators.at(static_cast<std::size_t>(priced_position));
                if (variant.quantity_count != priced.resource_prices.size()) {
                    throw std::logic_error(
                        "cached solver resource vector is incompatible");
                }
                double cost = 0.0;
                for (std::uint32_t quantity = 0;
                     quantity < variant.quantity_count; ++quantity) {
                    cost += transition_cache->variant_quantities.at(
                                variant.quantity_offset + quantity) *
                            priced.resource_prices[quantity].second;
                }
                variants.push_back({cost, variant.operator_index});
                if (cost < selected.cost - 1e-12 ||
                    (std::abs(cost - selected.cost) <= 1e-12 &&
                     variant.operator_index < selected.operator_index)) {
                    selected.operator_index = variant.operator_index;
                    selected.cost = cost;
                    selected.choice_option_offset =
                        variant.choice_option_offset;
                    selected.choice_option_count =
                        variant.choice_option_count;
                }
            }
            if (variants.size() <= 1) continue;
            result.diagnostics.equivalent_actions_collapsed +=
                static_cast<std::uint32_t>(variants.size() - 1);
            const std::string& representative = calc.operators().at(
                selected.operator_index).id;
            for (const auto& [cost, operator_index] : variants) {
                if (operator_index == selected.operator_index) continue;
                const std::string& candidate =
                    calc.operators().at(operator_index).id;
                if (std::abs(cost - selected.cost) <= 1e-12) {
                    ++result.diagnostics.equivalent_price_ties;
                    add_action_reason(
                        "included", candidate,
                        "certified_equivalent_kernel_price_tie_with_" +
                            representative);
                } else {
                    add_action_reason(
                        "pruned", candidate,
                        "certified_equivalent_kernel_more_expensive_than_" +
                            representative);
                }
            }
        }
    }

    void prepare_iteration() {
        const auto started = std::chrono::steady_clock::now();
        if (!cache_pending && !queue.empty() &&
            expanded_count >= options.max_expanded_states) {
            result.diagnostics.state_cap_hit = true;
            record_cap("max_expanded_states", true);
        }
        result.diagnostics.expanded_states = expanded_count;

        if (cache_pending) {
            expanded = transition_cache->expanded;
        } else {
            transition_cache->discovered_states = calc.state_count();
            transition_cache->expanded_states = expanded_count;
            transition_cache->expanded = expanded;
            transition_cache->expanded.resize(
                transition_cache->discovered_states, 0);
            transition_cache->state_rows.resize(
                transition_cache->discovered_states);
        }
        const std::uint32_t state_count = transition_cache->discovered_states;
        result.diagnostics.discovered_states = state_count;
        result.diagnostics.frontier_states = state_count - expanded_count;
        expanded.resize(state_count, 0);
        result.expanded = expanded;
        result.values.assign(state_count, kValueCeiling);
        result.policy.assign(state_count, PolicyOperatorRef{});
        result.unveil_preferences.assign(state_count, {});
        result.option_unveil_preferences.assign(state_count, {});
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                ++result.diagnostics.goal_states;
                result.values[state] = 0.0;
            } else if (!result.expanded[state]) {
                /* Past-the-cap frontier: no Bellman backing, keep infinite so
                 * plans through it are never preferred. */
                result.values[state] = kInfinity;
            }
        }
        residual = kValueCeiling;
        phase = SolvePhase::Iterating;
        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        result.diagnostics.algebraic_self_loops =
            transition_cache->algebraic_self_loops;
        result.diagnostics.reforge_frontier_work =
            calc.telemetry().reforge_frontier_work;
        prepare_priced_rows();
        check_solver_byte_cap();
        /* The solve now owns the compact CSR rows used by every later phase;
         * release evaluator distributions so transitions are stored once. A
         * reusable price-only context is the separate S7.5 cache-reuse pass. */
        calc.release_solve_transition_caches();
        if (!cache_pending && !result.diagnostics.resource_cap_hit &&
            queue.empty()) {
            transition_cache->focused_partial = focused_mode;
            calc.retain_solve_transition_cache(transition_cache);
        }
        kernel_rows_by_hash.clear();
        kernel_rows_by_hash.rehash(0);
        cache_pending = false;
        peak_owned_bytes = std::max(
            peak_owned_bytes, estimated_owned_bytes());
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    double operator_q(
        const std::uint32_t state,
        const PricedOperator& priced) {
        const PlannerOperator& planner = calc.operators().at(priced.index);
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            const OptionKernel& kernel =
                calc.option_kernel(state, priced.index);
            if (!kernel.supported || !kernel.legal) return kInfinity;
            double expected = 0.0;
            for (const auto& [key, quantity] : kernel.expected_resources) {
                const auto found = std::find_if(
                    priced.resource_prices.begin(),
                    priced.resource_prices.end(),
                    [&](const auto& price) { return price.first == key; });
                if (found == priced.resource_prices.end()) return kInfinity;
                expected += quantity * found->second;
            }
            for (const OutcomeEntry& exit : kernel.exits) {
                const double value = result.values[exit.state];
                if (value == kInfinity) return kInfinity;
                expected += exit.probability * value;
            }
            for (const OutcomeChoiceGroup& group :
                 kernel.observation_choice_groups) {
                double best = kInfinity;
                for (const std::uint32_t successor : group.states) {
                    best = std::min(best, result.values[successor]);
                }
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        const std::uint32_t action_index = planner.primitive_action;
        if (!action_legal(session, calc.registry().actions[action_index],
                          calc.state(state))) {
            return kInfinity;
        }
        const OutcomeDistribution& distribution =
            calc.outcomes(state, action_index);
        if (!distribution.supported) return kInfinity;
        double expected = priced.cost;
        if (!distribution.choice_groups.empty()) {
            for (const OutcomeChoiceGroup& group :
                 distribution.choice_groups) {
                double best = kInfinity;
                for (std::uint32_t successor : group.states) {
                    best = std::min(best, result.values[successor]);
                }
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        for (const OutcomeEntry& entry : distribution.entries) {
            const double value = result.values[entry.state];
            if (value == kInfinity) return kInfinity;
            expected += entry.probability * value;
        }
        return expected;
    }

    double sparse_row_q(
        const std::size_t row_index,
        std::uint32_t& transition_work) const {
        const SparseRow& row = transition_cache->rows.at(row_index);
        double constant = priced_rows.at(row_index).cost;
        transition_work = 0;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const double value = result.values[
                transition_cache->successors.at(offset)];
            ++transition_work;
            if (value == kInfinity) return kInfinity;
            constant += transition_cache->probabilities.at(offset) * value;
        }
        std::vector<std::pair<double, double>> self_choices;
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            double best = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                best = std::min(
                    best,
                    result.values[transition_cache->choice_successors.at(
                        group.successor_offset + s)]);
            }
            transition_work += group.successor_count;
            if (group.has_self) {
                self_choices.push_back({best, group.probability});
            } else {
                if (best == kInfinity) return kInfinity;
                constant += group.probability * best;
            }
        }
        /* Solve the row-local fixed point exactly:
         *
         * x = constant + p_self*x + sum g*min(x, alternate_g)
         *
         * Sorting the observed-choice thresholds identifies which offers
         * return to the outer policy and which take a concrete successor. */
        std::sort(
            self_choices.begin(), self_choices.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
        double loop_probability = row.self_probability;
        for (const auto& [unused, probability] : self_choices) {
            (void)unused;
            loop_probability += probability;
        }
        std::size_t fixed_choices = 0;
        while (true) {
            const double denominator = 1.0 - loop_probability;
            const double value = denominator > 1e-15
                                     ? constant / denominator
                                     : kInfinity;
            if (fixed_choices >= self_choices.size() ||
                value <= self_choices[fixed_choices].first + 1e-12) {
                return value;
            }
            const auto [alternate, probability] =
                self_choices[fixed_choices++];
            if (!std::isfinite(alternate)) return kInfinity;
            constant += probability * alternate;
            loop_probability -= probability;
        }
    }

    struct PolicyEdge {
        std::uint32_t target = kNoId;
        double probability = 0.0;
    };

    struct PolicyRow {
        std::uint64_t edge_offset = 0;
        std::uint32_t edge_count = 0;
        double cost = 0.0;
    };

    bool select_policy_rows() {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (policy_rows.size() != result.values.size()) {
            policy_rows.assign(result.values.size(), no_row);
        }
        bool improved = false;
        residual = 0.0;
        for (std::uint32_t state = 0; state < result.values.size(); ++state) {
            if (!result.expanded[state] || result.goal_states[state]) continue;
            const StateRowSpan& span = transition_cache->state_rows.at(state);
            double best = kInfinity;
            std::uint64_t best_row = no_row;
            for (std::uint32_t row = 0; row < span.count; ++row) {
                std::uint32_t work = 0;
                const std::uint64_t absolute = span.offset + row;
                const double q = sparse_row_q(absolute, work);
                ++result.diagnostics.bellman_action_evaluations;
                if (q < best - options.epsilon) {
                    best = q;
                    best_row = absolute;
                }
            }
            ++result.diagnostics.bellman_backups;
            if (std::isfinite(best)) {
                residual = std::max(
                    residual, std::abs(result.values[state] - best));
            }
            if (best_row != no_row && policy_rows[state] != best_row) {
                policy_rows[state] = best_row;
                improved = true;
            }
        }
        result.diagnostics.residual = residual;
        return improved;
    }

    bool evaluate_fixed_policy() {
        improper_policy_states.clear();
        result.diagnostics.policy_evaluation_failure.clear();
        const auto fail = [&](const char* reason) {
            result.diagnostics.policy_evaluation_failure = reason;
            return false;
        };
        const std::size_t state_count = result.values.size();
        std::vector<PolicyRow> rows(state_count);
        std::vector<PolicyEdge> edges;
        edges.reserve(transition_cache->successors.size());
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) continue;
            if (policy_rows[state] == no_row) continue;
            const std::uint64_t row_index = policy_rows[state];
            const SparseRow& sparse = transition_cache->rows.at(row_index);
            PolicyRow& row = rows[state];
            row.edge_offset = edges.size();
            row.cost = priced_rows.at(row_index).cost;
            double self_probability = sparse.self_probability;
            for (std::uint32_t i = 0; i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                edges.push_back({
                    transition_cache->successors.at(offset),
                    transition_cache->probabilities.at(offset)});
            }
            for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(sparse.choice_offset + i);
                std::uint32_t selected =
                    choice.has_self ? state : kNoId;
                double selected_value =
                    choice.has_self ? result.values[state] : kInfinity;
                for (std::uint32_t successor = 0;
                     successor < choice.successor_count; ++successor) {
                    const std::uint32_t candidate =
                        transition_cache->choice_successors.at(
                            choice.successor_offset + successor);
                    const double candidate_value = result.values[candidate];
                    if (candidate_value < selected_value - options.epsilon ||
                        (std::abs(candidate_value - selected_value) <=
                             options.epsilon &&
                         candidate < selected)) {
                        selected = candidate;
                        selected_value = candidate_value;
                    }
                }
                if (selected == kNoId) return fail("empty_observation_choice");
                if (selected == state) {
                    self_probability += choice.probability;
                } else {
                    edges.push_back({selected, choice.probability});
                }
            }
            const double denominator = 1.0 - self_probability;
            if (!(denominator > 1e-15)) {
                return fail("algebraic_self_loop_does_not_exit");
            }
            row.cost /= denominator;
            for (std::size_t edge = row.edge_offset;
                 edge < edges.size(); ++edge) {
                edges[edge].probability /= denominator;
            }
            row.edge_count = static_cast<std::uint32_t>(
                edges.size() - row.edge_offset);
        }

        struct Frame {
            std::uint32_t state = kNoId;
            std::uint32_t next_edge = 0;
        };
        std::vector<std::uint32_t> index(state_count, kNoId);
        std::vector<std::uint32_t> lowlink(state_count, kNoId);
        std::vector<std::uint8_t> on_stack(state_count, 0);
        std::vector<std::uint32_t> stack;
        std::vector<std::vector<std::uint32_t>> components;
        std::uint32_t next_index = 0;
        const auto push = [&](const std::uint32_t state,
                              std::vector<Frame>& dfs) {
            index[state] = lowlink[state] = next_index++;
            stack.push_back(state);
            on_stack[state] = 1;
            dfs.push_back({state, 0});
        };
        for (std::uint32_t root = 0; root < state_count; ++root) {
            if (!result.expanded[root] || result.goal_states[root] ||
                policy_rows[root] == no_row ||
                index[root] != kNoId) {
                continue;
            }
            std::vector<Frame> dfs;
            push(root, dfs);
            while (!dfs.empty()) {
                Frame& frame = dfs.back();
                const PolicyRow& row = rows[frame.state];
                if (frame.next_edge < row.edge_count) {
                    const std::uint32_t target = edges.at(
                        row.edge_offset + frame.next_edge++).target;
                    if (target >= state_count || !result.expanded[target] ||
                        result.goal_states[target] ||
                        policy_rows[target] == no_row) {
                        continue;
                    }
                    if (index[target] == kNoId) {
                        push(target, dfs);
                    } else if (on_stack[target]) {
                        lowlink[frame.state] = std::min(
                            lowlink[frame.state], index[target]);
                    }
                    continue;
                }
                const std::uint32_t completed = frame.state;
                dfs.pop_back();
                if (!dfs.empty()) {
                    lowlink[dfs.back().state] = std::min(
                        lowlink[dfs.back().state], lowlink[completed]);
                }
                if (lowlink[completed] == index[completed]) {
                    components.emplace_back();
                    while (true) {
                        const std::uint32_t member = stack.back();
                        stack.pop_back();
                        on_stack[member] = 0;
                        components.back().push_back(member);
                        if (member == completed) break;
                    }
                    std::sort(
                        components.back().begin(), components.back().end());
                }
            }
        }

        std::vector<std::uint32_t> component_by_state(state_count, kNoId);
        for (std::uint32_t component = 0;
             component < components.size(); ++component) {
            for (const std::uint32_t state : components[component]) {
                component_by_state[state] = component;
            }
        }
        std::vector<std::int32_t> local(state_count, -1);
        std::uint64_t policy_scratch =
            rows.capacity() * sizeof(PolicyRow) +
            edges.capacity() * sizeof(PolicyEdge) +
            index.capacity() * sizeof(std::uint32_t) +
            lowlink.capacity() * sizeof(std::uint32_t) +
            on_stack.capacity() * sizeof(std::uint8_t) +
            stack.capacity() * sizeof(std::uint32_t) +
            component_by_state.capacity() * sizeof(std::uint32_t) +
            local.capacity() * sizeof(std::int32_t) +
            components.capacity() * sizeof(std::vector<std::uint32_t>);
        for (const auto& component : components) {
            policy_scratch += component.capacity() * sizeof(std::uint32_t);
        }
        peak_policy_scratch_bytes = std::max(
            peak_policy_scratch_bytes, policy_scratch);
        for (std::uint32_t component = 0;
             component < components.size(); ++component) {
            const std::vector<std::uint32_t>& members = components[component];
            const std::size_t n = members.size();
            bool has_exit = false;
            for (const std::uint32_t state : members) {
                const PolicyRow& row = rows[state];
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge = edges.at(row.edge_offset + e);
                    if (result.goal_states[edge.target] ||
                        component_by_state[edge.target] != component) {
                        has_exit = true;
                        break;
                    }
                }
                if (has_exit) break;
            }
            if (!has_exit) {
                improper_policy_states = members;
                return fail("improper_closed_component");
            }
            for (std::size_t i = 0; i < n; ++i) local[members[i]] =
                static_cast<std::int32_t>(i);
            std::vector<long double> rhs(n, 0.0L);
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint32_t state = members[i];
                rhs[i] = rows[state].cost;
                const PolicyRow& row = rows[state];
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge = edges.at(row.edge_offset + e);
                    if (edge.target >= state_count) {
                        return fail("successor_outside_cached_closure");
                    }
                    if (result.goal_states[edge.target]) continue;
                    if (!result.expanded[edge.target]) {
                        if (focused_lower_mode) continue;
                        return fail("policy_reaches_unexpanded_frontier");
                    }
                    if (!std::isfinite(result.values[edge.target])) {
                        return fail("policy_reaches_unexpanded_frontier");
                    }
                    if (component_by_state[edge.target] != component) {
                        rhs[i] += static_cast<long double>(edge.probability) *
                                  result.values[edge.target];
                    }
                }
            }

            std::vector<double> solved(n, 0.0);
            if (n <= 1024) {
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + n * (n + 1) * sizeof(double));
                const std::size_t stride = n + 1;
                std::vector<double> matrix(n * stride, 0.0);
                const auto cell = [&](const std::size_t row,
                                      const std::size_t column) -> double& {
                    return matrix[row * stride + column];
                };
                for (std::size_t i = 0; i < n; ++i) {
                    cell(i, i) = 1.0;
                    cell(i, n) = static_cast<double>(rhs[i]);
                    const PolicyRow& row = rows[members[i]];
                    for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                        const PolicyEdge& edge = edges.at(row.edge_offset + e);
                        if (component_by_state[edge.target] == component) {
                            cell(i, static_cast<std::size_t>(
                                local[edge.target])) -= edge.probability;
                        }
                    }
                }
                for (std::size_t column = 0; column < n; ++column) {
                    std::size_t pivot = column;
                    for (std::size_t row = column + 1; row < n; ++row) {
                        if (std::fabs(cell(row, column)) >
                            std::fabs(cell(pivot, column))) {
                            pivot = row;
                        }
                    }
                    if (std::fabs(cell(pivot, column)) <= 1e-15) {
                        return fail("dense_policy_component_is_singular");
                    }
                    if (pivot != column) {
                        for (std::size_t k = column; k <= n; ++k) {
                            std::swap(cell(pivot, k), cell(column, k));
                        }
                    }
                    for (std::size_t row = column + 1; row < n; ++row) {
                        const double factor =
                            cell(row, column) / cell(column, column);
                        if (factor == 0.0) continue;
                        for (std::size_t k = column; k <= n; ++k) {
                            cell(row, k) -= factor * cell(column, k);
                        }
                    }
                }
                for (std::size_t back = n; back-- > 0;) {
                    double value = cell(back, n);
                    for (std::size_t column = back + 1; column < n; ++column) {
                        value -= cell(back, column) * solved[column];
                    }
                    solved[back] = value / cell(back, back);
                }
            } else {
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + n * 8 * sizeof(double));
                /* Large fixed-policy components use BiCGSTAB on the sparse
                 * M-matrix (I-P). It avoids quadratic storage while retaining
                 * an explicit residual check before values are accepted. */
                std::vector<double> b(n), x(n, 0.0), r(n), r0(n), p(n, 0.0),
                    v(n, 0.0), s(n), t(n);
                for (std::size_t i = 0; i < n; ++i) {
                    b[i] = static_cast<double>(rhs[i]);
                    r[i] = r0[i] = b[i];
                }
                const auto dot = [](const auto& left, const auto& right) {
                    long double value = 0.0L;
                    for (std::size_t i = 0; i < left.size(); ++i) {
                        value += static_cast<long double>(left[i]) * right[i];
                    }
                    return static_cast<double>(value);
                };
                const auto multiply = [&](const std::vector<double>& input,
                                          std::vector<double>& output) {
                    for (std::size_t i = 0; i < n; ++i) {
                        double value = input[i];
                        const PolicyRow& row = rows[members[i]];
                        for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                            const PolicyEdge& edge = edges.at(
                                row.edge_offset + e);
                            if (component_by_state[edge.target] == component) {
                                value -= edge.probability * input[
                                    static_cast<std::size_t>(
                                        local[edge.target])];
                            }
                        }
                        output[i] = value;
                    }
                };
                const double tolerance = options.epsilon * std::max(
                    1.0, std::sqrt(std::max(0.0, dot(b, b))));
                double rho_previous = 1.0;
                double alpha = 1.0;
                double omega = 1.0;
                bool converged = std::sqrt(std::max(0.0, dot(r, r))) <=
                                 tolerance;
                const std::size_t max_iterations = std::min<std::size_t>(
                    100000, std::max<std::size_t>(1000, n * 10));
                for (std::size_t iteration = 0;
                     !converged && iteration < max_iterations; ++iteration) {
                    const double rho = dot(r0, r);
                    if (std::abs(rho) <= 1e-30 ||
                        std::abs(omega) <= 1e-30) break;
                    const double beta =
                        (rho / rho_previous) * (alpha / omega);
                    for (std::size_t i = 0; i < n; ++i) {
                        p[i] = r[i] + beta * (p[i] - omega * v[i]);
                    }
                    multiply(p, v);
                    const double denominator = dot(r0, v);
                    if (std::abs(denominator) <= 1e-30) break;
                    alpha = rho / denominator;
                    for (std::size_t i = 0; i < n; ++i) {
                        s[i] = r[i] - alpha * v[i];
                    }
                    if (std::sqrt(std::max(0.0, dot(s, s))) <= tolerance) {
                        for (std::size_t i = 0; i < n; ++i) {
                            x[i] += alpha * p[i];
                        }
                        converged = true;
                        break;
                    }
                    multiply(s, t);
                    const double tt = dot(t, t);
                    if (std::abs(tt) <= 1e-30) break;
                    omega = dot(t, s) / tt;
                    for (std::size_t i = 0; i < n; ++i) {
                        x[i] += alpha * p[i] + omega * s[i];
                        r[i] = s[i] - omega * t[i];
                    }
                    converged =
                        std::sqrt(std::max(0.0, dot(r, r))) <= tolerance;
                    rho_previous = rho;
                }
                if (!converged) {
                    return fail("sparse_policy_component_did_not_converge");
                }
                solved = std::move(x);
            }
            for (std::size_t i = 0; i < n; ++i) {
                if (!std::isfinite(solved[i]) || solved[i] < -1e-8) {
                    return fail("fixed_policy_value_is_invalid");
                }
                result.values[members[i]] = std::max(0.0, solved[i]);
                local[members[i]] = -1;
            }
        }
        return true;
    }

    bool repair_improper_policy() {
        if (improper_policy_states.empty()) return false;
        std::vector<std::uint8_t> in_component(result.values.size(), 0);
        for (const std::uint32_t state : improper_policy_states) {
            in_component[state] = 1;
        }
        double best_q = kInfinity;
        std::uint32_t best_state = kNoId;
        std::uint64_t best_row = std::numeric_limits<std::uint64_t>::max();
        for (const std::uint32_t state : improper_policy_states) {
            const StateRowSpan& span = transition_cache->state_rows.at(state);
            for (std::uint32_t i = 0; i < span.count; ++i) {
                const std::uint64_t row_index = span.offset + i;
                if (policy_rows[state] == row_index) continue;
                const SparseRow& row = transition_cache->rows.at(row_index);
                bool exits = false;
                for (std::uint32_t transition = 0;
                     transition < row.transition_count; ++transition) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            row.transition_offset + transition);
                    if (result.goal_states[successor] ||
                        !in_component[successor]) {
                        exits = true;
                        break;
                    }
                }
                for (std::uint32_t choice_index = 0;
                     !exits && choice_index < row.choice_count;
                     ++choice_index) {
                    const SparseChoiceGroup& choice =
                        transition_cache->choices.at(
                            row.choice_offset + choice_index);
                    std::uint32_t selected =
                        choice.has_self ? state : kNoId;
                    double selected_value =
                        choice.has_self ? result.values[state] : kInfinity;
                    for (std::uint32_t successor_index = 0;
                         successor_index < choice.successor_count;
                         ++successor_index) {
                        const std::uint32_t successor =
                            transition_cache->choice_successors.at(
                                choice.successor_offset + successor_index);
                        const double value = result.values[successor];
                        if (value < selected_value - options.epsilon ||
                            (std::abs(value - selected_value) <=
                                 options.epsilon &&
                             successor < selected)) {
                            selected = successor;
                            selected_value = value;
                        }
                    }
                    exits = selected != kNoId &&
                            (result.goal_states[selected] ||
                             !in_component[selected]);
                }
                if (!exits) continue;
                std::uint32_t work = 0;
                const double q = sparse_row_q(row_index, work);
                if (q < best_q - options.epsilon) {
                    best_q = q;
                    best_state = state;
                    best_row = row_index;
                }
            }
        }
        if (best_state == kNoId) return false;
        policy_rows[best_state] = best_row;
        improper_policy_states.clear();
        return true;
    }

    bool run_policy_iteration_unit() {
        const auto started = std::chrono::steady_clock::now();
        if (!policy_initialized) {
            /* A few alternating algebraic Gauss-Seidel passes propagate the
             * proper restart/goal bound before Howard initialization. Starting
             * directly from the finite ceiling can otherwise choose a closed
             * cross-state cycle whose one-step Q is finite but whose fixed
             * policy is improper. This is a bounded seed, not the convergence
             * algorithm. */
            for (std::uint32_t pass = 0; pass < 4; ++pass) {
                for (std::uint32_t offset = 0;
                     offset < result.values.size(); ++offset) {
                    const std::uint32_t state =
                        pass % 2 == 0
                            ? static_cast<std::uint32_t>(
                                  result.values.size() - 1 - offset)
                            : offset;
                    if (!result.expanded[state] || result.goal_states[state]) {
                        continue;
                    }
                    std::uint32_t work = 0;
                    const double best = backup_state(state, work);
                    if (best < result.values[state]) {
                        result.values[state] = best;
                    }
                }
            }
            select_policy_rows();
            policy_initialized = true;
        }
        if (!evaluate_fixed_policy()) {
            if (repair_improper_policy()) {
                ++sweeps;
                result.diagnostics.sweeps = sweeps;
                result.diagnostics.policy_improvement_rounds = sweeps;
                result.diagnostics.optimization_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - started)
                            .count());
                backup_active = true;
                return true;
            }
            policy_iteration_failed = true;
            result.diagnostics.policy_iteration_fallback = true;
            backup_active = false;
            result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            return false;
        }
        const bool improved = select_policy_rows();
        ++sweeps;
        result.diagnostics.sweeps = sweeps;
        result.diagnostics.policy_improvement_rounds = sweeps;
        ++result.diagnostics.bellman_work_units;
        result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        if ((!improved && residual <= options.epsilon) ||
            sweeps >= options.max_sweeps) {
            backup_active = false;
        } else {
            backup_active = true;
        }
        return true;
    }

    void begin_focused_lower_solve() {
        focused_mode = true;
        focus_optimizing = true;
        focused_lower_mode = true;
        result.diagnostics.focused_expansion = true;
        const std::uint32_t state_count = calc.state_count();
        transition_cache->state_rows.resize(state_count);
        result.values.assign(state_count, 0.0);
        result.expanded = expanded;
        result.expanded.resize(state_count, 0);
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
            }
        }
        prepare_priced_rows();
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_iteration_failed = false;
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        result.diagnostics.policy_evaluation_failure.clear();
    }

    bool collect_focused_fringe(std::vector<std::uint32_t>& fringe) const {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (result.start_state >= result.values.size()) return false;
        std::vector<std::uint8_t> visited(result.values.size(), 0);
        std::vector<std::uint8_t> queued_fringe(result.values.size(), 0);
        std::deque<std::uint32_t> walk{result.start_state};
        const auto route = [&](const std::uint32_t successor) {
            if (result.goal_states[successor]) return;
            if (!result.expanded[successor]) {
                if (!queued_fringe[successor]) {
                    queued_fringe[successor] = 1;
                    fringe.push_back(successor);
                }
            } else if (!visited[successor]) {
                walk.push_back(successor);
            }
        };
        while (!walk.empty()) {
            const std::uint32_t state = walk.front();
            walk.pop_front();
            if (visited[state] || result.goal_states[state]) continue;
            visited[state] = 1;
            if (!result.expanded[state] || state >= policy_rows.size() ||
                policy_rows[state] == no_row) {
                return false;
            }
            const SparseRow& row = transition_cache->rows.at(
                policy_rows[state]);
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                route(transition_cache->successors.at(
                    row.transition_offset + i));
            }
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(row.choice_offset + i);
                std::uint32_t selected = choice.has_self ? state : kNoId;
                double selected_value =
                    choice.has_self ? result.values[state] : kInfinity;
                for (std::uint32_t s = 0; s < choice.successor_count; ++s) {
                    const std::uint32_t successor =
                        transition_cache->choice_successors.at(
                            choice.successor_offset + s);
                    const double value = result.values[successor];
                    if (value < selected_value - options.epsilon ||
                        (std::abs(value - selected_value) <=
                             options.epsilon &&
                         successor < selected)) {
                        selected = successor;
                        selected_value = value;
                    }
                }
                if (selected != state && selected != kNoId) route(selected);
            }
        }
        return true;
    }

    void finish_focused_lower_solve() {
        ++result.diagnostics.focused_expansion_rounds;
        result.diagnostics.focused_lower_bound =
            result.values.at(result.start_state);
        result.diagnostics.focused_expansion_ns +=
            result.diagnostics.optimization_ns;
        std::vector<std::uint32_t> fringe;
        const bool complete = collect_focused_fringe(fringe);
        focused_closure_proved = complete && fringe.empty();

        queue.clear();
        queued.assign(calc.state_count(), 0);
        for (std::uint32_t state = 0; state < expanded.size(); ++state) {
            if (expanded[state]) queued[state] = 1;
        }
        if (!focused_closure_proved) {
            if (!complete) {
                /* A lower policy gap should be rare; fall back to the
                 * previously discovered frontier without claiming a proof. */
                for (std::uint32_t state = 0; state < calc.state_count();
                     ++state) {
                    if (!queued[state]) fringe.push_back(state);
                }
            }
            for (const std::uint32_t state : fringe) enqueue(state);
        }
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));

        focus_optimizing = false;
        focused_lower_mode = false;
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_iteration_failed = false;
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        result.diagnostics.sweeps = 0;
        result.diagnostics.policy_improvement_rounds = 0;
        result.diagnostics.residual = kValueCeiling;
        result.diagnostics.bellman_backups = 0;
        result.diagnostics.bellman_action_evaluations = 0;
        result.diagnostics.bellman_work_units = 0;
        result.diagnostics.optimization_ns = 0;
        result.diagnostics.policy_evaluation_failure.clear();
    }

    void run_focused_lower_unit() {
        if (!policy_iteration_failed) {
            if (!run_policy_iteration_unit()) {
                /* Focused lower bounds cannot use the descending prioritized
                 * fallback. Preserve the bound and resume full expansion. */
                focus_optimizing = false;
                focused_lower_mode = false;
                focused_mode = false;
                policy_iteration_failed = false;
                policy_initialized = false;
                return;
            }
        }
        if (!backup_active && policy_initialized &&
            residual <= options.epsilon) {
            finish_focused_lower_solve();
        }
    }

    double backup_state(
        const std::uint32_t state,
        std::uint32_t& transition_work) {
        double best = kInfinity;
        transition_work = 0;
        const StateRowSpan& span = transition_cache->state_rows.at(state);
        ++result.diagnostics.bellman_backups;
        for (std::uint32_t row = 0; row < span.count; ++row) {
            std::uint32_t row_work = 0;
            best = std::min(
                best, sparse_row_q(span.offset + row, row_work));
            transition_work += row_work;
            ++result.diagnostics.bellman_action_evaluations;
        }
        return best;
    }

    void begin_priority_measurement() {
        backup_active = true;
        backup_stage = BackupStage::Measure;
        backup_cursor = 0;
        measured_residual = 0.0;
        prioritized_states.clear();
    }

    void run_bellman_unit() {
        const auto started = std::chrono::steady_clock::now();
        if (!backup_active) begin_priority_measurement();
        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        constexpr std::uint32_t kStatesPerUnit = 256;
        constexpr std::uint32_t kTransitionsPerUnit = 4096;
        std::uint32_t states_done = 0;
        std::uint32_t transitions_done = 0;
        const std::uint32_t work_count =
            backup_stage == BackupStage::Measure
                ? state_count
                : static_cast<std::uint32_t>(prioritized_states.size());
        while (backup_cursor < work_count &&
               states_done < kStatesPerUnit &&
               (transitions_done < kTransitionsPerUnit || states_done == 0)) {
            const std::uint32_t state =
                backup_stage == BackupStage::Measure
                    ? backup_cursor
                    : prioritized_states[backup_cursor].second;
            ++backup_cursor;
            if (!result.expanded[state] || result.goal_states[state]) {
                continue;
            }
            std::uint32_t state_work = 0;
            const double best = backup_state(state, state_work);
            transitions_done += state_work;
            ++states_done;
            const double before = result.values[state];
            const double improvement =
                best < before ? before - best : 0.0;
            if (backup_stage == BackupStage::Measure) {
                measured_residual = std::max(measured_residual, improvement);
                if (improvement > options.epsilon) {
                    prioritized_states.push_back({improvement, state});
                }
            } else if (best < before) {
                result.values[state] = best;
            }
        }
        if (backup_cursor >= work_count) {
            if (backup_stage == BackupStage::Measure) {
                residual = measured_residual;
                result.diagnostics.residual = residual;
                if (residual <= options.epsilon ||
                    sweeps >= options.max_sweeps) {
                    backup_active = false;
                } else {
                    std::sort(
                        prioritized_states.begin(),
                        prioritized_states.end(),
                        [](const auto& left, const auto& right) {
                            return left.first != right.first
                                       ? left.first > right.first
                                       : left.second < right.second;
                        });
                    backup_stage = BackupStage::Apply;
                    backup_cursor = 0;
                }
            } else {
                ++sweeps;
                result.diagnostics.sweeps = sweeps;
                begin_priority_measurement();
            }
        }
        ++result.diagnostics.bellman_work_units;
        result.diagnostics.max_bellman_unit_transitions = std::max(
            result.diagnostics.max_bellman_unit_transitions,
            transitions_done);
        result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining > 0 && phase != SolvePhase::Done) {
            if (phase == SolvePhase::Expanding) {
                if (focus_optimizing) {
                    run_focused_lower_unit();
                    --remaining;
                    continue;
                }
                if (queue.empty() && focused_mode &&
                    !focused_closure_proved &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    --remaining;
                    continue;
                }
                if (queue.empty() ||
                    expanded_count >= options.max_expanded_states ||
                    focused_closure_proved) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break; /* expose the phase boundary to callers */
                }
                expand_one();
                --remaining;
                if (!focused_mode && expanded_count >= next_focus_checkpoint &&
                    queue.size() > 1024 &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    continue;
                }
                if (result.diagnostics.resource_cap_hit ||
                    expanded_count >= options.max_expanded_states) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break;
                }
                if (queue.empty()) {
                    if (focused_mode && !focused_closure_proved) {
                        begin_focused_lower_solve();
                    } else {
                        prepare_iteration();
                        if (result.diagnostics.resource_cap_hit) {
                            phase = SolvePhase::Done;
                        }
                        break;
                    }
                }
                continue;
            }

            if (!backup_active && policy_initialized &&
                (residual <= options.epsilon ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
                break;
            }
            if (!policy_iteration_failed) {
                if (!run_policy_iteration_unit()) {
                    begin_priority_measurement();
                }
            } else {
                run_bellman_unit();
            }
            --remaining;
            if (!backup_active &&
                (residual <= options.epsilon ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
            }
        }
    }

    SolveProgress progress() const {
        SolveProgress value;
        value.phase = phase;
        value.done = phase == SolvePhase::Done;
        value.expanded_states = expanded_count;
        value.sweeps = sweeps;
        value.residual = residual;
        value.start_value_bound = kValueCeiling;
        if (!result.values.empty() &&
            result.start_state < result.values.size()) {
            value.start_value_bound = result.values[result.start_state];
        }
        return value;
    }

    SolveTelemetrySnapshot telemetry_snapshot(bool abandoned) const {
        SolveTelemetrySnapshot snapshot;
        snapshot.phase = phase;
        snapshot.abandoned = abandoned;
        snapshot.diagnostics = result.diagnostics;
        snapshot.diagnostics.expanded_states = expanded_count;
        snapshot.diagnostics.discovered_states = calc.state_count();
        snapshot.diagnostics.frontier_states =
            snapshot.diagnostics.discovered_states - expanded_count;
        snapshot.diagnostics.goal_states = 0;
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                ++snapshot.diagnostics.goal_states;
            }
        }
        snapshot.diagnostics.sweeps = sweeps;
        snapshot.diagnostics.residual = residual;
        snapshot.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, estimated_owned_bytes());
        snapshot.raw_start_bound = progress().start_value_bound;
        return snapshot;
    }

    SolveResult finish() {
        if (phase != SolvePhase::Done) {
            throw std::logic_error("solver work is not finished");
        }
        if (consumed) {
            throw std::logic_error("solver work was already finished");
        }
        const auto extraction_started = std::chrono::steady_clock::now();

        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        /* Deterministic argmin: cost ties break toward lower cost-to-go
         * variance, then lower action index by stable registry traversal. */
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) continue;
            double best_q = kInfinity;
            double best_variance = kInfinity;
            std::uint32_t best_operator = kNoId;
            std::uint64_t best_row_index = std::numeric_limits<std::uint64_t>::max();
            const StateRowSpan& span =
                transition_cache->state_rows.at(state);
            for (std::uint32_t row_index = 0; row_index < span.count;
                 ++row_index) {
                const std::uint64_t absolute_row = span.offset + row_index;
                const SparseRow& row = transition_cache->rows.at(absolute_row);
                ++result.diagnostics.extraction_action_evaluations;
                std::uint32_t transition_work = 0;
                const double q = sparse_row_q(absolute_row, transition_work);
                if (q == kInfinity) continue;
                double mean = 0.0;
                std::vector<std::pair<double, double>> random_values;
                if (row.self_probability > 0.0) {
                    random_values.push_back(
                        {row.self_probability, result.values[state]});
                    mean += row.self_probability * result.values[state];
                }
                for (std::uint32_t i = 0;
                     i < row.transition_count; ++i) {
                    const std::uint64_t offset =
                        row.transition_offset + i;
                    random_values.push_back(
                        {transition_cache->probabilities.at(offset),
                         result.values[
                             transition_cache->successors.at(offset)]});
                    mean += transition_cache->probabilities.at(offset) *
                            result.values[
                                transition_cache->successors.at(offset)];
                }
                for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(row.choice_offset + i);
                    double chosen = group.has_self
                                        ? result.values[state]
                                        : kInfinity;
                    for (std::uint32_t s = 0;
                         s < group.successor_count; ++s) {
                        chosen = std::min(
                            chosen,
                            result.values[transition_cache->choice_successors.at(
                                group.successor_offset + s)]);
                    }
                    random_values.push_back(
                        {group.probability, chosen});
                    mean += group.probability * chosen;
                }
                double variance = 0.0;
                for (const auto& [probability, value] : random_values) {
                    const double delta = value - mean;
                    variance += probability * delta * delta;
                }
                const bool better =
                    q < best_q - options.epsilon ||
                    (q < best_q + options.epsilon &&
                     variance < best_variance - options.epsilon);
                if (better) {
                    best_q = q;
                    best_variance = variance;
                    best_operator = priced_rows[absolute_row].operator_index;
                    best_row_index = absolute_row;
                }
            }
            result.policy[state] =
                best_operator == kNoId
                    ? PolicyOperatorRef{}
                    : PolicyOperatorRef{
                          calc.operators()[best_operator].kind,
                          best_operator};
            if (best_operator != kNoId &&
                best_row_index != std::numeric_limits<std::uint64_t>::max() &&
                calc.operators()[best_operator].kind ==
                    PlannerOperatorKind::Primitive &&
                calc.registry()
                        .actions[calc.operators()[best_operator]
                                     .primitive_action]
                        .params.type == ActionType::Unveil) {
                std::vector<OutcomeChoiceOption> choice_options;
                const PricedSparseRow& best_row = priced_rows.at(
                    best_row_index);
                for (std::uint32_t i = 0;
                     i < best_row.choice_option_count; ++i) {
                    choice_options.push_back(
                        transition_cache->choice_options.at(
                            best_row.choice_option_offset + i));
                }
                std::sort(
                    choice_options.begin(), choice_options.end(),
                    [&](const OutcomeChoiceOption& a,
                        const OutcomeChoiceOption& b) {
                        const double left = result.values[a.state];
                        const double right = result.values[b.state];
                        return left != right ? left < right
                                             : a.mod_id < b.mod_id;
                    });
                for (const OutcomeChoiceOption& option : choice_options) {
                    result.unveil_preferences[state].push_back(
                        option.mod_id);
                }
            } else if (best_operator != kNoId &&
                       best_row_index !=
                           std::numeric_limits<std::uint64_t>::max() &&
                       calc.operators()[best_operator].kind ==
                           PlannerOperatorKind::FixedOption &&
                       priced_rows[best_row_index].choice_option_count != 0) {
                std::map<std::uint32_t, std::vector<OutcomeChoiceOption>>
                    by_observation;
                const PricedSparseRow& best_row = priced_rows.at(
                    best_row_index);
                for (std::uint32_t i = 0;
                     i < best_row.choice_option_count; ++i) {
                    const OutcomeChoiceOption& choice =
                        transition_cache->choice_options.at(
                            best_row.choice_option_offset + i);
                    by_observation[choice.observation_state].push_back(
                        choice);
                }
                for (auto& [observation_state, choices] : by_observation) {
                    std::sort(
                        choices.begin(), choices.end(),
                        [&](const OutcomeChoiceOption& a,
                            const OutcomeChoiceOption& b) {
                            const double left = result.values[a.state];
                            const double right = result.values[b.state];
                            return left != right ? left < right
                                                 : a.mod_id < b.mod_id;
                        });
                    ObservedUnveilPreference preference;
                    preference.observation_state = observation_state;
                    for (const OutcomeChoiceOption& choice : choices) {
                        preference.choices.push_back(
                            {choice.mod_id, choice.state,
                             choice.actual_state});
                    }
                    result.option_unveil_preferences[state].push_back(
                        std::move(preference));
                }
            }
        }

        result.policy_reachable.assign(state_count, 0);
        if (result.start_state < state_count) {
            std::deque<std::uint32_t> walk{result.start_state};
            while (!walk.empty()) {
                const std::uint32_t state = walk.front();
                walk.pop_front();
                if (result.policy_reachable[state]) continue;
                result.policy_reachable[state] = 1;
                ++result.diagnostics.policy_reachable_states;
                const std::uint32_t operator_index = result.policy[state];
                if (operator_index == kNoId) continue;
                const StateRowSpan& span =
                    transition_cache->state_rows.at(state);
                const SparseRow* selected = nullptr;
                for (std::uint32_t i = 0; i < span.count; ++i) {
                    const SparseRow& row =
                        transition_cache->rows.at(span.offset + i);
                    if (priced_rows[span.offset + i].operator_index ==
                        operator_index) {
                        selected = &row;
                        break;
                    }
                }
                if (selected == nullptr) continue;
                for (std::uint32_t i = 0;
                     i < selected->transition_count; ++i) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            selected->transition_offset + i);
                    if (!result.policy_reachable[successor]) {
                        walk.push_back(successor);
                    }
                }
                for (std::uint32_t i = 0; i < selected->choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(
                            selected->choice_offset + i);
                    for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                        const std::uint32_t successor =
                            transition_cache->choice_successors.at(
                                group.successor_offset + s);
                        if (!result.policy_reachable[successor]) {
                            walk.push_back(successor);
                        }
                    }
                }
            }
        }

        if (result.diagnostics.focused_expansion &&
            result.start_state < result.values.size()) {
            result.diagnostics.focused_upper_bound =
                result.values[result.start_state];
            result.diagnostics.focused_optimality_gap =
                std::max(
                    0.0,
                    result.diagnostics.focused_upper_bound -
                        result.diagnostics.focused_lower_bound);
        }
        const bool focused_exact =
            !result.diagnostics.focused_expansion ||
            (focused_closure_proved &&
             result.diagnostics.focused_optimality_gap <=
                 options.epsilon * 10.0);
        result.converged = focused_exact &&
                           !result.diagnostics.state_cap_hit &&
                           !result.diagnostics.resource_cap_hit &&
                           residual <= options.epsilon &&
                           result.start_state < state_count &&
                           result.values[result.start_state] < kValueCeiling;
        result.diagnostics.extraction_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - extraction_started)
                .count());
        result.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, estimated_owned_bytes());
        consumed = true;
        return std::move(result);
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this) + calc.estimated_owned_bytes();
        bytes += operators.capacity() * sizeof(PricedOperator);
        for (const PricedOperator& priced : operators) {
            bytes += priced.resource_prices.capacity() *
                     sizeof(std::pair<std::string, double>);
            for (const auto& [key, price] : priced.resource_prices) {
                (void)price;
                bytes += key.capacity() + 1;
            }
        }
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->estimated_owned_bytes();
        }
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += peak_policy_scratch_bytes;
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<std::uint64_t>>) +
                  2 * sizeof(void*));
        for (const auto& [unused, rows] : kernel_rows_by_hash) {
            (void)unused;
            bytes += rows.capacity() * sizeof(std::uint64_t);
        }
        bytes += result.values.capacity() * sizeof(double);
        bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += result.expanded.capacity() * sizeof(std::uint8_t);
        bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
        bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
        bytes += result.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& preferences : result.unveil_preferences) {
            bytes += preferences.capacity() * sizeof(std::uint32_t);
        }
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        for (const auto& preferences :
             result.option_unveil_preferences) {
            bytes += preferences.capacity() *
                     sizeof(ObservedUnveilPreference);
            for (const ObservedUnveilPreference& preference : preferences) {
                bytes += preference.choices.capacity() *
                         sizeof(ObservedUnveilChoice);
            }
        }
        const auto string_vector_bytes = [](const auto& values) {
            std::uint64_t total =
                values.capacity() * sizeof(std::string);
            for (const std::string& value : values) {
                total += value.capacity() + 1;
            }
            return total;
        };
        bytes += string_vector_bytes(
            result.diagnostics.skipped_missing_price);
        bytes += string_vector_bytes(result.diagnostics.skipped_unsupported);
        bytes += string_vector_bytes(
            result.diagnostics.action_inclusion_reasons);
        bytes += string_vector_bytes(result.diagnostics.cap_hits);
        return bytes;
    }
};

SolveWork::SolveWork(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options)
    : impl_(std::make_unique<Impl>(calc, start_item, prices, options)) {}

SolveWork::~SolveWork() = default;
SolveWork::SolveWork(SolveWork&&) noexcept = default;
SolveWork& SolveWork::operator=(SolveWork&&) noexcept = default;

void SolveWork::step(const std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

SolveProgress SolveWork::progress() const {
    return impl_->progress();
}

SolveTelemetrySnapshot SolveWork::telemetry_snapshot(bool abandoned) const {
    return impl_->telemetry_snapshot(abandoned);
}

SolveResult SolveWork::finish() {
    return impl_->finish();
}

SolveResult solve(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options) {
    SolveWork work(calc, start_item, prices, options);
    while (!work.progress().done) work.step(4096);
    return work.finish();
}

std::string serialize_solve_log(
    const CalcContext& calc,
    const SolveResult& result) {
    std::string log;
    char buffer[256];
    for (std::uint32_t state = 0; state < result.values.size(); ++state) {
        if (!result.expanded[state]) continue;
        const AbstractState& features = calc.state(state);
        std::snprintf(buffer, sizeof(buffer),
                      "{\"state\":%u,\"value\":%.9g,\"action\":", state,
                      result.values[state]);
        log += buffer;
        const std::uint32_t action = result.policy[state];
        if (action == kNoId) {
            log += "null";
        } else {
            log += '"';
            log += calc.operators().at(action).id;
            log += '"';
        }
        std::snprintf(
            buffer, sizeof(buffer),
            ",\"goal\":%d,\"reachable\":%d,\"rarity\":%u,\"prefixes\":%u,"
            "\"suffixes\":%u,\"blocked\":%u,\"flags\":%u,"
            "\"veiled_side\":%d,\"searing_tier\":%u,\"eater_tier\":%u,"
            "\"slots\":[",
            result.goal_states[state] ? 1 : 0,
            result.policy_reachable[state] ? 1 : 0, features.rarity,
            features.prefix_count, features.suffix_count,
            features.blocked_mask, features.flags, features.veiled_side,
            features.searing_exarch_tier,
            features.eater_of_worlds_tier);
        log += buffer;
        for (std::size_t i = 0; i < calc.layout().slots.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.slot_status[i]);
        }
        log += "],\"junk\":[";
        for (std::size_t i = 0; i < features.junk_counts.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.junk_counts[i]);
        }
        log += "]}\n";
    }
    return log;
}

std::string serialize_solver_telemetry(
    const CalcContext& calc,
    const SolveResult* result,
    const SolveTelemetrySnapshot* snapshot,
    const std::optional<std::uint64_t>& registry_generation_ns,
    const PolicyCompilationTelemetry* compilation) {
    const CalcTelemetry& cache = calc.telemetry();
    const SolveDiagnostics* diagnostics =
        result != nullptr
            ? &result->diagnostics
            : (snapshot == nullptr ? nullptr : &snapshot->diagnostics);
    const bool qualified_action_subset =
        diagnostics != nullptr &&
        (!diagnostics->skipped_missing_price.empty() ||
         diagnostics->evaluator_supported_actions <
             diagnostics->candidate_actions ||
         !diagnostics->skipped_unsupported.empty());
    const auto count_or_null = [](const std::uint64_t* value) {
        return value == nullptr ? std::string("null")
                                : std::to_string(*value);
    };
    const auto optional_count = [](const auto& value) {
        return value.has_value() ? std::to_string(*value)
                                 : std::string("null");
    };
    const auto bool_json = [](bool value) {
        return value ? "true" : "false";
    };

    std::string json = "{\"version\":\"solver_telemetry_v1\"";
    json += ",\"availability\":{";
    json += "\"evaluator_support\":\"applied_before_expansion\"";
    json += ",\"relevance_filter\":\"explicit_envelope_or_conservative_include\"";
    json += ",\"dominance_filter\":\"certified_abstract_kernel_equivalence\"";
    json += ",\"policy_improvement_rounds\":\"available\"";
    json += ",\"optimality_gap\":\"available_for_focused_expansion\"";
    json += ",\"verification\":\"external_harness\"}";

    json += ",\"execution\":{";
    if (result != nullptr) {
        json += "\"status\":\"complete\",\"phase\":\"done\"";
    } else if (snapshot != nullptr) {
        const char* phase = snapshot->phase == SolvePhase::Expanding
                                ? "expanding"
                                : (snapshot->phase == SolvePhase::Iterating
                                       ? "iterating"
                                       : "ready_to_finish");
        json += "\"status\":\"";
        json += snapshot->abandoned ? "abandoned" : "in_progress";
        json += "\",\"phase\":\"" + std::string(phase) + "\"";
    } else {
        json += "\"status\":\"not_started\",\"phase\":null";
    }
    json += "}";

    const std::uint64_t registry_actions =
        diagnostics == nullptr
            ? calc.registry().actions.size()
            : diagnostics->registry_actions;
    const std::uint64_t candidate_actions =
        diagnostics == nullptr ? calc.candidates().size()
                               : diagnostics->candidate_actions;
    json += ",\"actions\":{\"registry\":" +
            std::to_string(registry_actions);
    json += ",\"registry_before_lazy\":" + std::to_string(
        registry_actions + calc.registry().fossil_loadouts_deferred);
    json += ",\"candidate\":" + std::to_string(candidate_actions);
    if (diagnostics == nullptr) {
        json += ",\"evaluator_supported\":null";
        json += ",\"priced_scanned\":null,\"supported_priced\":null";
        json += ",\"unsupported_requested\":null";
        json += ",\"relevance_reduced\":null,\"dominance_reduced\":null";
        json += ",\"deferred\":null,\"equivalent_price_ties\":null";
        json += ",\"missing_price\":null,\"unsupported_observed\":null";
    } else {
        json += ",\"evaluator_supported\":" +
                std::to_string(diagnostics->evaluator_supported_actions);
        json += ",\"priced_scanned\":" +
                std::to_string(diagnostics->priced_scanned_actions);
        json += ",\"supported_priced\":" +
                std::to_string(diagnostics->supported_priced_actions);
        json += ",\"unsupported_requested\":" +
                std::to_string(diagnostics->candidate_actions -
                               diagnostics->evaluator_supported_actions);
        json += ",\"relevance_reduced\":" + std::to_string(
                    diagnostics->relevance_reduced_actions);
        json += ",\"dominance_reduced\":" + std::to_string(
                    diagnostics->equivalent_actions_collapsed);
        json += ",\"deferred\":" + std::to_string(
                    diagnostics->deferred_actions);
        json += ",\"equivalent_price_ties\":" + std::to_string(
                    diagnostics->equivalent_price_ties);
        json += ",\"missing_price\":" + std::to_string(
                    diagnostics->skipped_missing_price.size());
        json += ",\"unsupported_observed\":" + std::to_string(
                    diagnostics->skipped_unsupported.size());
    }
    json += "}";

    json += ",\"action_control\":{";
    json += "\"explicit_envelope\":" + std::string(bool_json(
        calc.action_control().explicit_envelope));
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().dependency_primitives);
    json += ",\"goal_relevant_pruned\":" + std::to_string(
        calc.action_control().pruned_outside_goal_relevance);
    json += ",\"fossil_loadouts\":{";
    json += "\"possible\":" + std::to_string(
        calc.registry().fossil_loadouts_possible);
    json += ",\"generated\":" + std::to_string(
        calc.registry().fossil_loadouts_generated);
    json += ",\"deferred\":" + std::to_string(
        calc.registry().fossil_loadouts_deferred);
    json += ",\"lazy\":" + std::string(bool_json(
        calc.registry().fossil_generation_lazy));
    json += ",\"mode\":\"";
    json += calc.registry().fossil_generation_goal_relevant
                ? "goal_relevant"
                : calc.registry().fossil_generation_lazy ? "requested"
                                                         : "exhaustive";
    json += "\"}";
    json += ",\"reasons\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->action_inclusion_reasons.size(); ++i) {
            if (i != 0) json += ',';
            json += '"';
            for (const char c : diagnostics->action_inclusion_reasons[i]) {
                if (c == '"' || c == '\\') json += '\\';
                json += c;
            }
            json += '"';
        }
    }
    json += "]}";

    json += ",\"planner\":{\"registry\":" +
            std::to_string(calc.operators().size());
    json += ",\"candidate\":" +
            std::to_string(calc.candidate_operators().size());
    json += ",\"fixed_options\":" +
            std::to_string(calc.operators().size() -
                           calc.registry().actions.size()) +
            "}";

    json += ",\"abstraction\":{\"discriminating_tags\":" +
            std::to_string(calc.layout().discriminating_tag_ids.size());
    json += ",\"junk_classes\":" +
            std::to_string(calc.layout().junk_classes.size()) + "}";

    json += ",\"states\":{";
    if (diagnostics == nullptr) {
        json += "\"discovered\":null,\"expanded\":null,\"frontier\":null";
        json += ",\"goal\":null,\"policy_reachable\":null";
    } else {
        json += "\"discovered\":" +
                std::to_string(diagnostics->discovered_states);
        json += ",\"expanded\":" +
                std::to_string(diagnostics->expanded_states);
        json += ",\"frontier\":" +
                std::to_string(diagnostics->frontier_states);
        json += ",\"goal\":" +
                std::to_string(diagnostics->goal_states);
        if (result == nullptr) {
            json += ",\"policy_reachable\":null";
        } else {
            json += ",\"policy_reachable\":" +
                    std::to_string(diagnostics->policy_reachable_states);
        }
    }
    json += "}";

    json += ",\"focused_expansion\":{";
    if (diagnostics == nullptr) {
        json += "\"used\":null,\"rounds\":null,\"lower_bound\":null";
        json += ",\"upper_bound\":null,\"optimality_gap\":null";
        json += ",\"duration_ns\":null";
    } else {
        json += "\"used\":" + std::string(bool_json(
            diagnostics->focused_expansion));
        json += ",\"rounds\":" + std::to_string(
            diagnostics->focused_expansion_rounds);
        const auto append_bound = [&](const double value) {
            if (std::isfinite(value)) {
                char buffer[40];
                std::snprintf(buffer, sizeof(buffer), "%.17g", value);
                json += buffer;
            } else {
                json += "null";
            }
        };
        json += ",\"lower_bound\":";
        append_bound(diagnostics->focused_lower_bound);
        json += ",\"upper_bound\":";
        append_bound(diagnostics->focused_upper_bound);
        json += ",\"optimality_gap\":";
        append_bound(diagnostics->focused_optimality_gap);
        json += ",\"duration_ns\":" + std::to_string(
            diagnostics->focused_expansion_ns);
    }
    json += "}";

    json += ",\"work\":{\"state_action_rows\":" +
            std::to_string(diagnostics == nullptr
                               ? cache.state_action_rows
                               : diagnostics->sparse_rows);
    json += ",\"transition_entries\":" +
            std::to_string(diagnostics == nullptr
                               ? cache.transition_entries
                               : diagnostics->sparse_transitions);
    json += ",\"outcome_entries\":" +
            std::to_string(cache.outcome_entries);
    json += ",\"choice_groups\":" +
            std::to_string(cache.choice_groups);
    json += ",\"choice_successor_entries\":" +
            std::to_string(cache.choice_successor_entries);
    if (diagnostics == nullptr) {
        json += ",\"bellman_backups\":null";
        json += ",\"bellman_action_evaluations\":null";
        json += ",\"extraction_action_evaluations\":null";
        json += ",\"bellman_work_units\":null";
        json += ",\"max_bellman_unit_transitions\":null";
        json += ",\"algebraic_self_loops\":null";
    } else {
        json += ",\"bellman_backups\":" +
                std::to_string(diagnostics->bellman_backups);
        json += ",\"bellman_action_evaluations\":" +
                std::to_string(diagnostics->bellman_action_evaluations);
        json += ",\"extraction_action_evaluations\":" +
                std::to_string(diagnostics->extraction_action_evaluations);
        json += ",\"bellman_work_units\":" +
                std::to_string(diagnostics->bellman_work_units);
        json += ",\"max_bellman_unit_transitions\":" +
                std::to_string(
                    diagnostics->max_bellman_unit_transitions);
        json += ",\"algebraic_self_loops\":" +
                std::to_string(diagnostics->algebraic_self_loops);
    }
    json += "}";

    json += ",\"cache\":{\"distribution\":{\"requests\":" +
            std::to_string(cache.distribution_requests);
    json += ",\"hits\":" + std::to_string(cache.distribution_hits);
    json += ",\"misses\":" + std::to_string(cache.distribution_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_distribution_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.distribution_build_ns);
    json += ",\"released_after_sparse_copy\":" +
            std::string(bool_json(diagnostics != nullptr)) + "}";
    json += ",\"transition_graph\":{\"reused\":";
    json += diagnostics == nullptr
                ? "null"
                : bool_json(diagnostics->transition_cache_reused);
    json += "}";
    json += ",\"reforge\":{\"requests\":" +
            std::to_string(cache.reforge_requests);
    json += ",\"hits\":" + std::to_string(cache.reforge_hits);
    json += ",\"misses\":" + std::to_string(cache.reforge_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_reforge_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.reforge_build_ns);
    json += ",\"frontier_work\":" +
            std::to_string(cache.reforge_frontier_work) + "}}";

    json += ",\"optimization\":{";
    json += "\"method\":\"";
    json += diagnostics != nullptr && diagnostics->policy_iteration_fallback
                ? "policy_iteration_scc_with_prioritized_fallback"
                : "policy_iteration_scc";
    json += "\"";
    json += ",\"policy_evaluation_failure\":";
    if (diagnostics == nullptr ||
        diagnostics->policy_evaluation_failure.empty()) {
        json += "null";
    } else {
        json += "\"" + diagnostics->policy_evaluation_failure + "\"";
    }
    if (result == nullptr && snapshot == nullptr) {
        json += ",\"status\":\"not_run\",\"converged\":null";
        json += ",\"sweeps\":null,\"policy_improvement_rounds\":null";
        json += ",\"residual\":null,\"optimality_gap\":null";
        json += ",\"state_cap_hit\":null";
        json += ",\"resource_cap_hit\":null,\"cap_hits\":[]";
        json += ",\"full_request_status\":\"not_run\"";
    } else if (result == nullptr) {
        json += ",\"status\":\"";
        json += snapshot->abandoned ? "abandoned" : "in_progress";
        json += "\",\"converged\":null";
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":" + std::to_string(
                    diagnostics->policy_improvement_rounds);
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(diagnostics->focused_optimality_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                diagnostics->focused_optimality_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[]";
        json += ",\"full_request_status\":\"incomplete_not_finished\"";
    } else {
        const char* status = result->converged
                                 ? (qualified_action_subset
                                        ? "exact_supported_priced_subset"
                                        : "exact_abstract")
                                 : (diagnostics->resource_cap_hit
                                        ? "incomplete_resource_cap"
                                        : (diagnostics->state_cap_hit
                                               ? "incomplete_state_cap"
                                               : "not_converged"));
        json += ",\"status\":\"" + std::string(status) + "\"";
        json += ",\"converged\":" +
                std::string(bool_json(result->converged));
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":" + std::to_string(
                    diagnostics->policy_improvement_rounds);
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(diagnostics->focused_optimality_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                diagnostics->focused_optimality_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[";
        for (std::size_t i = 0; i < diagnostics->cap_hits.size(); ++i) {
            if (i != 0) json += ',';
            json += "\"" + diagnostics->cap_hits[i] + "\"";
        }
        json += "]";
        json += ",\"full_request_status\":\"";
        if (diagnostics->resource_cap_hit) {
            json += "incomplete_resource_cap";
        } else if (!result->converged) {
            json += "incomplete_solve";
        } else if (qualified_action_subset) {
            json += "incomplete_action_subset";
        } else {
            json += "exact_abstract_within_tolerance";
        }
        json += "\"";
    }
    json += "}";

    json += ",\"timings_ns\":{\"registry_generation\":" +
            optional_count(registry_generation_ns);
    json += ",\"abstract_layout\":" +
            std::to_string(calc.layout_build_ns());
    if (diagnostics == nullptr) {
        json += ",\"solve_setup\":null,\"expansion\":null";
        json += ",\"transition_calculation\":null,\"optimization\":null";
        json += ",\"extraction\":null";
    } else {
        json += ",\"solve_setup\":" +
                std::to_string(diagnostics->solve_setup_ns);
        json += ",\"expansion\":" +
                std::to_string(diagnostics->expansion_ns);
        json += ",\"transition_calculation\":" +
                std::to_string(cache.distribution_build_ns);
        json += ",\"optimization\":" +
                std::to_string(diagnostics->optimization_ns);
        if (result == nullptr) {
            json += ",\"extraction\":null";
        } else {
            json += ",\"extraction\":" +
                    std::to_string(diagnostics->extraction_ns);
        }
    }
    json += ",\"compilation\":" +
            count_or_null(compilation == nullptr ? nullptr
                                                 : &compilation->duration_ns);
    json += ",\"verification\":null}";

    const std::uint64_t current_bytes = calc.estimated_owned_bytes();
    json += ",\"memory\":{\"solver_owned_bytes_estimate\":" +
            std::to_string(diagnostics == nullptr
                               ? current_bytes
                               : diagnostics->solver_owned_bytes_estimate);
    json += ",\"estimate_kind\":\"selected_allocations_not_process_heap\"";
    json += ",\"abstract_state_bytes\":" +
            std::to_string(sizeof(AbstractState));
    json += ",\"state_payload\":\"inline_sparse_junk_counts\"}";

    json += ",\"compilation\":{";
    if (compilation == nullptr) {
        json += "\"available\":false,\"working_states\":null";
        json += ",\"policy_regions\":null";
        json += ",\"nodes\":null,\"edges\":null";
        json += ",\"strategy_json_bytes\":null";
        json += ",\"cap_hit\":null";
    } else {
        json += "\"available\":true,\"working_states\":" +
                std::to_string(compilation->working_states);
        json += ",\"policy_regions\":" +
                std::to_string(compilation->policy_regions);
        json += ",\"nodes\":" + std::to_string(compilation->nodes);
        json += ",\"edges\":" + std::to_string(compilation->edges);
        json += ",\"strategy_json_bytes\":" +
                std::to_string(compilation->strategy_json_bytes);
        json += ",\"cap_hit\":";
        if (compilation->cap_hit.empty()) {
            json += "null";
        } else {
            json += "\"" + compilation->cap_hit + "\"";
        }
    }
    json += "}";

    const bool has_result_bound =
        result != nullptr && result->start_state < result->values.size() &&
        std::isfinite(result->values[result->start_state]);
    const bool has_snapshot_bound =
        snapshot != nullptr && std::isfinite(snapshot->raw_start_bound);
    json += ",\"value\":{\"start\":";
    if (!has_result_bound || !result->converged) {
        json += "null";
    } else {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      result->values[result->start_state]);
        json += buffer;
    }
    json += ",\"start_status\":";
    if (result == nullptr && snapshot == nullptr) {
        json += "\"not_run\"";
    } else if (result == nullptr) {
        json += snapshot->abandoned ? "\"abandoned_before_completion\""
                                    : "\"solve_in_progress\"";
    } else if (result->converged) {
        json += qualified_action_subset
                    ? "\"exact_supported_priced_subset_within_tolerance\""
                    : "\"exact_abstract_within_tolerance\"";
    } else {
        json += "\"unavailable_incomplete_solve\"";
    }
    json += ",\"start_scope\":";
    if (result == nullptr || !result->converged) {
        json += "null";
    } else {
        json += qualified_action_subset ? "\"supported_priced_subset\""
                                        : "\"full_requested_action_set\"";
    }
    json += ",\"raw_start_bound\":";
    if (has_result_bound) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      result->values[result->start_state]);
        json += buffer;
    } else if (has_snapshot_bound) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      snapshot->raw_start_bound);
        json += buffer;
    } else {
        json += "null";
    }
    json += "},\"verification\":null}";
    return json;
}

} // namespace solver
} // namespace poecraft
