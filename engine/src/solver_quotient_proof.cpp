#include "solver_quotient_proof.hpp"

#include "solver_solve_types.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <numeric>
#include <string>
#include <tuple>

namespace poecraft {
namespace solver {
namespace quotient {

namespace {

using solve_detail::WideFloat;

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t checked_add(
        const std::uint64_t left,
        const std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        throw std::overflow_error("proof memory/count overflow");
    }
    return left + right;
}

std::uint64_t checked_multiply(
        const std::uint64_t count,
        const std::uint64_t size) {
    if (count != 0 &&
        size > std::numeric_limits<std::uint64_t>::max() / count) {
        throw std::overflow_error("proof memory size overflow");
    }
    return count * size;
}

void hash_token(std::uint64_t& hash, const std::uint64_t token) {
    hash ^= token;
    hash *= kFnvPrime;
}

void hash_key(std::uint64_t& hash, const StableKey& key) {
    hash_token(hash, key.size());
    for (const std::uint64_t token : key) hash_token(hash, token);
}

void hash_selector(
        std::uint64_t& hash,
        const RefinementAffixSelector& selector) {
    hash_token(hash, selector.required_affix_traits);
    hash_token(hash, selector.forbidden_affix_traits);
    hash_token(hash, selector.required_item_traits);
    hash_token(hash, selector.forbidden_item_traits);
    hash_token(hash, selector.required_tag_ids.size());
    for (const std::uint32_t tag : selector.required_tag_ids) {
        hash_token(hash, tag);
    }
}

void hash_requirement(
        std::uint64_t& hash,
        const ObservationRequirement& requirement) {
    hash_token(hash, requirement.item_features);
    hash_token(hash, requirement.modifier_tag_ids.size());
    for (const std::uint32_t tag : requirement.modifier_tag_ids) {
        hash_token(hash, tag);
    }
    hash_token(hash, requirement.affix_observations.size());
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        hash_token(hash, observation.features);
        hash_selector(hash, observation.selector);
    }
}

void hash_features(
        std::uint64_t& hash,
        const FeatureSignature& signature) {
    hash_token(hash, signature.size());
    for (const refinement::FeatureAtom& atom : signature) {
        hash_token(hash, static_cast<std::uint8_t>(atom.feature));
        hash_token(hash, atom.subject);
        hash_key(hash, atom.value);
        hash_token(hash, atom.affix_traits);
        hash_token(hash, atom.item_traits);
        hash_token(hash, atom.modifier_tag_ids.size());
        for (const std::uint32_t tag : atom.modifier_tag_ids) {
            hash_token(hash, tag);
        }
    }
}

double probability_sum(const std::vector<double>& values) {
    WideFloat sum{0.0};
    for (const double value : values) sum += WideFloat{value};
    return sum.value();
}

template <typename Value, typename Probability>
double probability_sum(
        const std::vector<Value>& values,
        Probability probability) {
    WideFloat sum{0.0};
    for (const Value& value : values) {
        sum += WideFloat{probability(value)};
    }
    return sum.value();
}

bool valid_probability(const double value) {
    return std::isfinite(value) && value >= 0.0;
}

void require_identity(const StableKey& value, const char* label) {
    if (value.empty()) {
        throw std::invalid_argument(
            std::string("certified row is missing ") + label);
    }
}

std::uint64_t stable_key_capacity_bytes(const StableKey& key) {
    return checked_multiply(key.capacity(), sizeof(std::uint64_t));
}

std::uint64_t replay_slice_bytes(
        const std::vector<CoverageCarrier>& carriers) {
    std::uint64_t bytes = checked_multiply(
        carriers.capacity(), sizeof(CoverageCarrier));
    for (const CoverageCarrier& carrier : carriers) {
        bytes = checked_add(bytes, stable_key_capacity_bytes(carrier.stable_key));
        bytes = checked_add(
            bytes, stable_key_capacity_bytes(carrier.range_identity));
    }
    return bytes;
}

template <typename T>
void sort_unique_ids(std::vector<T>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void replace_capacity(
        std::uint64_t& total,
        const std::size_t previous,
        const std::size_t current) {
    if (current >= previous) {
        total = checked_add(total, current - previous);
    } else {
        total -= previous - current;
    }
}

void add_payload_identity_storage(
        ProofStoreStorageStats& stats,
        const CertifiedRowIdentity& identity) {
    const auto add_payload_key = [&](const StableKey& key) {
        stats.payload_key_u64_capacity = checked_add(
            stats.payload_key_u64_capacity, key.capacity());
    };
    add_payload_key(identity.source_coarse_parent);
    add_payload_key(identity.semantic_action_identity);
    add_payload_key(identity.runtime_contract_program_identity);
    add_payload_key(identity.exact_choice_recipe_identity);
    add_payload_key(identity.session_identity);
    add_payload_key(identity.layout_identity);
    add_payload_key(identity.goal_identity);
    add_payload_key(identity.artifact_identity);
    add_payload_key(identity.start_identity);
    add_payload_key(identity.solver_options_identity);
    stats.requirement_tag_capacity = checked_add(
        stats.requirement_tag_capacity,
        identity.observation_requirement.modifier_tag_ids.capacity());
    stats.requirement_affix_capacity = checked_add(
        stats.requirement_affix_capacity,
        identity.observation_requirement.affix_observations.capacity());
    for (const RefinementAffixObservation& observation :
         identity.observation_requirement.affix_observations) {
        stats.requirement_selector_tag_capacity = checked_add(
            stats.requirement_selector_tag_capacity,
            observation.selector.required_tag_ids.capacity());
    }
    stats.feature_capacity = checked_add(
        stats.feature_capacity, identity.observed_features.capacity());
    for (const refinement::FeatureAtom& atom : identity.observed_features) {
        stats.feature_value_u64_capacity = checked_add(
            stats.feature_value_u64_capacity, atom.value.capacity());
        stats.feature_tag_capacity = checked_add(
            stats.feature_tag_capacity, atom.modifier_tag_ids.capacity());
    }
    stats.coverage_range_capacity = checked_add(
        stats.coverage_range_capacity, identity.coverage.ranges.capacity());
    stats.coverage_key_u64_capacity = checked_add(
        stats.coverage_key_u64_capacity,
        identity.coverage.strict_kernel_identity.capacity());
    stats.coverage_key_u64_capacity = checked_add(
        stats.coverage_key_u64_capacity,
        identity.coverage.replay_authority_identity.capacity());
    stats.coverage_key_u64_capacity = checked_add(
        stats.coverage_key_u64_capacity,
        identity.coverage.normalized_enumeration_identity.capacity());
    for (const CoverageRange& range : identity.coverage.ranges) {
        stats.coverage_key_u64_capacity = checked_add(
            stats.coverage_key_u64_capacity,
            range.range_identity.capacity());
    }
}

void add_obligation_identity_storage(
        ProofStoreStorageStats& stats,
        const UnresolvedAlternativeObligationIdentity& identity,
        std::vector<ObligationSharedKeyAllocation>&
            shared_key_allocations) {
    const auto add_key = [&](const StableKey& key) {
        stats.obligation_key_u64_capacity = checked_add(
            stats.obligation_key_u64_capacity, key.capacity());
    };
    add_key(identity.action.semantic_action_identity);
    add_key(identity.action.runtime_contract_program_identity);
    add_key(identity.action.exact_choice_recipe_identity);
    const auto add_shared_key = [&](const SharedStableKey& key) {
        const StableKey* storage = key.storage_identity();
        if (storage == nullptr) return;
        const auto found = std::find_if(
            shared_key_allocations.begin(), shared_key_allocations.end(),
            [&](const ObligationSharedKeyAllocation& candidate) {
                return candidate.identity == storage;
            });
        if (found != shared_key_allocations.end()) return;
        shared_key_allocations.push_back({storage, key.capacity()});
        stats.obligation_shared_key_allocation_capacity =
            shared_key_allocations.capacity();
        stats.obligation_shared_key_object_count =
            shared_key_allocations.size();
        stats.obligation_shared_key_u64_capacity = checked_add(
            stats.obligation_shared_key_u64_capacity, key.capacity());
    };
    add_shared_key(identity.price_identity);
    add_shared_key(identity.vocabulary_identity);
    add_key(identity.optimistic_lower.authority_identity());
    add_key(identity.resumable_work_identity);
    stats.obligation_requirement_tag_capacity = checked_add(
        stats.obligation_requirement_tag_capacity,
        identity.observation_requirement.modifier_tag_ids.capacity());
    stats.obligation_requirement_affix_capacity = checked_add(
        stats.obligation_requirement_affix_capacity,
        identity.observation_requirement.affix_observations.capacity());
    for (const RefinementAffixObservation& observation :
         identity.observation_requirement.affix_observations) {
        stats.obligation_requirement_selector_tag_capacity = checked_add(
            stats.obligation_requirement_selector_tag_capacity,
            observation.selector.required_tag_ids.capacity());
    }
}

} // namespace

ProofMemoryLimit::ProofMemoryLimit(
        const std::uint64_t requested,
        const std::uint64_t cap)
    : std::runtime_error(
          "proof memory cap exceeded: requested=" +
          std::to_string(requested) + " cap=" + std::to_string(cap)),
      requested_bytes_(requested),
      cap_bytes_(cap) {}

ProofMemoryLedger::ProofMemoryLedger(const std::uint64_t cap_bytes)
    : cap_bytes_(cap_bytes) {}

std::size_t ProofMemoryLedger::index(const ProofMemoryCategory category) {
    const std::size_t result = static_cast<std::size_t>(category);
    if (result >= kProofMemoryCategoryCount) {
        throw std::invalid_argument("invalid proof memory category");
    }
    return result;
}

void ProofMemoryLedger::refresh_total_and_peak() {
    std::uint64_t total = 0;
    for (const std::uint64_t bytes : value_.bytes) {
        total = checked_add(total, bytes);
    }
    if (total > cap_bytes_) {
        throw ProofMemoryLimit(total, cap_bytes_);
    }
    value_.total_bytes = total;
    value_.peak_total_bytes = std::max(value_.peak_total_bytes, total);
}

ProofMemorySnapshot ProofMemoryLedger::snapshot() const {
    return value_;
}

void ProofMemoryLedger::set_owned_bytes(
        const ProofMemoryCategory category,
        const std::uint64_t bytes) {
    const std::size_t offset = index(category);
    const std::uint64_t previous = value_.bytes[offset];
    value_.bytes[offset] = bytes;
    try {
        refresh_total_and_peak();
    } catch (...) {
        value_.bytes[offset] = previous;
        refresh_total_and_peak();
        throw;
    }
}

void ProofMemoryLedger::charge(
        const ProofMemoryCategory category,
        const std::uint64_t bytes) {
    const std::size_t offset = index(category);
    set_owned_bytes(
        category, checked_add(value_.bytes[offset], bytes));
}

void ProofMemoryLedger::release(
        const ProofMemoryCategory category,
        const std::uint64_t bytes) {
    const std::size_t offset = index(category);
    if (bytes > value_.bytes[offset]) {
        throw std::logic_error("proof memory ledger release underflow");
    }
    set_owned_bytes(category, value_.bytes[offset] - bytes);
}

void ProofMemoryLedger::charge_live_slice(const std::uint64_t bytes) {
    charge(ProofMemoryCategory::LiveReplaySlice, bytes);
    value_.live_slice_count = checked_add(value_.live_slice_count, 1);
    value_.peak_live_slice_count = std::max(
        value_.peak_live_slice_count, value_.live_slice_count);
    value_.live_slice_bytes = value_.bytes[index(
        ProofMemoryCategory::LiveReplaySlice)];
    value_.peak_live_slice_bytes = std::max(
        value_.peak_live_slice_bytes, value_.live_slice_bytes);
}

void ProofMemoryLedger::release_live_slice(const std::uint64_t bytes) {
    if (value_.live_slice_count == 0) {
        throw std::logic_error("proof live-slice count underflow");
    }
    --value_.live_slice_count;
    release(ProofMemoryCategory::LiveReplaySlice, bytes);
    value_.live_slice_bytes = value_.bytes[index(
        ProofMemoryCategory::LiveReplaySlice)];
}

ScopedProofMemoryCharge::ScopedProofMemoryCharge(
        ProofMemoryLedger& ledger,
        const ProofMemoryCategory category,
        const std::uint64_t bytes)
    : ledger_(&ledger), category_(category), bytes_(bytes) {
    ledger_->charge(category_, bytes_);
}

ScopedProofMemoryCharge::~ScopedProofMemoryCharge() {
    reset();
}

ScopedProofMemoryCharge::ScopedProofMemoryCharge(
        ScopedProofMemoryCharge&& other) noexcept
    : ledger_(std::exchange(other.ledger_, nullptr)),
      category_(other.category_),
      bytes_(std::exchange(other.bytes_, 0)) {}

ScopedProofMemoryCharge& ScopedProofMemoryCharge::operator=(
        ScopedProofMemoryCharge&& other) noexcept {
    if (this == &other) return *this;
    reset();
    ledger_ = std::exchange(other.ledger_, nullptr);
    category_ = other.category_;
    bytes_ = std::exchange(other.bytes_, 0);
    return *this;
}

void ScopedProofMemoryCharge::reset() {
    if (ledger_ == nullptr) return;
    ledger_->release(category_, bytes_);
    ledger_ = nullptr;
    bytes_ = 0;
}

CoverageReplaySlice::CoverageReplaySlice(
        std::vector<CoverageCarrier> carriers,
        ProofMemoryLedger& ledger,
        const std::uint64_t charged_bytes)
    : carriers_(std::move(carriers)),
      ledger_(&ledger),
      charged_bytes_(charged_bytes) {
    ledger_->charge_live_slice(charged_bytes_);
}

CoverageReplaySlice::~CoverageReplaySlice() {
    reset();
}

CoverageReplaySlice::CoverageReplaySlice(
        CoverageReplaySlice&& other) noexcept
    : carriers_(std::move(other.carriers_)),
      ledger_(std::exchange(other.ledger_, nullptr)),
      charged_bytes_(std::exchange(other.charged_bytes_, 0)) {}

CoverageReplaySlice& CoverageReplaySlice::operator=(
        CoverageReplaySlice&& other) noexcept {
    if (this == &other) return *this;
    reset();
    carriers_ = std::move(other.carriers_);
    ledger_ = std::exchange(other.ledger_, nullptr);
    charged_bytes_ = std::exchange(other.charged_bytes_, 0);
    return *this;
}

void CoverageReplaySlice::reset() {
    if (ledger_ != nullptr) {
        ledger_->release_live_slice(charged_bytes_);
    }
    ledger_ = nullptr;
    charged_bytes_ = 0;
    std::vector<CoverageCarrier>().swap(carriers_);
}

CoverageDescriptor canonical_coverage_descriptor(CoverageDescriptor value) {
    require_identity(value.strict_kernel_identity, "strict kernel identity");
    require_identity(value.replay_authority_identity, "replay authority identity");
    require_identity(
        value.normalized_enumeration_identity,
        "normalized enumeration identity");
    if (value.ranges.empty()) {
        throw std::invalid_argument("coverage descriptor has no replay ranges");
    }
    for (const CoverageRange& range : value.ranges) {
        require_identity(range.range_identity, "coverage range identity");
        if (range.count == 0 || !valid_probability(range.total_probability)) {
            throw std::invalid_argument("invalid coverage range count or mass");
        }
        (void)checked_add(range.begin, range.count);
    }
    std::sort(
        value.ranges.begin(), value.ranges.end(),
        [](const CoverageRange& left, const CoverageRange& right) {
            return std::tuple{
                       left.range_identity, left.begin, left.count,
                       std::bit_cast<std::uint64_t>(left.total_probability)} <
                   std::tuple{
                       right.range_identity, right.begin, right.count,
                       std::bit_cast<std::uint64_t>(right.total_probability)};
        });
    std::uint64_t count = 0;
    for (std::size_t i = 0; i < value.ranges.size(); ++i) {
        const CoverageRange& range = value.ranges[i];
        count = checked_add(count, range.count);
        if (i == 0) continue;
        const CoverageRange& previous = value.ranges[i - 1];
        if (previous.range_identity == range.range_identity &&
            checked_add(previous.begin, previous.count) > range.begin) {
            throw std::invalid_argument("coverage replay ranges overlap");
        }
    }
    if (count != value.exact_source_count ||
        !valid_probability(value.exact_total_probability) ||
        probability_sum(
            value.ranges,
            [](const CoverageRange& range) {
                return range.total_probability;
            }) != value.exact_total_probability) {
        throw std::invalid_argument("coverage descriptor count/mass mismatch");
    }
    return value;
}

CoverageReplaySlice replay_coverage(
        const CoverageDescriptor& descriptor,
        const StableKey& replay_authority_identity,
        const CoverageReplayFunction& replay,
        ProofMemoryLedger& ledger) {
    const CoverageDescriptor canonical =
        canonical_coverage_descriptor(descriptor);
    if (canonical != descriptor) {
        throw std::invalid_argument("coverage descriptor is not canonical");
    }
    if (canonical.replay_authority_identity != replay_authority_identity) {
        throw std::invalid_argument("coverage replay authority mismatch");
    }
    if (!replay) throw std::invalid_argument("coverage replay callback is empty");

    std::vector<CoverageCarrier> carriers;
    carriers.reserve(canonical.exact_source_count);
    for (const CoverageRange& range : canonical.ranges) {
        std::vector<CoverageCarrier> part = replay(range);
        if (part.size() != range.count ||
            probability_sum(
                part,
                [](const CoverageCarrier& carrier) {
                    return carrier.probability;
                }) != range.total_probability) {
            throw std::invalid_argument("coverage replay range count/mass mismatch");
        }
        for (const CoverageCarrier& carrier : part) {
            require_identity(carrier.stable_key, "replayed carrier identity");
            if (carrier.range_identity != range.range_identity ||
                carrier.enumeration_index < range.begin ||
                carrier.enumeration_index >=
                    checked_add(range.begin, range.count)) {
                throw std::invalid_argument(
                    "replayed carrier is outside its coverage range");
            }
            if (!valid_probability(carrier.probability)) {
                throw std::invalid_argument("invalid replayed carrier mass");
            }
        }
        carriers.insert(
            carriers.end(),
            std::make_move_iterator(part.begin()),
            std::make_move_iterator(part.end()));
    }
    std::sort(
        carriers.begin(), carriers.end(),
        [](const CoverageCarrier& left, const CoverageCarrier& right) {
            return std::tuple{
                       left.stable_key, left.range_identity,
                       left.enumeration_index,
                       std::bit_cast<std::uint64_t>(left.probability)} <
                   std::tuple{
                       right.stable_key, right.range_identity,
                       right.enumeration_index,
                       std::bit_cast<std::uint64_t>(right.probability)};
        });
    for (std::size_t i = 1; i < carriers.size(); ++i) {
        if (carriers[i - 1].stable_key == carriers[i].stable_key) {
            throw std::invalid_argument("coverage replay contains duplicate carrier");
        }
    }
    std::vector<std::pair<StableKey, std::uint64_t>> locators;
    locators.reserve(carriers.size());
    for (const CoverageCarrier& carrier : carriers) {
        locators.emplace_back(
            carrier.range_identity, carrier.enumeration_index);
    }
    std::sort(locators.begin(), locators.end());
    if (std::adjacent_find(locators.begin(), locators.end()) !=
        locators.end()) {
        throw std::invalid_argument(
            "coverage replay contains duplicate enumeration positions");
    }
    if (carriers.size() != canonical.exact_source_count ||
        probability_sum(
            carriers,
            [](const CoverageCarrier& carrier) {
                return carrier.probability;
            }) != canonical.exact_total_probability) {
        throw std::invalid_argument("coverage replay total count/mass mismatch");
    }
    const std::uint64_t bytes = replay_slice_bytes(carriers);
    return CoverageReplaySlice(std::move(carriers), ledger, bytes);
}

CarrierWideOptimisticLowerQ::CarrierWideOptimisticLowerQ(
        const OptimisticLowerQProvenanceKind kind,
        StableKey authority_identity,
        SharedStableKey source_cell_identity,
        const std::uint64_t exact_carriers_covered,
        const double exact_probability_mass,
        const double lower_q)
    : kind_(kind),
      authority_identity_(std::move(authority_identity)),
      source_cell_identity_(std::move(source_cell_identity)),
      exact_carriers_covered_(exact_carriers_covered),
      exact_probability_mass_(exact_probability_mass),
      lower_q_(lower_q) {}

CarrierWideOptimisticLowerQ trivial_carrier_wide_lower_q(
        const StableKey& source_cell_identity,
        const CoverageDescriptor& coverage) {
    return trivial_carrier_wide_lower_q(
        SharedStableKey{source_cell_identity}, coverage);
}

CarrierWideOptimisticLowerQ trivial_carrier_wide_lower_q(
        const SharedStableKey& source_cell_identity,
        const CoverageDescriptor& coverage) {
    require_identity(
        source_cell_identity.value(), "lower-Q source cell identity");
    const CoverageDescriptor canonical =
        canonical_coverage_descriptor(coverage);
    return {
        OptimisticLowerQProvenanceKind::TrivialNonnegativeCost,
        {0x706f656372616674ull, 0x6c6f7765725f7a65ull, 1},
        source_cell_identity,
        canonical.exact_source_count,
        canonical.exact_total_probability,
        0.0};
}

CarrierWideOptimisticLowerQ certify_carrier_wide_lower_q(
        const StableKey& source_cell_identity,
        const CoverageDescriptor& coverage,
        StableKey authority_identity,
        std::vector<CarrierLowerQWitness> witnesses) {
    return certify_carrier_wide_lower_q(
        SharedStableKey{source_cell_identity}, coverage,
        std::move(authority_identity), std::move(witnesses));
}

CarrierWideOptimisticLowerQ certify_carrier_wide_lower_q(
        const SharedStableKey& source_cell_identity,
        const CoverageDescriptor& coverage,
        StableKey authority_identity,
        std::vector<CarrierLowerQWitness> witnesses) {
    require_identity(
        source_cell_identity.value(), "lower-Q source cell identity");
    require_identity(authority_identity, "lower-Q authority identity");
    const CoverageDescriptor canonical =
        canonical_coverage_descriptor(coverage);
    if (witnesses.size() != canonical.exact_source_count) {
        throw std::invalid_argument(
            "carrier-wide lower-Q witness count does not cover source cell");
    }
    for (const CarrierLowerQWitness& witness : witnesses) {
        require_identity(witness.range_identity, "lower-Q range identity");
        if (!std::isfinite(witness.lower_q) || witness.lower_q < 0.0) {
            throw std::invalid_argument("invalid carrier lower-Q witness");
        }
    }
    std::sort(
        witnesses.begin(), witnesses.end(),
        [](const CarrierLowerQWitness& left,
           const CarrierLowerQWitness& right) {
            return std::tie(left.range_identity, left.enumeration_index) <
                   std::tie(right.range_identity, right.enumeration_index);
        });
    std::size_t witness_offset = 0;
    double minimum = std::numeric_limits<double>::infinity();
    for (const CoverageRange& range : canonical.ranges) {
        for (std::uint64_t index = range.begin;
             index < checked_add(range.begin, range.count);
             ++index) {
            if (witness_offset >= witnesses.size() ||
                witnesses[witness_offset].range_identity !=
                    range.range_identity ||
                witnesses[witness_offset].enumeration_index != index) {
                throw std::invalid_argument(
                    "carrier-wide lower-Q witnesses do not exactly cover source cell");
            }
            minimum = std::min(
                minimum, witnesses[witness_offset].lower_q);
            ++witness_offset;
        }
    }
    if (witness_offset != witnesses.size() || !std::isfinite(minimum)) {
        throw std::invalid_argument(
            "carrier-wide lower-Q witnesses contain duplicate or foreign carriers");
    }
    return {
        OptimisticLowerQProvenanceKind::CarrierWideWitness,
        std::move(authority_identity),
        source_cell_identity,
        canonical.exact_source_count,
        canonical.exact_total_probability,
        minimum};
}

AlternativeActionIdentity canonical_alternative_action_identity(
        AlternativeActionIdentity value) {
    require_identity(
        value.semantic_action_identity,
        "alternative semantic action identity");
    require_identity(
        value.runtime_contract_program_identity,
        "alternative runtime contract/program identity");
    require_identity(
        value.exact_choice_recipe_identity,
        "alternative exact choice recipe identity");
    return value;
}

UnresolvedAlternativeObligationIdentity
canonical_unresolved_alternative_obligation_identity(
        UnresolvedAlternativeObligationIdentity value) {
    require_identity(
        value.source_cell_identity.value(),
        "alternative source cell identity");
    require_identity(value.price_identity, "alternative price identity");
    require_identity(
        value.vocabulary_identity,
        "alternative vocabulary identity");
    require_identity(
        value.resumable_work_identity,
        "alternative resumable-work identity");
    value.observation_requirement =
        refinement::canonical_observation_requirement(
            std::move(value.observation_requirement));
    value.action =
        canonical_alternative_action_identity(std::move(value.action));
    const CarrierWideOptimisticLowerQ& lower = value.optimistic_lower;
    require_identity(lower.authority_identity(), "lower-Q authority identity");
    if (lower.source_cell_identity() != value.source_cell_identity.value() ||
        lower.exact_carriers_covered() == 0 ||
        !valid_probability(lower.exact_probability_mass()) ||
        lower.exact_probability_mass() <= 0.0 ||
        !std::isfinite(lower.lower_q()) || lower.lower_q() < 0.0) {
        throw std::invalid_argument(
            "alternative optimistic lower-Q does not cover its source cell");
    }
    if (lower.kind() ==
            OptimisticLowerQProvenanceKind::TrivialNonnegativeCost &&
        lower.lower_q() != 0.0) {
        throw std::invalid_argument(
            "trivial optimistic lower-Q must remain zero");
    }
    if (!std::isfinite(value.scheduling_priority)) {
        throw std::invalid_argument(
            "alternative scheduling priority is not finite");
    }
    return value;
}

std::uint64_t unresolved_alternative_obligation_identity_hash(
        const UnresolvedAlternativeObligationIdentity& identity) {
    std::uint64_t hash = kFnvOffset;
    hash_token(hash, identity.source_cell_id);
    hash_key(hash, identity.source_cell_identity.value());
    hash_requirement(hash, identity.observation_requirement);
    hash_token(hash, identity.action.action_id);
    hash_key(hash, identity.action.semantic_action_identity);
    hash_key(hash, identity.action.runtime_contract_program_identity);
    hash_key(hash, identity.action.exact_choice_recipe_identity);
    hash_key(hash, identity.price_identity);
    hash_key(hash, identity.vocabulary_identity);
    hash_token(hash, identity.requirement_generation);
    hash_token(hash, identity.source_generation);
    hash_token(hash, identity.target_generation);
    hash_token(hash, identity.partition_generation);
    hash_token(hash, identity.action_generation);
    hash_token(hash, identity.admission_generation);
    hash_token(hash, identity.price_generation);
    hash_token(hash, identity.vocabulary_generation);
    hash_token(
        hash,
        static_cast<std::uint8_t>(identity.optimistic_lower.kind()));
    hash_key(hash, identity.optimistic_lower.authority_identity());
    hash_key(hash, identity.optimistic_lower.source_cell_identity());
    hash_token(hash, identity.optimistic_lower.exact_carriers_covered());
    hash_token(
        hash,
        std::bit_cast<std::uint64_t>(
            identity.optimistic_lower.exact_probability_mass()));
    hash_token(
        hash,
        std::bit_cast<std::uint64_t>(identity.optimistic_lower.lower_q()));
    hash_token(
        hash,
        std::bit_cast<std::uint64_t>(identity.scheduling_priority));
    hash_key(hash, identity.resumable_work_identity);
    return hash;
}

const char* alternative_obligation_status_name(
        const AlternativeObligationStatus status) {
    switch (status) {
    case AlternativeObligationStatus::Unscheduled: return "unscheduled";
    case AlternativeObligationStatus::LowerOnly: return "lower_only";
    case AlternativeObligationStatus::Scheduled: return "scheduled";
    case AlternativeObligationStatus::PartiallyEvaluated:
        return "partially_evaluated";
    case AlternativeObligationStatus::Certified: return "certified";
    case AlternativeObligationStatus::ConditionallyNoncompetitive:
        return "conditionally_noncompetitive";
    case AlternativeObligationStatus::Stale: return "stale";
    case AlternativeObligationStatus::ResourceInterrupted:
        return "resource_interrupted";
    }
    return "unknown";
}

CertifiedRowIdentity canonical_certified_row_identity(
        CertifiedRowIdentity value) {
    require_identity(value.source_coarse_parent, "source coarse parent");
    require_identity(value.semantic_action_identity, "semantic action identity");
    require_identity(
        value.runtime_contract_program_identity,
        "runtime contract/program identity");
    require_identity(value.session_identity, "session identity");
    require_identity(value.layout_identity, "layout identity");
    require_identity(value.goal_identity, "goal identity");
    require_identity(value.artifact_identity, "artifact identity");
    require_identity(value.start_identity, "start identity");
    require_identity(value.solver_options_identity, "solver options identity");
    value.observation_requirement =
        refinement::canonical_observation_requirement(
            std::move(value.observation_requirement));
    value.observed_features = refinement::canonical_feature_signature(
        std::move(value.observed_features));
    value.coverage = canonical_coverage_descriptor(std::move(value.coverage));
    if (!valid_probability(value.exact_total_probability) ||
        value.exact_total_probability != value.coverage.exact_total_probability) {
        throw std::invalid_argument("certified row coverage/row mass mismatch");
    }
    for (const ProofProjectedArc& arc : value.projected_arcs) {
        require_identity(
            arc.target_cell_identity.value(), "projected target identity");
        if (!valid_probability(arc.probability)) {
            throw std::invalid_argument("invalid projected arc probability");
        }
    }
    std::sort(
        value.projected_arcs.begin(), value.projected_arcs.end(),
        [](const ProofProjectedArc& left, const ProofProjectedArc& right) {
            return std::tuple{
                       left.label, left.target_cell_identity,
                       std::bit_cast<std::uint64_t>(left.probability)} <
                   std::tuple{
                       right.label, right.target_cell_identity,
                       std::bit_cast<std::uint64_t>(right.probability)};
        });
    for (std::size_t i = 1; i < value.projected_arcs.size(); ++i) {
        const ProofProjectedArc& previous = value.projected_arcs[i - 1];
        const ProofProjectedArc& current = value.projected_arcs[i];
        if (previous.label == current.label &&
            previous.target_cell_identity == current.target_cell_identity) {
            throw std::invalid_argument(
                "projected arcs must be accumulated before certification");
        }
    }
    if (probability_sum(
            value.projected_arcs.value(),
            [](const ProofProjectedArc& arc) { return arc.probability; }) !=
        value.exact_total_probability) {
        throw std::invalid_argument("certified row projected mass mismatch");
    }
    return value;
}

std::uint64_t certified_row_identity_hash(
        const CertifiedRowIdentity& identity) {
    std::uint64_t hash = kFnvOffset;
    hash_key(hash, identity.source_coarse_parent);
    hash_requirement(hash, identity.observation_requirement);
    hash_features(hash, identity.observed_features);
    hash_token(hash, identity.action_id);
    hash_key(hash, identity.semantic_action_identity);
    hash_key(hash, identity.runtime_contract_program_identity);
    hash_key(hash, identity.exact_choice_recipe_identity);
    hash_key(hash, identity.session_identity);
    hash_key(hash, identity.layout_identity);
    hash_key(hash, identity.goal_identity);
    hash_key(hash, identity.artifact_identity);
    hash_key(hash, identity.start_identity);
    hash_key(hash, identity.solver_options_identity);
    hash_key(hash, identity.coverage.strict_kernel_identity);
    hash_key(hash, identity.coverage.replay_authority_identity);
    hash_key(hash, identity.coverage.normalized_enumeration_identity);
    hash_token(hash, identity.coverage.ranges.size());
    for (const CoverageRange& range : identity.coverage.ranges) {
        hash_key(hash, range.range_identity);
        hash_token(hash, range.begin);
        hash_token(hash, range.count);
        hash_token(hash, std::bit_cast<std::uint64_t>(range.total_probability));
    }
    hash_token(hash, identity.coverage.exact_source_count);
    hash_token(
        hash,
        std::bit_cast<std::uint64_t>(
            identity.coverage.exact_total_probability));
    hash_token(hash, std::bit_cast<std::uint64_t>(identity.exact_total_probability));
    hash_token(hash, identity.projected_arcs.size());
    for (const ProofProjectedArc& arc : identity.projected_arcs) {
        hash_key(hash, arc.label);
        hash_key(hash, arc.target_cell_identity.value());
        hash_token(hash, std::bit_cast<std::uint64_t>(arc.probability));
    }
    return hash;
}

ProofStore::ProofStore(const std::uint64_t max_owned_bytes)
    : ledger_(max_owned_bytes) {
    refresh_owned_bytes();
}

std::pair<std::uint32_t, bool> ProofStore::intern_payload(
        CertifiedRowIdentity identity,
        const std::optional<std::uint64_t> forced_hash_for_test) {
    identity = canonical_certified_row_identity(std::move(identity));
    std::uint64_t projected_hash = kFnvOffset;
    hash_token(projected_hash, identity.projected_arcs.size());
    for (const ProofProjectedArc& arc : identity.projected_arcs) {
        hash_key(projected_hash, arc.label);
        hash_key(projected_hash, arc.target_cell_identity.value());
        hash_token(
            projected_hash,
            std::bit_cast<std::uint64_t>(arc.probability));
    }
    auto& projected_bucket = projected_arc_buckets_[projected_hash];
    const auto projected = std::find_if(
        projected_bucket.begin(), projected_bucket.end(),
        [&](const auto& candidate) {
            return candidate != nullptr &&
                   *candidate == identity.projected_arcs.value();
        });
    if (projected != projected_bucket.end()) {
        identity.projected_arcs = SharedProjectedArcs{*projected};
    } else {
        const std::size_t previous_capacity = projected_bucket.capacity();
        projected_bucket.push_back(identity.projected_arcs.shared_value());
        replace_capacity(
            storage_stats_.projected_arc_bucket_pointer_capacity,
            previous_capacity, projected_bucket.capacity());
        storage_stats_.projected_arc_bucket_count =
            projected_arc_buckets_.size();
        storage_stats_.arc_capacity = checked_add(
            storage_stats_.arc_capacity,
            identity.projected_arcs.capacity());
        for (const ProofProjectedArc& arc : identity.projected_arcs) {
            storage_stats_.arc_key_u64_capacity = checked_add(
                storage_stats_.arc_key_u64_capacity,
                arc.label.capacity());
        }
    }
    const std::uint64_t semantic_hash = certified_row_identity_hash(identity);
    const std::uint64_t bucket_hash =
        forced_hash_for_test.value_or(semantic_hash);
    auto bucket = std::lower_bound(
        payload_buckets_.begin(), payload_buckets_.end(), bucket_hash,
        [](const ProofPayloadHashBucket& candidate, const std::uint64_t hash) {
            return candidate.hash < hash;
        });
    if (bucket != payload_buckets_.end() && bucket->hash == bucket_hash) {
        for (const std::uint32_t payload_id : bucket->payload_ids) {
            const auto& candidate = payloads_.at(payload_id);
            if (candidate != nullptr && candidate->identity == identity) {
                return {payload_id, true};
            }
        }
    } else {
        bucket = payload_buckets_.insert(
            bucket, ProofPayloadHashBucket{bucket_hash, {}});
    }
    storage_stats_.payload_bucket_capacity = payload_buckets_.capacity();
    const std::uint32_t payload_id =
        static_cast<std::uint32_t>(payloads_.size());
    payloads_.push_back(std::make_shared<const CertifiedRowPayload>(
        CertifiedRowPayload{std::move(identity), semantic_hash}));
    storage_stats_.payload_pointer_capacity = payloads_.capacity();
    ++storage_stats_.payload_object_count;
    add_payload_identity_storage(
        storage_stats_, payloads_.back()->identity);
    bucket = std::lower_bound(
        payload_buckets_.begin(), payload_buckets_.end(), bucket_hash,
        [](const ProofPayloadHashBucket& candidate, const std::uint64_t hash) {
            return candidate.hash < hash;
        });
    const std::size_t previous_bucket_capacity =
        bucket->payload_ids.capacity();
    bucket->payload_ids.push_back(payload_id);
    replace_capacity(
        storage_stats_.payload_bucket_id_capacity,
        previous_bucket_capacity, bucket->payload_ids.capacity());
    refresh_owned_bytes();
    return {payload_id, false};
}

void ProofStore::detach_row_indexes(const RowProofUseSite& use) {
    const auto erase_row = [&](std::vector<std::uint64_t>& values) {
        values.erase(
            std::remove(values.begin(), values.end(), use.row_id),
            values.end());
    };
    if (use.source_cell_id < source_rows_.size()) {
        erase_row(source_rows_[use.source_cell_id]);
    }
    for (const TargetGenerationDependency& dependency :
         use.target_dependencies) {
        if (dependency.cell_id < target_rows_.size()) {
            erase_row(target_rows_[dependency.cell_id]);
        }
    }
}

void ProofStore::attach_row(
        const std::uint64_t row_id,
        const std::uint32_t payload_id,
        const std::uint32_t source_cell_id,
        const std::uint64_t source_generation,
        std::vector<TargetGenerationDependency> target_dependencies,
        const std::uint64_t action_generation,
        const std::uint64_t admission_generation) {
    (void)payloads_.at(payload_id);
    std::sort(
        target_dependencies.begin(), target_dependencies.end(),
        [](const TargetGenerationDependency& left,
           const TargetGenerationDependency& right) {
            return std::tie(left.cell_id, left.generation) <
                   std::tie(right.cell_id, right.generation);
        });
    for (std::size_t i = 1; i < target_dependencies.size(); ++i) {
        if (target_dependencies[i - 1].cell_id ==
            target_dependencies[i].cell_id) {
            throw std::invalid_argument("duplicate target dependency");
        }
    }
    if (row_id >= use_sites_.size()) use_sites_.resize(row_id + 1);
    storage_stats_.use_site_capacity = use_sites_.capacity();
    const std::size_t previous_dependency_capacity =
        use_sites_[row_id].target_dependencies.capacity();
    if (use_sites_[row_id].present) detach_row_indexes(use_sites_[row_id]);
    if (source_cell_id >= source_rows_.size()) {
        source_rows_.resize(static_cast<std::size_t>(source_cell_id) + 1);
    }
    storage_stats_.source_index_outer_capacity = source_rows_.capacity();
    const std::size_t previous_source_capacity =
        source_rows_[source_cell_id].capacity();
    source_rows_[source_cell_id].push_back(row_id);
    sort_unique_ids(source_rows_[source_cell_id]);
    replace_capacity(
        storage_stats_.source_index_row_capacity,
        previous_source_capacity, source_rows_[source_cell_id].capacity());
    for (const TargetGenerationDependency& dependency : target_dependencies) {
        if (dependency.cell_id >= target_rows_.size()) {
            target_rows_.resize(
                static_cast<std::size_t>(dependency.cell_id) + 1);
        }
        storage_stats_.target_index_outer_capacity = target_rows_.capacity();
        const std::size_t previous_target_capacity =
            target_rows_[dependency.cell_id].capacity();
        target_rows_[dependency.cell_id].push_back(row_id);
        sort_unique_ids(target_rows_[dependency.cell_id]);
        replace_capacity(
            storage_stats_.target_index_row_capacity,
            previous_target_capacity,
            target_rows_[dependency.cell_id].capacity());
    }
    use_sites_[row_id] = RowProofUseSite{
        true,
        true,
        row_id,
        payload_id,
        source_cell_id,
        source_generation,
        std::move(target_dependencies),
        action_generation,
        admission_generation};
    replace_capacity(
        storage_stats_.use_target_dependency_capacity,
        previous_dependency_capacity,
        use_sites_[row_id].target_dependencies.capacity());
    refresh_owned_bytes();
}

std::uint64_t ProofStore::invalidate_source(
        const std::uint32_t source_cell_id) {
    std::uint64_t invalidated = 0;
    if (source_cell_id < source_rows_.size()) {
        for (const std::uint64_t row_id : source_rows_[source_cell_id]) {
            if (row_id < use_sites_.size() && use_sites_[row_id].valid) {
                use_sites_[row_id].valid = false;
                ++invalidated;
            }
        }
    }
    bool obligation_invalidated = false;
    for (UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        if (obligation.identity.source_cell_id == source_cell_id &&
            obligation.status != AlternativeObligationStatus::Stale) {
            obligation.status = AlternativeObligationStatus::Stale;
            obligation.certified_row_id.reset();
            obligation.conditional_upper_q.reset();
            obligation.conditional_q_generation = 0;
            obligation_invalidated = true;
        }
    }
    if (invalidated != 0 || obligation_invalidated) {
        ++q_generation_;
        ++policy_generation_;
    }
    return invalidated;
}

std::uint64_t ProofStore::invalidate_target(
        const std::uint32_t target_cell_id) {
    std::uint64_t invalidated = 0;
    if (target_cell_id < target_rows_.size()) {
        for (const std::uint64_t row_id : target_rows_[target_cell_id]) {
            if (row_id < use_sites_.size() && use_sites_[row_id].valid) {
                use_sites_[row_id].valid = false;
                ++invalidated;
            }
        }
    }
    bool obligation_invalidated = false;
    for (UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        if (obligation.status ==
                AlternativeObligationStatus::ConditionallyNoncompetitive) {
            /* The optimistic carrier lower remains valid, but the Q value it
             * was compared with does not. Resume this same obligation under
             * the new Q generation instead of discarding completed work. */
            obligation.status = AlternativeObligationStatus::Scheduled;
            obligation.certified_row_id.reset();
            obligation.conditional_upper_q.reset();
            obligation.conditional_q_generation = 0;
            obligation_invalidated = true;
        } else if (obligation.status ==
                       AlternativeObligationStatus::Certified &&
                   obligation.certified_row_id.has_value()) {
            const std::uint64_t row_id = *obligation.certified_row_id;
            if (row_id < use_sites_.size() &&
                use_sites_[row_id].present &&
                !use_sites_[row_id].valid) {
                /* Only certified alternatives whose projected row actually
                 * depended on the split target need reproof. */
                obligation.status =
                    AlternativeObligationStatus::Scheduled;
                obligation.certified_row_id.reset();
                obligation.conditional_upper_q.reset();
                obligation.conditional_q_generation = 0;
                obligation_invalidated = true;
            }
        }
    }
    if (invalidated != 0 || obligation_invalidated) {
        ++q_generation_;
        ++policy_generation_;
    }
    return invalidated;
}

std::uint64_t ProofStore::invalidate_action_generation(
        const std::uint64_t current) {
    std::uint64_t invalidated = 0;
    for (RowProofUseSite& use : use_sites_) {
        if (use.present && use.valid && use.action_generation != current) {
            use.valid = false;
            ++invalidated;
        }
    }
    bool obligation_invalidated = false;
    for (UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        if (obligation.status != AlternativeObligationStatus::Stale &&
            obligation.identity.action_generation != current) {
            obligation.status = AlternativeObligationStatus::Stale;
            obligation.certified_row_id.reset();
            obligation.conditional_upper_q.reset();
            obligation.conditional_q_generation = 0;
            obligation_invalidated = true;
        }
    }
    if (invalidated != 0 || obligation_invalidated) {
        ++q_generation_;
        ++policy_generation_;
    }
    return invalidated;
}

std::uint64_t ProofStore::invalidate_admission_generation(
        const std::uint64_t current) {
    std::uint64_t invalidated = 0;
    for (RowProofUseSite& use : use_sites_) {
        if (use.present && use.valid && use.admission_generation != current) {
            use.valid = false;
            ++invalidated;
        }
    }
    bool obligation_invalidated = false;
    for (UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        if (obligation.status != AlternativeObligationStatus::Stale &&
            obligation.identity.admission_generation != current) {
            obligation.status = AlternativeObligationStatus::Stale;
            obligation.certified_row_id.reset();
            obligation.conditional_upper_q.reset();
            obligation.conditional_q_generation = 0;
            obligation_invalidated = true;
        }
    }
    if (invalidated != 0 || obligation_invalidated) {
        ++q_generation_;
        ++policy_generation_;
    }
    return invalidated;
}

void ProofStore::note_price_change() {
    for (UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        if (obligation.status != AlternativeObligationStatus::Stale) {
            obligation.status = AlternativeObligationStatus::Stale;
            obligation.certified_row_id.reset();
            obligation.conditional_upper_q.reset();
            obligation.conditional_q_generation = 0;
        }
    }
    ++price_generation_;
    ++q_generation_;
    ++policy_generation_;
}

ProofValidationStatus ProofStore::validate_row(
        const std::uint64_t row_id,
        const CertifiedRowIdentity& expected_identity,
        const ProofValidationContext& context) const {
    if (row_id >= use_sites_.size() || !use_sites_[row_id].present) {
        return ProofValidationStatus::MissingRow;
    }
    const RowProofUseSite& use = use_sites_[row_id];
    if (!use.valid) return ProofValidationStatus::Invalidated;
    if (use.payload_id >= payloads_.size() || payloads_[use.payload_id] == nullptr) {
        return ProofValidationStatus::CorruptPayload;
    }
    CertifiedRowIdentity canonical;
    try {
        canonical = canonical_certified_row_identity(expected_identity);
    } catch (const std::exception&) {
        return ProofValidationStatus::FullKeyMismatch;
    }
    if (payloads_[use.payload_id]->identity != canonical) {
        return ProofValidationStatus::FullKeyMismatch;
    }
    if (use.source_generation != context.source_generation) {
        return ProofValidationStatus::StaleSourceGeneration;
    }
    for (const TargetGenerationDependency& dependency :
         use.target_dependencies) {
        if (!context.target_generation) {
            return ProofValidationStatus::StaleTargetGeneration;
        }
        const std::optional<std::uint64_t> current =
            context.target_generation(dependency.cell_id);
        if (!current.has_value() || *current != dependency.generation) {
            return ProofValidationStatus::StaleTargetGeneration;
        }
    }
    if (use.action_generation != context.action_generation) {
        return ProofValidationStatus::StaleActionGeneration;
    }
    if (use.admission_generation != context.admission_generation) {
        return ProofValidationStatus::StaleAdmissionGeneration;
    }
    return ProofValidationStatus::Current;
}

CoverageReplaySlice ProofStore::replay_row_coverage(
        const std::uint64_t row_id,
        const StableKey& replay_authority_identity,
        const CoverageReplayFunction& replay) {
    const RowProofUseSite& use = use_site(row_id);
    if (!use.valid) {
        throw std::invalid_argument("cannot replay invalidated row proof");
    }
    return replay_coverage(
        payload(use.payload_id).identity.coverage,
        replay_authority_identity,
        replay,
        ledger_);
}

const CertifiedRowPayload& ProofStore::payload(
        const std::uint32_t payload_id) const {
    const auto& value = payloads_.at(payload_id);
    if (value == nullptr) throw std::logic_error("corrupt proof payload slot");
    return *value;
}

const RowProofUseSite& ProofStore::use_site(
        const std::uint64_t row_id) const {
    const RowProofUseSite& value = use_sites_.at(row_id);
    if (!value.present) throw std::out_of_range("missing proof use site");
    return value;
}

bool ProofStore::has_use_site(const std::uint64_t row_id) const {
    return row_id < use_sites_.size() && use_sites_[row_id].present;
}

std::uint32_t ProofStore::payload_count() const {
    return static_cast<std::uint32_t>(payloads_.size());
}

std::uint64_t ProofStore::valid_use_site_count() const {
    return std::count_if(
        use_sites_.begin(), use_sites_.end(),
        [](const RowProofUseSite& use) {
            return use.present && use.valid;
        });
}

std::pair<std::uint32_t, bool> ProofStore::intern_alternative_obligation(
        UnresolvedAlternativeObligationIdentity identity,
        const std::optional<std::uint64_t> forced_hash_for_test) {
    identity = canonical_unresolved_alternative_obligation_identity(
        std::move(identity));
    const std::uint64_t semantic_hash =
        unresolved_alternative_obligation_identity_hash(identity);
    const std::uint64_t bucket_hash =
        forced_hash_for_test.value_or(semantic_hash);
    auto bucket = std::lower_bound(
        alternative_buckets_.begin(), alternative_buckets_.end(),
        bucket_hash,
        [](const AlternativeObligationHashBucket& candidate,
           const std::uint64_t hash) {
            return candidate.hash < hash;
        });
    if (bucket != alternative_buckets_.end() &&
        bucket->hash == bucket_hash) {
        for (const std::uint32_t obligation_id : bucket->obligation_ids) {
            const UnresolvedAlternativeObligation& candidate =
                alternative_obligations_.at(obligation_id);
            if (candidate.identity == identity) {
                return {obligation_id, true};
            }
        }
    } else {
        bucket = alternative_buckets_.insert(
            bucket, AlternativeObligationHashBucket{bucket_hash, {}});
    }
    storage_stats_.obligation_bucket_capacity =
        alternative_buckets_.capacity();
    const std::uint32_t obligation_id =
        static_cast<std::uint32_t>(alternative_obligations_.size());
    alternative_obligations_.push_back({
        obligation_id,
        semantic_hash,
        std::move(identity),
        AlternativeObligationStatus::Unscheduled,
        0,
        std::nullopt,
        std::nullopt,
        0});
    storage_stats_.obligation_capacity =
        alternative_obligations_.capacity();
    add_obligation_identity_storage(
        storage_stats_, alternative_obligations_.back().identity,
        obligation_shared_key_allocations_);
    bucket = std::lower_bound(
        alternative_buckets_.begin(), alternative_buckets_.end(),
        bucket_hash,
        [](const AlternativeObligationHashBucket& candidate,
           const std::uint64_t hash) {
            return candidate.hash < hash;
        });
    const std::size_t previous_bucket_capacity =
        bucket->obligation_ids.capacity();
    bucket->obligation_ids.push_back(obligation_id);
    replace_capacity(
        storage_stats_.obligation_bucket_id_capacity,
        previous_bucket_capacity, bucket->obligation_ids.capacity());
    refresh_owned_bytes();
    return {obligation_id, false};
}

void ProofStore::transition_alternative_obligation(
        const std::uint32_t obligation_id,
        const AlternativeObligationStatus status,
        const std::optional<std::uint64_t> work_completed,
        const std::optional<std::uint64_t> certified_row_id,
        const std::optional<double> conditional_upper_q,
        const std::uint64_t conditional_q_generation) {
    UnresolvedAlternativeObligation& obligation =
        alternative_obligations_.at(obligation_id);
    const AlternativeObligationStatus previous = obligation.status;
    const auto allowed = [&]() {
        if (status == previous) return true;
        if (status == AlternativeObligationStatus::Stale) return true;
        switch (previous) {
        case AlternativeObligationStatus::Unscheduled:
            return status == AlternativeObligationStatus::LowerOnly ||
                   status == AlternativeObligationStatus::Scheduled;
        case AlternativeObligationStatus::LowerOnly:
            return status == AlternativeObligationStatus::Scheduled ||
                   status == AlternativeObligationStatus::
                       ConditionallyNoncompetitive;
        case AlternativeObligationStatus::Scheduled:
            return status == AlternativeObligationStatus::LowerOnly ||
                   status == AlternativeObligationStatus::
                       PartiallyEvaluated ||
                   status == AlternativeObligationStatus::Certified ||
                   status == AlternativeObligationStatus::
                       ConditionallyNoncompetitive ||
                   status == AlternativeObligationStatus::
                       ResourceInterrupted;
        case AlternativeObligationStatus::PartiallyEvaluated:
            return status == AlternativeObligationStatus::Scheduled ||
                   status == AlternativeObligationStatus::Certified ||
                   status == AlternativeObligationStatus::
                       ResourceInterrupted;
        case AlternativeObligationStatus::ResourceInterrupted:
            return status == AlternativeObligationStatus::LowerOnly ||
                   status == AlternativeObligationStatus::Scheduled;
        case AlternativeObligationStatus::ConditionallyNoncompetitive:
            return status == AlternativeObligationStatus::Scheduled;
        case AlternativeObligationStatus::Certified:
        case AlternativeObligationStatus::Stale:
            return false;
        }
        return false;
    };
    if (!allowed()) {
        throw std::invalid_argument(
            "invalid alternative-obligation lifecycle transition");
    }
    const std::uint64_t retained_work =
        work_completed.value_or(obligation.work_completed);
    if (retained_work < obligation.work_completed) {
        throw std::invalid_argument(
            "alternative-obligation work cannot move backward");
    }
    if (status == AlternativeObligationStatus::Certified) {
        if (!certified_row_id.has_value() ||
            conditional_upper_q.has_value()) {
            throw std::invalid_argument(
                "certified alternative obligation requires only a row id");
        }
    } else if (status ==
                   AlternativeObligationStatus::
                       ConditionallyNoncompetitive) {
        if (!conditional_upper_q.has_value() ||
            !std::isfinite(*conditional_upper_q) ||
            obligation.identity.optimistic_lower.lower_q() <
                *conditional_upper_q ||
            certified_row_id.has_value()) {
            throw std::invalid_argument(
                "noncompetitive obligation lacks a sound current upper comparison");
        }
    } else if (certified_row_id.has_value() ||
               conditional_upper_q.has_value()) {
        throw std::invalid_argument(
            "unresolved alternative cannot retain a certified row or upper proof");
    }
    obligation.status = status;
    obligation.work_completed = retained_work;
    obligation.certified_row_id = certified_row_id;
    obligation.conditional_upper_q = conditional_upper_q;
    obligation.conditional_q_generation =
        status == AlternativeObligationStatus::ConditionallyNoncompetitive
            ? conditional_q_generation
            : 0;
}

AlternativeObligationValidationStatus
ProofStore::validate_alternative_obligation(
        const std::uint32_t obligation_id,
        const UnresolvedAlternativeObligationIdentity& expected_identity,
        const AlternativeObligationValidationContext& context) const {
    if (obligation_id >= alternative_obligations_.size()) {
        return AlternativeObligationValidationStatus::MissingObligation;
    }
    UnresolvedAlternativeObligationIdentity canonical;
    try {
        canonical = canonical_unresolved_alternative_obligation_identity(
            expected_identity);
    } catch (const std::exception&) {
        return AlternativeObligationValidationStatus::FullKeyMismatch;
    }
    const UnresolvedAlternativeObligation& obligation =
        alternative_obligations_[obligation_id];
    if (obligation.identity != canonical) {
        return AlternativeObligationValidationStatus::FullKeyMismatch;
    }
    if (obligation.status == AlternativeObligationStatus::Stale) {
        return AlternativeObligationValidationStatus::StaleLifecycle;
    }
    const UnresolvedAlternativeObligationIdentity& identity =
        obligation.identity;
    if (identity.source_cell_identity.value() !=
        context.source_cell_identity) {
        return AlternativeObligationValidationStatus::StaleSourceIdentity;
    }
    if (identity.price_identity.value() != context.price_identity) {
        return AlternativeObligationValidationStatus::StalePriceIdentity;
    }
    if (identity.vocabulary_identity.value() != context.vocabulary_identity) {
        return AlternativeObligationValidationStatus::StaleVocabularyIdentity;
    }
    if (identity.requirement_generation != context.requirement_generation) {
        return AlternativeObligationValidationStatus::
            StaleRequirementGeneration;
    }
    if (identity.source_generation != context.source_generation) {
        return AlternativeObligationValidationStatus::StaleSourceGeneration;
    }
    if (identity.target_generation != context.target_generation) {
        return AlternativeObligationValidationStatus::StaleTargetGeneration;
    }
    if (identity.partition_generation != context.partition_generation) {
        return AlternativeObligationValidationStatus::
            StalePartitionGeneration;
    }
    if (identity.action_generation != context.action_generation) {
        return AlternativeObligationValidationStatus::StaleActionGeneration;
    }
    if (identity.admission_generation != context.admission_generation) {
        return AlternativeObligationValidationStatus::
            StaleAdmissionGeneration;
    }
    if (identity.price_generation != context.price_generation) {
        return AlternativeObligationValidationStatus::StalePriceGeneration;
    }
    if (identity.vocabulary_generation != context.vocabulary_generation) {
        return AlternativeObligationValidationStatus::
            StaleVocabularyGeneration;
    }
    if (obligation.status ==
            AlternativeObligationStatus::ConditionallyNoncompetitive &&
        obligation.conditional_q_generation != context.q_generation) {
        return AlternativeObligationValidationStatus::
            StaleConditionalQGeneration;
    }
    return AlternativeObligationValidationStatus::Current;
}

const UnresolvedAlternativeObligation& ProofStore::alternative_obligation(
        const std::uint32_t obligation_id) const {
    return alternative_obligations_.at(obligation_id);
}

std::uint32_t ProofStore::alternative_obligation_count() const {
    return static_cast<std::uint32_t>(alternative_obligations_.size());
}

std::vector<std::uint32_t>
ProofStore::ordered_pending_alternative_obligations() const {
    std::vector<std::uint32_t> pending;
    for (const UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        switch (obligation.status) {
        case AlternativeObligationStatus::Unscheduled:
        case AlternativeObligationStatus::LowerOnly:
        case AlternativeObligationStatus::Scheduled:
        case AlternativeObligationStatus::PartiallyEvaluated:
        case AlternativeObligationStatus::ResourceInterrupted:
            pending.push_back(obligation.obligation_id);
            break;
        case AlternativeObligationStatus::Certified:
        case AlternativeObligationStatus::ConditionallyNoncompetitive:
        case AlternativeObligationStatus::Stale:
            break;
        }
    }
    std::sort(
        pending.begin(), pending.end(),
        [&](const std::uint32_t left_id, const std::uint32_t right_id) {
            const UnresolvedAlternativeObligationIdentity& left =
                alternative_obligations_[left_id].identity;
            const UnresolvedAlternativeObligationIdentity& right =
                alternative_obligations_[right_id].identity;
            return std::tuple{
                       -left.scheduling_priority,
                       left.optimistic_lower.lower_q(),
                       left.source_cell_id,
                       left.action.action_id,
                       left.action.semantic_action_identity,
                       left.action.exact_choice_recipe_identity,
                       left_id} <
                   std::tuple{
                       -right.scheduling_priority,
                       right.optimistic_lower.lower_q(),
                       right.source_cell_id,
                       right.action.action_id,
                       right.action.semantic_action_identity,
                       right.action.exact_choice_recipe_identity,
                       right_id};
        });
    return pending;
}

std::optional<double>
ProofStore::optimistic_alternative_lower_for_source(
        const std::uint32_t source_cell_id) const {
    std::optional<double> lower;
    for (const UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        if (obligation.identity.source_cell_id != source_cell_id) {
            continue;
        }
        switch (obligation.status) {
        case AlternativeObligationStatus::Unscheduled:
        case AlternativeObligationStatus::LowerOnly:
        case AlternativeObligationStatus::Scheduled:
        case AlternativeObligationStatus::PartiallyEvaluated:
        case AlternativeObligationStatus::ConditionallyNoncompetitive:
        case AlternativeObligationStatus::ResourceInterrupted:
            if (!lower.has_value() ||
                obligation.identity.optimistic_lower.lower_q() < *lower) {
                lower = obligation.identity.optimistic_lower.lower_q();
            }
            break;
        case AlternativeObligationStatus::Certified:
        case AlternativeObligationStatus::Stale:
            break;
        }
    }
    return lower;
}

bool ProofStore::alternative_obligation_supports_executable_upper(
        const std::uint32_t obligation_id) const {
    const UnresolvedAlternativeObligation& obligation =
        alternative_obligations_.at(obligation_id);
    return obligation.status == AlternativeObligationStatus::Certified &&
           obligation.certified_row_id.has_value();
}

bool ProofStore::alternative_obligation_blocks_exactness(
        const std::uint32_t obligation_id,
        const double current_upper_q,
        const std::uint64_t current_q_generation) const {
    const UnresolvedAlternativeObligation& obligation =
        alternative_obligations_.at(obligation_id);
    if (obligation.status == AlternativeObligationStatus::Certified) {
        return !obligation.certified_row_id.has_value();
    }
    if (obligation.status ==
            AlternativeObligationStatus::ConditionallyNoncompetitive &&
        obligation.conditional_q_generation == current_q_generation &&
        std::isfinite(current_upper_q) &&
        obligation.identity.optimistic_lower.lower_q() >= current_upper_q) {
        return false;
    }
    return true;
}

AlternativeActionAccountingAudit ProofStore::audit_alternative_actions(
        const std::vector<AccountedAlternativeAction>& admitted,
        const std::vector<AccountedAlternativeAction>& accounted,
        const std::map<std::uint32_t, double>& current_upper_q_by_source,
        const std::uint64_t current_q_generation) const {
    AlternativeActionAccountingAudit audit;
    audit.admitted_actions = admitted.size();
    const auto same_action = [](const AccountedAlternativeAction& left,
                                const AccountedAlternativeAction& right) {
        return left.source_cell_id == right.source_cell_id &&
               left.action == right.action;
    };
    for (std::size_t i = 0; i < admitted.size(); ++i) {
        try {
            (void)canonical_alternative_action_identity(admitted[i].action);
        } catch (const std::exception&) {
            ++audit.invalid_accounting;
            continue;
        }
        if (std::any_of(
                admitted.begin(), admitted.begin() + i,
                [&](const AccountedAlternativeAction& prior) {
                    return same_action(prior, admitted[i]);
                })) {
            ++audit.duplicate_admissions;
            continue;
        }
        std::vector<const AccountedAlternativeAction*> matches;
        for (const AccountedAlternativeAction& entry : accounted) {
            if (same_action(admitted[i], entry)) matches.push_back(&entry);
        }
        if (matches.empty()) {
            ++audit.unaccounted_actions;
            continue;
        }
        if (matches.size() != 1) {
            ++audit.duplicate_accounting;
            continue;
        }
        const AccountedAlternativeAction& entry = *matches.front();
        switch (entry.kind) {
        case AlternativeActionAccountingKind::CurrentSelectedCertified:
            if (!entry.certified_row_id.has_value() ||
                entry.obligation_id.has_value()) {
                ++audit.invalid_accounting;
            } else {
                ++audit.selected_certified_actions;
            }
            break;
        case AlternativeActionAccountingKind::OtherCertified:
            if (!entry.certified_row_id.has_value() ||
                entry.obligation_id.has_value()) {
                ++audit.invalid_accounting;
            } else {
                ++audit.other_certified_actions;
            }
            break;
        case AlternativeActionAccountingKind::UnresolvedObligation:
            if (!entry.obligation_id.has_value() ||
                entry.certified_row_id.has_value() ||
                *entry.obligation_id >= alternative_obligations_.size()) {
                ++audit.invalid_accounting;
                break;
            }
            {
                const UnresolvedAlternativeObligation& obligation =
                    alternative_obligations_[*entry.obligation_id];
                if (obligation.identity.source_cell_id !=
                        entry.source_cell_id ||
                    obligation.identity.action != entry.action ||
                    obligation.status ==
                        AlternativeObligationStatus::Certified) {
                    ++audit.invalid_accounting;
                    break;
                }
                if (obligation.status == AlternativeObligationStatus::
                        ConditionallyNoncompetitive &&
                    current_upper_q_by_source.contains(
                        obligation.identity.source_cell_id) &&
                    !alternative_obligation_blocks_exactness(
                        obligation.obligation_id,
                        current_upper_q_by_source.at(
                            obligation.identity.source_cell_id),
                        current_q_generation)) {
                    ++audit.conditionally_noncompetitive_actions;
                } else {
                    ++audit.unresolved_actions;
                }
            }
            break;
        }
    }
    for (const AccountedAlternativeAction& entry : accounted) {
        if (std::none_of(
                admitted.begin(), admitted.end(),
                [&](const AccountedAlternativeAction& candidate) {
                    return same_action(candidate, entry);
                })) {
            ++audit.invalid_accounting;
        }
    }
    std::map<std::uint32_t, std::uint64_t> selected_by_source;
    for (const AccountedAlternativeAction& entry : accounted) {
        if (entry.kind == AlternativeActionAccountingKind::
                CurrentSelectedCertified) {
            ++selected_by_source[entry.source_cell_id];
        }
    }
    for (const auto& [source, count] : selected_by_source) {
        (void)source;
        if (count > 1) audit.invalid_accounting += count - 1;
    }
    audit.complete = audit.unaccounted_actions == 0 &&
                     audit.duplicate_admissions == 0 &&
                     audit.duplicate_accounting == 0 &&
                     audit.invalid_accounting == 0;
    audit.exact_alternative_envelope_closed =
        audit.complete && audit.unresolved_actions == 0;
    return audit;
}

ProofStoreStorageStats ProofStore::storage_stats() const {
    ProofStoreStorageStats stats;
    stats.payload_pointer_capacity = payloads_.capacity();
    stats.payload_bucket_capacity = payload_buckets_.capacity();
    for (const ProofPayloadHashBucket& bucket : payload_buckets_) {
        stats.payload_bucket_id_capacity = checked_add(
            stats.payload_bucket_id_capacity, bucket.payload_ids.capacity());
    }
    stats.projected_arc_bucket_count = projected_arc_buckets_.size();
    for (const auto& [hash, bucket] : projected_arc_buckets_) {
        (void)hash;
        stats.projected_arc_bucket_pointer_capacity = checked_add(
            stats.projected_arc_bucket_pointer_capacity,
            bucket.capacity());
        for (const auto& shared : bucket) {
            if (shared == nullptr) continue;
            stats.arc_capacity = checked_add(
                stats.arc_capacity, shared->capacity());
            for (const ProofProjectedArc& arc : *shared) {
                stats.arc_key_u64_capacity = checked_add(
                    stats.arc_key_u64_capacity, arc.label.capacity());
            }
        }
    }
    stats.payload_object_count = payloads_.size();
    for (const auto& shared : payloads_) {
        if (shared == nullptr) continue;
        const CertifiedRowIdentity& identity = shared->identity;
        const auto add_payload_key = [&](const StableKey& key) {
            stats.payload_key_u64_capacity = checked_add(
                stats.payload_key_u64_capacity, key.capacity());
        };
        add_payload_key(identity.source_coarse_parent);
        add_payload_key(identity.semantic_action_identity);
        add_payload_key(identity.runtime_contract_program_identity);
        add_payload_key(identity.exact_choice_recipe_identity);
        add_payload_key(identity.session_identity);
        add_payload_key(identity.layout_identity);
        add_payload_key(identity.goal_identity);
        add_payload_key(identity.artifact_identity);
        add_payload_key(identity.start_identity);
        add_payload_key(identity.solver_options_identity);
        stats.requirement_tag_capacity = checked_add(
            stats.requirement_tag_capacity,
            identity.observation_requirement.modifier_tag_ids.capacity());
        stats.requirement_affix_capacity = checked_add(
            stats.requirement_affix_capacity,
            identity.observation_requirement.affix_observations.capacity());
        for (const RefinementAffixObservation& observation :
             identity.observation_requirement.affix_observations) {
            stats.requirement_selector_tag_capacity = checked_add(
                stats.requirement_selector_tag_capacity,
                observation.selector.required_tag_ids.capacity());
        }
        stats.feature_capacity = checked_add(
            stats.feature_capacity, identity.observed_features.capacity());
        for (const refinement::FeatureAtom& atom : identity.observed_features) {
            stats.feature_value_u64_capacity = checked_add(
                stats.feature_value_u64_capacity, atom.value.capacity());
            stats.feature_tag_capacity = checked_add(
                stats.feature_tag_capacity, atom.modifier_tag_ids.capacity());
        }
        stats.coverage_range_capacity = checked_add(
            stats.coverage_range_capacity, identity.coverage.ranges.capacity());
        stats.coverage_key_u64_capacity = checked_add(
            stats.coverage_key_u64_capacity,
            identity.coverage.strict_kernel_identity.capacity());
        stats.coverage_key_u64_capacity = checked_add(
            stats.coverage_key_u64_capacity,
            identity.coverage.replay_authority_identity.capacity());
        stats.coverage_key_u64_capacity = checked_add(
            stats.coverage_key_u64_capacity,
            identity.coverage.normalized_enumeration_identity.capacity());
        for (const CoverageRange& range : identity.coverage.ranges) {
            stats.coverage_key_u64_capacity = checked_add(
                stats.coverage_key_u64_capacity,
                range.range_identity.capacity());
        }
    }
    stats.use_site_capacity = use_sites_.capacity();
    for (const RowProofUseSite& use : use_sites_) {
        stats.use_target_dependency_capacity = checked_add(
            stats.use_target_dependency_capacity,
            use.target_dependencies.capacity());
    }
    stats.source_index_outer_capacity = source_rows_.capacity();
    for (const auto& rows : source_rows_) {
        stats.source_index_row_capacity = checked_add(
            stats.source_index_row_capacity, rows.capacity());
    }
    stats.target_index_outer_capacity = target_rows_.capacity();
    for (const auto& rows : target_rows_) {
        stats.target_index_row_capacity = checked_add(
            stats.target_index_row_capacity, rows.capacity());
    }
    stats.obligation_capacity = alternative_obligations_.capacity();
    stats.obligation_bucket_capacity = alternative_buckets_.capacity();
    stats.obligation_shared_key_allocation_capacity =
        obligation_shared_key_allocations_.capacity();
    for (const AlternativeObligationHashBucket& bucket :
         alternative_buckets_) {
        stats.obligation_bucket_id_capacity = checked_add(
            stats.obligation_bucket_id_capacity,
            bucket.obligation_ids.capacity());
    }
    std::vector<const StableKey*> shared_obligation_keys;
    for (const UnresolvedAlternativeObligation& obligation :
         alternative_obligations_) {
        const UnresolvedAlternativeObligationIdentity& identity =
            obligation.identity;
        const auto add_key = [&](const StableKey& key) {
            stats.obligation_key_u64_capacity = checked_add(
                stats.obligation_key_u64_capacity, key.capacity());
        };
        add_key(identity.action.semantic_action_identity);
        add_key(identity.action.runtime_contract_program_identity);
        add_key(identity.action.exact_choice_recipe_identity);
        const auto add_shared_key = [&](const SharedStableKey& key) {
            const StableKey* storage = key.storage_identity();
            if (storage == nullptr ||
                std::find(
                    shared_obligation_keys.begin(),
                    shared_obligation_keys.end(), storage) !=
                    shared_obligation_keys.end()) {
                return;
            }
            shared_obligation_keys.push_back(storage);
            stats.obligation_shared_key_u64_capacity = checked_add(
                stats.obligation_shared_key_u64_capacity, key.capacity());
        };
        add_shared_key(identity.price_identity);
        add_shared_key(identity.vocabulary_identity);
        add_key(identity.optimistic_lower.authority_identity());
        add_key(identity.resumable_work_identity);
        stats.obligation_requirement_tag_capacity = checked_add(
            stats.obligation_requirement_tag_capacity,
            identity.observation_requirement.modifier_tag_ids.capacity());
        stats.obligation_requirement_affix_capacity = checked_add(
            stats.obligation_requirement_affix_capacity,
            identity.observation_requirement.affix_observations.capacity());
        for (const RefinementAffixObservation& observation :
             identity.observation_requirement.affix_observations) {
            stats.obligation_requirement_selector_tag_capacity = checked_add(
                stats.obligation_requirement_selector_tag_capacity,
                observation.selector.required_tag_ids.capacity());
        }
    }
    stats.obligation_shared_key_object_count =
        shared_obligation_keys.size();
    return stats;
}

void ProofStore::refresh_owned_bytes() {
    const ProofStoreStorageStats& stats = storage_stats_;
    std::uint64_t payload_bytes = sizeof(*this);
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.payload_pointer_capacity,
            sizeof(std::shared_ptr<const CertifiedRowPayload>)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.payload_bucket_capacity, sizeof(ProofPayloadHashBucket)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.payload_bucket_id_capacity, sizeof(std::uint32_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.payload_object_count, sizeof(CertifiedRowPayload)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(stats.payload_key_u64_capacity, sizeof(std::uint64_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(stats.requirement_tag_capacity, sizeof(std::uint32_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.requirement_affix_capacity,
            sizeof(RefinementAffixObservation)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.requirement_selector_tag_capacity,
            sizeof(std::uint32_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.feature_capacity, sizeof(refinement::FeatureAtom)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.feature_value_u64_capacity, sizeof(std::uint64_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(stats.feature_tag_capacity, sizeof(std::uint32_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(stats.arc_capacity, sizeof(ProofProjectedArc)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(stats.arc_key_u64_capacity, sizeof(std::uint64_t)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.projected_arc_bucket_count,
            sizeof(std::pair<
                const std::uint64_t,
                std::vector<std::shared_ptr<
                    SharedProjectedArcs::Container>>>) +
                3 * sizeof(void*)));
    payload_bytes = checked_add(
        payload_bytes,
        checked_multiply(
            stats.projected_arc_bucket_pointer_capacity,
            sizeof(std::shared_ptr<SharedProjectedArcs::Container>)));

    const std::uint64_t certificate_bytes = checked_multiply(
        stats.use_site_capacity, sizeof(RowProofUseSite));
    std::uint64_t dependency_bytes = checked_multiply(
        stats.use_target_dependency_capacity,
        sizeof(TargetGenerationDependency));
    dependency_bytes = checked_add(
        dependency_bytes,
        checked_multiply(
            stats.source_index_outer_capacity,
            sizeof(std::vector<std::uint64_t>)));
    dependency_bytes = checked_add(
        dependency_bytes,
        checked_multiply(
            stats.source_index_row_capacity, sizeof(std::uint64_t)));
    dependency_bytes = checked_add(
        dependency_bytes,
        checked_multiply(
            stats.target_index_outer_capacity,
            sizeof(std::vector<std::uint64_t>)));
    dependency_bytes = checked_add(
        dependency_bytes,
        checked_multiply(
            stats.target_index_row_capacity, sizeof(std::uint64_t)));
    std::uint64_t coverage_bytes = checked_multiply(
        stats.coverage_range_capacity, sizeof(CoverageRange));
    coverage_bytes = checked_add(
        coverage_bytes,
        checked_multiply(
            stats.coverage_key_u64_capacity, sizeof(std::uint64_t)));
    std::uint64_t obligation_bytes = checked_multiply(
        stats.obligation_capacity,
        sizeof(UnresolvedAlternativeObligation));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_bucket_capacity,
            sizeof(AlternativeObligationHashBucket)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_bucket_id_capacity,
            sizeof(std::uint32_t)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_key_u64_capacity,
            sizeof(std::uint64_t)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_shared_key_allocation_capacity,
            sizeof(ObligationSharedKeyAllocation)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_shared_key_object_count,
            sizeof(StableKey) + 2 * sizeof(void*)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_shared_key_u64_capacity,
            sizeof(std::uint64_t)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_requirement_tag_capacity,
            sizeof(std::uint32_t)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_requirement_affix_capacity,
            sizeof(RefinementAffixObservation)));
    obligation_bytes = checked_add(
        obligation_bytes,
        checked_multiply(
            stats.obligation_requirement_selector_tag_capacity,
            sizeof(std::uint32_t)));

    ledger_.set_owned_bytes(ProofMemoryCategory::ProofPayload, payload_bytes);
    ledger_.set_owned_bytes(ProofMemoryCategory::Certificate, certificate_bytes);
    ledger_.set_owned_bytes(
        ProofMemoryCategory::DependencySidecar, dependency_bytes);
    ledger_.set_owned_bytes(
        ProofMemoryCategory::CoverageDescriptor, coverage_bytes);
    ledger_.set_owned_bytes(
        ProofMemoryCategory::AlternativeObligation, obligation_bytes);
}

void ProofStore::clear_and_release() {
    if (ledger_.snapshot().live_slice_count != 0) {
        throw std::logic_error("cannot release proof store with live replay slices");
    }
    std::vector<std::shared_ptr<const CertifiedRowPayload>>().swap(payloads_);
    std::vector<ProofPayloadHashBucket>().swap(payload_buckets_);
    std::map<
        std::uint64_t,
        std::vector<std::shared_ptr<SharedProjectedArcs::Container>>>()
        .swap(projected_arc_buckets_);
    std::vector<RowProofUseSite>().swap(use_sites_);
    std::vector<std::vector<std::uint64_t>>().swap(source_rows_);
    std::vector<std::vector<std::uint64_t>>().swap(target_rows_);
    std::vector<UnresolvedAlternativeObligation>().swap(
        alternative_obligations_);
    std::vector<AlternativeObligationHashBucket>().swap(
        alternative_buckets_);
    std::vector<ObligationSharedKeyAllocation>().swap(
        obligation_shared_key_allocations_);
    storage_stats_ = {};
    price_generation_ = 0;
    q_generation_ = 0;
    policy_generation_ = 0;
    refresh_owned_bytes();
}

} // namespace quotient
} // namespace solver
} // namespace poecraft
