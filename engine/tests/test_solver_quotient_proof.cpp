#include "tests.hpp"

#include "../src/solver_quotient_proof.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;
using namespace poecraft::solver::quotient;

namespace {

CertifiedRowIdentity make_identity(const std::uint64_t seed = 0) {
    refinement::SelectedAction action;
    action.action_id = 17;
    action.semantic_key = {200 + seed, 201};
    action.contract.schema_version = kActionRefinementContractVersion;

    CertifiedRowIdentity identity;
    identity.source_coarse_parent = {10 + seed, 11};
    identity.observation_requirement.item_features =
        refinement_feature(RefinementFeature::Rarity);
    identity.observation_requirement.modifier_tag_ids = {9, 3};
    RefinementAffixObservation affix;
    affix.features =
        refinement_feature(RefinementFeature::ModifierSide);
    affix.selector.required_affix_traits = kRefinementAffixPrefix;
    affix.selector.required_tag_ids = {8, 2};
    identity.observation_requirement.affix_observations.push_back(affix);
    refinement::FeatureAtom feature;
    feature.feature = RefinementFeature::Rarity;
    feature.value = {PC_RARITY_RARE};
    identity.observed_features.push_back(feature);
    identity.action_id = action.action_id;
    identity.semantic_action_identity = action.semantic_key;
    identity.runtime_contract_program_identity =
        refinement::canonical_selected_runtime_contract_identity(action);
    identity.exact_choice_recipe_identity = {300 + seed};
    identity.session_identity = {400 + seed};
    identity.layout_identity = {500 + seed};
    identity.goal_identity = {600 + seed};
    identity.artifact_identity = {700 + seed};
    identity.start_identity = {800 + seed};
    identity.solver_options_identity = {900 + seed};
    identity.coverage.strict_kernel_identity = {1000 + seed};
    identity.coverage.replay_authority_identity = {1100 + seed};
    identity.coverage.normalized_enumeration_identity = {1200 + seed};
    identity.coverage.ranges = {
        CoverageRange{{1300 + seed}, 2, 2, 0.5},
        CoverageRange{{1300 + seed}, 0, 2, 0.5}};
    identity.coverage.exact_source_count = 4;
    identity.coverage.exact_total_probability = 1.0;
    identity.exact_total_probability = 1.0;
    identity.projected_arcs = {
        ProofProjectedArc{{2}, {1500 + seed}, 0.75},
        ProofProjectedArc{{1}, {1400 + seed}, 0.25}};
    return identity;
}

CoverageReplayFunction make_replay() {
    return [](const CoverageRange& range) {
        std::vector<CoverageCarrier> carriers;
        for (std::uint64_t offset = 0; offset < range.count; ++offset) {
            carriers.push_back(
                CoverageCarrier{
                    {2000 + range.begin + offset},
                    range.range_identity,
                    range.begin + offset,
                    0.25});
        }
        /* Normalization must make callback enumeration order irrelevant. */
        std::reverse(carriers.begin(), carriers.end());
        return carriers;
    };
}

ProofValidationContext validation_context(
        const std::uint64_t source_generation,
        std::map<std::uint32_t, std::uint64_t> targets,
        const std::uint64_t action_generation = 3,
        const std::uint64_t admission_generation = 5) {
    ProofValidationContext context;
    context.source_generation = source_generation;
    context.target_generation =
        [targets = std::move(targets)](const std::uint32_t cell)
            -> std::optional<std::uint64_t> {
            const auto found = targets.find(cell);
            if (found == targets.end()) return std::nullopt;
            return found->second;
        };
    context.action_generation = action_generation;
    context.admission_generation = admission_generation;
    return context;
}

UnresolvedAlternativeObligationIdentity make_obligation_identity(
        const std::uint64_t seed = 0,
        const double scheduling_priority = 0.0,
        const bool trivial_lower = false) {
    const CertifiedRowIdentity row = make_identity(seed);
    UnresolvedAlternativeObligationIdentity identity;
    identity.source_cell_id = static_cast<std::uint32_t>(40 + seed);
    identity.source_cell_identity = {5000 + seed};
    identity.observation_requirement = row.observation_requirement;
    identity.action.action_id = static_cast<std::uint32_t>(17 + seed);
    identity.action.semantic_action_identity = {5100 + seed};
    identity.action.runtime_contract_program_identity = {5200 + seed};
    identity.action.exact_choice_recipe_identity = {5300 + seed};
    identity.price_identity = {5400 + seed};
    identity.vocabulary_identity = {5500 + seed};
    identity.requirement_generation = 1;
    identity.source_generation = 2;
    identity.target_generation = 3;
    identity.partition_generation = 4;
    identity.action_generation = 5;
    identity.admission_generation = 6;
    identity.price_generation = 7;
    identity.vocabulary_generation = 8;
    if (trivial_lower) {
        identity.optimistic_lower = trivial_carrier_wide_lower_q(
            identity.source_cell_identity, row.coverage);
    } else {
        identity.optimistic_lower = certify_carrier_wide_lower_q(
            identity.source_cell_identity,
            row.coverage,
            {5600 + seed},
            {
                {{1300 + seed}, 0, 14.0 + seed},
                {{1300 + seed}, 1, 12.0 + seed},
                {{1300 + seed}, 2, 10.0 + seed},
                {{1300 + seed}, 3, 16.0 + seed},
            });
    }
    identity.scheduling_priority = scheduling_priority;
    identity.resumable_work_identity = {5700 + seed};
    return canonical_unresolved_alternative_obligation_identity(
        std::move(identity));
}

AlternativeObligationValidationContext obligation_context(
        const UnresolvedAlternativeObligationIdentity& identity,
        const std::uint64_t q_generation = 9) {
    AlternativeObligationValidationContext context;
    context.source_cell_identity = identity.source_cell_identity;
    context.price_identity = identity.price_identity;
    context.vocabulary_identity = identity.vocabulary_identity;
    context.requirement_generation = identity.requirement_generation;
    context.source_generation = identity.source_generation;
    context.target_generation = identity.target_generation;
    context.partition_generation = identity.partition_generation;
    context.action_generation = identity.action_generation;
    context.admission_generation = identity.admission_generation;
    context.price_generation = identity.price_generation;
    context.vocabulary_generation = identity.vocabulary_generation;
    context.q_generation = q_generation;
    return context;
}

void check_distinct_payload(
        ProofStore& store,
        const CertifiedRowIdentity& baseline,
        CertifiedRowIdentity changed) {
    const auto [base_id, base_reused] = store.intern_payload(baseline);
    const auto [changed_id, changed_reused] =
        store.intern_payload(std::move(changed));
    PC_CHECK(!base_reused || base_id == 0);
    PC_CHECK(!changed_reused);
    PC_CHECK(base_id != changed_id);
}

void run_collision_and_identity_tests() {
    ProofStore collision_store;
    const CertifiedRowIdentity first = make_identity();
    CertifiedRowIdentity second = first;
    second.goal_identity = {99999};
    const auto [first_id, first_reused] =
        collision_store.intern_payload(first, 7);
    const auto [second_id, second_reused] =
        collision_store.intern_payload(second, 7);
    const auto [first_again, first_again_reused] =
        collision_store.intern_payload(first, 7);
    PC_CHECK(!first_reused);
    PC_CHECK(!second_reused);
    PC_CHECK(first_id != second_id);
    PC_CHECK(first_again_reused);
    PC_CHECK(first_again == first_id);
    PC_CHECK(
        collision_store.payload(first_id).identity !=
        collision_store.payload(second_id).identity);

    const CertifiedRowIdentity baseline = make_identity();
    {
        ProofStore store;
        CertifiedRowIdentity changed = baseline;
        changed.observation_requirement.item_features |=
            refinement_feature(RefinementFeature::PrefixCount);
        check_distinct_payload(store, baseline, std::move(changed));
    }
    {
        ProofStore store;
        CertifiedRowIdentity changed = baseline;
        changed.runtime_contract_program_identity.push_back(45);
        check_distinct_payload(store, baseline, std::move(changed));
    }
    {
        ProofStore store;
        CertifiedRowIdentity changed = baseline;
        ++changed.action_id;
        changed.semantic_action_identity.push_back(46);
        check_distinct_payload(store, baseline, std::move(changed));
    }
    {
        ProofStore store;
        CertifiedRowIdentity changed = baseline;
        changed.exact_choice_recipe_identity.push_back(47);
        check_distinct_payload(store, baseline, std::move(changed));
    }
    {
        ProofStore store;
        CertifiedRowIdentity changed = baseline;
        changed.coverage.strict_kernel_identity.push_back(48);
        check_distinct_payload(store, baseline, std::move(changed));
    }
    for (const int mismatch : {0, 1, 2, 3, 4, 5}) {
        ProofStore store;
        CertifiedRowIdentity changed = baseline;
        switch (mismatch) {
        case 0: changed.session_identity.push_back(50); break;
        case 1: changed.layout_identity.push_back(51); break;
        case 2: changed.goal_identity.push_back(52); break;
        case 3: changed.artifact_identity.push_back(53); break;
        case 4: changed.start_identity.push_back(54); break;
        case 5: changed.solver_options_identity.push_back(55); break;
        }
        check_distinct_payload(store, baseline, std::move(changed));
    }
}

void run_dependency_generation_tests() {
    ProofStore store;
    const CertifiedRowIdentity identity = make_identity();
    const std::uint32_t payload_id = store.intern_payload(identity).first;
    store.attach_row(10, payload_id, 4, 1, {{7, 2}}, 3, 5);
    PC_CHECK(
        store.validate_row(
            10, identity, validation_context(1, {{7, 2}})) ==
        ProofValidationStatus::Current);
    PC_CHECK(
        store.validate_row(
            10, identity, validation_context(2, {{7, 2}})) ==
        ProofValidationStatus::StaleSourceGeneration);
    PC_CHECK(
        store.validate_row(
            10, identity, validation_context(1, {{7, 3}})) ==
        ProofValidationStatus::StaleTargetGeneration);
    PC_CHECK(
        store.validate_row(
            10, identity, validation_context(1, {{7, 2}}, 4, 5)) ==
        ProofValidationStatus::StaleActionGeneration);
    PC_CHECK(
        store.validate_row(
            10, identity, validation_context(1, {{7, 2}}, 3, 6)) ==
        ProofValidationStatus::StaleAdmissionGeneration);

    CertifiedRowIdentity wrong = identity;
    wrong.goal_identity = {8888};
    PC_CHECK(
        store.validate_row(
            10, wrong, validation_context(1, {{7, 2}})) ==
        ProofValidationStatus::FullKeyMismatch);

    PC_CHECK(store.invalidate_source(4) == 1);
    PC_CHECK(!store.use_site(10).valid);
    store.attach_row(10, payload_id, 5, 2, {{7, 2}}, 3, 5);
    PC_CHECK(store.use_site(10).source_cell_id == 5);
    PC_CHECK(store.use_site(10).payload_id == payload_id);
    PC_CHECK(
        store.validate_row(
            10, identity, validation_context(2, {{7, 2}})) ==
        ProofValidationStatus::Current);

    store.attach_row(11, payload_id, 6, 1, {{7, 2}}, 3, 5);
    store.attach_row(12, payload_id, 6, 1, {{8, 9}}, 3, 5);
    PC_CHECK(store.invalidate_target(7) == 2);
    PC_CHECK(!store.use_site(10).valid);
    PC_CHECK(!store.use_site(11).valid);
    PC_CHECK(store.use_site(12).valid);

    store.attach_row(20, payload_id, 20, 1, {{20, 1}}, 3, 5);
    store.attach_row(21, payload_id, 21, 9, {{21, 4}}, 3, 5);
    PC_CHECK(store.payload_count() == 1);
    PC_CHECK(store.use_site(20).payload_id == store.use_site(21).payload_id);
    PC_CHECK(store.invalidate_source(20) == 1);
    PC_CHECK(!store.use_site(20).valid);
    PC_CHECK(store.use_site(21).valid);

    store.attach_row(30, payload_id, 30, 1, {{30, 1}}, 3, 5);
    PC_CHECK(store.invalidate_action_generation(4) == 3);
    store.attach_row(31, payload_id, 31, 1, {{31, 1}}, 4, 5);
    PC_CHECK(store.invalidate_admission_generation(6) == 1);

    store.attach_row(40, payload_id, 40, 1, {{40, 1}}, 4, 6);
    const std::uint64_t old_price = store.price_generation();
    const std::uint64_t old_q = store.q_generation();
    const std::uint64_t old_policy = store.policy_generation();
    store.note_price_change();
    PC_CHECK(store.price_generation() == old_price + 1);
    PC_CHECK(store.q_generation() == old_q + 1);
    PC_CHECK(store.policy_generation() == old_policy + 1);
    PC_CHECK(store.use_site(40).valid);
    PC_CHECK(
        store.validate_row(
            40, identity,
            validation_context(1, {{40, 1}}, 4, 6)) ==
        ProofValidationStatus::Current);
}

void run_coverage_replay_tests() {
    ProofStore store;
    const CertifiedRowIdentity identity = make_identity();
    const std::uint32_t payload_id = store.intern_payload(identity).first;
    store.attach_row(0, payload_id, 0, 1, {{1, 1}}, 1, 1);
    const CoverageReplayFunction replay = make_replay();
    CoverageReplaySlice first = store.replay_row_coverage(
        0, identity.coverage.replay_authority_identity, replay);
    CoverageReplaySlice second = store.replay_row_coverage(
        0, identity.coverage.replay_authority_identity, replay);
    PC_CHECK(first.carriers() == second.carriers());
    PC_CHECK(first.carriers().size() == 4);
    PC_CHECK(first.carriers().front().stable_key == StableKey{2000});
    const ProofMemorySnapshot live = store.ledger().snapshot();
    PC_CHECK(live.live_slice_count == 2);
    PC_CHECK(live.peak_live_slice_count == 2);
    PC_CHECK(live.live_slice_bytes ==
             first.charged_bytes() + second.charged_bytes());
    first.reset();
    PC_CHECK(store.ledger().snapshot().live_slice_count == 1);
    second.reset();
    PC_CHECK(store.ledger().snapshot().live_slice_count == 0);
    PC_CHECK(store.ledger().snapshot().live_slice_bytes == 0);

    bool authority_rejected = false;
    try {
        (void)store.replay_row_coverage(0, {999}, replay);
    } catch (const std::invalid_argument&) {
        authority_rejected = true;
    }
    PC_CHECK(authority_rejected);

    bool corrupt_count_rejected = false;
    try {
        CoverageDescriptor corrupt = identity.coverage;
        ++corrupt.exact_source_count;
        (void)canonical_coverage_descriptor(std::move(corrupt));
    } catch (const std::invalid_argument&) {
        corrupt_count_rejected = true;
    }
    PC_CHECK(corrupt_count_rejected);

    bool corrupt_mass_rejected = false;
    try {
        CertifiedRowIdentity corrupt = identity;
        corrupt.projected_arcs[0].probability = 0.5;
        (void)canonical_certified_row_identity(std::move(corrupt));
    } catch (const std::invalid_argument&) {
        corrupt_mass_rejected = true;
    }
    PC_CHECK(corrupt_mass_rejected);
}

void run_alternative_lower_and_collision_tests() {
    const CertifiedRowIdentity row = make_identity();
    const StableKey source_cell = {5000};
    const CarrierWideOptimisticLowerQ lower =
        certify_carrier_wide_lower_q(
            source_cell,
            row.coverage,
            {5600},
            {
                {{1300}, 3, 40.0},
                {{1300}, 1, 20.0},
                {{1300}, 0, 10.0},
                {{1300}, 2, 30.0},
            });
    PC_CHECK(lower.lower_q() == 10.0);
    PC_CHECK(lower.lower_q() != 25.0);
    PC_CHECK(lower.exact_carriers_covered() == 4);
    PC_CHECK(lower.exact_probability_mass() == 1.0);
    const CarrierWideOptimisticLowerQ trivial =
        trivial_carrier_wide_lower_q(source_cell, row.coverage);
    PC_CHECK(trivial.lower_q() == 0.0);
    PC_CHECK(
        trivial.kind() ==
        OptimisticLowerQProvenanceKind::TrivialNonnegativeCost);

    bool incomplete_rejected = false;
    try {
        (void)certify_carrier_wide_lower_q(
            source_cell,
            row.coverage,
            {5600},
            {
                {{1300}, 0, 10.0},
                {{1300}, 1, 20.0},
                {{1300}, 2, 30.0},
            });
    } catch (const std::invalid_argument&) {
        incomplete_rejected = true;
    }
    PC_CHECK(incomplete_rejected);
    bool duplicate_rejected = false;
    try {
        (void)certify_carrier_wide_lower_q(
            source_cell,
            row.coverage,
            {5600},
            {
                {{1300}, 0, 10.0},
                {{1300}, 1, 20.0},
                {{1300}, 1, 30.0},
                {{1300}, 3, 40.0},
            });
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    PC_CHECK(duplicate_rejected);

    ProofStore store;
    const UnresolvedAlternativeObligationIdentity first =
        make_obligation_identity();
    UnresolvedAlternativeObligationIdentity collision = first;
    collision.action.exact_choice_recipe_identity = {99999};
    const auto [first_id, first_reused] =
        store.intern_alternative_obligation(first, 77);
    const auto [collision_id, collision_reused] =
        store.intern_alternative_obligation(collision, 77);
    const auto [first_again, first_again_reused] =
        store.intern_alternative_obligation(first, 77);
    PC_CHECK(!first_reused);
    PC_CHECK(!collision_reused);
    PC_CHECK(first_id != collision_id);
    PC_CHECK(first_again_reused);
    PC_CHECK(first_again == first_id);
    PC_CHECK(
        store.alternative_obligation(first_id).semantic_hash !=
        store.alternative_obligation(collision_id).semantic_hash);
}

void run_alternative_lifecycle_and_invalidation_tests() {
    ProofStore store;
    const UnresolvedAlternativeObligationIdentity identity =
        make_obligation_identity();
    const std::uint32_t id =
        store.intern_alternative_obligation(identity).first;
    PC_CHECK(
        store.alternative_obligation(id).status ==
        AlternativeObligationStatus::Unscheduled);
    PC_CHECK(!store.alternative_obligation_supports_executable_upper(id));
    PC_CHECK(store.alternative_obligation_blocks_exactness(id, 9.0, 9));
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::LowerOnly);
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::Scheduled);
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::PartiallyEvaluated, 10);
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::ResourceInterrupted, 11);
    PC_CHECK(
        std::string(alternative_obligation_status_name(
            store.alternative_obligation(id).status)) ==
        "resource_interrupted");
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::Scheduled, 11);
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::Certified, 12, 42);
    PC_CHECK(store.alternative_obligation_supports_executable_upper(id));
    PC_CHECK(!store.alternative_obligation_blocks_exactness(id, 9.0, 9));
    store.transition_alternative_obligation(
        id, AlternativeObligationStatus::Stale);
    PC_CHECK(store.alternative_obligation(id).work_completed == 12);
    PC_CHECK(!store.alternative_obligation_supports_executable_upper(id));
    PC_CHECK(store.alternative_obligation_blocks_exactness(id, 9.0, 9));

    const UnresolvedAlternativeObligationIdentity deferred_identity =
        make_obligation_identity(1);
    const std::uint32_t deferred =
        store.intern_alternative_obligation(deferred_identity).first;
    store.transition_alternative_obligation(
        deferred, AlternativeObligationStatus::LowerOnly);
    store.transition_alternative_obligation(
        deferred,
        AlternativeObligationStatus::ConditionallyNoncompetitive,
        0,
        std::nullopt,
        10.5,
        9);
    PC_CHECK(!store.alternative_obligation_supports_executable_upper(deferred));
    PC_CHECK(!store.alternative_obligation_blocks_exactness(
        deferred, 10.5, 9));
    PC_CHECK(store.alternative_obligation_blocks_exactness(
        deferred, 10.5, 10));
    PC_CHECK(store.alternative_obligation_blocks_exactness(
        deferred, 11.5, 9));

    const AlternativeObligationValidationContext current =
        obligation_context(deferred_identity, 9);
    PC_CHECK(
        store.validate_alternative_obligation(
            deferred, deferred_identity, current) ==
        AlternativeObligationValidationStatus::Current);
    const auto expect_context_stale = [&](const auto mutate,
                                          const auto expected) {
        AlternativeObligationValidationContext changed = current;
        mutate(changed);
        PC_CHECK(
            store.validate_alternative_obligation(
                deferred, deferred_identity, changed) == expected);
    };
    expect_context_stale(
        [](auto& value) { value.source_cell_identity.push_back(1); },
        AlternativeObligationValidationStatus::StaleSourceIdentity);
    expect_context_stale(
        [](auto& value) { value.price_identity.push_back(1); },
        AlternativeObligationValidationStatus::StalePriceIdentity);
    expect_context_stale(
        [](auto& value) { value.vocabulary_identity.push_back(1); },
        AlternativeObligationValidationStatus::StaleVocabularyIdentity);
    expect_context_stale(
        [](auto& value) { ++value.requirement_generation; },
        AlternativeObligationValidationStatus::StaleRequirementGeneration);
    expect_context_stale(
        [](auto& value) { ++value.source_generation; },
        AlternativeObligationValidationStatus::StaleSourceGeneration);
    expect_context_stale(
        [](auto& value) { ++value.target_generation; },
        AlternativeObligationValidationStatus::StaleTargetGeneration);
    expect_context_stale(
        [](auto& value) { ++value.partition_generation; },
        AlternativeObligationValidationStatus::StalePartitionGeneration);
    expect_context_stale(
        [](auto& value) { ++value.action_generation; },
        AlternativeObligationValidationStatus::StaleActionGeneration);
    expect_context_stale(
        [](auto& value) { ++value.admission_generation; },
        AlternativeObligationValidationStatus::StaleAdmissionGeneration);
    expect_context_stale(
        [](auto& value) { ++value.price_generation; },
        AlternativeObligationValidationStatus::StalePriceGeneration);
    expect_context_stale(
        [](auto& value) { ++value.vocabulary_generation; },
        AlternativeObligationValidationStatus::StaleVocabularyGeneration);
    expect_context_stale(
        [](auto& value) { ++value.q_generation; },
        AlternativeObligationValidationStatus::StaleConditionalQGeneration);
    UnresolvedAlternativeObligationIdentity wrong = deferred_identity;
    wrong.action.exact_choice_recipe_identity.push_back(9);
    PC_CHECK(
        store.validate_alternative_obligation(
            deferred, wrong, current) ==
        AlternativeObligationValidationStatus::FullKeyMismatch);
    store.transition_alternative_obligation(
        deferred, AlternativeObligationStatus::Stale);
    PC_CHECK(
        store.validate_alternative_obligation(
            deferred, deferred_identity, current) ==
        AlternativeObligationValidationStatus::StaleLifecycle);
}

AccountedAlternativeAction accounting_entry(
    const UnresolvedAlternativeObligationIdentity& identity,
    AlternativeActionAccountingKind kind,
    std::optional<std::uint32_t> obligation_id = std::nullopt,
    std::optional<std::uint64_t> row_id = std::nullopt);

void run_alternative_verdict_revocation_tests() {
    using Mutator = std::function<void(
        UnresolvedAlternativeObligationIdentity&,
        AlternativeObligationValidationContext&)>;
    const std::vector<std::pair<
        AlternativeObligationValidationStatus, Mutator>> cases = {
        {
            AlternativeObligationValidationStatus::
                StaleSourceIdentity,
            [](auto& identity, auto& context) {
                ++identity.source_cell_id;
                identity.source_cell_identity.push_back(9001);
                ++identity.source_generation;
                ++identity.partition_generation;
                identity.optimistic_lower =
                    trivial_carrier_wide_lower_q(
                        identity.source_cell_identity,
                        make_identity(30).coverage);
                context = obligation_context(identity, 31);
            },
        },
        {
            AlternativeObligationValidationStatus::
                StaleTargetGeneration,
            [](auto& identity, auto& context) {
                ++identity.target_generation;
                context = obligation_context(identity, 31);
            },
        },
        {
            AlternativeObligationValidationStatus::
                StaleRequirementGeneration,
            [](auto& identity, auto& context) {
                identity.observation_requirement.modifier_tag_ids
                    .push_back(9003);
                ++identity.requirement_generation;
                context = obligation_context(identity, 31);
            },
        },
        {
            AlternativeObligationValidationStatus::StalePriceIdentity,
            [](auto& identity, auto& context) {
                identity.price_identity.push_back(9004);
                ++identity.price_generation;
                context = obligation_context(identity, 31);
            },
        },
        {
            AlternativeObligationValidationStatus::
                StaleVocabularyIdentity,
            [](auto& identity, auto& context) {
                identity.vocabulary_identity.push_back(9005);
                ++identity.vocabulary_generation;
                context = obligation_context(identity, 31);
            },
        },
    };

    for (std::size_t index = 0; index < cases.size(); ++index) {
        ProofStore store;
        const UnresolvedAlternativeObligationIdentity original =
            make_obligation_identity(30 + index);
        const std::uint32_t original_id =
            store.intern_alternative_obligation(original).first;
        store.transition_alternative_obligation(
            original_id, AlternativeObligationStatus::LowerOnly);
        store.transition_alternative_obligation(
            original_id,
            AlternativeObligationStatus::ConditionallyNoncompetitive,
            0, std::nullopt,
            original.optimistic_lower.lower_q(), 30);
        PC_CHECK(!store.alternative_obligation_blocks_exactness(
            original_id,
            original.optimistic_lower.lower_q(), 30));

        UnresolvedAlternativeObligationIdentity replacement = original;
        AlternativeObligationValidationContext replacement_context =
            obligation_context(replacement, 31);
        cases[index].second(replacement, replacement_context);
        replacement = canonical_unresolved_alternative_obligation_identity(
            std::move(replacement));
        PC_CHECK(
            store.validate_alternative_obligation(
                original_id, original, replacement_context) ==
            cases[index].first);

        store.transition_alternative_obligation(
            original_id, AlternativeObligationStatus::Stale);
        const std::uint32_t replacement_id =
            store.intern_alternative_obligation(replacement).first;
        store.transition_alternative_obligation(
            replacement_id, AlternativeObligationStatus::LowerOnly);
        store.transition_alternative_obligation(
            replacement_id, AlternativeObligationStatus::Scheduled);
        PC_CHECK((
            store.ordered_pending_alternative_obligations() ==
            std::vector<std::uint32_t>{replacement_id}));
        PC_CHECK(store.alternative_obligation_blocks_exactness(
            replacement_id,
            replacement.optimistic_lower.lower_q(), 31));

        const std::vector<AccountedAlternativeAction> admitted = {
            accounting_entry(
                replacement,
                AlternativeActionAccountingKind::
                    UnresolvedObligation),
        };
        const std::vector<AccountedAlternativeAction> accounted = {
            accounting_entry(
                replacement,
                AlternativeActionAccountingKind::
                    UnresolvedObligation,
                replacement_id),
        };
        const AlternativeActionAccountingAudit audit =
            store.audit_alternative_actions(
                admitted, accounted,
                {{replacement.source_cell_id,
                  replacement.optimistic_lower.lower_q()}},
                31);
        PC_CHECK(audit.complete);
        PC_CHECK(!audit.exact_alternative_envelope_closed);
        PC_CHECK(audit.unresolved_actions == 1);
    }
}

AccountedAlternativeAction accounting_entry(
        const UnresolvedAlternativeObligationIdentity& identity,
        const AlternativeActionAccountingKind kind,
        const std::optional<std::uint32_t> obligation_id,
        const std::optional<std::uint64_t> row_id) {
    return {
        identity.source_cell_id,
        identity.action,
        kind,
        row_id,
        obligation_id};
}

void run_alternative_order_and_accounting_tests() {
    ProofStore store;
    const auto selected = make_obligation_identity(10);
    const auto certified = make_obligation_identity(11);
    const auto unresolved = make_obligation_identity(12, 2.0);
    const auto noncompetitive = make_obligation_identity(13, 3.0);
    const auto lower_priority = make_obligation_identity(14, 1.0);
    const std::uint32_t unresolved_id =
        store.intern_alternative_obligation(unresolved).first;
    const std::uint32_t noncompetitive_id =
        store.intern_alternative_obligation(noncompetitive).first;
    const std::uint32_t lower_priority_id =
        store.intern_alternative_obligation(lower_priority).first;
    PC_CHECK((
        store.ordered_pending_alternative_obligations() ==
        std::vector<std::uint32_t>{
            noncompetitive_id, unresolved_id, lower_priority_id}));

    store.transition_alternative_obligation(
        noncompetitive_id, AlternativeObligationStatus::LowerOnly);
    store.transition_alternative_obligation(
        noncompetitive_id,
        AlternativeObligationStatus::ConditionallyNoncompetitive,
        0,
        std::nullopt,
        noncompetitive.optimistic_lower.lower_q(),
        22);

    std::vector<AccountedAlternativeAction> admitted = {
        accounting_entry(
            selected,
            AlternativeActionAccountingKind::UnresolvedObligation),
        accounting_entry(
            certified,
            AlternativeActionAccountingKind::UnresolvedObligation),
        accounting_entry(
            unresolved,
            AlternativeActionAccountingKind::UnresolvedObligation),
        accounting_entry(
            noncompetitive,
            AlternativeActionAccountingKind::UnresolvedObligation),
    };
    std::vector<AccountedAlternativeAction> accounted = {
        accounting_entry(
            selected,
            AlternativeActionAccountingKind::CurrentSelectedCertified,
            std::nullopt,
            100),
        accounting_entry(
            certified,
            AlternativeActionAccountingKind::OtherCertified,
            std::nullopt,
            101),
        accounting_entry(
            unresolved,
            AlternativeActionAccountingKind::UnresolvedObligation,
            unresolved_id),
        accounting_entry(
            noncompetitive,
            AlternativeActionAccountingKind::UnresolvedObligation,
            noncompetitive_id),
    };
    AlternativeActionAccountingAudit audit =
        store.audit_alternative_actions(
            admitted,
            accounted,
            {{noncompetitive.source_cell_id,
              noncompetitive.optimistic_lower.lower_q()}},
            22);
    PC_CHECK(audit.complete);
    PC_CHECK(!audit.exact_alternative_envelope_closed);
    PC_CHECK(audit.selected_certified_actions == 1);
    PC_CHECK(audit.other_certified_actions == 1);
    PC_CHECK(audit.unresolved_actions == 1);
    PC_CHECK(audit.conditionally_noncompetitive_actions == 1);

    store.transition_alternative_obligation(
        unresolved_id, AlternativeObligationStatus::LowerOnly);
    store.transition_alternative_obligation(
        unresolved_id,
        AlternativeObligationStatus::ConditionallyNoncompetitive,
        0,
        std::nullopt,
        unresolved.optimistic_lower.lower_q(),
        22);
    audit = store.audit_alternative_actions(
        admitted,
        accounted,
        {{unresolved.source_cell_id,
          unresolved.optimistic_lower.lower_q()},
         {noncompetitive.source_cell_id,
          noncompetitive.optimistic_lower.lower_q()}},
        22);
    PC_CHECK(audit.complete);
    PC_CHECK(audit.exact_alternative_envelope_closed);
    PC_CHECK(audit.unresolved_actions == 0);
    PC_CHECK(audit.conditionally_noncompetitive_actions == 2);

    accounted.erase(accounted.begin() + 2);
    audit = store.audit_alternative_actions(
        admitted,
        accounted,
        {{noncompetitive.source_cell_id,
          noncompetitive.optimistic_lower.lower_q()}},
        22);
    PC_CHECK(!audit.complete);
    PC_CHECK(audit.unaccounted_actions == 1);

    admitted.push_back(admitted.front());
    audit = store.audit_alternative_actions(
        admitted,
        accounted,
        {{noncompetitive.source_cell_id,
          noncompetitive.optimistic_lower.lower_q()}},
        22);
    PC_CHECK(audit.duplicate_admissions == 1);
}

std::array<std::uint64_t, kProofMemoryCategoryCount>
independent_store_bytes(
        const ProofStoreStorageStats& stats) {
    std::array<std::uint64_t, kProofMemoryCategoryCount> expected{};
    const auto index = [](const ProofMemoryCategory category) {
        return static_cast<std::size_t>(category);
    };
    std::uint64_t payload = sizeof(ProofStore);
    payload += stats.payload_pointer_capacity *
               sizeof(std::shared_ptr<const CertifiedRowPayload>);
    payload += stats.payload_bucket_capacity *
               sizeof(ProofPayloadHashBucket);
    payload += stats.payload_bucket_id_capacity * sizeof(std::uint32_t);
    payload += stats.payload_object_count * sizeof(CertifiedRowPayload);
    payload += stats.payload_key_u64_capacity * sizeof(std::uint64_t);
    payload += stats.requirement_tag_capacity * sizeof(std::uint32_t);
    payload += stats.requirement_affix_capacity *
               sizeof(RefinementAffixObservation);
    payload += stats.requirement_selector_tag_capacity * sizeof(std::uint32_t);
    payload += stats.feature_capacity * sizeof(refinement::FeatureAtom);
    payload += stats.feature_value_u64_capacity * sizeof(std::uint64_t);
    payload += stats.feature_tag_capacity * sizeof(std::uint32_t);
    payload += stats.arc_capacity * sizeof(ProofProjectedArc);
    payload += stats.arc_key_u64_capacity * sizeof(std::uint64_t);
    payload += stats.projected_arc_bucket_count *
               (sizeof(std::pair<
                    const std::uint64_t,
                    std::vector<std::shared_ptr<
                        SharedProjectedArcs::Container>>>) +
                3 * sizeof(void*));
    payload += stats.projected_arc_bucket_pointer_capacity *
               sizeof(std::shared_ptr<SharedProjectedArcs::Container>);
    expected[index(ProofMemoryCategory::ProofPayload)] = payload;
    expected[index(ProofMemoryCategory::Certificate)] =
        stats.use_site_capacity * sizeof(RowProofUseSite);
    expected[index(ProofMemoryCategory::DependencySidecar)] =
        stats.use_target_dependency_capacity *
            sizeof(TargetGenerationDependency) +
        stats.source_index_outer_capacity *
            sizeof(std::vector<std::uint64_t>) +
        stats.source_index_row_capacity * sizeof(std::uint64_t) +
        stats.target_index_outer_capacity *
            sizeof(std::vector<std::uint64_t>) +
        stats.target_index_row_capacity * sizeof(std::uint64_t);
    expected[index(ProofMemoryCategory::CoverageDescriptor)] =
        stats.coverage_range_capacity * sizeof(CoverageRange) +
        stats.coverage_key_u64_capacity * sizeof(std::uint64_t);
    expected[index(ProofMemoryCategory::AlternativeObligation)] =
        stats.obligation_capacity *
            sizeof(UnresolvedAlternativeObligation) +
        stats.obligation_bucket_capacity *
            sizeof(AlternativeObligationHashBucket) +
        stats.obligation_bucket_id_capacity * sizeof(std::uint32_t) +
        stats.obligation_key_u64_capacity * sizeof(std::uint64_t) +
        stats.obligation_shared_key_allocation_capacity *
            sizeof(ObligationSharedKeyAllocation) +
        stats.obligation_shared_key_object_count *
            (sizeof(StableKey) + 2 * sizeof(void*)) +
        stats.obligation_shared_key_u64_capacity * sizeof(std::uint64_t) +
        stats.obligation_requirement_tag_capacity * sizeof(std::uint32_t) +
        stats.obligation_requirement_affix_capacity *
            sizeof(RefinementAffixObservation) +
        stats.obligation_requirement_selector_tag_capacity *
            sizeof(std::uint32_t);
    return expected;
}

void run_ledger_conservation_test() {
    ProofStore store;
    const ProofMemorySnapshot starting = store.ledger().snapshot();
    PC_CHECK(starting.total_bytes == sizeof(ProofStore));

    const CertifiedRowIdentity identity = make_identity();
    const auto [payload_id, reused] = store.intern_payload(identity);
    const std::uint64_t payload_bytes_once =
        store.ledger().snapshot().bytes[static_cast<std::size_t>(
            ProofMemoryCategory::ProofPayload)];
    const auto [shared_id, shared] = store.intern_payload(identity);
    PC_CHECK(!reused);
    PC_CHECK(shared);
    PC_CHECK(payload_id == shared_id);
    PC_CHECK(
        store.ledger().snapshot().bytes[static_cast<std::size_t>(
            ProofMemoryCategory::ProofPayload)] == payload_bytes_once);

    store.attach_row(1, payload_id, 1, 1, {{2, 1}, {3, 1}}, 1, 1);
    store.attach_row(40, payload_id, 4, 2, {{2, 1}, {5, 3}}, 1, 1);
    PC_CHECK(store.valid_use_site_count() == 2);
    PC_CHECK(store.invalidate_target(2) == 2);
    store.attach_row(40, payload_id, 6, 3, {{7, 4}}, 1, 1);
    (void)store.intern_alternative_obligation(make_obligation_identity());

    const ProofStoreStorageStats stats = store.storage_stats();
    const auto expected = independent_store_bytes(stats);
    const ProofMemorySnapshot allocated = store.ledger().snapshot();
    for (const ProofMemoryCategory category : {
             ProofMemoryCategory::ProofPayload,
             ProofMemoryCategory::Certificate,
             ProofMemoryCategory::DependencySidecar,
             ProofMemoryCategory::CoverageDescriptor,
             ProofMemoryCategory::AlternativeObligation}) {
        const std::size_t offset = static_cast<std::size_t>(category);
        PC_CHECK(allocated.bytes[offset] == expected[offset]);
    }
    PC_CHECK(stats.payload_object_count == 1);
    PC_CHECK(stats.use_site_capacity >= 41);
    PC_CHECK(stats.use_target_dependency_capacity >= 3);

    {
        ScopedProofMemoryCharge partition(
            store.ledger(), ProofMemoryCategory::Partition, 111);
        ScopedProofMemoryCharge carriers(
            store.ledger(), ProofMemoryCategory::Carrier, 222);
        ScopedProofMemoryCharge rows(
            store.ledger(), ProofMemoryCategory::RowKernel, 333);
        ScopedProofMemoryCharge scratch(
            store.ledger(), ProofMemoryCategory::Scratch, 444);
        CoverageReplaySlice first = store.replay_row_coverage(
            40, identity.coverage.replay_authority_identity, make_replay());
        CoverageReplaySlice second = store.replay_row_coverage(
            40, identity.coverage.replay_authority_identity, make_replay());
        const ProofMemorySnapshot live = store.ledger().snapshot();
        PC_CHECK(live.bytes[static_cast<std::size_t>(
                     ProofMemoryCategory::Partition)] == 111);
        PC_CHECK(live.bytes[static_cast<std::size_t>(
                     ProofMemoryCategory::Carrier)] == 222);
        PC_CHECK(live.bytes[static_cast<std::size_t>(
                     ProofMemoryCategory::RowKernel)] == 333);
        PC_CHECK(live.bytes[static_cast<std::size_t>(
                     ProofMemoryCategory::Scratch)] == 444);
        PC_CHECK(live.live_slice_count == 2);
        PC_CHECK(live.peak_live_slice_count >= 2);
        PC_CHECK(live.peak_live_slice_bytes >=
                 first.charged_bytes() + second.charged_bytes());
    }
    const ProofMemorySnapshot released = store.ledger().snapshot();
    for (const ProofMemoryCategory category : {
             ProofMemoryCategory::LiveReplaySlice,
             ProofMemoryCategory::Partition,
             ProofMemoryCategory::Carrier,
             ProofMemoryCategory::RowKernel,
             ProofMemoryCategory::Scratch}) {
        PC_CHECK(released.bytes[static_cast<std::size_t>(category)] == 0);
    }
    PC_CHECK(released.live_slice_count == 0);

    store.clear_and_release();
    const ProofMemorySnapshot final = store.ledger().snapshot();
    PC_CHECK(final.bytes == starting.bytes);
    PC_CHECK(final.total_bytes == starting.total_bytes);
    PC_CHECK(final.live_slice_count == starting.live_slice_count);
    PC_CHECK(final.live_slice_bytes == starting.live_slice_bytes);

    ProofMemoryLedger limited(100);
    bool cap_rejected = false;
    try {
        limited.charge(ProofMemoryCategory::Scratch, 101);
    } catch (const ProofMemoryLimit&) {
        cap_rejected = true;
    }
    PC_CHECK(cap_rejected);
    PC_CHECK(limited.snapshot().total_bytes == 0);
}

} // namespace

void run_solver_quotient_proof_tests() {
    run_collision_and_identity_tests();
    run_dependency_generation_tests();
    run_coverage_replay_tests();
    run_alternative_lower_and_collision_tests();
    run_alternative_lifecycle_and_invalidation_tests();
    run_alternative_verdict_revocation_tests();
    run_alternative_order_and_accounting_tests();
    run_ledger_conservation_test();
}
