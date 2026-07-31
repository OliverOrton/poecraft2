#include "solver_internal.hpp"
#include "solver_policy_refinement.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "poecraft/bitset.h"

/*
 * Compile an exact solver policy into the ordinary strategy graph format.
 * Policy states with the same action and continuation region share operation
 * nodes, while a collision-checked decision DAG routes each concrete item to
 * its exact policy region. A final default edge makes abstraction drift or a
 * vocabulary mismatch fail loudly. Expected-cost annotations are presentation
 * metadata consumed by the editor/board, never execution authority.
 */
namespace poecraft {
namespace solver {

namespace {

[[noreturn]] void gap(const std::string& message) {
    throw std::runtime_error("policy compile: " + message);
}

std::string json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

std::string number(double value) {
    char buffer[40];
    /* Cost annotations are presentation metadata. Fixed micro-chaos
     * precision keeps native/WASM documents byte-stable across harmless
     * last-bit differences in dense fixed-policy evaluation. */
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    return buffer;
}

void add_owned_bytes(
    std::uint64_t& total,
    const std::uint64_t addition) {
    if (addition >
        std::numeric_limits<std::uint64_t>::max() - total) {
        total = std::numeric_limits<std::uint64_t>::max();
    } else {
        total += addition;
    }
}

std::uint64_t owned_string_bytes(const std::string& value) {
    return static_cast<std::uint64_t>(value.capacity()) + 1;
}

const char* rarity_name(std::uint8_t rarity) {
    switch (rarity) {
    case PC_RARITY_NORMAL:
        return "normal";
    case PC_RARITY_MAGIC:
        return "magic";
    default:
        return "rare";
    }
}

void append_composite_range(
    std::string& out,
    const char* type,
    const std::vector<std::string>& parts,
    const std::size_t first,
    const std::size_t last) {
    constexpr std::size_t kMaxCompositeChildren = 1024;
    const std::size_t count = last - first;
    if (count == 1) {
        out += parts[first];
        return;
    }
    out += std::string("{\"type\":\"") + type +
           "\",\"conditions\":[";
    if (count <= kMaxCompositeChildren) {
        for (std::size_t index = first; index < last; ++index) {
            if (index > first) out += ',';
            out += parts[index];
        }
    } else {
        const std::size_t child_span = std::max<std::size_t>(
            kMaxCompositeChildren,
            (count + kMaxCompositeChildren - 1) /
                kMaxCompositeChildren);
        bool first_child = true;
        for (std::size_t child_first = first;
             child_first < last;
             child_first += child_span) {
            if (!first_child) out += ',';
            first_child = false;
            append_composite_range(
                out, type, parts, child_first,
                std::min(last, child_first + child_span));
        }
    }
    out += "]}";
}

std::string composite_of(
    const char* type,
    const std::vector<std::string>& parts,
    const char* empty_identity) {
    if (parts.empty()) return empty_identity;
    if (parts.size() == 1) return parts.front();
    std::string out;
    append_composite_range(out, type, parts, 0, parts.size());
    return out;
}

std::string all_of(const std::vector<std::string>& parts) {
    return composite_of(
        "all", parts, "{\"type\":\"always\"}");
}

std::string any_of(const std::vector<std::string>& parts) {
    return composite_of("any", parts, "{\"type\":\"any\",\"conditions\":[]}");
}

std::string at_least(std::size_t count,
                     const std::vector<std::string>& parts) {
    std::string out = "{\"type\":\"at_least\",\"count\":" +
                      std::to_string(count) + ",\"conditions\":[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += ',';
        out += parts[i];
    }
    out += "]}";
    return out;
}

std::string not_of(const std::string& part) {
    return "{\"type\":\"not\",\"conditions\":[" + part + "]}";
}

std::string rarity_condition(std::uint8_t rarity) {
    return std::string("{\"type\":\"rarity_is\",\"rarity\":\"") +
           rarity_name(rarity) + "\"}";
}

std::string count_condition(const char* type, std::uint8_t count) {
    return std::string("{\"type\":\"") + type +
           "\",\"min\":" + std::to_string(count) +
           ",\"max\":" + std::to_string(count) + "}";
}

std::string item_flag_condition(const char* flag) {
    return std::string("{\"type\":\"item_flag\",\"flag\":\"") +
           flag + "\"}";
}

std::string eldritch_tier_condition(
    const char* side,
    std::uint8_t tier) {
    return std::string("{\"type\":\"eldritch_tier\",\"side\":\"") +
           side + "\",\"min\":" + std::to_string(tier) +
           ",\"max\":" + std::to_string(tier) + "}";
}

/* Per-slot condition builders resolved once up front. */
struct SlotVocabulary {
    std::string member;     /* any-tier membership */
    std::string satisfied;  /* membership at the required tier */
};

std::string mod_key_of(const SessionImpl& session, std::uint32_t mod_id) {
    const DataImpl& data = *session.data;
    return data.string_at(data.mod_key_sid[session.global_index[mod_id]]);
}

std::string mod_count_condition_for_mask(
    const SessionImpl& session,
    const std::vector<std::uint64_t>& member_mask,
    std::uint8_t count,
    std::uint8_t required_flags,
    bool allow_family_cover,
    const char* subject) {
    std::map<std::uint32_t, std::uint32_t> representative_by_family;
    pc_bitset_for_each(
        member_mask.data(), session.words, [&](std::size_t bit) {
            const std::uint32_t mod = static_cast<std::uint32_t>(bit);
            representative_by_family.emplace(session.family_id[mod], mod);
        });
    bool exact_family_cover =
        allow_family_cover && !representative_by_family.empty();
    for (std::uint32_t mod = 0;
         exact_family_cover && mod < session.mod_count; ++mod) {
        const bool family_selected =
            representative_by_family.count(session.family_id[mod]) != 0;
        const bool class_selected =
            pc_bitset_test(member_mask.data(), mod);
        if (family_selected != class_selected) exact_family_cover = false;
    }

    std::string out = exact_family_cover
                          ? "{\"type\":\"mod_family_count\",\"family_mod_keys\":["
                          : "{\"type\":\"mod_count\",\"mod_keys\":[";
    bool first = true;
    const auto append_key = [&](const std::uint32_t mod) {
            const std::string key =
                mod_key_of(session, mod);
            if (key.empty()) {
                gap(std::string(subject) + " member " +
                    std::to_string(mod) +
                    " has no stable modifier key");
            }
            if (!first) out += ',';
            first = false;
            out += "\"" + json_escape(key) + "\"";
    };
    if (exact_family_cover) {
        for (const auto& [unused_family, representative] :
             representative_by_family) {
            (void)unused_family;
            append_key(representative);
        }
    } else {
        pc_bitset_for_each(
            member_mask.data(), session.words, [&](std::size_t bit) {
                append_key(static_cast<std::uint32_t>(bit));
            });
    }
    if (first) gap(std::string(subject) + " has no members");
    out += "]";
    if ((required_flags & PC_MOD_SLOT_FRACTURED) != 0) {
        out += ",\"fractured\":true";
    }
    if ((required_flags & PC_MOD_SLOT_CRAFTED) != 0) {
        out += ",\"crafted\":true";
    }
    out += ",\"min\":" + std::to_string(count) +
           ",\"max\":" + std::to_string(count) + "}";
    return out;
}

std::string mod_count_condition(
    const SessionImpl& session,
    const JunkClass& junk,
    std::uint8_t count,
    std::uint8_t required_flags = 0) {
    return mod_count_condition_for_mask(
        session, junk.member_mask, count, required_flags, true,
        "junk class");
}

std::string goal_member_class_condition(
    const SessionImpl& session,
    const ResolvedGoalSlot& slot,
    std::size_t slot_index,
    GoalSlotStatus status,
    std::uint32_t token) {
    if (token == 0 || token > slot.member_classes.size()) {
        gap(
            "goal slot " + std::to_string(slot_index) +
            " has an invalid exact member-class token");
    }
    const GoalMemberClass& member_class =
        slot.member_classes[token - 1];
    if (member_class.status != status) {
        gap(
            "goal slot " + std::to_string(slot_index) +
            " member-class token disagrees with tier status");
    }
    /* Emit every exact member key. A class is a semantic union, not an
     * arbitrarily selected representative modifier identity. */
    return mod_count_condition_for_mask(
        session, member_class.member_mask, 1, 0, false,
        "goal member class");
}

std::string with_slot_flags(
    std::string condition,
    bool fractured,
    bool crafted) {
    if (!fractured && !crafted) return condition;
    if (condition.empty() || condition.back() != '}') {
        gap("slot condition is not a JSON object");
    }
    condition.pop_back();
    if (fractured) condition += ",\"fractured\":true";
    if (crafted) condition += ",\"crafted\":true";
    condition += '}';
    return condition;
}

std::uint32_t first_bit(const std::vector<std::uint64_t>& mask,
                        std::size_t words) {
    std::uint32_t found = kNoId;
    pc_bitset_for_each(mask.data(), words, [&](std::size_t bit) {
        if (found == kNoId) found = static_cast<std::uint32_t>(bit);
    });
    return found;
}

SlotVocabulary slot_vocabulary(const SessionImpl& session,
                               const ResolvedGoalSlot& slot,
                               std::size_t slot_index) {
    const DataImpl& data = *session.data;
    SlotVocabulary vocabulary;
    if (slot.spec.family_id != kNoId) {
        const std::uint32_t representative =
            first_bit(slot.satisfying_mask, session.words);
        const std::string key = mod_key_of(session, representative);
        if (key.empty()) {
            gap("goal slot " + std::to_string(slot_index) +
                " has no stable modifier key");
        }
        vocabulary.member =
            "{\"type\":\"has_mod_family\",\"family_mod_key\":\"" +
            json_escape(key) + "\",\"min_tier\":0}";
        vocabulary.satisfied =
            "{\"type\":\"has_mod_family\",\"family_mod_key\":\"" +
            json_escape(key) + "\",\"min_tier\":" +
            std::to_string(slot.spec.min_tier) + "}";
    } else {
        const std::string key =
            data.string_at(data.group_key_sids[slot.spec.group_id]);
        if (key.empty()) {
            gap("goal slot " + std::to_string(slot_index) +
                " group has no stable key");
        }
        vocabulary.member = "{\"type\":\"has_mod_group\",\"group\":\"" +
                            json_escape(key) + "\",\"min_tier\":0}";
        vocabulary.satisfied =
            "{\"type\":\"has_mod_group\",\"group\":\"" +
            json_escape(key) + "\",\"min_tier\":" +
            std::to_string(slot.spec.min_tier) + "}";
    }
    return vocabulary;
}

std::string abstract_state_condition(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const std::vector<SlotVocabulary>& vocabulary,
    const AbstractState& state) {
    std::vector<std::string> parts{rarity_condition(state.rarity)};
    parts.push_back(
        count_condition("prefix_count_range", state.prefix_count));
    parts.push_back(
        count_condition("suffix_count_range", state.suffix_count));
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        const auto status =
            static_cast<GoalSlotStatus>(state.slot_status[i]);
        const bool fractured =
            (state.fractured_goal_mask & (1u << i)) != 0;
        const bool crafted =
            (state.crafted_goal_mask & (1u << i)) != 0;
        switch (status) {
        case GoalSlotStatus::Satisfied:
            parts.push_back(vocabulary[i].satisfied);
            parts.push_back(
                fractured
                    ? with_slot_flags(vocabulary[i].member, true, false)
                    : not_of(with_slot_flags(
                          vocabulary[i].member, true, false)));
            parts.push_back(
                crafted
                    ? with_slot_flags(vocabulary[i].member, false, true)
                    : not_of(with_slot_flags(
                          vocabulary[i].member, false, true)));
            break;
        case GoalSlotStatus::PresentBelowTier:
            parts.push_back(vocabulary[i].member);
            parts.push_back(not_of(vocabulary[i].satisfied));
            parts.push_back(
                fractured
                    ? with_slot_flags(vocabulary[i].member, true, false)
                    : not_of(with_slot_flags(
                          vocabulary[i].member, true, false)));
            parts.push_back(
                crafted
                    ? with_slot_flags(vocabulary[i].member, false, true)
                    : not_of(with_slot_flags(
                          vocabulary[i].member, false, true)));
            break;
        case GoalSlotStatus::Absent:
            parts.push_back(not_of(vocabulary[i].member));
            break;
        }
        const ResolvedGoalSlot& slot = layout.slots[i];
        const std::uint32_t member_class_token =
            state.goal_member_class_tokens[i];
        if (slot.member_classes.empty()) {
            if (member_class_token != 0) {
                gap(
                    "coarse goal slot " + std::to_string(i) +
                    " carries an exact member-class token");
            }
        } else if (status == GoalSlotStatus::Absent) {
            if (member_class_token != 0) {
                gap(
                    "absent goal slot " + std::to_string(i) +
                    " carries an exact member-class token");
            }
        } else {
            parts.push_back(goal_member_class_condition(
                session, slot, i, status, member_class_token));
        }
    }
    static const std::pair<std::uint32_t, const char*> flag_conditions[] = {
        {kFlagCorrupted, "corrupted"},
        {kFlagMirrored, "mirrored"},
        {kFlagSplit, "split"},
        {kFlagSynthesised, "synthesised"},
        {kFlagFractured, "fractured"},
        {kFlagCraftedMod, "crafted"},
        {kFlagVeiledMod, "veiled"},
        {kFlagMultimod, "multimod"},
        {kFlagNoAttack, "no_attack"},
        {kFlagNoCaster, "no_caster"},
        {kFlagPrefixesLocked, "prefixes_locked"},
        {kFlagSuffixesLocked, "suffixes_locked"},
        {kFlagInfluenced, "influenced"},
        {kFlagEldritchImplicit, "eldritch_implicit"},
    };
    for (const auto& [flag, name] : flag_conditions) {
        const std::string condition = item_flag_condition(name);
        parts.push_back((state.flags & flag) != 0
                            ? condition
                            : not_of(condition));
    }
    /* Item flags establish that a metamod is active. When that ordinary
     * crafted affix is fractured, distinguish the exact carrier identity so
     * compiled routing agrees with the native transition. */
    const DataImpl& data = *session.data;
    const std::pair<std::uint32_t, int> metamod_flags[] = {
        {kFlagMultimod, data.metamod_multimod_code},
        {kFlagNoAttack, data.metamod_no_attack_code},
        {kFlagNoCaster, data.metamod_no_caster_code},
        {kFlagPrefixesLocked, data.metamod_prefixes_locked_code},
        {kFlagSuffixesLocked, data.metamod_suffixes_locked_code},
    };
    for (const auto& [flag, code] : metamod_flags) {
        if ((state.flags & flag) == 0 || code < 0) continue;
        std::uint32_t representative = kNoId;
        for (const std::uint32_t mod : session.bench_mod_ids) {
            if (mod < session.metamod_type.size() &&
                session.metamod_type[mod] == code) {
                representative = mod;
                break;
            }
        }
        if (representative == kNoId) {
            gap("active metamod has no stable session modifier");
        }
        const std::string member =
            "{\"type\":\"has_mod_family\",\"family_mod_key\":\"" +
            json_escape(mod_key_of(session, representative)) +
            "\",\"min_tier\":0}";
        parts.push_back(with_slot_flags(member, false, true));
        const std::string fractured =
            with_slot_flags(member, true, false);
        parts.push_back((state.fractured_metamod_flags & flag) != 0
                            ? fractured
                            : not_of(fractured));
    }
    const std::string veiled_prefix = item_flag_condition("veiled_prefix");
    const std::string veiled_suffix = item_flag_condition("veiled_suffix");
    parts.push_back(state.veiled_side == PC_SIDE_PREFIX
                        ? veiled_prefix
                        : not_of(veiled_prefix));
    parts.push_back(state.veiled_side == PC_SIDE_SUFFIX
                        ? veiled_suffix
                        : not_of(veiled_suffix));
    parts.push_back(
        "{\"type\":\"influence_bits\",\"value\":" +
        std::to_string(state.influence_bits) + "}");
    parts.push_back(eldritch_tier_condition(
        "searing", state.searing_exarch_tier));
    parts.push_back(eldritch_tier_condition(
        "eater", state.eater_of_worlds_tier));
    for (std::size_t i = 0; i < layout.junk_classes.size(); ++i) {
        parts.push_back(mod_count_condition(
            session, layout.junk_classes[i], state.junk_counts[i]));
        parts.push_back(mod_count_condition(
            session, layout.junk_classes[i],
            state.fractured_junk_counts[i], PC_MOD_SLOT_FRACTURED));
        parts.push_back(mod_count_condition(
            session, layout.junk_classes[i], state.crafted_junk_counts[i],
            PC_MOD_SLOT_CRAFTED));
        parts.push_back(mod_count_condition(
            session, layout.junk_classes[i],
            state.fractured_crafted_junk_counts[i],
            PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED));
    }
    return all_of(parts);
}

struct QuotientFeature {
    std::uint64_t value = 0;
    std::string condition;
};

std::vector<QuotientFeature> quotient_features(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const std::vector<SlotVocabulary>& vocabulary,
    const AbstractState& state) {
    std::vector<QuotientFeature> features;
    features.push_back({state.rarity, rarity_condition(state.rarity)});
    features.push_back({
        state.prefix_count,
        count_condition("prefix_count_range", state.prefix_count)});
    features.push_back({
        state.suffix_count,
        count_condition("suffix_count_range", state.suffix_count)});
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        const auto status =
            static_cast<GoalSlotStatus>(state.slot_status[i]);
        std::string status_condition;
        switch (status) {
        case GoalSlotStatus::Satisfied:
            status_condition = vocabulary[i].satisfied;
            break;
        case GoalSlotStatus::PresentBelowTier:
            status_condition = all_of({
                vocabulary[i].member, not_of(vocabulary[i].satisfied)});
            break;
        case GoalSlotStatus::Absent:
            status_condition = not_of(vocabulary[i].member);
            break;
        }
        features.push_back({state.slot_status[i], status_condition});
        const ResolvedGoalSlot& slot = layout.slots[i];
        const std::uint32_t member_class_token =
            state.goal_member_class_tokens[i];
        if (!slot.member_classes.empty()) {
            std::string member_class_condition;
            if (status == GoalSlotStatus::Absent) {
                if (member_class_token != 0) {
                    gap(
                        "absent goal slot " + std::to_string(i) +
                        " carries an exact member-class token");
                }
                member_class_condition = not_of(vocabulary[i].member);
            } else {
                member_class_condition = goal_member_class_condition(
                    session, slot, i, status, member_class_token);
            }
            features.push_back({
                member_class_token, std::move(member_class_condition)});
        } else if (member_class_token != 0) {
            gap(
                "coarse goal slot " + std::to_string(i) +
                " carries an exact member-class token");
        }
        const bool fractured =
            (state.fractured_goal_mask & (1u << i)) != 0;
        const bool crafted =
            (state.crafted_goal_mask & (1u << i)) != 0;
        const std::string fractured_condition =
            with_slot_flags(vocabulary[i].member, true, false);
        const std::string crafted_condition =
            with_slot_flags(vocabulary[i].member, false, true);
        features.push_back({
            fractured ? 1u : 0u,
            fractured ? fractured_condition
                       : not_of(fractured_condition)});
        features.push_back({
            crafted ? 1u : 0u,
            crafted ? crafted_condition : not_of(crafted_condition)});
    }
    static const std::pair<std::uint32_t, const char*> flag_conditions[] = {
        {kFlagCorrupted, "corrupted"},
        {kFlagMirrored, "mirrored"},
        {kFlagSplit, "split"},
        {kFlagSynthesised, "synthesised"},
        {kFlagFractured, "fractured"},
        {kFlagCraftedMod, "crafted"},
        {kFlagVeiledMod, "veiled"},
        {kFlagMultimod, "multimod"},
        {kFlagNoAttack, "no_attack"},
        {kFlagNoCaster, "no_caster"},
        {kFlagPrefixesLocked, "prefixes_locked"},
        {kFlagSuffixesLocked, "suffixes_locked"},
        {kFlagInfluenced, "influenced"},
        {kFlagEldritchImplicit, "eldritch_implicit"},
    };
    for (const auto& [flag, name] : flag_conditions) {
        const bool present = (state.flags & flag) != 0;
        const std::string condition = item_flag_condition(name);
        features.push_back({
            present ? 1u : 0u,
            present ? condition : not_of(condition)});
    }
    const std::string veiled_prefix = item_flag_condition("veiled_prefix");
    const std::string veiled_suffix = item_flag_condition("veiled_suffix");
    features.push_back({
        static_cast<std::uint8_t>(state.veiled_side + 1),
        all_of({
            state.veiled_side == PC_SIDE_PREFIX
                ? veiled_prefix
                : not_of(veiled_prefix),
            state.veiled_side == PC_SIDE_SUFFIX
                ? veiled_suffix
                : not_of(veiled_suffix)})});
    features.push_back({
        state.influence_bits,
        "{\"type\":\"influence_bits\",\"value\":" +
            std::to_string(state.influence_bits) + "}"});
    features.push_back({
        state.searing_exarch_tier,
        eldritch_tier_condition("searing", state.searing_exarch_tier)});
    features.push_back({
        state.eater_of_worlds_tier,
        eldritch_tier_condition("eater", state.eater_of_worlds_tier)});
    for (std::size_t i = 0; i < layout.junk_classes.size(); ++i) {
        const JunkClass& junk = layout.junk_classes[i];
        features.push_back({
            state.junk_counts[i],
            mod_count_condition(session, junk, state.junk_counts[i])});
        features.push_back({
            state.fractured_junk_counts[i],
            mod_count_condition(
                session, junk, state.fractured_junk_counts[i],
                PC_MOD_SLOT_FRACTURED)});
        features.push_back({
            state.crafted_junk_counts[i],
            mod_count_condition(
                session, junk, state.crafted_junk_counts[i],
                PC_MOD_SLOT_CRAFTED)});
        features.push_back({
            state.fractured_crafted_junk_counts[i],
            mod_count_condition(
                session, junk, state.fractured_crafted_junk_counts[i],
                PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)});
    }
    return features;
}

std::vector<std::uint32_t> quotient_feature_values(
    const AbstractLayout& layout,
    const AbstractState& state) {
    std::vector<std::uint32_t> values;
    values.push_back(state.rarity);
    values.push_back(state.prefix_count);
    values.push_back(state.suffix_count);
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        values.push_back(state.slot_status[i]);
        if (!layout.slots[i].member_classes.empty()) {
            values.push_back(state.goal_member_class_tokens[i]);
        }
        values.push_back(
            (state.fractured_goal_mask & (1u << i)) != 0 ? 1u : 0u);
        values.push_back(
            (state.crafted_goal_mask & (1u << i)) != 0 ? 1u : 0u);
    }
    static const std::uint32_t flags[] = {
        kFlagCorrupted, kFlagMirrored, kFlagSplit, kFlagSynthesised,
        kFlagFractured, kFlagCraftedMod, kFlagVeiledMod, kFlagMultimod,
        kFlagNoAttack, kFlagNoCaster, kFlagPrefixesLocked,
        kFlagSuffixesLocked, kFlagInfluenced, kFlagEldritchImplicit,
    };
    for (const std::uint32_t flag : flags) {
        values.push_back((state.flags & flag) != 0 ? 1u : 0u);
    }
    values.push_back(static_cast<std::uint8_t>(state.veiled_side + 1));
    values.push_back(state.influence_bits);
    values.push_back(state.searing_exarch_tier);
    values.push_back(state.eater_of_worlds_tier);
    for (std::size_t i = 0; i < layout.junk_classes.size(); ++i) {
        values.push_back(state.junk_counts[i]);
        values.push_back(state.fractured_junk_counts[i]);
        values.push_back(state.crafted_junk_counts[i]);
        values.push_back(state.fractured_crafted_junk_counts[i]);
    }
    return values;
}

struct QuotientFeatureIndex {
    std::size_t width = 0;
    std::uint32_t non_goal_states = 0;
    std::vector<std::uint32_t> values;
    std::vector<std::unordered_map<
        std::uint32_t, std::vector<std::uint32_t>>> non_goal_buckets;

    std::uint32_t at(
        const std::uint32_t state, const std::size_t feature) const {
        return values.at(static_cast<std::size_t>(state) * width + feature);
    }

    const std::uint32_t* row(const std::uint32_t state) const noexcept {
        return values.data() + static_cast<std::size_t>(state) * width;
    }
};

std::string quotient_class_condition(
    const CalcContext& calc,
    const std::vector<SlotVocabulary>& vocabulary,
    const std::uint32_t representative,
    const std::vector<std::uint32_t>& members,
    const QuotientFeatureIndex& feature_index,
    const std::vector<std::uint32_t>& class_by_state) {
    const SessionImpl& session = calc.session();
    const AbstractLayout& layout = calc.layout();
    const std::vector<QuotientFeature> representative_features =
        quotient_features(
            session, layout, vocabulary, calc.state(representative));
    const auto exact_fallback = [&]() {
        std::vector<std::string> conditions;
        conditions.reserve(members.size());
        for (const std::uint32_t member : members) {
            conditions.push_back(abstract_state_condition(
                session, layout, vocabulary, calc.state(member)));
        }
        std::sort(conditions.begin(), conditions.end());
        conditions.erase(
            std::unique(conditions.begin(), conditions.end()),
            conditions.end());
        return conditions.size() == 1 ? conditions.front()
                                      : any_of(conditions);
    };
    if (representative_features.empty()) return exact_fallback();
    std::vector<std::uint8_t> constant(representative_features.size(), 1);
    const std::uint32_t* representative_values =
        feature_index.row(representative);
    for (const std::uint32_t member : members) {
        const std::uint32_t* member_values = feature_index.row(member);
        for (std::size_t feature = 0; feature < constant.size(); ++feature) {
            if (member_values[feature] != representative_values[feature]) {
                constant[feature] = 0;
            }
        }
    }
    std::vector<std::size_t> selected;
    std::vector<std::uint8_t> used(constant.size(), 0);

    const std::size_t class_size = members.size();
    const std::size_t outsider_count =
        feature_index.non_goal_states - class_size;
    if (outsider_count == 0) {
        std::vector<std::string> goal_parts{
            rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        for (const SlotVocabulary& slot : vocabulary) {
            satisfied.push_back(slot.satisfied);
        }
        goal_parts.push_back(
            calc.goal().required_satisfied_slots() == satisfied.size()
                ? all_of(satisfied)
                : at_least(
                      calc.goal().required_satisfied_slots(), satisfied));
        return not_of(all_of(goal_parts));
    }

    /* The old implementation evaluated every feature against every strict
     * state before choosing the first discriminator for every quotient
     * class.  Use the exact inverted buckets to make the identical greedy
     * choice in O(features), then continue over only the surviving bucket. */
    std::size_t first = constant.size();
    std::size_t first_remaining = outsider_count;
    for (std::size_t feature = 0; feature < constant.size(); ++feature) {
        if (!constant[feature]) continue;
        const std::uint32_t value = representative_values[feature];
        const auto& by_value = feature_index.non_goal_buckets[feature];
        const auto found = by_value.find(value);
        const std::size_t bucket_size =
            found == by_value.end() ? 0 : found->second.size();
        const std::size_t outside_class = bucket_size - class_size;
        if (outside_class < first_remaining) {
            first = feature;
            first_remaining = outside_class;
        }
    }
    if (first == constant.size()) return exact_fallback();
    used[first] = 1;
    selected.push_back(first);
    std::vector<std::uint32_t> remaining;
    const auto& first_bucket =
        feature_index.non_goal_buckets[first].at(
            representative_values[first]);
    remaining.reserve(first_remaining);
    for (const std::uint32_t state : first_bucket) {
        if (class_by_state[state] != representative) {
            remaining.push_back(state);
        }
    }
    std::vector<std::size_t> covered(constant.size(), 0);
    while (!remaining.empty()) {
        std::size_t best = constant.size();
        std::size_t best_covered = 0;
        std::fill(covered.begin(), covered.end(), 0);
        for (const std::uint32_t state : remaining) {
            const std::uint32_t* state_values = feature_index.row(state);
            for (std::size_t feature = 0;
                 feature < constant.size(); ++feature) {
                if (!constant[feature] || used[feature]) continue;
                covered[feature] +=
                    state_values[feature] != representative_values[feature];
            }
        }
        for (std::size_t feature = 0; feature < constant.size(); ++feature) {
            if (!constant[feature] || used[feature]) continue;
            if (covered[feature] > best_covered) {
                best = feature;
                best_covered = covered[feature];
            }
        }
        if (best == constant.size() || best_covered == 0) {
            return exact_fallback();
        }
        used[best] = 1;
        selected.push_back(best);
        remaining.erase(
            std::remove_if(
                remaining.begin(), remaining.end(),
                [&](const std::uint32_t state) {
                    return feature_index.row(state)[best] !=
                           representative_values[best];
                }),
            remaining.end());
    }
    if (selected.empty()) {
        std::vector<std::string> goal_parts{
            rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        for (const SlotVocabulary& slot : vocabulary) {
            satisfied.push_back(slot.satisfied);
        }
        goal_parts.push_back(
            calc.goal().required_satisfied_slots() == satisfied.size()
                ? all_of(satisfied)
                : at_least(
                      calc.goal().required_satisfied_slots(), satisfied));
        return not_of(all_of(goal_parts));
    }
    std::vector<std::string> conditions;
    conditions.reserve(selected.size());
    for (const std::size_t feature : selected) {
        conditions.push_back(representative_features[feature].condition);
    }
    return all_of(conditions);
}

std::string hex_u64(const std::uint64_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (std::size_t index = 0; index < out.size(); ++index) {
        const std::size_t shift = (out.size() - index - 1) * 4;
        out[index] = digits[(value >> shift) & 0xfu];
    }
    return out;
}

std::string stable_key_json(
        const refinement::StableKey& value) {
    std::string out = "[";
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index != 0) out += ',';
        out += "\"" + hex_u64(value[index]) + "\"";
    }
    return out + "]";
}

std::string sorted_u32_json(
        const std::vector<std::uint32_t>& values) {
    std::string out = "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out += ',';
        out += std::to_string(values[index]);
    }
    return out + "]";
}

refinement::StableKey count_observation_membership(
        const AbstractLayout& layout,
        const std::uint32_t mod) {
    refinement::StableKey bits(
        (layout.count_observations.size() + 63) / 64, 0);
    for (std::size_t observation = 0;
         observation < layout.count_observations.size();
         ++observation) {
        const std::vector<std::uint64_t>& mask =
            layout.count_observations[observation].member_mask;
        if (!mask.empty() && pc_bitset_test(mask.data(), mod)) {
            bits[observation / 64] |=
                std::uint64_t{1} << (observation % 64);
        }
    }
    return bits;
}

std::string observation_signature_condition(
        const SessionImpl& session,
        const AbstractLayout& layout,
        const refinement::ObservationRequirement& input_requirement,
        const refinement::FeatureSignature& input_signature) {
    const refinement::ObservationRequirement requirement =
        refinement::canonical_observation_requirement(
            input_requirement);
    const refinement::FeatureSignature signature =
        refinement::canonical_feature_signature(
            input_signature);
    if (requirement != input_requirement ||
        signature != input_signature) {
        gap(
            "refined policy supplied a non-canonical observation "
            "route");
    }

    std::string out =
        "{\"type\":\"observation_signature\",\"version\":" +
        std::to_string(kObservationSignatureConditionVersion) +
        ",\"requirement\":{\"item_features\":" +
        std::to_string(requirement.item_features) +
        ",\"modifier_tag_ids\":" +
        sorted_u32_json(requirement.modifier_tag_ids) +
        ",\"affix_observations\":[";
    for (std::size_t index = 0;
         index < requirement.affix_observations.size(); ++index) {
        if (index != 0) out += ',';
        const RefinementAffixObservation& observation =
            requirement.affix_observations[index];
        out +=
            "{\"features\":" +
            std::to_string(observation.features) +
            ",\"selector\":{\"required_affix_traits\":" +
            std::to_string(
                observation.selector.required_affix_traits) +
            ",\"forbidden_affix_traits\":" +
            std::to_string(
                observation.selector.forbidden_affix_traits) +
            ",\"required_item_traits\":" +
            std::to_string(
                observation.selector.required_item_traits) +
            ",\"forbidden_item_traits\":" +
            std::to_string(
                observation.selector.forbidden_item_traits) +
            ",\"required_tag_ids\":" +
            sorted_u32_json(
                observation.selector.required_tag_ids) +
            "}}";
    }
    out += "]},\"signature\":[";
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (index != 0) out += ',';
        const refinement::FeatureAtom& atom = signature[index];
        out +=
            "{\"feature\":" +
            std::to_string(
                static_cast<std::uint8_t>(atom.feature)) +
            ",\"subject\":" + std::to_string(atom.subject) +
            ",\"value\":" + stable_key_json(atom.value) + "}";
    }
    out += "],\"goal_status_tier_class_by_mod\":[";
    bool first = true;
    const bool goal_context_required =
        std::any_of(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            [](const RefinementAffixObservation& observation) {
                return (
                    observation.features &
                    refinement_feature(
                        RefinementFeature::
                            GoalStatusTierClass)) != 0;
            });
    if (goal_context_required) {
        for (std::uint32_t mod = 0;
             mod < session.mod_count; ++mod) {
            refinement::StableKey value{
                0u,
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Absent)};
            for (std::size_t slot = 0;
                 slot < layout.slots.size(); ++slot) {
                if (!pc_bitset_test(
                        layout.slots[slot].member_mask.data(),
                        mod)) {
                    continue;
                }
                if (value.front() != 0) {
                    gap(
                        "observation-signature goal context has "
                        "overlapping modifier slots");
                }
                value = {
                    slot + 1,
                    static_cast<std::uint8_t>(
                        pc_bitset_test(
                            layout.slots[slot]
                                .satisfying_mask.data(),
                            mod)
                            ? GoalSlotStatus::Satisfied
                            : GoalSlotStatus::
                                  PresentBelowTier)};
            }
            if (value.front() == 0) continue;
            if (!first) out += ',';
            first = false;
            out +=
                "{\"mod_key\":\"" +
                json_escape(mod_key_of(session, mod)) +
                "\",\"value\":" +
                stable_key_json(value) + "}";
        }
    }
    out +=
        "],\"count_observation_count\":" +
        std::to_string(layout.count_observations.size()) +
        ",\"count_observation_membership_by_mod\":[";
    first = true;
    const bool count_context_required =
        std::any_of(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            [](const RefinementAffixObservation& observation) {
                return (
                    observation.features &
                    refinement_feature(
                        RefinementFeature::
                            CountObservationMembership)) != 0;
            });
    if (count_context_required) {
        for (std::uint32_t mod = 0;
             mod < session.mod_count; ++mod) {
            const refinement::StableKey value =
                count_observation_membership(layout, mod);
            if (std::all_of(
                    value.begin(), value.end(),
                    [](const std::uint64_t word) {
                        return word == 0;
                    })) {
                continue;
            }
            if (!first) out += ',';
            first = false;
            out +=
                "{\"mod_key\":\"" +
                json_escape(mod_key_of(session, mod)) +
                "\",\"value\":" +
                stable_key_json(value) + "}";
        }
    }
    return out + "]}";
}

std::string operation_json(const SessionImpl& session,
                           const ActionDescriptor& action) {
    const DataImpl& data = *session.data;
    if (action.synthetic) return "{\"type\":\"restart\"}";
    switch (action.params.type) {
    case ActionType::Essence:
        if (action.params.essence_index >=
            data.essence_key_sids.size()) {
            gap(
                "essence action has no stable session key");
        }
        return "{\"type\":\"essence\",\"essence_key\":\"" +
               json_escape(data.string_at(
                   data.essence_key_sids[action.params.essence_index])) +
               "\"}";
    case ActionType::Fossil: {
        std::string out = "{\"type\":\"fossil\",\"fossils\":[";
        for (std::size_t i = 0; i < action.params.fossil_indices.size();
             ++i) {
            if (action.params.fossil_indices[i] >=
                data.fossil_key_sids.size()) {
                gap(
                    "fossil action has no stable session key");
            }
            if (i > 0) out += ',';
            out += '"';
            out += json_escape(data.string_at(
                data.fossil_key_sids[action.params.fossil_indices[i]]));
            out += '"';
        }
        return out + "]}";
    }
    case ActionType::Bench:
    case ActionType::Unveil:
        return std::string("{\"type\":\"") +
               (action.params.type == ActionType::Bench ? "bench"
                                                        : "unveil") +
               "\",\"mod_key\":\"" +
               json_escape(mod_key_of(session, action.params.mod_id)) +
               "\"}";
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
        return std::string("{\"type\":\"") +
               (action.params.type == ActionType::HarvestReforge
                    ? "harvest_reforge"
                    : "harvest_augment") +
               "\",\"target_tag\":\"" +
               json_escape(data.tag_name_by_id.at(
                   action.params.target_tag_id)) +
               "\"}";
    case ActionType::HarvestResist:
        return "{\"type\":\"harvest_resist\",\"source_tag\":\"" +
               json_escape(
                   data.tag_name_by_id.at(action.params.source_tag_id)) +
               "\",\"target_tag\":\"" +
               json_escape(
                   data.tag_name_by_id.at(action.params.target_tag_id)) +
               "\"}";
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
        return std::string("{\"type\":\"") +
               (action.params.type == ActionType::EldritchEmber
                    ? "eldritch_ember"
                    : "eldritch_ichor") +
               "\",\"tier\":" + std::to_string(action.params.tier) + "}";
    case ActionType::InfluenceExalt:
        return "{\"type\":\"influence_exalt\",\"influence\":\"" +
               json_escape(data.influence_name_by_code.at(
                   static_cast<std::size_t>(
                       action.params.influence_code))) +
               "\"}";
    case ActionType::Transmute:
        return "{\"type\":\"transmute\"}";
    case ActionType::Augment:
        return "{\"type\":\"augment\"}";
    case ActionType::Alteration:
        return "{\"type\":\"alteration\"}";
    case ActionType::Regal:
        return "{\"type\":\"regal\"}";
    case ActionType::Alchemy:
        return "{\"type\":\"alchemy\"}";
    case ActionType::Chaos:
        return "{\"type\":\"chaos\"}";
    case ActionType::Exalt:
        return "{\"type\":\"exalt\"}";
    case ActionType::Annul:
        return "{\"type\":\"annul\"}";
    case ActionType::Scour:
        return "{\"type\":\"scour\"}";
    case ActionType::VeiledChaos:
        return "{\"type\":\"veiled_chaos\"}";
    case ActionType::VeiledExalt:
        return "{\"type\":\"veiled_exalt\"}";
    case ActionType::EldritchExalt:
        return "{\"type\":\"eldritch_exalt\"}";
    case ActionType::EldritchChaos:
        return "{\"type\":\"eldritch_chaos\"}";
    case ActionType::EldritchAnnul:
        return "{\"type\":\"eldritch_annul\"}";
    case ActionType::Fracture:
        return "{\"type\":\"fracture\"}";
    case ActionType::RemoveCraftedModifiers:
        return "{\"type\":\"remove_crafted_modifiers\"}";
    }
    gap("action type has no strategy operation serializer");
}

std::string accounting_roles_json(
    const PlannerOperator& planner,
    std::uint32_t action_index,
    bool fracture_use = false) {
    std::vector<std::string> roles;
    const auto add = [&](const char* role) {
        if (std::find(roles.begin(), roles.end(), role) == roles.end()) {
            roles.emplace_back(role);
        }
    };
    if (planner.kind == PlannerOperatorKind::FixedOption) {
        switch (planner.option_kind) {
        case FixedOptionKind::FracturePrepare:
            add(fracture_use ? "fracture" : "fracture_preparation");
            if (!fracture_use) add("retry_action");
            break;
        case FixedOptionKind::TemporaryBenchRepeat:
            if (action_index == planner.setup_action) add("temporary_blocker");
            if (action_index == planner.followup_action) add("retry_action");
            if (action_index == planner.cleanup_action) {
                add("cleanup_or_replacement");
            }
            break;
        case FixedOptionKind::ProtectedRepeat:
            if (action_index == planner.setup_action) add("protection_setup");
            if (action_index == planner.followup_action) add("retry_action");
            if (action_index == planner.cleanup_action) {
                add("cleanup_or_replacement");
            }
            break;
        case FixedOptionKind::ProtectedSide:
            if (action_index == planner.setup_action) add("protection_setup");
            break;
        case FixedOptionKind::MultimodFinish:
            if (action_index == planner.setup_action) {
                add("multimod_setup");
            } else {
                add("multimod_finish");
                add("permanent_goal_bench");
                add("deterministic_finish");
            }
            break;
        case FixedOptionKind::Renewal:
            add("retry_action");
            break;
        default:
            break;
        }
    }
    if (planner.automatic_kind == AutomaticCandidateKind::PermanentBench) {
        add("permanent_goal_bench");
        add("deterministic_finish");
    }
    if (roles.empty()) return {};
    std::string out = ",\"accounting_roles\":[";
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (i != 0) out += ',';
        out += "\"" + roles[i] + "\"";
    }
    return out + ']';
}

} // namespace

std::string compile_policy_strategy_json(
    CalcContext& calc,
    const SolveResult& result,
    const std::string& name,
    PolicyCompilationTelemetry* telemetry,
    const std::uint64_t max_strategy_json_bytes,
    const refinement::RefinedPolicyCompileRouting* refined_routing,
    const std::uint64_t max_compiler_owned_bytes) {
    const std::uint64_t strategy_json_limit = std::min(
        result.options.max_strategy_json_bytes,
        max_strategy_json_bytes);
    std::uint64_t compiler_peak_owned_bytes = 0;
    const auto observe_compiler_owned =
        [&](const std::uint64_t bytes) {
            compiler_peak_owned_bytes =
                std::max(compiler_peak_owned_bytes, bytes);
            if (telemetry != nullptr) {
                telemetry->peak_owned_bytes =
                    compiler_peak_owned_bytes;
            }
            if (bytes > max_compiler_owned_bytes) {
                if (telemetry != nullptr) {
                    telemetry->cap_hit =
                        "max_solver_owned_bytes";
                }
                throw SolverResourceLimit(
                    "max_solver_owned_bytes",
                    max_compiler_owned_bytes);
            }
        };
    if (!result.refined_policy_artifact.strategy_json.empty()) {
        if (result.refined_policy_artifact.strategy_json.size() >
            strategy_json_limit) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("retained refined policy exceeded "
                "max_strategy_json_bytes");
        }
        if (telemetry != nullptr) {
            telemetry->working_states =
                result.refined_policy_artifact.working_states;
            telemetry->policy_regions =
                result.refined_policy_artifact.policy_regions;
            telemetry->nodes = result.refined_policy_artifact.nodes;
            telemetry->edges = result.refined_policy_artifact.edges;
            telemetry->strategy_json_bytes =
                result.refined_policy_artifact.strategy_json.size();
        }
        observe_compiler_owned(
            result.refined_policy_artifact.strategy_json.size() + 1);
        return result.refined_policy_artifact.strategy_json;
    }
    const SessionImpl& session = calc.session();
    const DataImpl& data = *session.data;
    const AbstractLayout& layout = calc.layout();
    const auto primitive_observes_modifier_offer =
        [&](const PlannerOperator& planner) {
            return planner.kind == PlannerOperatorKind::Primitive &&
                   planner.primitive_action <
                       calc.registry().actions.size() &&
                   action_observes_modifier_offer(
                       calc.registry().actions[
                           planner.primitive_action]);
        };
    const auto option_observes_modifier_offer =
        [&](const PlannerOperator& planner) {
            return planner.kind == PlannerOperatorKind::FixedOption &&
                   !planner.primitive_program.empty() &&
                   planner.primitive_program.back() <
                       calc.registry().actions.size() &&
                   action_observes_modifier_offer(
                       calc.registry().actions[
                           planner.primitive_program.back()]);
        };

    if (result.start_state == kNoId ||
        result.start_state >= result.values.size()) {
        gap("solve result has no start state");
    }

    std::vector<SlotVocabulary> vocabulary;
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        vocabulary.push_back(
            slot_vocabulary(session, layout.slots[i], i));
    }

    /*
     * A witnessed fixed destructive renewal has one action-local behavior:
     * apply the selected reforge, succeed on the goal, otherwise repeat.
     * The native incumbent already proved the complete gated row. Recheck
     * action legality and the engine-owned preserved-boundary signature for
     * every policy-reachable non-goal carrier before emitting the compact
     * loop. This is a fixed-policy proof, not Bellman state equivalence.
     */
    if (result.primitive_renewal_witness.valid) {
        const PrimitiveRenewalWitness& witness =
            result.primitive_renewal_witness;
        const bool bounded =
            result.policy_status == SolvePolicyStatus::BoundedFeasible ||
            result.policy_status ==
                SolvePolicyStatus::BoundedNearOptimal;
        if (!bounded ||
            !result.options.goal_progress_gated_reforges ||
            witness.operator_index >= calc.operators().size() ||
            witness.primitive_action >=
                calc.registry().actions.size() ||
            witness.kernel_signature.empty() ||
            !(witness.success_probability > 0.0) ||
            !std::isfinite(witness.value) ||
            witness.value < 0.0 ||
            result.start_state >= result.policy_reachable.size() ||
            !result.policy_reachable[result.start_state] ||
            result.policy_reachable.size() != result.values.size() ||
            result.goal_states.size() != result.values.size() ||
            result.policy.size() != result.values.size()) {
            gap("gated primitive renewal witness is incomplete");
        }
        const PlannerOperator& planner =
            calc.operators().at(witness.operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action != witness.primitive_action ||
            result.policy[result.start_state].index !=
                witness.operator_index ||
            result.policy[result.start_state].kind != planner.kind) {
            gap("gated primitive renewal witness action changed");
        }
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(witness.primitive_action);
        if (!action_transition_facts(descriptor.params.type).renewal) {
            gap("gated primitive renewal witness is not destructive");
        }
        std::uint64_t working_states = 0;
        for (std::uint32_t state = 0;
             state < result.values.size(); ++state) {
            if (!result.policy_reachable[state] ||
                result.goal_states[state]) {
                continue;
            }
            ++working_states;
            if (result.policy[state].index != witness.operator_index ||
                result.policy[state].kind != planner.kind ||
                !action_legal(session, descriptor, calc.state(state))) {
                gap("gated primitive renewal policy changed on a "
                    "reachable carrier");
            }
            std::vector<std::uint64_t> signature;
            if (!calc.exact_reforge_kernel_signature(
                    state, witness.primitive_action, signature) ||
                signature != witness.kernel_signature) {
                gap("gated primitive renewal kernel signature changed");
            }
        }
        if (working_states == 0 ||
            working_states !=
                witness.validated_non_goal_states) {
            gap("gated primitive renewal reachable domain changed");
        }
        const double value_tolerance =
            1e-9 * std::max(1.0, std::abs(witness.value));
        if (!std::isfinite(result.evaluated_policy_cost) ||
            std::abs(
                result.evaluated_policy_cost -
                witness.value) > value_tolerance ||
            !std::isfinite(result.upper_bound) ||
            std::abs(
                result.upper_bound - witness.value) >
                value_tolerance) {
            gap("gated primitive renewal value changed");
        }

        pc_item_state start_item;
        if (result.has_exact_start_item) {
            start_item = result.exact_start_item;
        } else if (!calc.materialize(result.start_state, start_item)) {
            gap("gated primitive renewal start cannot be materialized");
        }
        std::vector<std::string> goal_parts{
            rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        satisfied.reserve(vocabulary.size());
        for (const SlotVocabulary& slot : vocabulary) {
            satisfied.push_back(slot.satisfied);
        }
        goal_parts.push_back(
            calc.goal().required_satisfied_slots() ==
                    satisfied.size()
                ? all_of(satisfied)
                : at_least(
                      calc.goal().required_satisfied_slots(),
                      satisfied));
        const std::string goal_condition = all_of(goal_parts);

        std::string json =
            "{\"version\":\"v1\",\"name\":\"" +
            json_escape(name) +
            "\",\"description\":\"Bounded executable fixed "
            "destructive-renewal policy exact within the "
            "zero-progress-reroll restriction; not a global optimum\","
            "\"base_state\":{\"base_key\":\"" +
            json_escape(
                data.string_at(
                    data.base_metadata_path_sid[session.base_index])) +
            "\",\"item_level\":" +
            std::to_string(session.item_level) +
            ",\"rarity\":\"" + rarity_name(start_item.rarity) + "\"";
        std::uint32_t item_flags = 0;
        if (start_item.item_flags & PC_ITEM_CORRUPTED) {
            item_flags |= PC_ITEM_CORRUPTED;
        }
        if (start_item.item_flags & PC_ITEM_MIRRORED) {
            item_flags |= PC_ITEM_MIRRORED;
        }
        if (start_item.item_flags & PC_ITEM_SPLIT) {
            item_flags |= PC_ITEM_SPLIT;
        }
        if (start_item.item_flags & PC_ITEM_SYNTHESISED) {
            item_flags |= PC_ITEM_SYNTHESISED;
        }
        json += ",\"item_flags\":" + std::to_string(item_flags);
        json += ",\"generic_influence_bits\":" +
                std::to_string(start_item.generic_influence_bits);
        json += ",\"searing_exarch_tier\":" +
                std::to_string(start_item.searing_exarch_tier);
        json += ",\"eater_of_worlds_tier\":" +
                std::to_string(start_item.eater_of_worlds_tier);
        const auto append_start_mods =
            [&](const char* field, const pc_mod_slot* mods,
                const std::uint8_t count) {
                json += ",\"";
                json += field;
                json += "\":[";
                for (std::uint8_t i = 0; i < count; ++i) {
                    if (i != 0) json += ',';
                    json += "{\"mod_key\":\"" +
                            json_escape(mod_key_of(
                                session, mods[i].mod_id)) + "\"";
                    if ((mods[i].flags &
                         PC_MOD_SLOT_FRACTURED) != 0) {
                        json += ",\"fractured\":true";
                    }
                    if ((mods[i].flags &
                         PC_MOD_SLOT_CRAFTED) != 0) {
                        json += ",\"crafted\":true";
                    }
                    if ((mods[i].flags &
                         PC_MOD_SLOT_VEILED) != 0) {
                        json += ",\"veiled\":true";
                    }
                    json += '}';
                }
                json += ']';
            };
        append_start_mods(
            "prefixes", start_item.prefixes,
            start_item.prefix_count);
        append_start_mods(
            "suffixes", start_item.suffixes,
            start_item.suffix_count);
        json +=
            "},\"start_node_id\":\"start\",\"nodes\":["
            "{\"id\":\"start\",\"kind\":\"start\"},"
            "{\"id\":\"router\",\"kind\":\"router\"},"
            "{\"id\":\"goal\",\"kind\":\"terminal\","
            "\"terminal\":\"success\"},"
            "{\"id\":\"renewal\",\"kind\":\"operation\","
            "\"expected_cost\":" +
            number(witness.value) +
            ",\"operation\":" +
            operation_json(session, descriptor) +
            "}],\"edges\":["
            "{\"id\":\"e0\",\"from\":\"start\",\"to\":\"router\","
            "\"priority\":0,\"is_default\":true},"
            "{\"id\":\"e1\",\"from\":\"router\",\"to\":\"goal\","
            "\"priority\":0,\"condition\":" +
            goal_condition +
            "},"
            "{\"id\":\"e2\",\"from\":\"router\",\"to\":\"renewal\","
            "\"priority\":1,\"is_default\":true},"
            "{\"id\":\"e3\",\"from\":\"renewal\",\"to\":\"router\","
            "\"priority\":0,\"is_default\":true}]}";
        constexpr std::uint32_t kNodes = 4;
        constexpr std::uint32_t kEdges = 4;
        if (kNodes > result.options.max_compiled_nodes) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_compiled_nodes";
            }
            gap("compiled fixed renewal exceeded max_compiled_nodes");
        }
        if (kEdges > result.options.max_compiled_edges) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_compiled_edges";
            }
            gap("compiled fixed renewal exceeded max_compiled_edges");
        }
        if (json.size() > strategy_json_limit) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("compiled fixed renewal exceeded "
                "max_strategy_json_bytes");
        }
        observe_compiler_owned(
            owned_string_bytes(json));
        if (telemetry != nullptr) {
            telemetry->working_states =
                static_cast<std::uint32_t>(working_states);
            telemetry->policy_regions = 1;
            telemetry->nodes = kNodes;
            telemetry->edges = kEdges;
            telemetry->strategy_json_bytes = json.size();
        }
        return json;
    }

    /* Collect and validate the policy-reachable working states. */
    std::vector<std::uint32_t> compiled_states;
    for (std::uint32_t state_id = 0; state_id < result.values.size();
         ++state_id) {
        if (!result.policy_reachable[state_id] ||
            result.goal_states[state_id]) {
            continue;
        }
        if (result.policy[state_id] == kNoId) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has no action");
        }
        if (result.policy[state_id] >= calc.operators().size()) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has an unknown planner operator");
        }
        const PlannerOperator& selected =
            calc.operators().at(result.policy[state_id]);
        if (result.policy[state_id].kind != selected.kind) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has a mismatched planner-operator tag");
        }
        compiled_states.push_back(state_id);
    }
    if (compiled_states.empty()) gap("policy reaches no working states");

    const auto gated_primitive_reforge =
        [&](const std::uint32_t state_id) {
        if (!result.options.goal_progress_gated_reforges) return false;
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        if (planner.kind != PlannerOperatorKind::Primitive) return false;
        const ActionDescriptor& action =
            calc.registry().actions.at(planner.primitive_action);
        return !action.synthetic &&
               action_transition_facts(action.params.type).renewal;
    };

    /* Abstract-state ids follow transition discovery order, which is allowed
     * to differ between native and WASM. Canonicalize the compile order and
     * emitted node ids by the exact state predicate so policy compression
     * produces the same document for the same solved policy. */
    std::map<std::uint32_t, std::string> state_conditions;
    std::map<std::uint32_t, std::vector<std::uint32_t>> quotient_members;
    std::uint64_t retained_compile_condition_bytes = 0;
    const bool structured_refined_route =
        refined_routing != nullptr;
    std::map<
        std::uint32_t,
        const refinement::RefinedPolicyCompileClass*>
        refined_class_by_representative;
    std::vector<std::string> refined_condition_by_class;
    std::map<std::uint32_t, std::vector<std::uint32_t>>
        refined_classes_by_coarse_parent;
    std::vector<std::uint32_t> refined_parent_order;
    std::map<std::uint32_t, std::string>
        refined_parent_condition;
    std::map<std::uint32_t, std::string>
        refined_parent_router;
    const auto refined_route_owned_bytes = [&]() {
        std::uint64_t bytes = 0;
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_condition_by_class.capacity()) *
                sizeof(std::string));
        for (const std::string& condition :
             refined_condition_by_class) {
            add_owned_bytes(
                bytes, owned_string_bytes(condition));
        }
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_parent_order.capacity()) *
                sizeof(std::uint32_t));
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_class_by_representative.size()) *
                (sizeof(std::uint32_t) +
                 sizeof(const refinement::RefinedPolicyCompileClass*) +
                 3 * sizeof(void*)));
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_classes_by_coarse_parent.size()) *
                (sizeof(std::uint32_t) +
                 sizeof(std::vector<std::uint32_t>) +
                 3 * sizeof(void*)));
        for (const auto& [unused, class_ids] :
             refined_classes_by_coarse_parent) {
            (void)unused;
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    class_ids.capacity()) *
                    sizeof(std::uint32_t));
        }
        const auto add_string_map =
            [&](const std::map<std::uint32_t, std::string>& values) {
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(values.size()) *
                        (sizeof(std::uint32_t) +
                         sizeof(std::string) +
                         3 * sizeof(void*)));
                for (const auto& [unused, value] : values) {
                    (void)unused;
                    add_owned_bytes(
                        bytes, owned_string_bytes(value));
                }
            };
        add_string_map(refined_parent_condition);
        add_string_map(refined_parent_router);
        return bytes;
    };
    if (structured_refined_route) {
        if (result.policy.size() != result.values.size() ||
            result.expanded.size() != result.values.size() ||
            result.goal_states.size() != result.values.size() ||
            result.policy_reachable.size() != result.values.size() ||
            result.behavioral_representative_by_state.size() !=
                result.values.size()) {
            gap(
                "refined policy compile sidecar disagrees with the "
                "strict state table");
        }
        refined_condition_by_class.resize(
            refined_routing->classes.size());
        std::vector<std::uint32_t> refined_member_owner(
            result.values.size(), kNoId);
        std::vector<std::vector<std::uint64_t>>
            refined_operator_key_by_class(
                refined_routing->classes.size());
        std::vector<std::vector<std::uint64_t>>
            refined_recipe_by_class(
                refined_routing->classes.size());
        for (std::size_t class_index = 0;
             class_index < refined_routing->classes.size();
             ++class_index) {
            const refinement::RefinedPolicyCompileClass& policy_class =
                refined_routing->classes[class_index];
            const bool members_are_canonical =
                std::is_sorted(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end()) &&
                std::adjacent_find(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end()) ==
                    policy_class.strict_members.end() &&
                std::all_of(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end(),
                    [&](std::uint32_t member) {
                        return member < result.values.size();
                    });
            if (policy_class.class_id != class_index ||
                policy_class.coarse_state == kNoId ||
                policy_class.representative_state >=
                    result.values.size() ||
                policy_class.strict_members.empty() ||
                !members_are_canonical ||
                !std::binary_search(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end(),
                    policy_class.representative_state)) {
                gap(
                    "refined policy compile sidecar is not canonical");
            }
            const auto [unused, inserted] =
                refined_class_by_representative.emplace(
                    policy_class.representative_state,
                    &policy_class);
            (void)unused;
            if (!inserted) {
                gap(
                    "refined policy compile sidecar repeats a "
                    "representative state");
            }
            for (const std::uint32_t member :
                 policy_class.strict_members) {
                if (refined_member_owner[member] != kNoId) {
                    gap(
                        "refined policy compile sidecar overlaps strict "
                        "member classes");
                }
                refined_member_owner[member] =
                    policy_class.class_id;
            }
            refined_condition_by_class[policy_class.class_id] =
                observation_signature_condition(
                    session,
                    layout,
                    policy_class.required_observations,
                    policy_class.observation_signature);
            if (!policy_class.terminal) {
                refined_classes_by_coarse_parent[
                    policy_class.coarse_state]
                        .push_back(policy_class.class_id);
            }
            observe_compiler_owned(
                refined_route_owned_bytes());
            for (const refinement::ProjectedTransition& transition :
                 policy_class.transitions) {
                if (transition.successor_class >=
                    refined_routing->classes.size()) {
                    gap(
                        "refined route transition has an invalid "
                        "successor");
                }
            }
        }

        for (std::uint32_t state = 0;
             state < result.values.size(); ++state) {
            const bool represented =
                result.expanded[state] != 0;
            const std::uint32_t owner =
                refined_member_owner[state];
            if (represented != (owner != kNoId)) {
                gap(
                    represented
                        ? "refined policy compile sidecar omits a "
                          "represented strict member"
                        : "refined policy compile sidecar contains an "
                          "unrepresented strict member");
            }
            if (owner == kNoId) continue;
            const refinement::RefinedPolicyCompileClass& policy_class =
                refined_routing->classes.at(owner);
            if (result.behavioral_representative_by_state[state] !=
                    policy_class.representative_state ||
                (result.goal_states[state] != 0) !=
                    policy_class.terminal) {
                gap(
                    "refined policy compile sidecar disagrees with its "
                    "strict assignment");
            }
        }

        /*
         * The shared refinement engine deliberately scopes selected-action
         * separation to one coarse policy location. Preserve that control
         * flow in the executable graph instead of flattening every exact
         * subclass onto one router. A parent guard is the union of all
         * policy-reachable strict semantic states represented by that coarse
         * location; no representative modifier identity is selected.
         *
         * These guards are re-evaluated after every operation. An action that
         * destroys an observed feature therefore routes directly into the
         * collapsed successor parent, while a preserving action retains only
         * the semantic strict class that actually survives.
         */
        std::map<
            refinement::StableKey,
            std::uint32_t>
            parent_by_stable_key;
        std::vector<
            std::pair<refinement::StableKey, std::uint32_t>>
            ordered_parents;
        std::map<std::string, std::uint32_t>
            globally_routed_member_owner;
        const auto parent_build_owned_bytes =
            [&](const std::vector<std::string>* current_conditions) {
                std::uint64_t bytes =
                    refined_route_owned_bytes();
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        parent_by_stable_key.size()) *
                        (sizeof(refinement::StableKey) +
                         sizeof(std::uint32_t) +
                         3 * sizeof(void*)));
                for (const auto& [key, unused] :
                     parent_by_stable_key) {
                    (void)unused;
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(
                            key.capacity()) *
                            sizeof(std::uint64_t));
                }
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        ordered_parents.capacity()) *
                        sizeof(ordered_parents.front()));
                for (const auto& [key, unused] :
                     ordered_parents) {
                    (void)unused;
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(
                            key.capacity()) *
                            sizeof(std::uint64_t));
                }
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        globally_routed_member_owner.size()) *
                        (sizeof(std::string) +
                         sizeof(std::uint32_t) +
                         3 * sizeof(void*)));
                for (const auto& [condition, unused] :
                     globally_routed_member_owner) {
                    (void)unused;
                    add_owned_bytes(
                        bytes, owned_string_bytes(condition));
                }
                if (current_conditions != nullptr) {
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(
                            current_conditions->capacity()) *
                            sizeof(std::string));
                    for (const std::string& condition :
                         *current_conditions) {
                        add_owned_bytes(
                            bytes,
                            owned_string_bytes(condition));
                    }
                }
                return bytes;
            };
        for (const auto& [coarse_state, class_ids] :
             refined_classes_by_coarse_parent) {
            const refinement::StableKey& coarse_state_key =
                refined_routing->classes.at(class_ids.front())
                    .coarse_state_key;
            if (coarse_state_key.empty()) {
                gap(
                    "refined coarse parent has no deterministic "
                    "semantic key");
            }
            for (const std::uint32_t class_id : class_ids) {
                if (refined_routing->classes.at(class_id)
                        .coarse_state_key != coarse_state_key) {
                    gap(
                        "refined coarse parent has inconsistent "
                        "semantic keys");
                }
            }
            const auto [same_key, inserted_key] =
                parent_by_stable_key.emplace(
                    coarse_state_key, coarse_state);
            if (!inserted_key &&
                same_key->second != coarse_state) {
                gap(
                    "distinct refined coarse parents share one "
                    "semantic key");
            }

            std::vector<std::string> member_conditions;
            std::optional<bool> retry_basin;
            for (const std::uint32_t class_id : class_ids) {
                const refinement::RefinedPolicyCompileClass& policy_class =
                    refined_routing->classes.at(class_id);
                for (const std::uint32_t member :
                     policy_class.strict_members) {
                    const bool member_retry_basin =
                        calc.state(member).goal_progress_retry_basin != 0;
                    if (retry_basin.has_value() &&
                        *retry_basin != member_retry_basin) {
                        gap(
                            "refined coarse parent mixes ordinary and "
                            "retry-basin control states");
                    }
                    retry_basin = member_retry_basin;
                    const std::string condition =
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(member));
                    if (!member_retry_basin) {
                        const auto [owner, inserted] =
                            globally_routed_member_owner.emplace(
                                condition, coarse_state);
                        if (!inserted &&
                            owner->second != coarse_state) {
                            gap(
                                "overlapping refined coarse-parent "
                                "guards");
                        }
                    }
                    member_conditions.push_back(condition);
                }
            }
            std::sort(
                member_conditions.begin(), member_conditions.end());
            member_conditions.erase(
                std::unique(
                    member_conditions.begin(),
                    member_conditions.end()),
                member_conditions.end());
            if (member_conditions.empty()) {
                gap(
                    "refined coarse parent has no represented strict "
                    "member");
            }
            /*
             * Retry-basin identity is policy memory, not an observable item
             * predicate. It is entered only through the gated reforge's local
             * control-flow edge, so publishing it on the global item router
             * would overlap the ordinary parent with identical item state.
             */
            if (!retry_basin.value_or(false)) {
                refined_parent_condition.emplace(
                    coarse_state, any_of(member_conditions));
                ordered_parents.emplace_back(
                    coarse_state_key, coarse_state);
            }
            observe_compiler_owned(
                parent_build_owned_bytes(&member_conditions));
        }
        std::sort(ordered_parents.begin(), ordered_parents.end());
        std::uint32_t parent_router_index = 0;
        for (const auto& [unused_key, coarse_state] :
             ordered_parents) {
            (void)unused_key;
            refined_parent_order.push_back(coarse_state);
            refined_parent_router.emplace(
                coarse_state,
                "refined_parent_" +
                    std::to_string(parent_router_index++));
        }
        observe_compiler_owned(
            parent_build_owned_bytes(nullptr));

        const auto fixed_option_recipe =
            [&](const std::uint32_t member,
                const std::uint32_t operator_index,
                const PlannerOperator& planner) {
                const OptionKernel& kernel =
                    calc.option_kernel(member, operator_index);
                const std::vector<ObservedUnveilPreference>
                    empty_preferences;
                const std::vector<ObservedUnveilPreference>&
                    preferences =
                        member <
                                result
                                    .option_unveil_preferences
                                    .size()
                            ? result
                                  .option_unveil_preferences[
                                      member]
                            : empty_preferences;
                return fixed_option_executable_recipe_key(
                    fixed_option_executable_recipe(
                        calc, member, planner, kernel,
                        preferences));
            };
        const auto primitive_recipe =
            [&](const std::uint32_t member,
                const PlannerOperator& planner) {
                std::vector<std::uint64_t> recipe{
                    0x70637072696d7231ull}; /* "pcprimr1" */
                if (planner.primitive_action >=
                    calc.registry().actions.size()) {
                    gap(
                        "refined primitive operator is outside the "
                        "strict action registry");
                }
                if (!primitive_observes_modifier_offer(planner)) {
                    return recipe;
                }
                if (member >= result.unveil_preferences.size() ||
                    result.unveil_preferences[member].empty()) {
                    gap(
                        "refined observed-choice member has no populated "
                        "preference sidecar");
                }
                std::set<std::uint32_t> seen_mods;
                for (const std::uint32_t mod :
                     result.unveil_preferences[member]) {
                    if (mod >= session.mod_count ||
                        !seen_mods.insert(mod).second) {
                        gap(
                            "refined observed-choice preference has an invalid "
                            "or repeated modifier");
                    }
                    recipe.push_back(mod);
                }
                return recipe;
            };

        for (const refinement::RefinedPolicyCompileClass& policy_class :
             refined_routing->classes) {
            if (policy_class.terminal) continue;
            if (!policy_class.selected_action.has_value()) {
                gap(
                    "refined non-terminal route has no selected "
                    "action");
            }
            const auto representative =
                refined_class_by_representative.find(
                    policy_class.representative_state);
            if (representative ==
                    refined_class_by_representative.end() ||
                std::find(
                    compiled_states.begin(),
                    compiled_states.end(),
                    policy_class.representative_state) ==
                    compiled_states.end()) {
                gap(
                    "refined working class is absent from the lifted "
                    "policy");
            }
            const refinement::SelectedAction& selected =
                *policy_class.selected_action;
            if (selected.action_id >= calc.operators().size() ||
                selected.semantic_key.empty()) {
                gap(
                    "refined route has an invalid semantic operator "
                    "decision");
            }
            const PlannerOperator& planner =
                calc.operators().at(selected.action_id);
            const std::vector<std::uint64_t> operator_key =
                planner_operator_semantic_key(planner);
            if (operator_key.empty()) {
                gap(
                    "refined route operator has no semantic key");
            }
            refined_operator_key_by_class[policy_class.class_id] =
                operator_key;

            std::optional<std::vector<std::uint64_t>>
                representative_recipe;
            for (const std::uint32_t member :
                 policy_class.strict_members) {
                const PolicyOperatorRef policy =
                    result.policy[member];
                if (policy.index != selected.action_id ||
                    policy.kind != planner.kind ||
                    policy.index >= calc.operators().size() ||
                    planner_operator_semantic_key(
                        calc.operators().at(policy.index)) !=
                        operator_key) {
                    gap(
                        "refined route member operator disagrees with "
                        "the selected strict decision");
                }
                const std::vector<std::uint64_t> recipe =
                    planner.kind ==
                            PlannerOperatorKind::FixedOption
                        ? fixed_option_recipe(
                              member, selected.action_id,
                              planner)
                        : primitive_recipe(member, planner);
                if (!representative_recipe.has_value()) {
                    representative_recipe = recipe;
                } else if (*representative_recipe != recipe) {
                    gap(
                        "refined route members have different "
                        "executable operator recipes");
                }
            }
            refined_recipe_by_class[policy_class.class_id] =
                *representative_recipe;
        }

        const auto same_refined_decision =
            [&](const refinement::RefinedPolicyCompileClass& left,
                const refinement::RefinedPolicyCompileClass& right) {
                return left.selected_action.has_value() &&
                       right.selected_action.has_value() &&
                       left.selected_action->action_id ==
                           right.selected_action->action_id &&
                       left.selected_action->semantic_key ==
                           right.selected_action->semantic_key &&
                       refined_operator_key_by_class[left.class_id] ==
                           refined_operator_key_by_class[
                               right.class_id] &&
                       refined_recipe_by_class[left.class_id] ==
                           refined_recipe_by_class[
                               right.class_id];
            };
        std::map<
            std::pair<std::uint32_t, std::string>,
            std::uint32_t>
            first_class_by_condition;
        for (const refinement::RefinedPolicyCompileClass& policy_class :
             refined_routing->classes) {
            if (policy_class.terminal) continue;
            const std::string& condition =
                refined_condition_by_class[policy_class.class_id];
            const auto [found, inserted] =
                first_class_by_condition.emplace(
                    std::pair{
                        policy_class.coarse_state, condition},
                    policy_class.class_id);
            if (inserted) continue;
            const refinement::RefinedPolicyCompileClass& first_class =
                refined_routing->classes.at(found->second);
            if (!same_refined_decision(
                    first_class, policy_class)) {
                gap(
                    "indistinguishable_refined_policy_actions");
            }
        }

        /*
         * A policy router only decides the next executable operation. Exact
         * classes with different values or projected kernels may therefore
         * overlap when every matching predicate selects the same semantic
         * action: whichever equal-action edge wins, the operation is
         * identical and successor routing is evaluated again on the concrete
         * result. An overlap between different semantic actions is not
         * executable and remains a hard compiler assertion.
         */
        for (const refinement::RefinedPolicyCompileClass& owner :
             refined_routing->classes) {
            if (owner.terminal) continue;
            for (const std::uint32_t member :
                 owner.strict_members) {
                if (member >= result.values.size()) {
                    gap(
                        "refined route member is outside the strict "
                        "state table");
                }
                bool matched_owner_condition = false;
                bool conflicting_decision = false;
                for (const refinement::RefinedPolicyCompileClass&
                         candidate : refined_routing->classes) {
                    if (candidate.terminal ||
                        candidate.coarse_state !=
                            owner.coarse_state) {
                        continue;
                    }
                    const refinement::AbstractFeatureExtraction
                        extraction =
                            refinement::
                                extract_strict_abstract_features(
                                    session,
                                    layout,
                                    calc.state(member),
                                    candidate
                                        .required_observations);
                    if (!extraction.complete()) continue;
                    if (refinement::observe_features(
                            extraction.features,
                            candidate.required_observations) ==
                        candidate.observation_signature) {
                        matched_owner_condition =
                            matched_owner_condition ||
                            refined_condition_by_class[
                                candidate.class_id] ==
                                refined_condition_by_class[
                                    owner.class_id];
                        conflicting_decision =
                            conflicting_decision ||
                            !same_refined_decision(owner, candidate);
                    }
                }
                if (!matched_owner_condition) {
                    gap(
                        "incomplete_refined_policy_observation_signature");
                }
                if (conflicting_decision) {
                    gap(
                        "overlapping_refined_policy_actions");
                }
            }
        }
    }
    QuotientFeatureIndex feature_index;
    if (!structured_refined_route) {
        const std::size_t state_count = result.values.size();
        if (state_count != 0) {
            feature_index.width =
                quotient_feature_values(layout, calc.state(0)).size();
        }
        feature_index.values.reserve(state_count * feature_index.width);
        feature_index.non_goal_buckets.resize(feature_index.width);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.behavioral_representative_by_state.empty()) {
                quotient_members[
                    result.behavioral_representative_by_state[state]]
                    .push_back(state);
            }
            const std::vector<std::uint32_t> values =
                quotient_feature_values(layout, calc.state(state));
            if (values.size() != feature_index.width) {
                gap("quotient feature width changed within one solve");
            }
            feature_index.values.insert(
                feature_index.values.end(), values.begin(), values.end());
            if (!calc.is_goal_state(calc.state(state))) {
                ++feature_index.non_goal_states;
                for (std::size_t feature = 0;
                     feature < values.size(); ++feature) {
                    feature_index.non_goal_buckets[feature][values[feature]]
                        .push_back(state);
                }
            }
        }
    }
    for (const std::uint32_t state_id : compiled_states) {
        if (structured_refined_route) {
            const auto policy_class =
                refined_class_by_representative.find(state_id);
            if (policy_class ==
                    refined_class_by_representative.end() ||
                policy_class->second->terminal) {
                gap(
                    "lifted working state has no refined routing "
                    "class");
            }
            state_conditions.emplace(
                state_id,
                refined_condition_by_class[
                    policy_class->second->class_id]);
        } else if (!result.behavioral_representative_by_state.empty()) {
            state_conditions.emplace(
                state_id,
                quotient_class_condition(
                    calc, vocabulary, state_id,
                    quotient_members.at(state_id), feature_index,
                    result.behavioral_representative_by_state));
        }
    }
    if (structured_refined_route) {
        retained_compile_condition_bytes =
            refined_route_owned_bytes();
        add_owned_bytes(
            retained_compile_condition_bytes,
            static_cast<std::uint64_t>(
                state_conditions.size()) *
                (sizeof(std::uint32_t) +
                 sizeof(std::string) +
                 3 * sizeof(void*)));
        for (const auto& [unused, condition] :
             state_conditions) {
            (void)unused;
            add_owned_bytes(
                retained_compile_condition_bytes,
                owned_string_bytes(condition));
        }
        observe_compiler_owned(
            retained_compile_condition_bytes);
    }
    std::sort(
        compiled_states.begin(), compiled_states.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            if (structured_refined_route ||
                !result.behavioral_representative_by_state.empty()) {
                const std::string& left_condition = state_conditions.at(left);
                const std::string& right_condition = state_conditions.at(right);
                if (left_condition != right_condition) {
                    return left_condition < right_condition;
                }
            } else {
                const std::uint32_t* left_values = feature_index.row(left);
                const std::uint32_t* right_values = feature_index.row(right);
                for (std::size_t feature = 0;
                     feature < feature_index.width; ++feature) {
                    if (left_values[feature] != right_values[feature]) {
                        return left_values[feature] < right_values[feature];
                    }
                }
            }
            if (result.policy[left].index != result.policy[right].index) {
                return result.policy[left].index < result.policy[right].index;
            }
            const std::string left_cost = number(result.values[left]);
            const std::string right_cost = number(result.values[right]);
            if (left_cost != right_cost) return left_cost < right_cost;
            return left < right;
        });
    std::vector<std::uint32_t> canonical_state_ids(
        result.values.size(), kNoId);
    for (std::uint32_t i = 0; i < compiled_states.size(); ++i) {
        canonical_state_ids[compiled_states[i]] = i;
    }
    const auto state_node = [&](const std::uint32_t state_id) {
        if (state_id >= canonical_state_ids.size() ||
            canonical_state_ids[state_id] == kNoId) {
            gap("policy node requested for a non-working state");
        }
        return "s" + std::to_string(canonical_state_ids[state_id]);
    };

    /* Exact policy-region compression. States may share one emitted
     * continuation when the selected program is state-independent. Expected
     * cost is only an annotation, so omit it for a shared region whose member
     * values differ. Observation-owned and state-local retry options remain
     * singleton regions so their concrete routing recipes cannot be
     * conflated. */
    std::map<std::string, std::vector<std::uint32_t>> leaders_by_key;
    std::map<std::uint32_t, std::vector<std::uint32_t>> states_by_leader;
    std::vector<std::uint32_t> emitted_states;
    for (const std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool primitive_observed_choice =
            primitive_observes_modifier_offer(planner);
        const bool product_local_fracture =
            calc.product_solver_parent() &&
            planner.kind == PlannerOperatorKind::Primitive &&
            planner.automatic_kind == AutomaticCandidateKind::Fracture;
        const bool state_local_option =
            planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::TemporaryBenchRepeat ||
             planner.option_kind == FixedOptionKind::FracturePrepare ||
             planner.option_kind == FixedOptionKind::ImprintRetry);
        std::uint32_t leader = state_id;
        if (!primitive_observed_choice && !product_local_fracture &&
            !state_local_option &&
            !gated_primitive_reforge(state_id)) {
            const std::string key =
                std::to_string(result.policy[state_id].index);
            std::vector<std::uint32_t>& leaders = leaders_by_key[key];
            if (leaders.empty()) {
                leaders.push_back(state_id);
                emitted_states.push_back(state_id);
            }
            leader = leaders.back();
        } else {
            emitted_states.push_back(state_id);
        }
        states_by_leader[leader].push_back(state_id);
    }
    std::vector<std::uint32_t> policy_region_by_state(
        result.values.size(), kNoId);
    std::map<std::uint32_t, std::optional<std::string>> region_expected_cost;
    std::uint32_t restart_region_leader = kNoId;
    for (const std::uint32_t leader : emitted_states) {
        const std::vector<std::uint32_t>& members =
            states_by_leader.at(leader);
        for (const std::uint32_t member : members) {
            policy_region_by_state[member] = leader;
        }
        const std::string first_value = number(result.values[members.front()]);
        const bool uniform = std::all_of(
            members.begin() + 1, members.end(),
            [&](const std::uint32_t member) {
                return number(result.values[member]) == first_value;
            });
        region_expected_cost.emplace(
            leader, uniform ? std::optional<std::string>{first_value}
                            : std::nullopt);
        const PlannerOperator& planner =
            calc.operators().at(result.policy[leader]);
        if (planner.kind == PlannerOperatorKind::Primitive &&
            planner.primitive_action < calc.registry().actions.size() &&
            calc.registry().actions[planner.primitive_action].synthetic) {
            if (restart_region_leader != kNoId) {
                gap("policy produced multiple Restart regions");
            }
            restart_region_leader = leader;
        }
    }
    std::map<std::uint32_t, std::uint32_t> gated_retry_state_by_state;
    for (const std::uint32_t state_id : compiled_states) {
        if (!gated_primitive_reforge(state_id)) continue;
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const OutcomeDistribution& distribution = calc.outcomes(
            state_id, planner.primitive_action, true);
        if (!distribution.supported ||
            !distribution.goal_progress_gated) {
            gap("selected gated reforge has no gated exact kernel");
        }
        if (!(distribution.gated_retry_probability > 0.0)) continue;
        const std::uint32_t retry_state =
            distribution.gated_retry_state;
        if (retry_state == kNoId ||
            retry_state >= result.policy_reachable.size() ||
            (!result.behavioral_representative_by_state.empty() &&
             retry_state >=
                 result.behavioral_representative_by_state.size()) ||
            calc.state(retry_state).goal_progress_retry_basin == 0) {
            gap("selected gated reforge retry basin is outside the "
                "executable policy");
        }
        const std::uint32_t retry_policy_state =
            result.behavioral_representative_by_state.empty()
                ? retry_state
                : result.behavioral_representative_by_state[
                      retry_state];
        if (retry_policy_state == kNoId ||
            retry_policy_state >= result.policy_reachable.size() ||
            !result.policy_reachable[retry_policy_state] ||
            result.goal_states[retry_policy_state] ||
            result.policy[retry_policy_state] == kNoId ||
            calc.state(retry_policy_state)
                    .goal_progress_retry_basin == 0) {
            gap("selected gated reforge retry basin is outside the "
                "executable policy");
        }
        gated_retry_state_by_state.emplace(
            state_id, retry_policy_state);
    }
    const auto expected_cost_annotation =
        [&](const std::uint32_t leader) -> std::string {
        const auto found = region_expected_cost.find(leader);
        if (found == region_expected_cost.end() ||
            !found->second.has_value()) {
            return {};
        }
        return ",\"expected_cost\":" + *found->second;
    };

    /* Spell the exact policy domain as a compressed decision tree over the
     * existing v1 condition vocabulary. A region-wide DNF repeats the complete
     * state predicate tens of thousands of times for ordinary reforge
     * policies. This tree checks every abstract feature on every accepted path
     * and defaults to offpolicy at each branch, so sharing changes only the
     * representation, never the set of routed strict states. */
    struct PolicyRouteEntry {
        std::uint32_t state = kNoId;
        std::uint32_t leader = kNoId;
    };
    struct PolicyRouteEdge {
        std::string to;
        std::string condition;
    };
    struct PolicyRouteNode {
        std::string id;
        std::vector<PolicyRouteEdge> edges;
    };
    struct PolicyRouteBranch {
        std::string to;
        std::string guard;
    };
    std::vector<PolicyRouteEntry> policy_route_entries;
    const bool strict_policy_route =
        result.behavioral_representative_by_state.empty();
    if (strict_policy_route) {
        policy_route_entries.reserve(compiled_states.size());
        for (const std::uint32_t state : compiled_states) {
            if (calc.state(state).goal_progress_retry_basin != 0) {
                continue;
            }
            const std::uint32_t leader = policy_region_by_state.at(state);
            if (leader != restart_region_leader) {
                policy_route_entries.push_back({state, leader});
            }
        }
    }
    const bool use_exact_policy_tree =
        strict_policy_route && !policy_route_entries.empty();
    const bool bounded_policy =
        result.policy_status == SolvePolicyStatus::BoundedFeasible ||
        result.policy_status == SolvePolicyStatus::BoundedNearOptimal;
    std::uint32_t bounded_default_restart_action = kNoId;
    if (bounded_policy && restart_region_leader == kNoId) {
        for (std::uint32_t action = 0;
             action < calc.registry().actions.size(); ++action) {
            if (calc.registry().actions[action].synthetic) {
                bounded_default_restart_action = action;
                break;
            }
        }
        if (bounded_default_restart_action == kNoId) {
            gap("bounded policy has no explicit safe Restart default");
        }
    }
    std::uint32_t product_fracture_restart_action = kNoId;
    const bool selected_product_fracture = std::any_of(
        emitted_states.begin(), emitted_states.end(),
        [&](const std::uint32_t state) {
            const PlannerOperator& planner =
                calc.operators().at(result.policy[state]);
            return calc.product_solver_parent() &&
                   planner.kind == PlannerOperatorKind::Primitive &&
                   planner.automatic_kind ==
                       AutomaticCandidateKind::Fracture;
        });
    if (selected_product_fracture) {
        for (std::uint32_t action = 0;
             action < calc.registry().actions.size(); ++action) {
            if (calc.registry().actions[action].synthetic) {
                product_fracture_restart_action = action;
                break;
            }
        }
        if (product_fracture_restart_action == kNoId) {
            gap("product-local Fracture route has no Restart operation");
        }
    }
    const bool dedicated_product_fracture_restart =
        selected_product_fracture && restart_region_leader == kNoId &&
        bounded_default_restart_action == kNoId;
    const std::string product_fracture_restart_node =
        restart_region_leader != kNoId
            ? state_node(restart_region_leader)
            : bounded_default_restart_action != kNoId
                  ? "bounded_default_restart"
                  : "product_fracture_restart";
    const std::string policy_route_default_node =
        strict_policy_route && restart_region_leader != kNoId
            ? state_node(restart_region_leader)
            : bounded_policy ? "bounded_default_restart" : "offpolicy";
    std::vector<std::map<std::uint32_t, std::string>>
        feature_condition_cache(feature_index.width);
    const auto feature_condition =
        [&](const std::uint32_t state,
            const std::size_t feature) -> const std::string& {
        const std::uint32_t value = feature_index.at(state, feature);
        auto& cache = feature_condition_cache.at(feature);
        auto found = cache.find(value);
        if (found != cache.end()) return found->second;
        const std::vector<QuotientFeature> features = quotient_features(
            session, layout, vocabulary, calc.state(state));
        if (features.size() != feature_index.width) {
            gap("policy route feature width changed within one solve");
        }
        for (std::size_t index = 0; index < features.size(); ++index) {
            feature_condition_cache[index].try_emplace(
                static_cast<std::uint32_t>(features[index].value),
                features[index].condition);
        }
        return feature_condition_cache.at(feature).at(value);
    };
    std::vector<PolicyRouteNode> policy_route_nodes;
    std::map<std::string, std::string> policy_route_node_by_signature;
    std::vector<std::size_t> route_features(feature_index.width);
    for (std::size_t feature = 0; feature < route_features.size(); ++feature) {
        route_features[feature] = feature;
    }
    std::function<PolicyRouteBranch(
        const std::vector<PolicyRouteEntry>&,
        const std::vector<std::size_t>&)> build_policy_route;
    build_policy_route =
        [&](const std::vector<PolicyRouteEntry>& entries,
            const std::vector<std::size_t>& features) -> PolicyRouteBranch {
        if (entries.empty()) gap("empty exact policy route partition");
        std::vector<std::string> constants;
        std::vector<std::size_t> varying;
        varying.reserve(features.size());
        for (const std::size_t feature : features) {
            const std::uint32_t first =
                feature_index.at(entries.front().state, feature);
            const bool constant = std::all_of(
                entries.begin() + 1, entries.end(),
                [&](const PolicyRouteEntry& entry) {
                    return feature_index.at(entry.state, feature) == first;
                });
            if (constant) {
                constants.push_back(
                    feature_condition(entries.front().state, feature));
            } else {
                varying.push_back(feature);
            }
        }
        const std::string guard = all_of(constants);
        if (varying.empty()) {
            const std::uint32_t leader = entries.front().leader;
            if (!std::all_of(
                    entries.begin() + 1, entries.end(),
                    [&](const PolicyRouteEntry& entry) {
                        return entry.leader == leader;
                    })) {
                gap("identical exact policy states select different regions");
            }
            return {state_node(leader), guard};
        }

        /* Prefer the widest, then most balanced exact partition. This keeps
         * the router shallow without assigning semantic meaning to hashes or
         * state discovery ids. */
        std::size_t selected = varying.front();
        std::size_t selected_distinct = 0;
        std::uint64_t selected_square_sum =
            std::numeric_limits<std::uint64_t>::max();
        for (const std::size_t feature : varying) {
            std::map<std::uint32_t, std::uint32_t> counts;
            for (const PolicyRouteEntry& entry : entries) {
                ++counts[feature_index.at(entry.state, feature)];
            }
            std::uint64_t square_sum = 0;
            for (const auto& [value, count] : counts) {
                (void)value;
                square_sum += static_cast<std::uint64_t>(count) * count;
            }
            if (counts.size() > selected_distinct ||
                (counts.size() == selected_distinct &&
                 square_sum < selected_square_sum)) {
                selected = feature;
                selected_distinct = counts.size();
                selected_square_sum = square_sum;
            }
        }
        std::vector<std::size_t> child_features;
        child_features.reserve(varying.size() - 1);
        for (const std::size_t feature : varying) {
            if (feature != selected) child_features.push_back(feature);
        }
        std::map<std::uint32_t, std::vector<PolicyRouteEntry>> groups;
        for (const PolicyRouteEntry& entry : entries) {
            groups[feature_index.at(entry.state, selected)].push_back(entry);
        }
        std::vector<PolicyRouteEdge> edges;
        edges.reserve(groups.size());
        for (auto& [value, members] : groups) {
            (void)value;
            const PolicyRouteBranch child =
                build_policy_route(members, child_features);
            edges.push_back({
                child.to,
                all_of({feature_condition(
                            members.front().state, selected),
                        child.guard})});
        }
        std::string signature;
        for (const PolicyRouteEdge& route_edge : edges) {
            signature += std::to_string(route_edge.to.size()) + ":" +
                         route_edge.to + ":" +
                         std::to_string(route_edge.condition.size()) + ":" +
                         route_edge.condition + ";";
        }
        const auto shared = policy_route_node_by_signature.find(signature);
        if (shared != policy_route_node_by_signature.end()) {
            return {shared->second, guard};
        }
        const std::string node_id =
            "policy_route_" + std::to_string(policy_route_nodes.size());
        policy_route_nodes.push_back({node_id, std::move(edges)});
        policy_route_node_by_signature.emplace(std::move(signature), node_id);
        if (policy_route_nodes.size() + 4 >
            result.options.max_compiled_nodes) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_nodes";
            gap("exact policy router exceeded max_compiled_nodes (" +
                std::to_string(result.options.max_compiled_nodes) + ")");
        }
        return {node_id, guard};
    };
    PolicyRouteBranch policy_route_root;
    if (use_exact_policy_tree) {
        policy_route_root =
            build_policy_route(policy_route_entries, route_features);
    }
    std::map<std::uint32_t, OptionKernel> compiled_option_kernels;
    for (const std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        if (planner.kind != PlannerOperatorKind::FixedOption ||
            (planner.option_kind != FixedOptionKind::Renewal &&
             planner.option_kind != FixedOptionKind::ProtectedRepeat &&
             planner.option_kind != FixedOptionKind::TemporaryBenchRepeat &&
             planner.option_kind != FixedOptionKind::FracturePrepare &&
             planner.option_kind != FixedOptionKind::ImprintRetry)) {
            continue;
        }
        const OptionKernel& kernel = calc.option_kernel(
            state_id, result.policy[state_id].index);
        if (!kernel.supported || !kernel.legal) {
            gap("selected S7.4 option has no legal expansion kernel");
        }
        compiled_option_kernels.emplace(state_id, kernel);
    }
    std::uint32_t node_count =
        4 + static_cast<std::uint32_t>(policy_route_nodes.size()) +
        static_cast<std::uint32_t>(
            refined_parent_router.size()) +
        (bounded_default_restart_action != kNoId ? 1u : 0u) +
        (dedicated_product_fracture_restart ? 1u : 0u);
    const auto check_node_cap = [&]() {
        if (node_count > result.options.max_compiled_nodes) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_nodes";
            gap("compiled policy exceeded max_compiled_nodes (" +
                std::to_string(result.options.max_compiled_nodes) + ")");
        }
    };
    check_node_cap();

    const AbstractState& start = calc.state(result.start_state);
    pc_item_state start_item;
    if (result.has_exact_start_item) {
        start_item = result.exact_start_item;
    } else if (!calc.materialize(result.start_state, start_item)) {
        gap("start state cannot be materialized exactly");
    }

    /* --- emit --------------------------------------------------------------- */
    std::string json = "{\"version\":\"v1\",\"name\":\"";
    json += json_escape(name);
    if (result.options.goal_progress_gated_reforges) {
        json +=
            "\",\"description\":\"Exact within the zero-progress-reroll "
            "policy restriction; excluded zero-progress salvage routes are "
            "not globally optimized";
    }
    json += "\",\"base_state\":{\"base_key\":\"";
    json += json_escape(
        data.string_at(data.base_metadata_path_sid[session.base_index]));
    json += "\",\"item_level\":";
    json += std::to_string(session.item_level);
    json += ",\"rarity\":\"";
    json += rarity_name(start.rarity);
    std::uint32_t item_flags = 0;
    if (start.flags & kFlagCorrupted) item_flags |= PC_ITEM_CORRUPTED;
    if (start.flags & kFlagMirrored) item_flags |= PC_ITEM_MIRRORED;
    if (start.flags & kFlagSplit) item_flags |= PC_ITEM_SPLIT;
    if (start.flags & kFlagSynthesised) item_flags |= PC_ITEM_SYNTHESISED;
    json += "\",\"item_flags\":" + std::to_string(item_flags);
    json += ",\"generic_influence_bits\":" +
            std::to_string(start.influence_bits);
    json += ",\"searing_exarch_tier\":" +
            std::to_string(start.searing_exarch_tier);
    json += ",\"eater_of_worlds_tier\":" +
            std::to_string(start.eater_of_worlds_tier);
    const auto start_mods = [&](const char* name, const pc_mod_slot* slots,
                                std::uint8_t count) {
        json += ",\"";
        json += name;
        json += "\":[";
        for (std::uint8_t i = 0; i < count; ++i) {
            if (i > 0) json += ',';
            json += "{\"mod_key\":\"";
            json += json_escape(mod_key_of(session, slots[i].mod_id));
            json += "\"";
            if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) != 0) {
                json += ",\"fractured\":true";
            }
            if ((slots[i].flags & PC_MOD_SLOT_CRAFTED) != 0) {
                json += ",\"crafted\":true";
            }
            if ((slots[i].flags & PC_MOD_SLOT_VEILED) != 0) {
                json += ",\"veiled\":true";
            }
            json += '}';
        }
        json += ']';
    };
    start_mods("prefixes", start_item.prefixes, start_item.prefix_count);
    start_mods("suffixes", start_item.suffixes, start_item.suffix_count);
    json += "},\"start_node_id\":\"start\",\"nodes\":[";
    json += "{\"id\":\"start\",\"kind\":\"start\"},";
    json += "{\"id\":\"router\",\"kind\":\"router\"},";
    json +=
        "{\"id\":\"goal\",\"kind\":\"terminal\",\"terminal\":\"success\"},";
    json += "{\"id\":\"offpolicy\",\"kind\":\"terminal\",\"terminal\":"
            "\"failure\",\"reason\":\"item left the policy-reachable "
            "state set\"}";
    if (bounded_default_restart_action != kNoId) {
        json +=
            ",{\"id\":\"bounded_default_restart\",\"kind\":\"operation\",";
        json += "\"operation\":" + operation_json(
            session,
            calc.registry().actions.at(bounded_default_restart_action));
        json += "}";
    }
    if (dedicated_product_fracture_restart) {
        json +=
            ",{\"id\":\"product_fracture_restart\",\"kind\":\"operation\",";
        json += "\"operation\":" + operation_json(
            session,
            calc.registry().actions.at(product_fracture_restart_action));
        json += "}";
    }
    for (const PolicyRouteNode& route : policy_route_nodes) {
        json += ",{\"id\":\"" + route.id + "\",\"kind\":\"router\"}";
    }
    for (const std::uint32_t coarse_state :
         refined_parent_order) {
        const std::string& router_id =
            refined_parent_router.at(coarse_state);
        json += ",{\"id\":\"" + router_id +
                "\",\"kind\":\"router\"}";
    }
    for (std::uint32_t state_id : emitted_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool observed_choice =
            primitive_observes_modifier_offer(planner);
        const auto gated_retry =
            gated_retry_state_by_state.find(state_id);
        if (gated_retry != gated_retry_state_by_state.end()) {
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_gated_route\",\"kind\":\"router\"}";
            ++node_count;
            check_node_cap();
        }
        if (observed_choice) {
            if (state_id >= result.unveil_preferences.size() ||
                result.unveil_preferences[state_id].empty()) {
                gap("observed-choice state " +
                    std::to_string(state_id) +
                    " has no resolved option preference");
            }
            json += ",{\"id\":\"";
            json += state_node(state_id);
            json += "\",\"kind\":\"router\"}";
            ++node_count;
            check_node_cap();
            for (std::size_t option = 0;
                 option < result.unveil_preferences[state_id].size();
                 ++option) {
                const std::uint32_t mod_id =
                    result.unveil_preferences[state_id][option];
                json += ",{\"id\":\"";
                json += state_node(state_id);
                json += "_u" + std::to_string(option);
                json += "\",\"kind\":\"operation\"";
                json += expected_cost_annotation(state_id);
                ActionDescriptor selected =
                    calc.registry().actions.at(
                        planner.primitive_action);
                selected.params.mod_id = mod_id;
                json += ",\"operation\":" +
                        operation_json(session, selected) + "}";
                ++node_count;
                check_node_cap();
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::ImprintRetry) {
            if (planner.primitive_program.empty()) {
                gap("imprint retry option has no attempt program");
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "\",\"kind\":\"operation\"" +
                    expected_cost_annotation(state_id) +
                    ",\"operation\":{\"type\":\"bestiary:imprint\"}}";
            ++node_count;
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::uint32_t action_index =
                    planner.primitive_program[step];
                json += ",{\"id\":\"" + state_node(state_id) + "_o" +
                        std::to_string(step) +
                        "\",\"kind\":\"operation\",\"operation\":" +
                        operation_json(
                            session,
                            calc.registry().actions.at(action_index)) +
                        accounting_roles_json(planner, action_index) + "}";
                ++node_count;
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_imprint_route\",\"kind\":\"router\"}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_restore\",\"kind\":\"operation\",\"operation\":"
                    "{\"type\":\"bestiary:restore_imprint\"}}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::TemporaryBenchRepeat)) {
            const bool observed =
                option_observes_modifier_offer(planner);
            const std::size_t primitive_steps =
                planner.primitive_program.size() - (observed ? 1u : 0u);
            if (primitive_steps == 0) {
                gap("renewal option has no executable rolling step");
            }
            for (std::size_t step = 0; step < primitive_steps; ++step) {
                const std::uint32_t action_index =
                    planner.primitive_program[step];
                json += ",{\"id\":\"" + state_node(state_id);
                if (step > 0) json += "_o" + std::to_string(step);
                json += "\",\"kind\":\"operation\"";
                if (step == 0) {
                    json += expected_cost_annotation(state_id);
                }
                json += ",\"operation\":" + operation_json(
                    session, calc.registry().actions.at(action_index));
                json += accounting_roles_json(planner, action_index) + "}";
                ++node_count;
                check_node_cap();
            }
            if (observed) {
                const auto& preferences =
                    result.option_unveil_preferences.at(state_id);
                if (preferences.empty()) {
                    gap("observed renewal has no choice preferences");
                }
                json += ",{\"id\":\"" + state_node(state_id) +
                        "_observe\",\"kind\":\"router\"}";
                ++node_count;
                check_node_cap();
                for (std::size_t observation = 0;
                     observation < preferences.size(); ++observation) {
                    json += ",{\"id\":\"" + state_node(state_id) +
                            "_obs" + std::to_string(observation) +
                            "\",\"kind\":\"router\"}";
                    ++node_count;
                    check_node_cap();
                    for (std::size_t choice = 0;
                         choice < preferences[observation].choices.size();
                         ++choice) {
                        json += ",{\"id\":\"" +
                                state_node(state_id) + "_obs" +
                                std::to_string(observation) + "_u" +
                                std::to_string(choice) +
                                "\",\"kind\":\"operation\",\"operation\":";
                        ActionDescriptor selected =
                            calc.registry().actions.at(
                                planner.primitive_program.back());
                        selected.params.mod_id =
                            preferences[observation].choices[choice].mod_id;
                        json += operation_json(session, selected) + "}";
                        ++node_count;
                        check_node_cap();
                    }
                }
            } else {
                json += ",{\"id\":\"" + state_node(state_id) +
                        "_retry\",\"kind\":\"router\"}";
                ++node_count;
                check_node_cap();
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::FracturePrepare) {
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            if (kernel.entry_continues) {
                json += ",{\"id\":\"" + state_node(state_id) +
                        "\",\"kind\":\"operation\"" +
                        expected_cost_annotation(state_id) +
                        ",\"operation\":" + operation_json(
                            session, calc.registry().actions.at(
                                         planner.conditional_action)) +
                        accounting_roles_json(
                            planner, planner.conditional_action, true) + "}";
                ++node_count;
                check_node_cap();
                continue;
            }
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::uint32_t action_index =
                    planner.primitive_program[step];
                json += ",{\"id\":\"" + state_node(state_id);
                if (step > 0) json += "_o" + std::to_string(step);
                json += "\",\"kind\":\"operation\"";
                if (step == 0) {
                    json += expected_cost_annotation(state_id);
                }
                json += ",\"operation\":" + operation_json(
                    session, calc.registry().actions.at(action_index));
                json += accounting_roles_json(planner, action_index) + "}";
                ++node_count;
                check_node_cap();
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture_route\",\"kind\":\"router\"}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture\",\"kind\":\"operation\",\"operation\":" +
                    operation_json(
                        session, calc.registry().actions.at(
                                     planner.conditional_action)) +
                    accounting_roles_json(
                        planner, planner.conditional_action, true) + "}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        const bool product_local_fracture =
            calc.product_solver_parent() &&
            planner.kind == PlannerOperatorKind::Primitive &&
            planner.automatic_kind == AutomaticCandidateKind::Fracture;
        if (product_local_fracture) {
            if (planner.primitive_program.size() != 1) {
                gap("product-local Fracture must be one primitive operation");
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "\",\"kind\":\"operation\"" +
                    expected_cost_annotation(state_id) +
                    ",\"operation\":" +
                    operation_json(
                        session,
                        calc.registry().actions.at(
                            planner.primitive_program.front())) +
                    accounting_roles_json(
                        planner, planner.primitive_program.front()) + "}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture_route\",\"kind\":\"router\"}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        const std::vector<std::uint32_t>& program =
            planner.primitive_program;
        if (program.empty()) {
            gap("planner operator " + planner.id + " has no primitive program");
        }
        for (std::size_t step = 0; step < program.size(); ++step) {
            const std::uint32_t action_index = program[step];
            json += ",{\"id\":\"";
            json += state_node(state_id);
            if (step > 0) json += "_o" + std::to_string(step);
            json += "\",\"kind\":\"operation\"";
            if (step == 0) {
                json += expected_cost_annotation(state_id);
            }
            json += ",\"operation\":";
            json += operation_json(
                session, calc.registry().actions.at(action_index));
            json += accounting_roles_json(planner, action_index);
            json += "}";
            ++node_count;
            check_node_cap();
        }
    }
    json += "],\"edges\":[";
    {
        std::uint64_t owned =
            retained_compile_condition_bytes;
        add_owned_bytes(
            owned, owned_string_bytes(json));
        observe_compiler_owned(owned);
    }

    std::uint32_t edge_counter = 0;
    const auto edge = [&](const std::string& from, const std::string& to,
                          int priority, const std::string& condition,
                          bool is_default,
                          const char* accounting_role = nullptr) {
        if (edge_counter > 0) json += ',';
        json += "{\"id\":\"e";
        json += std::to_string(edge_counter++);
        json += "\",\"from\":\"";
        json += from;
        json += "\",\"to\":\"";
        json += to;
        json += "\",\"priority\":";
        json += std::to_string(priority);
        if (is_default) {
            json += ",\"is_default\":true";
        } else {
            json += ",\"condition\":";
            json += condition;
        }
        if (accounting_role != nullptr) {
            json += ",\"accounting_roles\":[\"";
            json += accounting_role;
            json += "\"]";
        }
        json += "}";
        if (edge_counter > result.options.max_compiled_edges) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_edges";
            gap("compiled policy exceeded max_compiled_edges (" +
                std::to_string(result.options.max_compiled_edges) + ")");
        }
        if (json.size() > strategy_json_limit) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("compiled policy exceeded max_strategy_json_bytes (" +
                std::to_string(strategy_json_limit) +
                ")");
        }
        std::uint64_t owned =
            retained_compile_condition_bytes;
        add_owned_bytes(
            owned, owned_string_bytes(json));
        observe_compiler_owned(owned);
    };

    edge("start", "router", 0, "", true);

    /* Goal first: the configured number of satisfied slots at the finished
     * rarity succeeds regardless of junk. */
    {
        std::vector<std::string> parts{rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            satisfied.push_back(vocabulary[i].satisfied);
        }
        parts.push_back(
            calc.goal().required_satisfied_slots() == satisfied.size()
                ? all_of(satisfied)
                : at_least(calc.goal().required_satisfied_slots(),
                           satisfied));
        edge("router", "goal", 0, all_of(parts), false);
    }

    int root_default_priority = 2;
    if (strict_policy_route) {
        if (use_exact_policy_tree) {
            edge(
                "router", policy_route_root.to, 1,
                policy_route_root.guard, false);
        }
    } else if (structured_refined_route) {
        int parent_priority = 1;
        for (const std::uint32_t coarse_state :
             refined_parent_order) {
            const std::string& router_id =
                refined_parent_router.at(coarse_state);
            edge(
                "router", router_id, parent_priority++,
                refined_parent_condition.at(coarse_state), false);
        }
        root_default_priority = parent_priority;
        for (const std::uint32_t coarse_state :
             refined_parent_order) {
            const std::vector<std::uint32_t>& class_ids =
                refined_classes_by_coarse_parent.at(coarse_state);
            const std::string& router_id =
                refined_parent_router.at(coarse_state);
            std::set<std::pair<std::string, std::string>>
                emitted_routes;
            int route_priority = 0;
            for (const std::uint32_t class_id : class_ids) {
                const refinement::RefinedPolicyCompileClass& policy_class =
                    refined_routing->classes.at(class_id);
                if (policy_class.terminal) continue;
                const std::uint32_t state =
                    policy_class.representative_state;
                const std::uint32_t leader =
                    policy_region_by_state.at(state);
                if (leader == kNoId) {
                    gap(
                        "refined policy class has no emitted policy "
                        "region");
                }
                const std::pair<std::string, std::string> route{
                    state_node(leader),
                    refined_condition_by_class[class_id]};
                if (!emitted_routes.insert(route).second) {
                    continue;
                }
                edge(
                    router_id, route.first, route_priority++,
                    route.second, false);
            }
            edge(
                router_id, policy_route_default_node,
                route_priority, "", true);
        }
    } else {
        std::set<std::pair<std::string, std::string>>
            emitted_refined_routes;
        for (const std::uint32_t leader : emitted_states) {
            for (const std::uint32_t state : states_by_leader.at(leader)) {
                if (calc.state(state).goal_progress_retry_basin != 0) {
                    continue;
                }
                const std::pair<std::string, std::string> route{
                    state_node(leader),
                    state_conditions.at(state)};
                if (structured_refined_route &&
                    !emitted_refined_routes.insert(route).second) {
                    continue;
                }
                edge(
                    "router", route.first, 1,
                    route.second, false);
            }
        }
    }
    edge(
        "router", policy_route_default_node,
        root_default_priority, "", true);
    for (const PolicyRouteNode& route : policy_route_nodes) {
        int priority = 0;
        for (const PolicyRouteEdge& route_edge : route.edges) {
            edge(
                route.id, route_edge.to, priority++,
                route_edge.condition, false);
        }
        edge(route.id, policy_route_default_node, priority, "", true);
    }
    if (bounded_default_restart_action != kNoId) {
        edge("bounded_default_restart", "router", 0, "", true);
    }
    if (dedicated_product_fracture_restart) {
        edge("product_fracture_restart", "router", 0, "", true, "retry");
    }
    for (std::uint32_t state_id : emitted_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool observed_choice =
            primitive_observes_modifier_offer(planner);
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::ImprintRetry) {
            const std::string base = state_node(state_id);
            edge(base, base + "_o0", 0, "", true);
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::string from =
                    base + "_o" + std::to_string(step);
                const std::string to =
                    step + 1 < planner.primitive_program.size()
                        ? base + "_o" + std::to_string(step + 1)
                        : base + "_imprint_route";
                edge(from, to, 0, "", true);
            }
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            const std::string route = base + "_imprint_route";
            for (std::size_t i = 0; i < kernel.retry_states.size(); ++i) {
                edge(
                    route, base + "_restore", static_cast<int>(i),
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(kernel.retry_states[i])),
                    false);
            }
            edge(
                route, "router",
                static_cast<int>(kernel.retry_states.size()), "", true);
            edge(base + "_restore", base, 0, "", true, "retry");
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::TemporaryBenchRepeat)) {
            const bool observed =
                option_observes_modifier_offer(planner);
            const std::size_t primitive_steps =
                planner.primitive_program.size() - (observed ? 1u : 0u);
            for (std::size_t step = 0; step < primitive_steps; ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to;
                if (step + 1 < primitive_steps) {
                    to = state_node(state_id) + "_o" +
                         std::to_string(step + 1);
                } else {
                    to = state_node(state_id) +
                         (observed ? "_observe" : "_retry");
                }
                edge(from, to, 0, "", true);
            }
            if (observed) {
                const OptionKernel& kernel =
                    compiled_option_kernels.at(state_id);
                const auto& preferences =
                    result.option_unveil_preferences.at(state_id);
                const std::string dispatcher =
                    state_node(state_id) + "_observe";
                for (std::size_t observation = 0;
                     observation < preferences.size(); ++observation) {
                    const std::string offer =
                        state_node(state_id) + "_obs" +
                        std::to_string(observation);
                    edge(
                        dispatcher, offer,
                        static_cast<int>(observation),
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(preferences[observation]
                                           .observation_state)),
                        false);
                    for (std::size_t choice = 0;
                         choice < preferences[observation].choices.size();
                         ++choice) {
                        const ObservedUnveilChoice& selected =
                            preferences[observation].choices[choice];
                        const std::string operation =
                            offer + "_u" + std::to_string(choice);
                        edge(
                            offer, operation, static_cast<int>(choice),
                            "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                                json_escape(mod_key_of(
                                    session, selected.mod_id)) +
                                "\"}",
                            false);
                        const bool retry_choice =
                            structured_refined_route
                                ? fixed_option_choice_retries_locally(
                                      state_id, kernel,
                                      selected.successor_state,
                                      selected.actual_state)
                                : fixed_option_choice_retries_locally(
                                      state_id, kernel,
                                      selected.successor_state,
                                      selected.actual_state,
                                      result
                                          .behavioral_representative_by_state);
                        edge(
                            operation,
                            retry_choice
                                ? state_node(state_id)
                                : "router",
                            0, "", true);
                    }
                    edge(
                        offer, "offpolicy",
                        static_cast<int>(
                            preferences[observation].choices.size()),
                        "", true);
                }
                /* No-offer and other expressible stopped paths are exits,
                 * not fabricated option failures. */
                edge(
                    dispatcher, "router",
                    static_cast<int>(preferences.size()), "", true);
            } else {
                const OptionKernel& kernel =
                    compiled_option_kernels.at(state_id);
                const std::string retry =
                    state_node(state_id) + "_retry";
                for (std::size_t i = 0; i < kernel.retry_states.size(); ++i) {
                    edge(
                        retry, state_node(state_id),
                        static_cast<int>(i),
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(kernel.retry_states[i])),
                        false,
                        planner.option_kind == FixedOptionKind::ProtectedRepeat
                            ? "protection_reapplication"
                            : "retry");
                }
                edge(
                    retry, "router",
                    static_cast<int>(kernel.retry_states.size()), "", true);
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::FracturePrepare) {
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            if (kernel.entry_continues) {
                edge(state_node(state_id), "router", 0, "", true);
                continue;
            }
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to =
                    step + 1 < planner.primitive_program.size()
                        ? state_node(state_id) + "_o" +
                              std::to_string(step + 1)
                        : state_node(state_id) +
                              "_fracture_route";
                edge(from, to, 0, "", true);
            }
            const std::string route =
                state_node(state_id) + "_fracture_route";
            int priority = 0;
            for (const std::uint32_t continuation :
                 kernel.continuation_states) {
                edge(
                    route,
                    state_node(state_id) + "_fracture",
                    priority++,
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(continuation)),
                    false);
            }
            for (const std::uint32_t retry_state : kernel.retry_states) {
                edge(
                    route, state_node(state_id), priority++,
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(retry_state)),
                    false, "retry");
            }
            edge(route, "router", priority, "", true);
            edge(
                state_node(state_id) + "_fracture", "router",
                0, "", true);
            continue;
        }
        const bool product_local_fracture =
            calc.product_solver_parent() &&
            planner.kind == PlannerOperatorKind::Primitive &&
            planner.automatic_kind == AutomaticCandidateKind::Fracture;
        if (product_local_fracture) {
            const AbstractState& source = calc.state(state_id);
            std::vector<std::string> acceptable_hits;
            for (std::uint32_t slot = 0;
                 slot < layout.slots.size(); ++slot) {
                const std::uint32_t bit = 1u << slot;
                if ((planner.relevant_goal_mask & bit) != 0 &&
                    source.slot_status[slot] ==
                        static_cast<std::uint8_t>(
                            GoalSlotStatus::Satisfied) &&
                    (source.fractured_goal_mask & bit) == 0) {
                    acceptable_hits.push_back(with_slot_flags(
                        vocabulary[slot].satisfied, true, false));
                }
            }
            if (acceptable_hits.empty()) {
                gap("selected product-local Fracture has no acceptable hit");
            }
            const std::string base = state_node(state_id);
            const std::string route = base + "_fracture_route";
            edge(base, route, 0, "", true);
            edge(route, "router", 0, any_of(acceptable_hits), false);
            edge(
                route, product_fracture_restart_node,
                1, "", true, "retry");
            continue;
        }
        if (!observed_choice) {
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to = "router";
                if (step + 1 < planner.primitive_program.size()) {
                    to = state_node(state_id) + "_o" +
                         std::to_string(step + 1);
                } else if (
                    gated_retry_state_by_state.count(state_id) != 0) {
                    to = state_node(state_id) + "_gated_route";
                }
                edge(from, to, 0, "", true);
            }
            const auto gated_retry =
                gated_retry_state_by_state.find(state_id);
            if (gated_retry != gated_retry_state_by_state.end()) {
                std::vector<std::string> satisfied;
                satisfied.reserve(vocabulary.size());
                for (const SlotVocabulary& slot : vocabulary) {
                    satisfied.push_back(slot.satisfied);
                }
                const std::string zero_progress =
                    satisfied.empty()
                        ? "{\"type\":\"always\"}"
                        : not_of(any_of(satisfied));
                const std::uint32_t retry_leader =
                    policy_region_by_state.at(gated_retry->second);
                if (retry_leader == kNoId) {
                    gap("gated retry basin has no emitted policy region");
                }
                edge(
                    state_node(state_id) + "_gated_route",
                    state_node(retry_leader), 0,
                    zero_progress, false, "retry");
                edge(
                    state_node(state_id) + "_gated_route",
                    "router", 1, "", true);
            }
            continue;
        }
        const auto& preferences = result.unveil_preferences[state_id];
        for (std::size_t option = 0; option < preferences.size(); ++option) {
            const std::string operation =
                state_node(state_id) + "_u" +
                std::to_string(option);
            const std::string condition =
                "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                json_escape(mod_key_of(session, preferences[option])) +
                "\"}";
            edge(state_node(state_id), operation,
                 static_cast<int>(option), condition, false);
            edge(operation, "router", 0, "", true);
        }
        edge(state_node(state_id), "offpolicy",
             static_cast<int>(preferences.size()), "", true);
    }
    json += "]}";
    if (json.size() > strategy_json_limit) {
        if (telemetry != nullptr) telemetry->cap_hit = "max_strategy_json_bytes";
        gap("compiled policy exceeded max_strategy_json_bytes (" +
            std::to_string(strategy_json_limit) + ")");
    }
    {
        std::uint64_t owned =
            retained_compile_condition_bytes;
        add_owned_bytes(
            owned, owned_string_bytes(json));
        observe_compiler_owned(owned);
    }
    if (telemetry != nullptr) {
        telemetry->working_states = static_cast<std::uint32_t>(
            compiled_states.size());
        telemetry->policy_regions = static_cast<std::uint32_t>(
            emitted_states.size());
        telemetry->nodes = node_count;
        telemetry->edges = edge_counter;
        telemetry->strategy_json_bytes = json.size();
    }
    return json;
}

} // namespace solver
} // namespace poecraft
