#ifndef POECRAFT_SOLVER_DIAGNOSTIC_OPTIONS_HPP
#define POECRAFT_SOLVER_DIAGNOSTIC_OPTIONS_HPP

#include <cstdint>

namespace poecraft {
namespace solver {

/*
 * Native-benchmark-only opt-ins occupy the high end of pc_solve_options'
 * solver_flags word. They are deliberately absent from the public C header,
 * WASM parser, and product protocol.
 */
inline constexpr std::uint32_t kHighImpactExecutableUppersDiagnosticFlag =
    1u << 31;
inline constexpr std::uint32_t kProjectedReforgeFrontierDiagnosticFlag =
    1u << 30;
inline constexpr std::uint32_t kFactoredTerminalReforgeDiagnosticFlag =
    1u << 29;
inline constexpr std::uint32_t kDisableReforgeResourceAccountingDiagnosticFlag =
    1u << 28;

} // namespace solver
} // namespace poecraft

#endif
