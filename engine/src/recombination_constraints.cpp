#include "recombination_constraints.hpp"
#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace poecraft {
namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::invalid_argument(reason); }
bool natural_witness(const SessionImpl& session, unsigned mod) {
    const auto& data = *session.data; const auto p = session.global_index.at(mod);
    if (session.base_spawn_weight.at(mod) > 0) return true;
    const auto source_class = data.base_item_class_id.at(session.base_index);
    // Actual ordinary carrier tags, same item class, ordered first-match rows.
    // Level is irrelevant to a retained canonical tier's category.
    for (unsigned base = 0; base < data.base_count; ++base) {
        if (data.base_item_class_id.at(base) != source_class ||
            data.base_session_support.at(base) != PC_SESSION_SUPPORT_ORDINARY ||
            data.base_domain_code.at(base) != data.mod_domain_code.at(p)) continue;
        std::unordered_set<std::uint32_t> tags(data.base_tag_ids.begin() + data.base_tag_offsets.at(base),
            data.base_tag_ids.begin() + data.base_tag_offsets.at(base + 1));
        for (auto row = data.spawn_offsets.at(p); row < data.spawn_offsets.at(p + 1); ++row)
            if (tags.count(data.spawn_tag_ids.at(row))) {
                if (data.spawn_weights.at(row) > 0) return true;
                break;
            }
    }
    return false;
}
std::string quoted(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { const char* h = "0123456789abcdef"; out += "\\u00"; out += h[c>>4]; out += h[c&15]; }
        else out += char(c);
    }
    return out + "\"";
}
RecombOccurrence occurrence(const SessionImpl& session, const pc_mod_slot& slot, unsigned input, unsigned index) {
    RecombOccurrence result; result.input = std::uint8_t(input); result.slot = std::uint8_t(index);
    result.global_mod_id = session.data->mod_global_ids.at(session.global_index.at(slot.mod_id));
    result.groups.assign(session.group_ids.begin() + session.group_offsets.at(slot.mod_id),
                         session.group_ids.begin() + session.group_offsets.at(slot.mod_id + 1));
    return result;
}
}
const char* recombination_origin_name(RecombOrigin origin) {
    switch (origin) {
    case RecombOrigin::Natural: return "natural";
    case RecombOrigin::EssenceOnly: return "essence_only";
    case RecombOrigin::Metamod: return "metamod";
    case RecombOrigin::Delve: return "delve";
    case RecombOrigin::Unveiled: return "unveiled";
    case RecombOrigin::VeilTemplate: return "veiled_template";
    case RecombOrigin::Elevated: return "elevated";
    case RecombOrigin::BeastAspect: return "beast_aspect";
    case RecombOrigin::CraftedUnresolved: return "crafted_unresolved";
    case RecombOrigin::InfluencedNatural: return "influenced_natural";
    case RecombOrigin::NonNaturalUnresolved: return "non_natural_unresolved";
    case RecombOrigin::NotExplicit: return "not_explicit";
    }
    throw std::logic_error("Unknown recombination origin");
}
const char* recombination_exclusivity_name(RecombExclusivity kind) {
    switch (kind) {
    case RecombExclusivity::NonExclusive: return "non_exclusive";
    case RecombExclusivity::Exclusive: return "exclusive";
    case RecombExclusivity::Unresolved: return "unresolved";
    }
    throw std::logic_error("Unknown recombination exclusivity");
}
RecombModConstraint classify_recombination_mod(const SessionImpl& session, std::uint32_t mod) {
    require(mod < session.mod_count, "Recombination modifier is unavailable");
    const auto& data = *session.data; const auto p = session.global_index.at(mod);
    const auto global = data.mod_global_ids.at(p);
    RecombModConstraint result;
    result.explicit_modifier = session.gen_type.at(mod) == 0 || session.gen_type.at(mod) == 1;
    if (!result.explicit_modifier) { result.origin = RecombOrigin::NotExplicit; return result; }
    result.positive_source_spawn_proxy = session.base_spawn_weight.at(mod) > 0;
    const auto exclusive = [&](RecombOrigin origin) {
        auto out = result; out.origin = origin; out.exclusivity = RecombExclusivity::Exclusive; return out;
    };
    // Canonical metadata/relationships, not display-name or crafted-flag guesses.
    if (session.metamod_type.at(mod) >= 0) return exclusive(RecombOrigin::Metamod);
    if (session.flags.at(mod) & 1) return exclusive(RecombOrigin::EssenceOnly); // compiled essence_only flag
    const auto domain = data.domain_name(data.mod_domain_code.at(p));
    if (domain == "delve") return exclusive(RecombOrigin::Delve);
    if (data.special_unveiled_code >= 0 && session.special_kind.at(mod) == data.special_unveiled_code) return exclusive(RecombOrigin::Unveiled);
    if (data.special_veiled_template_code >= 0 && session.special_kind.at(mod) == data.special_veiled_template_code) return exclusive(RecombOrigin::VeilTemplate);
    if (std::any_of(data.influence_elevations.begin(), data.influence_elevations.end(),
            [&](const auto& relation) { return relation.second == global; })) return exclusive(RecombOrigin::Elevated);
    const auto type = data.string_at(data.mod_type_key_sid.at(p));
    if (type == "GrantsBirdAspect" || type == "GrantsCatAspect" ||
        type == "GrantsCrabAspect" || type == "GrantsSpiderAspect") return exclusive(RecombOrigin::BeastAspect);
    if (domain == "crafted") { result.origin = RecombOrigin::CraftedUnresolved; return result; }
    if (session.influence_code.at(mod) > 0) {
        result.origin = RecombOrigin::InfluencedNatural;
        result.exclusivity = RecombExclusivity::NonExclusive; return result;
    }
    if (session.flags.at(mod) == 0 && session.special_kind.at(mod) < 0 &&
            data.mod_domain_code.at(p) == data.base_domain_code.at(session.base_index) &&
            natural_witness(session, mod)) {
        result.origin = RecombOrigin::Natural; result.exclusivity = RecombExclusivity::NonExclusive;
        result.natural_on_compatible_base = true; result.natural_on_source = result.positive_source_spawn_proxy;
        result.guaranteed_natural_essence_source =
            static_cast<ReachKind>(session.reach_kind.at(mod)) == ReachKind::Essence;
    }
    return result;
}
bool recombination_mods_can_coexist(const RecombOccurrence& a, RecombExclusivity ac,
        const RecombOccurrence& b, RecombExclusivity bc) {
    if (a.global_mod_id == b.global_mod_id) return false;
    if (ac == RecombExclusivity::Exclusive && bc == RecombExclusivity::Exclusive) return false;
    for (auto group : a.groups)
        if (std::find(b.groups.begin(), b.groups.end(), group) != b.groups.end()) return false;
    // True establishes only these known constraints, not unresolved class legality.
    return true;
}
void validate_random_recomb_carrier_session(const SessionImpl& session) {
    require(session.data != nullptr, "Random recombination carrier data is missing");
    const auto& data = *session.data;
    require(!session.is_cluster() && session.base_index < data.base_count && session.rare_affix_cap == 3 &&
            data.base_session_support.at(session.base_index) == PC_SESSION_SUPPORT_ORDINARY,
            "Random recombination currently supports ordinary equipment carriers");
    require(session.item_level >= 1 && session.item_level <= 100, "Invalid represented recombination carrier level");
}
void validate_recombination_item_structure(const CraftResource& resource) {
    validate_craft_resource(resource);
    const auto& item = resource.item; const auto& session = *resource.session;
    require(item.prefix_count <= pc_item_max_prefix(&item) && item.suffix_count <= pc_item_max_suffix(&item),
            "Recombination item affix count exceeds its rarity capacity");
    std::set<std::uint32_t> groups;
    for (unsigned side = 0; side < 2; ++side) {
        const auto slots = side ? item.suffixes : item.prefixes;
        const auto count = side ? item.suffix_count : item.prefix_count;
        for (unsigned i = 0; i < count; ++i) {
            const auto id = slots[i].mod_id;
            require(session.group_offsets.at(id) < session.group_offsets.at(id+1),
                    "Recombination modifier has no canonical exclusion group");
            for (auto row = session.group_offsets.at(id); row < session.group_offsets.at(id+1); ++row)
                require(groups.insert(session.group_ids.at(row)).second,
                        "Recombination item contains conflicting physical modifiers");
        }
    }
}
void validate_recombination_resource_pair(const CraftResource& a, const CraftResource& b) {
    validate_craft_resource(a); validate_craft_resource(b);
    require(!a.identity.empty() && a.identity != b.identity && !a.role.empty() && !b.role.empty() && a.role != b.role,
            "Recombination requires distinct physical input identities and roles");
    const auto& da = *a.session->data; const auto& db = *b.session->data;
    const auto identity = [](const DataImpl& data) { return std::array<std::string,4>{
        data.artifact_data_hash,data.artifact_source_hash,data.artifact_game_data_hash,data.artifact_strings_hash}; };
    if (a.session->data != b.session->data) {
        const auto hashes = identity(da);
        require(std::all_of(hashes.begin(),hashes.end(),[](const auto& h){return !h.empty();}) && hashes == identity(db),
                "Recombination data identities differ or are incomplete");
    }
    require(da.base_item_class_id.at(a.session->base_index) == db.base_item_class_id.at(b.session->base_index),
            "Recombination input classes differ");
}
std::string inspect_random_recombination_constraints(const CraftResource& a, const CraftResource& b) {
    validate_recombination_resource_pair(a,b);
    std::array<const CraftResource*,2> inputs{&a,&b};
    std::vector<std::uint32_t> retained;
    for (const auto input : inputs) for (unsigned side = 0; side < 2; ++side) {
        const auto slots = side ? input->item.suffixes : input->item.prefixes;
        const auto count = side ? input->item.suffix_count : input->item.prefix_count;
        for (unsigned i = 0; i < count; ++i)
            retained.push_back(input->session->data->mod_global_ids.at(input->session->global_index.at(slots[i].mod_id)));
    }
    std::sort(retained.begin(),retained.end()); retained.erase(std::unique(retained.begin(),retained.end()),retained.end());
    std::array<SessionImpl,2> carriers;
    for (unsigned c = 0; c < 2; ++c) {
        carriers[c].data = inputs[c]->session->data; carriers[c].base_index = inputs[c]->session->base_index;
        carriers[c].item_level = random_recomb_item_level(a.session->item_level,b.session->item_level);
        build_session(carriers[c],retained); // existing native ordered-weight owner
    }
    std::vector<RecombOccurrence> occurrences; std::vector<RecombModConstraint> classes;
    std::array<unsigned,2> physical{}, known_exclusive{}, unresolved{};
    std::ostringstream out; out << std::setprecision(17);
    out << "{\"version\":\"recombination_constraints_v1\",\"constraint_authority\":" << quoted(kRecombinationConstraintAuthority)
        << ",\"probability_law_complete\":false,\"count_model\":null,\"side_order_model\":null,"
           "\"maximum_exclusive_output_occurrences\":1,\"data_identity\":[";
    const auto& data = *a.session->data; bool data_comma = false;
    for (const auto& hash : {data.artifact_data_hash,data.artifact_source_hash,data.artifact_game_data_hash,data.artifact_strings_hash}) {
        if (data_comma) out << ','; data_comma = true; out << quoted(hash);
    }
    out << "],\"carriers\":[";
    for (unsigned c = 0; c < 2; ++c) {
        if (c) out << ',';
        out << "{\"carrier\":" << c << ",\"base_metadata_path\":"
            << quoted(carriers[c].data->string_at(carriers[c].data->base_metadata_path_sid.at(carriers[c].base_index)))
            << ",\"output_item_level\":" << carriers[c].item_level << '}';
    }
    out << "],\"occurrences\":[";
    bool comma = false;
    for (unsigned input = 0; input < 2; ++input) for (unsigned side = 0; side < 2; ++side) {
        const auto& resource = *inputs[input]; const auto& session = *resource.session;
        const auto slots = side ? resource.item.suffixes : resource.item.prefixes;
        const auto count = side ? resource.item.suffix_count : resource.item.prefix_count;
        for (unsigned index = 0; index < count; ++index) {
            const auto& slot = slots[index]; const auto category = classify_recombination_mod(session,slot.mod_id);
            auto row = occurrence(session,slot,input,index);
            occurrences.push_back(row); classes.push_back(category); ++physical[side];
            known_exclusive[side] += category.exclusivity == RecombExclusivity::Exclusive;
            unresolved[side] += category.exclusivity == RecombExclusivity::Unresolved;
            if (comma) out << ','; comma = true;
            out << "{\"index\":" << occurrences.size()-1 << ",\"input\":" << input << ",\"side\":" << side
                << ",\"slot\":" << index << ",\"global_mod_id\":" << row.global_mod_id << ",\"mod_key\":"
                << quoted(session.data->string_at(session.data->mod_key_sid.at(session.global_index.at(slot.mod_id))))
                << ",\"origin\":" << quoted(recombination_origin_name(category.origin))
                << ",\"exclusivity\":" << quoted(recombination_exclusivity_name(category.exclusivity))
                << ",\"slot_flags\":" << unsigned(slot.flags)
                << ",\"positive_source_spawn_proxy\":" << (category.positive_source_spawn_proxy?"true":"false")
                << ",\"natural_on_source\":" << (category.natural_on_source?"true":"false")
                << ",\"natural_on_compatible_base\":" << (category.natural_on_compatible_base?"true":"false")
                << ",\"guaranteed_natural_essence_source\":" << (category.guaranteed_natural_essence_source?"true":"false")
                << ",\"carrier_spawn_proxies\":[";
            for (unsigned c = 0; c < 2; ++c) {
                if (c) out << ',';
                const auto id = carriers[c].session_id_by_global_id.at(row.global_mod_id);
                out << carriers[c].base_spawn_weight.at(id);
            }
            out << "],\"groups\":[";
            for (unsigned g = 0; g < row.groups.size(); ++g) { if (g) out << ','; out << row.groups[g]; }
            out << "]}";
        }
    }
    out << "],\"sides\":[";
    for (unsigned side = 0; side < 2; ++side) {
        if (side) out << ',';
        out << "{\"side\":" << side << ",\"physical_count\":" << physical[side]
            << ",\"known_exclusive_occurrences\":" << known_exclusive[side]
            << ",\"unresolved_occurrences\":" << unresolved[side]
            << ",\"effective_count\":null}";
    }
    out << "],\"known_conflicts\":["; comma = false;
    for (unsigned i = 0; i < occurrences.size(); ++i) for (unsigned j = i+1; j < occurrences.size(); ++j)
        if (!recombination_mods_can_coexist(occurrences[i],classes[i].exclusivity,occurrences[j],classes[j].exclusivity)) {
            if (comma) out << ','; comma = true; out << '[' << i << ',' << j << ']';
        }
    out << "]}"; return out.str();
}
}
