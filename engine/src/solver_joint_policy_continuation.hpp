#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace poecraft::solver::solve_detail {

using JointPolicySemanticKey = std::vector<std::uint64_t>;

enum class JointPolicyContinuationLifecycle : std::uint8_t {
    Inactive = 0,
    Captured,
    WaitingForExactContinuation,
    Resumable,
    Advancing,
    CompleteCandidate,
    Refused,
};

enum class JointPolicyContinuationRefusal : std::uint8_t {
    None = 0,
    StaleIdentity,
    StaleGeneration,
    StructuralPrefixInvalid,
    BoundarySnapshotIncompatible,
    ExactNoncompetitive,
    ImproperClosedComponent,
    UnsupportedContinuation,
    ActionOrRowUnavailable,
    ResourceInterrupted,
    CompilerRefused,
    ExactEvaluatorRefused,
    ReplacedByBetterCandidate,
};

inline constexpr std::string_view joint_policy_continuation_refusal_name(
        const JointPolicyContinuationRefusal refusal) {
    switch (refusal) {
    case JointPolicyContinuationRefusal::None: return "none";
    case JointPolicyContinuationRefusal::StaleIdentity:
        return "stale_identity";
    case JointPolicyContinuationRefusal::StaleGeneration:
        return "stale_generation";
    case JointPolicyContinuationRefusal::StructuralPrefixInvalid:
        return "structural_prefix_invalid";
    case JointPolicyContinuationRefusal::BoundarySnapshotIncompatible:
        return "boundary_snapshot_incompatible";
    case JointPolicyContinuationRefusal::ExactNoncompetitive:
        return "exact_noncompetitive";
    case JointPolicyContinuationRefusal::ImproperClosedComponent:
        return "improper_closed_component";
    case JointPolicyContinuationRefusal::UnsupportedContinuation:
        return "unsupported_continuation";
    case JointPolicyContinuationRefusal::ActionOrRowUnavailable:
        return "action_or_row_unavailable";
    case JointPolicyContinuationRefusal::ResourceInterrupted:
        return "resource_interrupted";
    case JointPolicyContinuationRefusal::CompilerRefused:
        return "compiler_refused";
    case JointPolicyContinuationRefusal::ExactEvaluatorRefused:
        return "exact_evaluator_refused";
    case JointPolicyContinuationRefusal::ReplacedByBetterCandidate:
        return "replaced_by_better_candidate";
    }
    return "unknown";
}

/*
 * Immutable authority and append-only-prefix boundary for one ordinary
 * selected-policy attempt. Value/proof generations are recorded as provenance
 * but deliberately are not part of structural compatibility: a fixed prefix
 * remains an executable candidate after unrelated global value improvement.
 */
struct JointPolicyContinuationContext {
    std::uint64_t goal_identity = 0;
    std::uint64_t economy_identity = 0;
    std::uint64_t caller_scope_identity = 0;
    std::uint64_t action_vocabulary_identity = 0;
    std::uint64_t action_vocabulary_size = 0;
    std::uint64_t mechanics_artifact_identity = 0;
    std::uint64_t exact_terminal_identity = 0;
    std::uint64_t boundary_identity = 0;
    std::uint64_t graph_prefix_identity = 0;
    std::uint64_t graph_row_count = 0;
    std::uint64_t graph_priced_row_count = 0;
    std::uint64_t graph_successor_count = 0;
    std::uint64_t graph_probability_count = 0;
    std::uint64_t graph_choice_count = 0;
    std::uint64_t graph_choice_successor_count = 0;
    std::uint64_t graph_choice_option_count = 0;
    std::uint64_t source_generation = 0;
    std::uint64_t target_generation = 0;
    std::uint64_t action_generation = 0;
    std::uint64_t admission_generation = 0;
    std::uint64_t value_snapshot_generation = 0;
    std::uint64_t proof_snapshot_generation = 0;
    std::uint64_t incumbent_identity = 0;
    double incumbent_exact_cost = std::numeric_limits<double>::infinity();

    bool operator==(const JointPolicyContinuationContext&) const = default;
};

struct JointPolicyContinuationNode {
    std::uint32_t locator = std::numeric_limits<std::uint32_t>::max();
    JointPolicySemanticKey semantic_key;

    bool operator==(const JointPolicyContinuationNode&) const = default;
};

struct JointPolicyContinuationChoice {
    std::uint32_t ordinal = 0;
    JointPolicyContinuationNode selected;

    bool operator==(const JointPolicyContinuationChoice&) const = default;
};

struct JointPolicyContinuationDecision {
    JointPolicyContinuationNode source;
    std::uint64_t row_locator = std::numeric_limits<std::uint64_t>::max();
    JointPolicySemanticKey row_semantic_key;
    JointPolicySemanticKey action_semantic_key;
    std::vector<JointPolicyContinuationChoice> observed_choices;

    bool operator==(const JointPolicyContinuationDecision&) const = default;
};

struct JointPolicyContinuationResolvedState {
    enum class Kind : std::uint8_t {
        Goal = 0,
        Row,
        CertifiedBoundary,
        Missing,
        Improper,
        Unsupported,
        ResourceInterrupted,
    };

    Kind kind = Kind::Missing;
    JointPolicyContinuationNode source;
    std::uint64_t row_locator = std::numeric_limits<std::uint64_t>::max();
    JointPolicySemanticKey row_semantic_key;
    JointPolicySemanticKey action_semantic_key;
    std::vector<JointPolicyContinuationNode> successors;
    std::vector<JointPolicyContinuationChoice> observed_choices;
};

class ResumableJointPolicyContinuation {
public:
    using Resolve = std::function<JointPolicyContinuationResolvedState(
        const JointPolicyContinuationNode&)>;
    using ValidateDecision = std::function<JointPolicyContinuationRefusal(
        const JointPolicyContinuationDecision&)>;

    struct AdvanceResult {
        JointPolicyContinuationLifecycle lifecycle =
            JointPolicyContinuationLifecycle::Inactive;
        JointPolicyContinuationRefusal refusal =
            JointPolicyContinuationRefusal::None;
        std::uint64_t work = 0;
    };

    JointPolicyContinuationContext context;
    JointPolicyContinuationLifecycle lifecycle =
        JointPolicyContinuationLifecycle::Inactive;
    JointPolicyContinuationRefusal refusal =
        JointPolicyContinuationRefusal::None;
    std::uint64_t semantic_identity = 0;
    double candidate_root_estimate = std::numeric_limits<double>::infinity();
    std::uint64_t root_estimate_provenance = 0;
    std::vector<double> selection_values;
    std::vector<double> certified_boundary_values;
    std::vector<std::uint32_t> certified_frontier_actions;
    std::vector<JointPolicyContinuationNode> walk;
    std::vector<std::uint8_t> processed;
    std::vector<std::uint8_t> reachable;
    std::vector<JointPolicyContinuationDecision> fixed_decisions;
    std::size_t cursor = 0;
    JointPolicyContinuationNode missing;
    std::uint64_t capture_count = 0;
    std::uint64_t resume_count = 0;
    std::uint64_t yield_count = 0;
    std::uint64_t rebase_count = 0;
    std::uint64_t stale_discard_count = 0;
    std::uint64_t noncompetitive_discard_count = 0;
    std::uint64_t completion_count = 0;
    std::uint64_t rows_appended = 0;
    std::uint64_t retained_work = 0;
    std::uint64_t transient_peak_bytes = 0;
    std::uint64_t retained_peak_bytes = 0;
    std::size_t released_fixed_decision_count = 0;
    std::size_t released_walk_state_count = 0;
    std::vector<JointPolicySemanticKey> missing_state_history;

    static ResumableJointPolicyContinuation capture(
            JointPolicyContinuationContext snapshot,
            JointPolicyContinuationNode start,
            std::vector<double> selection_snapshot,
            std::vector<double> boundary_values,
            std::vector<std::uint32_t> boundary_actions,
            const double root_estimate,
            const std::uint64_t root_provenance,
            const std::size_t state_capacity) {
        ResumableJointPolicyContinuation candidate;
        candidate.context = snapshot;
        candidate.lifecycle = JointPolicyContinuationLifecycle::Captured;
        candidate.selection_values = std::move(selection_snapshot);
        candidate.certified_boundary_values = std::move(boundary_values);
        candidate.certified_frontier_actions = std::move(boundary_actions);
        candidate.candidate_root_estimate = root_estimate;
        candidate.root_estimate_provenance = root_provenance;
        candidate.walk.push_back(std::move(start));
        candidate.processed.assign(state_capacity, 0);
        candidate.reachable.assign(state_capacity, 0);
        candidate.capture_count = 1;
        candidate.semantic_identity = candidate.compute_capture_identity();
        candidate.refresh_retained_peak();
        return candidate;
    }

    JointPolicyContinuationRefusal compatible_with(
            const JointPolicyContinuationContext& current) const {
        if (context.goal_identity != current.goal_identity ||
            context.economy_identity != current.economy_identity ||
            context.caller_scope_identity != current.caller_scope_identity ||
            context.action_vocabulary_identity !=
                current.action_vocabulary_identity ||
            context.mechanics_artifact_identity !=
                current.mechanics_artifact_identity ||
            context.exact_terminal_identity !=
                current.exact_terminal_identity) {
            return JointPolicyContinuationRefusal::StaleIdentity;
        }
        if (context.boundary_identity != current.boundary_identity) {
            return JointPolicyContinuationRefusal::
                BoundarySnapshotIncompatible;
        }
        if (context.graph_prefix_identity != current.graph_prefix_identity) {
            return JointPolicyContinuationRefusal::StructuralPrefixInvalid;
        }
        if (current.action_vocabulary_size <
                context.action_vocabulary_size ||
            current.source_generation < context.source_generation ||
            current.target_generation < context.target_generation ||
            current.action_generation != context.action_generation ||
            current.admission_generation != context.admission_generation) {
            return JointPolicyContinuationRefusal::StaleGeneration;
        }
        return JointPolicyContinuationRefusal::None;
    }

    JointPolicyContinuationRefusal validate_prefix(
            const ValidateDecision& validate) const {
        for (const JointPolicyContinuationDecision& decision :
             fixed_decisions) {
            const JointPolicyContinuationRefusal invalid = validate(decision);
            if (invalid != JointPolicyContinuationRefusal::None) {
                return invalid;
            }
        }
        return JointPolicyContinuationRefusal::None;
    }

    bool mark_resumable(const JointPolicyContinuationNode& completed) {
        if (lifecycle !=
                JointPolicyContinuationLifecycle::
                    WaitingForExactContinuation ||
            completed != missing) {
            return false;
        }
        lifecycle = JointPolicyContinuationLifecycle::Resumable;
        return true;
    }

    bool extend_state_capacity(const std::size_t state_capacity) {
        if (state_capacity < processed.size() ||
            processed.size() != reachable.size()) {
            return false;
        }
        processed.resize(state_capacity, 0);
        reachable.resize(state_capacity, 0);
        refresh_retained_peak();
        return true;
    }

    AdvanceResult advance(
            const Resolve& resolve,
            const ValidateDecision& validate,
            const std::uint64_t max_work) {
        if (lifecycle == JointPolicyContinuationLifecycle::Refused ||
            lifecycle == JointPolicyContinuationLifecycle::CompleteCandidate ||
            lifecycle == JointPolicyContinuationLifecycle::Inactive) {
            return {lifecycle, refusal, 0};
        }
        if (lifecycle ==
            JointPolicyContinuationLifecycle::
                WaitingForExactContinuation) {
            return {lifecycle, refusal, 0};
        }
        if (lifecycle == JointPolicyContinuationLifecycle::Resumable) {
            const JointPolicyContinuationRefusal invalid =
                validate_prefix(validate);
            if (invalid != JointPolicyContinuationRefusal::None) {
                release(invalid);
                return {lifecycle, refusal, 0};
            }
            ++resume_count;
        }
        lifecycle = JointPolicyContinuationLifecycle::Advancing;
        std::uint64_t work = 0;
        while (cursor < walk.size()) {
            if (work == max_work) {
                retained_work += work;
                release(JointPolicyContinuationRefusal::ResourceInterrupted);
                return {lifecycle, refusal, work};
            }
            ++work;
            const JointPolicyContinuationNode state = walk[cursor];
            if (state.locator >= processed.size()) {
                retained_work += work;
                release(JointPolicyContinuationRefusal::StaleGeneration);
                return {lifecycle, refusal, work};
            }
            if (processed[state.locator]) {
                ++cursor;
                continue;
            }
            reachable[state.locator] = 1;
            JointPolicyContinuationResolvedState resolved = resolve(state);
            transient_peak_bytes = std::max(
                transient_peak_bytes, resolved_transient_bytes(resolved));
            if (resolved.source != state) {
                retained_work += work;
                release(JointPolicyContinuationRefusal::StaleIdentity);
                return {lifecycle, refusal, work};
            }
            switch (resolved.kind) {
            case JointPolicyContinuationResolvedState::Kind::Goal:
            case JointPolicyContinuationResolvedState::Kind::
                    CertifiedBoundary:
                processed[state.locator] = 1;
                ++cursor;
                break;
            case JointPolicyContinuationResolvedState::Kind::Row: {
                JointPolicyContinuationDecision decision{
                    state,
                    resolved.row_locator,
                    std::move(resolved.row_semantic_key),
                    std::move(resolved.action_semantic_key),
                    std::move(resolved.observed_choices)};
                fixed_decisions.push_back(std::move(decision));
                ++rows_appended;
                for (JointPolicyContinuationNode& successor :
                     resolved.successors) {
                    if (successor.locator >= processed.size()) {
                        retained_work += work;
                        release(
                            JointPolicyContinuationRefusal::StaleGeneration);
                        return {lifecycle, refusal, work};
                    }
                    if (!reachable[successor.locator]) {
                        walk.push_back(std::move(successor));
                    }
                }
                for (const JointPolicyContinuationChoice& choice :
                     fixed_decisions.back().observed_choices) {
                    if (choice.selected.locator >= processed.size()) {
                        retained_work += work;
                        release(
                            JointPolicyContinuationRefusal::StaleGeneration);
                        return {lifecycle, refusal, work};
                    }
                    if (!reachable[choice.selected.locator]) {
                        walk.push_back(choice.selected);
                    }
                }
                processed[state.locator] = 1;
                ++cursor;
                break;
            }
            case JointPolicyContinuationResolvedState::Kind::Missing:
                missing = state;
                missing_state_history.push_back(state.semantic_key);
                lifecycle = JointPolicyContinuationLifecycle::
                    WaitingForExactContinuation;
                ++yield_count;
                retained_work += work;
                refresh_retained_peak();
                return {lifecycle, refusal, work};
            case JointPolicyContinuationResolvedState::Kind::Improper:
                retained_work += work;
                release(
                    JointPolicyContinuationRefusal::ImproperClosedComponent);
                return {lifecycle, refusal, work};
            case JointPolicyContinuationResolvedState::Kind::Unsupported:
                retained_work += work;
                release(
                    JointPolicyContinuationRefusal::UnsupportedContinuation);
                return {lifecycle, refusal, work};
            case JointPolicyContinuationResolvedState::Kind::
                    ResourceInterrupted:
                retained_work += work;
                release(JointPolicyContinuationRefusal::ResourceInterrupted);
                return {lifecycle, refusal, work};
            }
        }
        retained_work += work;
        missing = {};
        lifecycle = JointPolicyContinuationLifecycle::CompleteCandidate;
        ++completion_count;
        refresh_retained_peak();
        return {lifecycle, refusal, work};
    }

    void release(const JointPolicyContinuationRefusal reason) {
        refusal = reason;
        lifecycle = JointPolicyContinuationLifecycle::Refused;
        stale_discard_count +=
            reason == JointPolicyContinuationRefusal::StaleIdentity ||
            reason == JointPolicyContinuationRefusal::StaleGeneration ||
            reason ==
                JointPolicyContinuationRefusal::StructuralPrefixInvalid ||
            reason == JointPolicyContinuationRefusal::
                BoundarySnapshotIncompatible;
        noncompetitive_discard_count +=
            reason == JointPolicyContinuationRefusal::ExactNoncompetitive;
        refresh_retained_peak();
        released_fixed_decision_count = std::max(
            released_fixed_decision_count, fixed_decisions.size());
        released_walk_state_count = std::max(
            released_walk_state_count, walk.size());
        release_vector(selection_values);
        release_vector(certified_boundary_values);
        release_vector(certified_frontier_actions);
        release_vector(walk);
        release_vector(processed);
        release_vector(reachable);
        release_vector(fixed_decisions);
    }

    std::size_t fixed_decision_count() const {
        return std::max(
            released_fixed_decision_count, fixed_decisions.size());
    }

    std::size_t walk_state_count() const {
        return std::max(released_walk_state_count, walk.size());
    }

    void rebase() {
        ++rebase_count;
        release(JointPolicyContinuationRefusal::StructuralPrefixInvalid);
    }

    bool exactly_noncompetitive(
            const double independently_admissible_candidate_lower,
            const double verified_upper) const {
        return std::isfinite(independently_admissible_candidate_lower) &&
            std::isfinite(verified_upper) &&
            independently_admissible_candidate_lower >= verified_upper;
    }

    static bool candidate_precedes(
            const ResumableJointPolicyContinuation& left,
            const ResumableJointPolicyContinuation& right) {
        if (left.candidate_root_estimate != right.candidate_root_estimate) {
            return left.candidate_root_estimate < right.candidate_root_estimate;
        }
        if (left.semantic_identity != right.semantic_identity) {
            return left.semantic_identity < right.semantic_identity;
        }
        /* The hash is only an accelerator. Preserve a deterministic total
         * order under collision by comparing the canonical capture material. */
        if (left.context != right.context) {
            return canonical_context_precedes(left.context, right.context);
        }
        if (left.selection_values != right.selection_values) {
            return std::lexicographical_compare(
                left.selection_values.begin(), left.selection_values.end(),
                right.selection_values.begin(), right.selection_values.end());
        }
        return std::lexicographical_compare(
            left.walk.begin(), left.walk.end(),
            right.walk.begin(), right.walk.end(),
            [](const JointPolicyContinuationNode& a,
               const JointPolicyContinuationNode& b) {
                return a.semantic_key != b.semantic_key
                    ? std::lexicographical_compare(
                          a.semantic_key.begin(), a.semantic_key.end(),
                          b.semantic_key.begin(), b.semantic_key.end())
                    : a.locator < b.locator;
            });
    }

    std::uint64_t retained_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        auto add = [&](const std::uint64_t amount) {
            bytes = amount > std::numeric_limits<std::uint64_t>::max() - bytes
                ? std::numeric_limits<std::uint64_t>::max()
                : bytes + amount;
        };
        add(selection_values.capacity() * sizeof(double));
        add(certified_boundary_values.capacity() * sizeof(double));
        add(certified_frontier_actions.capacity() * sizeof(std::uint32_t));
        add(walk.capacity() * sizeof(JointPolicyContinuationNode));
        for (const JointPolicyContinuationNode& node : walk) {
            add(node.semantic_key.capacity() * sizeof(std::uint64_t));
        }
        add(processed.capacity() * sizeof(std::uint8_t));
        add(reachable.capacity() * sizeof(std::uint8_t));
        add(fixed_decisions.capacity() *
            sizeof(JointPolicyContinuationDecision));
        for (const JointPolicyContinuationDecision& decision :
             fixed_decisions) {
            add(decision.source.semantic_key.capacity() *
                sizeof(std::uint64_t));
            add(decision.row_semantic_key.capacity() *
                sizeof(std::uint64_t));
            add(decision.action_semantic_key.capacity() *
                sizeof(std::uint64_t));
            add(decision.observed_choices.capacity() *
                sizeof(JointPolicyContinuationChoice));
            for (const JointPolicyContinuationChoice& choice :
                 decision.observed_choices) {
                add(choice.selected.semantic_key.capacity() *
                    sizeof(std::uint64_t));
            }
        }
        add(missing.semantic_key.capacity() * sizeof(std::uint64_t));
        add(missing_state_history.capacity() *
            sizeof(JointPolicySemanticKey));
        for (const JointPolicySemanticKey& key : missing_state_history) {
            add(key.capacity() * sizeof(std::uint64_t));
        }
        return bytes;
    }

private:
    template <typename T>
    static void release_vector(std::vector<T>& values) {
        std::vector<T>().swap(values);
    }

    static bool canonical_context_precedes(
            const JointPolicyContinuationContext& left,
            const JointPolicyContinuationContext& right) {
        return std::tuple{
                   left.goal_identity,
                   left.economy_identity,
                   left.caller_scope_identity,
                   left.action_vocabulary_identity,
                   left.action_vocabulary_size,
                   left.mechanics_artifact_identity,
                   left.exact_terminal_identity,
                   left.boundary_identity,
                   left.graph_prefix_identity,
                   left.graph_row_count,
                   left.graph_priced_row_count,
                   left.graph_successor_count,
                   left.graph_probability_count,
                   left.graph_choice_count,
                   left.graph_choice_successor_count,
                   left.graph_choice_option_count,
                   left.source_generation,
                   left.target_generation,
                   left.action_generation,
                   left.admission_generation,
                   left.value_snapshot_generation,
                   left.proof_snapshot_generation,
                   left.incumbent_identity,
                   std::bit_cast<std::uint64_t>(left.incumbent_exact_cost)} <
            std::tuple{
                   right.goal_identity,
                   right.economy_identity,
                   right.caller_scope_identity,
                   right.action_vocabulary_identity,
                   right.action_vocabulary_size,
                   right.mechanics_artifact_identity,
                   right.exact_terminal_identity,
                   right.boundary_identity,
                   right.graph_prefix_identity,
                   right.graph_row_count,
                   right.graph_priced_row_count,
                   right.graph_successor_count,
                   right.graph_probability_count,
                   right.graph_choice_count,
                   right.graph_choice_successor_count,
                   right.graph_choice_option_count,
                   right.source_generation,
                   right.target_generation,
                   right.action_generation,
                   right.admission_generation,
                   right.value_snapshot_generation,
                   right.proof_snapshot_generation,
                   right.incumbent_identity,
                   std::bit_cast<std::uint64_t>(right.incumbent_exact_cost)};
    }

    static void mix(std::uint64_t& hash, const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }

    std::uint64_t compute_capture_identity() const {
        std::uint64_t hash = 1469598103934665603ULL;
        constexpr std::string_view schema =
            "resumable_joint_policy_continuation_v1";
        for (const unsigned char byte : schema) mix(hash, byte);
        mix(hash, context.goal_identity);
        mix(hash, context.economy_identity);
        mix(hash, context.caller_scope_identity);
        mix(hash, context.action_vocabulary_identity);
        mix(hash, context.action_vocabulary_size);
        mix(hash, context.mechanics_artifact_identity);
        mix(hash, context.exact_terminal_identity);
        mix(hash, context.boundary_identity);
        mix(hash, context.graph_prefix_identity);
        mix(hash, context.source_generation);
        mix(hash, context.target_generation);
        mix(hash, context.action_generation);
        mix(hash, context.admission_generation);
        mix(hash, context.incumbent_identity);
        mix(hash, std::bit_cast<std::uint64_t>(
            context.incumbent_exact_cost));
        mix(hash, std::bit_cast<std::uint64_t>(candidate_root_estimate));
        mix(hash, root_estimate_provenance);
        for (const JointPolicyContinuationNode& node : walk) {
            mix(hash, node.semantic_key.size());
            for (const std::uint64_t word : node.semantic_key) mix(hash, word);
        }
        mix(hash, selection_values.size());
        for (const double value : selection_values) {
            mix(hash, std::bit_cast<std::uint64_t>(value));
        }
        return hash;
    }

    static std::uint64_t resolved_transient_bytes(
            const JointPolicyContinuationResolvedState& resolved) {
        std::uint64_t bytes = sizeof(resolved);
        auto add = [&](const std::uint64_t amount) {
            bytes = amount > std::numeric_limits<std::uint64_t>::max() - bytes
                ? std::numeric_limits<std::uint64_t>::max()
                : bytes + amount;
        };
        add(resolved.source.semantic_key.capacity() * sizeof(std::uint64_t));
        add(resolved.row_semantic_key.capacity() * sizeof(std::uint64_t));
        add(resolved.action_semantic_key.capacity() * sizeof(std::uint64_t));
        add(resolved.successors.capacity() *
            sizeof(JointPolicyContinuationNode));
        for (const JointPolicyContinuationNode& successor :
             resolved.successors) {
            add(successor.semantic_key.capacity() * sizeof(std::uint64_t));
        }
        add(resolved.observed_choices.capacity() *
            sizeof(JointPolicyContinuationChoice));
        for (const JointPolicyContinuationChoice& choice :
             resolved.observed_choices) {
            add(choice.selected.semantic_key.capacity() *
                sizeof(std::uint64_t));
        }
        return bytes;
    }

    void refresh_retained_peak() {
        retained_peak_bytes = std::max(
            retained_peak_bytes, retained_owned_bytes());
    }
};

} // namespace poecraft::solver::solve_detail
