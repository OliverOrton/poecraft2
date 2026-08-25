#include "tests.hpp"

#include <string>

namespace pctest {
int g_checks = 0;
int g_failures = 0;
} // namespace pctest

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--core-only") {
        const char* artifact_dir = argc > 2 ? argv[2] : nullptr;
        const char* fixtures_dir = argc > 3 ? argv[3] : nullptr;
        run_bitset_tests();
        run_rng_tests();
        run_item_state_tests();
        run_blocking_tests();
        run_data_loader_tests(artifact_dir);
        run_session_builder_tests(artifact_dir, fixtures_dir);
        run_action_tests(artifact_dir);
        run_bestiary_tests();
        run_simulator_tests(artifact_dir);
        std::printf("engine core tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--bestiary-only") {
        run_bestiary_tests();
        std::printf("bestiary tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 2 && std::string(argv[1]) == "--solver-compile-only") {
        run_solver_compile_tests(argv[2]);
        std::printf("solver compile tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 2 && std::string(argv[1]) == "--solver-imprint-only") {
        run_solver_imprint_tests(argv[2]);
        std::printf("solver Imprint tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--solver-s8-3-only") {
        run_solver_s8_3_tests();
        std::printf("solver S8.3 tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) == "--solver-automatic-veiled-only") {
        run_solver_automatic_veiled_tests();
        std::printf("solver automatic Veiled tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--solver-eval-only") {
        run_solver_eval_tests(argc > 2 ? argv[2] : nullptr);
        std::printf("solver evaluator tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--solver-solve-only") {
        run_solver_solve_tests(argc > 2 ? argv[2] : nullptr);
        std::printf("solver solve tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) == "--solver-carrier-bounds-only") {
        run_solver_carrier_bound_tests();
        std::printf(
            "solver carrier-bound tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) == "--solver-automatic-eldritch-only") {
        run_solver_automatic_eldritch_tests();
        std::printf(
            "solver automatic Eldritch tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) == "--solver-policy-refinement-only") {
        run_solver_policy_refinement_tests();
        std::printf(
            "solver policy-refinement tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--solver-calc-only") {
        run_solver_calc_tests(argc > 2 ? argv[2] : nullptr);
        std::printf("solver calc tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) == "--solver-family-contract-only") {
        run_solver_action_family_contract_tests(
            argc > 2 ? argv[2] : nullptr);
        std::printf(
            "solver family-contract tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) ==
            "--solver-calc-gated-equivalence-only") {
        run_solver_calc_gated_equivalence_tests();
        std::printf(
            "solver calc gated equivalence tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--solver-abstract-only") {
        run_solver_abstract_tests(argc > 2 ? argv[2] : nullptr);
        std::printf("solver abstract tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 && std::string(argv[1]) == "--solver-refinement-only") {
        run_solver_refinement_tests();
        std::printf("solver refinement tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 1 &&
        std::string(argv[1]) == "--solver-quotient-proof-only") {
        run_solver_quotient_proof_tests();
        run_solver_quotient_partition_tests();
        run_solver_quotient_bellman_tests();
        std::printf(
            "solver quotient-proof tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 2 && std::string(argv[1]) == "--solver-api-only") {
        run_solver_api_tests(argv[2]);
        std::printf("solver API tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 2 && std::string(argv[1]) ==
                        "--solver-feasibility-only") {
        run_solver_feasibility_tests(argv[2]);
        std::printf("solver feasibility tests: %d checks, %d failures\n",
                    pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    if (argc > 3 && std::string(argv[1]) ==
                        "--session-builder-only") {
        run_session_builder_tests(argv[2], argv[3]);
        std::printf(
            "session-builder tests: %d checks, %d failures\n",
            pctest::g_checks, pctest::g_failures);
        return pctest::g_failures == 0 ? 0 : 1;
    }
    run_bitset_tests();
    run_rng_tests();
    run_item_state_tests();
    run_blocking_tests();

    /* The data-loader and session-builder suites need the compiled artifact
     * directory (argv[1]) and the spec fixtures directory (argv[2]), passed by
     * scripts/test.ps1; when absent those suites skip so a bare invocation
     * still exercises the pure units. */
    const char* artifact_dir = argc > 1 ? argv[1] : nullptr;
    const char* fixtures_dir = argc > 2 ? argv[2] : nullptr;
    run_data_loader_tests(artifact_dir);
    run_session_builder_tests(artifact_dir, fixtures_dir);
    run_action_tests(artifact_dir);
    run_bestiary_tests();
    run_simulator_tests(artifact_dir);
    run_solver_abstract_tests(artifact_dir);
    run_solver_calc_tests(artifact_dir);
    run_solver_solve_tests(artifact_dir);
    run_solver_compile_tests(artifact_dir);
    run_solver_eval_tests(artifact_dir);
    run_solver_api_tests(artifact_dir);
    run_solver_s8_3_tests();
    run_solver_refinement_tests();
    run_solver_quotient_proof_tests();
    run_solver_quotient_partition_tests();
    run_solver_quotient_bellman_tests();

    std::printf("engine tests: %d checks, %d failures\n", pctest::g_checks,
                pctest::g_failures);
    return pctest::g_failures == 0 ? 0 : 1;
}
