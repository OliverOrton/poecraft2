namespace solver = poecraft::solver;
struct pc_solver {
    std::shared_ptr<const poecraft::SessionImpl> session;
    std::unique_ptr<solver::CalcContext> calc;
    /* Product solving may use a deliberately coarse parent abstraction.
     * Calculator queries remain strict and lazily allocate their own exact
     * context so primitive semantics and outcome identities do not change. */
    std::unique_ptr<solver::CalcContext> exact_calc;
    std::unique_ptr<solver::SolveWork> solve_work;
    std::optional<solver::SolveResult> solved;
    std::optional<std::uint64_t> registry_generation_ns;
    std::optional<solver::PolicyCompilationTelemetry> compilation;
    std::string compiled_strategy; /* scratch for the buffer queries */
    std::string solve_log;
    std::string abandoned_telemetry;
    bool abandoned_telemetry_capped = false;
    std::uint64_t abandoned_telemetry_limit = 0;
    std::uint64_t peak_owned_bytes = 0;
    std::optional<solver::CarrierLadderExactBoundaryDiagnosticConfig>
        carrier_ladder_exact_boundary_diagnostic;
};
