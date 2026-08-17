#include "solver_policy_refinement_helpers.hpp"
#include "solver_compile_contracts.hpp"

#include <chrono>
#include <string_view>

namespace poecraft {
namespace solver {
namespace refinement {

namespace {

bool policy_graphs_differ_only_at_bounded_defaults(
        const std::string& product,
        const PolicyCompilationTelemetry& product_compilation,
        const std::string& certification,
        const PolicyCompilationTelemetry& certification_compilation) {
    if (product_compilation.policy_route_default_mode !=
            "product_safe_restart" ||
        certification_compilation.policy_route_default_mode !=
            "certification_fail_closed" ||
        product_compilation.policy_route_default_edges !=
            certification_compilation.policy_route_default_edges ||
        certification_compilation.policy_route_offpolicy_default_edges !=
            certification_compilation.policy_route_default_edges ||
        product_compilation.nodes != certification_compilation.nodes ||
        product_compilation.edges != certification_compilation.edges) {
        return false;
    }

    const auto normalized = [](const std::string& graph) {
        std::string value = graph;
        constexpr std::string_view default_suffix =
            ",\"is_default\":true}";
        constexpr std::string_view edge_prefix = "{\"id\":\"e";
        constexpr std::string_view from_prefix =
            "\"from\":\"policy_route_";
        constexpr std::string_view to_prefix = "\"to\":\"";
        std::vector<std::pair<std::size_t, std::size_t>> targets;
        std::size_t offset = 0;
        while ((offset = value.find(default_suffix, offset)) !=
               std::string::npos) {
            const std::size_t edge = value.rfind(edge_prefix, offset);
            const std::size_t from = value.find(from_prefix, edge);
            if (edge != std::string::npos &&
                from != std::string::npos && from < offset) {
                const std::size_t to = value.find(to_prefix, from);
                if (to != std::string::npos && to < offset) {
                    const std::size_t target = to + to_prefix.size();
                    const std::size_t target_end = value.find('\"', target);
                    if (target_end != std::string::npos &&
                        target_end < offset) {
                        targets.emplace_back(
                            target, target_end - target);
                    }
                }
            }
            offset += default_suffix.size();
        }
        for (auto target = targets.rbegin(); target != targets.rend();
             ++target) {
            value.replace(target->first, target->second, "offpolicy");
        }
        return std::pair<std::string, std::size_t>{
            std::move(value), targets.size()};
    };
    auto normalized_product = normalized(product);
    auto normalized_certification = normalized(certification);
    return normalized_product.second ==
            product_compilation.policy_route_default_edges &&
        normalized_certification.second ==
            certification_compilation.policy_route_default_edges &&
        normalized_product.first == normalized_certification.first;
}

} // namespace

struct CompiledPolicyAssertionWork::Impl {
    enum class Stage : std::uint8_t {
        Compiling,
        Evaluating,
        Done,
    };

    CalcContext& coarse;
    const SolveResult& solved;
    const std::unordered_map<std::string, double>& prices;
    SolveOptions options;
    std::string strategy_name;
    const RefinedPolicyCompileRouting* refined_routing = nullptr;
    const std::string* emitted_strategy_json = nullptr;
    const PolicyCompilationTelemetry* emitted_compilation = nullptr;
    CompiledPolicyAssertion result;
    Stage stage = Stage::Compiling;
    std::shared_ptr<StrategyImpl> parsed_strategy;
    std::shared_ptr<EconomyImpl> economy;
    std::unique_ptr<StrategyEvalWork> evaluation_work;
    std::chrono::steady_clock::time_point evaluation_started{};

    Impl(
            CalcContext& coarse_value,
            const SolveResult& solved_value,
            const std::unordered_map<std::string, double>& prices_value,
            const SolveOptions& options_value,
            std::string strategy_name_value,
            const RefinedPolicyCompileRouting* routing,
            const std::string* emitted_json,
            const PolicyCompilationTelemetry* emitted_telemetry)
        : coarse(coarse_value),
          solved(solved_value),
          prices(prices_value),
          options(options_value),
          strategy_name(std::move(strategy_name_value)),
          refined_routing(routing),
          emitted_strategy_json(emitted_json),
          emitted_compilation(emitted_telemetry) {
        result.solver_cost = solved.evaluated_policy_cost;
    }

    void finish_failure(
            const CompiledPolicyAssertionStatus status,
            std::string reason,
            std::string cap = {}) {
        result.status = status;
        result.failure_reason = std::move(reason);
        result.resource_cap = std::move(cap);
        result.failure_classification =
            compiled_policy_failure_classification(result);
        stage = Stage::Done;
    }

    void record_evaluation_time() {
        result.exact_evaluation_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - evaluation_started)
                .count());
    }

    std::string failure_with_graph_context(const char* message) const {
        return std::string{message} +
            "; compilation_nodes=" +
            std::to_string(result.compilation.nodes) +
            ", compilation_edges=" +
            std::to_string(result.compilation.edges) +
            ", policy_regions=" +
            std::to_string(result.compilation.policy_regions) +
            ", infrastructure_nodes=" +
            std::to_string(result.compilation.infrastructure_nodes) +
            ", policy_route_nodes=" +
            std::to_string(result.compilation.policy_route_nodes) +
            ", local_gated_route_nodes=" +
            std::to_string(result.compilation.local_gated_route_nodes) +
            ", primitive_region_nodes=" +
            std::to_string(result.compilation.primitive_region_nodes) +
            ", additional_recipe_nodes=" +
            std::to_string(result.compilation.additional_recipe_nodes) +
            ", reachable_states=" +
            std::to_string(result.evaluation.raw_pairs_discovered) +
            ", state_action_pairs=" +
            std::to_string(result.evaluation.refined_pairs) +
            ", evaluator_owned=" +
            std::to_string(result.evaluation.owned_bytes_estimate) +
            ", evaluator_peak=" +
            std::to_string(result.evaluation.peak_owned_bytes_estimate) +
            ", evaluator_budget=" +
            std::to_string(result.evaluator_memory_budget);
    }

    void compile_and_prepare() {
        if (!solved.policy_available) {
            finish_failure(
                CompiledPolicyAssertionStatus::NoPolicy,
                "solve did not publish a policy");
            return;
        }
        result.retained_solver_bytes =
            estimated_retained_solver_bytes(coarse, &solved);
        result.publication_peak_owned_bytes =
            result.retained_solver_bytes;
        if (result.retained_solver_bytes >=
            options.max_solver_owned_bytes) {
            finish_failure(
                CompiledPolicyAssertionStatus::ResourceCap,
                "retained solver state leaves no memory for exact policy "
                "compilation",
                "max_solver_owned_bytes");
            return;
        }
        const std::uint64_t compilation_memory =
            options.max_solver_owned_bytes -
            result.retained_solver_bytes;
        if (compilation_memory <= 1) {
            finish_failure(
                CompiledPolicyAssertionStatus::ResourceCap,
                "retained solver state leaves no memory for exact policy "
                "JSON",
                "max_solver_owned_bytes");
            return;
        }
        const std::uint64_t compilation_json_limit =
            std::min<std::uint64_t>(
                options.max_strategy_json_bytes,
                compilation_memory - 1);
        const bool json_limited_by_memory =
            compilation_json_limit < options.max_strategy_json_bytes;
        if (emitted_strategy_json != nullptr) {
            if (emitted_strategy_json->empty()) {
                finish_failure(
                    CompiledPolicyAssertionStatus::CompilationFailure,
                    "precompiled policy assertion received empty JSON");
                return;
            }
            if (emitted_strategy_json->size() > compilation_json_limit) {
                const std::string cap = json_limited_by_memory
                    ? "max_solver_owned_bytes"
                    : "max_strategy_json_bytes";
                finish_failure(
                    CompiledPolicyAssertionStatus::ResourceCap,
                    "precompiled policy reached " + cap,
                    cap);
                return;
            }
            result.strategy_json = *emitted_strategy_json;
            if (emitted_compilation != nullptr) {
                result.compilation = *emitted_compilation;
            }
            std::uint64_t live = result.retained_solver_bytes;
            saturating_add(
                live,
                std::max<std::uint64_t>(
                    result.compilation.peak_owned_bytes,
                    result.strategy_json.capacity() + 1));
            result.publication_peak_owned_bytes = std::max(
                result.publication_peak_owned_bytes, live);
        } else {
            const auto compilation_started =
                std::chrono::steady_clock::now();
            const auto record_compilation_time = [&] {
                result.compilation_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        compilation_started)
                        .count());
            };
            try {
                result.strategy_json = compile_policy_strategy_json(
                    coarse, solved, strategy_name, &result.compilation,
                    compilation_json_limit, refined_routing,
                    compilation_memory,
                    PolicyRouteDefaultMode::CertificationFailClosed);
                record_compilation_time();
                std::uint64_t live = result.retained_solver_bytes;
                saturating_add(
                    live,
                    std::max<std::uint64_t>(
                        result.compilation.peak_owned_bytes,
                        result.strategy_json.capacity() + 1));
                result.publication_peak_owned_bytes = std::max(
                    result.publication_peak_owned_bytes, live);
                if (!result.compilation.cap_hit.empty()) {
                    const std::string cap =
                        json_limited_by_memory &&
                                result.compilation.cap_hit ==
                                    "max_strategy_json_bytes"
                            ? "max_solver_owned_bytes"
                            : result.compilation.cap_hit;
                    finish_failure(
                        CompiledPolicyAssertionStatus::ResourceCap,
                        "compiled policy reached " + cap,
                        cap);
                    return;
                }
            } catch (const SolverResourceLimit& error) {
                record_compilation_time();
                std::uint64_t live = result.retained_solver_bytes;
                saturating_add(
                    live, result.compilation.peak_owned_bytes);
                result.publication_peak_owned_bytes = std::max(
                    result.publication_peak_owned_bytes, live);
                finish_failure(
                    CompiledPolicyAssertionStatus::ResourceCap,
                    error.what(), error.cap_name());
                return;
            } catch (const std::length_error& error) {
                record_compilation_time();
                finish_failure(
                    CompiledPolicyAssertionStatus::ResourceCap,
                    error.what(), resource_cap_from_message(error.what()));
                return;
            } catch (const std::exception& error) {
                record_compilation_time();
                const bool capped = !result.compilation.cap_hit.empty();
                const std::string cap =
                    json_limited_by_memory &&
                            result.compilation.cap_hit ==
                                "max_strategy_json_bytes"
                        ? "max_solver_owned_bytes"
                        : result.compilation.cap_hit;
                finish_failure(
                    capped
                        ? CompiledPolicyAssertionStatus::ResourceCap
                        : CompiledPolicyAssertionStatus::CompilationFailure,
                    error.what(), cap);
                return;
            }
        }

        evaluation_started = std::chrono::steady_clock::now();
        try {
            const std::uint64_t strategy_json_bytes =
                result.strategy_json.capacity() + 1;
            std::uint64_t remaining_memory =
                options.max_solver_owned_bytes -
                result.retained_solver_bytes;
            const auto consume_memory = [&]
                    (const std::uint64_t bytes, const char* subject) {
                if (bytes >= remaining_memory) {
                    finish_failure(
                        CompiledPolicyAssertionStatus::ResourceCap,
                        std::string{"no remaining solver memory for "} +
                            subject,
                        "max_solver_owned_bytes");
                    return false;
                }
                remaining_memory -= bytes;
                return true;
            };
            if (!consume_memory(
                    strategy_json_bytes,
                    "parsed exact policy evaluation")) {
                record_evaluation_time();
                return;
            }
            const std::shared_ptr<const SessionImpl> session =
                borrow_session(coarse);
            parsed_strategy = compile_strategy_json(
                session,
                result.strategy_json.data(),
                result.strategy_json.size());
            result.parsed_strategy_bytes =
                strategy_impl_owned_bytes(*parsed_strategy);
            {
                std::uint64_t live = result.retained_solver_bytes;
                saturating_add(live, strategy_json_bytes);
                saturating_add(live, result.parsed_strategy_bytes);
                result.publication_peak_owned_bytes = std::max(
                    result.publication_peak_owned_bytes, live);
            }
            if (!consume_memory(
                    result.parsed_strategy_bytes,
                    "the parsed exact strategy")) {
                record_evaluation_time();
                return;
            }
            economy = std::make_shared<EconomyImpl>();
            economy->id = "policy-guided-exact-refinement";
            economy->prices = prices;
            result.economy_bytes = economy_owned_bytes(
                economy->prices, economy->id.capacity());
            {
                std::uint64_t live = result.retained_solver_bytes;
                saturating_add(live, strategy_json_bytes);
                saturating_add(live, result.parsed_strategy_bytes);
                saturating_add(live, result.economy_bytes);
                result.publication_peak_owned_bytes = std::max(
                    result.publication_peak_owned_bytes, live);
            }
            if (!consume_memory(
                    result.economy_bytes,
                    "the exact-evaluation economy")) {
                record_evaluation_time();
                return;
            }

            result.evaluator_memory_budget = remaining_memory;
            StrategyEvalOptions evaluation_options;
            evaluation_options.epsilon = 1e-12;
            evaluation_options.max_sweeps =
                std::max<std::uint32_t>(1, options.max_sweeps);
            evaluation_options.max_states =
                std::max<std::uint32_t>(
                    1, options.max_discovered_states);
            evaluation_options.max_pairs = std::max<std::uint32_t>(
                1, bounded_u32(options.max_state_action_rows));
            evaluation_options.max_transitions =
                std::max<std::uint32_t>(
                    1, bounded_u32(options.max_transitions));
            evaluation_options.max_owned_bytes =
                result.evaluator_memory_budget;
            evaluation_options.max_output_json_bytes =
                options.max_strategy_json_bytes;
            evaluation_options.max_reforge_work =
                options.max_reforge_work;
            evaluation_options.economy = economy;
            evaluation_work = std::make_unique<StrategyEvalWork>(
                parsed_strategy, evaluation_options);
            stage = Stage::Evaluating;
        } catch (const SolverResourceLimit& error) {
            record_evaluation_time();
            finish_failure(
                CompiledPolicyAssertionStatus::ResourceCap,
                failure_with_graph_context(error.what()),
                error.cap_name());
        } catch (const std::length_error& error) {
            record_evaluation_time();
            finish_failure(
                CompiledPolicyAssertionStatus::ResourceCap,
                failure_with_graph_context(error.what()),
                resource_cap_from_message(error.what()));
        } catch (const std::exception& error) {
            record_evaluation_time();
            finish_failure(
                CompiledPolicyAssertionStatus::ExactEvaluationFailure,
                failure_with_graph_context(error.what()));
        }
    }

    void advance_evaluation(const std::uint32_t max_work_items) {
        try {
            evaluation_work->step(max_work_items);
            if (!evaluation_work->progress().done) return;
            result.evaluation = evaluation_work->take_result();
            record_evaluation_time();
            std::uint64_t live = result.retained_solver_bytes;
            saturating_add(live, result.strategy_json.capacity() + 1);
            saturating_add(live, result.parsed_strategy_bytes);
            saturating_add(live, result.economy_bytes);
            saturating_add(
                live,
                result.evaluation.peak_owned_bytes_estimate);
            result.publication_peak_owned_bytes = std::max(
                result.publication_peak_owned_bytes, live);
            finalize_compiled_policy_assertion(result);
            if (result.executable && result.proper &&
                result.zero_off_policy &&
                result.evaluation.cost_complete &&
                std::isfinite(result.exact_cost)) {
                result.certification_strategy_json =
                    std::move(result.strategy_json);
                result.certification_compilation =
                    result.compilation;
                result.compilation = {};
                evaluation_work.reset();
                parsed_strategy.reset();
                economy.reset();
                const std::uint64_t certification_bytes =
                    result.certification_strategy_json.capacity() + 1;
                std::uint64_t paired_retained =
                    result.retained_solver_bytes;
                saturating_add(paired_retained, certification_bytes);
                saturating_add(
                    paired_retained,
                    result.evaluation
                        .retained_output_owned_bytes_estimate);
                if (paired_retained >=
                    options.max_solver_owned_bytes) {
                    finish_failure(
                        CompiledPolicyAssertionStatus::ResourceCap,
                        "certification artifact leaves no memory for the "
                        "paired product graph",
                        "max_solver_owned_bytes");
                    return;
                }
                try {
                    result.strategy_json = compile_policy_strategy_json(
                        coarse, solved, strategy_name,
                        &result.compilation,
                        options.max_strategy_json_bytes,
                        refined_routing,
                        options.max_solver_owned_bytes - paired_retained,
                        PolicyRouteDefaultMode::ProductSafeRestart);
                } catch (const SolverResourceLimit& error) {
                    finish_failure(
                        CompiledPolicyAssertionStatus::ResourceCap,
                        error.what(), error.cap_name());
                    return;
                } catch (const std::length_error& error) {
                    finish_failure(
                        CompiledPolicyAssertionStatus::ResourceCap,
                        error.what(),
                        resource_cap_from_message(error.what()));
                    return;
                } catch (const std::exception& error) {
                    finish_failure(
                        CompiledPolicyAssertionStatus::CompilationFailure,
                        error.what());
                    return;
                }
                if (!policy_graphs_differ_only_at_bounded_defaults(
                        result.strategy_json,
                        result.compilation,
                        result.certification_strategy_json,
                        result.certification_compilation)) {
                    finish_failure(
                        CompiledPolicyAssertionStatus::CompilationFailure,
                        "paired product and certification graphs differ "
                        "outside compiler-designated bounded defaults");
                    return;
                }
                result.paired_default_only = true;
                std::uint64_t paired_live = paired_retained;
                saturating_add(
                    paired_live,
                    std::max<std::uint64_t>(
                        result.compilation.peak_owned_bytes,
                        result.strategy_json.capacity() + 1));
                result.publication_peak_owned_bytes = std::max(
                    result.publication_peak_owned_bytes, paired_live);
            }
            stage = Stage::Done;
        } catch (const SolverResourceLimit& error) {
            result.evaluation = evaluation_work->diagnostic_result();
            record_evaluation_time();
            finish_failure(
                CompiledPolicyAssertionStatus::ResourceCap,
                failure_with_graph_context(error.what()),
                error.cap_name());
        } catch (const std::length_error& error) {
            result.evaluation = evaluation_work->diagnostic_result();
            record_evaluation_time();
            finish_failure(
                CompiledPolicyAssertionStatus::ResourceCap,
                failure_with_graph_context(error.what()),
                resource_cap_from_message(error.what()));
        } catch (const std::exception& error) {
            result.evaluation = evaluation_work->diagnostic_result();
            record_evaluation_time();
            finish_failure(
                CompiledPolicyAssertionStatus::ExactEvaluationFailure,
                failure_with_graph_context(error.what()));
        }
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining =
            std::max<std::uint32_t>(1, max_work_items);
        while (remaining != 0 && stage != Stage::Done) {
            if (stage == Stage::Compiling) {
                compile_and_prepare();
                --remaining;
                continue;
            }
            /* Preserve the evaluator's logical boundary even for callers
             * requesting large outer batches. This keeps broad kernels and
             * SCC iterations observable to the native solve scheduler. */
            advance_evaluation(1);
            --remaining;
        }
    }

    CompiledPolicyAssertionProgress progress() const {
        CompiledPolicyAssertionProgress value;
        value.done = stage == Stage::Done;
        value.phase = stage == Stage::Compiling
            ? CompiledPolicyAssertionPhase::Compiling
            : stage == Stage::Evaluating
                ? CompiledPolicyAssertionPhase::Certifying
                : CompiledPolicyAssertionPhase::Done;
        if (evaluation_work != nullptr) {
            value.evaluation = evaluation_work->progress();
        }
        return value;
    }

    std::uint64_t retained_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        saturating_add(bytes, result.strategy_json.capacity() + 1);
        saturating_add(
            bytes, result.certification_strategy_json.capacity() + 1);
        saturating_add(
            bytes,
            result.evaluation.retained_output_owned_bytes_estimate);
        saturating_add(bytes, result.failure_reason.capacity() + 1);
        saturating_add(bytes, result.failure_classification.capacity() + 1);
        saturating_add(bytes, result.resource_cap.capacity() + 1);
        saturating_add(
            bytes, result.compilation.policy_route_default_mode.capacity() +
                1);
        saturating_add(
            bytes,
            result.certification_compilation
                    .policy_route_default_mode.capacity() +
                1);
        saturating_add(bytes, result.compilation.cap_hit.capacity() + 1);
        saturating_add(
            bytes,
            result.certification_compilation.cap_hit.capacity() + 1);
        if (parsed_strategy != nullptr) {
            saturating_add(
                bytes, strategy_impl_owned_bytes(*parsed_strategy));
        }
        if (economy != nullptr) {
            saturating_add(
                bytes,
                economy_owned_bytes(
                    economy->prices, economy->id.capacity()));
        }
        if (evaluation_work != nullptr) {
            saturating_add(bytes, evaluation_work->live_owned_bytes());
        }
        return bytes;
    }
};

CompiledPolicyAssertionWork::CompiledPolicyAssertionWork(
        CalcContext& coarse,
        const SolveResult& solved,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        std::string strategy_name,
        const RefinedPolicyCompileRouting* refined_routing,
        const std::string* emitted_strategy_json,
        const PolicyCompilationTelemetry* emitted_compilation)
    : impl_(std::make_unique<Impl>(
          coarse, solved, prices, options, std::move(strategy_name),
          refined_routing, emitted_strategy_json,
          emitted_compilation)) {}

CompiledPolicyAssertionWork::~CompiledPolicyAssertionWork() = default;
CompiledPolicyAssertionWork::CompiledPolicyAssertionWork(
    CompiledPolicyAssertionWork&&) noexcept = default;
CompiledPolicyAssertionWork& CompiledPolicyAssertionWork::operator=(
    CompiledPolicyAssertionWork&&) noexcept = default;

void CompiledPolicyAssertionWork::step(
        const std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

CompiledPolicyAssertionProgress
CompiledPolicyAssertionWork::progress() const {
    return impl_->progress();
}

CompiledPolicyAssertion CompiledPolicyAssertionWork::take_result() {
    if (impl_->stage != Impl::Stage::Done) {
        throw std::logic_error(
            "compiled policy assertion work is not finished");
    }
    return std::move(impl_->result);
}

std::uint64_t CompiledPolicyAssertionWork::retained_bytes() const {
    return impl_->retained_bytes();
}

CompiledPolicyAssertion assert_compiled_policy_exact(
        CalcContext& coarse,
        const SolveResult& solved,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinedPolicyCompileRouting* refined_routing,
        const std::string* emitted_strategy_json,
        const PolicyCompilationTelemetry* emitted_compilation) {
    CompiledPolicyAssertionWork work(
        coarse, solved, prices, options, strategy_name,
        refined_routing, emitted_strategy_json,
        emitted_compilation);
    while (!work.progress().done) work.step(4096);
    return work.take_result();
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
