#include "solver_solve_types.hpp"

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <span>
#include <type_traits>

namespace poecraft {
namespace solver {
namespace {

namespace fs = std::filesystem;

constexpr std::array<char, 16> kMagic{
    'P', 'C', 'S', 'O', 'L', 'V', 'E', 'G',
    'R', 'A', 'P', 'H', 'V', '1', '\r', '\n'};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint32_t kEndianMarker = 0x01020304u;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t hash_bytes(
        std::uint64_t hash, const void* data, const std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

class PayloadWriter {
  public:
    explicit PayloadWriter(std::ostream& stream) : stream_(stream) {}

    void bytes(const void* data, const std::size_t size) {
        if (size == 0) return;
        stream_.write(static_cast<const char*>(data),
                      static_cast<std::streamsize>(size));
        if (!stream_) {
            throw std::runtime_error(
                "failed to write solver development checkpoint");
        }
        size_ += size;
        checksum_ = hash_bytes(checksum_, data, size);
    }

    template <typename T>
    void pod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        bytes(&value, sizeof(value));
    }

    template <typename T>
    void pod_vector(const std::vector<T>& values) {
        static_assert(std::is_trivially_copyable_v<T>);
        pod(static_cast<std::uint64_t>(values.size()));
        bytes(values.data(), values.size() * sizeof(T));
    }

    void string(const std::string_view value) {
        pod(static_cast<std::uint64_t>(value.size()));
        bytes(value.data(), value.size());
    }

    std::uint64_t size() const { return size_; }
    std::uint64_t checksum() const { return checksum_; }

  private:
    std::ostream& stream_;
    std::uint64_t size_ = 0;
    std::uint64_t checksum_ = kFnvOffset;
};

class PayloadReader {
  public:
    PayloadReader(std::istream& stream, const std::uint64_t size)
        : stream_(stream), remaining_(size) {}

    void bytes(void* data, const std::size_t size) {
        if (size > remaining_) {
            throw std::runtime_error(
                "truncated solver development checkpoint payload");
        }
        if (size != 0) {
            stream_.read(static_cast<char*>(data),
                         static_cast<std::streamsize>(size));
            if (!stream_) {
                throw std::runtime_error(
                    "truncated solver development checkpoint payload");
            }
            checksum_ = hash_bytes(checksum_, data, size);
            remaining_ -= size;
        }
    }

    template <typename T>
    T pod() {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        bytes(&value, sizeof(value));
        return value;
    }

    template <typename T>
    std::vector<T> pod_vector() {
        static_assert(std::is_trivially_copyable_v<T>);
        const std::uint64_t count = pod<std::uint64_t>();
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T) ||
            count * sizeof(T) > remaining_) {
            throw std::runtime_error(
                "invalid solver development checkpoint vector length: count=" +
                std::to_string(count) + ", width=" +
                std::to_string(sizeof(T)) + ", remaining=" +
                std::to_string(remaining_));
        }
        std::vector<T> values(static_cast<std::size_t>(count));
        bytes(values.data(), values.size() * sizeof(T));
        return values;
    }

    std::string string() {
        const std::uint64_t size = pod<std::uint64_t>();
        if (size > remaining_ ||
            size > std::numeric_limits<std::size_t>::max()) {
            throw std::runtime_error(
                "invalid solver development checkpoint string length");
        }
        std::string value(static_cast<std::size_t>(size), '\0');
        bytes(value.data(), value.size());
        return value;
    }

    std::uint64_t remaining() const { return remaining_; }
    std::uint64_t checksum() const { return checksum_; }

  private:
    std::istream& stream_;
    std::uint64_t remaining_ = 0;
    std::uint64_t checksum_ = kFnvOffset;
};

template <typename T>
void write_plain_header(std::ostream& stream, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    if (!stream) {
        throw std::runtime_error(
            "failed to write solver development checkpoint header");
    }
}

template <typename T>
T read_plain_header(std::istream& stream) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!stream) {
        throw std::runtime_error(
            "truncated solver development checkpoint header");
    }
    return value;
}

void write_planner(PayloadWriter& out, const PlannerOperator& value) {
    out.string(value.id);
    out.string(value.display_name);
    out.pod(value.kind);
    out.pod(value.option_kind);
    out.pod(value.primitive_action);
    out.string(value.primitive_action_id);
    out.pod_vector(value.primitive_program);
    out.pod(static_cast<std::uint64_t>(
        value.primitive_program_action_ids.size()));
    for (const std::string& id : value.primitive_program_action_ids) {
        out.string(id);
    }
    out.pod(value.intended_side);
    out.pod_vector(value.exit_goal_slots);
    out.pod(value.exit_min_satisfied);
    out.pod(value.carrier_goal_slot);
    out.pod(value.conditional_action);
    out.string(value.conditional_action_id);
    out.pod(value.bestiary_create_action);
    out.string(value.bestiary_create_action_id);
    out.pod(value.bestiary_restore_action);
    out.string(value.bestiary_restore_action_id);
    out.pod(value.automatic_kind);
    out.pod(value.relevant_goal_mask);
    out.pod(value.setup_action);
    out.string(value.setup_action_id);
    out.pod(value.followup_action);
    out.string(value.followup_action_id);
    out.pod(value.cleanup_action);
    out.string(value.cleanup_action_id);
    out.pod(value.constructive_finish_action);
    out.string(value.constructive_finish_action_id);
    out.pod(static_cast<std::uint64_t>(value.resource_quantities.size()));
    for (const auto& [key, quantity] : value.resource_quantities) {
        out.string(key);
        out.pod(quantity);
    }
}

PlannerOperator read_planner(PayloadReader& in) {
    PlannerOperator value;
    value.id = in.string();
    value.display_name = in.string();
    value.kind = in.pod<PlannerOperatorKind>();
    value.option_kind = in.pod<FixedOptionKind>();
    value.primitive_action = in.pod<std::uint32_t>();
    value.primitive_action_id = in.string();
    value.primitive_program = in.pod_vector<std::uint32_t>();
    const std::uint64_t program_ids = in.pod<std::uint64_t>();
    if (program_ids > in.remaining() / sizeof(std::uint64_t)) {
        throw std::runtime_error(
            "invalid planner program count in development checkpoint");
    }
    value.primitive_program_action_ids.reserve(
        static_cast<std::size_t>(program_ids));
    for (std::uint64_t i = 0; i < program_ids; ++i) {
        value.primitive_program_action_ids.push_back(in.string());
    }
    value.intended_side = in.pod<std::int8_t>();
    value.exit_goal_slots = in.pod_vector<std::uint32_t>();
    value.exit_min_satisfied = in.pod<std::uint32_t>();
    value.carrier_goal_slot = in.pod<std::uint32_t>();
    value.conditional_action = in.pod<std::uint32_t>();
    value.conditional_action_id = in.string();
    value.bestiary_create_action = in.pod<std::uint32_t>();
    value.bestiary_create_action_id = in.string();
    value.bestiary_restore_action = in.pod<std::uint32_t>();
    value.bestiary_restore_action_id = in.string();
    value.automatic_kind = in.pod<AutomaticCandidateKind>();
    value.relevant_goal_mask = in.pod<std::uint32_t>();
    value.setup_action = in.pod<std::uint32_t>();
    value.setup_action_id = in.string();
    value.followup_action = in.pod<std::uint32_t>();
    value.followup_action_id = in.string();
    value.cleanup_action = in.pod<std::uint32_t>();
    value.cleanup_action_id = in.string();
    value.constructive_finish_action = in.pod<std::uint32_t>();
    value.constructive_finish_action_id = in.string();
    const std::uint64_t resources = in.pod<std::uint64_t>();
    if (resources > in.remaining() / (sizeof(std::uint64_t) + sizeof(double))) {
        throw std::runtime_error(
            "invalid planner resource count in development checkpoint");
    }
    value.resource_quantities.reserve(static_cast<std::size_t>(resources));
    for (std::uint64_t i = 0; i < resources; ++i) {
        std::string key = in.string();
        const double quantity = in.pod<double>();
        value.resource_quantities.emplace_back(
            std::move(key), quantity);
    }
    return value;
}

void write_evidence(
        PayloadWriter& out, const OptionKernel::AutomaticEvidence& value) {
    out.pod(value.candidate);
    out.pod(value.eligible);
    out.pod(value.kernel_changed);
    out.pod(value.setup_complete);
    out.pod(value.cleanup_complete);
    out.pod(value.recovery_complete);
    out.pod(value.exits_complete);
    out.pod(value.relevant_goal_mask);
    out.pod(value.kernel_change_mechanisms);
    out.pod(value.baseline_kernel_hash);
    out.pod(value.candidate_kernel_hash);
    out.pod(value.fracture_raw_affix_count);
    out.pod(value.fracture_acceptable_affix_count);
    out.pod(value.fracture_restart_state);
    out.pod(value.fracture_hit_probability);
    out.pod(value.fracture_miss_probability);
    out.pod(value.fracture_probability_sum);
    out.string(value.legality_result);
    out.string(value.reason);
}

OptionKernel::AutomaticEvidence read_evidence(PayloadReader& in) {
    OptionKernel::AutomaticEvidence value;
    value.candidate = in.pod<bool>();
    value.eligible = in.pod<bool>();
    value.kernel_changed = in.pod<bool>();
    value.setup_complete = in.pod<bool>();
    value.cleanup_complete = in.pod<bool>();
    value.recovery_complete = in.pod<bool>();
    value.exits_complete = in.pod<bool>();
    value.relevant_goal_mask = in.pod<std::uint32_t>();
    value.kernel_change_mechanisms = in.pod<std::uint32_t>();
    value.baseline_kernel_hash = in.pod<std::uint64_t>();
    value.candidate_kernel_hash = in.pod<std::uint64_t>();
    value.fracture_raw_affix_count = in.pod<std::uint32_t>();
    value.fracture_acceptable_affix_count = in.pod<std::uint32_t>();
    value.fracture_restart_state = in.pod<std::uint32_t>();
    value.fracture_hit_probability = in.pod<double>();
    value.fracture_miss_probability = in.pod<double>();
    value.fracture_probability_sum = in.pod<double>();
    value.legality_result = in.string();
    value.reason = in.string();
    return value;
}

void write_automatic_record(
        PayloadWriter& out,
        const SolveTransitionCache::AutomaticCandidateRecord& value) {
    out.pod(value.state_id);
    out.pod(value.operator_index);
    out.string(value.candidate_id);
    out.pod(value.candidate_kind);
    out.string(value.setup_action_id);
    out.string(value.followup_action_id);
    out.string(value.cleanup_action_id);
#define PC_WRITE_AUTO_FIELD(field) out.pod(value.field)
    PC_WRITE_AUTO_FIELD(eligible);
    PC_WRITE_AUTO_FIELD(collapsed);
    PC_WRITE_AUTO_FIELD(deferred);
    PC_WRITE_AUTO_FIELD(missing_price);
    PC_WRITE_AUTO_FIELD(telemetry_kind);
    PC_WRITE_AUTO_FIELD(template_hit);
    PC_WRITE_AUTO_FIELD(template_id);
    PC_WRITE_AUTO_FIELD(raw_outcomes);
    PC_WRITE_AUTO_FIELD(admission_ns);
    PC_WRITE_AUTO_FIELD(kernel_evaluation_ns);
    PC_WRITE_AUTO_FIELD(outcome_mapping_ns);
    PC_WRITE_AUTO_FIELD(template_matching_ns);
    PC_WRITE_AUTO_FIELD(protected_side_evaluations);
    PC_WRITE_AUTO_FIELD(protected_repeat_evaluations);
    PC_WRITE_AUTO_FIELD(protected_retry_checks);
    PC_WRITE_AUTO_FIELD(protected_retry_certificates);
    PC_WRITE_AUTO_FIELD(protected_retry_fallbacks);
    PC_WRITE_AUTO_FIELD(protected_attempt_ns);
    PC_WRITE_AUTO_FIELD(protected_baseline_ns);
    PC_WRITE_AUTO_FIELD(protected_normalization_ns);
    PC_WRITE_AUTO_FIELD(protected_finish_ns);
    PC_WRITE_AUTO_FIELD(precompiled_classes);
    PC_WRITE_AUTO_FIELD(precompile_ns);
    PC_WRITE_AUTO_FIELD(precompiled_bytes);
    PC_WRITE_AUTO_FIELD(candidate_variants);
    PC_WRITE_AUTO_FIELD(effect_classes);
    PC_WRITE_AUTO_FIELD(collapsed_variants);
    PC_WRITE_AUTO_FIELD(enumeration_ns);
    PC_WRITE_AUTO_FIELD(row_ns);
    PC_WRITE_AUTO_FIELD(selected_bytes);
    PC_WRITE_AUTO_FIELD(retained_rows);
    PC_WRITE_AUTO_FIELD(retained_transitions);
    PC_WRITE_AUTO_FIELD(count_candidate);
#undef PC_WRITE_AUTO_FIELD
    write_evidence(out, value.evidence);
}

SolveTransitionCache::AutomaticCandidateRecord read_automatic_record(
        PayloadReader& in) {
    SolveTransitionCache::AutomaticCandidateRecord value;
    value.state_id = in.pod<std::uint32_t>();
    value.operator_index = in.pod<std::uint32_t>();
    value.candidate_id = in.string();
    value.candidate_kind = in.pod<AutomaticCandidateKind>();
    value.setup_action_id = in.string();
    value.followup_action_id = in.string();
    value.cleanup_action_id = in.string();
#define PC_READ_AUTO_FIELD(type, field) value.field = in.pod<type>()
    PC_READ_AUTO_FIELD(bool, eligible);
    PC_READ_AUTO_FIELD(bool, collapsed);
    PC_READ_AUTO_FIELD(bool, deferred);
    PC_READ_AUTO_FIELD(bool, missing_price);
    PC_READ_AUTO_FIELD(AutomaticTelemetryKind, telemetry_kind);
    PC_READ_AUTO_FIELD(bool, template_hit);
    PC_READ_AUTO_FIELD(std::uint64_t, template_id);
    PC_READ_AUTO_FIELD(std::uint64_t, raw_outcomes);
    PC_READ_AUTO_FIELD(std::uint64_t, admission_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, kernel_evaluation_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, outcome_mapping_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, template_matching_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_side_evaluations);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_repeat_evaluations);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_retry_checks);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_retry_certificates);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_retry_fallbacks);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_attempt_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_baseline_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_normalization_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, protected_finish_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, precompiled_classes);
    PC_READ_AUTO_FIELD(std::uint64_t, precompile_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, precompiled_bytes);
    PC_READ_AUTO_FIELD(std::uint64_t, candidate_variants);
    PC_READ_AUTO_FIELD(std::uint64_t, effect_classes);
    PC_READ_AUTO_FIELD(std::uint64_t, collapsed_variants);
    PC_READ_AUTO_FIELD(std::uint64_t, enumeration_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, row_ns);
    PC_READ_AUTO_FIELD(std::uint64_t, selected_bytes);
    PC_READ_AUTO_FIELD(std::uint64_t, retained_rows);
    PC_READ_AUTO_FIELD(std::uint64_t, retained_transitions);
    PC_READ_AUTO_FIELD(bool, count_candidate);
#undef PC_READ_AUTO_FIELD
    value.evidence = read_evidence(in);
    return value;
}

void write_action_envelope_ledger(
        PayloadWriter& out,
        const solve_detail::ActionEnvelopeLedger& ledger) {
    out.pod(ledger.scheduler_view_enabled);
    out.pod(ledger.transition_count());
    std::vector<std::uint64_t> keys;
    keys.reserve(ledger.entries().size());
    for (const auto& [key, unused] : ledger.entries()) {
        (void)unused;
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    out.pod(static_cast<std::uint64_t>(keys.size()));
    for (const std::uint64_t key : keys) {
        const solve_detail::ActionEnvelopeEntry& entry =
            ledger.entries().at(key);
        out.pod(key);
        out.pod(entry.state);
        out.pod(entry.operator_index);
        out.pod(entry.lifecycle);
        out.pod(entry.lane);
        out.pod(entry.authority);
        out.pod(entry.stop_owner);
        out.pod(entry.evidence);
        out.pod(entry.row_index);
        out.pod(entry.revision);
        out.string(entry.detail);
    }
}

solve_detail::ActionEnvelopeLedger read_action_envelope_ledger(
        PayloadReader& in) {
    const bool scheduler_view_enabled = in.pod<bool>();
    const std::uint64_t transition_count = in.pod<std::uint64_t>();
    const std::uint64_t count = in.pod<std::uint64_t>();
    if (count > in.remaining() / 40) {
        throw std::runtime_error(
            "invalid action-envelope count in development checkpoint");
    }
    solve_detail::ActionEnvelopeLedger::Map entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint64_t key = in.pod<std::uint64_t>();
        solve_detail::ActionEnvelopeEntry entry;
        entry.state = in.pod<std::uint32_t>();
        entry.operator_index = in.pod<std::uint32_t>();
        entry.lifecycle = in.pod<solve_detail::ActionEnvelopeState>();
        entry.lane = in.pod<solve_detail::ActionEnvelopeLane>();
        entry.authority =
            in.pod<solve_detail::ActionEnvelopeProofAuthority>();
        entry.stop_owner = in.pod<solve_detail::ActionEnvelopeStopOwner>();
        entry.evidence = in.pod<std::uint32_t>();
        entry.row_index = in.pod<std::uint64_t>();
        entry.revision = in.pod<std::uint64_t>();
        entry.detail = in.string();
        if (!entries.emplace(key, std::move(entry)).second) {
            throw std::runtime_error(
                "duplicate action-envelope entry in development checkpoint");
        }
    }
    solve_detail::ActionEnvelopeLedger ledger;
    ledger.restore_checkpoint(
        scheduler_view_enabled, transition_count, std::move(entries));
    return ledger;
}

void write_cache(PayloadWriter& out, const SolveTransitionCache& value) {
#define PC_WRITE_CACHE_FIELD(field) out.pod(value.field)
    PC_WRITE_CACHE_FIELD(start_state);
    out.pod_vector(value.operator_indices);
    PC_WRITE_CACHE_FIELD(max_states);
    PC_WRITE_CACHE_FIELD(max_discovered_states);
    PC_WRITE_CACHE_FIELD(max_expanded_states);
    PC_WRITE_CACHE_FIELD(max_state_action_rows);
    PC_WRITE_CACHE_FIELD(max_transitions);
    PC_WRITE_CACHE_FIELD(max_reforge_work);
    PC_WRITE_CACHE_FIELD(max_solver_owned_bytes);
    PC_WRITE_CACHE_FIELD(max_diagnostic_samples);
    PC_WRITE_CACHE_FIELD(full_evidence);
    PC_WRITE_CACHE_FIELD(kernel_reuse);
    PC_WRITE_CACHE_FIELD(goal_progress_gated_reforges);
    PC_WRITE_CACHE_FIELD(consider_imprint_programs);
    PC_WRITE_CACHE_FIELD(allow_economic_restart);
    PC_WRITE_CACHE_FIELD(discovered_states);
    PC_WRITE_CACHE_FIELD(expanded_states);
    PC_WRITE_CACHE_FIELD(strict_discovered_states);
    PC_WRITE_CACHE_FIELD(quotient_states);
    PC_WRITE_CACHE_FIELD(exact_quotient);
    out.pod_vector(value.behavioral_representative_by_state);
    out.pod_vector(value.expanded);
    out.pod_vector(value.state_rows);
    out.pod_vector(value.rows);
    PC_WRITE_CACHE_FIELD(accounts_variant_arena);
    out.pod_vector(value.variant_arena->variants);
    out.pod_vector(value.variant_arena->row_variant_indices);
    out.pod_vector(value.variant_arena->variant_quantities);
    out.pod_vector(value.successors);
    out.pod_vector(value.probabilities);
    out.pod_vector(value.choices);
    out.pod_vector(value.choice_successors);
    out.pod_vector(value.choice_options);
    PC_WRITE_CACHE_FIELD(automatic_rows_considered);
    PC_WRITE_CACHE_FIELD(automatic_rows_eligible);
    PC_WRITE_CACHE_FIELD(automatic_rows_rejected);
    PC_WRITE_CACHE_FIELD(automatic_rows_collapsed);
    PC_WRITE_CACHE_FIELD(automatic_rows_deferred);
    out.pod(value.automatic_kind_telemetry);
    out.pod(value.automatic_admission_phases);
    out.pod(static_cast<std::uint64_t>(
        value.automatic_candidate_samples.size()));
    for (const auto& record : value.automatic_candidate_samples) {
        write_automatic_record(out, record);
    }
    out.pod_vector(value.product_fracture_rows);
    write_action_envelope_ledger(out, value.action_envelope_ledger);
    PC_WRITE_CACHE_FIELD(algebraic_self_loops);
    PC_WRITE_CACHE_FIELD(focused_partial);
#undef PC_WRITE_CACHE_FIELD
}

std::shared_ptr<SolveTransitionCache> read_cache(PayloadReader& in) {
    auto value = std::make_shared<SolveTransitionCache>();
#define PC_READ_CACHE_FIELD(type, field) value->field = in.pod<type>()
    PC_READ_CACHE_FIELD(std::uint32_t, start_state);
    value->operator_indices = in.pod_vector<std::uint32_t>();
    PC_READ_CACHE_FIELD(std::uint32_t, max_states);
    PC_READ_CACHE_FIELD(std::uint32_t, max_discovered_states);
    PC_READ_CACHE_FIELD(std::uint32_t, max_expanded_states);
    PC_READ_CACHE_FIELD(std::uint64_t, max_state_action_rows);
    PC_READ_CACHE_FIELD(std::uint64_t, max_transitions);
    PC_READ_CACHE_FIELD(std::uint64_t, max_reforge_work);
    PC_READ_CACHE_FIELD(std::uint64_t, max_solver_owned_bytes);
    PC_READ_CACHE_FIELD(std::uint32_t, max_diagnostic_samples);
    PC_READ_CACHE_FIELD(bool, full_evidence);
    PC_READ_CACHE_FIELD(bool, kernel_reuse);
    PC_READ_CACHE_FIELD(bool, goal_progress_gated_reforges);
    PC_READ_CACHE_FIELD(bool, consider_imprint_programs);
    PC_READ_CACHE_FIELD(bool, allow_economic_restart);
    PC_READ_CACHE_FIELD(std::uint32_t, discovered_states);
    PC_READ_CACHE_FIELD(std::uint32_t, expanded_states);
    PC_READ_CACHE_FIELD(std::uint32_t, strict_discovered_states);
    PC_READ_CACHE_FIELD(std::uint32_t, quotient_states);
    PC_READ_CACHE_FIELD(bool, exact_quotient);
    value->behavioral_representative_by_state =
        in.pod_vector<std::uint32_t>();
    value->expanded = in.pod_vector<std::uint8_t>();
    value->state_rows = in.pod_vector<StateRowSpan>();
    value->rows = in.pod_vector<SparseRow>();
    PC_READ_CACHE_FIELD(bool, accounts_variant_arena);
    value->variant_arena = std::make_shared<SparseVariantArena>();
    value->variant_arena->variants = in.pod_vector<SparseVariant>();
    value->variant_arena->row_variant_indices =
        in.pod_vector<std::uint32_t>();
    value->variant_arena->variant_quantities = in.pod_vector<double>();
    value->successors = in.pod_vector<std::uint32_t>();
    value->probabilities = in.pod_vector<double>();
    value->choices = in.pod_vector<SparseChoiceGroup>();
    value->choice_successors = in.pod_vector<std::uint32_t>();
    value->choice_options = in.pod_vector<OutcomeChoiceOption>();
    PC_READ_CACHE_FIELD(std::uint32_t, automatic_rows_considered);
    PC_READ_CACHE_FIELD(std::uint32_t, automatic_rows_eligible);
    PC_READ_CACHE_FIELD(std::uint32_t, automatic_rows_rejected);
    PC_READ_CACHE_FIELD(std::uint32_t, automatic_rows_collapsed);
    PC_READ_CACHE_FIELD(std::uint32_t, automatic_rows_deferred);
    value->automatic_kind_telemetry = in.pod<decltype(
        value->automatic_kind_telemetry)>();
    value->automatic_admission_phases =
        in.pod<AutomaticAdmissionPhaseTelemetry>();
    const std::uint64_t samples = in.pod<std::uint64_t>();
    if (samples > value->max_diagnostic_samples ||
        samples > in.remaining() / 32) {
        throw std::runtime_error(
            "invalid automatic sample count in development checkpoint");
    }
    value->automatic_candidate_samples.reserve(
        static_cast<std::size_t>(samples));
    for (std::uint64_t i = 0; i < samples; ++i) {
        value->automatic_candidate_samples.push_back(
            read_automatic_record(in));
    }
    value->reconcile_automatic_sample_owned_bytes();
    value->product_fracture_rows =
        in.pod_vector<SolveTransitionCache::ProductFractureRowWitness>();
    value->action_envelope_ledger = read_action_envelope_ledger(in);
    PC_READ_CACHE_FIELD(std::uint64_t, algebraic_self_loops);
    PC_READ_CACHE_FIELD(bool, focused_partial);
#undef PC_READ_CACHE_FIELD
    return value;
}

void validate_cache(
        const CalcContext& calc,
        const std::vector<AbstractState>& states,
        const std::size_t operator_count,
        const SolveTransitionCache& cache) {
    const auto fail = [](const char* reason) {
        throw std::runtime_error(
            std::string("invalid solver development checkpoint graph: ") +
            reason);
    };
    if (cache.focused_partial) fail("focused partial closure");
    if (cache.quotient_proofs != nullptr) fail("strict proof store present");
    if (cache.variant_arena == nullptr) fail("missing variant arena");
    if (cache.discovered_states != states.size() ||
        cache.expanded.size() != states.size() ||
        cache.state_rows.size() != states.size()) {
        fail("state cardinality mismatch");
    }
    if (!cache.behavioral_representative_by_state.empty() &&
        cache.behavioral_representative_by_state.size() != states.size()) {
        fail("behavioral representative cardinality mismatch");
    }
    if (cache.start_state >= states.size()) fail("start state out of range");
    for (const std::uint32_t index : cache.operator_indices) {
        if (index >= operator_count) fail("operator index out of range");
    }
    for (std::size_t state = 0; state < states.size(); ++state) {
        const bool represented =
            !cache.behavioral_representative_by_state.empty() &&
            cache.behavioral_representative_by_state[state] != state &&
            cache.behavioral_representative_by_state[state] < states.size();
        if (!cache.expanded[state] && !calc.is_goal_state(states[state]) &&
            !represented) {
            fail("nonterminal representative state is not expanded");
        }
        const StateRowSpan span = cache.state_rows[state];
        if (span.count == 0) continue;
        if (span.offset >= cache.rows.size() ||
            span.tail >= cache.rows.size()) {
            fail("state row span out of range");
        }
        std::uint64_t row = span.offset;
        for (std::uint32_t i = 0; i < span.count; ++i) {
            if (row >= cache.rows.size() ||
                cache.rows[row].owner_state != state) {
                fail("state row ownership chain mismatch");
            }
            if (i + 1 == span.count) {
                if (row != span.tail) fail("state row tail mismatch");
            } else {
                row = cache.rows[row].next_owner_row;
            }
        }
    }
    for (const SparseRow& row : cache.rows) {
        if (row.owner_state >= states.size() ||
            row.transition_offset + row.transition_count >
                cache.successors.size() ||
            row.transition_offset + row.transition_count >
                cache.probabilities.size() ||
            row.choice_offset + row.choice_count > cache.choices.size() ||
            row.variant_offset + row.variant_count >
                cache.variant_arena->row_variant_indices.size()) {
            fail("row arena range out of bounds");
        }
    }
    for (const std::uint32_t successor : cache.successors) {
        if (successor >= states.size()) fail("successor out of range");
    }
    for (const double probability : cache.probabilities) {
        if (!std::isfinite(probability) || probability < 0.0) {
            fail("invalid transition probability");
        }
    }
    for (const SparseChoiceGroup& choice : cache.choices) {
        if (!std::isfinite(choice.probability) || choice.probability < 0.0 ||
            choice.successor_offset + choice.successor_count >
                cache.choice_successors.size()) {
            fail("invalid choice group");
        }
    }
    for (const std::uint32_t successor : cache.choice_successors) {
        if (successor >= states.size()) fail("choice successor out of range");
    }
    const auto valid_optional_state = [&](const std::uint32_t state) {
        return state == kNoId || state < states.size();
    };
    for (const OutcomeChoiceOption& option : cache.choice_options) {
        if (!valid_optional_state(option.state) ||
            !valid_optional_state(option.observation_state) ||
            !valid_optional_state(option.actual_state)) {
            fail("choice option state out of range");
        }
    }
    for (const std::uint32_t variant :
         cache.variant_arena->row_variant_indices) {
        if (variant != kNoId &&
            variant >= cache.variant_arena->variants.size()) {
            fail("row variant index out of range");
        }
    }
    for (const SparseVariant& variant : cache.variant_arena->variants) {
        if (variant.operator_index >= operator_count ||
            variant.quantity_offset + variant.quantity_count >
                cache.variant_arena->variant_quantities.size() ||
            variant.choice_option_offset + variant.choice_option_count >
                cache.choice_options.size()) {
            fail("variant arena range out of bounds");
        }
    }
    for (const auto& [unused_key, entry] :
         cache.action_envelope_ledger.entries()) {
        (void)unused_key;
        if ((entry.state != kNoId && entry.state >= states.size()) ||
            entry.operator_index >= operator_count ||
            (entry.row_index != std::numeric_limits<std::uint64_t>::max() &&
             entry.row_index >= cache.rows.size())) {
            throw std::runtime_error(
                "invalid solver development checkpoint graph: "
                "action-envelope reference out of range (state=" +
                std::to_string(entry.state) + ", operator=" +
                std::to_string(entry.operator_index) + ", row=" +
                std::to_string(entry.row_index) + ", states=" +
                std::to_string(states.size()) + ", operators=" +
                std::to_string(operator_count) + ", rows=" +
                std::to_string(cache.rows.size()) + ")");
        }
    }
}

std::array<std::uint32_t, 9> format_sizes() {
    return {
        static_cast<std::uint32_t>(sizeof(AbstractState)),
        static_cast<std::uint32_t>(sizeof(SparseRow)),
        static_cast<std::uint32_t>(sizeof(StateRowSpan)),
        static_cast<std::uint32_t>(sizeof(SparseVariant)),
        static_cast<std::uint32_t>(sizeof(SparseChoiceGroup)),
        static_cast<std::uint32_t>(sizeof(OutcomeChoiceOption)),
        static_cast<std::uint32_t>(sizeof(AutomaticKindTelemetry)),
        static_cast<std::uint32_t>(sizeof(AutomaticAdmissionPhaseTelemetry)),
        static_cast<std::uint32_t>(sizeof(
            SolveTransitionCache::ProductFractureRowWitness)),
    };
}

} // namespace

void CalcContext::save_development_solve_checkpoint(
        const std::string& path,
        const std::string_view caller_identity) const {
    static_assert(std::is_trivially_copyable_v<AbstractState>);
    static_assert(std::is_trivially_copyable_v<SparseRow>);
    static_assert(std::is_trivially_copyable_v<AutomaticKindTelemetry>);
    if (path.empty() || caller_identity.empty()) {
        throw std::invalid_argument(
            "development checkpoint path and identity are required");
    }
    if (state_local_automatic_admission_cursor_.has_value() ||
        reforge_evaluation_cursor_.has_value()) {
        throw std::runtime_error(
            "development checkpoint refused while a calculator row is in flight");
    }
    if (solve_transition_cache_ == nullptr) {
        throw std::runtime_error(
            "development checkpoint requires a completed reusable coarse graph");
    }
    validate_cache(
        *this, states_, operators_.size(), *solve_transition_cache_);

    const fs::path destination(path);
    const fs::path temporary = destination.string() + ".tmp";
    std::error_code error;
    fs::remove(temporary, error);
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error(
            "could not open solver development checkpoint for writing");
    }
    stream.write(kMagic.data(), kMagic.size());
    write_plain_header(stream, kFormatVersion);
    write_plain_header(stream, kEndianMarker);
    for (const std::uint32_t size : format_sizes()) {
        write_plain_header(stream, size);
    }
    const std::streampos payload_size_position = stream.tellp();
    write_plain_header(stream, std::uint64_t{0});
    write_plain_header(stream, std::uint64_t{0});

    PayloadWriter out(stream);
    out.string(caller_identity);
    out.pod(static_cast<std::uint64_t>(initial_operator_count_));
    out.pod(static_cast<std::uint64_t>(static_candidate_operator_count_));
    out.pod(static_cast<std::uint64_t>(operators_.size()));
    for (std::size_t i = initial_operator_count_; i < operators_.size(); ++i) {
        write_planner(out, operators_[i]);
    }
    out.pod_vector(candidate_operators_);
    std::vector<std::uint32_t> dependencies(
        admitted_automatic_dependencies_.begin(),
        admitted_automatic_dependencies_.end());
    std::sort(dependencies.begin(), dependencies.end());
    out.pod_vector(dependencies);
    std::vector<std::uint32_t> state_local_indices(
        state_local_automatic_operator_indices_.begin(),
        state_local_automatic_operator_indices_.end());
    std::sort(state_local_indices.begin(), state_local_indices.end());
    out.pod_vector(state_local_indices);
    std::vector<std::uint32_t> carrier_ids;
    carrier_ids.reserve(state_local_automatic_operators_.size());
    for (const auto& [state, unused] : state_local_automatic_operators_) {
        (void)unused;
        carrier_ids.push_back(state);
    }
    std::sort(carrier_ids.begin(), carrier_ids.end());
    out.pod(static_cast<std::uint64_t>(carrier_ids.size()));
    for (const std::uint32_t state : carrier_ids) {
        out.pod(state);
        out.pod_vector(state_local_automatic_operators_.at(state));
    }
    out.pod_vector(states_);
    write_cache(out, *solve_transition_cache_);

    stream.flush();
    if (!stream) {
        throw std::runtime_error(
            "failed to flush solver development checkpoint");
    }
    stream.seekp(payload_size_position);
    write_plain_header(stream, out.size());
    write_plain_header(stream, out.checksum());
    stream.close();
    if (!stream) {
        throw std::runtime_error(
            "failed to close solver development checkpoint");
    }
    fs::remove(destination, error);
    error.clear();
    fs::rename(temporary, destination, error);
    if (error) {
        fs::remove(temporary);
        throw std::runtime_error(
            "failed to publish solver development checkpoint: " +
            error.message());
    }
}

void CalcContext::load_development_solve_checkpoint(
        const std::string& path,
        const std::string_view expected_caller_identity) {
    if (path.empty() || expected_caller_identity.empty()) {
        throw std::invalid_argument(
            "development checkpoint path and identity are required");
    }
    if (state_local_automatic_admission_cursor_.has_value() ||
        reforge_evaluation_cursor_.has_value() ||
        solve_transition_cache_ != nullptr || state_count() != 0 ||
        development_checkpoint_replay_pending_) {
        throw std::runtime_error(
            "development checkpoint load requires a fresh solver context");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error(
            "could not open solver development checkpoint for reading");
    }
    std::array<char, kMagic.size()> magic{};
    stream.read(magic.data(), magic.size());
    if (!stream || magic != kMagic) {
        throw std::runtime_error(
            "invalid solver development checkpoint magic");
    }
    if (read_plain_header<std::uint32_t>(stream) != kFormatVersion) {
        throw std::runtime_error(
            "unsupported solver development checkpoint version");
    }
    if (read_plain_header<std::uint32_t>(stream) != kEndianMarker) {
        throw std::runtime_error(
            "solver development checkpoint byte order mismatch");
    }
    for (const std::uint32_t expected : format_sizes()) {
        if (read_plain_header<std::uint32_t>(stream) != expected) {
            throw std::runtime_error(
                "solver development checkpoint binary layout mismatch");
        }
    }
    const std::uint64_t payload_size =
        read_plain_header<std::uint64_t>(stream);
    const std::uint64_t expected_checksum =
        read_plain_header<std::uint64_t>(stream);
    const std::streampos payload_start = stream.tellg();
    stream.seekg(0, std::ios::end);
    const std::streampos file_end = stream.tellg();
    if (payload_start < 0 || file_end < payload_start ||
        static_cast<std::uint64_t>(file_end - payload_start) != payload_size) {
        throw std::runtime_error(
            "solver development checkpoint payload length mismatch");
    }
    stream.seekg(payload_start);
    PayloadReader in(stream, payload_size);
    if (in.string() != expected_caller_identity) {
        throw std::runtime_error(
            "solver development checkpoint caller identity mismatch");
    }
    const std::uint64_t saved_initial_operators = in.pod<std::uint64_t>();
    const std::uint64_t saved_static_candidates = in.pod<std::uint64_t>();
    const std::uint64_t saved_operator_count = in.pod<std::uint64_t>();
    if (saved_initial_operators != initial_operator_count_ ||
        saved_static_candidates != static_candidate_operator_count_ ||
        saved_operator_count < saved_initial_operators ||
        saved_operator_count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(
            "solver development checkpoint static planner mismatch");
    }
    std::vector<PlannerOperator> dynamic_operators;
    dynamic_operators.reserve(static_cast<std::size_t>(
        saved_operator_count - saved_initial_operators));
    for (std::uint64_t i = saved_initial_operators;
         i < saved_operator_count; ++i) {
        dynamic_operators.push_back(read_planner(in));
    }
    std::vector<std::uint32_t> candidate_operators =
        in.pod_vector<std::uint32_t>();
    std::vector<std::uint32_t> dependencies =
        in.pod_vector<std::uint32_t>();
    std::vector<std::uint32_t> state_local_indices =
        in.pod_vector<std::uint32_t>();
    const std::uint64_t carrier_count = in.pod<std::uint64_t>();
    if (carrier_count > in.remaining() / sizeof(std::uint64_t)) {
        throw std::runtime_error(
            "invalid carrier count in solver development checkpoint");
    }
    std::vector<std::pair<std::uint32_t, std::vector<std::uint32_t>>>
        carrier_operators;
    carrier_operators.reserve(static_cast<std::size_t>(carrier_count));
    for (std::uint64_t i = 0; i < carrier_count; ++i) {
        const std::uint32_t state = in.pod<std::uint32_t>();
        std::vector<std::uint32_t> operators =
            in.pod_vector<std::uint32_t>();
        carrier_operators.emplace_back(
            state, std::move(operators));
    }
    std::vector<AbstractState> states = in.pod_vector<AbstractState>();
    std::shared_ptr<SolveTransitionCache> cache = read_cache(in);
    if (in.remaining() != 0 || in.checksum() != expected_checksum) {
        throw std::runtime_error(
            "solver development checkpoint checksum mismatch");
    }

    const auto index_in_range = [&](const std::uint32_t index) {
        return index < saved_operator_count;
    };
    if (!std::all_of(candidate_operators.begin(), candidate_operators.end(),
                     index_in_range) ||
        !std::all_of(dependencies.begin(), dependencies.end(),
                     index_in_range) ||
        !std::all_of(state_local_indices.begin(), state_local_indices.end(),
                     index_in_range)) {
        throw std::runtime_error(
            "solver development checkpoint planner index out of range");
    }
    for (const auto& [state, operators] : carrier_operators) {
        if (state >= states.size() ||
            !std::all_of(operators.begin(), operators.end(), index_in_range)) {
            throw std::runtime_error(
                "solver development checkpoint carrier admission mismatch");
        }
    }
    validate_cache(*this, states, saved_operator_count, *cache);

    std::unordered_set<std::uint32_t> saved_state_local(
        state_local_indices.begin(), state_local_indices.end());
    for (std::size_t offset = 0; offset < dynamic_operators.size(); ++offset) {
        const std::uint32_t expected_index = static_cast<std::uint32_t>(
            initial_operator_count_ + offset);
        const std::uint32_t imported = import_planner_operator(
            dynamic_operators[offset],
            saved_state_local.contains(expected_index));
        if (imported != expected_index) {
            throw std::runtime_error(
                "solver development checkpoint dynamic planner order mismatch");
        }
    }
    if (operators_.size() != saved_operator_count) {
        throw std::runtime_error(
            "solver development checkpoint planner cardinality mismatch");
    }
    candidate_operators_ = std::move(candidate_operators);
    admitted_automatic_dependencies_.clear();
    admitted_automatic_dependencies_.insert(
        dependencies.begin(), dependencies.end());
    state_local_automatic_operator_indices_.clear();
    state_local_automatic_operator_indices_.insert(
        state_local_indices.begin(), state_local_indices.end());
    state_local_automatic_operators_.clear();
    owned_state_local_operator_bytes_ = 0;
    for (auto& [state, values] : carrier_operators) {
        auto [entry, inserted] = state_local_automatic_operators_.emplace(
            state, std::move(values));
        if (!inserted) {
            throw std::runtime_error(
                "solver development checkpoint duplicate carrier admission");
        }
        account_state_local_operators(entry->second);
    }
    action_control_.automatic_options = static_cast<std::uint32_t>(
        std::count_if(
            candidate_operators_.begin(), candidate_operators_.end(),
            [&](const std::uint32_t index) {
                return operators_.at(index).automatic_kind !=
                    AutomaticCandidateKind::None;
            }));
    action_control_.automatic_dependency_primitives =
        static_cast<std::uint32_t>(dependencies.size());
    for (const AbstractState& state : states) {
        const std::uint32_t expected = state_count();
        if (intern_state(state) != expected) {
            throw std::runtime_error(
                "solver development checkpoint state interning order mismatch");
        }
    }
    solve_transition_cache_ = std::move(cache);
    development_checkpoint_replay_pending_ = true;
    development_checkpoint_replay_required_ = true;
}

} // namespace solver
} // namespace poecraft
