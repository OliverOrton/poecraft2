#include "poecraft/api.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
extern "C" uint64_t pc_solver_progress_sequence(pc_solver_handle);
extern "C" pc_result pc_solver_progress_trace(pc_solver_handle, uint64_t, char*, size_t, size_t*, pc_error_info*);
std::string read(const char* path) { std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error(path); std::ostringstream s; s << f.rdbuf(); return s.str(); }
int main() {
    pc_error_info error{}; pc_error_info_init(&error);
    auto check=[&](bool ok) { if (!ok) throw std::runtime_error(error.message); };
    try {
        check(pc_solver_progress_sequence(nullptr)==0);
        pc_data_handle data=nullptr;
        check(pc_data_load_file("data/compiled/current/manifest.json", &data, &error)==PC_RESULT_OK);
        pc_session_options session_options{}; session_options.struct_size=sizeof(session_options); session_options.abi_version=PC_ABI_VERSION;
        session_options.base_metadata_path="Metadata/Items/Armours/BodyArmours/BodyStrDex20"; session_options.item_level=86;
        pc_session_handle session=nullptr; check(pc_session_create(data,&session_options,&session,&error)==PC_RESULT_OK);
        const auto goal=read("docs/archive/2026-09-04-free-value-bellman-research/native-goal.json");
        const auto prices=read("docs/archive/2026-09-04-free-value-bellman-research/native-economy.json");
        pc_economy_handle economy=nullptr; check(pc_economy_load_json(prices.data(),prices.size(),&economy,&error)==PC_RESULT_OK);
        pc_solver_handle solver=nullptr; check(pc_solver_create(session,goal.data(),goal.size(),&solver,&error)==PC_RESULT_OK);
        check(pc_solver_progress_sequence(solver)==0);
        pc_item_init_options init{}; init.struct_size=sizeof(init); init.abi_version=PC_ABI_VERSION; init.rarity=PC_RARITY_RARE;
        pc_item_state item{}; check(pc_item_init(session,&init,&item,&error)==PC_RESULT_OK);
        pc_solve_options options{}; options.struct_size=sizeof(options); options.abi_version=PC_ABI_VERSION;
        options.max_solver_owned_bytes=1ull<<30;
        check(pc_solver_solve_begin(solver,&item,economy,&options,&error)==PC_RESULT_OK);
        check(pc_solver_solve_request_bounded_finish(solver,&error)==PC_RESULT_OK);
        const auto sequence=pc_solver_progress_sequence(solver); check(sequence>0);
        for(int i=0;i<100;++i) check(pc_solver_progress_sequence(solver)==sequence);
        size_t length=0; check(pc_solver_progress_trace(solver,0,nullptr,0,&length,&error)==PC_RESULT_OK);
        std::string trace(length+1,'\0'); check(pc_solver_progress_trace(solver,0,trace.data(),trace.size(),&length,&error)==PC_RESULT_OK);
        check(pc_solver_progress_sequence(solver)==sequence);
        struct GuardedProgress { pc_solve_progress progress{}; uint64_t canary=0xF0E1D2C3B4A59687ull; } guarded;
        check(pc_solver_solve_step(solver,1,&guarded.progress,&error)==PC_RESULT_OK);
        check(guarded.canary==0xF0E1D2C3B4A59687ull);
        check(guarded.progress.struct_size==sizeof(pc_solve_progress));
        pc_solver_solve_abandon(solver); check(pc_solver_progress_sequence(solver)==0);
        pc_solver_destroy(solver); pc_economy_destroy(economy); pc_session_destroy(session); pc_data_destroy(data);
        std::cout << "{\"old_header_progress_bytes\":" << sizeof(pc_solve_progress) << ",\"adjacent_canary_unchanged\":true,\"sequence_reads_passive\":true,\"trace_read_passive\":true,\"null_and_abandon_zero\":true}\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
