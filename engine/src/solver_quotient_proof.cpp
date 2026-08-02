#include "solver_quotient_proof.hpp"

#include "solver_solve_types.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
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
    if (total > cap_bytes_) throw ProofMemoryLimit(total, cap_bytes_);
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
        require_identity(arc.target_cell_identity, "projected target identity");
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
            value.projected_arcs,
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
        hash_key(hash, arc.target_cell_identity);
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
    const std::uint32_t payload_id =
        static_cast<std::uint32_t>(payloads_.size());
    payloads_.push_back(std::make_shared<const CertifiedRowPayload>(
        CertifiedRowPayload{std::move(identity), semantic_hash}));
    bucket = std::lower_bound(
        payload_buckets_.begin(), payload_buckets_.end(), bucket_hash,
        [](const ProofPayloadHashBucket& candidate, const std::uint64_t hash) {
            return candidate.hash < hash;
        });
    bucket->payload_ids.push_back(payload_id);
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
    if (use_sites_[row_id].present) detach_row_indexes(use_sites_[row_id]);
    if (source_cell_id >= source_rows_.size()) {
        source_rows_.resize(static_cast<std::size_t>(source_cell_id) + 1);
    }
    source_rows_[source_cell_id].push_back(row_id);
    sort_unique_ids(source_rows_[source_cell_id]);
    for (const TargetGenerationDependency& dependency : target_dependencies) {
        if (dependency.cell_id >= target_rows_.size()) {
            target_rows_.resize(
                static_cast<std::size_t>(dependency.cell_id) + 1);
        }
        target_rows_[dependency.cell_id].push_back(row_id);
        sort_unique_ids(target_rows_[dependency.cell_id]);
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
    refresh_owned_bytes();
}

std::uint64_t ProofStore::invalidate_source(
        const std::uint32_t source_cell_id) {
    if (source_cell_id >= source_rows_.size()) return 0;
    std::uint64_t invalidated = 0;
    for (const std::uint64_t row_id : source_rows_[source_cell_id]) {
        if (row_id < use_sites_.size() && use_sites_[row_id].valid) {
            use_sites_[row_id].valid = false;
            ++invalidated;
        }
    }
    if (invalidated != 0) {
        ++q_generation_;
        ++policy_generation_;
    }
    return invalidated;
}

std::uint64_t ProofStore::invalidate_target(
        const std::uint32_t target_cell_id) {
    if (target_cell_id >= target_rows_.size()) return 0;
    std::uint64_t invalidated = 0;
    for (const std::uint64_t row_id : target_rows_[target_cell_id]) {
        if (row_id < use_sites_.size() && use_sites_[row_id].valid) {
            use_sites_[row_id].valid = false;
            ++invalidated;
        }
    }
    if (invalidated != 0) {
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
    if (invalidated != 0) {
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
    if (invalidated != 0) {
        ++q_generation_;
        ++policy_generation_;
    }
    return invalidated;
}

void ProofStore::note_price_change() {
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

ProofStoreStorageStats ProofStore::storage_stats() const {
    ProofStoreStorageStats stats;
    stats.payload_pointer_capacity = payloads_.capacity();
    stats.payload_bucket_capacity = payload_buckets_.capacity();
    for (const ProofPayloadHashBucket& bucket : payload_buckets_) {
        stats.payload_bucket_id_capacity = checked_add(
            stats.payload_bucket_id_capacity, bucket.payload_ids.capacity());
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
        stats.arc_capacity = checked_add(
            stats.arc_capacity, identity.projected_arcs.capacity());
        for (const ProofProjectedArc& arc : identity.projected_arcs) {
            stats.arc_key_u64_capacity = checked_add(
                stats.arc_key_u64_capacity,
                checked_add(arc.label.capacity(),
                            arc.target_cell_identity.capacity()));
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
    return stats;
}

void ProofStore::refresh_owned_bytes() {
    const ProofStoreStorageStats stats = storage_stats();
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

    ledger_.set_owned_bytes(ProofMemoryCategory::ProofPayload, payload_bytes);
    ledger_.set_owned_bytes(ProofMemoryCategory::Certificate, certificate_bytes);
    ledger_.set_owned_bytes(
        ProofMemoryCategory::DependencySidecar, dependency_bytes);
    ledger_.set_owned_bytes(
        ProofMemoryCategory::CoverageDescriptor, coverage_bytes);
}

void ProofStore::clear_and_release() {
    if (ledger_.snapshot().live_slice_count != 0) {
        throw std::logic_error("cannot release proof store with live replay slices");
    }
    std::vector<std::shared_ptr<const CertifiedRowPayload>>().swap(payloads_);
    std::vector<ProofPayloadHashBucket>().swap(payload_buckets_);
    std::vector<RowProofUseSite>().swap(use_sites_);
    std::vector<std::vector<std::uint64_t>>().swap(source_rows_);
    std::vector<std::vector<std::uint64_t>>().swap(target_rows_);
    price_generation_ = 0;
    q_generation_ = 0;
    policy_generation_ = 0;
    refresh_owned_bytes();
}

} // namespace quotient
} // namespace solver
} // namespace poecraft
