#include "recombination.hpp"
#include "recombination_constraints.hpp"
#include <algorithm>
#include <functional>
#include <cmath>
#include <tuple>
#include <sstream>
#include <map>
#include <set>
#include <stdexcept>

namespace poecraft {
namespace {
void require(bool ok, const char* reason) {
    if (!ok) throw std::invalid_argument(reason);
}
bool conflict(const RecombOccurrence& a, const RecombOccurrence& b) {
    return !recombination_mods_can_coexist(a, a.exclusive ? RecombExclusivity::Exclusive : RecombExclusivity::NonExclusive,
        b, b.exclusive ? RecombExclusivity::Exclusive : RecombExclusivity::NonExclusive);
}
void validate_pool(const std::vector<RecombOccurrence>& pool, bool blocking = false) {
    require(pool.size() <= 6, "Random recombination side exceeds six physical occurrences");
    require(blocking || std::count_if(pool.begin(), pool.end(), [](const auto& o) { return o.exclusive; }) <= 1,
            "Multiple-exclusive count law is unresolved; physical padding cannot use the ordinary row");
    std::set<std::pair<unsigned, unsigned>> identities;
    for (const auto& o : pool) {
        require(o.input < 2 && o.slot < 3 && o.global_mod_id != PC_MOD_NONE &&
                !o.groups.empty() && identities.emplace(o.input, o.slot).second,
                "Invalid random recombination occurrence identity or groups");
    }
}
std::vector<unsigned> candidates(const std::vector<RecombOccurrence>& pool,
                                const std::vector<unsigned>& selected) {
    std::vector<unsigned> result;
    for (unsigned i = 0; i < pool.size(); ++i) {
        if (!pool[i].spawn_weight) continue;
        bool blocked = false;
        for (auto picked : selected) if (conflict(pool[i], pool[picked])) { blocked = true; break; }
        if (!blocked) result.push_back(i);
    }
    return result;
}
std::uint64_t total_weight(const std::vector<RecombOccurrence>& pool,
                           const std::vector<unsigned>& ids) {
    std::uint64_t total = 0;
    for (auto i : ids) total += pool[i].spawn_weight;
    return total;
}
std::uint32_t global_id(const SessionImpl& session, const pc_mod_slot& slot) {
    return session.data->mod_global_ids.at(session.global_index.at(slot.mod_id));
}
void remap_slots(const SessionImpl& from, const SessionImpl& to,
                 pc_mod_slot* slots, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        auto& slot = slots[i];
        slot.mod_id = to.session_id_by_global_id.at(global_id(from, slot));
        slot.group_id = static_cast<std::uint16_t>(to.primary_group.at(slot.mod_id));
        for (unsigned j = 0; j < slot.veiled_option_count; ++j) {
            const auto global = from.data->mod_global_ids.at(from.global_index.at(slot.veiled_option_mod_ids[j]));
            slot.veiled_option_mod_ids[j] = to.session_id_by_global_id.at(global);
        }
        if (slot.veiled_chosen_mod_id != PC_MOD_NONE) {
            const auto global = from.data->mod_global_ids.at(from.global_index.at(slot.veiled_chosen_mod_id));
            slot.veiled_chosen_mod_id = to.session_id_by_global_id.at(global);
        }
    }
}
void validate_ordinary_input(const CraftResource& resource) {
    validate_recombination_item_structure(resource);
    const auto& s = *resource.session; const auto& item = resource.item;
    validate_random_recomb_carrier_session(s);
    require(!(item.item_flags & ~(PC_ITEM_SPLIT | PC_ITEM_SYNTHESISED)) &&
            item.generic_influence_bits == 0,
            "Random recombination input category is unsupported");

    const auto check = [&](const pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            const auto id = slot.mod_id;
            require(slot.flags == 0 && slot.veiled_option_count == 0 &&
                    slot.veiled_chosen_mod_id == PC_MOD_NONE && s.influence_code.at(id) <= 0,
                    "Random recombination fractured/crafted/veiled-slot/influence output treatment is unsupported");
            const auto category = classify_recombination_mod(s,id);
            require(category.exclusivity != RecombExclusivity::Unresolved,
                    "Random recombination modifier exclusivity is unresolved; inspect native constraints");
            const bool natural = category.origin == RecombOrigin::Natural;
            const bool single_special = category.origin == RecombOrigin::EssenceOnly ||
                category.origin == RecombOrigin::Unveiled || category.origin == RecombOrigin::Delve ||
                category.origin == RecombOrigin::BeastAspect;
            require(natural || single_special, "Random recombination known category still requires output/weight authority");
            if (natural) require(category.natural_on_source || category.guaranteed_natural_essence_source,
                    "Random recombination non-native source requires a native guaranteed Essence origin");
        }
    };
    check(item.prefixes, item.prefix_count); check(item.suffixes, item.suffix_count);
}
void validate_pair_projection(const RandomRecombPair& pair) {
    const bool blocking = pair.model_id == kRandomRecombBlockingModel;
    require(pair.version == 1 && (blocking || pair.model_id == kRandomRecombModel || pair.model_id == kRandomRecombExtendedModel) &&
            pair.game_odds_estimated && pair.full_item_apply_supported == !blocking && bool(pair.scenario) == blocking,
            "Unsupported random recombination projection/model version");
    if (blocking) {
        require(!pair.scenario->id.empty(), "Blocking analysis requires an explicit scenario identity");
        for (auto alpha : pair.scenario->prefix_first)
            require(std::isfinite(alpha) && alpha >= 0 && alpha <= 1,
                    "Blocking analysis requires explicit carrier-specific first-side probabilities");
    }
    for (const auto& carrier : pair.carriers) {
        require(carrier.output_session != nullptr, "Random recombination output session is missing");
        validate_pool(carrier.sides[0], blocking); validate_pool(carrier.sides[1], blocking);
        unsigned exclusive = 0;
        for (const auto& side : carrier.sides) for (const auto& mod : side) exclusive += mod.exclusive;
        require(blocking || exclusive <= 1, "Multiple-exclusive joint count/side-order law is unresolved");
        for (const auto& p : carrier.sides[0]) for (const auto& s : carrier.sides[1])
            require(recombination_mods_can_coexist(p, RecombExclusivity::NonExclusive, s, RecombExclusivity::NonExclusive),
                    "Random recombination cross-side canonical group overlap is held");
    }
}
}
std::string random_recomb_item_key(const pc_item_state& item) {
    std::ostringstream out;
    for (auto v : {item.rarity, item.quality, item.memory_strands, item.lifecycle,
            item.item_flags, item.generic_influence_bits, item.searing_exarch_tier,
            item.eater_of_worlds_tier, item.socket_count, item.link_mask})
        out << unsigned(v) << ':';
    for (unsigned i = 0; i < item.socket_count; ++i) out << unsigned(item.socket_colors[i]) << ':';
    const auto slots = [&](const pc_mod_slot* values, unsigned count) {
        out << '/' << count << ':';
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = values[i];
            out << slot.mod_id << ':' << slot.group_id << ':' << unsigned(slot.flags)
                << ':' << unsigned(slot.roll_count) << ':';
            for (unsigned j = 0; j < slot.roll_count; ++j) out << slot.rolls[j] << ':';
            out << unsigned(slot.veiled_option_count) << ':';
            for (unsigned j = 0; j < slot.veiled_option_count; ++j) out << slot.veiled_option_mod_ids[j] << ':';
            out << slot.veiled_chosen_mod_id << ';';
        }
    };
    slots(item.prefixes, item.prefix_count); slots(item.suffixes, item.suffix_count);
    slots(item.implicits, item.implicit_count); slots(item.enchantments, item.enchantment_count);
    return out.str();
}
const std::array<unsigned, 4>& random_recomb_count_row(unsigned count) {
    static constexpr std::array<std::array<unsigned, 4>, 7> rows{{
        {{1000, 0, 0, 0}}, {{410, 590, 0, 0}}, {{0, 667, 333, 0}},
        {{0, 400, 500, 100}}, {{0, 100, 600, 300}},
        {{0, 0, 430, 570}}, {{0, 0, 300, 700}}
    }};
    require(count < rows.size(), "Random recombination count table is unavailable");
    return rows[count];
}
std::uint32_t random_recomb_item_level(std::uint32_t a, std::uint32_t b) {
    require(a >= 1 && a <= 100 && b >= 1 && b <= 100, "Invalid recombination input level");
    return std::min((a + b) / 2 + 2, std::max(a, b));
}
std::vector<RecombSideOutcome> enumerate_random_recomb_side(const std::vector<RecombOccurrence>& pool) {
    validate_pool(pool);
    std::map<std::pair<unsigned, std::vector<unsigned>>, double> masses;
    const auto& row = random_recomb_count_row(static_cast<unsigned>(pool.size()));
    for (unsigned count = 0; count < row.size(); ++count) {
        if (!row[count]) continue;
        std::function<void(std::vector<unsigned>, double)> visit;
        visit = [&](std::vector<unsigned> picked, double mass) {
            const auto available = candidates(pool, picked);
            if (picked.size() == count || available.empty()) {
                std::sort(picked.begin(), picked.end());
                masses[{count, picked}] += mass;
                return;
            }
            const auto total = total_weight(pool, available);
            for (auto i : available) {
                auto next = picked; next.push_back(i);
                visit(std::move(next), mass * (static_cast<double>(pool[i].spawn_weight) / total));
            }
        };
        visit({}, row[count] / 1000.0);
    }
    std::vector<RecombSideOutcome> result;
    for (const auto& [key, mass] : masses) result.push_back({mass, key.first, key.second});
    return result;
}
RecombSideOutcome sample_random_recomb_side(Rng& rng, const std::vector<RecombOccurrence>& pool) {
    validate_pool(pool); // refuse before drawing
    const auto& row = random_recomb_count_row(static_cast<unsigned>(pool.size()));
    const auto saved = rng;
    try {
        auto roll = rng.next_below(1000);
        unsigned count = 0;
        for (; count < 3; ++count) { if (roll < row[count]) break; roll -= row[count]; }
        RecombSideOutcome result; result.requested_count = count;
        while (result.occurrences.size() < count) {
            const auto available = candidates(pool, result.occurrences);
            if (available.empty()) break;
            auto draw = rng.next_below(total_weight(pool, available));
            for (auto i : available) {
                if (draw < pool[i].spawn_weight) { result.occurrences.push_back(i); break; }
                draw -= pool[i].spawn_weight;
            }
        }
        std::sort(result.occurrences.begin(), result.occurrences.end());
        // A sampled projection carries no estimated probability inferred from a single draw.
        return result;
    } catch (...) { rng = saved; throw; }
}
RandomRecombPair prepare_random_recomb_pair(const CraftResource& a, const CraftResource& b, const RecombScenario* scenario) {
    validate_ordinary_input(a); validate_ordinary_input(b);
    validate_recombination_resource_pair(a,b);
    const auto& da = *a.session->data;
    const auto identity = [](const DataImpl& d) { return std::array<std::string, 4>{
        d.artifact_data_hash, d.artifact_source_hash, d.artifact_game_data_hash, d.artifact_strings_hash}; };

    const bool exceptional = (a.item.prefix_count == 1 && a.item.suffix_count == 0 &&
                              b.item.prefix_count == 0 && b.item.suffix_count == 1) ||
                             (b.item.prefix_count == 1 && b.item.suffix_count == 0 &&
                              a.item.prefix_count == 0 && a.item.suffix_count == 1);
    require(!exceptional, "Random recombination 1p0s plus 0p1s joint law is unresolved");
    RandomRecombPair pair;
    pair.inputs = {a, b}; pair.data_identity = identity(da);
    unsigned exclusive_count = 0;
    for (const auto& resource : pair.inputs) for (unsigned side = 0; side < 2; ++side) {
        const auto slots = side ? resource.item.suffixes : resource.item.prefixes;
        const auto count = side ? resource.item.suffix_count : resource.item.prefix_count;
        for (unsigned i = 0; i < count; ++i) {
            const auto category = classify_recombination_mod(*resource.session, slots[i].mod_id);
            exclusive_count += category.exclusivity == RecombExclusivity::Exclusive;
            if (category.exclusivity == RecombExclusivity::Exclusive || !category.natural_on_source)
                pair.model_id = kRandomRecombExtendedModel;
        }
    }
    require(scenario || exclusive_count <= 1, "Multiple-exclusive joint count/side-order law is unresolved");
    if (scenario) {
        pair.model_id = kRandomRecombBlockingModel; pair.scenario = *scenario;
        pair.full_item_apply_supported = false;
    }
    const auto level = random_recomb_item_level(a.session->item_level, b.session->item_level);
    for (unsigned c = 0; c < 2; ++c) {
        const auto& carrier = pair.inputs[c];
        const auto& source_session = *carrier.session;
        std::vector<std::uint32_t> retained;
        const auto append = [&](const SessionImpl& s, const pc_mod_slot* slots, unsigned count) {
            for (unsigned i = 0; i < count; ++i) {
                retained.push_back(global_id(s, slots[i]));
                for (unsigned j = 0; j < slots[i].veiled_option_count; ++j)
                    retained.push_back(s.data->mod_global_ids.at(s.global_index.at(slots[i].veiled_option_mod_ids[j])));
                if (slots[i].veiled_chosen_mod_id != PC_MOD_NONE)
                    retained.push_back(s.data->mod_global_ids.at(s.global_index.at(slots[i].veiled_chosen_mod_id)));
            }
        };
        for (const auto& input : pair.inputs) {
            append(*input.session, input.item.prefixes, input.item.prefix_count);
            append(*input.session, input.item.suffixes, input.item.suffix_count);
        }
        append(source_session, carrier.item.implicits, carrier.item.implicit_count);
        append(source_session, carrier.item.enchantments, carrier.item.enchantment_count);
        std::sort(retained.begin(), retained.end());
        retained.erase(std::unique(retained.begin(), retained.end()), retained.end());
        auto output_session = std::make_shared<SessionImpl>();
        output_session->data = carrier.session->data;
        output_session->base_index = source_session.base_index; output_session->item_level = level;
        build_session(*output_session, retained);
        auto& out = pair.carriers[c]; out.output_session = output_session;
        out.properties = carrier.item;
        pc_item_clear_side(&out.properties, PC_SIDE_PREFIX);
        pc_item_clear_side(&out.properties, PC_SIDE_SUFFIX);
        out.properties.rarity = PC_RARITY_RARE;
        remap_slots(source_session, *output_session, out.properties.implicits, out.properties.implicit_count);
        remap_slots(source_session, *output_session, out.properties.enchantments, out.properties.enchantment_count);
        for (unsigned side = 0; side < 2; ++side) for (unsigned input = 0; input < 2; ++input) {
            const auto& r = pair.inputs[input]; const auto& s = *r.session;
            const auto slots = side == 0 ? r.item.prefixes : r.item.suffixes;
            const unsigned count = side == 0 ? r.item.prefix_count : r.item.suffix_count;
            for (unsigned i = 0; i < count; ++i) {
                RecombOccurrence o;
                o.input = static_cast<std::uint8_t>(input); o.slot = static_cast<std::uint8_t>(i);
                o.global_mod_id = global_id(s, slots[i]);
                o.output_mod_id = output_session->session_id_by_global_id.at(o.global_mod_id);
                o.spawn_weight = output_session->base_spawn_weight.at(o.output_mod_id);
                o.exclusive = classify_recombination_mod(s, slots[i].mod_id).exclusivity == RecombExclusivity::Exclusive;
                require(!o.exclusive || o.spawn_weight > 0,
                        "Known exclusive modifier has no carrier spawn proxy; special selection weights are unresolved");
                o.groups.assign(s.group_ids.begin() + s.group_offsets.at(slots[i].mod_id),
                                s.group_ids.begin() + s.group_offsets.at(slots[i].mod_id + 1));
                out.sides[side].push_back(std::move(o));
            }
        }
    }
    validate_pair_projection(pair);
    return pair;
}
std::vector<RecombJointOutcome> enumerate_random_recomb_joint(
        const std::array<std::vector<RecombOccurrence>, 2>& pools, unsigned first_side) {
    require(first_side < 2, "Invalid first side for blocking analysis");
    std::array<unsigned,2> effective{};
    for (unsigned side = 0; side < 2; ++side) {
        validate_pool(pools[side], true);
        bool exclusive = false;
        for (const auto& o : pools[side]) { if (o.exclusive) exclusive = true; else ++effective[side]; }
        effective[side] += exclusive;
    }
    using Key = std::tuple<unsigned,unsigned,std::vector<unsigned>,std::vector<unsigned>>;
    std::map<Key,double> masses;
    const auto& pr = random_recomb_count_row(effective[0]);
    const auto& sr = random_recomb_count_row(effective[1]);
    for (unsigned p = 0; p < 4; ++p) for (unsigned s = 0; s < 4; ++s) {
        if (!pr[p] || !sr[s]) continue;
        const std::array<unsigned,2> requested{p,s};
        std::function<void(unsigned,std::array<std::vector<unsigned>,2>,double)> visit;
        visit = [&](unsigned stage, std::array<std::vector<unsigned>,2> selected, double mass) {
            if (stage == 2) {
                for (auto& side : selected) std::sort(side.begin(),side.end());
                masses[{p,s,selected[0],selected[1]}] += mass; return;
            }
            const unsigned side = stage == 0 ? first_side : 1-first_side;
            std::vector<unsigned> available;
            for (unsigned i = 0; i < pools[side].size(); ++i) {
                const auto& candidate = pools[side][i]; if (!candidate.spawn_weight) continue;
                bool blocked = false;
                for (unsigned d = 0; d < 2; ++d) for (auto chosen : selected[d])
                    if (conflict(candidate,pools[d][chosen])) blocked = true;
                if (!blocked) available.push_back(i);
            }
            if (selected[side].size() == requested[side] || available.empty()) {
                visit(stage+1,std::move(selected),mass); return;
            }
            const auto total = total_weight(pools[side],available);
            for (auto i : available) {
                auto next = selected; next[side].push_back(i);
                visit(stage,std::move(next),mass * (static_cast<double>(pools[side][i].spawn_weight)/total));
            }
        };
        visit(0,{},(pr[p]/1000.0)*(sr[s]/1000.0));
    }
    std::vector<RecombJointOutcome> result;
    for (const auto& [key,mass] : masses)
        result.push_back({mass,{0,std::get<0>(key),std::get<2>(key)},{0,std::get<1>(key),std::get<3>(key)}});
    return result;
}
std::vector<RandomRecombOutcome> enumerate_random_recomb_pair(const RandomRecombPair& pair) {
    validate_pair_projection(pair);
    std::vector<RandomRecombOutcome> result;
    for (unsigned c = 0; c < 2; ++c) {
        if (pair.scenario) {
            for (unsigned first = 0; first < 2; ++first) {
                const double weight = first == 0 ? pair.scenario->prefix_first[c] : 1-pair.scenario->prefix_first[c];
                if (!weight) continue;
                for (const auto& row : enumerate_random_recomb_joint(pair.carriers[c].sides, first))
                    result.push_back({.5*weight*row.probability,c,row.prefixes,row.suffixes});
            }
            continue;
        }
        const auto ps = enumerate_random_recomb_side(pair.carriers[c].sides[0]);
        const auto ss = enumerate_random_recomb_side(pair.carriers[c].sides[1]);
        for (const auto& p : ps) for (const auto& s : ss)
            result.push_back({0.5 * p.probability * s.probability, c, p, s});
    }
    return result;
}
RandomRecombOutcome sample_random_recomb_selection(Rng& rng, const RandomRecombPair& pair) {
    validate_pair_projection(pair);
    require(pair.full_item_apply_supported, "Advanced blocking analysis has no approved random Apply law");
    const auto saved = rng;
    try {
        RandomRecombOutcome result; result.carrier = static_cast<unsigned>(rng.next_below(2));
        const auto& carrier = pair.carriers[result.carrier];
        result.prefixes = sample_random_recomb_side(rng, carrier.sides[0]);
        result.suffixes = sample_random_recomb_side(rng, carrier.sides[1]);
        return result;
    } catch (...) { rng = saved; throw; }
}

pc_item_state materialize_random_recomb_outcome(const RandomRecombPair& pair,
        const RandomRecombOutcome& outcome) {
    validate_pair_projection(pair);
    require(outcome.carrier < 2, "Invalid random recombination carrier");
    const auto& carrier = pair.carriers[outcome.carrier];
    auto item = carrier.properties;
    const auto add = [&](unsigned side, const RecombSideOutcome& selection) {
        require(selection.requested_count <= 3 && selection.occurrences.size() <= selection.requested_count,
                "Invalid recombination selected count");
        std::vector<unsigned> previous;
        for (auto index : selection.occurrences) {
            require(index < carrier.sides[side].size(), "Invalid recombination selected occurrence");
            const auto& occurrence = carrier.sides[side][index];
            require(occurrence.spawn_weight > 0, "Ineligible recombination selected occurrence");
            for (auto picked : previous)
                require(!conflict(occurrence, carrier.sides[side][picked]),
                        "Conflicting recombination selected occurrences");
            previous.push_back(index);
            const auto& source = pair.inputs[occurrence.input];
            auto slot = side == 0 ? source.item.prefixes[occurrence.slot] : source.item.suffixes[occurrence.slot];
            // Approved approximation: selected canonical tier and recorded rolls
            // survive, with no fabricated numerical reroll or tier upgrade.
            slot.mod_id = occurrence.output_mod_id;
            slot.group_id = static_cast<std::uint16_t>(carrier.output_session->primary_group.at(slot.mod_id));
            if (side == 0) item.prefixes[item.prefix_count++] = slot;
            else item.suffixes[item.suffix_count++] = slot;
        }
    };
    for (auto p : outcome.prefixes.occurrences) for (auto s : outcome.suffixes.occurrences)
        require(!conflict(carrier.sides[0].at(p),carrier.sides[1].at(s)), "Global recombination output conflict");
    add(0, outcome.prefixes); add(1, outcome.suffixes);
    return item;
}
CraftTransaction prepare_random_recomb_transaction(ActionContextImpl& context,
        const RandomRecombPair& pair, const std::string& output_identity) {
    validate_pair_projection(pair);
    require(!output_identity.empty() && output_identity != pair.inputs[0].identity &&
            output_identity != pair.inputs[1].identity, "Recombination requires a new output identity");
    require(context.session == pair.inputs[0].session || context.session == pair.inputs[1].session,
            "Recombination context must belong to an input session");
    const auto saved = context.rng;
    try {
        const auto selection = sample_random_recomb_selection(context.rng, pair);
        CraftResource output{output_identity, "", pair.carriers[selection.carrier].output_session,
                             materialize_random_recomb_outcome(pair, selection)};
        // No station gold/dust cost has been guessed. An empty currency-key list
        // is not cost completeness; the pair/result explicitly report both unknown.
        return prepare_two_input_result({pair.inputs[0], pair.inputs[1]}, output, {});
    } catch (...) { context.rng = saved; throw; }
}
CraftTransaction apply_random_recomb_transaction(ActionContextImpl& context,
        std::vector<CraftResource>& resources, const RandomRecombPair& pair,
        const std::string& output_identity) {
    const auto saved = context.rng;
    try {
        auto transaction = prepare_random_recomb_transaction(context, pair, output_identity);
        commit_craft_transaction(resources, transaction);
        return transaction;
    } catch (...) { context.rng = saved; throw; }
}
}
