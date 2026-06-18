#include "tests.hpp"

#include "poecraft/api.h"
#include "poecraft/session.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string slurp(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

} // namespace

void run_data_loader_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("data-loader suite skipped (no artifact directory)\n");
        return;
    }
    const std::string dir = artifact_dir;
    const std::string manifest_path = dir + "/manifest.json";

    pc_error_info error;
    pc_error_info_init(&error);

    /* --- file load --- */
    pc_data_handle data = nullptr;
    pc_result rc = pc_data_load_file(manifest_path.c_str(), &data, &error);
    if (rc != PC_RESULT_OK) {
        std::printf("data load failed (%d): %s\n", error.code, error.message);
        PC_CHECK(rc == PC_RESULT_OK);
        return;
    }
    PC_CHECK(data != nullptr);

    pc_data_summary summary;
    PC_CHECK(pc_data_get_summary(data, &summary, &error) == PC_RESULT_OK);
    PC_CHECK(summary.complete_dataset == 1);
    PC_CHECK(summary.base_item_count > 0);
    PC_CHECK(summary.mod_count > 0);
    PC_CHECK(summary.ordinary_session_base_count > 0);
    std::printf("loaded %u bases (%u ordinary), %u mods, %u strings\n",
                summary.base_item_count, summary.ordinary_session_base_count,
                summary.mod_count, summary.string_count);

    /* --- capacity check against the real dataset --- */
    pc_capacity_report report;
    pc_result cap_rc = pc_data_check_capacities(data, &report, &error);
    std::printf(
        "capacities: max_base_implicits=%u (limit %u), max_mod_stats=%u "
        "(limit %u), max_base_tags=%u, max_mod_groups=%u\n",
        report.max_base_implicits, PC_MAX_IMPLICITS, report.max_mod_stats,
        PC_MAX_ROLL_VALUES, report.max_base_tags, report.max_mod_groups);
    PC_CHECK(report.base_implicits_ok == 1);
    PC_CHECK(report.mod_stats_ok == 1);
    PC_CHECK(cap_rc == PC_RESULT_OK);

    /* --- memory bundle load (the WebAssembly path) --- */
    {
        const std::string manifest_text = slurp(manifest_path);
        const std::string strings_text = slurp(dir + "/strings.json");
        const std::string game_text = slurp(dir + "/game-data.json");
        std::string bundle = "{\"manifest\":";
        bundle += manifest_text;
        bundle += ",\"strings\":";
        bundle += strings_text;
        bundle += ",\"game_data\":";
        bundle += game_text;
        bundle += "}";
        pc_data_handle mem_data = nullptr;
        pc_result mem_rc = pc_data_load_memory(bundle.data(), bundle.size(),
                                               &mem_data, &error);
        PC_CHECK(mem_rc == PC_RESULT_OK);
        if (mem_rc == PC_RESULT_OK) {
            pc_data_summary mem_summary;
            PC_CHECK(pc_data_get_summary(mem_data, &mem_summary, &error) ==
                     PC_RESULT_OK);
            PC_CHECK(mem_summary.base_item_count == summary.base_item_count);
            PC_CHECK(mem_summary.mod_count == summary.mod_count);
            pc_data_destroy(mem_data);
        }
    }

    /* --- resolve multiple ordinary bases by metadata path --- */
    std::vector<std::string> ordinary_paths;
    std::string cluster_path;
    for (uint32_t i = 0; i < summary.base_item_count; ++i) {
        const char* path = nullptr;
        int32_t support = -1;
        if (pc_data_get_base_path(data, i, &path, &support, &error) !=
            PC_RESULT_OK) {
            continue;
        }
        if (support == PC_SESSION_SUPPORT_ORDINARY &&
            ordinary_paths.size() < 4) {
            ordinary_paths.emplace_back(path);
        } else if (support == PC_SESSION_SUPPORT_CLUSTER_UNSUPPORTED &&
                   cluster_path.empty()) {
            cluster_path = path;
        }
    }
    PC_CHECK(ordinary_paths.size() >= 2);

    pc_session_handle first_session = nullptr;
    for (const std::string& path : ordinary_paths) {
        pc_session_options options;
        options.struct_size = sizeof(options);
        options.abi_version = PC_ABI_VERSION;
        options.base_metadata_path = path.c_str();
        options.item_level = 86;
        pc_session_handle session = nullptr;
        pc_result session_rc =
            pc_session_create(data, &options, &session, &error);
        PC_CHECK(session_rc == PC_RESULT_OK);
        if (session_rc != PC_RESULT_OK) {
            continue;
        }
        pc_base_info info;
        PC_CHECK(pc_session_get_base_info(session, &info, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(info.session_support == PC_SESSION_SUPPORT_ORDINARY);
        PC_CHECK(std::string(info.metadata_path) == path);
        PC_CHECK(info.item_level == 86);
        if (first_session == nullptr) {
            first_session = session; /* keep one alive for later checks */
        } else {
            pc_session_destroy(session);
        }
    }
    PC_CHECK(first_session != nullptr);

    /* --- cluster sessions are explicitly unsupported --- */
    if (!cluster_path.empty()) {
        pc_session_options options;
        options.struct_size = sizeof(options);
        options.abi_version = PC_ABI_VERSION;
        options.base_metadata_path = cluster_path.c_str();
        options.item_level = 86;
        pc_session_handle cluster_session = nullptr;
        pc_result cluster_rc =
            pc_session_create(data, &options, &cluster_session, &error);
        PC_CHECK(cluster_rc == PC_RESULT_UNSUPPORTED_FEATURE);
        PC_CHECK(cluster_session == nullptr);
    }

    /* unknown base path resolves to NOT_FOUND */
    {
        pc_session_options options;
        options.struct_size = sizeof(options);
        options.abi_version = PC_ABI_VERSION;
        options.base_metadata_path = "Metadata/Items/DoesNotExist";
        options.item_level = 1;
        pc_session_handle missing = nullptr;
        PC_CHECK(pc_session_create(data, &options, &missing, &error) ==
                 PC_RESULT_NOT_FOUND);
    }

    /* --- two action contexts share one immutable session with independent
     * random state --- */
    pc_action_context_options ctx_options;
    ctx_options.struct_size = sizeof(ctx_options);
    ctx_options.abi_version = PC_ABI_VERSION;
    ctx_options.seed = 42;
    pc_action_context_handle ctx_a = nullptr;
    pc_action_context_handle ctx_b = nullptr;
    PC_CHECK(pc_action_context_create(first_session, &ctx_options, &ctx_a,
                                      &error) == PC_RESULT_OK);
    PC_CHECK(pc_action_context_create(first_session, &ctx_options, &ctx_b,
                                      &error) == PC_RESULT_OK);
    /* identical seeds -> identical independent streams */
    bool same_stream = true;
    for (int i = 0; i < 16; ++i) {
        uint64_t va = 0;
        uint64_t vb = 0;
        pc_action_context_next_u64(ctx_a, &va, &error);
        pc_action_context_next_u64(ctx_b, &vb, &error);
        if (va != vb) {
            same_stream = false;
            break;
        }
    }
    PC_CHECK(same_stream);
    /* advancing one context does not disturb the other */
    pc_action_context_reseed(ctx_a, 1, &error);
    pc_action_context_reseed(ctx_b, 2, &error);
    uint64_t da = 0;
    uint64_t db = 0;
    pc_action_context_next_u64(ctx_a, &da, &error);
    pc_action_context_next_u64(ctx_b, &db, &error);
    PC_CHECK(da != db);

    /* --- child handle outlives the parent data handle --- */
    pc_data_destroy(data); /* destroy parent first */
    pc_base_info info_after_destroy;
    PC_CHECK(pc_session_get_base_info(first_session, &info_after_destroy,
                                      &error) == PC_RESULT_OK);
    PC_CHECK(info_after_destroy.session_support ==
             PC_SESSION_SUPPORT_ORDINARY);

    /* item init + debug format buffer-too-small / required-count pattern */
    pc_item_init_options item_options;
    item_options.struct_size = sizeof(item_options);
    item_options.abi_version = PC_ABI_VERSION;
    item_options.rarity = PC_RARITY_RARE;
    item_options.with_implicits = 0;
    pc_item_state item;
    PC_CHECK(pc_item_init(first_session, &item_options, &item, &error) ==
             PC_RESULT_OK);
    PC_CHECK(item.rarity == PC_RARITY_RARE);

    /* Base implicits resolve through the full dense session universe. */
    item_options.with_implicits = 1;
    PC_CHECK(pc_item_init(first_session, &item_options, &item, &error) ==
             PC_RESULT_OK);
    PC_CHECK(item.implicit_count == info_after_destroy.implicit_count);

    size_t needed = 0;
    pc_result small_rc =
        pc_item_debug_format(first_session, &item, nullptr, 0, &needed, &error);
    PC_CHECK(small_rc == PC_RESULT_BUFFER_TOO_SMALL);
    PC_CHECK(needed > 0);
    std::vector<char> buffer(needed + 1);
    PC_CHECK(pc_item_debug_format(first_session, &item, buffer.data(),
                                  buffer.size(), &needed, &error) ==
             PC_RESULT_OK);

    pc_action_context_destroy(ctx_a);
    pc_action_context_destroy(ctx_b);
    pc_session_destroy(first_session);
}
