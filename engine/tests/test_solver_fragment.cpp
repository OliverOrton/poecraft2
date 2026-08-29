#include "../benchmarks/solver_executable_fragment.hpp"
#include "../benchmarks/solver_executable_fragment_engine.hpp"
#include "../src/solver_policy_refinement.hpp"
#include "../src/solver_solve_types.hpp"
#include "tests.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace poecraft::solver {

struct SolveWorkTestAccess {
    using Impl = SolveWork::Impl;
};

} // namespace poecraft::solver

namespace {

using namespace poecraft::solver::fragment_v1;
using poecraft::solver::refinement::CompiledPolicyAssertion;

template <typename Value>
concept ProbabilityBearingFragmentView = requires(const Value& value) {
    value.exits();
    value.expected_resources();
    value.expected_action_counts();
    value.priced_expected_cost();
    value.exit_probability_sum();
};

static_assert(!std::is_default_constructible_v<VerifiedLeafFragmentV1>);
static_assert(
    !std::is_default_constructible_v<VerifiedLeafStructuralControlV1>);
static_assert(!std::is_default_constructible_v<FlattenedFragmentCandidateV1>);
static_assert(
    !ProbabilityBearingFragmentView<VerifiedLeafStructuralControlV1>);
static_assert(
    ProbabilityBearingFragmentView<VerifiedLeafFragmentV1>);
static_assert(!std::is_constructible_v<
              VerifiedLeafFragmentV1, ExecutableFragmentProposalV1>);
static_assert(!std::is_constructible_v<
              FlattenedFragmentCandidateV1,
              ExecutableFragmentProposalV1>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentProposalV1, ExecutableFragmentIRV1>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentProposalV1, VerifiedLeafFragmentV1>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentProposalV1,
              FlattenedFragmentCandidateV1>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentIRV1, VerifiedLeafFragmentV1>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentIRV1, FlattenedFragmentCandidateV1>);
static_assert(!std::is_convertible_v<
              VerifiedLeafFragmentV1, FlattenedFragmentCandidateV1>);
static_assert(!std::is_convertible_v<ExecutableFragmentProposalV1, double>);
static_assert(!std::is_convertible_v<ExecutableFragmentIRV1, double>);
static_assert(!std::is_convertible_v<VerifiedLeafFragmentV1, double>);
static_assert(!std::is_convertible_v<FlattenedFragmentCandidateV1, double>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentProposalV1, CompiledPolicyAssertion>);
static_assert(!std::is_convertible_v<
              ExecutableFragmentIRV1, CompiledPolicyAssertion>);
static_assert(!std::is_convertible_v<
              VerifiedLeafFragmentV1, CompiledPolicyAssertion>);
static_assert(!std::is_convertible_v<
              FlattenedFragmentCandidateV1, CompiledPolicyAssertion>);
static_assert(!std::is_constructible_v<
              poecraft::solver::SolveWorkTestAccess::Impl::
                  BoundedPolicyIncumbent,
              FlattenedFragmentCandidateV1>);

FragmentConditionV1 condition(
        std::string type,
        StableParametersV1 parameters = {}) {
    FragmentConditionV1 value;
    value.type = std::move(type);
    value.parameters = std::move(parameters);
    return value;
}

FragmentEdgeV1 edge(
        std::string id,
        std::string target,
        FragmentConditionV1 when,
        const std::uint32_t priority,
        const bool fail_closed_default = false) {
    FragmentEdgeV1 value;
    value.edge_id = std::move(id);
    value.target_node_id = std::move(target);
    value.condition = std::move(when);
    value.priority = priority;
    value.certification_fail_closed_default = fail_closed_default;
    return value;
}

FragmentNodeV1 exit_node(
        std::string id,
        const FragmentExitKindV1 kind,
        std::string label) {
    FragmentNodeV1 value;
    value.node_id = std::move(id);
    value.kind = FragmentNodeKindV1::Exit;
    value.exit.kind = kind;
    value.exit.label = std::move(label);
    return value;
}

ExactStateV1 exact_state(
        const std::uint64_t word,
        std::string projection = {},
        std::vector<FragmentConditionV1> true_conditions = {},
        std::vector<std::string> offered_choices = {}) {
    ExactStateV1 value;
    value.key.words = {word};
    value.hard_execution_state = {static_cast<std::int64_t>(word % 3)};
    value.diagnostic_carrier_projection = std::move(projection);
    for (const FragmentConditionV1& true_condition : true_conditions) {
        value.true_condition_identities.push_back(
            canonical_fragment_condition_identity_v1(true_condition)
                .canonical_bytes);
    }
    std::sort(
        value.true_condition_identities.begin(),
        value.true_condition_identities.end());
    value.offered_choice_ids = std::move(offered_choices);
    std::sort(
        value.offered_choice_ids.begin(),
        value.offered_choice_ids.end());
    return value;
}

std::string state_identity(const ExactStateV1& state) {
    std::string out;
    for (const std::uint64_t word : state.key.words) {
        out += std::to_string(word) + "/";
    }
    out += state.key.opaque_bytes + ":";
    for (const std::int64_t hard : state.hard_execution_state) {
        out += std::to_string(hard) + "/";
    }
    return out;
}

class FixtureOracle final : public ExactPrimitiveOracleV1 {
public:
    std::map<std::pair<std::string, std::string>, PrimitiveExpansionV1>
        expansions;
    std::set<std::string> unexpressible_conditions;

    void set(
            const ExactStateV1& state,
            std::string action,
            PrimitiveExpansionV1 expansion) {
        expansions.emplace(
            std::make_pair(state_identity(state), std::move(action)),
            std::move(expansion));
    }

    PrimitiveExpansionV1 expand_primitive(
            const ExactStateV1& state,
            const std::string& action) const override {
        const auto found = expansions.find({state_identity(state), action});
        if (found == expansions.end()) {
            PrimitiveExpansionV1 missing;
            missing.action_known = false;
            return missing;
        }
        return found->second;
    }

    std::optional<bool> evaluate_condition(
            const FragmentConditionV1& value,
            const ExactStateV1& state) const override {
        const std::string identity =
            canonical_fragment_condition_identity_v1(value).canonical_bytes;
        if (unexpressible_conditions.contains(identity)) return std::nullopt;
        if (value.type == "always") return true;
        if (value.type == "not") {
            const auto child = evaluate_condition(value.children.front(), state);
            if (!child) return std::nullopt;
            return !*child;
        }
        if (value.type == "all" || value.type == "all_of") {
            for (const FragmentConditionV1& child : value.children) {
                const auto matched = evaluate_condition(child, state);
                if (!matched) return std::nullopt;
                if (!*matched) return false;
            }
            return true;
        }
        if (value.type == "any" || value.type == "any_of") {
            for (const FragmentConditionV1& child : value.children) {
                const auto matched = evaluate_condition(child, state);
                if (!matched) return std::nullopt;
                if (*matched) return true;
            }
            return false;
        }
        if (value.type == "at_least") {
            std::uint32_t matches = 0;
            for (const FragmentConditionV1& child : value.children) {
                const auto matched = evaluate_condition(child, state);
                if (!matched) return std::nullopt;
                if (*matched) ++matches;
            }
            return matches >= value.at_least_count;
        }
        if (value.type == "has_unveil_option") {
            const auto key = std::find_if(
                value.parameters.begin(), value.parameters.end(),
                [](const auto& parameter) {
                    return parameter.first == "mod_key";
                });
            if (key == value.parameters.end()) return std::nullopt;
            return std::binary_search(
                state.offered_choice_ids.begin(),
                state.offered_choice_ids.end(), key->second);
        }
        return std::binary_search(
            state.true_condition_identities.begin(),
            state.true_condition_identities.end(), identity);
    }
};

PrimitivePhysicalOutcomeV1 physical(
        std::string id,
        const double probability,
        ExactStateV1 successor,
        ResourceVectorV1 resources = {}) {
    PrimitivePhysicalOutcomeV1 value;
    value.physical_outcome_id = std::move(id);
    value.probability = probability;
    value.successor = std::move(successor);
    value.resource_quantities = std::move(resources);
    return value;
}

PrimitiveExpansionV1 expansion(
        std::vector<PrimitivePhysicalOutcomeV1> outcomes) {
    PrimitiveExpansionV1 value;
    for (const PrimitivePhysicalOutcomeV1& outcome : outcomes) {
        value.authoritative_outcomes.push_back(
            {outcome.physical_outcome_id, outcome.probability});
    }
    value.physical_outcomes = std::move(outcomes);
    return value;
}

struct Fixture {
    LeafVerificationContextV1 context;
    ExecutableFragmentIRV1 ir;
    FixtureOracle oracle;
};

Fixture deterministic_fixture() {
    Fixture fixture;
    const FragmentConditionV1 success =
        condition("has_mod_group", {{"group", "goal"}});
    fixture.context.exact_entry = exact_state(1);
    fixture.context.caller_action_scope_identity = "scope:all-primitives-v1";
    fixture.context.disabled_action_family_identity = "families:none-v1";
    fixture.context.exact_goal_identity = "goal:one-exact-mod-v1";
    fixture.context.mechanics_artifact_identity = "artifact:test-v1";
    fixture.context.clean_base_state.base_metadata_path =
        "Metadata/Items/Test/VerifiedFragmentBase";
    fixture.context.clean_base_state.item_level = 86;
    fixture.context.resource_vocabulary = {"transmute"};
    fixture.context.prices = {{"transmute", 2.0}};

    fixture.ir.fragment_id = "deterministic-success-v1";
    fixture.ir.exact_entry_product_state_identity =
        canonical_exact_entry_identity_v1(
            fixture.context.exact_entry,
            fixture.context.initial_controller_memory);
    fixture.ir.caller_action_scope_identity =
        fixture.context.caller_action_scope_identity;
    fixture.ir.disabled_action_family_identity =
        fixture.context.disabled_action_family_identity;
    fixture.ir.exact_goal_identity = fixture.context.exact_goal_identity;
    fixture.ir.mechanics_artifact_identity =
        fixture.context.mechanics_artifact_identity;
    fixture.ir.exact_state_key_semantics_version = "exact-test-key-v1";
    fixture.ir.refinement_semantics_version = "refinement-test-v1";
    fixture.ir.condition_semantics_version = "simulator-condition-v1";
    fixture.ir.clean_base_state = fixture.context.clean_base_state;
    fixture.ir.entry_node_id = "act";

    FragmentNodeV1 act;
    act.node_id = "act";
    act.kind = FragmentNodeKindV1::PrimitiveOperation;
    act.stable_action_identity = "transmute";
    act.edges = {
        edge("success", "success", success, 0),
        edge("default", "failure", condition("always"), 1, true),
    };
    fixture.ir.nodes = {
        act,
        exit_node("success", FragmentExitKindV1::FinalSuccess, "success"),
        exit_node(
            "failure", FragmentExitKindV1::CertificationFailure,
            "fail-closed"),
    };
    fixture.oracle.set(
        fixture.context.exact_entry, "transmute",
        expansion({physical(
            "goal", 1.0, exact_state(2, {}, {success}),
            {{"transmute", 1.0}})}));
    return fixture;
}

Fixture cyclic_fixture(const double success_probability = 0.4) {
    Fixture fixture = deterministic_fixture();
    const FragmentConditionV1 success =
        condition("has_mod_group", {{"group", "goal"}});
    const FragmentConditionV1 retry =
        condition("rarity_is", {{"rarity", "magic"}});
    fixture.ir.fragment_id = "proper-cyclic-retry-v1";
    fixture.context.exact_entry = exact_state(1, {}, {retry});
    fixture.ir.exact_entry_product_state_identity =
        canonical_exact_entry_identity_v1(
            fixture.context.exact_entry,
            fixture.context.initial_controller_memory);
    FragmentNodeV1& act = fixture.ir.nodes.front();
    act.edges = {
        edge("success", "success", success, 0),
        edge("retry", "act", retry, 1),
        edge("default", "failure", condition("always"), 2, true),
    };
    fixture.oracle = FixtureOracle{};
    fixture.oracle.set(
        fixture.context.exact_entry, "transmute",
        expansion({
            physical(
                "goal", success_probability,
                exact_state(2, {}, {success}), {{"transmute", 1.0}}),
            physical(
                "retry", 1.0 - success_probability,
                exact_state(1, {}, {retry}), {{"transmute", 1.0}}),
        }));
    return fixture;
}

void check_refusal(
        const Fixture& fixture,
        const std::string& code,
        const LeafVerificationLimitsV1& limits = {}) {
    const LeafVerificationResultV1 result =
        ExactLeafFragmentVerifierV1{}.verify(
            fixture.ir, fixture.context, fixture.oracle, limits);
    PC_CHECK(!result.ok());
    PC_CHECK(result.refusal.code == code);
    PC_CHECK(!result.verified.has_value());
}

void authority_and_identity_tests() {
    Fixture fixture = deterministic_fixture();
    const FragmentStructuralValidationV1 valid =
        validate_executable_fragment_ir_v1(fixture.ir);
    PC_CHECK(valid.valid);

    ExecutableFragmentIRV1 presentation = fixture.ir;
    std::reverse(presentation.nodes.begin(), presentation.nodes.end());
    presentation.display_label = "presentation only";
    for (FragmentNodeV1& node : presentation.nodes) {
        node.display_label = "label:" + node.node_id;
        node.display_order += 42;
        for (FragmentEdgeV1& value : node.edges) {
            value.display_label = "edge:" + value.edge_id;
            value.display_order -= 7;
        }
    }
    PC_CHECK(
        canonical_fragment_ir_identity_v1(presentation) == valid.identity);

    ExecutableFragmentIRV1 behavior = fixture.ir;
    behavior.nodes.front().stable_action_identity = "alchemy";
    PC_CHECK(
        !(canonical_fragment_ir_identity_v1(behavior) == valid.identity));

    CanonicalIdentityV1 collision_left = valid.identity;
    CanonicalIdentityV1 collision_right = valid.identity;
    collision_right.canonical_bytes.push_back('x');
    collision_right.digest = collision_left.digest;
    PC_CHECK(!(collision_left == collision_right));

    Fixture second = deterministic_fixture();
    second.context.exact_entry = exact_state(99);
    second.ir.exact_entry_product_state_identity =
        canonical_exact_entry_identity_v1(
            second.context.exact_entry,
            second.context.initial_controller_memory);
    second.oracle = FixtureOracle{};
    const FragmentConditionV1 success =
        condition("has_mod_group", {{"group", "goal"}});
    second.oracle.set(
        second.context.exact_entry, "transmute",
        expansion({physical(
            "goal", 1.0, exact_state(2, {}, {success}),
            {{"transmute", 1.0}})}));
    const auto first_result = ExactLeafFragmentVerifierV1{}.verify(
        fixture.ir, fixture.context, fixture.oracle);
    const auto second_result = ExactLeafFragmentVerifierV1{}.verify(
        second.ir, second.context, second.oracle);
    PC_CHECK(first_result.ok());
    PC_CHECK(second_result.ok());
    PC_CHECK(!(first_result.verified->certificate_identity() ==
               second_result.verified->certificate_identity()));

    ExecutableFragmentIRV1 symbolic = fixture.ir;
    symbolic.exact_entry_product_state_identity = "symbolic:any-clean-item";
    const auto symbolic_result = validate_executable_fragment_ir_v1(symbolic);
    PC_CHECK(!symbolic_result.valid);
    PC_CHECK(
        symbolic_result.refusal.code ==
        "symbolic_entry_domain_not_supported");
}

void malformed_ir_tests() {
    const Fixture baseline = deterministic_fixture();
    const auto structural_code = [](ExecutableFragmentIRV1 ir) {
        return validate_executable_fragment_ir_v1(ir).refusal.code;
    };

    ExecutableFragmentIRV1 ir = baseline.ir;
    ir.schema_version = 2;
    PC_CHECK(structural_code(ir) == "unsupported_schema_version");

    ir = baseline.ir;
    ir.clean_base_state.base_metadata_path.clear();
    PC_CHECK(structural_code(ir) == "missing_clean_base_state");

    ir = baseline.ir;
    ir.clean_base_state.rarity = "magic";
    PC_CHECK(structural_code(ir) == "unsupported_clean_base_state");

    ir = baseline.ir;
    ir.nodes.push_back(ir.nodes.front());
    PC_CHECK(structural_code(ir) == "duplicate_node");

    ir = baseline.ir;
    ir.nodes.front().edges.push_back(ir.nodes.front().edges.front());
    PC_CHECK(structural_code(ir) == "duplicate_edge");

    ir = baseline.ir;
    ir.entry_node_id = "absent";
    PC_CHECK(structural_code(ir) == "missing_entry");

    ir = baseline.ir;
    ir.nodes.erase(ir.nodes.begin() + 1, ir.nodes.end());
    PC_CHECK(structural_code(ir) == "missing_exit");

    ir = baseline.ir;
    ir.nodes.front().stable_action_identity = "fragment:nested";
    PC_CHECK(structural_code(ir) == "nested_fragment_call");

    ir = baseline.ir;
    ir.nodes.front().edges.back().certification_fail_closed_default = false;
    PC_CHECK(structural_code(ir) == "implicit_default");

    ir = baseline.ir;
    ir.nodes.front().stable_action_identity = "operator_index:17";
    PC_CHECK(structural_code(ir) == "unstable_action_identity");

    ir = baseline.ir;
    ir.nodes.front().stable_action_identity = "restart";
    PC_CHECK(structural_code(ir) == "implicit_restart");

    ir = baseline.ir;
    ir.nodes.front().stable_action_identity = "imprint_restore";
    PC_CHECK(structural_code(ir) == "imprint_not_supported");

    ir = baseline.ir;
    ir.nodes.front().edges.front().condition.type = "invented_condition";
    PC_CHECK(structural_code(ir) == "unknown_condition");

    ir = baseline.ir;
    ir.nodes.front().kind = static_cast<FragmentNodeKindV1>(255);
    PC_CHECK(structural_code(ir) == "invalid_node_kind");

    ir = baseline.ir;
    ir.nodes[1].exit.kind = static_cast<FragmentExitKindV1>(255);
    PC_CHECK(structural_code(ir) == "invalid_exit_kind");

    ir = baseline.ir;
    ir.controller_memory_schema = {"same", "same"};
    ir.initial_controller_memory = {0, 0};
    PC_CHECK(structural_code(ir) == "invalid_controller_memory_schema");

    Fixture unknown = deterministic_fixture();
    unknown.ir.nodes.front().stable_action_identity = "unknown-action";
    check_refusal(unknown, "unknown_action");

    Fixture imprint_entry = deterministic_fixture();
    imprint_entry.context.has_live_imprint_checkpoint = true;
    check_refusal(
        imprint_entry, "imprint_checkpoint_not_supported");

    Fixture mismatched_base = deterministic_fixture();
    ++mismatched_base.ir.clean_base_state.item_level;
    check_refusal(
        mismatched_base, "verification_context_identity_mismatch");

    Fixture unknown_resource = deterministic_fixture();
    PrimitiveExpansionV1 unknown_resource_expansion =
        unknown_resource.oracle.expand_primitive(
            unknown_resource.context.exact_entry, "transmute");
    unknown_resource_expansion.physical_outcomes.front()
        .resource_quantities = {{"invented-resource", 1.0}};
    unknown_resource.oracle = FixtureOracle{};
    unknown_resource.oracle.set(
        unknown_resource.context.exact_entry, "transmute",
        unknown_resource_expansion);
    check_refusal(unknown_resource, "unknown_resource_key");
}

void mass_and_projection_regression_tests() {
    Fixture historical = deterministic_fixture();
    historical.ir.fragment_id =
        "historical-coarse-row-missing-mass-v1";
    const FragmentConditionV1 progress =
        condition("has_mod_group", {{"group", "progress"}});
    const FragmentConditionV1 recoverable =
        condition("rarity_is", {{"rarity", "magic"}});
    const ExactStateV1 recoverable_state =
        exact_state(3, "same-carrier", {recoverable});
    const ExactStateV1 blocker_state = exact_state(4, "same-carrier");
    FragmentNodeV1& act = historical.ir.nodes.front();
    act.edges = {
        edge("progress", "success", progress, 0),
        edge("recoverable", "recoverable-exit", recoverable, 1),
        edge("default", "blocker-exit", condition("always"), 2, true),
    };
    historical.ir.nodes.insert(
        historical.ir.nodes.end() - 1,
        exit_node(
            "recoverable-exit", FragmentExitKindV1::Recoverable,
            "recoverable-miss"));
    historical.ir.nodes.insert(
        historical.ir.nodes.end() - 1,
        exit_node(
            "blocker-exit", FragmentExitKindV1::CertificationFailure,
            "wrong-carrier"));
    PrimitiveExpansionV1 rejected;
    rejected.authoritative_outcomes = {
        {"progress", 0.50},
        {"recoverable", 0.30},
        {"blocker", 0.20},
    };
    rejected.physical_outcomes = {
        physical("progress", 0.50, exact_state(2, {}, {progress})),
        physical("recoverable", 0.30, recoverable_state),
    };
    historical.oracle = FixtureOracle{};
    historical.oracle.set(
        historical.context.exact_entry, "transmute", rejected);
    check_refusal(historical, "authoritative_outcome_missing");
    PC_CHECK(std::fabs(0.50 + 0.30 - 0.80) < 1e-15);
    PC_CHECK(
        recoverable_state.diagnostic_carrier_projection ==
        blocker_state.diagnostic_carrier_projection);
    PC_CHECK(recoverable_state.key != blocker_state.key);
    PC_CHECK(
        historical.ir.nodes.front().edges[1].target_node_id !=
        historical.ir.nodes.front().edges[2].target_node_id);
    constexpr double archived_rejected_estimate_a = 12365.392875058;
    constexpr double archived_rejected_estimate_b = 12197.277488393;
    PC_CHECK(std::isfinite(archived_rejected_estimate_a));
    PC_CHECK(std::isfinite(archived_rejected_estimate_b));

    Fixture normalized = historical;
    PrimitiveExpansionV1 normalized_expansion = rejected;
    normalized_expansion.physical_outcomes = {
        physical("progress", 0.625, exact_state(2, {}, {progress})),
        physical("recoverable", 0.375, recoverable_state),
        physical("blocker", 0.0, blocker_state),
    };
    normalized.oracle = FixtureOracle{};
    normalized.oracle.set(
        normalized.context.exact_entry, "transmute",
        normalized_expansion);
    check_refusal(normalized, "authoritative_probability_bits_mismatch");

    Fixture duplicate = deterministic_fixture();
    PrimitiveExpansionV1 duplicate_expansion;
    duplicate_expansion.authoritative_outcomes = {
        {"same", 0.60}, {"other", 0.40}};
    duplicate_expansion.physical_outcomes = {
        physical("same", 0.60, exact_state(2)),
        physical("same", 0.60, exact_state(3)),
    };
    duplicate.oracle = FixtureOracle{};
    duplicate.oracle.set(
        duplicate.context.exact_entry, "transmute",
        duplicate_expansion);
    check_refusal(duplicate, "duplicate_physical_outcome");

    Fixture exits = deterministic_fixture();
    const FragmentConditionV1 left_condition =
        condition("has_mod_group", {{"group", "left"}});
    const FragmentConditionV1 right_condition =
        condition("has_mod_group", {{"group", "right"}});
    exits.ir.nodes.front().edges = {
        edge("left", "left-exit", left_condition, 0),
        edge("right", "right-exit", right_condition, 1),
        edge("default", "failure", condition("always"), 2, true),
    };
    exits.ir.nodes.insert(
        exits.ir.nodes.end() - 1,
        exit_node("left-exit", FragmentExitKindV1::Subgoal, "left"));
    exits.ir.nodes.insert(
        exits.ir.nodes.end() - 1,
        exit_node("right-exit", FragmentExitKindV1::Recoverable, "right"));
    exits.oracle = FixtureOracle{};
    exits.oracle.set(
        exits.context.exact_entry, "transmute",
        expansion({
            physical(
                "left", 0.5,
                exact_state(10, "same-carrier", {left_condition})),
            physical(
                "right", 0.5,
                exact_state(11, "same-carrier", {right_condition})),
        }));
    const auto exit_result = ExactLeafFragmentVerifierV1{}.verify(
        exits.ir, exits.context, exits.oracle);
    PC_CHECK(exit_result.ok());
    PC_CHECK(exit_result.verified->exits().size() == 2);
    PC_CHECK(
        exit_result.verified->exits()[0].identity.exact_item_key !=
        exit_result.verified->exits()[1].identity.exact_item_key);
    const auto rejected_nonfinal = SingleFragmentFlattenerV1{}.flatten(
        exit_result.verified->structural_control());
    PC_CHECK(!rejected_nonfinal.ok());
    PC_CHECK(
        rejected_nonfinal.refusal.code == "non_final_positive_exit");

    Fixture final_exits = exits;
    for (FragmentNodeV1& node : final_exits.ir.nodes) {
        if (node.node_id == "left-exit" || node.node_id == "right-exit") {
            node.exit.kind = FragmentExitKindV1::FinalSuccess;
        }
    }
    const auto final_exit_result = ExactLeafFragmentVerifierV1{}.verify(
        final_exits.ir, final_exits.context, final_exits.oracle);
    PC_CHECK(final_exit_result.ok());
    PC_CHECK(
        final_exit_result.verified->structural_control()
            .positive_exit_dispositions().size() == 2);
    PC_CHECK(
        !(final_exit_result.verified->structural_control()
              .positive_exit_dispositions()[0].exact_exit_identity ==
          final_exit_result.verified->structural_control()
              .positive_exit_dispositions()[1].exact_exit_identity));
    const auto flattened_final_exits = SingleFragmentFlattenerV1{}.flatten(
        final_exit_result.verified->structural_control());
    PC_CHECK(flattened_final_exits.ok());
    PC_CHECK(
        flattened_final_exits.candidate->ordinary_strategy_json().find(
            "\"id\":\"left-exit\"") != std::string::npos);
    PC_CHECK(
        flattened_final_exits.candidate->ordinary_strategy_json().find(
            "\"id\":\"right-exit\"") != std::string::npos);

    Fixture states = deterministic_fixture();
    const FragmentConditionV1 continue_condition =
        condition("rarity_is", {{"rarity", "magic"}});
    states.ir.nodes.front().edges = {
        edge("continue", "finish", continue_condition, 0),
        edge("default", "failure", condition("always"), 1, true),
    };
    FragmentNodeV1 finish = states.ir.nodes.front();
    finish.node_id = "finish";
    finish.edges = {
        edge("success", "success", condition("always"), 0),
        edge("default", "failure", condition("always"), 1, true),
    };
    states.ir.nodes.insert(states.ir.nodes.begin() + 1, finish);
    const ExactStateV1 exact_left =
        exact_state(20, "same-carrier", {continue_condition});
    const ExactStateV1 exact_right =
        exact_state(21, "same-carrier", {continue_condition});
    states.oracle = FixtureOracle{};
    states.oracle.set(
        states.context.exact_entry, "transmute",
        expansion({
            physical("left", 0.5, exact_left),
            physical("right", 0.5, exact_right),
        }));
    states.oracle.set(
        exact_left, "transmute",
        expansion({physical("done", 1.0, exact_state(30))}));
    states.oracle.set(
        exact_right, "transmute",
        expansion({physical("done", 1.0, exact_state(30))}));
    const auto state_result = ExactLeafFragmentVerifierV1{}.verify(
        states.ir, states.context, states.oracle);
    PC_CHECK(state_result.ok());
    PC_CHECK(state_result.verified->rows().size() == 3);

    Fixture association_left = deterministic_fixture();
    const FragmentConditionV1 goal =
        condition("has_mod_group", {{"group", "goal"}});
    const ExactStateV1 goal_a = exact_state(60, {}, {goal});
    const ExactStateV1 goal_b = exact_state(61, {}, {goal});
    association_left.oracle = FixtureOracle{};
    association_left.oracle.set(
        association_left.context.exact_entry, "transmute",
        expansion({
            physical("a", 0.5, goal_a),
            physical("b", 0.5, goal_b),
        }));
    Fixture association_right = association_left;
    association_right.oracle = FixtureOracle{};
    association_right.oracle.set(
        association_right.context.exact_entry, "transmute",
        expansion({
            physical("a", 0.5, goal_b),
            physical("b", 0.5, goal_a),
        }));
    const auto association_left_result =
        ExactLeafFragmentVerifierV1{}.verify(
            association_left.ir, association_left.context,
            association_left.oracle);
    const auto association_right_result =
        ExactLeafFragmentVerifierV1{}.verify(
            association_right.ir, association_right.context,
            association_right.oracle);
    PC_CHECK(association_left_result.ok());
    PC_CHECK(association_right_result.ok());
    PC_CHECK(
        !(association_left_result.verified->certificate_identity() ==
          association_right_result.verified->certificate_identity()));
}

void properness_resource_and_refusal_tests() {
    Fixture deterministic = deterministic_fixture();
    const FragmentConditionV1 zero_condition =
        condition("has_mod_group", {{"group", "zero"}});
    deterministic.ir.nodes.front().edges.insert(
        deterministic.ir.nodes.front().edges.end() - 1,
        edge("zero", "zero-route", zero_condition, 1));
    deterministic.ir.nodes.front().edges.back().priority = 2;
    FragmentNodeV1 zero_route;
    zero_route.node_id = "zero-route";
    zero_route.kind = FragmentNodeKindV1::Route;
    zero_route.edges = {
        edge("loop", "zero-route", zero_condition, 0),
        edge("default", "failure", condition("always"), 1, true),
    };
    deterministic.ir.nodes.insert(
        deterministic.ir.nodes.end() - 1, zero_route);
    PrimitiveExpansionV1 with_zero =
        deterministic.oracle.expand_primitive(
            deterministic.context.exact_entry, "transmute");
    with_zero.authoritative_outcomes.push_back({"zero", 0.0});
    with_zero.physical_outcomes.push_back(
        physical(
            "zero", 0.0, exact_state(77, {}, {zero_condition})));
    deterministic.oracle = FixtureOracle{};
    deterministic.oracle.set(
        deterministic.context.exact_entry, "transmute", with_zero);
    const auto zero_result = ExactLeafFragmentVerifierV1{}.verify(
        deterministic.ir, deterministic.context, deterministic.oracle);
    PC_CHECK(zero_result.ok());
    PC_CHECK(
        zero_result.verified->rows().front().physical_outcomes.size() == 2);
    PC_CHECK(zero_result.verified->rows().size() == 1);
    PC_CHECK(std::fabs(zero_result.verified->exit_probability_sum() - 1.0) <
             1e-12);

    Fixture cyclic = cyclic_fixture();
    const auto first = ExactLeafFragmentVerifierV1{}.verify(
        cyclic.ir, cyclic.context, cyclic.oracle);
    const auto repeat = ExactLeafFragmentVerifierV1{}.verify(
        cyclic.ir, cyclic.context, cyclic.oracle);
    PC_CHECK(first.ok());
    PC_CHECK(repeat.ok());
    if (!first.ok() || !repeat.ok()) {
        std::printf(
            "cyclic refusal: first=%s repeat=%s\n",
            first.refusal.code.c_str(), repeat.refusal.code.c_str());
        return;
    }
    PC_CHECK(first.verified->certificate_identity() ==
             repeat.verified->certificate_identity());
    PC_CHECK(first.verified->strongly_connected_components() >= 1);
    PC_CHECK(
        first.verified->positive_probability_cyclic_components() == 1);
    PC_CHECK(std::fabs(first.verified->exit_probability_sum() - 1.0) <
             1e-12);
    PC_CHECK(first.verified->max_probability_mass_error() < 1e-12);
    PC_CHECK(first.verified->max_resource_residual() < 1e-10);
    PC_CHECK(first.verified->expected_action_counts().size() == 1);
    PC_CHECK(std::fabs(
        first.verified->expected_action_counts().front().second - 2.5) <
        1e-10);
    PC_CHECK(first.verified->expected_resources().size() == 1);
    PC_CHECK(std::fabs(
        first.verified->expected_resources().front().second - 2.5) <
        1e-10);
    PC_CHECK(first.verified->priced_expected_cost().has_value());
    PC_CHECK(std::fabs(*first.verified->priced_expected_cost() - 5.0) <
             1e-10);
    PC_CHECK(first.verified->exits().size() == 1);
    PC_CHECK(std::fabs(
        first.verified->exits().front()
                .joint_resource_mass_from_entry.front().second -
            2.5) < 1e-10);

    Fixture missing_price = cyclic_fixture();
    missing_price.context.prices.clear();
    const auto missing_price_result = ExactLeafFragmentVerifierV1{}.verify(
        missing_price.ir, missing_price.context, missing_price.oracle);
    PC_CHECK(missing_price_result.ok());
    PC_CHECK(!missing_price_result.verified->priced_expected_cost());

    Fixture invalid_price = cyclic_fixture();
    invalid_price.context.prices["transmute"] =
        std::numeric_limits<double>::quiet_NaN();
    const auto invalid_price_result = ExactLeafFragmentVerifierV1{}.verify(
        invalid_price.ir, invalid_price.context, invalid_price.oracle);
    PC_CHECK(invalid_price_result.ok());
    PC_CHECK(!invalid_price_result.verified->priced_expected_cost());

    const auto flattened_first = SingleFragmentFlattenerV1{}.flatten(
        first.verified->structural_control());
    const auto flattened_missing_price =
        SingleFragmentFlattenerV1{}.flatten(
            missing_price_result.verified->structural_control());
    const auto flattened_invalid_price =
        SingleFragmentFlattenerV1{}.flatten(
            invalid_price_result.verified->structural_control());
    PC_CHECK(flattened_first.ok());
    PC_CHECK(flattened_missing_price.ok());
    PC_CHECK(flattened_invalid_price.ok());
    PC_CHECK(
        flattened_first.candidate->ordinary_strategy_json() ==
        flattened_missing_price.candidate->ordinary_strategy_json());
    PC_CHECK(
        flattened_first.candidate->ordinary_strategy_json() ==
        flattened_invalid_price.candidate->ordinary_strategy_json());
    PC_CHECK(
        flattened_first.candidate->candidate_identity() ==
        flattened_invalid_price.candidate->candidate_identity());

    for (const FragmentExitKindV1 nonfinal : {
             FragmentExitKindV1::Subgoal,
             FragmentExitKindV1::Recoverable,
             FragmentExitKindV1::CertificationFailure}) {
        Fixture positive_nonfinal = deterministic_fixture();
        for (FragmentNodeV1& node : positive_nonfinal.ir.nodes) {
            if (node.node_id == "success") node.exit.kind = nonfinal;
        }
        const auto positive_nonfinal_result =
            ExactLeafFragmentVerifierV1{}.verify(
                positive_nonfinal.ir, positive_nonfinal.context,
                positive_nonfinal.oracle);
        PC_CHECK(positive_nonfinal_result.ok());
        const auto rejected_nonfinal =
            SingleFragmentFlattenerV1{}.flatten(
                positive_nonfinal_result.verified->structural_control());
        PC_CHECK(!rejected_nonfinal.ok());
        PC_CHECK(
            rejected_nonfinal.refusal.code == "non_final_positive_exit");
    }

    Fixture improper = cyclic_fixture(0.0);
    check_refusal(improper, "improper_closed_nonexit_scc");
    const auto improper_result = ExactLeafFragmentVerifierV1{}.verify(
        improper.ir, improper.context, improper.oracle);
    PC_CHECK(!improper_result.refusal.canonical_component_witness.empty());

    Fixture illegal = deterministic_fixture();
    PrimitiveExpansionV1 illegal_expansion;
    illegal_expansion.legal = false;
    illegal.oracle = FixtureOracle{};
    illegal.oracle.set(
        illegal.context.exact_entry, "transmute", illegal_expansion);
    check_refusal(illegal, "illegal_action");

    Fixture unexpressible = deterministic_fixture();
    unexpressible.oracle.unexpressible_conditions.insert(
        canonical_fragment_condition_identity_v1(
            unexpressible.ir.nodes.front().edges.front().condition)
            .canonical_bytes);
    check_refusal(unexpressible, "unexpressible_predicate");

    Fixture observed = deterministic_fixture();
    observed.context.exact_entry = exact_state(1, {}, {}, {"mod:a", "mod:b"});
    observed.ir.exact_entry_product_state_identity =
        canonical_exact_entry_identity_v1(
            observed.context.exact_entry,
            observed.context.initial_controller_memory);
    FragmentNodeV1 choice;
    choice.node_id = "choice";
    choice.kind = FragmentNodeKindV1::ObservedChoice;
    choice.observed_choice_order = {"mod:a", "mod:b"};
    choice.edges = {
        edge(
            "a", "success",
            condition("has_unveil_option", {{"mod_key", "mod:a"}}), 0),
        edge(
            "b", "other",
            condition("has_unveil_option", {{"mod_key", "mod:b"}}), 1),
        edge("default", "failure", condition("always"), 2, true),
    };
    observed.ir.entry_node_id = "choice";
    observed.ir.nodes.insert(
        observed.ir.nodes.begin(), choice);
    observed.ir.nodes.insert(
        observed.ir.nodes.end() - 1,
        exit_node("other", FragmentExitKindV1::Subgoal, "other"));
    observed.oracle = FixtureOracle{};
    const auto observed_result = ExactLeafFragmentVerifierV1{}.verify(
        observed.ir, observed.context, observed.oracle);
    PC_CHECK(observed_result.ok());
    PC_CHECK(observed_result.verified->exits().size() == 1);
    PC_CHECK(
        observed_result.verified->exits().front().identity.descriptor.kind ==
        FragmentExitKindV1::FinalSuccess);

    Fixture observed_reverse = observed;
    FragmentNodeV1& reversed_choice = observed_reverse.ir.nodes.front();
    reversed_choice.observed_choice_order = {"mod:b", "mod:a"};
    reversed_choice.edges[0].priority = 1;
    reversed_choice.edges[1].priority = 0;
    const auto reverse_result = ExactLeafFragmentVerifierV1{}.verify(
        observed_reverse.ir, observed_reverse.context,
        observed_reverse.oracle);
    PC_CHECK(reverse_result.ok());
    PC_CHECK(
        reverse_result.verified->exits().front().identity.descriptor.kind ==
        FragmentExitKindV1::Subgoal);

    Fixture observed_mismatch = observed;
    observed_mismatch.ir.nodes.front().observed_choice_order = {
        "mod:b", "mod:a"};
    PC_CHECK(
        validate_executable_fragment_ir_v1(observed_mismatch.ir)
            .refusal.code == "observed_choice_order_mismatch");

    LeafVerificationLimitsV1 limits;
    limits.max_states = 0;
    check_refusal(cyclic, "max_states", limits);
    limits = {};
    limits.max_transitions = 0;
    check_refusal(cyclic, "max_transitions", limits);
    limits = {};
    limits.max_estimated_bytes = 1;
    check_refusal(cyclic, "max_estimated_bytes", limits);
    limits = {};
    limits.max_work_items = 0;
    check_refusal(cyclic, "max_work_items", limits);
    limits = {};
    limits.max_work_items = 9;
    check_refusal(cyclic, "max_work_items", limits);
    limits = {};
    limits.canceled = [] { return true; };
    check_refusal(cyclic, "canceled", limits);
    limits = {};
    limits.deadline = std::chrono::steady_clock::now();
    check_refusal(cyclic, "time_limit", limits);
    limits = {};
    limits.probability_sum_tolerance = 1e-6;
    check_refusal(
        cyclic, "unsupported_tolerance_configuration", limits);
}

void incumbent_isolation_tests() {
    using Impl = poecraft::solver::SolveWorkTestAccess::Impl;
    Impl::IncumbentPortfolio portfolio;
    Impl::BoundedPolicyIncumbent seeded;
    seeded.evaluated_policy_cost = 10.0;
    seeded.certified_upper_bound = 10.0;
    seeded.portfolio_identity = 0xfeedbeef;
    seeded.source_generation = 7;
    seeded.independently_certified = true;
    seeded.independently_evaluated = true;
    seeded.proper = true;
    seeded.executable = true;
    portfolio.observe_verified(seeded);

    const Fixture cheaper = cyclic_fixture();
    const auto verified = ExactLeafFragmentVerifierV1{}.verify(
        cheaper.ir, cheaper.context, cheaper.oracle);
    PC_CHECK(verified.ok());
    const auto flattened = SingleFragmentFlattenerV1{}.flatten(
        verified.verified->structural_control());
    PC_CHECK(flattened.ok());
    PC_CHECK(*verified.verified->priced_expected_cost() < 10.0);

    Fixture malformed = cheaper;
    malformed.ir.nodes.front().edges.pop_back();
    const auto malformed_result = ExactLeafFragmentVerifierV1{}.verify(
        malformed.ir, malformed.context, malformed.oracle);
    PC_CHECK(!malformed_result.ok());
    Fixture improper = cyclic_fixture(0.0);
    const auto improper_result = ExactLeafFragmentVerifierV1{}.verify(
        improper.ir, improper.context, improper.oracle);
    PC_CHECK(!improper_result.ok());

    PC_CHECK(portfolio.verified_executable_upper() == 10.0);
    PC_CHECK(portfolio.best_verified_identity == 0xfeedbeef);
    PC_CHECK(portfolio.verified_replacements == 0);
}

double resource_value(
        const ResourceVectorV1& resources,
        const std::string& key) {
    const auto found = std::find_if(
        resources.begin(), resources.end(),
        [&](const auto& value) { return value.first == key; });
    return found == resources.end()
        ? std::numeric_limits<double>::quiet_NaN()
        : found->second;
}

void engine_backed_renewal_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver fragment engine fixture skipped (no artifact)\n");
        return;
    }
    const auto first_build =
        build_clean_one_goal_transmute_scour_renewal_v1(artifact_dir);
    PC_CHECK(first_build.ok());
    if (!first_build.ok()) {
        std::printf(
            "solver fragment engine fixture: %s\n",
            first_build.refusal.c_str());
        return;
    }
    const EngineBackedRenewalFixtureV1& first_fixture =
        *first_build.fixture;
    PC_CHECK(first_fixture.exact_terminal_probability > 0.0);
    PC_CHECK(first_fixture.exact_terminal_probability < 1.0);
    PC_CHECK(first_fixture.exact_goal_plus_junk_probability > 0.0);
    PC_CHECK(first_fixture.exact_other_nonterminal_probability > 0.0);
    PC_CHECK(first_fixture.transmute_physical_outcomes > 2);

    LeafVerificationLimitsV1 limits;
    limits.max_states = 20000;
    limits.max_transitions = 1000000;
    limits.max_work_items = 1000000000;
    limits.max_estimated_bytes = 1024ull * 1024ull * 1024ull;
    const auto first = ExactLeafFragmentVerifierV1{}.verify(
        first_fixture.ir, first_fixture.context,
        *first_fixture.oracle, limits);
    PC_CHECK(first.ok());
    if (!first.ok()) {
        std::printf(
            "solver fragment renewal refusal: %s (%s), outcomes=%llu, "
            "p=%.17g\n",
            first.refusal.code.c_str(), first.refusal.witness.c_str(),
            static_cast<unsigned long long>(
                first_fixture.transmute_physical_outcomes),
            first_fixture.exact_terminal_probability);
        return;
    }

    const auto second_build =
        build_clean_one_goal_transmute_scour_renewal_v1(artifact_dir);
    PC_CHECK(second_build.ok());
    if (!second_build.ok()) return;
    const EngineBackedRenewalFixtureV1& second_fixture =
        *second_build.fixture;
    const auto second = ExactLeafFragmentVerifierV1{}.verify(
        second_fixture.ir, second_fixture.context,
        *second_fixture.oracle, limits);
    PC_CHECK(second.ok());
    if (!second.ok()) return;

    const double p = first_fixture.exact_terminal_probability;
    const double expected_transmute = 1.0 / p;
    const double expected_scour = (1.0 - p) / p;
    PC_CHECK(std::fabs(
        resource_value(
            first.verified->expected_resources(), "transmute") -
        expected_transmute) < 1e-9);
    PC_CHECK(std::fabs(
        resource_value(first.verified->expected_resources(), "scour") -
        expected_scour) < 1e-9);
    PC_CHECK(std::fabs(first.verified->exit_probability_sum() - 1.0) <
             kExecutableFragmentProbabilityToleranceV1);
    PC_CHECK(first.verified->positive_probability_cyclic_components() >= 1);
    PC_CHECK(first.verified->exits().size() == 1);
    PC_CHECK(
        first.verified->exits().front().identity.descriptor.kind ==
        FragmentExitKindV1::FinalSuccess);
    PC_CHECK(
        first.verified->certificate_identity() ==
        second.verified->certificate_identity());
    PC_CHECK(first.verified->ir_identity() == second.verified->ir_identity());
    PC_CHECK(
        first.verified->expected_resources() ==
        second.verified->expected_resources());
    PC_CHECK(
        first.verified->expected_action_counts() ==
        second.verified->expected_action_counts());
    PC_CHECK(
        first.verified->rows().size() == second.verified->rows().size());
    PC_CHECK(
        first.verified->exits().size() == second.verified->exits().size());
    PC_CHECK(
        first_fixture.forward_reference_identity ==
        second_fixture.forward_reference_identity);
    PC_CHECK(first_fixture.base_identity == second_fixture.base_identity);
    PC_CHECK(first_fixture.goal_identity == second_fixture.goal_identity);

    const auto flattened = SingleFragmentFlattenerV1{}.flatten(
        first.verified->structural_control());
    const auto flattened_repeat = SingleFragmentFlattenerV1{}.flatten(
        second.verified->structural_control());
    PC_CHECK(flattened.ok());
    PC_CHECK(flattened_repeat.ok());
    if (!flattened.ok() || !flattened_repeat.ok()) return;
    PC_CHECK(
        flattened.candidate->ordinary_strategy_json() ==
        flattened_repeat.candidate->ordinary_strategy_json());
    PC_CHECK(
        flattened.candidate->candidate_identity() ==
        flattened_repeat.candidate->candidate_identity());
    PC_CHECK(
        flattened.candidate->ordinary_strategy_json().find(
            "\"expected_cost\"") == std::string::npos);
    PC_CHECK(
        flattened.candidate->ordinary_strategy_json().find(
            "\"probability\"") == std::string::npos);
    PC_CHECK(
        flattened.candidate->ordinary_strategy_json().find(
            "\"is_default\":true") != std::string::npos);

    EngineBackedFragmentEvaluationLimitsV1 evaluation_limits;
    evaluation_limits.simulator_runs = 10000;
    const auto evaluated = EngineBackedFragmentEvaluatorV1{}.evaluate(
        *flattened.candidate, artifact_dir, {}, evaluation_limits);
    PC_CHECK(evaluated.ok());
    if (!evaluated.ok()) {
        std::printf(
            "solver fragment independent evaluation refusal: %s\n",
            evaluated.refusal.c_str());
        return;
    }
    const IndependentFragmentEvaluationV1& evaluation =
        *evaluated.evaluation;
    PC_CHECK(evaluation.converged);
    PC_CHECK(evaluation.proper);
    PC_CHECK(evaluation.cost_complete);
    PC_CHECK(evaluation.cost_reconciled);
    PC_CHECK(std::fabs(evaluation.success_probability - 1.0) < 1e-12);
    PC_CHECK(evaluation.failure_probability == 0.0);
    PC_CHECK(evaluation.stop_probability == 0.0);
    PC_CHECK(evaluation.action_not_applied_probability == 0.0);
    PC_CHECK(evaluation.no_matching_edge_probability == 0.0);
    PC_CHECK(evaluation.unresolved_probability == 0.0);
    PC_CHECK(evaluation.maximum_mass_error < 1e-10);
    PC_CHECK(evaluation.forward_maximum_delta < 1e-9);
    PC_CHECK(std::fabs(
        evaluation.expected_actions -
        (expected_transmute + expected_scour)) < 1e-9);
    PC_CHECK(std::fabs(
        evaluation.expected_consumption.at("transmute") -
        expected_transmute) < 1e-9);
    PC_CHECK(std::fabs(
        evaluation.expected_consumption.at("scour") -
        expected_scour) < 1e-9);
    const double expected_cost =
        0.05 * (expected_transmute + expected_scour);
    PC_CHECK(std::fabs(
        evaluation.total_expected_cost - expected_cost) < 1e-9);
    PC_CHECK(evaluation.compiled_nodes == 5);
    PC_CHECK(evaluation.compiled_edges == 6);
    PC_CHECK(
        evaluated.candidate->independently_evaluated_candidate_cost()
            .has_value());
    PC_CHECK(std::fabs(
        *evaluated.candidate->independently_evaluated_candidate_cost() -
        expected_cost) < 1e-9);
    PC_CHECK(evaluation.simulator_completed_runs == 10000);
    PC_CHECK(evaluation.simulator_success_count == 10000);
    PC_CHECK(evaluation.simulator_failure_count == 0);
    PC_CHECK(evaluation.simulator_stop_count == 0);
    PC_CHECK(evaluation.simulator_action_limit_count == 0);
    PC_CHECK(evaluation.simulator_cost_limit_count == 0);
    PC_CHECK(evaluation.simulator_step_limit_count == 0);
    PC_CHECK(evaluation.simulator_action_not_applied_count == 0);
    PC_CHECK(evaluation.simulator_no_matching_edge_count == 0);
    PC_CHECK(evaluation.simulator_missing_price_run_count == 0);
    PC_CHECK(evaluation.simulator_missing_price_action_count == 0);

    std::uint64_t transitions = 0;
    for (const VerifiedProductRowV1& row : first.verified->rows()) {
        transitions += row.transitions.size();
    }
    std::printf(
        "solver fragment renewal: outcomes=%llu rows=%llu transitions=%llu "
        "scc=%u cyclic_scc=%u exit_mass=%.17g mass_error=%.17g "
        "residual=%.17g p=%.17g transmute=%.17g scour=%.17g "
        "ir=%016llx certificate=%016llx flat=%016llx json=%llu "
        "nodes=%llu edges=%llu cost=%.17g eval_mass_error=%.17g "
        "forward_delta=%.17g work=%llu bytes=%llu sim=%llu/%llu "
        "sim_fail=%llu sim_stop=%llu sim_limit=%llu "
        "sim_inapplicable=%llu sim_missing_edge=%llu sim_missing_price=%llu\n",
        static_cast<unsigned long long>(
            first_fixture.transmute_physical_outcomes),
        static_cast<unsigned long long>(first.verified->rows().size()),
        static_cast<unsigned long long>(transitions),
        first.verified->strongly_connected_components(),
        first.verified->positive_probability_cyclic_components(),
        first.verified->exit_probability_sum(),
        first.verified->max_probability_mass_error(),
        first.verified->max_resource_residual(), p, expected_transmute,
        expected_scour,
        static_cast<unsigned long long>(
            first.verified->ir_identity().digest),
        static_cast<unsigned long long>(
            first.verified->certificate_identity().digest),
        static_cast<unsigned long long>(
            evaluated.candidate->candidate_identity().digest),
        static_cast<unsigned long long>(evaluation.strategy_json_bytes),
        static_cast<unsigned long long>(evaluation.compiled_nodes),
        static_cast<unsigned long long>(evaluation.compiled_edges),
        evaluation.total_expected_cost,
        evaluation.maximum_mass_error,
        evaluation.forward_maximum_delta,
        static_cast<unsigned long long>(first.verified->work_items()),
        static_cast<unsigned long long>(
            first.verified->peak_estimated_bytes()),
        static_cast<unsigned long long>(
            evaluation.simulator_success_count),
        static_cast<unsigned long long>(
            evaluation.simulator_completed_runs),
        static_cast<unsigned long long>(
            evaluation.simulator_failure_count),
        static_cast<unsigned long long>(
            evaluation.simulator_stop_count),
        static_cast<unsigned long long>(
            evaluation.simulator_action_limit_count +
            evaluation.simulator_cost_limit_count +
            evaluation.simulator_step_limit_count),
        static_cast<unsigned long long>(
            evaluation.simulator_action_not_applied_count),
        static_cast<unsigned long long>(
            evaluation.simulator_no_matching_edge_count),
        static_cast<unsigned long long>(
            evaluation.simulator_missing_price_run_count));
}

} // namespace

void run_solver_fragment_tests(const char* artifact_dir) {
    authority_and_identity_tests();
    malformed_ir_tests();
    mass_and_projection_regression_tests();
    properness_resource_and_refusal_tests();
    incumbent_isolation_tests();
    engine_backed_renewal_tests(artifact_dir);
}
