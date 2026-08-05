#pragma once

#include "solver_compile_contracts.hpp"
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

/* Minimize one executable policy region against every represented non-goal
 * state assigned to another region or to off-policy. `members` is the union
 * of strict states whose behavioral representatives select `target`. */
std::string policy_region_condition(
    const CalcContext& calc,
    const std::vector<SlotVocabulary>& vocabulary,
    const std::uint32_t representative,
    const std::vector<std::uint32_t>& members,
    const QuotientFeatureIndex& feature_index,
    const std::vector<std::uint32_t>& region_by_state,
    std::uint64_t* exact_state_fallbacks = nullptr) {
    const SessionImpl& session = calc.session();
    const AbstractLayout& layout = calc.layout();
    const std::vector<QuotientFeature> representative_features =
        quotient_features(
            session, layout, vocabulary, calc.state(representative));
    const auto exact_fallback = [&]() {
        if (exact_state_fallbacks != nullptr) {
            ++*exact_state_fallbacks;
        }
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

    const std::uint32_t target = region_by_state.at(representative);
    if (target == kNoId) return exact_fallback();
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
        if (region_by_state[state] != target) {
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

} // namespace
} // namespace solver
} // namespace poecraft
