#include "tests.hpp"

#include <filesystem>
#include <string>

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
     * scripts/test.ps1 or CTest; when absent those suites skip so a bare
     * invocation still exercises the pure units. The artifact is a generated,
     * gitignored build product, so a path may be passed for a directory that
     * was never compiled (fresh clone, CI): treat a missing manifest the same
     * as no path at all instead of failing the data suites. */
    const char* artifact_dir = argc > 1 ? argv[1] : nullptr;
    const char* fixtures_dir = argc > 2 ? argv[2] : nullptr;
    if (artifact_dir != nullptr) {
        const std::filesystem::path manifest =
            std::filesystem::path(artifact_dir) / "manifest.json";
        std::error_code probe_error;
        if (!std::filesystem::exists(manifest, probe_error) || probe_error) {
            std::printf("data suites skipped (no compiled artifact at %s)\n",
                        artifact_dir);
            artifact_dir = nullptr;
            fixtures_dir = nullptr;
        }
    }
    run_data_loader_tests(artifact_dir);
    run_session_builder_tests(artifact_dir, fixtures_dir);
    run_action_tests(artifact_dir);
    run_simulator_tests(artifact_dir);

    std::printf("engine tests: %d checks, %d failures\n", pctest::g_checks,
                pctest::g_failures);
    return pctest::g_failures == 0 ? 0 : 1;
}
