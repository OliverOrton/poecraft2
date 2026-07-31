#include "../src/solver_refinement.hpp"
#include "tests.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using namespace poecraft::solver;
using namespace poecraft::solver::refinement;
using poecraft::SessionImpl;

#define REFINE_CHECK(condition) PC_CHECK(condition)

FeatureAtom affix_atom(
        const RefinementFeature feature,
        const std::uint32_t subject,
        std::initializer_list<std::uint64_t> value,
        const std::uint16_t traits = kRefinementAffixPrefix) {
    return {
        feature, subject, StableKey{value}, traits, 0, {}};
}

ExactState state(
        const std::uint64_t key,
        const std::uint32_t coarse,
        FeatureSignature features = {},
        const bool goal = false) {
    ExactState out{
        {key}, coarse, std::move(features), goal, goal, {}};
    out.coarse_state_key = {
        0x74657374636f6172ull, /* "testcoar" */
        coarse};
    return out;
}

RefinementAffixObservation observe_all(
        const RefinementFeature feature) {
    return {refinement_feature(feature), {}};
}

ActionRefinementContract action_contract(
        std::vector<RefinementAffixObservation> observes = {},
        const bool preserves_affixes = true,
        const bool destroys_affixes = false) {
    ActionRefinementContract contract;
    contract.schema_version = kActionRefinementContractVersion;
    contract.preserved_item_features = kAllRefinementItemFeatures;
    contract.affix_observations = std::move(observes);
    if (preserves_affixes) contract.preserved_affixes.push_back({});
    if (destroys_affixes) contract.destroyed_affixes.push_back({});
    return contract;
}

class FixtureOracle final : public PolicyRefinementOracle {
public:
    std::map<std::uint32_t, std::vector<ExactState>> refinements;
    std::map<StableKey, SelectedAction> selections;
    std::map<StableKey, ExactActionKernel> kernels;
    bool reverse_enumeration = false;
    std::vector<std::uint32_t> enumeration_calls;

    std::vector<ExactState> enumerate_refinements(
            const std::uint32_t coarse_state) override {
        enumeration_calls.push_back(coarse_state);
        std::vector<ExactState> out = refinements.at(coarse_state);
        if (reverse_enumeration) {
            std::reverse(out.begin(), out.end());
        }
        return out;
    }

    std::optional<SelectedAction> selected_action(
            const ExactState& exact) override {
        const auto found = selections.find(exact.stable_key);
        if (found == selections.end()) return std::nullopt;
        return found->second;
    }

    ExactActionKernel exact_kernel(
            const ExactState& exact,
            const SelectedAction&) override {
        return kernels.at(exact.stable_key);
    }
};

void select(
        FixtureOracle& oracle,
        const ExactState& source,
        const std::uint32_t action_id,
        const std::uint64_t semantic_key,
        const ActionRefinementContract& contract,
        const double cost,
        std::vector<ExactTransition> transitions) {
    oracle.selections[source.stable_key] = {
        action_id, {semantic_key}, contract, {}, {}, {}};
    oracle.kernels[source.stable_key] = {
        cost, std::move(transitions)};
}

std::uint32_t assigned_class(
        const RefinementResult& result,
        const std::uint64_t state_key) {
    for (const StateClassAssignment& assignment : result.assignments) {
        if (assignment.exact_state == StableKey{state_key}) {
            return assignment.class_id;
        }
    }
    throw std::logic_error("missing assignment");
}

double evaluated_class_value(
        const PolicyEvaluationResult& result,
        const std::uint32_t class_id) {
    for (const RefinedClassValue& value : result.class_values) {
        if (value.class_id == class_id) return value.value;
    }
    throw std::logic_error("missing evaluated class");
}

std::string fingerprint(const RefinementResult& result) {
    std::ostringstream out;
    out << static_cast<int>(result.status) << ':'
        << result.telemetry.exact_states << ':'
        << result.telemetry.final_refinement_classes << ';';
    for (const StateClassAssignment& assignment : result.assignments) {
        out << assignment.coarse_state << ':'
            << assignment.exact_state.front() << '='
            << assignment.class_id << ';';
    }
    for (const RefinedPolicyClass& refined : result.classes) {
        out << 'c' << refined.class_id << '@'
            << refined.coarse_state << ':';
        for (const ProjectedTransition& transition :
             refined.transitions) {
            out << transition.successor_class << '='
                << transition.probability << ',';
        }
        out << ';';
    }
    return out.str();
}

std::string semantic_parent_fingerprint(
        const RefinementResult& result) {
    const auto append_key = [](std::ostringstream& out,
                               const StableKey& key) {
        out << '[';
        for (const std::uint64_t token : key) out << token << ',';
        out << ']';
    };
    std::ostringstream out;
    out << static_cast<int>(result.status) << ':'
        << result.telemetry.exact_states << ':'
        << result.telemetry.final_refinement_classes << ';';
    for (const StateClassAssignment& assignment : result.assignments) {
        append_key(out, assignment.exact_state);
        out << '@';
        append_key(out, assignment.coarse_state_key);
        out << '=' << assignment.class_id << ';';
    }
    for (const RefinedPolicyClass& refined : result.classes) {
        out << 'c' << refined.class_id << '@';
        append_key(out, refined.coarse_state_key);
        out << ':';
        for (const StableKey& member : refined.exact_members) {
            append_key(out, member);
        }
        out << ':';
        for (const ProjectedTransition& transition :
             refined.transitions) {
            out << transition.successor_class << '='
                << transition.probability << ',';
        }
        out << ';';
    }
    return out.str();
}

void remap_numeric_coarse_states(
        FixtureOracle& oracle,
        const std::map<std::uint32_t, std::uint32_t>& remap) {
    std::map<std::uint32_t, std::vector<ExactState>> refinements;
    for (auto& [coarse, states] : oracle.refinements) {
        for (ExactState& exact : states) {
            exact.coarse_state = remap.at(exact.coarse_state);
        }
        refinements.emplace(remap.at(coarse), std::move(states));
    }
    oracle.refinements = std::move(refinements);
    for (auto& [_, kernel] : oracle.kernels) {
        for (ExactTransition& transition : kernel.transitions) {
            transition.successor.coarse_state =
                remap.at(transition.successor.coarse_state);
        }
    }
}

void replace_semantic_coarse_key(
        FixtureOracle& oracle,
        const StableKey& exact_key,
        StableKey coarse_key) {
    for (auto& [_, states] : oracle.refinements) {
        for (ExactState& exact : states) {
            if (exact.stable_key == exact_key) {
                exact.coarse_state_key = coarse_key;
            }
        }
    }
    for (auto& [_, kernel] : oracle.kernels) {
        for (ExactTransition& transition : kernel.transitions) {
            if (transition.successor.stable_key == exact_key) {
                transition.successor.coarse_state_key = coarse_key;
            }
        }
    }
}

ClosedPartitionNode partition_node(
        const std::uint64_t stable_key,
        const std::uint64_t observation_key,
        const std::uint64_t immediate_key,
        const bool terminal = false) {
    return {
        {stable_key},
        {observation_key},
        {immediate_key},
        terminal,
        {}};
}

void add_partition_arc(
        std::vector<ClosedPartitionNode>& nodes,
        const std::uint32_t source,
        StableKey label,
        const std::optional<std::uint32_t> successor,
        const double probability) {
    nodes.at(source).arcs.push_back({
        std::move(label), successor, probability});
}

std::uint32_t partition_class(
        const ClosedPartitionResult& result,
        const std::uint64_t stable_key) {
    for (const ClosedPartitionClass& partition_class :
         result.classes) {
        if (std::find(
                partition_class.member_keys.begin(),
                partition_class.member_keys.end(),
                StableKey{stable_key}) !=
            partition_class.member_keys.end()) {
            return partition_class.class_id;
        }
    }
    throw std::logic_error("missing closed partition member");
}

std::vector<ClosedPartitionNode> permute_partition_nodes(
        const std::vector<ClosedPartitionNode>& nodes,
        const std::vector<std::uint32_t>& new_to_old) {
    REFINE_CHECK(new_to_old.size() == nodes.size());
    std::vector<std::uint32_t> old_to_new(nodes.size());
    for (std::uint32_t next = 0;
         next < new_to_old.size(); ++next) {
        REFINE_CHECK(new_to_old[next] < nodes.size());
        old_to_new[new_to_old[next]] = next;
    }
    std::vector<ClosedPartitionNode> permuted;
    permuted.reserve(nodes.size());
    for (const std::uint32_t old : new_to_old) {
        ClosedPartitionNode node = nodes[old];
        for (ClosedPartitionArc& arc : node.arcs) {
            if (arc.successor.has_value()) {
                arc.successor = old_to_new[*arc.successor];
            }
        }
        permuted.push_back(std::move(node));
    }
    return permuted;
}

FixtureOracle make_preservation_fixture(
        const bool equivalent_exclusion) {
    FixtureOracle oracle;
    const std::uint64_t right_exclusion =
        equivalent_exclusion ? 100 : 101;
    const ExactState a = state(
        10, 0,
        {affix_atom(
            RefinementFeature::ModifierExclusionSignature,
            10, {100})});
    const ExactState b = state(
        11, 0,
        {affix_atom(
            RefinementFeature::ModifierExclusionSignature,
            11, {right_exclusion})});
    const ExactState c = state(
        20, 1,
        {affix_atom(
            RefinementFeature::ModifierExclusionSignature,
            20, {100})});
    const ExactState d = state(
        21, 1,
        {affix_atom(
            RefinementFeature::ModifierExclusionSignature,
            21, {right_exclusion})});
    const ExactState goal_left = state(
        30, 2,
        {affix_atom(
            RefinementFeature::ModifierExclusionSignature,
            30, {700})},
        true);
    const ExactState goal_right = state(
        31, 2,
        {affix_atom(
            RefinementFeature::ModifierExclusionSignature,
            31, {800})},
        true);
    oracle.refinements[0] = {a, b};
    oracle.refinements[1] = {c, d};
    oracle.refinements[2] = {goal_left, goal_right};

    const ActionRefinementContract preserve =
        action_contract({}, true, false);
    const ActionRefinementContract consume = action_contract(
        {observe_all(
            RefinementFeature::ModifierExclusionSignature)},
        false, true);
    select(oracle, a, 1, 1000, preserve, 1.0, {{c, 1.0}});
    select(oracle, b, 1, 1000, preserve, 1.0, {{d, 1.0}});
    select(
        oracle, c, 2, 2000, consume, 2.0,
        {{goal_left, 1.0}});
    select(
        oracle, d, 2, 2000, consume, 2.0,
        {{goal_right, 1.0}});
    return oracle;
}

RefinementRequest request() {
    RefinementRequest value;
    value.coarse_roots = {0};
    return value;
}

std::vector<std::uint64_t> mod_mask(
        std::initializer_list<std::uint32_t> mods) {
    std::vector<std::uint64_t> mask(1, 0);
    for (const std::uint32_t mod : mods) {
        mask[0] |= std::uint64_t{1} << mod;
    }
    return mask;
}

struct AbstractAdapterFixture {
    SessionImpl session;
    AbstractLayout layout;
    AbstractState state;
    SelectedAction action;
};

AbstractAdapterFixture make_abstract_adapter_fixture() {
    AbstractAdapterFixture fixture;
    SessionImpl& session = fixture.session;
    session.mod_count = 4;
    session.words = 1;
    session.gen_type = {PC_SIDE_PREFIX, PC_SIDE_PREFIX,
                        PC_SIDE_SUFFIX, PC_SIDE_SUFFIX};
    session.required_level = {10, 10, 20, 20};
    session.group_offsets = {0, 1, 2, 3, 4};
    session.group_ids = {0, 0, 1, 1};
    session.group_masks = {mod_mask({0, 1}), mod_mask({2, 3})};
    session.class_offsets = {0, 1, 2, 3, 4};
    session.class_tag_ids = {5, 5, 6, 6};
    session.veiled_template_mask = mod_mask({});
    session.metamod_type = {-1, -1, 7, 7};

    CountObservation count;
    count.ids = {100};
    count.member_mask = mod_mask({0, 1});
    fixture.layout.count_observations.push_back(count);

    ResolvedGoalSlot goal;
    goal.member_mask = mod_mask({0, 1});
    goal.satisfying_mask = mod_mask({0, 1});
    GoalMemberClass goal_class;
    goal_class.status = GoalSlotStatus::Satisfied;
    goal_class.gen_type = PC_SIDE_PREFIX;
    goal_class.exclusion_effect_mask = mod_mask({0, 1});
    goal_class.count_observation_bits = {1};
    goal_class.member_mask = mod_mask({0, 1});
    goal_class.member_count = 2;
    goal.member_classes.push_back(goal_class);
    fixture.layout.slots.push_back(goal);

    JunkClass junk;
    junk.gen_type = PC_SIDE_SUFFIX;
    junk.exclusion_effect_mask = mod_mask({2, 3});
    junk.count_observation_bits = {0};
    junk.member_mask = mod_mask({2, 3});
    junk.member_count = 2;
    fixture.layout.junk_classes.push_back(junk);

    AbstractState& state = fixture.state;
    state.slot_status[0] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    state.goal_member_class_tokens[0] = 1;
    state.crafted_goal_mask = 1;
    state.prefix_count = 1;
    state.suffix_count = 2;
    state.rarity = PC_RARITY_RARE;
    state.influence_bits = 3;
    state.searing_exarch_tier = 2;
    state.eater_of_worlds_tier = 1;
    state.flags = kFlagCraftedMod | kFlagFractured |
                  kFlagSuffixesLocked | kFlagInfluenced |
                  kFlagEldritchImplicit;
    state.junk_counts.assign(1, 0);
    state.fractured_junk_counts.assign(1, 0);
    state.crafted_junk_counts.assign(1, 0);
    state.fractured_crafted_junk_counts.assign(1, 0);
    state.junk_counts[0] = 2;
    state.fractured_junk_counts[0] = 1;
    state.crafted_junk_counts[0] = 1;

    ActionRefinementContract contract;
    contract.schema_version = kActionRefinementContractVersion;
    contract.observed_item_features =
        refinement_feature(RefinementFeature::Rarity) |
        refinement_feature(RefinementFeature::PrefixCount) |
        refinement_feature(RefinementFeature::SuffixCount) |
        refinement_feature(RefinementFeature::PrefixLock) |
        refinement_feature(RefinementFeature::SuffixLock) |
        refinement_feature(RefinementFeature::Influence) |
        refinement_feature(RefinementFeature::SearingExarchTier) |
        refinement_feature(RefinementFeature::EaterOfWorldsTier) |
        refinement_feature(RefinementFeature::EldritchDominance);
    contract.observed_modifier_tag_ids = {5, 6};
    contract.affix_observations = {{
        refinement_feature(RefinementFeature::ModifierSide) |
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature) |
        refinement_feature(
            RefinementFeature::GoalStatusTierClass) |
        refinement_feature(RefinementFeature::ModifierCrafted) |
        refinement_feature(RefinementFeature::ModifierFractured) |
        refinement_feature(RefinementFeature::ModifierVeiled) |
        refinement_feature(
            RefinementFeature::ModifierClassificationTags) |
        refinement_feature(
            RefinementFeature::ModifierRequiredLevel) |
        refinement_feature(
            RefinementFeature::CountObservationMembership) |
        refinement_feature(
            RefinementFeature::ModifierMetamodRole),
        {}}};
    contract.preserved_item_features = kAllRefinementItemFeatures;
    contract.preserved_affixes.push_back({});
    fixture.action = {
        77, {7700}, contract, {}, {}, {}};
    return fixture;
}

void run_unset_metamod_observation_parity_tests() {
    SessionImpl session;
    session.data = std::make_shared<poecraft::DataImpl>();
    session.mod_count = 2;
    session.words = 1;
    session.gen_type = {PC_SIDE_PREFIX, PC_SIDE_SUFFIX};
    session.required_level = {1, 1};
    session.group_offsets = {0, 1, 2};
    session.group_ids = {0, 1};
    session.group_masks = {mod_mask({0}), mod_mask({1})};
    session.class_offsets = {0, 0, 0};
    session.veiled_template_mask = mod_mask({});
    session.metamod_type = {-1, -1};

    AbstractLayout layout;
    layout.junk_class_by_mod = {0, 1};
    JunkClass prefix;
    prefix.gen_type = PC_SIDE_PREFIX;
    prefix.exclusion_effect_mask = mod_mask({0});
    prefix.member_mask = mod_mask({0});
    prefix.member_count = 1;
    layout.junk_classes.push_back(std::move(prefix));
    JunkClass suffix;
    suffix.gen_type = PC_SIDE_SUFFIX;
    suffix.exclusion_effect_mask = mod_mask({1});
    suffix.member_mask = mod_mask({1});
    suffix.member_count = 1;
    layout.junk_classes.push_back(std::move(suffix));

    pc_item_state item{};
    REFINE_CHECK(pc_item_clear(&item) == PC_RESULT_OK);
    item.rarity = PC_RARITY_RARE;
    REFINE_CHECK(
        pc_item_add_mod(
            &item, PC_SIDE_PREFIX, 0, 0, 0, nullptr) ==
        PC_RESULT_OK);
    REFINE_CHECK(
        pc_item_add_mod(
            &item, PC_SIDE_SUFFIX, 1, 1, 0, nullptr) ==
        PC_RESULT_OK);
    const AbstractState state = project_item(session, layout, item);

    ObservationRequirement requirement;
    requirement.item_features =
        refinement_feature(RefinementFeature::Multimod) |
        refinement_feature(RefinementFeature::PrefixLock) |
        refinement_feature(RefinementFeature::SuffixLock) |
        refinement_feature(RefinementFeature::CannotRollAttack) |
        refinement_feature(RefinementFeature::CannotRollCaster);
    requirement.affix_observations.push_back({
        refinement_feature(RefinementFeature::ModifierSide) |
            refinement_feature(
                RefinementFeature::ModifierFractured),
        {}});
    RefinementAffixSelector locked_side;
    locked_side.required_affix_traits =
        kRefinementAffixOnLockedSide;
    requirement.affix_observations.push_back({
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature) |
            refinement_feature(
                RefinementFeature::ModifierMetamodRole),
        locked_side});

    const FeatureSignature physical = observe_exact_item_features(
        session, item, requirement, {});
    const AbstractFeatureExtraction abstract =
        extract_strict_abstract_features(
            session, layout, state, requirement);
    REFINE_CHECK(abstract.complete());
    const FeatureSignature abstract_observed =
        observe_features(abstract.features, requirement);
    REFINE_CHECK(physical == abstract_observed);
    for (const FeatureAtom& atom : physical) {
        if (atom.feature == RefinementFeature::Multimod ||
            atom.feature == RefinementFeature::PrefixLock ||
            atom.feature == RefinementFeature::SuffixLock ||
            atom.feature == RefinementFeature::CannotRollAttack ||
            atom.feature == RefinementFeature::CannotRollCaster) {
            REFINE_CHECK(atom.value == StableKey{0});
        }
        REFINE_CHECK(
            atom.feature !=
            RefinementFeature::ModifierExclusionSignature);
        REFINE_CHECK(
            atom.feature != RefinementFeature::ModifierMetamodRole);
    }
}

void run_abstract_adapter_tests() {
    AbstractAdapterFixture fixture = make_abstract_adapter_fixture();
    const ObservationRequirement requirement =
        observation_requirement_from_contract(
            fixture.action.contract);
    REFINE_CHECK(
        requirement.item_features ==
        fixture.action.contract.observed_item_features);
    REFINE_CHECK(
        requirement.affix_observations.size() == 1);

    const AbstractFeatureExtraction extracted =
        extract_strict_abstract_features(
            fixture.session, fixture.layout, fixture.state,
            requirement);
    REFINE_CHECK(extracted.complete());
    std::uint32_t exclusion_atoms = 0;
    std::uint32_t count_atoms = 0;
    std::uint32_t metamod_atoms = 0;
    std::uint32_t locked_affix_atoms = 0;
    for (const FeatureAtom& atom : extracted.features) {
        if (atom.feature ==
            RefinementFeature::ModifierExclusionSignature) {
            ++exclusion_atoms;
        }
        if (atom.feature ==
            RefinementFeature::CountObservationMembership) {
            ++count_atoms;
        }
        if (atom.feature ==
            RefinementFeature::ModifierMetamodRole) {
            ++metamod_atoms;
        }
        if ((atom.affix_traits &
             kRefinementAffixOnLockedSide) != 0) {
            ++locked_affix_atoms;
        }
    }
    REFINE_CHECK(exclusion_atoms == 3);
    REFINE_CHECK(count_atoms == 3);
    REFINE_CHECK(metamod_atoms == 3);
    REFINE_CHECK(locked_affix_atoms > 0);

    const std::optional<StableKey> signature =
        canonical_operation_state_signature(
            fixture.session, fixture.layout, fixture.state,
            fixture.action);
    REFINE_CHECK(signature.has_value());
    AbstractState changed = fixture.state;
    changed.rarity = PC_RARITY_MAGIC;
    const std::optional<StableKey> changed_signature =
        canonical_operation_state_signature(
            fixture.session, fixture.layout, changed,
            fixture.action);
    REFINE_CHECK(
        changed_signature.has_value() &&
        *changed_signature != *signature);

    fixture.session.required_level[3] = 21;
    const AbstractFeatureExtraction ambiguous =
        extract_strict_abstract_features(
            fixture.session, fixture.layout, fixture.state,
            requirement);
    REFINE_CHECK(!ambiguous.complete());
    REFINE_CHECK(
        (ambiguous.unavailable_features &
         refinement_feature(
             RefinementFeature::ModifierRequiredLevel)) != 0);
    REFINE_CHECK(!canonical_operation_state_signature(
        fixture.session, fixture.layout, fixture.state,
        fixture.action).has_value());

    fixture.session.required_level[3] = 20;
    fixture.session.metamod_type[3] = 8;
    const AbstractFeatureExtraction ambiguous_metamod =
        extract_strict_abstract_features(
            fixture.session, fixture.layout, fixture.state,
            requirement);
    REFINE_CHECK(!ambiguous_metamod.complete());
    REFINE_CHECK(
        (ambiguous_metamod.unavailable_features &
         refinement_feature(
             RefinementFeature::ModifierMetamodRole)) != 0);
    fixture.session.metamod_type[3] = 7;

    ActionRefinementContract exclusion_only =
        fixture.action.contract;
    exclusion_only.affix_observations[0].features =
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature);
    SelectedAction safe_action{
        78, {7800}, exclusion_only, {}, {}, {}};
    REFINE_CHECK(canonical_operation_state_signature(
        fixture.session, fixture.layout, fixture.state,
        safe_action).has_value());

    ObservationRequirement downstream;
    downstream.affix_observations = {{
        refinement_feature(
            RefinementFeature::CountObservationMembership),
        {}}};
    const ObservationRequirement preimage =
        preserved_observation_requirement(
            downstream, fixture.action.contract);
    REFINE_CHECK(!preimage.affix_observations.empty());
    const ObservationRequirement merged =
        merge_observation_requirements(requirement, preimage);
    REFINE_CHECK(
        merged.affix_observations.size() ==
        requirement.affix_observations.size());

    /*
     * Backward transfer follows feature-specific carrier flows. Fracture's
     * selected carrier becomes fractured without losing exclusion identity;
     * Unveil retains side/fracture but replaces exclusion identity.
     */
    ActionRefinementContract fracture;
    fracture.schema_version = kActionRefinementContractVersion;
    fracture.preserved_item_features = kAllRefinementItemFeatures;
    fracture.preserved_affixes.push_back({});
    fracture.affix_flows = {
        {{}, 0, 0, kAllRefinementAffixFeatures, true},
        {
            {0, kRefinementAffixFractured, 0, 0, {}},
            kRefinementAffixFractured,
            0,
            kAllRefinementAffixFeatures &
                ~refinement_feature(
                    RefinementFeature::ModifierFractured),
            true,
        }};
    ObservationRequirement fractured_exclusion;
    fractured_exclusion.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature),
        {kRefinementAffixFractured, 0, 0, 0, {}}}};
    const ObservationRequirement fracture_preimage =
        preserved_observation_requirement(
            fractured_exclusion, fracture);
    const auto has_observation =
        [](const ObservationRequirement& value,
           const RefinementFeature feature,
           const std::uint16_t required,
           const std::uint16_t forbidden) {
            return std::any_of(
                value.affix_observations.begin(),
                value.affix_observations.end(),
                [&](const RefinementAffixObservation& observation) {
                    return (observation.features &
                            refinement_feature(feature)) != 0 &&
                           observation.selector
                                   .required_affix_traits ==
                               required &&
                           observation.selector
                                   .forbidden_affix_traits ==
                               forbidden;
                });
        };
    REFINE_CHECK(has_observation(
        fracture_preimage,
        RefinementFeature::ModifierExclusionSignature,
        kRefinementAffixFractured, 0));
    REFINE_CHECK(has_observation(
        fracture_preimage,
        RefinementFeature::ModifierExclusionSignature,
        0, kRefinementAffixFractured));

    ActionRefinementContract unveil;
    unveil.schema_version = kActionRefinementContractVersion;
    unveil.preserved_item_features = kAllRefinementItemFeatures;
    unveil.affix_flows = {
        {
            {0, kRefinementAffixVeiled, 0, 0, {}},
            0,
            0,
            kAllRefinementAffixFeatures,
            true,
        },
        {
            {kRefinementAffixVeiled, 0, 0, 0, {}},
            0,
            kRefinementAffixVeiled,
            refinement_feature(
                RefinementFeature::ModifierSide) |
                refinement_feature(
                    RefinementFeature::ModifierCrafted) |
                refinement_feature(
                    RefinementFeature::ModifierFractured),
            false,
        }};
    ObservationRequirement after_unveil;
    after_unveil.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature) |
            refinement_feature(
                RefinementFeature::ModifierSide) |
            refinement_feature(
                RefinementFeature::ModifierFractured),
        {}}};
    const ObservationRequirement unveil_preimage =
        preserved_observation_requirement(
            after_unveil, unveil);
    REFINE_CHECK(has_observation(
        unveil_preimage,
        RefinementFeature::ModifierExclusionSignature,
        0, kRefinementAffixVeiled));
    REFINE_CHECK(!has_observation(
        unveil_preimage,
        RefinementFeature::ModifierExclusionSignature,
        kRefinementAffixVeiled, 0));
    REFINE_CHECK(has_observation(
        unveil_preimage,
        RefinementFeature::ModifierSide,
        kRefinementAffixVeiled, 0));
    REFINE_CHECK(has_observation(
        unveil_preimage,
        RefinementFeature::ModifierFractured,
        kRefinementAffixVeiled, 0));

    /*
     * Composite preservation is an ordered reverse composition, not a flat
     * union. Step one adds Crafted; step two adds Fractured. Observing the
     * final crafted+fractured carrier must therefore route on the original
     * plain prefix.
     */
    ActionRefinementContract add_crafted;
    add_crafted.schema_version =
        kActionRefinementContractVersion;
    add_crafted.affix_flows = {{
        {kRefinementAffixPrefix, 0, 0, 0, {}},
        kRefinementAffixCrafted,
        0,
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature),
        true}};
    ActionRefinementContract add_fractured;
    add_fractured.schema_version =
        kActionRefinementContractVersion;
    add_fractured.affix_flows = {{
        {
            static_cast<std::uint16_t>(
                kRefinementAffixPrefix |
                kRefinementAffixCrafted),
            0, 0, 0, {}},
        kRefinementAffixFractured,
        0,
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature),
        true}};
    SelectedAction composite;
    composite.action_id = 99;
    composite.semantic_key = {9900};
    composite.contract.schema_version =
        kActionRefinementContractVersion;
    composite.ordered_program = {
        add_crafted, add_fractured};
    ObservationRequirement final_carrier;
    final_carrier.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature),
        {
            static_cast<std::uint16_t>(
                kRefinementAffixPrefix |
                kRefinementAffixCrafted |
                kRefinementAffixFractured),
            0, 0, 0, {}}}};
    const ObservationRequirement composite_preimage =
        preserved_observation_requirement(
            final_carrier, composite);
    REFINE_CHECK(has_observation(
        composite_preimage,
        RefinementFeature::ModifierExclusionSignature,
        kRefinementAffixPrefix, 0));
    REFINE_CHECK(!has_observation(
        composite_preimage,
        RefinementFeature::ModifierExclusionSignature,
        static_cast<std::uint16_t>(
            kRefinementAffixPrefix |
            kRefinementAffixCrafted),
        0));

    /*
     * A conditional/early-exit composite must retain a downstream feature
     * when any authorized path preserves it, even if the longest path
     * destroys it. Treating only ordered_program as authoritative would
     * incorrectly erase the exclusion observation here.
     */
    const ActionRefinementContract preserve_identity =
        action_contract({}, true, false);
    const ActionRefinementContract destroy_identity =
        action_contract({}, false, true);
    SelectedAction path_alternatives;
    path_alternatives.action_id = 100;
    path_alternatives.semantic_key = {10000};
    path_alternatives.contract.schema_version =
        kActionRefinementContractVersion;
    path_alternatives.ordered_program = {
        preserve_identity, destroy_identity};
    path_alternatives.execution_paths = {
        {preserve_identity},
        {preserve_identity, destroy_identity}};
    const ObservationRequirement path_preimage =
        preserved_observation_requirement(
            final_carrier, path_alternatives);
    REFINE_CHECK(has_observation(
        path_preimage,
        RefinementFeature::ModifierExclusionSignature,
        static_cast<std::uint16_t>(
            kRefinementAffixPrefix |
            kRefinementAffixCrafted |
            kRefinementAffixFractured),
        0));

    SelectedAction longest_only = path_alternatives;
    longest_only.execution_paths.clear();
    const ObservationRequirement longest_preimage =
        preserved_observation_requirement(
            final_carrier, longest_only);
    REFINE_CHECK(!has_observation(
        longest_preimage,
        RefinementFeature::ModifierExclusionSignature,
        static_cast<std::uint16_t>(
            kRefinementAffixPrefix |
            kRefinementAffixCrafted |
            kRefinementAffixFractured),
        0));

    ActionRefinementContract resist_replace;
    resist_replace.schema_version =
        kActionRefinementContractVersion;
    resist_replace.preserved_item_features =
        kAllRefinementItemFeatures;
    RefinementAffixSelector resist_source;
    resist_source.forbidden_affix_traits =
        kRefinementAffixFractured |
        kRefinementAffixOnLockedSide;
    resist_source.required_tag_ids = {3, 5};
    resist_replace.affix_flows = {
        {{}, 0, 0, kAllRefinementAffixFeatures, true},
        {
            resist_source,
            0,
            kRefinementAffixCrafted |
                kRefinementAffixFractured |
                kRefinementAffixVeiled,
            refinement_feature(
                RefinementFeature::ModifierSide) |
                refinement_feature(
                    RefinementFeature::ModifierRequiredLevel),
            false,
        }};
    ObservationRequirement converted_resist;
    RefinementAffixSelector target_resist;
    target_resist.required_tag_ids = {7};
    converted_resist.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierSide) |
            refinement_feature(
                RefinementFeature::ModifierRequiredLevel) |
            refinement_feature(
                RefinementFeature::ModifierExclusionSignature) |
            refinement_feature(
                RefinementFeature::ModifierCrafted) |
            refinement_feature(
                RefinementFeature::ModifierVeiled),
        target_resist}};
    const ObservationRequirement resist_preimage =
        preserved_observation_requirement(
            converted_resist, resist_replace);
    const auto transformed_resist =
        std::find_if(
            resist_preimage.affix_observations.begin(),
            resist_preimage.affix_observations.end(),
            [&](const RefinementAffixObservation& observation) {
                return observation.selector.required_tag_ids ==
                       resist_source.required_tag_ids;
            });
    REFINE_CHECK(
        transformed_resist !=
        resist_preimage.affix_observations.end());
    if (transformed_resist !=
        resist_preimage.affix_observations.end()) {
        REFINE_CHECK(
            transformed_resist->features ==
            (refinement_feature(
                 RefinementFeature::ModifierSide) |
             refinement_feature(
                 RefinementFeature::ModifierRequiredLevel)));
    }

    ActionRefinementContract conditional_summary;
    conditional_summary.schema_version =
        kActionRefinementContractVersion;
    conditional_summary.preserved_item_features =
        kAllRefinementItemFeatures;
    conditional_summary.destroyed_item_features =
        refinement_feature(
            RefinementFeature::PrefixCount) |
        refinement_feature(
            RefinementFeature::CannotRollAttack);
    conditional_summary.item_affix_dependencies = {
        {
            refinement_feature(
                RefinementFeature::PrefixCount),
            refinement_feature(
                RefinementFeature::ModifierSide),
        },
        {
            refinement_feature(
                RefinementFeature::CannotRollAttack),
            refinement_feature(
                RefinementFeature::ModifierMetamodRole),
        }};
    conditional_summary.affix_flows = {
        {{}, 0, 0, kAllRefinementAffixFeatures, true}};
    ObservationRequirement summary_downstream;
    summary_downstream.item_features =
        conditional_summary.destroyed_item_features;
    const ObservationRequirement summary_preimage =
        preserved_observation_requirement(
            summary_downstream, conditional_summary);
    REFINE_CHECK(has_observation(
        summary_preimage,
        RefinementFeature::ModifierSide, 0, 0));
    REFINE_CHECK(has_observation(
        summary_preimage,
        RefinementFeature::ModifierMetamodRole, 0, 0));

    ActionRefinementContract scour_like;
    scour_like.schema_version =
        kActionRefinementContractVersion;
    scour_like.preserved_item_features =
        kAllRefinementItemFeatures;
    scour_like.destroyed_item_features =
        refinement_feature(RefinementFeature::Rarity);
    scour_like.item_affix_dependencies = {{
        refinement_feature(RefinementFeature::Rarity),
        refinement_feature(
            RefinementFeature::ModifierSide)}};
    RefinementAffixSelector surviving_fracture;
    surviving_fracture.required_affix_traits =
        kRefinementAffixFractured;
    surviving_fracture.forbidden_item_traits =
        kRefinementItemExactlyOneSideLocked;
    scour_like.affix_flows = {{
        surviving_fracture,
        0,
        0,
        kAllRefinementAffixFeatures,
        true}};
    ObservationRequirement rarity_downstream;
    rarity_downstream.item_features =
        refinement_feature(RefinementFeature::Rarity);
    const ObservationRequirement scour_preimage =
        preserved_observation_requirement(
            rarity_downstream, scour_like);
    FeatureAtom rarity_atom;
    rarity_atom.feature = RefinementFeature::Rarity;
    rarity_atom.value = {PC_RARITY_RARE};
    FeatureSignature zero_survivors = {rarity_atom};
    FeatureSignature one_survivor = zero_survivors;
    one_survivor.push_back(affix_atom(
        RefinementFeature::ModifierSide, 1,
        {PC_SIDE_PREFIX},
        kRefinementAffixPrefix |
            kRefinementAffixFractured));
    REFINE_CHECK(
        observe_features(
            zero_survivors, scour_preimage) !=
        observe_features(
            one_survivor, scour_preimage));

    ActionRefinementContract derived_scope =
        conditional_summary;
    derived_scope.destroyed_item_features =
        refinement_feature(RefinementFeature::PrefixLock) |
        refinement_feature(
            RefinementFeature::EldritchDominance);
    derived_scope.item_affix_dependencies.clear();
    ObservationRequirement scoped_downstream;
    RefinementAffixSelector locked_dominant;
    locked_dominant.required_affix_traits =
        kRefinementAffixOnLockedSide |
        kRefinementAffixOnEldritchDominantSide;
    locked_dominant.required_item_traits =
        kRefinementItemExactlyOneSideLocked |
        kRefinementItemHasEldritchDominance;
    scoped_downstream.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature),
        locked_dominant}};
    const ObservationRequirement scope_preimage =
        preserved_observation_requirement(
            scoped_downstream, derived_scope);
    REFINE_CHECK(has_observation(
        scope_preimage,
        RefinementFeature::ModifierExclusionSignature,
        0, 0));
    REFINE_CHECK(
        scope_preimage.affix_observations.front()
            .selector.required_item_traits == 0);
}

std::vector<ClosedPartitionNode> make_delayed_split_cycle() {
    std::vector<ClosedPartitionNode> nodes{
        partition_node(10, 1, 100),
        partition_node(11, 1, 100),
        partition_node(20, 2, 200),
        partition_node(21, 2, 200),
        partition_node(30, 3, 0, true),
        partition_node(31, 4, 0, true)};
    add_partition_arc(nodes, 0, {}, 2, 1.0);
    add_partition_arc(nodes, 1, {}, 3, 1.0);
    add_partition_arc(nodes, 2, {}, 0, 0.5);
    add_partition_arc(nodes, 2, {}, 4, 0.5);
    add_partition_arc(nodes, 3, {}, 1, 0.5);
    add_partition_arc(nodes, 3, {}, 5, 0.5);
    return nodes;
}

void run_closed_partition_cycle_merge_tests() {
    std::vector<ClosedPartitionNode> nodes{
        partition_node(10, 1, 100),
        partition_node(11, 1, 100),
        partition_node(20, 2, 200),
        partition_node(21, 2, 200)};
    /*
     * The raw successors differ, but both pairs form the same two-state
     * quotient cycle. Raw successor identity must not prevent the merge.
     */
    add_partition_arc(nodes, 0, {}, 2, 1.0);
    add_partition_arc(nodes, 1, {}, 3, 1.0);
    add_partition_arc(nodes, 2, {}, 0, 1.0);
    add_partition_arc(nodes, 3, {}, 1, 1.0);
    const ClosedPartitionResult result =
        refine_closed_probabilistic_partition(std::move(nodes));
    REFINE_CHECK(
        result.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(result.lumpable);
    REFINE_CHECK(result.initial_class_count == 2);
    REFINE_CHECK(result.final_class_count == 2);
    REFINE_CHECK(result.rounds == 1);
    REFINE_CHECK(partition_class(result, 10) ==
                 partition_class(result, 11));
    REFINE_CHECK(partition_class(result, 20) ==
                 partition_class(result, 21));
    REFINE_CHECK(partition_class(result, 10) !=
                 partition_class(result, 20));
}

void run_closed_partition_delayed_split_tests() {
    const ClosedPartitionResult result =
        refine_closed_probabilistic_partition(
            make_delayed_split_cycle());
    REFINE_CHECK(
        result.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(result.lumpable);
    REFINE_CHECK(result.initial_class_count == 4);
    REFINE_CHECK(result.final_class_count == 6);
    /*
     * Distinct absorbing observations split 20/21 first. That successor split
     * then propagates backward to 10/11 around the live cycle.
     */
    REFINE_CHECK(result.rounds >= 3);
    REFINE_CHECK(partition_class(result, 20) !=
                 partition_class(result, 21));
    REFINE_CHECK(partition_class(result, 10) !=
                 partition_class(result, 11));
}

void run_closed_partition_order_tests() {
    const std::vector<ClosedPartitionNode> canonical =
        make_delayed_split_cycle();
    const ClosedPartitionResult forward =
        refine_closed_probabilistic_partition(canonical);
    std::vector<ClosedPartitionNode> reversed =
        permute_partition_nodes(canonical, {5, 4, 3, 2, 1, 0});
    for (ClosedPartitionNode& node : reversed) {
        std::reverse(node.arcs.begin(), node.arcs.end());
    }
    const ClosedPartitionResult backward =
        refine_closed_probabilistic_partition(
            std::move(reversed));
    REFINE_CHECK(
        forward.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(
        backward.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(forward.rounds == backward.rounds);
    REFINE_CHECK(
        forward.initial_class_count ==
        backward.initial_class_count);
    REFINE_CHECK(
        forward.final_class_count ==
        backward.final_class_count);
    REFINE_CHECK(forward.classes == backward.classes);
}

void run_closed_partition_labeled_arc_tests() {
    std::vector<ClosedPartitionNode> labeled{
        partition_node(10, 1, 100),
        partition_node(11, 1, 100)};
    add_partition_arc(
        labeled, 0, {10}, std::nullopt, 0.25);
    add_partition_arc(
        labeled, 0, {20}, std::nullopt, 0.75);
    add_partition_arc(
        labeled, 1, {10}, std::nullopt, 0.75);
    add_partition_arc(
        labeled, 1, {20}, std::nullopt, 0.25);
    std::vector<ClosedPartitionNode> unlabeled = labeled;
    for (ClosedPartitionNode& node : unlabeled) {
        for (ClosedPartitionArc& arc : node.arcs) {
            arc.label.clear();
        }
    }

    const ClosedPartitionResult distinguished =
        refine_closed_probabilistic_partition(
            std::move(labeled));
    const ClosedPartitionResult merged =
        refine_closed_probabilistic_partition(
            std::move(unlabeled));
    REFINE_CHECK(
        distinguished.status ==
        ClosedPartitionStatus::Complete);
    REFINE_CHECK(distinguished.initial_class_count == 1);
    REFINE_CHECK(distinguished.final_class_count == 2);
    REFINE_CHECK(partition_class(distinguished, 10) !=
                 partition_class(distinguished, 11));
    REFINE_CHECK(
        merged.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(merged.final_class_count == 1);
    REFINE_CHECK(partition_class(merged, 10) ==
                 partition_class(merged, 11));
}

void run_observation_and_collapse_tests() {
    FixtureOracle distinct = make_preservation_fixture(false);
    /*
     * This carrier shares a reached successor's coarse parent, but no exact
     * kernel reaches it. Lazy discovery must not enumerate or expand it.
     */
    distinct.refinements[1].push_back(state(22, 1));
    const RefinementResult split =
        refine_policy_exact(distinct, request());
    REFINE_CHECK(split.status == RefinementStatus::Complete);
    REFINE_CHECK(split.executable);
    REFINE_CHECK(split.lumpable);
    REFINE_CHECK(split.telemetry.exact_states == 6);
    REFINE_CHECK(
        split.telemetry.policy_reachable_coarse_states == 3);
    REFINE_CHECK(
        distinct.enumeration_calls ==
        std::vector<std::uint32_t>{0});
    REFINE_CHECK(
        split.telemetry.observation_propagation_rounds >= 2);
    REFINE_CHECK(split.telemetry.collapse_events == 2);
    const std::size_t exclusion_feature =
        static_cast<std::size_t>(
            RefinementFeature::ModifierExclusionSignature);
    REFINE_CHECK(
        (split.telemetry.collapse_destroyed_feature_mask &
         refinement_feature(
             RefinementFeature::ModifierExclusionSignature)) != 0);
    REFINE_CHECK(
        split.telemetry
            .collapse_events_by_feature[exclusion_feature] == 2);
    REFINE_CHECK(
        (split.telemetry.collapse_preserved_feature_mask &
         refinement_feature(
             RefinementFeature::ModifierExclusionSignature)) != 0);
    REFINE_CHECK(
        split.telemetry
            .preservation_events_by_feature[exclusion_feature] == 2);
    REFINE_CHECK(split.telemetry.final_refinement_classes == 5);
    REFINE_CHECK(
        assigned_class(split, 10) != assigned_class(split, 11));
    REFINE_CHECK(
        assigned_class(split, 20) != assigned_class(split, 21));
    REFINE_CHECK(
        assigned_class(split, 30) == assigned_class(split, 31));

    const RefinedPolicyClass& root =
        split.classes.at(assigned_class(split, 10));
    REFINE_CHECK(std::any_of(
        root.required_observations.affix_observations.begin(),
        root.required_observations.affix_observations.end(),
        [](const RefinementAffixObservation& observation) {
            return (observation.features &
                    refinement_feature(
                        RefinementFeature::
                            ModifierExclusionSignature)) != 0;
        }));

    FixtureOracle equivalent = make_preservation_fixture(true);
    const RefinementResult merged =
        refine_policy_exact(equivalent, request());
    REFINE_CHECK(merged.status == RefinementStatus::Complete);
    REFINE_CHECK(merged.telemetry.final_refinement_classes == 3);
    REFINE_CHECK(merged.telemetry.merged_exact_states == 3);
    REFINE_CHECK(
        assigned_class(merged, 10) == assigned_class(merged, 11));
    REFINE_CHECK(
        assigned_class(merged, 20) == assigned_class(merged, 21));
    REFINE_CHECK(
        assigned_class(merged, 30) == assigned_class(merged, 31));

    FixtureOracle reversed = make_preservation_fixture(true);
    reversed.reverse_enumeration = true;
    const RefinementResult deterministic =
        refine_policy_exact(reversed, request());
    REFINE_CHECK(
        fingerprint(merged) == fingerprint(deterministic));
    REFINE_CHECK(
        reversed.enumeration_calls ==
        std::vector<std::uint32_t>{0});
}

void run_semantic_coarse_parent_identity_tests() {
    FixtureOracle baseline_oracle =
        make_preservation_fixture(false);
    const RefinementResult baseline =
        refine_policy_exact(baseline_oracle, request());
    REFINE_CHECK(baseline.status == RefinementStatus::Complete);

    FixtureOracle remapped_oracle =
        make_preservation_fixture(false);
    remap_numeric_coarse_states(
        remapped_oracle, {{0, 91}, {1, 7}, {2, 42}});
    RefinementRequest remapped_request = request();
    remapped_request.coarse_roots = {91};
    const RefinementResult remapped =
        refine_policy_exact(remapped_oracle, remapped_request);
    REFINE_CHECK(remapped.status == RefinementStatus::Complete);
    REFINE_CHECK(remapped.executable);
    REFINE_CHECK(remapped.lumpable);
    REFINE_CHECK(
        semantic_parent_fingerprint(baseline) ==
        semantic_parent_fingerprint(remapped));
    REFINE_CHECK(
        remapped.classes.at(assigned_class(remapped, 10))
            .coarse_state == 91);
    REFINE_CHECK(
        remapped.classes.at(assigned_class(remapped, 20))
            .coarse_state == 7);
    REFINE_CHECK(
        remapped.classes.at(assigned_class(remapped, 30))
            .coarse_state == 42);

    FixtureOracle empty_key = make_preservation_fixture(false);
    replace_semantic_coarse_key(empty_key, {10}, {});
    const RefinementResult empty_key_rejected =
        refine_policy_exact(empty_key, request());
    REFINE_CHECK(
        empty_key_rejected.status ==
        RefinementStatus::InvalidState);
    REFINE_CHECK(!empty_key_rejected.executable);
    REFINE_CHECK(
        empty_key_rejected.failure_reason ==
        "exact refinement has an empty coarse-state key");

    FixtureOracle inconsistent_numeric =
        make_preservation_fixture(false);
    replace_semantic_coarse_key(
        inconsistent_numeric, {11}, {0x696e636f6e736973ull});
    const RefinementResult inconsistent_numeric_rejected =
        refine_policy_exact(inconsistent_numeric, request());
    REFINE_CHECK(
        inconsistent_numeric_rejected.status ==
        RefinementStatus::InvalidState);
    REFINE_CHECK(!inconsistent_numeric_rejected.executable);
    REFINE_CHECK(
        inconsistent_numeric_rejected.failure_reason ==
        "one numeric coarse state names incompatible semantic keys");

    FixtureOracle non_unique_semantic =
        make_preservation_fixture(false);
    replace_semantic_coarse_key(
        non_unique_semantic, {20},
        {0x74657374636f6172ull, 0});
    const RefinementResult non_unique_semantic_rejected =
        refine_policy_exact(non_unique_semantic, request());
    REFINE_CHECK(
        non_unique_semantic_rejected.status ==
        RefinementStatus::InvalidState);
    REFINE_CHECK(!non_unique_semantic_rejected.executable);
    REFINE_CHECK(
        non_unique_semantic_rejected.failure_reason ==
        "one semantic coarse-state key names incompatible numeric states");
}

FixtureOracle make_cycle_fixture() {
    FixtureOracle oracle;
    const ExactState a = state(100, 0);
    const ExactState b = state(101, 0);
    const ExactState c = state(200, 1);
    const ExactState d = state(201, 1);
    const ExactState goal = state(300, 2, {}, true);
    oracle.refinements[0] = {a, b};
    oracle.refinements[1] = {c, d};
    oracle.refinements[2] = {goal};
    const ActionRefinementContract ordinary =
        action_contract({}, true, false);
    select(oracle, a, 10, 10, ordinary, 0.0, {{c, 1.0}});
    select(oracle, b, 10, 10, ordinary, 0.0, {{d, 1.0}});
    select(
        oracle, c, 11, 11, ordinary, 1.0,
        {{a, 0.5}, {goal, 0.5}});
    select(
        oracle, d, 11, 11, ordinary, 2.0,
        {{b, 0.5}, {goal, 0.5}});
    return oracle;
}

void run_fixed_point_tests() {
    FixtureOracle oracle = make_cycle_fixture();
    const RefinementResult result =
        refine_policy_exact(oracle, request());
    REFINE_CHECK(result.status == RefinementStatus::Complete);
    REFINE_CHECK(result.lumpable);
    REFINE_CHECK(result.telemetry.initial_observation_classes == 3);
    REFINE_CHECK(result.telemetry.final_refinement_classes == 5);
    REFINE_CHECK(result.telemetry.behavior_splits == 2);
    REFINE_CHECK(
        result.telemetry.partition_refinement_rounds >= 3);
    REFINE_CHECK(
        assigned_class(result, 100) !=
        assigned_class(result, 101));
    REFINE_CHECK(
        assigned_class(result, 200) !=
        assigned_class(result, 201));
    bool saw_cost = false;
    bool saw_projection = false;
    for (const RefinementCounterexample& witness :
         result.counterexamples) {
        saw_cost = saw_cost ||
            witness.kind == CounterexampleKind::ActionCost;
        saw_projection = saw_projection ||
            witness.kind ==
                CounterexampleKind::SuccessorProjection;
    }
    REFINE_CHECK(saw_cost);
    REFINE_CHECK(saw_projection);

    PolicyEvaluationRequest evaluation_request;
    evaluation_request.start_classes = {
        assigned_class(result, 100),
        assigned_class(result, 101)};
    const PolicyEvaluationResult evaluation =
        evaluate_refined_policy_exact(
            result, evaluation_request);
    REFINE_CHECK(
        evaluation.status == PolicyEvaluationStatus::Complete);
    REFINE_CHECK(evaluation.converged);
    REFINE_CHECK(evaluation.proper);
    REFINE_CHECK(evaluation.start_values.size() == 2);
    REFINE_CHECK(
        std::abs(evaluation.start_values[0] - 2.0) < 1e-12);
    REFINE_CHECK(
        std::abs(evaluation.start_values[1] - 4.0) < 1e-12);
    REFINE_CHECK(
        std::abs(evaluated_class_value(
            evaluation, assigned_class(result, 200)) - 2.0) <
        1e-12);
    REFINE_CHECK(
        evaluation.largest_component_classes == 2);
}

void run_selected_action_split_tests() {
    FixtureOracle oracle;
    const ExactState left = state(
        400, 0,
        {{RefinementFeature::Rarity, 0, {1}, 0, 0, {}}});
    const ExactState right = state(
        401, 0,
        {{RefinementFeature::Rarity, 0, {2}, 0, 0, {}}});
    const ExactState goal = state(402, 1, {}, true);
    oracle.refinements[0] = {left, right};
    const ActionRefinementContract ordinary =
        action_contract({}, true, false);
    select(
        oracle, left, 40, 4000, ordinary, 1.0,
        {{goal, 1.0}});
    select(
        oracle, right, 41, 4100, ordinary, 2.0,
        {{goal, 1.0}});

    const RefinementResult result =
        refine_policy_exact(oracle, request());
    REFINE_CHECK(result.status == RefinementStatus::Complete);
    REFINE_CHECK(
        result.telemetry.selected_action_routing_rounds > 0);
    REFINE_CHECK(
        assigned_class(result, 400) !=
        assigned_class(result, 401));
    REFINE_CHECK(result.telemetry.final_refinement_classes == 3);
    const RefinedPolicyClass& left_class =
        result.classes.at(assigned_class(result, 400));
    const RefinedPolicyClass& right_class =
        result.classes.at(assigned_class(result, 401));
    REFINE_CHECK(
        (left_class.required_observations.item_features &
         refinement_feature(RefinementFeature::Rarity)) != 0);
    REFINE_CHECK(
        left_class.required_observations ==
        right_class.required_observations);
    REFINE_CHECK(
        left_class.observation_signature !=
        right_class.observation_signature);
    REFINE_CHECK(
        oracle.enumeration_calls ==
        std::vector<std::uint32_t>{0});
    REFINE_CHECK(std::any_of(
        result.counterexamples.begin(),
        result.counterexamples.end(),
        [](const RefinementCounterexample& witness) {
            return witness.kind ==
                CounterexampleKind::SelectedAction;
        }));

    PolicyEvaluationRequest evaluation_request;
    evaluation_request.start_classes = {
        assigned_class(result, 400),
        assigned_class(result, 401)};
    const PolicyEvaluationResult evaluation =
        evaluate_refined_policy_exact(
            result, evaluation_request);
    REFINE_CHECK(
        evaluation.status == PolicyEvaluationStatus::Complete);
    REFINE_CHECK(evaluation.proper);
    REFINE_CHECK(evaluation.converged);
    REFINE_CHECK(
        evaluation.start_values == std::vector<double>({1.0, 2.0}));

    FixtureOracle round_capped = oracle;
    RefinementRequest round_limited = request();
    round_limited.limits.max_refinement_rounds = 1;
    const RefinementResult round_cap =
        refine_policy_exact(round_capped, round_limited);
    REFINE_CHECK(
        round_cap.status ==
        RefinementStatus::RefinementRoundCap);
    REFINE_CHECK(
        round_cap.resource_cap == "max_refinement_rounds");
    REFINE_CHECK(
        round_cap.telemetry.selected_action_routing_rounds == 1);

    FixtureOracle unobservable;
    const ExactState hidden_left = state(410, 0);
    const ExactState hidden_right = state(411, 0);
    unobservable.refinements[0] = {
        hidden_left, hidden_right};
    select(
        unobservable, hidden_left, 42, 4200,
        ordinary, 1.0, {{goal, 1.0}});
    select(
        unobservable, hidden_right, 43, 4300,
        ordinary, 1.0, {{goal, 1.0}});
    const RefinementResult hidden =
        refine_policy_exact(unobservable, request());
    REFINE_CHECK(
        hidden.status == RefinementStatus::InvalidContract);
    REFINE_CHECK(!hidden.executable);
    REFINE_CHECK(
        hidden.failure_reason ==
        "different exact policy actions have no observable "
        "distinguishing feature");

    /*
     * Routing observations are class-local. Rarity separates the first two
     * decisions, Influence separates the first and third, and the second and
     * third are already mutually exclusive through those local predicates.
     * A graph-wide union would incorrectly make every class retain both
     * features.
     */
    FixtureOracle local_routes;
    const ExactState route_a = state(
        420, 0,
        {
            {RefinementFeature::Rarity, 0, {1}, 0, 0, {}},
            {RefinementFeature::Influence, 0, {0}, 0, 0, {}},
        });
    const ExactState route_b = state(
        421, 0,
        {
            {RefinementFeature::Rarity, 0, {2}, 0, 0, {}},
            {RefinementFeature::Influence, 0, {0}, 0, 0, {}},
        });
    const ExactState route_c = state(
        422, 0,
        {
            {RefinementFeature::Rarity, 0, {1}, 0, 0, {}},
            {RefinementFeature::Influence, 0, {1}, 0, 0, {}},
        });
    local_routes.refinements[0] = {
        route_a, route_b, route_c};
    select(
        local_routes, route_a, 44, 4400,
        ordinary, 1.0, {{goal, 1.0}});
    select(
        local_routes, route_b, 45, 4500,
        ordinary, 1.0, {{goal, 1.0}});
    select(
        local_routes, route_c, 46, 4600,
        ordinary, 1.0, {{goal, 1.0}});
    const RefinementResult local =
        refine_policy_exact(local_routes, request());
    REFINE_CHECK(local.status == RefinementStatus::Complete);
    const ObservationRequirement& route_a_requirement =
        local.classes.at(assigned_class(local, 420))
            .required_observations;
    const ObservationRequirement& route_b_requirement =
        local.classes.at(assigned_class(local, 421))
            .required_observations;
    const ObservationRequirement& route_c_requirement =
        local.classes.at(assigned_class(local, 422))
            .required_observations;
    REFINE_CHECK(
        (route_a_requirement.item_features &
         refinement_feature(RefinementFeature::Rarity)) != 0);
    REFINE_CHECK(
        (route_a_requirement.item_features &
         refinement_feature(RefinementFeature::Influence)) != 0);
    REFINE_CHECK(
        (route_b_requirement.item_features &
         refinement_feature(RefinementFeature::Rarity)) != 0);
    REFINE_CHECK(
        (route_b_requirement.item_features &
         refinement_feature(RefinementFeature::Influence)) == 0);
    REFINE_CHECK(
        (route_c_requirement.item_features &
         refinement_feature(RefinementFeature::Rarity)) == 0);
    REFINE_CHECK(
        (route_c_requirement.item_features &
         refinement_feature(RefinementFeature::Influence)) != 0);

    /*
     * Each feature's multiset is equal, but the exclusion signature and
     * required level are paired on different affixes. A valid router must
     * compose both observations; a raw symmetric difference plus one-term
     * trial falsely reports this pair as unobservable.
     */
    FixtureOracle correlated;
    const ExactState correlated_left = state(
        430, 0,
        {
            affix_atom(
                RefinementFeature::ModifierExclusionSignature,
                0, {10}),
            affix_atom(
                RefinementFeature::ModifierRequiredLevel,
                0, {1}),
            affix_atom(
                RefinementFeature::ModifierExclusionSignature,
                1, {20}),
            affix_atom(
                RefinementFeature::ModifierRequiredLevel,
                1, {2}),
        });
    const ExactState correlated_right = state(
        431, 0,
        {
            affix_atom(
                RefinementFeature::ModifierExclusionSignature,
                0, {10}),
            affix_atom(
                RefinementFeature::ModifierRequiredLevel,
                0, {2}),
            affix_atom(
                RefinementFeature::ModifierExclusionSignature,
                1, {20}),
            affix_atom(
                RefinementFeature::ModifierRequiredLevel,
                1, {1}),
        });
    correlated.refinements[0] = {
        correlated_left, correlated_right};
    select(
        correlated, correlated_left, 47, 4700,
        ordinary, 1.0, {{goal, 1.0}});
    select(
        correlated, correlated_right, 48, 4800,
        ordinary, 2.0, {{goal, 1.0}});
    ObservationRequirement exclusion_only;
    exclusion_only.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierExclusionSignature),
        {}}};
    ObservationRequirement level_only;
    level_only.affix_observations = {{
        refinement_feature(
            RefinementFeature::ModifierRequiredLevel),
        {}}};
    REFINE_CHECK(
        observe_features(
            canonical_feature_signature(correlated_left.features),
            exclusion_only) ==
        observe_features(
            canonical_feature_signature(correlated_right.features),
            exclusion_only));
    REFINE_CHECK(
        observe_features(
            canonical_feature_signature(correlated_left.features),
            level_only) ==
        observe_features(
            canonical_feature_signature(correlated_right.features),
            level_only));
    const RefinementResult correlated_result =
        refine_policy_exact(correlated, request());
    REFINE_CHECK(
        correlated_result.status == RefinementStatus::Complete);
    const RefinedPolicyClass& correlated_class =
        correlated_result.classes.at(
            assigned_class(correlated_result, 430));
    RefinementFeatureMask correlated_features = 0;
    for (const RefinementAffixObservation& observation :
         correlated_class.required_observations.affix_observations) {
        correlated_features |= observation.features;
    }
    REFINE_CHECK(
        (correlated_features &
         refinement_feature(
             RefinementFeature::ModifierExclusionSignature)) != 0);
    REFINE_CHECK(
        (correlated_features &
         refinement_feature(
             RefinementFeature::ModifierRequiredLevel)) != 0);
    REFINE_CHECK(
        correlated_result.classes.at(
            assigned_class(correlated_result, 431))
                .required_observations ==
        correlated_class.required_observations);
    REFINE_CHECK(
        correlated_result.classes.at(
            assigned_class(correlated_result, 431))
                .observation_signature !=
        correlated_class.observation_signature);

    /*
     * The exact atoms deliberately correlate exclusion identity with every
     * transient selector trait.  Exclusion identity alone separates the two
     * selected decisions, so synthesized routes must not publish
     * crafted/fractured/Veiled/lock/Eldritch (or side) predicates.  Authored
     * routing and action-contract observations remain byte-for-byte present.
     */
    FixtureOracle incidental_traits;
    const std::uint16_t left_traits =
        kRefinementAffixPrefix |
        kRefinementAffixCrafted |
        kRefinementAffixFractured |
        kRefinementAffixVeiled |
        kRefinementAffixOnLockedSide |
        kRefinementAffixOnEldritchDominantSide;
    const std::uint16_t right_traits =
        kRefinementAffixSuffix |
        kRefinementAffixOnEldritchNonDominantSide;
    const std::uint8_t left_item_traits =
        kRefinementItemHasEldritchDominance |
        kRefinementItemExactlyOneSideLocked;
    const ExactState incidental_left = state(
        440, 0,
        {{RefinementFeature::ModifierExclusionSignature,
          0, {10}, left_traits, left_item_traits, {}}});
    const ExactState incidental_right = state(
        441, 0,
        {{RefinementFeature::ModifierExclusionSignature,
          0, {20}, right_traits, 0, {}}});
    incidental_traits.refinements[0] = {
        incidental_left, incidental_right};

    ActionRefinementContract authoritative = ordinary;
    RefinementAffixObservation contract_observation{
        refinement_feature(RefinementFeature::ModifierCrafted),
        {kRefinementAffixCrafted, 0, 0, 0, {}}};
    authoritative.affix_observations = {contract_observation};
    select(
        incidental_traits, incidental_left, 49, 4900,
        authoritative, 1.0, {{goal, 1.0}});
    select(
        incidental_traits, incidental_right, 50, 5000,
        authoritative, 2.0, {{goal, 1.0}});
    RefinementAffixObservation authored_route{
        refinement_feature(RefinementFeature::ModifierVeiled),
        {kRefinementAffixVeiled, 0,
         kRefinementItemExactlyOneSideLocked, 0, {}}};
    ObservationRequirement authored_requirement;
    authored_requirement.affix_observations = {authored_route};
    incidental_traits.selections.at(incidental_left.stable_key)
        .routing_observes = authored_requirement;
    incidental_traits.selections.at(incidental_right.stable_key)
        .routing_observes = authored_requirement;

    const RefinementResult minimized =
        refine_policy_exact(incidental_traits, request());
    REFINE_CHECK(minimized.status == RefinementStatus::Complete);
    for (const std::uint64_t state_key : {440ull, 441ull}) {
        const ObservationRequirement& requirement =
            minimized.classes.at(assigned_class(minimized, state_key))
                .required_observations;
        REFINE_CHECK(std::find(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            contract_observation) !=
            requirement.affix_observations.end());
        REFINE_CHECK(std::find(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            authored_route) !=
            requirement.affix_observations.end());
        const auto exclusion = std::find_if(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            [](const RefinementAffixObservation& observation) {
                return (observation.features &
                        refinement_feature(
                            RefinementFeature::
                                ModifierExclusionSignature)) != 0;
            });
        REFINE_CHECK(exclusion !=
                     requirement.affix_observations.end());
        REFINE_CHECK(exclusion->selector.required_affix_traits == 0);
        REFINE_CHECK(exclusion->selector.forbidden_affix_traits == 0);
        REFINE_CHECK(exclusion->selector.required_item_traits == 0);
        REFINE_CHECK(exclusion->selector.forbidden_item_traits == 0);
        REFINE_CHECK(exclusion->selector.required_tag_ids.empty());
    }
}

void run_improper_policy_evaluation_tests() {
    FixtureOracle oracle;
    const ExactState loop = state(500, 0);
    oracle.refinements[0] = {loop};
    const ActionRefinementContract ordinary =
        action_contract({}, true, false);
    select(
        oracle, loop, 50, 5000, ordinary, 1.0,
        {{loop, 1.0}});
    const RefinementResult refinement =
        refine_policy_exact(oracle, request());
    REFINE_CHECK(
        refinement.status == RefinementStatus::Complete);

    PolicyEvaluationRequest evaluation_request;
    evaluation_request.start_classes = {
        assigned_class(refinement, 500)};
    const PolicyEvaluationResult evaluation =
        evaluate_refined_policy_exact(
            refinement, evaluation_request);
    REFINE_CHECK(
        evaluation.status ==
        PolicyEvaluationStatus::ImproperPolicy);
    REFINE_CHECK(!evaluation.converged);
    REFINE_CHECK(!evaluation.proper);
    REFINE_CHECK(
        evaluation.failure_reason ==
        "improper_closed_component");
    REFINE_CHECK(evaluation.improper_component_count == 1);
    REFINE_CHECK(
        evaluation.improper_component_classes ==
        std::vector<std::uint32_t>{
            assigned_class(refinement, 500)});

    FixtureOracle multiple;
    const ExactState root = state(510, 0);
    const ExactState left = state(520, 1);
    const ExactState right = state(521, 2);
    const ExactState other = state(530, 3);
    multiple.refinements[0] = {root};
    select(
        multiple, root, 51, 5100, ordinary, 0.0,
        {{left, 0.5}, {other, 0.5}});
    select(
        multiple, left, 52, 5200, ordinary, 1.0,
        {{right, 1.0}});
    select(
        multiple, right, 53, 5300, ordinary, 1.0,
        {{left, 1.0}});
    select(
        multiple, other, 54, 5400, ordinary, 1.0,
        {{other, 1.0}});
    const RefinementResult multiple_refinement =
        refine_policy_exact(multiple, request());
    REFINE_CHECK(
        multiple_refinement.status == RefinementStatus::Complete);
    PolicyEvaluationRequest multiple_request;
    multiple_request.start_classes = {
        assigned_class(multiple_refinement, 510)};
    const PolicyEvaluationResult multiple_evaluation =
        evaluate_refined_policy_exact(
            multiple_refinement, multiple_request);
    REFINE_CHECK(
        multiple_evaluation.status ==
        PolicyEvaluationStatus::ImproperPolicy);
    REFINE_CHECK(
        multiple_evaluation.improper_component_count == 2);
    std::vector<std::uint32_t> cycle_component{
        assigned_class(multiple_refinement, 520),
        assigned_class(multiple_refinement, 521)};
    std::sort(cycle_component.begin(), cycle_component.end());
    const std::vector<std::uint32_t> singleton_component{
        assigned_class(multiple_refinement, 530)};
    const std::vector<std::uint32_t>& canonical_component =
        std::lexicographical_compare(
            cycle_component.begin(), cycle_component.end(),
            singleton_component.begin(), singleton_component.end())
            ? cycle_component
            : singleton_component;
    REFINE_CHECK(
        multiple_evaluation.improper_component_classes ==
        canonical_component);
    REFINE_CHECK(std::is_sorted(
        multiple_evaluation.improper_component_classes.begin(),
        multiple_evaluation.improper_component_classes.end()));

    FixtureOracle reversed = multiple;
    reversed.reverse_enumeration = true;
    const RefinementResult reversed_refinement =
        refine_policy_exact(reversed, request());
    PolicyEvaluationRequest reversed_request;
    reversed_request.start_classes = {
        assigned_class(reversed_refinement, 510)};
    const PolicyEvaluationResult reversed_evaluation =
        evaluate_refined_policy_exact(
            reversed_refinement, reversed_request);
    REFINE_CHECK(
        reversed_evaluation.status ==
        PolicyEvaluationStatus::ImproperPolicy);
    REFINE_CHECK(
        reversed_evaluation.improper_component_count ==
        multiple_evaluation.improper_component_count);
    REFINE_CHECK(
        reversed_evaluation.improper_component_classes ==
        multiple_evaluation.improper_component_classes);

    PolicyEvaluationRequest exact_memory_request = multiple_request;
    exact_memory_request.limits.max_estimated_memory_bytes =
        multiple_evaluation.peak_estimated_memory_bytes;
    const PolicyEvaluationResult exact_memory =
        evaluate_refined_policy_exact(
            multiple_refinement, exact_memory_request);
    REFINE_CHECK(
        exact_memory.status ==
        PolicyEvaluationStatus::ImproperPolicy);
    REFINE_CHECK(
        exact_memory.improper_component_classes ==
        canonical_component);
    PolicyEvaluationRequest short_memory_request = multiple_request;
    short_memory_request.limits.max_estimated_memory_bytes =
        multiple_evaluation.peak_estimated_memory_bytes - 1;
    const PolicyEvaluationResult short_memory =
        evaluate_refined_policy_exact(
            multiple_refinement, short_memory_request);
    REFINE_CHECK(
        short_memory.status ==
        PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        short_memory.resource_cap ==
        "max_estimated_memory_bytes");
}

void run_closed_partition_memory_cap_tests() {
    const std::vector<ClosedPartitionNode> nodes =
        make_delayed_split_cycle();
    const ClosedPartitionResult baseline =
        refine_closed_probabilistic_partition(nodes);
    REFINE_CHECK(
        baseline.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(
        baseline.peak_estimated_memory_bytes >
        baseline.estimated_memory_bytes);

    ClosedPartitionLimits exact_limits;
    exact_limits.max_estimated_memory_bytes =
        baseline.peak_estimated_memory_bytes;
    const ClosedPartitionResult exact =
        refine_closed_probabilistic_partition(
            nodes, exact_limits);
    REFINE_CHECK(
        exact.status == ClosedPartitionStatus::Complete);
    REFINE_CHECK(exact.classes == baseline.classes);

    ClosedPartitionLimits short_limits = exact_limits;
    short_limits.max_estimated_memory_bytes =
        baseline.peak_estimated_memory_bytes - 1;
    const ClosedPartitionResult capped =
        refine_closed_probabilistic_partition(
            nodes, short_limits);
    REFINE_CHECK(
        capped.status == ClosedPartitionStatus::ResourceCap);
    REFINE_CHECK(
        capped.resource_cap == "max_estimated_memory_bytes");
}

FixtureOracle make_large_returned_kernel_fixture() {
    FixtureOracle oracle;
    const ExactState source = state(600, 0);
    const ExactState goal = state(601, 1, {}, true);
    oracle.refinements[0] = {source};
    const ActionRefinementContract ordinary =
        action_contract({}, true, false);
    constexpr std::uint32_t kDuplicateTransitions = 2048;
    std::vector<ExactTransition> transitions;
    transitions.reserve(kDuplicateTransitions);
    for (std::uint32_t index = 0;
         index < kDuplicateTransitions; ++index) {
        transitions.push_back({
            goal, 1.0 / kDuplicateTransitions});
    }
    select(
        oracle, source, 60, 6000, ordinary, 1.0,
        std::move(transitions));
    return oracle;
}

void add_large_nested_contract(FixtureOracle& oracle) {
    RefinementAffixSelector selector;
    selector.required_tag_ids.reserve(8192);
    for (std::uint32_t tag = 1; tag <= 8192; ++tag) {
        selector.required_tag_ids.push_back(tag);
    }
    for (const StableKey& key :
         std::vector<StableKey>{{10}, {11}}) {
        oracle.selections.at(key)
            .contract.destroyed_affixes.push_back(selector);
    }
}

void add_large_runtime_path(FixtureOracle& oracle) {
    RefinementAffixSelector selector;
    selector.required_tag_ids.reserve(8192);
    for (std::uint32_t tag = 1; tag <= 8192; ++tag) {
        selector.required_tag_ids.push_back(tag);
    }
    ActionRefinementContract runtime =
        action_contract({}, true, false);
    runtime.destroyed_affixes.push_back(std::move(selector));
    for (const StableKey& key :
         std::vector<StableKey>{{10}, {11}}) {
        SelectedAction& selected =
            oracle.selections.at(key);
        selected.ordered_program = {runtime};
        selected.execution_paths = {{runtime}};
    }
}

void run_refinement_memory_accounting_tests() {
    FixtureOracle small = make_preservation_fixture(false);
    const RefinementResult small_baseline =
        refine_policy_exact(small, request());
    REFINE_CHECK(
        small_baseline.status == RefinementStatus::Complete);

    FixtureOracle large = make_preservation_fixture(false);
    add_large_nested_contract(large);
    const RefinementResult large_baseline =
        refine_policy_exact(large, request());
    REFINE_CHECK(
        large_baseline.status == RefinementStatus::Complete);
    REFINE_CHECK(
        large_baseline.telemetry.peak_estimated_memory_bytes >
        small_baseline.telemetry.peak_estimated_memory_bytes);

    FixtureOracle small_exact = make_preservation_fixture(false);
    RefinementRequest small_limit = request();
    small_limit.limits.max_estimated_memory_bytes =
        small_baseline.telemetry.peak_estimated_memory_bytes;
    const RefinementResult small_under_same_cap =
        refine_policy_exact(small_exact, small_limit);
    REFINE_CHECK(
        small_under_same_cap.status == RefinementStatus::Complete);

    FixtureOracle large_capped = make_preservation_fixture(false);
    add_large_nested_contract(large_capped);
    RefinementRequest nested_limit = request();
    nested_limit.limits.max_estimated_memory_bytes =
        small_baseline.telemetry.peak_estimated_memory_bytes;
    const RefinementResult nested_cap =
        refine_policy_exact(large_capped, nested_limit);
    REFINE_CHECK(
        nested_cap.status == RefinementStatus::ResourceCap);
    REFINE_CHECK(
        nested_cap.resource_cap == "max_estimated_memory_bytes");

    FixtureOracle path_heavy =
        make_preservation_fixture(false);
    add_large_runtime_path(path_heavy);
    const RefinementResult path_baseline =
        refine_policy_exact(path_heavy, request());
    REFINE_CHECK(
        path_baseline.status == RefinementStatus::Complete);
    REFINE_CHECK(
        path_baseline.telemetry.peak_estimated_memory_bytes >
        small_baseline.telemetry.peak_estimated_memory_bytes);

    FixtureOracle path_capped =
        make_preservation_fixture(false);
    add_large_runtime_path(path_capped);
    RefinementRequest path_limit = request();
    path_limit.limits.max_estimated_memory_bytes =
        small_baseline.telemetry.peak_estimated_memory_bytes;
    const RefinementResult path_cap =
        refine_policy_exact(path_capped, path_limit);
    REFINE_CHECK(
        path_cap.status == RefinementStatus::ResourceCap);
    REFINE_CHECK(
        path_cap.resource_cap ==
        "max_estimated_memory_bytes");

    FixtureOracle kernel = make_large_returned_kernel_fixture();
    const RefinementResult kernel_baseline =
        refine_policy_exact(kernel, request());
    REFINE_CHECK(
        kernel_baseline.status == RefinementStatus::Complete);
    REFINE_CHECK(
        kernel_baseline.telemetry.peak_estimated_memory_bytes >
        kernel_baseline.telemetry.estimated_memory_bytes +
            1024 * sizeof(ExactTransition));

    FixtureOracle kernel_exact =
        make_large_returned_kernel_fixture();
    RefinementRequest exact_limit = request();
    exact_limit.limits.max_estimated_memory_bytes =
        kernel_baseline.telemetry.peak_estimated_memory_bytes;
    const RefinementResult exact =
        refine_policy_exact(kernel_exact, exact_limit);
    REFINE_CHECK(exact.status == RefinementStatus::Complete);

    FixtureOracle kernel_capped =
        make_large_returned_kernel_fixture();
    RefinementRequest short_limit = exact_limit;
    short_limit.limits.max_estimated_memory_bytes =
        kernel_baseline.telemetry.peak_estimated_memory_bytes - 1;
    const RefinementResult capped =
        refine_policy_exact(kernel_capped, short_limit);
    REFINE_CHECK(capped.status == RefinementStatus::ResourceCap);
    REFINE_CHECK(
        capped.resource_cap == "max_estimated_memory_bytes");
}

RefinementResult make_cycle_evaluation_fixture(
        const std::uint32_t order) {
    RefinementResult refinement;
    refinement.status = RefinementStatus::Complete;
    refinement.executable = true;
    refinement.lumpable = true;
    const ActionRefinementContract ordinary =
        action_contract({}, true, false);
    for (std::uint32_t class_id = 0;
         class_id < order; ++class_id) {
        RefinedPolicyClass row;
        row.class_id = class_id;
        row.selected_action =
            SelectedAction{
                1, {1}, ordinary, {}, {}, {}};
        row.action_cost = 1.0;
        row.transitions = {
            {(class_id + 1) % order, 0.5},
            {order, 0.5}};
        std::sort(
            row.transitions.begin(), row.transitions.end(),
            [](const ProjectedTransition& left,
               const ProjectedTransition& right) {
                return left.successor_class <
                       right.successor_class;
            });
        refinement.classes.push_back(std::move(row));
    }
    RefinedPolicyClass terminal;
    terminal.class_id = order;
    terminal.goal = true;
    terminal.terminal = true;
    refinement.classes.push_back(std::move(terminal));
    return refinement;
}

void run_zero_probability_policy_evaluation_tests() {
    RefinementResult refinement;
    refinement.status = RefinementStatus::Complete;
    refinement.executable = true;
    refinement.lumpable = true;

    RefinedPolicyClass source;
    source.class_id = 0;
    source.selected_action = SelectedAction{
        1, {1}, action_contract({}, true, false), {}, {}, {}};
    source.action_cost = 2.0;
    source.transitions = {{1, 1.0}, {2, 0.0}};
    refinement.classes.push_back(std::move(source));

    for (std::uint32_t class_id = 1; class_id <= 2; ++class_id) {
        RefinedPolicyClass terminal;
        terminal.class_id = class_id;
        terminal.goal = true;
        terminal.terminal = true;
        refinement.classes.push_back(std::move(terminal));
    }

    PolicyEvaluationRequest request;
    request.start_classes = {0};
    const PolicyEvaluationResult evaluated =
        evaluate_refined_policy_exact(refinement, request);
    REFINE_CHECK(
        evaluated.status == PolicyEvaluationStatus::Complete);
    REFINE_CHECK(evaluated.converged);
    REFINE_CHECK(evaluated.proper);
    REFINE_CHECK(evaluated.reachable_classes == 2);
    REFINE_CHECK(evaluated.class_values.size() == 2);
    REFINE_CHECK(evaluated.start_values.size() == 1);
    REFINE_CHECK(
        std::abs(evaluated.start_values.front() - 2.0) <= 1e-12);
}

void run_policy_evaluation_memory_cap_tests() {
    constexpr std::uint32_t kSparseCycleOrder = 257;
    const RefinementResult cycle =
        make_cycle_evaluation_fixture(kSparseCycleOrder);
    PolicyEvaluationRequest baseline_request;
    baseline_request.start_classes = {0};
    const PolicyEvaluationResult baseline =
        evaluate_refined_policy_exact(
            cycle, baseline_request);
    REFINE_CHECK(
        baseline.status == PolicyEvaluationStatus::Complete);
    REFINE_CHECK(
        baseline.largest_component_classes ==
        kSparseCycleOrder);
    REFINE_CHECK(baseline.start_values.size() == 1);
    REFINE_CHECK(
        std::abs(baseline.start_values.front() - 2.0) <=
        1e-10);
    REFINE_CHECK(
        baseline.peak_estimated_memory_bytes >
        baseline.estimated_memory_bytes);

    RefinementResult iteration_limited_cycle = cycle;
    for (std::uint32_t class_id = 0;
         class_id < kSparseCycleOrder; ++class_id) {
        iteration_limited_cycle.classes[class_id].action_cost +=
            static_cast<double>((class_id * 17) % 29) / 32.0;
    }
    const PolicyEvaluationResult resumed_baseline =
        evaluate_refined_policy_exact(
            iteration_limited_cycle, baseline_request);
    REFINE_CHECK(
        resumed_baseline.status ==
        PolicyEvaluationStatus::Complete);
    REFINE_CHECK(resumed_baseline.converged);

    PolicyEvaluationRequest resumed_exact_request = baseline_request;
    resumed_exact_request.limits.max_estimated_memory_bytes =
        resumed_baseline.peak_estimated_memory_bytes;
    const PolicyEvaluationResult resumed_exact =
        evaluate_refined_policy_exact(
            iteration_limited_cycle, resumed_exact_request);
    REFINE_CHECK(
        resumed_exact.status == PolicyEvaluationStatus::Complete);
    REFINE_CHECK(
        resumed_exact.start_values ==
        resumed_baseline.start_values);

    PolicyEvaluationRequest resumed_short_request =
        resumed_exact_request;
    resumed_short_request.limits.max_estimated_memory_bytes =
        resumed_baseline.peak_estimated_memory_bytes - 1;
    const PolicyEvaluationResult resumed_memory_capped =
        evaluate_refined_policy_exact(
            iteration_limited_cycle, resumed_short_request);
    REFINE_CHECK(
        resumed_memory_capped.status ==
        PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        resumed_memory_capped.resource_cap ==
        "max_estimated_memory_bytes");

    PolicyEvaluationRequest iteration_cap_request;
    iteration_cap_request.start_classes = {0};
    iteration_cap_request.limits.max_component_iterations = 1;
    const PolicyEvaluationResult iteration_capped =
        evaluate_refined_policy_exact(
            iteration_limited_cycle, iteration_cap_request);
    REFINE_CHECK(
        iteration_capped.status ==
        PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        iteration_capped.resource_cap ==
        "max_component_iterations");
    REFINE_CHECK(!iteration_capped.converged);

    PolicyEvaluationRequest reachable_cap_request =
        baseline_request;
    reachable_cap_request.limits.max_reachable_classes =
        kSparseCycleOrder;
    const PolicyEvaluationResult reachable_capped =
        evaluate_refined_policy_exact(
            cycle, reachable_cap_request);
    REFINE_CHECK(
        reachable_capped.status ==
        PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        reachable_capped.resource_cap ==
        "max_reachable_classes");

    PolicyEvaluationRequest exact_request = baseline_request;
    exact_request.limits.max_estimated_memory_bytes =
        baseline.peak_estimated_memory_bytes;
    const PolicyEvaluationResult exact =
        evaluate_refined_policy_exact(cycle, exact_request);
    REFINE_CHECK(
        exact.status == PolicyEvaluationStatus::Complete);
    REFINE_CHECK(exact.start_values == baseline.start_values);

    PolicyEvaluationRequest short_request = exact_request;
    short_request.limits.max_estimated_memory_bytes =
        baseline.peak_estimated_memory_bytes - 1;
    const PolicyEvaluationResult capped =
        evaluate_refined_policy_exact(cycle, short_request);
    REFINE_CHECK(
        capped.status == PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        capped.resource_cap == "max_estimated_memory_bytes");
    REFINE_CHECK(!capped.converged);

    RefinementResult unreachable;
    unreachable.status = RefinementStatus::Complete;
    unreachable.executable = true;
    unreachable.lumpable = true;
    for (std::uint32_t class_id = 0;
         class_id < 128; ++class_id) {
        RefinedPolicyClass terminal;
        terminal.class_id = class_id;
        terminal.goal = true;
        terminal.terminal = true;
        unreachable.classes.push_back(std::move(terminal));
    }
    PolicyEvaluationRequest table_cap;
    table_cap.start_classes = {0};
    table_cap.limits.max_estimated_memory_bytes =
        unreachable.classes.size() *
            sizeof(const RefinedPolicyClass*) -
        1;
    const PolicyEvaluationResult early =
        evaluate_refined_policy_exact(
            unreachable, table_cap);
    REFINE_CHECK(
        early.status == PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        early.resource_cap == "max_estimated_memory_bytes");
    REFINE_CHECK(early.reachable_classes == 0);

    RefinementResult result_heavy;
    result_heavy.status = RefinementStatus::Complete;
    result_heavy.executable = true;
    result_heavy.lumpable = true;
    RefinedPolicyClass only_terminal;
    only_terminal.class_id = 0;
    only_terminal.goal = true;
    only_terminal.terminal = true;
    result_heavy.classes.push_back(std::move(only_terminal));
    PolicyEvaluationRequest many_starts;
    many_starts.start_classes.assign(4096, 0);
    const PolicyEvaluationResult result_baseline =
        evaluate_refined_policy_exact(
            result_heavy, many_starts);
    REFINE_CHECK(
        result_baseline.status ==
        PolicyEvaluationStatus::Complete);
    REFINE_CHECK(
        result_baseline.estimated_memory_bytes >=
        4096 * sizeof(double));
    PolicyEvaluationRequest result_short = many_starts;
    result_short.limits.max_estimated_memory_bytes =
        result_baseline.peak_estimated_memory_bytes - 1;
    const PolicyEvaluationResult result_capped =
        evaluate_refined_policy_exact(
            result_heavy, result_short);
    REFINE_CHECK(
        result_capped.status ==
        PolicyEvaluationStatus::ResourceCap);
    REFINE_CHECK(
        result_capped.resource_cap ==
        "max_estimated_memory_bytes");
}

void run_rejection_and_cap_tests() {
    FixtureOracle incomplete = make_preservation_fixture(false);
    incomplete.selections.at({10}).contract.schema_version = 0;
    const RefinementResult rejected =
        refine_policy_exact(incomplete, request());
    REFINE_CHECK(rejected.status == RefinementStatus::InvalidContract);
    REFINE_CHECK(!rejected.executable);

    FixtureOracle empty_path = make_preservation_fixture(false);
    empty_path.selections.at({10}).execution_paths = {{}};
    const RefinementResult empty_path_rejected =
        refine_policy_exact(empty_path, request());
    REFINE_CHECK(
        empty_path_rejected.status ==
        RefinementStatus::InvalidContract);
    REFINE_CHECK(!empty_path_rejected.executable);

    FixtureOracle omitted_longest =
        make_preservation_fixture(false);
    const ActionRefinementContract preserve =
        action_contract({}, true, false);
    omitted_longest.selections.at({10}).ordered_program = {
        preserve, preserve};
    omitted_longest.selections.at({10}).execution_paths = {
        {preserve}};
    const RefinementResult omitted_longest_rejected =
        refine_policy_exact(omitted_longest, request());
    REFINE_CHECK(
        omitted_longest_rejected.status ==
        RefinementStatus::InvalidContract);
    REFINE_CHECK(!omitted_longest_rejected.executable);

    FixtureOracle inconsistent_path =
        make_preservation_fixture(false);
    inconsistent_path.selections.at({10}).ordered_program = {
        preserve};
    inconsistent_path.selections.at({11}).ordered_program = {
        preserve, preserve};
    inconsistent_path.selections.at({10}).execution_paths = {
        {preserve}};
    inconsistent_path.selections.at({11}).execution_paths = {
        {preserve, preserve}};
    const RefinementResult inconsistent_path_rejected =
        refine_policy_exact(inconsistent_path, request());
    REFINE_CHECK(
        inconsistent_path_rejected.status ==
        RefinementStatus::InvalidContract);
    REFINE_CHECK(!inconsistent_path_rejected.executable);

    FixtureOracle incomplete_inner =
        make_preservation_fixture(false);
    ActionRefinementContract incomplete_runtime = preserve;
    incomplete_runtime.schema_version = 0;
    incomplete_inner.selections.at({10}).ordered_program = {
        preserve};
    incomplete_inner.selections.at({10}).execution_paths = {
        {preserve}, {incomplete_runtime}};
    const RefinementResult incomplete_inner_rejected =
        refine_policy_exact(incomplete_inner, request());
    REFINE_CHECK(
        incomplete_inner_rejected.status ==
        RefinementStatus::InvalidContract);
    REFINE_CHECK(!incomplete_inner_rejected.executable);

    FixtureOracle reversed_alternatives =
        make_preservation_fixture(false);
    const ActionRefinementContract destroy =
        action_contract({}, false, true);
    for (const StableKey& key : {StableKey{10}, StableKey{11}}) {
        reversed_alternatives.selections.at(key).ordered_program = {
            preserve, destroy};
    }
    reversed_alternatives.selections.at({10}).execution_paths = {
        {preserve}, {preserve, destroy}};
    reversed_alternatives.selections.at({11}).execution_paths = {
        {preserve, destroy}, {preserve}};
    const RefinementResult reversed_alternatives_result =
        refine_policy_exact(reversed_alternatives, request());
    REFINE_CHECK(
        reversed_alternatives_result.status ==
        RefinementStatus::Complete);
    REFINE_CHECK(reversed_alternatives_result.executable);

    FixtureOracle capped = make_preservation_fixture(false);
    RefinementRequest limited = request();
    limited.limits.max_exact_states = 1;
    const RefinementResult cap =
        refine_policy_exact(capped, limited);
    REFINE_CHECK(cap.status == RefinementStatus::ResourceCap);
    REFINE_CHECK(cap.resource_cap == "max_exact_states");
}

} // namespace

void run_solver_refinement_tests() {
    run_unset_metamod_observation_parity_tests();
    run_abstract_adapter_tests();
    run_closed_partition_cycle_merge_tests();
    run_closed_partition_delayed_split_tests();
    run_closed_partition_order_tests();
    run_closed_partition_labeled_arc_tests();
    run_closed_partition_memory_cap_tests();
    run_observation_and_collapse_tests();
    run_semantic_coarse_parent_identity_tests();
    run_fixed_point_tests();
    run_selected_action_split_tests();
    run_improper_policy_evaluation_tests();
    run_zero_probability_policy_evaluation_tests();
    run_refinement_memory_accounting_tests();
    run_policy_evaluation_memory_cap_tests();
    run_rejection_and_cap_tests();
}
