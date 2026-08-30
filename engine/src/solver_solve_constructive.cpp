#include "solver_solve_types.hpp"
#include "solver_compile_contracts.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

const char* solve_detail::retained_fallback_invalid_reason(
        const CertifiedFallbackContract& candidate,
        const CertifiedFallbackCurrentContext& current) {
        if (!candidate.complete_policy_or_witness ||
            !candidate.compiled_payload_present ||
            !candidate.compilation_provenance_present) {
            return "retained_artifact_provenance_missing";
        }
        if (candidate.goal_identity != current.goal_identity) {
            return "goal_identity_changed";
        }
        if (candidate.economy_identity != current.economy_identity) {
            return "economy_identity_changed";
        }
        if (candidate.action_vocabulary_size >
                current.action_vocabulary_size ||
            candidate.action_vocabulary_identity !=
                current.action_vocabulary_identity) {
            return "action_vocabulary_changed";
        }
        if (candidate.caller_scope_identity !=
            current.caller_scope_identity) {
            return "caller_scope_changed";
        }
        if (candidate.artifact_identity != current.artifact_identity) {
            return "artifact_generation_changed";
        }
        if (candidate.source_generation > current.source_generation ||
            candidate.target_generation > current.target_generation) {
            return "graph_generation_rewound";
        }
        if (candidate.graph_prefix_identity !=
            current.graph_prefix_identity) {
            return "graph_prefix_changed";
        }
        return nullptr;
    }

const char* solve_detail::certified_fallback_invalid_reason(
        const CertifiedFallbackContract& candidate,
        const CertifiedFallbackCurrentContext& current,
        const double epsilon) {
        if (const char* retained =
                retained_fallback_invalid_reason(candidate, current)) {
            return retained;
        }
        if (!candidate.independently_evaluated) {
            return "final_graph_not_independently_evaluated";
        }
        if (!candidate.proper) return "improper_policy";
        if (!candidate.executable) return "policy_not_executable";
        if (!std::isfinite(candidate.certified_upper_bound) ||
            !std::isfinite(candidate.evaluated_policy_cost) ||
            candidate.evaluated_policy_cost < 0.0 ||
            candidate.evaluated_policy_cost >
                candidate.certified_upper_bound +
                    epsilon * std::max(
                        1.0,
                        std::abs(candidate.certified_upper_bound)) *
                        10.0) {
            return "evaluated_cost_invalid";
        }
        return nullptr;
    }

bool solve_detail::certified_fallback_precedes(
        const CertifiedFallbackContract& left,
        const CertifiedFallbackContract& right) {
        if (left.certified_upper_bound !=
            right.certified_upper_bound) {
            return left.certified_upper_bound <
                right.certified_upper_bound;
        }
        if (left.root_operator != right.root_operator) {
            return left.root_operator < right.root_operator;
        }
        if (left.kind != right.kind) return left.kind < right.kind;
        if (left.witness_identity != right.witness_identity) {
            return left.witness_identity < right.witness_identity;
        }
        return left.portfolio_identity < right.portfolio_identity;
    }

bool solve_detail::certified_fallback_fits_memory(
        const std::uint64_t current_owned_bytes,
        const std::uint64_t candidate_owned_bytes,
        const std::uint64_t maximum_owned_bytes) {
        return current_owned_bytes <= maximum_owned_bytes &&
            candidate_owned_bytes <=
                maximum_owned_bytes - current_owned_bytes;
    }

CapturedBoundedPolicyRow solve_detail::capture_bounded_policy_row(
        const CalcContext& calc,
        const SolveTransitionCache& transition_cache,
        const std::vector<PricedSparseRow>& priced_rows,
        const std::uint32_t state,
        const std::uint64_t row,
        const std::uint32_t fallback_operator) {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t selected_operator = fallback_operator;
        CapturedBoundedPolicyRow captured;
        if (row != no_row) {
            if (row >= priced_rows.size() ||
                row >= transition_cache.rows.size() ||
                transition_cache.rows[row].owner_state != state) {
                throw std::logic_error(
                    "bounded incumbent row does not belong to its state");
            }
            const PricedSparseRow& priced = priced_rows[row];
            selected_operator = priced.operator_index;
            captured.cost = priced.cost;
            if (priced.choice_option_offset >
                    transition_cache.choice_options.size() ||
                priced.choice_option_count >
                    transition_cache.choice_options.size() -
                        priced.choice_option_offset) {
                throw std::logic_error(
                    "bounded incumbent row choice payload is out of range");
            }
            captured.choice_options.insert(
                captured.choice_options.end(),
                transition_cache.choice_options.begin() +
                    static_cast<std::ptrdiff_t>(
                        priced.choice_option_offset),
                transition_cache.choice_options.begin() +
                    static_cast<std::ptrdiff_t>(
                        priced.choice_option_offset +
                        priced.choice_option_count));
        }
        if (selected_operator == kNoId ||
            selected_operator >= calc.operators().size()) {
            throw std::logic_error(
                "bounded incumbent has a reachable state without an "
                "executable action");
        }
        const PlannerOperator& planner =
            calc.operators().at(selected_operator);
        captured.policy =
            PolicyOperatorRef{planner.kind, selected_operator};
        return captured;
    }

 void SolveWork::Impl::identity_mix(
        std::uint64_t& hash, const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }

 void SolveWork::Impl::identity_mix_string(
        std::uint64_t& hash, const std::string_view value) {
        for (const unsigned char byte : value) identity_mix(hash, byte);
        identity_mix(hash, value.size());
    }

std::uint64_t SolveWork::Impl::goal_identity() const {
        std::uint64_t hash = 1469598103934665603ULL;
        identity_mix(hash, 2); /* exact-explicit-affix terminal schema */
        identity_mix(hash, calc.goal().rarity);
        identity_mix(hash, calc.goal().required_satisfied_slots());
        identity_mix(hash, calc.layout().slots.size());
        for (const auto& slot : calc.layout().slots) {
            identity_mix(hash, slot.satisfying_mask.size());
            for (const std::uint64_t word : slot.satisfying_mask) {
                identity_mix(hash, word);
            }
        }
        return hash;
    }

std::uint64_t SolveWork::Impl::economy_identity() const {
        std::uint64_t hash = 1469598103934665603ULL;
        /* Preserve the original lexicographic key/value identity without
         * allocating a sorted copy. This path can run while a resource-stop
         * candidate and fixed-policy scratch are simultaneously live. */
        std::string_view previous;
        bool have_previous = false;
        for (std::size_t mixed = 0; mixed < prices.size(); ++mixed) {
            const std::pair<const std::string, double>* next = nullptr;
            for (const auto& entry : prices) {
                if (have_previous && !(previous < entry.first)) continue;
                if (next == nullptr || entry.first < next->first) {
                    next = &entry;
                }
            }
            if (next == nullptr) {
                throw std::logic_error(
                    "economy identity ordering did not consume every key");
            }
            identity_mix_string(hash, next->first);
            identity_mix(
                hash, std::bit_cast<std::uint64_t>(next->second));
            previous = next->first;
            have_previous = true;
        }
        return hash;
    }

std::uint64_t SolveWork::Impl::caller_scope_identity() const {
        std::uint64_t hash = 1469598103934665603ULL;
        identity_mix(hash, 1); /* caller-scope identity schema */
        identity_mix(hash, calc.action_control().explicit_envelope);
        identity_mix(hash, options.goal_progress_gated_reforges);
        identity_mix(hash, options.consider_imprint_programs);
        identity_mix(hash, options.allow_economic_restart);
        identity_mix(hash, calc.candidates().size());
        for (const std::uint32_t action : calc.candidates()) {
            identity_mix(hash, action);
        }
        return hash;
    }

std::uint64_t SolveWork::Impl::action_vocabulary_prefix_identity(
        const std::size_t count) const {
        std::uint64_t hash = 1469598103934665603ULL;
        const std::size_t retained = std::min(count, operators.size());
        for (std::size_t position = 0; position < retained; ++position) {
            const PricedOperator& priced = operators[position];
            const PlannerOperator& planner = calc.operators().at(priced.index);
            identity_mix(hash, priced.index);
            identity_mix(hash, static_cast<std::uint64_t>(planner.kind));
            identity_mix_string(hash, planner.id);
        }
        return hash;
    }

std::uint64_t SolveWork::Impl::action_vocabulary_identity() const {
        return action_vocabulary_prefix_identity(operators.size());
    }

std::uint64_t SolveWork::Impl::graph_identity() const {
        std::uint64_t hash = 1469598103934665603ULL;
        identity_mix(hash, transition_cache->discovered_states);
        identity_mix(hash, transition_cache->rows.size());
        identity_mix(hash, transition_cache->successors.size());
        identity_mix(hash, transition_cache->choice_successors.size());
        for (const SparseRow& row : transition_cache->rows) {
            identity_mix(hash, row.owner_state);
            identity_mix(hash, row.transition_offset);
            identity_mix(hash, row.transition_count);
            identity_mix(hash, row.choice_offset);
            identity_mix(hash, row.choice_count);
        }
        return hash;
    }

std::uint64_t SolveWork::Impl::artifact_identity() const {
        std::uint64_t hash = 1469598103934665603ULL;
        const DataImpl& data = *calc.session().data;
        identity_mix(hash, data.artifact_schema_version);
        identity_mix_string(hash, data.artifact_data_hash);
        identity_mix_string(hash, data.artifact_source_hash);
        identity_mix_string(hash, data.artifact_game_data_hash);
        identity_mix_string(hash, data.artifact_strings_hash);
        identity_mix(hash, calc.session().base_index);
        identity_mix(hash, calc.session().item_level);
        return hash;
    }

std::uint64_t SolveWork::Impl::incumbent_graph_prefix_identity(
        const std::uint64_t row_count,
        const std::uint64_t priced_row_count,
        const std::uint64_t successor_count,
        const std::uint64_t probability_count,
        const std::uint64_t choice_count,
        const std::uint64_t choice_successor_count,
        const std::uint64_t choice_option_count) const {
        std::uint64_t hash = fallback_graph_prefix_identity(
            row_count, priced_row_count);
        identity_mix(
            hash,
            fallback_transition_prefix_identity(
                successor_count, probability_count, choice_count,
                choice_successor_count, choice_option_count));
        return hash;
    }

std::uint64_t SolveWork::Impl::fallback_policy_identity(
        const FocusedFallbackPolicy& fallback) const {
        std::uint64_t hash = 1469598103934665603ULL;
        const auto mix_double = [&](const double value) {
            identity_mix(hash, std::bit_cast<std::uint64_t>(value));
        };
        identity_mix(hash, fallback.anchor_state);
        mix_double(fallback.anchor_state_value);
        identity_mix(hash, fallback.anchor_row);
        identity_mix(hash, fallback.anchor_operator);
        mix_double(fallback.renewal_state_value);
        identity_mix(hash, fallback.renewal_row);
        identity_mix(hash, fallback.renewal_operator);
        identity_mix(hash, fallback.finish_action);
        identity_mix(hash, fallback.renewal_rarity);
        identity_mix(hash, fallback.renewal_influence_bits);
        identity_mix(hash, fallback.renewal_searing_exarch_tier);
        identity_mix(hash, fallback.renewal_eater_of_worlds_tier);
        identity_mix(hash, fallback.renewal_kernel_signature.size());
        for (const std::uint64_t value :
             fallback.renewal_kernel_signature) {
            identity_mix(hash, value);
        }
        identity_mix(hash, fallback.primitive_renewal_modes.size());
        for (const auto& mode : fallback.primitive_renewal_modes) {
            mix_double(mode.value);
            identity_mix(hash, mode.operator_index);
            identity_mix(hash, mode.kernel_signature.size());
            for (const std::uint64_t value : mode.kernel_signature) {
                identity_mix(hash, value);
            }
        }
        std::vector<std::pair<std::uint32_t, double>> progress_values(
            fallback.progress_state_value.begin(),
            fallback.progress_state_value.end());
        std::sort(progress_values.begin(), progress_values.end());
        identity_mix(hash, progress_values.size());
        for (const auto& [state, value] : progress_values) {
            identity_mix(hash, state);
            mix_double(value);
        }
        std::vector<std::pair<std::uint32_t, std::uint32_t>>
            progress_operators(
                fallback.progress_state_operator.begin(),
                fallback.progress_state_operator.end());
        std::sort(progress_operators.begin(), progress_operators.end());
        identity_mix(hash, progress_operators.size());
        for (const auto& [state, op] : progress_operators) {
            identity_mix(hash, state);
            identity_mix(hash, op);
        }
        identity_mix(hash, fallback.goal_identity);
        identity_mix(hash, fallback.economy_identity);
        identity_mix(hash, fallback.action_vocabulary_identity);
        identity_mix(hash, fallback.action_vocabulary_size);
        identity_mix(hash, fallback.synthesis_graph_identity);
        return hash;
    }

std::uint64_t SolveWork::Impl::fallback_graph_prefix_identity(
        const std::uint64_t row_count,
        const std::uint64_t priced_row_count) const {
        if (row_count > transition_cache->rows.size() ||
            priced_row_count > priced_rows.size()) {
            return 0;
        }
        std::uint64_t hash = 1469598103934665603ULL;
        identity_mix(hash, row_count);
        for (std::uint64_t index = 0; index < row_count; ++index) {
            const SparseRow& row = transition_cache->rows[index];
            identity_mix(hash, row.owner_state);
            identity_mix(hash, row.variant_offset);
            identity_mix(hash, row.variant_count);
            identity_mix(hash, row.variant_capacity);
            identity_mix(hash, row.transition_offset);
            identity_mix(hash, row.transition_count);
            identity_mix(
                hash, std::bit_cast<std::uint64_t>(row.self_probability));
            identity_mix(
                hash,
                std::bit_cast<std::uint64_t>(
                    row.embedded_self_probability));
            identity_mix(hash, row.self_probability_embedded);
            identity_mix(hash, row.choice_offset);
            identity_mix(hash, row.choice_count);
        }
        identity_mix(hash, priced_row_count);
        for (std::uint64_t index = 0;
             index < priced_row_count; ++index) {
            const PricedSparseRow& row = priced_rows[index];
            identity_mix(hash, row.operator_index);
            identity_mix(hash, std::bit_cast<std::uint64_t>(row.cost));
            identity_mix(hash, row.choice_option_offset);
            identity_mix(hash, row.choice_option_count);
        }
        return hash;
    }

std::uint64_t SolveWork::Impl::fallback_transition_prefix_identity(
        const std::uint64_t successor_count,
        const std::uint64_t probability_count,
        const std::uint64_t choice_count,
        const std::uint64_t choice_successor_count,
        const std::uint64_t choice_option_count) const {
        if (successor_count > transition_cache->successors.size() ||
            probability_count > transition_cache->probabilities.size() ||
            choice_count > transition_cache->choices.size() ||
            choice_successor_count >
                transition_cache->choice_successors.size() ||
            choice_option_count > transition_cache->choice_options.size()) {
            return 0;
        }
        std::uint64_t hash = 1469598103934665603ULL;
        identity_mix(hash, successor_count);
        for (std::uint64_t index = 0;
             index < successor_count; ++index) {
            identity_mix(hash, transition_cache->successors[index]);
        }
        identity_mix(hash, probability_count);
        for (std::uint64_t index = 0;
             index < probability_count; ++index) {
            identity_mix(
                hash,
                std::bit_cast<std::uint64_t>(
                    transition_cache->probabilities[index]));
        }
        identity_mix(hash, choice_count);
        for (std::uint64_t index = 0; index < choice_count; ++index) {
            const SparseChoiceGroup& choice =
                transition_cache->choices[index];
            identity_mix(hash, choice.successor_offset);
            identity_mix(hash, choice.successor_count);
            identity_mix(
                hash,
                std::bit_cast<std::uint64_t>(choice.probability));
            identity_mix(hash, choice.has_self);
        }
        identity_mix(hash, choice_successor_count);
        for (std::uint64_t index = 0;
             index < choice_successor_count; ++index) {
            identity_mix(
                hash, transition_cache->choice_successors[index]);
        }
        identity_mix(hash, choice_option_count);
        for (std::uint64_t index = 0;
             index < choice_option_count; ++index) {
            const OutcomeChoiceOption& option =
                transition_cache->choice_options[index];
            identity_mix(hash, option.mod_id);
            identity_mix(hash, option.state);
            identity_mix(hash, option.observation_state);
            identity_mix(hash, option.actual_state);
        }
        return hash;
    }

bool SolveWork::Impl::reuse_successful_fallback_properness_proof(
        const FocusedFallbackPolicy& fallback) {
        FallbackValidationTelemetry& telemetry =
            result.diagnostics.fallback_validation;
        ++telemetry.successful_proof_cache_checks;
        ++telemetry.successful_proof_identity.checks;
        const auto started = std::chrono::steady_clock::now();
        const auto finish = [&](const bool matched, const char* reason) {
            telemetry.successful_proof_identity.duration_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count());
            if (matched) {
                ++telemetry.successful_proof_cache_hits;
            } else {
                ++telemetry.successful_proof_cache_misses;
                telemetry.successful_proof_last_miss_reason = reason;
            }
            return matched;
        };
        if (!options.fallback_properness_reuse_control) {
            return finish(false, "disabled");
        }
        if (!successful_fallback_properness_proof.has_value()) {
            return finish(false, "no_successful_proof");
        }
        const SuccessfulFallbackPropernessProof& proof =
            *successful_fallback_properness_proof;
        if (proof.version !=
                SuccessfulFallbackPropernessProof::kVersion) {
            return finish(false, "solver_proof_version_changed");
        }
        if (!proof.policy || proof.policy.get() != &fallback ||
            proof.policy_identity != fallback_policy_identity(fallback)) {
            return finish(false, "policy_identity_changed");
        }
        if (proof.graph != transition_cache.get()) {
            return finish(false, "graph_owner_changed");
        }
        if (proof.mechanics != &session) {
            return finish(false, "transition_mechanics_changed");
        }
        if (proof.goal_identity != goal_identity()) {
            return finish(false, "goal_identity_changed");
        }
        if (proof.economy_identity != economy_identity()) {
            return finish(false, "economy_identity_changed");
        }
        if (operators.size() < proof.action_vocabulary_size ||
            proof.action_vocabulary_identity !=
                action_vocabulary_prefix_identity(
                    proof.action_vocabulary_size)) {
            return finish(false, "action_vocabulary_prefix_changed");
        }
        if (proof.graph_prefix_identity !=
                fallback_graph_prefix_identity(
                    proof.row_count, proof.priced_row_count)) {
            return finish(false, "graph_prefix_changed");
        }
        if (proof.transition_prefix_identity !=
                fallback_transition_prefix_identity(
                    proof.successor_count, proof.probability_count,
                    proof.choice_count, proof.choice_successor_count,
                    proof.choice_option_count)) {
            return finish(false, "transition_prefix_changed");
        }
        return finish(true, nullptr);
    }

void SolveWork::Impl::remember_successful_fallback_properness_proof(
        const FocusedFallbackWitness& fallback) {
        if (!options.fallback_properness_reuse_control || !fallback) return;
        SuccessfulFallbackPropernessProof proof;
        proof.policy = fallback;
        proof.graph = transition_cache.get();
        proof.mechanics = &session;
        proof.goal_identity = goal_identity();
        proof.economy_identity = economy_identity();
        proof.action_vocabulary_size =
            static_cast<std::uint32_t>(operators.size());
        proof.action_vocabulary_identity =
            action_vocabulary_prefix_identity(
                proof.action_vocabulary_size);
        proof.policy_identity = fallback_policy_identity(*fallback);
        proof.row_count = transition_cache->rows.size();
        proof.priced_row_count = priced_rows.size();
        proof.successor_count = transition_cache->successors.size();
        proof.probability_count = transition_cache->probabilities.size();
        proof.choice_count = transition_cache->choices.size();
        proof.choice_successor_count =
            transition_cache->choice_successors.size();
        proof.choice_option_count =
            transition_cache->choice_options.size();
        proof.graph_prefix_identity = fallback_graph_prefix_identity(
            proof.row_count, proof.priced_row_count);
        proof.transition_prefix_identity =
            fallback_transition_prefix_identity(
                proof.successor_count, proof.probability_count,
                proof.choice_count, proof.choice_successor_count,
                proof.choice_option_count);
        successful_fallback_properness_proof = std::move(proof);
    }

void SolveWork::Impl::stamp_fallback_provenance(FocusedFallbackPolicy& fallback) const {
        fallback.goal_identity = goal_identity();
        fallback.economy_identity = economy_identity();
        fallback.action_vocabulary_identity =
            action_vocabulary_identity();
        fallback.action_vocabulary_size =
            static_cast<std::uint32_t>(operators.size());
        fallback.synthesis_graph_identity = graph_identity();
    }

const char* SolveWork::Impl::retained_fallback_invalid_reason(
        const FocusedFallbackPolicy& fallback) {
        FallbackValidationTelemetry& telemetry =
            result.diagnostics.fallback_validation;
        ++telemetry.calls;
        const auto total_started = std::chrono::steady_clock::now();
        const auto elapsed_ns = [](const auto started) {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
        };
        const auto finish = [&](const char* reason) {
            telemetry.total_ns += elapsed_ns(total_started);
            return reason;
        };

        if (reuse_successful_fallback_properness_proof(fallback)) {
            return finish(nullptr);
        }

        auto component_started = std::chrono::steady_clock::now();
        const std::uint64_t current_goal_identity = goal_identity();
        ++telemetry.goal_identity.checks;
        telemetry.goal_identity.duration_ns += elapsed_ns(component_started);
        if (fallback.goal_identity != current_goal_identity) {
            return finish("goal_identity_changed");
        }
        component_started = std::chrono::steady_clock::now();
        const std::uint64_t current_economy_identity = economy_identity();
        ++telemetry.economy_identity.checks;
        telemetry.economy_identity.duration_ns +=
            elapsed_ns(component_started);
        if (fallback.economy_identity != current_economy_identity) {
            return finish("economy_identity_changed");
        }
        component_started = std::chrono::steady_clock::now();
        const bool vocabulary_changed =
            operators.size() < fallback.action_vocabulary_size ||
            fallback.action_vocabulary_identity !=
                action_vocabulary_prefix_identity(
                    fallback.action_vocabulary_size);
        ++telemetry.action_vocabulary_identity.checks;
        telemetry.action_vocabulary_identity.duration_ns +=
            elapsed_ns(component_started);
        if (vocabulary_changed) {
            return finish("action_vocabulary_prefix_changed");
        }
        component_started = std::chrono::steady_clock::now();
        const char* structural_reason = [&]() -> const char* {
            if (fallback.anchor_state == kNoId ||
                fallback.anchor_state >= calc.state_count() ||
                !std::isfinite(fallback.anchor_state_value) ||
                fallback.anchor_state_value < 0.0) {
                return "invalid_anchor";
            }
            const auto operator_valid = [&](const std::uint32_t op) {
                return op != kNoId && op < calc.operators().size();
            };
            const auto row_valid = [&](const std::uint64_t row,
                                       const std::uint32_t owner,
                                       const std::uint32_t op) {
                return row < transition_cache->rows.size() &&
                    row < priced_rows.size() &&
                    transition_cache->rows[row].owner_state == owner &&
                    priced_rows[row].operator_index == op;
            };
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            if (fallback.anchor_row != no_row &&
                (!operator_valid(fallback.anchor_operator) ||
                 !row_valid(
                     fallback.anchor_row, fallback.anchor_state,
                     fallback.anchor_operator))) {
                return "anchor_row_changed";
            }
            if (fallback.renewal_row != no_row &&
                (!operator_valid(fallback.renewal_operator) ||
                 fallback.renewal_row >= transition_cache->rows.size() ||
                 !row_valid(
                     fallback.renewal_row,
                     transition_cache->rows[
                         fallback.renewal_row].owner_state,
                     fallback.renewal_operator) ||
                 !renewal_fallback_eligible(
                     transition_cache->rows[
                         fallback.renewal_row].owner_state,
                     fallback))) {
                return "renewal_row_or_signature_changed";
            }
            if (fallback.renewal_operator != kNoId &&
                !operator_valid(fallback.renewal_operator)) {
                return "renewal_operator_changed";
            }
            if (fallback.finish_action != kNoId &&
                fallback.finish_action >= calc.registry().actions.size()) {
                return "finish_action_changed";
            }
            for (const auto& [state, value] :
                 fallback.progress_state_value) {
                const auto op =
                    fallback.progress_state_operator.find(state);
                if (state >= calc.state_count() ||
                    !std::isfinite(value) || value < 0.0 ||
                    op == fallback.progress_state_operator.end() ||
                    !operator_valid(op->second)) {
                    return "progress_value_changed";
                }
            }
            for (const auto& [state, op] :
                 fallback.progress_state_operator) {
                if (state >= calc.state_count() || !operator_valid(op) ||
                    fallback.progress_state_value.find(state) ==
                        fallback.progress_state_value.end()) {
                    return "progress_operator_changed";
                }
            }
            for (const auto& mode : fallback.primitive_renewal_modes) {
                if (!std::isfinite(mode.value) || mode.value < 0.0 ||
                    !operator_valid(mode.operator_index) ||
                    mode.kernel_signature.empty()) {
                    return "primitive_mode_changed";
                }
            }
            return nullptr;
        }();
        ++telemetry.structural.checks;
        telemetry.structural.duration_ns += elapsed_ns(component_started);
        if (structural_reason != nullptr) {
            return finish(structural_reason);
        }

        component_started = std::chrono::steady_clock::now();
        std::uint32_t anchor_operator = kNoId;
        const double anchor_upper = fallback_terminal_upper(
            fallback.anchor_state, fallback, &anchor_operator);
        const bool anchor_proper =
            std::isfinite(anchor_upper) &&
            anchor_operator != kNoId &&
            anchor_operator < calc.operators().size();
        ++telemetry.anchor_properness.checks;
        telemetry.anchor_properness.duration_ns +=
            elapsed_ns(component_started);
        if (!anchor_proper) {
            return finish("anchor_not_proper");
        }
        component_started = std::chrono::steady_clock::now();
        const bool start_proper =
            std::isfinite(focused_start_upper_bound(fallback));
        ++telemetry.start_properness.checks;
        telemetry.start_properness.duration_ns +=
            elapsed_ns(component_started);
        if (!start_proper) {
            return finish("start_not_proper");
        }
        return finish(nullptr);
    }

auto SolveWork::Impl::acquire_focused_fallback(bool& complete)
        -> FocusedFallbackWitness {
        complete = true;
        std::array<FocusedFallbackWitness, 2> retained{
            focused_fallback_policy,
            output_incumbent.has_value()
                ? output_incumbent->fallback
                : FocusedFallbackWitness{}};
        bool invalid_retained = false;
        for (std::size_t i = 0; i < retained.size(); ++i) {
            if (!retained[i] ||
                (i != 0 && retained[i] == retained[0])) {
                continue;
            }
            const char* reason =
                retained_fallback_invalid_reason(*retained[i]);
            if (reason == nullptr) {
                remember_successful_fallback_properness_proof(retained[i]);
                ++result.diagnostics.constructive_policy_reuses;
                return retained[i];
            }
            invalid_retained = true;
            result.diagnostics.constructive_policy_last_refresh_reason =
                reason;
        }
        if (invalid_retained) {
            ++result.diagnostics.constructive_policy_refreshes;
            if (output_incumbent.has_value() &&
                output_incumbent->fallback &&
                retained_fallback_invalid_reason(
                    *output_incumbent->fallback) != nullptr) {
                output_incumbent.reset();
            }
        }
        if (!constructive_fallback_pending) {
            ++result.diagnostics.constructive_policy_syntheses;
        }
        std::optional<FocusedFallbackPolicy> synthesized =
            focused_fallback(complete);
        if (!complete) return {};
        if (synthesized.has_value()) {
            stamp_fallback_provenance(*synthesized);
            return std::make_shared<const FocusedFallbackPolicy>(
                std::move(*synthesized));
        }
        return {};
    }

void SolveWork::Impl::capture_incumbent_state(
        BoundedPolicyIncumbent& candidate,
        const std::uint32_t state,
        const std::uint64_t row) {
        if (state >= candidate.values.size() ||
            state >= candidate.policy.size() ||
            state >= candidate.policy_row_costs.size()) {
            throw std::logic_error(
                "bounded incumbent capture state is out of range");
        }
        std::uint32_t fallback_operator = kNoId;
        if (row == std::numeric_limits<std::uint64_t>::max()) {
            if (state < candidate.frontier_operators.size()) {
                fallback_operator = candidate.frontier_operators[state];
            }
            if (fallback_operator == kNoId && candidate.fallback) {
                fallback_operator = candidate.restart_operator;
            }
        }
        CapturedBoundedPolicyRow captured =
            capture_bounded_policy_row(
                calc, *transition_cache, priced_rows, state, row,
                fallback_operator);
        candidate.policy[state] = captured.policy;
        candidate.policy_row_costs[state] = captured.cost;

        const auto source = std::lower_bound(
            candidate.choice_sources.begin(),
            candidate.choice_sources.end(), state,
            [](const BoundedPolicyIncumbent::ChoiceSource& left,
               const std::uint32_t right) {
                return left.state < right;
            });
        if (captured.choice_options.empty()) {
            if (source != candidate.choice_sources.end() &&
                source->state == state) {
                candidate.choice_sources.erase(source);
            }
        } else if (source != candidate.choice_sources.end() &&
                   source->state == state) {
            source->choices = std::move(captured.choice_options);
        } else {
            candidate.choice_sources.insert(
                source,
                BoundedPolicyIncumbent::ChoiceSource{
                    state, std::move(captured.choice_options)});
        }
    }

void SolveWork::Impl::capture_incumbent_policy(
        BoundedPolicyIncumbent& candidate) {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        const std::size_t state_count = candidate.values.size();
        candidate.policy.assign(state_count, PolicyOperatorRef{});
        candidate.policy_row_costs.assign(state_count, kInfinity);
        candidate.choice_sources.clear();
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) continue;
            if (!candidate.policy_reachable.empty() &&
                (state >= candidate.policy_reachable.size() ||
                 !candidate.policy_reachable[state])) {
                continue;
            }
            const std::uint64_t row =
                state < candidate.policy_rows.size()
                    ? candidate.policy_rows[state]
                    : no_row;
            capture_incumbent_state(candidate, state, row);
        }
    }

void SolveWork::Impl::populate_incumbent_policy(
        BoundedPolicyIncumbent& candidate) {
        if (candidate.policy_materialized) return;
        const std::size_t state_count = candidate.values.size();
        if (candidate.policy.size() != state_count) {
            throw std::logic_error(
                "bounded incumbent captured policy size changed");
        }
        const auto vector_capacity_upper = [](
            const std::size_t elements) {
            if (elements == 0) return std::size_t{0};
            if (elements >
                (std::numeric_limits<std::size_t>::max() - 16) / 2) {
                return std::numeric_limits<std::size_t>::max();
            }
            return 2 * elements + 16;
        };
        candidate.unveil_preferences.clear();
        candidate.unveil_preferences.reserve(state_count);
        candidate.unveil_preferences.resize(state_count);
        candidate.option_unveil_preferences.clear();
        candidate.option_unveil_preferences.reserve(state_count);
        candidate.option_unveil_preferences.resize(state_count);
        if (candidate.unveil_preferences.capacity() >
                vector_capacity_upper(state_count) ||
            candidate.option_unveil_preferences.capacity() >
                vector_capacity_upper(state_count)) {
            throw std::logic_error(
                "bounded incumbent preference outer-vector projection was "
                "exceeded");
        }
        const auto order_choice_range = [&]<typename Iterator>(
            const ActionDescriptor& action,
            const Iterator first,
            const Iterator last) {
            if (!action_observes_modifier_offer(action)) {
                throw std::logic_error(
                    "observed-choice finalization requires an admitted "
                    "outcome-observation contract");
            }
            std::sort(
                first, last,
                [&](const OutcomeChoiceOption& left,
                    const OutcomeChoiceOption& right) {
                    if (left.state >= candidate.values.size() ||
                        right.state >= candidate.values.size()) {
                        throw std::logic_error(
                            "observed-choice finalization references a "
                            "state outside the value table");
                    }
                    const double left_value =
                        candidate.values[left.state];
                    const double right_value =
                        candidate.values[right.state];
                    if (left.state == right.state) {
                        return left.mod_id < right.mod_id;
                    }
                    return sparse_policy_choice_precedes(
                        left_value, left.state,
                        right_value, right.state);
                });
        };
        for (BoundedPolicyIncumbent::ChoiceSource& source :
             candidate.choice_sources) {
            const std::uint32_t state = source.state;
            if (state >= state_count ||
                candidate.policy[state].index >= calc.operators().size()) {
                throw std::logic_error(
                    "bounded incumbent captured choice source is invalid");
            }
            const PlannerOperator& planner =
                calc.operators().at(candidate.policy[state].index);
            if (planner.kind == PlannerOperatorKind::Primitive &&
                planner.primitive_action < calc.registry().actions.size() &&
                action_observes_modifier_offer(
                    calc.registry().actions[planner.primitive_action])) {
                order_observed_modifier_choices(
                    calc.registry().actions[
                        planner.primitive_action],
                    source.choices, candidate.values);
                std::vector<std::uint32_t>& preferences =
                    candidate.unveil_preferences[state];
                preferences.reserve(source.choices.size());
                if (preferences.capacity() >
                    vector_capacity_upper(source.choices.size())) {
                    throw std::logic_error(
                        "bounded incumbent primitive preference projection "
                        "was exceeded");
                }
                for (const OutcomeChoiceOption& choice : source.choices) {
                    preferences.push_back(choice.mod_id);
                }
            } else if (planner.kind == PlannerOperatorKind::FixedOption &&
                       !source.choices.empty()) {
                if (planner.primitive_program.empty() ||
                    planner.primitive_program.back() >=
                        calc.registry().actions.size()) {
                    throw std::logic_error(
                        "bounded observed-choice option has no final "
                        "primitive action");
                }
                std::sort(
                    source.choices.begin(), source.choices.end(),
                    [](const OutcomeChoiceOption& left,
                       const OutcomeChoiceOption& right) {
                        return std::tie(
                                   left.observation_state, left.mod_id,
                                   left.state, left.actual_state) <
                            std::tie(
                                   right.observation_state, right.mod_id,
                                   right.state, right.actual_state);
                    });
                std::size_t group_count = 0;
                for (std::size_t begin = 0;
                     begin < source.choices.size();) {
                    ++group_count;
                    const std::uint32_t observation =
                        source.choices[begin].observation_state;
                    do {
                        ++begin;
                    } while (begin < source.choices.size() &&
                             source.choices[begin].observation_state ==
                                 observation);
                }
                std::vector<ObservedUnveilPreference>& preferences =
                    candidate.option_unveil_preferences[state];
                preferences.reserve(group_count);
                if (preferences.capacity() >
                    vector_capacity_upper(group_count)) {
                    throw std::logic_error(
                        "bounded incumbent option preference projection was "
                        "exceeded");
                }
                const ActionDescriptor& final_action =
                    calc.registry().actions[
                        planner.primitive_program.back()];
                for (std::size_t begin = 0;
                     begin < source.choices.size();) {
                    std::size_t end = begin + 1;
                    const std::uint32_t observation_state =
                        source.choices[begin].observation_state;
                    while (end < source.choices.size() &&
                           source.choices[end].observation_state ==
                               observation_state) {
                        ++end;
                    }
                    order_choice_range(
                        final_action,
                        source.choices.begin() +
                            static_cast<std::ptrdiff_t>(begin),
                        source.choices.begin() +
                            static_cast<std::ptrdiff_t>(end));
                    ObservedUnveilPreference preference;
                    preference.observation_state = observation_state;
                    preference.choices.reserve(end - begin);
                    if (preference.choices.capacity() >
                        vector_capacity_upper(end - begin)) {
                        throw std::logic_error(
                            "bounded incumbent observed-choice projection "
                            "was exceeded");
                    }
                    for (std::size_t position = begin;
                         position < end; ++position) {
                        const OutcomeChoiceOption& choice =
                            source.choices[position];
                        preference.choices.push_back(
                            {choice.mod_id, choice.state, choice.actual_state});
                    }
                    preferences.push_back(std::move(preference));
                    begin = end;
                }
            }
        }
        candidate.choice_sources = {};
        candidate.policy_materialized = true;
    }

std::uint64_t SolveWork::Impl::incumbent_owned_bytes(
        const BoundedPolicyIncumbent& incumbent) const {
        std::uint64_t bytes = sizeof(BoundedPolicyIncumbent) +
            incumbent.kind.capacity() + 1 +
            incumbent.compilation_provenance.capacity() + 1 +
            incumbent.final_graph_verification_failure.capacity() + 1;
        bytes += incumbent.values.capacity() * sizeof(double);
        bytes += incumbent.policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += incumbent.policy_row_costs.capacity() * sizeof(double);
        bytes += incumbent.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += incumbent.choice_sources.capacity() *
                 sizeof(BoundedPolicyIncumbent::ChoiceSource);
        for (const auto& source : incumbent.choice_sources) {
            bytes += source.choices.capacity() *
                     sizeof(OutcomeChoiceOption);
        }
        bytes += incumbent.frontier_operators.capacity() *
                 sizeof(std::uint32_t);
        bytes += incumbent.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += incumbent.policy_reachable.capacity() *
                 sizeof(std::uint8_t);
        bytes += incumbent.primitive_renewal_witness
                     .kernel_signature.capacity() *
                 sizeof(std::uint64_t);
        bytes += incumbent.compiled_artifact.strategy_json.capacity() + 1;
        bytes += incumbent.compiled_artifact
                     .certification_strategy_json.capacity() + 1;
        bytes += incumbent.compiled_artifact
                     .policy_route_default_mode.capacity() + 1;
        bytes += incumbent.compiled_artifact
                     .certification_policy_route_default_mode.capacity() + 1;
        bytes += incumbent.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& preferences : incumbent.unveil_preferences) {
            bytes += preferences.capacity() * sizeof(std::uint32_t);
        }
        bytes += incumbent.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        for (const auto& preferences :
             incumbent.option_unveil_preferences) {
            bytes += preferences.capacity() *
                     sizeof(ObservedUnveilPreference);
            for (const auto& preference : preferences) {
                bytes += preference.choices.capacity() *
                         sizeof(ObservedUnveilChoice);
            }
        }
        if (incumbent.fallback &&
            incumbent.fallback != focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback = *incumbent.fallback;
            bytes += sizeof(FocusedFallbackPolicy);
            bytes += fallback.renewal_kernel_signature.capacity() *
                     sizeof(std::uint64_t);
            bytes += fallback.primitive_renewal_modes.capacity() *
                     sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
            for (const auto& mode : fallback.primitive_renewal_modes) {
                bytes += mode.kernel_signature.capacity() *
                         sizeof(std::uint64_t);
            }
            bytes += fallback.progress_state_value.size() *
                     (sizeof(std::pair<const std::uint32_t, double>) +
                      2 * sizeof(void*));
            bytes += fallback.progress_state_operator.size() *
                     (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                      2 * sizeof(void*));
        }
        return bytes;
    }

std::uint64_t SolveWork::Impl::carrier_ladder_boundary_owned_bytes(
        const CarrierLadderBoundaryCapture& capture) const {
        std::uint64_t bytes = sizeof(CarrierLadderBoundaryCapture);
        const std::uint64_t incumbent = incumbent_owned_bytes(capture.prefix);
        if (incumbent >= sizeof(BoundedPolicyIncumbent)) {
            bytes += incumbent - sizeof(BoundedPolicyIncumbent);
        }
        bytes += capture.stops.capacity() *
            sizeof(CarrierLadderBoundaryCapture::Stop);
        bytes += capture.target_state_key.capacity() *
            sizeof(std::uint64_t);
        for (const auto& stop : capture.stops) {
            bytes += stop.coarse_state_key.capacity() *
                sizeof(std::uint64_t);
        }
        bytes += capture.recovered_members.capacity() *
            sizeof(CarrierLadderBoundaryCapture::RecoveredMember);
        for (const auto& member : capture.recovered_members) {
            bytes += member.stable_key.capacity() * sizeof(std::uint64_t);
        }
        bytes += capture.reached_stops.capacity() *
            sizeof(CarrierLadderBoundaryCapture::ReachedStop);
        for (const auto& stop : capture.reached_stops) {
            bytes += stop.predecessor_stable_key.capacity() *
                sizeof(std::uint64_t);
            bytes += stop.predecessor_coarse_state_key.capacity() *
                sizeof(std::uint64_t);
            bytes += stop.selected_action_semantic_key.capacity() *
                sizeof(std::uint64_t);
            bytes += stop.stopped_stable_key.capacity() *
                sizeof(std::uint64_t);
            bytes += stop.stopped_coarse_state_key.capacity() *
                sizeof(std::uint64_t);
        }
        bytes += capture.status.capacity() + 1;
        bytes += capture.refusal.capacity() + 1;
        bytes += capture.recovery_status.capacity() + 1;
        bytes += capture.recovery_refusal.capacity() + 1;
        return bytes;
    }

SolveWork::Impl::CarrierLadderBoundaryCapture::RowServiceDisposition
SolveWork::Impl::classify_carrier_ladder_row_service_witness(
        const CarrierLadderBoundaryCapture::RowServiceWitness& witness) {
    using Disposition =
        CarrierLadderBoundaryCapture::RowServiceDisposition;
    if (!witness.observed) return Disposition::NotObserved;
    if (!witness.state_in_calc) return Disposition::Undiscovered;
    if (witness.goal) return Disposition::Goal;
    if (witness.selectable_row_count != 0) {
        return Disposition::SelectableRowAvailable;
    }
    if (witness.certified_frontier_operator != kNoId) {
        return Disposition::CertifiedFrontier;
    }
    /* The host-requested stop owns publication when both it and a later
     * observational resource flag are present. This matches the ordinary
     * termination precedence and does not claim that the missing row itself
     * is a cap. */
    if (witness.requested_bounded_finish) {
        return Disposition::RequestedBoundedFinish;
    }
    if (witness.resource_cap_hit) return Disposition::ResourceCap;
    if (!witness.row_span_present || witness.owned_row_count == 0) {
        return Disposition::NoRowSpan;
    }
    if (witness.completed_row_count != 0) {
        return Disposition::CompletedRowsInvalidOrUnpriced;
    }
    if (witness.action_envelope_entry_count == 0 &&
        !witness.queued && !witness.queue_contains_state &&
        !witness.expansion_active_for_state &&
        !witness.incremental_refinement_targets_state) {
        return Disposition::RowsNotScheduled;
    }
    if (witness.action_envelope_entry_count != 0 || witness.queued ||
        witness.queue_contains_state || witness.expansion_active_for_state ||
        witness.incremental_refinement_targets_state || witness.carrier) {
        return Disposition::RowsScheduledIncomplete;
    }
    return Disposition::OtherMissing;
}

SolveWork::Impl::CarrierLadderBoundaryCapture::RowServiceWitness
SolveWork::Impl::capture_carrier_ladder_row_service_witness(
        const std::uint32_t state,
        const std::vector<std::uint8_t>& completed_rows,
        const std::vector<std::uint8_t>& ordinary_result_expanded,
        const std::uint32_t certified_frontier_operator) const {
    using Witness = CarrierLadderBoundaryCapture::RowServiceWitness;
    Witness witness;
    witness.observed = true;
    witness.state = state;
    witness.state_in_calc = state < calc.state_count();
    witness.goal = witness.state_in_calc &&
        calc.is_goal_state(calc.state(state));
    witness.goal_mask = witness.state_in_calc
        ? satisfied_goal_mask_for_state(state) : 0;
    witness.goal_progress = std::popcount(witness.goal_mask);
    witness.certified_frontier_operator = certified_frontier_operator;
    witness.planner_operator_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(calc.operators().size(), kNoId));
    witness.priced_operator_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(operators.size(), kNoId));
    witness.static_candidate_operator_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(static_operator_indices.size(), kNoId));
    witness.delayed_candidate_operator_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(delayed_operator_indices.size(), kNoId));
    witness.dynamic_candidate_operator_count =
        static_cast<std::uint32_t>(std::min<std::size_t>(
            incremental_dynamic_operator_indices.size(), kNoId));
    witness.broad_expanded =
        state < expanded.size() && expanded[state] != 0;
    witness.ordinary_result_expanded =
        state < ordinary_result_expanded.size() &&
        ordinary_result_expanded[state] != 0;
    witness.transition_cache_expanded = transition_cache != nullptr &&
        state < transition_cache->expanded.size() &&
        transition_cache->expanded[state] != 0;
    witness.focused_strict_expanded =
        state < focused_strict_expanded.size() &&
        focused_strict_expanded[state] != 0;
    witness.queued = state < queued.size() && queued[state] != 0;
    witness.expansion_active_for_state = expansion_active &&
        expansion_state == state;
    witness.expansion_incremental_alternative =
        witness.expansion_active_for_state &&
        expansion_is_incremental_alternative;
    witness.incremental_refinement_active = incremental_refinement_active;
    witness.action_envelope_scheduler_view_enabled =
        action_envelope_ledger.scheduler_view_enabled;
    witness.requested_bounded_finish = requested_bounded_finish;
    witness.resource_cap_hit = result.diagnostics.resource_cap_hit;
    witness.observation_state_count = calc.state_count();
    witness.observation_graph_identity = graph_identity();
    if (transition_cache != nullptr) {
        witness.observation_row_count = transition_cache->rows.size();
        witness.observation_priced_row_count = priced_rows.size();
        witness.observation_successor_count =
            transition_cache->successors.size();
        witness.observation_probability_count =
            transition_cache->probabilities.size();
        witness.observation_choice_count =
            transition_cache->choices.size();
        witness.observation_choice_successor_count =
            transition_cache->choice_successors.size();
        witness.observation_choice_option_count =
            transition_cache->choice_options.size();
        witness.observation_graph_prefix_identity =
            incumbent_graph_prefix_identity(
                witness.observation_row_count,
                witness.observation_priced_row_count,
                witness.observation_successor_count,
                witness.observation_probability_count,
                witness.observation_choice_count,
                witness.observation_choice_successor_count,
                witness.observation_choice_option_count);
    }

    const auto position_of = [&](const auto& values) {
        const auto found = std::find(values.begin(), values.end(), state);
        return found == values.end()
            ? Witness::kNoPosition
            : static_cast<std::uint64_t>(
                  std::distance(values.begin(), found));
    };
    witness.queue_position = position_of(queue);
    witness.queue_contains_state =
        witness.queue_position != Witness::kNoPosition;
    witness.incremental_refinement_targets_state =
        incremental_refinement_active &&
        (witness.queued || witness.queue_contains_state ||
         witness.expansion_active_for_state);
    witness.carrier_position = position_of(incremental_carriers);
    witness.carrier = witness.carrier_position != Witness::kNoPosition;
    witness.automatic_order_position =
        position_of(incremental_automatic_carrier_order);
    witness.fairness_order_position =
        position_of(incremental_fairness_carrier_order);
    witness.high_progress_order_position =
        position_of(incremental_high_progress_carrier_order);
    witness.missing_frontier_position =
        position_of(incremental_anytime_missing_frontier_states);
    witness.missing_frontier =
        witness.missing_frontier_position != Witness::kNoPosition;
    for (std::size_t index = 0;
         index < incremental_priority_tasks.size(); ++index) {
        if (incremental_priority_tasks[index].state != state) continue;
        if (witness.first_priority_task_position == Witness::kNoPosition) {
            witness.first_priority_task_position = index;
        }
        ++witness.priority_task_count;
        if (index >= incremental_priority_task_cursor) {
            ++witness.pending_priority_task_count;
        }
    }
    witness.carrier_cursor = incremental_carrier_cursor;
    witness.automatic_carrier_cursor =
        incremental_automatic_carrier_cursor;
    witness.automatic_order_cursor = incremental_automatic_order_cursor;
    witness.fairness_carrier_cursor =
        incremental_fairness_carrier_cursor;
    witness.fairness_operator_cursor =
        incremental_fairness_operator_cursor;
    witness.high_progress_carrier_cursor =
        incremental_high_progress_carrier_cursor;
    witness.high_progress_operator_cursor =
        incremental_high_progress_operator_cursor;
    witness.closure_carrier_cursor = incremental_closure_carrier_cursor;
    witness.closure_operator_cursor = incremental_closure_operator_cursor;
    witness.priority_task_cursor = incremental_priority_task_cursor;

    std::vector<std::uint8_t> seen_selectable_operator(
        calc.operators().size(), 0);
    std::uint64_t selectable_operator_identity = 1469598103934665603ULL;
    identity_mix_string(
        selectable_operator_identity,
        "carrier_ladder_selectable_operator_set_v1");
    std::uint64_t row_identity = 1469598103934665603ULL;
    identity_mix_string(row_identity, "carrier_ladder_row_service_rows_v1");
    if (transition_cache != nullptr &&
        state < transition_cache->state_rows.size()) {
        witness.row_span_present = true;
        witness.declared_row_count =
            transition_cache->state_rows[state].count;
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, state)) {
            ++witness.owned_row_count;
            const SparseRow& row = transition_cache->rows.at(row_index);
            const bool owner_matches = row.owner_state == state;
            if (!owner_matches) ++witness.row_owner_mismatch_count;
            if (row.admitted) ++witness.admitted_row_count;
            const bool completed = row_index < completed_rows.size() &&
                completed_rows[row_index] != 0;
            if (completed) ++witness.completed_row_count;
            const bool priced = row_index < priced_rows.size();
            if (priced) ++witness.priced_row_count;
            const std::uint32_t operator_index = priced
                ? priced_rows[row_index].operator_index : kNoId;
            const bool valid_operator = priced && operator_index != kNoId &&
                operator_index < calc.operators().size();
            if (valid_operator) ++witness.valid_operator_row_count;
            const bool finite_nonnegative = valid_operator &&
                std::isfinite(priced_rows[row_index].cost) &&
                priced_rows[row_index].cost >= 0.0;
            if (finite_nonnegative) {
                ++witness.finite_nonnegative_priced_row_count;
            }
            const bool selectable = owner_matches && completed &&
                finite_nonnegative;
            if (selectable) {
                ++witness.selectable_row_count;
                if (!seen_selectable_operator[operator_index]) {
                    seen_selectable_operator[operator_index] = 1;
                    ++witness.selectable_operator_count;
                    identity_mix(
                        selectable_operator_identity, operator_index);
                }
            }
            witness.variant_count += row.variant_count;
            witness.transition_count += row.transition_count;
            witness.choice_count += row.choice_count;
            identity_mix(row_identity, row_index);
            identity_mix(row_identity, row.owner_state);
            identity_mix(row_identity, row.admitted);
            identity_mix(row_identity, completed);
            identity_mix(row_identity, priced);
            identity_mix(row_identity, operator_index);
            identity_mix(row_identity, finite_nonnegative);
            identity_mix(row_identity, selectable);
            if (priced) {
                identity_mix(
                    row_identity,
                    std::bit_cast<std::uint64_t>(priced_rows[row_index].cost));
            }
            identity_mix(row_identity, row.variant_count);
            identity_mix(row_identity, row.transition_count);
            identity_mix(row_identity, row.choice_count);
        }
    }
    witness.selectable_operator_identity = selectable_operator_identity;
    witness.row_identity = row_identity;
    for (const IncrementalAlternativeRow& alternative :
         incremental_alternative_rows) {
        if (alternative.state != state) continue;
        ++witness.alternative_row_count;
        if (alternative.row_index < completed_rows.size() &&
            completed_rows[alternative.row_index]) {
            ++witness.alternative_completed_row_count;
        }
    }

    std::uint64_t envelope_identity = 1469598103934665603ULL;
    identity_mix_string(
        envelope_identity,
        "carrier_ladder_row_service_action_envelope_v1");
    witness.action_envelope_transition_count =
        action_envelope_ledger.transition_count();
    const auto observe_envelope_entry =
        [&](const solve_detail::ActionEnvelopeEntry* entry) {
            if (entry == nullptr || entry->state != state) return;
            ++witness.action_envelope_entry_count;
            ++witness.action_envelope_lifecycle_counts.at(
                static_cast<std::size_t>(entry->lifecycle));
            ++witness.action_envelope_lane_counts.at(
                static_cast<std::size_t>(entry->lane));
            ++witness.action_envelope_authority_counts.at(
                static_cast<std::size_t>(entry->authority));
            ++witness.action_envelope_stop_owner_counts.at(
                static_cast<std::size_t>(entry->stop_owner));
            if (entry->row_index !=
                std::numeric_limits<std::uint64_t>::max()) {
                ++witness.action_envelope_row_entry_count;
            }
            if (action_envelope_ledger.scheduling_complete(
                    state, entry->operator_index)) {
                ++witness.action_envelope_scheduling_complete_count;
            }
            identity_mix(envelope_identity, entry->operator_index);
            identity_mix(
                envelope_identity,
                static_cast<std::uint64_t>(entry->lifecycle));
            identity_mix(
                envelope_identity,
                static_cast<std::uint64_t>(entry->lane));
            identity_mix(
                envelope_identity,
                static_cast<std::uint64_t>(entry->authority));
            identity_mix(
                envelope_identity,
                static_cast<std::uint64_t>(entry->stop_owner));
            identity_mix(envelope_identity, entry->evidence);
            identity_mix(envelope_identity, entry->row_index);
            identity_mix(envelope_identity, entry->revision);
            identity_mix_string(envelope_identity, entry->detail);
        };
    observe_envelope_entry(action_envelope_ledger.find(state, kNoId));
    for (std::uint32_t operator_index = 0;
         operator_index < calc.operators().size(); ++operator_index) {
        observe_envelope_entry(
            action_envelope_ledger.find(state, operator_index));
        if (incremental_completed_pairs.contains(
                solve_detail::ActionEnvelopeLedger::key(
                    state, operator_index))) {
            ++witness.completed_pair_count;
        }
    }
    witness.action_envelope_identity = envelope_identity;

    std::uint64_t scheduler_identity = 1469598103934665603ULL;
    identity_mix_string(
        scheduler_identity, "carrier_ladder_row_service_scheduler_v1");
    for (const std::uint64_t value : {
             witness.queue_position,
             witness.carrier_position,
             witness.automatic_order_position,
             witness.fairness_order_position,
             witness.high_progress_order_position,
             witness.missing_frontier_position,
             witness.first_priority_task_position,
             witness.carrier_cursor,
             witness.automatic_carrier_cursor,
             witness.automatic_order_cursor,
             witness.fairness_carrier_cursor,
             witness.fairness_operator_cursor,
             witness.high_progress_carrier_cursor,
             witness.high_progress_operator_cursor,
             witness.closure_carrier_cursor,
             witness.closure_operator_cursor,
             witness.priority_task_cursor}) {
        identity_mix(scheduler_identity, value);
    }
    identity_mix(scheduler_identity, witness.priority_task_count);
    identity_mix(scheduler_identity, witness.pending_priority_task_count);
    identity_mix(scheduler_identity, witness.expansion_active_for_state);
    identity_mix(
        scheduler_identity, witness.expansion_incremental_alternative);
    identity_mix(scheduler_identity, witness.incremental_refinement_active);
    identity_mix(
        scheduler_identity, witness.incremental_refinement_targets_state);
    witness.scheduler_identity = scheduler_identity;

    witness.disposition =
        classify_carrier_ladder_row_service_witness(witness);
    std::uint64_t facts_identity = 1469598103934665603ULL;
    identity_mix_string(
        facts_identity, "carrier_ladder_row_service_witness_facts_v1");
    const auto mix_fact_values = [&]<typename... Values>(Values... values) {
        (identity_mix(
             facts_identity, static_cast<std::uint64_t>(values)), ...);
    };
    mix_fact_values(
        witness.state,
        witness.goal_mask,
        witness.goal_progress,
        witness.certified_frontier_operator,
        witness.planner_operator_count,
        witness.priced_operator_count,
        witness.static_candidate_operator_count,
        witness.delayed_candidate_operator_count,
        witness.dynamic_candidate_operator_count,
        witness.declared_row_count,
        witness.owned_row_count,
        witness.row_owner_mismatch_count,
        witness.admitted_row_count,
        witness.completed_row_count,
        witness.alternative_row_count,
        witness.alternative_completed_row_count,
        witness.priced_row_count,
        witness.valid_operator_row_count,
        witness.finite_nonnegative_priced_row_count,
        witness.selectable_row_count,
        witness.selectable_operator_count,
        witness.priority_task_count,
        witness.pending_priority_task_count,
        witness.completed_pair_count,
        witness.variant_count,
        witness.transition_count,
        witness.choice_count,
        witness.action_envelope_transition_count,
        witness.action_envelope_entry_count,
        witness.action_envelope_row_entry_count,
        witness.action_envelope_scheduling_complete_count,
        witness.observation_state_count,
        witness.observation_row_count,
        witness.observation_priced_row_count,
        witness.observation_successor_count,
        witness.observation_probability_count,
        witness.observation_choice_count,
        witness.observation_choice_successor_count,
        witness.observation_choice_option_count,
        witness.selectable_operator_identity,
        witness.row_identity,
        witness.action_envelope_identity,
        witness.scheduler_identity,
        witness.observation_graph_identity,
        witness.observation_graph_prefix_identity,
        witness.disposition);
    const auto mix_counts = [&](const auto& counts) {
        for (const std::uint64_t count : counts) {
            identity_mix(facts_identity, count);
        }
    };
    mix_counts(witness.action_envelope_lifecycle_counts);
    mix_counts(witness.action_envelope_lane_counts);
    mix_counts(witness.action_envelope_authority_counts);
    mix_counts(witness.action_envelope_stop_owner_counts);
    for (const bool value : {
             witness.observed,
             witness.state_in_calc,
             witness.goal,
             witness.broad_expanded,
             witness.ordinary_result_expanded,
             witness.transition_cache_expanded,
             witness.focused_strict_expanded,
             witness.queued,
             witness.queue_contains_state,
             witness.row_span_present,
             witness.carrier,
             witness.missing_frontier,
             witness.expansion_active_for_state,
             witness.expansion_incremental_alternative,
             witness.incremental_refinement_active,
             witness.incremental_refinement_targets_state,
             witness.action_envelope_scheduler_view_enabled,
             witness.requested_bounded_finish,
             witness.resource_cap_hit}) {
        identity_mix(facts_identity, value);
    }
    witness.facts_identity = facts_identity;
    return witness;
}

void SolveWork::Impl::bind_carrier_ladder_row_service_witness_identity(
        CarrierLadderBoundaryCapture::RowServiceWitness& witness,
        const CarrierLadderBoundaryCapture& capture) const {
    std::uint64_t identity = 1469598103934665603ULL;
    identity_mix_string(
        identity, "carrier_ladder_row_service_witness_v1");
    identity_mix(identity, capture.goal_identity);
    identity_mix(identity, capture.economy_identity);
    identity_mix(identity, capture.caller_scope_identity);
    identity_mix(identity, capture.action_vocabulary_identity);
    identity_mix(identity, capture.graph_identity);
    identity_mix(identity, capture.graph_prefix_identity);
    identity_mix(identity, capture.artifact_identity);
    identity_mix(identity, capture.executable_identity);
    identity_mix(identity, capture.source_identity);
    identity_mix(identity, witness.facts_identity);
    witness.identity = identity;
}

void SolveWork::Impl::refresh_carrier_ladder_terminal_row_service_witness() {
    if (!carrier_ladder_exact_boundary_capture.has_value()) return;
    CarrierLadderBoundaryCapture& capture =
        *carrier_ladder_exact_boundary_capture;
    if (!capture.row_service_witness.observed ||
        transition_cache == nullptr) {
        return;
    }
    std::vector<std::uint8_t> completed(
        transition_cache->rows.size(), 0);
    for (std::uint64_t row = 0;
         row < transition_cache->rows.size(); ++row) {
        completed[row] = transition_cache->rows[row].admitted ? 1 : 0;
    }
    for (const IncrementalAlternativeRow& alternative :
         incremental_alternative_rows) {
        if (alternative.row_index < completed.size() &&
            alternative.row_index < priced_rows.size()) {
            completed[alternative.row_index] = 1;
        }
    }
    const std::uint32_t state = capture.row_service_witness.state;
    std::uint32_t frontier = kNoId;
    for (const CarrierLadderBoundaryCapture::Stop& stop : capture.stops) {
        if (stop.state == state && stop.kind ==
                CarrierLadderBoundaryCapture::StopKind::CertifiedFrontier) {
            frontier = stop.operator_index;
            break;
        }
    }
    capture.terminal_row_service_witness =
        capture_carrier_ladder_row_service_witness(
            state, completed, result.expanded, frontier);
    bind_carrier_ladder_row_service_witness_identity(
        capture.terminal_row_service_witness, capture);
}

void SolveWork::Impl::capture_carrier_ladder_exact_boundary(
        const std::uint32_t target_state,
        const std::vector<double>& candidate_values,
        const std::vector<std::uint64_t>& candidate_policy_rows,
        const std::vector<std::uint8_t>& candidate_reachable,
        const std::vector<std::uint32_t>& certified_frontier_operators,
        std::vector<CarrierLadderBoundaryCapture::Stop> stops,
        CarrierLadderBoundaryCapture::RowServiceWitness row_service_witness,
        const char* refusal) {
    if (options.carrier_ladder_exact_boundary_mode ==
            CarrierLadderExactBoundaryMode::Off ||
        carrier_ladder_exact_boundary_capture.has_value()) {
        return;
    }
    struct PrivateWallCharge {
        std::uint64_t& total_ns;
        std::chrono::steady_clock::time_point started_at =
            std::chrono::steady_clock::now();
        ~PrivateWallCharge() {
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_at);
            const std::uint64_t amount = static_cast<std::uint64_t>(
                std::max<std::int64_t>(0, elapsed.count()));
            total_ns = amount >
                    std::numeric_limits<std::uint64_t>::max() - total_ns
                ? std::numeric_limits<std::uint64_t>::max()
                : total_ns + amount;
        }
    } private_wall_charge{
        carrier_ladder_exact_boundary_private_wall_ns};
    CarrierLadderBoundaryCapture capture;
    capture.target_state = target_state;
    capture.row_service_witness = std::move(row_service_witness);
    if (target_state < calc.state_count()) {
        capture.target_state_key =
            exact_abstract_state_key(calc.state(target_state), 0);
    }
    capture.goal_identity = goal_identity();
    capture.economy_identity = economy_identity();
    capture.caller_scope_identity = caller_scope_identity();
    capture.action_vocabulary_identity = action_vocabulary_identity();
    capture.graph_identity = graph_identity();
    capture.artifact_identity = artifact_identity();
    capture.source_identity = 1469598103934665603ULL;
    identity_mix_string(
        capture.source_identity,
        "carrier_ladder_exact_boundary_source_v5");
    capture.executable_identity = 1469598103934665603ULL;
    identity_mix_string(
        capture.executable_identity,
        "poecraft_native_carrier_boundary_contract_v1");
    identity_mix(capture.executable_identity, PC_ABI_VERSION);

    if (refusal != nullptr) {
        capture.status = "refused_resource_cap";
        capture.refusal = refusal;
        capture.retained_owned_bytes =
            carrier_ladder_boundary_owned_bytes(capture);
        carrier_ladder_exact_boundary_capture.emplace(std::move(capture));
        return;
    }

    const auto& limits =
        options.carrier_ladder_exact_boundary_limits;
    if (candidate_values.size() > limits.max_prefix_states ||
        candidate_policy_rows.size() > limits.max_prefix_states ||
        candidate_reachable.size() > limits.max_prefix_states) {
        capture.status = "refused_resource_cap";
        capture.refusal = "max_prefix_states";
        capture.retained_owned_bytes =
            carrier_ladder_boundary_owned_bytes(capture);
        carrier_ladder_exact_boundary_capture.emplace(std::move(capture));
        return;
    }

    capture.prefix.values = candidate_values;
    capture.prefix.policy_rows = candidate_policy_rows;
    capture.prefix.policy_reachable = candidate_reachable;
    capture.prefix.frontier_operators = certified_frontier_operators;
    capture.prefix.behavioral_representative_by_state =
        result.behavioral_representative_by_state;
    capture.prefix.goal_identity = capture.goal_identity;
    capture.prefix.economy_identity = capture.economy_identity;
    capture.prefix.action_vocabulary_identity =
        capture.action_vocabulary_identity;
    capture.prefix.action_vocabulary_size = operators.size();
    capture.prefix.caller_scope_identity = capture.caller_scope_identity;
    capture.prefix.graph_identity = capture.graph_identity;
    capture.prefix.artifact_identity = capture.artifact_identity;
    capture.prefix.source_generation = transition_cache->rows.size();
    capture.prefix.target_generation = calc.state_count();
    capture.prefix.graph_row_count = transition_cache->rows.size();
    capture.prefix.graph_priced_row_count = priced_rows.size();
    capture.prefix.graph_successor_count =
        transition_cache->successors.size();
    capture.prefix.graph_probability_count =
        transition_cache->probabilities.size();
    capture.prefix.graph_choice_count =
        transition_cache->choices.size();
    capture.prefix.graph_choice_successor_count =
        transition_cache->choice_successors.size();
    capture.prefix.graph_choice_option_count =
        transition_cache->choice_options.size();
    capture.prefix.graph_prefix_identity = incumbent_graph_prefix_identity(
        capture.prefix.graph_row_count,
        capture.prefix.graph_priced_row_count,
        capture.prefix.graph_successor_count,
        capture.prefix.graph_probability_count,
        capture.prefix.graph_choice_count,
        capture.prefix.graph_choice_successor_count,
        capture.prefix.graph_choice_option_count);
    capture.graph_prefix_identity = capture.prefix.graph_prefix_identity;
    bind_carrier_ladder_row_service_witness_identity(
        capture.row_service_witness, capture);
    capture.prefix.kind = "carrier_ladder_failed_prefix_v1";
    capture_incumbent_policy(capture.prefix);

    std::sort(
        stops.begin(), stops.end(),
        [](const CarrierLadderBoundaryCapture::Stop& left,
           const CarrierLadderBoundaryCapture::Stop& right) {
            return std::tie(left.state, left.kind, left.operator_index) <
                   std::tie(right.state, right.kind, right.operator_index);
        });
    stops.erase(
        std::unique(
            stops.begin(), stops.end(),
            [](const CarrierLadderBoundaryCapture::Stop& left,
               const CarrierLadderBoundaryCapture::Stop& right) {
                return left.state == right.state &&
                       left.kind == right.kind &&
                       left.operator_index == right.operator_index;
            }),
        stops.end());
    capture.stops = std::move(stops);
    const bool has_certified_continuation =
        output_incumbent.has_value() &&
        output_incumbent->independently_certified &&
        output_incumbent->independently_evaluated &&
        output_incumbent->proper && output_incumbent->executable;
    for (CarrierLadderBoundaryCapture::Stop& stop : capture.stops) {
        if (stop.state >= calc.state_count()) {
            capture.status = "refused_invalid_prefix";
            capture.refusal = "exit_state_outside_graph";
            continue;
        }
        stop.coarse_state_key =
            exact_abstract_state_key(calc.state(stop.state), 0);
        if (stop.operator_index != kNoId &&
            stop.operator_index < calc.operators().size()) {
            const PlannerOperator& planner =
                calc.operators().at(stop.operator_index);
            std::uint64_t semantic = 1469598103934665603ULL;
            identity_mix(semantic, 1); /* exit operator schema */
            identity_mix(semantic, stop.operator_index);
            identity_mix(
                semantic, static_cast<std::uint64_t>(planner.kind));
            identity_mix_string(semantic, planner.id);
            stop.operator_semantic_identity = semantic;
        }
        if (stop.kind ==
                CarrierLadderBoundaryCapture::StopKind::CertifiedFrontier) {
            if (!has_certified_continuation ||
                stop.operator_semantic_identity == 0 ||
                stop.state >= output_incumbent->values.size() ||
                !std::isfinite(output_incumbent->values[stop.state])) {
                stop.kind = CarrierLadderBoundaryCapture::StopKind::
                    UnresolvedMissing;
                stop.operator_index = kNoId;
                stop.operator_semantic_identity = 0;
            } else {
                stop.incumbent_identity =
                    output_incumbent->portfolio_identity;
                stop.incumbent_graph_prefix_identity =
                    output_incumbent->graph_prefix_identity;
                stop.incumbent_artifact_identity =
                    output_incumbent->artifact_identity;
                stop.frontier_value_bits = std::bit_cast<std::uint64_t>(
                    output_incumbent->values[stop.state]);
                stop.independently_certified =
                    output_incumbent->independently_certified;
                stop.independently_evaluated =
                    output_incumbent->independently_evaluated;
                stop.proper = output_incumbent->proper;
                stop.executable = output_incumbent->executable;
            }
        }
        std::uint64_t continuation = 1469598103934665603ULL;
        identity_mix(continuation, 1); /* exit continuation schema */
        identity_mix(continuation, stop.state);
        identity_mix(
            continuation, static_cast<std::uint64_t>(stop.kind));
        identity_mix(continuation, stop.operator_index);
        for (const std::uint64_t word : stop.coarse_state_key) {
            identity_mix(continuation, word);
        }
        identity_mix(continuation, stop.operator_semantic_identity);
        identity_mix(continuation, stop.incumbent_identity);
        identity_mix(
            continuation, stop.incumbent_graph_prefix_identity);
        identity_mix(continuation, stop.incumbent_artifact_identity);
        identity_mix(continuation, stop.frontier_value_bits);
        identity_mix(continuation, stop.independently_certified);
        identity_mix(continuation, stop.independently_evaluated);
        identity_mix(continuation, stop.proper);
        identity_mix(continuation, stop.executable);
        stop.continuation_identity = continuation;
    }

    std::uint64_t prefix_identity = 1469598103934665603ULL;
    identity_mix(prefix_identity, 1); /* prefix schema */
    identity_mix(prefix_identity, result.start_state);
    identity_mix(prefix_identity, target_state);
    for (std::uint32_t state = 0;
         state < capture.prefix.policy_reachable.size(); ++state) {
        if (!capture.prefix.policy_reachable[state]) continue;
        ++capture.selected_states;
        identity_mix(prefix_identity, state);
        const std::uint64_t row =
            state < capture.prefix.policy_rows.size()
                ? capture.prefix.policy_rows[state]
                : std::numeric_limits<std::uint64_t>::max();
        identity_mix(prefix_identity, row);
        if (state < capture.prefix.policy.size()) {
            identity_mix(
                prefix_identity,
                static_cast<std::uint64_t>(
                    capture.prefix.policy[state].kind));
            identity_mix(prefix_identity, capture.prefix.policy[state].index);
        }
    }
    for (const auto& source : capture.prefix.choice_sources) {
        identity_mix(prefix_identity, source.state);
        identity_mix(prefix_identity, source.choices.size());
        for (const OutcomeChoiceOption& choice : source.choices) {
            identity_mix(prefix_identity, choice.observation_state);
            identity_mix(prefix_identity, choice.mod_id);
            identity_mix(prefix_identity, choice.state);
            identity_mix(prefix_identity, choice.actual_state);
        }
    }
    capture.prefix_identity = prefix_identity;

    std::uint64_t exit_identity = 1469598103934665603ULL;
    identity_mix(exit_identity, 1); /* exit-contract schema */
    for (const CarrierLadderBoundaryCapture::Stop& stop : capture.stops) {
        identity_mix(exit_identity, stop.state);
        identity_mix(exit_identity, static_cast<std::uint64_t>(stop.kind));
        identity_mix(exit_identity, stop.operator_index);
        identity_mix(exit_identity, stop.continuation_identity);
        switch (stop.kind) {
        case CarrierLadderBoundaryCapture::StopKind::RequestedEntry:
            break;
        case CarrierLadderBoundaryCapture::StopKind::GoalSuccess:
            ++capture.goal_stops;
            break;
        case CarrierLadderBoundaryCapture::StopKind::CertifiedFrontier:
            ++capture.certified_frontier_stops;
            break;
        case CarrierLadderBoundaryCapture::StopKind::UnresolvedMissing:
            ++capture.unresolved_stops;
            break;
        }
    }
    capture.exit_contract_identity = exit_identity;
    std::uint64_t selection = 1469598103934665603ULL;
    identity_mix(selection, 1); /* deterministic selection schema */
    identity_mix(selection, capture.goal_identity);
    identity_mix(selection, capture.economy_identity);
    identity_mix(selection, capture.caller_scope_identity);
    identity_mix(selection, capture.action_vocabulary_identity);
    identity_mix(selection, capture.graph_prefix_identity);
    identity_mix(selection, capture.artifact_identity);
    identity_mix(selection, capture.executable_identity);
    identity_mix(selection, capture.source_identity);
    identity_mix(selection, target_state);
    for (const std::uint64_t word : capture.target_state_key) {
        identity_mix(selection, word);
    }
    identity_mix(selection, capture.prefix_identity);
    identity_mix(selection, capture.exit_contract_identity);
    capture.selection_identity = selection;
    capture.status = capture.certified_frontier_stops > 0 ||
            capture.goal_stops > 0
        ? "captured"
        : "captured_no_independently_executable_exit";
    populate_incumbent_policy(capture.prefix);
    capture.retained_owned_bytes =
        carrier_ladder_boundary_owned_bytes(capture);
    if (capture.retained_owned_bytes > limits.max_owned_bytes) {
        CarrierLadderBoundaryCapture refused;
        refused.target_state = target_state;
        refused.target_state_key = capture.target_state_key;
        refused.goal_identity = capture.goal_identity;
        refused.economy_identity = capture.economy_identity;
        refused.caller_scope_identity = capture.caller_scope_identity;
        refused.action_vocabulary_identity =
            capture.action_vocabulary_identity;
        refused.graph_identity = capture.graph_identity;
        refused.graph_prefix_identity = capture.graph_prefix_identity;
        refused.artifact_identity = capture.artifact_identity;
        refused.executable_identity = capture.executable_identity;
        refused.source_identity = capture.source_identity;
        refused.row_service_witness = capture.row_service_witness;
        refused.terminal_row_service_witness =
            capture.terminal_row_service_witness;
        refused.status = "refused_resource_cap";
        refused.refusal = "max_owned_bytes";
        refused.retained_owned_bytes =
            carrier_ladder_boundary_owned_bytes(refused);
        capture = std::move(refused);
    }
    carrier_ladder_exact_boundary_capture.emplace(std::move(capture));
}

void SolveWork::Impl::refresh_carrier_ladder_exact_boundary_diagnostics(
        SolveDiagnostics& diagnostics) const {
    if (options.carrier_ladder_exact_boundary_mode ==
            CarrierLadderExactBoundaryMode::Off) {
        diagnostics.carrier_ladder_exact_boundary_json.clear();
        return;
    }
    const auto mode_name = [&] {
        switch (options.carrier_ladder_exact_boundary_mode) {
        case CarrierLadderExactBoundaryMode::Off: return "off";
        case CarrierLadderExactBoundaryMode::Record: return "record";
        case CarrierLadderExactBoundaryMode::Recover: return "recover";
        case CarrierLadderExactBoundaryMode::ResumableContinuation:
            return "resumable_continuation";
        }
        return "unknown";
    };
    const auto hex = [](const std::uint64_t value) {
        char text[17];
        std::snprintf(
            text, sizeof(text), "%016llx",
            static_cast<unsigned long long>(value));
        return std::string{text};
    };
    const auto& limits =
        options.carrier_ladder_exact_boundary_limits;
    std::string json =
        "{\"schema_version\":\"carrier_ladder_exact_boundary_v1\",";
    json += "\"state_key_version\":1,\"mode\":\"";
    json += mode_name();
    json += "\",\"caps\":{\"max_prefix_states\":" +
        std::to_string(limits.max_prefix_states);
    json += ",\"ordinary_finish_state_action_rows\":" +
        std::to_string(limits.ordinary_finish_state_action_rows);
    json += ",\"max_exact_states\":" +
        std::to_string(limits.max_exact_states);
    json += ",\"max_exact_rows\":" +
        std::to_string(limits.max_exact_rows);
    json += ",\"max_exact_transitions\":" +
        std::to_string(limits.max_exact_transitions);
    json += ",\"max_exact_work\":" +
        std::to_string(limits.max_exact_work);
    json += ",\"max_owned_bytes\":" +
        std::to_string(limits.max_owned_bytes);
    json += ",\"max_wall_time_ms\":" +
        std::to_string(limits.max_wall_time_ms);
    json += ",\"max_samples\":" +
        std::to_string(limits.max_samples) + "}";
    json += ",\"resumable_joint_policy_continuation\":";
    if (!resumable_joint_policy_candidate.has_value()) {
        json += "null";
    } else {
        const ResumableJointPolicyCandidateState& retained =
            *resumable_joint_policy_candidate;
        const auto& candidate = retained.continuation;
        const auto lifecycle_name = [&] {
            using Lifecycle =
                solve_detail::JointPolicyContinuationLifecycle;
            switch (candidate.lifecycle) {
            case Lifecycle::Inactive: return "inactive";
            case Lifecycle::Captured: return "captured";
            case Lifecycle::WaitingForExactContinuation:
                return "waiting_for_exact_continuation";
            case Lifecycle::Resumable: return "resumable";
            case Lifecycle::Advancing: return "advancing";
            case Lifecycle::CompleteCandidate: return "complete_candidate";
            case Lifecycle::Refused: return "refused";
            }
            return "unknown";
        };
        const auto semantic_key_identity = [&](const auto& key) {
            std::uint64_t identity = 1469598103934665603ULL;
            identity_mix_string(
                identity, "joint_policy_semantic_key_v1");
            identity_mix(identity, key.size());
            for (const std::uint64_t word : key) {
                identity_mix(identity, word);
            }
            return identity;
        };
        json += "{\"schema_version\":\"resumable_joint_policy_"
            "continuation_lineage_v1\"";
        json += ",\"candidate_identity\":\"" +
            hex(candidate.semantic_identity) + "\"";
        json += ",\"lifecycle\":\"" +
            std::string(lifecycle_name()) + "\"";
        json += ",\"refusal\":\"" + std::string(
            solve_detail::joint_policy_continuation_refusal_name(
                candidate.refusal)) + "\"";
        json += ",\"counts\":{\"captures\":" +
            std::to_string(candidate.capture_count);
        json += ",\"resumes\":" +
            std::to_string(candidate.resume_count);
        json += ",\"yields\":" +
            std::to_string(candidate.yield_count);
        json += ",\"rebases\":" +
            std::to_string(candidate.rebase_count);
        json += ",\"stale_discards\":" +
            std::to_string(candidate.stale_discard_count);
        json += ",\"noncompetitive_discards\":" +
            std::to_string(candidate.noncompetitive_discard_count);
        json += ",\"completions\":" +
            std::to_string(candidate.completion_count);
        json += ",\"handoffs\":" +
            std::to_string(retained.handoff_count) + "}";
        json += ",\"selected_prefix\":{\"rows_appended\":" +
            std::to_string(candidate.rows_appended);
        json += ",\"fixed_decisions\":" +
            std::to_string(candidate.fixed_decision_count());
        json += ",\"cursor\":" + std::to_string(candidate.cursor);
        json += ",\"walk_states\":" +
            std::to_string(candidate.walk_state_count());
        json += ",\"capture_reconstructions\":" +
            std::to_string(candidate.capture_count);
        json += ",\"global_rebuilds_between_resumes\":0}";
        json += ",\"missing_state_identities\":[";
        const std::size_t sample_count = std::min<std::size_t>(
            candidate.missing_state_history.size(), limits.max_samples);
        for (std::size_t index = 0; index < sample_count; ++index) {
            if (index != 0) json.push_back(',');
            const auto& key = candidate.missing_state_history[index];
            json += "{\"identity\":\"" +
                hex(semantic_key_identity(key)) + "\",\"words\":" +
                std::to_string(key.size()) + "}";
        }
        json += "]";
        json += ",\"generations\":{\"source_at_capture\":" +
            std::to_string(candidate.context.source_generation);
        json += ",\"target_at_capture\":" +
            std::to_string(candidate.context.target_generation);
        json += ",\"source_current\":" +
            std::to_string(
                transition_cache == nullptr
                    ? 0
                    : transition_cache->rows.size());
        json += ",\"target_current\":" +
            std::to_string(calc.state_count()) + "}";
        const auto current_context =
            current_joint_policy_continuation_context(&retained);
        json += ",\"identity_comparison\":{";
        const auto append_identity = [&](const char* name,
                                         const std::uint64_t captured,
                                         const std::uint64_t current) {
            json += "\"" + std::string(name) + "\":{\"captured\":\"" +
                hex(captured) + "\",\"current\":\"" + hex(current) +
                "\",\"equal\":" +
                std::string(captured == current ? "true" : "false") + "},";
        };
        append_identity(
            "goal", candidate.context.goal_identity,
            current_context.goal_identity);
        append_identity(
            "economy", candidate.context.economy_identity,
            current_context.economy_identity);
        append_identity(
            "caller_scope", candidate.context.caller_scope_identity,
            current_context.caller_scope_identity);
        append_identity(
            "action_vocabulary_prefix",
            candidate.context.action_vocabulary_identity,
            current_context.action_vocabulary_identity);
        append_identity(
            "artifact", candidate.context.mechanics_artifact_identity,
            current_context.mechanics_artifact_identity);
        append_identity(
            "terminal", candidate.context.exact_terminal_identity,
            current_context.exact_terminal_identity);
        append_identity(
            "boundary", candidate.context.boundary_identity,
            current_context.boundary_identity);
        json += "\"action_vocabulary_size\":{\"captured\":" +
            std::to_string(candidate.context.action_vocabulary_size) +
            ",\"current\":" +
            std::to_string(current_context.action_vocabulary_size) +
            ",\"monotone\":" + std::string(
                current_context.action_vocabulary_size >=
                        candidate.context.action_vocabulary_size
                    ? "true" : "false") + "}}";
        json += ",\"ordinary_interleave\":{\"events_between_resumes\":" +
            std::to_string(retained.ordinary_interleave_events);
        json += ",\"carrier_epoch_delta\":" + std::to_string(
            incremental_carrier_ladder_epochs -
            retained.carrier_epochs_at_capture);
        json += ",\"refinement_round_delta\":" + std::to_string(
            incremental_refinement_rounds -
            retained.refinement_rounds_at_capture);
        json += ",\"sweep_delta\":" + std::to_string(
            sweeps - retained.sweeps_at_capture) + "}";
        json += ",\"work\":{\"retained\":" +
            std::to_string(candidate.retained_work);
        json += ",\"retained_bytes\":" +
            std::to_string(candidate.retained_owned_bytes());
        json += ",\"retained_peak_bytes\":" +
            std::to_string(candidate.retained_peak_bytes);
        json += ",\"transient_peak_bytes\":" +
            std::to_string(candidate.transient_peak_bytes) + "}";
        json += ",\"root_estimate\":{\"value\":" +
            finite_json(candidate.candidate_root_estimate);
        json += ",\"provenance\":\"" +
            hex(candidate.root_estimate_provenance) + "\"}";
        const bool has_new_exact = retained.handed_off_for_evaluation &&
            output_incumbent.has_value() &&
            output_incumbent->portfolio_identity !=
                candidate.context.incumbent_identity &&
            output_incumbent->independently_evaluated;
        json += ",\"final_exact_cost\":" + finite_json(
            has_new_exact
                ? output_incumbent->evaluated_policy_cost
                : kInfinity);
        json += "}";
    }
    std::uint64_t terminal_attempts = 0;
    bool terminal_row_visibility_proved = true;
    for (const JointAnytimeAttemptLineage& lineage :
         joint_anytime_attempt_lineage) {
        if (lineage.trigger == JointAnytimeAttemptLineage::Trigger::
                                   TerminalPublication) {
            ++terminal_attempts;
            terminal_row_visibility_proved &=
                lineage.terminal_sees_all_completed_alternative_rows;
        }
    }
    json += ",\"joint_anytime_attempt_lineage\":{";
    json += "\"schema_version\":\"joint_anytime_attempt_lineage_v2\"";
    json += ",\"attempts\":" +
        std::to_string(joint_anytime_attempt_lineage.size());
    json += ",\"terminal_attempts\":" +
        std::to_string(terminal_attempts);
    json += ",\"complete_terminal_row_visibility_proved\":" +
        std::string(
            terminal_attempts != 0 && terminal_row_visibility_proved
                ? "true" : "false");
    json += ",\"records\":[";
    for (std::size_t attempt_index = 0;
         attempt_index < joint_anytime_attempt_lineage.size();
         ++attempt_index) {
        if (attempt_index != 0) json.push_back(',');
        const JointAnytimeAttemptLineage& lineage =
            joint_anytime_attempt_lineage[attempt_index];
        const char* trigger = "incremental_checkpoint";
        switch (lineage.trigger) {
        case JointAnytimeAttemptLineage::Trigger::IncrementalCheckpoint:
            break;
        case JointAnytimeAttemptLineage::Trigger::PublicationPreflight:
            trigger = "publication_preflight";
            break;
        case JointAnytimeAttemptLineage::Trigger::TerminalPublication:
            trigger = "terminal_publication";
            break;
        }
        json += "{\"ordinal\":" + std::to_string(lineage.ordinal) +
            ",\"trigger\":\"" + trigger + "\"";
        json += ",\"source_generation\":" +
            std::to_string(lineage.source_generation);
        json += ",\"target_generation\":" +
            std::to_string(lineage.target_generation);
        json += ",\"completed_rows\":{";
        json += "\"ordinary_admitted\":" +
            std::to_string(lineage.ordinary_admitted_rows);
        json += ",\"available\":" +
            std::to_string(lineage.completed_row_count);
        json += ",\"identity\":\"" +
            hex(lineage.completed_row_identity) + "\"";
        json += ",\"delta\":" +
            std::to_string(lineage.completed_row_delta);
        json += ",\"monotone\":" +
            std::string(lineage.completed_rows_monotone ? "true" : "false");
        json += ",\"completed_alternatives\":" +
            std::to_string(lineage.completed_alternative_rows);
        json += ",\"alternative_identity\":\"" +
            hex(lineage.alternative_row_identity) + "\"";
        json += ",\"alternative_delta\":" +
            std::to_string(lineage.alternative_row_delta);
        json += ",\"terminal_sees_all_completed_alternatives\":" +
            std::string(
                lineage.terminal_sees_all_completed_alternative_rows
                    ? "true" : "false") + "}";
        json += ",\"incumbent_before\":{";
        json += "\"present\":" + std::string(
            lineage.incumbent_identity_before != 0 ? "true" : "false");
        json += ",\"identity\":";
        if (lineage.incumbent_identity_before == 0) json += "null";
        else json += "\"" + hex(lineage.incumbent_identity_before) + "\"";
        json += ",\"estimate\":" +
            finite_json(lineage.incumbent_estimate_before);
        json += ",\"independently_evaluated\":" + std::string(
            lineage.incumbent_independently_evaluated_before
                ? "true" : "false");
        json += ",\"exact_cost\":" +
            finite_json(lineage.incumbent_exact_cost_before) + "}";
        json += ",\"candidate_root_estimate\":" +
            finite_json(lineage.candidate_root_estimate);
        json += ",\"selection\":{";
        json += "\"identity\":\"" +
            hex(lineage.selection_identity) + "\"";
        json += ",\"reachable_non_goal_states\":" +
            std::to_string(lineage.selected_state_count);
        json += ",\"samples_omitted\":" +
            std::to_string(lineage.selected_samples_omitted);
        json += ",\"decisions\":[";
        for (std::size_t decision_index = 0;
             decision_index < lineage.selected_decisions.size();
             ++decision_index) {
            if (decision_index != 0) json.push_back(',');
            const JointAnytimeAttemptLineage::SelectedDecision& decision =
                lineage.selected_decisions[decision_index];
            json += "{\"state\":" + std::to_string(decision.state);
            json += ",\"row\":";
            if (decision.row ==
                std::numeric_limits<std::uint64_t>::max()) {
                json += "null";
            } else {
                json += std::to_string(decision.row);
            }
            json += ",\"operator_index\":";
            if (decision.operator_index == kNoId) json += "null";
            else json += std::to_string(decision.operator_index);
            json += ",\"action\":";
            if (decision.operator_index == kNoId ||
                decision.operator_index >= calc.operators().size()) {
                json += "null";
            } else {
                append_json_string(
                    json, calc.operators()[decision.operator_index].id);
            }
            json += "}";
        }
        json += "]}";
        json += ",\"frontier_uses\":" +
            std::to_string(lineage.certified_frontier_uses);
        json += ",\"renewal_boundaries\":{";
        json += "\"successes\":" +
            std::to_string(lineage.renewal_boundary_successes);
        json += ",\"attempts\":" +
            std::to_string(lineage.renewal_boundary_attempts) + "}";
        json += ",\"fixed_policy_proper\":" +
            std::string(lineage.fixed_policy_proper ? "true" : "false");
        json +=
            ",\"online_handoff\":{\"requested_bounded_finish_at_attempt\":" +
            std::string(
                lineage.requested_bounded_finish_at_attempt
                    ? "true" : "false");
        json += ",\"refinement_selected\":" +
            std::string(
                lineage.missing_refinement_selected ? "true" : "false");
        json += ",\"refinement_selected_at_expanded_states\":";
        if (lineage.missing_refinement_selected_at_expanded_states == kNoId) {
            json += "null";
        } else {
            json += std::to_string(
                lineage.missing_refinement_selected_at_expanded_states);
        }
        if (lineage.first_missing_state == kNoId) {
            json += ",\"terminal_state\":null}";
        } else {
            const std::uint32_t missing = lineage.first_missing_state;
            const bool terminal_expanded =
                missing < expanded.size() && expanded[missing] != 0;
            const bool terminal_queued =
                missing < queued.size() && queued[missing] != 0;
            const bool terminal_carrier = std::find(
                incremental_carriers.begin(), incremental_carriers.end(),
                missing) != incremental_carriers.end();
            const bool terminal_open = std::find(
                incremental_anytime_missing_frontier_states.begin(),
                incremental_anytime_missing_frontier_states.end(),
                missing) !=
                incremental_anytime_missing_frontier_states.end();
            std::uint64_t terminal_owned_rows = 0;
            std::uint64_t terminal_valid_priced_rows = 0;
            if (transition_cache != nullptr &&
                missing < transition_cache->state_rows.size()) {
                const StateRowSpan span =
                    transition_cache->state_rows[missing];
                terminal_owned_rows = span.count;
                const std::uint64_t end = std::min<std::uint64_t>(
                    transition_cache->rows.size(), span.offset + span.count);
                for (std::uint64_t row = span.offset; row < end; ++row) {
                    if (row < priced_rows.size() &&
                        priced_rows[row].operator_index != kNoId &&
                        transition_cache->rows[row].owner_state == missing) {
                        ++terminal_valid_priced_rows;
                    }
                }
            }
            json += ",\"terminal_state\":{";
            json += "\"expanded\":" +
                std::string(terminal_expanded ? "true" : "false");
            json += ",\"queued\":" +
                std::string(terminal_queued ? "true" : "false");
            json += ",\"carrier\":" +
                std::string(terminal_carrier ? "true" : "false");
            json += ",\"still_open\":" +
                std::string(terminal_open ? "true" : "false");
            json += ",\"owned_rows\":" +
                std::to_string(terminal_owned_rows);
            json += ",\"valid_priced_rows\":" +
                std::to_string(terminal_valid_priced_rows) + "}}";
        }
        json += ",\"first_missing_continuation\":";
        if (lineage.first_missing_state == kNoId) {
            json += "null";
        } else {
            json += "{\"state\":" +
                std::to_string(lineage.first_missing_state) +
                ",\"goal_mask\":" +
                std::to_string(lineage.first_missing_goal_mask) + "}";
        }
        json += ",\"failure\":";
        if (lineage.failure.empty()) json += "null";
        else append_json_string(json, lineage.failure);
        json += ",\"candidate\":{";
        json += "\"installed_for_finalization\":" + std::string(
            lineage.installed_for_finalization ? "true" : "false");
        json += ",\"portfolio_identity\":";
        if (lineage.candidate_portfolio_identity == 0) json += "null";
        else json += "\"" + hex(lineage.candidate_portfolio_identity) + "\"";
        json += ",\"kind\":";
        if (lineage.candidate_kind.empty()) json += "null";
        else append_json_string(json, lineage.candidate_kind);
        json += "}";
        json += ",\"compilation\":{";
        json += "\"attempted\":" + std::string(
            lineage.compilation_attempted ? "true" : "false");
        json += ",\"succeeded\":" + std::string(
            lineage.compilation_succeeded ? "true" : "false");
        json += ",\"nodes\":" + std::to_string(lineage.compiled_nodes);
        json += ",\"edges\":" + std::to_string(lineage.compiled_edges);
        json += ",\"result\":";
        if (lineage.compilation_result.empty()) json += "null";
        else append_json_string(json, lineage.compilation_result);
        json += "}";
        json += ",\"independent_exact_evaluation\":{";
        json += "\"attempted\":" + std::string(
            lineage.independent_evaluation_attempted ? "true" : "false");
        json += ",\"succeeded\":" + std::string(
            lineage.independent_evaluation_succeeded ? "true" : "false");
        json += ",\"cost\":" +
            finite_json(lineage.independently_evaluated_cost);
        json += ",\"result\":";
        if (lineage.independent_evaluation_result.empty()) json += "null";
        else append_json_string(
            json, lineage.independent_evaluation_result);
        json += "}";
        json += ",\"portfolio\":{";
        json += "\"decision\":";
        if (lineage.portfolio_decision.empty()) json += "null";
        else append_json_string(json, lineage.portfolio_decision);
        json += ",\"reason\":";
        if (lineage.portfolio_reason.empty()) json += "null";
        else append_json_string(json, lineage.portfolio_reason);
        json += "}}";
    }
    json += "]}";
    if (!carrier_ladder_exact_boundary_capture.has_value()) {
        json += ",\"status\":\"not_observed\",\"capture\":null}";
        diagnostics.carrier_ladder_exact_boundary_json = std::move(json);
        return;
    }
    const CarrierLadderBoundaryCapture& capture =
        *carrier_ladder_exact_boundary_capture;
    json += ",\"status\":";
    append_json_string(json, capture.status);
    json += ",\"refusal\":";
    if (capture.refusal.empty()) json += "null";
    else append_json_string(json, capture.refusal);
    json += ",\"capture\":{\"boundary_kind\":\"missing_frontier\"";
    json += ",\"target_state\":" + std::to_string(capture.target_state);
    json += ",\"target_state_key\":[";
    for (std::size_t word = 0;
         word < capture.target_state_key.size(); ++word) {
        if (word != 0) json.push_back(',');
        json += "\"" + hex(capture.target_state_key[word]) + "\"";
    }
    json += "]";
    json += ",\"selection_identity\":\"" +
        hex(capture.selection_identity) + "\"";
    json += ",\"prefix_identity\":\"" +
        hex(capture.prefix_identity) + "\"";
    json += ",\"exit_contract_identity\":\"" +
        hex(capture.exit_contract_identity) + "\"";
    json += ",\"identities\":{\"goal\":\"" +
        hex(capture.goal_identity) + "\",\"economy\":\"" +
        hex(capture.economy_identity) + "\",\"caller_scope\":\"" +
        hex(capture.caller_scope_identity) +
        "\",\"action_vocabulary\":\"" +
        hex(capture.action_vocabulary_identity) +
        "\",\"graph\":\"" + hex(capture.graph_identity) +
        "\",\"graph_prefix\":\"" +
        hex(capture.graph_prefix_identity) +
        "\",\"artifact\":\"" + hex(capture.artifact_identity) +
        "\",\"executable\":\"" +
        hex(capture.executable_identity) + "\",\"source\":\"" +
        hex(capture.source_identity) + "\"}";
    const auto append_row_service_witness = [&]
        (const CarrierLadderBoundaryCapture::RowServiceWitness& service) {
    const char* service_disposition = "not_observed";
    switch (service.disposition) {
    case CarrierLadderBoundaryCapture::RowServiceDisposition::NotObserved:
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::Undiscovered:
        service_disposition = "undiscovered";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::Goal:
        service_disposition = "goal";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::
            SelectableRowAvailable:
        service_disposition = "selectable_row_available";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::
            CertifiedFrontier:
        service_disposition = "certified_frontier";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::
            RequestedBoundedFinish:
        service_disposition = "requested_bounded_finish";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::ResourceCap:
        service_disposition = "resource_cap";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::NoRowSpan:
        service_disposition = "no_row_span";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::
            RowsNotScheduled:
        service_disposition = "rows_not_scheduled";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::
            RowsScheduledIncomplete:
        service_disposition = "rows_scheduled_incomplete";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::
            CompletedRowsInvalidOrUnpriced:
        service_disposition = "completed_rows_invalid_or_unpriced";
        break;
    case CarrierLadderBoundaryCapture::RowServiceDisposition::OtherMissing:
        service_disposition = "other_missing";
        break;
    }
    const auto bool_json = [](const bool value) {
        return value ? "true" : "false";
    };
    const auto append_position = [&](const char* name,
                                     const std::uint64_t position) {
        json += ",\"" + std::string(name) + "\":";
        if (position == CarrierLadderBoundaryCapture::RowServiceWitness::
                            kNoPosition) {
            json += "null";
        } else {
            json += std::to_string(position);
        }
    };
    const auto append_named_counts = [&](const auto& counts,
                                         const auto& names) {
        json.push_back('{');
        for (std::size_t index = 0; index < counts.size(); ++index) {
            if (index != 0) json.push_back(',');
            append_json_string(json, names[index]);
            json += ":" + std::to_string(counts[index]);
        }
        json.push_back('}');
    };
    static constexpr std::array<const char*, 7> lifecycle_names = {
        "queued", "exact_row_complete", "exact_inapplicability_proved",
        "incumbent_dominated", "rolled_back_after_cap",
        "omitted_caller_scope", "unresolved_named_stop"};
    static constexpr std::array<const char*, 8> lane_names = {
        "unassigned", "restricted_anchor", "incremental_carrier_local",
        "incremental_automatic", "incremental_priority",
        "incremental_operator_major", "incremental_closure",
        "explicit_caller_scope"};
    static constexpr std::array<const char*, 7> authority_names = {
        "none", "exact_row_materialization", "exact_registry_legality",
        "independent_global_lower_vs_verified_upper",
        "explicit_caller_scope", "transactional_resource_cap",
        "named_open_obligation"};
    static constexpr std::array<const char*, 5> stop_owner_names = {
        "none", "resource_cap", "successor_frontier",
        "missing_verified_upper", "requested_bounded_finish"};
    json += "{\"schema_version\":"
        "\"carrier_ladder_row_service_witness_v1\",\"observed\":" +
        std::string(bool_json(service.observed)) +
        ",\"state\":" + std::to_string(service.state) +
        ",\"disposition\":\"" + service_disposition +
        "\",\"identity\":\"" + hex(service.identity) +
        "\",\"facts_identity\":\"" + hex(service.facts_identity) +
        "\",\"classification\":{\"state_in_calc\":" +
        std::string(bool_json(service.state_in_calc)) +
        ",\"goal\":" + std::string(bool_json(service.goal)) +
        ",\"goal_mask\":" + std::to_string(service.goal_mask) +
        ",\"goal_progress\":" + std::to_string(service.goal_progress) +
        ",\"certified_frontier_operator\":";
    if (service.certified_frontier_operator == kNoId) json += "null";
    else json += std::to_string(service.certified_frontier_operator);
    json += "},\"observation_graph\":{\"identity\":\"" +
        hex(service.observation_graph_identity) +
        "\",\"prefix_identity\":\"" +
        hex(service.observation_graph_prefix_identity) +
        "\",\"states\":" +
        std::to_string(service.observation_state_count) +
        ",\"rows\":" + std::to_string(service.observation_row_count) +
        ",\"priced_rows\":" +
        std::to_string(service.observation_priced_row_count) +
        ",\"successors\":" +
        std::to_string(service.observation_successor_count) +
        ",\"probabilities\":" +
        std::to_string(service.observation_probability_count) +
        ",\"choices\":" +
        std::to_string(service.observation_choice_count) +
        ",\"choice_successors\":" +
        std::to_string(service.observation_choice_successor_count) +
        ",\"choice_options\":" +
        std::to_string(service.observation_choice_option_count) + "}";
    json += ",\"expansion\":{\"broad\":" +
        std::string(bool_json(service.broad_expanded)) +
        ",\"ordinary_result\":" +
        std::string(bool_json(service.ordinary_result_expanded)) +
        ",\"transition_cache\":" +
        std::string(bool_json(service.transition_cache_expanded)) +
        ",\"focused_strict\":" +
        std::string(bool_json(service.focused_strict_expanded)) +
        ",\"queued_flag\":" + std::string(bool_json(service.queued)) +
        ",\"queue_contains_state\":" +
        std::string(bool_json(service.queue_contains_state)) +
        ",\"active_for_state\":" +
        std::string(bool_json(service.expansion_active_for_state)) +
        ",\"incremental_alternative\":" +
        std::string(bool_json(
            service.expansion_incremental_alternative)) + "}";
    json += ",\"candidate_scope\":{\"planner_operators\":" +
        std::to_string(service.planner_operator_count) +
        ",\"priced_operators\":" +
        std::to_string(service.priced_operator_count) +
        ",\"static_operators\":" +
        std::to_string(service.static_candidate_operator_count) +
        ",\"delayed_operators\":" +
        std::to_string(service.delayed_candidate_operator_count) +
        ",\"dynamic_operators\":" +
        std::to_string(service.dynamic_candidate_operator_count) + "}";
    json += ",\"rows\":{\"span_present\":" +
        std::string(bool_json(service.row_span_present)) +
        ",\"declared\":" + std::to_string(service.declared_row_count) +
        ",\"owned\":" + std::to_string(service.owned_row_count) +
        ",\"owner_mismatches\":" +
        std::to_string(service.row_owner_mismatch_count) +
        ",\"admitted\":" +
        std::to_string(service.admitted_row_count) +
        ",\"completed\":" +
        std::to_string(service.completed_row_count) +
        ",\"alternative\":" +
        std::to_string(service.alternative_row_count) +
        ",\"alternative_completed\":" +
        std::to_string(service.alternative_completed_row_count) +
        ",\"priced\":" + std::to_string(service.priced_row_count) +
        ",\"valid_operator\":" +
        std::to_string(service.valid_operator_row_count) +
        ",\"finite_nonnegative_priced\":" +
        std::to_string(service.finite_nonnegative_priced_row_count) +
        ",\"selectable\":" +
        std::to_string(service.selectable_row_count) +
        ",\"selectable_operators\":" +
        std::to_string(service.selectable_operator_count) +
        ",\"variants\":" + std::to_string(service.variant_count) +
        ",\"transitions\":" +
        std::to_string(service.transition_count) +
        ",\"choices\":" + std::to_string(service.choice_count) +
        ",\"selectable_operator_identity\":\"" +
        hex(service.selectable_operator_identity) +
        "\",\"row_identity\":\"" + hex(service.row_identity) + "\"}";
    json += ",\"scheduler\":{\"carrier\":" +
        std::string(bool_json(service.carrier)) +
        ",\"missing_frontier\":" +
        std::string(bool_json(service.missing_frontier)) +
        ",\"priority_tasks\":" +
        std::to_string(service.priority_task_count) +
        ",\"pending_priority_tasks\":" +
        std::to_string(service.pending_priority_task_count) +
        ",\"carrier_cursor\":" + std::to_string(service.carrier_cursor) +
        ",\"automatic_carrier_cursor\":" +
        std::to_string(service.automatic_carrier_cursor) +
        ",\"automatic_order_cursor\":" +
        std::to_string(service.automatic_order_cursor) +
        ",\"fairness_carrier_cursor\":" +
        std::to_string(service.fairness_carrier_cursor) +
        ",\"fairness_operator_cursor\":" +
        std::to_string(service.fairness_operator_cursor) +
        ",\"high_progress_carrier_cursor\":" +
        std::to_string(service.high_progress_carrier_cursor) +
        ",\"high_progress_operator_cursor\":" +
        std::to_string(service.high_progress_operator_cursor) +
        ",\"closure_carrier_cursor\":" +
        std::to_string(service.closure_carrier_cursor) +
        ",\"closure_operator_cursor\":" +
        std::to_string(service.closure_operator_cursor) +
        ",\"priority_task_cursor\":" +
        std::to_string(service.priority_task_cursor) +
        ",\"refinement_active\":" +
        std::string(bool_json(service.incremental_refinement_active)) +
        ",\"refinement_targets_state\":" +
        std::string(bool_json(
            service.incremental_refinement_targets_state)) +
        ",\"identity\":\"" + hex(service.scheduler_identity) + "\"";
    append_position("queue_position", service.queue_position);
    append_position("carrier_position", service.carrier_position);
    append_position(
        "automatic_order_position", service.automatic_order_position);
    append_position(
        "fairness_order_position", service.fairness_order_position);
    append_position(
        "high_progress_order_position",
        service.high_progress_order_position);
    append_position(
        "missing_frontier_position", service.missing_frontier_position);
    append_position(
        "first_priority_task_position",
        service.first_priority_task_position);
    json += "}";
    json += ",\"terminal\":{\"requested_bounded_finish\":" +
        std::string(bool_json(service.requested_bounded_finish)) +
        ",\"resource_cap_hit\":" +
        std::string(bool_json(service.resource_cap_hit)) + "}";
    json += ",\"action_envelope\":{\"scheduler_view_enabled\":" +
        std::string(bool_json(
            service.action_envelope_scheduler_view_enabled)) +
        ",\"transition_count\":" +
        std::to_string(service.action_envelope_transition_count) +
        ",\"entries\":" +
        std::to_string(service.action_envelope_entry_count) +
        ",\"row_entries\":" +
        std::to_string(service.action_envelope_row_entry_count) +
        ",\"scheduling_complete\":" +
        std::to_string(
            service.action_envelope_scheduling_complete_count) +
        ",\"completed_pairs\":" +
        std::to_string(service.completed_pair_count) +
        ",\"identity\":\"" + hex(service.action_envelope_identity) +
        "\",\"lifecycle_counts\":";
    append_named_counts(
        service.action_envelope_lifecycle_counts, lifecycle_names);
    json += ",\"lane_counts\":";
    append_named_counts(service.action_envelope_lane_counts, lane_names);
    json += ",\"authority_counts\":";
    append_named_counts(
        service.action_envelope_authority_counts, authority_names);
    json += ",\"stop_owner_counts\":";
    append_named_counts(
        service.action_envelope_stop_owner_counts, stop_owner_names);
    json += "}}";
    };
    json += ",\"row_service_witness_v1\":{\"initial\":";
    append_row_service_witness(capture.row_service_witness);
    json += ",\"terminal\":";
    append_row_service_witness(capture.terminal_row_service_witness);
    json += "}";
    json += ",\"counts\":{\"selected_states\":" +
        std::to_string(capture.selected_states) +
        ",\"goal_stops\":" + std::to_string(capture.goal_stops) +
        ",\"certified_frontier_stops\":" +
        std::to_string(capture.certified_frontier_stops) +
        ",\"unresolved_stops\":" +
        std::to_string(capture.unresolved_stops) + "}";
    json += ",\"retained_owned_bytes\":" +
        std::to_string(capture.retained_owned_bytes);
    json += ",\"stops\":[";
    const std::size_t sample_count = std::min<std::size_t>(
        capture.stops.size(), limits.max_samples);
    for (std::size_t index = 0; index < sample_count; ++index) {
        if (index != 0) json.push_back(',');
        const CarrierLadderBoundaryCapture::Stop& stop = capture.stops[index];
        const char* kind = "unresolved_missing";
        switch (stop.kind) {
        case CarrierLadderBoundaryCapture::StopKind::RequestedEntry:
            kind = "requested_entry";
            break;
        case CarrierLadderBoundaryCapture::StopKind::GoalSuccess:
            kind = "goal_success";
            break;
        case CarrierLadderBoundaryCapture::StopKind::CertifiedFrontier:
            kind = "certified_frontier";
            break;
        case CarrierLadderBoundaryCapture::StopKind::UnresolvedMissing:
            break;
        }
        json += "{\"state\":" + std::to_string(stop.state) +
            ",\"kind\":\"" + kind + "\",\"operator_index\":";
        if (stop.operator_index == kNoId) json += "null";
        else json += std::to_string(stop.operator_index);
        json += ",\"coarse_state_key\":[";
        for (std::size_t word = 0;
             word < stop.coarse_state_key.size(); ++word) {
            if (word != 0) json.push_back(',');
            json += "\"" + hex(stop.coarse_state_key[word]) + "\"";
        }
        json += "],\"operator_semantic_identity\":\"" +
            hex(stop.operator_semantic_identity) +
            "\",\"continuation_identity\":\"" +
            hex(stop.continuation_identity) +
            "\",\"incumbent_identity\":\"" +
            hex(stop.incumbent_identity) +
            "\",\"incumbent_graph_prefix_identity\":\"" +
            hex(stop.incumbent_graph_prefix_identity) +
            "\",\"incumbent_artifact_identity\":\"" +
            hex(stop.incumbent_artifact_identity) +
            "\",\"frontier_value_bits\":\"" +
            hex(stop.frontier_value_bits) +
            "\",\"independently_certified\":" +
            std::string(stop.independently_certified ? "true" : "false") +
            ",\"independently_evaluated\":" +
            std::string(stop.independently_evaluated ? "true" : "false") +
            ",\"proper\":" +
            std::string(stop.proper ? "true" : "false") +
            ",\"executable\":" +
            std::string(stop.executable ? "true" : "false") + "}";
    }
    json += "],\"stop_samples_omitted\":" +
        std::to_string(capture.stops.size() - sample_count);
    json += ",\"recovery\":{\"status\":";
    append_json_string(json, capture.recovery_status);
    json += ",\"refusal\":";
    if (capture.recovery_refusal.empty()) json += "null";
    else append_json_string(json, capture.recovery_refusal);
    json += ",\"complete_support\":" +
        std::string(capture.complete_support ? "true" : "false");
    json += ",\"absorption_proved\":" +
        std::string(capture.absorption_proved ? "true" : "false");
    json += ",\"exact_states\":" +
        std::to_string(capture.exact_states);
    json += ",\"exact_rows\":" + std::to_string(capture.exact_rows);
    json += ",\"exact_transitions\":" +
        std::to_string(capture.exact_transitions);
    json += ",\"work_items\":" + std::to_string(capture.recovery_work);
    json += ",\"peak_owned_bytes\":" +
        std::to_string(capture.recovery_peak_owned_bytes);
    json += ",\"wall_time_ms\":" +
        std::to_string(capture.recovery_wall_time_ms);
    json += ",\"member_identity\":\"" +
        hex(capture.exact_member_identity) + "\"";
    json += ",\"member_count\":" +
        std::to_string(capture.recovered_members.size());
    json += ",\"members\":[";
    const auto key_identity = [&](const std::vector<std::uint64_t>& key) {
        std::uint64_t identity = 1469598103934665603ULL;
        identity_mix(identity, 1); /* bounded stable-key projection schema */
        identity_mix(identity, key.size());
        for (const std::uint64_t word : key) {
            identity_mix(identity, word);
        }
        return identity;
    };
    const std::size_t member_samples = std::min<std::size_t>(
        capture.recovered_members.size(), limits.max_samples);
    const auto append_slot = [&](const pc_mod_slot& slot) {
        json += "{\"mod_id\":" + std::to_string(slot.mod_id) +
            ",\"group_id\":" + std::to_string(slot.group_id) +
            ",\"flags\":" + std::to_string(slot.flags) +
            ",\"rolls\":[";
        for (std::uint32_t roll = 0; roll < slot.roll_count; ++roll) {
            if (roll != 0) json.push_back(',');
            json += std::to_string(slot.rolls[roll]);
        }
        json += "],\"veiled_options\":[";
        for (std::uint32_t option = 0;
             option < slot.veiled_option_count; ++option) {
            if (option != 0) json.push_back(',');
            json += std::to_string(slot.veiled_option_mod_ids[option]);
        }
        json += "],\"veiled_chosen_mod_id\":";
        if (slot.veiled_chosen_mod_id == PC_MOD_NONE) json += "null";
        else json += std::to_string(slot.veiled_chosen_mod_id);
        json += "}";
    };
    const auto append_slots = [&](const pc_mod_slot* slots,
                                  const std::uint8_t count) {
        json.push_back('[');
        for (std::uint32_t index = 0; index < count; ++index) {
            if (index != 0) json.push_back(',');
            append_slot(slots[index]);
        }
        json.push_back(']');
    };
    for (std::size_t index = 0; index < member_samples; ++index) {
        if (index != 0) json.push_back(',');
        const auto& member = capture.recovered_members[index];
        json += "{\"coarse_state\":" +
            std::to_string(member.coarse_state) +
            ",\"stable_key_identity\":\"" +
            hex(key_identity(member.stable_key)) +
            "\",\"stable_key_words\":" +
            std::to_string(member.stable_key.size());
        const pc_item_state& item = member.item;
        json += ",\"item\":{\"rarity\":" +
            std::to_string(item.rarity) + ",\"quality\":" +
            std::to_string(item.quality) + ",\"item_flags\":" +
            std::to_string(item.item_flags) + ",\"prefixes\":";
        append_slots(item.prefixes, item.prefix_count);
        json += ",\"suffixes\":";
        append_slots(item.suffixes, item.suffix_count);
        json += ",\"implicits\":";
        append_slots(item.implicits, item.implicit_count);
        json += ",\"enchantments\":";
        append_slots(item.enchantments, item.enchantment_count);
        json += ",\"generic_influence_bits\":" +
            std::to_string(item.generic_influence_bits);
        json += ",\"searing_exarch_tier\":" +
            std::to_string(item.searing_exarch_tier);
        json += ",\"eater_of_worlds_tier\":" +
            std::to_string(item.eater_of_worlds_tier);
        json += ",\"socket_colors\":[";
        for (std::uint32_t socket = 0; socket < item.socket_count; ++socket) {
            if (socket != 0) json.push_back(',');
            json += std::to_string(item.socket_colors[socket]);
        }
        json += "],\"link_mask\":" + std::to_string(item.link_mask) +
            "}}";
    }
    json += "],\"member_samples_omitted\":" +
        std::to_string(capture.recovered_members.size() - member_samples);
    json += ",\"selected_prefix_support_v1\":{\"identity\":\"" +
        hex(capture.reached_stop_identity) + "\",\"reached_stop_count\":" +
        std::to_string(capture.reached_stop_count) +
        ",\"samples\":[";
    const auto append_key = [&](const std::vector<std::uint64_t>& key) {
        json.push_back('[');
        for (std::size_t word = 0; word < key.size(); ++word) {
            if (word != 0) json.push_back(',');
            json += "\"" + hex(key[word]) + "\"";
        }
        json.push_back(']');
    };
    for (std::size_t index = 0;
         index < capture.reached_stops.size(); ++index) {
        if (index != 0) json.push_back(',');
        const CarrierLadderBoundaryCapture::ReachedStop& reached =
            capture.reached_stops[index];
        const char* kind = "unresolved_missing";
        switch (reached.kind) {
        case CarrierLadderBoundaryCapture::StopKind::RequestedEntry:
            kind = "requested_entry";
            break;
        case CarrierLadderBoundaryCapture::StopKind::GoalSuccess:
            kind = "goal_success";
            break;
        case CarrierLadderBoundaryCapture::StopKind::CertifiedFrontier:
            kind = "certified_frontier";
            break;
        case CarrierLadderBoundaryCapture::StopKind::UnresolvedMissing:
            break;
        }
        json += "{\"predecessor_stable_key_identity\":\"" +
            hex(key_identity(reached.predecessor_stable_key)) +
            "\",\"predecessor_stable_key_words\":" +
            std::to_string(reached.predecessor_stable_key.size()) +
            ",\"predecessor_coarse_state\":" +
            std::to_string(reached.predecessor_coarse_state) +
            ",\"predecessor_coarse_state_key\":";
        append_key(reached.predecessor_coarse_state_key);
        json += ",\"selected_coarse_operator\":";
        if (reached.selected_coarse_operator == kNoId) json += "null";
        else json += std::to_string(reached.selected_coarse_operator);
        json += ",\"selected_strict_operator\":";
        if (reached.selected_strict_operator == kNoId) json += "null";
        else json += std::to_string(reached.selected_strict_operator);
        json += ",\"selected_action_semantic_identity\":\"" +
            hex(key_identity(reached.selected_action_semantic_key)) +
            "\",\"selected_action_semantic_key_words\":" +
            std::to_string(reached.selected_action_semantic_key.size()) +
            ",\"stopped_stable_key_identity\":\"" +
            hex(key_identity(reached.stopped_stable_key)) +
            "\",\"stopped_stable_key_words\":" +
            std::to_string(reached.stopped_stable_key.size()) +
            ",\"stopped_coarse_state\":" +
            std::to_string(reached.stopped_coarse_state) +
            ",\"stopped_coarse_state_key\":";
        append_key(reached.stopped_coarse_state_key);
        json += ",\"probability_bits\":\"" +
            hex(reached.probability_bits) + "\",\"captured_policy_row\":";
        if (reached.captured_policy_row ==
                std::numeric_limits<std::uint64_t>::max()) {
            json += "null";
        } else {
            json += std::to_string(reached.captured_policy_row);
        }
        json += ",\"stop_kind\":\"" + std::string(kind) +
            "\",\"captured_coarse_reachable\":" +
            std::string(
                reached.captured_coarse_reachable ? "true" : "false") +
            ",\"completed_selected_row\":" +
            std::string(
                reached.completed_selected_row ? "true" : "false") +
            ",\"disposition\":\"" +
            std::string(
                reached.completed_selected_row
                    ? "completed_selected_row_available"
                    : "unresolved_no_completed_selected_row") +
            "\"}";
    }
    json += "],\"samples_omitted\":" +
        std::to_string(capture.reached_stop_samples_omitted) + "}}}}";
    diagnostics.carrier_ladder_exact_boundary_json = std::move(json);
}

bool SolveWork::Impl::incumbent_precedes(
        const BoundedPolicyIncumbent& left,
        const BoundedPolicyIncumbent& right) const {
        const auto contract = [&](const BoundedPolicyIncumbent& value) {
            CertifiedFallbackContract view;
            view.certified_upper_bound = value.certified_upper_bound;
            view.root_operator =
                result.start_state < value.policy.size()
                    ? value.policy[result.start_state].index
                    : kNoId;
            view.kind = value.kind;
            view.witness_identity =
                value.primitive_renewal_witness.witness_hash;
            view.portfolio_identity = value.portfolio_identity;
            return view;
        };
        return certified_fallback_precedes(
            contract(left), contract(right));
    }

const char* SolveWork::Impl::retained_incumbent_invalid_reason(
        const BoundedPolicyIncumbent& incumbent) const {
        CertifiedFallbackContract candidate;
        candidate.certified_upper_bound =
            incumbent.certified_upper_bound;
        candidate.evaluated_policy_cost =
            incumbent.evaluated_policy_cost;
        candidate.goal_identity = incumbent.goal_identity;
        candidate.economy_identity = incumbent.economy_identity;
        candidate.action_vocabulary_identity =
            incumbent.action_vocabulary_identity;
        candidate.action_vocabulary_size =
            incumbent.action_vocabulary_size;
        candidate.caller_scope_identity =
            incumbent.caller_scope_identity;
        candidate.artifact_identity = incumbent.artifact_identity;
        candidate.source_generation = incumbent.source_generation;
        candidate.target_generation = incumbent.target_generation;
        candidate.graph_prefix_identity =
            incumbent.graph_prefix_identity;
        candidate.complete_policy_or_witness =
            incumbent.policy_materialized && !incumbent.policy.empty();
        candidate.compiled_payload_present =
            !incumbent.compiled_artifact.strategy_json.empty();
        candidate.compilation_provenance_present =
            !incumbent.compilation_provenance.empty();
        CertifiedFallbackCurrentContext current;
        current.goal_identity = goal_identity();
        current.economy_identity = economy_identity();
        current.action_vocabulary_size = operators.size();
        current.action_vocabulary_identity =
            action_vocabulary_prefix_identity(
                incumbent.action_vocabulary_size);
        current.caller_scope_identity = caller_scope_identity();
        current.artifact_identity = artifact_identity();
        current.source_generation = transition_cache->rows.size();
        current.target_generation = calc.state_count();
        current.graph_prefix_identity =
            incumbent_graph_prefix_identity(
                incumbent.graph_row_count,
                incumbent.graph_priced_row_count,
                incumbent.graph_successor_count,
                incumbent.graph_probability_count,
                incumbent.graph_choice_count,
                incumbent.graph_choice_successor_count,
                incumbent.graph_choice_option_count);
        return solve_detail::retained_fallback_invalid_reason(
            candidate, current);
    }

const char* SolveWork::Impl::certified_incumbent_invalid_reason(
        const BoundedPolicyIncumbent& incumbent) const {
        if (const char* retained =
                retained_incumbent_invalid_reason(incumbent)) {
            return retained;
        }
        if (!incumbent.independently_certified ||
            !incumbent.independently_evaluated) {
            return "final_graph_not_independently_evaluated";
        }
        if (!incumbent.proper) return "improper_policy";
        if (!incumbent.executable) return "policy_not_executable";
        if (!std::isfinite(incumbent.certified_upper_bound) ||
            !std::isfinite(incumbent.evaluated_policy_cost) ||
            incumbent.evaluated_policy_cost < 0.0 ||
            incumbent.evaluated_policy_cost >
                incumbent.certified_upper_bound +
                    options.epsilon * std::max(
                        1.0,
                        std::abs(incumbent.certified_upper_bound)) *
                        10.0) {
            return "evaluated_cost_invalid";
        }
        return nullptr;
    }

bool SolveWork::Impl::certify_incumbent_for_fallback(
        BoundedPolicyIncumbent& incumbent) {
        if (!incumbent.primitive_renewal_witness.valid) return false;
        populate_incumbent_policy(incumbent);
        const PrimitiveRenewalWitness& witness =
            incumbent.primitive_renewal_witness;
        if (result.start_state >= incumbent.policy.size() ||
            result.start_state >= incumbent.policy_row_costs.size() ||
            witness.operator_index >= calc.operators().size() ||
            incumbent.policy[result.start_state].index !=
                witness.operator_index ||
            !(witness.success_probability > 0.0) ||
            !std::isfinite(witness.success_probability) ||
            !std::isfinite(incumbent.policy_row_costs[result.start_state]) ||
            incumbent.policy_row_costs[result.start_state] < 0.0) {
            ++result.diagnostics.policy_refinement
                  .fallback_portfolio_invalidations;
            return false;
        }
        const double evaluated_cost =
            incumbent.policy_row_costs[result.start_state] /
            witness.success_probability;
        if (!std::isfinite(evaluated_cost) || evaluated_cost < 0.0 ||
            std::abs(
                evaluated_cost - incumbent.certified_upper_bound) >
                value_comparison_tolerance(
                    incumbent.certified_upper_bound)) {
            ++result.diagnostics.policy_refinement
                  .fallback_portfolio_invalidations;
            return false;
        }
        incumbent.evaluated_policy_cost = kInfinity;
        SolveResult proof;
        proof.policy_available = true;
        proof.policy_status = SolvePolicyStatus::BoundedFeasible;
        proof.termination = SolveTermination::RefusedResourceCap;
        proof.lower_bound = certified_global_lower_bound();
        proof.upper_bound = incumbent.certified_upper_bound;
        proof.evaluated_policy_cost = incumbent.evaluated_policy_cost;
        proof.start_state = result.start_state;
        proof.has_exact_start_item = result.has_exact_start_item;
        proof.exact_start_item = result.exact_start_item;
        proof.values = incumbent.values;
        proof.policy = incumbent.policy;
        proof.policy_reachable = incumbent.policy_reachable;
        proof.unveil_preferences = incumbent.unveil_preferences;
        proof.option_unveil_preferences =
            incumbent.option_unveil_preferences;
        proof.behavioral_representative_by_state =
            incumbent.behavioral_representative_by_state;
        proof.primitive_renewal_witness =
            incumbent.primitive_renewal_witness;
        proof.options = options;
        proof.goal_states.assign(proof.values.size(), 0);
        proof.expanded.assign(proof.values.size(), 0);
        for (std::uint32_t state = 0; state < proof.values.size(); ++state) {
            proof.goal_states[state] =
                calc.is_goal_state(calc.state(state)) ? 1 : 0;
        }
        PolicyCompilationTelemetry compilation;
        const std::uint64_t live = fast_estimated_owned_bytes();
        const std::uint64_t candidate_bytes = incumbent_owned_bytes(incumbent);
        if (live >= options.max_solver_owned_bytes ||
            candidate_bytes >= options.max_solver_owned_bytes - live) {
            ++result.diagnostics.policy_refinement
                  .fallback_portfolio_memory_rejections;
            return false;
        }
        try {
            incumbent.compiled_artifact.strategy_json =
                compile_policy_strategy_json(
                    calc, proof, "certified executable fallback",
                    &compilation, options.max_strategy_json_bytes,
                    nullptr,
                    options.max_solver_owned_bytes - live -
                        candidate_bytes);
        } catch (const std::exception&) {
            ++result.diagnostics.policy_refinement
                  .fallback_portfolio_compilation_failures;
            incumbent.compiled_artifact = {};
            return false;
        }
        if (!compilation.cap_hit.empty() ||
            incumbent.compiled_artifact.strategy_json.empty()) {
            ++result.diagnostics.policy_refinement
                  .fallback_portfolio_compilation_failures;
            incumbent.compiled_artifact = {};
            return false;
        }
        incumbent.compiled_artifact.working_states =
            compilation.working_states;
        incumbent.compiled_artifact.behavioral_classes =
            compilation.behavioral_classes;
        incumbent.compiled_artifact.policy_regions =
            compilation.policy_regions;
        incumbent.compiled_artifact.infrastructure_nodes =
            compilation.infrastructure_nodes;
        incumbent.compiled_artifact.policy_route_nodes =
            compilation.policy_route_nodes;
        incumbent.compiled_artifact.local_gated_route_nodes =
            compilation.local_gated_route_nodes;
        incumbent.compiled_artifact.primitive_region_nodes =
            compilation.primitive_region_nodes;
        incumbent.compiled_artifact.additional_recipe_nodes =
            compilation.additional_recipe_nodes;
        incumbent.compiled_artifact.nodes = compilation.nodes;
        incumbent.compiled_artifact.edges = compilation.edges;
        incumbent.compiled_artifact.total_condition_bytes =
            compilation.total_condition_bytes;
        incumbent.compiled_artifact.max_condition_bytes =
            compilation.max_condition_bytes;
        incumbent.compiled_artifact.condition_edges =
            compilation.condition_edges;
        incumbent.compiled_artifact.unique_condition_literals =
            compilation.unique_condition_literals;
        incumbent.compiled_artifact.repeated_condition_occurrences =
            compilation.repeated_condition_occurrences;
        incumbent.compiled_artifact.repeated_condition_bytes =
            compilation.repeated_condition_bytes;
        incumbent.compiled_artifact.policy_route_nondefault_edges =
            compilation.policy_route_nondefault_edges;
        incumbent.compiled_artifact.policy_route_distinct_targets =
            compilation.policy_route_distinct_targets;
        incumbent.compiled_artifact.same_target_branch_groups =
            compilation.same_target_branch_groups;
        incumbent.compiled_artifact.same_target_branch_edges =
            compilation.same_target_branch_edges;
        incumbent.compiled_artifact.projected_same_target_edge_savings =
            compilation.projected_same_target_edge_savings;
        incumbent.compiled_artifact.max_policy_route_out_degree =
            compilation.max_policy_route_out_degree;
        incumbent.compiled_artifact.max_policy_route_distinct_targets =
            compilation.max_policy_route_distinct_targets;
        incumbent.compiled_artifact.exact_state_fallbacks =
            compilation.exact_state_fallbacks;
        incumbent.compiled_artifact.junk_predicates =
            compilation.junk_predicates;
        incumbent.compiled_artifact.policy_route_default_edges =
            compilation.policy_route_default_edges;
        incumbent.compiled_artifact.policy_route_restart_default_edges =
            compilation.policy_route_restart_default_edges;
        incumbent.compiled_artifact.policy_route_offpolicy_default_edges =
            compilation.policy_route_offpolicy_default_edges;
        incumbent.compiled_artifact.policy_route_default_mode =
            compilation.policy_route_default_mode;
        incumbent.compiled_artifact.peak_owned_bytes =
            compilation.peak_owned_bytes;
        incumbent.compiled_artifact
            .previously_accounted_peak_owned_bytes =
            compilation.previously_accounted_peak_owned_bytes;
        incumbent.compiled_artifact.complete_peak_owned_bytes =
            compilation.complete_peak_owned_bytes;
        incumbent.compilation_provenance =
            "compiled_primitive_renewal_pending_final_graph_evaluation_v1";
        incumbent.proper = false;
        incumbent.executable = false;
        incumbent.independently_certified = false;
        incumbent.independently_evaluated = false;
        incumbent.retained_owned_bytes = incumbent_owned_bytes(incumbent);
        return true;
    }

bool SolveWork::Impl::retain_certified_incumbent(
        const BoundedPolicyIncumbent& incumbent,
        const std::uint64_t additional_live_bytes) {
        PolicyRefinementTelemetry& telemetry =
            result.diagnostics.policy_refinement;
        if (const char* reason =
                retained_incumbent_invalid_reason(incumbent)) {
            (void)reason;
            ++telemetry.fallback_portfolio_invalidations;
            return false;
        }
        for (const BoundedPolicyIncumbent& retained :
             certified_fallback_portfolio) {
            if (retained.portfolio_identity ==
                incumbent.portfolio_identity) {
                return true;
            }
        }
        constexpr std::size_t kMaximumFallbacks = 4;
        if (certified_fallback_portfolio.size() >= kMaximumFallbacks &&
            !incumbent_precedes(
                incumbent, certified_fallback_portfolio.back())) {
            return true;
        }
        const std::uint64_t candidate_dynamic_bytes =
            incumbent_owned_bytes(incumbent) -
            sizeof(BoundedPolicyIncumbent);
        std::uint64_t current_bytes = fast_estimated_owned_bytes();
        current_bytes = additional_live_bytes >
                std::numeric_limits<std::uint64_t>::max() - current_bytes
            ? std::numeric_limits<std::uint64_t>::max()
            : current_bytes + additional_live_bytes;
        std::uint64_t added_bytes = candidate_dynamic_bytes;
        if (certified_fallback_portfolio.size() < kMaximumFallbacks &&
            certified_fallback_portfolio.capacity() <
                   kMaximumFallbacks) {
            added_bytes +=
                (kMaximumFallbacks -
                 certified_fallback_portfolio.capacity()) *
                sizeof(BoundedPolicyIncumbent);
        }
        if (!certified_fallback_fits_memory(
                current_bytes, added_bytes,
                options.max_solver_owned_bytes)) {
            ++telemetry.fallback_portfolio_memory_rejections;
            return false;
        }
        if (certified_fallback_portfolio.capacity() <
            kMaximumFallbacks) {
            certified_fallback_portfolio.reserve(kMaximumFallbacks);
        }
        if (certified_fallback_portfolio.size() >= kMaximumFallbacks) {
            certified_fallback_portfolio.back() = incumbent;
        } else {
            certified_fallback_portfolio.push_back(incumbent);
        }
        std::sort(
            certified_fallback_portfolio.begin(),
            certified_fallback_portfolio.end(),
            [&](const BoundedPolicyIncumbent& left,
                const BoundedPolicyIncumbent& right) {
                return incumbent_precedes(left, right);
            });
        incumbent_portfolio.observe_verified(incumbent);
        if (options.carrier_ladder_exact_boundary_mode ==
                CarrierLadderExactBoundaryMode::ResumableContinuation) {
            audit_verified_incumbent_operator_proof_shadow(incumbent);
        }
        telemetry.fallback_portfolio_candidates =
            certified_fallback_portfolio.size();
        telemetry.fallback_portfolio_owned_bytes = 0;
        for (BoundedPolicyIncumbent& retained :
             certified_fallback_portfolio) {
            retained.retained_owned_bytes = incumbent_owned_bytes(retained);
            telemetry.fallback_portfolio_owned_bytes +=
                retained.retained_owned_bytes;
        }
        return true;
    }

bool SolveWork::Impl::retain_current_certified_incumbent() {
        if (!output_incumbent.has_value() ||
            output_incumbent->compiled_artifact.strategy_json.empty()) {
            return true;
        }
        return retain_certified_incumbent(*output_incumbent);
    }

auto SolveWork::Impl::best_current_certified_fallback()
        -> BoundedPolicyIncumbent* {
        PolicyRefinementTelemetry& telemetry =
            result.diagnostics.policy_refinement;
        for (auto candidate = certified_fallback_portfolio.begin();
             candidate != certified_fallback_portfolio.end();) {
            const char* retained_reason =
                retained_incumbent_invalid_reason(*candidate);
            if (retained_reason != nullptr) {
                ++telemetry.fallback_portfolio_invalidations;
                candidate = certified_fallback_portfolio.erase(candidate);
            } else if (certified_incumbent_invalid_reason(*candidate) ==
                       nullptr) {
                ++candidate;
            } else {
                ++candidate;
            }
        }
        telemetry.fallback_portfolio_candidates =
            certified_fallback_portfolio.size();
        telemetry.fallback_portfolio_owned_bytes = 0;
        for (const BoundedPolicyIncumbent& retained :
             certified_fallback_portfolio) {
            telemetry.fallback_portfolio_owned_bytes +=
                incumbent_owned_bytes(retained);
        }
        BoundedPolicyIncumbent* best = nullptr;
        for (BoundedPolicyIncumbent& retained :
             certified_fallback_portfolio) {
            if (certified_incumbent_invalid_reason(retained) != nullptr) {
                continue;
            }
            if (best == nullptr || incumbent_precedes(retained, *best)) {
                best = &retained;
            }
        }
        return best;
    }

bool SolveWork::Impl::commit_output_incumbent(
        BoundedPolicyIncumbent candidate) {
        const std::uint64_t displaced_identity =
            output_incumbent.has_value() &&
                output_incumbent->portfolio_identity !=
                    candidate.portfolio_identity
            ? output_incumbent->portfolio_identity
            : 0;
        record_upper_attribution_milestone(
            candidate.certified_upper_bound,
            candidate.independently_evaluated &&
                candidate.independently_certified);
        const std::uint64_t candidate_owned =
            incumbent_owned_bytes(candidate);
        const std::uint64_t candidate_dynamic =
            candidate_owned >= sizeof(BoundedPolicyIncumbent)
                ? candidate_owned - sizeof(BoundedPolicyIncumbent)
                : candidate_owned;
        if (output_incumbent.has_value() &&
            !output_incumbent->compiled_artifact.strategy_json.empty() &&
            output_incumbent->portfolio_identity !=
                candidate.portfolio_identity &&
            !retain_certified_incumbent(
                *output_incumbent, candidate_dynamic) &&
            !candidate.independently_certified) {
            /* Memory pressure cannot let an uncertified preferred policy
             * destroy the only executable result. Keep the old fallback as
             * the selected output instead. */
            return false;
        }
        output_incumbent = std::move(candidate);
        if (displaced_identity != 0 &&
            options.carrier_ladder_exact_boundary_mode !=
                CarrierLadderExactBoundaryMode::Off) {
            const bool retained = std::any_of(
                certified_fallback_portfolio.begin(),
                certified_fallback_portfolio.end(),
                [&](const BoundedPolicyIncumbent& fallback) {
                    return fallback.portfolio_identity ==
                        displaced_identity;
                });
            for (JointAnytimeAttemptLineage& lineage :
                 joint_anytime_attempt_lineage) {
                if (lineage.candidate_portfolio_identity !=
                    displaced_identity) {
                    continue;
                }
                lineage.portfolio_decision = retained
                    ? "retained_for_final_graph_comparison"
                    : "displaced_before_final_graph_comparison";
                lineage.portfolio_reason = retained
                    ? "compiled candidate retained in bounded portfolio"
                    : "candidate had no retained compiled artifact";
            }
        }
        incumbent_portfolio.observe_verified(*output_incumbent);
        result.diagnostics.incumbent_kind = output_incumbent->kind;
        result.diagnostics.incumbent_round = output_incumbent->round;
        result.diagnostics.incumbent_restart_state =
            output_incumbent->restart_state;
        result.diagnostics.incumbent_anchor_state =
            output_incumbent->fallback_anchor_state;
        result.diagnostics.incumbent_goal_identity =
            output_incumbent->goal_identity;
        result.diagnostics.incumbent_economy_identity =
            output_incumbent->economy_identity;
        result.diagnostics.incumbent_action_vocabulary_identity =
            output_incumbent->action_vocabulary_identity;
        result.diagnostics.incumbent_graph_identity =
            output_incumbent->graph_identity;
        result.diagnostics.incumbent_strict_state_provenance =
            output_incumbent->strict_state_provenance;
        result.diagnostics.policy_refinement.preferred_candidate_upper =
            output_incumbent->certified_upper_bound;
        return true;
    }

void SolveWork::Impl::install_output_incumbent(
        const double upper,
        const std::vector<double>& values,
        const std::vector<std::uint64_t>& selected_rows,
        const std::vector<std::uint32_t>& frontier_operators,
        const FocusedFallbackWitness& fallback,
        std::string kind,
        const std::vector<std::uint8_t>* policy_reachable,
        const PrimitiveRenewalWitness* primitive_renewal_witness,
        const bool replace_equal_incumbent,
        const bool record_memory_refusal,
        const bool prefer_materialized_over_unverified) {
        if (!std::isfinite(upper) || upper < 0.0 ||
            result.start_state >= values.size()) {
            return;
        }
        const auto saturated_product = [](
            const std::size_t count, const std::size_t width) {
            return count >
                    std::numeric_limits<std::uint64_t>::max() / width
                ? std::numeric_limits<std::uint64_t>::max()
                : static_cast<std::uint64_t>(count) * width;
        };
        const auto saturated_add = [](
            const std::uint64_t left, const std::uint64_t right) {
            return right >
                    std::numeric_limits<std::uint64_t>::max() - left
                ? std::numeric_limits<std::uint64_t>::max()
                : left + right;
        };
        const auto doubled = [&](const std::uint64_t value) {
            return saturated_add(value, value);
        };
        const auto vector_capacity_upper = [](
            const std::size_t elements) {
            if (elements == 0) return std::size_t{0};
            if (elements >
                (std::numeric_limits<std::size_t>::max() - 16) / 2) {
                return std::numeric_limits<std::size_t>::max();
            }
            return 2 * elements + 16;
        };
        const std::size_t state_count = values.size();
        std::uint64_t candidate_projection = 0;
        const auto add_projection = [&](const std::uint64_t value) {
            candidate_projection = saturated_add(
                candidate_projection, value);
        };
        /* Candidate construction coexists with the source vectors and with
         * resource-stop saved scratch. Reserve a conservative two-times
         * vector-growth authority before allocating any candidate member;
         * the exact selected walk below is the commit authority. */
        add_projection(saturated_product(
            vector_capacity_upper(state_count), sizeof(double)));
        add_projection(saturated_product(
            vector_capacity_upper(
                std::max(state_count, selected_rows.size())),
            sizeof(std::uint64_t)));
        add_projection(saturated_product(
            vector_capacity_upper(
                std::max(state_count, frontier_operators.size())),
            sizeof(std::uint32_t)));
        add_projection(saturated_product(
            vector_capacity_upper(state_count),
            sizeof(PolicyOperatorRef)));
        add_projection(saturated_product(
            vector_capacity_upper(state_count), sizeof(double)));
        add_projection(saturated_product(
            vector_capacity_upper(state_count),
            sizeof(BoundedPolicyIncumbent::ChoiceSource)));
        const std::uint64_t choice_capacity_elements = saturated_add(
            saturated_product(
                transition_cache->choice_options.size(), 2),
            saturated_product(state_count, 16));
        add_projection(
            choice_capacity_elements >
                    std::numeric_limits<std::size_t>::max()
                ? std::numeric_limits<std::uint64_t>::max()
                : saturated_product(
                      static_cast<std::size_t>(choice_capacity_elements),
                      sizeof(OutcomeChoiceOption)));
        add_projection(saturated_product(
            vector_capacity_upper(
                result.behavioral_representative_by_state.size()),
            sizeof(std::uint32_t)));
        if (policy_reachable != nullptr) {
            add_projection(saturated_product(
                vector_capacity_upper(policy_reachable->size()),
                sizeof(std::uint8_t)));
        }
        if (primitive_renewal_witness != nullptr) {
            add_projection(saturated_product(
                vector_capacity_upper(
                    primitive_renewal_witness->kernel_signature.size()),
                sizeof(std::uint64_t)));
        }
        add_projection(doubled(
            static_cast<std::uint64_t>(kind.size() + 1)));
        const auto candidate_bytes_fit = [&](const std::uint64_t added) {
            const std::uint64_t current = fast_estimated_owned_bytes();
            const std::uint64_t projected = saturated_add(current, added);
            if (projected <= options.max_solver_owned_bytes) {
                peak_owned_bytes = std::max(peak_owned_bytes, projected);
                return true;
            }
            (void)check_solver_byte_cap_fast(added);
            return false;
        };
        if (record_memory_refusal &&
            !candidate_bytes_fit(candidate_projection)) {
            return;
        }
        BoundedPolicyIncumbent candidate;
        if (record_memory_refusal) {
            candidate.values.reserve(state_count);
            candidate.policy_rows.reserve(
                std::max(state_count, selected_rows.size()));
            candidate.frontier_operators.reserve(
                std::max(state_count, frontier_operators.size()));
            candidate.behavioral_representative_by_state.reserve(
                result.behavioral_representative_by_state.size());
            if (policy_reachable != nullptr) {
                candidate.policy_reachable.reserve(
                    policy_reachable->size());
            }
            candidate.policy.reserve(state_count);
            candidate.policy_row_costs.reserve(state_count);
            candidate.choice_sources.reserve(state_count);
        }
        candidate.certified_upper_bound = upper;
        candidate.evaluated_policy_cost = upper;
        if (record_memory_refusal) {
            candidate.values.insert(
                candidate.values.end(), values.begin(), values.end());
        } else {
            candidate.values = values;
        }
        candidate.values[result.start_state] = upper;
        if (record_memory_refusal) {
            candidate.policy_rows.insert(
                candidate.policy_rows.end(),
                selected_rows.begin(), selected_rows.end());
        } else {
            candidate.policy_rows = selected_rows;
        }
        candidate.policy_rows.resize(
            candidate.values.size(),
            std::numeric_limits<std::uint64_t>::max());
        if (record_memory_refusal) {
            candidate.frontier_operators.insert(
                candidate.frontier_operators.end(),
                frontier_operators.begin(), frontier_operators.end());
        } else {
            candidate.frontier_operators = frontier_operators;
        }
        candidate.frontier_operators.resize(candidate.values.size(), kNoId);
        candidate.fallback = fallback;
        candidate.restart_operator = restart_operator_index;
        candidate.restart_state = restart_state;
        candidate.fallback_anchor_state =
            fallback ? fallback->anchor_state : kNoId;
        candidate.round = result.diagnostics.focused_expansion_rounds;
        candidate.kind = std::move(kind);
        candidate.goal_identity = goal_identity();
        candidate.economy_identity = economy_identity();
        candidate.action_vocabulary_identity =
            action_vocabulary_identity();
        candidate.action_vocabulary_size = operators.size();
        candidate.caller_scope_identity = caller_scope_identity();
        candidate.graph_identity = graph_identity();
        candidate.artifact_identity = artifact_identity();
        candidate.source_generation = transition_cache->rows.size();
        candidate.target_generation = calc.state_count();
        candidate.graph_row_count = transition_cache->rows.size();
        candidate.graph_priced_row_count = priced_rows.size();
        candidate.graph_successor_count =
            transition_cache->successors.size();
        candidate.graph_probability_count =
            transition_cache->probabilities.size();
        candidate.graph_choice_count = transition_cache->choices.size();
        candidate.graph_choice_successor_count =
            transition_cache->choice_successors.size();
        candidate.graph_choice_option_count =
            transition_cache->choice_options.size();
        candidate.graph_prefix_identity =
            incumbent_graph_prefix_identity(
                candidate.graph_row_count,
                candidate.graph_priced_row_count,
                candidate.graph_successor_count,
                candidate.graph_probability_count,
                candidate.graph_choice_count,
                candidate.graph_choice_successor_count,
                candidate.graph_choice_option_count);
        if (record_memory_refusal) {
            candidate.behavioral_representative_by_state.insert(
                candidate.behavioral_representative_by_state.end(),
                result.behavioral_representative_by_state.begin(),
                result.behavioral_representative_by_state.end());
        } else {
            candidate.behavioral_representative_by_state =
                result.behavioral_representative_by_state;
        }
        if (policy_reachable != nullptr) {
            if (policy_reachable->size() != candidate.values.size() ||
                !(*policy_reachable)[result.start_state]) {
                throw std::logic_error(
                    "bounded incumbent reachability witness is invalid");
            }
            if (record_memory_refusal) {
                candidate.policy_reachable.insert(
                    candidate.policy_reachable.end(),
                    policy_reachable->begin(), policy_reachable->end());
            } else {
                candidate.policy_reachable = *policy_reachable;
            }
        }
        if (primitive_renewal_witness != nullptr) {
            candidate.primitive_renewal_witness =
                *primitive_renewal_witness;
        }
        candidate.strict_state_provenance =
            result.behavioral_representative_by_state.empty();
        /* Capture graph-relative row decisions while this same-round graph
         * is still current. Preference sorting and its graph-sized nested
         * output remain deferred, but finish() must never reinterpret an
         * incumbent through a replacement graph or a repriced row variant. */
        capture_incumbent_policy(candidate);
        if (record_memory_refusal) {
            const auto capacity_exceeded = [&]<typename T>(
                const std::vector<T>& vector,
                const std::size_t requested) {
                return vector.capacity() >
                    vector_capacity_upper(requested);
            };
            std::uint64_t choice_capacity = 0;
            for (const BoundedPolicyIncumbent::ChoiceSource& source :
                 candidate.choice_sources) {
                choice_capacity = saturated_add(
                    choice_capacity, source.choices.capacity());
            }
            if (capacity_exceeded(candidate.values, state_count) ||
                capacity_exceeded(
                    candidate.policy_rows,
                    std::max(state_count, selected_rows.size())) ||
                capacity_exceeded(
                    candidate.frontier_operators,
                    std::max(state_count, frontier_operators.size())) ||
                capacity_exceeded(candidate.policy, state_count) ||
                capacity_exceeded(candidate.policy_row_costs, state_count) ||
                capacity_exceeded(candidate.choice_sources, state_count) ||
                capacity_exceeded(
                    candidate.behavioral_representative_by_state,
                    result.behavioral_representative_by_state.size()) ||
                (policy_reachable != nullptr &&
                 capacity_exceeded(
                     candidate.policy_reachable,
                     policy_reachable->size())) ||
                choice_capacity > choice_capacity_elements) {
                throw std::logic_error(
                    "resource-stop incumbent vector projection was exceeded");
            }
        }
        std::uint64_t identity = 1469598103934665603ULL;
        identity_mix(
            identity,
            std::bit_cast<std::uint64_t>(
                candidate.certified_upper_bound));
        identity_mix(identity, candidate.goal_identity);
        identity_mix(identity, candidate.economy_identity);
        identity_mix(identity, candidate.action_vocabulary_identity);
        identity_mix(identity, candidate.caller_scope_identity);
        /* The artifact owner address is an invalidation dependency, not a
         * deterministic portfolio tie-break. Schema/base/item identities
         * are stable semantic inputs and are mixed separately below. */
        identity_mix(
            identity, calc.session().data->artifact_schema_version);
        identity_mix(identity, calc.session().base_index);
        identity_mix(identity, calc.session().item_level);
        identity_mix(identity, candidate.source_generation);
        identity_mix(identity, candidate.target_generation);
        identity_mix(identity, candidate.graph_prefix_identity);
        identity_mix(
            identity, candidate.primitive_renewal_witness.witness_hash);
        if (result.start_state < candidate.policy.size()) {
            identity_mix(
                identity, candidate.policy[result.start_state].index);
            identity_mix(
                identity,
                static_cast<std::uint64_t>(
                    candidate.policy[result.start_state].kind));
        }
        identity_mix_string(identity, candidate.kind);
        candidate.portfolio_identity = identity;
        (void)certify_incumbent_for_fallback(candidate);
        const std::uint64_t candidate_owned =
            incumbent_owned_bytes(candidate);
        const std::uint64_t candidate_dynamic =
            candidate_owned >= sizeof(BoundedPolicyIncumbent)
                ? candidate_owned - sizeof(BoundedPolicyIncumbent)
                : candidate_owned;
        if (output_incumbent.has_value()) {
            const double incumbent_upper =
                output_incumbent->certified_upper_bound;
            const bool strictly_better =
                upper < incumbent_upper;
            const bool replace_equal =
                replace_equal_incumbent &&
                upper == incumbent_upper &&
                incumbent_precedes(candidate, *output_incumbent);
            const bool replace_unverified =
                prefer_materialized_over_unverified;
            /* The reachable-policy path has just proved this complete row /
             * certified-frontier composition proper. Preserve it as the
             * output candidate even when a cheaper coarse estimate is
             * already present: finalization captures that selected estimate
             * separately, while only this slot carries the diverse joint
             * policy forward to independent compilation and evaluation. */
            if (!strictly_better && !replace_equal &&
                !replace_unverified) {
                if (!candidate.compiled_artifact.strategy_json.empty()) {
                    (void)retain_certified_incumbent(
                        candidate, candidate_dynamic);
                }
                return;
            }
        }
        if (record_memory_refusal &&
            !candidate_bytes_fit(candidate_dynamic)) {
            return;
        }
        commit_output_incumbent(std::move(candidate));
    }

void SolveWork::Impl::install_fallback_output_incumbent(
        const FocusedFallbackWitness& witness) {
        if (!options.allow_economic_restart || !witness ||
            restart_operator_index == kNoId ||
            !std::isfinite(restart_cost) ||
            !std::isfinite(witness->anchor_state_value)) {
            return;
        }
        const FocusedFallbackPolicy& fallback = *witness;
        const std::size_t state_count = calc.state_count();
        std::vector<double> values(state_count, kInfinity);
        std::vector<std::uint32_t> frontier(state_count, kNoId);
        const double restart_value =
            restart_cost + fallback.anchor_state_value;
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                values[state] = 0.0;
                continue;
            }
            std::uint32_t terminal_operator = kNoId;
            const double terminal = fallback_terminal_upper(
                state, fallback, &terminal_operator);
            if (terminal_operator != kNoId && terminal <= restart_value) {
                values[state] = terminal;
                frontier[state] = terminal_operator;
            } else {
                values[state] = restart_value;
                frontier[state] = restart_operator_index;
            }
        }
        if (fallback.anchor_state < state_count) {
            values[fallback.anchor_state] = fallback.anchor_state_value;
            if (frontier[fallback.anchor_state] == kNoId) return;
        }
        const double upper = values.at(result.start_state);
        std::string kind = "constructive_fallback";
        if (result.diagnostics.progressive_fracture_status == "complete" &&
            std::isfinite(result.diagnostics.progressive_fracture_value) &&
            std::abs(
                upper -
                result.diagnostics.progressive_fracture_start_value) <=
                value_comparison_tolerance(upper)) {
            kind = "progressive_fracture";
        } else if (!result.diagnostics.destructive_renewal_action_id.empty()) {
            kind = "destructive_renewal";
        }
        install_output_incumbent(
            upper, values, {}, frontier, witness, std::move(kind));
    }

void SolveWork::Impl::install_direct_output_incumbent(
        const double upper, const std::uint64_t row) {
        if (!output_incumbent.has_value() ||
            !std::isfinite(upper) || upper < 0.0 ||
            upper >= output_incumbent->certified_upper_bound ||
            row >= priced_rows.size() ||
            row >= transition_cache->rows.size() ||
            transition_cache->rows[row].owner_state != result.start_state) {
            return;
        }
        BoundedPolicyIncumbent candidate = *output_incumbent;
        candidate.certified_upper_bound = upper;
        candidate.evaluated_policy_cost = upper;
        candidate.values[result.start_state] = upper;
        candidate.policy_rows[result.start_state] = row;
        candidate.round = result.diagnostics.focused_expansion_rounds;
        candidate.kind = "direct_executable_row";
        candidate.goal_identity = goal_identity();
        candidate.economy_identity = economy_identity();
        candidate.action_vocabulary_identity =
            action_vocabulary_identity();
        candidate.graph_identity = graph_identity();
        candidate.behavioral_representative_by_state =
            result.behavioral_representative_by_state;
        candidate.strict_state_provenance =
            result.behavioral_representative_by_state.empty();
        candidate.action_vocabulary_size = operators.size();
        candidate.caller_scope_identity = caller_scope_identity();
        candidate.artifact_identity = artifact_identity();
        candidate.source_generation = transition_cache->rows.size();
        candidate.target_generation = calc.state_count();
        candidate.portfolio_identity = 0;
        candidate.compiled_artifact = {};
        candidate.compilation_provenance.clear();
        candidate.independently_certified = false;
        candidate.independently_evaluated = false;
        candidate.proper = false;
        candidate.executable = false;
        candidate.unveil_preferences.clear();
        candidate.option_unveil_preferences.clear();
        candidate.policy_materialized = false;
        capture_incumbent_state(candidate, result.start_state, row);
        /*
         * A direct self/goal row can improve an earlier fixed-renewal
         * incumbent while selecting a different planner operator. The
         * incumbent policy remains executable, but the action-local renewal
         * witness no longer describes it; let the ordinary policy compiler
         * handle the updated row instead of publishing stale witness data.
         */
        if (candidate.primitive_renewal_witness.valid) {
            const PrimitiveRenewalWitness& witness =
                candidate.primitive_renewal_witness;
            const bool witness_matches =
                result.start_state < candidate.policy.size() &&
                witness.operator_index < calc.operators().size() &&
                candidate.policy[result.start_state].index ==
                    witness.operator_index &&
                candidate.policy[result.start_state].kind ==
                    calc.operators()[witness.operator_index].kind &&
                calc.operators()[witness.operator_index]
                        .primitive_action ==
                    witness.primitive_action;
            if (!witness_matches) {
                candidate.primitive_renewal_witness =
                    PrimitiveRenewalWitness{};
            }
        }
        commit_output_incumbent(std::move(candidate));
    }

solve_detail::JointPolicyContinuationContext
SolveWork::Impl::current_joint_policy_continuation_context(
        const ResumableJointPolicyCandidateState* const retained) const {
    solve_detail::JointPolicyContinuationContext context;
    context.goal_identity = goal_identity();
    context.economy_identity = economy_identity();
    context.caller_scope_identity = caller_scope_identity();
    context.action_vocabulary_size = operators.size();
    context.action_vocabulary_identity = retained == nullptr
        ? action_vocabulary_identity()
        : action_vocabulary_prefix_identity(
              retained->continuation.context.action_vocabulary_size);
    context.mechanics_artifact_identity = artifact_identity();
    context.exact_terminal_identity = goal_identity();
    const auto& continuation = retained == nullptr
        ? static_cast<const solve_detail::ResumableJointPolicyContinuation*>(
              nullptr)
        : &retained->continuation;
    if (continuation != nullptr) {
        context.boundary_identity = continuation->context.boundary_identity;
        context.graph_row_count = continuation->context.graph_row_count;
        context.graph_priced_row_count =
            continuation->context.graph_priced_row_count;
        context.graph_successor_count =
            continuation->context.graph_successor_count;
        context.graph_probability_count =
            continuation->context.graph_probability_count;
        context.graph_choice_count =
            continuation->context.graph_choice_count;
        context.graph_choice_successor_count =
            continuation->context.graph_choice_successor_count;
        context.graph_choice_option_count =
            continuation->context.graph_choice_option_count;
    } else {
        std::uint64_t boundary = 1469598103934665603ULL;
        identity_mix_string(boundary, "joint_policy_boundary_snapshot_v1");
        if (output_incumbent.has_value()) {
            identity_mix(boundary, output_incumbent->portfolio_identity);
            identity_mix(
                boundary,
                std::bit_cast<std::uint64_t>(
                    output_incumbent->evaluated_policy_cost));
            identity_mix(boundary, output_incumbent->values.size());
            for (const double value : output_incumbent->values) {
                identity_mix(boundary, std::bit_cast<std::uint64_t>(value));
            }
            identity_mix(
                boundary, output_incumbent->frontier_operators.size());
            for (const std::uint32_t action :
                 output_incumbent->frontier_operators) {
                identity_mix(boundary, action);
            }
        }
        context.boundary_identity = boundary;
        context.graph_row_count = transition_cache->rows.size();
        context.graph_priced_row_count = priced_rows.size();
        context.graph_successor_count = transition_cache->successors.size();
        context.graph_probability_count =
            transition_cache->probabilities.size();
        context.graph_choice_count = transition_cache->choices.size();
        context.graph_choice_successor_count =
            transition_cache->choice_successors.size();
        context.graph_choice_option_count =
            transition_cache->choice_options.size();
    }
    context.graph_prefix_identity = incumbent_graph_prefix_identity(
        context.graph_row_count, context.graph_priced_row_count,
        context.graph_successor_count, context.graph_probability_count,
        context.graph_choice_count, context.graph_choice_successor_count,
        context.graph_choice_option_count);
    context.source_generation = transition_cache->rows.size();
    context.target_generation = calc.state_count();
    /* The broad sparse graph is append-only in this solver. New rows are an
     * allowed generation advance, while action/admission removal has no
     * generation in this path and therefore remains schema generation one. */
    context.action_generation = 1;
    context.admission_generation = 1;
    context.value_snapshot_generation =
        static_cast<std::uint64_t>(sweeps) + incremental_reoptimizations;
    context.proof_snapshot_generation = incremental_anytime_policy_attempts;
    if (output_incumbent.has_value()) {
        context.incumbent_identity = output_incumbent->portfolio_identity;
        context.incumbent_exact_cost =
            output_incumbent->independently_evaluated &&
                    std::isfinite(output_incumbent->evaluated_policy_cost)
                ? output_incumbent->evaluated_policy_cost
                : output_incumbent->certified_upper_bound;
    }
    return context;
}

solve_detail::JointPolicyContinuationNode
SolveWork::Impl::joint_policy_continuation_node(
        const std::uint32_t state) const {
    solve_detail::JointPolicyContinuationNode node;
    node.locator = state;
    if (state < calc.state_count()) {
        node.semantic_key = exact_abstract_state_key(calc.state(state), 0);
    }
    return node;
}

bool SolveWork::Impl::joint_policy_row_completed(
        const std::uint64_t row) const {
    if (row >= transition_cache->rows.size() || row >= priced_rows.size() ||
        priced_rows[row].operator_index == kNoId) {
        return false;
    }
    if (transition_cache->rows[row].admitted) return true;
    return std::any_of(
        incremental_alternative_rows.begin(),
        incremental_alternative_rows.end(),
        [&](const IncrementalAlternativeRow& alternative) {
            return alternative.row_index == row;
        });
}

std::uint64_t SolveWork::Impl::select_joint_policy_seed_row(
        const std::uint32_t state,
        const std::vector<double>& selection_values) const {
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    if (state >= transition_cache->state_rows.size()) return no_row;
    const auto goal_probability = [&](const std::uint64_t row_index) {
        const SparseRow& row = transition_cache->rows.at(row_index);
        WideFloat probability{0.0};
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor < result.goal_states.size() &&
                result.goal_states[successor]) {
                probability += WideFloat{
                    transition_cache->probabilities.at(offset)};
            }
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            bool reaches_goal = false;
            for (std::uint32_t option = 0;
                 option < group.successor_count; ++option) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + option);
                reaches_goal |= successor < result.goal_states.size() &&
                    result.goal_states[successor];
            }
            if (reaches_goal) probability += WideFloat{group.probability};
        }
        return probability.value();
    };
    const auto progress_probability = [&](const std::uint64_t row_index) {
        const SparseRow& row = transition_cache->rows.at(row_index);
        const std::uint32_t owner_progress = std::popcount(
            satisfied_goal_mask_for_state(state));
        const auto advances = [&](const std::uint32_t successor) {
            return successor < result.goal_states.size() &&
                (result.goal_states[successor] ||
                 std::popcount(satisfied_goal_mask_for_state(successor)) >
                     owner_progress);
        };
        WideFloat probability{0.0};
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            if (advances(transition_cache->successors.at(offset))) {
                probability += WideFloat{
                    transition_cache->probabilities.at(offset)};
            }
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            bool advances_choice = false;
            for (std::uint32_t option = 0;
                 option < group.successor_count; ++option) {
                advances_choice |= advances(
                    transition_cache->choice_successors.at(
                        group.successor_offset + option));
            }
            if (advances_choice) probability += WideFloat{group.probability};
        }
        return probability.value();
    };
    std::uint64_t best = no_row;
    std::tuple<int, double, double, double, std::uint64_t> best_key{
        std::numeric_limits<int>::max(), kInfinity, kInfinity, kInfinity,
        no_row};
    for (const std::uint64_t row_index :
         state_row_indices(*transition_cache, state)) {
        if (!joint_policy_row_completed(row_index)) continue;
        const PricedSparseRow& priced = priced_rows[row_index];
        if (priced.operator_index >= calc.operators().size() ||
            !std::isfinite(priced.cost) || priced.cost < 0.0) {
            continue;
        }
        const double goal = goal_probability(row_index);
        const double progress = progress_probability(row_index);
        const bool restart = priced.operator_index == restart_operator_index;
        const int class_rank = progress > 0.0 ? 0 : restart ? 1 : 2;
        const double attempt_cost = progress > 0.0
            ? priced.cost / progress
            : priced.cost;
        const auto key = std::tuple{
            class_rank, attempt_cost, -goal, -progress, row_index};
        if (key < best_key) {
            best_key = key;
            best = row_index;
        }
    }
    (void)selection_values;
    return best;
}

solve_detail::JointPolicySemanticKey
SolveWork::Impl::joint_policy_row_semantic_key(
        const std::uint32_t state,
        const std::uint64_t row_index,
        const std::vector<double>& selection_values) const {
    solve_detail::JointPolicySemanticKey key{0x6a70726f775f7631ull};
    if (!joint_policy_row_completed(row_index) ||
        transition_cache->rows[row_index].owner_state != state) {
        return {};
    }
    const auto append_key = [&](const solve_detail::JointPolicySemanticKey& in) {
        key.push_back(in.size());
        key.insert(key.end(), in.begin(), in.end());
    };
    append_key(exact_abstract_state_key(calc.state(state), 0));
    const PricedSparseRow& priced = priced_rows[row_index];
    if (priced.operator_index >= calc.operators().size()) return {};
    append_key(planner_operator_semantic_key(
        calc.operators()[priced.operator_index]));
    key.push_back(std::bit_cast<std::uint64_t>(priced.cost));
    const SparseRow& row = transition_cache->rows[row_index];
    key.push_back(std::bit_cast<std::uint64_t>(row.self_probability));
    key.push_back(
        std::bit_cast<std::uint64_t>(row.embedded_self_probability));
    key.push_back(row.self_probability_embedded);
    key.push_back(row.transition_count);
    for (std::uint32_t i = 0; i < row.transition_count; ++i) {
        const std::uint64_t offset = row.transition_offset + i;
        key.push_back(std::bit_cast<std::uint64_t>(
            transition_cache->probabilities.at(offset)));
        append_key(exact_abstract_state_key(
            calc.state(transition_cache->successors.at(offset)), 0));
    }
    key.push_back(row.choice_count);
    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
        const SparseChoiceGroup& choice = transition_cache->choices.at(
            row.choice_offset + i);
        key.push_back(std::bit_cast<std::uint64_t>(choice.probability));
        key.push_back(choice.has_self);
        key.push_back(choice.successor_count);
        for (std::uint32_t option = 0;
             option < choice.successor_count; ++option) {
            append_key(exact_abstract_state_key(
                calc.state(transition_cache->choice_successors.at(
                    choice.successor_offset + option)),
                0));
        }
        const std::uint32_t selected =
            select_sparse_policy_choice_successor(
                *transition_cache, choice, state, selection_values);
        if (selected == kNoId) return {};
        append_key(exact_abstract_state_key(calc.state(selected), 0));
    }
    key.push_back(priced.choice_option_count);
    for (std::uint32_t i = 0; i < priced.choice_option_count; ++i) {
        const OutcomeChoiceOption& option = transition_cache->choice_options.at(
            priced.choice_option_offset + i);
        key.push_back(option.mod_id);
        key.push_back(option.state);
        key.push_back(option.observation_state);
        key.push_back(option.actual_state);
    }
    return key;
}

solve_detail::JointPolicyContinuationResolvedState
SolveWork::Impl::resolve_joint_policy_continuation_state(
        const solve_detail::JointPolicyContinuationNode& source,
        const ResumableJointPolicyCandidateState& retained) const {
    using Resolved = solve_detail::JointPolicyContinuationResolvedState;
    Resolved resolved;
    resolved.source = joint_policy_continuation_node(source.locator);
    if (resolved.source != source) {
        resolved.kind = Resolved::Kind::Unsupported;
        return resolved;
    }
    if (calc.is_goal_state(calc.state(source.locator))) {
        resolved.kind = Resolved::Kind::Goal;
        return resolved;
    }
    const auto& continuation = retained.continuation;
    const std::uint64_t row = select_joint_policy_seed_row(
        source.locator, continuation.selection_values);
    if (row == std::numeric_limits<std::uint64_t>::max()) {
        const bool boundary =
            source.locator < continuation.certified_boundary_values.size() &&
            source.locator < continuation.certified_frontier_actions.size() &&
            std::isfinite(
                continuation.certified_boundary_values[source.locator]) &&
            continuation.certified_boundary_values[source.locator] >= 0.0 &&
            continuation.certified_frontier_actions[source.locator] != kNoId &&
            continuation.certified_frontier_actions[source.locator] <
                calc.operators().size();
        resolved.kind = boundary
            ? Resolved::Kind::CertifiedBoundary
            : Resolved::Kind::Missing;
        return resolved;
    }
    resolved.kind = Resolved::Kind::Row;
    resolved.row_locator = row;
    resolved.row_semantic_key = joint_policy_row_semantic_key(
        source.locator, row, continuation.selection_values);
    if (resolved.row_semantic_key.empty()) {
        resolved.kind = Resolved::Kind::Unsupported;
        return resolved;
    }
    const PricedSparseRow& priced = priced_rows[row];
    resolved.action_semantic_key = planner_operator_semantic_key(
        calc.operators()[priced.operator_index]);
    const SparseRow& sparse = transition_cache->rows[row];
    for (std::uint32_t i = 0; i < sparse.transition_count; ++i) {
        const std::uint64_t offset = sparse.transition_offset + i;
        if (transition_cache->probabilities.at(offset) > 0.0) {
            resolved.successors.push_back(joint_policy_continuation_node(
                transition_cache->successors.at(offset)));
        }
    }
    for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
        const SparseChoiceGroup& choice = transition_cache->choices.at(
            sparse.choice_offset + i);
        if (!(choice.probability > 0.0)) continue;
        const std::uint32_t selected =
            select_sparse_policy_choice_successor(
                *transition_cache, choice, source.locator,
                continuation.selection_values);
        if (selected == kNoId) {
            resolved.kind = Resolved::Kind::Unsupported;
            return resolved;
        }
        resolved.observed_choices.push_back(
            {i, joint_policy_continuation_node(selected)});
    }
    return resolved;
}

solve_detail::JointPolicyContinuationRefusal
SolveWork::Impl::validate_joint_policy_continuation_decision(
        const solve_detail::JointPolicyContinuationDecision& decision,
        const ResumableJointPolicyCandidateState& retained) const {
    using Refusal = solve_detail::JointPolicyContinuationRefusal;
    if (decision.source !=
        joint_policy_continuation_node(decision.source.locator)) {
        return Refusal::StaleIdentity;
    }
    if (!joint_policy_row_completed(decision.row_locator) ||
        joint_policy_row_semantic_key(
            decision.source.locator, decision.row_locator,
            retained.continuation.selection_values) !=
                decision.row_semantic_key) {
        return Refusal::StructuralPrefixInvalid;
    }
    const PricedSparseRow& priced = priced_rows[decision.row_locator];
    if (priced.operator_index >= calc.operators().size() ||
        planner_operator_semantic_key(calc.operators()[priced.operator_index]) !=
            decision.action_semantic_key) {
        return Refusal::ActionOrRowUnavailable;
    }
    return Refusal::None;
}

bool SolveWork::Impl::capture_resumable_joint_policy_candidate(
        const std::uint32_t expected_missing_state,
        const std::vector<double>& selection_values,
        const std::vector<double>& certified_boundary_values,
        const std::vector<std::uint32_t>& certified_frontier_operators,
        const std::vector<std::uint8_t>& certified_boundary_reachable,
        const FocusedFallbackWitness& certified_fallback,
        const PrimitiveRenewalWitness& certified_renewal,
        const std::uint64_t root_estimate_provenance) {
    using Lifecycle = solve_detail::JointPolicyContinuationLifecycle;
    if (!resumable_joint_policy_continuation_enabled() ||
        resumable_joint_policy_candidate.has_value() ||
        !output_incumbent.has_value() ||
        !std::isfinite(output_incumbent->certified_upper_bound) ||
        output_incumbent->values.empty() ||
        output_incumbent->frontier_operators.empty() ||
        expected_missing_state >= calc.state_count() ||
        result.start_state >= selection_values.size()) {
        return false;
    }
    const double root_estimate = selection_values[result.start_state];
    if (!std::isfinite(root_estimate) || root_estimate < 0.0 ||
        root_estimate > output_incumbent->certified_upper_bound +
                value_comparison_tolerance(
                    output_incumbent->certified_upper_bound)) {
        return false;
    }
    ResumableJointPolicyCandidateState retained;
    retained.continuation =
        solve_detail::ResumableJointPolicyContinuation::capture(
            current_joint_policy_continuation_context(),
            joint_policy_continuation_node(result.start_state),
            selection_values, certified_boundary_values,
            certified_frontier_operators, root_estimate,
            root_estimate_provenance, calc.state_count());
    retained.certified_fallback = certified_fallback;
    retained.certified_renewal = certified_renewal;
    retained.certified_boundary_reachable = certified_boundary_reachable;
    retained.carrier_epochs_at_capture = incremental_carrier_ladder_epochs;
    retained.refinement_rounds_at_capture = incremental_refinement_rounds;
    retained.sweeps_at_capture = sweeps;
    retained.carrier_epochs_before_last_resume =
        incremental_carrier_ladder_epochs;
    retained.refinement_rounds_before_last_resume =
        incremental_refinement_rounds;
    retained.sweeps_before_last_resume = sweeps;
    retained.refresh_owned_payload_bytes();
    resumable_joint_policy_candidate.emplace(std::move(retained));
    ResumableJointPolicyCandidateState& candidate =
        *resumable_joint_policy_candidate;
    const auto advanced = candidate.continuation.advance(
        [&](const solve_detail::JointPolicyContinuationNode& state) {
            return resolve_joint_policy_continuation_state(state, candidate);
        },
        [&](const solve_detail::JointPolicyContinuationDecision& decision) {
            return validate_joint_policy_continuation_decision(
                decision, candidate);
        },
        options.carrier_ladder_exact_boundary_limits.max_exact_work);
    candidate.refresh_owned_payload_bytes();
    const bool captured_expected =
        advanced.lifecycle ==
            Lifecycle::WaitingForExactContinuation &&
        candidate.continuation.missing ==
            joint_policy_continuation_node(expected_missing_state) &&
        candidate.current_owned_payload_bytes <=
            options.carrier_ladder_exact_boundary_limits.max_owned_bytes;
    if (!captured_expected) {
        resumable_joint_policy_candidate.reset();
        return false;
    }
    return true;
}

SolveWork::Impl::ResumableJointPolicyAdvance
SolveWork::Impl::resume_joint_policy_candidate_if_ready() {
    using Lifecycle = solve_detail::JointPolicyContinuationLifecycle;
    using Refusal = solve_detail::JointPolicyContinuationRefusal;
    if (!resumable_joint_policy_continuation_enabled() ||
        !resumable_joint_policy_candidate.has_value()) {
        return ResumableJointPolicyAdvance::NoProgress;
    }
    ResumableJointPolicyCandidateState& retained =
        *resumable_joint_policy_candidate;
    if (retained.continuation.lifecycle == Lifecycle::CompleteCandidate) {
        return ResumableJointPolicyAdvance::Complete;
    }
    if (retained.continuation.lifecycle == Lifecycle::Refused) {
        return ResumableJointPolicyAdvance::Released;
    }
    const Refusal incompatible = retained.continuation.compatible_with(
        current_joint_policy_continuation_context(&retained));
    if (incompatible != Refusal::None) {
        retained.continuation.release(incompatible);
        return ResumableJointPolicyAdvance::Released;
    }
    if (retained.continuation.lifecycle !=
        Lifecycle::WaitingForExactContinuation) {
        return ResumableJointPolicyAdvance::NoProgress;
    }
    if (!retained.continuation.extend_state_capacity(calc.state_count())) {
        retained.continuation.release(Refusal::StaleGeneration);
        return ResumableJointPolicyAdvance::Released;
    }
    retained.refresh_owned_payload_bytes();
    if (retained.current_owned_payload_bytes >
        options.carrier_ladder_exact_boundary_limits.max_owned_bytes) {
        retained.continuation.release(Refusal::ResourceInterrupted);
        return ResumableJointPolicyAdvance::Released;
    }
    const std::uint32_t missing = retained.continuation.missing.locator;
    if (select_joint_policy_seed_row(
            missing, retained.continuation.selection_values) ==
        std::numeric_limits<std::uint64_t>::max()) {
        return ResumableJointPolicyAdvance::NoProgress;
    }
    retained.ordinary_interleave_events +=
        incremental_carrier_ladder_epochs -
            retained.carrier_epochs_before_last_resume +
        incremental_refinement_rounds -
            retained.refinement_rounds_before_last_resume +
        sweeps - retained.sweeps_before_last_resume;
    if (!retained.continuation.mark_resumable(
            joint_policy_continuation_node(missing))) {
        retained.continuation.release(Refusal::ActionOrRowUnavailable);
        return ResumableJointPolicyAdvance::Released;
    }
    const auto advanced = retained.continuation.advance(
        [&](const solve_detail::JointPolicyContinuationNode& state) {
            return resolve_joint_policy_continuation_state(state, retained);
        },
        [&](const solve_detail::JointPolicyContinuationDecision& decision) {
            return validate_joint_policy_continuation_decision(
                decision, retained);
        },
        options.carrier_ladder_exact_boundary_limits.max_exact_work);
    retained.refresh_owned_payload_bytes();
    retained.carrier_epochs_before_last_resume =
        incremental_carrier_ladder_epochs;
    retained.refinement_rounds_before_last_resume =
        incremental_refinement_rounds;
    retained.sweeps_before_last_resume = sweeps;
    if (retained.current_owned_payload_bytes >
        options.carrier_ladder_exact_boundary_limits.max_owned_bytes) {
        retained.continuation.release(Refusal::ResourceInterrupted);
        return ResumableJointPolicyAdvance::Released;
    }
    if (advanced.lifecycle == Lifecycle::WaitingForExactContinuation) {
        const std::uint32_t next = retained.continuation.missing.locator;
        if (std::find(
                incremental_anytime_missing_frontier_states.begin(),
                incremental_anytime_missing_frontier_states.end(), next) ==
            incremental_anytime_missing_frontier_states.end()) {
            incremental_anytime_missing_frontier_states.push_back(next);
            ++incremental_missing_frontier_discovered;
            incremental_missing_frontier_max_open = std::max<std::uint64_t>(
                incremental_missing_frontier_max_open,
                incremental_anytime_missing_frontier_states.size());
        }
        return ResumableJointPolicyAdvance::Yielded;
    }
    if (advanced.lifecycle == Lifecycle::CompleteCandidate) {
        return ResumableJointPolicyAdvance::Complete;
    }
    return ResumableJointPolicyAdvance::Released;
}

bool SolveWork::Impl::try_install_reachable_incumbent(
        const bool require_resource_stop,
        const JointAnytimeAttemptLineage::Trigger trigger) {
        std::optional<std::size_t> lineage_index;
        const auto record_attempt_failure = [&](std::string failure) {
            if (lineage_index.has_value()) {
                JointAnytimeAttemptLineage& lineage =
                    joint_anytime_attempt_lineage.at(*lineage_index);
                lineage.failure = failure;
                lineage.portfolio_decision =
                    "not_installed_for_finalization";
                lineage.portfolio_reason = failure;
            }
            if (!require_resource_stop) {
                incremental_anytime_policy_last_failure =
                    std::move(failure);
            }
        };
        if ((require_resource_stop &&
             !result.diagnostics.resource_cap_hit) ||
            transition_cache == nullptr ||
            result.start_state == kNoId ||
            result.start_state >= calc.state_count() ||
            transition_cache->rows.empty() || priced_rows.empty()) {
            record_attempt_failure("precondition_not_satisfied");
            return false;
        }

        ResumableJointPolicyAdvance continuation_advance =
            ResumableJointPolicyAdvance::NoProgress;
        if (!require_resource_stop) {
            continuation_advance = resume_joint_policy_candidate_if_ready();
            if (continuation_advance ==
                    ResumableJointPolicyAdvance::Yielded) {
                record_attempt_failure(
                    "resumable_candidate_waiting_for_next_continuation");
                return false;
            }
            if (continuation_advance ==
                    ResumableJointPolicyAdvance::Released) {
                if (resumable_joint_policy_candidate.has_value()) {
                    resumable_joint_policy_candidate
                        ->release_owned_payload();
                }
                record_attempt_failure(
                    "resumable_candidate_released");
            }
        }
        const bool retained_candidate_ready =
            continuation_advance == ResumableJointPolicyAdvance::Complete &&
            resumable_joint_policy_candidate.has_value() &&
            !resumable_joint_policy_candidate
                 ->handed_off_for_evaluation;

        const std::size_t state_count = calc.state_count();
        if (options.carrier_ladder_exact_boundary_mode !=
            CarrierLadderExactBoundaryMode::Off) {
            JointAnytimeAttemptLineage lineage;
            lineage.ordinal = joint_anytime_attempt_lineage.size() + 1;
            lineage.trigger = trigger;
            lineage.source_generation = transition_cache->rows.size();
            lineage.target_generation = state_count;
            lineage.requested_bounded_finish_at_attempt =
                requested_bounded_finish;
            if (output_incumbent.has_value()) {
                lineage.incumbent_identity_before =
                    output_incumbent->portfolio_identity;
                lineage.incumbent_estimate_before =
                    output_incumbent->certified_upper_bound;
                lineage.incumbent_independently_evaluated_before =
                    output_incumbent->independently_evaluated;
                if (output_incumbent->independently_evaluated) {
                    lineage.incumbent_exact_cost_before =
                        output_incumbent->evaluated_policy_cost;
                }
            }
            joint_anytime_attempt_lineage.push_back(std::move(lineage));
            lineage_index = joint_anytime_attempt_lineage.size() - 1;
        }
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        /* A completed staged row does not need every stochastic successor to
         * have another materialized row. The currently selected output is
         * already an independently executable continuation. Treat its
         * captured frontier actions (and, while still available, its exact
         * primitive-renewal witness) as fixed boundary conditions for this
         * candidate proof. The shared fixed-policy evaluator then proves the
         * materialized prefix against those certified terminal values. */
        std::vector<double> certified_boundary_values;
        std::vector<std::uint32_t> certified_frontier_operators;
        std::vector<std::uint8_t> certified_boundary_reachable;
        FocusedFallbackWitness certified_fallback;
        PrimitiveRenewalWitness certified_renewal;
        if (retained_candidate_ready) {
            const ResumableJointPolicyCandidateState& retained =
                *resumable_joint_policy_candidate;
            certified_boundary_values =
                retained.continuation.certified_boundary_values;
            certified_boundary_values.resize(state_count, kInfinity);
            certified_frontier_operators =
                retained.continuation.certified_frontier_actions;
            certified_frontier_operators.resize(state_count, kNoId);
            certified_boundary_reachable =
                retained.certified_boundary_reachable;
            certified_boundary_reachable.resize(state_count, 0);
            certified_fallback = retained.certified_fallback;
            certified_renewal = retained.certified_renewal;
        } else if (output_incumbent.has_value()) {
            certified_boundary_values = output_incumbent->values;
            certified_boundary_values.resize(state_count, kInfinity);
            certified_frontier_operators =
                output_incumbent->frontier_operators;
            certified_frontier_operators.resize(state_count, kNoId);
            certified_boundary_reachable =
                output_incumbent->policy_reachable;
            certified_boundary_reachable.resize(state_count, 0);
            certified_fallback = output_incumbent->fallback;
            certified_renewal =
                output_incumbent->primitive_renewal_witness;
        }
        const std::uint64_t predicted_scratch =
            static_cast<std::uint64_t>(state_count) *
                (3 * sizeof(double) + 2 * sizeof(std::uint64_t) +
                 6 * sizeof(std::uint8_t) + 3 * sizeof(std::uint32_t)) +
            static_cast<std::uint64_t>(transition_cache->rows.size()) *
                (sizeof(std::uint8_t) + sizeof(std::uint64_t));
        if (check_solver_byte_cap_fast(predicted_scratch)) {
            record_attempt_failure("entry_scratch_exceeds_byte_cap");
            return false;
        }

        std::vector<std::uint8_t> completed(
            transition_cache->rows.size(), 0);
        std::vector<std::uint64_t> temporarily_admitted;
        std::uint64_t ordinary_admitted_rows = 0;
        for (std::uint64_t row = 0;
             row < transition_cache->rows.size(); ++row) {
            completed[row] = transition_cache->rows[row].admitted ? 1 : 0;
            ordinary_admitted_rows += completed[row] ? 1 : 0;
        }
        /* An IncrementalAlternativeRow is appended only after its exact row
         * has been fully materialized. PendingValues/Unresolved describe its
         * optimization classification, not an incomplete kernel. Such a row
         * is therefore legal evidence for an executable upper even while it
         * remains outside the admitted minimization problem. */
        for (const IncrementalAlternativeRow& alternative :
             incremental_alternative_rows) {
            if (alternative.row_index >= completed.size() ||
                alternative.row_index >= priced_rows.size()) {
                continue;
            }
            completed[alternative.row_index] = 1;
            if (!transition_cache->rows[alternative.row_index].admitted) {
                transition_cache->rows[alternative.row_index].admitted = true;
                temporarily_admitted.push_back(alternative.row_index);
            }
        }
        if (lineage_index.has_value()) {
            const auto diagnostic_started =
                std::chrono::steady_clock::now();
            JointAnytimeAttemptLineage& lineage =
                joint_anytime_attempt_lineage.at(*lineage_index);
            lineage.ordinary_admitted_rows = ordinary_admitted_rows;
            std::uint64_t completed_identity = 1469598103934665603ULL;
            identity_mix_string(
                completed_identity,
                "joint_anytime_completed_row_set_v1");
            for (std::uint64_t row = 0; row < completed.size(); ++row) {
                if (!completed[row]) continue;
                ++lineage.completed_row_count;
                identity_mix(completed_identity, row);
                identity_mix(
                    completed_identity,
                    transition_cache->rows[row].owner_state);
                identity_mix(
                    completed_identity,
                    row < priced_rows.size()
                        ? priced_rows[row].operator_index
                        : static_cast<std::uint64_t>(kNoId));
            }
            lineage.completed_row_identity = completed_identity;
            std::uint64_t alternative_identity = 1469598103934665603ULL;
            identity_mix_string(
                alternative_identity,
                "joint_anytime_completed_alternative_rows_v1");
            for (const IncrementalAlternativeRow& alternative :
                 incremental_alternative_rows) {
                if (alternative.row_index >= completed.size() ||
                    alternative.row_index >= priced_rows.size()) {
                    continue;
                }
                ++lineage.completed_alternative_rows;
                identity_mix(alternative_identity, alternative.row_index);
                identity_mix(alternative_identity, alternative.operator_index);
            }
            lineage.alternative_row_identity = alternative_identity;
            if (*lineage_index != 0) {
                const JointAnytimeAttemptLineage& previous =
                    joint_anytime_attempt_lineage.at(*lineage_index - 1);
                lineage.completed_rows_monotone =
                    lineage.completed_row_count >=
                    previous.completed_row_count;
                if (lineage.completed_rows_monotone) {
                    lineage.completed_row_delta =
                        lineage.completed_row_count -
                        previous.completed_row_count;
                }
                if (lineage.completed_alternative_rows >=
                    previous.completed_alternative_rows) {
                    lineage.alternative_row_delta =
                        lineage.completed_alternative_rows -
                        previous.completed_alternative_rows;
                }
            } else {
                lineage.completed_row_delta =
                    lineage.completed_row_count;
                lineage.alternative_row_delta =
                    lineage.completed_alternative_rows;
            }
            lineage.terminal_sees_all_completed_alternative_rows =
                trigger == JointAnytimeAttemptLineage::Trigger::
                               TerminalPublication &&
                lineage.completed_alternative_rows ==
                    incremental_alternative_rows.size();
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - diagnostic_started);
            const std::uint64_t amount = static_cast<std::uint64_t>(
                std::max<std::int64_t>(0, elapsed.count()));
            carrier_ladder_exact_boundary_private_wall_ns = amount >
                    std::numeric_limits<std::uint64_t>::max() -
                        carrier_ladder_exact_boundary_private_wall_ns
                ? std::numeric_limits<std::uint64_t>::max()
                : carrier_ladder_exact_boundary_private_wall_ns + amount;
        }

        std::vector<double> saved_values = std::move(result.values);
        std::vector<std::uint8_t> saved_expanded =
            std::move(result.expanded);
        std::vector<std::uint64_t> saved_policy_rows =
            std::move(policy_rows);
        std::vector<std::uint32_t> saved_improper_policy_states =
            std::move(improper_policy_states);
        const std::string saved_policy_evaluation_failure =
            result.diagnostics.policy_evaluation_failure;
        const bool saved_focused_lower_mode = focused_lower_mode;
        const bool saved_policy_evaluation_incomplete =
            policy_evaluation_incomplete;

        std::vector<std::uint8_t> reachable(state_count, 0);
        std::vector<std::uint8_t> materialized(state_count, 0);
        std::vector<std::uint8_t> prior_reachable;
        std::vector<std::uint32_t> walk;
        const auto refresh_scratch_bytes = [&] {
            anytime_policy_scratch_bytes = 1;
            const auto add = [&](const std::uint64_t value) {
                anytime_policy_scratch_bytes =
                    value > std::numeric_limits<std::uint64_t>::max() -
                                    anytime_policy_scratch_bytes
                        ? std::numeric_limits<std::uint64_t>::max()
                        : anytime_policy_scratch_bytes + value;
            };
            add(saved_values.capacity() * sizeof(double));
            add(saved_expanded.capacity() * sizeof(std::uint8_t));
            add(saved_policy_rows.capacity() * sizeof(std::uint64_t));
            add(saved_improper_policy_states.capacity() *
                sizeof(std::uint32_t));
            add(saved_policy_evaluation_failure.capacity() + 1);
            add(completed.capacity() * sizeof(std::uint8_t));
            add(temporarily_admitted.capacity() * sizeof(std::uint64_t));
            add(certified_boundary_values.capacity() * sizeof(double));
            add(certified_frontier_operators.capacity() *
                sizeof(std::uint32_t));
            add(certified_boundary_reachable.capacity() *
                sizeof(std::uint8_t));
            add(certified_renewal.kernel_signature.capacity() *
                sizeof(std::uint64_t));
            add(reachable.capacity() * sizeof(std::uint8_t));
            add(materialized.capacity() * sizeof(std::uint8_t));
            add(prior_reachable.capacity() * sizeof(std::uint8_t));
            add(walk.capacity() * sizeof(std::uint32_t));
        };
        refresh_scratch_bytes();

        bool installed = false;
        std::string attempt_failure;
        std::uint64_t certified_frontier_uses = 0;
        std::uint64_t renewal_boundary_attempts = 0;
        std::uint64_t renewal_boundary_successes = 0;
        const auto restore = [&](const bool preserve_success_diagnostic) {
            reset_policy_iteration_units();
            result.values = std::move(saved_values);
            result.expanded = std::move(saved_expanded);
            policy_rows = std::move(saved_policy_rows);
            improper_policy_states =
                std::move(saved_improper_policy_states);
            focused_lower_mode = saved_focused_lower_mode;
            policy_evaluation_incomplete =
                saved_policy_evaluation_incomplete;
            if (!preserve_success_diagnostic) {
                result.diagnostics.policy_evaluation_failure =
                    saved_policy_evaluation_failure;
            }
            for (const std::uint64_t row : temporarily_admitted) {
                transition_cache->rows.at(row).admitted = false;
            }
            anytime_policy_scratch_bytes = 0;
        };

        try {
            result.values = retained_candidate_ready
                ? resumable_joint_policy_candidate
                      ->continuation.selection_values
                : certified_boundary_values.empty()
                      ? saved_values
                      : certified_boundary_values;
            result.values.resize(state_count, kValueCeiling);
            result.expanded.assign(state_count, 0);
            policy_rows.assign(state_count, no_row);
            if (retained_candidate_ready) {
                for (const auto& decision :
                     resumable_joint_policy_candidate
                         ->continuation.fixed_decisions) {
                    if (decision.source.locator < policy_rows.size()) {
                        policy_rows[decision.source.locator] =
                            decision.row_locator;
                    }
                }
                resumable_joint_policy_candidate
                    ->handed_off_for_evaluation = true;
                ++resumable_joint_policy_candidate->handoff_count;
            }
            if (result.goal_states.size() < state_count) {
                result.goal_states.resize(state_count, 0);
                for (std::uint32_t state = 0; state < state_count; ++state) {
                    result.goal_states[state] =
                        calc.is_goal_state(calc.state(state)) ? 1 : 0;
                }
            }
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.goal_states[state]) {
                    result.values[state] = 0.0;
                } else if (!std::isfinite(result.values[state]) ||
                           result.values[state] < 0.0) {
                    result.values[state] = kValueCeiling;
                }
            }
            /* In the fixed-policy evaluator this mode means non-expanded
             * successors retain their supplied boundary value. These are
             * upper-policy terminals here, not optimistic lower estimates. */
            focused_lower_mode = !certified_boundary_values.empty();
            reset_policy_iteration_units();

            const auto install_certified_renewal_boundary =
                [&](const std::uint32_t state) {
                    ++renewal_boundary_attempts;
                    if (!certified_renewal.valid ||
                        state >= certified_boundary_values.size() ||
                        state >= certified_frontier_operators.size() ||
                        certified_renewal.operator_index >=
                            calc.operators().size()) {
                        return false;
                    }
                    const PlannerOperator& planner = calc.operators().at(
                        certified_renewal.operator_index);
                    if (planner.kind != PlannerOperatorKind::Primitive ||
                        planner.primitive_action !=
                            certified_renewal.primitive_action ||
                        planner.primitive_action >=
                            calc.registry().actions.size() ||
                        !action_legal(
                            session,
                            calc.registry().actions.at(
                                planner.primitive_action),
                            calc.state(state))) {
                        return false;
                    }
                    std::vector<std::uint64_t> signature;
                    if (!calc.exact_reforge_kernel_signature(
                            state, planner.primitive_action, signature) ||
                        signature != certified_renewal.kernel_signature) {
                        return false;
                    }
                    certified_boundary_values[state] =
                        certified_renewal.value;
                    certified_frontier_operators[state] =
                        certified_renewal.operator_index;
                    result.values[state] = certified_renewal.value;
                    ++renewal_boundary_successes;
                    return true;
                };

            const auto has_certified_boundary =
                [&](const std::uint32_t state) {
                    if (state >= result.values.size()) return false;
                    if (state < certified_frontier_operators.size() &&
                        certified_frontier_operators[state] != kNoId &&
                        std::isfinite(result.values[state]) &&
                        result.values[state] >= 0.0 &&
                        result.values[state] < kValueCeiling) {
                        ++certified_frontier_uses;
                        return true;
                    }
                    const bool installed_renewal =
                        install_certified_renewal_boundary(state);
                    certified_frontier_uses += installed_renewal ? 1 : 0;
                    return installed_renewal;
                };

            const auto row_goal_probability =
                [&](const std::uint32_t owner,
                    const std::uint64_t row_index) {
                    const SparseRow& row =
                        transition_cache->rows.at(row_index);
                    WideFloat probability{0.0};
                    for (std::uint32_t i = 0;
                         i < row.transition_count; ++i) {
                        const std::uint64_t offset =
                            row.transition_offset + i;
                        const std::uint32_t successor =
                            transition_cache->successors.at(offset);
                        if (successor < result.goal_states.size() &&
                            result.goal_states[successor]) {
                            probability += WideFloat{
                                transition_cache->probabilities.at(offset)};
                        }
                    }
                    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                        const SparseChoiceGroup& group =
                            transition_cache->choices.at(
                                row.choice_offset + i);
                        bool can_choose_goal = false;
                        for (std::uint32_t option = 0;
                             option < group.successor_count; ++option) {
                            const std::uint32_t successor =
                                transition_cache->choice_successors.at(
                                    group.successor_offset + option);
                            can_choose_goal |=
                                successor < result.goal_states.size() &&
                                result.goal_states[successor];
                        }
                        if (can_choose_goal) {
                            probability += WideFloat{group.probability};
                        }
                    }
                    (void)owner;
                    return probability.value();
                };

            const auto row_progress_probability =
                [&](const std::uint32_t owner,
                    const std::uint64_t row_index) {
                    const SparseRow& row =
                        transition_cache->rows.at(row_index);
                    const std::uint32_t owner_progress = std::popcount(
                        satisfied_goal_mask_for_state(owner));
                    const auto advances = [&](const std::uint32_t successor) {
                        return successor < result.goal_states.size() &&
                            (result.goal_states[successor] ||
                             std::popcount(
                                 satisfied_goal_mask_for_state(successor)) >
                                 owner_progress);
                    };
                    WideFloat probability{0.0};
                    for (std::uint32_t i = 0;
                         i < row.transition_count; ++i) {
                        const std::uint64_t offset =
                            row.transition_offset + i;
                        if (advances(
                                transition_cache->successors.at(offset))) {
                            probability += WideFloat{
                                transition_cache->probabilities.at(offset)};
                        }
                    }
                    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                        const SparseChoiceGroup& group =
                            transition_cache->choices.at(
                                row.choice_offset + i);
                        bool can_choose_progress = false;
                        for (std::uint32_t option = 0;
                             option < group.successor_count; ++option) {
                            can_choose_progress |= advances(
                                transition_cache->choice_successors.at(
                                    group.successor_offset + option));
                        }
                        if (can_choose_progress) {
                            probability += WideFloat{group.probability};
                        }
                    }
                    return probability.value();
                };

            const auto select_initial_row =
                [&](const std::uint32_t state) {
                    std::uint64_t best = no_row;
                    /* CalcContext may discover strict/frontier carriers after
                     * the current sparse transition epoch was materialized.
                     * Such a state has no row span yet. It is an ordinary
                     * missing-frontier condition owned by the certified
                     * boundary/grow-in-place path below, not a cache error and
                     * not evidence for a free terminal continuation. */
                    if (state >= transition_cache->state_rows.size()) {
                        return best;
                    }
                    std::tuple<int, double, double, double, std::uint64_t>
                        best_key{
                        std::numeric_limits<int>::max(), kInfinity,
                        kInfinity, kInfinity, no_row};
                    for (const std::uint64_t row_index :
                         state_row_indices(*transition_cache, state)) {
                        if (row_index >= completed.size() ||
                            !completed[row_index] ||
                            row_index >= priced_rows.size()) {
                            continue;
                        }
                        const PricedSparseRow& priced =
                            priced_rows[row_index];
                        if (priced.operator_index == kNoId ||
                            priced.operator_index >= calc.operators().size() ||
                            !std::isfinite(priced.cost) || priced.cost < 0.0) {
                            continue;
                        }
                        const double goal_probability =
                            row_goal_probability(state, row_index);
                        const double progress_probability =
                            row_progress_probability(state, row_index);
                        const bool restart =
                            priced.operator_index == restart_operator_index;
                        /* This is only a seed for the existing exact proper-
                         * policy evaluator. Prefer rows that can advance to
                         * any larger requested subset (or exact terminal),
                         * then rank them by cost per advancement. Treating
                         * only direct five-mod success as useful caused tiny-
                         * probability renewal spam to hide already-completed
                         * 0 -> ... -> 4 -> 5 carrier ladders. */
                        const int class_rank = progress_probability > 0.0
                            ? 0
                            : restart ? 1 : 2;
                        const double attempt_cost =
                            progress_probability > 0.0
                                ? priced.cost / progress_probability
                                : priced.cost;
                        const auto key = std::tuple{
                            class_rank, attempt_cost,
                            -goal_probability, -progress_probability,
                            row_index};
                        if (key < best_key) {
                            best_key = key;
                            best = row_index;
                        }
                    }
                    return best;
                };

            const auto row_is_completed = [&](const std::uint32_t state,
                                               const std::uint64_t row) {
                return row < completed.size() && completed[row] &&
                    row < priced_rows.size() &&
                    transition_cache->rows[row].owner_state == state &&
                    priced_rows[row].operator_index != kNoId;
            };

            const auto capture_failed_prefix =
                [&](const std::uint32_t target_state) {
                    if ((options.carrier_ladder_exact_boundary_mode !=
                             CarrierLadderExactBoundaryMode::Record &&
                         options.carrier_ladder_exact_boundary_mode !=
                             CarrierLadderExactBoundaryMode::Recover) ||
                        carrier_ladder_exact_boundary_capture.has_value()) {
                        return;
                    }
                    const auto& private_limits =
                        options.carrier_ladder_exact_boundary_limits;
                    CarrierLadderBoundaryCapture::RowServiceWitness
                        row_service_witness;
                    std::vector<std::uint64_t> captured_policy_rows =
                        policy_rows;
                    std::vector<std::uint8_t> captured_reachable(
                        state_count, 0);
                    std::vector<std::uint8_t> visited(state_count, 0);
                    std::vector<std::uint32_t> capture_walk{
                        result.start_state};
                    std::vector<CarrierLadderBoundaryCapture::Stop> stops;
                    std::uint64_t work = 0;
                    const auto select_row_service_witness = [&] {
                        std::uint32_t service_state = kNoId;
                        for (const auto& stop : stops) {
                            bool unresolved = stop.kind ==
                                CarrierLadderBoundaryCapture::StopKind::
                                    UnresolvedMissing;
                            if (stop.kind ==
                                CarrierLadderBoundaryCapture::StopKind::
                                    CertifiedFrontier) {
                                const bool valid_certified_continuation =
                                    output_incumbent.has_value() &&
                                    output_incumbent
                                        ->independently_certified &&
                                    output_incumbent
                                        ->independently_evaluated &&
                                    output_incumbent->proper &&
                                    output_incumbent->executable &&
                                    stop.operator_index != kNoId &&
                                    stop.operator_index <
                                        calc.operators().size() &&
                                    stop.state <
                                        output_incumbent->values.size() &&
                                    std::isfinite(
                                        output_incumbent->values[stop.state]);
                                unresolved =
                                    !valid_certified_continuation;
                            }
                            if (!unresolved) {
                                continue;
                            }
                            service_state = std::min(
                                service_state, stop.state);
                        }
                        if (service_state == kNoId) {
                            service_state = target_state;
                        }
                        std::uint32_t frontier =
                            service_state <
                                    certified_frontier_operators.size()
                                ? certified_frontier_operators[service_state]
                                : kNoId;
                        const bool valid_certified_continuation =
                            output_incumbent.has_value() &&
                            output_incumbent->independently_certified &&
                            output_incumbent->independently_evaluated &&
                            output_incumbent->proper &&
                            output_incumbent->executable &&
                            frontier != kNoId &&
                            frontier < calc.operators().size() &&
                            service_state <
                                output_incumbent->values.size() &&
                            std::isfinite(
                                output_incumbent->values[service_state]);
                        if (!valid_certified_continuation) {
                            frontier = kNoId;
                        }
                        row_service_witness =
                            capture_carrier_ladder_row_service_witness(
                                service_state, completed, saved_expanded,
                                frontier);
                    };
                    const auto refuse = [&](const char* cap) {
                        select_row_service_witness();
                        capture_carrier_ladder_exact_boundary(
                            target_state, result.values,
                            captured_policy_rows,
                            captured_reachable,
                            certified_frontier_operators,
                            std::move(stops),
                            std::move(row_service_witness), cap);
                    };
                    const auto route = [&](const std::uint32_t successor) {
                        if (successor >= state_count) return false;
                        if (!visited[successor]) {
                            capture_walk.push_back(successor);
                        }
                        return true;
                    };
                    for (std::size_t cursor = 0;
                         cursor < capture_walk.size(); ++cursor) {
                        if (++work > private_limits.max_exact_work) {
                            refuse("max_exact_work");
                            return;
                        }
                        if (capture_walk.size() >
                            private_limits.max_prefix_states) {
                            refuse("max_prefix_states");
                            return;
                        }
                        const std::uint32_t state = capture_walk[cursor];
                        if (state >= state_count) {
                            refuse("prefix_state_outside_graph");
                            return;
                        }
                        if (visited[state]) continue;
                        visited[state] = 1;
                        if (result.goal_states[state]) {
                            stops.push_back({
                                state, kNoId,
                                CarrierLadderBoundaryCapture::StopKind::
                                    GoalSuccess});
                            continue;
                        }
                        if (!row_is_completed(
                                state, captured_policy_rows[state])) {
                            captured_policy_rows[state] =
                                select_initial_row(state);
                        }
                        if (!row_is_completed(
                                state, captured_policy_rows[state])) {
                            const std::uint32_t frontier =
                                state < certified_frontier_operators.size()
                                    ? certified_frontier_operators[state]
                                    : kNoId;
                            CarrierLadderBoundaryCapture::StopKind kind =
                                CarrierLadderBoundaryCapture::StopKind::
                                    UnresolvedMissing;
                            if (state == target_state) {
                                kind = CarrierLadderBoundaryCapture::StopKind::
                                    RequestedEntry;
                            } else if (frontier != kNoId) {
                                kind = CarrierLadderBoundaryCapture::StopKind::
                                    CertifiedFrontier;
                            }
                            stops.push_back({state, frontier, kind});
                            continue;
                        }
                        captured_reachable[state] = 1;
                        const SparseRow& row = transition_cache->rows.at(
                            captured_policy_rows[state]);
                        for (std::uint32_t index = 0;
                             index < row.transition_count; ++index) {
                            const std::uint64_t offset =
                                row.transition_offset + index;
                            if (transition_cache->probabilities.at(offset) >
                                    0.0 &&
                                !route(
                                    transition_cache->successors.at(
                                        offset))) {
                                refuse("prefix_successor_outside_graph");
                                return;
                            }
                        }
                        for (std::uint32_t index = 0;
                             index < row.choice_count; ++index) {
                            const SparseChoiceGroup& choice =
                                transition_cache->choices.at(
                                    row.choice_offset + index);
                            if (!(choice.probability > 0.0)) continue;
                            const std::uint32_t selected =
                                select_sparse_policy_choice_successor(
                                    *transition_cache, choice, state,
                                    result.values);
                            if (selected == kNoId || !route(selected)) {
                                refuse("prefix_observation_routing_invalid");
                                return;
                            }
                        }
                    }
                    select_row_service_witness();
                    capture_carrier_ladder_exact_boundary(
                        target_state, result.values,
                        captured_policy_rows,
                        captured_reachable,
                        certified_frontier_operators,
                        std::move(stops),
                        std::move(row_service_witness));
                };

            const auto rebuild_reachable =
                [&](std::uint64_t& choice_identity) {
                    reachable.assign(state_count, 0);
                    materialized.assign(state_count, 0);
                    walk.clear();
                    walk.push_back(result.start_state);
                    choice_identity = 1469598103934665603ULL;
                    const auto mix = [&](const std::uint64_t value) {
                        choice_identity ^= value;
                        choice_identity *= 1099511628211ULL;
                    };
                    for (std::size_t cursor = 0;
                         cursor < walk.size(); ++cursor) {
                        const std::uint32_t state = walk[cursor];
                        if (state >= state_count) {
                            attempt_failure =
                                "reachable_state_outside_cached_closure";
                            return false;
                        }
                        if (reachable[state]) continue;
                        reachable[state] = 1;
                        if (result.goal_states[state]) continue;
                        if (!row_is_completed(state, policy_rows[state])) {
                            policy_rows[state] = select_initial_row(state);
                        }
                        if (!row_is_completed(state, policy_rows[state])) {
                            if (!has_certified_boundary(state)) {
                                if (lineage_index.has_value()) {
                                    JointAnytimeAttemptLineage& lineage =
                                        joint_anytime_attempt_lineage.at(
                                            *lineage_index);
                                    if (lineage.first_missing_state == kNoId) {
                                        lineage.first_missing_state = state;
                                        lineage.first_missing_goal_mask =
                                            satisfied_goal_mask_for_state(
                                                state);
                                    }
                                }
                                if (!require_resource_stop &&
                                    std::find(
                                        incremental_anytime_missing_frontier_states
                                            .begin(),
                                        incremental_anytime_missing_frontier_states
                                            .end(),
                                        state) ==
                                        incremental_anytime_missing_frontier_states
                                            .end()) {
                                    incremental_anytime_missing_frontier_states
                                        .push_back(state);
                                    ++incremental_missing_frontier_discovered;
                                    incremental_missing_frontier_max_open =
                                        std::max<std::uint64_t>(
                                            incremental_missing_frontier_max_open,
                                            incremental_anytime_missing_frontier_states
                                                .size());
                                }
                                attempt_failure =
                                    "missing_completed_row_and_certified_"
                                    "frontier:state=" +
                                    std::to_string(state) +
                                    ":goal_mask=" + std::to_string(
                                        satisfied_goal_mask_for_state(state)) +
                                    ":broad_expanded=" +
                                    std::to_string(
                                        state < expanded.size() &&
                                        expanded[state] ? 1 : 0) +
                                    ":is_carrier=" +
                                    std::to_string(
                                        std::find(
                                            incremental_carriers.begin(),
                                            incremental_carriers.end(),
                                            state) !=
                                        incremental_carriers.end() ? 1 : 0) +
                                    ":owner_rows=" +
                                    std::to_string(
                                        state < transition_cache
                                                    ->state_rows.size()
                                            ? transition_cache
                                                  ->state_rows[state].count
                                            : 0);
                                std::uint64_t completed_identity =
                                    1469598103934665603ULL;
                                identity_mix_string(
                                    completed_identity,
                                    "resumable_joint_policy_completed_rows_v1");
                                for (std::uint64_t completed_row = 0;
                                     completed_row < completed.size();
                                     ++completed_row) {
                                    if (completed[completed_row]) {
                                        identity_mix(
                                            completed_identity,
                                            completed_row);
                                    }
                                }
                                capture_resumable_joint_policy_candidate(
                                    state, result.values,
                                    certified_boundary_values,
                                    certified_frontier_operators,
                                    certified_boundary_reachable,
                                    certified_fallback, certified_renewal,
                                    completed_identity);
                                capture_failed_prefix(state);
                                return false;
                            }
                            policy_rows[state] = no_row;
                            mix(state);
                            mix(static_cast<std::uint64_t>(
                                certified_frontier_operators[state]));
                            continue;
                        }
                        materialized[state] = 1;
                        const std::uint64_t row_index = policy_rows[state];
                        const SparseRow& row =
                            transition_cache->rows.at(row_index);
                        mix(state);
                        mix(row_index);
                        const auto route = [&](const std::uint32_t successor) {
                            if (successor >= state_count) return false;
                            if (!reachable[successor]) {
                                walk.push_back(successor);
                            }
                            return true;
                        };
                        for (std::uint32_t i = 0;
                             i < row.transition_count; ++i) {
                            const std::uint64_t offset =
                                row.transition_offset + i;
                            if (transition_cache->probabilities.at(offset) <=
                                0.0) {
                                continue;
                            }
                            if (!route(
                                    transition_cache->successors.at(offset))) {
                                return false;
                            }
                        }
                        for (std::uint32_t i = 0;
                             i < row.choice_count; ++i) {
                            const SparseChoiceGroup& choice =
                                transition_cache->choices.at(
                                    row.choice_offset + i);
                            if (choice.probability <= 0.0) continue;
                            const std::uint32_t selected =
                                select_sparse_policy_choice_successor(
                                    *transition_cache, choice, state,
                                    result.values);
                            if (selected == kNoId) {
                                attempt_failure =
                                    "empty_observation_choice";
                                return false;
                            }
                            mix(selected);
                            if (!route(selected)) return false;
                        }
                    }
                    result.expanded = materialized;
                    refresh_scratch_bytes();
                    if (check_solver_byte_cap_fast()) {
                        attempt_failure =
                            "reachable_walk_exceeds_byte_cap";
                        return false;
                    }
                    return true;
                };

            const std::size_t maximum_rounds = std::min<std::size_t>(
                4096, state_count + transition_cache->rows.size() + 1);
            std::uint64_t prior_choice_identity = 0;
            for (std::size_t round = 0; round < maximum_rounds; ++round) {
                std::uint64_t choice_identity = 0;
                if (!rebuild_reachable(choice_identity)) break;
                prior_reachable = reachable;
                prior_choice_identity = choice_identity;
                reset_policy_iteration_units();
                bool evaluated = false;
                const std::size_t maximum_units = std::min<std::size_t>(
                    1000000,
                    std::max<std::size_t>(
                        1024, walk.size() * 32 + 1024));
                for (std::size_t unit = 0; unit < maximum_units; ++unit) {
                    if (evaluate_fixed_policy()) {
                        evaluated = true;
                        break;
                    }
                    if (!policy_evaluation_incomplete) break;
                }
                if (evaluated) {
                    if (lineage_index.has_value()) {
                        joint_anytime_attempt_lineage.at(*lineage_index)
                            .fixed_policy_proper = true;
                    }
                    std::uint64_t evaluated_choice_identity = 0;
                    if (!rebuild_reachable(evaluated_choice_identity)) break;
                    if (reachable != prior_reachable ||
                        evaluated_choice_identity != prior_choice_identity) {
                        continue;
                    }
                    /* The progress-aware row ranking is only a proper seed.
                     * Once that complete policy has exact values, run the
                     * ordinary strict row selection over the same completed
                     * joint envelope. Without this step the first feasible
                     * Fossil/Harvest ladder was published or rejected as-is,
                     * even when another completed row was much cheaper at a
                     * 3/5 or 4/5 carrier. Every replacement is re-evaluated by
                     * the same SCC proof on the next round. */
                    bool improved = false;
                    while (!advance_policy_selection(improved)) {
                        if (check_solver_byte_cap_fast()) {
                            attempt_failure =
                                "policy_selection_exceeds_byte_cap";
                            break;
                        }
                    }
                    if (!attempt_failure.empty()) break;
                    if (improved) {
                        reset_policy_iteration_units();
                        continue;
                    }
                    const double upper =
                        result.values.at(result.start_state);
                    if (lineage_index.has_value()) {
                        joint_anytime_attempt_lineage.at(*lineage_index)
                            .candidate_root_estimate = upper;
                    }
                    if (!std::isfinite(upper) || upper < 0.0 ||
                        upper >= kValueCeiling) {
                        attempt_failure = "evaluated_upper_is_invalid";
                        break;
                    }
                    result.diagnostics.policy_evaluation_failure.clear();
                    /* A certified frontier is a complete executable policy,
                     * not a scalar terminal in the compiled strategy. The
                     * joint proof may evaluate only the newly materialized
                     * prefix against its certified values, but publication
                     * must retain the frontier's whole reachable policy as
                     * well. `prior_reachable` is no longer needed for another
                     * comparison after this stable round, so reuse its
                     * accounted storage for the publication union. */
                    prior_reachable = reachable;
                    if (!certified_boundary_reachable.empty()) {
                        for (std::size_t state = 0;
                             state < prior_reachable.size(); ++state) {
                            prior_reachable[state] =
                                prior_reachable[state] ||
                                certified_boundary_reachable[state];
                        }
                    }
                    /* Goal-progress-gated renewal compresses every
                     * zero-progress outcome into a synthetic retry carrier.
                     * That carrier need not be a raw outcome in the frontier
                     * witness, but the generic strategy compiler needs the
                     * explicit retry policy node. Close only certified
                     * frontier actions here: their values and operator are
                     * already owned by the incumbent, so this adds no new
                     * optimization claim. Exact compiled evaluation remains
                     * the publication authority for the composition. */
                    for (std::size_t state = 0;
                         state < certified_boundary_reachable.size();
                         ++state) {
                        if (!certified_boundary_reachable[state] ||
                            state >= certified_frontier_operators.size()) {
                            continue;
                        }
                        const std::uint32_t operator_index =
                            certified_frontier_operators[state];
                        if (operator_index == kNoId ||
                            operator_index >= calc.operators().size()) {
                            continue;
                        }
                        const PlannerOperator& planner =
                            calc.operators()[operator_index];
                        if (planner.kind !=
                                PlannerOperatorKind::Primitive ||
                            planner.primitive_action >=
                                calc.registry().actions.size()) {
                            continue;
                        }
                        const OutcomeDistribution& distribution =
                            calc.outcomes(
                                static_cast<std::uint32_t>(state),
                                planner.primitive_action, true);
                        if (!distribution.supported ||
                            !distribution.goal_progress_gated ||
                            !(distribution.gated_retry_probability > 0.0) ||
                            distribution.gated_retry_state == kNoId ||
                            distribution.gated_retry_state >= state_count) {
                            continue;
                        }
                        std::uint32_t retry_policy_state =
                            distribution.gated_retry_state;
                        if (!result.behavioral_representative_by_state
                                 .empty()) {
                            if (retry_policy_state >=
                                result.behavioral_representative_by_state
                                    .size()) {
                                continue;
                            }
                            retry_policy_state =
                                result.behavioral_representative_by_state[
                                    retry_policy_state];
                        }
                        if (retry_policy_state == kNoId ||
                            retry_policy_state >= state_count ||
                            calc.is_goal_state(
                                calc.state(retry_policy_state))) {
                            continue;
                        }
                        prior_reachable[retry_policy_state] = 1;
                        if (certified_frontier_operators[
                                retry_policy_state] == kNoId) {
                            certified_frontier_operators[
                                retry_policy_state] = operator_index;
                        }
                    }
                    /* Sparse Bellman rows are stored on behavioral
                     * representatives. The strict publication adapter,
                     * however, expands each selected operator's complete
                     * coarse kernel before it discovers strict carriers.
                     * Close the publication witness over those projected
                     * successors as well, supplying the already-certified
                     * renewal boundary wherever the materialized prefix has
                     * no row. Without this closure a candidate can be
                     * numerically proper in the quotient yet present the
                     * strict adapter with an unselected non-goal successor. */
                    walk.clear();
                    for (std::uint32_t state = 0;
                         state < prior_reachable.size(); ++state) {
                        if (prior_reachable[state]) walk.push_back(state);
                    }
                    bool publication_complete = true;
                    for (std::size_t cursor = 0;
                         cursor < walk.size() && publication_complete;
                         ++cursor) {
                        const std::uint32_t state = walk[cursor];
                        if (calc.is_goal_state(calc.state(state))) continue;
                        std::uint32_t operator_index = kNoId;
                        if (!row_is_completed(
                                state, policy_rows[state])) {
                            /* Strict-kernel closure can expose a successor
                             * that was not reachable in the coarse selected
                             * row, while the append-only action ledger
                             * already owns complete legal rows for it. Seed
                             * that strict-only carrier from the same
                             * progress-aware authority used by the joint
                             * proof. The downstream strict evaluator remains
                             * the cost/properness authority for the enlarged
                             * composition. */
                            policy_rows[state] =
                                select_initial_row(state);
                        }
                        const bool has_materialized_policy_row =
                            row_is_completed(state, policy_rows[state]);
                        if (has_materialized_policy_row) {
                            operator_index = priced_rows[
                                policy_rows[state]].operator_index;
                        } else if (state <
                                   certified_frontier_operators.size()) {
                            operator_index =
                                certified_frontier_operators[state];
                        }
                        if (operator_index == kNoId &&
                            install_certified_renewal_boundary(state)) {
                            operator_index =
                                certified_frontier_operators[state];
                        }
                        if (operator_index == kNoId ||
                            operator_index >= calc.operators().size()) {
                            publication_complete = false;
                            if (lineage_index.has_value()) {
                                JointAnytimeAttemptLineage& lineage =
                                    joint_anytime_attempt_lineage.at(
                                        *lineage_index);
                                if (lineage.first_missing_state == kNoId) {
                                    lineage.first_missing_state = state;
                                    lineage.first_missing_goal_mask =
                                        satisfied_goal_mask_for_state(state);
                                }
                            }
                            if (!require_resource_stop &&
                                std::find(
                                    incremental_anytime_missing_frontier_states
                                        .begin(),
                                    incremental_anytime_missing_frontier_states
                                        .end(),
                                    state) ==
                                    incremental_anytime_missing_frontier_states
                                        .end()) {
                                incremental_anytime_missing_frontier_states
                                    .push_back(state);
                                ++incremental_missing_frontier_discovered;
                                incremental_missing_frontier_max_open =
                                    std::max<std::uint64_t>(
                                        incremental_missing_frontier_max_open,
                                        incremental_anytime_missing_frontier_states
                                            .size());
                            }
                            attempt_failure =
                                "publication_successor_has_no_certified_"
                                "action:state=" +
                                std::to_string(state);
                            break;
                        }
                        const PlannerOperator& planner =
                            calc.operators()[operator_index];
                        const auto retain_successor =
                            [&](std::uint32_t successor) {
                                if (successor == kNoId) successor = state;
                                if (successor >= state_count) {
                                    publication_complete = false;
                                    return;
                                }
                                if (!result
                                         .behavioral_representative_by_state
                                         .empty()) {
                                    if (successor >=
                                        result
                                            .behavioral_representative_by_state
                                            .size()) {
                                        publication_complete = false;
                                        return;
                                    }
                                    successor =
                                        result
                                            .behavioral_representative_by_state
                                                [successor];
                                }
                                if (successor == kNoId ||
                                    successor >= state_count) {
                                    publication_complete = false;
                                    return;
                                }
                                if (!prior_reachable[successor]) {
                                    prior_reachable[successor] = 1;
                                    walk.push_back(successor);
                                }
                            };
                        if (planner.kind ==
                            PlannerOperatorKind::Primitive) {
                            if (planner.primitive_action >=
                                calc.registry().actions.size()) {
                                publication_complete = false;
                                break;
                            }
                            const OutcomeDistribution& distribution =
                                calc.outcomes(
                                    state, planner.primitive_action,
                                    options.goal_progress_gated_reforges);
                            if (!distribution.supported ||
                                !distribution.applicable) {
                                publication_complete = false;
                                attempt_failure =
                                    "publication_primitive_kernel_invalid:"
                                    "state=" + std::to_string(state);
                                break;
                            }
                            for (const OutcomeEntry& exit :
                                 distribution.entries) {
                                if (exit.probability > 0.0) {
                                    retain_successor(exit.state);
                                }
                            }
                            for (const OutcomeChoiceGroup& group :
                                 distribution.choice_groups) {
                                if (!(group.probability > 0.0)) continue;
                                for (const std::uint32_t successor :
                                     group.states) {
                                    retain_successor(successor);
                                }
                            }
                            if (distribution.goal_progress_gated &&
                                distribution.gated_retry_probability > 0.0) {
                                retain_successor(
                                    distribution.gated_retry_state);
                            }
                            continue;
                        }
                        const OptionKernel& kernel =
                            calc.option_kernel(state, operator_index);
                        if (!kernel.supported || !kernel.legal ||
                            !kernel.terminates_almost_surely) {
                            publication_complete = false;
                            attempt_failure =
                                "publication_fixed_option_kernel_invalid:"
                                "state=" + std::to_string(state);
                            break;
                        }
                        for (const OutcomeEntry& exit : kernel.exits) {
                            if (exit.probability > 0.0) {
                                retain_successor(exit.state);
                            }
                        }
                        for (const OutcomeChoiceGroup& group :
                             kernel.observation_choice_groups) {
                            if (!(group.probability > 0.0)) continue;
                            for (const std::uint32_t successor :
                                 group.states) {
                                retain_successor(successor);
                            }
                        }
                    }
                    if (!publication_complete) {
                        if (attempt_failure.empty()) {
                            attempt_failure =
                                "publication_fixed_option_closure_failed";
                        }
                        break;
                    }
                    const bool had_prior_incumbent =
                        output_incumbent.has_value();
                    const std::uint64_t prior_identity =
                        had_prior_incumbent
                            ? output_incumbent->portfolio_identity
                            : 0;
                    install_output_incumbent(
                        upper, result.values, policy_rows,
                        certified_frontier_operators,
                        certified_fallback,
                        require_resource_stop
                            ? "resource_stop_reachable_proper_policy"
                            : "anytime_reachable_proper_policy",
                        &prior_reachable,
                        certified_renewal.valid
                            ? &certified_renewal
                            : nullptr,
                        true, true, true);
                    installed = output_incumbent.has_value() &&
                        (!had_prior_incumbent ||
                         output_incumbent->portfolio_identity !=
                             prior_identity);
                    if (installed && lineage_index.has_value()) {
                        JointAnytimeAttemptLineage& lineage =
                            joint_anytime_attempt_lineage.at(
                                *lineage_index);
                        lineage.installed_for_finalization = true;
                        lineage.candidate_portfolio_identity =
                            output_incumbent->portfolio_identity;
                        lineage.candidate_kind = output_incumbent->kind;
                        lineage.compilation_attempted =
                            output_incumbent->primitive_renewal_witness.valid;
                        lineage.compilation_succeeded =
                            !output_incumbent->compiled_artifact
                                 .strategy_json.empty();
                        lineage.compiled_nodes =
                            output_incumbent->compiled_artifact.nodes;
                        lineage.compiled_edges =
                            output_incumbent->compiled_artifact.edges;
                        lineage.compilation_result =
                            lineage.compilation_succeeded
                                ? "compiled_for_final_graph_evaluation"
                                : lineage.compilation_attempted
                                      ? "fallback_compilation_not_retained"
                                      : "candidate_has_no_root_primitive_"
                                        "renewal_witness";
                        lineage.portfolio_decision =
                            "installed_for_finalization";
                        lineage.portfolio_reason =
                            "reachable fixed policy proved proper";
                    }
                    if (!installed) {
                        attempt_failure =
                            "proved_candidate_not_preferred:upper=" +
                            finite_json(upper) + ":prior=" +
                            finite_json(
                                had_prior_incumbent
                                    ? output_incumbent
                                          ->certified_upper_bound
                                    : kInfinity);
                    }
                    break;
                }
                if (improper_policy_states.empty() ||
                    !repair_improper_policy()) {
                    attempt_failure =
                        result.diagnostics.policy_evaluation_failure.empty()
                            ? "fixed_policy_not_proved"
                            : result.diagnostics.policy_evaluation_failure;
                    break;
                }
            }
        } catch (const SolverResourceLimit& limit) {
            record_cap(
                limit.cap_name(),
                limit.cap_name() == "max_discovered_states");
        } catch (...) {
            restore(false);
            throw;
        }

        if (lineage_index.has_value()) {
            const auto diagnostic_started =
                std::chrono::steady_clock::now();
            JointAnytimeAttemptLineage& lineage =
                joint_anytime_attempt_lineage.at(*lineage_index);
            lineage.certified_frontier_uses = certified_frontier_uses;
            lineage.renewal_boundary_attempts = renewal_boundary_attempts;
            lineage.renewal_boundary_successes =
                renewal_boundary_successes;
            const std::vector<std::uint8_t>& selected_reachable =
                installed && !prior_reachable.empty()
                    ? prior_reachable
                    : reachable;
            std::uint64_t selection_identity = 1469598103934665603ULL;
            identity_mix_string(
                selection_identity,
                "joint_anytime_selected_reachable_policy_v1");
            const auto& limits =
                options.carrier_ladder_exact_boundary_limits;
            const std::uint64_t owned_sample_limit =
                limits.max_owned_bytes /
                std::max<std::uint64_t>(
                    1, sizeof(JointAnytimeAttemptLineage::SelectedDecision));
            const std::uint64_t sample_limit = std::min<std::uint64_t>(
                limits.max_samples, owned_sample_limit);
            for (std::uint32_t state = 0;
                 state < selected_reachable.size(); ++state) {
                if (!selected_reachable[state] ||
                    (state < result.goal_states.size() &&
                     result.goal_states[state])) {
                    continue;
                }
                const std::uint64_t row = state < policy_rows.size()
                    ? policy_rows[state]
                    : no_row;
                std::uint32_t operator_index = kNoId;
                if (row < completed.size() && completed[row] &&
                    row < priced_rows.size()) {
                    operator_index = priced_rows[row].operator_index;
                } else if (state <
                           certified_frontier_operators.size()) {
                    operator_index =
                        certified_frontier_operators[state];
                }
                ++lineage.selected_state_count;
                identity_mix(selection_identity, state);
                identity_mix(selection_identity, row);
                identity_mix(selection_identity, operator_index);
                if (lineage.selected_decisions.size() < sample_limit) {
                    lineage.selected_decisions.push_back(
                        {state, row, operator_index});
                } else {
                    ++lineage.selected_samples_omitted;
                }
            }
            lineage.selection_identity = selection_identity;
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - diagnostic_started);
            const std::uint64_t amount = static_cast<std::uint64_t>(
                std::max<std::int64_t>(0, elapsed.count()));
            carrier_ladder_exact_boundary_private_wall_ns = amount >
                    std::numeric_limits<std::uint64_t>::max() -
                        carrier_ladder_exact_boundary_private_wall_ns
                ? std::numeric_limits<std::uint64_t>::max()
                : carrier_ladder_exact_boundary_private_wall_ns + amount;
        }
        restore(installed);
        if (!installed) {
            if (attempt_failure.empty()) {
                attempt_failure = "selection_rounds_exhausted";
            }
            attempt_failure +=
                ":frontier_uses=" +
                std::to_string(certified_frontier_uses) +
                ":renewal_boundaries=" +
                std::to_string(renewal_boundary_successes) + "/" +
                std::to_string(renewal_boundary_attempts);
            if (!require_resource_stop &&
                (attempt_failure.starts_with(
                     "missing_completed_row_and_certified_frontier") ||
                 attempt_failure.starts_with(
                     "publication_successor_has_no_certified_action"))) {
                incremental_anytime_next_row_checkpoint =
                    incremental_alternative_rows.size() ==
                            std::numeric_limits<std::size_t>::max()
                        ? std::numeric_limits<std::size_t>::max()
                        : incremental_alternative_rows.size() + 1;
            }
            record_attempt_failure(std::move(attempt_failure));
        }
        if (installed) {
            retain_action_reason(
                require_resource_stop
                    ? "included:resource_stop_start_reachable_proper_policy"
                    : "included:anytime_start_reachable_proper_policy");
        }
        return installed;
    }

void SolveWork::Impl::try_install_gated_root_renewal_incumbent(
        const std::uint32_t state,
        const std::uint64_t row,
        const PricedOperator& priced,
        const OutcomeDistribution& kernel) {
        if (!options.goal_progress_gated_reforges ||
            state != result.start_state ||
            priced.index >= calc.operators().size() ||
            row >= transition_cache->rows.size() ||
            transition_cache->rows[row].owner_state != state ||
            !kernel.supported || !kernel.stable_shared_kernel ||
            !kernel.goal_progress_gated ||
            !kernel.choice_groups.empty() ||
            !kernel.choice_options.empty() ||
            !(kernel.gated_terminal_probability > 0.0) ||
            !std::isfinite(priced.cost) || priced.cost < 0.0) {
            return;
        }
        const PlannerOperator& planner =
            calc.operators().at(priced.index);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action >= calc.registry().actions.size()) {
            return;
        }
        const std::uint32_t action_index = planner.primitive_action;
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(action_index);
        if (!action_transition_facts(descriptor.params.type).renewal ||
            !action_legal(session, descriptor, calc.state(state))) {
            return;
        }

        ++result.diagnostics.gated_root_renewal_candidates;
        const auto reject = [&](const char* reason) {
            ++result.diagnostics.gated_root_renewal_rejections;
            retain_action_reason(
                "rejected:gated_root_primitive_destructive_renewal:" +
                planner.id + ":" + reason);
        };
        std::vector<std::uint64_t> kernel_signature;
        if (!calc.exact_reforge_kernel_signature(
                state, action_index, kernel_signature) ||
            kernel_signature.empty()) {
            reject("missing_exact_retry_signature");
            return;
        }

        WideFloat success_probability_mass{0.0};
        std::vector<std::uint8_t> policy_reachable(
            calc.state_count(), 0);
        policy_reachable[state] = 1;
        bool exact_retry = true;
        for (const OutcomeEntry& outcome : kernel.entries) {
            if (!(outcome.probability > 0.0) ||
                outcome.state >= calc.state_count()) {
                continue;
            }
            policy_reachable[outcome.state] = 1;
            if (calc.is_goal_state(calc.state(outcome.state))) {
                success_probability_mass +=
                    WideFloat{outcome.probability};
                continue;
            }
            std::vector<std::uint64_t> retry_signature;
            if (!action_legal(
                    session, descriptor, calc.state(outcome.state)) ||
                !calc.exact_reforge_kernel_signature(
                    outcome.state, action_index, retry_signature) ||
                retry_signature != kernel_signature) {
                exact_retry = false;
                break;
            }
        }
        const double success_probability =
            success_probability_mass.value();
        const double probability_tolerance =
            1e-12 * std::max(
                1.0, std::abs(kernel.gated_terminal_probability));
        if (!exact_retry || !(success_probability > 0.0) ||
            std::abs(
                success_probability -
                kernel.gated_terminal_probability) >
                probability_tolerance) {
            reject(
                !exact_retry
                    ? "nonterminal_retry_signature_changed"
                    : !(success_probability > 0.0)
                          ? "no_terminal_success_probability"
                          : "terminal_probability_mismatch");
            return;
        }
        const double value =
            (WideFloat{priced.cost} /
             success_probability_mass)
                .value();
        if (!std::isfinite(value) || value < 0.0 ||
            value >= kValueCeiling) {
            reject("invalid_geometric_value");
            return;
        }

        std::vector<double> values(
            policy_reachable.size(), kInfinity);
        std::vector<std::uint64_t> selected_rows(
            policy_reachable.size(),
            std::numeric_limits<std::uint64_t>::max());
        std::vector<std::uint32_t> frontier(
            policy_reachable.size(), kNoId);
        std::uint64_t validated_non_goal_states = 0;
        for (std::uint32_t candidate = 0;
             candidate < policy_reachable.size(); ++candidate) {
            if (!policy_reachable[candidate]) continue;
            if (calc.is_goal_state(calc.state(candidate))) {
                values[candidate] = 0.0;
                continue;
            }
            values[candidate] = value;
            frontier[candidate] = priced.index;
            ++validated_non_goal_states;
        }
        selected_rows[state] = row;

        PrimitiveRenewalWitness witness;
        witness.valid = true;
        witness.operator_index = priced.index;
        witness.primitive_action = action_index;
        witness.success_probability = success_probability;
        witness.value = value;
        witness.gated_kernel_bits_hash =
            kernel.gated_kernel_bits_hash;
        witness.validated_non_goal_states =
            validated_non_goal_states;
        witness.kernel_signature = kernel_signature;
        std::uint64_t witness_hash = 1469598103934665603ULL;
        identity_mix(witness_hash, priced.index);
        identity_mix(witness_hash, action_index);
        identity_mix(
            witness_hash,
            std::bit_cast<std::uint64_t>(success_probability));
        identity_mix(
            witness_hash, std::bit_cast<std::uint64_t>(value));
        identity_mix(
            witness_hash, kernel.gated_kernel_bits_hash);
        identity_mix(
            witness_hash, validated_non_goal_states);
        for (const std::uint64_t part : kernel_signature) {
            identity_mix(witness_hash, part);
        }
        witness.witness_hash = witness_hash;

        install_output_incumbent(
            value, values, selected_rows, frontier, {},
            "gated_primitive_destructive_renewal",
            &policy_reachable, &witness, true);
        result.diagnostics.focused_upper_bound = value;
        result.diagnostics.destructive_renewal_action_id =
            planner.id;
        result.diagnostics.destructive_renewal_value = value;
        result.diagnostics.destructive_renewal_anchor_value = value;
        result.diagnostics.destructive_renewal_start_value = value;
        result.diagnostics
            .gated_root_renewal_validated_non_goal_states =
            validated_non_goal_states;
        result.diagnostics.gated_root_renewal_witness_hash =
            witness_hash;
        result.diagnostics
            .gated_root_renewal_success_probability =
            success_probability;
        retain_action_reason(
            "included:gated_root_primitive_destructive_renewal:" +
            planner.id + ":success=" +
            finite_json(success_probability) + ":value=" +
            finite_json(value) + ":validated_non_goal_states=" +
            std::to_string(validated_non_goal_states));
    }

double solve_detail::globally_certified_action_envelope_lower_bound(
        const double restricted_lower_bound,
        const bool incremental_action_generation,
        const bool incremental_action_envelope_closed,
        const double independent_goal_cover_lower_bound) {
        const double independent =
            std::isfinite(independent_goal_cover_lower_bound) &&
                    independent_goal_cover_lower_bound >= 0.0
                ? independent_goal_cover_lower_bound
                : 0.0;
        /*
         * A focused solve over the currently admitted rows is a useful
         * scheduling relaxation, but removing delayed actions from a
         * minimization can only raise its optimum. Until every delayed row
         * has been evaluated and classified, that restricted optimum is not
         * a lower bound for the requested full action envelope. The
         * independently admissible goal-cover relaxation remains valid for
         * every delayed action; keep the stronger restricted value in
         * diagnostics for scheduling only until that envelope closes.
         */
        if (incremental_action_generation &&
            !incremental_action_envelope_closed) {
            return independent;
        }
        const double restricted =
            std::isfinite(restricted_lower_bound) &&
                    restricted_lower_bound >= 0.0
                ? restricted_lower_bound
                : 0.0;
        return std::max(independent, restricted);
    }

SolveLowerBoundAuthority
solve_detail::classify_public_lower_bound_authority(
        const double lower_bound,
        const SolvePolicyStatus policy_status,
        const bool incremental_action_generation,
        const bool incremental_action_envelope_closed,
        const bool unclosed_strict_refinement,
        const double independent_goal_cover_lower_bound) {
        if (!std::isfinite(lower_bound) || lower_bound < 0.0) {
            return {};
        }
        if (policy_status == SolvePolicyStatus::Exact) {
            return {
                true,
                SolveLowerBoundProvenance::ExactPolicyClosure,
            };
        }
        const double independent_tolerance = 1e-12 * std::max(
            1.0, std::fabs(independent_goal_cover_lower_bound));
        if (std::isfinite(independent_goal_cover_lower_bound) &&
            independent_goal_cover_lower_bound > 0.0 &&
            lower_bound <= independent_goal_cover_lower_bound +
                independent_tolerance) {
            return {
                true,
                SolveLowerBoundProvenance::GlobalActionRelaxation,
            };
        }
        if (unclosed_strict_refinement) {
            return {
                false,
                SolveLowerBoundProvenance::
                    UnclosedStrictRefinementUniversalZero,
            };
        }
        if (incremental_action_generation &&
            !incremental_action_envelope_closed) {
            return {
                false,
                SolveLowerBoundProvenance::
                    OpenIncrementalEnvelopeUniversalZero,
            };
        }
        if (incremental_action_generation) {
            return {
                true,
                SolveLowerBoundProvenance::
                    ClosedIncrementalActionEnvelope,
            };
        }
        return {
            true,
            SolveLowerBoundProvenance::GlobalActionRelaxation,
        };
    }

double SolveWork::Impl::certified_global_lower_bound() const {
        return globally_certified_action_envelope_lower_bound(
            result.diagnostics.focused_lower_bound,
            incremental_action_generation,
            incremental_envelope_closed,
            result.diagnostics.independent_goal_cover_lower_bound);
    }

SolveGapTarget SolveWork::Impl::satisfied_gap_target() const {
        if (!output_incumbent.has_value()) return SolveGapTarget::None;
        const double lower = certified_global_lower_bound();
        const double upper = output_incumbent->certified_upper_bound;
        if (!std::isfinite(lower) || !std::isfinite(upper) ||
            lower > upper + value_comparison_tolerance(upper)) {
            return SolveGapTarget::None;
        }
        const bool absolute = options.max_absolute_optimality_gap > 0.0 &&
            upper - lower <= options.max_absolute_optimality_gap;
        const bool relative = options.max_relative_optimality_gap > 0.0 &&
            lower > 0.0 &&
            upper <= (1.0 + options.max_relative_optimality_gap) * lower;
        if (absolute && relative) return SolveGapTarget::Both;
        if (absolute) return SolveGapTarget::Absolute;
        if (relative) return SolveGapTarget::Relative;
        return SolveGapTarget::None;
    }

bool SolveWork::Impl::stop_for_satisfied_gap_target() {
        if (result.diagnostics.resource_cap_hit ||
            result.diagnostics.state_cap_hit ||
            (expanded_count >= options.max_expanded_states &&
             calc.state_count() > expanded_count)) {
            return false;
        }
        const SolveGapTarget fired = satisfied_gap_target();
        if (fired == SolveGapTarget::None) return false;
        target_gap_stop = true;
        target_gap_fired = fired;
        queue.clear();
        focus_optimizing = false;
        focused_lower_mode = false;
        focused_upper_mode = false;
        return true;
    }

std::optional<double> SolveWork::Impl::constructive_row_upper(
        const std::uint32_t state,
        const std::uint64_t row_index) {
        if (row_index >= transition_cache->rows.size() ||
            row_index >= priced_rows.size()) {
            return std::nullopt;
        }
        if (certified_state_upper.size() < calc.state_count()) {
            certified_state_upper.resize(calc.state_count(), kInfinity);
            certified_state_row.resize(
                calc.state_count(),
                std::numeric_limits<std::uint64_t>::max());
        }
        const SparseRow& row = transition_cache->rows.at(row_index);
        const PricedSparseRow& priced = priced_rows.at(row_index);
        if (priced.operator_index == kNoId ||
            !std::isfinite(priced.cost) || priced.cost < 0.0) {
            return std::nullopt;
        }
        const auto successor_upper = [&](const std::uint32_t successor) {
            if (calc.is_goal_state(calc.state(successor))) return 0.0;
            return successor < certified_state_upper.size()
                       ? certified_state_upper[successor]
                       : kInfinity;
        };
        double constant = priced.cost;
        double loop_probability = row.self_probability;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor == state) continue;
            const double upper = successor_upper(successor);
            if (!std::isfinite(upper)) return std::nullopt;
            constant += transition_cache->probabilities.at(offset) * upper;
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            double selected = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                selected = std::min(
                    selected,
                    successor_upper(
                        transition_cache->choice_successors.at(
                            group.successor_offset + s)));
            }
            if (std::isfinite(selected)) {
                constant += group.probability * selected;
            } else if (group.has_self) {
                loop_probability += group.probability;
            } else {
                return std::nullopt;
            }
        }
        const double denominator = 1.0 - loop_probability;
        if (denominator <= 1e-15) return std::nullopt;
        const double upper = constant / denominator;
        if (!std::isfinite(upper) || upper >= kValueCeiling) {
            return std::nullopt;
        }
        return upper;
    }

bool SolveWork::Impl::try_constructive_state_certificate(
        const std::uint32_t state,
        const std::uint64_t row_index) {
        if (!options.state_certificate_control || focused_mode ||
            cache_pending) {
            return false;
        }
        const std::optional<double> candidate =
            constructive_row_upper(state, row_index);
        if (!candidate.has_value()) return false;
        const double upper = *candidate;
        const std::uint32_t selected_operator =
            priced_rows.at(row_index).operator_index;
        double strict_min_other_lower = kInfinity;
        for (const std::uint32_t other : expansion_operator_indices) {
            if (other == selected_operator) continue;
            const double lower =
                operator_proof_lower(state, other).value;
            strict_min_other_lower =
                std::min(strict_min_other_lower, lower);
            const double separation = options.epsilon *
                std::max({1.0, std::abs(upper), std::abs(lower)});
            const bool separated = std::isfinite(lower) &&
                lower > upper + separation;
            record_operator_lower_attribution(
                other, lower, upper, false, separated);
            if (!separated) {
                return false;
            }
        }
        const std::uint64_t pruned =
            expansion_operator_indices.size() -
            expansion_operator_cursor;
        if (pruned == 0) return false;
        price_bound_state_pruning = true;
        if (certified_state_upper.size() < calc.state_count()) {
            certified_state_upper.resize(calc.state_count(), kInfinity);
            certified_state_row.resize(
                calc.state_count(),
                std::numeric_limits<std::uint64_t>::max());
        }
        certified_state_upper[state] = upper;
        certified_state_row[state] = row_index;
        ++result.diagnostics.constructive_state_certificates;
        result.diagnostics.constructive_state_operators_pruned += pruned;
        if (state == result.start_state &&
            !std::isfinite(
                result.diagnostics.constructive_upper_bound)) {
            result.diagnostics.constructive_upper_bound = upper;
            result.diagnostics.constructive_upper_first_expanded_state =
                expanded_count;
        }
        if (result.diagnostics.constructive_state_witnesses.size() <
            options.max_diagnostic_samples) {
            std::string witness = "{\"state\":" +
                std::to_string(state) + ",\"operator\":";
            append_json_string(
                witness,
                calc.operators().at(selected_operator).id);
            witness += ",\"constructive_upper\":" +
                finite_json(upper) +
                ",\"strict_min_other_lower\":" +
                finite_json(strict_min_other_lower) +
                ",\"operators_pruned\":" +
                std::to_string(pruned) +
                ",\"proof\":\"optimistic_goal_production_cover\"}";
            result.diagnostics.constructive_state_witnesses.push_back(
                std::move(witness));
        } else {
            ++result.diagnostics.constructive_state_witnesses_omitted;
        }
        retain_action_reason(
            "pruned:constructive_state_certificate:" +
            std::to_string(pruned));
        expansion_operator_cursor =
            static_cast<std::uint32_t>(
                expansion_operator_indices.size());
        return true;
    }

std::pair<double, std::uint32_t> SolveWork::Impl::constructive_direct_action_upper(
        const std::uint32_t state,
        const std::uint32_t action_index) const {
        if (action_index >= calc.registry().actions.size() ||
            action_index >= priced_operator_position.size()) {
            return {kInfinity, kNoId};
        }
        /* Primitive planner wrappers deliberately share registry indices. */
        const std::int32_t priced_position =
            priced_operator_position[action_index];
        if (priced_position < 0) return {kInfinity, kNoId};
        const PricedOperator& priced =
            operators.at(static_cast<std::size_t>(priced_position));
        const PlannerOperator& planner = calc.operators().at(priced.index);
        const ActionDescriptor& action =
            calc.registry().actions.at(action_index);
        const AbstractState& carrier = calc.state(state);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action != action_index ||
            action.params.type != ActionType::Bench ||
            action.kind != TransitionKind::Deterministic ||
            action.params.mod_id >= session.mod_count ||
            action.params.mod_id >= session.metamod_type.size() ||
            session.metamod_type[action.params.mod_id] >= 0 ||
            !std::isfinite(priced.cost) || priced.cost < 0.0 ||
            !action_legal(session, action, carrier) ||
            (carrier.flags & kFlagCraftedMod) != 0 ||
            carrier.rarity != calc.goal().rarity) {
            return {kInfinity, kNoId};
        }
        std::uint32_t satisfied = satisfied_goal_mask_for_state(state);
        std::uint32_t finish_mask = 0;
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            if (pc_bitset_test(
                    calc.layout().slots[slot].satisfying_mask.data(),
                    action.params.mod_id)) {
                finish_mask |= 1u << slot;
                if (carrier.slot_status[slot] !=
                        static_cast<std::uint8_t>(GoalSlotStatus::Absent) ||
                    (carrier.blocked_mask & (1u << slot)) != 0) {
                    return {kInfinity, kNoId};
                }
            }
        }
        if (finish_mask == 0 ||
            std::popcount(satisfied | finish_mask) <
                calc.goal().required_satisfied_slots()) {
            return {kInfinity, kNoId};
        }
        const std::uint8_t cap = rarity_affix_cap(session, carrier.rarity);
        const std::int8_t side = session.gen_type[action.params.mod_id];
        if ((side == PC_SIDE_PREFIX && carrier.prefix_count >= cap) ||
            (side == PC_SIDE_SUFFIX && carrier.suffix_count >= cap) ||
            (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX)) {
            return {kInfinity, kNoId};
        }
        const std::uint32_t resulting_explicit_count =
            carrier.prefix_count + carrier.suffix_count + 1u;
        if (resulting_explicit_count !=
            std::popcount(satisfied | finish_mask)) {
            return {kInfinity, kNoId};
        }
        return {priced.cost, priced.index};
    }

bool SolveWork::Impl::renewal_fallback_eligible(
        const std::uint32_t state,
        const FocusedFallbackPolicy& fallback) const {
        if (state >= calc.state_count() ||
            calc.is_goal_state(calc.state(state)) ||
            fallback.renewal_operator == kNoId ||
            fallback.renewal_operator >= calc.operators().size()) {
            return false;
        }
        const PlannerOperator& renewal =
            calc.operators().at(fallback.renewal_operator);
        const std::uint32_t first_action =
            renewal.kind == PlannerOperatorKind::Primitive
                ? renewal.primitive_action
                : renewal.primitive_program.empty()
                      ? kNoId
                      : renewal.primitive_program.front();
        if (first_action >= calc.registry().actions.size()) return false;
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(first_action);
        if (!action_transition_facts(descriptor.params.type).renewal) {
            return false;
        }
        const AbstractState& carrier = calc.state(state);
        if (carrier.rarity != fallback.renewal_rarity ||
            carrier.influence_bits != fallback.renewal_influence_bits ||
            carrier.searing_exarch_tier !=
                fallback.renewal_searing_exarch_tier ||
            carrier.eater_of_worlds_tier !=
                fallback.renewal_eater_of_worlds_tier ||
            carrier.fractured_goal_mask != 0 ||
            carrier.fractured_metamod_flags != 0 ||
            (carrier.flags & kProtectionFlags) != 0 ||
            !action_legal(session, descriptor, carrier)) {
            return false;
        }
        if (renewal.kind == PlannerOperatorKind::Primitive) {
            if (renewal.primitive_action != first_action ||
                fallback.renewal_kernel_signature.empty()) {
                return false;
            }
            std::vector<std::uint64_t> signature;
            return calc.exact_reforge_kernel_signature(
                       state, first_action, signature) &&
                   signature == fallback.renewal_kernel_signature;
        }
        if (renewal.kind != PlannerOperatorKind::FixedOption) return false;
        for (const std::uint8_t count : carrier.fractured_junk_counts) {
            if (count != 0) return false;
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            if (count != 0) return false;
        }
        return true;
    }

bool SolveWork::Impl::primitive_renewal_mode_eligible(
        const std::uint32_t state,
        const FocusedFallbackPolicy::PrimitiveRenewalMode& mode) const {
        if (state >= calc.state_count() || mode.operator_index == kNoId ||
            mode.operator_index >= calc.operators().size() ||
            mode.kernel_signature.empty()) {
            return false;
        }
        const PlannerOperator& planner =
            calc.operators().at(mode.operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action >= calc.registry().actions.size()) {
            return false;
        }
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(planner.primitive_action);
        if (!action_transition_facts(descriptor.params.type).renewal ||
            !action_legal(session, descriptor, calc.state(state))) {
            return false;
        }
        std::vector<std::uint64_t> signature;
        return calc.exact_reforge_kernel_signature(
                   state, planner.primitive_action, signature) &&
               signature == mode.kernel_signature;
    }

double SolveWork::Impl::fallback_terminal_upper(
        const std::uint32_t state,
        const FocusedFallbackPolicy& fallback,
        std::uint32_t* selected_operator ) const {
        if (calc.is_goal_state(calc.state(state))) {
            if (selected_operator != nullptr) *selected_operator = kNoId;
            return 0.0;
        }
        double best = kInfinity;
        std::uint32_t best_operator = kNoId;
        if (state == fallback.anchor_state &&
            std::isfinite(fallback.anchor_state_value) &&
            fallback.anchor_operator != kNoId) {
            best = fallback.anchor_state_value;
            best_operator = fallback.anchor_operator;
        }
        const auto progress = fallback.progress_state_value.find(state);
        if (progress != fallback.progress_state_value.end() &&
            std::isfinite(progress->second)) {
            best = progress->second;
            const auto progress_operator =
                fallback.progress_state_operator.find(state);
            if (progress_operator !=
                fallback.progress_state_operator.end()) {
                best_operator = progress_operator->second;
            }
        }
        if (renewal_fallback_eligible(state, fallback)) {
            if (fallback.renewal_state_value < best) {
                best = fallback.renewal_state_value;
                best_operator = fallback.renewal_operator;
            }
        }
        for (const auto& mode : fallback.primitive_renewal_modes) {
            if (mode.value < best &&
                primitive_renewal_mode_eligible(state, mode)) {
                best = mode.value;
                best_operator = mode.operator_index;
            }
        }
        const auto [finish, finish_operator] =
            constructive_direct_action_upper(state, fallback.finish_action);
        if (finish < best) {
            best = finish;
            best_operator = finish_operator;
        }
        if (selected_operator != nullptr) *selected_operator = best_operator;
        return best;
    }

auto SolveWork::Impl::magic_regal_fallback() -> std::optional<FocusedFallbackPolicy> {
        if (!options.allow_economic_restart || restart_state == kNoId ||
            restart_state >= calc.state_count() ||
            !std::isfinite(restart_cost) || restart_cost < 0.0) {
            return std::nullopt;
        }
        const AbstractState& anchor = calc.state(restart_state);
        if (anchor.rarity != PC_RARITY_NORMAL || anchor.prefix_count != 0 ||
            anchor.suffix_count != 0 ||
            (anchor.flags & (kFlagCraftedMod | kProtectionFlags)) != 0 ||
            anchor.fractured_goal_mask != 0 ||
            anchor.fractured_metamod_flags != 0) {
            return std::nullopt;
        }

        const auto primitive_of_type = [&](const ActionType type) {
            for (const std::uint32_t action : calc.candidates()) {
                if (action < calc.registry().actions.size() &&
                    calc.registry().actions[action].params.type == type) {
                    return action;
                }
            }
            return kNoId;
        };
        const std::uint32_t transmute =
            primitive_of_type(ActionType::Transmute);
        const std::uint32_t alteration =
            primitive_of_type(ActionType::Alteration);
        const std::uint32_t augment =
            primitive_of_type(ActionType::Augment);
        const std::uint32_t regal = primitive_of_type(ActionType::Regal);
        const std::uint32_t exalt = primitive_of_type(ActionType::Exalt);
        if (transmute == kNoId || alteration == kNoId || regal == kNoId) {
            return std::nullopt;
        }
        const auto primitive_cost = [&](const std::uint32_t action) {
            if (action >= priced_operator_position.size()) return kInfinity;
            const std::int32_t position = priced_operator_position[action];
            if (position < 0) return kInfinity;
            const PricedOperator& priced =
                operators.at(static_cast<std::size_t>(position));
            const PlannerOperator& planner = calc.operators().at(priced.index);
            return planner.kind == PlannerOperatorKind::Primitive &&
                           planner.primitive_action == action &&
                           std::isfinite(priced.cost) && priced.cost >= 0.0
                       ? priced.cost
                       : kInfinity;
        };
        const double transmute_cost = primitive_cost(transmute);
        const double alteration_cost = primitive_cost(alteration);
        const double augment_cost = primitive_cost(augment);
        const double regal_cost = primitive_cost(regal);
        const double exalt_cost = primitive_cost(exalt);
        if (!std::isfinite(transmute_cost) ||
            !std::isfinite(alteration_cost) ||
            !std::isfinite(regal_cost)) {
            return std::nullopt;
        }
        if (!action_legal(
                session, calc.registry().actions.at(transmute), anchor)) {
            return std::nullopt;
        }

        const auto exact_bench_mask = [&](const std::uint32_t action) {
            std::uint32_t mask = 0;
            if (action >= calc.registry().actions.size()) return mask;
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            if (descriptor.params.type != ActionType::Bench ||
                descriptor.params.mod_id >= session.mod_count) {
                return mask;
            }
            for (std::uint32_t slot = 0;
                 slot < calc.layout().slots.size(); ++slot) {
                if (pc_bitset_test(
                        calc.layout().slots[slot].satisfying_mask.data(),
                        descriptor.params.mod_id)) {
                    mask |= 1u << slot;
                }
            }
            return mask;
        };
        const auto same_kernel = [](const OutcomeDistribution& left,
                                    const OutcomeDistribution& right) {
            return left.supported == right.supported &&
                   left.applicable == right.applicable &&
                   left.entries == right.entries &&
                   left.choice_groups == right.choice_groups &&
                   left.choice_options == right.choice_options;
        };
        const auto progress_mask = [&](const std::uint32_t state,
                                       const std::uint32_t acquisition) {
            return satisfied_goal_mask_for_state(state) & acquisition;
        };

        FocusedFallbackPolicy best;
        best.anchor_state = restart_state;
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        const std::uint32_t all_slots =
            calc.layout().slots.size() == 32
                ? 0xffffffffu
                : (1u << calc.layout().slots.size()) - 1u;
        const std::uint32_t alteration_reach =
            action_goal_reach_mask(alteration);
        const std::uint32_t regal_reach = action_goal_reach_mask(regal);

        for (const std::uint32_t finish_action :
             calc.automatic_goal_bench_actions()) {
            const std::uint32_t finish_mask = exact_bench_mask(finish_action);
            const double finish_cost = primitive_cost(finish_action);
            if (finish_mask == 0 || std::popcount(finish_mask) >= required) {
                continue;
            }
            if (!std::isfinite(finish_cost)) continue;
            const std::uint32_t acquisition_count =
                required - std::popcount(finish_mask);
            if (acquisition_count != 2) continue;
            const std::uint32_t available = all_slots & ~finish_mask;
            for (std::uint32_t acquisition = available; acquisition != 0;
                 acquisition = (acquisition - 1u) & available) {
                if (std::popcount(acquisition) != acquisition_count ||
                    (alteration_reach & acquisition) != acquisition ||
                    (regal_reach & acquisition) != acquisition) {
                    continue;
                }

                const OutcomeDistribution& transmute_kernel =
                    calc.outcomes(
                        restart_state, transmute,
                        options.goal_progress_gated_reforges);
                if (!transmute_kernel.supported ||
                    !transmute_kernel.choice_groups.empty() ||
                    !transmute_kernel.choice_options.empty()) {
                    continue;
                }
                std::map<std::uint32_t, double> direct_carriers;
                std::vector<std::uint32_t> no_target_states;
                double no_target_probability = 0.0;
                for (const OutcomeEntry& outcome : transmute_kernel.entries) {
                    if (progress_mask(outcome.state, acquisition) != 0) {
                        direct_carriers[outcome.state] += outcome.probability;
                    } else {
                        no_target_probability += outcome.probability;
                        no_target_states.push_back(outcome.state);
                    }
                }
                if (no_target_states.empty()) continue;

                const OutcomeDistribution& alteration_kernel =
                    calc.outcomes(
                        no_target_states.front(), alteration,
                        options.goal_progress_gated_reforges);
                if (!alteration_kernel.supported ||
                    !alteration_kernel.choice_groups.empty() ||
                    !alteration_kernel.choice_options.empty()) {
                    continue;
                }
                bool retry_kernel_exact = true;
                for (const std::uint32_t state : no_target_states) {
                    if (!action_legal(
                            session,
                            calc.registry().actions.at(alteration),
                            calc.state(state)) ||
                        !same_kernel(
                            alteration_kernel,
                            calc.outcomes(
                                state, alteration,
                                options.goal_progress_gated_reforges))) {
                        retry_kernel_exact = false;
                        break;
                    }
                }
                std::map<std::uint32_t, double> alteration_exits;
                double alteration_exit_probability = 0.0;
                if (retry_kernel_exact) {
                    for (const OutcomeEntry& outcome :
                         alteration_kernel.entries) {
                        if (progress_mask(outcome.state, acquisition) != 0) {
                            alteration_exits[outcome.state] +=
                                outcome.probability;
                            alteration_exit_probability += outcome.probability;
                        } else if (!same_kernel(
                                       alteration_kernel,
                                       calc.outcomes(
                                           outcome.state, alteration,
                                           options
                                               .goal_progress_gated_reforges))) {
                            retry_kernel_exact = false;
                            break;
                        }
                    }
                }
                if (!retry_kernel_exact ||
                    alteration_exit_probability <= 1e-15) {
                    continue;
                }

                std::map<std::uint32_t, double> carrier_probability =
                    direct_carriers;
                for (const auto& [state, probability] : alteration_exits) {
                    carrier_probability[state] +=
                        no_target_probability * probability /
                        alteration_exit_probability;
                }

                std::map<std::uint32_t, const OutcomeDistribution*>
                    augment_kernels;
                if (augment != kNoId && std::isfinite(augment_cost)) {
                    std::set<std::uint32_t> augment_sources(
                        no_target_states.begin(), no_target_states.end());
                    for (const OutcomeEntry& outcome :
                         alteration_kernel.entries) {
                        if (progress_mask(outcome.state, acquisition) == 0) {
                            augment_sources.insert(outcome.state);
                        }
                    }
                    for (const std::uint32_t state : augment_sources) {
                        if (!action_legal(
                                session,
                                calc.registry().actions.at(augment),
                                calc.state(state))) {
                            continue;
                        }
                        const OutcomeDistribution& kernel =
                            calc.outcomes(
                                state, augment,
                                options.goal_progress_gated_reforges);
                        if (!kernel.supported ||
                            !kernel.choice_groups.empty() ||
                            !kernel.choice_options.empty()) {
                            continue;
                        }
                        bool valid = true;
                        for (const OutcomeEntry& outcome : kernel.entries) {
                            if (progress_mask(outcome.state, acquisition) != 0) {
                                carrier_probability.try_emplace(
                                    outcome.state, 0.0);
                            } else if (!same_kernel(
                                           alteration_kernel,
                                           calc.outcomes(
                                               outcome.state, alteration,
                                               options
                                                   .goal_progress_gated_reforges))) {
                                valid = false;
                                break;
                            }
                        }
                        if (valid) augment_kernels.emplace(state, &kernel);
                    }
                }

                struct CarrierEquation {
                    double probability = 0.0;
                    double constant = 0.0;
                    double restart_coefficient = 0.0;
                };
                std::unordered_map<std::uint32_t, CarrierEquation>
                    salvage_equations;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    salvage_operator;
                std::unordered_set<std::uint32_t> salvage_active;
                const double salvage_anchor_guess =
                    focused_fallback_policy
                        ? focused_fallback_policy->anchor_state_value
                        : kInfinity;
                std::function<CarrierEquation(std::uint32_t)> salvage;
                salvage = [&](const std::uint32_t state) -> CarrierEquation {
                    if (calc.is_goal_state(calc.state(state))) return {};
                    const auto cached = salvage_equations.find(state);
                    if (cached != salvage_equations.end()) {
                        return cached->second;
                    }
                    CarrierEquation equation;
                    const auto [finish, finish_operator] =
                        constructive_direct_action_upper(
                            state, finish_action);
                    if (progress_mask(state, acquisition) == acquisition &&
                        std::isfinite(finish)) {
                        equation.constant = finish;
                        salvage_operator[state] = finish_operator;
                        salvage_equations.emplace(state, equation);
                        return equation;
                    }
                    const bool exalt_relevant = exalt != kNoId &&
                        std::isfinite(exalt_cost) &&
                        (action_goal_reach_mask(exalt) & acquisition &
                         ~progress_mask(state, acquisition)) != 0 &&
                        action_legal(
                            session, calc.registry().actions.at(exalt),
                            calc.state(state));
                    if (exalt_relevant &&
                        std::isfinite(salvage_anchor_guess) &&
                        salvage_active.insert(state).second) {
                        const OutcomeDistribution& kernel =
                            calc.outcomes(
                                state, exalt,
                                options.goal_progress_gated_reforges);
                        bool acyclic = kernel.supported &&
                            kernel.choice_groups.empty() &&
                            kernel.choice_options.empty();
                        for (const OutcomeEntry& outcome : kernel.entries) {
                            if (!calc.is_goal_state(calc.state(outcome.state)) &&
                                calc.state(outcome.state).prefix_count +
                                        calc.state(outcome.state).suffix_count <=
                                    calc.state(state).prefix_count +
                                        calc.state(state).suffix_count) {
                                acyclic = false;
                                break;
                            }
                        }
                        if (acyclic) {
                            CarrierEquation exalt_equation;
                            exalt_equation.constant = exalt_cost;
                            for (const OutcomeEntry& outcome : kernel.entries) {
                                const CarrierEquation continuation =
                                    salvage(outcome.state);
                                exalt_equation.constant +=
                                    outcome.probability *
                                    continuation.constant;
                                exalt_equation.restart_coefficient +=
                                    outcome.probability *
                                    continuation.restart_coefficient;
                            }
                            const double exalt_value =
                                exalt_equation.constant +
                                exalt_equation.restart_coefficient *
                                    salvage_anchor_guess;
                            const double restart_value = restart_cost +
                                salvage_anchor_guess;
                            if (exalt_value < restart_value) {
                                salvage_operator[state] = exalt;
                                salvage_active.erase(state);
                                salvage_equations.emplace(
                                    state, exalt_equation);
                                return exalt_equation;
                            }
                        }
                        salvage_active.erase(state);
                    }
                    equation.constant = restart_cost;
                    equation.restart_coefficient = 1.0;
                    salvage_operator[state] = restart_operator_index;
                    salvage_equations.emplace(state, equation);
                    return equation;
                };
                std::map<std::uint32_t, CarrierEquation> equations;
                std::map<std::uint32_t, CarrierEquation> direct_equations;
                std::map<std::uint32_t, CarrierEquation> early_equations;
                std::map<std::uint32_t, CarrierEquation>
                    intermediate_equations;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    early_intermediate_state;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    carrier_operator;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    terminal_operator;
                std::unordered_map<std::uint32_t, double> terminal_constant;
                bool feasible = true;
                for (const auto& [carrier, probability] :
                     carrier_probability) {
                    if (!action_legal(
                            session, calc.registry().actions.at(regal),
                            calc.state(carrier))) {
                        feasible = false;
                        break;
                    }
                    const OutcomeDistribution& regal_kernel =
                        calc.outcomes(
                            carrier, regal,
                            options.goal_progress_gated_reforges);
                    if (!regal_kernel.supported ||
                        !regal_kernel.choice_groups.empty() ||
                        !regal_kernel.choice_options.empty()) {
                        feasible = false;
                        break;
                    }
                    CarrierEquation equation;
                    equation.probability = probability;
                    equation.constant = regal_cost;
                    for (const OutcomeEntry& outcome : regal_kernel.entries) {
                        const CarrierEquation continuation =
                            salvage(outcome.state);
                        equation.constant += outcome.probability *
                            continuation.constant;
                        equation.restart_coefficient +=
                            outcome.probability *
                            continuation.restart_coefficient;
                    }
                    carrier_operator[carrier] = regal;

                    /* A deterministic goal bench can be installed on the
                     * magic carrier before Regal. This preserves the one
                     * acquired natural goal and lets Regal pursue the other
                     * natural goal directly. The two primitive kernels below
                     * are retained as ordinary executable policy states. */
                    if (std::popcount(
                            progress_mask(carrier, acquisition)) == 1 &&
                        (calc.state(carrier).flags & kFlagCraftedMod) == 0 &&
                        action_legal(
                            session,
                            calc.registry().actions.at(finish_action),
                            calc.state(carrier))) {
                        const OutcomeDistribution& bench_kernel =
                            calc.outcomes(
                                carrier, finish_action,
                                options.goal_progress_gated_reforges);
                        if (bench_kernel.supported &&
                            bench_kernel.choice_groups.empty() &&
                            bench_kernel.choice_options.empty() &&
                            bench_kernel.entries.size() == 1 &&
                            std::abs(
                                bench_kernel.entries.front().probability -
                                1.0) <= 1e-12) {
                            const std::uint32_t benched =
                                bench_kernel.entries.front().state;
                            if (action_legal(
                                    session,
                                    calc.registry().actions.at(regal),
                                    calc.state(benched))) {
                                const OutcomeDistribution& early_regal =
                                    calc.outcomes(
                                        benched, regal,
                                        options
                                            .goal_progress_gated_reforges);
                                if (early_regal.supported &&
                                    early_regal.choice_groups.empty() &&
                                    early_regal.choice_options.empty()) {
                                    CarrierEquation intermediate;
                                    intermediate.constant = regal_cost;
                                    for (const OutcomeEntry& outcome :
                                         early_regal.entries) {
                                        const CarrierEquation continuation =
                                            salvage(outcome.state);
                                        intermediate.constant +=
                                            outcome.probability *
                                            continuation.constant;
                                        intermediate.restart_coefficient +=
                                            outcome.probability *
                                            continuation.restart_coefficient;
                                    }
                                    equation.constant = finish_cost +
                                        intermediate.constant;
                                    equation.restart_coefficient =
                                        intermediate.restart_coefficient;
                                    early_equations[carrier] = equation;
                                    intermediate_equations[benched] =
                                        intermediate;
                                    early_intermediate_state[carrier] =
                                        benched;
                                }
                            }
                        }
                    }
                    /* `equation` may now contain the early-bench choice.
                     * Reconstruct the direct Regal equation when an early
                     * alternative was recorded so policy improvement can
                     * choose independently for every strict carrier. */
                    if (early_equations.contains(carrier)) {
                        CarrierEquation direct;
                        direct.probability = probability;
                        direct.constant = regal_cost;
                        for (const OutcomeEntry& outcome :
                             regal_kernel.entries) {
                            const CarrierEquation continuation =
                                salvage(outcome.state);
                            direct.constant += outcome.probability *
                                continuation.constant;
                            direct.restart_coefficient +=
                                outcome.probability *
                                continuation.restart_coefficient;
                        }
                        direct_equations.emplace(carrier, direct);
                    } else {
                        direct_equations.emplace(carrier, equation);
                    }
                }
                equations = direct_equations;
                if (!feasible || equations.empty()) continue;

                struct MagicAffine {
                    double constant = 0.0;
                    double restart_coefficient = 0.0;
                    double alteration_coefficient = 0.0;
                };
                struct MagicSolution {
                    double anchor_value = kInfinity;
                    double alteration_constant = 0.0;
                    double alteration_coefficient = 0.0;
                    double constant = 0.0;
                    double coefficient = 0.0;
                };
                const auto solve_magic = [&]() -> MagicSolution {
                    const auto expression_for = [&] (
                        const std::uint32_t state) {
                        MagicAffine expression;
                        const auto target = equations.find(state);
                        if (target != equations.end()) {
                            expression.constant = target->second.constant;
                            expression.restart_coefficient =
                                target->second.restart_coefficient;
                            return expression;
                        }
                        const auto augmented = augment_kernels.find(state);
                        if (augmented == augment_kernels.end()) {
                            expression.alteration_coefficient = 1.0;
                            return expression;
                        }
                        expression.constant = augment_cost;
                        for (const OutcomeEntry& outcome :
                             augmented->second->entries) {
                            const auto exit = equations.find(outcome.state);
                            if (exit == equations.end()) {
                                expression.alteration_coefficient +=
                                    outcome.probability;
                            } else {
                                expression.constant += outcome.probability *
                                    exit->second.constant;
                                expression.restart_coefficient +=
                                    outcome.probability *
                                    exit->second.restart_coefficient;
                            }
                        }
                        return expression;
                    };
                    MagicAffine alteration_expression;
                    alteration_expression.constant = alteration_cost;
                    for (const OutcomeEntry& outcome :
                         alteration_kernel.entries) {
                        const MagicAffine expression =
                            expression_for(outcome.state);
                        alteration_expression.constant +=
                            outcome.probability * expression.constant;
                        alteration_expression.restart_coefficient +=
                            outcome.probability *
                            expression.restart_coefficient;
                        alteration_expression.alteration_coefficient +=
                            outcome.probability *
                            expression.alteration_coefficient;
                    }
                    const double alteration_denominator =
                        1.0 -
                        alteration_expression.alteration_coefficient;
                    if (alteration_denominator <= 1e-15) return {};
                    MagicSolution solved;
                    solved.alteration_constant =
                        alteration_expression.constant /
                        alteration_denominator;
                    solved.alteration_coefficient =
                        alteration_expression.restart_coefficient /
                        alteration_denominator;
                    solved.constant = transmute_cost;
                    for (const OutcomeEntry& outcome :
                         transmute_kernel.entries) {
                        const MagicAffine expression =
                            expression_for(outcome.state);
                        solved.constant += outcome.probability *
                            (expression.constant +
                             expression.alteration_coefficient *
                                 solved.alteration_constant);
                        solved.coefficient += outcome.probability *
                            (expression.restart_coefficient +
                             expression.alteration_coefficient *
                                 solved.alteration_coefficient);
                    }
                    const double denominator = 1.0 - solved.coefficient;
                    if (denominator <= 1e-15) return {};
                    solved.anchor_value = solved.constant / denominator;
                    return solved;
                };
                MagicSolution solved = solve_magic();
                for (std::uint32_t improvement = 0;
                     improvement < 32 &&
                     std::isfinite(solved.anchor_value);
                     ++improvement) {
                    bool changed = false;
                    for (const auto& [carrier, early] : early_equations) {
                        const CarrierEquation& direct =
                            direct_equations.at(carrier);
                        const double direct_value = direct.constant +
                            direct.restart_coefficient * solved.anchor_value;
                        const double early_value = early.constant +
                            early.restart_coefficient * solved.anchor_value;
                        const bool choose_early =
                            early_value < direct_value;
                        const bool was_early =
                            carrier_operator.at(carrier) == finish_action;
                        if (choose_early == was_early) continue;
                        equations[carrier] = choose_early ? early : direct;
                        carrier_operator[carrier] =
                            choose_early ? finish_action : regal;
                        changed = true;
                    }
                    if (!changed) break;
                    solved = solve_magic();
                }
                const double anchor_value = solved.anchor_value;
                const double alteration_constant =
                    solved.alteration_constant;
                const double alteration_coefficient =
                    solved.alteration_coefficient;
                const double constant = solved.constant;
                const double coefficient = solved.coefficient;
                if (!std::isfinite(anchor_value) ||
                    anchor_value >= best.anchor_state_value) {
                    continue;
                }

                const auto magic_expression = [&](const std::uint32_t state) {
                    MagicAffine expression;
                    const auto target = equations.find(state);
                    if (target != equations.end()) {
                        expression.constant = target->second.constant;
                        expression.restart_coefficient =
                            target->second.restart_coefficient;
                        return expression;
                    }
                    const auto augmented = augment_kernels.find(state);
                    if (augmented == augment_kernels.end()) {
                        expression.alteration_coefficient = 1.0;
                        return expression;
                    }
                    expression.constant = augment_cost;
                    for (const OutcomeEntry& outcome :
                         augmented->second->entries) {
                        const auto exit = equations.find(outcome.state);
                        if (exit == equations.end()) {
                            expression.alteration_coefficient +=
                                outcome.probability;
                        } else {
                            expression.constant +=
                                outcome.probability * exit->second.constant;
                            expression.restart_coefficient +=
                                outcome.probability *
                                exit->second.restart_coefficient;
                        }
                    }
                    return expression;
                };

                FocusedFallbackPolicy candidate;
                candidate.anchor_state = restart_state;
                candidate.anchor_state_value = anchor_value;
                candidate.finish_action = finish_action;
                candidate.progress_state_value[restart_state] = anchor_value;
                candidate.progress_state_operator[restart_state] = transmute;
                const double alteration_value =
                    alteration_constant +
                    alteration_coefficient * anchor_value;
                std::set<std::uint32_t> no_target_policy_states(
                    no_target_states.begin(), no_target_states.end());
                for (const OutcomeEntry& outcome : alteration_kernel.entries) {
                    if (progress_mask(outcome.state, acquisition) == 0) {
                        no_target_policy_states.insert(outcome.state);
                    }
                }
                for (const auto& [unused_state, kernel] : augment_kernels) {
                    (void)unused_state;
                    for (const OutcomeEntry& outcome : kernel->entries) {
                        if (progress_mask(outcome.state, acquisition) == 0) {
                            no_target_policy_states.insert(outcome.state);
                        }
                    }
                }
                for (const std::uint32_t state : no_target_policy_states) {
                    const MagicAffine expression = magic_expression(state);
                    candidate.progress_state_value[state] =
                        expression.constant +
                        expression.restart_coefficient * anchor_value +
                        expression.alteration_coefficient * alteration_value;
                    candidate.progress_state_operator[state] =
                        augment_kernels.contains(state) ? augment : alteration;
                }
                for (const auto& [state, equation] : equations) {
                    candidate.progress_state_value[state] =
                        equation.constant +
                        equation.restart_coefficient * anchor_value;
                    candidate.progress_state_operator[state] =
                        carrier_operator.at(state);
                }
                for (const auto& [state, equation] :
                     intermediate_equations) {
                    candidate.progress_state_value[state] =
                        equation.constant +
                        equation.restart_coefficient * anchor_value;
                    candidate.progress_state_operator[state] = regal;
                }
                for (const auto& [state, equation] : salvage_equations) {
                    candidate.progress_state_value[state] =
                        equation.constant +
                        equation.restart_coefficient * anchor_value;
                    candidate.progress_state_operator[state] =
                        salvage_operator.at(state);
                }
                for (const auto& [state, operator_index] :
                     terminal_operator) {
                    if (operator_index == restart_operator_index) {
                        candidate.progress_state_value[state] =
                            restart_cost + anchor_value;
                    } else {
                        candidate.progress_state_value[state] =
                            terminal_constant.at(state);
                    }
                    candidate.progress_state_operator[state] = operator_index;
                }
                ++result.diagnostics.constructive_policy_feasible_policies;
                retain_action_reason(
                    "included:magic_augment_regal_policy:" +
                    finite_json(anchor_value) + ":constant=" +
                    finite_json(constant) + ":retry=" +
                    finite_json(coefficient) + ":augment_states=" +
                    std::to_string(augment_kernels.size()));
                best = std::move(candidate);
            }
        }
        if (!std::isfinite(best.anchor_state_value)) return std::nullopt;
        return best;
    }

bool SolveWork::Impl::advance_primitive_destructive_renewal_fallback(
        std::optional<FocusedFallbackPolicy>& completed) {
        completed.reset();
        if (!options.allow_economic_restart) return true;
        PrimitiveDestructiveRenewalWork& work =
            primitive_destructive_renewal_work;
        if (!work.active) {
            if (restart_state == kNoId ||
                restart_state >= calc.state_count() ||
                result.start_state >= calc.state_count() ||
                restart_state >= transition_cache->state_rows.size()) {
                return true;
            }
            const AbstractState& anchor = calc.state(restart_state);
            if (anchor.rarity != PC_RARITY_NORMAL ||
                anchor.prefix_count != 0 || anchor.suffix_count != 0 ||
                (anchor.flags & (kFlagCraftedMod | kProtectionFlags)) != 0 ||
                anchor.fractured_goal_mask != 0 ||
                anchor.fractured_metamod_flags != 0) {
                return true;
            }
            work = PrimitiveDestructiveRenewalWork{};
            work.active = true;
            work.best.anchor_state = restart_state;
            work.materialized_alternatives.reserve(
                incremental_alternative_rows.size());
            for (const IncrementalAlternativeRow& alternative :
                 incremental_alternative_rows) {
                if (alternative.row_index < transition_cache->rows.size() &&
                    alternative.row_index < priced_rows.size()) {
                    work.materialized_alternatives.push_back(
                        alternative.row_index);
                }
            }
            std::sort(
                work.materialized_alternatives.begin(),
                work.materialized_alternatives.end());
            work.materialized_alternatives.erase(
                std::unique(
                    work.materialized_alternatives.begin(),
                    work.materialized_alternatives.end()),
                work.materialized_alternatives.end());
            work.renewal_sources.reserve(
                transition_cache->state_rows.size());
            if (result.start_state < transition_cache->state_rows.size()) {
                work.renewal_sources.push_back(result.start_state);
            }
            for (std::uint32_t state = 0;
                 state < transition_cache->state_rows.size(); ++state) {
                if (state == result.start_state || state >= expanded.size() ||
                    !expanded[state] ||
                    calc.is_goal_state(calc.state(state))) {
                    continue;
                }
                work.renewal_sources.push_back(state);
            }
        }
        constexpr std::uint32_t kRenewalSourcesPerCooperativeUnit = 32;
        std::uint32_t sources_processed = 0;
        while (work.renewal_source_cursor < work.renewal_sources.size() &&
               sources_processed++ < kRenewalSourcesPerCooperativeUnit) {
        const std::uint32_t renewal_source =
            work.renewal_sources[work.renewal_source_cursor++];
        std::unordered_set<std::uint64_t> inspected_renewals;
            const AbstractState& renewal_entry =
                calc.state(renewal_source);
            for (const std::uint64_t absolute :
                 state_row_indices(
                     *transition_cache, renewal_source)) {
                const SparseRow& row =
                    transition_cache->rows.at(absolute);
                if (!row.admitted &&
                    !std::binary_search(
                        work.materialized_alternatives.begin(),
                        work.materialized_alternatives.end(), absolute)) {
                    continue;
                }
                if (row.choice_count != 0) continue;
                for (std::uint32_t variant_offset = 0;
                     variant_offset < row.variant_count;
                     ++variant_offset) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                        transition_cache->variant_arena
                            ->row_variant_indices.at(
                                row.variant_offset + variant_offset));
                const std::uint64_t inspected_key =
                    (static_cast<std::uint64_t>(renewal_source) << 32) |
                    variant.operator_index;
                if (!inspected_renewals.insert(inspected_key).second) {
                    continue;
                }
                const PlannerOperator& renewal =
                    calc.operators().at(variant.operator_index);
                if (renewal.kind != PlannerOperatorKind::Primitive ||
                    renewal.primitive_action >=
                        calc.registry().actions.size()) {
                    continue;
                }
                const std::uint32_t renewal_action =
                    renewal.primitive_action;
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(renewal_action);
                if (!action_transition_facts(
                         descriptor.params.type).renewal ||
                    !action_legal(session, descriptor, renewal_entry)) {
                    continue;
                }
                double renewal_cost = 0.0;
                if (!priced_variant_cost(variant, renewal_cost) ||
                    !std::isfinite(renewal_cost) || renewal_cost < 0.0) {
                    continue;
                }
                const OutcomeDistribution& kernel = calc.outcomes(
                    renewal_source, renewal_action,
                    options.goal_progress_gated_reforges);
                if (!kernel.supported || !kernel.stable_shared_kernel ||
                    !kernel.choice_groups.empty() ||
                    !kernel.choice_options.empty()) {
                    continue;
                }
                std::vector<std::uint64_t> kernel_signature;
                if (!calc.exact_reforge_kernel_signature(
                        renewal_source, renewal_action,
                        kernel_signature)) {
                    continue;
                }
                ++result.diagnostics.constructive_policy_renewal_variants;
                double success_probability = 0.0;
                bool exact_retry = true;
                for (const OutcomeEntry& outcome : kernel.entries) {
                    if (calc.is_goal_state(calc.state(outcome.state))) {
                        success_probability += outcome.probability;
                        continue;
                    }
                    ++result.diagnostics.constructive_policy_exit_checks;
                    std::vector<std::uint64_t> retry_signature;
                    if (!action_legal(
                            session, descriptor,
                            calc.state(outcome.state)) ||
                        !calc.exact_reforge_kernel_signature(
                            outcome.state, renewal_action,
                            retry_signature) ||
                        retry_signature != kernel_signature) {
                        exact_retry = false;
                        break;
                    }
                }
                if (!exact_retry) continue;
                if (success_probability <= 1e-15) continue;
                const double renewal_value =
                    renewal_cost / success_probability;
                if (!std::isfinite(renewal_value) ||
                    renewal_value >= kValueCeiling) {
                    continue;
                }

                FocusedFallbackPolicy candidate;
                candidate.anchor_state = restart_state;
                candidate.renewal_state_value = renewal_value;
                candidate.renewal_row = absolute;
                candidate.renewal_operator = variant.operator_index;
                candidate.renewal_rarity = renewal_entry.rarity;
                candidate.renewal_influence_bits =
                    renewal_entry.influence_bits;
                candidate.renewal_searing_exarch_tier =
                    renewal_entry.searing_exarch_tier;
                candidate.renewal_eater_of_worlds_tier =
                    renewal_entry.eater_of_worlds_tier;
                candidate.renewal_kernel_signature =
                    std::move(kernel_signature);

                double anchor_value = kInfinity;
                std::uint32_t anchor_operator = kNoId;
                if (renewal_fallback_eligible(restart_state, candidate)) {
                    anchor_value = renewal_value;
                    anchor_operator = variant.operator_index;
                } else {
                    for (const std::uint64_t setup_absolute :
                         state_row_indices(
                             *transition_cache, restart_state)) {
                        const SparseRow& setup_row =
                            transition_cache->rows.at(setup_absolute);
                        if (!setup_row.admitted) continue;
                        if (setup_row.choice_count != 0) continue;
                        for (std::uint32_t setup_variant_offset = 0;
                             setup_variant_offset < setup_row.variant_count;
                             ++setup_variant_offset) {
                            const SparseVariant& setup_variant =
                                transition_cache->variant_arena->variants.at(
                                    transition_cache->variant_arena
                                        ->row_variant_indices.at(
                                            setup_row.variant_offset +
                                            setup_variant_offset));
                            const PlannerOperator& setup =
                                calc.operators().at(
                                    setup_variant.operator_index);
                            if (setup.kind !=
                                PlannerOperatorKind::Primitive) {
                                continue;
                            }
                            double constant = 0.0;
                            if (!priced_variant_cost(
                                    setup_variant, constant) ||
                                !std::isfinite(constant) ||
                                constant < 0.0) {
                                continue;
                            }
                            double loop_probability =
                                setup_row.self_probability;
                            bool feasible = true;
                            for (std::uint32_t i = 0;
                                 i < setup_row.transition_count; ++i) {
                                const std::uint64_t offset =
                                    setup_row.transition_offset + i;
                                const std::uint32_t successor =
                                    transition_cache->successors.at(offset);
                                const double probability =
                                    transition_cache->probabilities.at(
                                        offset);
                                if (successor == restart_state) {
                                    loop_probability += probability;
                                } else if (calc.is_goal_state(
                                               calc.state(successor))) {
                                    continue;
                                } else if (renewal_fallback_eligible(
                                               successor, candidate)) {
                                    constant += probability * renewal_value;
                                } else {
                                    feasible = false;
                                    break;
                                }
                            }
                            const double denominator =
                                1.0 - loop_probability;
                            if (!feasible || denominator <= 1e-15) continue;
                            const double value = constant / denominator;
                            if (value < anchor_value ||
                                (value == anchor_value &&
                                 setup_variant.operator_index <
                                     anchor_operator)) {
                                anchor_value = value;
                                anchor_operator =
                                    setup_variant.operator_index;
                            }
                        }
                    }
                }
                if (!std::isfinite(anchor_value) ||
                    anchor_operator == kNoId) {
                    continue;
                }
                candidate.anchor_state_value = anchor_value;
                candidate.progress_state_value[restart_state] =
                    anchor_value;
                candidate.progress_state_operator[restart_state] =
                    anchor_operator;
                const double start_value = focused_start_upper_bound(
                    candidate);
                if (!std::isfinite(start_value)) continue;
                ++result.diagnostics.constructive_policy_feasible_policies;
                if (start_value < work.best_start ||
                    (start_value == work.best_start &&
                     std::tie(
                         variant.operator_index, anchor_operator) <
                         std::tie(
                             work.best.renewal_operator,
                             work.best.progress_state_operator[
                                 restart_state]))) {
                    work.best_start = start_value;
                    work.best = std::move(candidate);
                }
            }
        }
        }
        if (work.renewal_source_cursor < work.renewal_sources.size()) {
            return false;
        }
        work.active = false;
        if (!std::isfinite(work.best.anchor_state_value)) {
            work = PrimitiveDestructiveRenewalWork{};
            return true;
        }
        const PlannerOperator& chosen =
            calc.operators().at(work.best.renewal_operator);
        result.diagnostics.destructive_renewal_action_id = chosen.id;
        result.diagnostics.destructive_renewal_value =
            work.best.renewal_state_value;
        result.diagnostics.destructive_renewal_anchor_value =
            work.best.anchor_state_value;
        result.diagnostics.destructive_renewal_start_value = work.best_start;
        std::string renewal_reason =
            "included:primitive_destructive_renewal_policy:" +
            chosen.id + ":renewal=" +
            finite_json(work.best.renewal_state_value) + ":anchor=" +
            finite_json(work.best.anchor_state_value) + ":start=" +
            finite_json(work.best_start);
        /* Preserve the selected executable incumbent even when broad action
         * admission already filled the bounded reason sample. */
        if (!result.diagnostics.action_inclusion_reasons.empty() &&
            result.diagnostics.action_inclusion_reasons.size() >=
                options.max_diagnostic_samples) {
            result.diagnostics.action_inclusion_reasons.back() =
                std::move(renewal_reason);
            ++result.diagnostics.action_inclusion_reasons_omitted;
        } else {
            retain_action_reason(std::move(renewal_reason));
        }
        completed = std::move(work.best);
        work = PrimitiveDestructiveRenewalWork{};
        return true;
    }

auto SolveWork::Impl::progressive_fracture_fallback(
        const FocusedFallbackPolicy& bootstrap) -> std::optional<FocusedFallbackPolicy> {
        result.diagnostics.progressive_fracture_status = "entered";
        if (bootstrap.renewal_operator == kNoId ||
            bootstrap.renewal_operator >= calc.operators().size() ||
            bootstrap.renewal_kernel_signature.empty() ||
            bootstrap.anchor_state != restart_state) {
            return std::nullopt;
        }
        const PlannerOperator& renewal =
            calc.operators().at(bootstrap.renewal_operator);
        if (renewal.kind != PlannerOperatorKind::Primitive ||
            renewal.primitive_action >= calc.registry().actions.size()) {
            return std::nullopt;
        }
        const std::uint32_t renewal_action = renewal.primitive_action;
        result.diagnostics.progressive_fracture_roll_action_id = renewal.id;
        const ActionDescriptor& renewal_descriptor =
            calc.registry().actions.at(renewal_action);
        if (!action_transition_facts(
                 renewal_descriptor.params.type).renewal) {
            return std::nullopt;
        }
        const std::int32_t renewal_position =
            bootstrap.renewal_operator < priced_operator_position.size()
                ? priced_operator_position[bootstrap.renewal_operator]
                : -1;
        if (renewal_position < 0) return std::nullopt;
        const double renewal_cost = operators.at(
            static_cast<std::size_t>(renewal_position)).cost;
        if (!std::isfinite(renewal_cost) || renewal_cost < 0.0) {
            return std::nullopt;
        }

        std::uint32_t fracture_operator = kNoId;
        std::uint32_t fracture_action = kNoId;
        double fracture_cost = kInfinity;
        for (const PricedOperator& priced : operators) {
            const PlannerOperator& planner =
                calc.operators().at(priced.index);
            if (planner.kind != PlannerOperatorKind::Primitive ||
                planner.primitive_action >=
                    calc.registry().actions.size() ||
                calc.registry().actions.at(
                    planner.primitive_action).params.type !=
                    ActionType::Fracture ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            if (priced.cost < fracture_cost ||
                (priced.cost == fracture_cost &&
                 priced.index < fracture_operator)) {
                fracture_cost = priced.cost;
                fracture_operator = priced.index;
                fracture_action = planner.primitive_action;
            }
        }
        if (fracture_operator == kNoId) return std::nullopt;
        result.diagnostics.progressive_fracture_status =
            "fracture_admitted";
        const ActionDescriptor& fracture_descriptor =
            calc.registry().actions.at(fracture_action);
        const OutcomeDistribution& acquisition_kernel = calc.outcomes(
            result.start_state, renewal_action,
            options.goal_progress_gated_reforges);
        if (!acquisition_kernel.supported ||
            !acquisition_kernel.stable_shared_kernel ||
            !acquisition_kernel.choice_groups.empty() ||
            !acquisition_kernel.choice_options.empty()) {
            return std::nullopt;
        }

        using AcquisitionClass =
            std::pair<std::uint32_t, std::uint32_t>;
        const auto acquisition_class = [&](const std::uint32_t state)
            -> std::optional<AcquisitionClass> {
            if (calc.is_goal_state(calc.state(state))) return std::nullopt;
            const AbstractState& carrier = calc.state(state);
            const std::uint32_t mask =
                satisfied_goal_mask_for_state(state);
            if (mask == 0 || carrier.fractured_goal_mask != 0 ||
                carrier.fractured_metamod_flags != 0 ||
                !action_legal(session, fracture_descriptor, carrier)) {
                return std::nullopt;
            }
            for (const std::uint8_t count :
                 carrier.fractured_junk_counts) {
                if (count != 0) return std::nullopt;
            }
            for (const std::uint8_t count :
                 carrier.fractured_crafted_junk_counts) {
                if (count != 0) return std::nullopt;
            }
            return AcquisitionClass{
                mask,
                static_cast<std::uint32_t>(
                    carrier.prefix_count + carrier.suffix_count)};
        };
        std::map<AcquisitionClass, double> class_mass;
        for (const OutcomeEntry& outcome : acquisition_kernel.entries) {
            const auto key = acquisition_class(outcome.state);
            if (key.has_value()) class_mass[*key] += outcome.probability;
        }
        if (class_mass.empty()) return std::nullopt;
        AcquisitionClass selected_class = class_mass.begin()->first;
        double selected_mass = class_mass.begin()->second;
        for (const auto& [key, probability] : class_mass) {
            if (probability > selected_mass ||
                (probability == selected_mass && key < selected_class)) {
                selected_class = key;
                selected_mass = probability;
            }
        }
        result.diagnostics.progressive_fracture_status =
            "acquisition_class_selected";
        result.diagnostics.progressive_fracture_class_mask =
            selected_class.first;
        result.diagnostics.progressive_fracture_class_mod_count =
            selected_class.second;
        result.diagnostics.progressive_fracture_class_probability =
            selected_mass;

        FocusedFallbackPolicy candidate;
        candidate.anchor_state = restart_state;
        candidate.renewal_row = bootstrap.renewal_row;
        candidate.renewal_operator = bootstrap.renewal_operator;
        candidate.renewal_rarity = bootstrap.renewal_rarity;
        candidate.renewal_influence_bits =
            bootstrap.renewal_influence_bits;
        candidate.renewal_searing_exarch_tier =
            bootstrap.renewal_searing_exarch_tier;
        candidate.renewal_eater_of_worlds_tier =
            bootstrap.renewal_eater_of_worlds_tier;
        candidate.renewal_kernel_signature =
            bootstrap.renewal_kernel_signature;

        std::map<std::vector<std::uint64_t>, std::size_t>
            post_mode_by_signature;
        const auto post_mode = [&](const std::uint32_t state)
            -> std::optional<std::size_t> {
            std::vector<std::uint64_t> signature;
            if (!calc.exact_reforge_kernel_signature(
                    state, renewal_action, signature)) {
                result.diagnostics.progressive_fracture_status =
                    "post_signature_refused";
                return std::nullopt;
            }
            const auto cached = post_mode_by_signature.find(signature);
            if (cached != post_mode_by_signature.end()) {
                return cached->second;
            }
            if (!action_legal(
                    session, renewal_descriptor, calc.state(state))) {
                result.diagnostics.progressive_fracture_status =
                    "post_renewal_illegal";
                return std::nullopt;
            }
            const OutcomeDistribution& kernel =
                calc.outcomes(
                    state, renewal_action,
                    options.goal_progress_gated_reforges);
            if (!kernel.supported || !kernel.stable_shared_kernel ||
                !kernel.choice_groups.empty() ||
                !kernel.choice_options.empty()) {
                result.diagnostics.progressive_fracture_status =
                    "post_kernel_not_stable";
                return std::nullopt;
            }
            double success_probability = 0.0;
            for (const OutcomeEntry& outcome : kernel.entries) {
                if (calc.is_goal_state(calc.state(outcome.state))) {
                    success_probability += outcome.probability;
                    continue;
                }
                std::vector<std::uint64_t> retry_signature;
                if (!action_legal(
                        session, renewal_descriptor,
                        calc.state(outcome.state)) ||
                    !calc.exact_reforge_kernel_signature(
                        outcome.state, renewal_action,
                        retry_signature) ||
                    retry_signature != signature) {
                    result.diagnostics.progressive_fracture_status =
                        "post_retry_kernel_mismatch";
                    return std::nullopt;
                }
            }
            if (!(success_probability > 0.0)) {
                const AbstractState& post = calc.state(state);
                result.diagnostics.progressive_fracture_status =
                    "post_goal_unreachable:state=" +
                    std::to_string(state) + ":satisfied=" +
                    std::to_string(
                        satisfied_goal_mask_for_state(state)) +
                    ":fractured=" +
                    std::to_string(post.fractured_goal_mask) +
                    ":prefixes=" +
                    std::to_string(post.prefix_count) +
                    ":suffixes=" +
                    std::to_string(post.suffix_count);
                return std::nullopt;
            }
            const double value = renewal_cost / success_probability;
            if (!std::isfinite(value) || value >= kValueCeiling) {
                result.diagnostics.progressive_fracture_status =
                    "post_value_ceiling";
                return std::nullopt;
            }
            const std::size_t index =
                candidate.primitive_renewal_modes.size();
            candidate.primitive_renewal_modes.push_back({
                value, bootstrap.renewal_operator, signature});
            post_mode_by_signature.emplace(
                std::move(signature), index);
            return index;
        };

        struct FractureBranch {
            double constant = kInfinity;
            double anchor_coefficient = 0.0;
        };
        std::unordered_map<std::uint32_t, FractureBranch> branch_by_state;
        std::unordered_set<std::uint32_t> failed_fracture_states;
        const auto fracture_branch = [&](const std::uint32_t state)
            -> std::optional<FractureBranch> {
            const auto cached = branch_by_state.find(state);
            if (cached != branch_by_state.end()) return cached->second;
            if (acquisition_class(state) != selected_class) {
                return std::nullopt;
            }
            FractureBranch branch{fracture_cost, 0.0};
            std::optional<ProductFractureKernel> local_fracture;
            const std::vector<OutcomeEntry>* fracture_entries = nullptr;
            if (calc.product_solver_parent()) {
                local_fracture = product_fracture_kernel(
                    state,
                    calc.operators()
                        .at(fracture_operator)
                        .relevant_goal_mask);
                if (!local_fracture->eligible) {
                    result.diagnostics.progressive_fracture_status =
                        "fracture_local_kernel_refused";
                    return std::nullopt;
                }
                fracture_entries = &local_fracture->exits;
            } else {
                const OutcomeDistribution& fractured =
                    calc.outcomes(
                        state, fracture_action,
                        options.goal_progress_gated_reforges);
                if (!fractured.supported ||
                    !fractured.choice_groups.empty() ||
                    !fractured.choice_options.empty()) {
                    result.diagnostics.progressive_fracture_status =
                        "fracture_kernel_refused";
                    return std::nullopt;
                }
                fracture_entries = &fractured.entries;
            }
            for (const OutcomeEntry& outcome : *fracture_entries) {
                if (calc.is_goal_state(calc.state(outcome.state))) continue;
                if (local_fracture.has_value() &&
                    outcome.state == local_fracture->restart_state) {
                    branch.constant +=
                        outcome.probability * restart_cost;
                    branch.anchor_coefficient += outcome.probability;
                    continue;
                }
                const std::uint32_t fractured_satisfied =
                    calc.state(outcome.state).fractured_goal_mask &
                    satisfied_goal_mask_for_state(outcome.state);
                if (fractured_satisfied != 0) {
                    const auto mode = post_mode(outcome.state);
                    if (!mode.has_value()) return std::nullopt;
                    branch.constant += outcome.probability *
                        candidate.primitive_renewal_modes[*mode].value;
                } else {
                    branch.constant += outcome.probability * restart_cost;
                    branch.anchor_coefficient += outcome.probability;
                    if (!local_fracture.has_value()) {
                        failed_fracture_states.insert(outcome.state);
                    }
                }
            }
            branch_by_state.emplace(state, branch);
            return branch;
        };

        double initial_constant = renewal_cost;
        double initial_anchor_coefficient = 0.0;
        double initial_loop_probability = 0.0;
        for (const OutcomeEntry& outcome : acquisition_kernel.entries) {
            if (calc.is_goal_state(calc.state(outcome.state))) continue;
            if (acquisition_class(outcome.state) == selected_class) {
                const auto branch = fracture_branch(outcome.state);
                if (!branch.has_value()) return std::nullopt;
                initial_constant +=
                    outcome.probability * branch->constant;
                initial_anchor_coefficient +=
                    outcome.probability * branch->anchor_coefficient;
            } else {
                initial_loop_probability += outcome.probability;
            }
        }
        result.diagnostics.progressive_fracture_status =
            "fracture_branches_complete";
        const double initial_denominator =
            1.0 - initial_loop_probability;
        if (initial_denominator <= 0.0) return std::nullopt;
        const double initial_alpha =
            initial_constant / initial_denominator;
        const double initial_beta =
            initial_anchor_coefficient / initial_denominator;

        const auto setup_operator_found =
            bootstrap.progress_state_operator.find(restart_state);
        if (setup_operator_found ==
            bootstrap.progress_state_operator.end()) {
            return std::nullopt;
        }
        const std::uint32_t setup_operator = setup_operator_found->second;
        const SparseRow* setup_row = nullptr;
        double setup_cost = kInfinity;
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, restart_state)) {
            const SparseRow& row = transition_cache->rows.at(row_index);
            if (!row.admitted) continue;
            if (row.choice_count != 0) continue;
            for (std::uint32_t variant_offset = 0;
                 variant_offset < row.variant_count; ++variant_offset) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                        transition_cache->variant_arena
                            ->row_variant_indices.at(
                                row.variant_offset + variant_offset));
                if (variant.operator_index != setup_operator) continue;
                double cost = 0.0;
                if (!priced_variant_cost(variant, cost)) continue;
                setup_row = &row;
                setup_cost = cost;
                break;
            }
            if (setup_row != nullptr) break;
        }
        if (setup_row == nullptr || !std::isfinite(setup_cost)) {
            result.diagnostics.progressive_fracture_status =
                "setup_row_unavailable";
            return std::nullopt;
        }
        double setup_constant = setup_cost;
        double setup_initial_coefficient = 0.0;
        double setup_anchor_coefficient = setup_row->self_probability;
        for (std::uint32_t i = 0;
             i < setup_row->transition_count; ++i) {
            const std::uint64_t offset = setup_row->transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            const double probability =
                transition_cache->probabilities.at(offset);
            if (successor == restart_state) {
                setup_anchor_coefficient += probability;
            } else if (calc.is_goal_state(calc.state(successor))) {
                continue;
            } else if (acquisition_class(successor) == selected_class) {
                const auto branch = fracture_branch(successor);
                if (!branch.has_value()) return std::nullopt;
                setup_constant += probability * branch->constant;
                setup_anchor_coefficient +=
                    probability * branch->anchor_coefficient;
            } else if (renewal_fallback_eligible(successor, bootstrap)) {
                setup_initial_coefficient += probability;
            } else {
                result.diagnostics.progressive_fracture_status =
                    "setup_successor_uncovered";
                return std::nullopt;
            }
        }
        result.diagnostics.progressive_fracture_status =
            "setup_composed";
        const double setup_denominator =
            1.0 - setup_anchor_coefficient;
        if (setup_denominator <= 0.0) return std::nullopt;
        const double setup_gamma = setup_constant / setup_denominator;
        const double setup_delta =
            setup_initial_coefficient / setup_denominator;
        const double coupled_denominator =
            1.0 - setup_delta * initial_beta;
        if (coupled_denominator <= 0.0) return std::nullopt;
        const double anchor_value =
            (setup_gamma + setup_delta * initial_alpha) /
            coupled_denominator;
        const double initial_value =
            initial_alpha + initial_beta * anchor_value;
        if (!std::isfinite(anchor_value) ||
            !std::isfinite(initial_value) ||
            anchor_value >= kValueCeiling ||
            initial_value >= kValueCeiling) {
            return std::nullopt;
        }
        candidate.anchor_state_value = anchor_value;
        candidate.renewal_state_value = initial_value;
        candidate.progress_state_value[restart_state] = anchor_value;
        candidate.progress_state_operator[restart_state] = setup_operator;
        for (const auto& [state, branch] : branch_by_state) {
            candidate.progress_state_value[state] =
                branch.constant +
                branch.anchor_coefficient * anchor_value;
            candidate.progress_state_operator[state] = fracture_operator;
        }
        for (const std::uint32_t state : failed_fracture_states) {
            candidate.progress_state_value[state] =
                restart_cost + anchor_value;
            candidate.progress_state_operator[state] =
                restart_operator_index;
        }
        const double start_value = focused_start_upper_bound(candidate);
        if (!std::isfinite(start_value)) return std::nullopt;

        result.diagnostics.progressive_fracture_roll_action_id = renewal.id;
        result.diagnostics.progressive_fracture_value = initial_value;
        result.diagnostics.progressive_fracture_anchor_value = anchor_value;
        result.diagnostics.progressive_fracture_start_value = start_value;
        result.diagnostics.progressive_fracture_class_mask =
            selected_class.first;
        result.diagnostics.progressive_fracture_class_mod_count =
            selected_class.second;
        result.diagnostics.progressive_fracture_class_probability =
            selected_mass;
        result.diagnostics.progressive_fracture_post_modes =
            static_cast<std::uint32_t>(
                candidate.primitive_renewal_modes.size());
        result.diagnostics.progressive_fracture_status = "complete";

        std::string reason =
            "included:progressive_fracture_policy:roll=" + renewal.id +
            ":fracture=" +
            calc.operators().at(fracture_operator).id +
            ":class_mask=" + std::to_string(selected_class.first) +
            ":class_mods=" + std::to_string(selected_class.second) +
            ":class_probability=" + finite_json(selected_mass) +
            ":post_modes=" +
            std::to_string(candidate.primitive_renewal_modes.size()) +
            ":renewal=" + finite_json(initial_value) +
            ":anchor=" + finite_json(anchor_value) +
            ":start=" + finite_json(start_value);
        if (!result.diagnostics.action_inclusion_reasons.empty() &&
            result.diagnostics.action_inclusion_reasons.size() >=
                options.max_diagnostic_samples) {
            result.diagnostics.action_inclusion_reasons.back() =
                std::move(reason);
            ++result.diagnostics.action_inclusion_reasons_omitted;
        } else {
            retain_action_reason(std::move(reason));
        }
        return candidate;
    }

auto SolveWork::Impl::focused_fallback(bool& complete)
        -> std::optional<FocusedFallbackPolicy> {
        complete = true;
        if (!options.allow_economic_restart) return std::nullopt;
        const std::uint32_t renewal_source = result.start_state;
        if (!constructive_fallback_pending) {
            ++result.diagnostics.constructive_policy_anchor_checks;
            if (renewal_source >= expanded.size() ||
                !expanded[renewal_source] ||
                renewal_source >= transition_cache->state_rows.size() ||
                restart_state == kNoId || restart_state >= expanded.size() ||
                !expanded[restart_state] ||
                restart_state >= transition_cache->state_rows.size()) {
                return std::nullopt;
            }
            constructive_progress_fallback = magic_regal_fallback();
            constructive_fallback_pending = true;
        }
        std::optional<FocusedFallbackPolicy> progress_fallback =
            std::nullopt;
        std::optional<FocusedFallbackPolicy> destructive_fallback;
        if (!advance_primitive_destructive_renewal_fallback(
                destructive_fallback)) {
            complete = false;
            return std::nullopt;
        }
        progress_fallback = std::move(constructive_progress_fallback);
        constructive_progress_fallback.reset();
        constructive_fallback_pending = false;
        if (destructive_fallback.has_value()) {
            try {
                std::optional<FocusedFallbackPolicy> progressive =
                    progressive_fracture_fallback(*destructive_fallback);
                if (progressive.has_value() &&
                    focused_start_upper_bound(*progressive) <
                        focused_start_upper_bound(*destructive_fallback)) {
                    destructive_fallback = std::move(progressive);
                }
            } catch (const SolverResourceLimit& limit) {
                std::string reason =
                    "rejected:progressive_fracture_policy:" +
                    limit.cap_name();
                if (!result.diagnostics.action_inclusion_reasons.empty() &&
                    result.diagnostics.action_inclusion_reasons.size() >=
                        options.max_diagnostic_samples) {
                    result.diagnostics.action_inclusion_reasons.back() =
                        std::move(reason);
                    ++result.diagnostics.action_inclusion_reasons_omitted;
                } else {
                    retain_action_reason(std::move(reason));
                }
            }
        }
        if (destructive_fallback.has_value() &&
            (!progress_fallback.has_value() ||
             focused_start_upper_bound(*destructive_fallback) <
                 focused_start_upper_bound(*progress_fallback))) {
            progress_fallback = std::move(destructive_fallback);
        }
        const AbstractState& renewal_carrier = calc.state(renewal_source);
        if (renewal_carrier.prefix_count != 0 ||
            renewal_carrier.suffix_count != 0 ||
            (renewal_carrier.flags & kFlagCraftedMod) != 0 ||
            renewal_carrier.fractured_goal_mask != 0 ||
            renewal_carrier.fractured_metamod_flags != 0 ||
            (renewal_carrier.flags & kProtectionFlags) != 0) {
            return progress_fallback;
        }
        for (const std::uint8_t count :
             renewal_carrier.fractured_junk_counts) {
            if (count != 0) return progress_fallback;
        }
        ++result.diagnostics.constructive_policy_anchor_eligible;
        FocusedFallbackPolicy best;
        best.anchor_state = restart_state;
        best.renewal_rarity = renewal_carrier.rarity;
        best.renewal_influence_bits = renewal_carrier.influence_bits;
        best.renewal_searing_exarch_tier =
            renewal_carrier.searing_exarch_tier;
        best.renewal_eater_of_worlds_tier =
            renewal_carrier.eater_of_worlds_tier;
        for (const std::uint64_t absolute :
             state_row_indices(*transition_cache, renewal_source)) {
            const SparseRow& row = transition_cache->rows.at(absolute);
            if (!row.admitted) continue;
            if (row.choice_count != 0) continue;
            for (std::uint32_t variant_offset = 0;
                 variant_offset < row.variant_count; ++variant_offset) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                        transition_cache->variant_arena
                            ->row_variant_indices.at(
                                row.variant_offset + variant_offset));
                const PlannerOperator& renewal =
                    calc.operators().at(variant.operator_index);
                if (renewal.automatic_kind !=
                        AutomaticCandidateKind::ConstructiveRenewal ||
                    renewal.constructive_finish_action == kNoId) {
                    continue;
                }
                ++result.diagnostics.constructive_policy_renewal_variants;
                double renewal_cost = 0.0;
                if (!priced_variant_cost(variant, renewal_cost) ||
                    !std::isfinite(renewal_cost) || renewal_cost < 0.0) {
                    continue;
                }
                double loop_probability = row.self_probability;
                double constant = renewal_cost;
                bool has_finish = false;
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint64_t offset = row.transition_offset + i;
                    const std::uint32_t successor =
                        transition_cache->successors.at(offset);
                    const double probability =
                        transition_cache->probabilities.at(offset);
                    if (successor == renewal_source) {
                        loop_probability += probability;
                        continue;
                    }
                    if (calc.is_goal_state(calc.state(successor))) {
                        has_finish = true;
                        continue;
                    }
                    ++result.diagnostics.constructive_policy_exit_checks;
                    const auto [finish, unused_operator] =
                        constructive_direct_action_upper(
                            successor,
                            renewal.constructive_finish_action);
                    (void)unused_operator;
                    if (std::isfinite(finish)) {
                        ++result.diagnostics
                              .constructive_policy_finishable_exits;
                        constant += probability * finish;
                        has_finish = true;
                    } else {
                        /* The approved destructive renewal is applied again
                         * on this non-fractured exit carrier. Its next
                         * attempt has the same exact clean-carrier kernel;
                         * no synthetic Restart or free setup is implied. */
                        loop_probability += probability;
                    }
                }
                const double denominator = 1.0 - loop_probability;
                if (!has_finish || denominator <= 1e-15) continue;
                const double value = constant / denominator;
                if (!std::isfinite(value) || value >= kValueCeiling) {
                    continue;
                }
                ++result.diagnostics.constructive_policy_feasible_policies;
                if (value < best.renewal_state_value ||
                    (value == best.renewal_state_value &&
                     std::tie(
                         absolute, variant.operator_index,
                         renewal.constructive_finish_action) <
                         std::tie(
                             best.renewal_row, best.renewal_operator,
                             best.finish_action))) {
                    best.renewal_state_value = value;
                    best.renewal_row = absolute;
                    best.renewal_operator = variant.operator_index;
                    best.finish_action =
                        renewal.constructive_finish_action;
                }
            }
        }
        if (!std::isfinite(best.renewal_state_value)) return progress_fallback;

        if (renewal_fallback_eligible(best.anchor_state, best)) {
            best.anchor_state_value = best.renewal_state_value;
            if (progress_fallback.has_value() &&
                progress_fallback->anchor_state_value <
                    best.anchor_state_value) {
                return progress_fallback;
            }
            return best;
        }

        /* Restart returns the engine's real fresh carrier, which need not
         * have the rarity of the diagnostic start. Compose an exact setup row
         * at that carrier with the renewable rare-state fallback; never
         * substitute the original start state for Restart's successor. */
        for (const std::uint64_t absolute :
             state_row_indices(*transition_cache, best.anchor_state)) {
            const SparseRow& row = transition_cache->rows.at(absolute);
            if (!row.admitted) continue;
            const PricedSparseRow& priced = priced_rows.at(absolute);
            if (priced.operator_index == kNoId ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            double constant = priced.cost;
            double loop_probability = row.self_probability;
            bool feasible = true;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor == best.anchor_state) {
                    loop_probability +=
                        transition_cache->probabilities.at(offset);
                    continue;
                }
                const double continuation =
                    fallback_terminal_upper(successor, best);
                if (!std::isfinite(continuation)) {
                    feasible = false;
                    break;
                }
                constant += transition_cache->probabilities.at(offset) *
                            continuation;
            }
            if (!feasible) continue;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& group =
                    transition_cache->choices.at(row.choice_offset + i);
                double selected = group.has_self ? kInfinity : kInfinity;
                for (std::uint32_t s = 0;
                     s < group.successor_count; ++s) {
                    selected = std::min(
                        selected,
                        fallback_terminal_upper(
                            transition_cache->choice_successors.at(
                                group.successor_offset + s),
                            best));
                }
                if (group.has_self) {
                    /* The observation can explicitly choose the carrier and
                     * therefore selects self only if its solved value beats
                     * every executable alternate. Conservatively treating
                     * self as the whole group remains feasible. */
                    loop_probability += group.probability;
                } else if (std::isfinite(selected)) {
                    constant += group.probability * selected;
                } else {
                    feasible = false;
                    break;
                }
            }
            const double denominator = 1.0 - loop_probability;
            if (!feasible || denominator <= 1e-15) continue;
            const double value = constant / denominator;
            if (value < best.anchor_state_value ||
                (value == best.anchor_state_value &&
                 absolute < best.anchor_row)) {
                best.anchor_state_value = value;
                best.anchor_row = absolute;
                best.anchor_operator = priced.operator_index;
            }
        }
        if (!std::isfinite(best.anchor_state_value)) return progress_fallback;
        if (progress_fallback.has_value() &&
            progress_fallback->anchor_state_value < best.anchor_state_value) {
            return progress_fallback;
        }
        return best;
    }

void SolveWork::Impl::sync_constructive_discovered_states() {
        const std::uint32_t state_count = calc.state_count();
        const std::uint32_t previous =
            static_cast<std::uint32_t>(result.values.size());
        if (state_count <= previous) return;
        transition_cache->state_rows.resize(state_count);
        transition_cache->discovered_states = state_count;
        expanded.resize(state_count, 0);
        queued.resize(state_count, 0);
        result.expanded.resize(state_count, 0);
        result.goal_states.resize(state_count, 0);
        result.values.resize(state_count, 0.0);
        for (std::uint32_t state = previous; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                continue;
            }
            result.values[state] =
                completion_proof_lower(state).value;
        }
    }

}
}
