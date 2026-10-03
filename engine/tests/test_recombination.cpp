#include "tests.hpp"
#include "../src/recombination.hpp"
#include "poecraft/multi_item.h"
#include "poecraft/bitset.h"
#include "poecraft/recombination.h"
#include "../src/handles_internal.hpp"
#include "../src/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <set>
#include <stdexcept>
using namespace poecraft;

void run_two_input_result_tests() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {101, 202, 303};
    data->artifact_game_data_hash = "fixture-game";
    data->artifact_strings_hash = "fixture-strings";
    auto left_session = std::make_shared<SessionImpl>();
    left_session->data = data;
    left_session->mod_count = 3;
    left_session->global_index = {0, 1, 2};
    left_session->gen_type = {0, 1, -1};
    left_session->primary_group = {1, 2, 3};
    auto right_session = std::make_shared<SessionImpl>(*left_session);
    right_session->global_index = {1, 0, 2};
    right_session->gen_type = {1, 0, -1};
    right_session->primary_group = {2, 1, 3};
    right_session->item_level = 70;
    auto output_session = std::make_shared<SessionImpl>(*left_session);
    output_session->item_level = 80;
    CraftResource left{"left-id", "left", left_session, {}},
                  right{"right-id", "right", right_session, {}},
                  output{"new-id", "output", output_session, {}};
    pc_item_clear(&left.item); pc_item_clear(&right.item); pc_item_clear(&output.item);
    left.item.rarity = right.item.rarity = output.item.rarity = PC_RARITY_RARE;
    PC_CHECK(pc_item_add_mod(&left.item, PC_SIDE_PREFIX, 0, 1,
                            PC_MOD_SLOT_FRACTURED, nullptr) == PC_RESULT_OK);
    PC_CHECK(pc_item_add_mod(&right.item, PC_SIDE_PREFIX, 1, 1,
                            PC_MOD_SLOT_CRAFTED, nullptr) == PC_RESULT_OK);
    PC_CHECK(pc_item_add_mod(&output.item, PC_SIDE_PREFIX, 0, 1,
                            PC_MOD_SLOT_FRACTURED, nullptr) == PC_RESULT_OK);
    left.item.prefixes[0].roll_count = 2;
    left.item.prefixes[0].rolls[0] = -8; left.item.prefixes[0].rolls[1] = 19;
    left.item.quality = 27; left.item.memory_strands = 17;
    left.item.socket_count = 2; left.item.socket_colors[0] = 2;
    left.item.socket_colors[1] = 3; left.item.link_mask = 1;
    left.item.generic_influence_bits = 4;
    output.item.prefixes[0] = left.item.prefixes[0];
    output.item.quality = 31; output.item.memory_strands = 9;
    output.item.item_flags = PC_ITEM_SPLIT;
    output.item.implicit_count = output.item.enchantment_count = 1;
    output.item.implicits[0].mod_id = output.item.enchantments[0].mod_id = 2;
    output.item.implicits[0].group_id = output.item.enchantments[0].group_id = 3;
    output.item.implicits[0].roll_count = 1; output.item.implicits[0].rolls[0] = 23;
    output.item.enchantments[0].veiled_option_count = 2;
    output.item.enchantments[0].veiled_option_mod_ids[0] = 0;
    output.item.enchantments[0].veiled_option_mod_ids[1] = 1;
    const auto input_left = left.item, input_right = right.item, expected_output = output.item;
    const auto transaction = prepare_two_input_result({left, right}, output, {"test-operation"});
    PC_CHECK(transaction.changes.size() == 3);
    PC_CHECK(transaction.consumed_price_keys == std::vector<std::string>{"test-operation"});
    PC_CHECK(std::memcmp(&left.item, &input_left, sizeof(pc_item_state)) == 0);
    PC_CHECK(std::memcmp(&right.item, &input_right, sizeof(pc_item_state)) == 0);
    std::vector<CraftResource> resources{left, right};
    commit_craft_transaction(resources, transaction);
    PC_CHECK(resources.size() == 3);
    PC_CHECK(resources[0].item.lifecycle == PC_ITEM_CONSUMED &&
             resources[1].item.lifecycle == PC_ITEM_CONSUMED);
    PC_CHECK(resources[2].identity == "new-id" && resources[2].session == output_session);
    PC_CHECK(std::memcmp(&resources[2].item, &expected_output, sizeof(pc_item_state)) == 0);
    auto retained = resources[0].item; retained.lifecycle = PC_ITEM_LIVE;
    PC_CHECK(std::memcmp(&retained, &input_left, sizeof(pc_item_state)) == 0);
    // Stale commit is atomic even after the first tentative consumption.
    auto stale = std::vector<CraftResource>{left, right};
    stale[1].item.quality = 42;
    bool rejected = false;
    try { commit_craft_transaction(stale, transaction); }
    catch (const std::invalid_argument&) { rejected = true; }
    PC_CHECK(rejected && stale.size() == 2 && stale[0].item.lifecycle == PC_ITEM_LIVE &&
             stale[1].item.lifecycle == PC_ITEM_LIVE && stale[1].item.quality == 42);
    const auto refuses = [&](const std::vector<CraftResource>& in, const CraftResource& out,
                             const std::vector<std::string>& prices = {}) {
        bool refused = false;
        try { (void)prepare_two_input_result(in, out, prices); }
        catch (const std::invalid_argument&) { refused = true; }
        PC_CHECK(refused);
    };
    refuses({left}, output); refuses({left, right, left}, output);
    refuses({left, left}, output); refuses({left, right}, left);
    auto bad = output;
    bad.role = "left"; refuses({left, right}, bad);
    bad = output; bad.identity.clear(); refuses({left, right}, bad);
    bad = output; bad.session.reset(); refuses({left, right}, bad);
    for (unsigned mutation = 0; mutation < 13; ++mutation) {
        bad = output;
        switch (mutation) {
        case 0: bad.item.lifecycle = PC_ITEM_CONSUMED; break;
        case 1: bad.item.prefix_count = PC_MAX_PREFIXES + 1; break;
        case 2: bad.item.suffix_count = PC_MAX_SUFFIXES + 1; break;
        case 3: bad.item.implicit_count = PC_MAX_IMPLICITS + 1; break;
        case 4: bad.item.enchantment_count = PC_MAX_ENCHANTS + 1; break;
        case 5: bad.item.socket_count = PC_MAX_SOCKETS + 1; break;
        case 6: bad.item.prefixes[0].mod_id = 3; break;
        case 7: bad.item.prefixes[0].group_id = 2; break;
        case 8: bad.item.prefixes[0].mod_id = 1; bad.item.prefixes[0].group_id = 2; break;
        case 9: bad.item.prefixes[0].roll_count = PC_MAX_ROLL_VALUES + 1; break;
        case 10: bad.item.enchantments[0].veiled_option_count = PC_MAX_VEILED_OPTIONS + 1; break;
        case 11: bad.item.enchantments[0].veiled_option_mod_ids[1] = 3; break;
        case 12: bad.item.enchantments[0].veiled_chosen_mod_id = 3; break;
        }
        refuses({left, right}, bad);
    }
    bad = left; bad.item.lifecycle = PC_ITEM_DESTROYED; refuses({bad, right}, output);
    bad = right; bad.item.rarity = 3; refuses({left, bad}, output);
    bad = right; bad.item.memory_strands = 101; refuses({left, bad}, output);
    refuses({left, right}, output, {""});
    refuses({left, right}, output, {"test-operation", "test-operation"});
    auto independently_loaded = std::make_shared<DataImpl>(*data);
    auto compatible_session = std::make_shared<SessionImpl>(*output_session);
    compatible_session->data = independently_loaded;
    bad = output; bad.session = compatible_session;
    PC_CHECK(prepare_two_input_result({left, right}, bad, {}).changes.size() == 3);
    independently_loaded->artifact_strings_hash = "different-strings";
    refuses({left, right}, bad);
    independently_loaded->artifact_strings_hash.clear();
    data->artifact_strings_hash.clear();
    refuses({left, right}, bad); // Blank identities cannot establish compatibility.
    compatible_session->data.reset();
    refuses({left, right}, bad);
}


namespace {
bool close(double a, double b) { return std::abs(a - b) < 1e-12; }
double side_mass(const std::vector<RecombSideOutcome>& outcomes, unsigned requested,
                 const std::vector<unsigned>& selected) {
    double mass = 0;
    for (const auto& o : outcomes)
        if (o.requested_count == requested && o.occurrences == selected) mass += o.probability;
    return mass;
}
RecombOccurrence occurrence(unsigned index, unsigned global, unsigned weight,
                           std::vector<std::uint32_t> groups) {
    RecombOccurrence o; o.input = static_cast<std::uint8_t>(index / 3);
    o.slot = static_cast<std::uint8_t>(index % 3); o.global_mod_id = global;
    o.spawn_weight = weight; o.groups = std::move(groups); return o;
}
void run_recomb_side_contracts() {
    for (unsigned count = 0; count <= 6; ++count) {
        const auto& row = random_recomb_count_row(count);
        PC_CHECK(std::accumulate(row.begin(), row.end(), 0u) == 1000);
    }
    const auto empty = enumerate_random_recomb_side({});
    PC_CHECK(empty.size() == 1 && empty[0].occurrences.empty() && close(empty[0].probability, 1));
    const auto singleton = enumerate_random_recomb_side({occurrence(0, 101, 1, {1})});
    PC_CHECK(close(side_mass(singleton, 0, {}), .41));
    PC_CHECK(close(side_mass(singleton, 1, {0}), .59));
    const std::vector<RecombOccurrence> weighted{occurrence(0, 101, 1, {1}), occurrence(1, 102, 3, {2})};
    const auto odds = enumerate_random_recomb_side(weighted);
    PC_CHECK(close(side_mass(odds, 1, {0}), .667 / 4));
    PC_CHECK(close(side_mass(odds, 1, {1}), .667 * 3 / 4));
    PC_CHECK(close(side_mass(odds, 2, {0, 1}), .333)); // sum both draw orders
    auto filtered = weighted; filtered[0].spawn_weight = 0;
    const auto filtered_odds = enumerate_random_recomb_side(filtered);
    PC_CHECK(close(side_mass(filtered_odds, 1, {1}), .667));
    PC_CHECK(close(side_mass(filtered_odds, 2, {1}), .333)); // count-before-filter, exhausted
    filtered[1].spawn_weight = 0;
    const auto exhausted = enumerate_random_recomb_side(filtered);
    PC_CHECK(close(side_mass(exhausted, 1, {}), .667));
    PC_CHECK(close(side_mass(exhausted, 2, {}), .333));
    const auto duplicates = enumerate_random_recomb_side({occurrence(0, 101, 1, {1}), occurrence(1, 101, 3, {1})});
    PC_CHECK(close(side_mass(duplicates, 2, {0}), .333 / 4));
    PC_CHECK(close(side_mass(duplicates, 2, {1}), .333 * 3 / 4));
    std::vector<RecombOccurrence> pool;
    for (unsigned i = 0; i < 6; ++i) pool.push_back(occurrence(i, 101 + i, i + 1, {42}));
    auto conflicts = enumerate_random_recomb_side(pool); double mass = 0;
    for (const auto& o : conflicts) { mass += o.probability; PC_CHECK(o.occurrences.size() == 1); }
    PC_CHECK(close(mass, 1));
    PC_CHECK(close(side_mass(conflicts, 3, {5}), .7 * 6 / 21));
    pool = {occurrence(0, 101, 1, {10, 99}), occurrence(1, 102, 1, {11, 99}), occurrence(2, 103, 1, {12})};
    const auto full_groups = enumerate_random_recomb_side(pool); mass = 0;
    for (const auto& o : full_groups) {
        mass += o.probability;
        PC_CHECK(o.occurrences != std::vector<unsigned>({0, 1}));
        PC_CHECK(o.occurrences.size() <= 2);
    }
    PC_CHECK(close(mass, 1));
    for (unsigned seed = 0; seed < 16; ++seed) {
        Rng rng(seed); const auto sample = sample_random_recomb_side(rng, pool);
        PC_CHECK(side_mass(full_groups, sample.requested_count, sample.occurrences) > 0);
    }
    Rng rng(11); auto before = rng;
    pool.push_back(pool.front()); bool refused = false;
    try { (void)sample_random_recomb_side(rng, pool); } catch (const std::invalid_argument&) { refused = true; }
    PC_CHECK(refused && rng.next_u64() == before.next_u64());
    refused = false;
    try { (void)random_recomb_count_row(7); } catch (const std::invalid_argument&) { refused = true; }
    PC_CHECK(refused);
    PC_CHECK(random_recomb_item_level(60, 86) == 75);
    PC_CHECK(random_recomb_item_level(86, 60) == 75);
    PC_CHECK(random_recomb_item_level(60, 60) == 60);
    PC_CHECK(random_recomb_item_level(1, 100) == 52);
}
void run_recomb_pair_contracts(const char* artifact_dir) {
    PC_CHECK(artifact_dir != nullptr);
    if (!artifact_dir) return;
    pc_error_info error{}; pc_data_handle data = nullptr;
    const auto manifest = std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(manifest.c_str(), &data, &error) == PC_RESULT_OK);
    if (!data) return;
    pc_session_handle low = nullptr, high = nullptr;
    pc_session_options options{}; options.struct_size = sizeof(options); options.abi_version = PC_ABI_VERSION;
    options.item_level = 60; options.base_metadata_path = "Metadata/Items/Rings/Ring1";
    PC_CHECK(pc_session_create(data, &options, &low, &error) == PC_RESULT_OK);
    options.item_level = 86; options.base_metadata_path = "Metadata/Items/Rings/Ring2";
    PC_CHECK(pc_session_create(data, &options, &high, &error) == PC_RESULT_OK);
    if (!low || !high) { pc_session_destroy(low); pc_session_destroy(high); pc_data_destroy(data); return; }
    pc_item_init_options item_options{}; item_options.struct_size = sizeof(item_options);
    item_options.abi_version = PC_ABI_VERSION; item_options.rarity = PC_RARITY_RARE; item_options.with_implicits = 1;
    CraftResource a{"a", "left", low->impl, {}}, b{"b", "right", high->impl, {}};
    PC_CHECK(pc_item_init(low, &item_options, &a.item, &error) == PC_RESULT_OK);
    PC_CHECK(pc_item_init(high, &item_options, &b.item, &error) == PC_RESULT_OK);
    std::uint32_t transferred = PC_MOD_NONE;
    const auto& s = *high->impl;
    for (std::uint32_t id = 0; id < s.mod_count; ++id)
        if (s.gen_type[id] == 0 && s.required_level[id] > 75 && s.flags[id] == 0 &&
            s.special_kind[id] < 0 && s.metamod_type[id] < 0 && s.influence_code[id] <= 0 && s.base_spawn_weight[id] > 0) {
            transferred = id; break;
        }
    PC_CHECK(transferred != PC_MOD_NONE);
    if (transferred == PC_MOD_NONE) { pc_session_destroy(low); pc_session_destroy(high); pc_data_destroy(data); return; }
    PC_CHECK(pc_item_add_mod(&b.item, PC_SIDE_PREFIX, transferred,
             static_cast<std::uint16_t>(s.primary_group[transferred]), 0, nullptr) == PC_RESULT_OK);
    b.item.prefixes[0].roll_count = 2; b.item.prefixes[0].rolls[0] = -7; b.item.prefixes[0].rolls[1] = 91;
    a.item.quality = 21; b.item.quality = 31; a.item.memory_strands = 23; b.item.memory_strands = 11;
    a.item.item_flags = PC_ITEM_SPLIT;
    const auto a_before = a.item, b_before = b.item;
    const auto pair = prepare_random_recomb_pair(a, b);
    PC_CHECK(pair.game_odds_estimated && pair.full_item_apply_supported);
    PC_CHECK(!pair.gold_cost_complete && !pair.dust_cost_complete);
    const auto global = s.data->mod_global_ids[s.global_index[transferred]];
    for (unsigned c = 0; c < 2; ++c) {
        const auto& carrier = pair.carriers[c]; const auto& out = *carrier.output_session;
        PC_CHECK(out.base_index == pair.inputs[c].session->base_index && out.item_level == 75);
        const auto id = out.session_id_by_global_id.at(global);
        PC_CHECK(out.required_level[id] > out.item_level);
        PC_CHECK(static_cast<ReachKind>(out.reach_kind[id]) == ReachKind::RetainedTransfer);
        PC_CHECK(!pc_bitset_test(out.normal_random_roll_mask.data(), id));
        PC_CHECK(carrier.properties.quality == pair.inputs[c].item.quality);
        PC_CHECK(carrier.properties.memory_strands == pair.inputs[c].item.memory_strands);
    }
    auto outcomes = enumerate_random_recomb_pair(pair); double mass = 0;
    std::array<double, 2> carrier_mass{};
    for (const auto& o : outcomes) {
        mass += o.probability; carrier_mass[o.carrier] += o.probability;
        const auto item = materialize_random_recomb_outcome(pair, o);
        PC_CHECK(item.rarity == PC_RARITY_RARE && item.lifecycle == PC_ITEM_LIVE);
        PC_CHECK(item.quality == pair.inputs[o.carrier].item.quality);
        PC_CHECK(item.memory_strands == pair.inputs[o.carrier].item.memory_strands);
        PC_CHECK(item.item_flags == pair.inputs[o.carrier].item.item_flags);
        if (item.prefix_count) {
            PC_CHECK(item.prefixes[0].roll_count == 2 && item.prefixes[0].rolls[0] == -7 && item.prefixes[0].rolls[1] == 91);
            const auto& output_s = *pair.carriers[o.carrier].output_session;
            PC_CHECK(output_s.data->mod_global_ids[output_s.global_index[item.prefixes[0].mod_id]] == global);
        }
        PC_CHECK(item.implicit_count == pair.inputs[o.carrier].item.implicit_count);
        if (item.implicit_count) {
            const auto& output_s = *pair.carriers[o.carrier].output_session;
            const auto& input_s = *pair.inputs[o.carrier].session;
            PC_CHECK(output_s.data->mod_global_ids[output_s.global_index[item.implicits[0].mod_id]] ==
                     input_s.data->mod_global_ids[input_s.global_index[pair.inputs[o.carrier].item.implicits[0].mod_id]]);
        }
    }
    PC_CHECK(close(mass, 1) && close(carrier_mass[0], .5) && close(carrier_mass[1], .5));
    PC_CHECK(std::memcmp(&a.item, &a_before, sizeof(a.item)) == 0 && std::memcmp(&b.item, &b_before, sizeof(b.item)) == 0);
    const auto refuse_pair = [&](const CraftResource& left, const CraftResource& right) {
        bool refused = false; try { (void)prepare_random_recomb_pair(left, right); }
        catch (const std::invalid_argument&) { refused = true; } PC_CHECK(refused);
    };
    auto bad = b; bad.identity = a.identity; refuse_pair(a, bad);
    bad = b; bad.role = a.role; refuse_pair(a, bad);
    bad = b; bad.item.prefixes[0].flags = PC_MOD_SLOT_FRACTURED; refuse_pair(a, bad);
    bad = b; bad.item.item_flags = PC_ITEM_CORRUPTED; refuse_pair(a, bad);
    bad = b; bad.item.generic_influence_bits = 1; refuse_pair(a, bad);
    bad = b; bad.item.lifecycle = PC_ITEM_CONSUMED; refuse_pair(a, bad);
    auto independently_loaded = std::make_shared<DataImpl>(*data->impl);
    auto separate_session = std::make_shared<SessionImpl>(*b.session); separate_session->data = independently_loaded;
    bad = b; bad.session = separate_session;
    PC_CHECK(prepare_random_recomb_pair(a, bad).version == 1);
    independently_loaded->artifact_data_hash += "different"; refuse_pair(a, bad);
    auto x = a, y = b; pc_item_clear_side(&y.item, PC_SIDE_PREFIX);
    std::uint32_t suffix = PC_MOD_NONE;
    for (std::uint32_t id = 0; id < s.mod_count; ++id)
        if (s.gen_type[id] == 1 && s.flags[id] == 0 && s.special_kind[id] < 0 &&
            s.metamod_type[id] < 0 && s.influence_code[id] <= 0 && s.base_spawn_weight[id] > 0) { suffix = id; break; }
    PC_CHECK(suffix != PC_MOD_NONE);
    if (suffix != PC_MOD_NONE) {
        // Exceptional 1p0s + 0p1s joint law is held in both input orientations.
        x = b; y = a; x.identity = "x"; y.identity = "y"; y.session = high->impl;
        pc_item_clear(&y.item); y.item.rarity = PC_RARITY_RARE;
        pc_item_add_mod(&y.item, PC_SIDE_SUFFIX, suffix, static_cast<std::uint16_t>(s.primary_group[suffix]), 0, nullptr);
        refuse_pair(x, y); refuse_pair(y, x);
    }
    pc_action_context_handle context = nullptr;
    pc_action_context_options context_options{}; context_options.struct_size = sizeof(context_options);
    context_options.abi_version = PC_ABI_VERSION; context_options.seed = 55;
    PC_CHECK(pc_action_context_create(low, &context_options, &context, &error) == PC_RESULT_OK);
    if (!context) { pc_session_destroy(low); pc_session_destroy(high); pc_data_destroy(data); return; }
    std::vector<CraftResource> inventory{a, b};
    const auto receipt = apply_random_recomb_transaction(*context->impl, inventory, pair, "new-output");
    PC_CHECK(inventory.size() == 3 && inventory[0].item.lifecycle == PC_ITEM_CONSUMED && inventory[1].item.lifecycle == PC_ITEM_CONSUMED);
    const auto stored_output = inventory[2].item; const auto stored_session = inventory[2].session;
    // Undo restores the snapshots; Redo uses the stored receipt without RNG.
    inventory = {a, b}; const auto before_redo = context->impl->rng;
    commit_craft_transaction(inventory, receipt);
    auto after_redo = context->impl->rng, expected_redo = before_redo;
    PC_CHECK(after_redo.next_u64() == expected_redo.next_u64());
    PC_CHECK(inventory[2].session == stored_session && std::memcmp(&inventory[2].item, &stored_output, sizeof(stored_output)) == 0);
    auto before_replay = context->impl->rng; bool refused = false;
    try { (void)apply_random_recomb_transaction(*context->impl, inventory, pair, "replay-output"); }
    catch (const std::invalid_argument&) { refused = true; }
    auto after_replay = context->impl->rng;
    PC_CHECK(refused && after_replay.next_u64() == before_replay.next_u64() && inventory.size() == 3);
    pc_craft_resource resources[3]{{"a", "left", low, &a.item}, {"b", "right", high, &b.item}, {}};
    pc_recombination_pair_handle api_pair = nullptr;
    PC_CHECK(pc_recombination_pair_create(&resources[0], &resources[1], 1, &api_pair, &error) == PC_RESULT_OK);
    if (api_pair) {
        size_t length = 0;
        PC_CHECK(pc_recombination_pair_calculate_json(api_pair, nullptr, 0, &length, &error) == PC_RESULT_BUFFER_TOO_SMALL && length > 0);
        std::vector<char> text(length + 1);
        PC_CHECK(pc_recombination_pair_calculate_json(api_pair, text.data(), text.size(), &length, &error) == PC_RESULT_OK);
        const auto parsed = json::Parser(text.data(), length).parse();
        PC_CHECK(parsed.at("model_id").as_string() == kRandomRecombModel);
        PC_CHECK(parsed.at("game_odds_estimated").as_bool() && !parsed.at("cost_complete").as_bool());
        PC_CHECK(parsed.at("gold_cost").is_null() && parsed.at("dust_cost").is_null());
        double api_mass = 0; for (const auto& o : parsed.at("outcomes").as_array()) api_mass += o.at("probability").as_number();
        PC_CHECK(close(api_mass, 1));
        pc_recombination_result result{}, result_before{};
        std::memset(&result, 0x77, sizeof(result)); std::memcpy(&result_before, &result, sizeof(result));
        const auto check_refusal = [&](uint32_t count, const char* output_id) {
            const auto before0 = a.item, before1 = b.item; auto expected = context->impl->rng;
            PC_CHECK(pc_recombination_pair_apply(api_pair, context, resources, count, output_id, &result, &error) != PC_RESULT_OK);
            auto actual = context->impl->rng;
            PC_CHECK(actual.next_u64() == expected.next_u64());
            PC_CHECK(std::memcmp(&a.item, &before0, sizeof(a.item)) == 0 && std::memcmp(&b.item, &before1, sizeof(b.item)) == 0);
            PC_CHECK(std::memcmp(&result, &result_before, sizeof(result)) == 0);
        };
        b.item.quality++; check_refusal(2, "output-id"); b.item.quality--;
        pc_item_state other = a.item;
        resources[2] = {"output-id", "spare", low, &other}; check_refusal(3, "output-id");
        resources[1].item = resources[0].item; check_refusal(2, "output-id"); resources[1].item = &b.item;
        check_refusal(2, "a");
        PC_CHECK(pc_recombination_pair_apply(api_pair, context, resources, 2, "output-id", &result, &error) == PC_RESULT_OK);
        PC_CHECK(result.transaction.resource_count == 3 && result.transaction.resources[2].effect == PC_RESOURCE_CREATED);
        PC_CHECK(a.item.lifecycle == PC_ITEM_CONSUMED && b.item.lifecycle == PC_ITEM_CONSUMED);
        PC_CHECK(!result.gold_cost_complete && !result.dust_cost_complete && result.transaction.consumed_price_key == nullptr);
        pc_recombination_result replay_result = result; auto expected = context->impl->rng;
        PC_CHECK(pc_recombination_pair_apply(api_pair, context, resources, 2, "replayed", &replay_result, &error) != PC_RESULT_OK);
        auto actual = context->impl->rng;
        PC_CHECK(actual.next_u64() == expected.next_u64() && std::memcmp(&replay_result, &result, sizeof(result)) == 0);
        pc_session_handle output_session = result.output_session;
        pc_recombination_pair_destroy(api_pair); api_pair = nullptr;
        pc_base_info output_info{};
        PC_CHECK(pc_session_get_base_info(output_session, &output_info, &error) == PC_RESULT_OK && output_info.item_level == 75);
        pc_session_destroy(output_session);
    }
    pc_recombination_pair_destroy(api_pair); pc_action_context_destroy(context);
    pc_session_destroy(low); pc_session_destroy(high); pc_data_destroy(data);
}
}
void run_random_recombination_tests(const char* artifact_dir) {
    run_recomb_side_contracts(); run_recomb_pair_contracts(artifact_dir);
}
