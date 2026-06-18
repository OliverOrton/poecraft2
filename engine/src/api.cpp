#include "poecraft/api.h"
#include "poecraft/bitset.h"
#include "poecraft/session.h"

#include "engine_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/* Opaque handle bodies. */
struct pc_data {
    std::shared_ptr<poecraft::DataImpl> impl;
};
struct pc_session {
    std::shared_ptr<poecraft::SessionImpl> impl;
};
struct pc_action_context {
    std::unique_ptr<poecraft::ActionContextImpl> impl;
};

namespace {

void set_error(pc_error_info* error, pc_result code, const char* message) {
    if (error == nullptr) {
        return;
    }
    error->struct_size = static_cast<uint32_t>(sizeof(pc_error_info));
    error->abi_version = PC_ABI_VERSION;
    error->code = static_cast<int32_t>(code);
    std::snprintf(error->message, sizeof(error->message), "%s",
                  message ? message : "");
}

void clear_error(pc_error_info* error) {
    set_error(error, PC_RESULT_OK, "");
}

bool read_file(const std::string& path, std::string& out, std::string& why) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        why = "cannot open file: " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

std::string directory_of(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return std::string();
    }
    return path.substr(0, slash + 1);
}

/* Copy text into a caller buffer using the query-required-count pattern. */
pc_result emit_text(
    const std::string& text,
    char* buffer,
    std::size_t buffer_size,
    std::size_t* out_length,
    pc_error_info* error) {
    if (out_length != nullptr) {
        *out_length = text.size();
    }
    if (buffer == nullptr || buffer_size == 0) {
        set_error(error, PC_RESULT_BUFFER_TOO_SMALL, "buffer required");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    if (text.size() + 1 > buffer_size) {
        set_error(error, PC_RESULT_BUFFER_TOO_SMALL,
                  "buffer too small for formatted text");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    std::memcpy(buffer, text.data(), text.size());
    buffer[text.size()] = '\0';
    clear_error(error);
    return PC_RESULT_OK;
}

} // namespace

uint32_t pc_abi_version(void) {
    return PC_ABI_VERSION;
}

void pc_error_info_init(pc_error_info* error) {
    clear_error(error);
}

// --- data loading -----------------------------------------------------------

pc_result pc_data_load_file(
    const char* manifest_path,
    pc_data_handle* out_data,
    pc_error_info* out_error) {
    if (manifest_path == nullptr || out_data == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_data = nullptr;
    const std::string manifest_file = manifest_path;
    const std::string dir = directory_of(manifest_file);
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    std::string why;
    if (!read_file(manifest_file, manifest_text, why) ||
        !read_file(dir + "strings.json", strings_text, why) ||
        !read_file(dir + "game-data.json", game_text, why)) {
        set_error(out_error, PC_RESULT_IO_ERROR, why.c_str());
        return PC_RESULT_IO_ERROR;
    }
    try {
        auto impl =
            poecraft::load_data_impl(manifest_text, strings_text, game_text);
        auto holder = std::make_unique<pc_data>();
        holder->impl = std::move(impl);
        *out_data = holder.release();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_DATA_ERROR, ex.what());
        return PC_RESULT_DATA_ERROR;
    }
}

pc_result pc_data_load_memory(
    const void* bytes,
    size_t byte_count,
    pc_data_handle* out_data,
    pc_error_info* out_error) {
    if (bytes == nullptr || out_data == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_data = nullptr;
    try {
        std::string bundle(static_cast<const char*>(bytes), byte_count);
        auto impl = poecraft::load_data_impl_bundle(bundle);
        auto holder = std::make_unique<pc_data>();
        holder->impl = std::move(impl);
        *out_data = holder.release();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_DATA_ERROR, ex.what());
        return PC_RESULT_DATA_ERROR;
    }
}

void pc_data_destroy(pc_data_handle data) {
    delete data;
}

pc_result pc_data_get_summary(
    pc_data_handle data,
    pc_data_summary* out_summary,
    pc_error_info* out_error) {
    if (data == nullptr || out_summary == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    out_summary->struct_size = static_cast<uint32_t>(sizeof(pc_data_summary));
    out_summary->abi_version = PC_ABI_VERSION;
    out_summary->artifact_schema_version = d.artifact_schema_version;
    out_summary->complete_dataset = d.complete_dataset ? 1 : 0;
    out_summary->string_count = static_cast<uint32_t>(d.strings.size());
    out_summary->tag_count = d.count_tags;
    out_summary->item_class_count = d.item_class_count;
    out_summary->base_item_count = d.base_count;
    out_summary->ordinary_session_base_count = d.count_ordinary_bases;
    out_summary->cluster_unsupported_base_count = d.count_cluster_bases;
    out_summary->unsupported_domain_base_count =
        d.count_unsupported_domain_bases;
    out_summary->mod_count = d.mod_count;
    out_summary->group_count = d.count_groups;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_data_get_base_path(
    pc_data_handle data,
    uint32_t base_index,
    const char** out_path,
    int32_t* out_session_support,
    pc_error_info* out_error) {
    if (data == nullptr || out_path == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    if (base_index >= d.base_count) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "base index out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_path = d.string_at(d.base_metadata_path_sid[base_index]).c_str();
    if (out_session_support != nullptr) {
        *out_session_support = d.base_session_support[base_index];
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_data_check_capacities(
    pc_data_handle data,
    pc_capacity_report* out_report,
    pc_error_info* out_error) {
    if (data == nullptr || out_report == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    auto max_delta = [](const std::vector<std::uint32_t>& offsets) -> uint32_t {
        uint32_t max_value = 0;
        for (std::size_t i = 1; i < offsets.size(); ++i) {
            max_value = std::max(max_value, offsets[i] - offsets[i - 1]);
        }
        return max_value;
    };

    out_report->struct_size = static_cast<uint32_t>(sizeof(pc_capacity_report));
    out_report->abi_version = PC_ABI_VERSION;
    out_report->max_base_implicits = max_delta(d.base_implicit_offsets);
    out_report->max_mod_stats = max_delta(d.stat_offsets);
    out_report->max_base_tags = max_delta(d.base_tag_offsets);
    out_report->max_mod_groups = max_delta(d.mod_group_offsets);
    out_report->max_essence_mod_links = max_delta(d.essence_mod_offsets);

    out_report->base_implicits_ok =
        out_report->max_base_implicits <= PC_MAX_IMPLICITS ? 1 : 0;
    out_report->mod_stats_ok =
        out_report->max_mod_stats <= PC_MAX_ROLL_VALUES ? 1 : 0;

    if (!out_report->base_implicits_ok || !out_report->mod_stats_ok) {
        set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED,
                  "dataset exceeds a provisional fixed capacity");
        return PC_RESULT_CAPACITY_EXCEEDED;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_data_debug_format(
    pc_data_handle data,
    char* buffer,
    size_t buffer_size,
    size_t* out_length,
    pc_error_info* out_error) {
    if (data == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null data");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    std::ostringstream out;
    out << "poecraft runtime dataset\n";
    out << "  schema_version: " << d.artifact_schema_version << "\n";
    out << "  complete_dataset: " << (d.complete_dataset ? "true" : "false")
        << "\n";
    out << "  strings: " << d.strings.size() << "\n";
    out << "  item_classes: " << d.item_class_count << "\n";
    out << "  base_items: " << d.base_count << " (ordinary "
        << d.count_ordinary_bases << ", cluster " << d.count_cluster_bases
        << ", unsupported_domain " << d.count_unsupported_domain_bases << ")\n";
    out << "  mods: " << d.mod_count << ", groups: " << d.count_groups << "\n";
    return emit_text(out.str(), buffer, buffer_size, out_length, out_error);
}

// --- sessions ---------------------------------------------------------------

pc_result pc_session_create(
    pc_data_handle data,
    const pc_session_options* options,
    pc_session_handle* out_session,
    pc_error_info* out_error) {
    if (data == nullptr || options == nullptr || out_session == nullptr ||
        options->base_metadata_path == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_session = nullptr;
    if (options->item_level < 1) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "item level must be at least 1");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    auto it = d.base_by_path.find(options->base_metadata_path);
    if (it == d.base_by_path.end()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "base metadata path not found");
        return PC_RESULT_NOT_FOUND;
    }
    const std::uint32_t base_index = it->second;
    const std::int32_t support = d.base_session_support[base_index];
    if (support == PC_SESSION_SUPPORT_CLUSTER_UNSUPPORTED) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                  "cluster-jewel sessions are not supported yet");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
    if (support != PC_SESSION_SUPPORT_ORDINARY) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                  "base domain does not support ordinary crafting sessions");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
    auto session = std::make_shared<poecraft::SessionImpl>();
    session->data = data->impl;
    session->base_index = base_index;
    session->item_level = options->item_level;
    try {
        poecraft::build_session(*session);
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
    auto holder = std::make_unique<pc_session>();
    holder->impl = std::move(session);
    *out_session = holder.release();
    clear_error(out_error);
    return PC_RESULT_OK;
}

void pc_session_destroy(pc_session_handle session) {
    delete session;
}

pc_result pc_session_get_base_info(
    pc_session_handle session,
    pc_base_info* out_info,
    pc_error_info* out_error) {
    if (session == nullptr || out_info == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    const poecraft::DataImpl& d = *s.data;
    const std::uint32_t i = s.base_index;

    out_info->struct_size = static_cast<uint32_t>(sizeof(pc_base_info));
    out_info->abi_version = PC_ABI_VERSION;
    out_info->global_base_item_id = d.base_global_ids[i];
    out_info->item_level = s.item_level;
    out_info->session_support = d.base_session_support[i];
    out_info->tag_count = d.base_tag_offsets[i + 1] - d.base_tag_offsets[i];
    out_info->implicit_count =
        d.base_implicit_offsets[i + 1] - d.base_implicit_offsets[i];
    out_info->metadata_path = d.string_at(d.base_metadata_path_sid[i]).c_str();
    out_info->name = d.string_at(d.base_name_sid[i]).c_str();
    const auto class_it = d.item_class_index_by_id.find(d.base_item_class_id[i]);
    out_info->item_class_key =
        class_it != d.item_class_index_by_id.end()
            ? d.string_at(d.item_class_key_sid[class_it->second]).c_str()
            : "";
    out_info->domain = d.domain_name(d.base_domain_code[i]).c_str();
    clear_error(out_error);
    return PC_RESULT_OK;
}

// --- session introspection --------------------------------------------------

pc_result pc_session_get_mod_count(
    pc_session_handle session,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (session == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_count = session->impl->mod_count;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_get_mod_info(
    pc_session_handle session,
    uint32_t session_mod_id,
    pc_mod_info* out_info,
    pc_error_info* out_error) {
    if (session == nullptr || out_info == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    if (session_mod_id >= s.mod_count) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "mod id out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *s.data;
    const std::uint32_t p = s.global_index[session_mod_id];
    out_info->struct_size = static_cast<uint32_t>(sizeof(pc_mod_info));
    out_info->abi_version = PC_ABI_VERSION;
    out_info->session_mod_id = session_mod_id;
    out_info->global_mod_id = d.mod_global_ids[p];
    out_info->key = d.string_at(d.mod_key_sid[p]).c_str();
    out_info->generation_type = s.gen_type[session_mod_id];
    out_info->reach_kind = s.reach_kind[session_mod_id];
    out_info->reach_influence = s.reach_influence[session_mod_id];
    out_info->reach_via = s.reach_via[session_mod_id].c_str();
    out_info->primary_group_id = s.primary_group[session_mod_id];
    out_info->required_level = s.required_level[session_mod_id];
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_dump_effective_tags(
    pc_session_handle session,
    const char** out_names,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (session == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    const poecraft::DataImpl& d = *s.data;
    const std::size_t total = s.effective_base_tag_ids.size();
    *out_count = static_cast<uint32_t>(total);
    if (out_names == nullptr || capacity < total) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < total; ++i) {
        const auto it = d.tag_name_by_id.find(s.effective_base_tag_ids[i]);
        out_names[i] = it != d.tag_name_by_id.end() ? it->second.c_str() : "";
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_dump_mod_groups(
    pc_session_handle session,
    uint32_t session_mod_id,
    uint32_t* out_group_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (session == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    if (session_mod_id >= s.mod_count) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "mod id out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const std::uint32_t begin = s.group_offsets[session_mod_id];
    const std::uint32_t end = s.group_offsets[session_mod_id + 1];
    const std::uint32_t total = end - begin;
    *out_count = total;
    if (out_group_ids == nullptr || capacity < total) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::uint32_t i = 0; i < total; ++i) {
        out_group_ids[i] = s.group_ids[begin + i];
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_dump_mask(
    pc_session_handle session,
    int mask_kind,
    uint32_t* out_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (session == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    const std::vector<std::uint64_t>* mask = nullptr;
    switch (mask_kind) {
    case PC_MASK_UNIVERSE: mask = &s.universe_mask; break;
    case PC_MASK_PREFIX: mask = &s.prefix_mask; break;
    case PC_MASK_SUFFIX: mask = &s.suffix_mask; break;
    case PC_MASK_NORMAL_RANDOM_ROLL: mask = &s.normal_random_roll_mask; break;
    case PC_MASK_POSITIVE_SPAWN: mask = &s.positive_spawn_weight_mask; break;
    case PC_MASK_POSITIVE_BASE: mask = &s.positive_base_weight_mask; break;
    default:
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "unknown mask kind");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const std::size_t total = poecraft::pc_bitset_count(mask->data(), s.words);
    *out_count = static_cast<uint32_t>(total);
    if (out_ids == nullptr || capacity < total) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    std::uint32_t write = 0;
    poecraft::pc_bitset_for_each(mask->data(), s.words,
                                 [&](std::size_t id) {
                                     out_ids[write++] =
                                         static_cast<uint32_t>(id);
                                 });
    clear_error(out_error);
    return PC_RESULT_OK;
}

// --- action contexts --------------------------------------------------------

pc_result pc_action_context_create(
    pc_session_handle session,
    const pc_action_context_options* options,
    pc_action_context_handle* out_context,
    pc_error_info* out_error) {
    if (session == nullptr || out_context == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_context = nullptr;
    const std::uint64_t seed = options != nullptr ? options->seed : 0;
    auto context = std::make_unique<poecraft::ActionContextImpl>(seed);
    context->session = session->impl;
    auto holder = std::make_unique<pc_action_context>();
    holder->impl = std::move(context);
    *out_context = holder.release();
    clear_error(out_error);
    return PC_RESULT_OK;
}

void pc_action_context_destroy(pc_action_context_handle context) {
    delete context;
}

pc_result pc_action_context_next_u64(
    pc_action_context_handle context,
    uint64_t* out_value,
    pc_error_info* out_error) {
    if (context == nullptr || out_value == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_value = context->impl->rng.next_u64();
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_action_context_reseed(
    pc_action_context_handle context,
    uint64_t seed,
    pc_error_info* out_error) {
    if (context == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null context");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    context->impl->rng.reseed(seed);
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_action_context_debug_pool(
    pc_action_context_handle context,
    const pc_item_state* item,
    int side_filter,
    pc_pool_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (context == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (side_filter < -1 || side_filter > 1) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "bad side filter");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *context->impl->session;
    const std::vector<poecraft::PoolEntry> pool =
        poecraft::build_normal_pool(s, item, side_filter);
    *out_count = static_cast<uint32_t>(pool.size());
    if (entries == nullptr || capacity < pool.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < pool.size(); ++i) {
        const poecraft::PoolEntry& src = pool[i];
        pc_pool_entry& dst = entries[i];
        dst.session_mod_id = src.session_mod_id;
        dst.global_mod_id = src.global_mod_id;
        dst.primary_group_id = src.primary_group;
        dst.generation_type = src.gen_type;
        dst.required_level = src.required_level;
        dst.spawn_weight = src.spawn_weight;
        dst.generation_multiplier_pct = src.generation_pct;
        dst.final_weight = src.final_weight;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

static pc_result emit_pool(
    const std::vector<poecraft::PoolEntry>& pool,
    pc_pool_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    *out_count = static_cast<uint32_t>(pool.size());
    if (entries == nullptr || capacity < pool.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < pool.size(); ++i) {
        const poecraft::PoolEntry& src = pool[i];
        pc_pool_entry& dst = entries[i];
        dst.session_mod_id = src.session_mod_id;
        dst.global_mod_id = src.global_mod_id;
        dst.primary_group_id = src.primary_group;
        dst.generation_type = src.gen_type;
        dst.required_level = src.required_level;
        dst.spawn_weight = src.spawn_weight;
        dst.generation_multiplier_pct = src.generation_pct;
        dst.final_weight = src.final_weight;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_action_context_debug_harvest_pool(
    pc_action_context_handle context,
    const pc_item_state* item,
    const char* target_tag,
    int side_filter,
    pc_pool_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (context == nullptr || target_tag == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (side_filter < -1 || side_filter > 1) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "bad side filter");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *context->impl->session;
    const poecraft::DataImpl& d = *s.data;
    const auto it = d.tag_id_by_name.find(target_tag);
    if (it == d.tag_id_by_name.end()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "unknown classification tag");
        return PC_RESULT_NOT_FOUND;
    }
    const std::vector<poecraft::PoolEntry> pool =
        poecraft::build_harvest_pool(s, item, it->second, side_filter);
    return emit_pool(pool, entries, capacity, out_count, out_error);
}

pc_result pc_session_dump_implicit_tag(
    pc_session_handle session,
    const char* tag,
    uint32_t* out_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (session == nullptr || tag == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    const poecraft::DataImpl& d = *s.data;
    const auto it = d.tag_id_by_name.find(tag);
    if (it == d.tag_id_by_name.end()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "unknown classification tag");
        return PC_RESULT_NOT_FOUND;
    }
    const std::uint32_t target = it->second;
    std::vector<std::uint32_t> ids;
    for (std::uint32_t m = 0; m < s.mod_count; ++m) {
        for (std::uint32_t i = s.class_offsets[m]; i < s.class_offsets[m + 1];
             ++i) {
            if (s.class_tag_ids[i] == target) {
                ids.push_back(m);
                break;
            }
        }
    }
    *out_count = static_cast<uint32_t>(ids.size());
    if (out_ids == nullptr || capacity < ids.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < ids.size(); ++i) {
        out_ids[i] = ids[i];
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_apply_action(
    pc_action_context_handle context,
    pc_item_state* item,
    const pc_action_request* request,
    pc_action_result* out_result,
    pc_error_info* out_error) {
    if (context == nullptr || item == nullptr || request == nullptr ||
        out_result == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (request->action_type < 0 ||
        request->action_type > static_cast<int32_t>(poecraft::ActionType::Scour)) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "unknown action type");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *context->impl->session;
    // Apply to a private copy so a failed action leaves the caller's item intact.
    pc_item_state scratch = *item;
    const poecraft::ActionOutcome outcome = poecraft::apply_action(
        s, context->impl->rng, &scratch,
        static_cast<poecraft::ActionType>(request->action_type));
    if (outcome.applied) {
        *item = scratch;
    }
    out_result->struct_size = static_cast<uint32_t>(sizeof(pc_action_result));
    out_result->abi_version = PC_ABI_VERSION;
    out_result->applied = outcome.applied ? 1 : 0;
    out_result->added = outcome.added;
    out_result->removed = outcome.removed;
    clear_error(out_error);
    return PC_RESULT_OK;
}

// --- item init / debug ------------------------------------------------------

pc_result pc_item_init(
    pc_session_handle session,
    const pc_item_init_options* options,
    pc_item_state* out_item,
    pc_error_info* out_error) {
    if (session == nullptr || out_item == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const bool want_implicits = options != nullptr && options->with_implicits;
    if (want_implicits) {
        /* Base implicits resolve to dense session mod ids, which are built in
         * Phase 5. Until then, refuse rather than emit unresolved ids. */
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                  "base implicits require the Phase 5 session mod universe");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
    pc_item_state scratch;
    pc_item_clear(&scratch);
    scratch.rarity = options != nullptr
                         ? options->rarity
                         : static_cast<uint8_t>(PC_RARITY_NORMAL);
    *out_item = scratch; /* commit only on success */
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_item_debug_format(
    pc_session_handle session,
    const pc_item_state* item,
    char* buffer,
    size_t buffer_size,
    size_t* out_length,
    pc_error_info* out_error) {
    if (session == nullptr || item == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    const poecraft::DataImpl& d = *s.data;
    const std::uint32_t i = s.base_index;

    static const char* rarity_names[] = {"normal", "magic", "rare"};
    const char* rarity =
        item->rarity < 3 ? rarity_names[item->rarity] : "unknown";

    std::ostringstream out;
    out << "item: " << d.string_at(d.base_name_sid[i]) << " (ilvl "
        << s.item_level << ")\n";
    out << "  rarity: " << rarity << ", quality: "
        << static_cast<unsigned>(item->quality) << "\n";
    out << "  prefixes: " << static_cast<unsigned>(item->prefix_count) << "/"
        << static_cast<unsigned>(pc_item_max_prefix(item)) << "\n";
    out << "  suffixes: " << static_cast<unsigned>(item->suffix_count) << "/"
        << static_cast<unsigned>(pc_item_max_suffix(item)) << "\n";
    out << "  implicits: " << static_cast<unsigned>(item->implicit_count)
        << ", enchantments: "
        << static_cast<unsigned>(item->enchantment_count) << "\n";
    return emit_text(out.str(), buffer, buffer_size, out_length, out_error);
}
