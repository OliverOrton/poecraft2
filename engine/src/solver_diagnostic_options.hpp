#ifndef POECRAFT_SOLVER_DIAGNOSTIC_OPTIONS_HPP
#define POECRAFT_SOLVER_DIAGNOSTIC_OPTIONS_HPP

#include <cstdint>

#include "poecraft/solver.h"
#include "solver_model.hpp"

namespace poecraft {
namespace solver {

/*
 * Native-only experiments occupy the high end of pc_solve_options'
 * solver_flags word and remain absent from the public C header. The
 * historical high-impact bit remains accepted as a compatibility alias; new
 * product callers select that durable capability through the public
 * Calculator profile or PC_SOLVER_FLAG_HIGH_IMPACT_EXECUTABLE_UPPERS.
 */
inline constexpr std::uint32_t kHighImpactExecutableUppersDiagnosticFlag =
    1u << 31;
inline constexpr std::uint32_t kProjectedReforgeFrontierDiagnosticFlag =
    1u << 30;
inline constexpr std::uint32_t kFactoredTerminalReforgeDiagnosticFlag =
    1u << 29;
inline constexpr std::uint32_t kDisableReforgeResourceAccountingDiagnosticFlag =
    1u << 28;
inline constexpr std::uint32_t kRawStrictReforgeOracleDiagnosticFlag =
    1u << 27;

/* Private benchmark construction hook. It retains the ordinary product
 * primitive registry and goal abstraction while allowing a finite subset of
 * generated AutomaticCandidateKind values. It is deliberately absent from
 * the public C header and every binding. */
pc_result create_solver_with_automatic_candidate_diagnostic(
    pc_session_handle session,
    const char* goal_json,
    std::size_t goal_json_size,
    std::uint32_t automatic_candidate_kind_mask,
    pc_solver_handle* out_solver,
    pc_error_info* out_error);

} // namespace solver
} // namespace poecraft

#endif
