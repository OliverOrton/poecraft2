#pragma once

#include "solver_solve_contracts.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace poecraft {
namespace solver {
namespace solve_detail {

/*
 * Gate 4 executable-upper search vocabulary.
 *
 * The attempted product planner did not qualify its fixed-work control and
 * was removed. This behavior-neutral type survives as the reusable exact
 * projection boundary for a future planner. It has no conversion to a proof
 * lower, no pruning flag, and no terminal authority.
 */
enum ExecutableCarrierDebt : std::uint32_t {
    kExecutableDebtNone = 0,
    kExecutableDebtBlockedGoal = 1u << 0,
    kExecutableDebtPrefixCapacity = 1u << 1,
    kExecutableDebtSuffixCapacity = 1u << 2,
    kExecutableDebtTerminalJunk = 1u << 3,
    kExecutableDebtFracturedJunk = 1u << 4,
};

struct ExecutableCarrierProjection {
    std::uint32_t state = kNoId;
    std::uint32_t satisfied_goal_mask = 0;
    std::uint32_t missing_goal_mask = 0;
    std::uint32_t blocked_mask = 0;
    std::uint32_t crafted_goal_mask = 0;
    std::uint32_t fractured_goal_mask = 0;
    std::uint32_t fractured_metamod_flags = 0;
    std::uint32_t protection_flags = 0;
    std::uint32_t other_flags = 0;
    std::uint32_t junk_count = 0;
    std::uint32_t crafted_junk_count = 0;
    std::uint32_t fractured_junk_count = 0;
    std::uint32_t fractured_crafted_junk_count = 0;
    std::uint32_t debt_flags = kExecutableDebtNone;
    std::uint8_t prefix_count = 0;
    std::uint8_t suffix_count = 0;
    std::uint8_t missing_prefix_goals = 0;
    std::uint8_t missing_suffix_goals = 0;
    std::uint8_t prefix_capacity = 0;
    std::uint8_t suffix_capacity = 0;
    std::uint8_t rarity = PC_RARITY_NORMAL;
    std::uint8_t influence_bits = 0;
    std::int8_t veiled_side = -1;
    std::uint8_t searing_exarch_tier = 0;
    std::uint8_t eater_of_worlds_tier = 0;

    bool operator==(const ExecutableCarrierProjection&) const = default;
};

struct ExecutableCarrierActionProjection {
    std::uint32_t operator_index = kNoId;
    std::uint32_t preserved_goal_mask = 0;
    std::uint32_t destroyed_goal_mask = 0;
    std::uint32_t created_goal_mask = 0;
    std::uint32_t preserved_fractured_goal_mask = 0;
    std::uint32_t destroyed_fractured_goal_mask = 0;
    std::uint32_t preserved_protection = 0;
    std::uint32_t destroyed_protection = 0;
    std::uint32_t preserved_properties = 0;
    std::uint32_t destroyed_properties = 0;
    std::uint32_t created_properties = 0;

    bool operator==(const ExecutableCarrierActionProjection&) const =
        default;
};

inline ExecutableCarrierProjection make_executable_carrier_projection(
        const AbstractState& carrier,
        const std::uint32_t state,
        const std::uint32_t satisfied_goal_mask,
        const std::uint32_t prefix_goal_mask,
        const std::uint32_t suffix_goal_mask,
        const std::uint8_t affix_cap,
        const std::uint32_t required_goal_count,
        const std::uint32_t protection_mask) {
    ExecutableCarrierProjection projection;
    projection.state = state;
    projection.satisfied_goal_mask = satisfied_goal_mask;
    projection.missing_goal_mask =
        (prefix_goal_mask | suffix_goal_mask) & ~satisfied_goal_mask;
    projection.blocked_mask = carrier.blocked_mask;
    projection.crafted_goal_mask = carrier.crafted_goal_mask;
    projection.fractured_goal_mask = carrier.fractured_goal_mask;
    projection.fractured_metamod_flags =
        carrier.fractured_metamod_flags;
    projection.protection_flags = carrier.flags & protection_mask;
    projection.other_flags = carrier.flags & ~protection_mask;
    for (const std::uint8_t count : carrier.junk_counts) {
        projection.junk_count += count;
    }
    for (const std::uint8_t count : carrier.crafted_junk_counts) {
        projection.crafted_junk_count += count;
    }
    for (const std::uint8_t count : carrier.fractured_junk_counts) {
        projection.fractured_junk_count += count;
    }
    for (const std::uint8_t count :
         carrier.fractured_crafted_junk_counts) {
        projection.fractured_crafted_junk_count += count;
    }
    projection.prefix_count = carrier.prefix_count;
    projection.suffix_count = carrier.suffix_count;
    projection.missing_prefix_goals = static_cast<std::uint8_t>(
        std::popcount(projection.missing_goal_mask & prefix_goal_mask));
    projection.missing_suffix_goals = static_cast<std::uint8_t>(
        std::popcount(projection.missing_goal_mask & suffix_goal_mask));
    projection.prefix_capacity = carrier.prefix_count < affix_cap
        ? static_cast<std::uint8_t>(affix_cap - carrier.prefix_count)
        : 0;
    projection.suffix_capacity = carrier.suffix_count < affix_cap
        ? static_cast<std::uint8_t>(affix_cap - carrier.suffix_count)
        : 0;
    projection.rarity = carrier.rarity;
    projection.influence_bits = carrier.influence_bits;
    projection.veiled_side = carrier.veiled_side;
    projection.searing_exarch_tier = carrier.searing_exarch_tier;
    projection.eater_of_worlds_tier = carrier.eater_of_worlds_tier;
    if ((carrier.blocked_mask & projection.missing_goal_mask) != 0) {
        projection.debt_flags |= kExecutableDebtBlockedGoal;
    }
    if (projection.missing_prefix_goals > projection.prefix_capacity) {
        projection.debt_flags |= kExecutableDebtPrefixCapacity;
    }
    if (projection.missing_suffix_goals > projection.suffix_capacity) {
        projection.debt_flags |= kExecutableDebtSuffixCapacity;
    }
    if (projection.fractured_junk_count != 0) {
        projection.debt_flags |= kExecutableDebtFracturedJunk;
    }
    if (std::popcount(projection.satisfied_goal_mask) >=
            required_goal_count &&
        projection.junk_count != 0) {
        projection.debt_flags |= kExecutableDebtTerminalJunk;
    }
    return projection;
}

static_assert(
    !std::is_convertible_v<ExecutableCarrierProjection, double>);
static_assert(
    !std::is_convertible_v<ExecutableCarrierActionProjection, double>);

} // namespace solve_detail
} // namespace solver
} // namespace poecraft
