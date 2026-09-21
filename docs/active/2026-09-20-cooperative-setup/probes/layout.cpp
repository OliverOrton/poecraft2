// Compile with the native/wasm target's normal headers and ABI flags.
// Static layout only; dynamic coroutine frames are measured by their owners.
#include "solver_solve_types.hpp"
#include "poecraft/solver.h"
#include <cstdio>

namespace poecraft::solver {
struct SolveWorkTestAccess {
    using Impl = SolveWork::Impl;
    static void print() {
        std::printf("{\"impl_bytes\":%zu,\"portfolio_bytes\":%zu,"
                    "\"candidate_bytes\":%zu,\"public_progress_bytes\":%zu",
            sizeof(Impl), sizeof(Impl::IncumbentPortfolio),
            sizeof(Impl::BoundedPolicyIncumbent), sizeof(pc_solve_progress));
#ifndef POECRAFT_BASELINE_LAYOUT
        std::printf(",\"setup_domain_bytes\":%zu,"
                    "\"setup_task_shell_bytes\":%zu,\"frame_header_bytes\":%zu,"
                    "\"static_layout_only\":true",
            sizeof(solve_detail::SetupStorage),
            sizeof(std::optional<solve_detail::CooperativeTask<bool>>),
            sizeof(solve_detail::CooperativeTask<bool>::promise_type::AllocationHeader));
#endif
        std::printf("}\n");
    }
};
}
int main() { poecraft::solver::SolveWorkTestAccess::print(); }
