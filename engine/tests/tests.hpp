#ifndef POECRAFT_TESTS_HPP
#define POECRAFT_TESTS_HPP

#include <cstdio>

namespace pctest {

extern int g_checks;
extern int g_failures;

inline void check(bool condition, const char* file, int line, const char* expr) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL %s:%d: %s\n", file, line, expr);
    }
}

} // namespace pctest

#define PC_CHECK(cond) ::pctest::check((cond), __FILE__, __LINE__, #cond)

/* Phase 4 test suites. */
void run_bitset_tests();
void run_rng_tests();
void run_item_state_tests();
void run_blocking_tests();
void run_data_loader_tests(const char* artifact_dir);
void run_session_builder_tests(const char* artifact_dir,
                               const char* fixtures_dir);
void run_action_tests(const char* artifact_dir);
void run_bestiary_tests();
void run_simulator_tests(const char* artifact_dir);

/* Solver phase S1-S5 suites. */
void run_solver_abstract_tests(const char* artifact_dir);
void run_solver_calc_tests(const char* artifact_dir);
void run_solver_solve_tests(const char* artifact_dir);
void run_solver_compile_tests(const char* artifact_dir);
void run_solver_eval_tests(const char* artifact_dir);
void run_solver_api_tests(const char* artifact_dir);
void run_solver_s8_3_tests();

#endif
