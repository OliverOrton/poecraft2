#include "tests.hpp"

namespace pctest {
int g_checks = 0;
int g_failures = 0;
} // namespace pctest

int main(int argc, char** argv) {
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
    run_simulator_tests(artifact_dir);

    std::printf("engine tests: %d checks, %d failures\n", pctest::g_checks,
                pctest::g_failures);
    return pctest::g_failures == 0 ? 0 : 1;
}
