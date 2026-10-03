#include "calculator_currency.hpp"
#include "currency_outcomes.hpp"
#include "poecraft/bitset.h"
#include "json.hpp"
#include "solver_action_family_contract.hpp"

#include <cmath>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace poecraft::solver {
CalculatorItemGoal parse_calculator_item_goal(const SessionImpl& session, const char* text, std::size_t size) {
    return parse_calculator_item_goal(session, json::Parser(text, size).parse());
}

CalculatorItemGoal parse_calculator_item_goal(const SessionImpl& session, const json::Value& root) {
    CalculatorItemGoal goal;
    if (const auto* mods = root.find("implicit_mod_keys")) {
        if (mods->type != json::Type::Array || mods->array.size() > PC_MAX_IMPLICITS)
            throw std::invalid_argument("Goal implicits must be a bounded array of modifier keys");
        for (const auto& key : mods->array) {
            if (key.type != json::Type::String) throw std::invalid_argument("Goal implicit needs a stable modifier key");
            const auto pos = session.data->mod_pos_by_key.find(key.string);
            if (pos == session.data->mod_pos_by_key.end()) throw std::invalid_argument("Unknown goal implicit");
            const auto id = session.session_id_by_global_id.find(session.data->mod_global_ids[pos->second]);
            if (id == session.session_id_by_global_id.end() ||
                    (std::find(session.base_implicit_mod_ids.begin(), session.base_implicit_mod_ids.end(), id->second) == session.base_implicit_mod_ids.end() &&
                     !pc_bitset_test(session.corrupted_implicit_mask.data(), id->second) &&
                     !pc_bitset_test(session.eldritch_implicit_mask.data(), id->second)))
                throw std::invalid_argument("Goal modifier is not an eligible implicit in this session");
            if (std::find(goal.implicit_mods.begin(), goal.implicit_mods.end(), id->second) != goal.implicit_mods.end())
                throw std::invalid_argument("Duplicate goal implicit");
            goal.implicit_mods.push_back(id->second);
        }
    }
    if (const auto* bits = root.find("influence_bits")) {
        if (bits->type != json::Type::Number || bits->number < 0 || bits->number > 63 || std::floor(bits->number) != bits->number)
            throw std::invalid_argument("Goal influence_bits must be an integer from 0 to 63");
        goal.influence_bits = static_cast<std::uint8_t>(bits->number);
    }
    if (const auto* corrupted = root.find("corrupted")) {
        if (corrupted->type != json::Type::Bool) throw std::invalid_argument("Goal corrupted must be boolean");
        goal.corrupted = corrupted->boolean;
    }
    return goal;
}

std::string calculate_currency_json(CalcContext& source,
        const pc_item_state& receiver, const std::string& action,
        const SessionImpl* donor_session, const pc_item_state* donor,
        const CalculatorItemGoal& item_goal,
        const std::vector<CalculatorGoal>& requested_goals,
        const CalculatorTerminalLaw* terminal_law) {
    const bool multi = requested_goals.size() > 1;
    if (requested_goals.size() > kMaxCalculatorGoals)
        throw std::invalid_argument("Calculator supports at most eight goal items");
    const auto goals = requested_goals.empty()
        ? std::vector<CalculatorGoal>{{"", source.goal(), item_goal}} : requested_goals;
    std::set<std::string> ids;
    std::size_t aggregate_slots = 0;
    for (const auto& goal : goals) {
        if (!requested_goals.empty() && (goal.id.empty() || goal.id.size() > 64 ||
                goal.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.") != std::string::npos ||
                !ids.insert(goal.id).second))
            throw std::invalid_argument("Calculator goal IDs must be unique bounded ASCII identifiers");
        aggregate_slots += goal.explicit_goal.slots.size();
        if (goal.explicit_goal.slots.size() > kMaxGoalSlots || aggregate_slots > 64)
            throw std::invalid_argument("Calculator goal-set slot resource cap exceeded");
    }
    const bool double_corruption = action == "double_corruption";
    const bool expanded = action == "awakener" || action == "dominance" || action == "vaal" || double_corruption || action == "observe";
    const auto found_action = source.registry().index_by_id.find(action);
    if (!expanded && found_action == source.registry().index_by_id.end()) throw std::invalid_argument("Unknown Calculator action");
    if (multi && !expanded) {
        const auto& descriptor = source.registry().actions.at(found_action->second);
        if ((descriptor.synthetic && descriptor.id != "restart") || refinement_contract_observes_modifier_offer(descriptor.refinement))
            throw std::invalid_argument("Multi-goal observed choice requires an explicit common policy; this action is unavailable");
    }
    const bool renewal = !expanded && !source.registry().actions[found_action->second].synthetic &&
        action_transition_facts(source.registry().actions[found_action->second].params.type).renewal;
    const bool omit_affixes = (renewal || action == "awakener" || action == "vaal") && std::all_of(goals.begin(), goals.end(), [](const auto& goal) {
            return goal.explicit_goal.slots.empty() && goal.explicit_goal.terminal.extras == ExtraExplicitPolicy::Allow &&
                !goal.explicit_goal.terminal.prefixes && !goal.explicit_goal.terminal.suffixes;
        });
    if ((receiver.item_flags & PC_ITEM_FORESEEN) || (donor && (donor->item_flags & PC_ITEM_FORESEEN)))
        throw std::invalid_argument("Hinekora's Lock information-state calculation is unavailable; foresight cannot be dropped");
    if (receiver.memory_strands || receiver.lifecycle != PC_ITEM_LIVE)
        throw std::invalid_argument("Calculation requires a live item without memory strands");
    if (!expanded && receiver.enchantment_count)
        throw std::invalid_argument("Crafting on retained enchantments is unavailable until their effect and socket contracts are implemented");
    const auto& session = source.session();
    if (!expanded) {
        const auto& descriptor = source.registry().actions.at(found_action->second);
        if (!descriptor.synthetic && descriptor.params.type == ActionType::Fossil)
            if (const char* reason = unavailable_fossil_reason(*session.data, descriptor.params.fossil_indices))
                throw std::invalid_argument(reason);
    }
    // Include concrete retained/upgrade tiers and all influence signatures.
    // The refill kernel builds its pool from the concrete item, so no donor
    // identity or retained conflict group is reconstructed from coarse junk.
    std::vector<std::uint64_t> reachable(session.words, 0);
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod)
        if (session.gen_type[mod] <= 1) pc_bitset_set(reachable.data(), mod);
    // There is no continuation after this observation. The native refill DP
    // retains complete physical exclusion groups through every draw; only its
    // terminal rows merge junk that has identical goal/count/flag observations.
    // Admitting Chaos as a future action here would unnecessarily distinguish
    // hundreds of thousands of final junk configurations on jewellery.
    // One-shot terminal observations never execute a Dominance search row.
    // Keep descriptor indices for the requested action but exclude the search
    // carrier lookup; expanded Dominance below uses complete native pairs.
    auto observation_registry = source.registry();
    observation_registry.index_by_id.erase("dominance");
    observation_registry.automatic_dominance = false;
    // Validate each original eight-slot layout separately. Cross-goal overlap
    // becomes count membership on one carrier, never additional solver slots.
    std::vector<CountObservation> observations;
    std::vector<std::vector<std::array<std::size_t, 3>>> slot_queries(goals.size());
    const auto query = [&](const std::vector<std::uint64_t>& mask) {
        CountObservation observation;
        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod)
            if (pc_bitset_test(mask.data(), mod)) observation.ids.push_back(mod);
        const auto found = std::find_if(observations.begin(), observations.end(),
            [&](const auto& old) { return old.ids == observation.ids; });
        if (found != observations.end()) return std::size_t(found - observations.begin());
        observations.push_back(std::move(observation));
        return observations.size() - 1;
    };
    std::vector<std::uint32_t> implicit_mods;
    std::vector<std::uint64_t> implicit_requirements(goals.size());
    for (std::size_t g = 0; g < goals.size(); ++g) {
        const auto layout = build_abstract_layout(session, goals[g].explicit_goal,
            observation_registry, {}, true, false);
        if (multi) for (const auto& slot : layout.slots) {
            std::vector<std::uint64_t> blockers(session.words,0);
            for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
                if (session.gen_type[mod] > 1 || pc_bitset_test(slot.member_mask.data(),mod)) continue;
                for (auto i = session.group_offsets[mod]; i < session.group_offsets[mod+1]; ++i)
                    if (std::binary_search(slot.blocking_group_ids.begin(),slot.blocking_group_ids.end(),session.group_ids[i])) {
                        pc_bitset_set(blockers.data(),mod); break;
                    }
            }
            slot_queries[g].push_back({query(slot.member_mask), query(slot.satisfying_mask), query(blockers)});
        }
        for (auto mod : goals[g].item_goal.implicit_mods) {
            auto found = std::find(implicit_mods.begin(), implicit_mods.end(), mod);
            const auto index = std::size_t(found - implicit_mods.begin());
            if (index >= 64) throw std::invalid_argument("Calculator implicit observation resource cap exceeded");
            if (found == implicit_mods.end()) implicit_mods.push_back(mod);
            implicit_requirements[g] |= std::uint64_t{1} << index;
        }
    }
    auto terminal_goal = goals[0].explicit_goal;
    if (multi) {
        terminal_goal.slots.clear(); terminal_goal.min_satisfied_slots = 0;
        terminal_goal.terminal = {}; terminal_goal.terminal.extras = ExtraExplicitPolicy::Allow;
        terminal_goal.fixed_options.clear(); terminal_goal.automatic_candidates = false;
    }
    CalcContext terminal(source.shared_session(), terminal_goal, observation_registry,
        {}, true, false, false, std::nullopt, observations, false, reachable,
        false, false, true, false, true, nullptr, false, false, true);
    auto& calc = terminal;
    // Oversized requests fail explicitly; never publish truncated mass.
    calc.set_solve_resource_caps(multi ? 125000 : 250000, multi ? 50000000 : 100000000, false, (multi ? 256ull : 512ull) * 1024 * 1024);
    // Preserve implicit-goal observations until the final joint predicate;
    // states with equal affixes but different implicits must not merge early.
    using Observation = std::pair<std::uint32_t, std::uint64_t>;
    std::map<Observation, long double> mass;
    const auto record = [&](Observation observation, long double probability) {
        if (!std::isfinite(probability) || probability < 0)
            throw std::logic_error("Calculator outcome has invalid probability");
        if (!probability) return;
        mass[observation] += probability;
        if (mass.size() > 250000)
            throw std::length_error("Calculator terminal observation resource cap exceeded");
    };
    const auto implicit_mask = [&](const pc_item_state& item) {
        std::uint64_t mask = 0;
        for (std::size_t goal = 0; goal < implicit_mods.size(); ++goal)
            for (unsigned i = 0; i < item.implicit_count; ++i)
                if (item.implicits[i].mod_id == implicit_mods[goal]) mask |= std::uint64_t{1} << goal;
        return mask;
    };
    const auto properties_match = [&](std::size_t g, const AbstractState& state, std::uint64_t mask) {
        const auto& item = goals[g].item_goal;
        return (mask & implicit_requirements[g]) == implicit_requirements[g] &&
            (!item.influence_bits || state.influence_bits == *item.influence_bits) &&
            (!item.corrupted || bool(state.flags & kFlagCorrupted) == *item.corrupted);
    };
    const auto goal_state = [&](std::size_t g, const AbstractState& state) {
        auto projected = state;
        if (multi) {
            projected.slot_status.fill(0);
            projected.blocked_mask = 0;
            const auto count = [&](std::size_t observation) {
                unsigned n = 0;
                for (auto c : calc.layout().count_observations.at(observation).junk_class_indices)
                    n += state.junk_counts[c];
                return n;
            };
            for (std::size_t i = 0; i < slot_queries[g].size(); ++i) {
                const auto [member, satisfying, blockers] = slot_queries[g][i];
                projected.slot_status[i] = count(satisfying) ? 2 : (count(member) ? 1 : 0);
                if (count(blockers)) projected.blocked_mask |= 1u << i;
            }
        }
        return projected;
    };
    const auto matches = [&](std::size_t g, const AbstractState& state, std::uint64_t mask) {
        return assess_terminal_goal(goals[g].explicit_goal, goal_state(g, state)).final_success &&
            properties_match(g, state, mask);
    };
    std::map<std::uint32_t, long double> implicit_present;
    std::map<std::uint32_t, std::uint64_t> implicit_weights;
    std::map<std::uint32_t, long double> implicit_added;
    std::map<std::pair<std::uint32_t, std::uint32_t>, long double> implicit_pairs;
    long double failed_mass = 0;
    std::uint64_t total_implicit_weight = 0;
    const auto observe_implicits = [&](const pc_item_state& item, long double p) {
        std::set<std::uint32_t> ids;
        for (unsigned i = 0; i < item.implicit_count; ++i) ids.insert(item.implicits[i].mod_id);
        for (const auto id : ids) implicit_present[id] += p;
    };
    const auto project = [&](pc_item_state item) {
        // These mechanics retain enchantments. Only explicit structure
        // is observed here; no enchantment/socket continuation is asserted.
        item.enchantment_count = 0;
        return calc.intern_item(item);
    };
    const auto add = [&](const pc_item_state& item, long double p) {
        record({project(item), implicit_mask(item)}, p);
    };
    const auto refill = [&](pc_item_state base, long double p,
                            std::uint8_t target, bool blocks, bool clear) {
        base.enchantment_count = 0;
        const auto distribution = calc.concrete_refill({base, target, blocks, clear, kNoId, !omit_affixes});
        if (!distribution->supported || !distribution->applicable)
            throw std::invalid_argument("Exact currency refill is unavailable for this input");
        for (const auto& entry : distribution->entries)
            record({entry.state, implicit_mask(base)}, p * entry.probability);
    };
    bool legal = !(receiver.item_flags & (PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED));
    if (action == "observe") {
        legal = true;
        if (terminal_law) {
            (*terminal_law)([&](const pc_item_state& item, long double probability) {
                if (item.lifecycle != PC_ITEM_LIVE || item.memory_strands || (item.item_flags & PC_ITEM_FORESEEN))
                    throw std::invalid_argument("Shared Calculator observer requires qualified live output carriers without foresight/strands");
                observe_implicits(item, probability);
                add(item, probability);
            });
        } else add(receiver, 1);
    } else if (session.is_cluster()) {
        if (expanded || found_action == calc.registry().index_by_id.end())
            throw std::invalid_argument("This cluster action law is not yet approved and qualified");
        const auto& descriptor = calc.registry().actions[found_action->second];
        if (solver_action_disabled(calc.goal(), descriptor))
            throw std::invalid_argument("Calculation action belongs to a disabled family");
        if (action == "restart") {
            pc_item_state next;
            pc_item_clear(&next);
            legal = true;
            add(next, 1);
        } else {
            ActionContextImpl context(0);
            context.session = source.shared_session();
            const auto result = visit_cluster_currency_outcomes(context, receiver, descriptor.params,
                [&](const pc_item_state& item, long double probability) {
                    observe_implicits(item, probability);
                    add(item, probability);
                }, 2000000, [&](const pc_item_state& item) { return project(item); });
            legal = result.applied;
        }
    } else if (!expanded) {
        const auto index = found_action->second;
        const auto& descriptor = calc.registry().actions[index];
        if (solver_action_disabled(calc.goal(), descriptor)) throw std::invalid_argument("Calculation action belongs to a disabled family");
        // The caller's layout may describe another action and may merge
        // incoming junk with different physical pool exclusions. Preserve
        // the requested action's continuation observations until execution;
        // coarsen only its terminal results. Renewals already consume the
        // concrete receiver through concrete_refill, and implicit-only
        // actions below likewise consume the concrete receiver directly.
        std::unique_ptr<CalcContext> incoming;
        if (!renewal && descriptor.params.type != ActionType::EldritchEmber &&
                descriptor.params.type != ActionType::EldritchIchor) {
            auto action_goal = terminal_goal;
            action_goal.fixed_options.clear();
            action_goal.automatic_candidates = false;
            incoming = std::make_unique<CalcContext>(source.shared_session(),
                action_goal, observation_registry, std::vector<std::uint32_t>{index},
                true, false, true, std::nullopt,
                observations, false, reachable, false, false, false, false, false, nullptr, false, false, true);
            incoming->set_solve_resource_caps(multi ? 125000 : 250000, multi ? 50000000 : 100000000, false,
                (multi ? 256ull : 512ull) * 1024 * 1024);
        }
        auto& execution = incoming ? *incoming : calc;
        const auto start = execution.intern_item(receiver);
        legal = action_legal(session, descriptor, execution.state(start));
        if (!legal) add(receiver, 1);
        else if (descriptor.params.type == ActionType::EldritchEmber || descriptor.params.type == ActionType::EldritchIchor) {
            const bool searing = descriptor.params.type == ActionType::EldritchEmber;
            const auto weights = eldritch_implicit_weights(session, searing, descriptor.params.tier);
            std::uint64_t total_weight = 0;
            for (const auto& [id, weight] : weights) total_weight += weight;
            if (!total_weight) throw std::invalid_argument("Eldritch implicit pool is empty");
            for (const auto& [id, weight] : weights) {
                auto next = receiver;
                if (!set_eldritch_implicit(session, next, searing, descriptor.params.tier, id))
                    throw std::invalid_argument("Eldritch result exceeds implicit capacity");
                add(next, static_cast<long double>(weight) / total_weight);
            }
        } else {
            auto prepared = receiver;
            prepared.enchantment_count = 0;
            const auto concrete = renewal ? calc.concrete_refill({prepared, 0,
                action_transition_facts(descriptor.params.type).respects_metamod_pool_blocks, true, index, !omit_affixes}) : nullptr;
            const auto& distribution = concrete ? *concrete : execution.outcomes(start, index);
            if (!distribution.supported) throw std::invalid_argument("Exact outcomes are unavailable for this action");
            legal = distribution.applicable;
            auto implicits = receiver;
            if (action == "restart") {
                pc_item_clear(&implicits);
                for (const auto id : session.base_implicit_mod_ids) implicits.implicits[implicits.implicit_count++].mod_id = id;
            }
            const auto special = descriptor.params.type == ActionType::Fossil
                ? fossil_implicit_outcomes(session, implicits, descriptor.params.fossil_indices)
                : std::vector<std::pair<pc_item_state, long double>>{{implicits, 1.0L}};
            for (const auto& entry : distribution.entries) for (const auto& [implicit_item, p] : special) {
                auto id = entry.state;
                if (incoming) {
                    pc_item_state next;
                    if (!incoming->materialize(id, next))
                        throw std::invalid_argument("Exact currency result cannot be materialized");
                    id = project(next);
                }
                if (descriptor.params.type == ActionType::Fossil) {
                    auto state = calc.state(id);
                    state.flags &= ~(kFlagCorrupted | kFlagMirrored);
                    if (implicit_item.item_flags & PC_ITEM_CORRUPTED) state.flags |= kFlagCorrupted;
                    if (implicit_item.item_flags & PC_ITEM_MIRRORED) state.flags |= kFlagMirrored;
                    id = calc.intern_state(state);
                }
                record({id, implicit_mask(implicit_item)}, entry.probability * p);
            }
            if (!legal && mass.empty()) add(receiver, 1);
        }
    } else if (action == "awakener") {
        if (!donor_session || !donor)
            throw std::invalid_argument("Choose an Awakener donor from Stash");
        const auto choices = awakener_choices(session, *donor_session, *donor, receiver);
        const long double p = 1.0L / (choices.donor.size() * choices.receiver.size());
        for (const auto a : choices.donor) for (const auto b : choices.receiver)
            refill(awakener_base(session, *donor, receiver, a, b), p, 0, false, false);
    } else if (!legal) {
        add(receiver, 1);
    } else if (action == "dominance") {
        const auto choices = dominance_choices(session, receiver);
        legal = choices.size() >= 2;
        if (!legal) add(receiver, 1);
        else {
            const long double p = 1.0L / (choices.size() * (choices.size() - 1));
            for (std::size_t a = 0; a < choices.size(); ++a)
                for (std::size_t b = 0; b < choices.size(); ++b)
                    if (a != b) add(dominance_result(session, receiver, choices[a], choices[b]), p);
        }
    } else {
        const auto weights = vaal_implicit_weights(session);
        for (const auto& [id, weight] : weights) {
            implicit_weights[id] = weight;
            total_implicit_weight += weight;
        }
        auto unchanged = receiver;
        unchanged.item_flags |= PC_ITEM_CORRUPTED;
        if (double_corruption) {
            // Owner-selected terminal query: changed-affix brick and destruction
            // are failures. Do not invent the influenced reforge's internal law.
            failed_mass = 0.5L;
            add(unchanged, 0.25L); // ignored socket branch
            observe_implicits(unchanged, 0.25L);
            const auto compatible = [&](std::uint32_t a, std::uint32_t b) {
                if (a == b) return false;
                for (auto i = session.group_offsets[a]; i < session.group_offsets[a + 1]; ++i)
                    for (auto j = session.group_offsets[b]; j < session.group_offsets[b + 1]; ++j)
                        if (session.group_ids[i] == session.group_ids[j]) return false;
                return true;
            };
            for (const auto& [first, first_weight] : weights) {
                std::uint64_t remaining = 0;
                for (const auto& [second, weight] : weights)
                    if (compatible(first, second)) remaining += weight;
                if (!remaining)
                    throw std::invalid_argument("Double corruption requires two compatible corruption implicits");
                for (const auto& [second, weight] : weights) {
                    if (!compatible(first, second)) continue;
                    const long double p = 0.25L * first_weight / total_implicit_weight * weight / remaining;
                    implicit_pairs[std::minmax(first, second)] += p;
                    implicit_added[first] += p;
                    implicit_added[second] += p;
                }
            }
            // Each unordered pair aggregates both draw orders. All old implicits
            // (including Eldritch tiers) are replaced; explicit affixes survive.
            for (const auto& [pair, p] : implicit_pairs) {
                auto base = receiver;
                base.implicit_count = 0;
                base.searing_exarch_tier = base.eater_of_worlds_tier = 0;
                auto next = vaal_implicit_result(session, base, pair.first, 0);
                const auto other = vaal_implicit_result(session, base, pair.second, 0);
                next.implicits[next.implicit_count++] = other.implicits[0];
                add(next, p);
                observe_implicits(next, p);
            }
        } else {
            add(unchanged, 0.5L); // unchanged + ignored socket branch
            observe_implicits(unchanged, 0.75L); // reforge also retains implicits
            refill(unchanged, 0.25L, 6, true, true);
            const auto replacements = std::max<unsigned>(1, receiver.implicit_count);
            for (const auto& [id, weight] : weights) {
                implicit_added[id] = 0.25L * weight / total_implicit_weight;
                const long double p = implicit_added[id] / replacements;
                for (unsigned removed = 0; removed < replacements; ++removed) {
                    const auto next = vaal_implicit_result(session, receiver, id, removed);
                    add(next, p);
                    observe_implicits(next, p);
                }
            }
        }
    }
    long double total = failed_mass, success = 0;
    std::vector<long double> successes(goals.size());
    std::vector<std::array<long double, kMaxGoalSlots>> goal_slots(goals.size());
    std::vector<std::vector<long double>> goal_implicits(goals.size());
    for (std::size_t g = 0; g < goals.size(); ++g) goal_implicits[g].resize(goals[g].item_goal.implicit_mods.size());
    for (const auto& [observation, p] : mass) {
        const auto [id, mask] = observation;
        total += p;
        const auto& state = calc.state(id);
        bool any = false;
        for (std::size_t g = 0; g < goals.size(); ++g) {
            if (legal && matches(g, state, mask)) { successes[g] += p; any = true; }
            const auto projected = goal_state(g, state);
            for (std::size_t i = 0; i < kMaxGoalSlots; ++i) if (projected.slot_status[i] == 2) goal_slots[g][i] += p;
            for (std::size_t i = 0; i < goal_implicits[g].size(); ++i) {
                const auto bit = std::find(implicit_mods.begin(), implicit_mods.end(), goals[g].item_goal.implicit_mods[i]) - implicit_mods.begin();
                if (mask & (std::uint64_t{1} << bit)) goal_implicits[g][i] += p;
            }
        }
        if (any) success += p;
    }
    const auto& slots = goal_slots[0];
    const auto& implicit_slots = goal_implicits[0];
    if (std::abs(total - 1.0L) > 1e-10L)
        throw std::logic_error("Currency calculation failed probability conservation");
    if (mass.size() > 250000) throw std::length_error("Calculator terminal observation resource cap exceeded");
    std::ostringstream out;
    out << std::setprecision(17) << "{\"ok\":true,\"supported\":true,\"legal\":"
        << (legal ? "true" : "false") << ",\"success_probability\":" << double(success)
        << ",\"slot_satisfied\":[";
    for (std::size_t i = 0; i < slots.size(); ++i) out << (i ? "," : "") << double(slots[i]);
    out << "],\"implicit_satisfied\":[";
    for (std::size_t i = 0; i < implicit_slots.size(); ++i) out << (i ? "," : "") << double(implicit_slots[i]);
    out << "],\"outcomes\":[";
    bool comma = false;
    unsigned row_id = 0;
    for (const auto& [observation, p] : mass) {
        const auto [id, mask] = observation;
        const auto& state = calc.state(id);
        const auto projected = goal_state(0, state);
        bool any = false;
        for (std::size_t g = 0; g < goals.size(); ++g) any |= legal && matches(g, state, mask);
        out << (comma ? "," : "") << "{\"state\":" << row_id++
            << ",\"affixes_unobserved\":" << (omit_affixes ? "true" : "false")
            << ",\"probability\":" << double(p) << ",\"rarity\":" << unsigned(state.rarity)
            << ",\"prefixes\":" << unsigned(state.prefix_count)
            << ",\"suffixes\":" << unsigned(state.suffix_count)
            << ",\"flags\":" << state.flags << ",\"blocked\":" << projected.blocked_mask
            << ",\"goal_properties_satisfied\":" << (properties_match(0, state, mask) ? "true" : "false")
            << ",\"influence_bits\":" << unsigned(state.influence_bits)
            << ",\"is_goal\":" << (any ? "true" : "false") << ",\"slots\":[";
        for (std::size_t i = 0; i < slots.size(); ++i) out << (i ? "," : "") << unsigned(projected.slot_status[i]);
        out << "]";
        if (!requested_goals.empty()) {
            out << ",\"matched_goal_ids\":[";
            bool matched_comma = false;
            for (std::size_t g = 0; g < goals.size(); ++g) if (legal && matches(g, state, mask)) {
                out << (matched_comma ? "," : "") << "\"" << goals[g].id << "\""; matched_comma = true;
            }
            out << "],\"goal_observations\":[";
            for (std::size_t g = 0; g < goals.size(); ++g) {
                const auto value = goal_state(g, state);
                out << (g ? "," : "") << "{\"id\":\"" << goals[g].id << "\",\"is_goal\":"
                    << (legal && matches(g, state, mask) ? "true" : "false")
                    << ",\"blocked\":" << value.blocked_mask
                    << ",\"goal_properties_satisfied\":" << (properties_match(g, state, mask) ? "true" : "false") << ",\"slots\":[";
                for (std::size_t i = 0; i < kMaxGoalSlots; ++i) out << (i ? "," : "") << unsigned(value.slot_status[i]);
                out << "]}";
            }
            out << "]";
        }
        out << "}";
        comma = true;
        if (out.tellp() > 64ll * 1024 * 1024) throw std::length_error("Calculator result byte cap exceeded");
    }
    if (failed_mass) {
        int terminal_id = -1;
        for (const auto* terminal : {"bricked", "destroyed"}) {
            out << (comma ? "," : "") << "{\"state\":" << terminal_id--
                << ",\"terminal\":\"" << terminal << "\",\"probability\":0.25,\"rarity\":-1,\"prefixes\":0,\"suffixes\":0,\"flags\":0,\"blocked\":0,\"is_goal\":false,\"matched_goal_ids\":[],\"slots\":[";
            for (std::size_t i = 0; i < slots.size(); ++i) out << (i ? "," : "") << 0;
            out << "]}";
            comma = true;
        }
    }
    out << "]";
    if (!requested_goals.empty()) {
        out << ",\"any_goal_probability\":" << double(success) << ",\"goal_results\":[";
        for (std::size_t g = 0; g < goals.size(); ++g) {
            out << (g ? "," : "") << "{\"id\":\"" << goals[g].id << "\",\"success_probability\":" << double(successes[g]) << ",\"slot_satisfied\":[";
            for (std::size_t i = 0; i < kMaxGoalSlots; ++i) out << (i ? "," : "") << double(goal_slots[g][i]);
            out << "],\"implicit_satisfied\":[";
            for (std::size_t i = 0; i < goal_implicits[g].size(); ++i) out << (i ? "," : "") << double(goal_implicits[g][i]);
            out << "]}";
        }
        out << "]";
    }
    if ((action == "vaal" || double_corruption) && legal) {
        out << (double_corruption
            ? ",\"double_corruption_branches\":{\"implicit\":0.25,\"sockets\":0.25,\"reforge\":0.25,\"destroyed\":0.25}"
            : ",\"vaal_branches\":{\"implicit\":0.25,\"sockets\":0.25,\"reforge\":0.25,\"unchanged\":0.25}");
        out << ",\"implicit_outcomes\":[";
        comma = false;
        for (const auto& [id, p] : implicit_present) {
            const auto weight = implicit_weights[id];
            out << (comma ? "," : "") << "{\"mod\":" << id << ",\"weight\":" << weight
                << ",\"added_probability\":" << double(implicit_added[id])
                << ",\"present_probability\":" << double(p) << "}";
            comma = true;
        }
        out << "]";
        if (double_corruption) {
            out << ",\"implicit_pairs\":[";
            comma = false;
            for (const auto& [pair, p] : implicit_pairs) {
                out << (comma ? "," : "") << "{\"mods\":[" << pair.first << "," << pair.second
                    << "],\"probability\":" << double(p) << "}";
                comma = true;
            }
            out << "]";
        }
    }
    out << "}";
    auto result = out.str();
    if (result.size() > 64ull * 1024 * 1024) throw std::length_error("Calculator result byte cap exceeded");
    return result;
}

std::string observe_calculator_terminal_law_json(CalcContext& output,
        const std::vector<CalculatorGoal>& goals, const CalculatorTerminalLaw& law) {
    if (goals.empty() || !law) throw std::invalid_argument("Shared Calculator observation requires goals and a terminal law");
    pc_item_state carrier; pc_item_clear(&carrier);
    return calculate_currency_json(output, carrier, "observe", nullptr, nullptr, {}, goals, &law);
}
}
