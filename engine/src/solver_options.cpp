#include "solver_options_runtime_helpers.hpp"

namespace poecraft {
namespace solver {

const OptionKernel& CalcContext::option_kernel(
    const std::uint32_t state_id,
    const std::uint32_t operator_index) {
    const PlannerOperator& option = operators_.at(operator_index);
    if (option.kind != PlannerOperatorKind::FixedOption) {
        throw std::invalid_argument("option kernel requested for primitive");
    }
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | operator_index;
    const auto cached = option_kernel_cache_.find(key);
    if (cached != option_kernel_cache_.end()) return *cached->second;

    auto result = std::make_shared<OptionKernel>();
    result->supported = true;
    result->legal = true;
    result->terminates_almost_surely = true;
    result->automatic.candidate =
        option.automatic_kind != AutomaticCandidateKind::None;
    result->automatic.relevant_goal_mask = option.relevant_goal_mask;
    if (result->automatic.candidate) {
        result->automatic.legality_result = "pending_exact_kernel";
    }

    if (option.option_kind == FixedOptionKind::Renewal ||
        option.option_kind == FixedOptionKind::ProtectedRepeat ||
        option.option_kind == FixedOptionKind::FracturePrepare ||
        option.option_kind == FixedOptionKind::ImprintRetry ||
        option.option_kind == FixedOptionKind::TemporaryBenchRepeat) {
        const auto finish = [&]() -> const OptionKernel& {
            const auto finish_started =
                option.option_kind == FixedOptionKind::ProtectedRepeat
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            if (result->automatic.candidate) {
                result->automatic.eligible =
                    result->supported && result->legal &&
                    result->terminates_almost_surely;
                if (result->automatic.reason.empty()) {
                    result->automatic.reason =
                        result->automatic.eligible
                            ? "complete_exact_automatic_kernel"
                            : !result->supported
                                  ? "exact_kernel_unsupported"
                                  : !result->legal
                                        ? "exact_legality_or_relevance_refused"
                                        : "retry_chain_not_almost_sure";
                }
                if (result->automatic.legality_result ==
                    "pending_exact_kernel") {
                    result->automatic.legality_result =
                        result->automatic.eligible ? "legal" : "illegal";
                }
            }
            std::sort(result->retry_states.begin(),
                      result->retry_states.end());
            result->retry_states.erase(
                std::unique(result->retry_states.begin(),
                            result->retry_states.end()),
                result->retry_states.end());
            std::sort(result->continuation_states.begin(),
                      result->continuation_states.end());
            result->continuation_states.erase(
                std::unique(result->continuation_states.begin(),
                            result->continuation_states.end()),
                result->continuation_states.end());
            std::shared_ptr<const OptionKernel> retained = result;
            if (result->supported && result->legal) {
                const std::uint64_t template_id =
                    option_template_hash(option, *result);
                result->retained_template_id = template_id;
                const auto bucket = option_kernel_templates_.find(template_id);
                if (bucket != option_kernel_templates_.end()) {
                    for (const OptionKernelTemplateMemo& memo :
                         bucket->second) {
                        if (memo.operator_index >= operators_.size() ||
                            !same_option_template_planner(
                                operators_.at(memo.operator_index), option) ||
                            memo.expected_resources !=
                                result->expected_resources ||
                            !same_complete_option_kernel(
                                *memo.kernel, *result)) {
                            continue;
                        }
                        retained = memo.kernel;
                        option_kernel_template_hit_keys_.insert(key);
                        break;
                    }
                }
                if (retained.get() == result.get()) {
                    auto& templates =
                        option_kernel_templates_[template_id];
                    const std::size_t old_capacity =
                        templates.capacity();
                    templates.push_back(
                        {operator_index, retained,
                         retained->expected_resources});
                    account_option_template_insert(
                        old_capacity, templates.back());
                }
            }
            account_option_cache_insert(key, retained);
            const auto inserted = option_kernel_cache_.emplace(
                key, std::move(retained));
            const OptionKernel& stored = *inserted.first->second;
            if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                telemetry_.protected_finish_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - finish_started)
                            .count());
            }
            return stored;
        };
        const bool cleanup_before_setup =
            option.option_kind == FixedOptionKind::TemporaryBenchRepeat &&
            option.primitive_program.size() == 4 &&
            option.primitive_program.front() == option.cleanup_action &&
            option.primitive_program[1] == option.setup_action;
        if (option.primitive_program.empty() ||
            (option.option_kind != FixedOptionKind::FracturePrepare &&
             !action_legal(
                 *session_, registry_.actions.at(
                                option.primitive_program.front()),
                 state(state_id)))) {
            result->legal = false;
            result->terminates_almost_surely = false;
            result->automatic.reason = "setup_or_first_step_illegal";
            return finish();
        }
        if (option.option_kind == FixedOptionKind::ImprintRetry) {
            if (!native_imprint_checkpoint_creation_legal(*this, state_id)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.legality_result =
                    "checkpoint_creation_illegal";
                result->automatic.reason =
                    "native_imprint_checkpoint_creation_refused";
                return finish();
            }
        }
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            if ((state(state_id).flags & lock_flag) != 0) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "protection_already_active_no_exact_reapplication";
                return finish();
            }
            if (!setup_applies_exactly(
                    *this, state_id, option.setup_action, lock_flag)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.setup_complete = false;
                result->automatic.kernel_change_mechanisms =
                    kAutomaticMetamodProtection;
                result->automatic.reason =
                    "setup_did_not_apply_exactly";
                return finish();
            }
        }
        if (option.option_kind == FixedOptionKind::TemporaryBenchRepeat &&
            state_has_unfractured_crafted(state(state_id)) &&
            !cleanup_before_setup) {
            result->legal = false;
            result->terminates_almost_surely = false;
            result->automatic.reason =
                "cleanup_would_remove_preexisting_crafted_carrier";
            return finish();
        }
        if (option.option_kind == FixedOptionKind::FracturePrepare) {
            const ActionDescriptor& fracture =
                registry_.actions.at(option.conditional_action);
            const AbstractState entry = state(state_id);
            const bool carrier_ready =
                option.carrier_goal_slot < layout_.slots.size() &&
                entry.slot_status[option.carrier_goal_slot] ==
                    static_cast<std::uint8_t>(GoalSlotStatus::Satisfied) &&
                action_legal(*session_, fracture, entry);
            if (carrier_ready) {
                std::map<std::string, double> resources;
                add_action_resources(resources, fracture, 1.0);
                result->expected_resources.assign(
                    resources.begin(), resources.end());
                result->expected_primitive_actions = 1.0;
                result->entry_continues = true;
                const OutcomeDistribution& distribution = outcomes(
                    state_id, option.conditional_action);
                if (!distribution.supported ||
                    !distribution.choice_groups.empty()) {
                    result->supported = false;
                    result->legal = false;
                    result->terminates_almost_surely = false;
                    return finish();
                }
                result->exits = distribution.entries;
                result->legal = !result->exits.empty();
                if (result->automatic.candidate) {
                    AttemptKernel fractured;
                    fractured.supported = distribution.supported;
                    fractured.fully_legal = result->legal;
                    fractured.entries = distribution.entries;
                    result->automatic.kernel_changed = true;
                    result->automatic.kernel_change_mechanisms =
                        kAutomaticCarrierFracture;
                    result->automatic.candidate_kernel_hash =
                        attempt_kernel_hash(fractured);
                    result->automatic.setup_complete = true;
                    result->automatic.cleanup_complete = true;
                    result->automatic.recovery_complete = true;
                    result->automatic.exits_complete = result->legal;
                    result->automatic.legality_result =
                        result->legal ? "legal" : "illegal";
                    result->automatic.reason =
                        result->legal
                            ? "exact_satisfying_carrier_fracture_route"
                            : "fracture_has_no_complete_exact_exits";
                }
                return finish();
            }
            if ((entry.flags &
                 (kFlagInfluenced | kFlagSynthesised | kFlagFractured)) !=
                0) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "carrier_flags_make_fracture_path_illegal";
                return finish();
            }
        }

        const auto attempt_started =
            option.option_kind == FixedOptionKind::ProtectedRepeat
                ? std::chrono::steady_clock::now()
                : std::chrono::steady_clock::time_point{};
        const AttemptKernel attempt = execute_attempt(
            *this, option.primitive_program, state_id);
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            telemetry_.protected_attempt_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - attempt_started)
                        .count());
        }
        if (!attempt.supported) {
            result->supported = false;
            result->legal = false;
            result->terminates_almost_surely = false;
            return finish();
        }
        if (!attempt.fully_legal) {
            result->legal = false;
            result->terminates_almost_surely = false;
            result->automatic.reason =
                option.option_kind == FixedOptionKind::TemporaryBenchRepeat
                    ? "missing_setup_followup_or_cleanup_route"
                    : "one_or_more_program_steps_illegal";
            return finish();
        }
        if (option.option_kind == FixedOptionKind::TemporaryBenchRepeat ||
            (option.option_kind == FixedOptionKind::ProtectedRepeat &&
             result->automatic.candidate)) {
            const AbstractState entry = state(state_id);
            const auto baseline_started =
                option.option_kind == FixedOptionKind::ProtectedRepeat
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            if (option.option_kind == FixedOptionKind::ProtectedRepeat &&
                result->automatic.candidate &&
                defer_automatic_protected_baseline_) {
                /* State-local automatic admission compares this exact
                 * candidate with the parent-context baseline after local
                 * normalization. The parent owns cross-carrier reforge
                 * sharing; no candidate is retained before that comparison. */
                result->automatic.kernel_changed = true;
                result->automatic_candidate_attempt_entries =
                    attempt.entries;
            } else {
                const AttemptKernel baseline = execute_attempt(
                    *this,
                    cleanup_before_setup
                        ? std::vector<std::uint32_t>{
                              option.cleanup_action,
                              option.followup_action}
                        : std::vector<std::uint32_t>{
                              option.followup_action},
                    state_id);
                if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                    telemetry_.protected_baseline_ns +=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() -
                                baseline_started)
                                .count());
                }
                result->automatic.baseline_kernel_hash =
                    attempt_kernel_hash(baseline);
                result->automatic.candidate_kernel_hash =
                    attempt_kernel_hash(attempt);
                result->automatic.kernel_changed =
                    baseline.supported && baseline.fully_legal &&
                    !same_attempt_outcomes(baseline, attempt);
            }
            if (cleanup_before_setup) {
                const AttemptKernel prepared = execute_attempt(
                    *this,
                    {option.cleanup_action, option.setup_action},
                    state_id);
                result->automatic.setup_complete =
                    prepared.supported && prepared.fully_legal &&
                    !prepared.entries.empty() &&
                    std::all_of(
                        prepared.entries.begin(), prepared.entries.end(),
                        [&](const OutcomeEntry& exit) {
                            return state_has_unfractured_crafted(
                                state(exit.state));
                        });
            } else {
                result->automatic.setup_complete = setup_applies_exactly(
                    *this, state_id, option.setup_action,
                    option.option_kind == FixedOptionKind::ProtectedRepeat
                        ? static_cast<std::uint32_t>(
                              option.intended_side == PC_SIDE_PREFIX
                                  ? kFlagPrefixesLocked
                                  : kFlagSuffixesLocked)
                        : std::uint32_t{0});
            }

            bool carrier_relevant = true;
            bool cleanup_complete = true;
            std::uint32_t target_mask = option.relevant_goal_mask;
            if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                std::uint32_t protected_mask = 0;
                for (std::uint32_t slot = 0;
                     slot < goal_.slots.size(); ++slot) {
                    if (goal_slot_side(session(), goal_.slots[slot]) ==
                        option.intended_side) {
                        protected_mask |= 1u << slot;
                    }
                }
                carrier_relevant =
                    (satisfied_goal_mask(entry) & protected_mask) != 0;
                target_mask &= ~protected_mask;
                const std::uint32_t lock_flag =
                    option.intended_side == PC_SIDE_PREFIX
                        ? kFlagPrefixesLocked
                        : kFlagSuffixesLocked;
                cleanup_complete =
                    all_exits_without_flag(*this, attempt.entries, lock_flag);
                result->automatic.kernel_change_mechanisms =
                    kAutomaticMetamodProtection;
            } else {
                cleanup_complete = all_exits_cleaned(*this, attempt.entries);
                const ActionDescriptor& blocker =
                    registry_.actions.at(option.setup_action);
                if (blocker.params.mod_id < session().metamod_type.size()) {
                    const int metamod =
                        session().metamod_type[blocker.params.mod_id];
                    if (metamod == session().data->metamod_no_attack_code ||
                        metamod == session().data->metamod_no_caster_code) {
                        result->automatic.kernel_change_mechanisms |=
                            kAutomaticMetamodPoolBlock;
                    }
                }
                if (blocker.params.mod_id < session().mod_count &&
                    session().group_offsets[blocker.params.mod_id] <
                        session().group_offsets[blocker.params.mod_id + 1]) {
                    result->automatic.kernel_change_mechanisms |=
                        kAutomaticGroupConflict;
                }
                if (blocker.params.mod_id < session().gen_type.size()) {
                    result->automatic.kernel_change_mechanisms |=
                        session().gen_type[blocker.params.mod_id] ==
                                PC_SIDE_PREFIX
                            ? kAutomaticPrefixSlot
                            : kAutomaticSuffixSlot;
                }
            }
            result->automatic.cleanup_complete = cleanup_complete;
            const bool relevant = carrier_relevant &&
                advances_goal_mask(*this, entry, attempt.entries, target_mask);
            if (!result->automatic.setup_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason = "setup_did_not_apply_exactly";
                return finish();
            }
            if (!result->automatic.kernel_changed) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason = "exact_successor_kernel_neutral";
                return finish();
            }
            if (!relevant) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    option.option_kind == FixedOptionKind::ProtectedRepeat
                        ? "no_satisfied_protected_carrier_or_goal_relevant_exit"
                        : "following_action_does_not_advance_relevant_goal";
                return finish();
            }
            if (!cleanup_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason = "cleanup_or_replacement_incomplete";
                return finish();
            }
            result->automatic.legality_result = "legal";
        }
        const OutcomeDistribution* entry_reforge_kernel = nullptr;
        std::uint32_t entry_reforge_action = kNoId;
        struct ProtectedReforgeCertificate {
            bool exact = false;
            const OutcomeDistribution* distribution = nullptr;
        };
        const auto protected_reforge_kernel =
            [&](const std::uint32_t candidate)
                -> ProtectedReforgeCertificate {
            if (option.option_kind != FixedOptionKind::ProtectedRepeat ||
                option.primitive_program.size() != 2 ||
                option.primitive_program.front() != option.setup_action ||
                option.primitive_program.back() != option.followup_action) {
                return {};
            }
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            if (!setup_applies_exactly(
                    *this, candidate, option.setup_action, lock_flag)) {
                /* The entry attempt is fully legal. An inapplicable exact
                 * setup therefore cannot describe the same attempt. */
                return {true, nullptr};
            }
            const OutcomeDistribution& setup =
                outcomes(candidate, option.setup_action);
            const OutcomeEntry* deterministic_exit = nullptr;
            for (const OutcomeEntry& exit : setup.entries) {
                if (exit.probability <= 0.0) continue;
                if (deterministic_exit != nullptr ||
                    exit.probability != 1.0) {
                    return {};
                }
                deterministic_exit = &exit;
            }
            if (deterministic_exit == nullptr) return {};
            if (!action_legal(
                    *session_,
                    registry_.actions.at(option.followup_action),
                    state(deterministic_exit->state))) {
                return {true, nullptr};
            }
            const OutcomeDistribution& reforge = outcomes(
                deterministic_exit->state, option.followup_action);
            if (!reforge.supported || !reforge.choice_groups.empty()) {
                return {true, nullptr};
            }
            return {true, &reforge};
        };
        const bool protected_reforge_certificate =
            option.option_kind == FixedOptionKind::ProtectedRepeat;
        if (protected_reforge_certificate) {
            entry_reforge_action = option.followup_action;
            entry_reforge_kernel =
                protected_reforge_kernel(state_id).distribution;
        } else if (!option.primitive_program.empty()) {
            entry_reforge_action = option.primitive_program.front();
            const ActionDescriptor& action =
                registry_.actions.at(entry_reforge_action);
            if (approved_renewal_roll(action)) {
                /* The reforge cache owns immutable shared distributions keyed
                 * by action and preserved base. Pointer identity is therefore
                 * an exact same-kernel certificate for the destructive first
                 * step. The remaining fixed program is identical on both
                 * carriers, including any observed final choice, so equal
                 * first-step kernels prove equal complete attempt kernels. */
                entry_reforge_kernel =
                    &outcomes(state_id, entry_reforge_action);
            }
        }
        std::map<std::uint32_t, AttemptKernel> retry_attempts;
        const auto retry_equivalent = [&](const std::uint32_t candidate) {
            if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                ++telemetry_.protected_retry_checks;
                const std::uint32_t lock_flag =
                    option.intended_side == PC_SIDE_PREFIX
                        ? kFlagPrefixesLocked
                        : kFlagSuffixesLocked;
                if ((state(candidate).flags & lock_flag) != 0) return false;
            }
            if (candidate == state_id) return true;
            if (entry_reforge_kernel != nullptr &&
                protected_reforge_certificate) {
                const ProtectedReforgeCertificate candidate_kernel =
                    protected_reforge_kernel(candidate);
                if (candidate_kernel.exact) {
                    if (candidate_kernel.distribution == nullptr) {
                        return false;
                    }
                    const bool same_kernel =
                        (entry_reforge_kernel->stable_shared_kernel &&
                         candidate_kernel.distribution ==
                             entry_reforge_kernel) ||
                        (attempt.choice_groups.empty() &&
                         attempt.choice_options.empty() &&
                         candidate_kernel.distribution->entries ==
                             attempt.entries);
                    if (same_kernel) {
                        ++telemetry_.protected_retry_certificates;
                    }
                    /* With a deterministic exact setup, the follow-up
                     * distribution is the complete attempt kernel. Equal
                     * entries certify retry equivalence; unequal entries
                     * certify that it is an outer exit. */
                    return same_kernel;
                }
            }
            if (entry_reforge_kernel != nullptr &&
                !protected_reforge_certificate &&
                action_legal(
                    *session_, registry_.actions.at(entry_reforge_action),
                    state(candidate))) {
                const OutcomeDistribution& candidate_kernel =
                    outcomes(candidate, entry_reforge_action);
                const bool same_first_step_kernel =
                    &candidate_kernel == entry_reforge_kernel ||
                    (candidate_kernel.supported ==
                         entry_reforge_kernel->supported &&
                     candidate_kernel.entries ==
                         entry_reforge_kernel->entries &&
                     candidate_kernel.choice_groups ==
                         entry_reforge_kernel->choice_groups &&
                     candidate_kernel.choice_options ==
                         entry_reforge_kernel->choice_options);
                if (same_first_step_kernel) return true;
            }
            if (protected_reforge_certificate) {
                ++telemetry_.protected_retry_fallbacks;
            }
            auto found = retry_attempts.find(candidate);
            if (found == retry_attempts.end()) {
                found = retry_attempts.emplace(
                    candidate,
                    execute_attempt(
                        *this, option.primitive_program, candidate)).first;
            }
            return same_attempt(attempt, found->second);
        };

        std::map<std::string, double> resources;
        add_scaled_resources(resources, attempt.expected_resources);
        result->expected_primitive_actions =
            attempt.expected_primitive_actions;
        const bool exit_contract_can_complete_goal =
            option.exit_min_satisfied >=
            goal_.required_satisfied_slots();
        const auto matches_option_exit =
            [&](const std::uint32_t candidate) {
                if (!option_exit_matches(state(candidate), option)) {
                    return false;
                }
                /* A declared all-goal exit is a terminal promise, not merely
                 * a slot-progress promise. It may not strand a carrier whose
                 * requested slots are present alongside unrelated explicit
                 * affixes. Subset programs remain ordinary intermediate
                 * carrier producers for the outer policy. */
                return !exit_contract_can_complete_goal ||
                       is_goal_state(state(candidate));
            };

        if (option.option_kind == FixedOptionKind::ImprintRetry) {
            if (!attempt.choice_groups.empty() ||
                !attempt.choice_options.empty()) {
                result->supported = false;
                result->legal = false;
                result->terminates_almost_surely = false;
                return finish();
            }
            const BestiaryActionDescriptor& create =
                session_->data->bestiary_actions.at(
                    option.bestiary_create_action);
            for (const std::string& price_key : create.cost_keys) {
                resources[price_key] += 1.0;
            }
            result->expected_primitive_actions += 1.0;
            std::map<std::uint32_t, double> exits;
            double retry_probability = 0.0;
            for (const OutcomeEntry& outcome : attempt.entries) {
                if (matches_option_exit(outcome.state)) {
                    exits[outcome.state] += outcome.probability;
                } else {
                    exits[kNoId] += outcome.probability;
                    retry_probability += outcome.probability;
                    result->expected_primitive_actions += outcome.probability;
                    result->retry_states.push_back(outcome.state);
                }
            }
            result->expected_resources.assign(
                resources.begin(), resources.end());
            for (const auto& [exit, probability] : exits) {
                result->exits.push_back({exit, probability});
            }
            result->terminates_almost_surely =
                retry_probability < 1.0 - 1e-15;
            result->legal = result->supported &&
                            result->terminates_almost_surely &&
                            !result->exits.empty();
            if (result->automatic.candidate) {
                AttemptKernel restored;
                restored.supported = result->supported;
                restored.fully_legal = result->legal;
                restored.expected_primitive_actions =
                    result->expected_primitive_actions;
                restored.expected_resources = result->expected_resources;
                restored.entries = result->exits;
                result->automatic.baseline_kernel_hash =
                    attempt_kernel_hash(attempt);
                result->automatic.candidate_kernel_hash =
                    attempt_kernel_hash(restored);
                result->automatic.kernel_changed =
                    retry_probability < 1.0 - 1e-15;
                result->automatic.kernel_change_mechanisms =
                    kAutomaticImprintCheckpoint;
                result->automatic.setup_complete = true;
                result->automatic.cleanup_complete = true;
                result->automatic.recovery_complete =
                    result->terminates_almost_surely;
                result->automatic.exits_complete = !result->exits.empty();
                result->automatic.legality_result =
                    result->legal ? "legal" : "illegal";
                result->automatic.reason = result->legal
                    ? "exact_imprint_checkpoint_attempt_restore_and_intermediate_exit"
                    : "imprint_attempt_has_no_almost_sure_intermediate_exit";
            }
            return finish();
        }

        if (option.option_kind == FixedOptionKind::FracturePrepare) {
            const ActionDescriptor& fracture =
                registry_.actions.at(option.conditional_action);
            const auto carrier_ready = [&](const std::uint32_t candidate) {
                const AbstractState& candidate_state = state(candidate);
                return option.carrier_goal_slot < layout_.slots.size() &&
                       candidate_state.slot_status[
                           option.carrier_goal_slot] ==
                           static_cast<std::uint8_t>(
                               GoalSlotStatus::Satisfied) &&
                       action_legal(
                           *session_, fracture, candidate_state);
            };
            std::map<std::uint32_t, double> exits;
            double retry_probability = 0.0;
            if (!attempt.choice_groups.empty()) {
                result->supported = false;
                result->legal = false;
                result->terminates_almost_surely = false;
                return finish();
            }
            for (const OutcomeEntry& prepared : attempt.entries) {
                if (carrier_ready(prepared.state)) {
                        result->continuation_states.push_back(
                            prepared.state);
                        result->expected_primitive_actions +=
                            prepared.probability;
                        add_action_resources(
                            resources, fracture, prepared.probability);
                        const OutcomeDistribution& fractured = outcomes(
                            prepared.state, option.conditional_action);
                        if (!fractured.supported ||
                            !fractured.choice_groups.empty()) {
                            result->supported = false;
                            result->legal = false;
                            result->terminates_almost_surely = false;
                            return finish();
                        }
                        for (const OutcomeEntry& outcome :
                             fractured.entries) {
                            exits[outcome.state] +=
                                prepared.probability * outcome.probability;
                        }
                } else if (retry_equivalent(prepared.state)) {
                        exits[kNoId] += prepared.probability;
                        retry_probability += prepared.probability;
                        result->retry_states.push_back(prepared.state);
                } else {
                        /* A changed preparation carrier, illegal Fracture,
                         * or other brick/salvage state remains visible. */
                        exits[prepared.state] += prepared.probability;
                }
            }
            result->expected_resources.assign(
                resources.begin(), resources.end());
            for (const auto& [exit, probability] : exits) {
                result->exits.push_back({exit, probability});
            }
            result->terminates_almost_surely =
                retry_probability < 1.0 - 1e-15;
            result->legal = result->supported &&
                            result->terminates_almost_surely &&
                            !result->exits.empty() &&
                            (!result->automatic.candidate ||
                             !result->continuation_states.empty());
            if (result->automatic.candidate) {
                AttemptKernel fractured;
                fractured.supported = result->supported;
                fractured.fully_legal = result->legal;
                fractured.entries = result->exits;
                result->automatic.kernel_changed =
                    !result->continuation_states.empty();
                result->automatic.kernel_change_mechanisms =
                    kAutomaticCarrierFracture;
                result->automatic.candidate_kernel_hash =
                    attempt_kernel_hash(fractured);
                result->automatic.setup_complete = attempt.fully_legal;
                result->automatic.cleanup_complete = true;
                result->automatic.recovery_complete =
                    result->terminates_almost_surely;
                result->automatic.exits_complete = !result->exits.empty();
                result->automatic.legality_result =
                    result->legal ? "legal" : "illegal";
                result->automatic.reason =
                    result->continuation_states.empty()
                        ? "preparation_never_reaches_exact_legal_carrier"
                        : result->legal
                              ? "exact_preparation_retry_and_fracture_exits"
                              : "fracture_retry_or_outer_exit_incomplete";
            }
            return finish();
        }

        const auto normalization_started =
            option.option_kind == FixedOptionKind::ProtectedRepeat
                ? std::chrono::steady_clock::now()
                : std::chrono::steady_clock::time_point{};
        if (matches_option_exit(state_id)) {
            result->legal = false;
            result->terminates_almost_surely = false;
            return finish();
        }
        std::map<std::uint32_t, double> exits;
        std::map<
            std::pair<std::uint32_t, std::vector<std::uint32_t>>,
            double> choices;
        std::map<std::uint32_t, std::uint32_t> normalized;
        const auto normalize = [&](const std::uint32_t actual) {
            const auto cached = normalized.find(actual);
            if (cached != normalized.end()) return cached->second;
            const bool retry =
                !matches_option_exit(actual) &&
                retry_equivalent(actual);
            const std::uint32_t successor = retry ? kNoId : actual;
            normalized.emplace(actual, successor);
            if (retry) result->retry_states.push_back(actual);
            return successor;
        };
        double forced_retry_probability = 0.0;
        for (const OutcomeEntry& outcome : attempt.entries) {
            const std::uint32_t successor = normalize(outcome.state);
            exits[successor] += outcome.probability;
            if (successor == kNoId) {
                forced_retry_probability += outcome.probability;
            }
        }
        for (const OutcomeChoiceGroup& group : attempt.choice_groups) {
            std::vector<std::uint32_t> successors;
            for (const std::uint32_t actual : group.states) {
                successors.push_back(normalize(actual));
            }
            std::sort(successors.begin(), successors.end());
            successors.erase(
                std::unique(successors.begin(), successors.end()),
                successors.end());
            choices[{group.observation_state, successors}] +=
                group.probability;
            if (successors.size() == 1 && successors.front() == kNoId) {
                forced_retry_probability += group.probability;
            }
        }
        for (const OutcomeChoiceOption& choice : attempt.choice_options) {
            result->observation_choice_options.push_back(
                {choice.mod_id, normalize(choice.actual_state),
                 choice.observation_state, choice.actual_state});
        }
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            telemetry_.protected_normalization_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        normalization_started)
                        .count());
        }
        result->expected_resources.assign(
            resources.begin(), resources.end());
        for (const auto& [exit, probability] : exits) {
            result->exits.push_back({exit, probability});
        }
        for (const auto& [choice, probability] : choices) {
            result->observation_choice_groups.push_back(
                {probability, choice.second, choice.first});
        }
        result->terminates_almost_surely =
            forced_retry_probability < 1.0 - 1e-15;
        result->legal = result->supported &&
                        result->terminates_almost_surely &&
                        (!result->exits.empty() ||
                         !result->observation_choice_groups.empty());
        if (result->automatic.candidate) {
            result->automatic.recovery_complete =
                result->terminates_almost_surely;
            result->automatic.exits_complete =
                !result->exits.empty() ||
                !result->observation_choice_groups.empty();
            if (option.automatic_kind == AutomaticCandidateKind::Veiled) {
                const bool observes_acquired_offer =
                    !option.primitive_program.empty() &&
                    registry_.actions.at(
                        option.primitive_program.back()).params.type ==
                        ActionType::Unveil;
                result->automatic.kernel_changed =
                    observes_acquired_offer &&
                    !result->observation_choice_groups.empty();
                result->automatic.kernel_change_mechanisms =
                    kAutomaticAcquisitionTimeOffer;
                result->automatic.setup_complete = attempt.fully_legal;
                result->automatic.cleanup_complete = true;
                result->automatic.reason = result->legal &&
                                                   observes_acquired_offer
                    ? "exact_acquisition_time_offer_and_best_continuation"
                    : "veiled_acquisition_observation_or_continuation_incomplete";
            }
            if (!result->legal && result->automatic.reason.empty()) {
                result->automatic.reason =
                    "success_failure_recovery_or_outer_exit_incomplete";
            }
        }
        return finish();
    }

    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        const std::uint32_t lock_flag =
            option.intended_side == PC_SIDE_PREFIX
                ? kFlagPrefixesLocked
                : kFlagSuffixesLocked;
        if (!setup_applies_exactly(
                *this, state_id, option.setup_action, lock_flag)) {
            result->legal = false;
            result->terminates_almost_surely = false;
            if (result->automatic.candidate) {
                result->automatic.setup_complete = false;
                result->automatic.kernel_change_mechanisms =
                    kAutomaticMetamodProtection;
                result->automatic.exits_complete = false;
                result->automatic.recovery_complete = false;
                result->automatic.eligible = false;
                result->automatic.legality_result = "illegal";
                result->automatic.reason =
                    "setup_did_not_apply_exactly";
            }
            std::shared_ptr<const OptionKernel> retained = result;
            account_option_cache_insert(key, retained);
            const auto inserted = option_kernel_cache_.emplace(
                key, std::move(retained));
            return *inserted.first->second;
        }
    }

    result->expected_primitive_actions =
        static_cast<double>(option.primitive_program.size());
    result->expected_resources = option.resource_quantities;

    const auto protected_side_program_started =
        option.option_kind == FixedOptionKind::ProtectedSide
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
    std::map<std::uint32_t, double> frontier{{state_id, 1.0}};
    for (std::size_t step = 0; step < option.primitive_program.size(); ++step) {
        const std::uint32_t action_index = option.primitive_program[step];
        const ActionDescriptor& action = registry_.actions.at(action_index);
        const bool final_step = step + 1 == option.primitive_program.size();
        std::map<std::uint32_t, double> next;
        for (const auto& [entry_state, entry_probability] : frontier) {
            const AbstractState& abstract = state(entry_state);
            if (!action_legal(*session_, action, abstract)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                if (option.option_kind ==
                        FixedOptionKind::EldritchSideIntent &&
                    option.automatic_kind ==
                        AutomaticCandidateKind::EldritchSide) {
                    result->automatic.reason =
                        "eldritch_side_program_step_illegal:" +
                        action.id;
                }
                frontier.clear();
                break;
            }
            if (option.option_kind == FixedOptionKind::EldritchSideIntent &&
                final_step &&
                !intended_eldritch_action_legal(
                    *this, abstract, action, option.intended_side,
                    entry_state)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                if (option.automatic_kind ==
                    AutomaticCandidateKind::EldritchSide) {
                    result->automatic.reason =
                        (abstract.flags & kFlagInfluenced) != 0
                            ? "eldritch_side_influenced_carrier_illegal"
                            : "eldritch_side_intended_dominance_illegal:" +
                                  action.id;
                }
                frontier.clear();
                break;
            }

            const OutcomeDistribution& distribution =
                outcomes(entry_state, action_index);
            if (!distribution.supported ||
                !distribution.choice_groups.empty()) {
                result->supported = false;
                result->legal = false;
                result->terminates_almost_surely = false;
                if (option.option_kind ==
                        FixedOptionKind::EldritchSideIntent &&
                    option.automatic_kind ==
                        AutomaticCandidateKind::EldritchSide) {
                    result->automatic.reason =
                        !distribution.supported
                            ? "eldritch_side_distribution_unsupported:" +
                                  action.id
                            : "eldritch_side_choice_distribution_unsupported:" +
                                  action.id;
                }
                frontier.clear();
                break;
            }
            if (distribution.entries.empty() &&
                option.option_kind ==
                    FixedOptionKind::EldritchSideIntent &&
                option.automatic_kind ==
                    AutomaticCandidateKind::EldritchSide) {
                result->automatic.reason =
                    "eldritch_side_distribution_empty:" +
                    action.id;
            }
            for (const OutcomeEntry& exit : distribution.entries) {
                /* Lock and Multimod setup primitives must actually apply.
                 * This is the exact crafted-count/open-side initiation check,
                 * not a charged no-op hidden inside the fixed option. */
                const bool requires_state_change =
                    (option.option_kind == FixedOptionKind::ProtectedSide &&
                     step == 0) ||
                    option.option_kind == FixedOptionKind::MultimodFinish;
                if (requires_state_change && exit.state == entry_state) {
                    result->legal = false;
                    result->terminates_almost_surely = false;
                    frontier.clear();
                    next.clear();
                    break;
                }
                next[exit.state] += entry_probability * exit.probability;
            }
            if (!result->legal) break;
        }
        if (!result->legal || !result->supported) break;
        frontier = std::move(next);

        if (option.option_kind == FixedOptionKind::ProtectedSide && step == 0) {
            const std::uint32_t required_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            for (const auto& [exit_state, probability] : frontier) {
                (void)probability;
                if ((state(exit_state).flags & required_flag) == 0) {
                    result->legal = false;
                    result->terminates_almost_surely = false;
                    frontier.clear();
                    break;
                }
            }
        }
    }
    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        telemetry_.protected_attempt_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    protected_side_program_started)
                    .count());
    }

    if (result->legal && result->supported) {
        for (const auto& [exit_state, probability] : frontier) {
            result->exits.push_back({exit_state, probability});
        }
        if (result->exits.empty()) {
            result->legal = false;
            result->terminates_almost_surely = false;
        }
    }
    const auto protected_side_evidence_started =
        option.option_kind == FixedOptionKind::ProtectedSide
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
    if (result->automatic.candidate) {
        const AbstractState entry = state(state_id);
        result->automatic.exits_complete = !result->exits.empty();
        result->automatic.recovery_complete =
            result->automatic.exits_complete;
        if (option.option_kind == FixedOptionKind::MultimodFinish) {
            result->automatic.kernel_changed = true;
            result->automatic.kernel_change_mechanisms =
                kAutomaticDeterministicFinish;
            result->automatic.setup_complete = result->legal;
            const bool exact_finish = std::all_of(
                result->exits.begin(), result->exits.end(),
                [&](const OutcomeEntry& exit) {
                    return exit.state != kNoId &&
                           is_goal_state(state(exit.state));
                });
            result->automatic.cleanup_complete = exact_finish;
            if (!exact_finish) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "multimod_program_leaves_non_goal_explicit_affix";
            } else if (!advances_goal_mask(
                    *this, entry, result->exits,
                    option.relevant_goal_mask)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "multimod_program_does_not_advance_goal_subset";
            } else {
                result->automatic.reason =
                    "legal_exact_multimod_goal_finish";
            }
        } else if (
            option.option_kind ==
                FixedOptionKind::EldritchSideIntent &&
            option.automatic_kind ==
                AutomaticCandidateKind::EldritchSide) {
            result->automatic.kernel_changed = true;
            result->automatic.kernel_change_mechanisms =
                kAutomaticEldritchDominance |
                (option.intended_side == PC_SIDE_PREFIX
                     ? kAutomaticPrefixSlot
                     : kAutomaticSuffixSlot);
            result->automatic.setup_complete =
                result->legal && result->supported;
            result->automatic.cleanup_complete = true;
            if (!result->automatic.exits_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                if (result->automatic.reason.empty()) {
                    result->automatic.reason =
                        "eldritch_side_exit_coverage_incomplete";
                }
            } else {
                result->automatic.reason =
                    option.primitive_program.size() == 1
                        ? "existing_dominance_exact_side_action"
                        : "exact_paid_dominance_setup_and_side_action";
            }
        } else if (option.option_kind == FixedOptionKind::ProtectedSide) {
            std::uint32_t protected_mask = 0;
            for (std::uint32_t slot = 0; slot < goal_.slots.size(); ++slot) {
                if (goal_slot_side(session(), goal_.slots[slot]) ==
                    option.intended_side) {
                    protected_mask |= 1u << slot;
                }
            }
            const std::uint32_t target_mask =
                option.relevant_goal_mask & ~protected_mask;
            const auto baseline_started =
                std::chrono::steady_clock::now();
            const AttemptKernel baseline = execute_attempt(
                *this, {option.followup_action}, state_id);
            telemetry_.protected_baseline_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - baseline_started)
                        .count());
            AttemptKernel candidate;
            candidate.supported = result->supported;
            candidate.fully_legal = result->legal;
            candidate.entries = result->exits;
            result->automatic.baseline_kernel_hash =
                attempt_kernel_hash(baseline);
            result->automatic.candidate_kernel_hash =
                attempt_kernel_hash(candidate);
            result->automatic.kernel_changed =
                baseline.supported && baseline.fully_legal &&
                !same_attempt_outcomes(baseline, candidate);
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            result->automatic.setup_complete = setup_applies_exactly(
                *this, state_id, option.setup_action, lock_flag);
            result->automatic.cleanup_complete = all_exits_without_flag(
                *this, result->exits, lock_flag);
            result->automatic.kernel_change_mechanisms =
                kAutomaticMetamodProtection;
            const bool carrier_relevant =
                (satisfied_goal_mask(entry) & protected_mask) != 0;
            const bool target_relevant = advances_goal_mask(
                *this, entry, result->exits, target_mask) ||
                clears_target_space(
                    *this, entry, result->exits, target_mask);
            if (!result->automatic.setup_complete ||
                !result->automatic.kernel_changed || !carrier_relevant ||
                !target_relevant || !result->automatic.cleanup_complete ||
                !result->automatic.exits_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    !result->automatic.setup_complete
                        ? "setup_did_not_apply_exactly"
                        : !result->automatic.kernel_changed
                              ? "exact_successor_kernel_neutral"
                              : !carrier_relevant || !target_relevant
                                    ? "unsupported_or_irrelevant_protection_combination"
                                    : !result->automatic.cleanup_complete
                                          ? "cleanup_or_replacement_incomplete"
                                          : "outer_exit_coverage_incomplete";
            } else {
                result->automatic.reason =
                    "exact_protected_side_kernel_and_complete_exits";
            }
        }
        result->automatic.eligible = result->supported && result->legal &&
                                     result->terminates_almost_surely;
        result->automatic.legality_result =
            result->automatic.eligible ? "legal" : "illegal";
    }
    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        telemetry_.protected_normalization_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    protected_side_evidence_started)
                    .count());
    }
    const auto protected_side_finish_started =
        option.option_kind == FixedOptionKind::ProtectedSide
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
    std::shared_ptr<const OptionKernel> retained = result;
    account_option_cache_insert(key, retained);
    const auto inserted =
        option_kernel_cache_.emplace(key, std::move(retained));
    const OptionKernel& stored = *inserted.first->second;
    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        telemetry_.protected_finish_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    protected_side_finish_started)
                    .count());
    }
    return stored;
}

} // namespace solver
} // namespace poecraft
