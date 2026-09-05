#include "solver_solve_types.hpp"
#include "solver_phase_lower.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

std::shared_ptr<const PreparedPhaseLowerView> SolveWork::Impl::prepare_phase_lower(
        const quotient::QuotientLowerBudget& budget) {
    prepare_goal_cover_cost();
    pc_item_state phase{};
    if (!calc.materialize(result.start_state, phase))
        throw std::invalid_argument("phase lower anchor cannot be materialized");
    return PhaseLowerProducer::prepare(calc, prices, phase, goal_cover_cost, budget);
}

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

std::uint32_t SolveWork::Impl::planner_goal_may_survive_mask(
        const std::uint32_t state_id,
        const std::uint32_t operator_index) {
        if (state_id >= calc.state_count()) {
            return calc.layout().slots.empty()
                ? 0u
                : (1u << calc.layout().slots.size()) - 1u;
        }
        if (operator_index >= calc.operators().size()) {
            return satisfied_goal_mask_for_state(state_id);
        }
        if (operator_index >= operator_goal_survival_paths.size()) {
            operator_goal_survival_paths.resize(operator_index + 1);
            operator_goal_survival_computed.resize(operator_index + 1, 0);
        }
        OperatorGoalSurvivalPaths& cached =
            operator_goal_survival_paths[operator_index];
        if (!operator_goal_survival_computed[operator_index]) {
            const PlannerOperatorRuntimeSemantics semantics =
                planner_operator_runtime_semantics(
                    calc.operators().at(operator_index),
                    calc.registry());
            const std::size_t path_count = semantics.execution_paths.empty()
                ? 1
                : semantics.execution_paths.size();
            cached.paths.reserve(path_count);
            owned_goal_survival_nested_bytes +=
                cached.paths.capacity() * sizeof(GoalSurvivalPath);
            const auto cache_path = [&](const auto& runtime_path) {
                GoalSurvivalPath path;
                path.actions.reserve(runtime_path.size());
                for (const PlannerOperatorRuntimeStep& step : runtime_path) {
                    path.actions.push_back(step.action);
                }
                owned_goal_survival_nested_bytes +=
                    path.actions.capacity() * sizeof(std::uint32_t);
                cached.paths.push_back(std::move(path));
            };
            if (semantics.execution_paths.empty()) {
                cache_path(semantics.ordered_program);
            } else {
                for (const auto& runtime_path : semantics.execution_paths) {
                    cache_path(runtime_path);
                }
            }
            operator_goal_survival_computed[operator_index] = 1;
        }

        const AbstractState& source = calc.state(state_id);
        const std::uint32_t satisfied =
            satisfied_goal_mask_for_state(state_id);
        if (satisfied == 0 || cached.paths.empty()) return satisfied;

        const auto item_traits = [](const AbstractState& item) {
            std::uint8_t traits =
                item.searing_exarch_tier != item.eater_of_worlds_tier
                    ? kRefinementItemHasEldritchDominance
                    : 0;
            const bool prefix_locked =
                (item.flags & kFlagPrefixesLocked) != 0;
            const bool suffix_locked =
                (item.flags & kFlagSuffixesLocked) != 0;
            if (prefix_locked != suffix_locked) {
                traits |= kRefinementItemExactlyOneSideLocked;
            }
            return traits;
        };
        const auto affix_traits = [&](const std::int8_t side,
                                      const bool crafted,
                                      const bool fractured,
                                      const bool veiled) {
            std::uint16_t traits = side == PC_SIDE_PREFIX
                ? kRefinementAffixPrefix
                : kRefinementAffixSuffix;
            if (crafted) traits |= kRefinementAffixCrafted;
            if (fractured) traits |= kRefinementAffixFractured;
            if (veiled) traits |= kRefinementAffixVeiled;
            if ((side == PC_SIDE_PREFIX &&
                 (source.flags & kFlagPrefixesLocked) != 0) ||
                (side == PC_SIDE_SUFFIX &&
                 (source.flags & kFlagSuffixesLocked) != 0)) {
                traits |= kRefinementAffixOnLockedSide;
            }
            if (source.searing_exarch_tier !=
                source.eater_of_worlds_tier) {
                const std::int8_t dominant =
                    source.searing_exarch_tier >
                            source.eater_of_worlds_tier
                        ? PC_SIDE_PREFIX
                        : PC_SIDE_SUFFIX;
                traits |= side == dominant
                    ? kRefinementAffixOnEldritchDominantSide
                    : kRefinementAffixOnEldritchNonDominantSide;
            }
            return traits;
        };
        const auto mod_tags = [&](const std::uint32_t mod) {
            std::vector<std::uint32_t> tags;
            if (mod + 1 >= session.class_offsets.size()) return tags;
            tags.reserve(
                session.class_offsets[mod + 1] -
                session.class_offsets[mod]);
            for (std::uint32_t offset = session.class_offsets[mod];
                 offset < session.class_offsets[mod + 1]; ++offset) {
                tags.push_back(session.class_tag_ids.at(offset));
            }
            std::sort(tags.begin(), tags.end());
            tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
            return tags;
        };
        struct TraitVariant {
            std::uint16_t affix = 0;
            std::uint8_t item = 0;
            bool operator==(const TraitVariant&) const = default;
        };
        const auto variant_less = [](const TraitVariant& left,
                                     const TraitVariant& right) {
            return std::tie(left.affix, left.item) <
                   std::tie(right.affix, right.item);
        };
        constexpr std::uint16_t dynamic_affix_traits =
            kRefinementAffixOnLockedSide |
            kRefinementAffixOnEldritchDominantSide |
            kRefinementAffixOnEldritchNonDominantSide;

        std::uint32_t survivors = 0;
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            const std::uint32_t bit = 1u << slot;
            if ((satisfied & bit) == 0) continue;
            const ResolvedGoalSlot& goal_slot =
                calc.layout().slots.at(slot);
            const std::uint32_t token =
                source.goal_member_class_tokens[slot];
            const std::vector<std::uint64_t>* possible_members =
                &goal_slot.satisfying_mask;
            if (token != 0 && token <= goal_slot.member_classes.size()) {
                possible_members =
                    &goal_slot.member_classes[token - 1].member_mask;
            }
            bool saw_possible_member = false;
            bool slot_survives = false;
            if (possible_members->size() < session.words ||
                goal_slot.satisfying_mask.size() < session.words) {
                survivors |= bit;
                continue;
            }
            for (std::size_t word = 0;
                 word < session.words && !slot_survives; ++word) {
                std::uint64_t members =
                    possible_members->at(word) &
                    goal_slot.satisfying_mask.at(word);
                while (members != 0 && !slot_survives) {
                    const std::uint32_t offset =
                        static_cast<std::uint32_t>(std::countr_zero(members));
                    const std::uint32_t mod =
                        static_cast<std::uint32_t>(word * 64 + offset);
                    members &= members - 1;
                    if (mod >= session.mod_count) continue;
                    const std::int8_t side = session.gen_type.at(mod);
                    if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) {
                        continue;
                    }
                    saw_possible_member = true;
                    const std::vector<std::uint32_t> tags = mod_tags(mod);
                    const bool crafted =
                        (source.crafted_goal_mask & bit) != 0;
                    const bool fractured =
                        (source.fractured_goal_mask & bit) != 0;
                    const bool veiled = source.veiled_side == side &&
                        modifier_is_veiled_template(session, mod);
                    const TraitVariant initial{
                        affix_traits(side, crafted, fractured, veiled),
                        item_traits(source)};
                    for (const GoalSurvivalPath& path : cached.paths) {
                        std::vector<TraitVariant> variants{initial};
                        for (const std::uint32_t action : path.actions) {
                            if (variants.empty()) break;
                            if (action == kNoId) continue;
                            if (action >= calc.registry().actions.size()) {
                                /* Unknown runtime semantics cannot justify
                                 * removing a source slot from an optimistic
                                 * set. */
                                slot_survives = true;
                                break;
                            }
                            const ActionRefinementContract& contract =
                                calc.registry().actions.at(action).refinement;
                            if (!contract.complete()) {
                                slot_survives = true;
                                break;
                            }
                            if (contract.resets_to_fresh_item) {
                                variants.clear();
                                break;
                            }
                            std::vector<TraitVariant> next;
                            for (const TraitVariant& variant : variants) {
                                for (const RefinementAffixFlow& flow :
                                     contract.affix_flows) {
                                    if (!flow.preserves_modifier_classification ||
                                        !refinement_selector_matches(
                                            flow.source_selector,
                                            variant.affix,
                                            variant.item,
                                            tags)) {
                                        continue;
                                    }
                                    const std::uint16_t transformed =
                                        static_cast<std::uint16_t>(
                                            (variant.affix |
                                             flow.set_affix_traits) &
                                            ~flow.cleared_affix_traits);
                                    const std::uint16_t stable =
                                        transformed & ~dynamic_affix_traits;
                                    /* Lock and Eldritch dominance can be
                                     * changed by an earlier program step.
                                     * Enumerating the complete small trait
                                     * domain is an existential
                                     * over-approximation: it may keep a slot,
                                     * but can never erase a real survivor. */
                                    for (std::uint8_t locked = 0;
                                         locked < 2; ++locked) {
                                        for (std::uint8_t dominance = 0;
                                             dominance < 3; ++dominance) {
                                            std::uint16_t dynamic = locked
                                                ? kRefinementAffixOnLockedSide
                                                : 0;
                                            if (dominance == 1) {
                                                dynamic |=
                                                    kRefinementAffixOnEldritchDominantSide;
                                            } else if (dominance == 2) {
                                                dynamic |=
                                                    kRefinementAffixOnEldritchNonDominantSide;
                                            }
                                            for (std::uint8_t traits = 0;
                                                 traits <=
                                                     kAllRefinementItemTraits;
                                                 ++traits) {
                                                next.push_back({
                                                    static_cast<std::uint16_t>(
                                                        stable | dynamic),
                                                    traits});
                                            }
                                        }
                                    }
                                }
                            }
                            std::sort(
                                next.begin(), next.end(), variant_less);
                            next.erase(
                                std::unique(next.begin(), next.end()),
                                next.end());
                            variants = std::move(next);
                        }
                        if (slot_survives || !variants.empty()) {
                            slot_survives = true;
                            break;
                        }
                    }
                }
            }
            /* A coarse state that cannot enumerate its representative member
             * is uncertainty, not proof of destruction. */
            if (!saw_possible_member || slot_survives) survivors |= bit;
        }
        return survivors;
    }

void SolveWork::Impl::prepare_goal_cover_cost() {
        if (goal_cover_cost_ready) return;
        goal_cover_cost_ready = true;
        ProofPatternContract& universal_contract = contract(
            ProofPatternKind::UniversalCover);
        ProofPatternContract& clean_contract = contract(
            ProofPatternKind::CleanMdp);
        ProofPatternContract& carrier_contract = contract(
            ProofPatternKind::CarrierMdp);
        ProofPatternContract& bounded_gain_contract = contract(
            ProofPatternKind::BoundedGainMdp);
        ProofPatternContract& debt_contract = contract(
            ProofPatternKind::TerminalDebt);
        universal_contract.residual = 0.0;
        universal_contract.solution_sweeps = 1;
        universal_contract.converged = true;
        debt_contract.residual = 0.0;
        debt_contract.solution_sweeps = 1;
        debt_contract.converged = true;
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
                auto& slots = by_side[side];
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

        /* Automatic Eldritch side intents are synthesized after the first
         * focused lower seeds may be requested, so their final primitives
         * are not necessarily present in candidate_operators yet. Include
         * them explicitly in the universal cover. Granting one primitive
         * the whole reachable goal subset for only its own price is cheaper
         * and more capable than every real setup-bearing compound, and is
         * therefore a safe (if deliberately coarse) fallback outside the
         * clean-carrier domain. The clean MDP below gives these actions a
         * stronger side-aware projection instead of using this coarse row. */
        const auto action_by_id = [&](const char* id) {
            const auto found = calc.registry().index_by_id.find(id);
            return found == calc.registry().index_by_id.end()
                ? kNoId
                : found->second;
        };
        const std::uint32_t eldritch_annul =
            action_by_id("eldritch_annul");
        const std::uint32_t eldritch_chaos =
            action_by_id("eldritch_chaos");
        const std::uint32_t eldritch_exalt =
            action_by_id("eldritch_exalt");
        const std::uint32_t ordinary_chaos = action_by_id("chaos");
        const std::uint32_t ordinary_exalt = action_by_id("exalt");
        if (session.eldritch_eligible) {
            include_action(eldritch_annul);
            include_action(eldritch_chaos);
            include_action(eldritch_exalt);
            /* These also supply the side-macro probability authority. If a
             * primitive was not requested, allowing it in the relaxation can
             * only make the lower problem easier, while keeping every
             * pool/weight calculation in CalcContext. */
            include_action(ordinary_chaos);
            include_action(ordinary_exalt);
        }
        const auto carrier_action_cost = [&](
            const std::uint32_t action_index) {
            const ActionDescriptor& action =
                calc.registry().actions.at(action_index);
            double cost = priced_action_cost(action);
            if (!action.cost_keys.empty()) return cost;
            /* A planner-only primitive shape can price resources on its
             * wrapper even when its registry descriptor has no cost keys.
             * Reuse that proved immediate quantity without treating
             * arbitrary zero-resource descriptors as priced. */
            cost = kInfinity;
            for (const PricedOperator& priced : operators) {
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                if (planner.kind == PlannerOperatorKind::Primitive &&
                    planner.primitive_action == action_index) {
                    cost = std::min(cost, priced.cost);
                }
            }
            return cost;
        };
        carrier_unproved_first_step_actions.clear();
        carrier_priced_first_step_actions.clear();
        carrier_goal_progress_eligibility_cache.clear();
        carrier_terminal_debt_cache.clear();
        for (const std::uint32_t action_index : relaxation_actions) {
            const ActionDescriptor& action =
                calc.registry().actions[action_index];
            if (action.synthetic && !options.allow_economic_restart) {
                /* Recovery-only Restart is a constituent of the Fracture
                 * option that owns it, not a standalone carrier choice. */
                continue;
            }
            const double cost = carrier_action_cost(action_index);
            if (!std::isfinite(cost)) {
                /* An unpriced primitive is not in the executable action
                 * envelope. State-local automatic admission applies the same
                 * complete-price check before it can create a row. */
                continue;
            }
            if (cost <= 0.0) {
                carrier_unproved_first_step_actions.push_back(action_index);
            } else {
                carrier_priced_first_step_actions.emplace_back(
                    action_index, cost);
            }
        }
        if (options.full_evidence &&
            result.start_state < calc.state_count()) {
            const AbstractState& start = calc.state(result.start_state);
            for (const std::uint32_t action_index :
                 carrier_unproved_first_step_actions) {
                const ActionDescriptor& action =
                    calc.registry().actions[action_index];
                if (action_legal(session, action, start)) {
                    retain_action_reason(
                        "fallback:carrier_unproved_first_step:" +
                        action.id);
                }
            }
        }
        const auto is_eldritch_explicit_mutator = [](const ActionType type) {
            return type == ActionType::EldritchAnnul ||
                   type == ActionType::EldritchChaos ||
                   type == ActionType::EldritchExalt;
        };

        /* Carrier-aware persistent-progress layer. This is the clean MDP
         * projected onto rarity x satisfied-goal mask after granting free
         * removal of every non-goal affix and perfect preservation of every
         * satisfied goal and carrier feature. Rarity legality remains exact:
         * a cheap Normal/Magic action is not an escape from a Rare carrier.
         * This is the measured hole in the probability-free universal cover.
         *
         * A stochastic macro keeps the current goal mask on failure and
         * grants every reachable missing goal on any useful hit. Its hit
         * probability is a union upper bound over the most favorable legal
         * blocker shape. Unknown productive transition families keep p=1,
         * exactly matching the universal cover locally. Identity-changing
         * shapes may grant terminal success after their immediate price.
         * Every choice is therefore no stronger than the existing clean MDP
         * assumptions, while rare/fractured/protected carriers no longer
         * inherit actions that cannot execute at their rarity. */
        constexpr std::uint32_t kCarrierRarityCount = 3;
        const auto carrier_index = [&](const std::uint8_t rarity,
                                       const std::uint32_t mask) {
            return static_cast<std::size_t>(rarity) * mask_count + mask;
        };
        const auto carrier_output_rarity = [](
            const ActionType type, const std::uint8_t input) {
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
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        struct CarrierTransition {
            double cost = 0.0;
            double success_probability = 0.0;
            std::size_t success = 0;
            std::size_t failure = 0;
            bool terminal = false;
            std::uint32_t action = kNoId;
        };
        const std::size_t carrier_state_count =
            kCarrierRarityCount * mask_count;
        std::vector<std::vector<CarrierTransition>> carrier_transitions(
            carrier_state_count);
        std::vector<std::uint8_t> carrier_goal(carrier_state_count, 0);
        std::map<std::pair<std::uint32_t, std::uint32_t>, double>
            carrier_progress_probability;
        const auto carrier_probability_upper = [&] (
            const std::uint32_t action,
            const std::uint32_t mask,
            const std::uint32_t available,
            const ActionDescriptor& descriptor,
            const std::uint8_t draws) {
            const auto key = std::pair{action, mask};
            const auto found = carrier_progress_probability.find(key);
            if (found != carrier_progress_probability.end()) {
                return found->second;
            }
            double probability = 0.0;
            for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                const std::uint32_t bit = 1u << slot;
                if ((available & bit) == 0) continue;
                double slot_probability = 0.0;
                for (std::uint8_t prefix_blockers = 0;
                     prefix_blockers <= 3; ++prefix_blockers) {
                    for (std::uint8_t suffix_blockers = 0;
                         suffix_blockers <= 3; ++suffix_blockers) {
                        double draw = draw_upper(
                            action, slot, mask, prefix_blockers,
                            suffix_blockers, false);
                        if (descriptor.params.type ==
                                ActionType::HarvestReforge ||
                            descriptor.params.type ==
                                ActionType::HarvestAugment) {
                            draw = std::max(
                                draw,
                                draw_upper(
                                    action, slot, mask, prefix_blockers,
                                    suffix_blockers, true));
                        }
                        slot_probability = std::max(
                            slot_probability,
                            std::min(
                                1.0,
                                static_cast<double>(draws) * draw));
                    }
                }
                probability = std::min(
                    1.0, probability + slot_probability);
            }
            carrier_progress_probability.emplace(key, probability);
            return probability;
        };
        for (std::uint8_t rarity = 0;
             rarity < kCarrierRarityCount; ++rarity) {
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                const std::size_t current = carrier_index(rarity, mask);
                if (rarity == calc.goal().rarity &&
                    std::popcount(mask) >= required) {
                    carrier_goal[current] = 1;
                    continue;
                }
                for (const std::uint32_t action : relaxation_actions) {
                    const ActionDescriptor& descriptor =
                        calc.registry().actions.at(action);
                    if ((descriptor.legality.rarity_mask &
                         (1u << rarity)) == 0 ||
                        (descriptor.synthetic &&
                         !options.allow_economic_restart)) {
                        continue;
                    }
                    const double cost = carrier_action_cost(action);
                    if (!std::isfinite(cost) || cost < 0.0) continue;
                    if (descriptor.synthetic) {
                        /* Restart has one exact abstraction successor: a
                         * fresh Normal carrier with no carried goal mask.
                         * Keeping the source mask here would make replacement
                         * spuriously preserve the item it discards; omitting
                         * the priced row could instead overstate the carrier
                         * lower whenever replacement is the cheapest route. */
                        const std::size_t fresh = carrier_index(
                            PC_RARITY_NORMAL, 0);
                        carrier_transitions[current].push_back(
                            CarrierTransition{
                                cost, 0.0, fresh, fresh, false, action});
                        continue;
                    }
                    const bool leaves_probability_identity =
                        (descriptor.sets_flags & kFlagInfluenced) != 0 ||
                        (descriptor.sets_flags & kProtectionFlags) != 0 ||
                        descriptor.params.type == ActionType::Fracture;
                    if (leaves_probability_identity) {
                        carrier_transitions[current].push_back(
                            CarrierTransition{
                                cost, 1.0, current, current, true, action});
                        continue;
                    }
                    const std::uint8_t next_rarity = carrier_output_rarity(
                        descriptor.params.type, rarity);
                    const std::uint32_t available =
                        action_goal_reach_mask(action) & ~mask;
                    const auto [draws, unused_one_total_draw] =
                        probabilistic_shape(descriptor.params.type);
                    (void)unused_one_total_draw;
                    if (available == 0) {
                        const std::size_t successor =
                            carrier_index(next_rarity, mask);
                        if (successor != current) {
                            carrier_transitions[current].push_back(
                                CarrierTransition{
                                    cost, 0.0, successor, successor, false,
                                    action});
                        }
                        continue;
                    }
                    if (draws == 0) {
                        const std::size_t successor = carrier_index(
                            next_rarity, mask | available);
                        carrier_transitions[current].push_back(
                            CarrierTransition{
                                cost, 1.0, successor, successor, false,
                                action});
                        continue;
                    }

                    const double probability = carrier_probability_upper(
                        action, mask, available, descriptor, draws);
                    if (!(probability > 0.0) ||
                        !std::isfinite(probability)) {
                        continue;
                    }
                    carrier_transitions[current].push_back(
                        CarrierTransition{
                            cost, probability,
                            carrier_index(next_rarity, mask | available),
                            carrier_index(next_rarity, mask), false, action});
                }
            }
        }

        const AbstractState& start_carrier =
            calc.state(result.start_state);
        const std::size_t start_carrier_index = carrier_index(
            start_carrier.rarity,
            satisfied_goal_mask_for_state(result.start_state));
        /* The reduced transition table below owns the global MDP solve.
         * Retain its small unreduced action partition transiently so the
         * envelope pattern can later ask which action actually pins any
         * projected carrier state. */
        const auto carrier_action_transitions = carrier_transitions;

        /* Fossil and Harvest vocabularies can contribute thousands of
         * actions with the same relaxed successors. For a fixed
         * success/failure pair, higher success probability and no greater
         * immediate cost dominates before Bellman iteration. Collapse that
         * exact Pareto relation so proof work scales with distinct relaxed
         * rows rather than product action count. */
        for (auto& transitions : carrier_transitions) {
            std::sort(
                transitions.begin(), transitions.end(),
                [](const CarrierTransition& left,
                   const CarrierTransition& right) {
                    return std::tuple{
                               left.terminal, left.success, left.failure,
                               -left.success_probability, left.cost} <
                           std::tuple{
                               right.terminal, right.success, right.failure,
                               -right.success_probability, right.cost};
                });
            std::vector<CarrierTransition> reduced;
            reduced.reserve(transitions.size());
            std::size_t begin = 0;
            while (begin < transitions.size()) {
                std::size_t end = begin + 1;
                while (end < transitions.size() &&
                       transitions[end].terminal ==
                           transitions[begin].terminal &&
                       transitions[end].success ==
                           transitions[begin].success &&
                       transitions[end].failure ==
                           transitions[begin].failure) {
                    ++end;
                }
                double cheapest_at_higher_probability = kInfinity;
                for (std::size_t i = begin; i < end; ++i) {
                    if (transitions[i].cost + 1e-15 <
                        cheapest_at_higher_probability) {
                        reduced.push_back(transitions[i]);
                        cheapest_at_higher_probability =
                            transitions[i].cost;
                    }
                }
                begin = end;
            }
            transitions = std::move(reduced);
        }
        /* Exclude nonproductive rarity cycles before value iteration. They
         * otherwise increase forever from the zero lower seed and force the
         * iteration cap despite having no path to the relaxed goal. */
        std::vector<std::uint8_t> carrier_can_finish = carrier_goal;
        for (std::size_t pass = 0; pass < carrier_state_count; ++pass) {
            bool changed = false;
            for (std::size_t state = 0;
                 state < carrier_state_count; ++state) {
                if (carrier_can_finish[state]) continue;
                for (const CarrierTransition& transition :
                     carrier_transitions[state]) {
                    const bool success_finishes = transition.terminal ||
                        carrier_can_finish[transition.success];
                    const bool failure_finishes =
                        transition.success_probability >= 1.0 ||
                        transition.failure == state ||
                        carrier_can_finish[transition.failure];
                    if (success_finishes && failure_finishes) {
                        carrier_can_finish[state] = 1;
                        changed = true;
                        break;
                    }
                }
            }
            if (!changed) break;
        }
        carrier_goal_progress_cost.assign(
            carrier_state_count, 0.0);
        std::vector<double> next_carrier_cost =
            carrier_goal_progress_cost;
        /* Solve to the reported residual. If the defensive sweep ceiling is
         * ever reached, the monotone iterate remains a safe subsolution but
         * cannot report convergence. */
        double carrier_residual = kInfinity;
        std::uint32_t carrier_sweeps = 0;
        constexpr std::uint32_t kMaxCarrierProofSweeps = 4096;
        for (std::uint32_t iteration = 0;
             iteration < kMaxCarrierProofSweeps; ++iteration) {
            carrier_sweeps = iteration + 1;
            double delta = 0.0;
            next_carrier_cost = carrier_goal_progress_cost;
            for (std::size_t current = 0;
                 current < carrier_state_count; ++current) {
                if (carrier_goal[current] ||
                    !carrier_can_finish[current]) continue;
                double best = kInfinity;
                for (const CarrierTransition& transition :
                     carrier_transitions[current]) {
                    if (!transition.terminal &&
                        (!carrier_can_finish[transition.success] ||
                         (transition.success_probability < 1.0 &&
                          transition.failure != current &&
                          !carrier_can_finish[transition.failure]))) {
                        continue;
                    }
                    double candidate = transition.cost;
                    if (!transition.terminal) {
                        if (transition.success_probability <= 0.0) {
                            candidate += carrier_goal_progress_cost[
                                transition.failure];
                        } else if (transition.failure == current) {
                            candidate =
                                (transition.cost +
                                 transition.success_probability *
                                     carrier_goal_progress_cost[
                                         transition.success]) /
                                transition.success_probability;
                        } else {
                            candidate += transition.success_probability *
                                carrier_goal_progress_cost[
                                    transition.success] +
                                (1.0 - transition.success_probability) *
                                    carrier_goal_progress_cost[
                                        transition.failure];
                        }
                    }
                    best = std::min(best, candidate);
                }
                if (std::isfinite(best)) {
                    next_carrier_cost[current] = std::max(
                        carrier_goal_progress_cost[current], best);
                }
                delta = std::max(
                    delta,
                    std::abs(
                        next_carrier_cost[current] -
                        carrier_goal_progress_cost[current]));
            }
            carrier_goal_progress_cost.swap(next_carrier_cost);
            carrier_residual = delta;
            if (delta <= 1e-12) break;
        }
        carrier_contract.residual = carrier_residual;
        carrier_contract.solution_sweeps = carrier_sweeps;
        carrier_contract.converged = carrier_residual <= 1e-12;
        const double start_carrier_progress =
            start_carrier_index < carrier_goal_progress_cost.size()
                ? carrier_goal_progress_cost[start_carrier_index]
                : kInfinity;
        carrier_contract.start_contribution = start_carrier_progress;
        carrier_contract.fallback_reason = carrier_contract.converged
            ? "complete_carrier_projection"
            : "interrupted_monotone_subsolution";
        const std::size_t action_count = calc.registry().actions.size();
        carrier_goal_action_floor.assign(
            carrier_state_count * action_count, kInfinity);
        for (std::size_t current = 0;
             current < carrier_action_transitions.size(); ++current) {
            for (const CarrierTransition& transition :
                 carrier_action_transitions[current]) {
                if (transition.action >= action_count) continue;
                if (!transition.terminal &&
                    (!carrier_can_finish[transition.success] ||
                     (transition.success_probability < 1.0 &&
                      transition.failure != current &&
                      !carrier_can_finish[transition.failure]))) {
                    continue;
                }
                double candidate = transition.cost;
                if (!transition.terminal) {
                    if (transition.success_probability <= 0.0) {
                        candidate += carrier_goal_progress_cost[
                            transition.failure];
                    } else if (transition.failure == current) {
                        candidate =
                            (transition.cost +
                             transition.success_probability *
                                 carrier_goal_progress_cost[
                                     transition.success]) /
                            transition.success_probability;
                    } else {
                        candidate += transition.success_probability *
                            carrier_goal_progress_cost[transition.success] +
                            (1.0 - transition.success_probability) *
                                carrier_goal_progress_cost[
                                    transition.failure];
                    }
                }
                const std::size_t index =
                    current * action_count + transition.action;
                carrier_goal_action_floor[index] = std::min(
                    carrier_goal_action_floor[index], candidate);
            }
        }
        double minimizing_carrier = kInfinity;
        for (const std::uint32_t action : relaxation_actions) {
            const std::size_t index =
                start_carrier_index * action_count + action;
            if (index >= carrier_goal_action_floor.size() ||
                !action_legal(
                    session, calc.registry().actions[action],
                    start_carrier)) {
                continue;
            }
            if (carrier_goal_action_floor[index] < minimizing_carrier) {
                minimizing_carrier = carrier_goal_action_floor[index];
                carrier_contract.minimizing_action =
                    calc.registry().actions[action].id;
            }
        }
        double start_debt = kInfinity;
        std::uint32_t start_debt_action = kNoId;
        for (const std::uint32_t action :
             carrier_unproved_first_step_actions) {
            if (action_legal(
                    session, calc.registry().actions[action],
                    start_carrier)) {
                start_debt = 0.0;
                start_debt_action = action;
                break;
            }
        }
        if (!std::isfinite(start_debt)) {
            for (const auto& [action, cost] :
                 carrier_priced_first_step_actions) {
                if (cost < start_debt &&
                    action_legal(
                        session, calc.registry().actions[action],
                        start_carrier)) {
                    start_debt = cost;
                    start_debt_action = action;
                }
            }
        }
        debt_contract.start_contribution =
            std::isfinite(start_debt) ? start_debt : 0.0;
        debt_contract.fallback_reason = std::isfinite(start_debt)
            ? "cheapest_legal_first_step"
            : "no_priced_local_step_zero_fallback";
        if (start_debt_action < calc.registry().actions.size()) {
            debt_contract.minimizing_action =
                calc.registry().actions[start_debt_action].id;
        }
        retain_action_reason(
            "included:carrier_persistent_progress_relaxation:" +
            finite_json(start_carrier_progress));

        /* A second, independent carrier pattern keeps only goal cardinality
         * but limits a successful action to at most one new goal per affix
         * draw. This removes the exact counterexample in which Regal (one
         * draw) was allowed to grant every missing goal at once. It still
         * grants perfect preservation, the most favorable source mask, and
         * an upper success probability, so it remains a relaxation. The
         * pattern is consumed only by the final envelope during this gate. */
        struct GainTransition {
            double cost = 0.0;
            double success_probability = 0.0;
            std::size_t success = 0;
            std::size_t failure = 0;
            bool terminal = false;
            std::uint32_t action = kNoId;
        };
        const std::size_t gain_count =
            static_cast<std::size_t>(required) + 1;
        const auto gain_index = [&] (
            const std::uint8_t rarity,
            const std::uint32_t progress) {
            return static_cast<std::size_t>(rarity) * gain_count +
                std::min<std::size_t>(progress, required);
        };
        const std::size_t gain_state_count =
            kCarrierRarityCount * gain_count;
        std::vector<std::vector<GainTransition>> gain_transitions(
            gain_state_count);
        std::vector<std::uint8_t> gain_goal(gain_state_count, 0);
        for (std::uint8_t rarity = 0;
             rarity < kCarrierRarityCount; ++rarity) {
            for (std::uint32_t progress = 0;
                 progress <= required; ++progress) {
                const std::size_t current = gain_index(rarity, progress);
                if (rarity == calc.goal().rarity && progress >= required) {
                    gain_goal[current] = 1;
                    continue;
                }
                for (const std::uint32_t action : relaxation_actions) {
                    const ActionDescriptor& descriptor =
                        calc.registry().actions.at(action);
                    if ((descriptor.legality.rarity_mask &
                         (1u << rarity)) == 0 ||
                        (descriptor.synthetic &&
                         !options.allow_economic_restart)) {
                        continue;
                    }
                    const double cost = carrier_action_cost(action);
                    if (!std::isfinite(cost) || cost < 0.0) continue;
                    if (descriptor.synthetic) {
                        const std::size_t fresh = gain_index(
                            PC_RARITY_NORMAL, 0);
                        gain_transitions[current].push_back(
                            {cost, 0.0, fresh, fresh, false, action});
                        continue;
                    }
                    const bool leaves_probability_identity =
                        (descriptor.sets_flags & kFlagInfluenced) != 0 ||
                        (descriptor.sets_flags & kProtectionFlags) != 0 ||
                        descriptor.params.type == ActionType::Fracture;
                    if (leaves_probability_identity) {
                        gain_transitions[current].push_back(
                            {cost, 1.0, current, current, true, action});
                        continue;
                    }
                    const std::uint8_t next_rarity = carrier_output_rarity(
                        descriptor.params.type, rarity);
                    const auto [draws, unused_one_total_draw] =
                        probabilistic_shape(descriptor.params.type);
                    (void)unused_one_total_draw;
                    for (std::uint32_t mask = 0;
                         mask < mask_count; ++mask) {
                        if (std::popcount(mask) != progress) continue;
                        const std::uint32_t available =
                            action_goal_reach_mask(action) & ~mask;
                        if (available == 0) {
                            const std::size_t successor = gain_index(
                                next_rarity, progress);
                            if (successor != current) {
                                gain_transitions[current].push_back(
                                    {cost, 0.0, successor, successor,
                                     false, action});
                            }
                            continue;
                        }
                        const std::uint32_t remaining = required - progress;
                        if (draws == 0) {
                            const std::uint32_t gain = std::min(
                                remaining,
                                static_cast<std::uint32_t>(
                                    std::popcount(available)));
                            gain_transitions[current].push_back(
                                {cost, 1.0,
                                 gain_index(next_rarity, progress + gain),
                                 gain_index(next_rarity, progress), false,
                                 action});
                            continue;
                        }
                        const std::uint32_t gain = std::min({
                            remaining,
                            static_cast<std::uint32_t>(draws),
                            static_cast<std::uint32_t>(
                                std::popcount(available))});
                        if (gain == 0) continue;
                        const double probability = carrier_probability_upper(
                            action, mask, available, descriptor, draws);
                        if (!(probability > 0.0) ||
                            !std::isfinite(probability)) {
                            continue;
                        }
                        gain_transitions[current].push_back(
                            {cost, probability,
                             gain_index(next_rarity, progress + gain),
                             gain_index(next_rarity, progress), false,
                             action});
                    }
                }
            }
        }
        const auto gain_action_transitions = gain_transitions;
        for (auto& transitions : gain_transitions) {
            std::sort(
                transitions.begin(), transitions.end(),
                [](const GainTransition& left,
                   const GainTransition& right) {
                    return std::tuple{
                               left.terminal, left.success, left.failure,
                               -left.success_probability, left.cost} <
                           std::tuple{
                               right.terminal, right.success, right.failure,
                               -right.success_probability, right.cost};
                });
            std::vector<GainTransition> reduced;
            std::size_t begin = 0;
            while (begin < transitions.size()) {
                std::size_t end = begin + 1;
                while (end < transitions.size() &&
                       transitions[end].terminal ==
                           transitions[begin].terminal &&
                       transitions[end].success ==
                           transitions[begin].success &&
                       transitions[end].failure ==
                           transitions[begin].failure) {
                    ++end;
                }
                double cheapest_at_higher_probability = kInfinity;
                for (std::size_t i = begin; i < end; ++i) {
                    if (transitions[i].cost + 1e-15 <
                        cheapest_at_higher_probability) {
                        reduced.push_back(transitions[i]);
                        cheapest_at_higher_probability = transitions[i].cost;
                    }
                }
                begin = end;
            }
            transitions = std::move(reduced);
        }
        std::vector<std::uint8_t> gain_can_finish = gain_goal;
        for (std::size_t pass = 0; pass < gain_state_count; ++pass) {
            bool changed = false;
            for (std::size_t state = 0;
                 state < gain_state_count; ++state) {
                if (gain_can_finish[state]) continue;
                for (const GainTransition& transition :
                     gain_transitions[state]) {
                    const bool success_finishes = transition.terminal ||
                        gain_can_finish[transition.success];
                    const bool failure_finishes =
                        transition.success_probability >= 1.0 ||
                        transition.failure == state ||
                        gain_can_finish[transition.failure];
                    if (success_finishes && failure_finishes) {
                        gain_can_finish[state] = 1;
                        changed = true;
                        break;
                    }
                }
            }
            if (!changed) break;
        }
        bounded_gain_goal_progress_cost.assign(gain_state_count, 0.0);
        std::vector<double> next_gain_cost =
            bounded_gain_goal_progress_cost;
        double gain_residual = kInfinity;
        std::uint32_t gain_sweeps = 0;
        constexpr std::uint32_t kMaxMonotoneProofSweeps = 4096;
        for (std::uint32_t iteration = 0;
             iteration < kMaxMonotoneProofSweeps; ++iteration) {
            gain_sweeps = iteration + 1;
            double delta = 0.0;
            next_gain_cost = bounded_gain_goal_progress_cost;
            for (std::size_t current = 0;
                 current < gain_state_count; ++current) {
                if (gain_goal[current] || !gain_can_finish[current]) continue;
                double best = kInfinity;
                for (const GainTransition& transition :
                     gain_transitions[current]) {
                    if (!transition.terminal &&
                        (!gain_can_finish[transition.success] ||
                         (transition.success_probability < 1.0 &&
                          transition.failure != current &&
                          !gain_can_finish[transition.failure]))) {
                        continue;
                    }
                    double candidate = transition.cost;
                    if (!transition.terminal) {
                        if (transition.success_probability <= 0.0) {
                            candidate += bounded_gain_goal_progress_cost[
                                transition.failure];
                        } else if (transition.failure == current) {
                            candidate =
                                (transition.cost +
                                 transition.success_probability *
                                     bounded_gain_goal_progress_cost[
                                         transition.success]) /
                                transition.success_probability;
                        } else {
                            candidate += transition.success_probability *
                                bounded_gain_goal_progress_cost[
                                    transition.success] +
                                (1.0 - transition.success_probability) *
                                    bounded_gain_goal_progress_cost[
                                        transition.failure];
                        }
                    }
                    best = std::min(best, candidate);
                }
                if (std::isfinite(best)) {
                    next_gain_cost[current] = std::max(
                        bounded_gain_goal_progress_cost[current], best);
                }
                delta = std::max(
                    delta,
                    std::abs(
                        next_gain_cost[current] -
                        bounded_gain_goal_progress_cost[current]));
            }
            bounded_gain_goal_progress_cost.swap(next_gain_cost);
            gain_residual = delta;
            if (delta <= 1e-12) break;
        }
        bounded_gain_contract.residual = gain_residual;
        bounded_gain_contract.solution_sweeps = gain_sweeps;
        bounded_gain_contract.converged = gain_residual <= 1e-12;
        bounded_gain_action_floor.assign(
            gain_state_count * action_count, kInfinity);
        for (std::size_t current = 0;
             current < gain_action_transitions.size(); ++current) {
            for (const GainTransition& transition :
                 gain_action_transitions[current]) {
                if (transition.action >= action_count) continue;
                double candidate = transition.cost;
                if (!transition.terminal) {
                    if (transition.success_probability <= 0.0) {
                        candidate += bounded_gain_goal_progress_cost[
                            transition.failure];
                    } else if (transition.failure == current) {
                        candidate =
                            (transition.cost +
                             transition.success_probability *
                                 bounded_gain_goal_progress_cost[
                                     transition.success]) /
                            transition.success_probability;
                    } else {
                        candidate += transition.success_probability *
                            bounded_gain_goal_progress_cost[
                                transition.success] +
                            (1.0 - transition.success_probability) *
                                bounded_gain_goal_progress_cost[
                                    transition.failure];
                    }
                }
                const std::size_t index =
                    current * action_count + transition.action;
                bounded_gain_action_floor[index] = std::min(
                    bounded_gain_action_floor[index], candidate);
            }
        }
        const std::uint32_t start_progress = std::min(
            required,
            static_cast<std::uint32_t>(
                std::popcount(
                    satisfied_goal_mask_for_state(result.start_state))));
        const std::size_t start_gain_index = gain_index(
            start_carrier.rarity, start_progress);
        bounded_gain_contract.start_contribution =
            bounded_gain_goal_progress_cost[start_gain_index];
        bounded_gain_contract.fallback_reason =
            bounded_gain_contract.converged
                ? "complete_bounded_gain_projection"
                : "interrupted_monotone_subsolution";
        double minimizing_gain = kInfinity;
        for (const std::uint32_t action : relaxation_actions) {
            const std::size_t index =
                start_gain_index * action_count + action;
            if (index >= bounded_gain_action_floor.size() ||
                !action_legal(
                    session, calc.registry().actions[action],
                    start_carrier)) {
                continue;
            }
            if (bounded_gain_action_floor[index] < minimizing_gain) {
                minimizing_gain = bounded_gain_action_floor[index];
                bounded_gain_contract.minimizing_action =
                    calc.registry().actions[action].id;
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
                    if (descriptor.synthetic &&
                        !options.allow_economic_restart) {
                        continue;
                    }
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
                            cover_predecessor[produced] = mask;
                            cover_action[produced] = action;
                            cover_subset[produced] = subset;
                        }
                    }
                }
            }
        };
        relax_cover(goal_cover_cost, false);
        const std::uint32_t start_satisfied =
            satisfied_goal_mask_for_state(result.start_state);
        double universal_start = kInfinity;
        std::uint32_t universal_action = kNoId;
        for (std::uint32_t produced = 0;
             produced < goal_cover_cost.size(); ++produced) {
            if (std::popcount(start_satisfied | produced) < required ||
                goal_cover_cost[produced] >= universal_start) {
                continue;
            }
            universal_start = goal_cover_cost[produced];
            universal_action = cover_action[produced];
        }
        universal_contract.start_contribution = universal_start;
        universal_contract.fallback_reason =
            std::isfinite(universal_start)
                ? "complete_probability_free_cover"
                : "no_finite_goal_cover";
        if (universal_action < calc.registry().actions.size()) {
            universal_contract.minimizing_action =
                calc.registry().actions[universal_action].id;
        }
        (void)cover_predecessor;
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
        const auto is_abstract_goal = [&](const std::uint8_t rarity,
                                          const std::uint32_t mask,
                                          const std::uint8_t prefixes,
                                          const std::uint8_t suffixes) {
            return rarity == calc.goal().rarity &&
                   std::popcount(mask) >= required &&
                   prefixes + suffixes == std::popcount(mask);
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
        const std::size_t start_clean_index = abstract_index(
            probability_anchor.rarity,
            satisfied_goal_mask_for_state(result.start_state),
            probability_anchor.prefix_count,
            probability_anchor.suffix_count);
        clean_goal_start_action_floor.assign(
            calc.registry().actions.size(), kInfinity);
        /*
         * Incremental generation deliberately establishes the Chaos support
         * before delayed destructive rows. Calling calc.outcomes here would
         * defeat that boundary by materializing those rows while merely
         * preparing a lower bound. The analytic probability envelopes below
         * are already an admissible relaxation; reserve exact destructive
         * envelopes for the ordinary all-actions solver.
         */
        for (const std::uint32_t action :
             incremental_action_generation
                 ? std::vector<std::uint32_t>{}
                 : relaxation_actions) {
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
                calc.outcomes(
                    carrier, action,
                    options.goal_progress_gated_reforges);
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
                            if (current == start_clean_index) {
                                std::fill(
                                    clean_goal_start_action_floor.begin(),
                                    clean_goal_start_action_floor.end(),
                                    kInfinity);
                            }
                            const double previous_current =
                                clean_goal_cover_cost[current];
                            if (is_abstract_goal(
                                    rarity, mask, prefixes, suffixes)) {
                                clean_goal_cover_cost[current] = 0.0;
                                continue;
                            }
                            double best = kInfinity;
                            std::uint32_t best_action = kNoId;
                            const auto consider = [&](const double candidate,
                                                      const std::uint32_t action) {
                                if (current == start_clean_index &&
                                    action <
                                        clean_goal_start_action_floor.size() &&
                                    std::isfinite(candidate) &&
                                    candidate >= 0.0) {
                                    clean_goal_start_action_floor[action] =
                                        std::min(
                                            clean_goal_start_action_floor[
                                                action],
                                            candidate);
                                }
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
                                    is_eldritch_explicit_mutator(
                                        considered_type) ||
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
                        if (descriptor.synthetic &&
                            !options.allow_economic_restart) {
                            continue;
                        }
                        if ((descriptor.legality.rarity_mask &
                             (1u << rarity)) == 0) {
                            continue;
                        }
                        const double cost = priced_action_cost(descriptor);
                        if (!std::isfinite(cost) || cost < 0.0) continue;

                        /* The generic action projection has no intended-side
                         * identity. Treating an Eldritch mutator as an
                         * ordinary deterministic reach row would erase its
                         * stochastic cost and make the useful clean bound
                         * collapse. The side-aware optimistic macros below
                         * cover both raw and automatic forms safely. */
                        if (session.eldritch_eligible &&
                            is_eldritch_explicit_mutator(
                                descriptor.params.type)) {
                            continue;
                        }

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

                    if (session.eldritch_eligible &&
                        rarity == PC_RARITY_RARE) {
                        std::array<std::uint32_t, 2> side_goal_masks{};
                        for (std::uint32_t slot = 0;
                             slot < slot_count; ++slot) {
                            const std::int8_t side = slot_side[slot];
                            if (side == PC_SIDE_PREFIX ||
                                side == PC_SIDE_SUFFIX) {
                                side_goal_masks[side] |= 1u << slot;
                            }
                        }
                        const auto normalized_successor = [&] (
                            const std::uint32_t successor_mask)
                                -> std::optional<std::size_t> {
                            const std::uint8_t successor_prefixes =
                                minimum_goal_affixes[successor_mask]
                                                    [PC_SIDE_PREFIX];
                            const std::uint8_t successor_suffixes =
                                minimum_goal_affixes[successor_mask]
                                                    [PC_SIDE_SUFFIX];
                            const std::uint8_t cap = rarity_affix_cap(
                                session, PC_RARITY_RARE);
                            if (successor_prefixes > cap ||
                                successor_suffixes > cap) {
                                return std::nullopt;
                            }
                            return abstract_index(
                                PC_RARITY_RARE, successor_mask,
                                successor_prefixes, successor_suffixes);
                        };
                        const auto final_cost = [&](
                            const std::uint32_t action) {
                            return action == kNoId
                                ? kInfinity
                                : priced_action_cost(
                                      calc.registry().actions.at(action));
                        };
                        const auto any_goal_probability_upper = [&] (
                            const std::uint32_t probability_action,
                            const std::uint32_t preserved_mask,
                            const std::uint32_t available) {
                            if (available == 0) return 0.0;
                            if (probability_action == kNoId ||
                                relaxation_action_position.find(
                                    probability_action) ==
                                    relaxation_action_position.end()) {
                                return 1.0;
                            }
                            const std::uint8_t cap = rarity_affix_cap(
                                session, PC_RARITY_RARE);
                            double probability = 0.0;
                            for (std::uint32_t slot = 0;
                                 slot < slot_count; ++slot) {
                                const std::uint32_t bit = 1u << slot;
                                if ((available & bit) == 0) continue;
                                const std::int8_t target_side =
                                    slot_side[slot];
                                std::uint8_t prefix_blockers = cap;
                                std::uint8_t suffix_blockers = cap;
                                const std::uint8_t preserved_prefixes =
                                    minimum_goal_affixes[preserved_mask]
                                                        [PC_SIDE_PREFIX];
                                const std::uint8_t preserved_suffixes =
                                    minimum_goal_affixes[preserved_mask]
                                                        [PC_SIDE_SUFFIX];
                                prefix_blockers = preserved_prefixes < cap
                                    ? static_cast<std::uint8_t>(
                                          cap - preserved_prefixes)
                                    : 0;
                                suffix_blockers = preserved_suffixes < cap
                                    ? static_cast<std::uint8_t>(
                                          cap - preserved_suffixes)
                                    : 0;
                                /* Leave one real target-side slot for the
                                 * satisfying draw. Every other open slot may
                                 * receive the strongest legal junk-group
                                 * exclusion. This is at least as favorable as
                                 * the carrier-local side-forced pool. */
                                if (target_side == PC_SIDE_PREFIX &&
                                    prefix_blockers > 0) {
                                    --prefix_blockers;
                                } else if (
                                    target_side == PC_SIDE_SUFFIX &&
                                    suffix_blockers > 0) {
                                    --suffix_blockers;
                                }
                                probability += cached_subset_probability(
                                    probability_action, preserved_mask, bit,
                                    prefix_blockers, suffix_blockers);
                            }
                            /* The sum is a union upper bound. On a relaxed
                             * success the macro grants every missing goal on
                             * the target side, so it is strictly stronger
                             * than any real one-attempt outcome. */
                            return std::clamp(probability, 0.0, 1.0);
                        };
                        const auto consider_side_roll = [&] (
                            const std::uint32_t final_action,
                            const std::uint32_t probability_action,
                            const std::int8_t side,
                            const bool requires_opposite_progress) {
                            const double cost = final_cost(final_action);
                            if (!std::isfinite(cost) || cost < 0.0) return;
                            const std::uint32_t side_mask =
                                side_goal_masks[side];
                            const std::uint32_t opposite_mask =
                                side_goal_masks[side == PC_SIDE_PREFIX
                                    ? PC_SIDE_SUFFIX
                                    : PC_SIDE_PREFIX];
                            const std::uint32_t available =
                                side_mask & ~mask;
                            if (available == 0 ||
                                (requires_opposite_progress &&
                                 (mask & opposite_mask) == 0)) {
                                return;
                            }
                            const std::uint8_t side_count =
                                side == PC_SIDE_PREFIX
                                    ? prefixes
                                    : suffixes;
                            if (requires_opposite_progress &&
                                side_count >= rarity_affix_cap(
                                    session, rarity)) {
                                return;
                            }
                            const double probability =
                                any_goal_probability_upper(
                                    probability_action,
                                    mask & opposite_mask, available);
                            if (!(probability > 0.0) ||
                                !std::isfinite(probability)) {
                                return;
                            }
                            const auto success = normalized_successor(
                                mask | available);
                            const auto failure = normalized_successor(mask);
                            if (!success.has_value() ||
                                !failure.has_value()) {
                                return;
                            }
                            const double success_value =
                                clean_goal_cover_cost[*success];
                            const double failure_value =
                                clean_goal_cover_cost[*failure];
                            /* An upper probability is useful only when the
                             * optimistic success is no worse than failure.
                             * Otherwise the safe lower endpoint is zero. */
                            const double favorable_probability =
                                success_value <= failure_value
                                    ? probability
                                    : 0.0;
                            double self_probability = 0.0;
                            double continuation = 0.0;
                            const auto accumulate = [&] (
                                const std::size_t successor,
                                const double mass,
                                const double value) {
                                if (successor == current) {
                                    self_probability += mass;
                                } else {
                                    continuation += mass * value;
                                }
                            };
                            accumulate(
                                *success, favorable_probability,
                                success_value);
                            accumulate(
                                *failure, 1.0 - favorable_probability,
                                failure_value);
                            if (self_probability < 1.0) {
                                consider(
                                    (cost + continuation) /
                                        (1.0 - self_probability),
                                    final_action);
                            }
                        };

                        for (const std::int8_t side : {
                                 static_cast<std::int8_t>(PC_SIDE_PREFIX),
                                 static_cast<std::int8_t>(PC_SIDE_SUFFIX)}) {
                            /* Setup dominance is granted for free. Chaos may
                             * improve either side from zero progress; Exalt
                             * retains its real automatic prerequisite that
                             * useful opposite-side progress already exists. */
                            consider_side_roll(
                                eldritch_chaos, ordinary_chaos, side, false);
                            consider_side_roll(
                                eldritch_exalt, ordinary_exalt, side, true);

                            const double annul_cost =
                                final_cost(eldritch_annul);
                            const auto normalized =
                                normalized_successor(mask);
                            const std::uint8_t minimum_side_count =
                                minimum_goal_affixes[mask][side];
                            const std::uint8_t current_side_count =
                                side == PC_SIDE_PREFIX
                                    ? prefixes
                                    : suffixes;
                            if (std::isfinite(annul_cost) &&
                                annul_cost >= 0.0 &&
                                normalized.has_value() &&
                                current_side_count > minimum_side_count &&
                                *normalized != current) {
                                /* One relaxed Annul removes every target-side
                                 * junk affix, preserves every goal, and grants
                                 * the best remaining shape. This dominates a
                                 * real random one-affix removal. */
                                consider(
                                    annul_cost +
                                        clean_goal_cover_cost[*normalized],
                                    eldritch_annul);
                            }
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
        clean_contract.residual = relaxation_delta;
        clean_contract.solution_sweeps = relaxation_sweeps;
        clean_contract.converged =
            relaxation_delta <= options.epsilon * 0.1;
        const AbstractState& start = calc.state(result.start_state);
        const double start_lower = clean_goal_cover_cost[abstract_index(
            start.rarity, satisfied_goal_mask_for_state(result.start_state),
            start.prefix_count, start.suffix_count)];
        const std::uint32_t start_policy = clean_goal_policy[abstract_index(
            start.rarity, satisfied_goal_mask_for_state(result.start_state),
            start.prefix_count, start.suffix_count)];
        const bool clean_start_eligible =
            clean_goal_cover_eligible(result.start_state);
        clean_contract.start_contribution = clean_start_eligible
            ? start_lower
            : kInfinity;
        clean_contract.fallback_reason = !clean_start_eligible
            ? "start_identity_outside_clean_projection"
            : (clean_contract.converged
                ? "complete_clean_projection"
                : "interrupted_monotone_subsolution");
        if (clean_start_eligible &&
            start_policy < calc.registry().actions.size()) {
            clean_contract.minimizing_action =
                calc.registry().actions[start_policy].id;
        }
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
                const std::uint8_t prefixes =
                    minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                const std::uint8_t suffixes =
                    minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                if (is_abstract_goal(
                        rarity, mask, prefixes, suffixes)) {
                    continue;
                }
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

bool SolveWork::Impl::restart_row_allowed(const std::uint32_t state) const {
        return options.allow_economic_restart &&
               restart_operator_index != kNoId &&
               state < calc.state_count();
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
        const std::uint32_t satisfied_count =
            std::popcount(satisfied_mask);
        if (!clean_carrier && satisfied_count >= required) {
            return 0.0;
        }
        if (clean_carrier && carrier_rarity == calc.goal().rarity &&
            satisfied_count >= required &&
            carrier_prefixes + carrier_suffixes == satisfied_count) {
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

}
}
