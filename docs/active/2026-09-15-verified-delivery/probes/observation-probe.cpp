// B's read-only saved-controller diagnostic. Calls the existing native model
// and observation fixed-point owners; builds no action row and changes no layout.
#include "solver_eval_helpers.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace poecraft;
using namespace poecraft::solver;
std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(path);
    return {std::istreambuf_iterator<char>(f), {}};
}
void requirement(std::ostream& o, const ObservationRequirement& r) {
    o << "{\"item_features\":" << r.item_features << ",\"affixes\":[";
    bool first = true;
    for (const auto& a : r.affix_observations) {
        if (!first) o << ',';
        first = false;
        o << "{\"features\":" << a.features
          << ",\"required_affix_traits\":" << a.selector.required_affix_traits
          << ",\"forbidden_affix_traits\":" << a.selector.forbidden_affix_traits
          << ",\"required_item_traits\":" << unsigned(a.selector.required_item_traits)
          << ",\"forbidden_item_traits\":" << unsigned(a.selector.forbidden_item_traits)
          << ",\"tag_count\":" << a.selector.required_tag_ids.size() << '}';
    }
    o << "],\"tag_count\":" << r.modifier_tag_ids.size() << '}';
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
        std::cout << '[';
        for (int arg = 1; arg < argc; ++arg) {
            if (arg != 1) std::cout << ',';
            const auto started = std::chrono::steady_clock::now();
            const auto text = read_file(argv[arg]);
            auto graph = compile_strategy_json(session, text.data(), text.size());
            auto model = derive_checked_model(graph, 200000, true);
            const auto model_done = std::chrono::steady_clock::now();
            StrategyEvalResult::ObservationPropagationTelemetry telemetry;
            auto requirements = derive_node_observation_requirements(*graph, model, 100000,
                &telemetry, [&](std::uint64_t bytes, const char*, std::uint64_t, std::uint64_t) {
                    if (bytes > (4ull << 30)) throw std::runtime_error("original checker memory cap");
                });
            const auto done = std::chrono::steady_clock::now();
            ObservationRequirement united;
            for (const auto& r : requirements)
                united = refinement::merge_observation_requirements(std::move(united), r);
            const auto& layout = model.calc->layout();
            std::cout << "{\"path\":" << std::quoted(argv[arg])
              << ",\"nodes\":" << graph->nodes.size()
              << ",\"goal_slots\":" << layout.slots.size()
              << ",\"junk_classes\":" << layout.junk_classes.size()
              << ",\"count_membership_observations\":" << layout.count_observations.size()
              << ",\"discriminating_tags\":" << layout.discriminating_tag_ids.size()
              << ",\"rounds\":" << telemetry.rounds
              << ",\"unique_requirements\":" << telemetry.unique_canonical_requirements
              << ",\"observation_peak_bytes\":" << telemetry.actual_peak_bytes
              << ",\"observation_retained_bytes\":" << telemetry.retained_bytes
              << ",\"model_ms\":" << std::chrono::duration<double, std::milli>(model_done-started).count()
              << ",\"fixed_point_ms\":" << std::chrono::duration<double, std::milli>(done-model_done).count()
              << ",\"union\":";
            requirement(std::cout, united);
            std::cout << ",\"per_node\":[";
            for (std::size_t i = 0; i < requirements.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << "{\"id\":" << std::quoted(graph->nodes[i].id) << ",\"required\":";
                requirement(std::cout, requirements[i]);
                std::cout << '}';
            }
            std::cout << "]}";
        }
        std::cout << "]\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
