#pragma once

#include "solver_model.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace poecraft {
namespace solver {

/*
 * Engine-private product vocabulary contract. This table names registry and
 * telemetry identities only; crafting legality and transition mechanics stay
 * with the native action implementations and ActionDescriptor metadata.
 */
enum class RegistryIdentityShape : std::uint8_t {
    Exact = 0,
    Parameterized = 1,
    FossilLoadout = 2,
    HarvestResistPair = 3,
};

enum class ActionCostKeyShape : std::uint8_t {
    Identity = 0,
    FossilComponents = 1,
    HarvestResistTarget = 2,
    Zero = 3,
    Scour = 4,
};

enum class ExpectedPriceProvenance : std::uint8_t {
    Quote = 0,
    Recipe = 1,
    Zero = 2,
    OwnerDefault = 3,
    Manual = 4,
    Mixed = 5,
};

enum class ProductReasonGroup : std::uint8_t {
    Currency = 0,
    Essence = 1,
    Fossil = 2,
    Bench = 3,
    Veiled = 4,
    Harvest = 5,
    Eldritch = 6,
    Influence = 7,
    Fracture = 8,
    Cleanup = 9,
};

enum class CarrierGenerationPath : std::uint8_t {
    NotApplicable = 0,
    RegistryPrimitive = 1,
    RegistryProgramComponent = 2,
    RegistryAndCarrierLocalAutomatic = 3,
    RegistryProductDeferred = 4,
    CarrierLocalAutomatic = 5,
    SeparateOperation = 6,
};

enum class SchedulerAdmissionPath : std::uint8_t {
    NotApplicable = 0,
    GlobalCandidate = 1,
    GoalFilteredCandidate = 2,
    CandidateOrAutomaticDependency = 3,
    AutomaticDependency = 4,
    CarrierLocalAutomatic = 5,
    ProductFilteredDeferred = 6,
    SeparateAutomatic = 7,
};

enum class RowCompletionPath : std::uint8_t {
    NotApplicable = 0,
    SharedPrimitive = 1,
    PrimitiveAndAutomaticOption = 2,
    AuthoredPrimitiveProductDeferred = 3,
    AutomaticOption = 4,
    AutomaticOptionComponent = 5,
};

enum class BellmanQPath : std::uint8_t {
    NotApplicable = 0,
    SharedPrimitive = 1,
    PrimitiveAndAutomaticOption = 2,
    AuthoredPrimitiveProductDeferred = 3,
    AutomaticOption = 4,
    AutomaticOptionComponent = 5,
};

enum class SelectedPolicyPath : std::uint8_t {
    NotApplicable = 0,
    PinnedVerified = 1,
    PinnedComponent = 2,
    PartialPinned = 3,
    AvailableUnpinned = 4,
    IncompleteUnqualified = 5,
    DeferredMechanic = 6,
};

enum class CompilerCoveragePath : std::uint8_t {
    NotApplicable = 0,
    GenericPrimitive = 1,
    ObservedChoicePrimitive = 2,
    CompoundAutomatic = 3,
    DedicatedSeparateOperation = 4,
};

enum class ExactEvaluatorCoveragePath : std::uint8_t {
    NotApplicable = 0,
    SharedPrimitive = 1,
    FamilySpecificPrimitive = 2,
    ObservedChoiceMechanicContradiction = 3,
    CompoundAutomatic = 4,
    DedicatedSeparateOperation = 5,
};

enum class SimulatorCoveragePath : std::uint8_t {
    NotApplicable = 0,
    NativePrimitive = 1,
    CompiledAutomaticProgram = 2,
    DedicatedSeparateOperation = 3,
};

enum class CApiCoveragePath : std::uint8_t {
    NotApplicable = 0,
    GenericStrategyFacade = 1,
    SolverAndStrategyFacade = 2,
    DedicatedBestiaryFacade = 3,
};

enum class ReleaseWasmCoveragePath : std::uint8_t {
    NotApplicable = 0,
    GenericFacadePendingRebuild = 1,
    AutomaticProofPendingRebuild = 2,
    DedicatedProofPendingRebuild = 3,
    ProductDeferredPendingRebuild = 4,
};

struct ActionSupportPaths {
    CarrierGenerationPath carrier_generation;
    SchedulerAdmissionPath scheduler_admission;
    RowCompletionPath row_completion;
    BellmanQPath bellman_q;
    SelectedPolicyPath selected_policy;
    CompilerCoveragePath compiler;
    ExactEvaluatorCoveragePath exact_evaluator;
    SimulatorCoveragePath simulator;
    CApiCoveragePath c_api;
    ReleaseWasmCoveragePath release_wasm;
};

inline constexpr ActionSupportPaths kCurrencySupportPaths{
    CarrierGenerationPath::RegistryProgramComponent,
    SchedulerAdmissionPath::GlobalCandidate,
    RowCompletionPath::SharedPrimitive,
    BellmanQPath::SharedPrimitive,
    SelectedPolicyPath::IncompleteUnqualified,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::SharedPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::GenericFacadePendingRebuild};

inline constexpr ActionSupportPaths kRegistryGoalFamilySupportPaths{
    CarrierGenerationPath::RegistryProgramComponent,
    SchedulerAdmissionPath::GoalFilteredCandidate,
    RowCompletionPath::SharedPrimitive,
    BellmanQPath::SharedPrimitive,
    SelectedPolicyPath::AvailableUnpinned,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::GenericFacadePendingRebuild};

inline constexpr ActionSupportPaths kProgramGoalFamilySupportPaths{
    CarrierGenerationPath::RegistryProgramComponent,
    SchedulerAdmissionPath::GoalFilteredCandidate,
    RowCompletionPath::SharedPrimitive,
    BellmanQPath::SharedPrimitive,
    SelectedPolicyPath::AvailableUnpinned,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::GenericFacadePendingRebuild};

inline constexpr ActionSupportPaths kBenchSupportPaths{
    CarrierGenerationPath::RegistryAndCarrierLocalAutomatic,
    SchedulerAdmissionPath::CandidateOrAutomaticDependency,
    RowCompletionPath::PrimitiveAndAutomaticOption,
    BellmanQPath::PrimitiveAndAutomaticOption,
    SelectedPolicyPath::PartialPinned,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};

inline constexpr ActionSupportPaths kVeiledAcquisitionSupportPaths{
    CarrierGenerationPath::RegistryProductDeferred,
    SchedulerAdmissionPath::ProductFilteredDeferred,
    RowCompletionPath::AuthoredPrimitiveProductDeferred,
    BellmanQPath::AuthoredPrimitiveProductDeferred,
    SelectedPolicyPath::DeferredMechanic,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::ObservedChoiceMechanicContradiction,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::GenericStrategyFacade,
    ReleaseWasmCoveragePath::ProductDeferredPendingRebuild};

inline constexpr ActionSupportPaths kUnveilSupportPaths{
    CarrierGenerationPath::RegistryProductDeferred,
    SchedulerAdmissionPath::ProductFilteredDeferred,
    RowCompletionPath::AuthoredPrimitiveProductDeferred,
    BellmanQPath::AuthoredPrimitiveProductDeferred,
    SelectedPolicyPath::DeferredMechanic,
    CompilerCoveragePath::ObservedChoicePrimitive,
    ExactEvaluatorCoveragePath::ObservedChoiceMechanicContradiction,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::GenericStrategyFacade,
    ReleaseWasmCoveragePath::ProductDeferredPendingRebuild};

inline constexpr ActionSupportPaths kEldritchSetupSupportPaths{
    CarrierGenerationPath::RegistryAndCarrierLocalAutomatic,
    SchedulerAdmissionPath::AutomaticDependency,
    RowCompletionPath::PrimitiveAndAutomaticOption,
    BellmanQPath::PrimitiveAndAutomaticOption,
    SelectedPolicyPath::PinnedComponent,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};

inline constexpr ActionSupportPaths kEldritchPinnedSupportPaths{
    CarrierGenerationPath::RegistryAndCarrierLocalAutomatic,
    SchedulerAdmissionPath::AutomaticDependency,
    RowCompletionPath::PrimitiveAndAutomaticOption,
    BellmanQPath::PrimitiveAndAutomaticOption,
    SelectedPolicyPath::PinnedVerified,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};

inline constexpr ActionSupportPaths kEldritchUnpinnedSupportPaths{
    CarrierGenerationPath::RegistryAndCarrierLocalAutomatic,
    SchedulerAdmissionPath::AutomaticDependency,
    RowCompletionPath::PrimitiveAndAutomaticOption,
    BellmanQPath::PrimitiveAndAutomaticOption,
    SelectedPolicyPath::AvailableUnpinned,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};

inline constexpr ActionSupportPaths kFractureSupportPaths{
    CarrierGenerationPath::RegistryAndCarrierLocalAutomatic,
    SchedulerAdmissionPath::GoalFilteredCandidate,
    RowCompletionPath::PrimitiveAndAutomaticOption,
    BellmanQPath::PrimitiveAndAutomaticOption,
    SelectedPolicyPath::AvailableUnpinned,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};

inline constexpr ActionSupportPaths kCleanupSupportPaths{
    CarrierGenerationPath::RegistryProgramComponent,
    SchedulerAdmissionPath::AutomaticDependency,
    RowCompletionPath::PrimitiveAndAutomaticOption,
    BellmanQPath::PrimitiveAndAutomaticOption,
    SelectedPolicyPath::AvailableUnpinned,
    CompilerCoveragePath::GenericPrimitive,
    ExactEvaluatorCoveragePath::FamilySpecificPrimitive,
    SimulatorCoveragePath::NativePrimitive,
    CApiCoveragePath::SolverAndStrategyFacade,
    ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};

struct ActionFamilyContract {
    ActionType action_type;
    PrimitiveTelemetryFamily telemetry_family;
    std::string_view operation_id;
    RegistryIdentityShape identity_shape;
    ActionCostKeyShape cost_key_shape;
    ExpectedPriceProvenance price_provenance;
    ProductReasonGroup product_reason_group;
    ActionSupportPaths support_paths;
};

inline constexpr std::size_t kActionTypeCount =
    static_cast<std::size_t>(ActionType::RemoveCraftedModifiers) + 1;

inline constexpr std::array<ActionFamilyContract, kActionTypeCount>
    kActionFamilyContracts{{
        {ActionType::Transmute, PrimitiveTelemetryFamily::Currency,
         "transmute", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Augment, PrimitiveTelemetryFamily::Currency,
         "augment", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Alteration, PrimitiveTelemetryFamily::Currency,
         "alteration", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Regal, PrimitiveTelemetryFamily::Currency,
         "regal", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Alchemy, PrimitiveTelemetryFamily::Currency,
         "alchemy", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Chaos, PrimitiveTelemetryFamily::Currency,
         "chaos", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Exalt, PrimitiveTelemetryFamily::Currency,
         "exalt", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Annul, PrimitiveTelemetryFamily::Currency,
         "annul", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Scour, PrimitiveTelemetryFamily::Currency,
         "scour", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Currency, kCurrencySupportPaths},
        {ActionType::Essence, PrimitiveTelemetryFamily::Essence,
         "essence", RegistryIdentityShape::Parameterized,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Essence, kRegistryGoalFamilySupportPaths},
        {ActionType::Fossil, PrimitiveTelemetryFamily::Fossil,
         "fossil", RegistryIdentityShape::FossilLoadout,
         ActionCostKeyShape::FossilComponents,
         ExpectedPriceProvenance::Quote, ProductReasonGroup::Fossil,
         kRegistryGoalFamilySupportPaths},
        {ActionType::Bench, PrimitiveTelemetryFamily::Bench,
         "bench", RegistryIdentityShape::Parameterized,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Recipe,
         ProductReasonGroup::Bench, kBenchSupportPaths},
        {ActionType::VeiledChaos, PrimitiveTelemetryFamily::VeiledChaos,
         "veiled_chaos", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Veiled, kVeiledAcquisitionSupportPaths},
        {ActionType::VeiledExalt, PrimitiveTelemetryFamily::VeiledExalt,
         "veiled_exalt", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Veiled, kVeiledAcquisitionSupportPaths},
        {ActionType::Unveil, PrimitiveTelemetryFamily::Unveil,
         "unveil", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Zero, ExpectedPriceProvenance::Zero,
         ProductReasonGroup::Veiled, kUnveilSupportPaths},
        {ActionType::HarvestReforge, PrimitiveTelemetryFamily::Harvest,
         "harvest_reforge", RegistryIdentityShape::Parameterized,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Recipe,
         ProductReasonGroup::Harvest, kProgramGoalFamilySupportPaths},
        {ActionType::HarvestAugment, PrimitiveTelemetryFamily::Harvest,
         "harvest_augment", RegistryIdentityShape::Parameterized,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Recipe,
         ProductReasonGroup::Harvest, kProgramGoalFamilySupportPaths},
        {ActionType::HarvestResist, PrimitiveTelemetryFamily::Harvest,
         "harvest_resist", RegistryIdentityShape::HarvestResistPair,
         ActionCostKeyShape::HarvestResistTarget,
         ExpectedPriceProvenance::Recipe,
         ProductReasonGroup::Harvest, kProgramGoalFamilySupportPaths},
        {ActionType::EldritchEmber, PrimitiveTelemetryFamily::EldritchSetup,
         "eldritch_ember", RegistryIdentityShape::Parameterized,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Eldritch, kEldritchSetupSupportPaths},
        {ActionType::EldritchIchor, PrimitiveTelemetryFamily::EldritchSetup,
         "eldritch_ichor", RegistryIdentityShape::Parameterized,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Eldritch, kEldritchSetupSupportPaths},
        {ActionType::EldritchExalt, PrimitiveTelemetryFamily::EldritchExalt,
         "eldritch_exalt", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Eldritch, kEldritchPinnedSupportPaths},
        {ActionType::EldritchChaos, PrimitiveTelemetryFamily::EldritchChaos,
         "eldritch_chaos", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Eldritch, kEldritchUnpinnedSupportPaths},
        {ActionType::EldritchAnnul, PrimitiveTelemetryFamily::EldritchAnnul,
         "eldritch_annul", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Eldritch, kEldritchUnpinnedSupportPaths},
        {ActionType::InfluenceExalt,
         PrimitiveTelemetryFamily::InfluenceExalt, "influence_exalt",
         RegistryIdentityShape::Parameterized, ActionCostKeyShape::Identity,
         ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Influence, kProgramGoalFamilySupportPaths},
        {ActionType::Fracture, PrimitiveTelemetryFamily::Fracture,
         "fracture", RegistryIdentityShape::Exact,
         ActionCostKeyShape::Identity, ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Fracture, kFractureSupportPaths},
        {ActionType::RemoveCraftedModifiers,
         PrimitiveTelemetryFamily::Bench, "remove_crafted_modifiers",
         RegistryIdentityShape::Exact, ActionCostKeyShape::Scour,
         ExpectedPriceProvenance::Quote,
         ProductReasonGroup::Cleanup, kCleanupSupportPaths},
    }};

constexpr bool support_paths_are_complete(
    const ActionSupportPaths& paths) {
    return paths.carrier_generation !=
               CarrierGenerationPath::NotApplicable &&
           paths.scheduler_admission !=
               SchedulerAdmissionPath::NotApplicable &&
           paths.row_completion != RowCompletionPath::NotApplicable &&
           paths.bellman_q != BellmanQPath::NotApplicable &&
           paths.selected_policy != SelectedPolicyPath::NotApplicable &&
           paths.compiler != CompilerCoveragePath::NotApplicable &&
           paths.exact_evaluator !=
               ExactEvaluatorCoveragePath::NotApplicable &&
           paths.simulator != SimulatorCoveragePath::NotApplicable &&
           paths.c_api != CApiCoveragePath::NotApplicable &&
           paths.release_wasm !=
               ReleaseWasmCoveragePath::NotApplicable;
}

constexpr bool veiled_support_paths_are_deferred(
    const ActionSupportPaths& paths) {
    return paths.carrier_generation ==
               CarrierGenerationPath::RegistryProductDeferred &&
           paths.scheduler_admission ==
               SchedulerAdmissionPath::ProductFilteredDeferred &&
           paths.row_completion ==
               RowCompletionPath::AuthoredPrimitiveProductDeferred &&
           paths.bellman_q ==
               BellmanQPath::AuthoredPrimitiveProductDeferred &&
           paths.selected_policy ==
               SelectedPolicyPath::DeferredMechanic &&
           paths.exact_evaluator ==
               ExactEvaluatorCoveragePath::
                   ObservedChoiceMechanicContradiction &&
           paths.release_wasm ==
               ReleaseWasmCoveragePath::ProductDeferredPendingRebuild;
}

constexpr bool action_family_contract_is_exhaustive() {
    for (std::size_t index = 0; index < kActionFamilyContracts.size();
         ++index) {
        if (static_cast<std::size_t>(
                kActionFamilyContracts[index].action_type) != index ||
            kActionFamilyContracts[index].telemetry_family ==
                PrimitiveTelemetryFamily::Other ||
            kActionFamilyContracts[index].telemetry_family ==
                PrimitiveTelemetryFamily::Count ||
            kActionFamilyContracts[index].operation_id.empty() ||
            kActionFamilyContracts[index].price_provenance ==
                ExpectedPriceProvenance::Mixed ||
            !support_paths_are_complete(
                kActionFamilyContracts[index].support_paths)) {
            return false;
        }
        for (std::size_t other = index + 1;
             other < kActionFamilyContracts.size(); ++other) {
            if (kActionFamilyContracts[index].operation_id ==
                kActionFamilyContracts[other].operation_id) {
                return false;
            }
        }
        if (kActionFamilyContracts[index].product_reason_group ==
                ProductReasonGroup::Veiled &&
            !veiled_support_paths_are_deferred(
                kActionFamilyContracts[index].support_paths)) {
            return false;
        }
    }
    return true;
}

static_assert(action_family_contract_is_exhaustive());

inline const ActionFamilyContract& action_family_contract(
    const ActionType type) {
    const int raw = static_cast<int>(type);
    if (raw < 0 || static_cast<std::size_t>(raw) >=
                       kActionFamilyContracts.size()) {
        throw std::logic_error(
            "action type is outside the family contract vocabulary");
    }
    const ActionFamilyContract& contract =
        kActionFamilyContracts[static_cast<std::size_t>(raw)];
    if (contract.action_type != type) {
        throw std::logic_error(
            "action family contract table is not indexed by ActionType");
    }
    return contract;
}

inline PrimitiveTelemetryFamily primitive_family_for_action(
    const ActionType type) {
    return action_family_contract(type).telemetry_family;
}

inline bool parameterized_identity_matches(
    const std::string_view id,
    const std::string_view operation_id) {
    return id.size() > operation_id.size() + 1 &&
           id.starts_with(operation_id) &&
           id[operation_id.size()] == ':';
}

inline bool harvest_resist_identity_matches(const std::string_view id) {
    constexpr std::string_view prefix = "harvest_resist:";
    if (!id.starts_with(prefix)) return false;
    const std::string_view pair = id.substr(prefix.size());
    const std::size_t separator = pair.find(':');
    return separator != std::string_view::npos && separator != 0 &&
           separator + 1 < pair.size() &&
           pair.find(':', separator + 1) == std::string_view::npos;
}

inline bool action_identity_matches_contract(
    const ActionFamilyContract& contract,
    const std::string_view id) {
    switch (contract.identity_shape) {
    case RegistryIdentityShape::Exact:
        return id == contract.operation_id;
    case RegistryIdentityShape::Parameterized:
    case RegistryIdentityShape::FossilLoadout:
        return parameterized_identity_matches(id, contract.operation_id);
    case RegistryIdentityShape::HarvestResistPair:
        return harvest_resist_identity_matches(id);
    }
    return false;
}

inline bool fossil_cost_keys_match(const ActionDescriptor& action) {
    constexpr std::string_view prefix = "fossil:";
    if (!action.id.starts_with(prefix) ||
        action.params.fossil_indices.empty()) {
        return false;
    }
    const std::size_t fossil_count = action.params.fossil_indices.size();
    if (action.cost_keys.size() != fossil_count + 1) return false;
    std::string_view loadout(action.id);
    loadout.remove_prefix(prefix.size());
    std::size_t begin = 0;
    for (std::size_t index = 0; index < fossil_count; ++index) {
        const std::size_t end = loadout.find('+', begin);
        const std::string_view key = loadout.substr(
            begin, end == std::string_view::npos
                       ? std::string_view::npos
                       : end - begin);
        if (key.empty() || action.cost_keys[index] !=
                               std::string(prefix) + std::string(key)) {
            return false;
        }
        if (index + 1 == fossil_count) {
            if (end != std::string_view::npos) return false;
        } else {
            if (end == std::string_view::npos) return false;
            begin = end + 1;
        }
    }
    return action.cost_keys.back() ==
           "resonator:" + std::to_string(fossil_count);
}

inline bool action_cost_keys_match_contract(
    const ActionDescriptor& action,
    const ActionFamilyContract& contract) {
    switch (contract.cost_key_shape) {
    case ActionCostKeyShape::Identity:
        return action.cost_keys.size() == 1 &&
               action.cost_keys.front() == action.id;
    case ActionCostKeyShape::FossilComponents:
        return fossil_cost_keys_match(action);
    case ActionCostKeyShape::HarvestResistTarget: {
        if (!harvest_resist_identity_matches(action.id) ||
            action.cost_keys.size() != 1) {
            return false;
        }
        const std::size_t target_separator = action.id.rfind(':');
        return action.cost_keys.front() ==
               "harvest_resist:" + action.id.substr(target_separator + 1);
    }
    case ActionCostKeyShape::Zero:
        return action.cost_keys.empty();
    case ActionCostKeyShape::Scour:
        return action.cost_keys.size() == 1 &&
               action.cost_keys.front() == "scour";
    }
    return false;
}

struct ProductReasonContract {
    ProductReasonGroup group;
    ProductActionRole role;
    std::string_view reason;
};

inline constexpr std::array<ProductReasonContract, 26>
    kProductReasonContracts{{
        {ProductReasonGroup::Currency, ProductActionRole::Candidate,
         "candidate_general_currency"},
        {ProductReasonGroup::Essence, ProductActionRole::Candidate,
         "candidate_exact_essence_goal"},
        {ProductReasonGroup::Essence, ProductActionRole::Filtered,
         "filtered_corruption_only_essence"},
        {ProductReasonGroup::Essence, ProductActionRole::Filtered,
         "filtered_essence_without_exact_goal_mod"},
        {ProductReasonGroup::Fossil, ProductActionRole::Candidate,
         "candidate_bounded_goal_relevant_fossil"},
        {ProductReasonGroup::Bench, ProductActionRole::Candidate,
         "candidate_direct_goal_bench"},
        {ProductReasonGroup::Bench, ProductActionRole::AutomaticDependency,
         "authored_metamod_dependency"},
        {ProductReasonGroup::Bench, ProductActionRole::AutomaticDependency,
         "automatic_protected_side_dependency"},
        {ProductReasonGroup::Bench, ProductActionRole::AutomaticDependency,
         "automatic_multimod_dependency"},
        {ProductReasonGroup::Bench, ProductActionRole::AutomaticDependency,
         "automatic_cannot_roll_dependency"},
        {ProductReasonGroup::Bench, ProductActionRole::AutomaticDependency,
         "automatic_temporary_bench_dependency"},
        {ProductReasonGroup::Bench, ProductActionRole::Filtered,
         "filtered_bench_without_goal_role"},
        {ProductReasonGroup::Bench, ProductActionRole::Filtered,
         "filtered_bench_without_option_effect"},
        {ProductReasonGroup::Bench, ProductActionRole::Filtered,
         "filtered_invalid_bench_mod"},
        {ProductReasonGroup::Veiled, ProductActionRole::Filtered,
         "filtered_veiled_option_deferred"},
        {ProductReasonGroup::Harvest, ProductActionRole::Candidate,
         "candidate_goal_tag_harvest"},
        {ProductReasonGroup::Harvest, ProductActionRole::Filtered,
         "filtered_harvest_without_goal_tag"},
        {ProductReasonGroup::Eldritch,
         ProductActionRole::AutomaticDependency,
         "automatic_eldritch_side_dependency"},
        {ProductReasonGroup::Eldritch, ProductActionRole::Filtered,
         "filtered_eldritch_without_automatic_options"},
        {ProductReasonGroup::Influence, ProductActionRole::Candidate,
         "candidate_goal_influence"},
        {ProductReasonGroup::Influence, ProductActionRole::Filtered,
         "filtered_influence_without_goal_mod"},
        {ProductReasonGroup::Fracture, ProductActionRole::Candidate,
         "candidate_fracture"},
        {ProductReasonGroup::Fracture, ProductActionRole::Filtered,
         "filtered_fracture_without_option"},
        {ProductReasonGroup::Cleanup,
         ProductActionRole::AutomaticDependency,
         "automatic_crafted_cleanup_dependency"},
        {ProductReasonGroup::Cleanup, ProductActionRole::Filtered,
         "filtered_cleanup_without_automatic_options"},
        {ProductReasonGroup::Cleanup, ProductActionRole::Filtered,
         "filtered_cleanup_without_materializable_family"},
    }};

inline bool product_reason_matches_contract(
    const ProductReasonGroup group,
    const ProductActionRole role,
    const std::string_view reason,
    const bool synthetic) {
    if (reason == "candidate_unfiltered") {
        return role == ProductActionRole::Candidate;
    }
    if (reason == "authored_option_dependency") {
        return role == ProductActionRole::AutomaticDependency;
    }
    if (synthetic && reason == "candidate_structural_restart") {
        return role == ProductActionRole::Candidate;
    }
    for (const ProductReasonContract& contract : kProductReasonContracts) {
        if (contract.group == group && contract.role == role &&
            contract.reason == reason) {
            return true;
        }
    }
    return false;
}

inline void validate_action_descriptor_family_contract(
    const ActionDescriptor& action) {
    if (action.synthetic) {
        if (action.id != "restart" || action.cost_keys.size() != 1 ||
            action.cost_keys.front() != "base" ||
            !product_reason_matches_contract(
                ProductReasonGroup::Currency, action.product_role,
                action.product_admission_reason, true)) {
            throw std::logic_error(
                "synthetic restart violates the action family contract");
        }
        return;
    }
    const ActionFamilyContract& contract =
        action_family_contract(action.params.type);
    if (!action_identity_matches_contract(contract, action.id)) {
        throw std::logic_error(
            "action '" + action.id +
            "' does not match its registry family identity");
    }
    if (!action_cost_keys_match_contract(action, contract)) {
        throw std::logic_error(
            "action '" + action.id +
            "' does not match its registry family cost-key shape");
    }
    if (!product_reason_matches_contract(
            contract.product_reason_group, action.product_role,
            action.product_admission_reason, false)) {
        throw std::logic_error(
            "action '" + action.id +
            "' has an unmapped product role/reason");
    }
}

struct ResolvedRegistryIdentity {
    PrimitiveTelemetryFamily family;
    ProductReasonGroup product_reason_group;
    ExpectedPriceProvenance price_provenance;
    bool synthetic = false;
};

inline ResolvedRegistryIdentity resolve_registry_identity_contract(
    const std::string_view id) {
    if (id == "restart") {
        return {PrimitiveTelemetryFamily::Currency,
                ProductReasonGroup::Currency,
                ExpectedPriceProvenance::Manual, true};
    }
    for (const ActionFamilyContract& contract : kActionFamilyContracts) {
        if (action_identity_matches_contract(contract, id)) {
            return {contract.telemetry_family,
                    contract.product_reason_group,
                    contract.price_provenance, false};
        }
    }
    throw std::logic_error(
        "registry identity '" + std::string(id) +
        "' is outside the action family contract vocabulary");
}

inline ExpectedPriceProvenance expected_price_provenance_for_action(
    const ActionDescriptor& action) {
    return action.synthetic
        ? ExpectedPriceProvenance::Manual
        : action_family_contract(action.params.type).price_provenance;
}

inline void validate_action_registry_family_contract(
    const ActionRegistry& registry) {
    std::array<std::uint32_t, kProductActionRoleCount> role_counts{};
    std::array<
        std::array<std::uint32_t, kPrimitiveTelemetryFamilyCount>,
        kProductActionRoleCount>
        role_family_counts{};
    std::map<std::string, std::uint32_t> reason_counts;

    for (const ActionDescriptor& action : registry.actions) {
        validate_action_descriptor_family_contract(action);
        const PrimitiveTelemetryFamily family = action.synthetic
            ? PrimitiveTelemetryFamily::Currency
            : primitive_family_for_action(action.params.type);
        const std::size_t role =
            static_cast<std::size_t>(action.product_role);
        ++role_counts[role];
        ++role_family_counts[role][static_cast<std::size_t>(family)];
        ++reason_counts[action.product_admission_reason];
    }
    for (const ProductActionAdmissionRecord& record :
         registry.product_filtered_actions) {
        const ResolvedRegistryIdentity resolved =
            resolve_registry_identity_contract(record.id);
        if (record.family != resolved.family ||
            !product_reason_matches_contract(
                resolved.product_reason_group, record.role, record.reason,
                resolved.synthetic)) {
            throw std::logic_error(
                "filtered action '" + record.id +
                "' violates the action family admission contract");
        }
        const std::size_t role = static_cast<std::size_t>(record.role);
        ++role_counts[role];
        ++role_family_counts[role][
            static_cast<std::size_t>(record.family)];
        ++reason_counts[record.reason];
    }
    if (registry.product_role_counts != role_counts ||
        registry.product_role_family_counts != role_family_counts ||
        registry.product_reason_counts != reason_counts) {
        throw std::logic_error(
            "registry product admission counters violate the action family "
            "contract");
    }
}

/* Bestiary Imprint is intentionally not a flat ActionDescriptor. Keep its
 * create/restore operation identities and mixed component provenance in the
 * same engine-private audit vocabulary without pretending they are primitive
 * registry actions. */
struct SeparateOperationFamilyContract {
    PrimitiveTelemetryFamily telemetry_family;
    std::string_view operation_id;
    ExpectedPriceProvenance price_provenance;
    ActionSupportPaths support_paths;
};

inline constexpr ActionSupportPaths kSeparateBestiarySupportPaths{
    CarrierGenerationPath::SeparateOperation,
    SchedulerAdmissionPath::SeparateAutomatic,
    RowCompletionPath::AutomaticOptionComponent,
    BellmanQPath::AutomaticOptionComponent,
    SelectedPolicyPath::PinnedComponent,
    CompilerCoveragePath::DedicatedSeparateOperation,
    ExactEvaluatorCoveragePath::DedicatedSeparateOperation,
    SimulatorCoveragePath::DedicatedSeparateOperation,
    CApiCoveragePath::DedicatedBestiaryFacade,
    ReleaseWasmCoveragePath::DedicatedProofPendingRebuild};

inline constexpr std::array<SeparateOperationFamilyContract, 2>
    kSeparateOperationFamilyContracts{{
        {PrimitiveTelemetryFamily::Bestiary, "bestiary:imprint",
         ExpectedPriceProvenance::Mixed,
         kSeparateBestiarySupportPaths},
        {PrimitiveTelemetryFamily::Bestiary, "bestiary:restore_imprint",
         ExpectedPriceProvenance::Zero,
         kSeparateBestiarySupportPaths},
    }};

struct SeparateOperationPriceComponentContract {
    std::string_view operation_id;
    std::string_view price_key;
    ExpectedPriceProvenance price_provenance;
};

inline constexpr std::array<SeparateOperationPriceComponentContract, 3>
    kSeparateOperationPriceComponentContracts{{
        {"bestiary:imprint", "beast:craicic-croaker",
         ExpectedPriceProvenance::Quote},
        {"bestiary:imprint", "beast:rare",
         ExpectedPriceProvenance::OwnerDefault},
        {"bestiary:restore_imprint", "",
         ExpectedPriceProvenance::Zero},
    }};

constexpr bool separate_operation_contract_is_complete() {
    bool create = false;
    bool restore = false;
    bool create_quote = false;
    bool create_owner_default = false;
    bool restore_zero = false;
    for (const SeparateOperationFamilyContract& contract :
         kSeparateOperationFamilyContracts) {
        if (contract.telemetry_family !=
                PrimitiveTelemetryFamily::Bestiary ||
            !support_paths_are_complete(contract.support_paths)) {
            return false;
        }
        create |= contract.operation_id == "bestiary:imprint" &&
                  contract.price_provenance ==
                      ExpectedPriceProvenance::Mixed;
        restore |= contract.operation_id == "bestiary:restore_imprint" &&
                   contract.price_provenance ==
                       ExpectedPriceProvenance::Zero;
    }
    for (const SeparateOperationPriceComponentContract& component :
         kSeparateOperationPriceComponentContracts) {
        create_quote |=
            component.operation_id == "bestiary:imprint" &&
            component.price_provenance ==
                ExpectedPriceProvenance::Quote;
        create_owner_default |=
            component.operation_id == "bestiary:imprint" &&
            component.price_provenance ==
                ExpectedPriceProvenance::OwnerDefault;
        restore_zero |=
            component.operation_id == "bestiary:restore_imprint" &&
            component.price_key.empty() &&
            component.price_provenance == ExpectedPriceProvenance::Zero;
    }
    return create && restore && create_quote && create_owner_default &&
           restore_zero;
}

static_assert(separate_operation_contract_is_complete());

struct AutomaticFamilyContract {
    AutomaticCandidateKind candidate_kind;
    AutomaticTelemetryKind telemetry_kind;
    std::string_view candidate_identity;
    ActionSupportPaths support_paths;
};

constexpr ActionSupportPaths automatic_support_paths(
    const SelectedPolicyPath selected_policy) {
    return {
        CarrierGenerationPath::CarrierLocalAutomatic,
        SchedulerAdmissionPath::CarrierLocalAutomatic,
        RowCompletionPath::AutomaticOption,
        BellmanQPath::AutomaticOption,
        selected_policy,
        CompilerCoveragePath::CompoundAutomatic,
        ExactEvaluatorCoveragePath::CompoundAutomatic,
        SimulatorCoveragePath::CompiledAutomaticProgram,
        CApiCoveragePath::SolverAndStrategyFacade,
        ReleaseWasmCoveragePath::AutomaticProofPendingRebuild};
}

inline constexpr ActionSupportPaths kNoAutomaticSupportPaths{
    CarrierGenerationPath::NotApplicable,
    SchedulerAdmissionPath::NotApplicable,
    RowCompletionPath::NotApplicable,
    BellmanQPath::NotApplicable,
    SelectedPolicyPath::NotApplicable,
    CompilerCoveragePath::NotApplicable,
    ExactEvaluatorCoveragePath::NotApplicable,
    SimulatorCoveragePath::NotApplicable,
    CApiCoveragePath::NotApplicable,
    ReleaseWasmCoveragePath::NotApplicable};

inline constexpr std::size_t kAutomaticCandidateKindCount =
    static_cast<std::size_t>(AutomaticCandidateKind::CannotRoll) + 1;

inline constexpr std::array<AutomaticFamilyContract,
                            kAutomaticCandidateKindCount>
    kAutomaticFamilyContracts{{
        {AutomaticCandidateKind::None, AutomaticTelemetryKind::None, "none",
         kNoAutomaticSupportPaths},
        {AutomaticCandidateKind::Fracture,
         AutomaticTelemetryKind::PrimitiveFracture, "fracture",
         automatic_support_paths(SelectedPolicyPath::AvailableUnpinned)},
        {AutomaticCandidateKind::PermanentBench,
         AutomaticTelemetryKind::PermanentBench, "permanent_bench",
         automatic_support_paths(SelectedPolicyPath::PinnedVerified)},
        {AutomaticCandidateKind::TemporaryBenchBlocker,
         AutomaticTelemetryKind::TemporaryBench,
         "temporary_bench_blocker",
         automatic_support_paths(SelectedPolicyPath::AvailableUnpinned)},
        {AutomaticCandidateKind::ProtectedMetamod,
         AutomaticTelemetryKind::ProtectedSide, "protected_metamod",
         automatic_support_paths(SelectedPolicyPath::AvailableUnpinned)},
        {AutomaticCandidateKind::MultimodFinish,
         AutomaticTelemetryKind::MultimodFinish, "multimod_finish",
         automatic_support_paths(SelectedPolicyPath::AvailableUnpinned)},
        {AutomaticCandidateKind::Imprint, AutomaticTelemetryKind::Imprint,
         "imprint",
         automatic_support_paths(SelectedPolicyPath::PinnedVerified)},
        {AutomaticCandidateKind::ConstructiveRenewal,
         AutomaticTelemetryKind::Renewal, "constructive_renewal",
         automatic_support_paths(
             SelectedPolicyPath::IncompleteUnqualified)},
        {AutomaticCandidateKind::EldritchSide,
         AutomaticTelemetryKind::EldritchSide, "eldritch_side",
         automatic_support_paths(SelectedPolicyPath::PinnedVerified)},
        {AutomaticCandidateKind::CannotRoll,
         AutomaticTelemetryKind::CannotRoll, "cannot_roll",
         automatic_support_paths(SelectedPolicyPath::AvailableUnpinned)},
    }};

constexpr bool automatic_family_contract_is_exhaustive() {
    for (std::size_t index = 0; index < kAutomaticFamilyContracts.size();
         ++index) {
        const AutomaticFamilyContract& contract =
            kAutomaticFamilyContracts[index];
        if (static_cast<std::size_t>(contract.candidate_kind) != index ||
            contract.candidate_identity.empty() ||
            (contract.candidate_kind != AutomaticCandidateKind::None &&
             (contract.telemetry_kind == AutomaticTelemetryKind::None ||
              !support_paths_are_complete(contract.support_paths)))) {
            return false;
        }
        if (contract.candidate_kind == AutomaticCandidateKind::None &&
            (contract.telemetry_kind != AutomaticTelemetryKind::None ||
             contract.support_paths.carrier_generation !=
                 CarrierGenerationPath::NotApplicable)) {
            return false;
        }
    }
    return true;
}

static_assert(automatic_family_contract_is_exhaustive());

inline AutomaticTelemetryKind automatic_telemetry_kind_for_candidate(
    const AutomaticCandidateKind kind) {
    const std::size_t index = static_cast<std::size_t>(kind);
    if (index >= kAutomaticFamilyContracts.size() ||
        kAutomaticFamilyContracts[index].candidate_kind != kind) {
        throw std::logic_error(
            "automatic candidate kind is outside the family contract "
            "vocabulary");
    }
    return kAutomaticFamilyContracts[index].telemetry_kind;
}

} // namespace solver
} // namespace poecraft
