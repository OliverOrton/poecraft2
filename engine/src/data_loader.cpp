#include "engine_internal.hpp"
#include "json.hpp"

#include <stdexcept>

namespace poecraft {

namespace {

using json::Value;

std::uint32_t read_count(const Value& section) {
    return static_cast<std::uint32_t>(section.at("count").as_int());
}

void read_u32(const Value& parent, const char* key,
              std::vector<std::uint32_t>& out) {
    json::read_int_array(parent.at(key), out);
}

void read_i32(const Value& parent, const char* key,
              std::vector<std::int32_t>& out) {
    json::read_int_array(parent.at(key), out);
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

static std::shared_ptr<DataImpl> build_data_impl(
    const Value& manifest,
    const Value& strings,
    const Value& game) {
    auto data = std::make_shared<DataImpl>();

    // --- manifest -----------------------------------------------------------
    data->artifact_schema_version = static_cast<std::uint32_t>(
        manifest.at("artifact_schema_version").as_int());
    const Value* complete = manifest.find("complete_dataset");
    data->complete_dataset = complete != nullptr &&
                             complete->type == json::Type::Bool &&
                             complete->boolean;

    const Value& counts = manifest.at("row_counts");
    auto count = [&](const char* key) -> std::uint32_t {
        const Value* v = counts.find(key);
        return v ? static_cast<std::uint32_t>(v->as_int()) : 0;
    };
    data->count_tags = count("tags");
    data->count_item_classes = count("item_classes");
    data->count_base_items = count("base_items");
    data->count_mods = count("mods");
    data->count_groups = count("groups");
    data->count_ordinary_bases = count("ordinary_session_bases");
    data->count_cluster_bases = count("cluster_unsupported_bases");
    data->count_unsupported_domain_bases = count("unsupported_domain_bases");

    // domain enum reverse map (name -> code  =>  code -> name)
    const Value& enums = manifest.at("enums");
    const Value& domain_enum = enums.at("domain");
    require(domain_enum.type == json::Type::Object, "enums.domain must be object");
    std::int64_t max_code = -1;
    for (const auto& member : domain_enum.object) {
        max_code = std::max<std::int64_t>(max_code, member.second.as_int());
    }
    data->domain_name_by_code.assign(static_cast<std::size_t>(max_code + 1),
                                     std::string());
    for (const auto& member : domain_enum.object) {
        data->domain_name_by_code[static_cast<std::size_t>(
            member.second.as_int())] = member.first;
        data->domain_code_by_name[member.first] =
            static_cast<int>(member.second.as_int());
    }

    // generation_type enum: locate the prefix/suffix codes.
    const Value& gen_enum = enums.at("generation_type");
    for (const auto& member : gen_enum.object) {
        if (member.first == "prefix") {
            data->gen_prefix_code = static_cast<int>(member.second.as_int());
        } else if (member.first == "suffix") {
            data->gen_suffix_code = static_cast<int>(member.second.as_int());
        }
    }

    // influence enum reverse map (includes "none" at 0).
    const Value& influence_enum = enums.at("influence");
    std::int64_t max_inf = -1;
    for (const auto& member : influence_enum.object) {
        max_inf = std::max<std::int64_t>(max_inf, member.second.as_int());
    }
    data->influence_name_by_code.assign(static_cast<std::size_t>(max_inf + 1),
                                        std::string());
    for (const auto& member : influence_enum.object) {
        data->influence_name_by_code[static_cast<std::size_t>(
            member.second.as_int())] = member.first;
    }
    const Value& fossil_weight_kind = enums.at("fossil_weight_kind");
    for (const auto& member : fossil_weight_kind.object) {
        if (member.first == "negative") {
            data->fossil_weight_negative_code =
                static_cast<int>(member.second.as_int());
        } else if (member.first == "positive") {
            data->fossil_weight_positive_code =
                static_cast<int>(member.second.as_int());
        }
    }
    const Value& fossil_mod_kind = enums.at("fossil_mod_kind");
    for (const auto& member : fossil_mod_kind.object) {
        if (member.first == "added") {
            data->fossil_mod_added_code =
                static_cast<int>(member.second.as_int());
        } else if (member.first == "forced") {
            data->fossil_mod_forced_code =
                static_cast<int>(member.second.as_int());
        }
    }

    // --- strings ------------------------------------------------------------
    const json::Array& string_array = strings.at("strings").as_array();
    data->strings.reserve(string_array.size());
    for (const auto& element : string_array) {
        data->strings.push_back(element.as_string());
    }
    const std::uint32_t declared_strings =
        static_cast<std::uint32_t>(strings.at("count").as_int());
    require(declared_strings == data->strings.size(),
            "strings.count does not match strings array length");

    // --- tags ---------------------------------------------------------------
    const Value& tags = game.at("tags");
    std::vector<std::uint32_t> tag_global_ids;
    std::vector<std::uint32_t> tag_name_sids;
    read_u32(tags, "global_tag_ids", tag_global_ids);
    read_u32(tags, "name_string_ids", tag_name_sids);
    require(tag_global_ids.size() == tag_name_sids.size(),
            "tag arrays inconsistent");
    data->tag_id_by_name.reserve(tag_global_ids.size());
    data->tag_name_by_id.reserve(tag_global_ids.size());
    for (std::size_t i = 0; i < tag_global_ids.size(); ++i) {
        const std::string& name = data->string_at(tag_name_sids[i]);
        data->tag_id_by_name.emplace(name, tag_global_ids[i]);
        data->tag_name_by_id.emplace(tag_global_ids[i], name);
    }

    // --- item classes -------------------------------------------------------
    const Value& item_classes = game.at("item_classes");
    data->item_class_count = read_count(item_classes);
    read_u32(item_classes, "global_item_class_ids", data->item_class_global_ids);
    read_u32(item_classes, "key_string_ids", data->item_class_key_sid);
    require(data->item_class_global_ids.size() == data->item_class_count,
            "item_classes arrays inconsistent");
    for (std::uint32_t i = 0; i < data->item_class_count; ++i) {
        data->item_class_index_by_id[data->item_class_global_ids[i]] = i;
    }

    // --- exclusivity groups -------------------------------------------------
    const Value& groups = game.at("groups");
    read_u32(groups, "key_string_ids", data->group_key_sids);
    require(data->group_key_sids.size() == data->count_groups,
            "groups arrays inconsistent");
    read_u32(groups, "display_name_string_ids", data->group_display_name_sids);
    require(
        data->group_display_name_sids.size() == data->count_groups,
        "groups display_name_string_ids length mismatch");
    data->group_id_by_key.reserve(data->group_key_sids.size());
    for (std::uint32_t i = 0; i < data->group_key_sids.size(); ++i) {
        data->group_id_by_key.emplace(data->string_at(data->group_key_sids[i]), i);
    }

    // --- base items ---------------------------------------------------------
    const Value& bases = game.at("base_items");
    data->base_count = read_count(bases);
    read_u32(bases, "global_base_item_ids", data->base_global_ids);
    read_u32(bases, "metadata_path_string_ids", data->base_metadata_path_sid);
    read_u32(bases, "name_string_ids", data->base_name_sid);
    read_u32(bases, "global_item_class_ids", data->base_item_class_id);
    read_i32(bases, "domain_codes", data->base_domain_code);
    read_i32(bases, "session_support_codes", data->base_session_support);
    read_i32(bases, "flags", data->base_flags);
    read_u32(bases, "tag_offsets", data->base_tag_offsets);
    read_u32(bases, "tag_ids", data->base_tag_ids);
    read_u32(bases, "implicit_offsets", data->base_implicit_offsets);
    read_i32(bases, "implicit_global_mod_ids",
             data->base_implicit_global_mod_ids);
    require(data->base_global_ids.size() == data->base_count &&
                data->base_metadata_path_sid.size() == data->base_count &&
                data->base_session_support.size() == data->base_count,
            "base_items arrays inconsistent");
    require(data->base_tag_offsets.size() == data->base_count + 1,
            "base tag_offsets must be base_count + 1");
    require(data->base_implicit_offsets.size() == data->base_count + 1,
            "base implicit_offsets must be base_count + 1");
    require(data->base_implicit_global_mod_ids.size() ==
                data->base_implicit_offsets.back(),
            "base implicit ids length must match implicit_offsets");
    data->base_by_path.reserve(data->base_count);
    for (std::uint32_t i = 0; i < data->base_count; ++i) {
        data->base_by_path.emplace(
            data->string_at(data->base_metadata_path_sid[i]), i);
    }

    // --- mods ---------------------------------------------------------------
    const Value& mods = game.at("mods");
    data->mod_count = read_count(mods);
    read_u32(mods, "global_mod_ids", data->mod_global_ids);
    read_u32(mods, "key_string_ids", data->mod_key_sid);
    read_i32(mods, "generation_type_codes", data->mod_gen_type_code);
    read_i32(mods, "domain_codes", data->mod_domain_code);
    read_u32(mods, "required_levels", data->mod_required_level);
    read_u32(mods, "primary_group_ids", data->mod_primary_group);
    read_i32(mods, "flags", data->mod_flags);
    read_i32(mods, "influence_codes", data->mod_influence_code);
    read_u32(mods, "group_offsets", data->mod_group_offsets);
    read_u32(mods, "group_ids", data->mod_group_ids_flat);
    require(data->mod_group_offsets.size() == data->mod_count + 1,
            "mod group_offsets must be mod_count + 1");
    require(data->mod_group_ids_flat.size() == data->mod_group_offsets.back(),
            "mod group_ids length must match group_offsets");
    read_u32(mods, "text_line_offsets", data->mod_text_line_offsets);
    read_u32(mods, "text_line_string_ids", data->mod_text_line_sids);
    require(data->mod_text_line_offsets.size() == data->mod_count + 1,
            "mod text_line_offsets must be mod_count + 1");
    require(
        data->mod_text_line_sids.size() == data->mod_text_line_offsets.back(),
        "mod text_line_string_ids length must match offsets");
    require(data->mod_key_sid.size() == data->mod_count &&
                data->mod_gen_type_code.size() == data->mod_count &&
                data->mod_domain_code.size() == data->mod_count &&
                data->mod_required_level.size() == data->mod_count &&
                data->mod_primary_group.size() == data->mod_count &&
                data->mod_influence_code.size() == data->mod_count,
            "mod parallel arrays inconsistent");
    data->mod_pos_by_global_id.reserve(data->mod_count);
    data->mod_pos_by_key.reserve(data->mod_count);
    for (std::uint32_t i = 0; i < data->mod_count; ++i) {
        data->mod_pos_by_global_id.emplace(data->mod_global_ids[i], i);
        data->mod_pos_by_key.emplace(data->string_at(data->mod_key_sid[i]), i);
    }

    // ordered spawn/generation weight rows
    const Value& spawn = game.at("spawn_weights");
    read_u32(spawn, "offsets", data->spawn_offsets);
    read_u32(spawn, "tag_ids", data->spawn_tag_ids);
    read_i32(spawn, "weights", data->spawn_weights);
    require(data->spawn_offsets.size() == data->mod_count + 1,
            "spawn_weights offsets must be mod_count + 1");
    const Value& generation = game.at("generation_weights");
    read_u32(generation, "offsets", data->gen_offsets);
    read_u32(generation, "tag_ids", data->gen_tag_ids);
    read_i32(generation, "weights", data->gen_weights);
    require(data->gen_offsets.size() == data->mod_count + 1,
            "generation_weights offsets must be mod_count + 1");
    const Value& classification = game.at("classification_tags");
    read_u32(classification, "offsets", data->class_offsets);
    read_u32(classification, "tag_ids", data->class_tag_ids);
    require(data->class_offsets.size() == data->mod_count + 1,
            "classification_tags offsets must be mod_count + 1");

    const Value& stats = game.at("stats");
    read_u32(stats, "offsets", data->stat_offsets);
    read_u32(stats, "stat_key_string_ids", data->stat_key_sids);
    require(data->stat_offsets.size() == data->mod_count + 1,
            "stats offsets must be mod_count + 1");
    require(data->stat_key_sids.size() == data->stat_offsets.back(),
            "stats stat_key_string_ids length must match offsets");

    // --- bench options ------------------------------------------------------
    const Value* bench = game.find("bench_options");
    if (bench != nullptr) {
        read_i32(*bench, "global_mod_ids", data->bench_global_mod_ids);
        read_u32(*bench, "item_class_offsets", data->bench_class_offsets);
        read_i32(*bench, "item_class_global_ids",
                 data->bench_class_global_ids);
    }

    // --- essences -----------------------------------------------------------
    const Value* essences = game.find("essences");
    if (essences != nullptr) {
        data->essence_count = read_count(*essences);
        read_u32(*essences, "key_string_ids", data->essence_key_sids);
        read_i32(*essences, "item_level_restrictions",
                 data->essence_item_level_restrictions);
        read_u32(*essences, "mod_offsets", data->essence_mod_offsets);
        read_u32(*essences, "item_class_key_string_ids",
                 data->essence_class_key_sids);
        read_i32(*essences, "linked_global_mod_ids",
                 data->essence_linked_global_mod_ids);
        for (std::uint32_t i = 0; i < data->essence_count; ++i) {
            data->essence_by_key.emplace(
                data->string_at(data->essence_key_sids[i]), i);
        }
    }

    // --- fossils ------------------------------------------------------------
    const Value* fossils = game.find("fossils");
    if (fossils != nullptr) {
        data->fossil_count = read_count(*fossils);
        read_u32(*fossils, "key_string_ids", data->fossil_key_sids);
        read_i32(*fossils, "rolls_lucky", data->fossil_rolls_lucky);
        read_u32(*fossils, "weight_offsets", data->fossil_weight_offsets);
        read_i32(*fossils, "weight_kind_codes",
                 data->fossil_weight_kind_codes);
        read_u32(*fossils, "weight_tag_ids", data->fossil_weight_tag_ids);
        read_i32(*fossils, "weight_values", data->fossil_weight_values);
        read_u32(*fossils, "mod_offsets", data->fossil_mod_offsets);
        read_i32(*fossils, "mod_kind_codes", data->fossil_mod_kind_codes);
        read_i32(*fossils, "linked_global_mod_ids",
                 data->fossil_linked_global_mod_ids);
        require(data->fossil_rolls_lucky.size() == data->fossil_count,
                "fossil rolls_lucky length must match fossil count");
        for (std::uint32_t i = 0; i < data->fossil_count; ++i) {
            data->fossil_by_key.emplace(
                data->string_at(data->fossil_key_sids[i]), i);
        }
    }

    // sanity: manifest counts agree with game-data section counts
    require(data->base_count == data->count_base_items,
            "base_items count disagrees with manifest");
    require(data->mod_count == data->count_mods,
            "mods count disagrees with manifest");

    // Build stable c_str() pointer arrays for mod text lines. Strings are
    // never reassigned after this point so the pointers stay valid for the
    // lifetime of the shared DataImpl.
    data->mod_text_line_ptrs.resize(data->mod_count);
    for (std::uint32_t i = 0; i < data->mod_count; ++i) {
        const std::uint32_t begin = data->mod_text_line_offsets[i];
        const std::uint32_t end = data->mod_text_line_offsets[i + 1];
        auto& slot = data->mod_text_line_ptrs[i];
        slot.reserve(end - begin);
        for (std::uint32_t j = begin; j < end; ++j) {
            slot.push_back(data->string_at(data->mod_text_line_sids[j]).c_str());
        }
    }

    return data;
}

std::shared_ptr<DataImpl> load_data_impl(
    const std::string& manifest_text,
    const std::string& strings_text,
    const std::string& game_data_text) {
    Value manifest =
        json::Parser(manifest_text.data(), manifest_text.size()).parse();
    Value strings =
        json::Parser(strings_text.data(), strings_text.size()).parse();
    Value game =
        json::Parser(game_data_text.data(), game_data_text.size()).parse();
    return build_data_impl(manifest, strings, game);
}

std::shared_ptr<DataImpl> load_data_impl_bundle(const std::string& bundle_text) {
    Value bundle = json::Parser(bundle_text.data(), bundle_text.size()).parse();
    const Value* manifest = bundle.find("manifest");
    const Value* strings = bundle.find("strings");
    const Value* game = bundle.find("game_data");
    if (manifest == nullptr || strings == nullptr || game == nullptr) {
        throw std::runtime_error(
            "memory bundle must contain manifest, strings, and game_data");
    }
    return build_data_impl(*manifest, *strings, *game);
}

} // namespace poecraft
