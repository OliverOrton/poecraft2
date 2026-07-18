#include "poecraft/api.h"
#include "poecraft/bitset.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"

#include "engine_internal.hpp"
#include "harvest_crafts.generated.hpp"
#include "handles_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

// Reject input option/request structs that declare an incompatible ABI so a
// newer caller against an older engine fails cleanly rather than reading a
// mismatched layout.
bool abi_ok(uint32_t abi_version, pc_error_info* error) {
    if (abi_version != PC_ABI_VERSION) {
        set_error(error, PC_RESULT_INVALID_ARGUMENT,
                  "incompatible struct abi_version");
        return false;
    }
    return true;
}

bool abi_struct_ok(
    uint32_t struct_size,
    uint32_t abi_version,
    std::size_t minimum_size,
    pc_error_info* error) {
    if (!abi_ok(abi_version, error)) return false;
    if (struct_size < minimum_size) {
        set_error(error, PC_RESULT_INVALID_ARGUMENT, "input struct too small");
        return false;
    }
    return true;
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

pc_result parse_action_request(
    const poecraft::ActionContextImpl& context,
    const pc_action_request& request,
    poecraft::ActionParameters& out_action,
    poecraft::PoolBuildRequest* out_pool,
    pc_error_info* error) {
    if (!abi_struct_ok(request.struct_size, request.abi_version,
                       sizeof(pc_action_request), error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (request.action_type < PC_ACTION_TRANSMUTE ||
        request.action_type > PC_ACTION_REMOVE_CRAFTED_MODIFIERS) {
        set_error(error, PC_RESULT_INVALID_ARGUMENT, "unknown action type");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    out_action = {};
    out_action.type =
        static_cast<poecraft::ActionType>(request.action_type);
    if (out_pool != nullptr) *out_pool = {};

    const poecraft::DataImpl& d = *context.session->data;
    if (request.action_type == PC_ACTION_ESSENCE) {
        if (request.essence_key == nullptr) {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "essence action requires essence_key");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        const auto it = d.essence_by_key.find(request.essence_key);
        if (it == d.essence_by_key.end()) {
            set_error(error, PC_RESULT_NOT_FOUND, "essence key not found");
            return PC_RESULT_NOT_FOUND;
        }
        out_action.essence_index = it->second;
        if (out_pool != nullptr) {
            out_pool->respects_metamod_pool_blocks =
                poecraft::action_transition_facts(
                    poecraft::ActionType::Essence)
                    .respects_metamod_pool_blocks;
        }
    } else if (request.action_type == PC_ACTION_FOSSIL) {
        if (request.fossil_count == 0 ||
            request.fossil_count > PC_MAX_FOSSILS_PER_ACTION) {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "fossil action requires 1-4 fossils");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        for (std::uint32_t i = 0; i < request.fossil_count; ++i) {
            if (request.fossil_keys[i] == nullptr) {
                set_error(error, PC_RESULT_INVALID_ARGUMENT,
                          "null fossil key");
                return PC_RESULT_INVALID_ARGUMENT;
            }
            const auto it = d.fossil_by_key.find(request.fossil_keys[i]);
            if (it == d.fossil_by_key.end()) {
                set_error(error, PC_RESULT_NOT_FOUND, "fossil key not found");
                return PC_RESULT_NOT_FOUND;
            }
            out_action.fossil_indices.push_back(it->second);
        }
        std::sort(out_action.fossil_indices.begin(),
                  out_action.fossil_indices.end());
        out_action.fossil_indices.erase(
            std::unique(out_action.fossil_indices.begin(),
                        out_action.fossil_indices.end()),
            out_action.fossil_indices.end());
        if (out_pool != nullptr) {
            out_pool->weight_kind = poecraft::PoolWeightKind::Fossil;
            out_pool->fossil_indices = out_action.fossil_indices;
            out_pool->respects_metamod_pool_blocks =
                poecraft::action_transition_facts(
                    poecraft::ActionType::Fossil)
                    .respects_metamod_pool_blocks;
        }
    } else if (request.action_type == PC_ACTION_BENCH ||
               request.action_type == PC_ACTION_UNVEIL) {
        if (request.mod_key == nullptr || request.mod_key[0] == '\0') {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "action requires mod_key");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        const auto global = d.mod_pos_by_key.find(request.mod_key);
        if (global == d.mod_pos_by_key.end()) {
            set_error(error, PC_RESULT_NOT_FOUND, "mod key not found");
            return PC_RESULT_NOT_FOUND;
        }
        const std::uint32_t global_id = d.mod_global_ids[global->second];
        const auto session_mod =
            context.session->session_id_by_global_id.find(global_id);
        if (session_mod ==
            context.session->session_id_by_global_id.end()) {
            set_error(error, PC_RESULT_NOT_FOUND,
                      "mod is not available in this session");
            return PC_RESULT_NOT_FOUND;
        }
        out_action.mod_id = session_mod->second;
    } else if (request.action_type == PC_ACTION_HARVEST_REFORGE ||
               request.action_type == PC_ACTION_HARVEST_AUGMENT) {
        if (request.target_tag == nullptr) {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "Harvest action requires target_tag");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        const auto tag = d.tag_id_by_name.find(request.target_tag);
        if (tag == d.tag_id_by_name.end()) {
            set_error(error, PC_RESULT_NOT_FOUND, "target tag not found");
            return PC_RESULT_NOT_FOUND;
        }
        const bool real_craft =
            request.action_type == PC_ACTION_HARVEST_REFORGE
                ? poecraft::harvest_reforge_tag_allowed(request.target_tag)
                : poecraft::harvest_augment_tag_allowed(request.target_tag);
        if (!real_craft) {
            set_error(error, PC_RESULT_NOT_FOUND,
                      "Harvest craft is not available for target tag");
            return PC_RESULT_NOT_FOUND;
        }
        out_action.target_tag_id = tag->second;
        if (out_pool != nullptr) {
            out_pool->weight_kind =
                poecraft::PoolWeightKind::HarvestSpawnOnly;
            out_pool->target_tag_id = tag->second;
        }
    } else if (request.action_type == PC_ACTION_HARVEST_RESIST) {
        if (request.source_tag == nullptr || request.target_tag == nullptr) {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "Harvest resistance action requires source_tag and target_tag");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        const auto source = d.tag_id_by_name.find(request.source_tag);
        const auto target = d.tag_id_by_name.find(request.target_tag);
        if (source == d.tag_id_by_name.end() ||
            target == d.tag_id_by_name.end()) {
            set_error(error, PC_RESULT_NOT_FOUND,
                      "Harvest resistance tag not found");
            return PC_RESULT_NOT_FOUND;
        }
        out_action.source_tag_id = source->second;
        out_action.target_tag_id = target->second;
    } else if (request.action_type == PC_ACTION_ELDRITCH_EMBER ||
               request.action_type == PC_ACTION_ELDRITCH_ICHOR) {
        if (request.tier < 1 || request.tier > 4) {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "Eldritch tier must be 1-4");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        out_action.tier = request.tier;
    } else if (request.action_type == PC_ACTION_INFLUENCE_EXALT) {
        if (request.influence == nullptr) {
            set_error(error, PC_RESULT_INVALID_ARGUMENT,
                      "influence exalt requires influence");
            return PC_RESULT_INVALID_ARGUMENT;
        }
        const auto influence =
            d.influence_code_by_name.find(request.influence);
        if (influence == d.influence_code_by_name.end() ||
            influence->second <= 0) {
            set_error(error, PC_RESULT_NOT_FOUND, "influence not found");
            return PC_RESULT_NOT_FOUND;
        }
        out_action.influence_code = influence->second;
        if (out_pool != nullptr)
            out_pool->influence_only_code = influence->second;
    }
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

pc_result pc_data_get_base_display(
    pc_data_handle data,
    uint32_t base_index,
    const char** out_name,
    const char** out_item_class_key,
    pc_error_info* out_error) {
    if (data == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    if (base_index >= d.base_count) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "base index out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (out_name != nullptr) {
        *out_name = d.string_at(d.base_name_sid[base_index]).c_str();
    }
    if (out_item_class_key != nullptr) {
        const auto it = d.item_class_index_by_id.find(
            d.base_item_class_id[base_index]);
        if (it != d.item_class_index_by_id.end()) {
            *out_item_class_key =
                d.string_at(d.item_class_key_sid[it->second]).c_str();
        } else {
            *out_item_class_key = "";
        }
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_data_get_base_drop_level(
    pc_data_handle data,
    uint32_t base_index,
    int32_t* out_drop_level,
    pc_error_info* out_error) {
    if (data == nullptr || out_drop_level == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::DataImpl& d = *data->impl;
    if (base_index >= d.base_count) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "base index out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_drop_level = d.base_drop_levels[base_index];
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
    if (!abi_struct_ok(options->struct_size, options->abi_version,
                       sizeof(pc_session_options), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
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
    out_info->family_id =
        session_mod_id < s.family_id.size() ? s.family_id[session_mod_id] : 0;
    out_info->required_level = s.required_level[session_mod_id];
    const std::uint32_t group = s.primary_group[session_mod_id];
    out_info->group_display_name =
        group < d.group_display_name_sids.size()
            ? d.string_at(d.group_display_name_sids[group]).c_str()
            : "";
    out_info->family_tier_index =
        session_mod_id < s.family_tier_index.size()
            ? s.family_tier_index[session_mod_id]
            : 0;
    const auto& lines = d.mod_text_line_ptrs[p];
    out_info->text_line_count = static_cast<std::uint32_t>(lines.size());
    out_info->text_lines = lines.empty() ? nullptr : lines.data();
    const auto& tags = s.classification_tag_name_ptrs[session_mod_id];
    out_info->classification_tag_count =
        static_cast<std::uint32_t>(tags.size());
    out_info->classification_tags = tags.empty() ? nullptr : tags.data();
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_item_max_prefix(
    pc_session_handle session,
    const pc_item_state* item,
    uint32_t* out_max,
    pc_error_info* out_error) {
    if (session == nullptr || item == nullptr || out_max == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    if (item->rarity == PC_RARITY_NORMAL) {
        *out_max = 0;
    } else if (item->rarity == PC_RARITY_MAGIC) {
        *out_max = 1;
    } else {
        *out_max = s.rare_affix_cap;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_item_max_suffix(
    pc_session_handle session,
    const pc_item_state* item,
    uint32_t* out_max,
    pc_error_info* out_error) {
    return pc_session_item_max_prefix(session, item, out_max, out_error);
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
    case PC_MASK_BASE_EXPLICIT_UNIVERSE:
        mask = &s.base_explicit_universe_mask;
        break;
    case PC_MASK_CRAFTED: mask = &s.crafted_mask; break;
    case PC_MASK_ESSENCE_ONLY: mask = &s.essence_only_mask; break;
    case PC_MASK_IMPLICIT: mask = &s.implicit_mask; break;
    case PC_MASK_DELVE: mask = &s.delve_mask; break;
    case PC_MASK_VEILED_TEMPLATE: mask = &s.veiled_template_mask; break;
    case PC_MASK_UNVEILED: mask = &s.unveiled_mask; break;
    case PC_MASK_CORRUPTED_IMPLICIT:
        mask = &s.corrupted_implicit_mask;
        break;
    case PC_MASK_ELDRITCH_IMPLICIT:
        mask = &s.eldritch_implicit_mask;
        break;
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
    if (options != nullptr &&
        !abi_struct_ok(options->struct_size, options->abi_version,
                       sizeof(pc_action_context_options), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
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
    poecraft::PoolBuildRequest request;
    request.side_filter = side_filter;
    const poecraft::WeightedPool& pool =
        poecraft::get_weighted_pool(*context->impl, item, request);
    *out_count = static_cast<uint32_t>(pool.entries.size());
    if (entries == nullptr || capacity < pool.entries.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < pool.entries.size(); ++i) {
        const poecraft::PoolEntry& src = pool.entries[i];
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
    const poecraft::WeightedPool& pool,
    pc_pool_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    *out_count = static_cast<uint32_t>(pool.entries.size());
    if (entries == nullptr || capacity < pool.entries.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < pool.entries.size(); ++i) {
        const poecraft::PoolEntry& src = pool.entries[i];
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
    poecraft::PoolBuildRequest request;
    request.weight_kind = poecraft::PoolWeightKind::HarvestSpawnOnly;
    request.target_tag_id = it->second;
    request.side_filter = side_filter;
    const poecraft::WeightedPool& pool =
        poecraft::get_weighted_pool(*context->impl, item, request);
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
    const std::vector<std::uint64_t>* mask =
        it->second < s.implicit_tag_masks.size() &&
                !s.implicit_tag_masks[it->second].empty()
            ? &s.implicit_tag_masks[it->second]
            : nullptr;
    const std::size_t total =
        mask == nullptr
            ? 0
            : poecraft::pc_bitset_count(mask->data(), s.words);
    *out_count = static_cast<std::uint32_t>(total);
    if (out_ids == nullptr || capacity < total) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    std::uint32_t write = 0;
    if (mask != nullptr) {
        poecraft::pc_bitset_for_each(
            mask->data(), s.words,
            [&](std::size_t id) {
                if (id < s.mod_count) {
                    out_ids[write++] = static_cast<std::uint32_t>(id);
                }
            });
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_session_dump_influence_mask(
    pc_session_handle session,
    int32_t influence_code,
    uint32_t* out_ids,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (session == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const poecraft::SessionImpl& s = *session->impl;
    if (influence_code < 0 ||
        static_cast<std::size_t>(influence_code) >=
            s.influence_masks.size()) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "influence code out of range");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& mask = s.influence_masks[static_cast<std::size_t>(influence_code)];
    const std::size_t total =
        poecraft::pc_bitset_count(mask.data(), s.words);
    *out_count = static_cast<std::uint32_t>(total);
    if (out_ids == nullptr || capacity < total) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    std::uint32_t write = 0;
    poecraft::pc_bitset_for_each(mask.data(), s.words, [&](std::size_t id) {
        if (id < s.mod_count) out_ids[write++] = static_cast<std::uint32_t>(id);
    });
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_debug_pool_query(
    pc_action_context_handle context,
    const pc_item_state* item,
    const pc_pool_query_request* request,
    pc_pool_debug_entry* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_pool_debug_summary* out_summary,
    pc_error_info* out_error) {
    if (context == nullptr || request == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!abi_struct_ok(request->struct_size, request->abi_version,
                       sizeof(pc_pool_query_request), out_error) ||
        !abi_struct_ok(request->action.struct_size,
                       request->action.abi_version,
                       sizeof(pc_action_request), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (request->side_filter < -1 || request->side_filter > 1) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "bad side filter");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const int action_type = request->action.action_type;
    if (action_type == PC_ACTION_ANNUL ||
        action_type == PC_ACTION_SCOUR ||
        action_type == PC_ACTION_BENCH ||
        action_type == PC_ACTION_VEILED_EXALT ||
        action_type == PC_ACTION_UNVEIL ||
        action_type == PC_ACTION_HARVEST_RESIST ||
        action_type == PC_ACTION_ELDRITCH_EMBER ||
        action_type == PC_ACTION_ELDRITCH_ICHOR ||
        action_type == PC_ACTION_ELDRITCH_ANNUL ||
        action_type == PC_ACTION_FRACTURE ||
        action_type == PC_ACTION_REMOVE_CRAFTED_MODIFIERS) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                  "action has no weighted add pool");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }

    poecraft::ActionParameters action;
    poecraft::PoolBuildRequest pool_request;
    const pc_result parse_result = parse_action_request(
        *context->impl, request->action, action, &pool_request, out_error);
    if (parse_result != PC_RESULT_OK) {
        return parse_result;
    }
    pool_request.side_filter = request->side_filter;

    std::vector<poecraft::PoolDebugRow> rows;
    poecraft::WeightedPool summary;
    bool cache_hit = false;
    poecraft::build_pool_debug_rows(
        *context->impl, item, pool_request, request->include_rejected != 0,
        rows, &summary, &cache_hit);
    *out_count = static_cast<std::uint32_t>(rows.size());

    if (out_summary != nullptr) {
        out_summary->struct_size =
            static_cast<std::uint32_t>(sizeof(pc_pool_debug_summary));
        out_summary->abi_version = PC_ABI_VERSION;
        out_summary->tag_signature_id =
            poecraft::intern_item_tag_signature(*context->impl, item);
        out_summary->cache_hit = cache_hit ? 1 : 0;
        out_summary->candidate_count =
            static_cast<std::uint32_t>(summary.entries.size());
        out_summary->prefix_total_weight = summary.prefix_total_weight;
        out_summary->suffix_total_weight = summary.suffix_total_weight;
        out_summary->combined_total_weight = summary.total_weight;
        out_summary->cache_hits = context->impl->pool_cache_hits;
        out_summary->cache_misses = context->impl->pool_cache_misses;
    }

    if (entries == nullptr || capacity < rows.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    const poecraft::SessionImpl& s = *context->impl->session;
    const poecraft::DataImpl& d = *s.data;
    auto tag_name = [&](std::uint32_t tag_id) -> const char* {
        const auto it = d.tag_name_by_id.find(tag_id);
        return it == d.tag_name_by_id.end() ? "" : it->second.c_str();
    };
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const poecraft::PoolDebugRow& src = rows[i];
        pc_pool_debug_entry& dst = entries[i];
        dst.struct_size = static_cast<std::uint32_t>(sizeof(dst));
        dst.abi_version = PC_ABI_VERSION;
        dst.session_mod_id = src.entry.session_mod_id;
        dst.global_mod_id = src.entry.global_mod_id;
        const std::uint32_t p = s.global_index[src.entry.session_mod_id];
        dst.key = d.string_at(d.mod_key_sid[p]).c_str();
        dst.generation_type = src.entry.gen_type;
        dst.reach_kind = s.reach_kind[src.entry.session_mod_id];
        dst.reach_via = s.reach_via[src.entry.session_mod_id].c_str();
        dst.tag_signature_id = src.tag_signature_id;
        dst.normal_random_member = src.normal_random_member ? 1 : 0;
        dst.side_allowed = src.side_allowed ? 1 : 0;
        dst.mechanic_allowed = src.mechanic_allowed ? 1 : 0;
        dst.influence_allowed = src.influence_allowed ? 1 : 0;
        dst.group_allowed = src.group_allowed ? 1 : 0;
        dst.positively_weighted = src.positively_weighted ? 1 : 0;
        dst.first_failure = src.first_failure;
        dst.blocking_group_id = src.blocking_group_id;
        dst.active_spawn_row = src.active_spawn_row;
        dst.active_spawn_tag = tag_name(src.active_spawn_tag_id);
        dst.active_spawn_weight = src.entry.spawn_weight;
        dst.active_generation_row = src.active_generation_row;
        dst.active_generation_tag = tag_name(src.active_generation_tag_id);
        dst.active_generation_pct = src.entry.generation_pct;
        dst.generation_applied = src.generation_applied ? 1 : 0;
        dst.special_multiplier_pct = src.special_multiplier_pct;
        dst.final_weight = src.entry.final_weight;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_action_context_debug_last_trace(
    pc_action_context_handle context,
    pc_action_trace_stage* stages,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (context == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& trace = context->impl->last_action_trace;
    *out_count = static_cast<std::uint32_t>(trace.size());
    if (stages == nullptr || capacity < trace.size()) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < trace.size(); ++i) {
        const poecraft::ActionTraceStage& src = trace[i];
        pc_action_trace_stage& dst = stages[i];
        dst.struct_size = static_cast<std::uint32_t>(sizeof(dst));
        dst.abi_version = PC_ABI_VERSION;
        dst.stage_index = src.stage_index;
        dst.direct = src.direct ? 1 : 0;
        dst.cache_hit = src.cache_hit ? 1 : 0;
        dst.tag_signature_id = src.tag_signature_id;
        dst.weight_kind = static_cast<std::int32_t>(src.weight_kind);
        dst.side_filter = src.side_filter;
        dst.prefix_total_weight = src.prefix_total_weight;
        dst.suffix_total_weight = src.suffix_total_weight;
        dst.combined_total_weight = src.combined_total_weight;
        dst.roll = src.roll;
        dst.chosen_session_mod_id = src.chosen_mod_id;
        dst.chosen_side = src.chosen_side;
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
    poecraft::ActionParameters action;
    const pc_result parse_result = parse_action_request(
        *context->impl, *request, action, nullptr, out_error);
    if (parse_result != PC_RESULT_OK) {
        return parse_result;
    }
    // Apply to a private copy so a failed action leaves the caller's item intact.
    pc_item_state working = *item;
    const poecraft::ActionOutcome outcome = poecraft::apply_action(
        *context->impl, &working, action);
    if (outcome.applied) {
        *item = working;
    }
    out_result->struct_size = static_cast<uint32_t>(sizeof(pc_action_result));
    out_result->abi_version = PC_ABI_VERSION;
    out_result->applied = outcome.applied ? 1 : 0;
    out_result->added = outcome.added;
    out_result->removed = outcome.removed;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_apply_action_batch(
    pc_action_context_handle context,
    pc_item_state* items,
    uint32_t item_count,
    const pc_action_request* request,
    pc_action_result* results,
    pc_batch_summary* out_summary,
    pc_error_info* out_error) {
    if (context == nullptr || request == nullptr || out_summary == nullptr ||
        (item_count > 0 && items == nullptr)) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    poecraft::ActionParameters action;
    const pc_result parse_result = parse_action_request(
        *context->impl, *request, action, nullptr, out_error);
    if (parse_result != PC_RESULT_OK) {
        return parse_result;
    }

    pc_batch_summary summary{};
    summary.struct_size = static_cast<uint32_t>(sizeof(summary));
    summary.abi_version = PC_ABI_VERSION;
    summary.item_count = item_count;
    for (uint32_t i = 0; i < item_count; ++i) {
        pc_item_state working = items[i];
        const poecraft::ActionOutcome outcome = poecraft::apply_action(
            *context->impl, &working, action);
        if (outcome.applied) {
            items[i] = working;
            ++summary.applied_count;
        }
        summary.total_added += static_cast<uint64_t>(outcome.added);
        summary.total_removed += static_cast<uint64_t>(outcome.removed);
        if (results != nullptr) {
            results[i].struct_size =
                static_cast<uint32_t>(sizeof(pc_action_result));
            results[i].abi_version = PC_ABI_VERSION;
            results[i].applied = outcome.applied ? 1 : 0;
            results[i].added = outcome.added;
            results[i].removed = outcome.removed;
        }
    }
    *out_summary = summary;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_action_context_perf_stats_query(
    pc_action_context_handle context,
    pc_action_perf_stats* out_stats,
    int32_t reset,
    pc_error_info* out_error) {
    if (context == nullptr || out_stats == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    poecraft::ActionContextImpl& impl = *context->impl;
    out_stats->struct_size =
        static_cast<std::uint32_t>(sizeof(pc_action_perf_stats));
    out_stats->abi_version = PC_ABI_VERSION;
    out_stats->pool_requests =
        impl.pool_cache_hits + impl.pool_cache_misses;
    out_stats->cache_hits = impl.pool_cache_hits;
    out_stats->cache_misses = impl.pool_cache_misses;
    out_stats->candidate_build_ns = impl.candidate_build_ns;
    out_stats->weighted_pool_build_ns = impl.weighted_pool_build_ns;
    out_stats->sampling_calls = impl.sampling_calls;
    out_stats->sampling_ns = impl.sampling_ns;
    if (reset) {
        impl.pool_cache_hits = 0;
        impl.pool_cache_misses = 0;
        impl.candidate_build_ns = 0;
        impl.weighted_pool_build_ns = 0;
        impl.sampling_calls = 0;
        impl.sampling_ns = 0;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_action_context_perf_timing_set(
    pc_action_context_handle context,
    int32_t enabled,
    pc_error_info* out_error) {
    if (context == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null context");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    context->impl->perf_timing_enabled = enabled != 0;
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
    if (options != nullptr &&
        !abi_struct_ok(options->struct_size, options->abi_version,
                       sizeof(pc_item_init_options), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const bool want_implicits = options != nullptr && options->with_implicits;
    pc_item_state scratch;
    pc_item_clear(&scratch);
    scratch.rarity = options != nullptr
                         ? options->rarity
                         : static_cast<uint8_t>(PC_RARITY_NORMAL);
    if (want_implicits) {
        const poecraft::SessionImpl& s = *session->impl;
        if (s.base_implicit_mod_ids.size() > PC_MAX_IMPLICITS) {
            set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED,
                      "base implicit count exceeds item capacity");
            return PC_RESULT_CAPACITY_EXCEEDED;
        }
        for (std::uint32_t mod_id : s.base_implicit_mod_ids) {
            pc_mod_slot* slot = &scratch.implicits[scratch.implicit_count++];
            slot->mod_id = mod_id;
            slot->group_id =
                static_cast<std::uint16_t>(s.primary_group[mod_id]);
        }
    }
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

    // Use the session's jewel-aware cap (2 affixes/side for rare jewels) rather
    // than the item-only pc_item_max_prefix helper, which assumes 3.
    unsigned cap = 0;
    if (item->rarity == PC_RARITY_MAGIC) {
        cap = 1;
    } else if (item->rarity == PC_RARITY_RARE) {
        cap = s.rare_affix_cap;
    }

    std::ostringstream out;
    out << "item: " << d.string_at(d.base_name_sid[i]) << " (ilvl "
        << s.item_level << ")\n";
    out << "  rarity: " << rarity << ", quality: "
        << static_cast<unsigned>(item->quality) << "\n";
    out << "  prefixes: " << static_cast<unsigned>(item->prefix_count) << "/"
        << cap << "\n";
    out << "  suffixes: " << static_cast<unsigned>(item->suffix_count) << "/"
        << cap << "\n";
    out << "  implicits: " << static_cast<unsigned>(item->implicit_count)
        << ", enchantments: "
        << static_cast<unsigned>(item->enchantment_count) << "\n";
    return emit_text(out.str(), buffer, buffer_size, out_length, out_error);
}

// --- native strategy simulator ---------------------------------------------

pc_result pc_strategy_compile_json(
    pc_session_handle session,
    const char* strategy_json,
    size_t strategy_json_size,
    pc_strategy_handle* out_strategy,
    pc_error_info* out_error) {
    if (session == nullptr || strategy_json == nullptr ||
        out_strategy == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_strategy = nullptr;
    try {
        auto holder = std::make_unique<pc_strategy>();
        holder->impl = poecraft::compile_strategy_json(
            session->impl, strategy_json, strategy_json_size);
        *out_strategy = holder.release();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_DATA_ERROR, ex.what());
        return PC_RESULT_DATA_ERROR;
    }
}

void pc_strategy_destroy(pc_strategy_handle strategy) {
    delete strategy;
}

pc_result pc_economy_load_json(
    const char* economy_json,
    size_t economy_json_size,
    pc_economy_handle* out_economy,
    pc_error_info* out_error) {
    if (economy_json == nullptr || out_economy == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_economy = nullptr;
    try {
        auto holder = std::make_unique<pc_economy>();
        holder->impl =
            poecraft::load_economy_json(economy_json, economy_json_size);
        *out_economy = holder.release();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_DATA_ERROR, ex.what());
        return PC_RESULT_DATA_ERROR;
    }
}

void pc_economy_destroy(pc_economy_handle economy) {
    delete economy;
}

pc_result pc_simulator_create(
    pc_session_handle session,
    pc_strategy_handle strategy,
    pc_economy_handle economy,
    pc_simulator_handle* out_simulator,
    pc_error_info* out_error) {
    if (session == nullptr || strategy == nullptr || out_simulator == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_simulator = nullptr;
    if (strategy->impl->session.get() != session->impl.get()) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "strategy belongs to a different session");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    auto holder = std::make_unique<pc_simulator>();
    holder->impl = std::make_unique<poecraft::SimulatorImpl>();
    holder->impl->session = session->impl;
    holder->impl->strategy = strategy->impl;
    if (economy != nullptr) holder->impl->economy = economy->impl;
    prepare_simulator_runtime(*holder->impl);
    *out_simulator = holder.release();
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_run_chunk(
    pc_simulator_handle simulator,
    const pc_simulation_options* options,
    uint32_t max_completed_runs,
    pc_simulation_progress* out_progress,
    pc_error_info* out_error) {
    if (simulator == nullptr || options == nullptr || out_progress == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!abi_struct_ok(options->struct_size, options->abi_version,
                       sizeof(pc_simulation_options), out_error)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (options->target_runs == 0 || options->max_actions_per_run == 0) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "target_runs and max_actions_per_run must be positive");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (options->max_actions_per_run > 1000000 ||
        options->max_graph_steps_per_run > 4000000) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "per-run safety limit exceeds engine maximum");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!std::isfinite(options->max_cost_per_run) ||
        options->max_cost_per_run < 0.0) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "max_cost_per_run must be finite and non-negative");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (options->max_cost_per_run > 0.0 &&
        simulator->impl->economy == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "a cost limit requires an economy snapshot");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (options->retained_trace_count > 0 &&
        options->max_trace_entries == 0) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "retained traces require max_trace_entries");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const std::uint64_t retained_trace_entries =
        static_cast<std::uint64_t>(options->retained_trace_count) *
        options->max_trace_entries;
    const std::uint64_t retained_examples =
        static_cast<std::uint64_t>(options->retained_success_count) +
        options->retained_failure_count;
    if (options->retained_trace_count > 1000 ||
        options->max_trace_entries > 10000 ||
        retained_trace_entries > 50000 ||
        retained_examples > 10000) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "retention option exceeds safety limit");
        return PC_RESULT_INVALID_ARGUMENT;
    }

    poecraft::SimulationOptionsInternal parsed;
    parsed.target_runs = options->target_runs;
    parsed.seed = options->seed;
    parsed.max_actions_per_run = options->max_actions_per_run;
    parsed.max_graph_steps_per_run = options->max_graph_steps_per_run;
    parsed.max_cost_per_run = options->max_cost_per_run;
    parsed.retained_trace_count = options->retained_trace_count;
    parsed.max_trace_entries = options->max_trace_entries;
    parsed.retained_success_count = options->retained_success_count;
    parsed.retained_failure_count = options->retained_failure_count;
    try {
        poecraft::run_simulator_chunk(
            *simulator->impl, parsed, max_completed_runs);
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what());
        return PC_RESULT_INVALID_ARGUMENT;
    }
    out_progress->struct_size =
        static_cast<std::uint32_t>(sizeof(pc_simulation_progress));
    out_progress->abi_version = PC_ABI_VERSION;
    out_progress->completed_runs = simulator->impl->summary.completed_runs;
    out_progress->target_runs = parsed.target_runs;
    out_progress->finished =
        out_progress->completed_runs >= out_progress->target_runs ? 1 : 0;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_get_summary(
    pc_simulator_handle simulator,
    pc_simulation_summary* out_summary,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_summary == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& src = simulator->impl->summary;
    out_summary->struct_size =
        static_cast<std::uint32_t>(sizeof(pc_simulation_summary));
    out_summary->abi_version = PC_ABI_VERSION;
    out_summary->completed_runs = src.completed_runs;
    out_summary->success_count = src.success_count;
    out_summary->failure_count = src.failure_count;
    out_summary->stop_count = src.stop_count;
    out_summary->total_actions = src.total_actions;
    out_summary->action_limit_count = src.action_limit_count;
    out_summary->cost_limit_count = src.cost_limit_count;
    out_summary->step_limit_count = src.step_limit_count;
    out_summary->no_matching_edge_count = src.no_matching_edge_count;
    out_summary->action_not_applied_count = src.action_not_applied_count;
    out_summary->missing_price_run_count = src.missing_price_run_count;
    out_summary->costed_action_count = src.costed_action_count;
    out_summary->missing_price_action_count = src.missing_price_action_count;
    out_summary->known_total_cost = src.known_total_cost;
    out_summary->cost_status =
        simulator->impl->economy == nullptr
            ? PC_COST_DISABLED
            : (src.missing_price_action_count == 0 ? PC_COST_COMPLETE
                                                    : PC_COST_INCOMPLETE);
    out_summary->seed = simulator->impl->options_set
                            ? simulator->impl->options.seed
                            : 0;
    out_summary->target_runs = simulator->impl->options_set
                                   ? simulator->impl->options.target_runs
                                   : 0;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_missing_price_query(
    pc_simulator_handle simulator,
    pc_price_key_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_entry_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::vector<std::string> keys;
    keys.reserve(simulator->impl->missing_prices.size());
    for (const auto& entry : simulator->impl->missing_prices) {
        keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    *out_entry_count = static_cast<std::uint32_t>(keys.size());
    if (entry_capacity < keys.size() || (entries == nullptr && !keys.empty())) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        entries[i].struct_size = static_cast<std::uint32_t>(sizeof(entries[i]));
        entries[i].abi_version = PC_ABI_VERSION;
        const auto found = simulator->impl->missing_prices.find(keys[i]);
        entries[i].key = found->first.c_str();
        entries[i].missing_count = found->second;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_get_trace_count(
    pc_simulator_handle simulator,
    uint32_t* out_trace_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_trace_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_trace_count =
        static_cast<std::uint32_t>(simulator->impl->traces.size());
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_trace_query(
    pc_simulator_handle simulator,
    uint32_t trace_index,
    pc_trace_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_entry_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (trace_index >= simulator->impl->traces.size()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "trace index out of range");
        return PC_RESULT_NOT_FOUND;
    }
    const auto& trace = simulator->impl->traces[trace_index].entries;
    *out_entry_count = static_cast<std::uint32_t>(trace.size());
    if (entry_capacity < trace.size() || (entries == nullptr && !trace.empty())) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < trace.size(); ++i) {
        const auto& src = trace[i];
        auto& dst = entries[i];
        dst.struct_size = static_cast<std::uint32_t>(sizeof(dst));
        dst.abi_version = PC_ABI_VERSION;
        dst.step_index = src.step_index;
        dst.node_id = src.node_id.c_str();
        dst.node_kind = src.node_kind;
        dst.action_type = src.action_type;
        dst.action_applied = src.action_applied ? 1 : 0;
        dst.matched_edge_id = src.matched_edge_id.c_str();
        dst.cumulative_actions = src.cumulative_actions;
        dst.known_cumulative_cost = src.known_cumulative_cost;
        dst.cost_complete = src.cost_complete ? 1 : 0;
        dst.terminal_kind = src.terminal_kind;
        dst.failure_reason = src.failure_reason;
        dst.item = src.item;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_get_example_count(
    pc_simulator_handle simulator,
    int32_t terminal_kind,
    uint32_t* out_example_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_example_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (terminal_kind < PC_TERMINAL_SUCCESS ||
        terminal_kind > PC_TERMINAL_STOP) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "unknown terminal kind");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& examples =
        terminal_kind == PC_TERMINAL_SUCCESS
            ? simulator->impl->success_examples
            : simulator->impl->failure_examples;
    std::uint32_t count = 0;
    for (const auto& example : examples) {
        if (example.terminal_kind == terminal_kind) ++count;
    }
    *out_example_count = count;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_example_query(
    pc_simulator_handle simulator,
    int32_t terminal_kind,
    uint32_t example_index,
    pc_simulation_example* out_example,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_example == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (terminal_kind < PC_TERMINAL_SUCCESS ||
        terminal_kind > PC_TERMINAL_STOP) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "unknown terminal kind");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& examples =
        terminal_kind == PC_TERMINAL_SUCCESS
            ? simulator->impl->success_examples
            : simulator->impl->failure_examples;
    const poecraft::SimulationExampleInternal* selected = nullptr;
    std::uint32_t seen = 0;
    for (const auto& example : examples) {
        if (example.terminal_kind != terminal_kind) continue;
        if (seen++ == example_index) {
            selected = &example;
            break;
        }
    }
    if (selected == nullptr) {
        set_error(out_error, PC_RESULT_NOT_FOUND,
                  "example index out of range");
        return PC_RESULT_NOT_FOUND;
    }
    out_example->struct_size =
        static_cast<std::uint32_t>(sizeof(pc_simulation_example));
    out_example->abi_version = PC_ABI_VERSION;
    out_example->terminal_kind = selected->terminal_kind;
    out_example->failure_reason = selected->failure_reason;
    out_example->terminal_node_id = selected->terminal_node_id.c_str();
    out_example->action_count = selected->action_count;
    out_example->known_total_cost = selected->known_total_cost;
    out_example->cost_complete = selected->cost_complete ? 1 : 0;
    out_example->item = selected->item;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_failure_summary_query(
    pc_simulator_handle simulator,
    pc_failure_summary_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_entry_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& summaries = simulator->impl->failure_summaries;
    *out_entry_count = static_cast<std::uint32_t>(summaries.size());
    if (entry_capacity < summaries.size() ||
        (entries == nullptr && !summaries.empty())) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        entries[i].struct_size = static_cast<std::uint32_t>(sizeof(entries[i]));
        entries[i].abi_version = PC_ABI_VERSION;
        entries[i].failure_reason = summaries[i].failure_reason;
        entries[i].node_id = summaries[i].node_id.c_str();
        entries[i].detail = summaries[i].detail.c_str();
        entries[i].count = summaries[i].count;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_action_distribution_query(
    pc_simulator_handle simulator,
    pc_action_distribution_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_entry_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::vector<std::size_t> node_indices;
    for (std::size_t i = 0; i < simulator->impl->action_counts.size(); ++i) {
        if (simulator->impl->action_counts[i] != 0) {
            node_indices.push_back(i);
        }
    }
    std::sort(
        node_indices.begin(),
        node_indices.end(),
        [&](std::size_t left, std::size_t right) {
            const auto left_count = simulator->impl->action_counts[left];
            const auto right_count = simulator->impl->action_counts[right];
            if (left_count != right_count) return left_count > right_count;
            return simulator->impl->strategy->nodes[left].id <
                   simulator->impl->strategy->nodes[right].id;
        });
    *out_entry_count = static_cast<std::uint32_t>(node_indices.size());
    if (entry_capacity < node_indices.size() ||
        (entries == nullptr && !node_indices.empty())) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < node_indices.size(); ++i) {
        const std::size_t node_index = node_indices[i];
        const auto& node = simulator->impl->strategy->nodes[node_index];
        entries[i].struct_size =
            static_cast<std::uint32_t>(sizeof(entries[i]));
        entries[i].abi_version = PC_ABI_VERSION;
        entries[i].node_id = node.id.c_str();
        entries[i].action_type = node.action_type;
        entries[i].count = simulator->impl->action_counts[node_index];
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_action_descriptor_distribution_query(
    pc_simulator_handle simulator,
    pc_action_descriptor_sample_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_entry_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::vector<std::string> keys;
    for (const auto& [key, count] :
         simulator->impl->action_descriptor_counts) {
        if (count != 0) keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    *out_entry_count = static_cast<std::uint32_t>(keys.size());
    if (entry_capacity < keys.size() || (entries == nullptr && !keys.empty())) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto found =
            simulator->impl->action_descriptor_counts.find(keys[i]);
        entries[i].struct_size = static_cast<std::uint32_t>(sizeof(entries[i]));
        entries[i].abi_version = PC_ABI_VERSION;
        entries[i].action_id = found->first.c_str();
        entries[i].count = found->second;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_simulator_material_distribution_query(
    pc_simulator_handle simulator,
    pc_material_sample_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error) {
    if (simulator == nullptr || out_entry_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::vector<std::string> keys;
    for (const auto& [key, count] : simulator->impl->material_counts) {
        if (count != 0) keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    *out_entry_count = static_cast<std::uint32_t>(keys.size());
    if (entry_capacity < keys.size() || (entries == nullptr && !keys.empty())) {
        set_error(out_error, PC_RESULT_BUFFER_TOO_SMALL, "buffer too small");
        return PC_RESULT_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto found = simulator->impl->material_counts.find(keys[i]);
        entries[i].struct_size = static_cast<std::uint32_t>(sizeof(entries[i]));
        entries[i].abi_version = PC_ABI_VERSION;
        entries[i].price_key = found->first.c_str();
        entries[i].count = found->second;
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

void pc_simulator_destroy(pc_simulator_handle simulator) {
    delete simulator;
}
