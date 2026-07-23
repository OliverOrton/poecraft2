#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

 void SolveWork::Impl::signature_string(
        std::vector<std::uint64_t>& out,
        const std::string& value) {
        out.push_back(value.size());
        for (const unsigned char byte : value) out.push_back(byte);
    }

void SolveWork::Impl::planner_observation_signature(
        std::vector<std::uint64_t>& out,
        const PlannerOperator& planner) const {
        out.push_back(static_cast<std::uint8_t>(planner.kind));
        out.push_back(static_cast<std::uint8_t>(planner.option_kind));
        out.push_back(static_cast<std::uint8_t>(planner.automatic_kind));
        out.push_back(static_cast<std::uint8_t>(planner.intended_side + 1));
        out.push_back(planner.primitive_action);
        out.push_back(planner.exit_min_satisfied);
        out.push_back(planner.carrier_goal_slot);
        out.push_back(planner.conditional_action);
        out.push_back(planner.bestiary_create_action);
        out.push_back(planner.bestiary_restore_action);
        out.push_back(planner.relevant_goal_mask);
        out.push_back(planner.setup_action);
        out.push_back(planner.followup_action);
        out.push_back(planner.cleanup_action);
        out.push_back(planner.primitive_program.size());
        for (const std::uint32_t action : planner.primitive_program) {
            out.push_back(action);
        }
        out.push_back(planner.exit_goal_slots.size());
        for (const std::uint32_t slot : planner.exit_goal_slots) {
            out.push_back(slot);
        }
        out.push_back(planner.resource_quantities.size());
        for (const auto& [key, quantity] : planner.resource_quantities) {
            signature_string(out, key);
            out.push_back(std::bit_cast<std::uint64_t>(quantity));
        }
    }

std::vector<std::uint64_t> SolveWork::Impl::row_observation_cache_key(
        const SolveTransitionCache& graph,
        const SparseRow& row,
        const std::vector<std::uint32_t>& partition) const {
        const double detached_self_probability =
            row.self_probability - row.embedded_self_probability;
        bool observes_owner_class = detached_self_probability > 0.0;
        for (std::uint32_t i = 0;
             !observes_owner_class && i < row.choice_count; ++i) {
            observes_owner_class =
                graph.choices.at(row.choice_offset + i).has_self;
        }
        std::vector<std::uint64_t> key{
            row.transition_offset,
            row.transition_count,
            row.choice_offset,
            row.choice_count,
            observes_owner_class ? partition.at(row.owner_state) : kNoId,
            std::bit_cast<std::uint64_t>(detached_self_probability)};
        std::vector<std::vector<std::uint64_t>> variants;
        variants.reserve(row.variant_count);
        for (std::uint32_t i = 0; i < row.variant_count; ++i) {
            const SparseVariant& variant = graph.variant_arena->variants.at(
                graph.variant_arena->row_variant_indices.at(
                    row.variant_offset + i));
            std::vector<std::uint64_t> tokens;
            planner_observation_signature(
                tokens, calc.operators().at(variant.operator_index));
            const std::int32_t priced_position =
                priced_operator_position.at(variant.operator_index);
            if (priced_position < 0) {
                throw std::logic_error(
                    "quotient row contains an unpriced operator");
            }
            const PricedOperator& priced =
                operators.at(static_cast<std::size_t>(priced_position));
            tokens.push_back(variant.quantity_count);
            if (variant.quantity_count != priced.resource_prices.size()) {
                throw std::logic_error(
                    "quotient resource observation is incompatible");
            }
            for (std::uint32_t q = 0; q < variant.quantity_count; ++q) {
                signature_string(tokens, priced.resource_prices[q].first);
                tokens.push_back(std::bit_cast<std::uint64_t>(
                    graph.variant_arena->variant_quantities.at(
                        variant.quantity_offset + q)));
            }
            tokens.push_back(variant.choice_option_count);
            for (std::uint32_t c = 0;
                 c < variant.choice_option_count; ++c) {
                const OutcomeChoiceOption& choice = graph.choice_options.at(
                    variant.choice_option_offset + c);
                const auto observed_class = [&](const std::uint32_t state) {
                    return state == kNoId ? kNoId : partition.at(state);
                };
                tokens.push_back(choice.mod_id);
                tokens.push_back(observed_class(choice.state));
                tokens.push_back(observed_class(choice.observation_state));
                tokens.push_back(observed_class(choice.actual_state));
            }
            variants.push_back(std::move(tokens));
        }
        std::sort(variants.begin(), variants.end());
        key.push_back(variants.size());
        for (const auto& variant : variants) {
            key.push_back(variant.size());
            key.insert(key.end(), variant.begin(), variant.end());
        }
        return key;
    }

void SolveWork::Impl::sort_projection_classes(
        std::vector<std::uint32_t>& classes,
        RowObservationCache& cache) const {
        if (classes.size() < 4096) {
            std::sort(classes.begin(), classes.end());
            return;
        }
        cache.radix_scratch.resize(classes.size());
        const auto radix_pass = [&cache](
            const std::vector<std::uint32_t>& source,
            std::vector<std::uint32_t>& target,
            const std::uint32_t shift) {
            cache.radix_counts.fill(0);
            for (const std::uint32_t value : source) {
                ++cache.radix_counts[(value >> shift) & 0xffffu];
            }
            std::uint32_t offset = 0;
            for (std::uint32_t& count : cache.radix_counts) {
                const std::uint32_t next = offset + count;
                count = offset;
                offset = next;
            }
            for (const std::uint32_t value : source) {
                target[cache.radix_counts[(value >> shift) & 0xffffu]++] =
                    value;
            }
        };
        radix_pass(classes, cache.radix_scratch, 0);
        radix_pass(cache.radix_scratch, classes, 16);
    }

void SolveWork::Impl::fill_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache,
        const bool secondary) const {
        auto& sums = secondary ? cache.secondary_transition_sums
                               : cache.transition_sums;
        auto& epochs = secondary ? cache.secondary_transition_epochs
                                 : cache.transition_epochs;
        auto& touched = secondary ? cache.secondary_touched_classes
                                  : cache.touched_classes;
        auto& epoch = secondary ? cache.secondary_transition_epoch
                                : cache.transition_epoch;
        if (sums.size() < partition.size()) {
            sums.resize(partition.size());
            epochs.resize(partition.size(), 0);
        }
        if (++epoch == 0) {
            std::fill(epochs.begin(), epochs.end(), 0);
            ++epoch;
        }
        touched.clear();
        const double self_probability =
            std::bit_cast<double>(key[5]);
        if (self_probability > 0.0) {
            const std::uint32_t owner_class =
                static_cast<std::uint32_t>(key[4]);
            epochs[owner_class] = epoch;
            sums[owner_class] = WideFloat{self_probability};
            touched.push_back(owner_class);
        }
        for (std::uint64_t i = 0; i < key[1]; ++i) {
            const std::uint64_t offset = key[0] + i;
            const std::uint32_t successor_class =
                partition.at(graph.successors.at(offset));
            if (epochs[successor_class] != epoch) {
                epochs[successor_class] = epoch;
                sums[successor_class] = WideFloat{0.0};
                touched.push_back(successor_class);
            }
            sums[successor_class] = sums[successor_class] +
                                    WideFloat{graph.probabilities.at(offset)};
        }
        sort_projection_classes(touched, cache);
    }

std::vector<std::uint32_t> SolveWork::Impl::projected_choice_classes(
        const SolveTransitionCache& graph,
        const SparseChoiceGroup& group,
        const std::vector<std::uint32_t>& partition) const {
        std::vector<std::uint32_t> classes;
        classes.reserve(group.successor_count);
        for (std::uint32_t s = 0; s < group.successor_count; ++s) {
            classes.push_back(partition.at(
                graph.choice_successors.at(group.successor_offset + s)));
        }
        std::sort(classes.begin(), classes.end());
        classes.erase(
            std::unique(classes.begin(), classes.end()), classes.end());
        return classes;
    }

std::uint64_t SolveWork::Impl::kernel_projection_hash(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        fill_kernel_projection(graph, key, partition, cache, false);
        std::uint64_t hash = 1469598103934665603ull;
        const auto append = [&hash](const std::uint64_t token) {
            hash ^= token;
            hash *= 1099511628211ull;
        };
        append(cache.touched_classes.size());
        for (const std::uint32_t successor_class : cache.touched_classes) {
            append(successor_class);
            append(std::bit_cast<std::uint64_t>(
                cache.transition_sums[successor_class].value()));
        }
        append(key[3]);
        for (std::uint64_t i = 0; i < key[3]; ++i) {
            const SparseChoiceGroup& group = graph.choices.at(key[2] + i);
            append(std::bit_cast<std::uint64_t>(group.probability));
            std::vector<std::uint32_t> classes =
                projected_choice_classes(graph, group, partition);
            if (group.has_self) {
                classes.push_back(static_cast<std::uint32_t>(key[4]));
                std::sort(classes.begin(), classes.end());
                classes.erase(
                    std::unique(classes.begin(), classes.end()),
                    classes.end());
            }
            append(classes.size());
            for (const std::uint32_t value : classes) append(value);
        }
        return hash;
    }

bool SolveWork::Impl::same_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& current,
        const std::array<std::uint64_t, 6>& candidate,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        fill_kernel_projection(graph, candidate, partition, cache, true);
        if (cache.touched_classes != cache.secondary_touched_classes) {
            return false;
        }
        for (const std::uint32_t successor_class : cache.touched_classes) {
            if (cache.transition_sums[successor_class].value() !=
                cache.secondary_transition_sums[successor_class].value()) {
                return false;
            }
        }
        if (current[3] != candidate[3]) return false;
        for (std::uint64_t i = 0; i < current[3]; ++i) {
            const SparseChoiceGroup& left = graph.choices.at(current[2] + i);
            const SparseChoiceGroup& right =
                graph.choices.at(candidate[2] + i);
            if (left.probability != right.probability) {
                return false;
            }
            std::vector<std::uint32_t> left_classes =
                projected_choice_classes(graph, left, partition);
            std::vector<std::uint32_t> right_classes =
                projected_choice_classes(graph, right, partition);
            if (left.has_self) {
                left_classes.push_back(
                    static_cast<std::uint32_t>(current[4]));
            }
            if (right.has_self) {
                right_classes.push_back(
                    static_cast<std::uint32_t>(candidate[4]));
            }
            std::sort(left_classes.begin(), left_classes.end());
            left_classes.erase(
                std::unique(left_classes.begin(), left_classes.end()),
                left_classes.end());
            std::sort(right_classes.begin(), right_classes.end());
            right_classes.erase(
                std::unique(right_classes.begin(), right_classes.end()),
                right_classes.end());
            if (left_classes != right_classes) return false;
        }
        return true;
    }

std::uint32_t SolveWork::Impl::intern_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        std::uint64_t key_hash = 1469598103934665603ull;
        for (const std::uint64_t token : key) {
            key_hash ^= token;
            key_hash *= 1099511628211ull;
        }
        auto& key_candidates = cache.kernel_projection_buckets[key_hash];
        for (const KernelProjectionMemo& memo : key_candidates) {
            if (memo.exact_key == key) return memo.class_id;
        }

        const std::uint64_t behavior_hash =
            kernel_projection_hash(graph, key, partition, cache);
        auto& behavior_candidates =
            cache.kernel_projection_behavior_buckets[behavior_hash];
        std::uint32_t class_id = kNoId;
        for (const KernelProjectionRepresentative& candidate :
             behavior_candidates) {
            if (same_kernel_projection(
                    graph, key, candidate.exact_key, partition, cache)) {
                class_id = candidate.class_id;
                break;
            }
        }
        if (class_id == kNoId) {
            class_id = cache.next_kernel_projection_class_id++;
            behavior_candidates.push_back({key, class_id});
        }
        key_candidates.push_back({key, class_id});
        return class_id;
    }

std::vector<std::uint64_t> SolveWork::Impl::row_behavior_signature(
        const SolveTransitionCache& graph,
        const SparseRow& row,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache,
        const std::vector<std::uint64_t>& exact_row_key) const {
        const double detached_self_probability =
            row.self_probability - row.embedded_self_probability;
        bool observes_owner_class = detached_self_probability > 0.0;
        for (std::uint32_t i = 0;
             !observes_owner_class && i < row.choice_count; ++i) {
            observes_owner_class =
                graph.choices.at(row.choice_offset + i).has_self;
        }
        const std::uint32_t observed_owner_class =
            observes_owner_class ? partition.at(row.owner_state) : kNoId;
        const std::array<std::uint64_t, 6> projection_key{
            row.transition_offset,
            row.transition_count,
            row.choice_offset,
            row.choice_count,
            observed_owner_class,
            std::bit_cast<std::uint64_t>(detached_self_probability)};
        const std::uint32_t projection_class = intern_kernel_projection(
            graph, projection_key, partition, cache);

        std::vector<std::uint64_t> out;
        out.reserve(1 + exact_row_key.size() - 6);
        out.push_back(projection_class);
        out.insert(
            out.end(), exact_row_key.begin() + 6, exact_row_key.end());
        return out;
    }

std::uint32_t SolveWork::Impl::intern_row_behavior(
        const SolveTransitionCache& graph,
        const std::uint32_t row_index,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        const SparseRow& row = graph.rows.at(row_index);
        const std::vector<std::uint64_t> key =
            row_observation_cache_key(graph, row, partition);
        const std::uint64_t key_hash = observation_signature_hash(key);
        auto& key_candidates = cache.exact_key_buckets[key_hash];
        for (const RowObservationRepresentative& candidate : key_candidates) {
            if (row_observation_cache_key(
                    graph, graph.rows.at(candidate.row_index), partition) ==
                key) {
                return candidate.class_id;
            }
        }

        const std::vector<std::uint64_t> signature =
            row_behavior_signature(graph, row, partition, cache, key);
        const std::uint64_t behavior_hash =
            observation_signature_hash(signature);
        auto& behavior_candidates = cache.behavior_buckets[behavior_hash];
        for (const RowObservationRepresentative& candidate :
             behavior_candidates) {
            const std::vector<std::uint64_t> candidate_key =
                row_observation_cache_key(
                    graph, graph.rows.at(candidate.row_index), partition);
            if (row_behavior_signature(
                    graph, graph.rows.at(candidate.row_index), partition,
                    cache, candidate_key) ==
                signature) {
                key_candidates.push_back({row_index, candidate.class_id});
                return candidate.class_id;
            }
        }
        const std::uint32_t class_id = cache.next_class_id++;
        behavior_candidates.push_back({row_index, class_id});
        key_candidates.push_back({row_index, class_id});
        return class_id;
    }

std::vector<std::uint64_t> SolveWork::Impl::state_behavior_signature(
        const SolveTransitionCache& graph,
        const std::uint32_t state,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache* cache) const {
        std::vector<std::uint64_t> out;
        out.push_back(calc.is_goal_state(calc.state(state)) ? 1u : 0u);
        out.push_back(
            state < graph.expanded.size() && graph.expanded[state] ? 1u : 0u);
        /* Refinement is monotone: a round may split an existing candidate
         * class but can never merge two classes that the exact coarse
         * observation already distinguished. This also makes termination
         * independent of incidental numeric class renumbering. */
        out.push_back(partition.at(state));
        const StateRowSpan span = state < graph.state_rows.size()
                                      ? graph.state_rows[state]
                                      : StateRowSpan{};
        std::vector<std::uint32_t> rows;
        rows.reserve(span.count);
        for (std::uint32_t i = 0; i < span.count; ++i) {
            rows.push_back(intern_row_behavior(
                graph, span.offset + i, partition, *cache));
        }
        std::sort(rows.begin(), rows.end());
        out.push_back(rows.size());
        for (const std::uint32_t row_class : rows) out.push_back(row_class);
        return out;
    }

std::vector<std::uint64_t> SolveWork::Impl::coarse_state_signature(
        const std::uint32_t state_id) const {
        const AbstractState& state = calc.state(state_id);
        /* Exact probabilistic bisimulation needs only terminal observation as
         * its seed. Every non-terminal mechanic fact (including rarity,
         * affix counts, carriers, groups, influence, and junk identity) is
         * observable only through admission or an action kernel, and the
         * complete row multiset is collision-checked in every refinement
         * round. Starting from the literal representation would make the
         * monotone refinement unable to prove an unobserved difference
         * irrelevant. */
        return {calc.is_goal_state(state) ? 1u : 0u};
    }

std::vector<std::uint64_t> SolveWork::Impl::focused_schedule_signature(
        const std::uint32_t state_id) {
        const AbstractState& state = calc.state(state_id);
        /* Scheduling classes are not equivalence classes and never merge a
         * state. Preserve representatives from distinct exact goal-progress
         * and affix-capacity regions so a large renewal outcome set cannot
         * make its most common zero-progress carriers monopolize a round. */
        return {
            calc.is_goal_state(state) ? 1u : 0u,
            satisfied_goal_mask_for_state(state_id),
            carrier_facts(state).goal_family_mask,
            state.blocked_mask,
            state.rarity,
            state.prefix_count,
            state.suffix_count,
            static_cast<std::uint64_t>(
                state.flags &
                (kFlagCraftedMod | kFlagPrefixesLocked |
                 kFlagSuffixesLocked))};
    }

 std::uint64_t SolveWork::Impl::observation_signature_hash(
        const std::vector<std::uint64_t>& signature) {
        std::uint64_t hash = 1469598103934665603ull;
        for (const std::uint64_t token : signature) {
            hash ^= token;
            hash *= 1099511628211ull;
        }
        return hash;
    }

std::string SolveWork::Impl::first_equivalence_witness(
        const SolveTransitionCache& graph,
        const std::uint32_t left,
        const std::uint32_t right) const {
        const StateRowSpan left_span = graph.state_rows.at(left);
        const StateRowSpan right_span = graph.state_rows.at(right);
        std::string action = "action_availability_or_projected_successor";
        const auto first_action = [&](const StateRowSpan span)
            -> std::string {
            if (span.count == 0) return {};
            const SparseRow& row = graph.rows.at(span.offset);
            if (row.variant_count == 0) return {};
            const SparseVariant& variant = graph.variant_arena->variants.at(
                graph.variant_arena->row_variant_indices.at(
                    row.variant_offset));
            return calc.operators().at(variant.operator_index).id;
        };
        const std::string left_action = first_action(left_span);
        const std::string right_action = first_action(right_span);
        if (!left_action.empty()) action = left_action;
        else if (!right_action.empty()) action = right_action;
        return "{\"left_state\":" + std::to_string(left) +
               ",\"right_state\":" + std::to_string(right) +
               ",\"action\":\"" + action +
               "\",\"reason\":\"exact_all_action_partition_split\"}";
    }

void SolveWork::Impl::collect_action_observation_cardinalities(
        const SolveTransitionCache& graph) {
        using ExactBuckets = std::unordered_map<
            std::uint64_t, std::vector<std::vector<std::uint64_t>>>;
        std::vector<ExactBuckets> observed(calc.operators().size());
        for (const SparseRow& row : graph.rows) {
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    graph.variant_arena->variants.at(
                    graph.variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                std::vector<std::uint64_t> signature{
                    row.transition_offset,
                    row.transition_count,
                    row.choice_offset,
                    row.choice_count,
                    std::bit_cast<std::uint64_t>(row.self_probability),
                    variant.quantity_count};
                for (std::uint32_t q = 0; q < variant.quantity_count; ++q) {
                    signature.push_back(std::bit_cast<std::uint64_t>(
                        graph.variant_arena->variant_quantities.at(
                            variant.quantity_offset + q)));
                }
                signature.push_back(variant.choice_option_count);
                for (std::uint32_t c = 0;
                     c < variant.choice_option_count; ++c) {
                    const OutcomeChoiceOption& choice = graph.choice_options.at(
                        variant.choice_option_offset + c);
                    signature.push_back(choice.mod_id);
                    signature.push_back(choice.state);
                    signature.push_back(choice.observation_state);
                    signature.push_back(choice.actual_state);
                }
                ExactBuckets& action = observed.at(variant.operator_index);
                const std::uint64_t hash = observation_signature_hash(signature);
                auto& collisions = action[hash];
                if (std::find(collisions.begin(), collisions.end(), signature) ==
                    collisions.end()) {
                    collisions.push_back(std::move(signature));
                }
            }
        }
        result.diagnostics.action_observation_cardinalities.clear();
        for (std::uint32_t operator_index = 0;
             operator_index < observed.size(); ++operator_index) {
            std::uint64_t cardinality = 0;
            for (const auto& [unused_hash, collisions] :
                 observed[operator_index]) {
                (void)unused_hash;
                cardinality += collisions.size();
            }
            if (cardinality == 0) continue;
            std::string entry = "{\"action\":";
            append_json_string(entry, calc.operators().at(operator_index).id);
            entry += ",\"strict_observation_signatures\":" +
                     std::to_string(cardinality) + "}";
            result.diagnostics.action_observation_cardinalities.push_back(
                std::move(entry));
        }
    }

std::vector<std::uint64_t> SolveWork::Impl::shadow_state_signature(
        const SolveTransitionCache& graph,
        const std::uint32_t state) const {
        std::vector<std::uint64_t> out;
        out.push_back(calc.is_goal_state(calc.state(state)) ? 1u : 0u);
        out.push_back(
            state < graph.expanded.size() && graph.expanded[state] ? 1u : 0u);
        const StateRowSpan span = state < graph.state_rows.size()
                                      ? graph.state_rows[state]
                                      : StateRowSpan{};
        std::vector<std::vector<std::uint64_t>> rows;
        rows.reserve(span.count);
        for (std::uint32_t r = 0; r < span.count; ++r) {
            const SparseRow& row = graph.rows.at(span.offset + r);
            std::vector<std::uint64_t> signature{
                row.transition_offset,
                row.transition_count,
                row.choice_offset,
                row.choice_count,
                std::bit_cast<std::uint64_t>(row.self_probability),
                row.variant_count};
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    graph.variant_arena->variants.at(
                    graph.variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                planner_observation_signature(
                    signature, calc.operators().at(variant.operator_index));
                signature.push_back(variant.quantity_count);
                for (std::uint32_t q = 0; q < variant.quantity_count; ++q) {
                    signature.push_back(std::bit_cast<std::uint64_t>(
                        graph.variant_arena->variant_quantities.at(
                            variant.quantity_offset + q)));
                }
                signature.push_back(variant.choice_option_count);
                for (std::uint32_t c = 0;
                     c < variant.choice_option_count; ++c) {
                    const OutcomeChoiceOption& choice = graph.choice_options.at(
                        variant.choice_option_offset + c);
                    signature.push_back(choice.mod_id);
                    signature.push_back(choice.state);
                    signature.push_back(choice.observation_state);
                    signature.push_back(choice.actual_state);
                }
            }
            rows.push_back(std::move(signature));
        }
        std::sort(rows.begin(), rows.end());
        out.push_back(rows.size());
        for (const auto& row : rows) {
            out.push_back(row.size());
            out.insert(out.end(), row.begin(), row.end());
        }
        return out;
    }

void SolveWork::Impl::build_quotient_graph(
        const std::vector<std::uint32_t>& partition,
        const std::uint32_t class_count) {
        const std::shared_ptr<SolveTransitionCache> strict = transition_cache;
        const std::uint32_t strict_count = strict->discovered_states;
        std::vector<std::uint32_t> representative(class_count, kNoId);
        if (result.start_state < strict_count) {
            representative[partition[result.start_state]] = result.start_state;
        }
        for (std::uint32_t state = 0; state < strict_count; ++state) {
            if (representative[partition[state]] == kNoId) {
                representative[partition[state]] = state;
            }
        }
        result.behavioral_representative_by_state.resize(strict_count);
        for (std::uint32_t state = 0; state < strict_count; ++state) {
            result.behavioral_representative_by_state[state] =
                representative[partition[state]];
        }

        auto quotient = std::make_shared<SolveTransitionCache>();
        quotient->start_state = result.start_state;
        quotient->operator_indices = strict->operator_indices;
        quotient->max_states = strict->max_states;
        quotient->max_discovered_states = strict->max_discovered_states;
        quotient->max_expanded_states = strict->max_expanded_states;
        quotient->max_state_action_rows = strict->max_state_action_rows;
        quotient->max_transitions = strict->max_transitions;
        quotient->max_reforge_work = strict->max_reforge_work;
        quotient->max_solver_owned_bytes = strict->max_solver_owned_bytes;
        quotient->max_diagnostic_samples = strict->max_diagnostic_samples;
        quotient->full_evidence = strict->full_evidence;
        quotient->kernel_reuse = strict->kernel_reuse;
        quotient->discovered_states = strict_count;
        quotient->strict_discovered_states = strict_count;
        quotient->quotient_states = class_count;
        quotient->exact_quotient = true;
        quotient->behavioral_representative_by_state =
            result.behavioral_representative_by_state;
        quotient->expanded.assign(strict_count, 0);
        quotient->state_rows.resize(strict_count);
        /* Variant/resource payloads are independent of the successor
         * projection. Copy each strict arena once, then let quotient rows
         * retain their original slices. Re-copying a shared strict row's
         * variants for every behavioral representative turns exact kernel
         * reuse back into graph-sized duplication. Choice-option state IDs
         * are projected once below because those IDs are observable. */
        quotient->variant_arena = strict->variant_arena;
        quotient->accounts_variant_arena =
            focused_strict_transition_cache == nullptr;
        quotient->choice_options = strict->choice_options;
        for (OutcomeChoiceOption& choice : quotient->choice_options) {
            const auto map_state = [&](std::uint32_t& state) {
                if (state != kNoId) {
                    state = result.behavioral_representative_by_state.at(
                        state);
                }
            };
            map_state(choice.state);
            map_state(choice.observation_state);
            map_state(choice.actual_state);
        }
        quotient->automatic_rows_considered =
            strict->automatic_rows_considered;
        quotient->automatic_rows_eligible = strict->automatic_rows_eligible;
        quotient->automatic_rows_rejected = strict->automatic_rows_rejected;
        quotient->automatic_rows_collapsed = strict->automatic_rows_collapsed;
        quotient->automatic_rows_deferred = strict->automatic_rows_deferred;
        quotient->automatic_kind_telemetry =
            strict->automatic_kind_telemetry;
        quotient->automatic_admission_phases =
            strict->automatic_admission_phases;
        quotient->automatic_candidate_samples =
            strict->automatic_candidate_samples;
        quotient->owned_automatic_sample_nested_bytes =
            strict->owned_automatic_sample_nested_bytes;
        quotient->focused_partial = strict->focused_partial;
        std::map<
            std::pair<std::uint64_t, std::uint32_t>,
            std::pair<std::uint64_t, std::uint32_t>>
            projected_transition_spans;

        std::uint32_t representative_expanded = 0;
        for (const std::uint32_t owner : representative) {
            if (owner == kNoId || owner >= strict->expanded.size() ||
                !strict->expanded[owner]) {
                continue;
            }
            quotient->expanded[owner] = 1;
            ++representative_expanded;
            const StateRowSpan source_span = strict->state_rows.at(owner);
            StateRowSpan& target_span = quotient->state_rows[owner];
            target_span.offset = quotient->rows.size();
            for (std::uint32_t r = 0; r < source_span.count; ++r) {
                const SparseRow& source =
                    strict->rows.at(source_span.offset + r);
                SparseRow row = source;
                row.owner_state = owner;
                const auto source_transition_span = std::make_pair(
                    source.transition_offset, source.transition_count);
                auto projected = projected_transition_spans.find(
                    source_transition_span);
                if (projected == projected_transition_spans.end()) {
                    const std::uint64_t projected_offset =
                        quotient->successors.size();
                    std::map<std::uint32_t, WideFloat> mapped;
                    for (std::uint32_t i = 0;
                         i < source.transition_count; ++i) {
                        const std::uint64_t offset =
                            source.transition_offset + i;
                        const std::uint32_t successor =
                            result.behavioral_representative_by_state.at(
                                strict->successors.at(offset));
                        mapped[successor] = mapped[successor] +
                            WideFloat{strict->probabilities.at(offset)};
                    }
                    for (const auto& [successor, probability] : mapped) {
                        quotient->successors.push_back(successor);
                        quotient->probabilities.push_back(
                            probability.value());
                    }
                    projected = projected_transition_spans.emplace(
                        source_transition_span,
                        std::make_pair(
                            projected_offset,
                            static_cast<std::uint32_t>(mapped.size())))
                                    .first;
                }
                row.transition_offset = projected->second.first;
                row.transition_count = projected->second.second;
                row.embedded_self_probability = 0.0;
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint64_t offset = row.transition_offset + i;
                    if (quotient->successors.at(offset) == owner) {
                        row.embedded_self_probability +=
                            quotient->probabilities.at(offset);
                    }
                }
                const double detached_self_probability =
                    source.self_probability -
                    source.embedded_self_probability;
                row.self_probability = detached_self_probability +
                                       row.embedded_self_probability;
                row.self_probability_embedded =
                    row.embedded_self_probability > 0.0;
                row.choice_offset = quotient->choices.size();
                for (std::uint32_t i = 0; i < source.choice_count; ++i) {
                    const SparseChoiceGroup& source_group =
                        strict->choices.at(source.choice_offset + i);
                    SparseChoiceGroup group;
                    group.successor_offset =
                        quotient->choice_successors.size();
                    group.probability = source_group.probability;
                    std::vector<std::uint32_t> successors;
                    if (source_group.has_self) successors.push_back(owner);
                    for (std::uint32_t s = 0;
                         s < source_group.successor_count; ++s) {
                        successors.push_back(
                            result.behavioral_representative_by_state.at(
                                strict->choice_successors.at(
                                    source_group.successor_offset + s)));
                    }
                    std::sort(successors.begin(), successors.end());
                    successors.erase(
                        std::unique(successors.begin(), successors.end()),
                        successors.end());
                    for (const std::uint32_t successor : successors) {
                        if (successor == owner) group.has_self = true;
                        else quotient->choice_successors.push_back(successor);
                    }
                    group.successor_count = static_cast<std::uint32_t>(
                        quotient->choice_successors.size() -
                        group.successor_offset);
                    quotient->choices.push_back(group);
                }
                row.choice_count = source.choice_count;
                row.variant_offset = source.variant_offset;
                for (std::uint32_t i = 0; i < source.variant_count; ++i) {
                    const SparseVariant& source_variant =
                        strict->variant_arena->variants.at(
                        strict->variant_arena->row_variant_indices.at(
                            source.variant_offset + i));
                    if (source_variant.operator_index == kNoId) {
                        throw std::logic_error(
                            "exact quotient source row has no operator: state=" +
                            std::to_string(owner) + " row=" +
                            std::to_string(source_span.offset + r));
                    }
                }
                row.variant_count = source.variant_count;
                if (row.self_probability > 0.0) {
                    ++quotient->algebraic_self_loops;
                }
                for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                    if (quotient->choices[row.choice_offset + i].has_self) {
                        ++quotient->algebraic_self_loops;
                    }
                }
                quotient->rows.push_back(row);
                ++target_span.count;
            }
        }
        quotient->expanded_states = representative_expanded;
        transition_cache = std::move(quotient);
        expanded = transition_cache->expanded;
        expanded_count = representative_expanded;
        priced_rows.clear();
        pricing_diagnostics_cursor = 0;
        policy_rows.clear();
    }

void SolveWork::Impl::prepare_focused_exact_quotient() {
        /* Focused lower solves retain strict Bellman identities. Completed
         * graphs are still refined by the all-action exact quotient; partial
         * frontier grouping remains scheduling-only. */
    }

void SolveWork::Impl::prepare_exact_outer_quotient() {
        const std::uint32_t state_count = transition_cache->discovered_states;
        result.diagnostics.strict_discovered_states = state_count;
        transition_cache->strict_discovered_states = state_count;
        /* A closed lower/constructive-upper bracket is already an exact
         * optimality proof. Its frontier states deliberately carry the
         * executable Restart fallback, not complete all-action rows, so they
         * must remain strict compiler identities rather than being treated as
         * candidates for the completed-graph behavioral quotient. */
        if (focused_bound_proved) {
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            transition_cache->exact_quotient = false;
            result.behavioral_representative_by_state.clear();
            transition_cache->behavioral_representative_by_state.clear();
            return;
        }
        if (state_count == 0) {
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            return;
        }

        auto [coarse, coarse_count] = exact_partition(
            state_count, [&](const std::uint32_t state) {
                return coarse_state_signature(state);
            });
        result.diagnostics.coarse_candidate_classes = coarse_count;
        std::vector<std::uint32_t> coarse_sizes(coarse_count, 0);
        for (const std::uint32_t value : coarse) ++coarse_sizes[value];
        for (const std::uint32_t size : coarse_sizes) {
            result.diagnostics.max_strict_states_per_coarse_class = std::max(
                result.diagnostics.max_strict_states_per_coarse_class, size);
        }

        const bool incomplete = result.diagnostics.resource_cap_hit ||
                                !queue.empty();
        if (incomplete && !options.full_evidence) {
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            return;
        }
        if (options.full_evidence) {
            collect_action_observation_cardinalities(*transition_cache);
        }

        if (incomplete) {
            auto [shadow, shadow_count] = exact_partition(
                state_count, [&](const std::uint32_t state) {
                    return shadow_state_signature(*transition_cache, state);
                });
            result.diagnostics.state_scaling_shadow_only = true;
            result.diagnostics.shadow_behavioral_classes = shadow_count;
            result.diagnostics.shadow_expanded_states_observed = expanded_count;
            result.diagnostics.quotient_refinement_rounds = 1;
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            std::vector<std::uint32_t> first_shadow(coarse_count, kNoId);
            std::vector<std::uint32_t> first_state(coarse_count, kNoId);
            for (std::uint32_t state = 0; state < state_count; ++state) {
                const std::uint32_t candidate = coarse[state];
                if (first_shadow[candidate] == kNoId) {
                    first_shadow[candidate] = shadow[state];
                    first_state[candidate] = state;
                } else if (first_shadow[candidate] != shadow[state]) {
                    ++result.diagnostics.witnessed_non_equivalences;
                    ++result.diagnostics.projected_successor_class_mismatches;
                    if (result.diagnostics.equivalence_witnesses.size() <
                        options.max_diagnostic_samples) {
                        result.diagnostics.equivalence_witnesses.push_back(
                            first_equivalence_witness(
                                *transition_cache, first_state[candidate],
                                state));
                    } else {
                        ++result.diagnostics.equivalence_witnesses_omitted;
                    }
                }
            }
            transition_cache->exact_quotient = !options.strict_states;
            return;
        }

        std::vector<std::uint32_t> partition = coarse;
        std::uint32_t class_count = coarse_count;
        for (;;) {
            RowObservationCache row_cache;
            auto [next_partition, next_class_count] = exact_partition(
                state_count, [&](const std::uint32_t state) {
                    return state_behavior_signature(
                        *transition_cache, state, partition, &row_cache);
                });
            ++result.diagnostics.quotient_refinement_rounds;
            if (next_partition == partition) {
                class_count = next_class_count;
                break;
            }
            partition = std::move(next_partition);
            class_count = next_class_count;
        }
        result.diagnostics.quotient_states = class_count;
        result.diagnostics.exact_behavioral_merges = state_count - class_count;
        transition_cache->quotient_states = class_count;

        std::vector<std::uint32_t> first_partition(coarse_count, kNoId);
        std::vector<std::uint32_t> first_state(coarse_count, kNoId);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            const std::uint32_t candidate = coarse[state];
            if (first_partition[candidate] == kNoId) {
                first_partition[candidate] = partition[state];
                first_state[candidate] = state;
            } else if (first_partition[candidate] != partition[state]) {
                ++result.diagnostics.witnessed_non_equivalences;
                ++result.diagnostics.projected_successor_class_mismatches;
                if (options.full_evidence &&
                    result.diagnostics.equivalence_witnesses.size() <
                        options.max_diagnostic_samples) {
                    result.diagnostics.equivalence_witnesses.push_back(
                        first_equivalence_witness(
                            *transition_cache, first_state[candidate], state));
                } else {
                    ++result.diagnostics.equivalence_witnesses_omitted;
                }
            }
        }
        if (!options.strict_states && class_count < state_count) {
            build_quotient_graph(partition, class_count);
        } else {
            transition_cache->exact_quotient = !options.strict_states;
            if (!options.strict_states) {
                result.behavioral_representative_by_state.resize(state_count);
                std::iota(
                    result.behavioral_representative_by_state.begin(),
                    result.behavioral_representative_by_state.end(), 0u);
                transition_cache->behavioral_representative_by_state =
                    result.behavioral_representative_by_state;
            }
        }
    }

}
}
