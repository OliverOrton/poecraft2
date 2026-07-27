#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

bool SolveWork::Impl::ensure_priced_operator(const std::uint32_t index) {
        if (index >= priced_operator_position.size()) {
            priced_operator_position.resize(index + 1, -1);
        }
        if (priced_operator_position[index] >= 0) return true;
        if (index >= reported_unsupported.size()) {
            reported_unsupported.resize(index + 1, false);
        }
        const PlannerOperator& planner = calc.operators().at(index);
        double cost = 0.0;
        std::vector<std::pair<std::string, double>> resource_prices;
        for (const auto& [key, quantity] : planner.resource_quantities) {
            const auto found = prices.find(key);
            if (found == prices.end()) {
                record_skipped_missing_price(planner.id);
                add_action_reason(
                    "unpriced", planner.id,
                    "missing_one_or_more_resource_prices");
                if (planner.automatic_kind !=
                    AutomaticCandidateKind::None) {
                    add_action_reason(
                        "rejected", planner.id,
                        "automatic_candidate_missing_price");
                }
                return false;
            }
            cost += quantity * found->second;
            resource_prices.push_back({key, found->second});
        }
        const bool supported =
            planner.kind == PlannerOperatorKind::FixedOption ||
            calc_supports(calc.registry().actions.at(
                planner.primitive_action));
        if (!supported) {
            reported_unsupported[index] = true;
            record_skipped_unsupported(planner.id);
            add_action_reason(
                "unsupported", planner.id,
                "no_exact_evaluator_for_requested_primitive");
            return false;
        }
        priced_operator_position[index] =
            static_cast<std::int32_t>(operators.size());
        operators.push_back({index, cost, std::move(resource_prices)});
        owned_operators_nested_bytes +=
            priced_operator_nested_bytes(operators.back());
        transition_cache->operator_indices.push_back(index);
        ++result.diagnostics.priced_scanned_actions;
        ++result.diagnostics.supported_priced_actions;
        return true;
    }

std::uint32_t SolveWork::Impl::action_goal_reach_mask(
        const std::uint32_t action_index) const {
        if (action_index == kNoId ||
            action_index >= calc.registry().actions.size()) {
            return 0;
        }
        const std::vector<std::uint64_t> reachable =
            action_explicit_affix_reachable_mask(
                session, calc.registry().actions.at(action_index));
        std::uint32_t mask = 0;
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            const std::vector<std::uint64_t>& satisfying =
                calc.layout().slots.at(slot).satisfying_mask;
            bool intersects = false;
            for (std::size_t word = 0;
                 word < reachable.size() && word < satisfying.size();
                 ++word) {
                intersects |= (reachable[word] & satisfying[word]) != 0;
            }
            if (intersects) mask |= 1u << slot;
        }
        return mask;
    }

std::uint32_t SolveWork::Impl::planner_goal_reach_mask(
        const std::uint32_t operator_index) {
        if (operator_index >= operator_goal_reach_mask.size()) {
            operator_goal_reach_mask.resize(operator_index + 1, 0);
            operator_goal_reach_computed.resize(operator_index + 1, 0);
        }
        if (operator_goal_reach_computed[operator_index]) {
            return operator_goal_reach_mask[operator_index];
        }
        const PlannerOperator& planner =
            calc.operators().at(operator_index);
        std::uint32_t mask = 0;
        const auto include = [&](const std::uint32_t action) {
            mask |= action_goal_reach_mask(action);
        };
        if (planner.kind == PlannerOperatorKind::Primitive) {
            include(planner.primitive_action);
        } else {
            for (const std::uint32_t action : planner.primitive_program) {
                include(action);
            }
            include(planner.conditional_action);
            include(planner.setup_action);
            include(planner.followup_action);
            include(planner.cleanup_action);
        }
        operator_goal_reach_mask[operator_index] = mask;
        operator_goal_reach_computed[operator_index] = 1;
        return mask;
    }

void SolveWork::Impl::prepare_goal_cover_cost() {
        if (goal_cover_cost_ready) return;
        goal_cover_cost_ready = true;
        const std::size_t slot_count = calc.layout().slots.size();
        const std::size_t mask_count = std::size_t{1} << slot_count;
        goal_cover_cost.assign(mask_count, kInfinity);
        goal_cover_cost[0] = 0.0;
        clean_goal_cover_cost.assign(mask_count, kInfinity);
        clean_goal_cover_cost[0] = 0.0;
        std::vector<std::uint32_t> cover_predecessor(mask_count, kNoId);
        std::vector<std::uint32_t> cover_action(mask_count, kNoId);
        std::vector<std::uint32_t> cover_subset(mask_count, 0);
        /* Probability-aware optimistic cover. Every stochastic primitive is
         * replaced by a stronger macro that retries for a requested nonempty
         * goal subset, preserves all prior progress, receives the best legal
         * junk blockers that can still leave the requested affix slots open,
         * and pays c / p_upper. A real strategy can only have lower success
         * probability or lose more carrier state, so this relaxed acyclic MDP
         * remains an admissible lower bound. Unknown transition families keep
         * the former p=1 set-cover behavior. */
        std::vector<std::int8_t> slot_side(slot_count, -1);
        for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
            for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
                if (pc_bitset_test(
                        calc.layout().slots[slot].satisfying_mask.data(),
                        mod)) {
                    slot_side[slot] = session.gen_type[mod];
                    break;
                }
            }
        }
        using DrawKey = std::tuple<
            std::uint32_t, std::uint32_t, std::uint32_t,
            std::uint8_t, std::uint8_t, bool>;
        std::map<DrawKey, double> draw_probability;
        const auto draw_upper = [&] (
            const std::uint32_t action,
            const std::uint32_t slot,
            const std::uint32_t satisfied,
            const std::uint8_t prefix_blockers,
            const std::uint8_t suffix_blockers,
            const bool guaranteed) {
            const DrawKey key{
                action, slot, satisfied, prefix_blockers,
                suffix_blockers, guaranteed};
            const auto found = draw_probability.find(key);
            if (found != draw_probability.end()) return found->second;
            const double probability =
                calc.optimistic_goal_draw_probability(
                    result.start_state, action, slot, satisfied,
                    prefix_blockers, suffix_blockers, guaranteed);
            draw_probability.emplace(key, probability);
            return probability;
        };
        const auto priced_action_cost = [&](const ActionDescriptor& action) {
            double cost = 0.0;
            for (const std::string& key : action.cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) || found->second < 0.0) {
                    return kInfinity;
                }
                cost += found->second;
            }
            return cost;
        };
        const auto probabilistic_shape = [](const ActionType type) {
            /* first: maximum prefix/suffix draws; second: one total draw. */
            switch (type) {
            case ActionType::Transmute:
            case ActionType::Alteration:
                return std::pair<std::uint8_t, bool>{1, false};
            case ActionType::Alchemy:
            case ActionType::Chaos:
            case ActionType::Fossil:
            case ActionType::HarvestReforge:
                return std::pair<std::uint8_t, bool>{3, false};
            case ActionType::Augment:
            case ActionType::Regal:
            case ActionType::Exalt:
            case ActionType::HarvestAugment:
                return std::pair<std::uint8_t, bool>{1, true};
            default:
                return std::pair<std::uint8_t, bool>{0, false};
            }
        };
        const auto subset_probability = [&](const std::uint32_t action,
                                             const std::uint32_t existing,
                                             const std::uint32_t subset,
                                             const std::uint8_t known_prefix_blockers,
                                             const std::uint8_t known_suffix_blockers) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            const auto [draws_per_side, one_total_draw] =
                probabilistic_shape(descriptor.params.type);
            if (draws_per_side == 0) return 1.0;
            const std::uint32_t subset_count = std::popcount(subset);
            if (one_total_draw && subset_count > 1) return 0.0;

            std::array<std::vector<std::uint32_t>, 2> by_side;
            for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                const std::int8_t side = slot_side[slot];
                if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) {
                    if ((subset & (1u << slot)) != 0) return 0.0;
                    continue;
                }
                if ((subset & (1u << slot)) != 0) {
                    by_side[side].push_back(slot);
                }
            }
            if (by_side[0].size() > draws_per_side ||
                by_side[1].size() > draws_per_side) {
                return 0.0;
            }
            const bool destructive_renewal =
                descriptor.params.type == ActionType::Transmute ||
                descriptor.params.type == ActionType::Alteration ||
                descriptor.params.type == ActionType::Alchemy ||
                descriptor.params.type == ActionType::Chaos ||
                descriptor.params.type == ActionType::Fossil ||
                descriptor.params.type == ActionType::HarvestReforge;
            /* A clean unprotected carrier cannot carry junk blockers through
             * a destructive renewal. The relaxed action may still preserve
             * already-satisfied goal slots, which is strictly more favorable
             * than the real renewal, but its fresh roll uses the clean pool.
             * Additive actions retain the stronger free-blocker relaxation. */
            const std::uint8_t prefix_blockers = destructive_renewal
                ? 0
                : known_prefix_blockers;
            const std::uint8_t suffix_blockers = destructive_renewal
                ? 0
                : known_suffix_blockers;
            double probability = 1.0;
            for (std::size_t side = 0; side < by_side.size(); ++side) {
                auto slots = by_side[side];
                if (slots.empty()) continue;
                std::sort(slots.begin(), slots.end());
                double best_order = 0.0;
                do {
                    double order = 1.0;
                    std::uint32_t acquired = existing;
                    for (const std::uint32_t slot : slots) {
                        double p = draw_upper(
                            action, slot, acquired, prefix_blockers,
                            suffix_blockers, false);
                        if (descriptor.params.type ==
                                ActionType::HarvestReforge ||
                            descriptor.params.type ==
                                ActionType::HarvestAugment) {
                            p = std::max(
                                p,
                                draw_upper(
                                    action, slot, acquired,
                                    prefix_blockers, suffix_blockers, true));
                        }
                        order *= p;
                        acquired |= 1u << slot;
                    }
                    best_order = std::max(best_order, order);
                } while (std::next_permutation(slots.begin(), slots.end()));
                double placements = 1.0;
                for (std::size_t i = 0; i < slots.size(); ++i) {
                    placements *= static_cast<double>(draws_per_side - i);
                }
                probability *= std::min(1.0, placements * best_order);
            }
            return std::min(1.0, probability);
        };

        std::vector<std::uint32_t> relaxation_actions = calc.candidates();
        const auto include_action = [&](const std::uint32_t action) {
            if (action != kNoId && action < calc.registry().actions.size() &&
                std::find(
                    relaxation_actions.begin(), relaxation_actions.end(),
                    action) == relaxation_actions.end()) {
                relaxation_actions.push_back(action);
            }
        };
        for (const std::uint32_t operator_index :
             calc.candidate_operators()) {
            const PlannerOperator& planner =
                calc.operators().at(operator_index);
            include_action(planner.primitive_action);
            for (const std::uint32_t action : planner.primitive_program) {
                include_action(action);
            }
            include_action(planner.conditional_action);
            include_action(planner.setup_action);
            include_action(planner.followup_action);
            include_action(planner.cleanup_action);
        }
        for (const std::uint32_t action :
             calc.automatic_goal_bench_actions()) {
            include_action(action);
        }
        for (const TemporaryBenchEffectClass& effect :
             calc.temporary_bench_effect_classes()) {
            include_action(effect.followup_action);
            for (const std::uint32_t action : effect.blocker_actions) {
                include_action(action);
            }
        }

        /* Keep a probability-free cover as the universal proof used for
         * price-bound action pruning and for carriers whose preserved
         * structure can change the pool. It gives every action any reachable
         * goal subset deterministically for one immediate price. */
        const auto relax_cover = [&] (
            std::vector<double>& cover,
            const bool probability_aware) {
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                if (!std::isfinite(cover[mask])) continue;
                for (const std::uint32_t action : relaxation_actions) {
                    const ActionDescriptor& descriptor =
                        calc.registry().actions.at(action);
                    const double cost = priced_action_cost(descriptor);
                    if (!std::isfinite(cost) || cost < 0.0) continue;
                    const std::uint32_t missing_reach =
                        action_goal_reach_mask(action) & ~mask;
                    for (std::uint32_t subset = missing_reach;
                         subset != 0;
                         subset = (subset - 1) & missing_reach) {
                        const double probability = probability_aware
                            ? subset_probability(
                                  action, mask, subset, 0, 0)
                            : 1.0;
                        if (!(probability > 0.0) ||
                            !std::isfinite(probability)) {
                            continue;
                        }
                        const std::uint32_t produced = mask | subset;
                        const double candidate =
                            cover[mask] + cost / probability;
                        if (candidate < cover[produced]) {
                            cover[produced] = candidate;
                            if (probability_aware) {
                                cover_predecessor[produced] = mask;
                                cover_action[produced] = action;
                                cover_subset[produced] = subset;
                            }
                        }
                    }
                }
            }
        };
        relax_cover(goal_cover_cost, false);
        (void)cover_predecessor;
        (void)cover_action;
        (void)cover_subset;
        if (slot_count < 2) {
            clean_goal_cover_cost.clear();
            clean_goal_escape_cost.clear();
            clean_goal_escape_action.clear();
            clean_goal_no_exalt_escape_cost.clear();
            clean_goal_no_exalt_escape_action.clear();
            return;
        }

        /* Goal-progress/rarity relaxation for clean carriers. It is a real
         * finite MDP rather than an acyclic set cover: destructive rolls
         * replace the prior goal subset, their zero-target outcomes land at
         * the action's output rarity with an empty subset, and Restart/Scour
         * must return through normal rarity. Outcome identities are made
         * optimistically clairvoyant and cumulative probabilities are union
         * bounds, so this MDP can only be easier than the exact item process. */
        constexpr std::uint32_t kRarityCount = 3;
        constexpr std::uint32_t kAffixCountStates = 4;
        const auto abstract_index = [&](const std::uint8_t rarity,
                                        const std::uint32_t mask,
                                        const std::uint8_t prefixes,
                                        const std::uint8_t suffixes) {
            return (((static_cast<std::size_t>(rarity) * mask_count + mask) *
                      kAffixCountStates + prefixes) *
                     kAffixCountStates + suffixes);
        };
        clean_goal_cover_cost.assign(
            kRarityCount * mask_count * kAffixCountStates *
                kAffixCountStates,
            0.0);
        clean_goal_escape_cost.assign(
            clean_goal_cover_cost.size(), kInfinity);
        clean_goal_escape_action.assign(
            clean_goal_cover_cost.size(), kNoId);
        clean_goal_no_exalt_escape_cost.assign(
            clean_goal_cover_cost.size(), kInfinity);
        clean_goal_no_exalt_escape_action.assign(
            clean_goal_cover_cost.size(), kNoId);
        std::vector<std::uint32_t> clean_goal_policy(
            clean_goal_cover_cost.size(), kNoId);
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        const auto is_abstract_goal = [&](const std::uint8_t rarity,
                                          const std::uint32_t mask) {
            return rarity == calc.goal().rarity &&
                   std::popcount(mask) >= required;
        };
        const auto output_rarity = [](const ActionType type,
                                      const std::uint8_t input) {
            switch (type) {
            case ActionType::Transmute:
            case ActionType::Alteration:
                return static_cast<std::uint8_t>(PC_RARITY_MAGIC);
            case ActionType::Alchemy:
            case ActionType::Chaos:
            case ActionType::Essence:
            case ActionType::Fossil:
            case ActionType::HarvestReforge:
            case ActionType::Regal:
                return static_cast<std::uint8_t>(PC_RARITY_RARE);
            case ActionType::Scour:
                return static_cast<std::uint8_t>(PC_RARITY_NORMAL);
            default:
                return input;
            }
        };
        const auto is_destructive = [](const ActionType type) {
            return type == ActionType::Transmute ||
                   type == ActionType::Alteration ||
                   type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        std::vector<std::array<std::uint8_t, 2>> minimum_goal_affixes(
            mask_count,
            {std::numeric_limits<std::uint8_t>::max(),
             std::numeric_limits<std::uint8_t>::max()});
        minimum_goal_affixes[0] = {0, 0};
        for (std::size_t side = 0; side < 2; ++side) {
            std::vector<std::uint8_t> minimum(
                mask_count, std::numeric_limits<std::uint8_t>::max());
            minimum[0] = 0;
            for (std::uint32_t covered = 0; covered < mask_count; ++covered) {
                if (minimum[covered] ==
                    std::numeric_limits<std::uint8_t>::max()) {
                    continue;
                }
                for (std::uint32_t mod = 0;
                     mod < session.mod_count; ++mod) {
                    if (session.gen_type[mod] != side) continue;
                    std::uint32_t mod_mask = 0;
                    for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                        if (slot_side[slot] == side &&
                            pc_bitset_test(
                                calc.layout().slots[slot]
                                    .satisfying_mask.data(),
                                mod)) {
                            mod_mask |= 1u << slot;
                        }
                    }
                    if (mod_mask == 0) continue;
                    const std::uint32_t produced = covered | mod_mask;
                    minimum[produced] = std::min<std::uint8_t>(
                        minimum[produced],
                        static_cast<std::uint8_t>(minimum[covered] + 1));
                }
            }
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                const std::uint32_t side_mask = [&] {
                    std::uint32_t value = 0;
                    for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                        if (slot_side[slot] == side &&
                            (mask & (1u << slot)) != 0) {
                            value |= 1u << slot;
                        }
                    }
                    return value;
                }();
                minimum_goal_affixes[mask][side] = minimum[side_mask];
            }
        }
        std::unordered_map<std::uint32_t, std::size_t>
            relaxation_action_position;
        for (std::size_t i = 0; i < relaxation_actions.size(); ++i) {
            relaxation_action_position.emplace(relaxation_actions[i], i);
        }
        std::vector<double> subset_probability_cache(
            relaxation_actions.size() * mask_count * mask_count *
                kAffixCountStates * kAffixCountStates,
            -1.0);
        const auto cached_subset_probability = [&] (
            const std::uint32_t action,
            const std::uint32_t existing,
            const std::uint32_t subset,
            const std::uint8_t prefix_blockers,
            const std::uint8_t suffix_blockers) {
            const std::size_t action_position =
                relaxation_action_position.at(action);
            const std::size_t index =
                ((((action_position * mask_count + existing) * mask_count +
                    subset) * kAffixCountStates + prefix_blockers) *
                  kAffixCountStates + suffix_blockers);
            double& cached = subset_probability_cache[index];
            if (cached < 0.0) {
                cached = subset_probability(
                    action, existing, subset,
                    prefix_blockers, suffix_blockers);
            }
            return cached;
        };
        struct RelaxedStochasticEnvelope {
            bool ready = false;
            double failure_probability = 1.0;
            std::vector<std::size_t> failure_successors;
            std::vector<double> success_probability;
            std::vector<std::vector<std::size_t>> success_successors;
        };
        std::vector<RelaxedStochasticEnvelope> stochastic_envelopes(
            clean_goal_cover_cost.size() * relaxation_actions.size());
        struct ExactRelaxedEntry {
            std::size_t successor = 0;
            double probability = 0.0;
        };
        std::unordered_map<std::uint32_t, std::vector<ExactRelaxedEntry>>
            exact_destructive_envelopes;
        const AbstractState& probability_anchor =
            calc.state(result.start_state);
        for (const std::uint32_t action : relaxation_actions) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            const auto [draws, unused_one_total] =
                probabilistic_shape(descriptor.params.type);
            (void)unused_one_total;
            if (draws == 0 || !is_destructive(descriptor.params.type)) {
                continue;
            }
            std::uint32_t carrier = kNoId;
            for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
                const AbstractState& candidate = calc.state(state);
                if (candidate.influence_bits !=
                        probability_anchor.influence_bits ||
                    candidate.searing_exarch_tier !=
                        probability_anchor.searing_exarch_tier ||
                    candidate.eater_of_worlds_tier !=
                        probability_anchor.eater_of_worlds_tier ||
                    candidate.fractured_goal_mask != 0 ||
                    candidate.fractured_metamod_flags != 0 ||
                    (candidate.flags & kProtectionFlags) != 0 ||
                    !action_legal(session, descriptor, candidate)) {
                    continue;
                }
                bool fractured_junk = false;
                for (const std::uint8_t count :
                     candidate.fractured_junk_counts) {
                    fractured_junk |= count != 0;
                }
                if (fractured_junk) continue;
                carrier = state;
                break;
            }
            if (carrier == kNoId) continue;
            const OutcomeDistribution& distribution =
                calc.outcomes(carrier, action);
            if (!distribution.supported ||
                !distribution.choice_groups.empty() ||
                !distribution.choice_options.empty()) {
                continue;
            }
            std::map<std::size_t, double> aggregated;
            for (const OutcomeEntry& outcome : distribution.entries) {
                const AbstractState& successor = calc.state(outcome.state);
                if (successor.rarity > PC_RARITY_RARE ||
                    successor.prefix_count >= kAffixCountStates ||
                    successor.suffix_count >= kAffixCountStates) {
                    continue;
                }
                aggregated[abstract_index(
                    successor.rarity,
                    satisfied_goal_mask_for_state(outcome.state),
                    successor.prefix_count,
                    successor.suffix_count)] += outcome.probability;
            }
            if (aggregated.empty()) continue;
            auto& stored = exact_destructive_envelopes[action];
            stored.reserve(aggregated.size());
            for (const auto& [successor, probability] : aggregated) {
                stored.push_back({successor, probability});
            }
        }
        constexpr std::uint32_t kRelaxationSweeps = 2048;
        std::uint32_t relaxation_sweeps = 0;
        double relaxation_delta = kInfinity;
        for (std::uint32_t sweep = 0; sweep < kRelaxationSweeps; ++sweep) {
            relaxation_sweeps = sweep + 1;
            double delta = 0.0;
            for (std::uint8_t rarity = PC_RARITY_NORMAL;
                 rarity <= PC_RARITY_RARE; ++rarity) {
                for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                    const std::uint8_t affix_cap = rarity_affix_cap(
                        session, rarity);
                    for (std::uint8_t prefixes = 0;
                         prefixes <= affix_cap; ++prefixes) {
                        for (std::uint8_t suffixes = 0;
                             suffixes <= affix_cap; ++suffixes) {
                            const std::size_t current = abstract_index(
                                rarity, mask, prefixes, suffixes);
                            const double previous_current =
                                clean_goal_cover_cost[current];
                            if (is_abstract_goal(rarity, mask)) {
                                clean_goal_cover_cost[current] = 0.0;
                                continue;
                            }
                            double best = kInfinity;
                            std::uint32_t best_action = kNoId;
                            const auto consider = [&](const double candidate,
                                                      const std::uint32_t action) {
                                if (candidate < best) {
                                    best = candidate;
                                    best_action = action;
                                }
                                if (action >= calc.registry().actions.size()) {
                                    return;
                                }
                                const ActionDescriptor& considered =
                                    calc.registry().actions.at(action);
                                const ActionType considered_type =
                                    considered.params.type;
                                const bool destructive_refined =
                                    considered_type == ActionType::Transmute ||
                                    considered_type == ActionType::Alteration ||
                                    considered_type == ActionType::Alchemy ||
                                    considered_type == ActionType::Chaos ||
                                    considered_type == ActionType::Essence ||
                                    considered_type == ActionType::Fossil ||
                                    considered_type ==
                                        ActionType::HarvestReforge;
                                const bool goal_bench_refined =
                                    considered_type == ActionType::Bench &&
                                    (considered.sets_flags &
                                     kProtectionFlags) == 0 &&
                                    action_goal_reach_mask(action) != 0;
                                const bool refined = considered.synthetic ||
                                    destructive_refined ||
                                    considered_type ==
                                        ActionType::Augment ||
                                    considered_type ==
                                        ActionType::Regal ||
                                    considered_type == ActionType::Scour ||
                                    goal_bench_refined;
                                if (!refined) {
                                    if (candidate <
                                        clean_goal_escape_cost[current]) {
                                        clean_goal_escape_cost[current] =
                                            candidate;
                                        clean_goal_escape_action[current] =
                                            action;
                                    }
                                    if (considered_type !=
                                            ActionType::Exalt &&
                                        candidate <
                                            clean_goal_no_exalt_escape_cost[
                                                current]) {
                                        clean_goal_no_exalt_escape_cost[
                                            current] = candidate;
                                        clean_goal_no_exalt_escape_action[
                                            current] = action;
                                    }
                                }
                            };
                            clean_goal_escape_cost[current] = kInfinity;
                            clean_goal_escape_action[current] = kNoId;
                            clean_goal_no_exalt_escape_cost[current] =
                                kInfinity;
                            clean_goal_no_exalt_escape_action[current] =
                                kNoId;
                            for (const std::uint32_t action : relaxation_actions) {
                        const ActionDescriptor& descriptor =
                            calc.registry().actions.at(action);
                        if ((descriptor.legality.rarity_mask &
                             (1u << rarity)) == 0) {
                            continue;
                        }
                        const double cost = priced_action_cost(descriptor);
                        if (!std::isfinite(cost) || cost < 0.0) continue;

                        if (descriptor.synthetic) {
                            const std::size_t successor = abstract_index(
                                PC_RARITY_NORMAL, 0, 0, 0);
                            if (successor != current) {
                                consider(
                                    cost + clean_goal_cover_cost[successor],
                                    action);
                            }
                            continue;
                        }
                        if ((descriptor.sets_flags & kProtectionFlags) != 0 ||
                            descriptor.params.type == ActionType::Fracture) {
                            /* Any route that leaves the clean domain first
                             * pays this action. Grant it the whole goal. */
                            consider(cost, action);
                            continue;
                        }
                        if (descriptor.params.type == ActionType::Scour) {
                            const std::size_t successor = abstract_index(
                                PC_RARITY_NORMAL, 0, 0, 0);
                            if (successor != current) {
                                consider(
                                    cost + clean_goal_cover_cost[successor],
                                    action);
                            }
                            continue;
                        }

                        const std::uint8_t next_rarity = output_rarity(
                            descriptor.params.type, rarity);
                        const std::uint32_t reach =
                            action_goal_reach_mask(action);
                        const auto [draws, one_total_draw] =
                            probabilistic_shape(descriptor.params.type);
                        if (draws == 0) {
                            const std::uint32_t next_mask = mask | reach;
                            std::uint8_t next_prefixes = prefixes;
                            std::uint8_t next_suffixes = suffixes;
                            if (next_mask != mask &&
                                descriptor.params.type == ActionType::Bench &&
                                descriptor.params.mod_id < session.mod_count) {
                                if (session.gen_type[descriptor.params.mod_id] ==
                                    PC_SIDE_PREFIX) {
                                    ++next_prefixes;
                                } else if (
                                    session.gen_type[descriptor.params.mod_id] ==
                                    PC_SIDE_SUFFIX) {
                                    ++next_suffixes;
                                }
                            }
                            const std::uint8_t next_cap = rarity_affix_cap(
                                session, next_rarity);
                            if (next_prefixes > next_cap ||
                                next_suffixes > next_cap) {
                                continue;
                            }
                            const std::size_t successor = abstract_index(
                                next_rarity, next_mask,
                                next_prefixes, next_suffixes);
                            if (successor != current) {
                                consider(
                                    cost + clean_goal_cover_cost[successor],
                                    action);
                            }
                            continue;
                        }

                        const bool destructive =
                            is_destructive(descriptor.params.type);
                        const auto exact_destructive =
                            exact_destructive_envelopes.find(action);
                        if (destructive &&
                            exact_destructive !=
                                exact_destructive_envelopes.end()) {
                            double self_probability = 0.0;
                            double continuation = 0.0;
                            for (const ExactRelaxedEntry& entry :
                                 exact_destructive->second) {
                                if (entry.successor == current) {
                                    self_probability += entry.probability;
                                } else {
                                    continuation += entry.probability *
                                        clean_goal_cover_cost[entry.successor];
                                }
                            }
                            if (self_probability < 1.0) {
                                consider(
                                    (cost + continuation) /
                                        (1.0 - self_probability),
                                    action);
                            }
                            continue;
                        }
                        const std::uint32_t available = destructive
                            ? reach
                            : (reach & ~mask);
                        bool disjoint_single_draw = one_total_draw;
                        for (std::uint32_t left = 0;
                             disjoint_single_draw && left < slot_count; ++left) {
                            if ((available & (1u << left)) == 0) continue;
                            for (std::uint32_t right = left + 1;
                                 right < slot_count; ++right) {
                                if ((available & (1u << right)) == 0) continue;
                                const auto& left_mask =
                                    calc.layout().slots[left].satisfying_mask;
                                const auto& right_mask =
                                    calc.layout().slots[right].satisfying_mask;
                                for (std::size_t word = 0;
                                     word < left_mask.size() &&
                                     word < right_mask.size(); ++word) {
                                    if ((left_mask[word] & right_mask[word]) !=
                                        0) {
                                        disjoint_single_draw = false;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!destructive && disjoint_single_draw &&
                            available != 0) {
                            const std::uint8_t goal_prefixes =
                                minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                            const std::uint8_t goal_suffixes =
                                minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                            const std::uint8_t prefix_blockers =
                                static_cast<std::uint8_t>(
                                    prefixes > goal_prefixes
                                        ? prefixes - goal_prefixes
                                        : 0);
                            const std::uint8_t suffix_blockers =
                                static_cast<std::uint8_t>(
                                    suffixes > goal_suffixes
                                        ? suffixes - goal_suffixes
                                        : 0);
                            double total_success = 0.0;
                            double continuation = 0.0;
                            double self_probability = 0.0;
                            bool feasible_single_draw = true;
                            for (std::uint32_t slot = 0;
                                 slot < slot_count; ++slot) {
                                const std::uint32_t subset = 1u << slot;
                                if ((available & subset) == 0) continue;
                                const double probability =
                                    cached_subset_probability(
                                        action, mask, subset,
                                        prefix_blockers, suffix_blockers);
                                const std::uint8_t added_prefixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_PREFIX];
                                const std::uint8_t added_suffixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_SUFFIX];
                                const std::uint8_t next_prefixes =
                                    static_cast<std::uint8_t>(
                                        prefixes + added_prefixes);
                                const std::uint8_t next_suffixes =
                                    static_cast<std::uint8_t>(
                                        suffixes + added_suffixes);
                                const std::uint8_t next_cap = rarity_affix_cap(
                                    session, next_rarity);
                                if (next_prefixes > next_cap ||
                                    next_suffixes > next_cap) {
                                    continue;
                                }
                                total_success += probability;
                                const std::size_t successor = abstract_index(
                                    next_rarity, mask | subset,
                                    next_prefixes, next_suffixes);
                                if (successor == current) {
                                    self_probability += probability;
                                } else {
                                    continuation += probability *
                                        clean_goal_cover_cost[successor];
                                }
                            }
                            if (total_success > 1.0 + 1e-12) {
                                feasible_single_draw = false;
                            }
                            if (feasible_single_draw) {
                                const std::size_t failure = abstract_index(
                                    next_rarity, mask, prefixes, suffixes);
                                const double failure_probability =
                                    std::max(0.0, 1.0 - total_success);
                                if (failure == current) {
                                    self_probability += failure_probability;
                                } else {
                                    continuation += failure_probability *
                                        clean_goal_cover_cost[failure];
                                }
                                if (self_probability < 1.0) {
                                    consider(
                                        (cost + continuation) /
                                            (1.0 - self_probability),
                                        action);
                                }
                                continue;
                            }
                        }
                        const std::size_t action_position =
                            relaxation_action_position.at(action);
                        RelaxedStochasticEnvelope& envelope =
                            stochastic_envelopes.at(
                                current * relaxation_actions.size() +
                                action_position);
                        if (!envelope.ready) {
                            envelope.ready = true;
                            const std::uint32_t max_count =
                                std::popcount(available);
                            std::vector<double> cumulative(
                                max_count + 2, 0.0);
                            envelope.success_probability.assign(
                                max_count + 1, 0.0);
                            envelope.success_successors.resize(max_count + 1);
                            const std::uint8_t goal_prefixes =
                                minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                            const std::uint8_t goal_suffixes =
                                minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                            const std::uint8_t prefix_blockers = destructive
                                ? 0
                                : static_cast<std::uint8_t>(
                                      prefixes > goal_prefixes
                                          ? prefixes - goal_prefixes
                                          : 0);
                            const std::uint8_t suffix_blockers = destructive
                                ? 0
                                : static_cast<std::uint8_t>(
                                      suffixes > goal_suffixes
                                          ? suffixes - goal_suffixes
                                          : 0);
                            for (std::uint32_t subset = available; subset != 0;
                                 subset = (subset - 1u) & available) {
                                const std::uint8_t added_prefixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_PREFIX];
                                const std::uint8_t added_suffixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_SUFFIX];
                                if (added_prefixes ==
                                        std::numeric_limits<
                                            std::uint8_t>::max() ||
                                    added_suffixes ==
                                        std::numeric_limits<
                                            std::uint8_t>::max()) {
                                    continue;
                                }
                                const std::uint8_t next_prefixes = destructive
                                    ? added_prefixes
                                    : static_cast<std::uint8_t>(
                                          prefixes + added_prefixes);
                                const std::uint8_t next_suffixes = destructive
                                    ? added_suffixes
                                    : static_cast<std::uint8_t>(
                                          suffixes + added_suffixes);
                                const std::uint8_t next_cap = rarity_affix_cap(
                                    session, next_rarity);
                                if (next_prefixes > next_cap ||
                                    next_suffixes > next_cap) {
                                    continue;
                                }
                                const std::uint32_t count =
                                    std::popcount(subset);
                                cumulative[count] = std::min(
                                    1.0,
                                    cumulative[count] +
                                        cached_subset_probability(
                                            action,
                                            destructive ? 0u : mask,
                                            subset,
                                            prefix_blockers,
                                            suffix_blockers));
                                envelope.success_successors[count].push_back(
                                    abstract_index(
                                        next_rarity,
                                        destructive ? subset : (mask | subset),
                                        next_prefixes, next_suffixes));
                            }
                            for (std::uint32_t count = 2;
                                 count < cumulative.size(); ++count) {
                                cumulative[count] = std::min(
                                    cumulative[count], cumulative[count - 1]);
                            }
                            envelope.failure_probability =
                                1.0 - cumulative[1];
                            for (std::uint32_t count = 1;
                                 count <= max_count; ++count) {
                                envelope.success_probability[count] =
                                    cumulative[count] - cumulative[count + 1];
                            }
                            if (destructive &&
                                (descriptor.params.type ==
                                     ActionType::Transmute ||
                                 descriptor.params.type ==
                                     ActionType::Alteration) &&
                                cumulative[1] > 0.0) {
                                envelope.failure_successors.push_back(
                                    abstract_index(next_rarity, 0, 1, 0));
                                envelope.failure_successors.push_back(
                                    abstract_index(next_rarity, 0, 0, 1));
                            } else {
                                envelope.failure_successors.push_back(
                                    abstract_index(
                                        next_rarity,
                                        destructive ? 0 : mask,
                                        destructive ? 0 : prefixes,
                                        destructive ? 0 : suffixes));
                            }
                        }

                        double self_probability = 0.0;
                        double continuation = 0.0;
                        const auto best_successor = [&](const auto& candidates) {
                            return std::min_element(
                                candidates.begin(), candidates.end(),
                                [&](const std::size_t left,
                                    const std::size_t right) {
                                    return clean_goal_cover_cost[left] <
                                           clean_goal_cover_cost[right];
                                });
                        };
                        const auto failure =
                            best_successor(envelope.failure_successors);
                        if (failure != envelope.failure_successors.end()) {
                            if (*failure == current) {
                                self_probability +=
                                    envelope.failure_probability;
                            } else {
                                continuation += envelope.failure_probability *
                                    clean_goal_cover_cost[*failure];
                            }
                        }
                        for (std::size_t count = 1;
                             count < envelope.success_probability.size();
                             ++count) {
                            const double probability =
                                envelope.success_probability[count];
                            if (!(probability > 0.0)) continue;
                            const auto successor = best_successor(
                                envelope.success_successors[count]);
                            if (successor ==
                                envelope.success_successors[count].end()) {
                                continue;
                            }
                            if (*successor == current) {
                                self_probability += probability;
                            } else {
                                continuation += probability *
                                    clean_goal_cover_cost[*successor];
                            }
                        }
                        if (self_probability < 1.0) {
                            consider(
                                (cost + continuation) /
                                    (1.0 - self_probability),
                                action);
                        }
                    }
                    if (!std::isfinite(best)) continue;
                    const double next = std::max(previous_current, best);
                    clean_goal_cover_cost[current] = next;
                    if (next == best) clean_goal_policy[current] = best_action;
                    delta = std::max(delta, next - previous_current);
                            }
                        }
                    }
            }
            relaxation_delta = delta;
            if (delta <= options.epsilon * 0.1) break;
        }
        const AbstractState& start = calc.state(result.start_state);
        const double start_lower = clean_goal_cover_cost[abstract_index(
            start.rarity, satisfied_goal_mask_for_state(result.start_state),
            start.prefix_count, start.suffix_count)];
        const std::uint32_t start_policy = clean_goal_policy[abstract_index(
            start.rarity, satisfied_goal_mask_for_state(result.start_state),
            start.prefix_count, start.suffix_count)];
        retain_action_reason(
            "included:clean_goal_progress_mdp:" + finite_json(start_lower) +
            ":sweeps=" + std::to_string(relaxation_sweeps) + ":" +
            "delta=" + finite_json(relaxation_delta) + ":" +
            (start_policy == kNoId
                ? std::string("none")
                : calc.registry().actions.at(start_policy).id));
        const std::size_t normal_empty = abstract_index(
            PC_RARITY_NORMAL, 0, 0, 0);
        const std::uint32_t normal_policy = clean_goal_policy[normal_empty];
        retain_action_reason(
            "included:clean_goal_progress_normal_empty:" +
            finite_json(clean_goal_cover_cost[normal_empty]) + ":" +
            (normal_policy == kNoId
                ? std::string("none")
                : calc.registry().actions.at(normal_policy).id));
        const std::uint32_t normal_escape =
            clean_goal_escape_action[normal_empty];
        retain_action_reason(
            "included:clean_goal_progress_normal_escape:" +
            finite_json(clean_goal_escape_cost[normal_empty]) + ":" +
            (normal_escape == kNoId
                ? std::string("none")
                : calc.registry().actions.at(normal_escape).id));
        for (const std::uint8_t rarity : {
                 static_cast<std::uint8_t>(PC_RARITY_MAGIC),
                 static_cast<std::uint8_t>(PC_RARITY_RARE)}) {
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                if (is_abstract_goal(rarity, mask)) continue;
                const std::uint8_t prefixes =
                    minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                const std::uint8_t suffixes =
                    minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                const std::uint8_t cap = rarity_affix_cap(session, rarity);
                if (prefixes > cap || suffixes > cap) continue;
                const std::size_t index = abstract_index(
                    rarity, mask, prefixes, suffixes);
                const std::uint32_t policy = clean_goal_policy[index];
                const std::uint32_t escape =
                    clean_goal_escape_action[index];
                retain_action_reason(
                    "included:clean_goal_progress_state:" +
                    std::to_string(rarity) + ":" +
                    std::to_string(mask) + ":" +
                    finite_json(clean_goal_cover_cost[index]) + ":" +
                    (policy == kNoId
                        ? std::string("none")
                        : calc.registry().actions.at(policy).id) +
                    ":escape=" +
                    finite_json(clean_goal_escape_cost[index]) + ":" +
                    (escape == kNoId
                        ? std::string("none")
                        : calc.registry().actions.at(escape).id));
            }
        }
    }

std::uint32_t SolveWork::Impl::satisfied_goal_mask_for_state(
        const std::uint32_t state) const {
        std::uint32_t mask = 0;
        const AbstractState& carrier = calc.state(state);
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            if (carrier.slot_status[slot] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                mask |= 1u << slot;
            }
        }
        return mask;
    }

double SolveWork::Impl::optimistic_completion_cost(
        const std::uint32_t satisfied_mask,
        const bool clean_carrier ,
        const std::uint8_t carrier_rarity ,
        const std::uint8_t carrier_prefixes ,
        const std::uint8_t carrier_suffixes ) {
        prepare_goal_cover_cost();
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        if (std::popcount(satisfied_mask) >= required &&
            (!clean_carrier || carrier_rarity == calc.goal().rarity)) {
            return 0.0;
        }
        if (clean_carrier) {
            const std::size_t mask_count = goal_cover_cost.size();
            constexpr std::size_t kAffixCountStates = 4;
            const std::size_t index =
                (((static_cast<std::size_t>(carrier_rarity) * mask_count +
                   satisfied_mask) * kAffixCountStates + carrier_prefixes) *
                 kAffixCountStates + carrier_suffixes);
            return index < clean_goal_cover_cost.size()
                ? clean_goal_cover_cost[index]
                : 0.0;
        }
        double best = kInfinity;
        for (std::uint32_t produced = 0;
             produced < goal_cover_cost.size(); ++produced) {
            if (std::popcount(satisfied_mask | produced) < required) {
                continue;
            }
            best = std::min(best, goal_cover_cost[produced]);
        }
        return best;
    }

bool SolveWork::Impl::clean_goal_cover_eligible(const std::uint32_t state) const {
        if (state >= calc.state_count() ||
            result.start_state >= calc.state_count()) {
            return false;
        }
        const AbstractState& carrier = calc.state(state);
        const AbstractState& start = calc.state(result.start_state);
        if ((carrier.flags & kProtectionFlags) != 0 ||
            carrier.fractured_goal_mask != 0 ||
            carrier.fractured_metamod_flags != 0 ||
            carrier.influence_bits != start.influence_bits ||
            carrier.searing_exarch_tier != start.searing_exarch_tier ||
            carrier.eater_of_worlds_tier != start.eater_of_worlds_tier) {
            return false;
        }
        for (const std::uint8_t count : carrier.fractured_junk_counts) {
            if (count != 0) return false;
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            if (count != 0) return false;
        }
        return true;
    }

double SolveWork::Impl::optimistic_completion_cost_for_state(
        const std::uint32_t state) {
        if (state >= calc.state_count()) return 0.0;
        const AbstractState& carrier = calc.state(state);
        const double coarse = optimistic_completion_cost(
            satisfied_goal_mask_for_state(state),
            clean_goal_cover_eligible(state), carrier.rarity,
            carrier.prefix_count, carrier.suffix_count);
        if (state < strict_clean_goal_cover_cost.size() &&
            std::isfinite(strict_clean_goal_cover_cost[state])) {
            return std::max(coarse, strict_clean_goal_cover_cost[state]);
        }
        return coarse;
    }

void SolveWork::Impl::prepare_strict_clean_goal_cover() {
        strict_clean_goal_cover_refresh_needed = false;
        const std::uint32_t initial_state_count = calc.state_count();
        if (strict_clean_goal_cover_state_count == initial_state_count) return;
        strict_clean_goal_cover_state_count = 0;
        strict_clean_goal_cover_cost.clear();
        if (!goal_cover_cost_ready || clean_goal_escape_cost.empty()) return;

        const auto refined_action = [&](const std::uint32_t action) {
            if (action >= calc.registry().actions.size()) return false;
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            const ActionType type = descriptor.params.type;
            const bool destructive =
                type == ActionType::Transmute ||
                type == ActionType::Alteration ||
                type == ActionType::Alchemy ||
                type == ActionType::Chaos ||
                type == ActionType::Essence ||
                type == ActionType::Fossil ||
                type == ActionType::HarvestReforge;
            const bool goal_bench = type == ActionType::Bench &&
                (descriptor.sets_flags & kProtectionFlags) == 0 &&
                action_goal_reach_mask(action) != 0;
            return descriptor.synthetic ||
                   destructive || type == ActionType::Augment ||
                   type == ActionType::Regal ||
                   type == ActionType::Exalt ||
                   type == ActionType::Scour ||
                   goal_bench;
        };
        std::vector<std::uint32_t> actions;
        for (const std::uint32_t action : calc.candidates()) {
            if (refined_action(action)) actions.push_back(action);
        }
        if (actions.empty()) return;
        const auto action_cost = [&](const std::uint32_t action) {
            double cost = 0.0;
            for (const std::string& key :
                 calc.registry().actions.at(action).cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) || found->second < 0.0) {
                    return kInfinity;
                }
                cost += found->second;
            }
            return cost;
        };
        const auto strict_state = [&](const std::uint32_t state) {
            if (state >= calc.state_count() ||
                calc.is_goal_state(calc.state(state)) ||
                !clean_goal_cover_eligible(state)) {
                return false;
            }
            const std::uint8_t rarity = calc.state(state).rarity;
            return rarity == PC_RARITY_NORMAL || rarity == PC_RARITY_MAGIC;
        };
        const std::size_t mask_count = goal_cover_cost.size();
        constexpr std::size_t kAffixCountStates = 4;
        const auto clean_index = [&](const std::uint32_t state) {
            const AbstractState& carrier = calc.state(state);
            return (((static_cast<std::size_t>(carrier.rarity) * mask_count +
                      satisfied_goal_mask_for_state(state)) *
                     kAffixCountStates + carrier.prefix_count) *
                    kAffixCountStates + carrier.suffix_count);
        };
        const auto coarse_value = [&](const std::uint32_t state) {
            if (state >= calc.state_count() ||
                calc.is_goal_state(calc.state(state))) {
                return 0.0;
            }
            const AbstractState& carrier = calc.state(state);
            return optimistic_completion_cost(
                satisfied_goal_mask_for_state(state),
                clean_goal_cover_eligible(state), carrier.rarity,
                carrier.prefix_count, carrier.suffix_count);
        };

        struct StrictRow {
            std::uint32_t action = kNoId;
            double immediate = 0.0;
            double fixed_continuation = 0.0;
            std::vector<OutcomeEntry> strict_entries;
            std::vector<std::pair<std::size_t, double>> rare_entries;
            std::vector<OutcomeEntry> concrete_rare_entries;
        };
        std::vector<std::uint32_t> strict_states;
        std::vector<std::uint32_t> concrete_rare_states;
        std::vector<std::uint8_t> included(initial_state_count, 0);
        std::vector<std::uint8_t> concrete_rare_included(
            initial_state_count, 0);
        for (std::uint32_t state = 0; state < initial_state_count; ++state) {
            if (strict_state(state)) {
                included[state] = 1;
                strict_states.push_back(state);
            }
        }
        using StrictRowPtr = std::shared_ptr<const StrictRow>;
        std::vector<std::vector<StrictRowPtr>> rows(initial_state_count);
        const auto destructive_kernel_action = [&](const std::uint32_t action) {
            const ActionType type =
                calc.registry().actions.at(action).params.type;
            return type == ActionType::Transmute ||
                   type == ActionType::Alteration ||
                   type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        std::unordered_map<std::uint32_t, const OutcomeDistribution*>
            shared_destructive_kernels;
        std::unordered_map<std::uint32_t, StrictRowPtr>
            shared_destructive_rows;
        bool exact = true;
        for (std::size_t cursor = 0; cursor < strict_states.size() && exact;
             ++cursor) {
            const std::uint32_t state = strict_states[cursor];
            if (rows.size() < calc.state_count()) rows.resize(calc.state_count());
            for (const std::uint32_t action : actions) {
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(action);
                if (!action_legal(session, descriptor, calc.state(state))) {
                    continue;
                }
                const double immediate = action_cost(action);
                if (!std::isfinite(immediate)) continue;
                if (destructive_kernel_action(action)) {
                    const auto shared = shared_destructive_rows.find(action);
                    if (shared != shared_destructive_rows.end()) {
                        rows[state].push_back(shared->second);
                        continue;
                    }
                }
                const OutcomeDistribution* distribution_pointer = nullptr;
                if (destructive_kernel_action(action)) {
                    const auto shared =
                        shared_destructive_kernels.find(action);
                    if (shared != shared_destructive_kernels.end()) {
                        distribution_pointer = shared->second;
                    }
                }
                if (distribution_pointer == nullptr) {
                    distribution_pointer = &calc.outcomes(state, action);
                    if (destructive_kernel_action(action)) {
                        shared_destructive_kernels.emplace(
                            action, distribution_pointer);
                    }
                }
                const OutcomeDistribution& distribution =
                    *distribution_pointer;
                if (!distribution.supported ||
                    !distribution.choice_groups.empty() ||
                    !distribution.choice_options.empty()) {
                    exact = false;
                    break;
                }
                if (included.size() < calc.state_count()) {
                    included.resize(calc.state_count(), 0);
                    concrete_rare_included.resize(calc.state_count(), 0);
                    rows.resize(calc.state_count());
                }
                StrictRow row;
                row.action = action;
                row.immediate = immediate;
                row.strict_entries.reserve(distribution.entries.size());
                for (const OutcomeEntry& outcome : distribution.entries) {
                    if (strict_state(outcome.state)) {
                        row.strict_entries.push_back(outcome);
                        if (!included[outcome.state]) {
                            included[outcome.state] = 1;
                            strict_states.push_back(outcome.state);
                        }
                    } else if (
                        clean_goal_cover_eligible(outcome.state) &&
                        calc.state(outcome.state).rarity == PC_RARITY_RARE) {
                        row.rare_entries.push_back(
                            {clean_index(outcome.state),
                             outcome.probability});
                    } else {
                        row.fixed_continuation +=
                            outcome.probability * coarse_value(outcome.state);
                    }
                }
                std::sort(
                    row.rare_entries.begin(), row.rare_entries.end(),
                    [](const auto& left, const auto& right) {
                        return left.first < right.first;
                    });
                std::size_t write = 0;
                for (const auto& entry : row.rare_entries) {
                    if (write != 0 &&
                        row.rare_entries[write - 1].first == entry.first) {
                        row.rare_entries[write - 1].second += entry.second;
                    } else {
                        row.rare_entries[write++] = entry;
                    }
                }
                row.rare_entries.resize(write);
                StrictRowPtr retained =
                    std::make_shared<StrictRow>(std::move(row));
                if (destructive_kernel_action(action)) {
                    shared_destructive_rows.emplace(action, retained);
                }
                rows[state].push_back(std::move(retained));
            }
        }
        if (!exact) return;

        const auto destructive_action = [&](const std::uint32_t action) {
            const ActionType type =
                calc.registry().actions.at(action).params.type;
            return type == ActionType::Transmute ||
                   type == ActionType::Alteration ||
                   type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        std::vector<StrictRowPtr> rare_rows;
        if (result.start_state < calc.state_count() &&
            calc.state(result.start_state).rarity == PC_RARITY_RARE &&
            clean_goal_cover_eligible(result.start_state)) {
            for (const std::uint32_t action : actions) {
                if (!destructive_action(action)) continue;
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(action);
                if (!action_legal(
                        session, descriptor,
                        calc.state(result.start_state))) {
                    continue;
                }
                const double immediate = action_cost(action);
                if (!std::isfinite(immediate)) continue;
                const auto retained_row =
                    shared_destructive_rows.find(action);
                if (retained_row != shared_destructive_rows.end()) {
                    rare_rows.push_back(retained_row->second);
                    continue;
                }
                const OutcomeDistribution* distribution_pointer = nullptr;
                const auto shared = shared_destructive_kernels.find(action);
                if (shared != shared_destructive_kernels.end()) {
                    distribution_pointer = shared->second;
                } else {
                    distribution_pointer =
                        &calc.outcomes(result.start_state, action);
                    shared_destructive_kernels.emplace(
                        action, distribution_pointer);
                }
                const OutcomeDistribution& distribution =
                    *distribution_pointer;
                if (!distribution.supported ||
                    !distribution.choice_groups.empty() ||
                    !distribution.choice_options.empty()) {
                    exact = false;
                    break;
                }
                StrictRow row;
                row.action = action;
                row.immediate = immediate;
                for (const OutcomeEntry& outcome : distribution.entries) {
                    if (strict_state(outcome.state)) {
                        row.strict_entries.push_back(outcome);
                    } else if (
                        clean_goal_cover_eligible(outcome.state) &&
                        calc.state(outcome.state).rarity == PC_RARITY_RARE) {
                        row.rare_entries.push_back(
                            {clean_index(outcome.state),
                             outcome.probability});
                    } else {
                        row.fixed_continuation +=
                            outcome.probability * coarse_value(outcome.state);
                    }
                }
                std::sort(
                    row.rare_entries.begin(), row.rare_entries.end(),
                    [](const auto& left, const auto& right) {
                        return left.first < right.first;
                    });
                std::size_t write = 0;
                for (const auto& entry : row.rare_entries) {
                    if (write != 0 &&
                        row.rare_entries[write - 1].first == entry.first) {
                        row.rare_entries[write - 1].second += entry.second;
                    } else {
                        row.rare_entries[write++] = entry;
                    }
                }
                row.rare_entries.resize(write);
                StrictRowPtr retained =
                    std::make_shared<StrictRow>(std::move(row));
                shared_destructive_rows.emplace(action, retained);
                rare_rows.push_back(std::move(retained));
            }
        }
        if (!exact) return;

        std::uint32_t strict_exalt = kNoId;
        for (const std::uint32_t action : actions) {
            if (calc.registry().actions.at(action).params.type ==
                ActionType::Exalt) {
                strict_exalt = action;
                break;
            }
        }
        std::unordered_map<std::uint32_t, StrictRowPtr>
            concrete_exalt_rows;
        for (std::size_t cursor = 0;
             cursor < concrete_rare_states.size() && exact; ++cursor) {
            const std::uint32_t state = concrete_rare_states[cursor];
            if (strict_exalt == kNoId ||
                !action_legal(
                    session, calc.registry().actions.at(strict_exalt),
                    calc.state(state))) {
                continue;
            }
            const double immediate = action_cost(strict_exalt);
            if (!std::isfinite(immediate)) continue;
            const OutcomeDistribution& distribution =
                calc.outcomes(state, strict_exalt);
            if (!distribution.supported ||
                !distribution.choice_groups.empty() ||
                !distribution.choice_options.empty()) {
                exact = false;
                break;
            }
            if (concrete_rare_included.size() < calc.state_count()) {
                concrete_rare_included.resize(calc.state_count(), 0);
            }
            StrictRow row;
            row.action = strict_exalt;
            row.immediate = immediate;
            bool acyclic = true;
            for (const OutcomeEntry& outcome : distribution.entries) {
                if (calc.is_goal_state(calc.state(outcome.state))) continue;
                if (clean_goal_cover_eligible(outcome.state) &&
                    calc.state(outcome.state).rarity == PC_RARITY_RARE &&
                    calc.state(outcome.state).prefix_count +
                            calc.state(outcome.state).suffix_count >
                        calc.state(state).prefix_count +
                            calc.state(state).suffix_count) {
                    row.concrete_rare_entries.push_back(outcome);
                    if (!concrete_rare_included[outcome.state]) {
                        concrete_rare_included[outcome.state] = 1;
                        concrete_rare_states.push_back(outcome.state);
                    }
                } else {
                    acyclic = false;
                    break;
                }
            }
            if (acyclic) {
                concrete_exalt_rows.emplace(
                    state,
                    std::make_shared<StrictRow>(std::move(row)));
            }
        }
        if (!exact) return;
        std::stable_sort(
            concrete_rare_states.begin(), concrete_rare_states.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const std::uint8_t left_count =
                    calc.state(left).prefix_count +
                    calc.state(left).suffix_count;
                const std::uint8_t right_count =
                    calc.state(right).prefix_count +
                    calc.state(right).suffix_count;
                return left_count != right_count
                    ? left_count > right_count
                    : left < right;
            });

        strict_clean_goal_cover_cost.assign(calc.state_count(), kInfinity);
        std::vector<std::uint32_t> strict_policy(
            calc.state_count(), kNoId);
        for (const std::uint32_t state : strict_states) {
            strict_clean_goal_cover_cost[state] = 0.0;
        }
        for (const std::uint32_t state : concrete_rare_states) {
            strict_clean_goal_cover_cost[state] = 0.0;
        }
        std::vector<double> rare_value(
            clean_goal_cover_cost.size(), kInfinity);
        std::vector<std::uint32_t> rare_policy(
            clean_goal_cover_cost.size(), kNoId);
        for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
            for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
                for (std::uint8_t suffixes = 0; suffixes <= 3; ++suffixes) {
                    const std::size_t index =
                        (((static_cast<std::size_t>(PC_RARITY_RARE) *
                           mask_count + mask) * kAffixCountStates +
                          prefixes) * kAffixCountStates + suffixes);
                    rare_value[index] =
                        calc.goal().rarity == PC_RARITY_RARE &&
                                std::popcount(mask) >=
                                    calc.goal().required_satisfied_slots()
                            ? 0.0
                            : 0.0;
                }
            }
        }
        double cheapest_reset = kInfinity;
        std::uint32_t cheapest_reset_action = kNoId;
        for (const std::uint32_t action : actions) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            if (descriptor.synthetic ||
                descriptor.params.type == ActionType::Scour) {
                const double cost = action_cost(action);
                if (cost < cheapest_reset) {
                    cheapest_reset = cost;
                    cheapest_reset_action = action;
                }
            }
        }
        constexpr std::uint32_t kStrictRelaxationSweeps = 4096;
        std::uint32_t sweeps = 0;
        double delta = kInfinity;
        for (std::uint32_t sweep = 0;
             sweep < kStrictRelaxationSweeps; ++sweep) {
            sweeps = sweep + 1;
            delta = 0.0;
            const double anchor_value =
                restart_state < strict_clean_goal_cover_cost.size()
                    ? strict_clean_goal_cover_cost[restart_state]
                    : 0.0;
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
                    for (std::uint8_t suffixes = 0; suffixes <= 3;
                         ++suffixes) {
                        const std::size_t index =
                            (((static_cast<std::size_t>(PC_RARITY_RARE) *
                               mask_count + mask) * kAffixCountStates +
                              prefixes) * kAffixCountStates + suffixes);
                        if (calc.goal().rarity == PC_RARITY_RARE &&
                            std::popcount(mask) >=
                                calc.goal().required_satisfied_slots()) {
                            rare_value[index] = 0.0;
                            continue;
                        }
                        const double previous = rare_value[index];
                        double best = clean_goal_escape_cost[index];
                        std::uint32_t best_action =
                            clean_goal_escape_action[index];
                        if (std::isfinite(cheapest_reset)) {
                            const double candidate =
                                cheapest_reset + anchor_value;
                            if (candidate < best) {
                                best = candidate;
                                best_action = cheapest_reset_action;
                            }
                        }
                        for (const StrictRowPtr& row_pointer : rare_rows) {
                            const StrictRow& row = *row_pointer;
                            double continuation = row.fixed_continuation;
                            double self_probability = 0.0;
                            for (const OutcomeEntry& outcome :
                                 row.strict_entries) {
                                continuation += outcome.probability *
                                    strict_clean_goal_cover_cost[
                                        outcome.state];
                            }
                            for (const auto& [successor, probability] :
                                 row.rare_entries) {
                                if (successor == index) {
                                    self_probability += probability;
                                } else {
                                    continuation +=
                                        probability * rare_value[successor];
                                }
                            }
                            if (self_probability < 1.0) {
                                const double candidate =
                                    (row.immediate + continuation) /
                                    (1.0 - self_probability);
                                if (candidate < best) {
                                    best = candidate;
                                    best_action = row.action;
                                }
                            }
                        }
                        for (const std::uint32_t action : actions) {
                            const ActionDescriptor& descriptor =
                                calc.registry().actions.at(action);
                            if (descriptor.params.type != ActionType::Bench ||
                                (descriptor.sets_flags & kProtectionFlags) !=
                                    0) {
                                continue;
                            }
                            const std::uint32_t reach =
                                action_goal_reach_mask(action);
                            if (reach == 0 || (reach & ~mask) == 0) continue;
                            const double immediate = action_cost(action);
                            if (!std::isfinite(immediate)) continue;
                            const std::uint32_t next_mask = mask | reach;
                            const std::size_t successor =
                                (((static_cast<std::size_t>(PC_RARITY_RARE) *
                                   mask_count + next_mask) *
                                  kAffixCountStates + prefixes) *
                                 kAffixCountStates + suffixes);
                            const double candidate =
                                immediate + rare_value[successor];
                            if (candidate < best) {
                                best = candidate;
                                best_action = action;
                            }
                        }
                        if (!std::isfinite(best)) continue;
                        const double next = std::max(previous, best);
                        rare_value[index] = next;
                        if (next == best) rare_policy[index] = best_action;
                        delta = std::max(delta, next - previous);
                    }
                }
            }
            for (const std::uint32_t state : concrete_rare_states) {
                const double previous =
                    strict_clean_goal_cover_cost[state];
                const std::size_t index = clean_index(state);
                double best = clean_goal_no_exalt_escape_cost[index];
                std::uint32_t best_action =
                    clean_goal_no_exalt_escape_action[index];
                if (std::isfinite(cheapest_reset)) {
                    const double candidate =
                        cheapest_reset + anchor_value;
                    if (candidate < best) {
                        best = candidate;
                        best_action = cheapest_reset_action;
                    }
                }
                for (const StrictRowPtr& row_pointer : rare_rows) {
                    const StrictRow& row = *row_pointer;
                    double continuation = row.fixed_continuation;
                    double self_probability = 0.0;
                    for (const OutcomeEntry& outcome :
                         row.concrete_rare_entries) {
                        if (outcome.state == state) {
                            self_probability += outcome.probability;
                        } else {
                            continuation += outcome.probability *
                                strict_clean_goal_cover_cost[outcome.state];
                        }
                    }
                    for (const OutcomeEntry& outcome : row.strict_entries) {
                        continuation += outcome.probability *
                            strict_clean_goal_cover_cost[outcome.state];
                    }
                    if (self_probability < 1.0) {
                        const double candidate =
                            (row.immediate + continuation) /
                            (1.0 - self_probability);
                        if (candidate < best) {
                            best = candidate;
                            best_action = row.action;
                        }
                    }
                }
                const auto exalt_row = concrete_exalt_rows.find(state);
                if (exalt_row != concrete_exalt_rows.end()) {
                    double candidate = exalt_row->second->immediate;
                    for (const OutcomeEntry& outcome :
                         exalt_row->second->concrete_rare_entries) {
                        candidate += outcome.probability *
                            strict_clean_goal_cover_cost[outcome.state];
                    }
                    if (candidate < best) {
                        best = candidate;
                        best_action = strict_exalt;
                    }
                }
                if (!std::isfinite(best)) continue;
                const double next = std::max(previous, best);
                strict_clean_goal_cover_cost[state] = next;
                if (next == best) strict_policy[state] = best_action;
                delta = std::max(delta, next - previous);
            }
            for (const std::uint32_t state : strict_states) {
                const double previous = strict_clean_goal_cover_cost[state];
                const std::size_t index = clean_index(state);
                double best = index < clean_goal_escape_cost.size()
                    ? clean_goal_escape_cost[index]
                    : kInfinity;
                std::uint32_t best_action =
                    index < clean_goal_escape_action.size()
                        ? clean_goal_escape_action[index]
                        : kNoId;
                for (const StrictRowPtr& row_pointer : rows[state]) {
                    const StrictRow& row = *row_pointer;
                    double continuation = row.fixed_continuation;
                    double self_probability = 0.0;
                    for (const OutcomeEntry& outcome : row.strict_entries) {
                        if (outcome.state == state) {
                            self_probability += outcome.probability;
                        } else {
                            continuation += outcome.probability *
                                strict_clean_goal_cover_cost[outcome.state];
                        }
                    }
                    for (const auto& [successor, probability] :
                         row.rare_entries) {
                        continuation += probability * rare_value[successor];
                    }
                    for (const OutcomeEntry& outcome :
                         row.concrete_rare_entries) {
                        continuation += outcome.probability *
                            strict_clean_goal_cover_cost[outcome.state];
                    }
                    if (self_probability < 1.0) {
                        const double candidate =
                            (row.immediate + continuation) /
                            (1.0 - self_probability);
                        if (candidate < best) {
                            best = candidate;
                            best_action = row.action;
                        }
                    }
                }
                if (!std::isfinite(best)) continue;
                const double next = std::max(previous, best);
                strict_clean_goal_cover_cost[state] = next;
                if (next == best) strict_policy[state] = best_action;
                delta = std::max(delta, next - previous);
            }
            if (delta <= options.epsilon * 0.1) break;
        }
        if (strict_clean_goal_cover_cost.size() < calc.state_count()) {
            strict_clean_goal_cover_cost.resize(
                calc.state_count(), kInfinity);
        }
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            if (!calc.is_goal_state(calc.state(state)) &&
                clean_goal_cover_eligible(state) &&
                calc.state(state).rarity == PC_RARITY_RARE &&
                (state >= concrete_rare_included.size() ||
                 !concrete_rare_included[state])) {
                strict_clean_goal_cover_cost[state] =
                    rare_value[clean_index(state)];
            }
        }
        strict_clean_goal_cover_state_count = calc.state_count();
        strict_clean_goal_cover_refresh_needed = true;
        const double anchor = restart_state < strict_clean_goal_cover_cost.size()
            ? strict_clean_goal_cover_cost[restart_state]
            : kInfinity;
        std::map<std::uint32_t, std::uint32_t> policy_counts;
        std::map<std::uint32_t, std::uint32_t> rare_policy_counts;
        for (const std::uint32_t state : strict_states) {
            if (strict_policy[state] != kNoId) {
                ++policy_counts[strict_policy[state]];
            }
        }
        for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
            for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
                for (std::uint8_t suffixes = 0; suffixes <= 3; ++suffixes) {
                    const std::size_t index =
                        (((static_cast<std::size_t>(PC_RARITY_RARE) *
                           mask_count + mask) * kAffixCountStates +
                          prefixes) * kAffixCountStates + suffixes);
                    if (rare_policy[index] != kNoId) {
                        ++rare_policy_counts[rare_policy[index]];
                    }
                }
            }
        }
        std::string strict_reason =
            "included:strict_clean_goal_progress_mdp:" +
            finite_json(anchor) + ":states=" +
            std::to_string(strict_states.size()) + ":sweeps=" +
            std::to_string(sweeps) + ":delta=" + finite_json(delta) +
            ":anchor_action=" +
            (restart_state < strict_policy.size() &&
                     strict_policy[restart_state] != kNoId
                 ? calc.registry().actions.at(
                       strict_policy[restart_state]).id
                 : std::string("none")) +
            ":policies=";
        bool first_policy = true;
        for (const auto& [action, count] : policy_counts) {
            if (!first_policy) strict_reason += ',';
            first_policy = false;
            strict_reason += calc.registry().actions.at(action).id + '=' +
                std::to_string(count);
        }
        strict_reason += ":rare_policies=";
        first_policy = true;
        for (const auto& [action, count] : rare_policy_counts) {
            if (!first_policy) strict_reason += ',';
            first_policy = false;
            strict_reason += calc.registry().actions.at(action).id + '=' +
                std::to_string(count);
        }
        if (!result.diagnostics.action_inclusion_reasons.empty() &&
            result.diagnostics.action_inclusion_reasons.size() >=
                options.max_diagnostic_samples) {
            result.diagnostics.action_inclusion_reasons.back() =
                std::move(strict_reason);
            ++result.diagnostics.action_inclusion_reasons_omitted;
        } else {
            retain_action_reason(std::move(strict_reason));
        }
    }

double SolveWork::Impl::optimistic_operator_lower(
        const std::uint32_t state,
        const std::uint32_t operator_index) {
        if (operator_index >= priced_operator_position.size()) {
            return kInfinity;
        }
        const std::int32_t position =
            priced_operator_position[operator_index];
        if (position < 0) return kInfinity;
        const PlannerOperator& planner =
            calc.operators().at(operator_index);
        double immediate =
            operators.at(static_cast<std::size_t>(position)).cost;
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            /* Some fixed programs have conditional later primitives whose
             * aggregate planner quantity is not an admissible immediate
             * lower bound. Every legal program executes its first ordinary
             * primitive at least once, so price only that guaranteed step.
             * Granting all later reachability below remains optimistic. */
            if (planner.primitive_program.empty()) return -kInfinity;
            immediate = 0.0;
            for (const std::string& key :
                 calc.registry().actions.at(
                     planner.primitive_program.front()).cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) ||
                    found->second < 0.0) {
                    return -kInfinity;
                }
                immediate += found->second;
            }
        }
        if (!std::isfinite(immediate) || immediate < 0.0) {
            return -kInfinity;
        }
        /* Grant the operator every slot any constituent could possibly
         * produce before pricing the remaining optimistic cover. */
        const std::uint32_t optimistic_satisfied =
            satisfied_goal_mask_for_state(state) |
            planner_goal_reach_mask(operator_index);
        const double continuation =
            optimistic_completion_cost(optimistic_satisfied);
        return immediate + continuation;
    }

}
}
