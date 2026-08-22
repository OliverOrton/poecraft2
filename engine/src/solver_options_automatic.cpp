#include "solver_options_runtime_helpers.hpp"

namespace poecraft {
namespace solver {

namespace {

bool same_automatic_admission_limits(
    const AutomaticAdmissionLimits& left,
    const AutomaticAdmissionLimits& right) {
    return left.max_state_action_rows == right.max_state_action_rows &&
           left.max_transitions == right.max_transitions &&
           left.max_solver_owned_bytes == right.max_solver_owned_bytes &&
           left.max_imprint_program_depth ==
               right.max_imprint_program_depth &&
           left.max_imprint_program_work ==
               right.max_imprint_program_work &&
           left.consider_imprint_programs ==
               right.consider_imprint_programs &&
           left.prices == right.prices &&
           left.incumbent_upper_bound == right.incumbent_upper_bound;
}

std::uint64_t automatic_cursor_add(
    const std::uint64_t left,
    const std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t automatic_cursor_string_bytes(const std::string& value) {
    return value.capacity() + 1;
}

std::uint64_t fixed_option_spec_nested_bytes(
    const FixedOptionSpec& spec) {
    std::uint64_t bytes = automatic_cursor_string_bytes(spec.action_id);
    bytes = automatic_cursor_add(
        bytes, automatic_cursor_string_bytes(
                   spec.constructive_finish_action_id));
    const auto strings = [&](const std::vector<std::string>& values) {
        std::uint64_t selected =
            values.capacity() * sizeof(std::string);
        for (const std::string& value : values) {
            selected = automatic_cursor_add(
                selected, automatic_cursor_string_bytes(value));
        }
        return selected;
    };
    bytes = automatic_cursor_add(bytes, strings(spec.setup_action_ids));
    bytes = automatic_cursor_add(bytes, strings(spec.bench_craft_ids));
    bytes = automatic_cursor_add(bytes, strings(spec.program_action_ids));
    return automatic_cursor_add(
        bytes,
        spec.exit_goal_slots.capacity() * sizeof(std::uint32_t));
}

std::uint64_t goal_spec_cursor_bytes(const GoalSpec& goal) {
    std::uint64_t bytes =
        goal.slots.capacity() * sizeof(GoalSlot) +
        goal.fixed_options.capacity() * sizeof(FixedOptionSpec);
    for (const FixedOptionSpec& spec : goal.fixed_options) {
        bytes = automatic_cursor_add(
            bytes, fixed_option_spec_nested_bytes(spec));
    }
    return bytes;
}

std::uint64_t planner_operator_cursor_bytes(
    const PlannerOperator& planner) {
    std::uint64_t bytes = 0;
    const std::array<const std::string*, 12> strings{
        &planner.id,
        &planner.display_name,
        &planner.primitive_action_id,
        &planner.conditional_action_id,
        &planner.bestiary_create_action_id,
        &planner.bestiary_restore_action_id,
        &planner.setup_action_id,
        &planner.followup_action_id,
        &planner.cleanup_action_id,
        &planner.constructive_finish_action_id,
        nullptr,
        nullptr};
    for (const std::string* value : strings) {
        if (value != nullptr) {
            bytes = automatic_cursor_add(
                bytes, automatic_cursor_string_bytes(*value));
        }
    }
    bytes = automatic_cursor_add(
        bytes,
        planner.primitive_program.capacity() * sizeof(std::uint32_t));
    bytes = automatic_cursor_add(
        bytes,
        planner.exit_goal_slots.capacity() * sizeof(std::uint32_t));
    bytes = automatic_cursor_add(
        bytes,
        planner.primitive_program_action_ids.capacity() *
            sizeof(std::string));
    for (const std::string& value :
         planner.primitive_program_action_ids) {
        bytes = automatic_cursor_add(
            bytes, automatic_cursor_string_bytes(value));
    }
    bytes = automatic_cursor_add(
        bytes,
        planner.resource_quantities.capacity() *
            sizeof(std::pair<std::string, double>));
    for (const auto& [key, unused_quantity] :
         planner.resource_quantities) {
        (void)unused_quantity;
        bytes = automatic_cursor_add(
            bytes, automatic_cursor_string_bytes(key));
    }
    return bytes;
}

std::uint64_t automatic_decisions_cursor_bytes(
    const std::vector<StateLocalAutomaticCandidate>& decisions) {
    std::uint64_t bytes = decisions.capacity() *
        sizeof(StateLocalAutomaticCandidate);
    for (const StateLocalAutomaticCandidate& decision : decisions) {
        bytes = automatic_cursor_add(
            bytes, automatic_cursor_string_bytes(decision.id));
        bytes = automatic_cursor_add(
            bytes,
            automatic_cursor_string_bytes(
                decision.evidence.legality_result));
        bytes = automatic_cursor_add(
            bytes,
            automatic_cursor_string_bytes(decision.evidence.reason));
    }
    return bytes;
}

std::uint64_t automatic_batch_cursor_bytes(
    const StateLocalAutomaticBatch& batch) {
    std::uint64_t bytes = automatic_decisions_cursor_bytes(batch.decisions);
    bytes = automatic_cursor_add(
        bytes,
        batch.admitted_operators.capacity() * sizeof(std::uint32_t));
    bytes = automatic_cursor_add(
        bytes, automatic_cursor_string_bytes(batch.resource_cap));
    bytes = automatic_cursor_add(
        bytes, automatic_cursor_string_bytes(batch.resource_reason));
    return bytes;
}

std::uint64_t synthesis_cursor_bytes(
    const AutomaticOptionSynthesis& synthesis) {
    std::uint64_t bytes =
        synthesis.specs.capacity() * sizeof(FixedOptionSpec) +
        synthesis.temporary_groups.capacity() *
            sizeof(TemporaryBenchCandidateGroup);
    for (const FixedOptionSpec& spec : synthesis.specs) {
        bytes = automatic_cursor_add(
            bytes, fixed_option_spec_nested_bytes(spec));
    }
    for (const TemporaryBenchCandidateGroup& group :
         synthesis.temporary_groups) {
        bytes = automatic_cursor_add(
            bytes,
            group.blocker_variants.capacity() * sizeof(std::uint32_t));
    }
    return bytes;
}

} // namespace

StateLocalAutomaticBatch CalcContext::admit_state_local_automatic_candidates(
    const std::uint32_t state_id,
    const AutomaticAdmissionLimits& limits) {
    StateLocalAutomaticBatch completed;
    while (!advance_state_local_automatic_candidates(
        state_id, limits, completed,
        std::numeric_limits<std::uint32_t>::max())) {
    }
    return completed;
}

bool CalcContext::advance_state_local_automatic_candidates(
    const std::uint32_t state_id,
    const AutomaticAdmissionLimits& limits,
    StateLocalAutomaticBatch& completed,
    const std::uint32_t max_checkpoints) {
    if (!state_local_automatic_admission_cursor_.has_value()) {
        StateLocalAutomaticAdmissionCursor created;
        created.state_id = state_id;
        created.limits = limits;
        created.first_operator = operators_.size();
        initialize_state_local_automatic_transaction(created);
        created.task = build_state_local_automatic_candidates(
            state_id, limits);
        state_local_automatic_admission_cursor_ = std::move(created);
    } else if (state_local_automatic_admission_cursor_->state_id !=
               state_id) {
        throw std::logic_error(
            "CalcContext already has a different carrier-local automatic "
            "admission in flight");
    } else if (!same_automatic_admission_limits(
                   state_local_automatic_admission_cursor_->limits,
                   limits)) {
        throw std::invalid_argument(
            "state-local automatic admission resumed with different limits");
    }
    StateLocalAutomaticAdmissionCursor& cursor =
        *state_local_automatic_admission_cursor_;

    const std::uint32_t checkpoints =
        std::max<std::uint32_t>(1, max_checkpoints);
    try {
        for (std::uint32_t i = 0; i < checkpoints; ++i) {
            const auto slice_started = std::chrono::steady_clock::now();
            const bool done = cursor.task.resume();
            const std::uint64_t slice_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - slice_started)
                    .count());
            ++cursor.resumes;
            cursor.max_slice_ns = std::max(cursor.max_slice_ns, slice_ns);
            if (!done) {
                ++cursor.suspensions;
                if (limits.max_solver_owned_bytes != 0 &&
                    fast_estimated_owned_bytes() >
                        limits.max_solver_owned_bytes) {
                    throw SolverResourceLimit(
                        "max_solver_owned_bytes",
                        limits.max_solver_owned_bytes);
                }
                continue;
            }
            completed = cursor.task.take_result();
            completed.continuation_resumes = cursor.resumes;
            completed.continuation_suspensions = cursor.suspensions;
            completed.max_continuation_slice_ns = cursor.max_slice_ns;
            if (completed.status ==
                StateLocalAutomaticBatchStatus::ResourceDeferred) {
                rollback_state_local_automatic_transaction(cursor);
            }
            deactivate_automatic_admission_context();
            state_local_automatic_admission_cursor_.reset();
            return true;
        }
    } catch (...) {
        rollback_state_local_automatic_transaction(cursor);
        deactivate_automatic_admission_context();
        state_local_automatic_admission_cursor_.reset();
        throw;
    }
    return false;
}

void CalcContext::cancel_state_local_automatic_candidates(
    const std::uint32_t state_id) {
    if (!state_local_automatic_admission_cursor_.has_value() ||
        state_local_automatic_admission_cursor_->state_id != state_id) {
        return;
    }
    rollback_state_local_automatic_transaction(
        *state_local_automatic_admission_cursor_);
    deactivate_automatic_admission_context();
    state_local_automatic_admission_cursor_.reset();
}

void CalcContext::cancel_state_local_automatic_candidates() {
    if (!state_local_automatic_admission_cursor_.has_value()) return;
    rollback_state_local_automatic_transaction(
        *state_local_automatic_admission_cursor_);
    deactivate_automatic_admission_context();
    state_local_automatic_admission_cursor_.reset();
}

std::uint64_t CalcContext::automatic_admission_cursor_bytes() const {
    if (!state_local_automatic_admission_cursor_.has_value()) return 0;
    const StateLocalAutomaticAdmissionCursor& cursor =
        *state_local_automatic_admission_cursor_;
    std::uint64_t bytes = cursor.task.retained_bytes();
    bytes = automatic_cursor_add(
        bytes,
        cursor.distribution_cache_keys_before.capacity() *
            sizeof(std::uint64_t));
    bytes = automatic_cursor_add(
        bytes,
        cursor.option_cache_keys_before.capacity() *
            sizeof(std::uint64_t));
    bytes = automatic_cursor_add(
        bytes,
        cursor.distribution_cache_keys_inserted.capacity() *
            sizeof(std::uint64_t));
    bytes = automatic_cursor_add(
        bytes,
        cursor.option_cache_keys_inserted.capacity() *
            sizeof(std::uint64_t));
    bytes = automatic_cursor_add(
        bytes,
        cursor.option_template_hit_keys_before.capacity() *
            sizeof(std::uint64_t));
    bytes = automatic_cursor_add(
        bytes,
        cursor.option_template_buckets_before.capacity() *
            sizeof(AutomaticTemplateBucketCheckpoint));
    bytes = automatic_cursor_add(
        bytes,
        cursor.transition_template_buckets_before.capacity() *
            sizeof(AutomaticTemplateBucketCheckpoint));
    bytes = automatic_cursor_add(
        bytes,
        cursor.operator_template_buckets_before.capacity() *
            sizeof(AutomaticTemplateBucketCheckpoint));
    return automatic_cursor_add(
        bytes,
        cursor.reforge_buckets_before.capacity() *
            sizeof(AutomaticReforgeBucketCheckpoint));
}

solve_detail::CooperativeTask<StateLocalAutomaticBatch>
CalcContext::build_state_local_automatic_candidates(
    const std::uint32_t state_id,
    AutomaticAdmissionLimits limits) {
    StateLocalAutomaticBatch batch;
    struct PublicationStaging {
        std::vector<std::uint32_t> candidate_operators;
        std::unordered_set<std::uint32_t> state_local_indices;
        std::unordered_set<std::uint32_t> dependencies;
        std::unordered_map<
            std::uint32_t, std::vector<std::uint32_t>> carrier_operators;
        std::size_t state_local_target_buckets = 0;
        std::size_t dependency_target_buckets = 0;
        std::size_t carrier_target_buckets = 0;
        std::size_t state_local_parent_bucket_upper = 0;
        std::size_t dependency_parent_bucket_upper = 0;
        std::size_t carrier_parent_bucket_upper = 0;
        std::uint64_t projected_parent_bucket_growth = 0;
        std::uint64_t automatic_option_additions = 0;
        std::uint64_t automatic_dependency_additions = 0;
        bool replace_candidate_operators = false;
    };
    const auto unique_missing_count = []<typename Parent>(
        const std::vector<std::uint32_t>& values,
        const Parent& parent) {
        std::size_t count = 0;
        for (std::size_t position = 0; position < values.size(); ++position) {
            const std::uint32_t value = values[position];
            if constexpr (requires { parent.contains(value); }) {
                if (parent.contains(value)) continue;
            } else {
                if (std::find(parent.begin(), parent.end(), value) !=
                    parent.end()) {
                    continue;
                }
            }
            if (std::find(
                    values.begin(), values.begin() + position, value) ==
                values.begin() + position) {
                ++count;
            }
        }
        return count;
    };
    /* The concrete native/WASM standard libraries both select a bucket count
     * below four times the minimum requested count. Keep a deliberately loose
     * selected-allocation authority and verify the implementation result after
     * staging, so no staging allocation can silently escape this preflight. */
    const auto publication_bucket_upper = [](
        const std::size_t elements,
        const float max_load_factor) {
        if (elements == 0) return std::size_t{0};
        const long double load =
            std::isfinite(max_load_factor) && max_load_factor > 0.0f
                ? static_cast<long double>(max_load_factor)
                : 1.0L;
        const long double minimum =
            static_cast<long double>(elements) / load;
        const std::size_t requested = minimum >=
                static_cast<long double>(
                    std::numeric_limits<std::size_t>::max())
            ? std::numeric_limits<std::size_t>::max()
            : static_cast<std::size_t>(minimum) + 1;
        if (requested >
            (std::numeric_limits<std::size_t>::max() - 64) / 4) {
            return std::numeric_limits<std::size_t>::max();
        }
        return 4 * requested + 64;
    };
    const auto publication_vector_upper = [](
        const std::size_t elements) {
        if (elements == 0) return std::size_t{0};
        if (elements >
            (std::numeric_limits<std::size_t>::max() - 16) / 2) {
            return std::numeric_limits<std::size_t>::max();
        }
        return 2 * elements + 16;
    };
    const auto publication_staging_projection = [&]
        (const std::vector<std::uint32_t>& admitted,
         const std::vector<std::uint32_t>& state_local,
         const std::vector<std::uint32_t>& dependencies) {
        const std::size_t candidate_additions = unique_missing_count(
            admitted, candidate_operators_);
        const std::size_t state_local_additions = unique_missing_count(
            state_local, state_local_automatic_operator_indices_);
        const std::size_t dependency_additions = unique_missing_count(
            dependencies, admitted_automatic_dependencies_);
        const std::size_t state_local_elements =
            state_local_automatic_operator_indices_.size() +
            state_local_additions;
        const std::size_t dependency_elements =
            admitted_automatic_dependencies_.size() +
            dependency_additions;
        const std::size_t carrier_elements =
            state_local_automatic_operators_.size() + 1;
        const std::size_t state_local_bucket_upper =
            state_local_additions == 0
                ? 0
                : publication_bucket_upper(
                      state_local_elements,
                      state_local_automatic_operator_indices_
                          .max_load_factor());
        const std::size_t dependency_bucket_upper =
            dependency_additions == 0
                ? 0
                : publication_bucket_upper(
                      dependency_elements,
                      admitted_automatic_dependencies_.max_load_factor());
        const std::size_t carrier_bucket_upper =
            publication_bucket_upper(
                carrier_elements,
                state_local_automatic_operators_.max_load_factor());
        std::uint64_t bytes = 0;
        const auto add_product = [&]
            (const std::size_t count, const std::size_t width) {
            if (count >
                std::numeric_limits<std::uint64_t>::max() / width) {
                bytes = std::numeric_limits<std::uint64_t>::max();
                return;
            }
            bytes = automatic_cursor_add(
                bytes, static_cast<std::uint64_t>(count) * width);
        };
        /* new_candidates and its final candidate copy coexist until the
         * staging call returns. */
        add_product(
            publication_vector_upper(candidate_additions),
            sizeof(std::uint32_t));
        if (candidate_additions != 0) {
            add_product(
                publication_vector_upper(
                    candidate_operators_.size() + candidate_additions),
                sizeof(std::uint32_t));
        }
        add_product(state_local_bucket_upper, sizeof(void*));
        add_product(
            state_local_additions,
            sizeof(std::uint32_t) + 2 * sizeof(void*));
        add_product(dependency_bucket_upper, sizeof(void*));
        add_product(
            dependency_additions,
            sizeof(std::uint32_t) + 2 * sizeof(void*));
        add_product(carrier_bucket_upper, sizeof(void*));
        add_product(
            1,
            sizeof(std::pair<
                const std::uint32_t, std::vector<std::uint32_t>>) +
                2 * sizeof(void*));
        add_product(
            publication_vector_upper(admitted.size()),
            sizeof(std::uint32_t));
        /* The old parent bucket arrays are already in fast owned bytes. A
         * growing rehash transiently allocates the complete new array while
         * those old arrays remain live. */
        if (state_local_bucket_upper >
            state_local_automatic_operator_indices_.bucket_count()) {
            add_product(state_local_bucket_upper, sizeof(void*));
        }
        if (dependency_bucket_upper >
            admitted_automatic_dependencies_.bucket_count()) {
            add_product(dependency_bucket_upper, sizeof(void*));
        }
        if (carrier_bucket_upper >
            state_local_automatic_operators_.bucket_count()) {
            add_product(carrier_bucket_upper, sizeof(void*));
        }
        return bytes;
    };
    const auto stage_publication = [&]
        (const std::vector<std::uint32_t>& admitted,
         const std::vector<std::uint32_t>& state_local,
         const std::vector<std::uint32_t>& dependencies) {
        PublicationStaging staged;
        const std::size_t candidate_additions = unique_missing_count(
            admitted, candidate_operators_);
        std::vector<std::uint32_t> new_candidates;
        new_candidates.reserve(candidate_additions);
        for (const std::uint32_t index : admitted) {
            if (std::find(
                    candidate_operators_.begin(),
                    candidate_operators_.end(), index) ==
                    candidate_operators_.end() &&
                std::find(
                    new_candidates.begin(), new_candidates.end(), index) ==
                    new_candidates.end()) {
                new_candidates.push_back(index);
            }
        }
        if (!new_candidates.empty()) {
            staged.candidate_operators.reserve(
                candidate_operators_.size() + new_candidates.size());
            staged.candidate_operators.insert(
                staged.candidate_operators.end(),
                candidate_operators_.begin(), candidate_operators_.end());
            staged.candidate_operators.insert(
                staged.candidate_operators.end(),
                new_candidates.begin(), new_candidates.end());
            staged.automatic_option_additions = new_candidates.size();
            staged.replace_candidate_operators = true;
        }
        if (new_candidates.capacity() >
                publication_vector_upper(candidate_additions) ||
            staged.candidate_operators.capacity() >
                publication_vector_upper(
                    candidate_operators_.size() + candidate_additions)) {
            throw std::logic_error(
                "automatic publication vector projection was exceeded");
        }

        staged.state_local_indices.max_load_factor(
            state_local_automatic_operator_indices_.max_load_factor());
        const std::size_t state_local_additions = unique_missing_count(
            state_local, state_local_automatic_operator_indices_);
        if (state_local_additions != 0) {
            staged.state_local_indices.reserve(
                state_local_automatic_operator_indices_.size() +
                state_local_additions);
        }
        for (const std::uint32_t index : state_local) {
            if (!state_local_automatic_operator_indices_.contains(index)) {
                staged.state_local_indices.insert(index);
            }
        }
        staged.state_local_target_buckets =
            staged.state_local_indices.empty()
                ? state_local_automatic_operator_indices_.bucket_count()
                : std::max(
                      state_local_automatic_operator_indices_.bucket_count(),
                      staged.state_local_indices.bucket_count());

        staged.dependencies.max_load_factor(
            admitted_automatic_dependencies_.max_load_factor());
        const std::size_t dependency_additions = unique_missing_count(
            dependencies, admitted_automatic_dependencies_);
        if (dependency_additions != 0) {
            staged.dependencies.reserve(
                admitted_automatic_dependencies_.size() +
                dependency_additions);
        }
        for (const std::uint32_t action : dependencies) {
            if (!admitted_automatic_dependencies_.contains(action)) {
                staged.dependencies.insert(action);
            }
        }
        staged.dependency_target_buckets =
            staged.dependencies.empty()
                ? admitted_automatic_dependencies_.bucket_count()
                : std::max(
                      admitted_automatic_dependencies_.bucket_count(),
                      staged.dependencies.bucket_count());
        staged.automatic_dependency_additions =
            staged.dependencies.size();

        staged.carrier_operators.max_load_factor(
            state_local_automatic_operators_.max_load_factor());
        staged.carrier_operators.reserve(
            state_local_automatic_operators_.size() + 1);
        std::vector<std::uint32_t> carrier_copy;
        carrier_copy.reserve(admitted.size());
        carrier_copy.insert(
            carrier_copy.end(), admitted.begin(), admitted.end());
        staged.carrier_operators.emplace(
            state_id, std::move(carrier_copy));
        staged.carrier_target_buckets = std::max(
            state_local_automatic_operators_.bucket_count(),
            staged.carrier_operators.bucket_count());

        staged.state_local_parent_bucket_upper = std::max(
            state_local_automatic_operator_indices_.bucket_count(),
            state_local_additions == 0
                ? std::size_t{0}
                : publication_bucket_upper(
                      state_local_automatic_operator_indices_.size() +
                          state_local_additions,
                      state_local_automatic_operator_indices_
                          .max_load_factor()));
        staged.dependency_parent_bucket_upper = std::max(
            admitted_automatic_dependencies_.bucket_count(),
            dependency_additions == 0
                ? std::size_t{0}
                : publication_bucket_upper(
                      admitted_automatic_dependencies_.size() +
                          dependency_additions,
                      admitted_automatic_dependencies_.max_load_factor()));
        staged.carrier_parent_bucket_upper = std::max(
            state_local_automatic_operators_.bucket_count(),
            publication_bucket_upper(
                state_local_automatic_operators_.size() + 1,
                state_local_automatic_operators_.max_load_factor()));

        const auto add_rehash_peak = [&]
            (const std::size_t before,
             const std::size_t after,
             const std::size_t upper) {
            if (after <= before) return;
            /* unordered rehash allocates the complete new bucket array while
             * the old parent array is still live. The parent ledger already
             * owns the old array, so the checkpoint must add the full target
             * array, not merely the eventual net growth. */
            staged.projected_parent_bucket_growth =
                automatic_cursor_add(
                    staged.projected_parent_bucket_growth,
                    upper >
                            std::numeric_limits<std::uint64_t>::max() /
                                sizeof(void*)
                        ? std::numeric_limits<std::uint64_t>::max()
                        : static_cast<std::uint64_t>(upper) *
                              sizeof(void*));
        };
        add_rehash_peak(
            state_local_automatic_operator_indices_.bucket_count(),
            staged.state_local_target_buckets,
            staged.state_local_parent_bucket_upper);
        add_rehash_peak(
            admitted_automatic_dependencies_.bucket_count(),
            staged.dependency_target_buckets,
            staged.dependency_parent_bucket_upper);
        add_rehash_peak(
            state_local_automatic_operators_.bucket_count(),
            staged.carrier_target_buckets,
            staged.carrier_parent_bucket_upper);
        if ((state_local_additions != 0 &&
             staged.state_local_indices.bucket_count() >
                publication_bucket_upper(
                    state_local_automatic_operator_indices_.size() +
                        state_local_additions,
                    staged.state_local_indices.max_load_factor())) ||
            (dependency_additions != 0 &&
             staged.dependencies.bucket_count() >
                publication_bucket_upper(
                    admitted_automatic_dependencies_.size() +
                        dependency_additions,
                    staged.dependencies.max_load_factor())) ||
            staged.carrier_operators.bucket_count() >
                publication_bucket_upper(
                    state_local_automatic_operators_.size() + 1,
                    staged.carrier_operators.max_load_factor())) {
            throw std::logic_error(
                "automatic publication bucket projection was exceeded");
        }
        const auto carrier_entry =
            staged.carrier_operators.find(state_id);
        if (carrier_entry == staged.carrier_operators.end() ||
            carrier_entry->second.capacity() >
                publication_vector_upper(admitted.size())) {
            throw std::logic_error(
                "automatic publication carrier vector projection was "
                "exceeded");
        }
        return staged;
    };
    const auto publication_staging_bytes = [&]
        (const PublicationStaging& staged) {
        std::uint64_t bytes =
            staged.candidate_operators.capacity() *
            sizeof(std::uint32_t);
        if (!staged.state_local_indices.empty()) {
            bytes = automatic_cursor_add(
                bytes,
                staged.state_local_indices.bucket_count() * sizeof(void*) +
                    staged.state_local_indices.size() *
                        (sizeof(std::uint32_t) + 2 * sizeof(void*)));
        }
        if (!staged.dependencies.empty()) {
            bytes = automatic_cursor_add(
                bytes,
                staged.dependencies.bucket_count() * sizeof(void*) +
                    staged.dependencies.size() *
                        (sizeof(std::uint32_t) + 2 * sizeof(void*)));
        }
        bytes = automatic_cursor_add(
            bytes,
            staged.carrier_operators.bucket_count() * sizeof(void*) +
                staged.carrier_operators.size() *
                    (sizeof(std::pair<
                         const std::uint32_t,
                         std::vector<std::uint32_t>>) +
                     2 * sizeof(void*)));
        for (const auto& [unused, indices] :
             staged.carrier_operators) {
            (void)unused;
            bytes = automatic_cursor_add(
                bytes,
                indices.capacity() * sizeof(std::uint32_t));
        }
        return automatic_cursor_add(
            bytes, staged.projected_parent_bucket_growth);
    };
    const auto commit_publication = [&]
        (PublicationStaging& staged) {
        if (staged.state_local_target_buckets >
            state_local_automatic_operator_indices_.bucket_count()) {
            state_local_automatic_operator_indices_.rehash(
                staged.state_local_target_buckets);
        }
        if (staged.dependency_target_buckets >
            admitted_automatic_dependencies_.bucket_count()) {
            admitted_automatic_dependencies_.rehash(
                staged.dependency_target_buckets);
        }
        if (staged.carrier_target_buckets >
            state_local_automatic_operators_.bucket_count()) {
            state_local_automatic_operators_.rehash(
                staged.carrier_target_buckets);
        }
        if (state_local_automatic_operator_indices_.bucket_count() >
                staged.state_local_parent_bucket_upper ||
            admitted_automatic_dependencies_.bucket_count() >
                staged.dependency_parent_bucket_upper ||
            state_local_automatic_operators_.bucket_count() >
                staged.carrier_parent_bucket_upper) {
            throw std::logic_error(
                "automatic publication parent bucket projection was "
                "exceeded");
        }
        if (staged.replace_candidate_operators) {
            candidate_operators_.swap(staged.candidate_operators);
            action_control_.automatic_options +=
                staged.automatic_option_additions;
        }
        state_local_automatic_operator_indices_.merge(
            staged.state_local_indices);
        admitted_automatic_dependencies_.merge(staged.dependencies);
        action_control_.automatic_dependency_primitives +=
            staged.automatic_dependency_additions;
        state_local_automatic_operators_.merge(
            staged.carrier_operators);
        if (!staged.state_local_indices.empty() ||
            !staged.dependencies.empty() ||
            !staged.carrier_operators.empty()) {
            throw std::logic_error(
                "automatic publication node merge was not allocation-free");
        }
        const auto stored =
            state_local_automatic_operators_.find(state_id);
        if (stored == state_local_automatic_operators_.end()) {
            throw std::logic_error(
                "automatic publication omitted carrier cache entry");
        }
        account_state_local_operators(stored->second);
    };
    const std::vector<std::uint32_t> empty_publication_members;
    const auto cached = state_local_automatic_operators_.find(state_id);
    if (cached != state_local_automatic_operators_.end()) {
        batch.cached = true;
        const std::size_t cached_capacity_upper =
            publication_vector_upper(cached->second.size());
        const std::uint64_t cached_copy_projection =
            cached_capacity_upper >
                    std::numeric_limits<std::uint64_t>::max() /
                        sizeof(std::uint32_t)
                ? std::numeric_limits<std::uint64_t>::max()
                : static_cast<std::uint64_t>(cached_capacity_upper) *
                      sizeof(std::uint32_t);
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                automatic_batch_cursor_bytes(batch),
                cached_copy_projection)};
        batch.admitted_operators.reserve(cached->second.size());
        if (batch.admitted_operators.capacity() >
            cached_capacity_upper) {
            throw std::logic_error(
                "automatic cached batch vector projection was exceeded");
        }
        batch.admitted_operators.insert(
            batch.admitted_operators.end(),
            cached->second.begin(), cached->second.end());
        co_await solve_detail::CooperativeCheckpoint{
            automatic_batch_cursor_bytes(batch)};
        co_return batch;
    }
    if (!goal_.automatic_candidates || is_goal_state(state(state_id))) {
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                automatic_batch_cursor_bytes(batch),
                publication_staging_projection(
                    batch.admitted_operators,
                    empty_publication_members,
                    empty_publication_members))};
        PublicationStaging publication = stage_publication(
            batch.admitted_operators, empty_publication_members,
            empty_publication_members);
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                automatic_batch_cursor_bytes(batch),
                publication_staging_bytes(publication))};
        commit_publication(publication);
        co_return batch;
    }

    pc_item_state carrier;
    if (!materialize(state_id, carrier)) {
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                automatic_batch_cursor_bytes(batch),
                publication_staging_projection(
                    batch.admitted_operators,
                    empty_publication_members,
                    empty_publication_members))};
        PublicationStaging publication = stage_publication(
            batch.admitted_operators, empty_publication_members,
            empty_publication_members);
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                automatic_batch_cursor_bytes(batch),
                publication_staging_bytes(publication))};
        commit_publication(publication);
        co_return batch;
    }

    const std::uint64_t admission_rows_before =
        telemetry_.state_action_rows;
    const std::uint64_t admission_transitions_before =
        telemetry_.transition_entries;
    const std::uint64_t admission_states_before =
        telemetry_.automatic_admission_discovered_states;
    const std::uint64_t admission_reforge_active_before =
        telemetry_.automatic_admission_reforge_active_work;
    const std::uint64_t admission_reforge_logical_before =
        telemetry_.automatic_admission_reforge_logical_work_v1;
    const auto finalize_batch_work = [&] {
        const auto delta = [](const std::uint64_t before,
                              const std::uint64_t after) {
            return after >= before ? after - before : after;
        };
        batch.phases.state_action_rows = delta(
            admission_rows_before, telemetry_.state_action_rows);
        batch.phases.transition_entries = delta(
            admission_transitions_before, telemetry_.transition_entries);
        batch.phases.discovered_states = delta(
            admission_states_before,
            telemetry_.automatic_admission_discovered_states);
        batch.phases.reforge_active_work = delta(
            admission_reforge_active_before,
            telemetry_.automatic_admission_reforge_active_work);
        batch.phases.reforge_logical_work_v1 = delta(
            admission_reforge_logical_before,
            telemetry_.automatic_admission_reforge_logical_work_v1);
    };

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
     * these one-shot compounds directly on the parent state lifecycle.
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
    /* Carrier-local contexts are only a cross-carrier performance cache; the
     * active coroutine owns its transient context across suspension.  A broad
     * product envelope can synthesize thousands of distinct context keys, so
     * retaining 64 cold contexts made every owned-byte checkpoint traverse a
     * large dead cache and could consume the public solver budget before the
     * compact admitted rows reached Bellman closure. The active candidate's
     * transient context supplies all correctness state, so keep no completed
     * carrier context alive across candidates; immutable option templates and
     * admitted parent rows remain the durable reuse layers. */
    constexpr std::size_t kRetainedAutomaticAdmissionContexts = 0;
    bool admission_context_created = false;
    std::unique_ptr<CalcContext> transient_context;
    CalcContext* local_pointer = nullptr;
    const auto retained = automatic_admission_contexts_.find(context_key);
    if (retained != automatic_admission_contexts_.end()) {
        local_pointer = retained->second.context.get();
        activate_automatic_admission_context(local_pointer);
        local_pointer->reset_solve_telemetry();
    } else {
        auto created = std::make_unique<CalcContext>(
            session_, local_goal, registry_, local_candidates,
            false, false, true);
        created->set_defer_automatic_protected_baseline(true);
        admission_context_created = true;
        if (automatic_admission_contexts_.size() <
            kRetainedAutomaticAdmissionContexts) {
            const auto [inserted, did_insert] =
                automatic_admission_contexts_.emplace(
                    context_key,
                    AutomaticAdmissionContext{std::move(created), 0});
            if (!did_insert) {
                throw std::logic_error(
                    "automatic admission context key insertion failed");
            }
            automatic_admission_context_key_bytes_ +=
                inserted->first.capacity() + 1;
            local_pointer = inserted->second.context.get();
            activate_automatic_admission_context(local_pointer);
        } else {
            transient_context = std::move(created);
            local_pointer = transient_context.get();
        }
    }
    CalcContext& local = *local_pointer;
    const std::uint32_t local_states_before = local.state_count();
    local.set_defer_automatic_protected_baseline(true);
    local.set_reforge_resource_accounting(
        reforge_resource_accounting_);
    local.set_reforge_provenance_context(
        reforge_row_owner_, ReforgeRowFamily::AutomaticOption);
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
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint64_t>::max(),
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
    if (limits.consider_imprint_programs) {
        ++shared_weights[
            static_cast<std::size_t>(AutomaticTelemetryKind::Imprint)];
    }
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

    std::vector<std::uint32_t> staged_dependencies;
    std::vector<std::uint32_t> staged_state_local_operators;
    const auto add_dependency = [&](const std::uint32_t action) {
        if (std::find(candidates_.begin(), candidates_.end(), action) ==
                candidates_.end() &&
            !admitted_automatic_dependencies_.contains(action) &&
            std::find(
                staged_dependencies.begin(), staged_dependencies.end(),
                action) == staged_dependencies.end()) {
            staged_dependencies.push_back(action);
        }
    };
    const auto admit_operator = [&](const std::uint32_t index) {
        batch.admitted_operators.push_back(index);
    };
    const auto stage_state_local_operator = [&](const std::uint32_t index) {
        if (std::find(
                staged_state_local_operators.begin(),
                staged_state_local_operators.end(), index) ==
            staged_state_local_operators.end()) {
            staged_state_local_operators.push_back(index);
        }
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
    const auto retained_cursor_nested_bytes = [&]() {
        std::uint64_t bytes = automatic_batch_cursor_bytes(batch);
        bytes = automatic_cursor_add(
            bytes, synthesis_cursor_bytes(synthesis));
        bytes = automatic_cursor_add(
            bytes, goal_spec_cursor_bytes(local_goal));
        bytes = automatic_cursor_add(
            bytes,
            parent_eldritch_specs.capacity() * sizeof(FixedOptionSpec));
        for (const FixedOptionSpec& spec : parent_eldritch_specs) {
            bytes = automatic_cursor_add(
                bytes, fixed_option_spec_nested_bytes(spec));
        }
        bytes = automatic_cursor_add(
            bytes,
            permanent_benches.capacity() * sizeof(std::uint32_t));
        bytes = automatic_cursor_add(
            bytes,
            local_candidates.capacity() * sizeof(std::uint32_t));
        bytes = automatic_cursor_add(
            bytes,
            local_option_indices.capacity() * sizeof(std::uint32_t));
        bytes = automatic_cursor_add(
            bytes, automatic_cursor_string_bytes(context_key));
        if (transient_context != nullptr) {
            bytes = automatic_cursor_add(
                bytes, local.fast_estimated_owned_bytes());
        }
        bytes = automatic_cursor_add(
            bytes,
            mapped_states.bucket_count() * sizeof(void*) +
                mapped_states.size() *
                    (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                     2 * sizeof(void*)));
        bytes = automatic_cursor_add(
            bytes,
            seen_option_kernels.capacity() * sizeof(const OptionKernel*));
        bytes = automatic_cursor_add(
            bytes,
            seen_primitive_kernels.capacity() *
                sizeof(decltype(seen_primitive_kernels)::value_type));
        for (const auto& [unused_kernel, resources] :
             seen_primitive_kernels) {
            (void)unused_kernel;
            bytes = automatic_cursor_add(
                bytes,
                resources.capacity() *
                    sizeof(std::pair<std::string, double>));
            for (const auto& [key, unused_quantity] : resources) {
                (void)unused_quantity;
                bytes = automatic_cursor_add(
                    bytes, automatic_cursor_string_bytes(key));
            }
        }
        bytes = automatic_cursor_add(
            bytes,
            staged_dependencies.capacity() * sizeof(std::uint32_t));
        bytes = automatic_cursor_add(
            bytes,
            staged_state_local_operators.capacity() *
                sizeof(std::uint32_t));
        bytes = automatic_cursor_add(
            bytes,
            temporary_evaluation_memo.bucket_count() * sizeof(void*) +
                temporary_evaluation_memo.size() *
                    (sizeof(decltype(
                         temporary_evaluation_memo)::value_type) +
                     2 * sizeof(void*)));
        for (const auto& [key, unused_kernel] :
             temporary_evaluation_memo) {
            (void)unused_kernel;
            bytes = automatic_cursor_add(
                bytes, automatic_cursor_string_bytes(key));
        }
        bytes = automatic_cursor_add(
            bytes,
            protected_kernel_comparisons.size() *
                (sizeof(decltype(
                     protected_kernel_comparisons)::value_type) +
                 3 * sizeof(void*)));
        return bytes;
    };
    const auto mark_resource_deferred = [&batch](
        const SolverResourceLimit& limit,
        std::string id,
        std::string reason_prefix) {
        batch.status =
            StateLocalAutomaticBatchStatus::ResourceDeferred;
        batch.resource_cap = limit.cap_name();
        batch.resource_limit = limit.limit();
        batch.resource_reason =
            std::move(reason_prefix) + limit.cap_name();
        batch.admitted_operators.clear();
        for (StateLocalAutomaticCandidate& decision : batch.decisions) {
            decision.admitted = false;
        }
        StateLocalAutomaticCandidate deferred;
        deferred.id = std::move(id);
        deferred.deferred = true;
        deferred.evidence.candidate = true;
        deferred.evidence.legality_result = "deferred_resource_cap";
        deferred.evidence.reason = batch.resource_reason;
        batch.decisions.push_back(std::move(deferred));
    };
    if (!parent_eldritch_specs.empty()) {
        GoalSpec parent_goal = goal_;
        parent_goal.automatic_candidates = false;
        parent_goal.fixed_options =
            std::move(parent_eldritch_specs);
        std::vector<PlannerOperator> parent_options =
            build_planner_operators(
                *session_, parent_goal, registry_, candidates_);
        const std::size_t first_staged_operator = operators_.size();
        std::vector<StateLocalAutomaticCandidate> staged_decisions;
        struct ParentEldritchRepresentative {
            std::uint32_t operator_index = kNoId;
            std::size_t decision_index = 0;
            double immediate_cost = 0.0;
        };
        std::vector<ParentEldritchRepresentative>
            parent_eldritch_representatives;
        const auto parent_eldritch_cursor_bytes = [&]() {
            std::uint64_t cursor_bytes =
                retained_cursor_nested_bytes();
            cursor_bytes = automatic_cursor_add(
                cursor_bytes,
                parent_options.capacity() * sizeof(PlannerOperator));
            for (const PlannerOperator& planner : parent_options) {
                cursor_bytes = automatic_cursor_add(
                    cursor_bytes,
                    planner_operator_cursor_bytes(planner));
            }
            cursor_bytes = automatic_cursor_add(
                cursor_bytes, goal_spec_cursor_bytes(parent_goal));
            return automatic_cursor_add(
                cursor_bytes,
                automatic_decisions_cursor_bytes(staged_decisions));
        };
        bool parent_eldritch_resource_deferred = false;
        try {
            for (std::uint32_t local_index =
                     static_cast<std::uint32_t>(registry_.actions.size());
                 local_index < parent_options.size(); ++local_index) {
                co_await solve_detail::CooperativeCheckpoint{
                    parent_eldritch_cursor_bytes()};
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
                decision.evidence.candidate = true;
                const bool proposed_has_prices =
                    program_has_prices(proposed);
                double exact_immediate_cost = 0.0;
                if (limits.prices != nullptr && proposed_has_prices) {
                    for (const auto& [key, quantity] :
                         proposed.resource_quantities) {
                        exact_immediate_cost +=
                            limits.prices->at(key) * quantity;
                    }
                }
                /* Parent-context Eldritch options used to bypass the exact
                 * immediate-cost authority applied to every locally mapped
                 * automatic option. This fixed program pays its complete
                 * resource vector before any continuation. A certified
                 * feasible carrier upper below that nonnegative cost proves
                 * the option cannot improve, without constructing its often
                 * large exact reforge kernel. */
                if (proposed_has_prices &&
                    std::isfinite(limits.incumbent_upper_bound) &&
                    exact_immediate_cost >
                        limits.incumbent_upper_bound + 1e-12) {
                    decision.evidence.eligible = false;
                    decision.evidence.legality_result =
                        "dominated_by_incumbent";
                    decision.evidence.reason =
                        "exact_expected_cost_exceeds_feasible_state_upper";
                    staged_decisions.push_back(std::move(decision));
                    check_limits(true);
                    continue;
                }
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
                const ReforgeProvenanceCheckpoint provenance =
                    begin_reforge_provenance(
                        reforge_row_owner_,
                        ReforgeRowFamily::AutomaticOption);
                const OptionKernel* kernel_pointer = nullptr;
                ++automatic_admission_reforge_scope_depth_;
                try {
                    kernel_pointer =
                        &option_kernel(state_id, operator_index);
                    --automatic_admission_reforge_scope_depth_;
                    finish_reforge_provenance(
                        provenance,
                        ReforgeRowDisposition::Completed);
                } catch (...) {
                    --automatic_admission_reforge_scope_depth_;
                    finish_reforge_provenance(
                        provenance,
                        ReforgeRowDisposition::Discarded);
                    throw;
                }
                const OptionKernel& kernel = *kernel_pointer;
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
                    bool collapsed = false;
                    for (auto representative =
                             parent_eldritch_representatives.begin();
                         representative !=
                             parent_eldritch_representatives.end();
                         ++representative) {
                        const OptionKernel& retained = option_kernel(
                            state_id, representative->operator_index);
                        if (same_complete_option_kernel(retained, kernel)) {
                            collapsed = true;
                            break;
                        }
                        if (limits.prices == nullptr ||
                            !same_option_transition_kernel(
                                retained, kernel)) {
                            continue;
                        }
                        if (exact_immediate_cost <
                            representative->immediate_cost) {
                            StateLocalAutomaticCandidate& prior =
                                staged_decisions.at(
                                    representative->decision_index);
                            prior.admitted = false;
                            prior.collapsed = true;
                            prior.evidence.reason =
                                "equivalent_exact_kernel_price_dominated";
                            parent_eldritch_representatives.erase(
                                representative);
                        } else {
                            collapsed = true;
                        }
                        break;
                    }
                    decision.collapsed = collapsed;
                    if (collapsed) {
                        decision.evidence.reason =
                            "equivalent_exact_kernel_price_dominated";
                    } else {
                        decision.admitted = true;
                    }
                }
                staged_decisions.push_back(std::move(decision));
                if (staged_decisions.back().admitted) {
                    parent_eldritch_representatives.push_back({
                        operator_index, staged_decisions.size() - 1,
                        exact_immediate_cost});
                }
                check_limits(true);
            }
        } catch (const SolverResourceLimit& limit) {
            rollback_staged_automatic_operators(
                state_id, first_staged_operator);
            mark_resource_deferred(
                limit, "automatic:parent_eldritch_generation",
                "parent_eldritch_kernel_generation_");
            finalize_batch_work();
            parent_eldritch_resource_deferred = true;
        }
        if (parent_eldritch_resource_deferred) {
            co_await solve_detail::CooperativeCheckpoint{
                parent_eldritch_cursor_bytes()};
            co_return batch;
        }
        for (StateLocalAutomaticCandidate& decision : staged_decisions) {
            if (decision.admitted && decision.operator_index != kNoId) {
                admit_operator(decision.operator_index);
                stage_state_local_operator(decision.operator_index);
                const PlannerOperator& planner =
                    operators_.at(decision.operator_index);
                for (const std::uint32_t dependency :
                     planner.primitive_program) {
                    add_dependency(dependency);
                }
            }
            batch.decisions.push_back(std::move(decision));
        }
    }
    bool local_work_merged = false;
    const auto merge_local_work = [&]() {
        if (local_work_merged) return;
        const CalcTelemetry& work = local.telemetry();
        const auto add_saturated = [](std::uint64_t& target,
                                      const std::uint64_t amount) {
            target = amount >
                             std::numeric_limits<std::uint64_t>::max() -
                                 target
                         ? std::numeric_limits<std::uint64_t>::max()
                         : target + amount;
        };
        telemetry_.distribution_requests += work.distribution_requests;
        telemetry_.distribution_hits += work.distribution_hits;
        telemetry_.distribution_misses += work.distribution_misses;
        telemetry_.distribution_build_ns += work.distribution_build_ns;
        add_saturated(
            telemetry_.state_action_rows, work.state_action_rows);
        add_saturated(
            telemetry_.transition_entries, work.transition_entries);
        const std::uint64_t local_discovered_states =
            local.state_count() >= local_states_before
                ? static_cast<std::uint64_t>(
                      local.state_count() - local_states_before)
                : static_cast<std::uint64_t>(local.state_count());
        add_saturated(
            telemetry_.automatic_admission_discovered_states,
            local_discovered_states);
        add_saturated(
            batch.phases.discovered_states,
            local_discovered_states);
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
        add_saturated(
            telemetry_.automatic_admission_reforge_active_work,
            work.reforge_frontier_work);
        add_saturated(
            telemetry_.automatic_admission_reforge_logical_work_v1,
            work.reforge_logical_work_v1);
        merge_nested_reforge_telemetry(work);
        local_work_merged = true;
    };

    try {
        const auto imprint_started = std::chrono::steady_clock::now();
        auto imprint_task =
            discover_automatic_imprint_options_cooperatively(
                local, local_state, limits);
        while (!imprint_task.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                automatic_cursor_add(
                    retained_cursor_nested_bytes(),
                    imprint_task.retained_bytes())};
        }
        const ImprintDiscoveryResult imprint =
            imprint_task.take_result();
        imprint_task.reset();
        batch.phases.imprint_programs_evaluated =
            imprint.work_used;
        batch.phases.imprint_programs_pruned =
            imprint.programs_pruned;
        batch.phases.imprint_distribution_dominated_programs =
            imprint.distribution_dominated_programs;
        batch.phases.imprint_price_pruned_programs =
            imprint.price_pruned_programs;
        batch.phases.imprint_price_bound_max_program_depth =
            imprint.price_bound_max_program_depth;
        batch.phases.imprint_max_evaluated_depth =
            imprint.max_evaluated_depth;
        batch.phases.imprint_max_frontier_size =
            imprint.max_frontier_size;
        batch.phases.imprint_price_bound_complete_carriers =
            imprint.price_bound_complete ? 1 : 0;
        batch.phases.imprint_action_state_evaluations =
            imprint.action_state_evaluations;
        batch.phases.imprint_outcomes_merged =
            imprint.outcomes_merged;
        batch.phases.imprint_max_atomic_outcomes_ns =
            imprint.max_atomic_outcomes_ns;
        const auto retained_imprint_cursor_bytes = [&]() {
            return automatic_cursor_add(
                retained_cursor_nested_bytes(),
                imprint_discovery_result_nested_bytes(imprint));
        };
        co_await solve_detail::CooperativeCheckpoint{
            retained_imprint_cursor_bytes()};
        const std::uint64_t imprint_discovery_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - imprint_started)
                    .count());
        bool imprint_time_attributed = false;
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
                std::string("automatic_imprint_program_generation_") + cap +
                "_limit_" + std::to_string(limit) + "_work_" +
                std::to_string(imprint.work_used);
            if (!imprint.depth_deferred_samples.empty()) {
                deferred.evidence.reason += "_frontier_samples_";
                for (std::size_t i = 0;
                     i < imprint.depth_deferred_samples.size(); ++i) {
                    if (i != 0) deferred.evidence.reason += ',';
                    deferred.evidence.reason +=
                        imprint.depth_deferred_samples[i];
                }
            }
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
        if (imprint.depth_deferred || imprint.work_deferred) {
            batch.status =
                StateLocalAutomaticBatchStatus::ResourceDeferred;
            batch.resource_cap = imprint.work_deferred
                                     ? "max_imprint_program_work"
                                     : "max_imprint_program_depth";
            batch.resource_limit = imprint.work_deferred
                                       ? imprint.work_limit
                                       : imprint.depth_limit;
            batch.resource_reason =
                "automatic_imprint_program_generation_" +
                batch.resource_cap;
            batch.admitted_operators.clear();
            for (StateLocalAutomaticCandidate& decision : batch.decisions) {
                decision.admitted = false;
            }
            merge_local_work();
            finalize_batch_work();
            co_await solve_detail::CooperativeCheckpoint{
                retained_imprint_cursor_bytes()};
            co_return batch;
        }
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

        for (const std::uint32_t action_index : permanent_benches) {
            co_await solve_detail::CooperativeCheckpoint{
                retained_imprint_cursor_bytes()};
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
            co_await solve_detail::CooperativeCheckpoint{
                retained_imprint_cursor_bytes()};
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
                                candidates_, false, false, true);
                    }
                    CalcContext& comparison_context =
                        *automatic_comparison_context_;
                    comparison_context.set_reforge_resource_accounting(
                        reforge_resource_accounting_);
                    comparison_context.set_reforge_provenance_context(
                        reforge_row_owner_,
                        ReforgeRowFamily::AutomaticOption);
                    const std::uint32_t comparison_states_before =
                        comparison_context.state_count();
                    const CalcTelemetry comparison_before =
                        comparison_context.telemetry();
                    comparison_context.set_solve_resource_caps(
                        std::numeric_limits<std::uint32_t>::max(),
                        std::numeric_limits<std::uint64_t>::max(), false,
                        solve_owned_bytes_cap_);
                    bool comparison_work_merged = false;
                    const auto merge_comparison_work = [&]() {
                        if (comparison_work_merged) return;
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
                        const std::uint64_t discovered_states =
                            comparison_context.state_count() >=
                                    comparison_states_before
                                ? static_cast<std::uint64_t>(
                                      comparison_context.state_count() -
                                      comparison_states_before)
                                : static_cast<std::uint64_t>(
                                      comparison_context.state_count());
                        const auto add_saturated = [](
                                                       std::uint64_t& target,
                                                       const std::uint64_t
                                                           amount) {
                            target = amount >
                                             std::numeric_limits<
                                                 std::uint64_t>::max() -
                                                 target
                                         ? std::numeric_limits<
                                               std::uint64_t>::max()
                                         : target + amount;
                        };
                        add_saturated(
                            telemetry_.state_action_rows,
                            comparison_after.state_action_rows -
                                comparison_before.state_action_rows);
                        add_saturated(
                            telemetry_.transition_entries,
                            comparison_after.transition_entries -
                                comparison_before.transition_entries);
                        add_saturated(
                            telemetry_
                                .automatic_admission_discovered_states,
                            discovered_states);
                        add_saturated(
                            batch.phases.discovered_states,
                            discovered_states);
                        add_saturated(
                            telemetry_
                                .automatic_admission_reforge_active_work,
                            comparison_after.reforge_frontier_work -
                                comparison_before.reforge_frontier_work);
                        add_saturated(
                            telemetry_
                                .automatic_admission_reforge_logical_work_v1,
                            comparison_after.reforge_logical_work_v1 -
                                comparison_before.reforge_logical_work_v1);
                        merge_nested_reforge_telemetry(
                            comparison_after, &comparison_before);
                        comparison_work_merged = true;
                        local.set_solve_resource_caps(
                            std::numeric_limits<std::uint32_t>::max(),
                            std::numeric_limits<std::uint64_t>::max(), false,
                            limits.max_solver_owned_bytes == 0
                                ? std::nullopt
                                : std::optional<std::uint64_t>{
                                      limits.max_solver_owned_bytes});
                    };
                    const std::uint32_t comparison_state =
                        comparison_context.intern_item(carrier);
                    const ActionDescriptor& baseline_action =
                        comparison_context.registry().actions.at(
                            local_planner.followup_action);
                    const bool baseline_legal = action_legal(
                        comparison_context.session(), baseline_action,
                        comparison_context.state(comparison_state));
                    const OutcomeDistribution* baseline_distribution = nullptr;
                    try {
                        baseline_distribution =
                            baseline_legal
                                ? &comparison_context.outcomes(
                                      comparison_state,
                                      local_planner.followup_action)
                                : nullptr;
                    } catch (...) {
                        try {
                            merge_comparison_work();
                        } catch (const SolverResourceLimit&) {
                            /* Preserve the comparison operation's original
                             * resource-limit witness. */
                        }
                        throw;
                    }
                    merge_comparison_work();
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

            struct PricedVariant {
                PlannerOperator planner;
                double immediate_cost = 0.0;
            };
            std::vector<PricedVariant> priced_variants;
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
                priced_variants.push_back({
                    std::move(admitted), exact_immediate_cost});
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

            if (temporary_group != nullptr && limits.prices != nullptr &&
                priced_variants.size() > 1) {
                /* Every blocker variant in one temporary effect group uses
                 * the same exact OptionKernel. At fixed solve prices its Q
                 * rows therefore differ only by immediate cost, so every
                 * non-cheapest variant is globally dominated for this
                 * carrier under every continuation value. Keep the first
                 * minimum to preserve deterministic tie authority. */
                const auto cheapest = std::min_element(
                    priced_variants.begin(), priced_variants.end(),
                    [](const PricedVariant& left,
                       const PricedVariant& right) {
                        return left.immediate_cost < right.immediate_cost;
                    });
                for (auto variant = priced_variants.begin();
                     variant != priced_variants.end(); ++variant) {
                    if (variant == cheapest) continue;
                    StateLocalAutomaticCandidate collapsed = base_decision;
                    collapsed.id = variant->planner.id;
                    collapsed.raw_outcomes = 0;
                    collapsed.collapsed = true;
                    collapsed.evidence.reason =
                        "equivalent_exact_kernel_price_dominated";
                    batch.decisions.push_back(std::move(collapsed));
                }
                PricedVariant retained = std::move(*cheapest);
                priced_variants.clear();
                priced_variants.push_back(std::move(retained));
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
            for (PricedVariant& priced : priced_variants) {
                PlannerOperator& admitted = priced.planner;
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
                stage_state_local_operator(operator_index);
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
        mark_resource_deferred(
            limit, "automatic:state_local_generation",
            "price_independent_kernel_generation_");
    }
    std::sort(
        batch.admitted_operators.begin(), batch.admitted_operators.end());
    batch.admitted_operators.erase(
        std::unique(
            batch.admitted_operators.begin(),
            batch.admitted_operators.end()),
        batch.admitted_operators.end());
    if (batch.status == StateLocalAutomaticBatchStatus::Complete) {
        /* Guard the temporary allocation peak before stage_publication itself
         * allocates candidate copies, staging nodes, or sizing buckets. */
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                retained_cursor_nested_bytes(),
                publication_staging_projection(
                    batch.admitted_operators,
                    staged_state_local_operators,
                    staged_dependencies))};
        PublicationStaging publication = stage_publication(
            batch.admitted_operators, staged_state_local_operators,
            staged_dependencies);
        /* The checkpoint owns every staged node/vector allocation plus the
         * exact parent bucket growth needed by the subsequent allocation-free
         * swaps and node merges. No outer membership is visible before it. */
        co_await solve_detail::CooperativeCheckpoint{
            automatic_cursor_add(
                retained_cursor_nested_bytes(),
                publication_staging_bytes(publication))};
        commit_publication(publication);
    } else {
        co_await solve_detail::CooperativeCheckpoint{
            retained_cursor_nested_bytes()};
    }
    finalize_batch_work();
    co_return batch;
}

} // namespace solver
} // namespace poecraft
