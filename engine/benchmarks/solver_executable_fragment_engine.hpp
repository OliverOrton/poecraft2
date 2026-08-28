#pragma once

#include "solver_executable_fragment.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace poecraft {
namespace solver {
namespace fragment_v1 {

inline constexpr const char* kCleanOneGoalRenewalCaseV1 =
    "clean_one_goal_transmute_scour_renewal_v1";
inline constexpr const char* kCleanOneGoalRenewalBaseV1 =
    "Metadata/Items/Armours/BodyArmours/BodyInt17";
inline constexpr const char* kCleanOneGoalRenewalGoalModV1 =
    "LocalIncreasedEnergyShield11";

struct EngineBackedRenewalFixtureV1 {
    ExecutableFragmentIRV1 ir;
    LeafVerificationContextV1 context;
    std::shared_ptr<const ExactPrimitiveOracleV1> oracle;
    double exact_terminal_probability = 0.0;
    double exact_goal_plus_junk_probability = 0.0;
    double exact_other_nonterminal_probability = 0.0;
    std::uint64_t transmute_physical_outcomes = 0;
    std::string base_identity;
    std::string goal_identity;
    std::string forward_reference_identity;
};

struct EngineBackedRenewalBuildResultV1 {
    std::optional<EngineBackedRenewalFixtureV1> fixture;
    std::string refusal;

    bool ok() const { return fixture.has_value(); }
};

EngineBackedRenewalBuildResultV1
build_clean_one_goal_transmute_scour_renewal_v1(
    const std::string& compiled_artifact_directory);

} // namespace fragment_v1
} // namespace solver
} // namespace poecraft
