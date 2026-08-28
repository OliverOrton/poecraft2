#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {
namespace fragment_v1 {

inline constexpr std::uint32_t kExecutableFragmentSchemaVersionV1 = 1;
inline constexpr const char* kExecutableFragmentVerifierVersionV1 =
    "exact_leaf_product_v1";
inline constexpr const char* kExecutableFragmentPropernessVersionV1 =
    "positive_scc_absorption_v1";
inline constexpr const char* kExecutableFragmentToleranceVersionV1 =
    "stored_double_mass_1e-12_residual_1e-10_v1";
inline constexpr double kExecutableFragmentProbabilityToleranceV1 = 1e-12;
inline constexpr double kExecutableFragmentResidualToleranceV1 = 1e-10;

using StableParametersV1 =
    std::vector<std::pair<std::string, std::string>>;
using ResourceVectorV1 =
    std::vector<std::pair<std::string, double>>;

struct CanonicalIdentityV1 {
    std::uint64_t digest = 0;
    std::string canonical_bytes;

    bool operator==(const CanonicalIdentityV1& other) const {
        return digest == other.digest &&
               canonical_bytes == other.canonical_bytes;
    }
};

struct ExactStateKeyV1 {
    std::vector<std::uint64_t> words;
    std::string opaque_bytes;

    bool operator==(const ExactStateKeyV1&) const = default;
    bool operator<(const ExactStateKeyV1& other) const;
};

struct ExactStateV1 {
    ExactStateKeyV1 key;
    std::vector<std::int64_t> hard_execution_state;

    /* Diagnostic-only. It is deliberately absent from every equality,
     * routing, SCC, merge, and certificate key. */
    std::string diagnostic_carrier_projection;

    /* The test oracle uses these to emulate already-typed simulator
     * observations. A real adapter evaluates the same FragmentConditionV1
     * through native condition authority instead. */
    std::vector<std::string> true_condition_identities;
    std::vector<std::string> offered_choice_ids;
};

struct FragmentConditionV1 {
    std::string type;
    StableParametersV1 parameters;
    std::vector<FragmentConditionV1> children;
    std::uint32_t at_least_count = 0;

    bool operator==(const FragmentConditionV1&) const = default;
};

enum class FragmentNodeKindV1 : std::uint8_t {
    Route = 0,
    PrimitiveOperation,
    ObservedChoice,
    Exit,
};

enum class FragmentExitKindV1 : std::uint8_t {
    FinalSuccess = 0,
    Subgoal,
    Recoverable,
    CertificationFailure,
};

struct FragmentExitDescriptorV1 {
    FragmentExitKindV1 kind =
        FragmentExitKindV1::CertificationFailure;
    std::string label;
    StableParametersV1 parameters;

    bool operator==(const FragmentExitDescriptorV1&) const = default;
};

struct FragmentEdgeV1 {
    std::string edge_id;
    std::string target_node_id;
    FragmentConditionV1 condition;
    std::uint32_t priority = 0;
    bool certification_fail_closed_default = false;

    /* Non-semantic presentation data. */
    std::string display_label;
    std::int32_t display_order = 0;
};

struct FragmentNodeV1 {
    std::string node_id;
    FragmentNodeKindV1 kind = FragmentNodeKindV1::Route;
    std::string stable_action_identity;
    std::vector<FragmentEdgeV1> edges;
    std::vector<std::string> observed_choice_order;
    FragmentExitDescriptorV1 exit;

    /* Non-semantic presentation data. */
    std::string display_label;
    std::int32_t display_order = 0;
};

struct ExecutableFragmentProposalV1 {
    std::string proposal_id;
    std::optional<double> heuristic_estimate;
    StableParametersV1 ranking_annotations;
};

struct ExecutableFragmentIRV1 {
    std::uint32_t schema_version =
        kExecutableFragmentSchemaVersionV1;
    std::string fragment_id;
    std::string exact_entry_product_state_identity;
    std::string caller_action_scope_identity;
    std::string disabled_action_family_identity;
    std::string exact_goal_identity;
    std::string mechanics_artifact_identity;
    std::string exact_state_key_semantics_version;
    std::string refinement_semantics_version;
    std::string condition_semantics_version;
    std::string verifier_version =
        kExecutableFragmentVerifierVersionV1;
    std::string properness_version =
        kExecutableFragmentPropernessVersionV1;
    std::string tolerance_version =
        kExecutableFragmentToleranceVersionV1;
    std::string entry_node_id;
    std::vector<std::string> controller_memory_schema;
    std::vector<std::int64_t> initial_controller_memory;
    std::vector<FragmentNodeV1> nodes;

    /* Non-semantic presentation data. */
    std::string display_label;
};

class SingleFragmentFlattenerV1;

class FlattenedFragmentCandidateV1 {
public:
    FlattenedFragmentCandidateV1(
        const FlattenedFragmentCandidateV1&) = default;
    FlattenedFragmentCandidateV1(
        FlattenedFragmentCandidateV1&&) noexcept = default;
    FlattenedFragmentCandidateV1& operator=(
        const FlattenedFragmentCandidateV1&) = default;
    FlattenedFragmentCandidateV1& operator=(
        FlattenedFragmentCandidateV1&&) noexcept = default;

    const CanonicalIdentityV1& candidate_identity() const {
        return candidate_identity_;
    }
    const std::string& ordinary_strategy_json() const {
        return ordinary_strategy_json_;
    }
    const std::optional<double>& independently_evaluated_candidate_cost()
            const {
        return independently_evaluated_candidate_cost_;
    }

private:
    struct ConstructionToken {};

    FlattenedFragmentCandidateV1(
        ConstructionToken,
        CanonicalIdentityV1 candidate_identity,
        std::string ordinary_strategy_json,
        std::optional<double> independently_evaluated_candidate_cost);

    CanonicalIdentityV1 candidate_identity_;
    std::string ordinary_strategy_json_;
    std::optional<double> independently_evaluated_candidate_cost_;

    friend class SingleFragmentFlattenerV1;
};

struct FragmentStructuralRefusalV1 {
    std::string code;
    std::string witness;
};

struct FragmentStructuralValidationV1 {
    bool valid = false;
    CanonicalIdentityV1 identity;
    FragmentStructuralRefusalV1 refusal;
};

CanonicalIdentityV1 canonical_fragment_condition_identity_v1(
    const FragmentConditionV1& condition);
CanonicalIdentityV1 canonical_fragment_ir_identity_v1(
    const ExecutableFragmentIRV1& ir);
std::string canonical_exact_entry_identity_v1(
    const ExactStateV1& state,
    const std::vector<std::int64_t>& initial_controller_memory);
FragmentStructuralValidationV1 validate_executable_fragment_ir_v1(
    const ExecutableFragmentIRV1& ir);

struct AuthoritativePhysicalOutcomeV1 {
    std::string physical_outcome_id;
    double probability = 0.0;
};

struct PrimitivePhysicalOutcomeV1 {
    std::string physical_outcome_id;
    double probability = 0.0;
    ExactStateV1 successor;
    std::optional<std::vector<std::int64_t>> next_controller_memory;
    ResourceVectorV1 resource_quantities;
};

struct PrimitiveExpansionV1 {
    bool action_known = true;
    bool legal = true;
    bool supported = true;
    std::string refusal_reason;
    std::vector<AuthoritativePhysicalOutcomeV1> authoritative_outcomes;
    std::vector<PrimitivePhysicalOutcomeV1> physical_outcomes;
};

class ExactPrimitiveOracleV1 {
public:
    virtual ~ExactPrimitiveOracleV1() = default;

    virtual PrimitiveExpansionV1 expand_primitive(
        const ExactStateV1& state,
        const std::string& stable_action_identity) const = 0;

    /* nullopt means the existing native condition vocabulary cannot express
     * or evaluate the distinction. Verification must refuse. */
    virtual std::optional<bool> evaluate_condition(
        const FragmentConditionV1& condition,
        const ExactStateV1& state) const = 0;
};

struct LeafVerificationContextV1 {
    ExactStateV1 exact_entry;
    std::vector<std::int64_t> initial_controller_memory;
    std::string caller_action_scope_identity;
    std::string disabled_action_family_identity;
    std::string exact_goal_identity;
    std::string mechanics_artifact_identity;
    std::set<std::string> resource_vocabulary;
    std::map<std::string, double> prices;
    bool has_live_imprint_checkpoint = false;
};

struct LeafVerificationLimitsV1 {
    std::uint32_t max_states = 4096;
    std::uint64_t max_transitions = 65536;
    std::uint64_t max_work_items = 1000000;
    std::uint64_t max_estimated_bytes = 256ull * 1024ull * 1024ull;
    double probability_sum_tolerance =
        kExecutableFragmentProbabilityToleranceV1;
    double residual_tolerance = kExecutableFragmentResidualToleranceV1;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::function<bool()> canceled;
};

struct ProductStateKeyV1 {
    ExactStateKeyV1 exact_item_key;
    std::vector<std::int64_t> hard_execution_state;
    std::string controller_node_id;
    std::vector<std::int64_t> controller_memory;

    bool operator==(const ProductStateKeyV1&) const = default;
    bool operator<(const ProductStateKeyV1& other) const;
};

struct ExitIdentityV1 {
    FragmentExitDescriptorV1 descriptor;
    ExactStateKeyV1 exact_item_key;
    std::vector<std::int64_t> hard_execution_state;
    std::vector<std::int64_t> controller_memory;

    bool operator==(const ExitIdentityV1&) const = default;
    bool operator<(const ExitIdentityV1& other) const;
};

struct VerifiedPhysicalOutcomeV1 {
    std::string physical_outcome_id;
    std::uint64_t probability_bits = 0;
    ExactStateKeyV1 successor_exact_item_key;
    std::vector<std::int64_t> successor_hard_execution_state;
    std::vector<std::int64_t> next_controller_memory;
    std::string selected_edge_id;
    ResourceVectorV1 resource_quantities;
};

struct VerifiedTransitionV1 {
    double probability = 0.0;
    std::optional<ProductStateKeyV1> target_state;
    std::optional<ExitIdentityV1> target_exit;
    ResourceVectorV1 probability_weighted_resource_mass;
};

struct VerifiedProductRowV1 {
    ProductStateKeyV1 source;
    std::string selected_action_identity;
    bool primitive_action = false;
    std::vector<VerifiedPhysicalOutcomeV1> physical_outcomes;
    std::vector<VerifiedTransitionV1> transitions;
    double probability_sum = 0.0;
};

struct VerifiedExitV1 {
    ExitIdentityV1 identity;
    double probability_from_entry = 0.0;
    ResourceVectorV1 joint_resource_mass_from_entry;
};

class VerifiedLeafFragmentV1 {
public:
    VerifiedLeafFragmentV1(const VerifiedLeafFragmentV1&) = default;
    VerifiedLeafFragmentV1(VerifiedLeafFragmentV1&&) noexcept = default;
    VerifiedLeafFragmentV1& operator=(
        const VerifiedLeafFragmentV1&) = default;
    VerifiedLeafFragmentV1& operator=(
        VerifiedLeafFragmentV1&&) noexcept = default;

    const ExecutableFragmentIRV1& structural_ir() const { return ir_; }
    const CanonicalIdentityV1& ir_identity() const { return ir_identity_; }
    const CanonicalIdentityV1& certificate_identity() const {
        return certificate_identity_;
    }
    const std::vector<VerifiedProductRowV1>& rows() const { return rows_; }
    const std::vector<VerifiedExitV1>& exits() const { return exits_; }
    const ResourceVectorV1& expected_resources() const {
        return expected_resources_;
    }
    const ResourceVectorV1& expected_action_counts() const {
        return expected_action_counts_;
    }
    const std::optional<double>& priced_expected_cost() const {
        return priced_expected_cost_;
    }
    double exit_probability_sum() const { return exit_probability_sum_; }
    double max_probability_mass_error() const {
        return max_probability_mass_error_;
    }
    double max_resource_residual() const { return max_resource_residual_; }
    std::uint32_t strongly_connected_components() const {
        return strongly_connected_components_;
    }
    std::uint32_t positive_probability_cyclic_components() const {
        return positive_probability_cyclic_components_;
    }
    std::uint64_t work_items() const { return work_items_; }
    std::uint64_t peak_estimated_bytes() const {
        return peak_estimated_bytes_;
    }

private:
    struct ConstructionToken {};

    VerifiedLeafFragmentV1(
        ConstructionToken,
        ExecutableFragmentIRV1 ir,
        CanonicalIdentityV1 ir_identity,
        CanonicalIdentityV1 certificate_identity,
        std::vector<VerifiedProductRowV1> rows,
        std::vector<VerifiedExitV1> exits,
        ResourceVectorV1 expected_resources,
        ResourceVectorV1 expected_action_counts,
        std::optional<double> priced_expected_cost,
        double exit_probability_sum,
        double max_probability_mass_error,
        double max_resource_residual,
        std::uint32_t strongly_connected_components,
        std::uint32_t positive_probability_cyclic_components,
        std::uint64_t work_items,
        std::uint64_t peak_estimated_bytes);

    ExecutableFragmentIRV1 ir_;
    CanonicalIdentityV1 ir_identity_;
    CanonicalIdentityV1 certificate_identity_;
    std::vector<VerifiedProductRowV1> rows_;
    std::vector<VerifiedExitV1> exits_;
    ResourceVectorV1 expected_resources_;
    ResourceVectorV1 expected_action_counts_;
    std::optional<double> priced_expected_cost_;
    double exit_probability_sum_ = 0.0;
    double max_probability_mass_error_ = 0.0;
    double max_resource_residual_ = 0.0;
    std::uint32_t strongly_connected_components_ = 0;
    std::uint32_t positive_probability_cyclic_components_ = 0;
    std::uint64_t work_items_ = 0;
    std::uint64_t peak_estimated_bytes_ = 0;

    friend class ExactLeafFragmentVerifierV1;
};

struct LeafVerificationRefusalV1 {
    std::string code;
    std::string witness;
    std::vector<std::string> canonical_component_witness;
    std::uint64_t work_items = 0;
    std::uint64_t peak_estimated_bytes = 0;
};

struct LeafVerificationResultV1 {
    std::optional<VerifiedLeafFragmentV1> verified;
    LeafVerificationRefusalV1 refusal;

    bool ok() const { return verified.has_value(); }
};

class ExactLeafFragmentVerifierV1 {
public:
    LeafVerificationResultV1 verify(
        const ExecutableFragmentIRV1& ir,
        const LeafVerificationContextV1& context,
        const ExactPrimitiveOracleV1& oracle,
        const LeafVerificationLimitsV1& limits = {}) const;
};

} // namespace fragment_v1
} // namespace solver
} // namespace poecraft
