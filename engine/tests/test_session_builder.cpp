#include "tests.hpp"

#include "poecraft/api.h"
#include "poecraft/session.h"

#include "../src/json.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

namespace json = poecraft::json;

std::string slurp(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

json::Value load_fixture(const std::string& path) {
    const std::string text = slurp(path);
    return json::Parser(text.data(), text.size()).parse();
}

// All session mod info, indexed by session mod id.
struct ModRow {
    std::string key;
    std::string reach_via;
    int gen_type = 0;
    int reach_kind = 0;
    int reach_influence = -1;
    uint32_t primary_group = 0;
    uint32_t family_id = 0;
    uint32_t family_tier = 0;
    uint32_t classification_tag_count = 0;
};

std::vector<ModRow> read_session_mods(pc_session_handle session) {
    pc_error_info error;
    uint32_t count = 0;
    pc_session_get_mod_count(session, &count, &error);
    std::vector<ModRow> rows(count);
    for (uint32_t i = 0; i < count; ++i) {
        pc_mod_info info;
        pc_session_get_mod_info(session, i, &info, &error);
        rows[i].key = info.key;
        rows[i].reach_via = info.reach_via;
        rows[i].gen_type = info.generation_type;
        rows[i].reach_kind = info.reach_kind;
        rows[i].reach_influence = info.reach_influence;
        rows[i].primary_group = info.primary_group_id;
        rows[i].family_id = info.family_id;
        rows[i].family_tier = info.family_tier_index;
        rows[i].classification_tag_count = info.classification_tag_count;
        PC_CHECK(info.family_tier_index > 0);
        if (info.classification_tag_count > 0) {
            PC_CHECK(info.classification_tags != nullptr);
            for (uint32_t tag = 0; tag < info.classification_tag_count; ++tag) {
                PC_CHECK(info.classification_tags[tag] != nullptr);
                PC_CHECK(info.classification_tags[tag][0] != '\0');
            }
        }
    }
    return rows;
}

const char* gen_name(int gen_type) {
    return gen_type == 0 ? "prefix" : "suffix";
}

void check_universe(pc_session_handle session, const std::string& fixture_dir) {
    json::Value fx = load_fixture(
        fixture_dir + "/session-pools/vaal-regalia-ilvl-86-session-universe.json");
    const std::vector<ModRow> rows = read_session_mods(session);
    pc_error_info error;
    uint32_t base_count = 0;
    pc_session_dump_mask(session, PC_MASK_BASE_EXPLICIT_UNIVERSE, nullptr, 0,
                         &base_count, &error);
    std::vector<uint32_t> base_ids(base_count);
    pc_session_dump_mask(session, PC_MASK_BASE_EXPLICIT_UNIVERSE,
                         base_ids.data(), base_count, &base_count, &error);

    // The historical fixture pins the base explicit universe. The full session
    // universe now also includes crafted/essence/implicit/delve direct rows.
    const auto& counts = fx.at("counts");
    int ordinary = 0;
    int influence = 0;
    for (uint32_t id : base_ids) {
        const auto& r = rows[id];
        if (r.reach_via == "base") {
            ++ordinary;
        } else {
            ++influence;
        }
    }
    PC_CHECK(static_cast<int>(base_ids.size()) == counts.at("total").as_int());
    PC_CHECK(ordinary == counts.at("ordinary").as_int());
    PC_CHECK(influence == counts.at("influence").as_int());
    PC_CHECK(rows.size() > base_ids.size());

    auto mask_count = [&](int kind) {
        uint32_t count = 0;
        pc_session_dump_mask(session, kind, nullptr, 0, &count, &error);
        return count;
    };
    PC_CHECK(mask_count(PC_MASK_CRAFTED) > 0);
    PC_CHECK(mask_count(PC_MASK_ESSENCE_ONLY) > 0);
    PC_CHECK(mask_count(PC_MASK_DELVE) > 0);

    bool saw_classification_tags = false;
    bool saw_multiple_families_in_one_exclusion_group = false;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        saw_classification_tags =
            saw_classification_tags || rows[i].classification_tag_count > 0;
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (rows[i].primary_group == rows[j].primary_group &&
                rows[i].family_id != rows[j].family_id) {
                saw_multiple_families_in_one_exclusion_group = true;
                break;
            }
        }
    }
    PC_CHECK(saw_classification_tags);
    PC_CHECK(saw_multiple_families_in_one_exclusion_group);

    // membership: (reach_via | gen | key)
    std::set<std::tuple<std::string, std::string, std::string>> actual;
    for (uint32_t id : base_ids) {
        const auto& r = rows[id];
        actual.emplace(r.reach_via, gen_name(r.gen_type), r.key);
    }
    std::set<std::tuple<std::string, std::string, std::string>> expected;
    const auto& by_reach = fx.at("mods_by_reachability");
    for (const auto& reach : by_reach.object) {
        for (const char* side : {"prefix", "suffix"}) {
            const json::Value* arr = reach.second.find(side);
            if (arr == nullptr) continue;
            for (const auto& key : arr->as_array()) {
                expected.emplace(reach.first, side, key.as_string());
            }
        }
    }
    PC_CHECK(actual == expected);

    // effective tags
    uint32_t tag_count = 0;
    pc_session_dump_effective_tags(session, nullptr, 0, &tag_count, &error);
    std::vector<const char*> names(tag_count);
    pc_session_dump_effective_tags(session, names.data(), tag_count, &tag_count,
                                   &error);
    std::set<std::string> actual_tags;
    for (const char* n : names) actual_tags.insert(n);
    std::set<std::string> expected_tags;
    for (const auto& t : fx.at("effective_tags").as_array()) {
        expected_tags.insert(t.as_string());
    }
    PC_CHECK(actual_tags == expected_tags);
}

void check_influenced_signature_and_cache(
    pc_session_handle session,
    pc_action_context_handle context,
    const pc_item_state& base_item) {
    const std::vector<ModRow> rows = read_session_mods(session);
    int influence_code = -1;
    for (const ModRow& row : rows) {
        if (row.reach_kind == PC_MOD_REACH_INFLUENCE) {
            influence_code = row.reach_influence;
            break;
        }
    }
    PC_CHECK(influence_code > 0 && influence_code <= 8);
    if (influence_code <= 0 || influence_code > 8) return;

    pc_item_state influenced = base_item;
    influenced.generic_influence_bits =
        static_cast<uint8_t>(1u << (influence_code - 1));

    pc_pool_query_request query{};
    query.struct_size = sizeof(query);
    query.abi_version = PC_ABI_VERSION;
    query.action.struct_size = sizeof(query.action);
    query.action.abi_version = PC_ABI_VERSION;
    query.action.action_type = PC_ACTION_EXALT;
    query.side_filter = -1;
    query.include_rejected = 0;

    pc_error_info error;
    uint32_t count = 0;
    pc_pool_debug_summary first{};
    pc_result rc = pc_debug_pool_query(
        context, &influenced, &query, nullptr, 0, &count, &first, &error);
    PC_CHECK(rc == PC_RESULT_BUFFER_TOO_SMALL);
    PC_CHECK(first.tag_signature_id != 0);
    std::vector<pc_pool_debug_entry> entries(count);
    pc_pool_debug_summary second{};
    PC_CHECK(pc_debug_pool_query(context, &influenced, &query, entries.data(),
                                 count, &count, &second,
                                 &error) == PC_RESULT_OK);
    PC_CHECK(second.cache_hit == 1);
    PC_CHECK(second.combined_total_weight ==
             second.prefix_total_weight + second.suffix_total_weight);
    bool saw_influence = false;
    for (const auto& entry : entries) {
        if (entry.reach_kind == PC_MOD_REACH_INFLUENCE) {
            saw_influence = true;
            PC_CHECK(entry.active_spawn_weight > 0);
        }
    }
    PC_CHECK(saw_influence);

    // Rich mode returns every session row and explains direct/special rows as
    // excluded from the normal-random pool.
    query.include_rejected = 1;
    uint32_t all_count = 0;
    pc_debug_pool_query(context, &influenced, &query, nullptr, 0, &all_count,
                        nullptr, &error);
    uint32_t session_count = 0;
    pc_session_get_mod_count(session, &session_count, &error);
    PC_CHECK(all_count == session_count);
    std::vector<pc_pool_debug_entry> all(all_count);
    pc_debug_pool_query(context, &influenced, &query, all.data(), all_count,
                        &all_count, nullptr, &error);
    bool explained_direct = false;
    for (const auto& entry : all) {
        if (entry.reach_kind == PC_MOD_REACH_ESSENCE) {
            PC_CHECK(entry.first_failure ==
                     PC_POOL_DEBUG_NOT_NORMAL_RANDOM);
            explained_direct = true;
            break;
        }
    }
    PC_CHECK(explained_direct);

    uint32_t influence_count = 0;
    pc_session_dump_influence_mask(session, influence_code, nullptr, 0,
                                   &influence_count, &error);
    PC_CHECK(influence_count > 0);

    pc_pool_query_request fossil_query{};
    fossil_query.struct_size = sizeof(fossil_query);
    fossil_query.abi_version = PC_ABI_VERSION;
    fossil_query.action.struct_size = sizeof(fossil_query.action);
    fossil_query.action.abi_version = PC_ABI_VERSION;
    fossil_query.action.action_type = PC_ACTION_FOSSIL;
    fossil_query.action.fossil_count = 1;
    fossil_query.action.fossil_keys[0] =
        "Metadata/Items/Currency/CurrencyDelveCraftingRandom";
    fossil_query.side_filter = -1;
    uint32_t fossil_count = 0;
    pc_debug_pool_query(context, &base_item, &fossil_query, nullptr, 0,
                        &fossil_count, nullptr, &error);
    std::vector<pc_pool_debug_entry> fossil_entries(fossil_count);
    pc_debug_pool_query(context, &base_item, &fossil_query,
                        fossil_entries.data(), fossil_count, &fossil_count,
                        nullptr, &error);
    bool saw_fossil_direct = false;
    for (const auto& entry : fossil_entries) {
        if (entry.reach_kind == PC_MOD_REACH_FOSSIL) {
            saw_fossil_direct = true;
            PC_CHECK(entry.final_weight > 0);
        }
    }
    PC_CHECK(saw_fossil_direct);
}

// Build the engine pool for a side and compare to a weighted-pool fixture.
void check_pool(
    pc_session_handle session,
    pc_action_context_handle context,
    const pc_item_state* item,
    int side_filter,
    const std::string& fixture_path) {
    json::Value fx = load_fixture(fixture_path);
    pc_error_info error;

    uint32_t count = 0;
    pc_action_context_debug_pool(context, item, side_filter, nullptr, 0, &count,
                                 &error);
    std::vector<pc_pool_entry> entries(count);
    pc_action_context_debug_pool(context, item, side_filter, entries.data(),
                                 count, &count, &error);

    // (key, spawn, gen_pct, final)
    std::set<std::tuple<std::string, int, int, int>> actual;
    long long total_final = 0;
    for (const auto& e : entries) {
        pc_mod_info info;
        pc_session_get_mod_info(session, e.session_mod_id, &info, &error);
        actual.emplace(info.key, static_cast<int>(e.spawn_weight),
                       static_cast<int>(e.generation_multiplier_pct),
                       static_cast<int>(e.final_weight));
        total_final += e.final_weight;
    }

    std::set<std::tuple<std::string, int, int, int>> expected;
    long long expected_final = 0;
    for (const auto& bucket : fx.at("pool").as_array()) {
        const int spawn = static_cast<int>(bucket.at("spawn_weight").as_int());
        const int gen =
            static_cast<int>(bucket.at("generation_multiplier_pct").as_int());
        const int final_w =
            static_cast<int>(bucket.at("final_weight").as_int());
        for (const auto& key : bucket.at("mod_ids").as_array()) {
            expected.emplace(key.as_string(), spawn, gen, final_w);
            expected_final += final_w;
        }
    }
    PC_CHECK(actual == expected);

    const auto& summary = fx.at("summary");
    const char* count_key =
        side_filter == 0 ? "prefix_count" : "suffix_count";
    PC_CHECK(static_cast<int>(entries.size()) ==
             summary.at(count_key).as_int());
    PC_CHECK(total_final == expected_final);
}

void check_combined(
    pc_session_handle session,
    pc_action_context_handle context,
    const pc_item_state* item,
    const std::string& fixture_dir) {
    (void)session;
    json::Value fx = load_fixture(
        fixture_dir +
        "/session-pools/vaal-regalia-ilvl-86-alchemy-combined.json");
    pc_error_info error;
    uint32_t count = 0;
    pc_action_context_debug_pool(context, item, -1, nullptr, 0, &count, &error);
    std::vector<pc_pool_entry> entries(count);
    pc_action_context_debug_pool(context, item, -1, entries.data(), count,
                                 &count, &error);
    long long total = 0;
    int prefixes = 0;
    int suffixes = 0;
    for (const auto& e : entries) {
        total += e.final_weight;
        if (e.generation_type == 0) ++prefixes; else ++suffixes;
    }
    const auto& summary = fx.at("summary");
    PC_CHECK(static_cast<int>(entries.size()) ==
             summary.at("total_count").as_int());
    PC_CHECK(total == summary.at("combined_total_weight").as_int());
    PC_CHECK(prefixes == summary.at("prefix_count").as_int());
    PC_CHECK(suffixes == summary.at("suffix_count").as_int());
}

// Harvest spawn-only targeting: the pool for a classification tag must equal
// (mods with that tag) intersect (mods with positive spawn weight), weighted by
// spawn only. Cross-checked against the independent implicit-tag and
// positive-spawn mask dumps.
void check_harvest(
    pc_session_handle session,
    pc_action_context_handle context,
    const pc_item_state* item) {
    pc_error_info error;

    uint32_t count = 0;
    pc_result rc = pc_action_context_debug_harvest_pool(
        context, item, "life", -1, nullptr, 0, &count, &error);
    PC_CHECK(rc == PC_RESULT_BUFFER_TOO_SMALL || rc == PC_RESULT_OK);
    std::vector<pc_pool_entry> entries(count);
    pc_action_context_debug_harvest_pool(context, item, "life", -1,
                                         entries.data(), count, &count, &error);
    PC_CHECK(count > 0);

    std::set<uint32_t> actual;
    for (const auto& e : entries) {
        actual.insert(e.session_mod_id);
        PC_CHECK(e.final_weight == e.spawn_weight); // spawn-only, no gen mult
        PC_CHECK(e.spawn_weight > 0);
    }

    // expected = normal random AND implicit_tag("life") AND positive_spawn.
    // Phase 13 adds unveiled/implicit registries to the same session universe,
    // but Harvest never draws those direct-mechanic rows.
    uint32_t life_n = 0;
    pc_session_dump_implicit_tag(session, "life", nullptr, 0, &life_n, &error);
    std::vector<uint32_t> life_ids(life_n);
    pc_session_dump_implicit_tag(session, "life", life_ids.data(), life_n,
                                 &life_n, &error);
    uint32_t spawn_n = 0;
    pc_session_dump_mask(session, PC_MASK_POSITIVE_SPAWN, nullptr, 0, &spawn_n,
                         &error);
    std::vector<uint32_t> spawn_ids(spawn_n);
    pc_session_dump_mask(session, PC_MASK_POSITIVE_SPAWN, spawn_ids.data(),
                         spawn_n, &spawn_n, &error);
    std::set<uint32_t> spawn_set(spawn_ids.begin(), spawn_ids.end());
    uint32_t normal_n = 0;
    pc_session_dump_mask(session, PC_MASK_NORMAL_RANDOM_ROLL, nullptr, 0,
                         &normal_n, &error);
    std::vector<uint32_t> normal_ids(normal_n);
    pc_session_dump_mask(session, PC_MASK_NORMAL_RANDOM_ROLL,
                         normal_ids.data(), normal_n, &normal_n, &error);
    std::set<uint32_t> normal_set(normal_ids.begin(), normal_ids.end());
    std::set<uint32_t> expected;
    for (uint32_t id : life_ids) {
        if (spawn_set.count(id) && normal_set.count(id))
            expected.insert(id);
    }
    PC_CHECK(actual == expected);

    // unknown tag -> NOT_FOUND
    uint32_t junk = 0;
    PC_CHECK(pc_action_context_debug_harvest_pool(
                 context, item, "not_a_real_tag", -1, nullptr, 0, &junk,
                 &error) == PC_RESULT_NOT_FOUND);
}

} // namespace

void run_session_builder_tests(const char* artifact_dir,
                               const char* fixtures_dir) {
    if (artifact_dir == nullptr || fixtures_dir == nullptr) {
        std::printf("session-builder suite skipped (missing paths)\n");
        return;
    }
    const std::string dir = artifact_dir;
    const std::string fixtures = fixtures_dir;

    pc_error_info error;
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    if (pc_data_load_file((dir + "/manifest.json").c_str(), &data, &error) !=
        PC_RESULT_OK) {
        std::printf("session suite: data load failed: %s\n", error.message);
        PC_CHECK(false);
        return;
    }

    // --- Vaal Regalia fixture matching --------------------------------------
    pc_session_options options;
    options.struct_size = sizeof(options);
    options.abi_version = PC_ABI_VERSION;
    options.base_metadata_path = "Metadata/Items/Armours/BodyArmours/BodyInt17";
    options.item_level = 86;
    pc_session_handle session = nullptr;
    pc_result rc = pc_session_create(data, &options, &session, &error);
    PC_CHECK(rc == PC_RESULT_OK);
    if (rc == PC_RESULT_OK) {
        pc_action_context_options ctx_options;
        ctx_options.struct_size = sizeof(ctx_options);
        ctx_options.abi_version = PC_ABI_VERSION;
        ctx_options.seed = 1;
        pc_action_context_handle context = nullptr;
        pc_action_context_create(session, &ctx_options, &context, &error);

        pc_item_state item;
        pc_item_init_options item_options;
        item_options.struct_size = sizeof(item_options);
        item_options.abi_version = PC_ABI_VERSION;
        item_options.rarity = PC_RARITY_RARE;
        item_options.with_implicits = 0;
        pc_item_init(session, &item_options, &item, &error);

        check_universe(session, fixtures);
        check_pool(session, context, &item, 0,
                   fixtures +
                       "/session-pools/vaal-regalia-ilvl-86-normal-prefix.json");
        check_pool(session, context, &item, 1,
                   fixtures +
                       "/session-pools/vaal-regalia-ilvl-86-normal-suffix.json");
        check_combined(session, context, &item, fixtures);
        check_harvest(session, context, &item);
        check_influenced_signature_and_cache(session, context, item);

        pc_action_context_destroy(context);
        pc_session_destroy(session);
    }

    // --- generic coverage: armour, weapon, jewel, abyss jewel ---------------
    uint32_t base_count = 0;
    pc_data_summary summary;
    pc_data_get_summary(data, &summary, &error);
    base_count = summary.base_item_count;

    bool found_armour = false, found_weapon = false, found_jewel = false,
         found_abyss = false, found_implicit = false;
    int built = 0;
    int with_mods = 0;
    for (uint32_t i = 0; i < base_count; ++i) {
        const char* path = nullptr;
        int32_t support = -1;
        if (pc_data_get_base_path(data, i, &path, &support, &error) !=
            PC_RESULT_OK) {
            continue;
        }
        if (support != PC_SESSION_SUPPORT_ORDINARY) {
            continue;
        }
        pc_session_options opt;
        opt.struct_size = sizeof(opt);
        opt.abi_version = PC_ABI_VERSION;
        opt.base_metadata_path = path;
        opt.item_level = 86;
        pc_session_handle s = nullptr;
        if (pc_session_create(data, &opt, &s, &error) != PC_RESULT_OK) {
            PC_CHECK(false); // every ordinary base must build
            continue;
        }
        ++built;
        uint32_t mc = 0;
        pc_session_get_mod_count(s, &mc, &error);
        if (mc > 0) ++with_mods;
        pc_base_info info;
        pc_session_get_base_info(s, &info, &error);
        if (!found_implicit && info.implicit_count > 0) {
            pc_item_init_options init{};
            init.struct_size = sizeof(init);
            init.abi_version = PC_ABI_VERSION;
            init.rarity = PC_RARITY_NORMAL;
            init.with_implicits = 1;
            pc_item_state implicit_item;
            PC_CHECK(pc_item_init(s, &init, &implicit_item, &error) ==
                     PC_RESULT_OK);
            PC_CHECK(implicit_item.implicit_count == info.implicit_count);
            uint32_t implicit_mask_count = 0;
            pc_session_dump_mask(s, PC_MASK_IMPLICIT, nullptr, 0,
                                 &implicit_mask_count, &error);
            PC_CHECK(implicit_mask_count >= info.implicit_count);
            found_implicit = true;
        }
        const std::string cls = info.item_class_key;
        if (mc > 0) {
            if (cls == "Body Armour") found_armour = true;
            else if (cls == "Bow") found_weapon = true;
            else if (cls == "Jewel") found_jewel = true;
            else if (cls == "AbyssJewel") found_abyss = true;
        }
        pc_session_destroy(s);
    }
    std::printf(
        "generic: built %d ordinary sessions, %d with mods (armour=%d weapon=%d "
        "jewel=%d abyss=%d)\n",
        built, with_mods, found_armour, found_weapon, found_jewel, found_abyss);
    PC_CHECK(built > 0);
    PC_CHECK(found_armour);
    PC_CHECK(found_weapon);
    PC_CHECK(found_jewel);
    PC_CHECK(found_abyss);
    PC_CHECK(found_implicit);

    pc_data_destroy(data);
}
