#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

void SolveWork::Impl::prepare_strict_clean_goal_cover() {
        strict_clean_goal_cover_refresh_needed = false;
        const std::uint32_t initial_state_count = calc.state_count();
        if (strict_clean_goal_cover_state_count == initial_state_count) return;
        strict_clean_goal_cover_state_count = 0;
        strict_clean_goal_cover_cost.clear();
        if (session.eldritch_eligible) return;
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
            return (descriptor.synthetic &&
                    options.allow_economic_restart) ||
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
                    distribution_pointer = &calc.outcomes(
                        state, action,
                        options.goal_progress_gated_reforges);
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
                        &calc.outcomes(
                            result.start_state, action,
                            options.goal_progress_gated_reforges);
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
                calc.outcomes(
                    state, strict_exalt,
                    options.goal_progress_gated_reforges);
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
            clean_goal_cover_cost.size(), 0.0);
        std::vector<std::uint32_t> rare_policy(
            clean_goal_cover_cost.size(), kNoId);
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
                                calc.goal().required_satisfied_slots() &&
                            prefixes + suffixes == std::popcount(mask)) {
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
        ProofPatternContract& strict_contract = contract(
            ProofPatternKind::StrictClean);
        strict_contract.residual = delta;
        strict_contract.solution_sweeps = sweeps;
        strict_contract.converged = delta <= options.epsilon * 0.1;
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

} // namespace solver
} // namespace poecraft

