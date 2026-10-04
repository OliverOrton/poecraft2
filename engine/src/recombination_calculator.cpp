#include "recombination_calculator.hpp"
#include "calculator_currency.hpp"
#include "json.hpp"
#include "poecraft/bitset.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace poecraft {
namespace {
using json::Value;
using json::Type;
Value number(double n) { Value v; v.type = Type::Number; v.number = n; return v; }
Value string(std::string s) { Value v; v.type = Type::String; v.string = std::move(s); return v; }
Value boolean(bool b) { Value v; v.type = Type::Bool; v.boolean = b; return v; }
Value array() { Value v; v.type = Type::Array; return v; }
Value object() { Value v; v.type = Type::Object; return v; }
Value& member(Value& v, const std::string& key) {
    for (auto& [k, value] : v.object) if (k == key) return value;
    v.object.emplace_back(key, Value{}); return v.object.back().second;
}
void serialize(std::ostream& out, const Value& v) {
    switch (v.type) {
    case Type::Null: out << "null"; break;
    case Type::Bool: out << (v.boolean ? "true" : "false"); break;
    case Type::Number:
        if (!std::isfinite(v.number)) throw std::logic_error("Nonfinite pair projection");
        out << v.number; break;
    case Type::String:
        out << '"';
        for (unsigned char c : v.string) {
            if (c == '"' || c == '\\') out << '\\' << char(c);
            else if (c < 32) { const char* hex = "0123456789abcdef"; out << "\\u00" << hex[c >> 4] << hex[c & 15]; }
            else out << char(c);
        }
        out << '"'; break;
    case Type::Array:
        out << '[';
        for (std::size_t i = 0; i < v.array.size(); ++i) { if (i) out << ','; serialize(out, v.array[i]); }
        out << ']'; break;
    case Type::Object:
        out << '{';
        for (std::size_t i = 0; i < v.object.size(); ++i) {
            if (i) out << ','; serialize(out, string(v.object[i].first));
            out << ':'; serialize(out, v.object[i].second);
        }
        out << '}'; break;
    }
}
// Match native display-family acquisition identity; canonical IDs already
// present in the reference take precedence over this extension to new tiers.
std::vector<std::uint32_t> family_signature(const SessionImpl& s, unsigned mod) {
    const auto p = s.global_index.at(mod);
    const auto reach = static_cast<ReachKind>(s.reach_kind.at(mod));
    std::vector<std::uint32_t> key{s.primary_group.at(mod), unsigned(s.gen_type.at(mod) + 2),
        unsigned(reach == ReachKind::RetainedTransfer && s.gen_type.at(mod) >= 0 && s.flags.at(mod) == 0
            ? unsigned(ReachKind::Base) + 1 : s.reach_kind.at(mod) + 1), unsigned(s.reach_influence.at(mod) + 2)};
    for (auto i = s.data->stat_offsets[p]; i < s.data->stat_offsets[p + 1]; ++i)
        key.push_back(s.data->stat_key_sids[i]);
    return key;
}
std::shared_ptr<SessionImpl> observation_session(const SessionImpl& reference, const SessionImpl& output) {
    auto session = std::make_shared<SessionImpl>();
    session->data = output.data; session->base_index = output.base_index; session->item_level = output.item_level;
    std::vector<std::uint32_t> retained;
    for (const auto* source : {&reference, &output})
        for (auto p : source->global_index) retained.push_back(source->data->mod_global_ids.at(p));
    std::sort(retained.begin(), retained.end()); retained.erase(std::unique(retained.begin(), retained.end()), retained.end());
    build_session(*session, retained);
    std::map<std::vector<std::uint32_t>, unsigned> reference_families;
    std::map<unsigned, std::set<unsigned, std::greater<unsigned>>> reference_levels;
    unsigned fresh = 0;
    for (unsigned m = 0; m < reference.mod_count; ++m) {
        const auto family = reference.family_id.at(m);
        reference_families.emplace(family_signature(reference, m), family);
        reference_levels[family].insert(reference.required_level.at(m));
        fresh = std::max(fresh, family + 1);
    }
    // Reserve independent IDs for output-only families. Never reuse an
    // unrelated session-local family number from another base/level.
    std::map<unsigned, unsigned> foreign;
    for (unsigned m = 0; m < session->mod_count; ++m) {
        const auto global = session->data->mod_global_ids.at(session->global_index.at(m));
        const auto original = reference.session_id_by_global_id.find(global);
        if (original != reference.session_id_by_global_id.end()) {
            session->family_id[m] = reference.family_id.at(original->second);
            session->family_tier_index[m] = reference.family_tier_index.at(original->second);
        } else if (const auto found = reference_families.find(family_signature(*session, m)); found != reference_families.end()) {
            session->family_id[m] = found->second;
            // Keep the original rank thresholds. A transferred tier better
            // than the original visible best satisfies tier 1 without moving
            // every original rank. Intermediate new tiers compare by level.
            unsigned rank = 1;
            for (auto level : reference_levels.at(found->second)) if (level > session->required_level[m]) ++rank;
            session->family_tier_index[m] = rank;
        } else {
            const auto [foreign_entry, inserted] = foreign.emplace(session->family_id[m], fresh);
            if (inserted) ++fresh;
            session->family_id[m] = foreign_entry->second;
        }
    }
    return session;
}
pc_item_state mapped_item(pc_item_state item, const SessionImpl& from, const SessionImpl& to) {
    const auto map = [&](pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            auto& slot = slots[i];
            const auto id = to.session_id_by_global_id.at(from.data->mod_global_ids.at(from.global_index.at(slot.mod_id)));
            slot.mod_id = id; slot.group_id = to.primary_group.at(id);
        }
    };
    map(item.prefixes, item.prefix_count); map(item.suffixes, item.suffix_count); map(item.implicits, item.implicit_count);
    // Projection observes explicit structure and represented implicit/flag
    // goals. Recorded rolls, strands, sockets and enchantments stay intact in
    // the full pair output/Apply; this terminal copy has no continuation law.
    item.memory_strands = 0; item.enchantment_count = 0;
    return item;
}
void add_half(Value& target, const Value& source) {
    if (target.type != source.type) throw std::logic_error("Pair goal projection shape mismatch");
    if (source.type == Type::Number) target.number = (target.number + source.number) * .5;
    else if (source.type == Type::Array) {
        if (target.array.size() != source.array.size()) throw std::logic_error("Pair goal projection length mismatch");
        for (std::size_t i = 0; i < target.array.size(); ++i) add_half(target.array[i], source.array[i]);
    } else throw std::logic_error("Pair goal projection must be numeric");
}
void validate_goal_projection(const Value& root) {
    if (root.at("version").as_string() != "calculator_goal_set_v1")
        throw std::invalid_argument("Pair Calculator requires calculator_goal_set_v1");
    if (const auto* actions = root.find("actions"); actions && (actions->type != Type::Array || !actions->array.empty()))
        throw std::invalid_argument("Pair Calculator does not accept strategy actions");
    const std::set<std::string> supported{"version", "rarity", "slots", "min_satisfied_slots", "allow_extra_modifiers",
        "implicit_mod_keys", "influence_bits", "corrupted"};
    for (const auto& entry : root.at("goals").as_array()) {
        const auto& goal = entry.at("goal");
        if (goal.type != Type::Object) throw std::invalid_argument("Pair goal must be an object");
        for (const auto& [key, value] : goal.object) if (!supported.count(key))
            throw std::invalid_argument("Unsupported pair structural goal field: " + key);
        if (const auto* slots = goal.find("slots")) for (const auto& slot : slots->as_array())
            if (const auto* tier = slot.find("min_tier"); tier &&
                (tier->type != Type::Number || !std::isfinite(tier->number) || tier->number < 0 ||
                 tier->number > 1000 || tier->number != std::floor(tier->number)))
                throw std::invalid_argument("Pair tier threshold must be a bounded nonnegative integer");
    }
}
}
void validate_random_recomb_goal_projection(const char* text, std::size_t size) {
    if (!text || size > 256 * 1024) throw std::invalid_argument("Pair goal request byte cap exceeded");
    validate_goal_projection(json::Parser(text, size).parse());
}
std::string calculate_random_recomb_goals_json(const RandomRecombPair& pair, const char* text, std::size_t size) {
    if (!text || size > 256 * 1024) throw std::invalid_argument("Pair goal request byte cap exceeded");
    validate_goal_projection(json::Parser(text, size).parse());
    const auto& reference = *pair.inputs[0].session;
    const auto bound = solver::bind_calculator_goal_set(pair.inputs[0].session, text, size);
    const auto outcomes = enumerate_random_recomb_pair(pair);
    std::array<Value, 2> observed;
    Value carriers = array();
    for (unsigned carrier = 0; carrier < 2; ++carrier) {
        const auto& output = *pair.carriers[carrier].output_session;
        auto session = observation_session(reference, output);
        // A tiered group goal needs a reference family threshold for every
        // new member. Refuse an unbound source category instead of interpreting
        // an unrelated output-local tier ordinal as the original threshold.
        const auto reference_max_family = *std::max_element(reference.family_id.begin(), reference.family_id.end());
        for (const auto& goal : bound) for (const auto& slot : goal.explicit_goal.slots)
            if (slot.group_id != solver::kNoId && slot.min_tier) for (unsigned m = 0; m < session->mod_count; ++m)
                if (session->family_id[m] > reference_max_family && session->gen_type[m] <= 1)
                    for (auto row = session->group_offsets[m]; row < session->group_offsets[m + 1]; ++row)
                        if (session->group_ids[row] == slot.group_id)
                            throw std::invalid_argument("Pair tiered group goal has an unbound output family");
        auto goals = bound;
        for (auto& goal : goals) for (auto& mod : goal.item_goal.implicit_mods)
            mod = session->session_id_by_global_id.at(reference.data->mod_global_ids.at(reference.global_index.at(mod)));
        std::vector<std::uint64_t> reachable(session->words, 0);
        for (unsigned mod = 0; mod < session->mod_count; ++mod)
            if (session->gen_type[mod] <= 1) pc_bitset_set(reachable.data(), mod);
        solver::ActionRegistry registry; // terminal-only: no solver/action admission
        solver::CalcContext calc(session, goals.front().explicit_goal, std::move(registry), {}, true, false,
            false, std::nullopt, {}, false, reachable, false, false, false, false, false, nullptr, true, false, true);
        const auto result = solver::observe_calculator_terminal_law_json(calc, goals,
            [&](const solver::CalculatorTerminalSink& sink) {
                for (const auto& outcome : outcomes) if (outcome.carrier == carrier)
                    sink(mapped_item(materialize_random_recomb_outcome(pair, outcome), output, *session),
                        static_cast<long double>(outcome.probability) * 2);
            });
        observed[carrier] = json::Parser(result.data(), result.size()).parse();
        Value metadata = object();
        member(metadata, "carrier") = number(carrier); member(metadata, "probability") = number(.5);
        member(metadata, "base_metadata_path") = string(output.data->string_at(output.data->base_metadata_path_sid.at(output.base_index)));
        member(metadata, "item_level") = number(output.item_level);
        carriers.array.push_back(std::move(metadata));
    }
    auto result = std::move(observed[0]);
    for (const auto* key : {"success_probability", "any_goal_probability", "slot_satisfied", "implicit_satisfied"})
        add_half(member(result, key), observed[1].at(key));
    auto& goals = member(result, "goal_results").array;
    const auto& other_goals = observed[1].at("goal_results").array;
    if (goals.size() != other_goals.size()) throw std::logic_error("Pair goal identity mismatch");
    for (std::size_t g = 0; g < goals.size(); ++g) {
        if (goals[g].at("id").string != other_goals[g].at("id").string) throw std::logic_error("Pair goal ID order mismatch");
        for (const auto* key : {"success_probability", "slot_satisfied", "implicit_satisfied"})
            add_half(member(goals[g], key), other_goals[g].at(key));
    }
    auto& rows = member(result, "outcomes").array;
    const auto first_size = rows.size();
    for (auto row : observed[1].at("outcomes").array) rows.push_back(std::move(row));
    long double mass = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto carrier = i < first_size ? 0u : 1u;
        member(rows[i], "state") = number(double(i));
        auto& probability = member(rows[i], "probability"); probability.number *= .5; mass += probability.number;
        member(rows[i], "carrier") = number(carrier);
        member(rows[i], "base_metadata_path") = carriers.array[carrier].at("base_metadata_path");
        member(rows[i], "item_level") = carriers.array[carrier].at("item_level");
    }
    if (std::abs(mass - 1) > 1e-10L) throw std::logic_error("Pair finalizer failed probability conservation");
    member(result, "pair_version") = number(pair.version);
    member(result, "model_id") = string(pair.model_id);
    member(result, "projection_id") = string(kRandomRecombProjection);
    member(result, "goal_projection_id") = string("calculator-structural-goals-carrier-session-v1");
    member(result, "game_odds_estimated") = boolean(true);
    member(result, "model_projection_exact") = boolean(true);
    if (pair.scenario) {
        member(result,"scenario_id")=string(pair.scenario->id);
        member(result,"configuration_id")=string(kRandomRecombBlockingConfiguration);
        Value order=array();for(auto alpha:pair.scenario->prefix_first)order.array.push_back(number(alpha));
        member(result,"prefix_first")=std::move(order);
    }
    member(result, "apply_supported") = boolean(pair.full_item_apply_supported);
    member(result, "cost_complete") = boolean(false);
    member(result, "gold_cost") = Value{}; member(result, "dust_cost") = Value{};
    Value identity = array(); for (const auto& hash : pair.data_identity) identity.array.push_back(string(hash));
    member(result, "data_identity") = std::move(identity); member(result, "carriers") = std::move(carriers);
    Value unobserved = array(); for (const auto* field : {"recorded_rolls", "memory_strands", "sockets", "enchantments", "defence_percentiles"}) unobserved.array.push_back(string(field));
    member(result, "unobserved_properties") = std::move(unobserved);
    std::ostringstream out; out << std::setprecision(17); serialize(out, result);
    auto serialized = out.str();
    if (serialized.size() > 64ull * 1024 * 1024) throw std::length_error("Pair Calculator result byte cap exceeded");
    return serialized;
}
}
