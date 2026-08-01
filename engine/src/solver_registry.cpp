#include "solver_calc_types.hpp"

#include "harvest_crafts.generated.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "poecraft/bitset.h"
#include "poecraft/session.h"

/*
 * Solver S1: enumerate the plannable action instances for one session as
 * declarative descriptors. Costs use the simulator economy price-key
 * vocabulary (simulator.cpp compile_operation) so a solved policy prices
 * identically to a hand-built strategy. Legality mirrors the precondition
 * checks in apply_action (actions_basic.cpp), which remains the execution
 * authority.
 */
namespace poecraft {
namespace solver {

namespace {

constexpr std::uint8_t kRarityNormal = 1u << PC_RARITY_NORMAL;
constexpr std::uint8_t kRarityMagic = 1u << PC_RARITY_MAGIC;
constexpr std::uint8_t kRarityRare = 1u << PC_RARITY_RARE;
constexpr std::uint8_t kRarityAny = kRarityNormal | kRarityMagic | kRarityRare;

const std::string& mod_key(const SessionImpl& session, std::uint32_t mod_id) {
    const DataImpl& data = *session.data;
    return data.string_at(data.mod_key_sid[session.global_index[mod_id]]);
}

std::uint32_t tag_id(const SessionImpl& session, const char* name) {
    const auto it = session.data->tag_id_by_name.find(name);
    return it == session.data->tag_id_by_name.end() ? kNoId : it->second;
}

bool session_tag_nonempty(const SessionImpl& session, std::uint32_t tag) {
    if (tag == kNoId || tag >= session.implicit_tag_masks.size()) return false;
    const auto& mask = session.implicit_tag_masks[tag];
    for (std::uint64_t word : mask) {
        if (word != 0) return true;
    }
    return false;
}

bool mod_has_tag(
    const SessionImpl& session,
    const std::uint32_t mod,
    const std::uint32_t tag) {
    if (mod >= session.mod_count) return false;
    for (std::uint32_t row = session.class_offsets[mod];
         row < session.class_offsets[mod + 1]; ++row) {
        if (session.class_tag_ids[row] == tag) return true;
    }
    return false;
}

bool goal_contains_mod_family(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options,
    const std::uint32_t mod) {
    if (mod >= session.mod_count) return false;
    return std::any_of(
        options.fossil_goal_mod_ids.begin(),
        options.fossil_goal_mod_ids.end(),
        [&](const std::vector<std::uint32_t>& slot) {
            return std::any_of(
                slot.begin(), slot.end(), [&](const std::uint32_t goal_mod) {
                    return goal_mod == mod ||
                           (goal_mod < session.mod_count &&
                            session.family_id[goal_mod] ==
                                session.family_id[mod]);
                });
        });
}

bool goal_contains_exact_mod(
    const ActionRegistryBuildOptions& options,
    const std::uint32_t mod) {
    return std::any_of(
        options.fossil_goal_mod_ids.begin(),
        options.fossil_goal_mod_ids.end(),
        [&](const std::vector<std::uint32_t>& slot) {
            return std::find(slot.begin(), slot.end(), mod) != slot.end();
        });
}

bool goal_has_tag(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options,
    const std::uint32_t tag) {
    return std::any_of(
        options.fossil_goal_mod_ids.begin(),
        options.fossil_goal_mod_ids.end(),
        [&](const std::vector<std::uint32_t>& slot) {
            return std::any_of(
                slot.begin(), slot.end(), [&](const std::uint32_t goal_mod) {
                    return mod_has_tag(session, goal_mod, tag);
                });
        });
}

bool goal_has_influence(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options,
    const int influence) {
    return std::any_of(
        options.fossil_goal_mod_ids.begin(),
        options.fossil_goal_mod_ids.end(),
        [&](const std::vector<std::uint32_t>& slot) {
            return std::any_of(
                slot.begin(), slot.end(), [&](const std::uint32_t goal_mod) {
                    return goal_mod < session.influence_code.size() &&
                           session.influence_code[goal_mod] == influence;
                });
        });
}

bool action_is_goal_relevant(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options,
    const ActionDescriptor& action) {
    if (std::find(
            options.option_dependency_action_ids.begin(),
            options.option_dependency_action_ids.end(), action.id) !=
        options.option_dependency_action_ids.end()) {
        return true;
    }
    if (action.synthetic) return true;
    switch (action.params.type) {
    case ActionType::Essence:
        return action.params.essence_index <
                   session.essence_guaranteed_mod_ids.size() &&
               goal_contains_exact_mod(
                   options,
                   session.essence_guaranteed_mod_ids[
                       action.params.essence_index]);
    case ActionType::Fossil:
        return true;
    case ActionType::Bench:
        /* Direct goal crafts can terminate a route. Structural metamods enter
         * product planning only as dependencies of bounded options, not as
         * standalone flag setters. */
        if (action.params.mod_id < session.metamod_type.size()) {
            if (options.automatic_candidates) return true;
            const int metamod =
                session.metamod_type[action.params.mod_id];
            const DataImpl& data = *session.data;
            if ((options.needs_prefix_lock &&
                 metamod == data.metamod_prefixes_locked_code) ||
                (options.needs_suffix_lock &&
                 metamod == data.metamod_suffixes_locked_code) ||
                (options.needs_multimod &&
                 metamod == data.metamod_multimod_code)) {
                return true;
            }
            return goal_contains_mod_family(
                session, options, action.params.mod_id);
        }
        return false;
    case ActionType::VeiledChaos:
    case ActionType::VeiledExalt:
    case ActionType::Unveil:
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
    case ActionType::EldritchExalt:
    case ActionType::EldritchChaos:
    case ActionType::EldritchAnnul:
        /* These mechanics cannot directly satisfy the explicit natural-mod
         * goal slots accepted by the current product solver. Keeping their
         * setup flags in every solve materially widens the abstract state. */
        return false;
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
        return goal_has_tag(session, options, action.params.target_tag_id);
    case ActionType::InfluenceExalt:
        return goal_has_influence(
            session, options, action.params.influence_code);
    case ActionType::Fracture:
        return options.needs_fracture || options.automatic_candidates;
    case ActionType::RemoveCraftedModifiers:
        /* Crafted cleanup remains outside the product envelope; Fracture is
         * now an ordinary product candidate, but cleanup remains structural
         * support for exact authored/automatic option routes. Exposing this
         * unfinished primitive to the product MDP creates flag/tag state
         * combinations that exhaust the normal state cap before iteration. */
        return options.automatic_candidates;
    default:
        /* General currency and restart are structural ways to reach any
         * explicit goal. */
        return true;
    }
}

void retain_goal_relevant_actions(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options,
    ActionRegistry& registry) {
    if (!options.goal_relevant_actions) return;
    const std::size_t before = registry.actions.size();
    registry.actions.erase(
        std::remove_if(
            registry.actions.begin(), registry.actions.end(),
            [&](const ActionDescriptor& action) {
                return !action_is_goal_relevant(session, options, action);
            }),
        registry.actions.end());
    registry.goal_relevant_actions_pruned =
        static_cast<std::uint32_t>(before - registry.actions.size());
    registry.index_by_id.clear();
    for (std::uint32_t index = 0; index < registry.actions.size(); ++index) {
        ActionDescriptor& action = registry.actions[index];
        if (options.automatic_candidates) {
            if (action.params.type == ActionType::Bench) {
                action.automatic_dependency_only =
                    !goal_contains_mod_family(
                        session, options, action.params.mod_id);
            } else if (action.params.type ==
                       ActionType::RemoveCraftedModifiers) {
                action.automatic_dependency_only = true;
            }
        }
        registry.index_by_id.emplace(registry.actions[index].id, index);
    }
}

struct GoalRelevantFossilCandidate {
    std::vector<std::uint32_t> combo;
    std::vector<long double> slot_shares;
    long double quality = 0.0L;
    bool useful = false;
};

GoalRelevantFossilCandidate score_fossil_combo(
    const SessionImpl& session,
    const std::vector<std::vector<std::uint32_t>>& goal_mods,
    std::vector<std::uint32_t> combo,
    const std::vector<long double>& baseline) {
    const DataImpl& data = *session.data;
    GoalRelevantFossilCandidate candidate;
    candidate.combo = std::move(combo);
    candidate.slot_shares.assign(goal_mods.size(), 0.0L);
    std::vector<long double> goal_weights(goal_mods.size(), 0.0L);
    long double total_weight = 0.0L;
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        if (!pc_bitset_test(session.normal_random_roll_mask.data(), mod) ||
            session.base_roll_weight[mod] == 0) {
            continue;
        }
        long double multiplier = 1.0L;
        for (const std::uint32_t fossil : candidate.combo) {
            for (std::uint32_t row = data.fossil_weight_offsets[fossil];
                 row < data.fossil_weight_offsets[fossil + 1]; ++row) {
                const int kind = data.fossil_weight_kind_codes[row];
                if ((kind != data.fossil_weight_negative_code &&
                     kind != data.fossil_weight_positive_code) ||
                    !mod_has_tag(
                        session, mod, data.fossil_weight_tag_ids[row])) {
                    continue;
                }
                const std::int32_t value = data.fossil_weight_values[row];
                if (value <= 0) {
                    multiplier = 0.0L;
                    break;
                }
                multiplier *= static_cast<long double>(value) / 100.0L;
            }
            if (multiplier == 0.0L) break;
        }
        const long double weight =
            static_cast<long double>(session.base_roll_weight[mod]) *
            multiplier;
        total_weight += weight;
        for (std::size_t slot = 0; slot < goal_mods.size(); ++slot) {
            if (std::find(goal_mods[slot].begin(), goal_mods[slot].end(), mod) !=
                goal_mods[slot].end()) {
                goal_weights[slot] += weight;
            }
        }
    }
    if (total_weight <= 0.0L) {
        candidate.quality = -std::numeric_limits<long double>::infinity();
        return candidate;
    }
    bool direct_goal = false;
    for (std::size_t slot = 0; slot < goal_mods.size(); ++slot) {
        candidate.slot_shares[slot] = goal_weights[slot] / total_weight;
        for (const std::uint32_t fossil : candidate.combo) {
            const auto direct_contains = [&](const auto& lists) {
                if (fossil >= lists.size()) return false;
                return std::any_of(
                    lists[fossil].begin(), lists[fossil].end(),
                    [&](const std::uint32_t mod) {
                        return std::find(
                                   goal_mods[slot].begin(),
                                   goal_mods[slot].end(), mod) !=
                               goal_mods[slot].end();
                    });
            };
            direct_goal = direct_goal ||
                          direct_contains(session.fossil_added_mod_ids) ||
                          direct_contains(session.fossil_forced_mod_ids);
        }
        const long double before =
            slot < baseline.size() ? baseline[slot] : 0.0L;
        if (candidate.slot_shares[slot] > before + 1e-18L) {
            candidate.useful = true;
        }
        const long double floor = 1e-24L;
        candidate.quality +=
            std::log(std::max(candidate.slot_shares[slot], floor)) -
            std::log(std::max(before, floor));
    }
    if (direct_goal) {
        candidate.useful = true;
        candidate.quality += 32.0L;
    }
    return candidate;
}

std::vector<std::vector<std::uint32_t>> goal_relevant_fossil_combos(
    const SessionImpl& session,
    const std::vector<std::uint32_t>& fossils,
    const ActionRegistryBuildOptions& options) {
    if (options.fossil_goal_mod_ids.empty()) return {};
    const GoalRelevantFossilCandidate baseline_candidate =
        score_fossil_combo(
            session, options.fossil_goal_mod_ids, {},
            std::vector<long double>(
                options.fossil_goal_mod_ids.size(), 0.0L));
    const std::vector<long double> baseline = baseline_candidate.slot_shares;
    constexpr std::size_t kBeamWidth = 96;
    constexpr std::size_t kPerSlotLeaders = 8;
    constexpr std::size_t kEmittedAggregateLeaders = 1;
    constexpr std::size_t kEmittedPerSlotLeaders = 1;
    std::vector<GoalRelevantFossilCandidate> beam(1);
    std::vector<std::vector<std::uint32_t>> result;
    std::vector<std::uint32_t> best_single_by_slot(
        options.fossil_goal_mod_ids.size(), kNoId);
    for (std::size_t size = 1; size <= PC_MAX_FOSSILS_PER_ACTION; ++size) {
        std::vector<GoalRelevantFossilCandidate> expanded;
        expanded.reserve(beam.size() * fossils.size());
        for (const GoalRelevantFossilCandidate& parent : beam) {
            std::size_t begin = 0;
            if (!parent.combo.empty()) {
                begin = static_cast<std::size_t>(
                    std::upper_bound(
                        fossils.begin(), fossils.end(), parent.combo.back()) -
                    fossils.begin());
            }
            for (std::size_t position = begin; position < fossils.size();
                 ++position) {
                std::vector<std::uint32_t> combo = parent.combo;
                combo.push_back(fossils[position]);
                expanded.push_back(score_fossil_combo(
                    session, options.fossil_goal_mod_ids,
                    std::move(combo), baseline));
            }
        }
        expanded.erase(
            std::remove_if(
                expanded.begin(), expanded.end(),
                [](const GoalRelevantFossilCandidate& candidate) {
                    return !std::isfinite(candidate.quality);
                }),
            expanded.end());
        const auto better = [](const GoalRelevantFossilCandidate& left,
                               const GoalRelevantFossilCandidate& right) {
            if (left.quality != right.quality) {
                return left.quality > right.quality;
            }
            return left.combo < right.combo;
        };
        std::sort(expanded.begin(), expanded.end(), better);
        std::vector<GoalRelevantFossilCandidate> next;
        const auto retain = [&](const GoalRelevantFossilCandidate& value) {
            if (next.size() >= kBeamWidth) return;
            if (std::none_of(
                    next.begin(), next.end(),
                    [&](const GoalRelevantFossilCandidate& existing) {
                        return existing.combo == value.combo;
                    })) {
                next.push_back(value);
            }
        };
        /* Preserve leaders for every goal slot before filling by aggregate
         * quality. This keeps focused Dense+element combinations available
         * without enumerating the complete fossil powerset. */
        for (std::size_t slot = 0;
             slot < options.fossil_goal_mod_ids.size(); ++slot) {
            std::vector<const GoalRelevantFossilCandidate*> leaders;
            leaders.reserve(expanded.size());
            for (const auto& candidate : expanded) {
                leaders.push_back(&candidate);
            }
            std::sort(
                leaders.begin(), leaders.end(),
                [slot](const auto* left, const auto* right) {
                    if (left->slot_shares[slot] !=
                        right->slot_shares[slot]) {
                        return left->slot_shares[slot] >
                               right->slot_shares[slot];
                    }
                    return left->combo < right->combo;
                });
            for (std::size_t i = 0;
                 i < std::min(kPerSlotLeaders, leaders.size()); ++i) {
                retain(*leaders[i]);
            }
        }
        for (const auto& candidate : expanded) retain(candidate);
        /* The wider beam is search state, not product action output. Preserve
         * the strongest aggregate signatures plus focused leaders for every
         * goal slot at each socket count; emitting the whole beam recreates a
         * smaller powerset and widens the abstract tag layout enough to exhaust
         * the normal product state cap before the first state is expanded. */
        const auto emit_result = [&](const GoalRelevantFossilCandidate& value) {
            if (!value.useful) return;
            if (std::find(result.begin(), result.end(), value.combo) ==
                result.end()) {
                result.push_back(value.combo);
            }
        };
        for (std::size_t slot = 0;
             slot < options.fossil_goal_mod_ids.size(); ++slot) {
            std::vector<const GoalRelevantFossilCandidate*> leaders;
            leaders.reserve(next.size());
            for (const auto& candidate : next) leaders.push_back(&candidate);
            std::sort(
                leaders.begin(), leaders.end(),
                [slot](const auto* left, const auto* right) {
                    if (left->slot_shares[slot] !=
                        right->slot_shares[slot]) {
                        return left->slot_shares[slot] >
                               right->slot_shares[slot];
                    }
                    return left->combo < right->combo;
                });
            for (std::size_t i = 0;
                 i < std::min(kEmittedPerSlotLeaders, leaders.size()); ++i) {
                emit_result(*leaders[i]);
            }
            if (size == 1 && !leaders.empty() &&
                !leaders.front()->combo.empty()) {
                best_single_by_slot[slot] = leaders.front()->combo.front();
            }
        }
        for (std::size_t i = 0;
             i < std::min(kEmittedAggregateLeaders, next.size()); ++i) {
            emit_result(next[i]);
        }
        beam = std::move(next);
        if (beam.empty()) break;
    }
    /* Preserve the direct cross-slot loadout assembled from each slot's best
     * singleton. This keeps combinations such as Frigid + Dense even when a
     * different two-socket signature wins the aggregate beam score. */
    std::vector<std::uint32_t> cross_slot;
    for (const std::uint32_t fossil : best_single_by_slot) {
        if (fossil != kNoId) cross_slot.push_back(fossil);
    }
    std::sort(cross_slot.begin(), cross_slot.end());
    cross_slot.erase(
        std::unique(cross_slot.begin(), cross_slot.end()), cross_slot.end());
    if (!cross_slot.empty() &&
        cross_slot.size() <= PC_MAX_FOSSILS_PER_ACTION) {
        GoalRelevantFossilCandidate cross_candidate = score_fossil_combo(
            session, options.fossil_goal_mod_ids, std::move(cross_slot),
            baseline);
        if (cross_candidate.useful &&
            std::find(
                result.begin(), result.end(), cross_candidate.combo) ==
                result.end()) {
            result.push_back(std::move(cross_candidate.combo));
        }
    }
    return result;
}

void add(ActionRegistry& registry, ActionDescriptor descriptor) {
    std::sort(descriptor.discriminating_tag_ids.begin(),
              descriptor.discriminating_tag_ids.end());
    registry.index_by_id.emplace(
        descriptor.id, static_cast<std::uint32_t>(registry.actions.size()));
    registry.actions.push_back(std::move(descriptor));
}

ActionDescriptor base_descriptor(
    const char* id,
    ActionType type,
    TransitionKind kind,
    std::uint8_t rarity_mask) {
    ActionDescriptor descriptor;
    descriptor.id = id;
    descriptor.display_name = id;
    descriptor.params.type = type;
    descriptor.kind = kind;
    descriptor.cost_keys = {id};
    descriptor.legality.rarity_mask = rarity_mask;
    return descriptor;
}

using Feature = RefinementFeature;
using FeatureMask = RefinementFeatureMask;

constexpr FeatureMask feature(const Feature value) {
    return refinement_feature(value);
}

constexpr FeatureMask kAffixCountFeatures =
    feature(Feature::PrefixCount) |
    feature(Feature::SuffixCount);

constexpr FeatureMask kCraftedSummaryFeatures =
    feature(Feature::HasCraftedModifier) |
    feature(Feature::Multimod) |
    feature(Feature::PrefixLock) |
    feature(Feature::SuffixLock) |
    feature(Feature::CannotRollAttack) |
    feature(Feature::CannotRollCaster);

void may_rewrite_item_features(
    ActionRefinementContract& contract,
    const FeatureMask features) {
    contract.destroyed_item_features |= features;
    const auto add_dependency =
        [&](const FeatureMask item_features,
            const FeatureMask affix_features) {
            const FeatureMask relevant =
                features & item_features;
            if (relevant != 0) {
                contract.item_affix_dependencies.push_back(
                    {relevant, affix_features});
            }
        };
    add_dependency(
        feature(Feature::Rarity) |
            feature(Feature::PrefixCount) |
            feature(Feature::SuffixCount),
        feature(Feature::ModifierSide));
    add_dependency(
        feature(Feature::HasCraftedModifier),
        feature(Feature::ModifierCrafted));
    add_dependency(
        feature(Feature::HasFracturedModifier),
        feature(Feature::ModifierFractured));
    add_dependency(
        feature(Feature::HasVeiledModifier),
        feature(Feature::ModifierVeiled));
    add_dependency(
        feature(Feature::Multimod) |
            feature(Feature::PrefixLock) |
            feature(Feature::SuffixLock) |
            feature(Feature::CannotRollAttack) |
            feature(Feature::CannotRollCaster),
        feature(Feature::ModifierMetamodRole));
}

void replace_item_features(
    ActionRefinementContract& contract,
    const FeatureMask features) {
    contract.destroyed_item_features |= features;
    contract.preserved_item_features &= ~features;
    for (RefinementItemAffixDependency& dependency :
         contract.item_affix_dependencies) {
        dependency.item_features &= ~features;
    }
}

RefinementAffixSelector affixes(
    const std::uint16_t required = 0,
    const std::uint16_t forbidden = 0,
    const std::uint8_t required_item = 0,
    const std::uint8_t forbidden_item = 0,
    std::vector<std::uint32_t> required_tags = {}) {
    RefinementAffixSelector selector;
    selector.required_affix_traits = required;
    selector.forbidden_affix_traits = forbidden;
    selector.required_item_traits = required_item;
    selector.forbidden_item_traits = forbidden_item;
    selector.required_tag_ids = std::move(required_tags);
    return selector;
}

bool selector_less(
    const RefinementAffixSelector& lhs,
    const RefinementAffixSelector& rhs) {
    return std::tie(
               lhs.required_affix_traits, lhs.forbidden_affix_traits,
               lhs.required_item_traits, lhs.forbidden_item_traits,
               lhs.required_tag_ids) <
           std::tie(
               rhs.required_affix_traits, rhs.forbidden_affix_traits,
               rhs.required_item_traits, rhs.forbidden_item_traits,
               rhs.required_tag_ids);
}

void canonicalize_selector(RefinementAffixSelector& selector) {
    std::sort(
        selector.required_tag_ids.begin(),
        selector.required_tag_ids.end());
    selector.required_tag_ids.erase(
        std::unique(
            selector.required_tag_ids.begin(),
            selector.required_tag_ids.end()),
        selector.required_tag_ids.end());
}

void canonicalize_selectors(
    std::vector<RefinementAffixSelector>& selectors) {
    for (RefinementAffixSelector& selector : selectors) {
        canonicalize_selector(selector);
    }
    std::sort(selectors.begin(), selectors.end(), selector_less);
    selectors.erase(
        std::unique(selectors.begin(), selectors.end()), selectors.end());
}

void observe_affixes(
    ActionRefinementContract& contract,
    const FeatureMask features,
    RefinementAffixSelector selector = {}) {
    if (features == 0) return;
    canonicalize_selector(selector);
    contract.affix_observations.push_back(
        RefinementAffixObservation{features, std::move(selector)});
}

void add_selector_observations(
    ActionRefinementContract& contract,
    const RefinementAffixSelector& selector) {
    const std::uint16_t traits =
        selector.required_affix_traits |
        selector.forbidden_affix_traits;
    FeatureMask affix_features = 0;
    if ((traits &
         (kRefinementAffixPrefix | kRefinementAffixSuffix |
          kRefinementAffixOnLockedSide |
          kRefinementAffixOnEldritchDominantSide |
          kRefinementAffixOnEldritchNonDominantSide)) != 0) {
        affix_features |= feature(Feature::ModifierSide);
    }
    if ((traits & kRefinementAffixCrafted) != 0) {
        affix_features |= feature(Feature::ModifierCrafted);
    }
    if ((traits & kRefinementAffixFractured) != 0) {
        affix_features |= feature(Feature::ModifierFractured);
    }
    if ((traits & kRefinementAffixVeiled) != 0) {
        affix_features |= feature(Feature::ModifierVeiled);
    }
    if (!selector.required_tag_ids.empty()) {
        affix_features |= feature(Feature::ModifierClassificationTags);
        contract.observed_modifier_tag_ids.insert(
            contract.observed_modifier_tag_ids.end(),
            selector.required_tag_ids.begin(),
            selector.required_tag_ids.end());
    }
    if (affix_features != 0) {
        /* Selector membership itself is an observation over every occupied
         * explicit, not only over the members that happen to match. */
        observe_affixes(contract, affix_features);
    }
    if ((traits & kRefinementAffixOnLockedSide) != 0) {
        contract.observed_item_features |=
            feature(Feature::PrefixLock) |
            feature(Feature::SuffixLock);
    }
    if ((traits &
         (kRefinementAffixOnEldritchDominantSide |
          kRefinementAffixOnEldritchNonDominantSide)) != 0 ||
        ((selector.required_item_traits |
          selector.forbidden_item_traits) &
         kRefinementItemHasEldritchDominance) != 0) {
        contract.observed_item_features |=
            feature(Feature::EldritchDominance);
    }
    if (((selector.required_item_traits |
          selector.forbidden_item_traits) &
         kRefinementItemExactlyOneSideLocked) != 0) {
        contract.observed_item_features |=
            feature(Feature::PrefixLock) |
            feature(Feature::SuffixLock);
    }
}

bool observation_less(
    const RefinementAffixObservation& lhs,
    const RefinementAffixObservation& rhs) {
    if (selector_less(lhs.selector, rhs.selector)) return true;
    if (selector_less(rhs.selector, lhs.selector)) return false;
    return lhs.features < rhs.features;
}

bool flow_less(
    const RefinementAffixFlow& lhs,
    const RefinementAffixFlow& rhs) {
    if (selector_less(lhs.source_selector, rhs.source_selector)) return true;
    if (selector_less(rhs.source_selector, lhs.source_selector)) return false;
    return std::tie(
               lhs.set_affix_traits,
               lhs.cleared_affix_traits,
               lhs.preserved_features,
               lhs.preserves_modifier_classification) <
           std::tie(
               rhs.set_affix_traits,
               rhs.cleared_affix_traits,
               rhs.preserved_features,
               rhs.preserves_modifier_classification);
}

bool dependency_less(
    const RefinementItemAffixDependency& lhs,
    const RefinementItemAffixDependency& rhs) {
    return std::tie(
               lhs.item_features,
               lhs.survivor_affix_features) <
           std::tie(
               rhs.item_features,
               rhs.survivor_affix_features);
}

RefinementAffixFlow identity_affix_flow(
    RefinementAffixSelector selector) {
    return {
        std::move(selector),
        0,
        0,
        kAllRefinementAffixFeatures,
        true};
}

void canonicalize_contract(ActionRefinementContract& contract) {
    contract.item_affix_dependencies.erase(
        std::remove_if(
            contract.item_affix_dependencies.begin(),
            contract.item_affix_dependencies.end(),
            [](const RefinementItemAffixDependency& dependency) {
                return dependency.item_features == 0 ||
                       dependency.survivor_affix_features == 0;
            }),
        contract.item_affix_dependencies.end());
    std::sort(
        contract.item_affix_dependencies.begin(),
        contract.item_affix_dependencies.end(),
        dependency_less);
    contract.item_affix_dependencies.erase(
        std::unique(
            contract.item_affix_dependencies.begin(),
            contract.item_affix_dependencies.end()),
        contract.item_affix_dependencies.end());
    canonicalize_selectors(contract.preserved_affixes);
    canonicalize_selectors(contract.destroyed_affixes);
    if (contract.affix_flows.empty()) {
        contract.affix_flows.reserve(
            contract.preserved_affixes.size());
        for (const RefinementAffixSelector& selector :
             contract.preserved_affixes) {
            contract.affix_flows.push_back(
                identity_affix_flow(selector));
        }
    }
    for (RefinementAffixFlow& flow : contract.affix_flows) {
        canonicalize_selector(flow.source_selector);
    }
    std::sort(
        contract.affix_flows.begin(),
        contract.affix_flows.end(), flow_less);
    contract.affix_flows.erase(
        std::unique(
            contract.affix_flows.begin(),
            contract.affix_flows.end()),
        contract.affix_flows.end());
    for (const RefinementAffixSelector& selector :
         contract.preserved_affixes) {
        add_selector_observations(contract, selector);
    }
    for (const RefinementAffixSelector& selector :
         contract.destroyed_affixes) {
        add_selector_observations(contract, selector);
    }
    for (const RefinementAffixFlow& flow : contract.affix_flows) {
        add_selector_observations(
            contract, flow.source_selector);
    }
    for (RefinementAffixObservation& observation :
         contract.affix_observations) {
        canonicalize_selector(observation.selector);
    }
    std::sort(
        contract.affix_observations.begin(),
        contract.affix_observations.end(), observation_less);
    std::vector<RefinementAffixObservation> merged;
    for (RefinementAffixObservation observation :
         contract.affix_observations) {
        if (!merged.empty() &&
            merged.back().selector == observation.selector) {
            merged.back().features |= observation.features;
        } else {
            merged.push_back(std::move(observation));
        }
    }
    contract.affix_observations = std::move(merged);
    std::sort(
        contract.observed_modifier_tag_ids.begin(),
        contract.observed_modifier_tag_ids.end());
    contract.observed_modifier_tag_ids.erase(
        std::unique(
            contract.observed_modifier_tag_ids.begin(),
            contract.observed_modifier_tag_ids.end()),
        contract.observed_modifier_tag_ids.end());
}

void observe_legality(
    ActionRefinementContract& contract,
    const LegalityPredicate& legality) {
    if (legality.rarity_mask != kRarityAny) {
        contract.observed_item_features |= feature(Feature::Rarity);
    }
    if (legality.requires_open_affix ||
        legality.requires_removable_affix ||
        legality.min_total_affixes != 0) {
        contract.observed_item_features |=
            feature(Feature::PrefixCount) |
            feature(Feature::SuffixCount);
    }
    const std::uint32_t flags =
        legality.required_flags | legality.forbidden_flags;
    if ((flags & kFlagCorrupted) != 0) {
        contract.observed_item_features |= feature(Feature::Corrupted);
    }
    if ((flags & kFlagMirrored) != 0) {
        contract.observed_item_features |= feature(Feature::Mirrored);
    }
    if ((flags & kFlagSplit) != 0) {
        contract.observed_item_features |= feature(Feature::Split);
    }
    if ((flags & kFlagSynthesised) != 0) {
        contract.observed_item_features |= feature(Feature::Synthesised);
    }
    if ((flags & kFlagFractured) != 0) {
        contract.observed_item_features |=
            feature(Feature::HasFracturedModifier);
    }
    if ((flags & kFlagCraftedMod) != 0) {
        contract.observed_item_features |=
            feature(Feature::HasCraftedModifier);
    }
    if ((flags & kFlagVeiledMod) != 0) {
        contract.observed_item_features |=
            feature(Feature::HasVeiledModifier);
    }
    if ((flags & kFlagMultimod) != 0) {
        contract.observed_item_features |= feature(Feature::Multimod);
    }
    if ((flags & kFlagNoAttack) != 0) {
        contract.observed_item_features |=
            feature(Feature::CannotRollAttack);
    }
    if ((flags & kFlagNoCaster) != 0) {
        contract.observed_item_features |=
            feature(Feature::CannotRollCaster);
    }
    if ((flags & kFlagPrefixesLocked) != 0) {
        contract.observed_item_features |=
            feature(Feature::PrefixLock);
    }
    if ((flags & kFlagSuffixesLocked) != 0) {
        contract.observed_item_features |=
            feature(Feature::SuffixLock);
    }
    if ((flags & kFlagInfluenced) != 0) {
        contract.observed_item_features |= feature(Feature::Influence);
    }
    if ((flags & kFlagEldritchImplicit) != 0) {
        contract.observed_item_features |=
            feature(Feature::EldritchPresence);
    }
}

void observe_pool_add(
    ActionRefinementContract& contract,
    const RefinementAffixSelector& selector = {}) {
    contract.observed_item_features |=
        feature(Feature::Rarity) |
        feature(Feature::PrefixCount) |
        feature(Feature::SuffixCount) |
        feature(Feature::CannotRollAttack) |
        feature(Feature::CannotRollCaster) |
        feature(Feature::Influence);
    /* This selector is intentionally over every occupied explicit, including
     * goal-slot members. A coarse goal status is not an exclusion signature. */
    observe_affixes(
        contract, feature(Feature::ModifierExclusionSignature), selector);
}

void set_standard_renewal(
    ActionRefinementContract& contract,
    const ActionTransitionFacts facts) {
    may_rewrite_item_features(
        contract,
        kAffixCountFeatures |
            kCraftedSummaryFeatures |
            feature(Feature::HasVeiledModifier));
    contract.observed_item_features |=
        feature(Feature::PrefixCount) |
        feature(Feature::SuffixCount) |
        feature(Feature::Influence);
    if (facts.respects_metamod_pool_blocks) {
        contract.observed_item_features |=
            feature(Feature::CannotRollAttack) |
            feature(Feature::CannotRollCaster);
    }
    std::uint16_t destroyed_forbidden = 0;
    if (facts.preserves_fractured_affixes) {
        const RefinementAffixSelector fractured =
            affixes(kRefinementAffixFractured);
        contract.preserved_affixes.push_back(fractured);
        FeatureMask preserved_features =
            feature(Feature::ModifierExclusionSignature) |
            feature(Feature::ModifierSide);
        if (facts.respects_metamod_pool_blocks) {
            preserved_features |=
                feature(Feature::ModifierMetamodRole);
        }
        observe_affixes(
            contract, preserved_features, fractured);
        destroyed_forbidden |= kRefinementAffixFractured;
    }
    if (facts.respects_metamod_side_locks) {
        const RefinementAffixSelector locked =
            affixes(kRefinementAffixOnLockedSide);
        contract.preserved_affixes.push_back(locked);
        FeatureMask preserved_features =
            feature(Feature::ModifierExclusionSignature);
        if (facts.respects_metamod_pool_blocks) {
            preserved_features |=
                feature(Feature::ModifierMetamodRole);
        }
        observe_affixes(
            contract, preserved_features, locked);
        destroyed_forbidden |= kRefinementAffixOnLockedSide;
    }
    contract.destroyed_affixes.push_back(
        affixes(0, destroyed_forbidden));
}

ActionRefinementContract derive_refinement_contract(
    const SessionImpl& session,
    const ActionDescriptor& action) {
    ActionRefinementContract contract;
    contract.preserved_item_features =
        kAllRefinementItemFeatures;
    observe_legality(contract, action.legality);

    if (action.synthetic) {
        contract.schema_version = kActionRefinementContractVersion;
        contract.preserved_item_features = 0;
        contract.destroyed_item_features =
            kAllRefinementItemFeatures;
        contract.resets_to_fresh_item = true;
        contract.destroyed_affixes.push_back(affixes());
        canonicalize_contract(contract);
        return contract;
    }

    const ActionTransitionFacts transition =
        action_transition_facts(action.params.type);
    switch (action.params.type) {
    case ActionType::Transmute:
    case ActionType::Alteration:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Essence:
    case ActionType::Fossil:
    case ActionType::VeiledChaos:
    case ActionType::HarvestReforge:
        set_standard_renewal(contract, transition);
        if (action.params.type == ActionType::Transmute ||
            action.params.type == ActionType::Alchemy ||
            action.params.type == ActionType::Essence ||
            action.params.type == ActionType::Fossil) {
            replace_item_features(
                contract, feature(Feature::Rarity));
        }
        if (action.params.type == ActionType::VeiledChaos) {
            replace_item_features(
                contract,
                feature(Feature::HasVeiledModifier));
        }
        if (action.params.type == ActionType::Fossil) {
            bool can_corrupt = false;
            bool can_mirror = false;
            for (const std::uint32_t fossil :
                 action.params.fossil_indices) {
                if (fossil >= session.data->fossil_count) continue;
                can_mirror =
                    can_mirror ||
                    (fossil <
                         session.data->fossil_mirrors.size() &&
                     session.data->fossil_mirrors[fossil] != 0);
                can_corrupt =
                    can_corrupt ||
                    session.data->string_at(
                        session.data->fossil_name_sids[fossil]) ==
                        "Bloodstained Fossil";
            }
            if (can_corrupt) {
                may_rewrite_item_features(
                    contract, feature(Feature::Corrupted));
            }
            if (can_mirror) {
                may_rewrite_item_features(
                    contract, feature(Feature::Mirrored));
            }
        }
        break;

    case ActionType::EldritchChaos: {
        may_rewrite_item_features(
            contract,
            kAffixCountFeatures |
                kCraftedSummaryFeatures |
                feature(Feature::HasVeiledModifier));
        contract.observed_item_features |=
            feature(Feature::PrefixCount) |
            feature(Feature::SuffixCount) |
            feature(Feature::Influence) |
            feature(Feature::CannotRollAttack) |
            feature(Feature::CannotRollCaster) |
            feature(Feature::EldritchDominance);
        const RefinementAffixSelector fractured =
            affixes(kRefinementAffixFractured);
        const RefinementAffixSelector nondominant = affixes(
            kRefinementAffixOnEldritchNonDominantSide, 0,
            kRefinementItemHasEldritchDominance);
        const RefinementAffixSelector locked_without_dominance = affixes(
            kRefinementAffixOnLockedSide, 0, 0,
            kRefinementItemHasEldritchDominance);
        contract.preserved_affixes = {
            fractured, nondominant, locked_without_dominance};
        for (const RefinementAffixSelector& selector :
             contract.preserved_affixes) {
            observe_affixes(
                contract,
                feature(Feature::ModifierExclusionSignature) |
                    feature(Feature::ModifierSide) |
                    feature(Feature::ModifierMetamodRole),
                selector);
        }
        contract.destroyed_affixes = {
            affixes(
                kRefinementAffixOnEldritchDominantSide,
                kRefinementAffixFractured,
                kRefinementItemHasEldritchDominance),
            affixes(
                0,
                kRefinementAffixFractured |
                    kRefinementAffixOnLockedSide,
                0, kRefinementItemHasEldritchDominance)};
        break;
    }

    case ActionType::Augment:
    case ActionType::Regal:
    case ActionType::Exalt:
    case ActionType::InfluenceExalt:
        contract.preserved_affixes.push_back(affixes());
        observe_pool_add(contract);
        may_rewrite_item_features(contract, kAffixCountFeatures);
        if (action.params.type == ActionType::Regal) {
            replace_item_features(
                contract, feature(Feature::Rarity));
        } else if (
            action.params.type == ActionType::InfluenceExalt) {
            replace_item_features(
                contract, feature(Feature::Influence));
        }
        break;

    case ActionType::VeiledExalt:
        contract.preserved_affixes.push_back(affixes());
        may_rewrite_item_features(contract, kAffixCountFeatures);
        replace_item_features(
            contract, feature(Feature::HasVeiledModifier));
        contract.observed_item_features |=
            feature(Feature::Rarity) |
            feature(Feature::PrefixCount) |
            feature(Feature::SuffixCount);
        /* The sampled engine creates the unveil offer immediately. */
        observe_affixes(
            contract, feature(Feature::ModifierExclusionSignature));
        break;

    case ActionType::EldritchExalt:
        contract.preserved_affixes.push_back(affixes());
        observe_pool_add(contract);
        may_rewrite_item_features(contract, kAffixCountFeatures);
        contract.observed_item_features |=
            feature(Feature::EldritchDominance);
        break;

    case ActionType::Annul:
        contract.preserved_affixes.push_back(affixes());
        may_rewrite_item_features(
            contract,
            kAffixCountFeatures |
                kCraftedSummaryFeatures |
                feature(Feature::HasVeiledModifier));
        contract.destroyed_affixes.push_back(affixes(
            0, kRefinementAffixFractured |
                   kRefinementAffixOnLockedSide));
        break;

    case ActionType::EldritchAnnul:
        contract.preserved_affixes.push_back(affixes());
        may_rewrite_item_features(
            contract,
            kAffixCountFeatures |
                kCraftedSummaryFeatures |
                feature(Feature::HasVeiledModifier));
        contract.destroyed_affixes = {
            affixes(
                kRefinementAffixOnEldritchDominantSide,
                kRefinementAffixFractured,
                kRefinementItemHasEldritchDominance),
            affixes(
                0,
                kRefinementAffixFractured |
                    kRefinementAffixOnLockedSide,
                0, kRefinementItemHasEldritchDominance)};
        break;

    case ActionType::Scour:
        may_rewrite_item_features(
            contract,
                feature(Feature::Rarity) |
                kAffixCountFeatures |
                kCraftedSummaryFeatures |
                feature(Feature::HasFracturedModifier) |
                feature(Feature::HasVeiledModifier));
        contract.preserved_affixes = {
            affixes(
                kRefinementAffixOnLockedSide, 0,
                kRefinementItemExactlyOneSideLocked),
            affixes(
                kRefinementAffixFractured, 0, 0,
                kRefinementItemExactlyOneSideLocked)};
        contract.destroyed_affixes = {
            affixes(
                0, kRefinementAffixOnLockedSide,
                kRefinementItemExactlyOneSideLocked),
            affixes(
                0, kRefinementAffixFractured, 0,
                kRefinementItemExactlyOneSideLocked)};
        observe_affixes(
            contract, feature(Feature::ModifierSide));
        break;

    case ActionType::RemoveCraftedModifiers:
        may_rewrite_item_features(
            contract,
            kAffixCountFeatures |
                kCraftedSummaryFeatures);
        contract.preserved_affixes = {
            affixes(0, kRefinementAffixCrafted),
            affixes(kRefinementAffixFractured)};
        contract.destroyed_affixes.push_back(
            affixes(
                kRefinementAffixCrafted,
                kRefinementAffixFractured));
        observe_affixes(
            contract,
            feature(Feature::ModifierSide),
            affixes(
                kRefinementAffixCrafted,
                kRefinementAffixFractured));
        break;

    case ActionType::Bench:
        contract.preserved_affixes.push_back(affixes());
        may_rewrite_item_features(
            contract,
            kAffixCountFeatures |
                kCraftedSummaryFeatures);
        contract.observed_item_features |=
            feature(Feature::Rarity) |
            feature(Feature::PrefixCount) |
            feature(Feature::SuffixCount) |
            feature(Feature::Multimod);
        observe_affixes(
            contract,
            feature(Feature::ModifierExclusionSignature) |
                feature(Feature::ModifierCrafted));
        break;

    case ActionType::Unveil:
        contract.outcome_observation =
            RefinementOutcomeObservation::ModifierOffer;
        replace_item_features(
            contract, feature(Feature::HasVeiledModifier));
        {
            const RefinementAffixSelector ordinary =
                affixes(0, kRefinementAffixVeiled);
            const RefinementAffixSelector veiled =
                affixes(kRefinementAffixVeiled);
            contract.preserved_affixes.push_back(ordinary);
            contract.destroyed_affixes.push_back(veiled);
            contract.affix_flows = {
                identity_affix_flow(ordinary),
                {
                    veiled,
                    0,
                    kRefinementAffixVeiled,
                    feature(Feature::ModifierSide) |
                        feature(Feature::ModifierCrafted) |
                        feature(Feature::ModifierFractured),
                    false,
                }};
        }
        observe_affixes(
            contract,
            feature(Feature::ModifierExclusionSignature) |
                feature(Feature::ModifierSide));
        break;

    case ActionType::HarvestAugment:
        contract.preserved_affixes.push_back(affixes());
        may_rewrite_item_features(
            contract,
            kAffixCountFeatures |
                kCraftedSummaryFeatures |
                feature(Feature::HasVeiledModifier));
        contract.destroyed_affixes.push_back(affixes(
            0, kRefinementAffixFractured |
                   kRefinementAffixOnLockedSide));
        observe_pool_add(contract);
        break;

    case ActionType::HarvestResist: {
        contract.preserved_affixes.push_back(affixes());
        may_rewrite_item_features(
            contract,
            kCraftedSummaryFeatures |
                feature(Feature::HasVeiledModifier));
        const std::uint32_t resistance =
            tag_id(session, "resistance");
        std::vector<std::uint32_t> source_tags = {
            action.params.source_tag_id};
        if (resistance != kNoId) source_tags.push_back(resistance);
        const RefinementAffixSelector source = affixes(
            0, kRefinementAffixFractured |
                   kRefinementAffixOnLockedSide,
            0, 0, std::move(source_tags));
        contract.destroyed_affixes.push_back(source);
        contract.affix_flows = {
            identity_affix_flow(affixes()),
            {
                source,
                0,
                kRefinementAffixCrafted |
                    kRefinementAffixFractured |
                    kRefinementAffixVeiled,
                feature(Feature::ModifierSide) |
                    feature(Feature::ModifierRequiredLevel),
                false,
            }};
        observe_affixes(
            contract,
            feature(Feature::ModifierRequiredLevel), source);
        observe_pool_add(contract);
        break;
    }

    case ActionType::EldritchEmber:
        contract.observed_item_features |=
            feature(Feature::EaterOfWorldsTier);
        replace_item_features(
            contract,
            feature(Feature::SearingExarchTier) |
                feature(Feature::EldritchPresence) |
                feature(Feature::EldritchDominance));
        contract.preserved_affixes.push_back(affixes());
        break;

    case ActionType::EldritchIchor:
        contract.observed_item_features |=
            feature(Feature::SearingExarchTier);
        replace_item_features(
            contract,
            feature(Feature::EaterOfWorldsTier) |
                feature(Feature::EldritchPresence) |
                feature(Feature::EldritchDominance));
        contract.preserved_affixes.push_back(affixes());
        break;

    case ActionType::Fracture:
        contract.preserved_affixes.push_back(affixes());
        contract.affix_flows = {
            identity_affix_flow(affixes()),
            {
                affixes(0, kRefinementAffixFractured),
                kRefinementAffixFractured,
                0,
                kAllRefinementAffixFeatures &
                    ~feature(Feature::ModifierFractured),
                true,
            }};
        replace_item_features(
            contract, feature(Feature::HasFracturedModifier));
        contract.observed_item_features |=
            feature(Feature::PrefixCount) |
            feature(Feature::SuffixCount);
        break;

    default:
        /* A newly admitted engine action must add its semantics above. The
         * registry validator rejects this incomplete contract before solve. */
        canonicalize_contract(contract);
        return contract;
    }
    contract.schema_version = kActionRefinementContractVersion;
    canonicalize_contract(contract);
    return contract;
}

ActionPreservationMetadata derive_preservation_metadata(
    const SessionImpl& session,
    const ActionDescriptor& action) {
    ActionPreservationMetadata metadata;
    if (action.synthetic) {
        metadata.can_destroy = kAllCarrierProperties;
        metadata.can_make_unreachable = kAllCarrierProperties;
        return metadata;
    }

    const ActionTransitionFacts facts =
        action_transition_facts(action.params.type);
    metadata.destructive_renewal = facts.renewal;
    metadata.preserves_fractured_affixes =
        facts.preserves_fractured_affixes;
    metadata.respects_prefix_lock = facts.respects_metamod_side_locks;
    metadata.respects_suffix_lock = facts.respects_metamod_side_locks;
    metadata.respects_cannot_roll_attack =
        facts.respects_metamod_pool_blocks;
    metadata.respects_cannot_roll_caster =
        facts.respects_metamod_pool_blocks;

    if (facts.renewal) {
        metadata.can_preserve = kCarrierFracturedState;
        if (facts.respects_metamod_side_locks) {
            metadata.can_preserve |=
                kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
                kCarrierJunkBlockers | kCarrierCraftedState |
                kCarrierPrefixSide | kCarrierSuffixSide |
                kCarrierActiveProtection;
        } else {
            /* A fractured modifier may itself occupy either affix side or
             * carry goal/crafted/protection identity. Its preservation is
             * independent of metamods and is refined by the exact witness. */
            metadata.can_preserve |=
                kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
                kCarrierJunkBlockers | kCarrierCraftedState |
                kCarrierPrefixSide | kCarrierSuffixSide |
                kCarrierActiveProtection;
        }
        metadata.can_destroy =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers | kCarrierCraftedState |
            kCarrierPrefixSide | kCarrierSuffixSide |
            kCarrierActiveProtection;
        metadata.can_create =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers | kCarrierPrefixSide | kCarrierSuffixSide;
        if (action.params.type == ActionType::VeiledChaos) {
            metadata.can_create |= kCarrierCraftedState;
        }
        metadata.can_make_unreachable = metadata.can_destroy;
        return metadata;
    }

    metadata.can_preserve = kAllCarrierProperties;
    switch (action.params.type) {
    case ActionType::Augment:
    case ActionType::Regal:
    case ActionType::Exalt:
    case ActionType::VeiledExalt:
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
    case ActionType::EldritchExalt:
    case ActionType::InfluenceExalt:
        metadata.can_create =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers | kCarrierPrefixSide | kCarrierSuffixSide;
        if (action.params.type == ActionType::VeiledExalt) {
            metadata.can_create |= kCarrierCraftedState;
        }
        if (action.params.type == ActionType::HarvestAugment ||
            action.params.type == ActionType::HarvestResist) {
            metadata.can_destroy =
                kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
                kCarrierJunkBlockers | kCarrierPrefixSide |
                kCarrierSuffixSide;
            metadata.can_make_unreachable = metadata.can_destroy;
        }
        break;
    case ActionType::Annul:
    case ActionType::EldritchAnnul:
        metadata.can_destroy =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers | kCarrierCraftedState |
            kCarrierPrefixSide | kCarrierSuffixSide |
            kCarrierActiveProtection;
        metadata.can_make_unreachable = metadata.can_destroy;
        break;
    case ActionType::Scour:
    case ActionType::RemoveCraftedModifiers:
        metadata.can_destroy =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers | kCarrierCraftedState |
            kCarrierPrefixSide | kCarrierSuffixSide |
            kCarrierActiveProtection;
        metadata.can_make_unreachable = metadata.can_destroy;
        break;
    case ActionType::Unveil:
        metadata.can_destroy =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers | kCarrierActiveProtection;
        metadata.can_create =
            kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
            kCarrierJunkBlockers;
        metadata.can_make_unreachable = metadata.can_destroy;
        break;
    case ActionType::Bench: {
        metadata.can_create =
            kCarrierCraftedState | kCarrierPrefixSide | kCarrierSuffixSide;
        const int metamod =
            action.params.mod_id < session.metamod_type.size()
                ? session.metamod_type[action.params.mod_id]
                : -1;
        if (metamod >= 0) metadata.can_create |= kCarrierActiveProtection;
        else {
            metadata.can_create |=
                kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset;
        }
        break;
    }
    case ActionType::Fracture:
        metadata.can_create = kCarrierFracturedState;
        metadata.preserves_fractured_affixes = true;
        break;
    default:
        break;
    }
    return metadata;
}

void add_basic_currency(ActionRegistry& registry) {
    {
        auto d = base_descriptor("transmute", ActionType::Transmute,
                                 TransitionKind::Reforge, kRarityNormal);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("augment", ActionType::Augment,
                                 TransitionKind::SingleSlot, kRarityMagic);
        d.legality.requires_open_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("alteration", ActionType::Alteration,
                                 TransitionKind::Reforge, kRarityMagic);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("regal", ActionType::Regal,
                                 TransitionKind::SingleSlot, kRarityMagic);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("alchemy", ActionType::Alchemy,
                                 TransitionKind::Reforge, kRarityNormal);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("chaos", ActionType::Chaos,
                                 TransitionKind::Reforge, kRarityRare);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("exalt", ActionType::Exalt,
                                 TransitionKind::SingleSlot, kRarityRare);
        d.legality.requires_open_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("annul", ActionType::Annul,
                                 TransitionKind::SingleSlot,
                                 kRarityMagic | kRarityRare);
        d.legality.requires_removable_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("scour", ActionType::Scour,
                                 TransitionKind::Deterministic,
                                 kRarityMagic | kRarityRare);
        d.clears_flags = kFlagCraftedMod | kFlagVeiledMod;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor(
            "remove_crafted_modifiers", ActionType::RemoveCraftedModifiers,
            TransitionKind::Deterministic, kRarityMagic | kRarityRare);
        d.cost_keys = {"scour"};
        d.legality.required_flags = kFlagCraftedMod;
        d.clears_flags =
            kFlagCraftedMod | kFlagMultimod | kFlagNoAttack |
            kFlagNoCaster | kFlagPrefixesLocked | kFlagSuffixesLocked;
        add(registry, std::move(d));
    }
}

void add_essences(const SessionImpl& session, ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    for (std::uint32_t i = 0; i < data.essence_count; ++i) {
        if (i >= session.essence_guaranteed_mod_ids.size() ||
            session.essence_guaranteed_mod_ids[i] == kNoId) {
            continue;
        }
        const std::int32_t restriction =
            data.essence_item_level_restrictions[i];
        if (restriction >= 0 &&
            session.item_level > static_cast<std::uint32_t>(restriction)) {
            continue;
        }
        const std::string& key = data.string_at(data.essence_key_sids[i]);
        ActionDescriptor d;
        d.id = "essence:" + key;
        d.display_name = d.id;
        d.params.type = ActionType::Essence;
        d.params.essence_index = i;
        d.kind = TransitionKind::Reforge;
        d.cost_keys = {"essence:" + key};
        d.legality.rarity_mask = kRarityAny;
        add(registry, std::move(d));
    }
}

void add_fossils(
    const SessionImpl& session,
    ActionRegistry& registry,
    const ActionRegistryBuildOptions& options) {
    const DataImpl& data = *session.data;
    /* Real socketable fossils carry display names; the nameless
     * RandomFossilOutcome rows are Tangled Fossil internals, not player
     * currency, and must not become plannable actions. */
    std::vector<std::uint32_t> fossils;
    for (std::uint32_t i = 0; i < data.fossil_count; ++i) {
        if (!data.string_at(data.fossil_name_sids[i]).empty()) {
            fossils.push_back(i);
        }
    }

    const auto choose = [](std::size_t n, std::size_t k) {
        if (k > n) return std::size_t{0};
        std::size_t value = 1;
        for (std::size_t i = 1; i <= k; ++i) {
            value = value * (n - (k - i)) / i;
        }
        return value;
    };
    const std::size_t possible =
        choose(fossils.size(), 1) + choose(fossils.size(), 2) +
        choose(fossils.size(), 3) + choose(fossils.size(), 4);
    registry.fossil_loadouts_possible = static_cast<std::uint32_t>(possible);
    registry.fossil_generation_lazy = !options.exhaustive_fossils;
    registry.fossil_generation_goal_relevant =
        options.goal_relevant_fossils;

    /* Every emitted 1-4 fossil resonator loadout is a distinct action. In
     * reduced mode explicit signatures and/or the bounded goal-relevant beam
     * are materialized; everything else remains diagnostically deferred. */
    const auto emit = [&](const std::vector<std::uint32_t>& combo) {
        if (combo.empty()) return;
        ActionDescriptor d;
        d.params.type = ActionType::Fossil;
        d.params.fossil_indices = combo;
        d.kind = TransitionKind::Reforge;
        d.legality.rarity_mask = kRarityAny;

        std::vector<std::string> keys;
        std::vector<std::string> names;
        for (std::uint32_t fossil : combo) {
            keys.push_back(data.string_at(data.fossil_key_sids[fossil]));
            names.push_back(data.string_at(data.fossil_name_sids[fossil]));
        }
        std::sort(keys.begin(), keys.end());
        d.id = "fossil:";
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) d.id += "+";
            d.id += keys[i];
            d.cost_keys.push_back("fossil:" + keys[i]);
        }
        if (registry.index_by_id.contains(d.id)) return;
        d.cost_keys.push_back("resonator:" + std::to_string(keys.size()));
        d.display_name = names[0];
        for (std::size_t i = 1; i < names.size(); ++i) {
            d.display_name += " + " + names[i];
        }
        /* Fossil tags change the pool used by this reforge; they do not make
         * an already-rolled junk modifier decision-relevant to a later
         * reforge. Adding every weight tag to the state discriminator widens
         * the junk partition without preserving any future predicate. */
        add(registry, std::move(d));
        ++registry.fossil_loadouts_generated;
    };

    static_assert(PC_MAX_FOSSILS_PER_ACTION == 4);
    if (!options.exhaustive_fossils) {
        std::unordered_map<std::string, std::uint32_t> fossil_by_key;
        for (std::uint32_t fossil : fossils) {
            fossil_by_key.emplace(
                data.string_at(data.fossil_key_sids[fossil]), fossil);
        }
        std::vector<std::string> requested =
            options.requested_fossil_action_ids;
        std::sort(requested.begin(), requested.end());
        requested.erase(std::unique(requested.begin(), requested.end()),
                        requested.end());
        for (const std::string& id : requested) {
            if (!id.starts_with("fossil:")) continue;
            std::vector<std::uint32_t> combo;
            std::size_t begin = 7;
            bool valid = begin < id.size();
            while (begin <= id.size()) {
                const std::size_t end = id.find('+', begin);
                const std::string key = id.substr(
                    begin, end == std::string::npos
                               ? std::string::npos
                               : end - begin);
                const auto found = fossil_by_key.find(key);
                if (key.empty() || found == fossil_by_key.end()) {
                    valid = false;
                    break;
                }
                combo.push_back(found->second);
                if (end == std::string::npos) {
                    begin = id.size() + 1;
                    break;
                }
                begin = end + 1;
            }
            if (valid && !combo.empty() &&
                combo.size() <= PC_MAX_FOSSILS_PER_ACTION) {
                std::sort(combo.begin(), combo.end());
                combo.erase(std::unique(combo.begin(), combo.end()),
                            combo.end());
                if (combo.size() <= PC_MAX_FOSSILS_PER_ACTION) emit(combo);
            }
        }
        if (options.goal_relevant_fossils) {
            for (const auto& combo :
                 goal_relevant_fossil_combos(session, fossils, options)) {
                emit(combo);
            }
        }
        registry.fossil_loadouts_deferred =
            registry.fossil_loadouts_possible -
            registry.fossil_loadouts_generated;
        return;
    }
    const std::size_t count = fossils.size();
    for (std::size_t a = 0; a < count; ++a) {
        emit({fossils[a]});
        for (std::size_t b = a + 1; b < count; ++b) {
            emit({fossils[a], fossils[b]});
            for (std::size_t c = b + 1; c < count; ++c) {
                emit({fossils[a], fossils[b], fossils[c]});
                for (std::size_t e = c + 1; e < count; ++e) {
                    emit({fossils[a], fossils[b], fossils[c], fossils[e]});
                }
            }
        }
    }
    registry.fossil_loadouts_deferred = 0;
}

void add_bench(const SessionImpl& session, ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    const std::uint32_t attack = tag_id(session, "attack");
    const std::uint32_t caster = tag_id(session, "caster");
    for (std::uint32_t mod_id : session.bench_mod_ids) {
        const std::string& key = mod_key(session, mod_id);
        ActionDescriptor d;
        d.id = "bench:" + key;
        d.display_name = d.id;
        d.params.type = ActionType::Bench;
        d.params.mod_id = mod_id;
        d.kind = TransitionKind::Deterministic;
        d.cost_keys = {"bench:" + key};
        d.legality.rarity_mask = kRarityMagic | kRarityRare;
        d.legality.requires_open_affix = true;
        const std::int32_t metamod = session.metamod_type[mod_id];
        if (metamod >= 0 && metamod == data.metamod_multimod_code) {
            d.sets_flags = kFlagMultimod;
        } else if (metamod >= 0 && metamod == data.metamod_no_attack_code) {
            d.sets_flags = kFlagNoAttack;
            /* While active, pool actions treat attack-tagged junk
             * differently, so the tag discriminates. */
            if (attack != kNoId) d.discriminating_tag_ids.push_back(attack);
        } else if (metamod >= 0 && metamod == data.metamod_no_caster_code) {
            d.sets_flags = kFlagNoCaster;
            if (caster != kNoId) d.discriminating_tag_ids.push_back(caster);
        } else if (metamod >= 0 &&
                   metamod == data.metamod_prefixes_locked_code) {
            d.sets_flags = kFlagPrefixesLocked;
        } else if (metamod >= 0 &&
                   metamod == data.metamod_suffixes_locked_code) {
            d.sets_flags = kFlagSuffixesLocked;
        } else {
            d.sets_flags = kFlagCraftedMod;
        }
        add(registry, std::move(d));
    }
}

void add_veiled(const SessionImpl& session, ActionRegistry& registry) {
    const bool veiled_supported =
        session.veiled_prefix_mod_id != kNoId ||
        session.veiled_suffix_mod_id != kNoId;
    if (!veiled_supported) return;
    {
        auto d = base_descriptor("veiled_chaos", ActionType::VeiledChaos,
                                 TransitionKind::Reforge, kRarityRare);
        d.legality.forbidden_flags |= kFlagVeiledMod;
        d.sets_flags = kFlagVeiledMod;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("veiled_exalt", ActionType::VeiledExalt,
                                 TransitionKind::SingleSlot, kRarityRare);
        d.legality.forbidden_flags |= kFlagVeiledMod;
        d.legality.requires_open_affix = true;
        d.sets_flags = kFlagVeiledMod;
        add(registry, std::move(d));
    }
    {
        /* The unveil choice is resolved by the calculation engine's special
         * enumerator (S3); one descriptor stands for "unveil, choosing the
         * option the policy prefers". */
        ActionDescriptor d;
        d.id = "unveil";
        d.display_name = "unveil";
        d.params.type = ActionType::Unveil;
        d.kind = TransitionKind::Special;
        /* Selecting one of the options is the zero-cost resolution step;
         * the preceding veiled currency owns the economic cost. */
        d.cost_keys.clear();
        d.legality.rarity_mask = kRarityMagic | kRarityRare;
        d.legality.required_flags = kFlagVeiledMod;
        d.clears_flags = kFlagVeiledMod;
        add(registry, std::move(d));
    }
}

void add_harvest(const SessionImpl& session, ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    /* The checked-in economy recipe manifest is the owner-approved craft
     * allowlist. It generates these native arrays at configure time, so the
     * registry never promotes arbitrary data tags into fictional crafts. */
    const auto add_targeted = [&](const std::string_view name,
                                  const ActionType type) {
        const auto found = data.tag_id_by_name.find(std::string(name));
        if (found == data.tag_id_by_name.end() ||
            !session_tag_nonempty(session, found->second)) {
            return;
        }
        ActionDescriptor d;
        const bool reforge = type == ActionType::HarvestReforge;
        d.id = std::string(reforge ? "harvest_reforge:"
                                   : "harvest_augment:") +
               std::string(name);
        d.display_name = d.id;
        d.params.type = type;
        d.params.target_tag_id = found->second;
        d.kind = reforge ? TransitionKind::Reforge : TransitionKind::Special;
        d.cost_keys = {d.id};
        d.legality.rarity_mask =
            reforge ? kRarityRare : kRarityMagic | kRarityRare;
        if (!reforge) {
            /* Harvest augment is intentionally add-then-remove (ruled by
             * the project owner). */
            d.legality.requires_open_affix = true;
            d.legality.forbidden_flags |=
                kFlagInfluenced | kFlagEldritchImplicit;
        }
        /* Target tags choose this action's roll pool. Existing mod tags do not
         * affect a later targeted reforge/augment, so they are not persistent
         * state discriminators. Resistance conversion below is different: it
         * explicitly selects an existing source-tagged modifier. */
        add(registry, std::move(d));
    };
    for (const std::string_view name : kHarvestReforgeTags) {
        add_targeted(name, ActionType::HarvestReforge);
    }
    for (const std::string_view name : kHarvestAugmentTags) {
        add_targeted(name, ActionType::HarvestAugment);
    }

    /* Resistance conversion exists only between the three elements
     * (confirmed on the official wiki's Harvest craft list): six ordered
     * fire/cold/lightning pairs, each requiring session mods that carry
     * both the element tag and the resistance tag. */
    const std::uint32_t resistance = tag_id(session, "resistance");
    if (resistance == kNoId || !session_tag_nonempty(session, resistance)) {
        return;
    }
    std::vector<std::uint32_t> resist_tags;
    const auto& resist_mask = session.implicit_tag_masks[resistance];
    for (const char* element : {"fire", "cold", "lightning"}) {
        const std::uint32_t tag = tag_id(session, element);
        if (tag == kNoId || tag >= session.implicit_tag_masks.size()) {
            continue;
        }
        const auto& mask = session.implicit_tag_masks[tag];
        if (mask.empty()) continue;
        bool overlaps = false;
        for (std::size_t w = 0; w < mask.size() && w < resist_mask.size();
             ++w) {
            if (mask[w] & resist_mask[w]) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) resist_tags.push_back(tag);
    }
    for (std::uint32_t source : resist_tags) {
        for (std::uint32_t target : resist_tags) {
            if (source == target) continue;
            const std::string& source_name = data.tag_name_by_id.at(source);
            const std::string& target_name = data.tag_name_by_id.at(target);
            ActionDescriptor d;
            d.id = "harvest_resist:" + source_name + ":" + target_name;
            d.display_name = d.id;
            d.params.type = ActionType::HarvestResist;
            d.params.source_tag_id = source;
            d.params.target_tag_id = target;
            d.kind = TransitionKind::SingleSlot;
            /* Lifeforce depends on the resistance being created, not the
             * resistance being removed. Keep the vocabulary target-specific
             * so economy recipes cannot accidentally price all six actions
             * with one arbitrary lifeforce. */
            d.cost_keys = {"harvest_resist:" + target_name};
            d.legality.rarity_mask = kRarityMagic | kRarityRare;
            d.discriminating_tag_ids = {source, target, resistance};
            add(registry, std::move(d));
        }
    }
}

void add_eldritch(const SessionImpl& session, ActionRegistry& registry) {
    if (!session.eldritch_eligible) return;
    const auto tier_actions = [&](const char* base_id, ActionType type,
                                  const auto& by_tier) {
        for (std::uint32_t tier = 1; tier <= 4; ++tier) {
            if (tier >= by_tier.size() || by_tier[tier].empty()) continue;
            ActionDescriptor d;
            d.id = std::string(base_id) + ":" + std::to_string(tier);
            d.display_name = d.id;
            d.params.type = type;
            d.params.tier = tier;
            d.kind = TransitionKind::SingleSlot;
            d.cost_keys = {d.id};
            d.legality.rarity_mask = kRarityAny;
            d.legality.requires_session_eldritch = true;
            d.legality.forbidden_flags |= kFlagInfluenced;
            d.sets_flags = kFlagEldritchImplicit;
            add(registry, std::move(d));
        }
    };
    tier_actions("eldritch_ember", ActionType::EldritchEmber,
                 session.eldritch_searing_tier_mod_ids);
    tier_actions("eldritch_ichor", ActionType::EldritchIchor,
                 session.eldritch_eater_tier_mod_ids);
    {
        auto d = base_descriptor("eldritch_exalt", ActionType::EldritchExalt,
                                 TransitionKind::SingleSlot, kRarityRare);
        d.legality.requires_session_eldritch = true;
        d.legality.requires_open_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("eldritch_chaos", ActionType::EldritchChaos,
                                 TransitionKind::Reforge, kRarityRare);
        d.legality.requires_session_eldritch = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("eldritch_annul", ActionType::EldritchAnnul,
                                 TransitionKind::SingleSlot, kRarityAny);
        d.legality.requires_session_eldritch = true;
        d.legality.requires_removable_affix = true;
        add(registry, std::move(d));
    }
}

void add_influence_exalts(const SessionImpl& session,
                          ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    for (int code = 1;
         code < static_cast<int>(session.influence_masks.size()); ++code) {
        const auto& mask = session.influence_masks[code];
        bool nonempty = false;
        for (std::uint64_t word : mask) {
            if (word != 0) {
                nonempty = true;
                break;
            }
        }
        if (!nonempty) continue;
        if (data.influence_name_by_code.size() <=
            static_cast<std::size_t>(code)) {
            continue;
        }
        const std::string& name = data.influence_name_by_code[code];
        if (name.empty()) continue;
        ActionDescriptor d;
        d.id = "influence_exalt:" + name;
        d.display_name = d.id;
        d.params.type = ActionType::InfluenceExalt;
        d.params.influence_code = code;
        d.kind = TransitionKind::SingleSlot;
        d.cost_keys = {d.id};
        d.legality.rarity_mask = kRarityRare;
        d.legality.requires_open_affix = true;
        d.legality.forbidden_flags |=
            kFlagInfluenced | kFlagEldritchImplicit | kFlagFractured;
        d.sets_flags = kFlagInfluenced;
        add(registry, std::move(d));
    }
}

void add_structural(ActionRegistry& registry) {
    {
        auto d = base_descriptor("fracture", ActionType::Fracture,
                                 TransitionKind::Special, kRarityRare);
        d.legality.forbidden_flags |=
            kFlagInfluenced | kFlagSynthesised | kFlagFractured;
        d.legality.min_total_affixes = 4;
        d.sets_flags = kFlagFractured;
        add(registry, std::move(d));
    }
    {
        /* Synthetic: buy a fresh base and start over. Always legal — it is
         * how the policy leaves corrupted/mirrored dead ends — and it upper-
         * bounds every state's value (see plan, Problem Formalization). */
        ActionDescriptor d;
        d.id = "restart";
        d.display_name = "restart (fresh base)";
        d.synthetic = true;
        d.kind = TransitionKind::Deterministic;
        d.cost_keys = {"base"};
        d.legality.rarity_mask = kRarityAny;
        d.legality.forbidden_flags = 0;
        add(registry, std::move(d));
    }
}

} // namespace

ActionRefinementContract derive_action_refinement_contract(
    const SessionImpl& session,
    const ActionDescriptor& action) {
    return derive_refinement_contract(session, action);
}

std::vector<std::uint64_t> modifier_exclusion_effect_signature(
    const SessionImpl& session,
    const std::uint32_t mod_id) {
    std::vector<std::uint64_t> signature(session.words, 0);
    if (mod_id >= session.mod_count ||
        mod_id + 1 >= session.group_offsets.size()) {
        return signature;
    }
    for (std::uint32_t offset = session.group_offsets[mod_id];
         offset < session.group_offsets[mod_id + 1]; ++offset) {
        const std::uint32_t group = session.group_ids[offset];
        if (group >= session.group_masks.size() ||
            session.group_masks[group].empty()) {
            continue;
        }
        pc_bitset_or(
            signature.data(), signature.data(),
            session.group_masks[group].data(), session.words);
    }
    return signature;
}

bool modifier_is_veiled_template(
    const SessionImpl& session,
    const std::uint32_t mod_id) {
    if (mod_id == session.veiled_prefix_mod_id ||
        mod_id == session.veiled_suffix_mod_id) {
        return true;
    }
    return mod_id < session.mod_count &&
           session.veiled_template_mask.size() >= session.words &&
           pc_bitset_test(
               session.veiled_template_mask.data(), mod_id);
}

std::vector<std::uint64_t> action_refinement_contract_signature(
    const ActionRefinementContract& contract) {
    std::vector<std::uint64_t> signature;
    signature.reserve(
        8 + contract.observed_modifier_tag_ids.size() +
        contract.affix_observations.size() * 3 +
        contract.item_affix_dependencies.size() * 2 +
        contract.affix_flows.size() * 6 +
        contract.preserved_affixes.size() * 2 +
        contract.destroyed_affixes.size() * 2);
    signature.push_back(contract.schema_version);
    signature.push_back(contract.observed_item_features);
    signature.push_back(contract.preserved_item_features);
    signature.push_back(contract.destroyed_item_features);
    /* Preserve the existing signature token for every non-choice action so
     * qualified identities remain stable. */
    signature.push_back(
        (contract.resets_to_fresh_item ? 1ull : 0ull) |
        (static_cast<std::uint64_t>(contract.outcome_observation) << 8));
    signature.push_back(contract.observed_modifier_tag_ids.size());
    for (const std::uint32_t tag :
         contract.observed_modifier_tag_ids) {
        signature.push_back(tag);
    }
    const auto append_selector =
        [&](const RefinementAffixSelector& selector) {
            signature.push_back(
                static_cast<std::uint64_t>(
                    selector.required_affix_traits) |
                (static_cast<std::uint64_t>(
                     selector.forbidden_affix_traits)
                 << 16) |
                (static_cast<std::uint64_t>(
                     selector.required_item_traits)
                 << 32) |
                (static_cast<std::uint64_t>(
                     selector.forbidden_item_traits)
                 << 40));
            signature.push_back(selector.required_tag_ids.size());
            for (const std::uint32_t tag :
                 selector.required_tag_ids) {
                signature.push_back(tag);
            }
        };
    signature.push_back(contract.affix_observations.size());
    for (const RefinementAffixObservation& observation :
         contract.affix_observations) {
        signature.push_back(observation.features);
        append_selector(observation.selector);
    }
    signature.push_back(
        contract.item_affix_dependencies.size());
    for (const RefinementItemAffixDependency& dependency :
         contract.item_affix_dependencies) {
        signature.push_back(dependency.item_features);
        signature.push_back(
            dependency.survivor_affix_features);
    }
    signature.push_back(contract.affix_flows.size());
    for (const RefinementAffixFlow& flow :
         contract.affix_flows) {
        append_selector(flow.source_selector);
        signature.push_back(flow.set_affix_traits);
        signature.push_back(flow.cleared_affix_traits);
        signature.push_back(flow.preserved_features);
        signature.push_back(
            flow.preserves_modifier_classification ? 1 : 0);
    }
    signature.push_back(contract.preserved_affixes.size());
    for (const RefinementAffixSelector& selector :
         contract.preserved_affixes) {
        append_selector(selector);
    }
    signature.push_back(contract.destroyed_affixes.size());
    for (const RefinementAffixSelector& selector :
         contract.destroyed_affixes) {
        append_selector(selector);
    }
    return signature;
}

void validate_action_refinement_contract(
    const ActionDescriptor& action) {
    const ActionRefinementContract& contract = action.refinement;
    const auto fail = [&](const char* reason) {
        throw std::logic_error(
            "action '" + action.id +
            "' has incomplete refinement contract: " + reason);
    };
    if (!contract.complete()) fail("unsupported schema");
    if (contract.outcome_observation !=
            RefinementOutcomeObservation::None &&
        contract.outcome_observation !=
            RefinementOutcomeObservation::ModifierOffer) {
        fail("unsupported outcome observation");
    }
    if (contract.outcome_observation !=
        calc_outcome_observation(action)) {
        fail("outcome observation disagrees with exact calculator handler");
    }
    if (contract.resets_to_fresh_item &&
        contract.outcome_observation !=
            RefinementOutcomeObservation::None) {
        fail("fresh reset cannot expose an observed choice");
    }
    if ((contract.observed_item_features &
         ~kAllRefinementItemFeatures) != 0) {
        fail("affix feature recorded as an item observation");
    }
    if (((contract.preserved_item_features |
          contract.destroyed_item_features) &
         ~kAllRefinementItemFeatures) != 0 ||
        (contract.preserved_item_features |
         contract.destroyed_item_features) !=
            kAllRefinementItemFeatures) {
        fail("item feature effects are incomplete");
    }
    const auto validate_selector =
        [&](const RefinementAffixSelector& selector) {
            if (((selector.required_affix_traits |
                  selector.forbidden_affix_traits) &
                 ~kAllRefinementAffixTraits) != 0 ||
                ((selector.required_item_traits |
                  selector.forbidden_item_traits) &
                 ~kAllRefinementItemTraits) != 0) {
                fail("selector contains unknown traits");
            }
            if ((selector.required_affix_traits &
                 selector.forbidden_affix_traits) != 0 ||
                (selector.required_item_traits &
                 selector.forbidden_item_traits) != 0) {
                fail("contradictory selector traits");
            }
            if ((selector.required_affix_traits &
                 (kRefinementAffixPrefix |
                  kRefinementAffixSuffix)) ==
                (kRefinementAffixPrefix |
                 kRefinementAffixSuffix)) {
                fail("selector requires both affix sides");
            }
            if ((selector.required_affix_traits &
                 (kRefinementAffixOnEldritchDominantSide |
                  kRefinementAffixOnEldritchNonDominantSide)) ==
                (kRefinementAffixOnEldritchDominantSide |
                 kRefinementAffixOnEldritchNonDominantSide)) {
                fail("selector requires both Eldritch sides");
            }
            if (!std::is_sorted(
                    selector.required_tag_ids.begin(),
                    selector.required_tag_ids.end()) ||
                std::adjacent_find(
                    selector.required_tag_ids.begin(),
                    selector.required_tag_ids.end()) !=
                    selector.required_tag_ids.end() ||
                std::find(
                    selector.required_tag_ids.begin(),
                    selector.required_tag_ids.end(), kNoId) !=
                    selector.required_tag_ids.end()) {
                fail("non-canonical selector tags");
            }
        };
    if (!std::is_sorted(
            contract.observed_modifier_tag_ids.begin(),
            contract.observed_modifier_tag_ids.end()) ||
        std::adjacent_find(
            contract.observed_modifier_tag_ids.begin(),
            contract.observed_modifier_tag_ids.end()) !=
            contract.observed_modifier_tag_ids.end() ||
        std::find(
            contract.observed_modifier_tag_ids.begin(),
            contract.observed_modifier_tag_ids.end(), kNoId) !=
            contract.observed_modifier_tag_ids.end()) {
        fail("non-canonical observed tags");
    }
    for (const RefinementAffixObservation& observation :
         contract.affix_observations) {
        if (observation.features == 0 ||
            (observation.features &
             ~kAllRefinementAffixFeatures) != 0) {
            fail("invalid affix observation feature");
        }
        validate_selector(observation.selector);
    }
    for (const RefinementItemAffixDependency& dependency :
         contract.item_affix_dependencies) {
        if (dependency.item_features == 0 ||
            (dependency.item_features &
             ~kAllRefinementItemFeatures) != 0 ||
            (dependency.item_features &
             ~(contract.preserved_item_features &
               contract.destroyed_item_features)) != 0 ||
            dependency.survivor_affix_features == 0 ||
            (dependency.survivor_affix_features &
             ~kAllRefinementAffixFeatures) != 0) {
            fail("invalid rewritten-item affix dependency");
        }
    }
    for (const RefinementAffixFlow& flow :
         contract.affix_flows) {
        validate_selector(flow.source_selector);
        if (flow.preserved_features == 0 ||
            (flow.preserved_features &
             ~kAllRefinementAffixFeatures) != 0) {
            fail("invalid affix-flow feature mask");
        }
        if (((flow.set_affix_traits |
              flow.cleared_affix_traits) &
             ~kAllRefinementAffixTraits) != 0 ||
            (flow.set_affix_traits &
             flow.cleared_affix_traits) != 0) {
            fail("invalid affix-flow trait mutation");
        }
        if (!flow.preserves_modifier_classification &&
            (flow.preserved_features &
             feature(
                 RefinementFeature::
                     ModifierClassificationTags)) != 0) {
            fail("replacement flow preserves classification feature");
        }
    }
    for (const RefinementAffixSelector& selector :
         contract.preserved_affixes) {
        validate_selector(selector);
    }
    for (const RefinementAffixSelector& selector :
         contract.destroyed_affixes) {
        validate_selector(selector);
    }
    if (!std::is_sorted(
            contract.affix_observations.begin(),
            contract.affix_observations.end(), observation_less) ||
        std::adjacent_find(
            contract.affix_observations.begin(),
            contract.affix_observations.end(),
            [](const RefinementAffixObservation& lhs,
               const RefinementAffixObservation& rhs) {
                return lhs.selector == rhs.selector;
            }) != contract.affix_observations.end() ||
        !std::is_sorted(
            contract.preserved_affixes.begin(),
            contract.preserved_affixes.end(), selector_less) ||
        std::adjacent_find(
            contract.preserved_affixes.begin(),
            contract.preserved_affixes.end()) !=
            contract.preserved_affixes.end() ||
        !std::is_sorted(
            contract.destroyed_affixes.begin(),
            contract.destroyed_affixes.end(), selector_less) ||
        std::adjacent_find(
            contract.destroyed_affixes.begin(),
            contract.destroyed_affixes.end()) !=
            contract.destroyed_affixes.end() ||
        !std::is_sorted(
            contract.affix_flows.begin(),
            contract.affix_flows.end(), flow_less) ||
        std::adjacent_find(
            contract.affix_flows.begin(),
            contract.affix_flows.end()) !=
            contract.affix_flows.end() ||
        !std::is_sorted(
            contract.item_affix_dependencies.begin(),
            contract.item_affix_dependencies.end(),
            dependency_less) ||
        std::adjacent_find(
            contract.item_affix_dependencies.begin(),
            contract.item_affix_dependencies.end()) !=
            contract.item_affix_dependencies.end()) {
        fail("non-canonical selector order");
    }
    const bool observes_tags = std::any_of(
        contract.affix_observations.begin(),
        contract.affix_observations.end(),
        [](const RefinementAffixObservation& observation) {
            return (observation.features &
                    refinement_feature(
                        RefinementFeature::
                            ModifierClassificationTags)) != 0;
        });
    if (observes_tags !=
        !contract.observed_modifier_tag_ids.empty()) {
        fail("tag observation vocabulary is incomplete");
    }
    const auto selector_tags_are_observed =
        [&](const RefinementAffixSelector& selector) {
            return std::all_of(
                selector.required_tag_ids.begin(),
                selector.required_tag_ids.end(),
                [&](const std::uint32_t tag) {
                    return std::binary_search(
                        contract.observed_modifier_tag_ids.begin(),
                        contract.observed_modifier_tag_ids.end(), tag);
                });
        };
    const bool selectors_have_observed_tags =
        std::all_of(
            contract.affix_observations.begin(),
            contract.affix_observations.end(),
            [&](const RefinementAffixObservation& observation) {
                return selector_tags_are_observed(
                    observation.selector);
            }) &&
        std::all_of(
            contract.affix_flows.begin(),
            contract.affix_flows.end(),
            [&](const RefinementAffixFlow& flow) {
                return selector_tags_are_observed(
                    flow.source_selector);
            }) &&
        std::all_of(
            contract.preserved_affixes.begin(),
            contract.preserved_affixes.end(),
            selector_tags_are_observed) &&
        std::all_of(
            contract.destroyed_affixes.begin(),
            contract.destroyed_affixes.end(),
            selector_tags_are_observed);
    if (!selectors_have_observed_tags) {
        fail("selector tag is absent from the observation vocabulary");
    }
    if (contract.resets_to_fresh_item) {
        if (contract.preserved_item_features != 0 ||
            contract.destroyed_item_features !=
                kAllRefinementItemFeatures ||
            !contract.affix_flows.empty() ||
            !contract.preserved_affixes.empty() ||
            contract.destroyed_affixes.size() != 1 ||
            contract.destroyed_affixes.front() !=
                RefinementAffixSelector{}) {
            fail("fresh reset must destroy every exact affix");
        }
        return;
    }

    /*
     * Admission proves that the finite source trait domain is total. Tags are
     * intentionally not enumerated: a tag-scoped destruction is accompanied
     * by an unscoped survivor flow for its complement in every current
     * mechanic, and selectors cannot express a forbidden tag.
     */
    for (std::uint16_t affix_traits = 0;
         affix_traits <= kAllRefinementAffixTraits;
         ++affix_traits) {
        const std::uint16_t sides =
            affix_traits &
            (kRefinementAffixPrefix |
             kRefinementAffixSuffix);
        if (sides != kRefinementAffixPrefix &&
            sides != kRefinementAffixSuffix) {
            continue;
        }
        for (std::uint8_t item_traits = 0;
             item_traits <= kAllRefinementItemTraits;
             ++item_traits) {
            const std::uint16_t eldritch_sides =
                affix_traits &
                (kRefinementAffixOnEldritchDominantSide |
                 kRefinementAffixOnEldritchNonDominantSide);
            const bool has_dominance =
                (item_traits &
                 kRefinementItemHasEldritchDominance) != 0;
            if ((!has_dominance && eldritch_sides != 0) ||
                (has_dominance &&
                 eldritch_sides !=
                     kRefinementAffixOnEldritchDominantSide &&
                 eldritch_sides !=
                     kRefinementAffixOnEldritchNonDominantSide)) {
                continue;
            }

            bool covered = refinement_selectors_match(
                contract.destroyed_affixes,
                affix_traits, item_traits);
            for (const RefinementAffixFlow& flow :
                 contract.affix_flows) {
                if (!refinement_selector_matches(
                        flow.source_selector,
                        affix_traits, item_traits)) {
                    continue;
                }
                covered = true;
                const std::uint16_t target_traits =
                    static_cast<std::uint16_t>(
                        (affix_traits |
                         flow.set_affix_traits) &
                        ~flow.cleared_affix_traits);
                const auto preserved =
                    [&](const RefinementFeature feature) {
                        return (flow.preserved_features &
                                refinement_feature(feature)) != 0;
                    };
                const auto trait_changed =
                    [&](const std::uint16_t mask) {
                        return (affix_traits & mask) !=
                               (target_traits & mask);
                    };
                if ((preserved(RefinementFeature::ModifierSide) &&
                     trait_changed(
                         kRefinementAffixPrefix |
                         kRefinementAffixSuffix)) ||
                    (preserved(RefinementFeature::ModifierCrafted) &&
                     trait_changed(kRefinementAffixCrafted)) ||
                    (preserved(RefinementFeature::ModifierFractured) &&
                     trait_changed(kRefinementAffixFractured)) ||
                    (preserved(RefinementFeature::ModifierVeiled) &&
                     trait_changed(kRefinementAffixVeiled))) {
                    fail("affix flow mutates a preserved feature");
                }
                const std::uint16_t target_sides =
                    target_traits &
                    (kRefinementAffixPrefix |
                     kRefinementAffixSuffix);
                if (target_sides != kRefinementAffixPrefix &&
                    target_sides != kRefinementAffixSuffix) {
                    fail("affix flow produces invalid side traits");
                }
                const std::uint16_t target_eldritch_sides =
                    target_traits &
                    (kRefinementAffixOnEldritchDominantSide |
                     kRefinementAffixOnEldritchNonDominantSide);
                if ((!has_dominance &&
                     target_eldritch_sides != 0) ||
                    (has_dominance &&
                     target_eldritch_sides !=
                         kRefinementAffixOnEldritchDominantSide &&
                     target_eldritch_sides !=
                         kRefinementAffixOnEldritchNonDominantSide)) {
                    fail("affix flow produces invalid Eldritch traits");
                }
            }
            if (!covered) {
                fail("affix effect domain is incomplete");
            }
        }
    }
}

void canonicalize_and_validate_action_refinement_contract(
        ActionDescriptor& action) {
    canonicalize_contract(action.refinement);
    validate_action_refinement_contract(action);
}

void canonicalize_and_validate_action_refinement_contract(
        const SessionImpl& session,
        ActionDescriptor& action) {
    canonicalize_and_validate_action_refinement_contract(action);
    for (const std::uint32_t tag :
         action.refinement.observed_modifier_tag_ids) {
        if (tag >= session.implicit_tag_masks.size()) {
            throw std::logic_error(
                "action '" + action.id +
                "' has incomplete refinement contract: observed tag is "
                "outside the session vocabulary");
        }
    }
}

ActionRegistry build_action_registry(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options) {
    ActionRegistry registry;
    add_basic_currency(registry);
    add_essences(session, registry);
    add_fossils(session, registry, options);
    add_bench(session, registry);
    add_veiled(session, registry);
    add_harvest(session, registry);
    add_eldritch(session, registry);
    add_influence_exalts(session, registry);
    add_structural(registry);
    for (ActionDescriptor& action : registry.actions) {
        action.refinement =
            derive_action_refinement_contract(session, action);
        canonicalize_and_validate_action_refinement_contract(
            session, action);
        action.preservation =
            derive_preservation_metadata(session, action);
    }
    retain_goal_relevant_actions(session, options, registry);
    return registry;
}

} // namespace solver
} // namespace poecraft
