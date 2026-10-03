#include "recombination.hpp"
#include "recombination_calculator.hpp"
#include "handles_internal.hpp"
#include "poecraft/recombination.h"
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

struct pc_recombination_pair { poecraft::RandomRecombPair impl; };
namespace {
pc_result fail(pc_error_info* error, pc_result code, const char* reason) {
    if (error) { pc_error_info_init(error); error->code = code;
        std::snprintf(error->message, sizeof(error->message), "%s", reason); }
    return code;
}
std::string quote(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
        else out += static_cast<char>(c);
    }
    return out + "\"";
}
void write_slot(std::ostream& out, const pc_mod_slot& slot, const poecraft::SessionImpl& session) {
    out << "{\"mod_id\":" << slot.mod_id << ",\"global_mod_id\":"
        << session.data->mod_global_ids.at(session.global_index.at(slot.mod_id))
        << ",\"group_id\":" << slot.group_id << ",\"flags\":" << unsigned(slot.flags)
        << ",\"rolls\":[";
    for (unsigned i = 0; i < slot.roll_count; ++i) { if (i) out << ','; out << slot.rolls[i]; }
    out << "]}";
}
void write_item(std::ostream& out, const pc_item_state& item, const poecraft::SessionImpl& session) {
    out << "{\"rarity\":" << unsigned(item.rarity) << ",\"quality\":" << unsigned(item.quality)
        << ",\"memory_strands\":" << unsigned(item.memory_strands)
        << ",\"lifecycle\":" << unsigned(item.lifecycle) << ",\"item_flags\":" << unsigned(item.item_flags)
        << ",\"influence_bits\":" << unsigned(item.generic_influence_bits)
        << ",\"searing_exarch_tier\":" << unsigned(item.searing_exarch_tier)
        << ",\"eater_of_worlds_tier\":" << unsigned(item.eater_of_worlds_tier)
        << ",\"socket_colors\":[";
    for (unsigned i = 0; i < item.socket_count; ++i) { if (i) out << ','; out << unsigned(item.socket_colors[i]); }
    out << "],\"link_mask\":" << unsigned(item.link_mask);
    const auto slots = [&](const char* name, const pc_mod_slot* values, unsigned count) {
        out << ',' << quote(name) << ":[";
        for (unsigned i = 0; i < count; ++i) { if (i) out << ','; write_slot(out, values[i], session); }
        out << ']';
    };
    slots("prefixes", item.prefixes, item.prefix_count); slots("suffixes", item.suffixes, item.suffix_count);
    slots("implicits", item.implicits, item.implicit_count); slots("enchantments", item.enchantments, item.enchantment_count);
    out << '}';
}
std::string calculate_json(const poecraft::RandomRecombPair& pair) {
    std::ostringstream out; out << std::setprecision(17);
    out << "{\"pair_version\":1,\"model_id\":" << quote(poecraft::kRandomRecombModel)
        << ",\"projection_id\":" << quote(poecraft::kRandomRecombProjection)
        << ",\"game_odds_estimated\":true,\"apply_supported\":true,"
           "\"gold_cost\":null,\"dust_cost\":null,\"cost_complete\":false,\"data_identity\":[";
    for (unsigned i = 0; i < pair.data_identity.size(); ++i) { if (i) out << ','; out << quote(pair.data_identity[i]); }
    out << "],\"carriers\":[";
    for (unsigned c = 0; c < 2; ++c) {
        if (c) out << ','; const auto& s = *pair.carriers[c].output_session;
        out << "{\"carrier\":" << c << ",\"probability\":0.5,\"base_metadata_path\":"
            << quote(s.data->string_at(s.data->base_metadata_path_sid.at(s.base_index)))
            << ",\"item_level\":" << s.item_level << '}';
    }
    out << "],\"outcomes\":["; bool first = true;
    for (const auto& outcome : poecraft::enumerate_random_recomb_pair(pair)) {
        if (!first) out << ','; first = false;
        out << "{\"probability\":" << outcome.probability << ",\"carrier\":" << outcome.carrier
            << ",\"requested_prefix_count\":" << outcome.prefixes.requested_count
            << ",\"requested_suffix_count\":" << outcome.suffixes.requested_count << ",\"selected\":[";
        bool first_selected = true;
        const auto selected = [&](unsigned side, const poecraft::RecombSideOutcome& selection) {
            for (auto index : selection.occurrences) {
                if (!first_selected) out << ','; first_selected = false;
                const auto& o = pair.carriers[outcome.carrier].sides[side].at(index);
                out << "{\"input\":" << unsigned(o.input) << ",\"slot\":" << unsigned(o.slot)
                    << ",\"side\":" << side << ",\"global_mod_id\":" << o.global_mod_id << '}';
            }
        };
        selected(0, outcome.prefixes); selected(1, outcome.suffixes);
        out << "],\"item\":";
        write_item(out, poecraft::materialize_random_recomb_outcome(pair, outcome), *pair.carriers[outcome.carrier].output_session);
        out << '}';
    }
    return out.str() + "]}";
}
}
pc_result pc_recombination_pair_create(const pc_craft_resource* a, const pc_craft_resource* b,
        uint32_t version, pc_recombination_pair_handle* out, pc_error_info* error) {
    if (!a || !b || !out || version != PC_RECOMBINATION_PAIR_VERSION || !a->identity || !b->identity ||
        !a->role || !b->role || !a->session || !b->session || !a->item || !b->item || a->item == b->item)
        return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid or aliased recombination pair request");
    try {
        auto pair = std::make_unique<pc_recombination_pair>();
        pair->impl = poecraft::prepare_random_recomb_pair(
            {a->identity, a->role, a->session->impl, *a->item}, {b->identity, b->role, b->session->impl, *b->item});
        *out = pair.release(); if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::invalid_argument& ex) { return fail(error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what()); }
      catch (const std::exception& ex) { return fail(error, PC_RESULT_INTERNAL_ERROR, ex.what()); }
}
void pc_recombination_pair_destroy(pc_recombination_pair_handle pair) { delete pair; }
pc_result pc_recombination_pair_output_session(pc_recombination_pair_handle pair,
        uint32_t carrier, pc_session_handle* out, pc_error_info* error) {
    if (!pair || carrier > 1 || !out) return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid recombination carrier");
    try {
        auto session = std::make_unique<pc_session>();
        session->impl = std::const_pointer_cast<poecraft::SessionImpl>(pair->impl.carriers[carrier].output_session);
        *out = session.release(); if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::exception& ex) { return fail(error, PC_RESULT_INTERNAL_ERROR, ex.what()); }
}
pc_result pc_recombination_pair_calculate_json(pc_recombination_pair_handle pair,
        char* buffer, size_t size, size_t* length, pc_error_info* error) {
    if (!pair || !length) return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid recombination calculation request");
    try {
        const auto text = calculate_json(pair->impl); *length = text.size();
        if (!buffer || size < text.size() + 1) return fail(error, PC_RESULT_BUFFER_TOO_SMALL, "Recombination JSON buffer required");
        std::memcpy(buffer, text.c_str(), text.size() + 1);
        if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::exception& ex) { return fail(error, PC_RESULT_INTERNAL_ERROR, ex.what()); }
}
pc_result pc_recombination_pair_goal_outcomes_json(pc_recombination_pair_handle pair,
        const char* goals, size_t goals_size, char* buffer, size_t size,
        size_t* length, pc_error_info* error) {
    if (!pair || !goals || !length)
        return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid recombination goal request");
    try {
        const auto text = poecraft::calculate_random_recomb_goals_json(pair->impl, goals, goals_size);
        *length = text.size();
        if (!buffer || size < text.size() + 1)
            return fail(error, PC_RESULT_BUFFER_TOO_SMALL, "Recombination goal JSON buffer required");
        std::memcpy(buffer, text.c_str(), text.size() + 1);
        if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::length_error& ex) { return fail(error, PC_RESULT_CAPACITY_EXCEEDED, ex.what()); }
      catch (const std::invalid_argument& ex) { return fail(error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what()); }
      catch (const std::exception& ex) { return fail(error, PC_RESULT_INVALID_ARGUMENT, ex.what()); }
}
pc_result pc_recombination_pair_apply(pc_recombination_pair_handle pair,
        pc_action_context_handle context, pc_craft_resource* resources, uint32_t count,
        const char* output_identity, pc_recombination_result* result, pc_error_info* error) {
    if (!pair || !context || !resources || count < 2 || count > PC_MAX_CRAFT_RESOURCES ||
        !output_identity || !result)
        return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid recombination Apply request");
    const auto saved = context->impl->rng;
    try {
        std::vector<poecraft::CraftResource> inventory;
        std::set<pc_item_state*> addresses;
        for (uint32_t i = 0; i < count; ++i) {
            const auto& r = resources[i];
            if (!r.identity || !r.role || !r.session || !r.item || !addresses.insert(r.item).second)
                throw std::invalid_argument("Null or aliased recombination inventory resource");
            inventory.push_back({r.identity, r.role, r.session->impl, *r.item});
        }
        // Allocate the output wrapper before commit so allocation failure can
        // never consume inputs without a deliverable result.
        auto output_session = std::make_unique<pc_session>();
        const auto transaction = poecraft::apply_random_recomb_transaction(*context->impl, inventory,
                                                                          pair->impl, output_identity);
        const auto& output = transaction.changes.back().after;
        output_session->impl = std::const_pointer_cast<poecraft::SessionImpl>(output.session);
        pc_recombination_result next{};
        next.struct_size = sizeof(next); next.abi_version = PC_ABI_VERSION;
        next.pair_version = PC_RECOMBINATION_PAIR_VERSION; next.model_id = poecraft::kRandomRecombModel;
        next.carrier = output.session == pair->impl.carriers[0].output_session ? 0 : 1;
        next.output_session = output_session.get(); next.output_item = output.item;
        next.transaction.struct_size = sizeof(next.transaction); next.transaction.abi_version = PC_ABI_VERSION;
        next.transaction.resource_count = 3; next.transaction.consumed_price_key = nullptr;
        for (unsigned i = 0; i < 3; ++i) {
            auto& change = next.transaction.resources[i];
            change.effect = transaction.changes[i].effect;
            change.before = transaction.changes[i].before.item; change.after = transaction.changes[i].after.item;
            if (i == 2) change.identity = output_identity;
            else for (uint32_t j = 0; j < count; ++j)
                if (transaction.changes[i].identity == resources[j].identity) { change.identity = resources[j].identity; break; }
        }
        for (uint32_t i = 0; i < count; ++i) *resources[i].item = inventory[i].item;
        *result = next; output_session.release();
        if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::invalid_argument& ex) {
        context->impl->rng = saved; return fail(error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
    } catch (const std::exception& ex) {
        context->impl->rng = saved; return fail(error, PC_RESULT_INTERNAL_ERROR, ex.what());
    }
}
