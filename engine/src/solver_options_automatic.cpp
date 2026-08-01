#include "solver_options_runtime_helpers.hpp"

namespace poecraft {
namespace solver {

StateLocalAutomaticBatch CalcContext::admit_state_local_automatic_candidates(
    const std::uint32_t state_id,
    const AutomaticAdmissionLimits& limits) {
    StateLocalAutomaticBatch batch;
    const auto cached = state_local_automatic_operators_.find(state_id);
    if (cached != state_local_automatic_operators_.end()) {
        batch.cached = true;
        batch.admitted_operators = cached->second;
        return batch;
    }
    if (!goal_.automatic_candidates || is_goal_state(state(state_id))) {
        const auto [stored, inserted] =
            state_local_automatic_operators_.emplace(
            state_id, std::vector<std::uint32_t>{});
        if (inserted) account_state_local_operators(stored->second);
        return batch;
    }

    pc_item_state carrier;
    if (!materialize(state_id, carrier)) {
        const auto [stored, inserted] =
            state_local_automatic_operators_.emplace(
            state_id, std::vector<std::uint32_t>{});
        if (inserted) account_state_local_operators(stored->second);
        return batch;
    }

    const auto shared_started = std::chrono::steady_clock::now();
    const auto synthesis_started = std::chrono::steady_clock::now();
    AutomaticOptionSynthesis synthesis =
        synthesize_automatic_options(
            *this, state_id, carrier, limits.prices);
    /*
     * Eldritch side intents operate on the parent carrier's exact preserved
     * side and can add parent-layout delta states. Do not reproject them
     * through the temporary admission context: an option-specific finer junk
     * partition can choose a different representative and fail to
     * rematerialize even though the parent raw actions are exact. Evaluate
     * these four one-shot compounds directly on the parent state lifecycle.
     */
    std::vector<FixedOptionSpec> parent_eldritch_specs;
    for (auto it = synthesis.specs.begin();
         it != synthesis.specs.end();) {
        if (it->kind == FixedOptionKind::EldritchSideIntent &&
            it->automatic_kind ==
                AutomaticCandidateKind::EldritchSide) {
            parent_eldritch_specs.push_back(std::move(*it));
            it = synthesis.specs.erase(it);
        } else {
            ++it;
        }
    }
    batch.phases.carriers = 1;
    batch.phases.synthesis_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - synthesis_started)
            .count());
    batch.temporary_precompiled_classes =
        synthesis.temporary_precompiled_classes;
    batch.temporary_precompile_ns = temporary_bench_precompile_ns_;
    batch.temporary_precompiled_bytes = temporary_bench_precompiled_bytes_;
    batch.temporary_candidate_variants =
        synthesis.temporary_candidate_variants;
    batch.temporary_effect_classes =
        synthesis.temporary_effect_classes;
    batch.temporary_collapsed_variants =
        synthesis.temporary_collapsed_variants;
    batch.temporary_enumeration_ns =
        synthesis.temporary_enumeration_ns;
    std::vector<std::uint32_t> permanent_benches;
    std::vector<std::uint32_t> local_candidates = candidates_;
    for (std::uint32_t index = 0; index < registry_.actions.size(); ++index) {
        const PlannerOperator& planner = operators_.at(index);
        if (planner.automatic_kind !=
                AutomaticCandidateKind::PermanentBench ||
            std::find(candidates_.begin(), candidates_.end(), index) !=
                candidates_.end() ||
            !action_legal(*session_, registry_.actions[index], state(state_id))) {
            continue;
        }
        permanent_benches.push_back(index);
        if (std::find(
                local_candidates.begin(), local_candidates.end(), index) ==
            local_candidates.end()) {
            local_candidates.push_back(index);
        }
    }

    GoalSpec local_goal = goal_;
    local_goal.automatic_candidates = false;
    local_goal.fixed_options = std::move(synthesis.specs);
    const auto local_context_started = std::chrono::steady_clock::now();
    const std::string context_key = automatic_context_key(
        local_goal.fixed_options, local_candidates);
    constexpr std::size_t kRetainedAutomaticAdmissionContexts = 64;
    bool admission_context_created = false;
    std::unique_ptr<CalcContext> transient_context;
    CalcContext* local_pointer = nullptr;
    const auto retained = automatic_admission_contexts_.find(context_key);
    if (retained != automatic_admission_contexts_.end()) {
        local_pointer = retained->second.get();
        local_pointer->reset_solve_telemetry();
    } else {
        auto created = std::make_unique<CalcContext>(
            session_, local_goal, registry_, local_candidates,
            false, false, true);
        created->set_defer_automatic_protected_baseline(true);
        admission_context_created = true;
        if (automatic_admission_contexts_.size() <
            kRetainedAutomaticAdmissionContexts) {
            local_pointer = created.get();
            automatic_admission_contexts_.emplace(
                context_key, std::move(created));
        } else {
            transient_context = std::move(created);
            local_pointer = transient_context.get();
        }
    }
    CalcContext& local = *local_pointer;
    local.set_defer_automatic_protected_baseline(true);
    batch.phases.local_context_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - local_context_started)
            .count());
    batch.phases.local_planner_build_ns =
        admission_context_created ? local.planner_build_ns() : 0;
    batch.phases.local_layout_build_ns =
        admission_context_created ? local.layout_build_ns() : 0;
    batch.phases.local_ledger_init_ns =
        admission_context_created ? local.owned_byte_ledger_init_ns() : 0;
    const std::uint64_t local_attributed_ns =
        batch.phases.local_planner_build_ns +
        batch.phases.local_layout_build_ns +
        batch.phases.local_ledger_init_ns;
    batch.phases.local_context_other_ns =
        batch.phases.local_context_ns > local_attributed_ns
            ? batch.phases.local_context_ns - local_attributed_ns
            : 0;
    local.set_solve_resource_caps(
        limits.max_discovered_states == 0
            ? std::numeric_limits<std::uint32_t>::max()
            : limits.max_discovered_states,
        limits.max_reforge_work == 0
            ? std::numeric_limits<std::uint64_t>::max()
            : limits.max_reforge_work,
        false,
        limits.max_solver_owned_bytes == 0
            ? std::nullopt
            : std::optional<std::uint64_t>{
                  limits.max_solver_owned_bytes});
    const std::uint32_t local_state = local.intern_item(carrier);
    const std::uint32_t base_operator_count =
        static_cast<std::uint32_t>(local.operators().size());
    std::vector<std::uint32_t> local_option_indices;
    local_option_indices.reserve(
        base_operator_count - registry_.actions.size() + 8);
    for (std::uint32_t index =
             static_cast<std::uint32_t>(registry_.actions.size());
         index < base_operator_count; ++index) {
        local_option_indices.push_back(index);
    }
    std::array<std::uint64_t, kAutomaticTelemetryKindCount> shared_weights{};
    ++shared_weights[static_cast<std::size_t>(AutomaticTelemetryKind::Imprint)];
    for (const std::uint32_t index : permanent_benches) {
        (void)index;
        ++shared_weights[static_cast<std::size_t>(
            AutomaticTelemetryKind::PermanentBench)];
    }
    for (const std::uint32_t index : local_option_indices) {
        const AutomaticTelemetryKind kind =
            telemetry_kind_for_candidate(
                local.operators().at(index).automatic_kind);
        if (kind != AutomaticTelemetryKind::None) {
            ++shared_weights[static_cast<std::size_t>(kind)];
        }
    }
    const std::uint64_t shared_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - shared_started)
            .count());
    const std::uint64_t total_shared_weight = std::accumulate(
        shared_weights.begin(), shared_weights.end(), std::uint64_t{0});
    if (total_shared_weight != 0) {
        for (std::size_t i = 0; i < shared_weights.size(); ++i) {
            batch.shared_admission_ns[i] =
                shared_ns * shared_weights[i] / total_shared_weight;
        }
    }
    std::unordered_map<std::uint32_t, std::uint32_t> mapped_states;
    mapped_states.emplace(local_state, state_id);
    std::vector<const OptionKernel*> seen_option_kernels;
    std::vector<std::pair<
        const OutcomeDistribution*,
        std::vector<std::pair<std::string, double>>>>
        seen_primitive_kernels;

    const auto check_limits = [&](const bool force_bytes = false) {
        const CalcTelemetry& work = local.telemetry();
        if (limits.max_state_action_rows != 0 &&
            work.state_action_rows > limits.max_state_action_rows) {
            throw SolverResourceLimit(
                "max_state_action_rows", limits.max_state_action_rows);
        }
        if (limits.max_transitions != 0 &&
            work.transition_entries > limits.max_transitions) {
            throw SolverResourceLimit(
                "max_transitions", limits.max_transitions);
        }
        if (limits.max_solver_owned_bytes == 0) return;
        if (!force_bytes) return;
        const std::uint64_t owned_bytes = fast_estimated_owned_bytes() +
            (transient_context != nullptr
                 ? local.fast_estimated_owned_bytes()
                 : 0);
        if (owned_bytes >
            limits.max_solver_owned_bytes) {
            throw SolverResourceLimit(
                "max_solver_owned_bytes", limits.max_solver_owned_bytes);
        }
    };

    const auto add_dependency = [&](const std::uint32_t action) {
        if (std::find(candidates_.begin(), candidates_.end(), action) ==
                candidates_.end() &&
            admitted_automatic_dependencies_.insert(action).second) {
            ++action_control_.automatic_dependency_primitives;
        }
    };
    const auto admit_operator = [&](const std::uint32_t index) {
        if (std::find(
                candidate_operators_.begin(), candidate_operators_.end(),
                index) == candidate_operators_.end()) {
            candidate_operators_.push_back(index);
            ++action_control_.automatic_options;
        }
        batch.admitted_operators.push_back(index);
    };
    const auto has_prices = [&](const PlannerOperator& planner) {
        if (limits.prices == nullptr) return true;
        return std::all_of(
            planner.resource_quantities.begin(),
            planner.resource_quantities.end(),
            [&](const auto& resource) {
                return limits.prices->contains(resource.first);
            });
    };
    const auto action_has_prices = [&](const std::uint32_t action) {
        if (limits.prices == nullptr) return true;
        return std::all_of(
            registry_.actions.at(action).cost_keys.begin(),
            registry_.actions.at(action).cost_keys.end(),
            [&](const std::string& key) {
                return limits.prices->contains(key);
            });
    };
    const auto program_has_prices = [&](const PlannerOperator& planner) {
        return std::all_of(
                   planner.primitive_program.begin(),
                   planner.primitive_program.end(), action_has_prices) &&
               (planner.conditional_action == kNoId ||
                action_has_prices(planner.conditional_action));
    };
    if (!parent_eldritch_specs.empty()) {
        GoalSpec parent_goal = goal_;
        parent_goal.automatic_candidates = false;
        parent_goal.fixed_options =
            std::move(parent_eldritch_specs);
        std::vector<PlannerOperator> parent_options =
            build_planner_operators(
                *session_, parent_goal, registry_, candidates_);
        for (std::uint32_t local_index =
                 static_cast<std::uint32_t>(registry_.actions.size());
             local_index < parent_options.size(); ++local_index) {
            PlannerOperator& proposed =
                parent_options[local_index];
            if (proposed.option_kind !=
                    FixedOptionKind::EldritchSideIntent ||
                proposed.automatic_kind !=
                    AutomaticCandidateKind::EldritchSide) {
                continue;
            }
            StateLocalAutomaticCandidate decision;
            decision.id = proposed.id;
            decision.kind =
                AutomaticCandidateKind::EldritchSide;
            decision.telemetry_kind =
                AutomaticTelemetryKind::EldritchSide;
            const auto existing = std::find_if(
                operators_.begin(), operators_.end(),
                [&](const PlannerOperator& candidate) {
                    return candidate.id == proposed.id &&
                           candidate.kind == proposed.kind &&
                           candidate.option_kind ==
                               proposed.option_kind &&
                           candidate.intended_side ==
                               proposed.intended_side &&
                           candidate.primitive_program ==
                               proposed.primitive_program;
                });
            std::uint32_t operator_index = kNoId;
            if (existing == operators_.end()) {
                operator_index = static_cast<std::uint32_t>(
                    operators_.size());
                operators_.push_back(std::move(proposed));
                account_new_operator(operators_.back());
                decision.selected_bytes =
                    sizeof(PlannerOperator);
            } else {
                operator_index = static_cast<std::uint32_t>(
                    std::distance(operators_.begin(), existing));
            }
            decision.operator_index = operator_index;
            const PlannerOperator& planner =
                operators_.at(operator_index);
            const auto kernel_started =
                std::chrono::steady_clock::now();
            const OptionKernel& kernel =
                option_kernel(state_id, operator_index);
            decision.kernel_evaluation_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<
                        std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        kernel_started)
                        .count());
            decision.raw_outcomes = outcome_count(kernel);
            decision.evidence = kernel.automatic;
            decision.selected_bytes +=
                option_kernel_selected_bytes(kernel);
            if (!program_has_prices(planner)) {
                decision.missing_price = true;
                decision.evidence.eligible = false;
                decision.evidence.legality_result =
                    "not_admitted_missing_price";
                decision.evidence.reason =
                    "automatic_candidate_missing_price";
            } else if (decision.evidence.eligible) {
                decision.admitted = true;
                admit_operator(operator_index);
                state_local_automatic_operator_indices_.insert(
                    operator_index);
                for (const std::uint32_t dependency :
                     planner.primitive_program) {
                    add_dependency(dependency);
                }
            }
            batch.decisions.push_back(std::move(decision));
            if (limits.max_state_action_rows != 0 &&
                telemetry_.state_action_rows >
                    limits.max_state_action_rows) {
                throw SolverResourceLimit(
                    "max_state_action_rows",
                    limits.max_state_action_rows);
            }
            if (limits.max_transitions != 0 &&
                telemetry_.transition_entries >
                    limits.max_transitions) {
                throw SolverResourceLimit(
                    "max_transitions", limits.max_transitions);
            }
            check_limits(true);
        }
    }
    bool local_work_merged = false;
    const auto merge_local_work = [&]() {
        if (local_work_merged) return;
        local_work_merged = true;
        const CalcTelemetry& work = local.telemetry();
        const auto add_bounded = [&](std::uint64_t& target,
                                     const std::uint64_t amount,
                                     const std::uint64_t limit,
                                     const char* cap) {
            if (limit != 0 && amount > limit - std::min(target, limit)) {
                target = limit;
                throw SolverResourceLimit(cap, limit);
            }
            target += amount;
        };
        telemetry_.distribution_requests += work.distribution_requests;
        telemetry_.distribution_hits += work.distribution_hits;
        telemetry_.distribution_misses += work.distribution_misses;
        telemetry_.distribution_build_ns += work.distribution_build_ns;
        add_bounded(
            telemetry_.state_action_rows, work.state_action_rows,
            limits.max_state_action_rows, "max_state_action_rows");
        add_bounded(
            telemetry_.transition_entries, work.transition_entries,
            limits.max_transitions, "max_transitions");
        telemetry_.outcome_entries += work.outcome_entries;
        telemetry_.choice_groups += work.choice_groups;
        telemetry_.choice_successor_entries +=
            work.choice_successor_entries;
        telemetry_.reforge_requests += work.reforge_requests;
        telemetry_.reforge_hits += work.reforge_hits;
        telemetry_.reforge_misses += work.reforge_misses;
        telemetry_.reforge_build_ns += work.reforge_build_ns;
        telemetry_.protected_retry_checks +=
            work.protected_retry_checks;
        telemetry_.protected_retry_certificates +=
            work.protected_retry_certificates;
        telemetry_.protected_retry_fallbacks +=
            work.protected_retry_fallbacks;
        telemetry_.protected_attempt_ns += work.protected_attempt_ns;
        telemetry_.protected_baseline_ns += work.protected_baseline_ns;
        telemetry_.protected_normalization_ns +=
            work.protected_normalization_ns;
        telemetry_.protected_finish_ns += work.protected_finish_ns;
        telemetry_.owned_byte_audit_requests +=
            work.owned_byte_audit_requests;
        telemetry_.owned_byte_audit_ns += work.owned_byte_audit_ns;
        telemetry_.owned_byte_ledger_requests +=
            work.owned_byte_ledger_requests;
        telemetry_.owned_byte_ledger_ns +=
            work.owned_byte_ledger_ns;
        telemetry_.owned_byte_reconciliations +=
            work.owned_byte_reconciliations;
        telemetry_.owned_byte_ledger_max_overestimate = std::max(
            telemetry_.owned_byte_ledger_max_overestimate,
            work.owned_byte_ledger_max_overestimate);
        for (std::size_t i = 0; i < kPrimitiveTelemetryFamilyCount; ++i) {
            PrimitiveFamilyTelemetry& target =
                telemetry_.primitive_families[i];
            const PrimitiveFamilyTelemetry& source =
                work.primitive_families[i];
            target.requests += source.requests;
            target.cache_hits += source.cache_hits;
            target.rows += source.rows;
            target.raw_outcomes += source.raw_outcomes;
            target.transitions += source.transitions;
            target.build_ns += source.build_ns;
            target.row_ns += source.row_ns;
            target.selected_bytes += source.selected_bytes;
        }
        consume_reforge_work(work.reforge_frontier_work);
    };

    try {
        const auto imprint_started = std::chrono::steady_clock::now();
        const ImprintDiscoveryResult imprint =
            discover_automatic_imprint_options(
                local, local_state, limits, check_limits);
        const std::uint64_t imprint_discovery_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - imprint_started)
                    .count());
        bool imprint_time_attributed = false;
        if (!imprint.specs.empty()) {
            GoalSpec imprint_goal = local_goal;
            imprint_goal.fixed_options = imprint.specs;
            std::vector<PlannerOperator> imprint_operators =
                build_planner_operators(
                    *session_, imprint_goal, registry_, local_candidates);
            for (std::uint32_t index =
                     static_cast<std::uint32_t>(registry_.actions.size());
                 index < imprint_operators.size(); ++index) {
                local.operators_.push_back(
                    std::move(imprint_operators[index]));
                local.account_new_operator(local.operators_.back());
                local_option_indices.push_back(
                    static_cast<std::uint32_t>(
                        local.operators_.size() - 1));
            }
            check_limits();
        }
        if (imprint.missing_price) {
            StateLocalAutomaticCandidate missing;
            missing.id = "automatic:imprint_discovery";
            missing.kind = AutomaticCandidateKind::Imprint;
            missing.telemetry_kind = AutomaticTelemetryKind::Imprint;
            missing.admission_ns = imprint_discovery_ns;
            imprint_time_attributed = true;
            missing.missing_price = true;
            missing.evidence.candidate = true;
            missing.evidence.legality_result =
                "not_evaluated_missing_price";
            missing.evidence.reason =
                "automatic_imprint_checkpoint_price_missing";
            batch.decisions.push_back(std::move(missing));
        }
        const auto add_imprint_boundary = [&](const char* cap,
                                              const std::uint64_t limit) {
            StateLocalAutomaticCandidate deferred;
            deferred.id = "automatic:imprint_program_discovery";
            deferred.kind = AutomaticCandidateKind::Imprint;
            deferred.telemetry_kind = AutomaticTelemetryKind::Imprint;
            if (!imprint_time_attributed) {
                deferred.admission_ns = imprint_discovery_ns;
                imprint_time_attributed = true;
            }
            deferred.deferred = true;
            deferred.evidence.candidate = true;
            deferred.evidence.kernel_change_mechanisms =
                kAutomaticImprintCheckpoint;
            deferred.evidence.legality_result = "deferred_resource_cap";
            deferred.evidence.reason =
                std::string("price_independent_kernel_generation_") + cap +
                "_limit_" + std::to_string(limit) + "_work_" +
                std::to_string(imprint.work_used);
            batch.decisions.push_back(std::move(deferred));
        };
        if (imprint.depth_deferred) {
            add_imprint_boundary(
                "max_imprint_program_depth", imprint.depth_limit);
        }
        if (imprint.work_deferred) {
            add_imprint_boundary(
                "max_imprint_program_work", imprint.work_limit);
        }

        for (const std::uint32_t action_index : permanent_benches) {
            const auto candidate_started = std::chrono::steady_clock::now();
            StateLocalAutomaticCandidate decision;
            const PlannerOperator& planner = operators_.at(action_index);
            decision.id = planner.id;
            decision.kind = planner.automatic_kind;
            decision.telemetry_kind =
                telemetry_kind_for_candidate(decision.kind);
            decision.evidence.candidate = true;
            decision.evidence.relevant_goal_mask = planner.relevant_goal_mask;
            if (!has_prices(planner)) {
                decision.missing_price = true;
                decision.evidence.legality_result =
                    "not_evaluated_missing_price";
                decision.evidence.reason =
                    "automatic_candidate_missing_price";
                decision.admission_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(decision));
                continue;
            }
            auto exact_distribution =
                std::make_shared<OutcomeDistribution>();
            pc_item_state successor = carrier;
            (void)apply_action(
                context_, &successor,
                registry_.actions.at(action_index).params);
            exact_distribution->supported = true;
            exact_distribution->entries.push_back(
                {intern_item(successor), 1.0});
            const OutcomeDistribution& distribution = *exact_distribution;
            decision.raw_outcomes = outcome_count(distribution);
            bool advances = false;
            for (const OutcomeEntry& exit : distribution.entries) {
                const AbstractState& next = state(exit.state);
                for (std::uint32_t slot = 0;
                     slot < layout_.slots.size(); ++slot) {
                    advances |=
                        (planner.relevant_goal_mask & (1u << slot)) != 0 &&
                        next.slot_status[slot] >
                            state(state_id).slot_status[slot];
                }
            }
            decision.evidence.eligible = distribution.supported && advances;
            decision.evidence.kernel_changed = advances;
            decision.evidence.setup_complete = advances;
            decision.evidence.cleanup_complete = true;
            decision.evidence.recovery_complete = true;
            decision.evidence.exits_complete = !distribution.entries.empty();
            decision.evidence.kernel_change_mechanisms =
                kAutomaticDeterministicFinish;
            decision.evidence.legality_result =
                advances ? "legal" : "irrelevant";
            decision.evidence.reason =
                advances ? "legal_permanent_goal_bench_successor"
                         : "permanent_bench_does_not_advance_goal";
            if (decision.evidence.eligible) {
                const auto resources = planner.resource_quantities;
                const auto duplicate = std::find_if(
                    seen_primitive_kernels.begin(),
                    seen_primitive_kernels.end(),
                    [&](const auto& seen) {
                        return same_complete_distribution(
                                   *seen.first, distribution) &&
                               seen.second == resources;
                    });
                if (duplicate != seen_primitive_kernels.end()) {
                    decision.collapsed = true;
                } else {
                    seen_primitive_kernels.push_back(
                        {&distribution, resources});
                    const std::uint64_t key =
                        (static_cast<std::uint64_t>(state_id) << 32) |
                        action_index;
                    account_distribution_cache_insert(
                        key, exact_distribution);
                    distribution_cache_[key] =
                        std::move(exact_distribution);
                    decision.operator_index = action_index;
                    decision.admitted = true;
                    admit_operator(action_index);
                }
            }
            decision.admission_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - candidate_started)
                    .count());
            batch.decisions.push_back(std::move(decision));
            check_limits();
        }

        std::unordered_map<
            std::string,
            std::shared_ptr<const OptionKernel>>
            temporary_evaluation_memo;
        struct ProtectedKernelComparison {
            bool supported = false;
            bool fully_legal = false;
            bool changed = false;
            std::uint64_t baseline_hash = 0;
            std::uint64_t candidate_hash = 0;
        };
        std::map<
            std::pair<std::uint32_t, std::uint32_t>,
            ProtectedKernelComparison>
            protected_kernel_comparisons;
        const auto temporary_group_for = [&](const PlannerOperator& planner)
            -> const TemporaryBenchCandidateGroup* {
            if (planner.option_kind !=
                FixedOptionKind::TemporaryBenchRepeat) {
                return nullptr;
            }
            const auto found = std::find_if(
                synthesis.temporary_groups.begin(),
                synthesis.temporary_groups.end(),
                [&](const TemporaryBenchCandidateGroup& group) {
                    return group.representative_blocker ==
                               planner.setup_action &&
                           group.followup_action ==
                               planner.followup_action &&
                           group.goal_slot < kMaxGoalSlots &&
                           planner.exit_goal_slots.size() == 1 &&
                           planner.exit_goal_slots.front() == group.goal_slot;
                });
            return found == synthesis.temporary_groups.end()
                       ? nullptr
                       : &*found;
        };
        for (const std::uint32_t local_operator : local_option_indices) {
            const auto candidate_started = std::chrono::steady_clock::now();
            const PlannerOperator& local_planner =
                local.operators().at(local_operator);
            const TemporaryBenchCandidateGroup* temporary_group =
                temporary_group_for(local_planner);
            StateLocalAutomaticCandidate base_decision;
            base_decision.id = local_planner.id;
            base_decision.kind = local_planner.automatic_kind;
            base_decision.telemetry_kind =
                telemetry_kind_for_candidate(base_decision.kind);
            const bool measure_protected =
                base_decision.telemetry_kind ==
                AutomaticTelemetryKind::ProtectedSide;
            const bool direct_fracture =
                local_planner.option_kind ==
                    FixedOptionKind::FracturePrepare &&
                local_planner.carrier_goal_slot < local.layout().slots.size() &&
                local.state(local_state).slot_status[
                    local_planner.carrier_goal_slot] ==
                    static_cast<std::uint8_t>(GoalSlotStatus::Satisfied) &&
                local_planner.conditional_action != kNoId &&
                action_legal(
                    local.session(),
                    local.registry().actions.at(
                        local_planner.conditional_action),
                    local.state(local_state));
            if (temporary_group == nullptr &&
                (!has_prices(local_planner) ||
                 (measure_protected &&
                  !program_has_prices(local_planner))) &&
                !direct_fracture) {
                base_decision.missing_price = true;
                base_decision.evidence.candidate = true;
                base_decision.evidence.relevant_goal_mask =
                    local_planner.relevant_goal_mask;
                base_decision.evidence.legality_result =
                    "not_evaluated_missing_price";
                base_decision.evidence.reason =
                    "automatic_candidate_missing_price";
                base_decision.admission_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(base_decision));
                continue;
            }
            if (temporary_group != nullptr && limits.prices != nullptr) {
                const bool common_prices =
                    action_has_prices(local_planner.followup_action) &&
                    action_has_prices(local_planner.cleanup_action);
                const bool any_priced_variant = common_prices && std::any_of(
                    temporary_group->blocker_variants.begin(),
                    temporary_group->blocker_variants.end(),
                    action_has_prices);
                if (!any_priced_variant) {
                    bool first = true;
                    for (const std::uint32_t blocker :
                         temporary_group->blocker_variants) {
                        StateLocalAutomaticCandidate missing = base_decision;
                        missing.id =
                            "option:temporary_bench_repeat:" +
                            local.registry().actions.at(blocker).id + ':' +
                            local.registry().actions.at(
                                local_planner.followup_action).id +
                            ":until:" +
                            std::to_string(
                                local_planner.exit_min_satisfied) + ':' +
                            std::to_string(
                                local_planner.exit_goal_slots.front());
                        missing.missing_price = true;
                        missing.evidence.candidate = true;
                        missing.evidence.relevant_goal_mask =
                            local_planner.relevant_goal_mask;
                        missing.evidence.legality_result =
                            "not_evaluated_missing_price";
                        missing.evidence.reason =
                            "automatic_candidate_missing_price";
                        if (first) {
                            missing.admission_ns = static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    candidate_started)
                                    .count());
                            first = false;
                        }
                        batch.decisions.push_back(std::move(missing));
                    }
                    continue;
                }
            }
            const auto kernel_evaluation_started =
                measure_protected
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            const CalcTelemetry protected_before =
                measure_protected ? local.telemetry() : CalcTelemetry{};
            const std::string evaluation_key = temporary_evaluation_key(
                local.session(), local.registry(), local_planner);
            const OptionKernel* local_kernel_ptr = nullptr;
            const auto reused = evaluation_key.empty()
                                    ? temporary_evaluation_memo.end()
                                    : temporary_evaluation_memo.find(
                                          evaluation_key);
            if (reused != temporary_evaluation_memo.end()) {
                auto kernel = std::make_shared<OptionKernel>(
                    *reused->second);
                kernel->expected_resources =
                    local_planner.resource_quantities;
                kernel->retained_template_id = 0;
                kernel->retained_template_storage = false;
                const std::uint64_t local_key =
                    (static_cast<std::uint64_t>(local_state) << 32) |
                    local_operator;
                local_kernel_ptr = kernel.get();
                local.account_option_cache_insert(local_key, kernel);
                local.option_kernel_cache_[local_key] = std::move(kernel);
            } else {
                local_kernel_ptr = &local.option_kernel(
                    local_state, local_operator);
                if (!evaluation_key.empty()) {
                    const std::uint64_t local_key =
                        (static_cast<std::uint64_t>(local_state) << 32) |
                        local_operator;
                    temporary_evaluation_memo.emplace(
                        evaluation_key,
                        local.option_kernel_cache_.at(local_key));
                }
            }
            const OptionKernel& local_kernel = *local_kernel_ptr;
            const ProtectedKernelComparison* protected_comparison = nullptr;
            if (local_planner.option_kind ==
                    FixedOptionKind::ProtectedRepeat &&
                local_kernel.automatic.eligible) {
                const std::pair<std::uint32_t, std::uint32_t> comparison_key{
                    local_planner.setup_action,
                    local_planner.followup_action};
                auto found = protected_kernel_comparisons.find(
                    comparison_key);
                if (found == protected_kernel_comparisons.end()) {
                    if (automatic_comparison_context_ == nullptr) {
                        GoalSpec comparison_goal = goal_;
                        comparison_goal.automatic_candidates = false;
                        comparison_goal.fixed_options.clear();
                        automatic_comparison_context_ =
                            std::make_unique<CalcContext>(
                                session_, comparison_goal, registry_,
                                candidates_, false, false, true,
                                solve_discovered_state_cap_);
                        automatic_comparison_context_->set_solve_resource_caps(
                            solve_discovered_state_cap_.value_or(
                                std::numeric_limits<std::uint32_t>::max()),
                            std::numeric_limits<std::uint64_t>::max(), false,
                            solve_owned_bytes_cap_);
                    }
                    CalcContext& comparison_context =
                        *automatic_comparison_context_;
                    const CalcTelemetry comparison_before =
                        comparison_context.telemetry();
                    const std::uint32_t comparison_state =
                        comparison_context.intern_item(carrier);
                    const ActionDescriptor& baseline_action =
                        comparison_context.registry().actions.at(
                            local_planner.followup_action);
                    const bool baseline_legal = action_legal(
                        comparison_context.session(), baseline_action,
                        comparison_context.state(comparison_state));
                    const OutcomeDistribution* baseline_distribution =
                        baseline_legal
                            ? &comparison_context.outcomes(
                                  comparison_state,
                                  local_planner.followup_action)
                            : nullptr;
                    const CalcTelemetry& comparison_after =
                        comparison_context.telemetry();
                    telemetry_.distribution_requests +=
                        comparison_after.distribution_requests -
                        comparison_before.distribution_requests;
                    telemetry_.distribution_hits +=
                        comparison_after.distribution_hits -
                        comparison_before.distribution_hits;
                    telemetry_.distribution_misses +=
                        comparison_after.distribution_misses -
                        comparison_before.distribution_misses;
                    telemetry_.distribution_build_ns +=
                        comparison_after.distribution_build_ns -
                        comparison_before.distribution_build_ns;
                    telemetry_.outcome_entries +=
                        comparison_after.outcome_entries -
                        comparison_before.outcome_entries;
                    telemetry_.choice_groups +=
                        comparison_after.choice_groups -
                        comparison_before.choice_groups;
                    telemetry_.choice_successor_entries +=
                        comparison_after.choice_successor_entries -
                        comparison_before.choice_successor_entries;
                    telemetry_.reforge_requests +=
                        comparison_after.reforge_requests -
                        comparison_before.reforge_requests;
                    telemetry_.reforge_hits +=
                        comparison_after.reforge_hits -
                        comparison_before.reforge_hits;
                    telemetry_.reforge_misses +=
                        comparison_after.reforge_misses -
                        comparison_before.reforge_misses;
                    telemetry_.reforge_build_ns +=
                        comparison_after.reforge_build_ns -
                        comparison_before.reforge_build_ns;
                    consume_reforge_work(
                        comparison_after.reforge_frontier_work -
                        comparison_before.reforge_frontier_work);
                    ProtectedKernelComparison comparison;
                    comparison.supported =
                        baseline_distribution != nullptr &&
                        baseline_distribution->supported &&
                        baseline_distribution->choice_groups.empty();
                    comparison.fully_legal = baseline_legal;
                    bool same_outcomes = comparison.supported &&
                        baseline_distribution->entries.size() ==
                            local_kernel
                                .automatic_candidate_attempt_entries.size();
                    if (same_outcomes) {
                        std::unordered_multimap<
                            std::size_t,
                            std::pair<const AbstractState*, double>>
                            candidate_outcomes;
                        candidate_outcomes.reserve(
                            local_kernel
                                .automatic_candidate_attempt_entries.size());
                        for (const OutcomeEntry& entry :
                             local_kernel
                                 .automatic_candidate_attempt_entries) {
                            const AbstractState& candidate_state =
                                local.state(entry.state);
                            candidate_outcomes.emplace(
                                abstract_state_hash(candidate_state),
                                std::pair{
                                    &candidate_state, entry.probability});
                        }
                        for (const OutcomeEntry& entry :
                             baseline_distribution->entries) {
                            const AbstractState& baseline_state =
                                comparison_context.state(entry.state);
                            const auto [first, last] =
                                candidate_outcomes.equal_range(
                                    abstract_state_hash(baseline_state));
                            const bool matched = std::any_of(
                                first, last, [&](const auto& candidate) {
                                    return *candidate.second.first ==
                                               baseline_state &&
                                           candidate.second.second ==
                                               entry.probability;
                                });
                            if (!matched) {
                                same_outcomes = false;
                                break;
                            }
                        }
                    }
                    comparison.changed =
                        comparison.supported && comparison.fully_legal &&
                        !same_outcomes;
                    if (baseline_distribution != nullptr) {
                        AttemptKernel baseline;
                        baseline.supported =
                            baseline_distribution->supported;
                        baseline.fully_legal = baseline_legal;
                        baseline.entries = baseline_distribution->entries;
                        comparison.baseline_hash =
                            attempt_kernel_hash(baseline);
                        comparison_context.release_outcome(
                            comparison_state,
                            local_planner.followup_action);
                    }
                    AttemptKernel candidate;
                    candidate.entries =
                        local_kernel.automatic_candidate_attempt_entries;
                    comparison.candidate_hash =
                        attempt_kernel_hash(candidate);
                    found = protected_kernel_comparisons.emplace(
                        comparison_key, comparison).first;
                }
                protected_comparison = &found->second;
            }
            if (measure_protected) {
                const CalcTelemetry& protected_after = local.telemetry();
                base_decision.kernel_evaluation_ns =
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            kernel_evaluation_started)
                            .count());
                base_decision.protected_side_evaluations =
                    local_planner.option_kind ==
                            FixedOptionKind::ProtectedSide
                        ? 1
                        : 0;
                base_decision.protected_repeat_evaluations =
                    local_planner.option_kind ==
                            FixedOptionKind::ProtectedRepeat
                        ? 1
                        : 0;
                base_decision.protected_retry_checks =
                    protected_after.protected_retry_checks -
                    protected_before.protected_retry_checks;
                base_decision.protected_retry_certificates =
                    protected_after.protected_retry_certificates -
                    protected_before.protected_retry_certificates;
                base_decision.protected_retry_fallbacks =
                    protected_after.protected_retry_fallbacks -
                    protected_before.protected_retry_fallbacks;
                base_decision.protected_attempt_ns =
                    protected_after.protected_attempt_ns -
                    protected_before.protected_attempt_ns;
                base_decision.protected_baseline_ns =
                    protected_after.protected_baseline_ns -
                    protected_before.protected_baseline_ns;
                base_decision.protected_normalization_ns =
                    protected_after.protected_normalization_ns -
                    protected_before.protected_normalization_ns;
                base_decision.protected_finish_ns =
                    protected_after.protected_finish_ns -
                    protected_before.protected_finish_ns;
            }
            base_decision.raw_outcomes = outcome_count(local_kernel);
            if (base_decision.kind == AutomaticCandidateKind::Imprint &&
                !imprint_time_attributed) {
                base_decision.admission_ns += imprint_discovery_ns;
                imprint_time_attributed = true;
            }
            base_decision.evidence = local_kernel.automatic;
            if (protected_comparison != nullptr) {
                base_decision.evidence.baseline_kernel_hash =
                    protected_comparison->baseline_hash;
                base_decision.evidence.candidate_kernel_hash =
                    protected_comparison->candidate_hash;
                base_decision.evidence.kernel_changed =
                    protected_comparison->changed;
                if (!protected_comparison->supported ||
                    !protected_comparison->fully_legal ||
                    !protected_comparison->changed) {
                    base_decision.evidence.eligible = false;
                    base_decision.evidence.legality_result = "illegal";
                    base_decision.evidence.reason =
                        !protected_comparison->supported
                            ? "exact_kernel_unsupported"
                            : !protected_comparison->fully_legal
                                  ? "one_or_more_program_steps_illegal"
                                  : "exact_successor_kernel_neutral";
                }
            }
            if (temporary_group == nullptr && limits.prices != nullptr &&
                std::any_of(
                    local_kernel.expected_resources.begin(),
                    local_kernel.expected_resources.end(),
                    [&](const auto& resource) {
                        return !limits.prices->contains(resource.first);
                    })) {
                base_decision.missing_price = true;
                base_decision.evidence.legality_result =
                    "not_admitted_missing_price";
                base_decision.evidence.reason =
                    "automatic_candidate_missing_price";
                base_decision.admission_ns += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(base_decision));
                continue;
            }
            bool collapse_non_temporary = false;
            if (base_decision.evidence.eligible &&
                temporary_group == nullptr) {
                const auto duplicate = std::find_if(
                    seen_option_kernels.begin(), seen_option_kernels.end(),
                    [&](const OptionKernel* seen) {
                        return same_complete_option_kernel(
                            *seen, local_kernel);
                    });
                if (duplicate != seen_option_kernels.end()) {
                    collapse_non_temporary = true;
                } else {
                    seen_option_kernels.push_back(&local_kernel);
                }
            }
            if (!base_decision.evidence.eligible || collapse_non_temporary) {
                base_decision.collapsed = collapse_non_temporary;
                base_decision.admission_ns += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(base_decision));
                check_limits();
                continue;
            }

            std::vector<PlannerOperator> admitted_variants;
            if (temporary_group == nullptr) {
                PlannerOperator admitted = local_planner;
                admitted.resource_quantities = local_kernel.expected_resources;
                admitted_variants.push_back(std::move(admitted));
            } else {
                admitted_variants.reserve(
                    temporary_group->blocker_variants.size());
                for (const std::uint32_t blocker :
                     temporary_group->blocker_variants) {
                    admitted_variants.push_back(temporary_variant_planner(
                        local.registry(), local_planner, local_kernel,
                        blocker));
                }
            }

            std::vector<PlannerOperator> priced_variants;
            priced_variants.reserve(admitted_variants.size());
            const std::size_t first_variant_decision =
                batch.decisions.size();
            for (PlannerOperator& admitted : admitted_variants) {
                if (!has_prices(admitted)) {
                    StateLocalAutomaticCandidate missing = base_decision;
                    missing.id = admitted.id;
                    missing.raw_outcomes = 0;
                    missing.missing_price = true;
                    missing.evidence.legality_result =
                        "not_admitted_missing_price";
                    missing.evidence.reason =
                        "automatic_candidate_missing_price";
                    batch.decisions.push_back(std::move(missing));
                    continue;
                }
                double exact_immediate_cost = 0.0;
                if (limits.prices != nullptr) {
                    for (const auto& [key, quantity] :
                         admitted.resource_quantities) {
                        exact_immediate_cost +=
                            limits.prices->at(key) * quantity;
                    }
                }
                if (std::isfinite(limits.incumbent_upper_bound) &&
                    exact_immediate_cost >
                        limits.incumbent_upper_bound + 1e-12) {
                    StateLocalAutomaticCandidate dominated = base_decision;
                    dominated.id = admitted.id;
                    dominated.raw_outcomes = 0;
                    dominated.evidence.eligible = false;
                    dominated.evidence.legality_result =
                        "dominated_by_incumbent";
                    dominated.evidence.reason =
                        "exact_expected_cost_exceeds_feasible_state_upper";
                    batch.decisions.push_back(std::move(dominated));
                    continue;
                }
                priced_variants.push_back(std::move(admitted));
            }
            if (priced_variants.empty()) {
                const std::uint64_t elapsed = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                if (batch.decisions.size() > first_variant_decision) {
                    batch.decisions[first_variant_decision].admission_ns +=
                        elapsed;
                } else {
                    base_decision.admission_ns +=
                        elapsed;
                    batch.decisions.push_back(std::move(base_decision));
                }
                check_limits();
                continue;
            }

            const auto outcome_mapping_started =
                measure_protected
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            auto mapped = std::make_shared<OptionKernel>(
                map_local_option_kernel(
                    local, *this, local_kernel, mapped_states));
            if (protected_comparison != nullptr) {
                mapped->automatic.baseline_kernel_hash =
                    base_decision.evidence.baseline_kernel_hash;
                mapped->automatic.kernel_changed =
                    base_decision.evidence.kernel_changed;
                mapped->automatic.eligible =
                    base_decision.evidence.eligible;
                mapped->automatic.legality_result =
                    base_decision.evidence.legality_result;
                mapped->automatic.reason =
                    base_decision.evidence.reason;
            }
            if (measure_protected) {
                base_decision.outcome_mapping_ns =
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            outcome_mapping_started)
                            .count());
            }
            const auto template_matching_started =
                measure_protected
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            const std::uint64_t transition_template_id =
                option_transition_hash(*mapped);
            mapped->retained_template_id = transition_template_id;
            std::shared_ptr<const OptionKernel> retained_kernel;
            bool transition_template_hit = false;
            bool new_transition_template = false;
            const auto transition_bucket =
                option_transition_templates_.find(transition_template_id);
            if (transition_bucket != option_transition_templates_.end()) {
                for (const auto& candidate : transition_bucket->second) {
                    if (!same_option_transition_kernel(*candidate, *mapped)) {
                        continue;
                    }
                    retained_kernel = candidate;
                    transition_template_hit = true;
                    break;
                }
            }
            if (retained_kernel == nullptr) {
                retained_kernel = mapped;
                auto& transition_templates =
                    option_transition_templates_[transition_template_id];
                const std::size_t old_capacity =
                    transition_templates.capacity();
                transition_templates.push_back(retained_kernel);
                account_transition_template_insert(
                    old_capacity, retained_kernel);
                new_transition_template = true;
            }

            bool first_variant = true;
            for (PlannerOperator& admitted : priced_variants) {
                StateLocalAutomaticCandidate decision = base_decision;
                decision.id = admitted.id;
                decision.raw_outcomes = first_variant
                                            ? base_decision.raw_outcomes
                                            : 0;
                decision.template_id = transition_template_id;
                decision.template_hit = transition_template_hit ||
                                        !first_variant;
                std::uint32_t operator_index = kNoId;
                bool new_operator = false;
                const std::uint64_t planner_id =
                    option_planner_hash(admitted);
                const auto planner_bucket =
                    option_operator_templates_.find(planner_id);
                if (planner_bucket != option_operator_templates_.end()) {
                    for (const std::uint32_t candidate :
                         planner_bucket->second) {
                        if (candidate < operators_.size() &&
                            same_option_template_planner(
                                operators_.at(candidate), admitted)) {
                            operator_index = candidate;
                            break;
                        }
                    }
                }
                if (operator_index == kNoId) {
                    operator_index = static_cast<std::uint32_t>(
                        operators_.size());
                    operators_.push_back(admitted);
                    account_new_operator(operators_.back());
                    auto& operator_templates =
                        option_operator_templates_[planner_id];
                    const std::size_t old_capacity =
                        operator_templates.capacity();
                    operator_templates.push_back(operator_index);
                    account_operator_template_insert(
                        old_capacity, operator_templates);
                    new_operator = true;
                }
                if (new_operator) {
                    decision.selected_bytes = sizeof(PlannerOperator);
                }
                if (new_transition_template && first_variant) {
                    decision.selected_bytes +=
                        option_kernel_selected_bytes(*retained_kernel);
                }
                const std::uint64_t key =
                    (static_cast<std::uint64_t>(state_id) << 32) |
                    operator_index;
                account_option_cache_insert(key, retained_kernel);
                option_kernel_cache_[key] = retained_kernel;
                if (decision.template_hit) {
                    option_kernel_template_hit_keys_.insert(key);
                }
                state_local_automatic_operator_indices_.insert(
                    operator_index);
                decision.operator_index = operator_index;
                decision.admitted = true;
                admit_operator(operator_index);
                if (new_operator) {
                    for (const std::uint32_t dependency :
                         operators_.at(operator_index).primitive_program) {
                        add_dependency(dependency);
                    }
                    if (operators_.at(operator_index).conditional_action !=
                        kNoId) {
                        add_dependency(
                            operators_.at(operator_index).conditional_action);
                    }
                }
                if (first_variant) {
                    if (decision.telemetry_kind ==
                        AutomaticTelemetryKind::ProtectedSide) {
                        decision.template_matching_ns =
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    template_matching_started)
                                    .count());
                    }
                    decision.admission_ns += static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            candidate_started)
                            .count());
                }
                batch.decisions.push_back(std::move(decision));
                first_variant = false;
            }
            check_limits();
        }

        if (!imprint_time_attributed && imprint_discovery_ns != 0) {
            StateLocalAutomaticCandidate timing;
            timing.id = "automatic:imprint_discovery";
            timing.kind = AutomaticCandidateKind::Imprint;
            timing.telemetry_kind = AutomaticTelemetryKind::Imprint;
            timing.admission_ns = imprint_discovery_ns;
            timing.evidence.candidate = true;
            timing.evidence.legality_result = "not_applicable";
            timing.evidence.reason = "no_legal_imprint_checkpoint_carrier";
            batch.decisions.push_back(std::move(timing));
        }

        check_limits(true);
        merge_local_work();
    } catch (const SolverResourceLimit& limit) {
        if (!local_work_merged) {
            try {
                merge_local_work();
            } catch (const SolverResourceLimit&) {
                /* The deferred witness below owns the exact cap name from
                 * the operation that first stopped admission. */
            }
        }
        StateLocalAutomaticCandidate deferred;
        deferred.id = "automatic:state_local_generation";
        deferred.deferred = true;
        deferred.evidence.candidate = true;
        deferred.evidence.legality_result = "deferred_resource_cap";
        deferred.evidence.reason =
            "price_independent_kernel_generation_" + limit.cap_name();
        batch.decisions.push_back(std::move(deferred));
    }
    std::sort(
        batch.admitted_operators.begin(), batch.admitted_operators.end());
    batch.admitted_operators.erase(
        std::unique(
            batch.admitted_operators.begin(),
            batch.admitted_operators.end()),
        batch.admitted_operators.end());
    const auto [stored, inserted] =
        state_local_automatic_operators_.emplace(
            state_id, batch.admitted_operators);
    if (inserted) account_state_local_operators(stored->second);
    return batch;
}

} // namespace solver
} // namespace poecraft
