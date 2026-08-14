#include "engine_internal.hpp"
#include "harvest_crafts.generated.hpp"
#include "json.hpp"
#include "poecraft/session.h"
#include "solver_eval_types.hpp"
#include "solver_refinement.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace poecraft {

namespace {

using json::Type;
using json::Value;

[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error("strategy: " + message);
}

const Value& require_object_member(
    const Value& object,
    const char* key,
    Type type) {
    const Value* value = object.find(key);
    if (value == nullptr || value->type != type) {
        invalid(std::string("expected ") + key);
    }
    return *value;
}

std::string string_member(
    const Value& object,
    const char* key,
    const std::string& fallback = std::string()) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::String) {
        invalid(std::string(key) + " must be a string");
    }
    return value->string;
}

int int_member(const Value& object, const char* key, int fallback) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::Number || !std::isfinite(value->number)) {
        invalid(std::string(key) + " must be a number");
    }
    return static_cast<int>(value->number);
}

std::uint32_t uint_member(
        const Value& object,
        const char* key,
        const std::uint32_t fallback) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::Number ||
        !std::isfinite(value->number) ||
        value->number < 0.0 ||
        value->number >
            static_cast<double>(
                std::numeric_limits<std::uint32_t>::max()) ||
        std::floor(value->number) != value->number) {
        invalid(std::string(key) + " must be an unsigned integer");
    }
    return static_cast<std::uint32_t>(value->number);
}

std::uint64_t parse_hex_u64(
        const Value& value,
        const char* subject) {
    if (value.type != Type::String || value.string.size() != 16) {
        invalid(std::string(subject) +
                " must be a 16-digit lowercase hexadecimal string");
    }
    std::uint64_t parsed = 0;
    for (const char character : value.string) {
        std::uint8_t digit = 0;
        if (character >= '0' && character <= '9') {
            digit = static_cast<std::uint8_t>(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            digit = static_cast<std::uint8_t>(
                character - 'a' + 10);
        } else {
            invalid(std::string(subject) +
                    " must use lowercase hexadecimal digits");
        }
        parsed = (parsed << 4) | digit;
    }
    return parsed;
}

std::vector<std::uint64_t> parse_hex_vector(
        const Value& object,
        const char* key) {
    const Value& values =
        require_object_member(object, key, Type::Array);
    std::vector<std::uint64_t> parsed;
    parsed.reserve(values.array.size());
    for (const Value& value : values.array) {
        parsed.push_back(parse_hex_u64(value, key));
    }
    return parsed;
}

std::vector<std::uint32_t> parse_sorted_u32_array(
        const Value& object,
        const char* key) {
    const Value& values =
        require_object_member(object, key, Type::Array);
    std::vector<std::uint32_t> parsed;
    parsed.reserve(values.array.size());
    for (const Value& value : values.array) {
        if (value.type != Type::Number ||
            !std::isfinite(value.number) ||
            value.number < 0.0 ||
            value.number >
                static_cast<double>(
                    std::numeric_limits<std::uint32_t>::max()) ||
            std::floor(value.number) != value.number) {
            invalid(std::string(key) +
                    " entries must be unsigned integers");
        }
        parsed.push_back(
            static_cast<std::uint32_t>(value.number));
    }
    if (!std::is_sorted(parsed.begin(), parsed.end()) ||
        std::adjacent_find(
            parsed.begin(), parsed.end()) != parsed.end()) {
        invalid(std::string(key) +
                " must be sorted and unique");
    }
    return parsed;
}

bool bool_member(const Value& object, const char* key, bool fallback) {
    const Value* value = object.find(key);
    if (value == nullptr) return fallback;
    if (value->type != Type::Bool) {
        invalid(std::string(key) + " must be a bool");
    }
    return value->boolean;
}

std::vector<std::string> accounting_roles_member(const Value& object) {
    const Value* value = object.find("accounting_roles");
    if (value == nullptr) return {};
    if (value->type != Type::Array) {
        invalid("accounting_roles must be an array");
    }
    static const std::vector<std::string> allowed{
        "ordinary_crafting", "restart", "fracture_preparation",
        "fracture", "retry_action", "retry", "temporary_blocker",
        "permanent_goal_bench", "multimod_setup", "multimod_finish",
        "protection_setup", "protection_reapplication", "cleanup_or_replacement",
        "deterministic_finish"};
    std::vector<std::string> roles;
    for (const Value& entry : value->array) {
        if (entry.type != Type::String ||
            std::find(allowed.begin(), allowed.end(), entry.string) ==
                allowed.end()) {
            invalid("unknown accounting role");
        }
        if (std::find(roles.begin(), roles.end(), entry.string) == roles.end()) {
            roles.push_back(entry.string);
        }
    }
    return roles;
}

int rarity_from_name(const std::string& name) {
    if (name == "normal") return PC_RARITY_NORMAL;
    if (name == "magic") return PC_RARITY_MAGIC;
    if (name == "rare") return PC_RARITY_RARE;
    invalid("unknown rarity: " + name);
}

std::uint8_t side_cap(const SessionImpl& session, const pc_item_state& item) {
    if (item.rarity == PC_RARITY_MAGIC) return 1;
    if (item.rarity == PC_RARITY_RARE) return session.rare_affix_cap;
    return 0;
}

void add_start_mod(
    const SessionImpl& session,
    pc_item_state& item,
    const Value& value,
    int side) {
    std::string key;
    bool fractured = false;
    bool crafted = false;
    bool veiled = false;
    if (value.type == Type::String) {
        key = value.string;
    } else if (value.type == Type::Object) {
        key = string_member(value, "mod_key");
        if (key.empty()) key = string_member(value, "key");
        fractured = bool_member(value, "fractured", false);
        crafted = bool_member(value, "crafted", false);
        veiled = bool_member(value, "veiled", false);
    } else {
        invalid("base_state explicit mods must be strings or objects");
    }
    if (key.empty()) invalid("base_state explicit mod requires mod_key");

    const DataImpl& data = *session.data;
    const auto pos_it = data.mod_pos_by_key.find(key);
    if (pos_it == data.mod_pos_by_key.end()) {
        invalid("base_state mod key not found: " + key);
    }
    const std::uint32_t global_id = data.mod_global_ids[pos_it->second];
    const auto session_it = session.session_id_by_global_id.find(global_id);
    if (session_it == session.session_id_by_global_id.end()) {
        invalid("base_state mod is not in the session: " + key);
    }
    const std::uint32_t mod_id = session_it->second;
    if (session.gen_type[mod_id] != side) {
        invalid("base_state mod is on the wrong affix side: " + key);
    }
    std::uint8_t flags = 0;
    if (fractured) flags |= PC_MOD_SLOT_FRACTURED;
    if (crafted) flags |= PC_MOD_SLOT_CRAFTED;
    if (veiled) flags |= PC_MOD_SLOT_VEILED;
    if (pc_item_add_mod(
            &item,
            side,
            mod_id,
            static_cast<std::uint16_t>(session.primary_group[mod_id]),
            flags,
            nullptr) != PC_RESULT_OK) {
        invalid("base_state has too many explicit mods");
    }
}

pc_item_state parse_start_item(
    const SessionImpl& session,
    const Value& base_state) {
    const DataImpl& data = *session.data;
    const std::string expected_base =
        data.string_at(data.base_metadata_path_sid[session.base_index]);
    const std::string base_key = string_member(base_state, "base_key");
    if (base_key.empty() || base_key != expected_base) {
        invalid("base_state.base_key does not match the compile session");
    }
    const int item_level =
        int_member(base_state, "item_level", static_cast<int>(session.item_level));
    if (item_level != static_cast<int>(session.item_level)) {
        invalid("base_state.item_level does not match the compile session");
    }

    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = static_cast<std::uint8_t>(
        rarity_from_name(string_member(base_state, "rarity", "normal")));

    if (bool_member(base_state, "with_implicits", true)) {
        if (session.base_implicit_mod_ids.size() > PC_MAX_IMPLICITS) {
            invalid("base implicit count exceeds item capacity");
        }
        for (std::uint32_t mod_id : session.base_implicit_mod_ids) {
            pc_mod_slot& slot = item.implicits[item.implicit_count++];
            slot.mod_id = mod_id;
            slot.group_id =
                static_cast<std::uint16_t>(session.primary_group[mod_id]);
        }
    }

    item.quality =
        static_cast<std::uint8_t>(int_member(base_state, "quality", 0));
    item.item_flags =
        static_cast<std::uint8_t>(int_member(base_state, "item_flags", 0));
    if (bool_member(base_state, "corrupted", false)) {
        item.item_flags |= PC_ITEM_CORRUPTED;
    }
    if (bool_member(base_state, "mirrored", false)) {
        item.item_flags |= PC_ITEM_MIRRORED;
    }
    if (bool_member(base_state, "split", false)) {
        item.item_flags |= PC_ITEM_SPLIT;
    }
    if (bool_member(base_state, "synthesised", false)) {
        item.item_flags |= PC_ITEM_SYNTHESISED;
    }
    item.generic_influence_bits = static_cast<std::uint8_t>(
        int_member(base_state, "generic_influence_bits", 0));
    item.searing_exarch_tier = static_cast<std::uint8_t>(
        int_member(base_state, "searing_exarch_tier", 0));
    item.eater_of_worlds_tier = static_cast<std::uint8_t>(
        int_member(base_state, "eater_of_worlds_tier", 0));
    if (item.searing_exarch_tier > 4 || item.eater_of_worlds_tier > 4) {
        invalid("base-state eldritch tiers must be 0-4");
    }

    for (const auto& spec :
         (base_state.find("prefixes") != nullptr
              ? require_object_member(base_state, "prefixes", Type::Array).array
              : json::Array{})) {
        add_start_mod(session, item, spec, PC_SIDE_PREFIX);
    }
    for (const auto& spec :
         (base_state.find("suffixes") != nullptr
              ? require_object_member(base_state, "suffixes", Type::Array).array
              : json::Array{})) {
        add_start_mod(session, item, spec, PC_SIDE_SUFFIX);
    }
    return item;
}

CompiledCondition compile_condition(
    const SessionImpl& session,
    const Value& value,
    int depth = 0) {
    if (depth > 32) invalid("condition nesting is too deep");
    if (value.type != Type::Object) invalid("condition must be an object");
    const std::string type = string_member(value, "type");
    if (type.empty()) invalid("condition requires type");

    const DataImpl& data = *session.data;
    CompiledCondition out;
    if (type == "always") {
        out.kind = ConditionKind::Always;
        return out;
    }
    if (type == "has_mod_group") {
        const std::string group = string_member(value, "group");
        const auto it = data.group_id_by_key.find(group);
        if (it == data.group_id_by_key.end()) {
            invalid("unknown mod group: " + group);
        }
        out.kind = ConditionKind::HasModGroup;
        out.group_id = it->second;
        out.min_value = int_member(value, "min_tier", 0);
        if (bool_member(value, "fractured", false)) {
            out.required_flags |= PC_MOD_SLOT_FRACTURED;
        }
        if (bool_member(value, "crafted", false)) {
            out.required_flags |= PC_MOD_SLOT_CRAFTED;
        }
        if (out.min_value < 0) {
            invalid("has_mod_group min_tier must be non-negative");
        }
        return out;
    }
    if (type == "has_mod_family") {
        std::string key = string_member(value, "family_mod_key");
        if (key.empty()) key = string_member(value, "mod_key");
        if (key.empty()) {
            invalid("has_mod_family requires family_mod_key");
        }
        const auto pos_it = data.mod_pos_by_key.find(key);
        if (pos_it == data.mod_pos_by_key.end()) {
            invalid("unknown modifier family key: " + key);
        }
        const std::uint32_t global_id = data.mod_global_ids[pos_it->second];
        const auto session_it =
            session.session_id_by_global_id.find(global_id);
        if (session_it == session.session_id_by_global_id.end()) {
            invalid("modifier family is not in the session: " + key);
        }
        const std::uint32_t mod_id = session_it->second;
        out.kind = ConditionKind::HasModFamily;
        out.family_id = session.family_id[mod_id];
        out.min_value = int_member(value, "min_tier", 0);
        if (bool_member(value, "fractured", false)) {
            out.required_flags |= PC_MOD_SLOT_FRACTURED;
        }
        if (bool_member(value, "crafted", false)) {
            out.required_flags |= PC_MOD_SLOT_CRAFTED;
        }
        if (out.min_value < 0) {
            invalid("has_mod_family min_tier must be non-negative");
        }
        return out;
    }
    const auto resolve_mod_key = [&](const std::string& key,
                                     const char* label) {
        const auto pos = data.mod_pos_by_key.find(key);
        if (pos == data.mod_pos_by_key.end()) {
            invalid(std::string("unknown ") + label + ": " + key);
        }
        const auto session_mod = session.session_id_by_global_id.find(
            data.mod_global_ids[pos->second]);
        if (session_mod == session.session_id_by_global_id.end()) {
            invalid(std::string(label) + " is not in the session: " + key);
        }
        return session_mod->second;
    };
    if (type == "observation_signature") {
        const Value& requirement_value =
            require_object_member(value, "requirement", Type::Object);
        out.kind = ConditionKind::ObservationSignature;
        CompiledObservationSignature& compiled =
            out.observation_signature;
        compiled.version =
            uint_member(value, "version", 0);
        if (compiled.version !=
            kObservationSignatureConditionVersion) {
            invalid("unsupported observation_signature version");
        }
        compiled.item_features =
            uint_member(requirement_value, "item_features", 0);
        if ((compiled.item_features &
             ~solver::kAllRefinementItemFeatures) != 0) {
            invalid(
                "observation_signature item_features contains "
                "unknown or affix features");
        }
        compiled.modifier_tag_ids =
            parse_sorted_u32_array(
                requirement_value, "modifier_tag_ids");
        const Value& observations = require_object_member(
            requirement_value, "affix_observations", Type::Array);
        compiled.affix_observations.reserve(
            observations.array.size());
        for (const Value& observation_value :
             observations.array) {
            if (observation_value.type != Type::Object) {
                invalid(
                    "observation_signature affix observation must "
                    "be an object");
            }
            CompiledObservationAffixRequirement observation;
            observation.features =
                uint_member(observation_value, "features", 0);
            if (observation.features == 0 ||
                (observation.features &
                 ~solver::kAllRefinementAffixFeatures) != 0) {
                invalid(
                    "observation_signature affix features are "
                    "empty or invalid");
            }
            const Value& selector = require_object_member(
                observation_value, "selector", Type::Object);
            const std::uint32_t required_affix_traits =
                uint_member(
                    selector, "required_affix_traits", 0);
            const std::uint32_t forbidden_affix_traits =
                uint_member(
                    selector, "forbidden_affix_traits", 0);
            const std::uint32_t required_item_traits =
                uint_member(
                    selector, "required_item_traits", 0);
            const std::uint32_t forbidden_item_traits =
                uint_member(
                    selector, "forbidden_item_traits", 0);
            if (required_affix_traits >
                    std::numeric_limits<std::uint16_t>::max() ||
                forbidden_affix_traits >
                    std::numeric_limits<std::uint16_t>::max() ||
                required_item_traits >
                    std::numeric_limits<std::uint8_t>::max() ||
                forbidden_item_traits >
                    std::numeric_limits<std::uint8_t>::max() ||
                (required_affix_traits &
                 ~solver::kAllRefinementAffixTraits) != 0 ||
                (forbidden_affix_traits &
                 ~solver::kAllRefinementAffixTraits) != 0 ||
                (required_item_traits &
                 ~solver::kAllRefinementItemTraits) != 0 ||
                (forbidden_item_traits &
                 ~solver::kAllRefinementItemTraits) != 0) {
                invalid(
                    "observation_signature selector traits are "
                    "invalid");
            }
            observation.selector.required_affix_traits =
                static_cast<std::uint16_t>(
                    required_affix_traits);
            observation.selector.forbidden_affix_traits =
                static_cast<std::uint16_t>(
                    forbidden_affix_traits);
            observation.selector.required_item_traits =
                static_cast<std::uint8_t>(
                    required_item_traits);
            observation.selector.forbidden_item_traits =
                static_cast<std::uint8_t>(
                    forbidden_item_traits);
            observation.selector.required_tag_ids =
                parse_sorted_u32_array(
                    selector, "required_tag_ids");
            compiled.affix_observations.push_back(
                std::move(observation));
        }
        const Value& atoms =
            require_object_member(value, "signature", Type::Array);
        compiled.atoms.reserve(atoms.array.size());
        for (const Value& atom_value : atoms.array) {
            if (atom_value.type != Type::Object) {
                invalid(
                    "observation_signature atom must be an object");
            }
            const std::uint32_t feature =
                uint_member(atom_value, "feature", solver::kNoId);
            if (feature >= static_cast<std::uint8_t>(
                               solver::RefinementFeature::Count)) {
                invalid(
                    "observation_signature atom feature is invalid");
            }
            CompiledObservationAtom atom;
            atom.feature = static_cast<std::uint8_t>(feature);
            atom.subject =
                uint_member(atom_value, "subject", 0);
            atom.value =
                parse_hex_vector(atom_value, "value");
            compiled.atoms.push_back(std::move(atom));
        }
        const auto parse_mod_values =
            [&](const char* key,
                std::vector<CompiledObservationModValue>& output) {
                const Value& entries =
                    require_object_member(value, key, Type::Array);
                output.reserve(entries.array.size());
                for (const Value& entry : entries.array) {
                    if (entry.type != Type::Object) {
                        invalid(
                            std::string(key) +
                            " entry must be an object");
                    }
                    const std::string mod_key =
                        string_member(entry, "mod_key");
                    if (mod_key.empty()) {
                        invalid(
                            std::string(key) +
                            " entry requires mod_key");
                    }
                    output.push_back({
                        resolve_mod_key(
                            mod_key,
                            "observation_signature modifier"),
                        parse_hex_vector(entry, "value")});
                }
                if (!std::is_sorted(
                        output.begin(), output.end(),
                        [](const CompiledObservationModValue& left,
                           const CompiledObservationModValue& right) {
                            return left.mod_id < right.mod_id;
                        }) ||
                    std::adjacent_find(
                        output.begin(), output.end(),
                        [](const CompiledObservationModValue& left,
                           const CompiledObservationModValue& right) {
                            return left.mod_id == right.mod_id;
                        }) != output.end()) {
                    invalid(
                        std::string(key) +
                        " must resolve to sorted unique modifiers");
                }
            };
        parse_mod_values(
            "goal_status_tier_class_by_mod",
            compiled.goal_status_tier_class_by_mod);
        compiled.count_observation_count =
            uint_member(
                value, "count_observation_count", 0);
        parse_mod_values(
            "count_observation_membership_by_mod",
            compiled.count_observation_membership_by_mod);

        solver::refinement::ObservationRequirement requirement;
        requirement.item_features = compiled.item_features;
        requirement.modifier_tag_ids =
            compiled.modifier_tag_ids;
        for (const CompiledObservationAffixRequirement& observation :
             compiled.affix_observations) {
            solver::RefinementAffixObservation converted;
            converted.features = observation.features;
            converted.selector.required_affix_traits =
                observation.selector.required_affix_traits;
            converted.selector.forbidden_affix_traits =
                observation.selector.forbidden_affix_traits;
            converted.selector.required_item_traits =
                observation.selector.required_item_traits;
            converted.selector.forbidden_item_traits =
                observation.selector.forbidden_item_traits;
            converted.selector.required_tag_ids =
                observation.selector.required_tag_ids;
            requirement.affix_observations.push_back(
                std::move(converted));
        }
        if (solver::refinement::canonical_observation_requirement(
                requirement) != requirement) {
            invalid(
                "observation_signature requirement is not canonical");
        }
        solver::refinement::FeatureSignature signature;
        signature.reserve(compiled.atoms.size());
        for (const CompiledObservationAtom& atom :
             compiled.atoms) {
            signature.push_back({
                static_cast<solver::RefinementFeature>(
                    atom.feature),
                atom.subject,
                atom.value,
                0,
                0,
                {}});
        }
        if (solver::refinement::canonical_feature_signature(
                signature) != signature) {
            invalid(
                "observation_signature signature is not canonical");
        }
        try {
            out.observation_program =
                solver::refinement::
                    make_compiled_observation_program(
                        session, compiled);
        } catch (const std::exception& error) {
            invalid(
                std::string{
                    "invalid observation_signature program: "} +
                error.what());
        }
        return out;
    }
    if (type == "mod_count") {
        const Value* keys = value.find("mod_keys");
        if (keys == nullptr || keys->type != Type::Array ||
            keys->array.empty()) {
            invalid("mod_count requires a non-empty mod_keys array");
        }
        out.kind = ConditionKind::ModCount;
        for (const Value& key : keys->array) {
            if (key.type != Type::String) {
                invalid("mod_count mod_keys must be strings");
            }
            out.mod_ids.push_back(resolve_mod_key(key.string, "modifier"));
        }
        std::sort(out.mod_ids.begin(), out.mod_ids.end());
        out.mod_ids.erase(
            std::unique(out.mod_ids.begin(), out.mod_ids.end()),
            out.mod_ids.end());
        out.min_value = int_member(value, "min", 0);
        out.max_value = int_member(
            value, "max", PC_MAX_PREFIXES + PC_MAX_SUFFIXES);
        if (bool_member(value, "fractured", false)) {
            out.required_flags |= PC_MOD_SLOT_FRACTURED;
        }
        if (bool_member(value, "crafted", false)) {
            out.required_flags |= PC_MOD_SLOT_CRAFTED;
        }
        if (out.min_value < 0 || out.max_value < out.min_value) {
            invalid("mod_count range is invalid");
        }
        return out;
    }
    if (type == "mod_family_count") {
        const Value* keys = value.find("family_mod_keys");
        if (keys == nullptr || keys->type != Type::Array ||
            keys->array.empty()) {
            invalid(
                "mod_family_count requires a non-empty family_mod_keys array");
        }
        out.kind = ConditionKind::ModFamilyCount;
        for (const Value& key : keys->array) {
            if (key.type != Type::String) {
                invalid("mod_family_count family_mod_keys must be strings");
            }
            const std::uint32_t mod_id =
                resolve_mod_key(key.string, "modifier family");
            out.family_ids.push_back(session.family_id[mod_id]);
        }
        std::sort(out.family_ids.begin(), out.family_ids.end());
        out.family_ids.erase(
            std::unique(out.family_ids.begin(), out.family_ids.end()),
            out.family_ids.end());
        out.min_value = int_member(value, "min", 0);
        out.max_value = int_member(
            value, "max", PC_MAX_PREFIXES + PC_MAX_SUFFIXES);
        if (bool_member(value, "fractured", false)) {
            out.required_flags |= PC_MOD_SLOT_FRACTURED;
        }
        if (bool_member(value, "crafted", false)) {
            out.required_flags |= PC_MOD_SLOT_CRAFTED;
        }
        if (out.min_value < 0 || out.max_value < out.min_value) {
            invalid("mod_family_count range is invalid");
        }
        return out;
    }
    if (type == "item_flag") {
        static const std::pair<const char*, ItemFlagKind> flags[] = {
            {"corrupted", ItemFlagKind::Corrupted},
            {"mirrored", ItemFlagKind::Mirrored},
            {"split", ItemFlagKind::Split},
            {"synthesised", ItemFlagKind::Synthesised},
            {"fractured", ItemFlagKind::Fractured},
            {"crafted", ItemFlagKind::Crafted},
            {"veiled", ItemFlagKind::Veiled},
            {"veiled_prefix", ItemFlagKind::VeiledPrefix},
            {"veiled_suffix", ItemFlagKind::VeiledSuffix},
            {"multimod", ItemFlagKind::Multimod},
            {"no_attack", ItemFlagKind::NoAttack},
            {"no_caster", ItemFlagKind::NoCaster},
            {"prefixes_locked", ItemFlagKind::PrefixesLocked},
            {"suffixes_locked", ItemFlagKind::SuffixesLocked},
            {"influenced", ItemFlagKind::Influenced},
            {"eldritch_implicit", ItemFlagKind::EldritchImplicit},
        };
        const std::string flag = string_member(value, "flag");
        const auto found = std::find_if(
            std::begin(flags), std::end(flags),
            [&](const auto& entry) { return flag == entry.first; });
        if (found == std::end(flags)) invalid("unknown item flag: " + flag);
        out.kind = ConditionKind::ItemFlag;
        out.item_flag = found->second;
        return out;
    }
    if (type == "influence_bits") {
        out.kind = ConditionKind::InfluenceBits;
        out.min_value = int_member(value, "value", 0);
        if (out.min_value < 0 || out.min_value > 255) {
            invalid("influence_bits value must be 0-255");
        }
        return out;
    }
    if (type == "eldritch_tier") {
        const std::string side = string_member(value, "side");
        if (side != "searing" && side != "eater") {
            invalid("eldritch_tier side must be searing or eater");
        }
        out.kind = ConditionKind::EldritchTier;
        out.eldritch_side = side == "eater" ? 1 : 0;
        out.min_value = int_member(value, "min", 0);
        out.max_value = int_member(value, "max", 4);
        if (out.min_value < 0 || out.max_value < out.min_value ||
            out.max_value > 4) {
            invalid("eldritch_tier range is invalid");
        }
        return out;
    }
    if (type == "has_unveil_option") {
        const std::string key = string_member(value, "mod_key");
        if (key.empty()) invalid("has_unveil_option requires mod_key");
        out.kind = ConditionKind::HasUnveilOption;
        out.mod_ids.push_back(resolve_mod_key(key, "unveil option"));
        return out;
    }
    if (type == "rarity_is") {
        std::string rarity = string_member(value, "rarity");
        if (rarity.empty()) rarity = string_member(value, "value");
        out.kind = ConditionKind::RarityIs;
        out.min_value = rarity_from_name(rarity);
        out.max_value = out.min_value;
        return out;
    }

    auto compile_range = [&](ConditionKind kind, int default_max) {
        out.kind = kind;
        out.min_value = int_member(
            value,
            "min",
            int_member(value, "value", int_member(value, "count", 0)));
        out.max_value = int_member(value, "max", default_max);
        if (out.min_value < 0 || out.max_value < out.min_value) {
            invalid("condition count range is invalid");
        }
    };
    if (type == "open_prefix_count") {
        compile_range(ConditionKind::OpenPrefixCount, PC_MAX_PREFIXES);
        return out;
    }
    if (type == "open_suffix_count") {
        compile_range(ConditionKind::OpenSuffixCount, PC_MAX_SUFFIXES);
        return out;
    }
    if (type == "prefix_count_range") {
        compile_range(ConditionKind::PrefixCountRange, PC_MAX_PREFIXES);
        return out;
    }
    if (type == "suffix_count_range") {
        compile_range(ConditionKind::SuffixCountRange, PC_MAX_SUFFIXES);
        return out;
    }

    const Value* children = value.find("conditions");
    if (children == nullptr) children = value.find("children");
    if (type == "all" || type == "all_of" || type == "any" ||
        type == "any_of" || type == "not" || type == "at_least") {
        if (children == nullptr || children->type != Type::Array) {
            invalid("composite condition requires conditions array");
        }
        if (children->array.size() > 1024) {
            invalid("composite condition has too many children");
        }
        if (type == "all" || type == "all_of") out.kind = ConditionKind::All;
        if (type == "any" || type == "any_of") out.kind = ConditionKind::Any;
        if (type == "not") out.kind = ConditionKind::Not;
        if (type == "at_least") {
            out.kind = ConditionKind::AtLeast;
            out.min_value = int_member(value, "count", 1);
        }
        for (const auto& child : children->array) {
            out.children.push_back(
                compile_condition(session, child, depth + 1));
        }
        if (out.kind == ConditionKind::Not && out.children.size() != 1) {
            invalid("not condition requires exactly one child");
        }
        if (out.kind == ConditionKind::AtLeast &&
            (out.min_value < 0 ||
             out.min_value > static_cast<int>(out.children.size()))) {
            invalid("at_least count is outside the child range");
        }
        return out;
    }
    invalid("unknown condition type: " + type);
}

std::size_t condition_hash(const CompiledCondition& condition) {
    std::size_t hash = static_cast<std::size_t>(1469598103934665603ULL);
    const auto combine = [&](const std::uint64_t value) {
        hash ^= static_cast<std::size_t>(value) +
                static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                (hash << 6) + (hash >> 2);
    };
    combine(static_cast<std::uint8_t>(condition.kind));
    combine(condition.group_id);
    combine(condition.family_id);
    combine(condition.required_flags);
    combine(static_cast<std::uint8_t>(condition.item_flag));
    combine(condition.eldritch_side);
    combine(static_cast<std::uint32_t>(condition.min_value));
    combine(static_cast<std::uint32_t>(condition.max_value));
    combine(condition.mod_ids.size());
    for (const std::uint32_t mod_id : condition.mod_ids) combine(mod_id);
    combine(condition.family_ids.size());
    for (const std::uint32_t family_id : condition.family_ids) {
        combine(family_id);
    }
    const CompiledObservationSignature& observation =
        condition.observation_signature;
    combine(observation.version);
    combine(observation.item_features);
    combine(observation.modifier_tag_ids.size());
    for (const std::uint32_t tag : observation.modifier_tag_ids) {
        combine(tag);
    }
    combine(observation.affix_observations.size());
    for (const CompiledObservationAffixRequirement& affix :
         observation.affix_observations) {
        combine(affix.features);
        combine(affix.selector.required_affix_traits);
        combine(affix.selector.forbidden_affix_traits);
        combine(affix.selector.required_item_traits);
        combine(affix.selector.forbidden_item_traits);
        combine(affix.selector.required_tag_ids.size());
        for (const std::uint32_t tag :
             affix.selector.required_tag_ids) {
            combine(tag);
        }
    }
    combine(observation.atoms.size());
    for (const CompiledObservationAtom& atom :
         observation.atoms) {
        combine(atom.feature);
        combine(atom.subject);
        combine(atom.value.size());
        for (const std::uint64_t value : atom.value) {
            combine(value);
        }
    }
    const auto combine_mod_values =
        [&](const std::vector<CompiledObservationModValue>& values) {
            combine(values.size());
            for (const CompiledObservationModValue& value : values) {
                combine(value.mod_id);
                combine(value.value.size());
                for (const std::uint64_t token : value.value) {
                    combine(token);
                }
            }
        };
    combine_mod_values(
        observation.goal_status_tier_class_by_mod);
    combine(observation.count_observation_count);
    combine_mod_values(
        observation.count_observation_membership_by_mod);
    combine(condition.children.size());
    for (const CompiledCondition& child : condition.children) {
        combine(condition_hash(child));
    }
    return hash;
}

bool condition_equal(
    const CompiledCondition& left,
    const CompiledCondition& right) {
    if (left.kind != right.kind || left.group_id != right.group_id ||
        left.family_id != right.family_id ||
        left.required_flags != right.required_flags ||
        left.item_flag != right.item_flag ||
        left.eldritch_side != right.eldritch_side ||
        left.min_value != right.min_value ||
        left.max_value != right.max_value ||
        left.mod_ids != right.mod_ids ||
        left.family_ids != right.family_ids ||
        left.observation_signature !=
            right.observation_signature ||
        left.children.size() != right.children.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.children.size(); ++i) {
        if (!condition_equal(left.children[i], right.children[i])) {
            return false;
        }
    }
    return true;
}

void assign_condition_memo_slots(
    CompiledCondition& condition,
    std::unordered_map<
        std::size_t, std::vector<const CompiledCondition*>>& by_hash,
    std::uint32_t& next_slot,
    std::unordered_map<
        std::size_t, std::vector<const CompiledCondition*>>& count_by_hash,
    std::uint32_t& next_count_slot) {
    for (CompiledCondition& child : condition.children) {
        assign_condition_memo_slots(
            child, by_hash, next_slot, count_by_hash, next_count_slot);
    }
    if (condition.kind == ConditionKind::ModCount ||
        condition.kind == ConditionKind::ModFamilyCount) {
        std::size_t count_hash =
            static_cast<std::size_t>(1469598103934665603ULL);
        const auto combine = [&](const std::uint64_t value) {
            count_hash ^= static_cast<std::size_t>(value) +
                          static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                          (count_hash << 6) + (count_hash >> 2);
        };
        combine(static_cast<std::uint8_t>(condition.kind));
        combine(condition.required_flags);
        for (const std::uint32_t mod : condition.mod_ids) combine(mod);
        for (const std::uint32_t family : condition.family_ids) {
            combine(family);
        }
        for (const CompiledCondition* existing :
             count_by_hash[count_hash]) {
            if (existing->kind == condition.kind &&
                existing->required_flags == condition.required_flags &&
                existing->mod_ids == condition.mod_ids &&
                existing->family_ids == condition.family_ids) {
                condition.count_memo_slot = existing->count_memo_slot;
                break;
            }
        }
        if (condition.count_memo_slot ==
            std::numeric_limits<std::uint32_t>::max()) {
            condition.count_memo_slot = next_count_slot++;
            count_by_hash[count_hash].push_back(&condition);
        }
    }
    const std::size_t hash = condition_hash(condition);
    for (const CompiledCondition* existing : by_hash[hash]) {
        if (condition_equal(*existing, condition)) {
            condition.memo_slot = existing->memo_slot;
            return;
        }
    }
    condition.memo_slot = next_slot++;
    by_hash[hash].push_back(&condition);
}

struct DispatchClause {
    std::uint32_t condition = 0;
    bool expected = true;
};

struct DispatchCandidate {
    std::uint32_t edge = 0;
    std::vector<DispatchClause> clauses;
};

std::size_t direct_feature_hash(const CompiledCondition& condition) {
    std::size_t hash = static_cast<std::size_t>(1469598103934665603ULL);
    const auto combine = [&](const std::uint64_t value) {
        hash ^= static_cast<std::size_t>(value) +
                static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                (hash << 6) + (hash >> 2);
    };
    combine(static_cast<std::uint8_t>(condition.kind));
    switch (condition.kind) {
    case ConditionKind::HasModGroup:
        combine(condition.group_id);
        combine(condition.required_flags);
        combine(static_cast<std::uint32_t>(condition.min_value));
        break;
    case ConditionKind::HasModFamily:
        combine(condition.family_id);
        combine(condition.required_flags);
        combine(static_cast<std::uint32_t>(condition.min_value));
        break;
    case ConditionKind::ModCount:
        combine(condition.required_flags);
        for (const std::uint32_t mod : condition.mod_ids) combine(mod);
        break;
    case ConditionKind::ModFamilyCount:
        combine(condition.required_flags);
        for (const std::uint32_t family : condition.family_ids) {
            combine(family);
        }
        break;
    case ConditionKind::ItemFlag:
        combine(static_cast<std::uint8_t>(condition.item_flag));
        break;
    case ConditionKind::EldritchTier:
        combine(condition.eldritch_side);
        break;
    case ConditionKind::HasUnveilOption:
        for (const std::uint32_t mod : condition.mod_ids) combine(mod);
        break;
    case ConditionKind::ObservationSignature:
        combine(condition_hash(condition));
        break;
    default:
        break;
    }
    return hash;
}

bool direct_feature_equal(
    const CompiledCondition& left,
    const CompiledCondition& right) {
    if (left.kind != right.kind) return false;
    switch (left.kind) {
    case ConditionKind::HasModGroup:
        return left.group_id == right.group_id &&
               left.required_flags == right.required_flags &&
               left.min_value == right.min_value;
    case ConditionKind::HasModFamily:
        return left.family_id == right.family_id &&
               left.required_flags == right.required_flags &&
               left.min_value == right.min_value;
    case ConditionKind::ModCount:
        return left.required_flags == right.required_flags &&
               left.mod_ids == right.mod_ids;
    case ConditionKind::ModFamilyCount:
        return left.required_flags == right.required_flags &&
               left.family_ids == right.family_ids;
    case ConditionKind::ItemFlag:
        return left.item_flag == right.item_flag;
    case ConditionKind::EldritchTier:
        return left.eldritch_side == right.eldritch_side;
    case ConditionKind::HasUnveilOption:
        return left.mod_ids == right.mod_ids;
    case ConditionKind::ObservationSignature:
        return left.observation_signature ==
               right.observation_signature;
    default:
        return true;
    }
}

bool direct_required_value(
    const CompiledCondition& condition,
    const bool expected,
    std::int16_t& value) {
    switch (condition.kind) {
    case ConditionKind::Always:
    case ConditionKind::HasModGroup:
    case ConditionKind::HasModFamily:
    case ConditionKind::ItemFlag:
    case ConditionKind::HasUnveilOption:
    case ConditionKind::ObservationSignature:
        value = expected ? 1 : 0;
        return true;
    case ConditionKind::RarityIs:
    case ConditionKind::InfluenceBits:
        if (!expected) return false;
        value = static_cast<std::int16_t>(condition.min_value);
        return true;
    case ConditionKind::OpenPrefixCount:
    case ConditionKind::OpenSuffixCount:
    case ConditionKind::PrefixCountRange:
    case ConditionKind::SuffixCountRange:
    case ConditionKind::ModCount:
    case ConditionKind::ModFamilyCount:
    case ConditionKind::EldritchTier:
        if (!expected || condition.min_value != condition.max_value) {
            return false;
        }
        value = static_cast<std::int16_t>(condition.min_value);
        return true;
    case ConditionKind::All:
    case ConditionKind::Any:
    case ConditionKind::Not:
    case ConditionKind::AtLeast:
        return false;
    }
    return false;
}

std::size_t direct_signature_hash(const std::vector<std::int16_t>& values) {
    std::size_t hash = static_cast<std::size_t>(1469598103934665603ULL);
    for (const std::int16_t value : values) {
        hash ^= static_cast<std::size_t>(
                    static_cast<std::uint16_t>(value)) +
                static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                (hash << 6) + (hash >> 2);
    }
    hash ^= values.size() + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
            (hash << 6) + (hash >> 2);
    return hash;
}

bool flatten_dispatch_condition(
    const CompiledCondition& condition,
    bool expected,
    StrategyNode& node,
    std::unordered_map<std::uint32_t, std::uint32_t>& local_by_memo,
    std::vector<DispatchClause>& clauses) {
    if (condition.kind == ConditionKind::All && expected) {
        for (const CompiledCondition& child : condition.children) {
            if (!flatten_dispatch_condition(
                    child, true, node, local_by_memo, clauses)) {
                return false;
            }
        }
        return true;
    }
    if (condition.kind == ConditionKind::Not &&
        condition.children.size() == 1) {
        return flatten_dispatch_condition(
            condition.children.front(), !expected, node, local_by_memo,
            clauses);
    }
    if (condition.kind == ConditionKind::Any ||
        condition.kind == ConditionKind::AtLeast ||
        condition.kind == ConditionKind::All ||
        condition.kind == ConditionKind::Not) {
        return false;
    }
    const auto [found, inserted] = local_by_memo.emplace(
        condition.memo_slot,
        static_cast<std::uint32_t>(node.dispatch_conditions.size()));
    if (inserted) node.dispatch_conditions.push_back(condition);
    const std::uint32_t local = found->second;
    for (const DispatchClause& clause : clauses) {
        if (clause.condition == local) {
            return clause.expected == expected;
        }
    }
    clauses.push_back({local, expected});
    return true;
}

void build_dispatch(StrategyNode& node) {
    node.dispatch_conditions.clear();
    node.dispatch_nodes.clear();
    node.dispatch_signatures.clear();
    node.direct_dispatch_features.clear();
    node.direct_dispatch_signatures.clear();
    if (node.edges.size() < 8) return;

    std::unordered_map<std::uint32_t, std::uint32_t> local_by_memo;
    std::vector<DispatchCandidate> candidates;
    candidates.reserve(node.edges.size());
    for (std::uint32_t edge = 0; edge < node.edges.size(); ++edge) {
        DispatchCandidate candidate;
        candidate.edge = edge;
        if (!node.edges[edge].is_default &&
            !flatten_dispatch_condition(
                node.edges[edge].condition, true, node, local_by_memo,
                candidate.clauses)) {
            node.dispatch_conditions.clear();
            return;
        }
        candidates.push_back(std::move(candidate));
    }

    std::unordered_map<std::size_t, std::vector<std::uint32_t>>
        direct_features_by_hash;
    for (const DispatchCandidate& candidate : candidates) {
        for (const DispatchClause& clause : candidate.clauses) {
            const CompiledCondition& condition =
                node.dispatch_conditions[clause.condition];
            const std::size_t hash = direct_feature_hash(condition);
            bool found = false;
            for (const std::uint32_t feature :
                 direct_features_by_hash[hash]) {
                if (direct_feature_equal(
                        node.direct_dispatch_features[feature], condition)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                direct_features_by_hash[hash].push_back(
                    static_cast<std::uint32_t>(
                        node.direct_dispatch_features.size()));
                node.direct_dispatch_features.push_back(condition);
            }
        }
    }
    constexpr std::int16_t kUnassigned =
        std::numeric_limits<std::int16_t>::min();
    for (const DispatchCandidate& candidate : candidates) {
        if (node.edges[candidate.edge].is_default) continue;
        StrategyDirectDispatchSignature signature;
        signature.edge = candidate.edge;
        signature.values.assign(
            node.direct_dispatch_features.size(), kUnassigned);
        bool supported = true;
        for (const DispatchClause& clause : candidate.clauses) {
            const CompiledCondition& condition =
                node.dispatch_conditions[clause.condition];
            const std::size_t hash = direct_feature_hash(condition);
            std::uint32_t feature = std::numeric_limits<std::uint32_t>::max();
            for (const std::uint32_t possible :
                 direct_features_by_hash[hash]) {
                if (direct_feature_equal(
                        node.direct_dispatch_features[possible], condition)) {
                    feature = possible;
                    break;
                }
            }
            std::int16_t required = 0;
            if (feature == std::numeric_limits<std::uint32_t>::max() ||
                !direct_required_value(
                    condition, clause.expected, required) ||
                (signature.values[feature] != kUnassigned &&
                 signature.values[feature] != required)) {
                supported = false;
                break;
            }
            signature.values[feature] = required;
        }
        if (!supported) continue;

        /* Absence of a family/group at a looser threshold also proves every
         * stricter-tier and required-flag variant false. This fills the
         * predicates omitted as logically redundant by the policy compiler. */
        for (std::uint32_t target = 0;
             target < signature.values.size(); ++target) {
            if (signature.values[target] != kUnassigned) continue;
            const CompiledCondition& wanted =
                node.direct_dispatch_features[target];
            if (wanted.kind != ConditionKind::HasModFamily &&
                wanted.kind != ConditionKind::HasModGroup) {
                continue;
            }
            for (std::uint32_t source = 0;
                 source < signature.values.size(); ++source) {
                const CompiledCondition& known =
                    node.direct_dispatch_features[source];
                if (known.kind != wanted.kind) continue;
                const bool same_target =
                    wanted.kind == ConditionKind::HasModFamily
                        ? known.family_id == wanted.family_id
                        : known.group_id == wanted.group_id;
                const bool false_implies_target =
                    signature.values[source] == 0 &&
                    (known.required_flags & wanted.required_flags) ==
                        known.required_flags &&
                    (known.min_value == 0 ||
                     (wanted.min_value != 0 &&
                      known.min_value >= wanted.min_value));
                const bool true_implies_target =
                    signature.values[source] == 1 &&
                    (wanted.required_flags & known.required_flags) ==
                        wanted.required_flags &&
                    (wanted.min_value == 0 ||
                     (known.min_value != 0 &&
                      wanted.min_value >= known.min_value));
                if (same_target &&
                    (false_implies_target || true_implies_target)) {
                    signature.values[target] =
                        true_implies_target ? 1 : 0;
                    break;
                }
            }
        }
        if (std::find(
                signature.values.begin(), signature.values.end(),
                kUnassigned) != signature.values.end()) {
            continue;
        }
        auto& collisions = node.direct_dispatch_signatures[
            direct_signature_hash(signature.values)];
        const bool duplicate = std::any_of(
            collisions.begin(), collisions.end(),
            [&](const StrategyDirectDispatchSignature& existing) {
                return existing.values == signature.values;
            });
        if (!duplicate) collisions.push_back(std::move(signature));
    }

    const auto signature_hash = [](const std::vector<std::uint32_t>& values) {
        std::size_t hash =
            static_cast<std::size_t>(1469598103934665603ULL);
        for (const std::uint32_t value : values) {
            hash ^= static_cast<std::size_t>(value) +
                    static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                    (hash << 6) + (hash >> 2);
        }
        hash ^= values.size() + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                (hash << 6) + (hash >> 2);
        return hash;
    };
    for (const DispatchCandidate& candidate : candidates) {
        if (node.edges[candidate.edge].is_default) continue;
        StrategyDispatchSignature signature;
        signature.edge = candidate.edge;
        for (const DispatchClause& clause : candidate.clauses) {
            if (clause.expected) {
                signature.true_conditions.push_back(clause.condition);
            }
        }
        std::sort(
            signature.true_conditions.begin(),
            signature.true_conditions.end());
        auto& collisions =
            node.dispatch_signatures[signature_hash(
                signature.true_conditions)];
        const bool duplicate = std::any_of(
            collisions.begin(), collisions.end(),
            [&](const StrategyDispatchSignature& existing) {
                return existing.true_conditions ==
                       signature.true_conditions;
            });
        if (!duplicate) collisions.push_back(std::move(signature));
    }
    if (candidates.size() > 1024 && !node.dispatch_signatures.empty()) {
        return;
    }

    constexpr std::size_t kMaxDispatchNodes = 200000;
    const auto build = [&](const auto& self,
                           const std::vector<DispatchCandidate>& active)
        -> std::uint32_t {
        if (active.empty()) {
            throw std::length_error("empty strategy dispatch branch");
        }
        if (node.dispatch_nodes.size() >= kMaxDispatchNodes) {
            throw std::length_error("strategy dispatch DAG exceeded cap");
        }
        const std::uint32_t index = static_cast<std::uint32_t>(
            node.dispatch_nodes.size());
        node.dispatch_nodes.emplace_back();
        if (active.front().clauses.empty()) {
            node.dispatch_nodes[index].edge = active.front().edge;
            return index;
        }

        struct Counts {
            std::uint32_t when_true = 0;
            std::uint32_t when_false = 0;
        };
        std::unordered_map<std::uint32_t, Counts> counts;
        for (const DispatchCandidate& candidate : active) {
            for (const DispatchClause& clause : candidate.clauses) {
                Counts& value = counts[clause.condition];
                if (clause.expected) ++value.when_true;
                else ++value.when_false;
            }
        }
        std::uint32_t selected = active.front().clauses.front().condition;
        std::size_t selected_worst = active.size();
        std::size_t selected_constrained = 0;
        for (const auto& [condition, value] : counts) {
            const std::size_t constrained =
                value.when_true + value.when_false;
            const std::size_t wild = active.size() - constrained;
            const std::size_t worst = wild + std::max(
                value.when_true, value.when_false);
            if (worst < selected_worst ||
                (worst == selected_worst &&
                 constrained > selected_constrained)) {
                selected = condition;
                selected_worst = worst;
                selected_constrained = constrained;
            }
        }

        const auto branch = [&](const bool value) {
            std::vector<DispatchCandidate> next;
            next.reserve(active.size());
            for (const DispatchCandidate& candidate : active) {
                DispatchCandidate copy = candidate;
                const auto clause = std::find_if(
                    copy.clauses.begin(), copy.clauses.end(),
                    [&](const DispatchClause& entry) {
                        return entry.condition == selected;
                    });
                if (clause != copy.clauses.end()) {
                    if (clause->expected != value) continue;
                    copy.clauses.erase(clause);
                }
                next.push_back(std::move(copy));
            }
            return next;
        };
        const std::vector<DispatchCandidate> when_true = branch(true);
        const std::vector<DispatchCandidate> when_false = branch(false);
        const std::uint32_t true_index = self(self, when_true);
        const std::uint32_t false_index = self(self, when_false);
        node.dispatch_nodes[index].condition = selected;
        node.dispatch_nodes[index].when_true = true_index;
        node.dispatch_nodes[index].when_false = false_index;
        return index;
    };
    try {
        (void)build(build, candidates);
    } catch (const std::length_error&) {
        node.dispatch_nodes.clear();
    }
}

bool action_type_from_name(const std::string& name, ActionType& out) {
    static const std::pair<const char*, ActionType> table[] = {
        {"transmute", ActionType::Transmute},
        {"augment", ActionType::Augment},
        {"alteration", ActionType::Alteration},
        {"regal", ActionType::Regal},
        {"alchemy", ActionType::Alchemy},
        {"chaos", ActionType::Chaos},
        {"exalt", ActionType::Exalt},
        {"annul", ActionType::Annul},
        {"scour", ActionType::Scour},
        {"essence", ActionType::Essence},
        {"fossil", ActionType::Fossil},
        {"bench", ActionType::Bench},
        {"veiled_chaos", ActionType::VeiledChaos},
        {"veiled_exalt", ActionType::VeiledExalt},
        {"unveil", ActionType::Unveil},
        {"harvest_reforge", ActionType::HarvestReforge},
        {"harvest_augment", ActionType::HarvestAugment},
        {"harvest_resist", ActionType::HarvestResist},
        {"eldritch_ember", ActionType::EldritchEmber},
        {"eldritch_ichor", ActionType::EldritchIchor},
        {"eldritch_exalt", ActionType::EldritchExalt},
        {"eldritch_chaos", ActionType::EldritchChaos},
        {"eldritch_annul", ActionType::EldritchAnnul},
        {"influence_exalt", ActionType::InfluenceExalt},
        {"fracture", ActionType::Fracture},
        {"remove_crafted_modifiers", ActionType::RemoveCraftedModifiers},
    };
    for (const auto& entry : table) {
        if (name == entry.first) {
            out = entry.second;
            return true;
        }
    }
    return false;
}

void compile_operation(
    const SessionImpl& session,
    const Value& operation,
    StrategyNode& node) {
    const std::string type = string_member(operation, "type");
    if (type == "condition_check_only") {
        node.kind = StrategyNodeKind::Router;
        node.action_type = -1;
        return;
    }
    if (type == "restart") {
        /* Buy a fresh base and keep crafting: no engine action runs; the
         * run loop resets the item in place. */
        node.action_type = kStrategyRestartOperation;
        node.price_keys = {"base"};
        return;
    }
    const DataImpl& data = *session.data;
    const auto bestiary = data.bestiary_action_by_id.find(type);
    if (bestiary != data.bestiary_action_by_id.end()) {
        node.bestiary_action_index = bestiary->second;
        const BestiaryActionDescriptor& descriptor =
            data.bestiary_actions.at(bestiary->second);
        node.action_type =
            descriptor.operation == BestiaryOperationKind::Create
                ? kStrategyBestiaryImprintOperation
                : kStrategyBestiaryRestoreImprintOperation;
        node.price_keys = descriptor.cost_keys;
        return;
    }
    ActionType action_type;
    if (!action_type_from_name(type, action_type)) {
        invalid("unknown operation type: " + type);
    }
    node.action.type = action_type;
    node.action_type = static_cast<int>(action_type);
    node.price_keys = {type};

    const Value* params = operation.find("params");
    const Value& source =
        params != nullptr && params->type == Type::Object ? *params : operation;
    if (action_type == ActionType::Essence) {
        std::string key = string_member(source, "essence_key");
        if (key.empty()) key = string_member(source, "essence");
        if (key.empty()) invalid("essence operation requires essence_key");
        const auto it = data.essence_by_key.find(key);
        if (it == data.essence_by_key.end()) {
            invalid("unknown essence key: " + key);
        }
        node.action.essence_index = it->second;
        node.price_keys = {"essence:" + key};
    } else if (action_type == ActionType::Fossil) {
        const Value* fossils = source.find("fossils");
        if (fossils == nullptr || fossils->type != Type::Array ||
            fossils->array.empty() ||
            fossils->array.size() > PC_MAX_FOSSILS_PER_ACTION) {
            invalid("fossil operation requires 1-4 fossils");
        }
        std::vector<std::string> keys;
        for (const auto& entry : fossils->array) {
            if (entry.type != Type::String) {
                invalid("fossil keys must be strings");
            }
            const auto it = data.fossil_by_key.find(entry.string);
            if (it == data.fossil_by_key.end()) {
                invalid("unknown fossil key: " + entry.string);
            }
            node.action.fossil_indices.push_back(it->second);
            keys.push_back(entry.string);
        }
        std::sort(node.action.fossil_indices.begin(),
                  node.action.fossil_indices.end());
        node.action.fossil_indices.erase(
            std::unique(node.action.fossil_indices.begin(),
                        node.action.fossil_indices.end()),
            node.action.fossil_indices.end());
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        node.price_keys.clear();
        for (const auto& key : keys) {
            node.price_keys.push_back("fossil:" + key);
        }
        node.price_keys.push_back(
            "resonator:" + std::to_string(keys.size()));
    } else if (action_type == ActionType::Bench ||
               action_type == ActionType::Unveil) {
        const std::string key = string_member(source, "mod_key");
        if (key.empty()) invalid(type + " operation requires mod_key");
        const auto pos = data.mod_pos_by_key.find(key);
        if (pos == data.mod_pos_by_key.end())
            invalid("unknown mod key: " + key);
        const auto session_mod = session.session_id_by_global_id.find(
            data.mod_global_ids[pos->second]);
        if (session_mod == session.session_id_by_global_id.end())
            invalid("mod is unavailable in this session: " + key);
        node.action.mod_id = session_mod->second;
        node.price_keys = action_type == ActionType::Bench
                              ? std::vector<std::string>{"bench:" + key}
                              : std::vector<std::string>{};
    } else if (action_type == ActionType::HarvestReforge ||
               action_type == ActionType::HarvestAugment) {
        std::string tag = string_member(source, "target_tag");
        if (tag.empty()) tag = string_member(source, "tag");
        const auto it = data.tag_id_by_name.find(tag);
        if (it == data.tag_id_by_name.end())
            invalid("unknown Harvest tag: " + tag);
        const bool real_craft =
            action_type == ActionType::HarvestReforge
                ? harvest_reforge_tag_allowed(tag)
                : harvest_augment_tag_allowed(tag);
        if (!real_craft)
            invalid("unavailable Harvest craft tag: " + tag);
        node.action.target_tag_id = it->second;
        node.price_keys = {type + ":" + tag};
    } else if (action_type == ActionType::HarvestResist) {
        const std::string source_tag = string_member(source, "source_tag");
        const std::string target_tag = string_member(source, "target_tag");
        const auto from = data.tag_id_by_name.find(source_tag);
        const auto to = data.tag_id_by_name.find(target_tag);
        if (from == data.tag_id_by_name.end() ||
            to == data.tag_id_by_name.end())
            invalid("unknown Harvest resistance tag");
        node.action.source_tag_id = from->second;
        node.action.target_tag_id = to->second;
        node.price_keys = {"harvest_resist:" + target_tag};
    } else if (action_type == ActionType::EldritchEmber ||
               action_type == ActionType::EldritchIchor) {
        node.action.tier =
            static_cast<std::uint32_t>(int_member(source, "tier", 0));
        if (node.action.tier < 1 || node.action.tier > 4)
            invalid("Eldritch tier must be 1-4");
        node.price_keys = {type + ":" + std::to_string(node.action.tier)};
    } else if (action_type == ActionType::InfluenceExalt) {
        const std::string influence = string_member(source, "influence");
        const auto it = data.influence_exalt_code_by_name.find(influence);
        if (it == data.influence_exalt_code_by_name.end() ||
            it->second <= 0) {
            invalid("unknown Influence Exalt currency: " + influence);
        }
        node.action.influence_code = it->second;
        node.price_keys = {
            "influence_exalt:" +
            data.influence_exalt_name_by_code.at(
                static_cast<std::size_t>(it->second))};
    } else if (action_type == ActionType::RemoveCraftedModifiers) {
        node.price_keys = {"scour"};
    }
}

bool has_group(
    const SessionImpl& session,
    const pc_item_state& item,
    std::uint32_t group_id,
    int min_tier,
    std::uint8_t required_flags) {
    auto side_has = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t mod_id = slots[i].mod_id;
            if (mod_id >= session.mod_count) continue;
            if ((slots[i].flags & required_flags) != required_flags) continue;
            const std::uint32_t tier = session.family_tier_index[mod_id];
            if (min_tier > 0 &&
                (tier == 0 || tier > static_cast<std::uint32_t>(min_tier))) {
                continue;
            }
            for (std::uint32_t p = session.group_offsets[mod_id];
                 p < session.group_offsets[mod_id + 1]; ++p) {
                if (session.group_ids[p] == group_id) return true;
            }
        }
        return false;
    };
    return side_has(item.prefixes, item.prefix_count) ||
           side_has(item.suffixes, item.suffix_count);
}

int count_mods(
    const pc_item_state& item,
    const std::vector<std::uint32_t>& mod_ids,
    std::uint8_t required_flags) {
    int count = 0;
    const auto visit = [&](const pc_mod_slot* slots, std::uint8_t size) {
        for (std::uint8_t i = 0; i < size; ++i) {
            if (std::binary_search(
                    mod_ids.begin(), mod_ids.end(), slots[i].mod_id) &&
                (slots[i].flags & required_flags) == required_flags) {
                ++count;
            }
        }
    };
    visit(item.prefixes, item.prefix_count);
    visit(item.suffixes, item.suffix_count);
    return count;
}

int count_mod_families(
    const SessionImpl& session,
    const pc_item_state& item,
    const std::vector<std::uint32_t>& family_ids,
    std::uint8_t required_flags) {
    int count = 0;
    const auto visit = [&](const pc_mod_slot* slots, std::uint8_t size) {
        for (std::uint8_t i = 0; i < size; ++i) {
            const std::uint32_t mod_id = slots[i].mod_id;
            if (mod_id < session.family_id.size() &&
                std::binary_search(
                    family_ids.begin(), family_ids.end(),
                    session.family_id[mod_id]) &&
                (slots[i].flags & required_flags) == required_flags) {
                ++count;
            }
        }
    };
    visit(item.prefixes, item.prefix_count);
    visit(item.suffixes, item.suffix_count);
    return count;
}

int count_condition_value(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item,
    SimulatorImpl* simulator) {
    if (simulator != nullptr &&
        condition.count_memo_slot < simulator->count_cache_generation.size() &&
        simulator->count_cache_generation[condition.count_memo_slot] ==
            simulator->current_condition_generation) {
        return simulator->count_cache_values[condition.count_memo_slot];
    }
    const int count = condition.kind == ConditionKind::ModFamilyCount
                          ? count_mod_families(
                                session, item, condition.family_ids,
                                condition.required_flags)
                          : count_mods(
                                item, condition.mod_ids,
                                condition.required_flags);
    if (simulator != nullptr &&
        condition.count_memo_slot < simulator->count_cache_generation.size()) {
        simulator->count_cache_generation[condition.count_memo_slot] =
            simulator->current_condition_generation;
        simulator->count_cache_values[condition.count_memo_slot] =
            static_cast<std::int8_t>(count);
    }
    return count;
}

bool item_has_slot_flag(
    const pc_item_state& item,
    std::uint8_t flag,
    int side = -1) {
    const auto visit = [&](const pc_mod_slot* slots, std::uint8_t size) {
        for (std::uint8_t i = 0; i < size; ++i) {
            if (slots[i].flags & flag) return true;
        }
        return false;
    };
    return (side != PC_SIDE_SUFFIX &&
            visit(item.prefixes, item.prefix_count)) ||
           (side != PC_SIDE_PREFIX &&
            visit(item.suffixes, item.suffix_count));
}

bool item_has_metamod_code(
    const SessionImpl& session,
    const pc_item_state& item,
    int code) {
    if (code < 0) return false;
    const auto visit = [&](const pc_mod_slot* slots, std::uint8_t size) {
        for (std::uint8_t i = 0; i < size; ++i) {
            const std::uint32_t mod = slots[i].mod_id;
            if (mod < session.metamod_type.size() &&
                session.metamod_type[mod] == code) {
                return true;
            }
        }
        return false;
    };
    return visit(item.prefixes, item.prefix_count) ||
           visit(item.suffixes, item.suffix_count);
}

bool has_item_flag(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item) {
    const DataImpl& data = *session.data;
    switch (condition.item_flag) {
    case ItemFlagKind::Corrupted:
        return (item.item_flags & PC_ITEM_CORRUPTED) != 0;
    case ItemFlagKind::Mirrored:
        return (item.item_flags & PC_ITEM_MIRRORED) != 0;
    case ItemFlagKind::Split:
        return (item.item_flags & PC_ITEM_SPLIT) != 0;
    case ItemFlagKind::Synthesised:
        return (item.item_flags & PC_ITEM_SYNTHESISED) != 0;
    case ItemFlagKind::Fractured:
        return item_has_slot_flag(item, PC_MOD_SLOT_FRACTURED);
    case ItemFlagKind::Crafted:
        return item_has_slot_flag(item, PC_MOD_SLOT_CRAFTED);
    case ItemFlagKind::Veiled:
        return item_has_slot_flag(item, PC_MOD_SLOT_VEILED);
    case ItemFlagKind::VeiledPrefix:
        return item_has_slot_flag(
            item, PC_MOD_SLOT_VEILED, PC_SIDE_PREFIX);
    case ItemFlagKind::VeiledSuffix:
        return item_has_slot_flag(
            item, PC_MOD_SLOT_VEILED, PC_SIDE_SUFFIX);
    case ItemFlagKind::Multimod:
        return item_has_metamod_code(
            session, item, data.metamod_multimod_code);
    case ItemFlagKind::NoAttack:
        return item_has_metamod_code(
            session, item, data.metamod_no_attack_code);
    case ItemFlagKind::NoCaster:
        return item_has_metamod_code(
            session, item, data.metamod_no_caster_code);
    case ItemFlagKind::PrefixesLocked:
        return item_has_metamod_code(
            session, item, data.metamod_prefixes_locked_code);
    case ItemFlagKind::SuffixesLocked:
        return item_has_metamod_code(
            session, item, data.metamod_suffixes_locked_code);
    case ItemFlagKind::Influenced:
        return item.generic_influence_bits != 0;
    case ItemFlagKind::EldritchImplicit:
        return item.searing_exarch_tier != 0 ||
               item.eater_of_worlds_tier != 0;
    }
    return false;
}

bool has_unveil_option(
    const pc_item_state& item,
    std::uint32_t mod_id) {
    const auto visit = [&](const pc_mod_slot* slots, std::uint8_t size) {
        for (std::uint8_t i = 0; i < size; ++i) {
            if ((slots[i].flags & PC_MOD_SLOT_VEILED) == 0) continue;
            for (std::uint8_t option = 0;
                 option < slots[i].veiled_option_count; ++option) {
                if (slots[i].veiled_option_mod_ids[option] == mod_id) {
                    return true;
                }
            }
        }
        return false;
    };
    return visit(item.prefixes, item.prefix_count) ||
           visit(item.suffixes, item.suffix_count);
}

bool has_family(
    const SessionImpl& session,
    const pc_item_state& item,
    std::uint32_t family_id,
    int min_tier,
    std::uint8_t required_flags) {
    auto side_has = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t mod_id = slots[i].mod_id;
            if (mod_id >= session.mod_count ||
                session.family_id[mod_id] != family_id) {
                continue;
            }
            if ((slots[i].flags & required_flags) != required_flags) continue;
            const std::uint32_t tier = session.family_tier_index[mod_id];
            if (min_tier == 0 ||
                (tier != 0 && tier <= static_cast<std::uint32_t>(min_tier))) {
                return true;
            }
        }
        return false;
    };
    return side_has(item.prefixes, item.prefix_count) ||
           side_has(item.suffixes, item.suffix_count);
}

bool evaluate_condition_cached(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item,
    SimulatorImpl* simulator);

bool evaluate_condition_impl(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item,
    SimulatorImpl* simulator) {
    switch (condition.kind) {
    case ConditionKind::Always:
        return true;
    case ConditionKind::HasModGroup:
        return has_group(
            session, item, condition.group_id, condition.min_value,
            condition.required_flags);
    case ConditionKind::HasModFamily:
        return has_family(
            session,
            item,
            condition.family_id,
            condition.min_value,
            condition.required_flags);
    case ConditionKind::RarityIs:
        return item.rarity == condition.min_value;
    case ConditionKind::OpenPrefixCount: {
        const int open =
            std::max(0, static_cast<int>(side_cap(session, item)) -
                            static_cast<int>(item.prefix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::OpenSuffixCount: {
        const int open =
            std::max(0, static_cast<int>(side_cap(session, item)) -
                            static_cast<int>(item.suffix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::PrefixCountRange:
        return item.prefix_count >= condition.min_value &&
               item.prefix_count <= condition.max_value;
    case ConditionKind::SuffixCountRange:
        return item.suffix_count >= condition.min_value &&
               item.suffix_count <= condition.max_value;
    case ConditionKind::ModCount: {
        const int count = count_condition_value(
            condition, session, item, simulator);
        return count >= condition.min_value && count <= condition.max_value;
    }
    case ConditionKind::ModFamilyCount: {
        const int count = count_condition_value(
            condition, session, item, simulator);
        return count >= condition.min_value && count <= condition.max_value;
    }
    case ConditionKind::ItemFlag:
        return has_item_flag(condition, session, item);
    case ConditionKind::InfluenceBits:
        return item.generic_influence_bits == condition.min_value;
    case ConditionKind::EldritchTier: {
        const int tier = condition.eldritch_side == 0
                             ? item.searing_exarch_tier
                             : item.eater_of_worlds_tier;
        return tier >= condition.min_value && tier <= condition.max_value;
    }
    case ConditionKind::HasUnveilOption:
        return !condition.mod_ids.empty() &&
               has_unveil_option(item, condition.mod_ids.front());
    case ConditionKind::ObservationSignature:
        if (condition.observation_program == nullptr) {
            throw std::logic_error(
                "observation-signature condition has no program");
        }
        return solver::refinement::observe_exact_item_features(
                   session,
                   item,
                   condition.observation_program->requirement,
                   condition.observation_program->context) ==
               condition.observation_program->signature;
    case ConditionKind::All:
        return std::all_of(
            condition.children.begin(),
            condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_condition_cached(
                    child, session, item, simulator);
            });
    case ConditionKind::Any:
        return std::any_of(
            condition.children.begin(),
            condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_condition_cached(
                    child, session, item, simulator);
            });
    case ConditionKind::Not:
        return !evaluate_condition_cached(
            condition.children.front(), session, item, simulator);
    case ConditionKind::AtLeast: {
        int matches = 0;
        for (const auto& child : condition.children) {
            if (evaluate_condition_cached(child, session, item, simulator)) {
                ++matches;
            }
        }
        return matches >= condition.min_value;
    }
    }
    return false;
}

bool evaluate_condition_cached(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item,
    SimulatorImpl* simulator) {
    const std::uint32_t no_slot =
        std::numeric_limits<std::uint32_t>::max();
    if (simulator != nullptr && condition.memo_slot != no_slot &&
        condition.memo_slot < simulator->condition_cache_generation.size() &&
        simulator->condition_cache_generation[condition.memo_slot] ==
            simulator->current_condition_generation) {
        return simulator->condition_cache_values[condition.memo_slot] != 0;
    }
    const bool value =
        evaluate_condition_impl(condition, session, item, simulator);
    if (simulator != nullptr && condition.memo_slot != no_slot &&
        condition.memo_slot < simulator->condition_cache_generation.size()) {
        simulator->condition_cache_generation[condition.memo_slot] =
            simulator->current_condition_generation;
        simulator->condition_cache_values[condition.memo_slot] = value ? 1 : 0;
    }
    return value;
}

bool evaluate_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item) {
    return evaluate_condition_cached(condition, session, item, nullptr);
}

std::int16_t direct_dispatch_feature_value(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item,
    SimulatorImpl& simulator) {
    switch (condition.kind) {
    case ConditionKind::Always:
        return 1;
    case ConditionKind::HasModGroup:
    case ConditionKind::HasModFamily:
    case ConditionKind::ItemFlag:
    case ConditionKind::HasUnveilOption:
    case ConditionKind::ObservationSignature:
        return evaluate_condition_cached(
                   condition, session, item, &simulator)
                   ? 1
                   : 0;
    case ConditionKind::RarityIs:
        return item.rarity;
    case ConditionKind::OpenPrefixCount:
        return static_cast<std::int16_t>(std::max(
            0, static_cast<int>(side_cap(session, item)) -
                   static_cast<int>(item.prefix_count)));
    case ConditionKind::OpenSuffixCount:
        return static_cast<std::int16_t>(std::max(
            0, static_cast<int>(side_cap(session, item)) -
                   static_cast<int>(item.suffix_count)));
    case ConditionKind::PrefixCountRange:
        return item.prefix_count;
    case ConditionKind::SuffixCountRange:
        return item.suffix_count;
    case ConditionKind::ModCount:
    case ConditionKind::ModFamilyCount:
        return static_cast<std::int16_t>(count_condition_value(
            condition, session, item, &simulator));
    case ConditionKind::InfluenceBits:
        return item.generic_influence_bits;
    case ConditionKind::EldritchTier:
        return condition.eldritch_side == 0
                   ? item.searing_exarch_tier
                   : item.eater_of_worlds_tier;
    case ConditionKind::All:
    case ConditionKind::Any:
    case ConditionKind::Not:
    case ConditionKind::AtLeast:
        return 0;
    }
    return 0;
}

const StrategyEdge* select_edge(
    const StrategyNode& node,
    const SessionImpl& session,
    const pc_item_state& item,
    SimulatorImpl& simulator) {
    if (node.edges.size() == 1 && node.edges.front().is_default) {
        return &node.edges.front();
    }
    if (++simulator.current_condition_generation == 0) {
        std::fill(
            simulator.condition_cache_generation.begin(),
            simulator.condition_cache_generation.end(), 0);
        std::fill(
            simulator.count_cache_generation.begin(),
            simulator.count_cache_generation.end(), 0);
        simulator.current_condition_generation = 1;
    }
    if (!node.direct_dispatch_signatures.empty()) {
        std::vector<std::int16_t>& values =
            simulator.direct_dispatch_values_scratch;
        values.clear();
        values.reserve(node.direct_dispatch_features.size());
        for (const CompiledCondition& feature :
             node.direct_dispatch_features) {
            values.push_back(direct_dispatch_feature_value(
                feature, session, item, simulator));
        }
        const auto found = node.direct_dispatch_signatures.find(
            direct_signature_hash(values));
        if (found != node.direct_dispatch_signatures.end()) {
            for (const StrategyDirectDispatchSignature& signature :
                 found->second) {
                if (signature.values == values &&
                    signature.edge < node.edges.size()) {
                    return &node.edges[signature.edge];
                }
            }
        }
        /* A miss falls through to the verified condition dispatcher. */
    }
    if (!node.dispatch_signatures.empty()) {
        std::vector<std::uint32_t>& true_conditions =
            simulator.true_conditions_scratch;
        true_conditions.clear();
        true_conditions.reserve(node.dispatch_conditions.size());
        std::size_t hash =
            static_cast<std::size_t>(1469598103934665603ULL);
        for (std::uint32_t condition = 0;
             condition < node.dispatch_conditions.size(); ++condition) {
            if (!evaluate_condition_cached(
                    node.dispatch_conditions[condition], session, item,
                    &simulator)) {
                continue;
            }
            true_conditions.push_back(condition);
            hash ^= static_cast<std::size_t>(condition) +
                    static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                    (hash << 6) + (hash >> 2);
        }
        hash ^= true_conditions.size() +
                static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                (hash << 6) + (hash >> 2);
        const auto found = node.dispatch_signatures.find(hash);
        if (found != node.dispatch_signatures.end()) {
            for (const StrategyDispatchSignature& signature :
                 found->second) {
                if (signature.true_conditions == true_conditions &&
                    signature.edge < node.edges.size()) {
                    return &node.edges[signature.edge];
                }
            }
        }
        /* A signature miss is legal for hand-authored wildcard conditions.
         * Fall through to the exact priority-preserving dispatch below. */
    }
    if (!node.dispatch_nodes.empty()) {
        std::uint32_t dispatch = 0;
        while (dispatch < node.dispatch_nodes.size()) {
            const StrategyDispatchNode& decision =
                node.dispatch_nodes[dispatch];
            if (decision.edge !=
                std::numeric_limits<std::uint32_t>::max()) {
                return decision.edge < node.edges.size()
                           ? &node.edges[decision.edge]
                           : nullptr;
            }
            if (decision.condition >= node.dispatch_conditions.size()) {
                return nullptr;
            }
            const bool value = evaluate_condition_cached(
                node.dispatch_conditions[decision.condition], session, item,
                &simulator);
            dispatch = value ? decision.when_true : decision.when_false;
        }
        return nullptr;
    }
    const StrategyEdge* fallback = nullptr;
    for (const auto& edge : node.edges) {
        if (edge.is_default) {
            fallback = &edge;
        } else if (evaluate_condition_cached(
                       edge.condition, session, item, &simulator)) {
            return &edge;
        }
    }
    return fallback;
}

bool options_equal(
    const SimulationOptionsInternal& a,
    const SimulationOptionsInternal& b) {
    return a.target_runs == b.target_runs && a.seed == b.seed &&
           a.max_actions_per_run == b.max_actions_per_run &&
           a.max_graph_steps_per_run == b.max_graph_steps_per_run &&
           a.max_cost_per_run == b.max_cost_per_run &&
           a.retained_trace_count == b.retained_trace_count &&
           a.max_trace_entries == b.max_trace_entries &&
           a.retained_success_count == b.retained_success_count &&
           a.retained_failure_count == b.retained_failure_count;
}

void append_trace(
    RetainedTrace* trace,
    const SimulationOptionsInternal& options,
    TraceEntryInternal entry,
    bool final_entry = false) {
    if (trace == nullptr || options.max_trace_entries == 0) return;
    if (trace->entries.size() < options.max_trace_entries) {
        trace->entries.push_back(std::move(entry));
    } else if (final_entry) {
        trace->entries.back() = std::move(entry);
    }
}

const char* failure_default_detail(int reason) {
    switch (reason) {
    case PC_SIM_FAILURE_TERMINAL: return "failure terminal";
    case PC_SIM_FAILURE_ACTION_LIMIT: return "action limit reached";
    case PC_SIM_FAILURE_COST_LIMIT: return "cost limit reached";
    case PC_SIM_FAILURE_STEP_LIMIT: return "graph step limit reached";
    case PC_SIM_FAILURE_NO_MATCHING_EDGE: return "no matching or default edge";
    case PC_SIM_FAILURE_ACTION_NOT_APPLIED: return "action was not applicable";
    case PC_SIM_FAILURE_MISSING_PRICE: return "cost limit requires a missing price";
    default: return "";
    }
}

bool action_needs_rollback(ActionType type) {
    // These actions either reject before mutating the item, or report applied
    // after their first mutation. The reforges also always report applied once
    // entered; an empty pool simply yields fewer mods. Avoid copying the full
    // item state for their overwhelmingly common success path. Keep the
    // conservative rollback for compound actions that can mutate before a
    // later refusal (including Scour's rarity normalization).
    switch (type) {
    case ActionType::Transmute:
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Exalt:
    case ActionType::Annul:
    case ActionType::Fracture:
        return false;
    default:
        return true;
    }
}

void aggregate_failure(
    SimulatorImpl& simulator,
    int reason,
    const std::string& node_id,
    const std::string& detail) {
    for (auto& entry : simulator.failure_summaries) {
        if (entry.failure_reason == reason && entry.node_id == node_id &&
            entry.detail == detail) {
            ++entry.count;
            return;
        }
    }
    simulator.failure_summaries.push_back(
        {reason, node_id, detail, 1});
}

struct RunResult {
    int terminal_kind = PC_TERMINAL_FAILURE;
    int failure_reason = PC_SIM_FAILURE_NONE;
    std::string terminal_node_id;
    std::string detail;
    std::uint64_t actions = 0;
    double known_cost = 0.0;
    bool cost_complete = true;
    pc_item_state item{};
};

RunResult run_one(SimulatorImpl& simulator, RetainedTrace* trace) {
    const StrategyImpl& strategy = *simulator.strategy;
    const SessionImpl& session = *simulator.session;
    const SimulationOptionsInternal& options = simulator.options;
    const std::uint64_t derived_steps =
        static_cast<std::uint64_t>(options.max_actions_per_run) * 4u +
        static_cast<std::uint64_t>(strategy.nodes.size()) * 4u + 16u;
    const std::uint64_t max_steps =
        options.max_graph_steps_per_run != 0
            ? options.max_graph_steps_per_run
            : std::min<std::uint64_t>(
                  derived_steps, std::numeric_limits<std::uint32_t>::max());

    RunResult result;
    result.item = strategy.start_item;
    (void)ensure_unveil_options(*simulator.context, &result.item);
    BestiaryCraftState bestiary_state;
    bestiary_state.item = result.item;
    bestiary_state.live_item_identity = 1;
    std::uint32_t node_index = strategy.start_node;
    std::uint64_t graph_steps = 0;
    bool run_missing_price = false;

    auto finish_failure = [&](int reason,
                              const StrategyNode& node,
                              const std::string& detail,
                              int terminal_kind = PC_TERMINAL_FAILURE) {
        result.terminal_kind = terminal_kind;
        result.failure_reason = reason;
        result.terminal_node_id = node.id;
        result.detail = detail.empty() ? failure_default_detail(reason) : detail;
        if (trace != nullptr && options.max_trace_entries != 0) {
            TraceEntryInternal entry;
            entry.step_index = static_cast<std::uint32_t>(graph_steps);
            entry.node_id = node.id;
            entry.node_kind = static_cast<int>(node.kind);
            entry.action_type = node.action_type;
            entry.cumulative_actions = result.actions;
            entry.known_cumulative_cost = result.known_cost;
            entry.cost_complete = result.cost_complete;
            entry.terminal_kind = terminal_kind;
            entry.failure_reason = reason;
            entry.item = result.item;
            append_trace(trace, options, std::move(entry), true);
        }
    };

    while (true) {
        const StrategyNode& node = strategy.nodes[node_index];
        ++graph_steps;
        if (graph_steps > max_steps) {
            finish_failure(PC_SIM_FAILURE_STEP_LIMIT, node, "");
            break;
        }

        if (node.kind == StrategyNodeKind::Terminal) {
            result.terminal_kind = node.terminal_kind;
            result.terminal_node_id = node.id;
            if (node.terminal_kind == PC_TERMINAL_FAILURE) {
                result.failure_reason = PC_SIM_FAILURE_TERMINAL;
                result.detail =
                    node.reason.empty() ? "failure terminal" : node.reason;
            } else if (node.terminal_kind == PC_TERMINAL_STOP) {
                result.failure_reason = PC_SIM_FAILURE_TERMINAL;
                result.detail = node.reason.empty() ? "stop terminal" : node.reason;
            }
            if (trace != nullptr && options.max_trace_entries != 0) {
                TraceEntryInternal entry;
                entry.step_index =
                    static_cast<std::uint32_t>(graph_steps - 1);
                entry.node_id = node.id;
                entry.node_kind = static_cast<int>(node.kind);
                entry.cumulative_actions = result.actions;
                entry.known_cumulative_cost = result.known_cost;
                entry.cost_complete = result.cost_complete;
                entry.terminal_kind = node.terminal_kind;
                entry.failure_reason = result.failure_reason;
                entry.item = result.item;
                append_trace(trace, options, std::move(entry), true);
            }
            break;
        }

        bool applied = false;
        if (node.kind == StrategyNodeKind::Operation) {
            if (result.actions >= options.max_actions_per_run) {
                finish_failure(PC_SIM_FAILURE_ACTION_LIMIT, node, "");
                break;
            }

            const bool price_known =
                simulator.node_prices_known[node_index] != 0;
            const double price = simulator.node_prices[node_index];
            const std::vector<std::string>& missing_keys =
                simulator.node_missing_price_keys[node_index];

            if (!price_known && options.max_cost_per_run > 0.0) {
                for (const auto& price_key : missing_keys) {
                    ++simulator.missing_prices[price_key];
                }
                result.cost_complete = false;
                run_missing_price = true;
                ++simulator.summary.missing_price_action_count;
                finish_failure(PC_SIM_FAILURE_MISSING_PRICE, node, "");
                break;
            }
            if (price_known && simulator.economy != nullptr &&
                options.max_cost_per_run > 0.0 &&
                result.known_cost + price > options.max_cost_per_run) {
                finish_failure(PC_SIM_FAILURE_COST_LIMIT, node, "");
                break;
            }

            ActionOutcome outcome;
            if (node.action_type == kStrategyRestartOperation) {
                const int removed =
                    result.item.prefix_count + result.item.suffix_count;
                pc_item_clear(&result.item);
                if (session.base_implicit_mod_ids.size() <=
                    PC_MAX_IMPLICITS) {
                    for (std::uint32_t mod_id :
                         session.base_implicit_mod_ids) {
                        pc_mod_slot& slot =
                            result.item.implicits[
                                result.item.implicit_count++];
                        slot.mod_id = mod_id;
                        slot.group_id = static_cast<std::uint16_t>(
                            session.primary_group[mod_id]);
                    }
                }
                outcome.applied = true;
                outcome.removed = removed;
                bestiary_state.item = result.item;
                ++bestiary_state.live_item_identity;
                bestiary_state.checkpoint_active = false;
                bestiary_state.checkpoint_bound_identity = 0;
                bestiary_state.checkpoint = {};
            } else if (
                node.action_type == kStrategyBestiaryImprintOperation ||
                node.action_type ==
                    kStrategyBestiaryRestoreImprintOperation) {
                bestiary_state.item = result.item;
                const BestiaryActionOutcome bestiary_outcome =
                    apply_bestiary_action(
                        *session.data, bestiary_state,
                        node.bestiary_action_index);
                result.item = bestiary_state.item;
                outcome.applied = bestiary_outcome.applied;
            } else {
                pc_item_state rollback;
                const bool needs_rollback =
                    action_needs_rollback(node.action.type);
                if (needs_rollback) rollback = result.item;
                outcome = apply_action(*simulator.context, &result.item,
                                       node.action);
                if (!outcome.applied && needs_rollback) {
                    result.item = rollback;
                }
            }
            ++result.actions;
            ++simulator.action_counts[node_index];
            if (!outcome.applied) {
                finish_failure(PC_SIM_FAILURE_ACTION_NOT_APPLIED, node, "");
                break;
            }

            applied = true;
            ++simulator.applied_action_counts[node_index];
            if (price_known && simulator.economy != nullptr) {
                result.known_cost += price;
                ++simulator.summary.costed_action_count;
            } else if (simulator.economy != nullptr) {
                for (const auto& price_key : missing_keys) {
                    ++simulator.missing_prices[price_key];
                }
                result.cost_complete = false;
                run_missing_price = true;
                ++simulator.summary.missing_price_action_count;
            }
        }

        const StrategyEdge* edge =
            select_edge(node, session, result.item, simulator);
        if (trace != nullptr && options.max_trace_entries != 0) {
            TraceEntryInternal entry;
            entry.step_index = static_cast<std::uint32_t>(graph_steps - 1);
            entry.node_id = node.id;
            entry.node_kind = static_cast<int>(node.kind);
            entry.action_type = node.action_type;
            entry.action_applied = applied;
            entry.matched_edge_id = edge != nullptr ? edge->id : "";
            entry.cumulative_actions = result.actions;
            entry.known_cumulative_cost = result.known_cost;
            entry.cost_complete = result.cost_complete;
            entry.item = result.item;
            append_trace(trace, options, std::move(entry));
        }
        if (edge == nullptr) {
            finish_failure(PC_SIM_FAILURE_NO_MATCHING_EDGE, node, "");
            break;
        }
        node_index = edge->target;
    }

    if (run_missing_price) {
        ++simulator.summary.missing_price_run_count;
    }
    return result;
}

} // namespace

bool evaluate_compiled_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const pc_item_state& item) {
    return evaluate_condition(condition, session, item);
}

std::shared_ptr<StrategyImpl> compile_strategy_json(
    std::shared_ptr<const SessionImpl> session,
    const char* strategy_json,
    std::size_t strategy_json_size) {
    if (session == nullptr || strategy_json == nullptr) {
        invalid("null compile input");
    }
    Value root = json::Parser(strategy_json, strategy_json_size).parse();
    if (root.type != Type::Object) invalid("root must be an object");
    if (string_member(root, "version") != "v1") {
        invalid("version must be v1");
    }

    auto strategy = std::make_shared<StrategyImpl>();
    strategy->session = std::move(session);
    strategy->name = string_member(root, "name");
    strategy->start_item = parse_start_item(
        *strategy->session,
        require_object_member(root, "base_state", Type::Object));

    const Value& nodes = require_object_member(root, "nodes", Type::Array);
    if (nodes.array.empty()) invalid("nodes must not be empty");
    if (nodes.array.size() > 10000) invalid("node count exceeds safety limit");
    strategy->nodes.reserve(nodes.array.size());
    for (const auto& node_value : nodes.array) {
        if (node_value.type != Type::Object) invalid("node must be an object");
        StrategyNode node;
        node.id = string_member(node_value, "id");
        if (node.id.empty()) invalid("node requires a non-empty id");
        if (strategy->node_by_id.count(node.id) != 0) {
            invalid("duplicate node id: " + node.id);
        }
        const std::string kind = string_member(node_value, "kind");
        if (kind == "start") {
            node.kind = StrategyNodeKind::Start;
        } else if (kind == "operation") {
            node.kind = StrategyNodeKind::Operation;
            compile_operation(
                *strategy->session,
                require_object_member(
                    node_value, "operation", Type::Object),
                node);
        } else if (kind == "router") {
            node.kind = StrategyNodeKind::Router;
        } else if (kind == "terminal") {
            node.kind = StrategyNodeKind::Terminal;
            const std::string terminal = string_member(node_value, "terminal");
            if (terminal == "success") {
                node.terminal_kind = PC_TERMINAL_SUCCESS;
            } else if (terminal == "failure") {
                node.terminal_kind = PC_TERMINAL_FAILURE;
            } else if (terminal == "stop") {
                node.terminal_kind = PC_TERMINAL_STOP;
            } else {
                invalid("unknown terminal kind: " + terminal);
            }
            node.reason = string_member(node_value, "reason");
        } else {
            invalid("unknown node kind: " + kind);
        }
        node.accounting_roles = accounting_roles_member(node_value);
        strategy->node_by_id.emplace(
            node.id, static_cast<std::uint32_t>(strategy->nodes.size()));
        strategy->nodes.push_back(std::move(node));
    }

    const std::string start_id = string_member(root, "start_node_id");
    const auto start_it = strategy->node_by_id.find(start_id);
    if (start_it == strategy->node_by_id.end() ||
        strategy->nodes[start_it->second].kind != StrategyNodeKind::Start) {
        invalid("start_node_id must reference a start node");
    }
    strategy->start_node = start_it->second;
    std::size_t start_count = 0;
    for (const auto& node : strategy->nodes) {
        if (node.kind == StrategyNodeKind::Start) ++start_count;
    }
    if (start_count != 1) invalid("strategy must contain exactly one start node");

    const Value& edges = require_object_member(root, "edges", Type::Array);
    if (edges.array.size() > 50000) invalid("edge count exceeds safety limit");
    std::unordered_map<std::string, bool> edge_ids;
    std::vector<std::uint32_t> default_counts(strategy->nodes.size(), 0);
    std::uint32_t source_order = 0;
    for (const auto& edge_value : edges.array) {
        if (edge_value.type != Type::Object) invalid("edge must be an object");
        StrategyEdge edge;
        edge.id = string_member(edge_value, "id");
        if (edge.id.empty() || edge_ids.count(edge.id) != 0) {
            invalid("edge ids must be non-empty and unique");
        }
        edge_ids.emplace(edge.id, true);
        const std::string from = string_member(edge_value, "from");
        const std::string to = string_member(edge_value, "to");
        const auto from_it = strategy->node_by_id.find(from);
        const auto to_it = strategy->node_by_id.find(to);
        if (from_it == strategy->node_by_id.end() ||
            to_it == strategy->node_by_id.end()) {
            invalid("edge endpoint does not reference a node");
        }
        if (strategy->nodes[from_it->second].kind ==
            StrategyNodeKind::Terminal) {
            invalid("terminal nodes cannot have outgoing edges");
        }
        edge.target = to_it->second;
        edge.priority = int_member(edge_value, "priority", 0);
        edge.is_default = bool_member(edge_value, "is_default", false);
        edge.source_order = source_order++;
        if (edge.is_default) {
            if (++default_counts[from_it->second] > 1) {
                invalid("a node may have at most one default edge");
            }
            edge.condition.kind = ConditionKind::Always;
        } else {
            const Value* condition = edge_value.find("condition");
            edge.condition =
                condition != nullptr
                    ? compile_condition(*strategy->session, *condition)
                    : CompiledCondition{};
        }
        edge.accounting_roles = accounting_roles_member(edge_value);
        strategy->nodes[from_it->second].edges.push_back(std::move(edge));
    }

    for (auto& node : strategy->nodes) {
        std::stable_sort(
            node.edges.begin(),
            node.edges.end(),
            [](const StrategyEdge& a, const StrategyEdge& b) {
                if (a.priority != b.priority) return a.priority < b.priority;
                return a.source_order < b.source_order;
            });
    }
    std::unordered_map<
        std::size_t, std::vector<const CompiledCondition*>>
        conditions_by_hash;
    std::unordered_map<
        std::size_t, std::vector<const CompiledCondition*>>
        count_conditions_by_hash;
    std::uint32_t next_condition_slot = 0;
    std::uint32_t next_count_slot = 0;
    for (StrategyNode& node : strategy->nodes) {
        for (StrategyEdge& edge : node.edges) {
            if (!edge.is_default) {
                assign_condition_memo_slots(
                    edge.condition, conditions_by_hash,
                    next_condition_slot, count_conditions_by_hash,
                    next_count_slot);
            }
        }
        build_dispatch(node);
    }
    strategy->condition_memo_slots = next_condition_slot;
    strategy->count_memo_slots = next_count_slot;
    return strategy;
}

std::shared_ptr<EconomyImpl> load_economy_json(
    const char* economy_json,
    std::size_t economy_json_size) {
    if (economy_json == nullptr) {
        throw std::runtime_error("economy: null JSON");
    }
    Value root = json::Parser(economy_json, economy_json_size).parse();
    if (root.type != Type::Object) {
        throw std::runtime_error("economy: root must be an object");
    }
    const Value* version = root.find("version");
    if (version != nullptr &&
        (version->type != Type::String || version->string != "v1")) {
        throw std::runtime_error("economy: version must be v1");
    }
    const Value* prices = root.find("prices");
    if (prices == nullptr) prices = &root;
    if (prices->type != Type::Object) {
        throw std::runtime_error("economy: prices must be an object");
    }

    auto economy = std::make_shared<EconomyImpl>();
    const Value* id = root.find("id");
    if (id != nullptr && id->type == Type::String) economy->id = id->string;
    for (const auto& member : prices->object) {
        if (member.first == "version" || member.first == "id" ||
            member.first == "prices") {
            continue;
        }
        if (member.second.type != Type::Number ||
            !std::isfinite(member.second.number) ||
            member.second.number < 0.0) {
            throw std::runtime_error(
                "economy: price must be a finite non-negative number: " +
                member.first);
        }
        economy->prices[member.first] = member.second.number;
    }
    return economy;
}

void prepare_simulator_runtime(SimulatorImpl& simulator) {
    if (simulator.strategy == nullptr) {
        throw std::runtime_error("simulator strategy is not initialized");
    }
    const std::size_t node_count = simulator.strategy->nodes.size();
    if (simulator.node_prices.size() == node_count) return;
    if (simulator.summary.completed_runs != 0) {
        throw std::runtime_error(
            "simulator runtime cache changed after simulation started");
    }

    simulator.action_counts.assign(node_count, 0);
    simulator.accounted_action_counts.assign(node_count, 0);
    simulator.applied_action_counts.assign(node_count, 0);
    simulator.accounted_applied_action_counts.assign(node_count, 0);
    simulator.action_descriptor_ids.assign(node_count, {});
    const solver::ActionRegistry accounting_registry =
        solver::build_action_registry(*simulator.session);
    for (std::size_t node_index = 0; node_index < node_count; ++node_index) {
        const solver::ResolvedStrategyOperation operation =
            solver::resolve_strategy_operation(
                simulator.strategy->nodes[node_index],
                accounting_registry, *simulator.session);
        if (operation.kind ==
                solver::ResolvedStrategyOperationKind::Bestiary) {
            simulator.action_descriptor_ids[node_index] =
                simulator.session->data->bestiary_actions.at(
                    operation.descriptor_index).id;
        } else if (operation.resolved()) {
            simulator.action_descriptor_ids[node_index] =
                accounting_registry.actions[
                    operation.descriptor_index].id;
        }
    }
    simulator.node_prices.assign(node_count, 0.0);
    simulator.node_prices_known.assign(node_count, 1);
    simulator.node_missing_price_keys.assign(node_count, {});
    if (simulator.economy != nullptr) {
        for (std::size_t node_index = 0; node_index < node_count;
             ++node_index) {
            const StrategyNode& node = simulator.strategy->nodes[node_index];
            for (const std::string& key : node.price_keys) {
                const auto price = simulator.economy->prices.find(key);
                if (price == simulator.economy->prices.end()) {
                    simulator.node_prices_known[node_index] = 0;
                    simulator.node_missing_price_keys[node_index].push_back(
                        key);
                } else {
                    simulator.node_prices[node_index] += price->second;
                }
            }
        }
    }
    simulator.condition_cache_generation.assign(
        simulator.strategy->condition_memo_slots, 0);
    simulator.condition_cache_values.assign(
        simulator.strategy->condition_memo_slots, 0);
    simulator.count_cache_generation.assign(
        simulator.strategy->count_memo_slots, 0);
    simulator.count_cache_values.assign(
        simulator.strategy->count_memo_slots, 0);
}

void run_simulator_chunk(
    SimulatorImpl& simulator,
    const SimulationOptionsInternal& options,
    std::uint32_t max_completed_runs) {
    prepare_simulator_runtime(simulator);
    if (!simulator.options_set) {
        simulator.options = options;
        simulator.options_set = true;
        simulator.context = std::make_unique<ActionContextImpl>(options.seed);
        simulator.context->session = simulator.session;
        simulator.context->capture_action_trace = false;
    } else if (!options_equal(simulator.options, options)) {
        throw std::runtime_error(
            "simulation options changed after the first chunk");
    }

    const std::uint64_t remaining =
        simulator.options.target_runs - simulator.summary.completed_runs;
    const std::uint64_t run_count =
        std::min<std::uint64_t>(remaining, max_completed_runs);
    for (std::uint64_t i = 0; i < run_count; ++i) {
        RetainedTrace trace;
        RetainedTrace* trace_ptr =
            simulator.traces.size() < simulator.options.retained_trace_count
                ? &trace
                : nullptr;
        RunResult result = run_one(simulator, trace_ptr);
        if (trace_ptr != nullptr) {
            simulator.traces.push_back(std::move(trace));
        }

        ++simulator.summary.completed_runs;
        simulator.summary.total_actions += result.actions;
        simulator.summary.known_total_cost += result.known_cost;
        if (result.terminal_kind == PC_TERMINAL_SUCCESS) {
            ++simulator.summary.success_count;
        } else if (result.terminal_kind == PC_TERMINAL_STOP) {
            ++simulator.summary.stop_count;
        } else {
            ++simulator.summary.failure_count;
        }
        switch (result.failure_reason) {
        case PC_SIM_FAILURE_ACTION_LIMIT:
            ++simulator.summary.action_limit_count;
            break;
        case PC_SIM_FAILURE_COST_LIMIT:
            ++simulator.summary.cost_limit_count;
            break;
        case PC_SIM_FAILURE_STEP_LIMIT:
            ++simulator.summary.step_limit_count;
            break;
        case PC_SIM_FAILURE_NO_MATCHING_EDGE:
            ++simulator.summary.no_matching_edge_count;
            break;
        case PC_SIM_FAILURE_ACTION_NOT_APPLIED:
            ++simulator.summary.action_not_applied_count;
            break;
        default:
            break;
        }
        if (result.terminal_kind != PC_TERMINAL_SUCCESS) {
            aggregate_failure(
                simulator,
                result.failure_reason,
                result.terminal_node_id,
                result.detail);
        }

        const bool retain_success =
            result.terminal_kind == PC_TERMINAL_SUCCESS &&
            simulator.success_examples.size() <
                simulator.options.retained_success_count;
        const bool retain_failure =
            result.terminal_kind != PC_TERMINAL_SUCCESS &&
            simulator.failure_examples.size() <
                simulator.options.retained_failure_count;
        if (retain_success || retain_failure) {
            SimulationExampleInternal example;
            example.terminal_kind = result.terminal_kind;
            example.failure_reason = result.failure_reason;
            example.terminal_node_id = result.terminal_node_id;
            example.action_count = result.actions;
            example.known_total_cost = result.known_cost;
            example.cost_complete = result.cost_complete;
            example.item = result.item;
            if (retain_success) {
                simulator.success_examples.push_back(std::move(example));
            } else {
                simulator.failure_examples.push_back(std::move(example));
            }
        }
    }
    /*
     * Per-node counters are the simulator hot-path authority. Aggregate the
     * public action/material maps once per externally visible chunk instead
     * of performing string hashes for every applied action. This preserves
     * exact cumulative accounting across arbitrary chunk sizes while keeping
     * long compiled-policy verification dominated by craft execution and
     * dispatch rather than reporting bookkeeping.
     */
    for (std::size_t node_index = 0;
         node_index < simulator.action_counts.size(); ++node_index) {
        const std::uint64_t total = simulator.action_counts[node_index];
        const std::uint64_t accounted =
            simulator.accounted_action_counts[node_index];
        if (total < accounted) {
            throw std::logic_error(
                "simulator action accounting counter moved backwards");
        }
        const std::uint64_t delta = total - accounted;
        if (delta == 0) continue;
        const std::string& descriptor =
            simulator.action_descriptor_ids[node_index];
        if (!descriptor.empty()) {
            simulator.action_descriptor_counts[descriptor] += delta;
        }
        simulator.accounted_action_counts[node_index] = total;

        const std::uint64_t applied =
            simulator.applied_action_counts[node_index];
        const std::uint64_t accounted_applied =
            simulator.accounted_applied_action_counts[node_index];
        if (applied < accounted_applied) {
            throw std::logic_error(
                "simulator applied-action accounting counter moved "
                "backwards");
        }
        const std::uint64_t applied_delta = applied - accounted_applied;
        if (applied_delta != 0) {
            for (const std::string& price_key :
                 simulator.strategy->nodes[node_index].price_keys) {
                simulator.material_counts[price_key] += applied_delta;
            }
            simulator.accounted_applied_action_counts[node_index] = applied;
        }
    }
}

} // namespace poecraft
