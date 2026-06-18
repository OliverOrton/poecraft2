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

    // counts
    const auto& counts = fx.at("counts");
    int ordinary = 0;
    int influence = 0;
    for (const auto& r : rows) {
        if (r.reach_via == "base") {
            ++ordinary;
        } else {
            ++influence;
        }
    }
    PC_CHECK(static_cast<int>(rows.size()) == counts.at("total").as_int());
    PC_CHECK(ordinary == counts.at("ordinary").as_int());
    PC_CHECK(influence == counts.at("influence").as_int());

    // membership: (reach_via | gen | key)
    std::set<std::tuple<std::string, std::string, std::string>> actual;
    for (const auto& r : rows) {
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
    pc_error_info error;
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

    // expected = implicit_tag("life") AND positive_spawn
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
    std::set<uint32_t> expected;
    for (uint32_t id : life_ids) {
        if (spawn_set.count(id)) expected.insert(id);
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

        pc_action_context_destroy(context);
        pc_session_destroy(session);
    }

    // --- generic coverage: armour, weapon, jewel, abyss jewel ---------------
    uint32_t base_count = 0;
    pc_data_summary summary;
    pc_data_get_summary(data, &summary, &error);
    base_count = summary.base_item_count;

    bool found_armour = false, found_weapon = false, found_jewel = false,
         found_abyss = false;
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

    pc_data_destroy(data);
}
