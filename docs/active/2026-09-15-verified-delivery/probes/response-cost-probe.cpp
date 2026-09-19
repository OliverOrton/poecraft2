// B's second and final diagnostic: cold-check two saved related controllers
// with original evaluator limits. No response cache or matrix inverse is built.
#include "solver_eval_types.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace poecraft;
using namespace poecraft::solver;
std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(path);
    return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char** argv) {
    try {
        auto data = load_data_impl(read_file("data/compiled/current/manifest.json"),
            read_file("data/compiled/current/strings.json"), read_file("data/compiled/current/game-data.json"));
        auto session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = data->base_by_path.at("Metadata/Items/Rings/Ring10");
        session->item_level = 86;
        build_session(*session);
        const auto economy_text = read_file("out/verified-delivery/B-economy.json");
        StrategyEvalOptions options;
        options.economy = load_economy_json(economy_text.data(), economy_text.size());
        options.max_states = 2000000;
        options.max_pairs = 10000000;
        options.max_transitions = 40000000;
        options.max_owned_bytes = 4ull << 30;
        options.max_reforge_work = 400000000;
        std::cout << std::setprecision(17) << '[';
        for (int arg = 1; arg < argc; ++arg) {
            if (arg != 1) std::cout << ',';
            const auto started = std::chrono::steady_clock::now();
            const auto text = read_file(argv[arg]);
            auto graph = compile_strategy_json(session, text.data(), text.size());
            const auto result = evaluate_strategy(*graph, options);
            const auto& t = result.stage_timings;
            if (!result.converged || !result.cost_complete || result.success_probability < 1-1e-10)
                throw std::runtime_error("incomplete original-root check");
            std::cout << "{\"path\":" << std::quoted(argv[arg])
              << ",\"cost\":" << result.total_expected_cost
              << ",\"expected_primitive_actions\":" << result.expected_actions
              << ",\"success_probability\":" << result.success_probability
              << ",\"raw_pairs\":" << result.raw_pairs_discovered
              << ",\"refined_pairs\":" << result.refined_pairs
              << ",\"peak_bytes\":" << result.peak_owned_bytes_estimate
              << ",\"retained_output_bytes\":" << result.retained_output_owned_bytes_estimate
              << ",\"logical_work\":" << result.reforge_logical_work_v1
              << ",\"wall_ms\":" << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-started).count()
              << ",\"model_ms\":" << t.model_setup_ns/1e6
              << ",\"observations_ms\":" << t.observation_preparation_ns/1e6
              << ",\"discovery_ms\":" << t.pair_discovery_ns/1e6
              << ",\"refinement_ms\":" << t.pair_refinement_ns/1e6
              << ",\"components_ms\":" << t.component_construction_ns/1e6
              << ",\"solve_ms\":" << t.component_solve_ns/1e6
              << ",\"continuation_ms\":" << t.continuation_solve_ns/1e6 << '}';
        }
        std::cout << "]\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
