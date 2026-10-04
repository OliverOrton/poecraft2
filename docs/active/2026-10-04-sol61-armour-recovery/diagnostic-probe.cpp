// Diagnostic only: force the existing fourth proposal through the production
// selective-service owner, preserving aggregate work/memory and entry checks.
#define main released_benchmark_main
#include "../../engine/benchmarks/solver_benchmark.cpp"
#undef main

int main(int argc,char** argv) {
    using namespace poecraft::solver;
    const auto began=Clock::now();
    const auto elapsed=[&] { return std::chrono::duration<double>(Clock::now()-began).count(); };
    std::string stage="load"; pc_data_handle data=nullptr;
    try {
        if (argc!=4) throw std::runtime_error("usage: probe artifact case output-prefix");
        pc_error_info error; pc_error_info_init(&error);
        const auto load=pc_data_load_file(argv[1],&data,&error);
        if (load!=PC_RESULT_OK) throw std::runtime_error(api_error("load",load,error));
        const auto spec=parse_json(read_file(argv[2]),argv[2]);
        NativeHandles handles; pc_item_state root; create_case_objects(data,spec,handles,root);
        auto& calc=solver_lower_diagnostic_calculator(handles.solver);
        SolveOptions caps; apply_solve_profile_defaults(caps,SolveProfile::CalculatorProductV1);
        caps.full_evidence=true; caps.allow_economic_restart=false;
        SolveWorkTestAccess::Impl owner(calc,root,handles.economy->impl->prices,caps);
        if (!owner.options.product_original_root_continuations) throw std::runtime_error("original_request_not_product_eligible");
        owner.selective_service_orientation=3; // diagnostic; normal count stays3
        using Phase=SolveWorkTestAccess::Impl::SelectiveServicePhase;
        bool graph_saved=false,root_reported=false; double exact_deadline=150;
        std::uint32_t census=0,positive=0,validated=0;
        std::cout << "{\"phase\":\"resolved\",\"normal_proposals\":" << product_completion_proposal_count(calc)
                  << ",\"diagnostic_proposal\":3,\"goal_slots\":" << calc.goal().slots.size()
                  << ",\"states_cap\":" << owner.options.max_discovered_states
                  << ",\"bytes_cap\":" << owner.options.max_solver_owned_bytes
                  << ",\"work_cap\":" << owner.options.max_reforge_work << "}" << std::endl;
        while (owner.selective_service_phase!=Phase::Done) {
            const auto before=owner.selective_service_phase;
            stage=before==Phase::Checking?"root_exact":before==Phase::Validating?"entry_validation":"construction";
            const auto deadline=before==Phase::Checking?exact_deadline:before==Phase::Validating?150.0:120.0;
            if (elapsed()>=deadline) throw std::runtime_error("native_deadline:"+stage);
            owner.advance_selective_completion_service();
            if (!graph_saved && owner.selective_service_phase==Phase::Checking) {
                graph_saved=true; exact_deadline=std::min(150.0,elapsed()+30);
                std::ofstream(std::string(argv[3])+".strategy.json") << owner.selective_service_graph;
                std::cout << "{\"phase\":\"construction\",\"candidate\":true,\"source_states\":" << owner.selective_service_calc->state_count()
                          << ",\"elapsed_seconds\":" << elapsed() << "}" << std::endl;
            }
            if (!root_reported && owner.selective_service_phase==Phase::Validating) {
                root_reported=true; const auto& checked=owner.selective_service_checker->result();
                census=static_cast<std::uint32_t>(checked.policy_entries.entries.size());
                positive=owner.selective_service_validator->positive_entries();
                std::cout << std::setprecision(17) << "{\"phase\":\"root_exact\",\"accepted\":" << (finder_evaluation_accepted(checked)?"true":"false")
                          << ",\"cost\":" << checked.total_expected_cost << ",\"success_probability\":" << checked.success_probability
                          << ",\"cost_complete\":" << (checked.cost_complete?"true":"false") << ",\"converged\":" << (checked.converged?"true":"false")
                          << ",\"failure\":" << checked.failure_probability << ",\"stop\":" << checked.stop_probability
                          << ",\"not_applied\":" << checked.action_not_applied_probability << ",\"no_edge\":" << checked.no_matching_edge_probability
                          << ",\"unresolved\":" << checked.unresolved_probability << ",\"entries\":" << census
                          << ",\"positive\":" << positive << ",\"elapsed_seconds\":" << elapsed() << "}" << std::endl;
                std::ofstream materials(std::string(argv[3])+".materials.json"); materials << std::setprecision(17) << '{';bool first=true;
                for (const auto& [key,quantity]:checked.expected_consumption) { if(!first)materials<<',';first=false;materials<<escape_json(key)<<':'<<quantity; }
                materials << "}\n";
            }
            if (owner.selective_service_validator) validated=owner.selective_service_validator->validated_entries();
            if (before==Phase::Validating && owner.selective_service_phase==Phase::Done && owner.result.diagnostics.selective_completion_service_checks==1)
                validated=census; // service increments this only after validator completes
        }
        const auto& diagnostic=owner.result.diagnostics;
        const auto* retained=owner.prune_and_select_certified_fallback();
        std::cout << std::setprecision(17) << "{\"phase\":\"service_final\",\"status\":" << escape_json(diagnostic.selective_completion_service_status)
                  << ",\"checks\":" << diagnostic.selective_completion_service_checks << ",\"root_checked\":" << (root_reported?"true":"false")
                  << ",\"validated\":" << validated << ",\"census\":" << census << ",\"positive\":" << positive
                  << ",\"retained\":" << (retained?"true":"false") << ",\"work_used\":" << calc.telemetry().reforge_logical_work_v1
                  << ",\"native_peak_bytes\":" << owner.peak_owned_bytes << ",\"elapsed_seconds\":" << elapsed() << "}" << std::endl;
        pc_data_destroy(data);return 0;
    } catch(const std::exception& error) {
        std::cout << "{\"phase\":" << escape_json(stage) << ",\"refusal\":" << escape_json(error.what()) << ",\"elapsed_seconds\":" << elapsed() << "}" << std::endl;
        pc_data_destroy(data);return 2;
    }
}
