#pragma once

#include "solver_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {
namespace refinement {

/*
 * Policy-guided exact refinement is independent of CalcContext and
 * SolveResult. A production adapter supplies lazy strict-state enumeration and
 * exact primitive kernels; this module owns the common observation fixed
 * point, deterministic partition refinement, and lumpability proof.
 *
 * A StableKey must be collision-free exact carrier identity. It must never be
 * a representative modifier selected from a coarse class or a hash alone.
 */
using StableKey = std::vector<std::uint64_t>;

/*
 * One exact semantic fact. Item features have no affix metadata. Affix
 * features share `subject` across every fact about one occupied explicit.
 * The adapter supplies the traits/tags used by the registry-owned scoped
 * action contract. Observation signatures canonicalize affix subjects away,
 * so incidental slot order and unobserved raw identity cannot split a class.
 */
struct FeatureAtom {
    RefinementFeature feature = RefinementFeature::Rarity;
    std::uint32_t subject = 0;
    StableKey value;
    std::uint16_t affix_traits = 0;
    std::uint8_t item_traits = 0;
    std::vector<std::uint32_t> modifier_tag_ids;

    bool operator==(const FeatureAtom&) const = default;
};

using FeatureSignature = std::vector<FeatureAtom>;

/*
 * The same composable vocabulary is used for primitive mechanics and policy
 * router conditions. Affix observation selectors are conjunctions and their
 * vector is a union, matching ActionRefinementContract.
 */
struct ObservationRequirement {
    RefinementFeatureMask item_features = 0;
    std::vector<std::uint32_t> modifier_tag_ids;
    std::vector<RefinementAffixObservation> affix_observations;

    bool operator==(const ObservationRequirement&) const = default;
};

FeatureSignature canonical_feature_signature(FeatureSignature signature);

ObservationRequirement canonical_observation_requirement(
    ObservationRequirement requirement);

ObservationRequirement merge_observation_requirements(
    ObservationRequirement target,
    const ObservationRequirement& addition);

/* Canonical full-tuple serialization used when an existing observation
 * projection must be supplied to another collision-checked authority. */
StableKey canonical_observation_identity(
    const StableKey& coarse_parent,
    const ObservationRequirement& requirement,
    const FeatureSignature& exact_features);

ObservationRequirement observation_requirement_from_contract(
    const ActionRefinementContract& contract);

/*
 * Backward preimage through one action. May-preserve/may-destroy overlap keeps
 * the observation in the preimage while collapse telemetry can still record
 * that another outcome destroys it.
 */
ObservationRequirement preserved_observation_requirement(
    const ObservationRequirement& downstream,
    const ActionRefinementContract& contract);

FeatureSignature observe_features(
    const FeatureSignature& exact_features,
    const ObservationRequirement& requirement);

/*
 * Context needed to interpret goal-relative and compiled-count observations
 * on a concrete simulator item. Entries are indexed by session modifier id.
 * Empty per-mod values mean the feature is not present in the requirement.
 */
struct ExactObservationContext {
    std::vector<StableKey> goal_status_tier_class_by_mod;
    std::vector<StableKey> count_observation_membership_by_mod;
};

struct CompiledObservationProgram {
    ObservationRequirement requirement;
    FeatureSignature signature;
    ExactObservationContext context;
};

std::shared_ptr<const CompiledObservationProgram>
make_compiled_observation_program(
    const SessionImpl& session,
    const CompiledObservationSignature& compiled);

/*
 * Concrete counterpart of extract_strict_abstract_features(). This is shared
 * by compiled strategy routing and refinement tests so the strategy parser,
 * simulator, compiler, and exact evaluator all implement Obs_R through one
 * authority.
 */
FeatureSignature observe_exact_item_features(
    const SessionImpl& session,
    const pc_item_state& item,
    const ObservationRequirement& requirement,
    const ExactObservationContext& context);

struct AbstractFeatureExtraction {
    FeatureSignature features;
    RefinementFeatureMask unavailable_features = 0;

    bool complete() const {
        return unavailable_features == 0;
    }
};

/*
 * Extract only the observations requested by `requirement`. Every affix
 * property is proven uniform over its strict goal/junk member class. If the
 * supplied layout discarded a requested distinction, the feature is reported
 * unavailable instead of selecting a representative modifier.
 */
AbstractFeatureExtraction extract_strict_abstract_features(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state,
    const ObservationRequirement& requirement);

/*
 * `semantic_key` is the collision-free executable operation identity. It is
 * distinct from action_refinement_contract_signature(): two operations may
 * observe the same facts while having different transitions or parameters.
 */
struct SelectedAction {
    std::uint32_t action_id = 0;
    StableKey semantic_key;
    ActionRefinementContract contract;
    /*
     * Longest ordered primitive program plus every engine-authorized runtime
     * path for a composite PlannerOperator. Empty paths mean the one-step
     * `contract` above. Downstream observations are reverse-composed through
     * each path and their preimages are merged; a flat union or one assumed
     * longest path is insufficient for conditional/early-exit transforms.
     */
    std::vector<ActionRefinementContract> ordered_program;
    std::vector<std::vector<ActionRefinementContract>> execution_paths;
    ObservationRequirement routing_observes;
};

ObservationRequirement observation_requirement_from_selected_action(
    const SelectedAction& action);

ObservationRequirement preserved_observation_requirement(
    const ObservationRequirement& downstream,
    const SelectedAction& action);

/* Existing canonical runtime contract/program identity used by refinement,
 * compilation, and proof-carrying rows. */
StableKey canonical_selected_runtime_contract_identity(
    const SelectedAction& action);

/*
 * Shared cyclic policy-observation fixed point. Adapters describe only graph
 * topology and selected semantic contracts; this engine owns backward
 * preservation, deterministic ordering, round caps, and collapse accounting.
 */
struct PolicyObservationNode {
    std::uint32_t state_id = 0;
    std::optional<SelectedAction> selected_action;
    ObservationRequirement direct_observes;
    std::vector<std::uint32_t> successors;
    /* Optional one-level authorities avoid copying an interned selection or
     * one absolute successor row into every exact carrier. The named state
     * must exist in the same input and own the corresponding payload. */
    std::optional<std::uint32_t> selected_action_source;
    std::optional<std::uint32_t> successor_source;
};

struct PolicyObservationAssignment {
    std::uint32_t state_id = 0;
    ObservationRequirement required;
};

struct PolicyObservationFixedPoint {
    bool complete = false;
    bool round_cap = false;
    std::string failure_reason;
    std::uint32_t rounds = 0;
    std::uint64_t collapse_events = 0;
    RefinementFeatureMask collapse_destroyed_feature_mask = 0;
    RefinementFeatureMask collapse_preserved_feature_mask = 0;
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(RefinementFeature::Count)>
        collapse_events_by_feature{};
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(RefinementFeature::Count)>
        preservation_events_by_feature{};
    std::vector<PolicyObservationAssignment> assignments;
};

PolicyObservationFixedPoint
propagate_policy_observations(
    std::vector<PolicyObservationNode> nodes,
    std::uint32_t max_rounds);

/*
 * Shared exact probabilistic partition over a closed concrete graph.
 *
 * `stable_key` is used only to make node and class identities independent of
 * caller enumeration order. `observation_key` contains the exact facts an
 * observer may distinguish and seeds the initial partition.
 * `immediate_key` contains non-transition behavior that must agree inside a
 * final class (for example legality and exact cost); it participates in the
 * first refinement split so telemetry can distinguish observation-triggered
 * classes from behavior-triggered classes.
 *
 * Arcs with the same label are one observable outcome channel. An empty label
 * is the ordinary unlabeled channel. A successor indexes the caller's input
 * node vector; nullopt is absorption outside the graph. Every non-terminal row
 * must be stochastic. Terminal nodes have no arcs.
 *
 * Refinement is split-only and runs to a deterministic fixed point. Complete
 * results have passed a final exact lumpability proof: every member of a class
 * has equal observation/immediate behavior and equal probability into every
 * (label, successor-class) pair.
 */
struct ClosedPartitionArc {
    StableKey label;
    std::optional<std::uint32_t> successor;
    double probability = 0.0;
};

struct ClosedPartitionNode {
    StableKey stable_key;
    StableKey observation_key;
    StableKey immediate_key;
    bool terminal = false;
    std::vector<ClosedPartitionArc> arcs;
    /* Optional one-level authority naming another input node whose absolute
     * arc row is identical. The referencing node must not own arcs. */
    std::optional<std::uint32_t> arc_source;
    std::optional<std::uint32_t> observation_source;
    std::optional<std::uint32_t> immediate_source;
};

struct ClosedPartitionProjectedArc {
    StableKey label;
    std::optional<std::uint32_t> successor_class;
    double probability = 0.0;

    bool operator==(const ClosedPartitionProjectedArc&) const = default;
};

struct ClosedPartitionClass {
    std::uint32_t class_id = 0;
    std::vector<StableKey> member_keys;
    StableKey observation_key;
    StableKey immediate_key;
    bool terminal = false;
    std::vector<ClosedPartitionProjectedArc> arcs;

    bool operator==(const ClosedPartitionClass&) const = default;
};

struct ClosedPartitionLimits {
    std::uint32_t max_classes =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_rounds = 1000;
    /*
     * `retained_estimated_memory_bytes` is caller-owned memory that remains
     * live for the duration of this partition. It participates in the same
     * hard cap but is not owned by the returned result.
     */
    std::uint64_t retained_estimated_memory_bytes = 0;
    std::uint64_t max_estimated_memory_bytes =
        std::numeric_limits<std::uint64_t>::max();
    double probability_sum_tolerance = 1e-12;
};

enum class ClosedPartitionStatus {
    Complete = 0,
    EmptyGraph,
    InvalidGraph,
    ResourceCap,
    RefinementRoundCap,
    NonLumpable,
};

struct ClosedPartitionResult {
    ClosedPartitionStatus status = ClosedPartitionStatus::EmptyGraph;
    bool lumpable = false;
    std::string failure_reason;
    std::string resource_cap;
    std::uint32_t rounds = 0;
    std::uint32_t initial_class_count = 0;
    std::uint32_t final_class_count = 0;
    std::uint64_t lumpability_checks = 0;
    /* Returned partition plus caller-retained bytes; peak is total-live. */
    std::uint64_t estimated_memory_bytes = 0;
    std::uint64_t peak_estimated_memory_bytes = 0;
    /*
     * These vectors preserve caller input order even though class ids are
     * assigned in canonical stable-key order.
     */
    std::vector<std::uint32_t> initial_class_by_node;
    std::vector<std::uint32_t> class_by_node;
    std::vector<ClosedPartitionClass> classes;
};

ClosedPartitionResult refine_closed_probabilistic_partition(
    std::vector<ClosedPartitionNode> nodes,
    ClosedPartitionLimits limits = {});

/* Replay-backed form of the same split-only partition authority. The caller
 * retains only stable enumerator positions and reconstructs one canonical
 * node at a time; no complete exact carrier graph is materialized. A supplied
 * previous partition is a monotone lower-resolution seed and may only split.
 */
using ClosedPartitionReplayFunction =
    std::function<ClosedPartitionNode(std::uint32_t)>;

ClosedPartitionResult refine_closed_probabilistic_partition_replay(
    std::uint32_t node_count,
    const ClosedPartitionReplayFunction& replay,
    const std::vector<std::uint32_t>& previous_partition = {},
    bool retain_member_keys = false,
    ClosedPartitionLimits limits = {});

/*
 * Collision-free operation/state key for evaluator row collapse. Returns
 * nullopt when the layout cannot represent any feature observed by the action
 * or its policy router.
 */
std::optional<StableKey> canonical_operation_state_signature(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state,
    const SelectedAction& action);

struct ExactState {
    StableKey stable_key;
    std::uint32_t coarse_state = 0;
    FeatureSignature features;
    bool goal = false;
    bool terminal = false;
    /*
     * Collision-free identity of the coarse parent. Numeric coarse_state is
     * retained only for table lookup; ordering and published refinement
     * identities must not depend on discovery ids.
     */
    StableKey coarse_state_key;

    bool operator==(const ExactState&) const = default;
};

struct ExactTransition {
    ExactState successor;
    double probability = 0.0;
};

struct ExactActionKernel {
    double action_cost = 0.0;
    std::vector<ExactTransition> transitions;
};

class RefinementOracleResourceLimit : public std::runtime_error {
public:
    RefinementOracleResourceLimit(
        std::string cap_name,
        std::string message)
        : std::runtime_error(std::move(message)),
          cap_name_(std::move(cap_name)) {}

    const std::string& cap_name() const {
        return cap_name_;
    }

private:
    std::string cap_name_;
};

/*
 * Production adapter seam:
 *
 * - enumerate_refinements() is called exactly once for each requested coarse
 *   root and nowhere else. It must enumerate every strict carrier represented
 *   by that root, without materializing a representative.
 * - selected_action() may initially return the coarse policy action for every
 *   member, then return locally improved actions per exact subclass.
 * - exact_kernel() calls strict CalcContext/native primitive authority. Its
 *   full ExactState successor records are the only source of non-root exact
 *   states, so discovery follows only policy-reachable concrete successors
 *   and never enumerates an entire successor coarse parent.
 * - exact_kernel_reuse_key() may name a previously completed absolute row
 *   only when the native authority proves collision-free equality. The
 *   default disables reuse; callers must never infer it from a hash alone.
 *
 * The core sorts and cross-checks every record, so oracle enumeration order
 * or kernel transition order cannot change class identities.
 */
class PolicyRefinementOracle {
public:
    virtual ~PolicyRefinementOracle() = default;

    virtual std::vector<ExactState> enumerate_refinements(
        std::uint32_t coarse_state) = 0;

    virtual std::optional<SelectedAction> selected_action(
        const ExactState& state) = 0;

    virtual ExactActionKernel exact_kernel(
        const ExactState& state,
        const SelectedAction& action) = 0;

    virtual std::optional<StableKey> exact_kernel_reuse_key(
        const ExactState& state,
        const SelectedAction& action) {
        (void)state;
        (void)action;
        return std::nullopt;
    }

    virtual void note_exact_kernel_reuse() {}

    /*
     * Selected-owned memory retained by the adapter while the shared graph
     * is live. The core charges this together with its own graph on every
     * discovery boundary, so a growing strict transition cache cannot spend
     * the graph's full remaining budget independently.
     */
    virtual std::uint64_t estimated_owned_bytes() const {
        return 0;
    }
};

struct RefinementLimits {
    std::uint32_t max_coarse_states = 100000;
    std::uint32_t max_exact_states = 1000000;
    std::uint32_t max_exact_kernels = 1000000;
    std::uint64_t max_transitions = 10000000;
    std::uint32_t max_refinement_classes = 1000000;
    std::uint32_t max_refinement_rounds = 1000;
    std::uint64_t max_estimated_memory_bytes =
        std::numeric_limits<std::uint64_t>::max();
    std::uint32_t max_witnesses = 16;
    double probability_sum_tolerance = 1e-12;
    /* Exact strict-row evaluator selection. Production uses V3; native-only
     * controls retain V1 as oracle/rollback and V2 as a diagnostic. */
    bool use_projected_reforge_frontier = false;
    bool use_factored_terminal_reforge = false;
};

struct RefinementRequest {
    std::vector<std::uint32_t> coarse_roots;
    RefinementLimits limits;
};

enum class RefinementStatus {
    Complete = 0,
    EmptyRequest,
    OracleFailure,
    InvalidState,
    InvalidContract,
    MissingPolicyAction,
    InvalidKernel,
    ResourceCap,
    RefinementRoundCap,
    NonLumpable,
};

enum class CounterexampleKind {
    Observation = 0,
    SelectedAction,
    ActionCost,
    SuccessorProjection,
};

struct RefinementCounterexample {
    CounterexampleKind kind = CounterexampleKind::Observation;
    std::uint32_t coarse_state = 0;
    StableKey left_state;
    StableKey right_state;
    FeatureSignature differing_features;

    bool operator==(const RefinementCounterexample&) const = default;
};

struct ProjectedTransition {
    std::uint32_t successor_class = 0;
    double probability = 0.0;

    bool operator==(const ProjectedTransition&) const = default;
};

struct RefinedPolicyClass {
    std::uint32_t class_id = 0;
    std::uint32_t coarse_state = 0;
    bool goal = false;
    bool terminal = false;
    std::vector<StableKey> exact_members;
    /* Replay-backed quotient publication may retain collision-free strict
     * enumerator positions instead of duplicating every semantic key. */
    std::vector<std::uint32_t> strict_members;
    ObservationRequirement required_observations;
    FeatureSignature observation_signature;
    std::optional<SelectedAction> selected_action;
    double action_cost = 0.0;
    std::vector<ProjectedTransition> transitions;
    StableKey coarse_state_key;
};

struct StateClassAssignment {
    std::uint32_t coarse_state = 0;
    std::uint32_t class_id = 0;
    std::uint32_t exact_member_index = 0;

    bool operator==(const StateClassAssignment&) const = default;
};

struct RefinementTelemetry {
    std::uint32_t policy_reachable_coarse_states = 0;
    std::uint32_t exact_states = 0;
    std::uint32_t exact_kernels = 0;
    std::uint64_t exact_transitions = 0;
    std::uint32_t selected_action_routing_rounds = 0;
    std::uint32_t observation_propagation_rounds = 0;
    std::uint32_t partition_refinement_rounds = 0;
    std::uint32_t initial_observation_classes = 0;
    std::uint32_t final_refinement_classes = 0;
    std::uint32_t refinement_trigger_coarse_states = 0;
    std::uint32_t behavior_splits = 0;
    std::uint32_t merged_exact_states = 0;
    std::uint64_t collapse_events = 0;
    RefinementFeatureMask collapse_destroyed_feature_mask = 0;
    RefinementFeatureMask collapse_preserved_feature_mask = 0;
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(RefinementFeature::Count)>
        collapse_events_by_feature{};
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(RefinementFeature::Count)>
        preservation_events_by_feature{};
    std::uint64_t lumpability_checks = 0;
    std::uint64_t estimated_memory_bytes = 0;
    std::uint64_t peak_estimated_memory_bytes = 0;
    std::uint32_t witnesses_omitted = 0;
};

struct RefinementResult {
    RefinementStatus status = RefinementStatus::EmptyRequest;
    bool executable = false;
    bool lumpable = false;
    std::string failure_reason;
    std::string resource_cap;
    RefinementTelemetry telemetry;
    std::vector<StateClassAssignment> assignments;
    std::vector<RefinedPolicyClass> classes;
    std::vector<RefinementCounterexample> counterexamples;
};

inline const StableKey& assignment_exact_state(
        const RefinementResult& result,
        const StateClassAssignment& assignment) {
    return result.classes.at(assignment.class_id).exact_members.at(
        assignment.exact_member_index);
}

inline const StableKey& assignment_coarse_state_key(
        const RefinementResult& result,
        const StateClassAssignment& assignment) {
    return result.classes.at(assignment.class_id).coarse_state_key;
}

RefinementResult refine_policy_exact(
    PolicyRefinementOracle& oracle,
    RefinementRequest request);

/*
 * Exact evaluation of the executable quotient produced by
 * refine_policy_exact(). Only classes reachable from `start_classes` are
 * evaluated. Terminal classes absorb with value zero; every reachable
 * non-terminal class must select an action and have a stochastic row.
 *
 * Properness is proved structurally before values are solved: every reachable
 * bottom SCC must be terminal. Values are then obtained deterministically
 * from (I - P)V = c, one sink-first SCC at a time. `class_values` is sorted by
 * class_id and contains only reachable classes. `start_values` preserves the
 * caller's requested order, including duplicates.
 */
struct PolicyEvaluationLimits {
    std::uint32_t max_reachable_classes = 1000000;
    /* Per-SCC BiCGSTAB iterations plus fallback Gauss-Seidel sweeps. */
    std::uint32_t max_component_iterations = 100000;
    /*
     * Evaluator-owned scratch and returned result only, including the shared
     * WideFloat dense/sparse fixed-policy solve for each reachable SCC. There
     * is no hidden component-size ceiling: this declared byte cap is
     * preflighted before allocation. The refinement input is borrowed and
     * remains the caller's retained-memory responsibility.
     */
    std::uint64_t max_estimated_memory_bytes =
        std::numeric_limits<std::uint64_t>::max();
    double probability_sum_tolerance = 1e-12;
    double residual_tolerance = 1e-10;
};

struct PolicyEvaluationRequest {
    std::vector<std::uint32_t> start_classes;
    PolicyEvaluationLimits limits;
};

enum class PolicyEvaluationStatus {
    Complete = 0,
    InvalidRefinement,
    EmptyStartSet,
    InvalidStartClass,
    InvalidPolicy,
    ImproperPolicy,
    ResourceCap,
    SingularSystem,
    NumericFailure,
    ResidualFailure,
};

struct RefinedClassValue {
    std::uint32_t class_id = 0;
    double value = 0.0;

    bool operator==(const RefinedClassValue&) const = default;
};

struct PolicyEvaluationResult {
    PolicyEvaluationStatus status =
        PolicyEvaluationStatus::InvalidRefinement;
    bool converged = false;
    bool proper = false;
    std::string failure_reason;
    std::string resource_cap;
    std::uint32_t reachable_classes = 0;
    std::uint32_t strongly_connected_components = 0;
    std::uint32_t largest_component_classes = 0;
    /*
     * When status is ImproperPolicy, this is the lexicographically first
     * reachable closed non-terminal SCC, with its class ids sorted. The
     * count covers every such SCC reachable from the requested starts.
     * Returning one canonical component is enough to drive the next local
     * repair while keeping the witness bounded by the evaluator memory cap.
     */
    std::uint32_t improper_component_count = 0;
    std::vector<std::uint32_t> improper_component_classes;
    double max_residual = 0.0;
    /* Retained result bytes at success and peak evaluator-owned bytes. */
    std::uint64_t estimated_memory_bytes = 0;
    std::uint64_t peak_estimated_memory_bytes = 0;
    std::vector<RefinedClassValue> class_values;
    std::vector<double> start_values;
};

PolicyEvaluationResult evaluate_refined_policy_exact(
    const RefinementResult& refinement,
    PolicyEvaluationRequest request);

} // namespace refinement
} // namespace solver
} // namespace poecraft
