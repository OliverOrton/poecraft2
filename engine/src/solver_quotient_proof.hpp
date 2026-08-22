#pragma once

#include "solver_refinement.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {
namespace quotient {

using refinement::FeatureSignature;
using refinement::ObservationRequirement;
using refinement::StableKey;

/*
 * Collision-free shared ownership for an already-canonical stable key. The
 * pointee is compared, never its address; sharing changes storage only.
 */
class SharedStableKey {
public:
    SharedStableKey() = default;
    SharedStableKey(StableKey value)
        : value_(std::make_shared<const StableKey>(std::move(value))) {}
    SharedStableKey(std::initializer_list<std::uint64_t> value)
        : SharedStableKey(StableKey{value}) {}
    explicit SharedStableKey(std::shared_ptr<const StableKey> value)
        : value_(std::move(value)) {}

    const StableKey& value() const {
        static const StableKey empty;
        return value_ == nullptr ? empty : *value_;
    }
    operator const StableKey&() const { return value(); }
    bool empty() const { return value().empty(); }
    std::size_t capacity() const { return value().capacity(); }
    const StableKey* storage_identity() const { return value_.get(); }
    void push_back(const std::uint64_t token) {
        StableKey copy = value();
        copy.push_back(token);
        value_ = std::make_shared<const StableKey>(std::move(copy));
    }

    bool operator==(const SharedStableKey& other) const {
        if (storage_identity() == other.storage_identity()) return true;
        return value() == other.value();
    }
    auto operator<=>(const SharedStableKey& other) const {
        if (storage_identity() == other.storage_identity()) {
            return std::strong_ordering::equal;
        }
        return value() <=> other.value();
    }

private:
    std::shared_ptr<const StableKey> value_;
};

/* One quotient cell has one canonical observation contract but commonly has
 * many admitted actions. Share the immutable contract across obligations. */
class SharedObservationRequirement {
public:
    SharedObservationRequirement() = default;
    SharedObservationRequirement(ObservationRequirement value)
        : value_(std::make_shared<const ObservationRequirement>(
              std::move(value))) {}

    const ObservationRequirement& value() const {
        static const ObservationRequirement empty;
        return value_ == nullptr ? empty : *value_;
    }
    operator const ObservationRequirement&() const { return value(); }
    const ObservationRequirement* storage_identity() const {
        return value_.get();
    }

    bool operator==(const SharedObservationRequirement& other) const {
        if (storage_identity() == other.storage_identity()) return true;
        return value() == other.value();
    }

private:
    std::shared_ptr<const ObservationRequirement> value_;
};

enum class ProofMemoryCategory : std::uint8_t {
    ProofPayload = 0,
    Certificate,
    DependencySidecar,
    CoverageDescriptor,
    AlternativeObligation,
    LiveReplaySlice,
    Partition,
    Carrier,
    RowKernel,
    Scratch,
    Count,
};

inline constexpr std::size_t kProofMemoryCategoryCount =
    static_cast<std::size_t>(ProofMemoryCategory::Count);

struct ProofMemorySnapshot {
    std::array<std::uint64_t, kProofMemoryCategoryCount> bytes{};
    std::uint64_t total_bytes = 0;
    std::uint64_t peak_total_bytes = 0;
    std::uint64_t live_slice_count = 0;
    std::uint64_t peak_live_slice_count = 0;
    std::uint64_t live_slice_bytes = 0;
    std::uint64_t peak_live_slice_bytes = 0;

    bool operator==(const ProofMemorySnapshot&) const = default;
};

class ProofMemoryLimit final : public std::runtime_error {
public:
    ProofMemoryLimit(std::uint64_t requested, std::uint64_t cap);

    std::uint64_t requested_bytes() const { return requested_bytes_; }
    std::uint64_t cap_bytes() const { return cap_bytes_; }

private:
    std::uint64_t requested_bytes_ = 0;
    std::uint64_t cap_bytes_ = 0;
};

class ProofMemoryLedger {
public:
    explicit ProofMemoryLedger(
        std::uint64_t cap_bytes =
            std::numeric_limits<std::uint64_t>::max());

    ProofMemorySnapshot snapshot() const;
    std::uint64_t cap_bytes() const { return cap_bytes_; }

    void set_owned_bytes(ProofMemoryCategory category, std::uint64_t bytes);
    void charge(ProofMemoryCategory category, std::uint64_t bytes);
    void release(ProofMemoryCategory category, std::uint64_t bytes);
    void charge_live_slice(std::uint64_t bytes);
    void release_live_slice(std::uint64_t bytes);

private:
    std::uint64_t cap_bytes_ = 0;
    ProofMemorySnapshot value_;

    static std::size_t index(ProofMemoryCategory category);
    void refresh_total_and_peak();
};

class ScopedProofMemoryCharge {
public:
    ScopedProofMemoryCharge() = default;
    ScopedProofMemoryCharge(
        ProofMemoryLedger& ledger,
        ProofMemoryCategory category,
        std::uint64_t bytes);
    ~ScopedProofMemoryCharge();

    ScopedProofMemoryCharge(const ScopedProofMemoryCharge&) = delete;
    ScopedProofMemoryCharge& operator=(const ScopedProofMemoryCharge&) = delete;
    ScopedProofMemoryCharge(ScopedProofMemoryCharge&& other) noexcept;
    ScopedProofMemoryCharge& operator=(ScopedProofMemoryCharge&& other) noexcept;

    void reset();

private:
    ProofMemoryLedger* ledger_ = nullptr;
    ProofMemoryCategory category_ = ProofMemoryCategory::Scratch;
    std::uint64_t bytes_ = 0;
};

/*
 * Coverage is a deterministic recipe, not a retained carrier population.
 * `replay_authority_identity` names the exact enumerator. Each range names a
 * collision-free slice in that enumerator and carries its exact count/mass.
 * A range may therefore be retained only for a split-affected slice without
 * growing one process-wide carrier vector.
 */
struct CoverageRange {
    StableKey range_identity;
    std::uint64_t begin = 0;
    std::uint64_t count = 0;
    double total_probability = 0.0;

    bool operator==(const CoverageRange&) const = default;
};

struct CoverageDescriptor {
    StableKey strict_kernel_identity;
    StableKey replay_authority_identity;
    StableKey normalized_enumeration_identity;
    std::vector<CoverageRange> ranges;
    std::uint64_t exact_source_count = 0;
    double exact_total_probability = 0.0;

    bool operator==(const CoverageDescriptor&) const = default;
};

struct CoverageCarrier {
    StableKey stable_key;
    StableKey range_identity;
    std::uint64_t enumeration_index = 0;
    double probability = 0.0;

    bool operator==(const CoverageCarrier&) const = default;
};

using CoverageReplayFunction =
    std::function<std::vector<CoverageCarrier>(const CoverageRange&)>;

class CoverageReplaySlice {
public:
    CoverageReplaySlice() = default;
    ~CoverageReplaySlice();

    CoverageReplaySlice(const CoverageReplaySlice&) = delete;
    CoverageReplaySlice& operator=(const CoverageReplaySlice&) = delete;
    CoverageReplaySlice(CoverageReplaySlice&& other) noexcept;
    CoverageReplaySlice& operator=(CoverageReplaySlice&& other) noexcept;

    const std::vector<CoverageCarrier>& carriers() const { return carriers_; }
    std::uint64_t charged_bytes() const { return charged_bytes_; }
    void reset();

private:
    friend CoverageReplaySlice replay_coverage(
        const CoverageDescriptor&,
        const StableKey&,
        const CoverageReplayFunction&,
        ProofMemoryLedger&);

    CoverageReplaySlice(
        std::vector<CoverageCarrier> carriers,
        ProofMemoryLedger& ledger,
        std::uint64_t charged_bytes);

    std::vector<CoverageCarrier> carriers_;
    ProofMemoryLedger* ledger_ = nullptr;
    std::uint64_t charged_bytes_ = 0;
};

CoverageDescriptor canonical_coverage_descriptor(CoverageDescriptor value);

CoverageReplaySlice replay_coverage(
    const CoverageDescriptor& descriptor,
    const StableKey& replay_authority_identity,
    const CoverageReplayFunction& replay,
    ProofMemoryLedger& ledger);

enum class OptimisticLowerQProvenanceKind : std::uint8_t {
    TrivialNonnegativeCost = 0,
    CarrierWideWitness,
};

struct CarrierLowerQWitness {
    StableKey range_identity;
    std::uint64_t enumeration_index = 0;
    double lower_q = 0.0;

    bool operator==(const CarrierLowerQWitness&) const = default;
};

/*
 * A retained lower-Q certificate is uniform over a complete source-cell
 * coverage. Nontrivial construction takes the minimum carrier witness; an
 * average is deliberately not representable. The zero certificate is the
 * universally safe fallback under the solver's nonnegative-cost contract.
 */
class CarrierWideOptimisticLowerQ {
public:
    CarrierWideOptimisticLowerQ() = default;

    OptimisticLowerQProvenanceKind kind() const { return kind_; }
    const StableKey& authority_identity() const {
        return authority_identity_;
    }
    const StableKey& source_cell_identity() const {
        return source_cell_identity_.value();
    }
    std::uint64_t exact_carriers_covered() const {
        return exact_carriers_covered_;
    }
    double exact_probability_mass() const {
        return exact_probability_mass_;
    }
    double lower_q() const { return lower_q_; }

    bool operator==(const CarrierWideOptimisticLowerQ&) const = default;

private:
    OptimisticLowerQProvenanceKind kind_ =
        OptimisticLowerQProvenanceKind::TrivialNonnegativeCost;
    StableKey authority_identity_;
    SharedStableKey source_cell_identity_;
    std::uint64_t exact_carriers_covered_ = 0;
    double exact_probability_mass_ = 0.0;
    double lower_q_ = 0.0;

    CarrierWideOptimisticLowerQ(
        OptimisticLowerQProvenanceKind kind,
        StableKey authority_identity,
        SharedStableKey source_cell_identity,
        std::uint64_t exact_carriers_covered,
        double exact_probability_mass,
        double lower_q);

    friend CarrierWideOptimisticLowerQ trivial_carrier_wide_lower_q(
        const StableKey&,
        const CoverageDescriptor&);
    friend CarrierWideOptimisticLowerQ trivial_carrier_wide_lower_q(
        const SharedStableKey&,
        const CoverageDescriptor&);
    friend CarrierWideOptimisticLowerQ certify_carrier_wide_lower_q(
        const StableKey&,
        const CoverageDescriptor&,
        StableKey,
        std::vector<CarrierLowerQWitness>);
    friend CarrierWideOptimisticLowerQ certify_carrier_wide_lower_q(
        const SharedStableKey&,
        const CoverageDescriptor&,
        StableKey,
        std::vector<CarrierLowerQWitness>);
};

CarrierWideOptimisticLowerQ trivial_carrier_wide_lower_q(
    const StableKey& source_cell_identity,
    const CoverageDescriptor& coverage);

CarrierWideOptimisticLowerQ trivial_carrier_wide_lower_q(
    const SharedStableKey& source_cell_identity,
    const CoverageDescriptor& coverage);

CarrierWideOptimisticLowerQ certify_carrier_wide_lower_q(
    const StableKey& source_cell_identity,
    const CoverageDescriptor& coverage,
    StableKey authority_identity,
    std::vector<CarrierLowerQWitness> witnesses);

CarrierWideOptimisticLowerQ certify_carrier_wide_lower_q(
    const SharedStableKey& source_cell_identity,
    const CoverageDescriptor& coverage,
    StableKey authority_identity,
    std::vector<CarrierLowerQWitness> witnesses);

struct AlternativeActionIdentity {
    std::uint32_t action_id = 0;
    SharedStableKey semantic_action_identity;
    SharedStableKey runtime_contract_program_identity;
    SharedStableKey exact_choice_recipe_identity;

    bool operator==(const AlternativeActionIdentity&) const = default;
    auto operator<=>(const AlternativeActionIdentity&) const = default;
};

AlternativeActionIdentity canonical_alternative_action_identity(
    AlternativeActionIdentity value);

struct UnresolvedAlternativeObligationIdentity {
    std::uint32_t source_cell_id = 0;
    SharedStableKey source_cell_identity;
    SharedObservationRequirement observation_requirement;
    AlternativeActionIdentity action;
    SharedStableKey price_identity;
    SharedStableKey vocabulary_identity;
    std::uint64_t requirement_generation = 0;
    std::uint64_t source_generation = 0;
    std::uint64_t target_generation = 0;
    std::uint64_t partition_generation = 0;
    std::uint64_t action_generation = 0;
    std::uint64_t admission_generation = 0;
    std::uint64_t price_generation = 0;
    std::uint64_t vocabulary_generation = 0;
    CarrierWideOptimisticLowerQ optimistic_lower;
    double scheduling_priority = 0.0;
    SharedStableKey resumable_work_identity;

    bool operator==(
        const UnresolvedAlternativeObligationIdentity&) const = default;
};

UnresolvedAlternativeObligationIdentity
canonical_unresolved_alternative_obligation_identity(
    UnresolvedAlternativeObligationIdentity value);

std::uint64_t unresolved_alternative_obligation_identity_hash(
    const UnresolvedAlternativeObligationIdentity& identity);

enum class AlternativeObligationStatus : std::uint8_t {
    Unscheduled = 0,
    LowerOnly,
    Scheduled,
    PartiallyEvaluated,
    Certified,
    ConditionallyNoncompetitive,
    Stale,
    ResourceInterrupted,
};

const char* alternative_obligation_status_name(
    AlternativeObligationStatus status);

struct UnresolvedAlternativeObligation {
    std::uint32_t obligation_id = 0;
    std::uint64_t semantic_hash = 0;
    UnresolvedAlternativeObligationIdentity identity;
    AlternativeObligationStatus status =
        AlternativeObligationStatus::Unscheduled;
    std::uint64_t work_completed = 0;
    std::optional<std::uint64_t> certified_row_id;
    std::optional<double> conditional_upper_q;
    std::uint64_t conditional_q_generation = 0;
};

struct AlternativeObligationValidationContext {
    StableKey source_cell_identity;
    StableKey price_identity;
    StableKey vocabulary_identity;
    std::uint64_t requirement_generation = 0;
    std::uint64_t source_generation = 0;
    std::uint64_t target_generation = 0;
    std::uint64_t partition_generation = 0;
    std::uint64_t action_generation = 0;
    std::uint64_t admission_generation = 0;
    std::uint64_t price_generation = 0;
    std::uint64_t vocabulary_generation = 0;
    std::uint64_t q_generation = 0;
};

enum class AlternativeObligationValidationStatus : std::uint8_t {
    Current = 0,
    MissingObligation,
    FullKeyMismatch,
    StaleLifecycle,
    StaleSourceIdentity,
    StalePriceIdentity,
    StaleVocabularyIdentity,
    StaleRequirementGeneration,
    StaleSourceGeneration,
    StaleTargetGeneration,
    StalePartitionGeneration,
    StaleActionGeneration,
    StaleAdmissionGeneration,
    StalePriceGeneration,
    StaleVocabularyGeneration,
    StaleConditionalQGeneration,
};

enum class AlternativeActionAccountingKind : std::uint8_t {
    CurrentSelectedCertified = 0,
    OtherCertified,
    UnresolvedObligation,
};

struct AccountedAlternativeAction {
    std::uint32_t source_cell_id = 0;
    AlternativeActionIdentity action;
    AlternativeActionAccountingKind kind =
        AlternativeActionAccountingKind::UnresolvedObligation;
    std::optional<std::uint64_t> certified_row_id;
    std::optional<std::uint32_t> obligation_id;
};

struct AlternativeActionAccountingAudit {
    bool complete = false;
    bool exact_alternative_envelope_closed = false;
    std::uint64_t admitted_actions = 0;
    std::uint64_t selected_certified_actions = 0;
    std::uint64_t other_certified_actions = 0;
    std::uint64_t unresolved_actions = 0;
    std::uint64_t conditionally_noncompetitive_actions = 0;
    std::uint64_t unaccounted_actions = 0;
    std::uint64_t duplicate_admissions = 0;
    std::uint64_t duplicate_accounting = 0;
    std::uint64_t invalid_accounting = 0;
};

struct ProofProjectedArc {
    StableKey label;
    SharedStableKey target_cell_identity;
    double probability = 0.0;

    bool operator==(const ProofProjectedArc&) const = default;
};

class SharedProjectedArcs {
public:
    using Container = std::vector<ProofProjectedArc>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    SharedProjectedArcs() = default;
    SharedProjectedArcs(Container value)
        : value_(std::make_shared<Container>(std::move(value))) {}
    SharedProjectedArcs(std::initializer_list<ProofProjectedArc> value)
        : SharedProjectedArcs(Container{value}) {}
    explicit SharedProjectedArcs(std::shared_ptr<Container> value)
        : value_(std::move(value)) {}

    const Container& value() const {
        static const Container empty;
        return value_ == nullptr ? empty : *value_;
    }
    const std::shared_ptr<Container>& shared_value() const { return value_; }
    bool empty() const { return value().empty(); }
    std::size_t size() const { return value().size(); }
    std::size_t capacity() const { return value().capacity(); }
    const_iterator begin() const { return value().begin(); }
    const_iterator end() const { return value().end(); }
    iterator begin() { return mutable_value().begin(); }
    iterator end() { return mutable_value().end(); }
    void push_back(ProofProjectedArc arc) {
        mutable_value().push_back(std::move(arc));
    }
    const ProofProjectedArc& operator[](const std::size_t index) const {
        return value().at(index);
    }
    ProofProjectedArc& operator[](const std::size_t index) {
        return mutable_value().at(index);
    }

    bool operator==(const SharedProjectedArcs& other) const {
        return value() == other.value();
    }

private:
    std::shared_ptr<Container> value_;

    Container& mutable_value() {
        if (value_ == nullptr) {
            value_ = std::make_shared<Container>();
        } else if (value_.use_count() != 1) {
            value_ = std::make_shared<Container>(*value_);
        }
        return *value_;
    }
};

/*
 * Dependency generations are deliberately absent. This is the durable,
 * collision-checked row identity. Callers supply the existing canonical
 * runtime-contract/program and exact choice-recipe identities; the proof
 * store does not define a second action vocabulary.
 */
struct CertifiedRowIdentity {
    StableKey source_coarse_parent;
    ObservationRequirement observation_requirement;
    FeatureSignature observed_features;
    std::uint32_t action_id = 0;
    StableKey semantic_action_identity;
    StableKey runtime_contract_program_identity;
    StableKey exact_choice_recipe_identity;
    StableKey session_identity;
    StableKey layout_identity;
    StableKey goal_identity;
    StableKey artifact_identity;
    StableKey start_identity;
    StableKey solver_options_identity;
    CoverageDescriptor coverage;
    double exact_total_probability = 0.0;
    SharedProjectedArcs projected_arcs;

    bool operator==(const CertifiedRowIdentity&) const = default;
};

CertifiedRowIdentity canonical_certified_row_identity(
    CertifiedRowIdentity value);

std::uint64_t certified_row_identity_hash(
    const CertifiedRowIdentity& identity);

struct CertifiedRowPayload {
    CertifiedRowIdentity identity;
    std::uint64_t semantic_hash = 0;
};

struct TargetGenerationDependency {
    std::uint32_t cell_id = 0;
    std::uint64_t generation = 0;

    bool operator==(const TargetGenerationDependency&) const = default;
};

struct RowProofUseSite {
    bool present = false;
    bool valid = false;
    std::uint64_t row_id = 0;
    std::uint32_t payload_id = 0;
    std::uint32_t source_cell_id = 0;
    std::uint64_t source_generation = 0;
    std::vector<TargetGenerationDependency> target_dependencies;
    std::uint64_t action_generation = 0;
    std::uint64_t admission_generation = 0;
};

struct ProofValidationContext {
    std::uint64_t source_generation = 0;
    std::function<std::optional<std::uint64_t>(std::uint32_t)>
        target_generation;
    std::uint64_t action_generation = 0;
    std::uint64_t admission_generation = 0;
};

enum class ProofValidationStatus : std::uint8_t {
    Current = 0,
    MissingRow,
    Invalidated,
    CorruptPayload,
    FullKeyMismatch,
    StaleSourceGeneration,
    StaleTargetGeneration,
    StaleActionGeneration,
    StaleAdmissionGeneration,
};

struct ProofStoreStorageStats {
    std::uint64_t payload_pointer_capacity = 0;
    std::uint64_t payload_bucket_capacity = 0;
    std::uint64_t payload_bucket_id_capacity = 0;
    std::uint64_t payload_object_count = 0;
    std::uint64_t payload_key_u64_capacity = 0;
    std::uint64_t requirement_tag_capacity = 0;
    std::uint64_t requirement_affix_capacity = 0;
    std::uint64_t requirement_selector_tag_capacity = 0;
    std::uint64_t feature_capacity = 0;
    std::uint64_t feature_value_u64_capacity = 0;
    std::uint64_t feature_tag_capacity = 0;
    std::uint64_t arc_capacity = 0;
    std::uint64_t arc_key_u64_capacity = 0;
    std::uint64_t projected_arc_bucket_count = 0;
    std::uint64_t projected_arc_bucket_pointer_capacity = 0;
    std::uint64_t coverage_range_capacity = 0;
    std::uint64_t coverage_key_u64_capacity = 0;
    std::uint64_t use_site_capacity = 0;
    std::uint64_t use_target_dependency_capacity = 0;
    std::uint64_t source_index_outer_capacity = 0;
    std::uint64_t source_index_row_capacity = 0;
    std::uint64_t target_index_outer_capacity = 0;
    std::uint64_t target_index_row_capacity = 0;
    std::uint64_t obligation_capacity = 0;
    std::uint64_t obligation_bucket_count = 0;
    std::uint64_t obligation_bucket_id_capacity = 0;
    std::uint64_t obligation_key_u64_capacity = 0;
    std::uint64_t obligation_shared_key_allocation_capacity = 0;
    std::uint64_t obligation_shared_key_object_count = 0;
    std::uint64_t obligation_shared_key_u64_capacity = 0;
    std::uint64_t obligation_shared_requirement_allocation_capacity = 0;
    std::uint64_t obligation_shared_requirement_object_count = 0;
    std::uint64_t obligation_requirement_tag_capacity = 0;
    std::uint64_t obligation_requirement_affix_capacity = 0;
    std::uint64_t obligation_requirement_selector_tag_capacity = 0;
};

struct ObligationSharedKeyAllocation {
    const StableKey* identity = nullptr;
    std::uint64_t u64_capacity = 0;
};

struct ProofPayloadHashBucket {
    std::uint64_t hash = 0;
    std::vector<std::uint32_t> payload_ids;
};

class ProofStore {
public:
    explicit ProofStore(
        std::uint64_t max_owned_bytes =
            std::numeric_limits<std::uint64_t>::max());

    std::pair<std::uint32_t, bool> intern_payload(
        CertifiedRowIdentity identity,
        std::optional<std::uint64_t> forced_hash_for_test = std::nullopt);

    void attach_row(
        std::uint64_t row_id,
        std::uint32_t payload_id,
        std::uint32_t source_cell_id,
        std::uint64_t source_generation,
        std::vector<TargetGenerationDependency> target_dependencies,
        std::uint64_t action_generation,
        std::uint64_t admission_generation);

    std::uint64_t invalidate_source(std::uint32_t source_cell_id);
    std::uint64_t invalidate_target(std::uint32_t target_cell_id);
    std::uint64_t invalidate_action_generation(std::uint64_t current);
    std::uint64_t invalidate_admission_generation(std::uint64_t current);

    void note_price_change();

    ProofValidationStatus validate_row(
        std::uint64_t row_id,
        const CertifiedRowIdentity& expected_identity,
        const ProofValidationContext& context) const;

    CoverageReplaySlice replay_row_coverage(
        std::uint64_t row_id,
        const StableKey& replay_authority_identity,
        const CoverageReplayFunction& replay);

    const CertifiedRowPayload& payload(std::uint32_t payload_id) const;
    const RowProofUseSite& use_site(std::uint64_t row_id) const;
    bool has_use_site(std::uint64_t row_id) const;
    std::uint32_t payload_count() const;
    std::uint64_t valid_use_site_count() const;

    std::pair<std::uint32_t, bool> intern_alternative_obligation(
        UnresolvedAlternativeObligationIdentity identity,
        std::optional<std::uint64_t> forced_hash_for_test = std::nullopt);
    void transition_alternative_obligation(
        std::uint32_t obligation_id,
        AlternativeObligationStatus status,
        std::optional<std::uint64_t> work_completed = std::nullopt,
        std::optional<std::uint64_t> certified_row_id = std::nullopt,
        std::optional<double> conditional_upper_q = std::nullopt,
        std::uint64_t conditional_q_generation = 0);
    AlternativeObligationValidationStatus validate_alternative_obligation(
        std::uint32_t obligation_id,
        const UnresolvedAlternativeObligationIdentity& expected_identity,
        const AlternativeObligationValidationContext& context) const;
    const UnresolvedAlternativeObligation& alternative_obligation(
        std::uint32_t obligation_id) const;
    std::uint32_t alternative_obligation_count() const;
    std::vector<std::uint32_t> ordered_pending_alternative_obligations() const;
    std::optional<double> optimistic_alternative_lower_for_source(
        std::uint32_t source_cell_id) const;
    bool alternative_obligation_supports_executable_upper(
        std::uint32_t obligation_id) const;
    bool alternative_obligation_blocks_exactness(
        std::uint32_t obligation_id,
        double current_upper_q,
        std::uint64_t current_q_generation) const;
    AlternativeActionAccountingAudit audit_alternative_actions(
        const std::vector<AccountedAlternativeAction>& admitted,
        const std::vector<AccountedAlternativeAction>& accounted,
        const std::map<std::uint32_t, double>& current_upper_q_by_source,
        std::uint64_t current_q_generation) const;

    std::uint64_t price_generation() const { return price_generation_; }
    std::uint64_t q_generation() const { return q_generation_; }
    std::uint64_t policy_generation() const { return policy_generation_; }

    ProofMemoryLedger& ledger() { return ledger_; }
    const ProofMemoryLedger& ledger() const { return ledger_; }
    ProofStoreStorageStats storage_stats() const;

    void clear_and_release();

private:
    ProofMemoryLedger ledger_;
    ProofStoreStorageStats storage_stats_;
    std::map<
        std::uint64_t,
        std::vector<std::shared_ptr<SharedProjectedArcs::Container>>>
        projected_arc_buckets_;
    std::vector<std::shared_ptr<const CertifiedRowPayload>> payloads_;
    std::vector<ProofPayloadHashBucket> payload_buckets_;
    std::vector<RowProofUseSite> use_sites_;
    std::vector<std::vector<std::uint64_t>> source_rows_;
    std::vector<std::vector<std::uint64_t>> target_rows_;
    std::vector<UnresolvedAlternativeObligation> alternative_obligations_;
    std::map<std::uint64_t, std::vector<std::uint32_t>>
        alternative_buckets_;
    std::vector<ObligationSharedKeyAllocation>
        obligation_shared_key_allocations_;
    std::vector<const ObservationRequirement*>
        obligation_shared_requirement_allocations_;
    std::uint64_t price_generation_ = 0;
    std::uint64_t q_generation_ = 0;
    std::uint64_t policy_generation_ = 0;

    void detach_row_indexes(const RowProofUseSite& use);
    void refresh_owned_bytes();
};

} // namespace quotient
} // namespace solver
} // namespace poecraft
