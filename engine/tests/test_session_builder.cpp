#include "tests.hpp"

#include "poecraft/api.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include "../src/json.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
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
    uint32_t required_level = 0;
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
        rows[i].required_level = info.required_level;
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

bool reliability_check(
    bool condition,
    const std::string& base,
    const std::string& item_class,
    const std::string& detail) {
    if (!condition) {
        std::printf(
            "cross-base reliability failure: base=%s class=%s %s\n",
            base.c_str(), item_class.c_str(), detail.c_str());
    }
    PC_CHECK(condition);
    return condition;
}

std::string quoted_json(const std::string& value) {
    std::string out = "\"";
    for (const char c : value) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
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

// Harvest targeted-natural selection: the pool for a classification tag must
// equal (mods with that tag) intersect (ordinary positive final roll weight).
// Cross-checked against the independent implicit-tag, normal-random, and
// positive-base mask dumps.
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
        PC_CHECK(
            e.final_weight ==
            (static_cast<std::uint64_t>(e.spawn_weight) *
             e.generation_multiplier_pct) /
                100);
        PC_CHECK(e.spawn_weight > 0);
        PC_CHECK(e.generation_multiplier_pct > 0);
    }

    // expected = normal random AND implicit_tag("life") AND positive base.
    // Phase 13 adds unveiled/implicit registries to the same session universe,
    // but Harvest never draws those direct-mechanic rows.
    uint32_t life_n = 0;
    pc_session_dump_implicit_tag(session, "life", nullptr, 0, &life_n, &error);
    std::vector<uint32_t> life_ids(life_n);
    pc_session_dump_implicit_tag(session, "life", life_ids.data(), life_n,
                                 &life_n, &error);
    uint32_t positive_n = 0;
    pc_session_dump_mask(session, PC_MASK_POSITIVE_BASE, nullptr, 0,
                         &positive_n, &error);
    std::vector<uint32_t> positive_ids(positive_n);
    pc_session_dump_mask(session, PC_MASK_POSITIVE_BASE,
                         positive_ids.data(), positive_n, &positive_n,
                         &error);
    std::set<uint32_t> positive_set(
        positive_ids.begin(), positive_ids.end());
    uint32_t normal_n = 0;
    pc_session_dump_mask(session, PC_MASK_NORMAL_RANDOM_ROLL, nullptr, 0,
                         &normal_n, &error);
    std::vector<uint32_t> normal_ids(normal_n);
    pc_session_dump_mask(session, PC_MASK_NORMAL_RANDOM_ROLL,
                         normal_ids.data(), normal_n, &normal_n, &error);
    std::set<uint32_t> normal_set(normal_ids.begin(), normal_ids.end());
    std::set<uint32_t> expected;
    for (uint32_t id : life_ids) {
        if (positive_set.count(id) && normal_set.count(id))
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

    // --- complete cross-base structural reliability audit -------------------
    namespace fs = std::filesystem;
    const fs::path repo_root =
        fs::absolute(fs::path(fixtures)).parent_path().parent_path();
    const json::Value current_economy = load_fixture(
        (repo_root / "apps" / "web" / "public" / "economy" /
         "snapshots" /
         "de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0.json")
            .string());
    std::set<std::string> current_price_keys;
    for (const auto& [key, unused] :
         current_economy.at("prices").object) {
        (void)unused;
        current_price_keys.insert(key);
    }
    std::set<std::string> explicit_missing_price_keys;
    for (const json::Value& key :
         current_economy.at("metadata").at("missing_keys").array) {
        explicit_missing_price_keys.insert(key.as_string());
    }

    uint32_t base_count = 0;
    pc_data_summary summary;
    pc_data_get_summary(data, &summary, &error);
    base_count = summary.base_item_count;
    PC_CHECK(summary.ordinary_session_base_count == 979);

    bool found_armour = false, found_weapon = false, found_jewel = false,
         found_abyss = false, found_implicit = false;
    int built = 0;
    int with_mods = 0;
    int feasible_goal_witnesses = 0;
    int price_accounted_vocabularies = 0;
    int compiled_evaluator_smokes = 0;
    std::set<std::string> expected_classes;
    std::set<std::string> built_classes;
    std::set<std::string> level_switched_classes;
    std::set<std::string> stochastic_transition_classes;
    std::map<std::string, std::set<std::string>> actions_by_class;
    for (uint32_t i = 0; i < base_count; ++i) {
        const char* unused_path = nullptr;
        int32_t support = -1;
        if (pc_data_get_base_path(
                data, i, &unused_path, &support, &error) !=
                PC_RESULT_OK ||
            support != PC_SESSION_SUPPORT_ORDINARY) {
            continue;
        }
        const char* unused_name = nullptr;
        const char* item_class = nullptr;
        PC_CHECK(pc_data_get_base_display(
                     data, i, &unused_name, &item_class, &error) ==
                 PC_RESULT_OK);
        if (item_class != nullptr) expected_classes.insert(item_class);
    }

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
        const char* display_name = nullptr;
        const char* data_class = nullptr;
        int32_t drop_level = -1;
        if (pc_data_get_base_display(
                data, i, &display_name, &data_class, &error) !=
                PC_RESULT_OK ||
            pc_data_get_base_drop_level(
                data, i, &drop_level, &error) != PC_RESULT_OK) {
            reliability_check(
                false, path == nullptr ? "<null>" : path,
                data_class == nullptr ? "<null>" : data_class,
                "data identity lookup failed");
            continue;
        }
        const std::string base_path =
            path == nullptr ? "<null>" : path;
        const std::string class_key =
            data_class == nullptr ? "<null>" : data_class;
        reliability_check(
            !base_path.empty() && display_name != nullptr &&
                display_name[0] != '\0' && !class_key.empty(),
            base_path, class_key, "empty base identity");

        pc_session_options opt;
        opt.struct_size = sizeof(opt);
        opt.abi_version = PC_ABI_VERSION;
        opt.base_metadata_path = path;
        opt.item_level = 86;
        pc_session_handle s = nullptr;
        if (pc_session_create(data, &opt, &s, &error) != PC_RESULT_OK) {
            reliability_check(
                false, base_path, class_key,
                std::string("session creation failed: ") + error.message);
            continue;
        }
        ++built;
        built_classes.insert(class_key);

        pc_base_info info{};
        const bool base_info_ok =
            pc_session_get_base_info(s, &info, &error) == PC_RESULT_OK;
        reliability_check(
            base_info_ok && info.metadata_path != nullptr &&
                base_path == info.metadata_path &&
                info.item_class_key != nullptr &&
                class_key == info.item_class_key &&
                info.item_level == 86 &&
                info.session_support == PC_SESSION_SUPPORT_ORDINARY,
            base_path, class_key,
            "session base identity or item level mismatched");

        uint32_t mc = 0;
        reliability_check(
            pc_session_get_mod_count(s, &mc, &error) == PC_RESULT_OK,
            base_path, class_key, "mod count query failed");
        if (mc > 0) ++with_mods;

        std::map<
            std::uint32_t,
            std::map<std::uint32_t, std::uint32_t>>
            required_level_by_family_tier;
        std::string representative_mod;
        std::set<int> influence_codes;
        for (std::uint32_t mod = 0; mod < mc; ++mod) {
            pc_mod_info mod_info{};
            if (!reliability_check(
                    pc_session_get_mod_info(
                        s, mod, &mod_info, &error) == PC_RESULT_OK,
                    base_path, class_key,
                    "mod info query failed: session_mod_id=" +
                        std::to_string(mod))) {
                continue;
            }
            const std::string mod_key =
                mod_info.key == nullptr ? "<null>" : mod_info.key;
            reliability_check(
                mod_info.session_mod_id == mod &&
                    mod_info.key != nullptr && mod_info.key[0] != '\0' &&
                    mod_info.family_tier_index > 0,
                base_path, class_key,
                "invalid modifier identity: mod=" + mod_key +
                    " family=" + std::to_string(mod_info.family_id) +
                    " tier=" +
                    std::to_string(mod_info.family_tier_index));
            const bool is_affix =
                mod_info.generation_type == PC_SIDE_PREFIX ||
                mod_info.generation_type == PC_SIDE_SUFFIX;
            if (!is_affix) {
                continue;
            }
            auto& tiers =
                required_level_by_family_tier[mod_info.family_id];
            const auto [tier, inserted] = tiers.emplace(
                mod_info.family_tier_index, mod_info.required_level);
            if (!inserted) {
                reliability_check(
                    tier->second == mod_info.required_level,
                    base_path, class_key,
                    "family tier has inconsistent required levels: mod=" +
                        mod_key + " family=" +
                        std::to_string(mod_info.family_id) + " tier=" +
                        std::to_string(mod_info.family_tier_index));
            }
            if (mod_info.reach_kind == PC_MOD_REACH_INFLUENCE) {
                influence_codes.insert(mod_info.reach_influence);
            }
            if (representative_mod.empty() &&
                mod_info.reach_kind == PC_MOD_REACH_BASE &&
                mod_info.required_level <= 86) {
                representative_mod = mod_key;
            }
        }
        for (const auto& [family, tiers] :
             required_level_by_family_tier) {
            std::uint32_t expected_tier = 1;
            std::uint32_t previous_required_level =
                std::numeric_limits<std::uint32_t>::max();
            for (const auto& [tier, required_level] : tiers) {
                reliability_check(
                    tier == expected_tier &&
                        required_level <= previous_required_level,
                    base_path, class_key,
                    "family tier ordering failed: family=" +
                        std::to_string(family) + " tier=" +
                        std::to_string(tier) + " required_level=" +
                        std::to_string(required_level));
                expected_tier = tier + 1;
                previous_required_level = required_level;
            }
        }

        pc_action_context_options context_options{};
        context_options.struct_size = sizeof(context_options);
        context_options.abi_version = PC_ABI_VERSION;
        context_options.seed = 0xC05B4A5EULL + i;
        pc_action_context_handle cross_base_context = nullptr;
        reliability_check(
            pc_action_context_create(
                s, &context_options, &cross_base_context, &error) ==
                PC_RESULT_OK,
            base_path, class_key, "action context creation failed");

        pc_item_init_options rare_options{};
        rare_options.struct_size = sizeof(rare_options);
        rare_options.abi_version = PC_ABI_VERSION;
        rare_options.rarity = PC_RARITY_RARE;
        rare_options.with_implicits = 1;
        pc_item_state rare_item{};
        reliability_check(
            pc_item_init(
                s, &rare_options, &rare_item, &error) == PC_RESULT_OK,
            base_path, class_key, "rare item initialization failed");
        pc_item_state restored_item = rare_item;
        reliability_check(
            std::memcmp(
                &rare_item, &restored_item, sizeof(rare_item)) == 0,
            base_path, class_key,
            "value-copy item restoration changed state");
        std::size_t original_length = 0;
        std::size_t restored_length = 0;
        reliability_check(
            pc_item_debug_format(
                s, &rare_item, nullptr, 0, &original_length, &error) ==
                PC_RESULT_BUFFER_TOO_SMALL &&
                pc_item_debug_format(
                    s, &restored_item, nullptr, 0, &restored_length,
                    &error) == PC_RESULT_BUFFER_TOO_SMALL &&
                original_length == restored_length,
            base_path, class_key,
            "item serialization length changed after restoration");

        reliability_check(
            !representative_mod.empty(), base_path, class_key,
            "ordinary base has no reachable natural family at item level 86");
        if (!representative_mod.empty()) {
            const std::string goal =
                "{\"version\":\"v1\",\"rarity\":\"rare\","
                "\"fossil_mode\":\"goal_relevant\",\"slots\":[{"
                "\"family_mod_key\":" +
                quoted_json(representative_mod) +
                ",\"min_tier\":1}]}";
            pc_solver_handle structural_solver = nullptr;
            if (reliability_check(
                    pc_solver_create(
                        s, goal.data(), goal.size(), &structural_solver,
                        &error) == PC_RESULT_OK,
                    base_path, class_key,
                    "representative family goal failed: mod=" +
                        representative_mod)) {
                std::uint32_t action_count = 0;
                reliability_check(
                    pc_solver_action_count(
                        structural_solver, &action_count, &error) ==
                            PC_RESULT_OK &&
                        action_count > 0,
                    base_path, class_key,
                    "action registry is empty: goal_mod=" +
                        representative_mod);
                std::uint32_t fully_priced_action_count = 0;
                bool vocabulary_explicitly_accounted = true;
                for (std::uint32_t action = 0;
                     action < action_count; ++action) {
                    pc_solver_action_info action_info{};
                    if (reliability_check(
                            pc_solver_get_action_info(
                                structural_solver, action, &action_info,
                                &error) == PC_RESULT_OK &&
                                action_info.id != nullptr &&
                                action_info.id[0] != '\0',
                            base_path, class_key,
                            "invalid action registry row: action=" +
                                std::to_string(action))) {
                        actions_by_class[class_key].insert(action_info.id);
                        bool fully_priced = true;
                        bool explicitly_accounted = true;
                        for (std::uint32_t cost = 0;
                             cost < action_info.cost_key_count; ++cost) {
                            const std::string key =
                                action_info.cost_keys[cost];
                            const bool priced =
                                current_price_keys.count(key) != 0;
                            fully_priced = fully_priced && priced;
                            explicitly_accounted =
                                explicitly_accounted &&
                                (priced ||
                                 explicit_missing_price_keys.count(key) != 0);
                        }
                        reliability_check(
                            explicitly_accounted, base_path, class_key,
                            "action price key is neither priced nor explicitly missing: action=" +
                                std::string(action_info.id));
                        vocabulary_explicitly_accounted =
                            vocabulary_explicitly_accounted &&
                            explicitly_accounted;
                        if (fully_priced && action_info.synthetic == 0 &&
                            action_info.cost_key_count > 0) {
                            ++fully_priced_action_count;
                        }
                    }
                }
                reliability_check(
                    fully_priced_action_count > 0,
                    base_path, class_key,
                    "goal registry has no fully priced non-synthetic action");
                if (vocabulary_explicitly_accounted &&
                    fully_priced_action_count > 0) {
                    ++price_accounted_vocabularies;
                }
                std::uint32_t candidate_count = 0;
                pc_result candidates_result = pc_solver_candidates(
                    structural_solver, nullptr, 0, &candidate_count,
                    &error);
                reliability_check(
                    (candidates_result == PC_RESULT_OK ||
                     candidates_result == PC_RESULT_BUFFER_TOO_SMALL) &&
                        candidate_count > 0,
                    base_path, class_key,
                    "goal produced no candidate actions: mod=" +
                        representative_mod);

                pc_goal_feasibility feasibility{};
                const bool feasibility_ok = reliability_check(
                    pc_solver_goal_feasibility(
                        structural_solver, &rare_item, &feasibility,
                        &error) == PC_RESULT_OK &&
                        feasibility.status ==
                            PC_GOAL_FEASIBILITY_FEASIBLE &&
                        feasibility.reason ==
                            PC_GOAL_FEASIBILITY_REASON_NATURAL_REFORGE_WITNESS &&
                        feasibility.goal_slot_count == 1 &&
                        feasibility.required_slot_count == 1 &&
                        feasibility.eligible_slot_count == 1 &&
                        feasibility.natural_pool_mod_count > 0 &&
                        feasibility.natural_pool_weight > 0 &&
                        feasibility.witness_action_index != UINT32_MAX,
                    base_path, class_key,
                    "representative natural-family feasibility witness failed: mod=" +
                        representative_mod);
                if (feasibility_ok) ++feasible_goal_witnesses;

                std::uint32_t restart_action = UINT32_MAX;
                if (reliability_check(
                        pc_solver_find_action(
                            structural_solver, "restart",
                            &restart_action, &error) == PC_RESULT_OK,
                        base_path, class_key,
                        "restart action missing from registry")) {
                    pc_calc_summary calc_summary{};
                    std::uint32_t outcome_count = 0;
                    const pc_result calc_result =
                        pc_calc_action_outcomes(
                            structural_solver, &rare_item,
                            restart_action, nullptr, 0, &outcome_count,
                            &calc_summary, &error);
                    reliability_check(
                        (calc_result == PC_RESULT_OK ||
                         calc_result == PC_RESULT_BUFFER_TOO_SMALL) &&
                            calc_summary.supported != 0 &&
                            calc_summary.legal != 0 &&
                            outcome_count > 0,
                        base_path, class_key,
                        "restart exact legality/evaluation failed");
                }

                std::uint32_t scour_action = UINT32_MAX;
                if (reliability_check(
                        pc_solver_find_action(
                            structural_solver, "scour", &scour_action,
                            &error) == PC_RESULT_OK,
                        base_path, class_key,
                        "scour action missing from registry")) {
                    pc_calc_summary scour_summary{};
                    std::uint32_t scour_outcome_count = 0;
                    const pc_result scour_result =
                        pc_calc_action_outcomes(
                            structural_solver, &rare_item, scour_action,
                            nullptr, 0, &scour_outcome_count,
                            &scour_summary, &error);
                    reliability_check(
                        (scour_result == PC_RESULT_OK ||
                         scour_result == PC_RESULT_BUFFER_TOO_SMALL) &&
                            scour_summary.supported != 0 &&
                            scour_summary.legal != 0 &&
                            scour_outcome_count == 1,
                        base_path, class_key,
                        "scour deterministic transition failed");
                }

                if (stochastic_transition_classes.insert(class_key).second) {
                    pc_item_init_options normal_options{};
                    normal_options.struct_size = sizeof(normal_options);
                    normal_options.abi_version = PC_ABI_VERSION;
                    normal_options.rarity = PC_RARITY_NORMAL;
                    normal_options.with_implicits = 0;
                    pc_item_state normal_item{};
                    std::uint32_t transmute_action = UINT32_MAX;
                    const bool transmute_ready =
                        pc_item_init(
                            s, &normal_options, &normal_item,
                            &error) == PC_RESULT_OK &&
                        pc_solver_find_action(
                            structural_solver, "transmute",
                            &transmute_action, &error) == PC_RESULT_OK;
                    pc_calc_summary transmute_summary{};
                    std::uint32_t transmute_outcome_count = 0;
                    pc_result transmute_result = PC_RESULT_INVALID_ARGUMENT;
                    if (transmute_ready) {
                        transmute_result = pc_calc_action_outcomes(
                            structural_solver, &normal_item,
                            transmute_action, nullptr, 0,
                            &transmute_outcome_count,
                            &transmute_summary, &error);
                    }
                    reliability_check(
                        transmute_ready &&
                            (transmute_result == PC_RESULT_OK ||
                             transmute_result ==
                                 PC_RESULT_BUFFER_TOO_SMALL) &&
                            transmute_summary.supported != 0 &&
                            transmute_summary.legal != 0 &&
                            transmute_outcome_count > 0,
                        base_path, class_key,
                        "class representative transmute transition failed");
                }

                std::uint32_t eldritch_count = 0;
                pc_session_dump_mask(
                    s, PC_MASK_ELDRITCH_IMPLICIT, nullptr, 0,
                    &eldritch_count, &error);
                if (eldritch_count > 0) {
                    std::uint32_t eldritch_action = UINT32_MAX;
                    if (reliability_check(
                        pc_solver_find_action(
                            structural_solver, "eldritch_exalt",
                            &eldritch_action, &error) == PC_RESULT_OK,
                        base_path, class_key,
                        "Eldritch-eligible base omitted eldritch_exalt")) {
                        pc_item_state dominated_item = rare_item;
                        dominated_item.searing_exarch_tier = 2;
                        dominated_item.eater_of_worlds_tier = 1;
                        pc_calc_summary eldritch_summary{};
                        std::uint32_t eldritch_outcome_count = 0;
                        const pc_result eldritch_result =
                            pc_calc_action_outcomes(
                                structural_solver, &dominated_item,
                                eldritch_action, nullptr, 0,
                                &eldritch_outcome_count,
                                &eldritch_summary, &error);
                        reliability_check(
                            (eldritch_result == PC_RESULT_OK ||
                             eldritch_result ==
                                 PC_RESULT_BUFFER_TOO_SMALL) &&
                                eldritch_summary.supported != 0 &&
                                eldritch_summary.legal != 0 &&
                                eldritch_outcome_count > 0,
                            base_path, class_key,
                            "Eldritch eligibility/action legality failed");
                    }
                }

                const std::string strategy_json =
                    std::string(
                        "{\"version\":\"v1\",\"name\":\"cross-base "
                        "scour smoke\",\"start_node_id\":\"start\","
                        "\"base_state\":{\"base_key\":") +
                    quoted_json(base_path) +
                    ",\"item_level\":86,\"rarity\":\"rare\"},"
                    "\"nodes\":[{\"id\":\"start\",\"kind\":\"start\"},"
                    "{\"id\":\"scour\",\"kind\":\"operation\","
                    "\"operation\":{\"type\":\"scour\",\"params\":{}}},"
                    "{\"id\":\"success\",\"kind\":\"terminal\","
                    "\"terminal\":\"success\"}],\"edges\":["
                    "{\"id\":\"begin\",\"from\":\"start\","
                    "\"to\":\"scour\",\"priority\":0,"
                    "\"condition\":{\"type\":\"always\"}},"
                    "{\"id\":\"done\",\"from\":\"scour\","
                    "\"to\":\"success\",\"priority\":0,"
                    "\"condition\":{\"type\":\"always\"}}]}";
                pc_strategy_handle strategy = nullptr;
                const bool strategy_compiled = reliability_check(
                    pc_strategy_compile_json(
                        s, strategy_json.data(), strategy_json.size(),
                        &strategy, &error) == PC_RESULT_OK &&
                        strategy != nullptr,
                    base_path, class_key,
                    "one-operation strategy compilation failed");
                if (strategy_compiled) {
                    pc_strategy_eval_options eval_options{};
                    eval_options.struct_size = sizeof(eval_options);
                    eval_options.abi_version = PC_ABI_VERSION;
                    std::size_t eval_length = 0;
                    const pc_result eval_query = pc_strategy_evaluate(
                        strategy, &eval_options, nullptr, 0,
                        &eval_length, &error);
                    std::string eval_json(eval_length + 1, '\0');
                    const pc_result eval_result =
                        eval_query == PC_RESULT_OK
                            ? pc_strategy_evaluate(
                                  strategy, &eval_options,
                                  eval_json.data(), eval_json.size(),
                                  &eval_length, &error)
                            : eval_query;
                    eval_json.resize(
                        std::min(eval_length, eval_json.size()));
                    const bool eval_ok = reliability_check(
                        eval_result == PC_RESULT_OK &&
                            eval_json.find("\"converged\":true") !=
                                std::string::npos &&
                            eval_json.find("\"success\":1") !=
                                std::string::npos,
                        base_path, class_key,
                        "one-operation exact evaluator smoke failed");
                    if (eval_ok) ++compiled_evaluator_smokes;
                    pc_strategy_destroy(strategy);
                }
                pc_solver_destroy(structural_solver);
            }
        }

        for (const int influence : influence_codes) {
            std::uint32_t influence_count = 0;
            const pc_result influence_result =
                pc_session_dump_influence_mask(
                    s, influence, nullptr, 0, &influence_count, &error);
            reliability_check(
                influence > 0 && influence <= 8 &&
                    (influence_result == PC_RESULT_OK ||
                     influence_result == PC_RESULT_BUFFER_TOO_SMALL) &&
                    influence_count > 0,
                base_path, class_key,
                "influence reachability mask missing: influence=" +
                    std::to_string(influence));
        }

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
        const std::string cls = class_key;
        if (mc > 0) {
            if (cls == "Body Armour") found_armour = true;
            else if (cls == "Bow") found_weapon = true;
            else if (cls == "Jewel") found_jewel = true;
            else if (cls == "AbyssJewel") found_abyss = true;
        }
        if (level_switched_classes.insert(class_key).second) {
            pc_session_options low_options = opt;
            low_options.item_level = 1;
            pc_session_handle low_session = nullptr;
            if (reliability_check(
                    pc_session_create(
                        data, &low_options, &low_session, &error) ==
                        PC_RESULT_OK,
                    base_path, class_key,
                    "low-item-level session creation failed")) {
                pc_base_info low_info{};
                std::uint32_t low_mod_count = 0;
                reliability_check(
                    pc_session_get_base_info(
                        low_session, &low_info, &error) == PC_RESULT_OK &&
                        low_info.metadata_path != nullptr &&
                        base_path == low_info.metadata_path &&
                        low_info.item_level == 1 &&
                        pc_session_get_mod_count(
                            low_session, &low_mod_count, &error) ==
                            PC_RESULT_OK &&
                        low_mod_count <= mc,
                    base_path, class_key,
                    "base/iLvl switch retained stale session state");
                pc_session_destroy(low_session);
                pc_base_info high_info{};
                reliability_check(
                    pc_session_get_base_info(
                        s, &high_info, &error) == PC_RESULT_OK &&
                        high_info.metadata_path != nullptr &&
                        base_path == high_info.metadata_path &&
                        high_info.item_level == 86,
                    base_path, class_key,
                    "low-iLvl workspace contaminated live high-iLvl session");
            }
        }
        pc_action_context_destroy(cross_base_context);
        pc_session_destroy(s);
    }
    std::printf(
        "generic: built %d ordinary sessions, %d with mods (armour=%d weapon=%d "
        "jewel=%d abyss=%d); feasible=%d priced=%d compiled_eval=%d\n",
        built, with_mods, found_armour, found_weapon, found_jewel, found_abyss,
        feasible_goal_witnesses, price_accounted_vocabularies,
        compiled_evaluator_smokes);
    PC_CHECK(built == 979);
    PC_CHECK(feasible_goal_witnesses == built);
    PC_CHECK(price_accounted_vocabularies == built);
    PC_CHECK(compiled_evaluator_smokes == built);
    PC_CHECK(built_classes == expected_classes);
    PC_CHECK(level_switched_classes == expected_classes);
    for (const std::string& item_class : expected_classes) {
        reliability_check(
            !actions_by_class[item_class].empty() &&
                actions_by_class[item_class].count("restart") != 0,
            "<class-wide>", item_class,
            "item class has no verified action vocabulary");
    }
    PC_CHECK(found_armour);
    PC_CHECK(found_weapon);
    PC_CHECK(found_jewel);
    PC_CHECK(found_abyss);
    PC_CHECK(found_implicit);

    pc_data_destroy(data);
}
