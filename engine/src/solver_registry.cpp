#include "solver_internal.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "poecraft/bitset.h"
#include "poecraft/session.h"

/*
 * Solver S1: enumerate the plannable action instances for one session as
 * declarative descriptors. Costs use the simulator economy price-key
 * vocabulary (simulator.cpp compile_operation) so a solved policy prices
 * identically to a hand-built strategy. Legality mirrors the precondition
 * checks in apply_action (actions_basic.cpp), which remains the execution
 * authority.
 */
namespace poecraft {
namespace solver {

namespace {

constexpr std::uint8_t kRarityNormal = 1u << PC_RARITY_NORMAL;
constexpr std::uint8_t kRarityMagic = 1u << PC_RARITY_MAGIC;
constexpr std::uint8_t kRarityRare = 1u << PC_RARITY_RARE;
constexpr std::uint8_t kRarityAny = kRarityNormal | kRarityMagic | kRarityRare;

const std::string& mod_key(const SessionImpl& session, std::uint32_t mod_id) {
    const DataImpl& data = *session.data;
    return data.string_at(data.mod_key_sid[session.global_index[mod_id]]);
}

std::uint32_t tag_id(const SessionImpl& session, const char* name) {
    const auto it = session.data->tag_id_by_name.find(name);
    return it == session.data->tag_id_by_name.end() ? kNoId : it->second;
}

bool session_tag_nonempty(const SessionImpl& session, std::uint32_t tag) {
    if (tag == kNoId || tag >= session.implicit_tag_masks.size()) return false;
    const auto& mask = session.implicit_tag_masks[tag];
    for (std::uint64_t word : mask) {
        if (word != 0) return true;
    }
    return false;
}

void add(ActionRegistry& registry, ActionDescriptor descriptor) {
    std::sort(descriptor.discriminating_tag_ids.begin(),
              descriptor.discriminating_tag_ids.end());
    registry.index_by_id.emplace(
        descriptor.id, static_cast<std::uint32_t>(registry.actions.size()));
    registry.actions.push_back(std::move(descriptor));
}

ActionDescriptor base_descriptor(
    const char* id,
    ActionType type,
    TransitionKind kind,
    std::uint8_t rarity_mask) {
    ActionDescriptor descriptor;
    descriptor.id = id;
    descriptor.display_name = id;
    descriptor.params.type = type;
    descriptor.kind = kind;
    descriptor.cost_keys = {id};
    descriptor.legality.rarity_mask = rarity_mask;
    return descriptor;
}

void add_basic_currency(ActionRegistry& registry) {
    {
        auto d = base_descriptor("transmute", ActionType::Transmute,
                                 TransitionKind::Reforge, kRarityNormal);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("augment", ActionType::Augment,
                                 TransitionKind::SingleSlot, kRarityMagic);
        d.legality.requires_open_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("alteration", ActionType::Alteration,
                                 TransitionKind::Reforge, kRarityMagic);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("regal", ActionType::Regal,
                                 TransitionKind::SingleSlot, kRarityMagic);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("alchemy", ActionType::Alchemy,
                                 TransitionKind::Reforge, kRarityNormal);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("chaos", ActionType::Chaos,
                                 TransitionKind::Reforge, kRarityRare);
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("exalt", ActionType::Exalt,
                                 TransitionKind::SingleSlot, kRarityRare);
        d.legality.requires_open_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("annul", ActionType::Annul,
                                 TransitionKind::SingleSlot,
                                 kRarityMagic | kRarityRare);
        d.legality.requires_removable_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("scour", ActionType::Scour,
                                 TransitionKind::Deterministic,
                                 kRarityMagic | kRarityRare);
        d.clears_flags = kFlagCraftedMod | kFlagVeiledMod;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor(
            "remove_crafted_modifiers", ActionType::RemoveCraftedModifiers,
            TransitionKind::Deterministic, kRarityMagic | kRarityRare);
        d.cost_keys = {"scour"};
        d.legality.required_flags = kFlagCraftedMod;
        d.clears_flags =
            kFlagCraftedMod | kFlagMultimod | kFlagNoAttack |
            kFlagNoCaster | kFlagPrefixesLocked | kFlagSuffixesLocked;
        add(registry, std::move(d));
    }
}

void add_essences(const SessionImpl& session, ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    for (std::uint32_t i = 0; i < data.essence_count; ++i) {
        if (i >= session.essence_guaranteed_mod_ids.size() ||
            session.essence_guaranteed_mod_ids[i] == kNoId) {
            continue;
        }
        const std::int32_t restriction =
            data.essence_item_level_restrictions[i];
        if (restriction >= 0 &&
            session.item_level > static_cast<std::uint32_t>(restriction)) {
            continue;
        }
        const std::string& key = data.string_at(data.essence_key_sids[i]);
        ActionDescriptor d;
        d.id = "essence:" + key;
        d.display_name = d.id;
        d.params.type = ActionType::Essence;
        d.params.essence_index = i;
        d.kind = TransitionKind::Reforge;
        d.cost_keys = {"essence:" + key};
        d.legality.rarity_mask = kRarityAny;
        add(registry, std::move(d));
    }
}

void add_fossils(
    const SessionImpl& session,
    ActionRegistry& registry,
    const ActionRegistryBuildOptions& options) {
    const DataImpl& data = *session.data;
    /* Real socketable fossils carry display names; the nameless
     * RandomFossilOutcome rows are Tangled Fossil internals, not player
     * currency, and must not become plannable actions. */
    std::vector<std::uint32_t> fossils;
    for (std::uint32_t i = 0; i < data.fossil_count; ++i) {
        if (!data.string_at(data.fossil_name_sids[i]).empty()) {
            fossils.push_back(i);
        }
    }

    const auto choose = [](std::size_t n, std::size_t k) {
        if (k > n) return std::size_t{0};
        std::size_t value = 1;
        for (std::size_t i = 1; i <= k; ++i) {
            value = value * (n - (k - i)) / i;
        }
        return value;
    };
    const std::size_t possible =
        choose(fossils.size(), 1) + choose(fossils.size(), 2) +
        choose(fossils.size(), 3) + choose(fossils.size(), 4);
    registry.fossil_loadouts_possible = static_cast<std::uint32_t>(possible);
    registry.fossil_generation_lazy = !options.exhaustive_fossils;

    /* Every emitted 1-4 fossil resonator loadout is a distinct action. In
     * reduced mode only explicitly requested signatures are materialized;
     * the remaining combinations stay diagnostic deferred actions rather
     * than allocating 15k descriptors before layout construction. */
    const auto emit = [&](const std::vector<std::uint32_t>& combo) {
        ActionDescriptor d;
        d.params.type = ActionType::Fossil;
        d.params.fossil_indices = combo;
        d.kind = TransitionKind::Reforge;
        d.legality.rarity_mask = kRarityAny;

        std::vector<std::string> keys;
        std::vector<std::string> names;
        for (std::uint32_t fossil : combo) {
            keys.push_back(data.string_at(data.fossil_key_sids[fossil]));
            names.push_back(data.string_at(data.fossil_name_sids[fossil]));
            const std::uint32_t begin = data.fossil_weight_offsets[fossil];
            const std::uint32_t end = data.fossil_weight_offsets[fossil + 1];
            for (std::uint32_t row = begin; row < end; ++row) {
                d.discriminating_tag_ids.push_back(
                    data.fossil_weight_tag_ids[row]);
            }
        }
        std::sort(keys.begin(), keys.end());
        d.id = "fossil:";
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) d.id += "+";
            d.id += keys[i];
            d.cost_keys.push_back("fossil:" + keys[i]);
        }
        d.cost_keys.push_back("resonator:" + std::to_string(keys.size()));
        d.display_name = names[0];
        for (std::size_t i = 1; i < names.size(); ++i) {
            d.display_name += " + " + names[i];
        }
        std::sort(d.discriminating_tag_ids.begin(),
                  d.discriminating_tag_ids.end());
        d.discriminating_tag_ids.erase(
            std::unique(d.discriminating_tag_ids.begin(),
                        d.discriminating_tag_ids.end()),
            d.discriminating_tag_ids.end());
        add(registry, std::move(d));
        ++registry.fossil_loadouts_generated;
    };

    static_assert(PC_MAX_FOSSILS_PER_ACTION == 4);
    if (!options.exhaustive_fossils) {
        std::unordered_map<std::string, std::uint32_t> fossil_by_key;
        for (std::uint32_t fossil : fossils) {
            fossil_by_key.emplace(
                data.string_at(data.fossil_key_sids[fossil]), fossil);
        }
        std::vector<std::string> requested =
            options.requested_fossil_action_ids;
        std::sort(requested.begin(), requested.end());
        requested.erase(std::unique(requested.begin(), requested.end()),
                        requested.end());
        for (const std::string& id : requested) {
            if (!id.starts_with("fossil:")) continue;
            std::vector<std::uint32_t> combo;
            std::size_t begin = 7;
            while (begin <= id.size()) {
                const std::size_t end = id.find('+', begin);
                const std::string key = id.substr(
                    begin, end == std::string::npos
                               ? std::string::npos
                               : end - begin);
                const auto found = fossil_by_key.find(key);
                if (found == fossil_by_key.end()) break;
                combo.push_back(found->second);
                if (end == std::string::npos) {
                    begin = id.size() + 1;
                    break;
                }
                begin = end + 1;
            }
            if (!combo.empty() && combo.size() <= PC_MAX_FOSSILS_PER_ACTION) {
                std::sort(combo.begin(), combo.end());
                combo.erase(std::unique(combo.begin(), combo.end()),
                            combo.end());
                emit(combo);
            }
        }
        registry.fossil_loadouts_deferred =
            registry.fossil_loadouts_possible -
            registry.fossil_loadouts_generated;
        return;
    }
    const std::size_t count = fossils.size();
    for (std::size_t a = 0; a < count; ++a) {
        emit({fossils[a]});
        for (std::size_t b = a + 1; b < count; ++b) {
            emit({fossils[a], fossils[b]});
            for (std::size_t c = b + 1; c < count; ++c) {
                emit({fossils[a], fossils[b], fossils[c]});
                for (std::size_t e = c + 1; e < count; ++e) {
                    emit({fossils[a], fossils[b], fossils[c], fossils[e]});
                }
            }
        }
    }
    registry.fossil_loadouts_deferred = 0;
}

void add_bench(const SessionImpl& session, ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    const std::uint32_t attack = tag_id(session, "attack");
    const std::uint32_t caster = tag_id(session, "caster");
    for (std::uint32_t mod_id : session.bench_mod_ids) {
        const std::string& key = mod_key(session, mod_id);
        ActionDescriptor d;
        d.id = "bench:" + key;
        d.display_name = d.id;
        d.params.type = ActionType::Bench;
        d.params.mod_id = mod_id;
        d.kind = TransitionKind::Deterministic;
        d.cost_keys = {"bench:" + key};
        d.legality.rarity_mask = kRarityMagic | kRarityRare;
        d.legality.requires_open_affix = true;
        const std::int32_t metamod = session.metamod_type[mod_id];
        if (metamod >= 0 && metamod == data.metamod_multimod_code) {
            d.sets_flags = kFlagMultimod;
        } else if (metamod >= 0 && metamod == data.metamod_no_attack_code) {
            d.sets_flags = kFlagNoAttack;
            /* While active, pool actions treat attack-tagged junk
             * differently, so the tag discriminates. */
            if (attack != kNoId) d.discriminating_tag_ids.push_back(attack);
        } else if (metamod >= 0 && metamod == data.metamod_no_caster_code) {
            d.sets_flags = kFlagNoCaster;
            if (caster != kNoId) d.discriminating_tag_ids.push_back(caster);
        } else if (metamod >= 0 &&
                   metamod == data.metamod_prefixes_locked_code) {
            d.sets_flags = kFlagPrefixesLocked;
        } else if (metamod >= 0 &&
                   metamod == data.metamod_suffixes_locked_code) {
            d.sets_flags = kFlagSuffixesLocked;
        } else {
            d.sets_flags = kFlagCraftedMod;
        }
        add(registry, std::move(d));
    }
}

void add_veiled(const SessionImpl& session, ActionRegistry& registry) {
    const bool veiled_supported =
        session.veiled_prefix_mod_id != kNoId ||
        session.veiled_suffix_mod_id != kNoId;
    if (!veiled_supported) return;
    {
        auto d = base_descriptor("veiled_chaos", ActionType::VeiledChaos,
                                 TransitionKind::Reforge, kRarityRare);
        d.legality.forbidden_flags |= kFlagVeiledMod;
        d.sets_flags = kFlagVeiledMod;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("veiled_exalt", ActionType::VeiledExalt,
                                 TransitionKind::SingleSlot, kRarityRare);
        d.legality.forbidden_flags |= kFlagVeiledMod;
        d.legality.requires_open_affix = true;
        d.sets_flags = kFlagVeiledMod;
        add(registry, std::move(d));
    }
    {
        /* The unveil choice is resolved by the calculation engine's special
         * enumerator (S3); one descriptor stands for "unveil, choosing the
         * option the policy prefers". */
        ActionDescriptor d;
        d.id = "unveil";
        d.display_name = "unveil";
        d.params.type = ActionType::Unveil;
        d.kind = TransitionKind::Special;
        /* Selecting one of the options is the zero-cost resolution step;
         * the preceding veiled currency owns the economic cost. */
        d.cost_keys.clear();
        d.legality.rarity_mask = kRarityMagic | kRarityRare;
        d.legality.required_flags = kFlagVeiledMod;
        d.clears_flags = kFlagVeiledMod;
        add(registry, std::move(d));
    }
}

void add_harvest(const SessionImpl& session, ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    /* One reforge/augment action per classification tag with session
     * members; the engine executes arbitrary tags, the solver plans over
     * the ones that exist here. */
    for (std::uint32_t tag = 0;
         tag < static_cast<std::uint32_t>(session.implicit_tag_masks.size());
         ++tag) {
        if (!session_tag_nonempty(session, tag)) continue;
        const auto name_it = data.tag_name_by_id.find(tag);
        if (name_it == data.tag_name_by_id.end()) continue;
        const std::string& name = name_it->second;
        {
            ActionDescriptor d;
            d.id = "harvest_reforge:" + name;
            d.display_name = d.id;
            d.params.type = ActionType::HarvestReforge;
            d.params.target_tag_id = tag;
            d.kind = TransitionKind::Reforge;
            d.cost_keys = {d.id};
            d.legality.rarity_mask = kRarityRare;
            d.discriminating_tag_ids = {tag};
            add(registry, std::move(d));
        }
        {
            /* Harvest augment is intentionally add-then-remove (ruled by
             * the project owner): the targeted mod is added, then one
             * random other non-fractured mod on an unlocked side is
             * removed. Two-stage, so Special rather than SingleSlot. */
            ActionDescriptor d;
            d.id = "harvest_augment:" + name;
            d.display_name = d.id;
            d.params.type = ActionType::HarvestAugment;
            d.params.target_tag_id = tag;
            d.kind = TransitionKind::Special;
            d.cost_keys = {d.id};
            d.legality.rarity_mask = kRarityMagic | kRarityRare;
            d.legality.requires_open_affix = true;
            d.legality.forbidden_flags |=
                kFlagInfluenced | kFlagEldritchImplicit;
            d.discriminating_tag_ids = {tag};
            add(registry, std::move(d));
        }
    }

    /* Resistance conversion exists only between the three elements
     * (confirmed on the official wiki's Harvest craft list): six ordered
     * fire/cold/lightning pairs, each requiring session mods that carry
     * both the element tag and the resistance tag. */
    const std::uint32_t resistance = tag_id(session, "resistance");
    if (resistance == kNoId || !session_tag_nonempty(session, resistance)) {
        return;
    }
    std::vector<std::uint32_t> resist_tags;
    const auto& resist_mask = session.implicit_tag_masks[resistance];
    for (const char* element : {"fire", "cold", "lightning"}) {
        const std::uint32_t tag = tag_id(session, element);
        if (tag == kNoId || tag >= session.implicit_tag_masks.size()) {
            continue;
        }
        const auto& mask = session.implicit_tag_masks[tag];
        if (mask.empty()) continue;
        bool overlaps = false;
        for (std::size_t w = 0; w < mask.size() && w < resist_mask.size();
             ++w) {
            if (mask[w] & resist_mask[w]) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) resist_tags.push_back(tag);
    }
    for (std::uint32_t source : resist_tags) {
        for (std::uint32_t target : resist_tags) {
            if (source == target) continue;
            const std::string& source_name = data.tag_name_by_id.at(source);
            const std::string& target_name = data.tag_name_by_id.at(target);
            ActionDescriptor d;
            d.id = "harvest_resist:" + source_name + ":" + target_name;
            d.display_name = d.id;
            d.params.type = ActionType::HarvestResist;
            d.params.source_tag_id = source;
            d.params.target_tag_id = target;
            d.kind = TransitionKind::SingleSlot;
            /* Lifeforce depends on the resistance being created, not the
             * resistance being removed. Keep the vocabulary target-specific
             * so economy recipes cannot accidentally price all six actions
             * with one arbitrary lifeforce. */
            d.cost_keys = {"harvest_resist:" + target_name};
            d.legality.rarity_mask = kRarityMagic | kRarityRare;
            d.discriminating_tag_ids = {source, target, resistance};
            add(registry, std::move(d));
        }
    }
}

void add_eldritch(const SessionImpl& session, ActionRegistry& registry) {
    if (!session.eldritch_eligible) return;
    const auto tier_actions = [&](const char* base_id, ActionType type,
                                  const auto& by_tier) {
        for (std::uint32_t tier = 1; tier <= 4; ++tier) {
            if (tier >= by_tier.size() || by_tier[tier].empty()) continue;
            ActionDescriptor d;
            d.id = std::string(base_id) + ":" + std::to_string(tier);
            d.display_name = d.id;
            d.params.type = type;
            d.params.tier = tier;
            d.kind = TransitionKind::SingleSlot;
            d.cost_keys = {d.id};
            d.legality.rarity_mask = kRarityAny;
            d.legality.requires_session_eldritch = true;
            d.legality.forbidden_flags |= kFlagInfluenced;
            d.sets_flags = kFlagEldritchImplicit;
            add(registry, std::move(d));
        }
    };
    tier_actions("eldritch_ember", ActionType::EldritchEmber,
                 session.eldritch_searing_tier_mod_ids);
    tier_actions("eldritch_ichor", ActionType::EldritchIchor,
                 session.eldritch_eater_tier_mod_ids);
    {
        auto d = base_descriptor("eldritch_exalt", ActionType::EldritchExalt,
                                 TransitionKind::SingleSlot, kRarityRare);
        d.legality.requires_session_eldritch = true;
        d.legality.requires_open_affix = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("eldritch_chaos", ActionType::EldritchChaos,
                                 TransitionKind::Reforge, kRarityRare);
        d.legality.requires_session_eldritch = true;
        add(registry, std::move(d));
    }
    {
        auto d = base_descriptor("eldritch_annul", ActionType::EldritchAnnul,
                                 TransitionKind::SingleSlot, kRarityAny);
        d.legality.requires_session_eldritch = true;
        d.legality.requires_removable_affix = true;
        add(registry, std::move(d));
    }
}

void add_influence_exalts(const SessionImpl& session,
                          ActionRegistry& registry) {
    const DataImpl& data = *session.data;
    for (int code = 1;
         code < static_cast<int>(session.influence_masks.size()); ++code) {
        const auto& mask = session.influence_masks[code];
        bool nonempty = false;
        for (std::uint64_t word : mask) {
            if (word != 0) {
                nonempty = true;
                break;
            }
        }
        if (!nonempty) continue;
        if (data.influence_name_by_code.size() <=
            static_cast<std::size_t>(code)) {
            continue;
        }
        const std::string& name = data.influence_name_by_code[code];
        if (name.empty()) continue;
        ActionDescriptor d;
        d.id = "influence_exalt:" + name;
        d.display_name = d.id;
        d.params.type = ActionType::InfluenceExalt;
        d.params.influence_code = code;
        d.kind = TransitionKind::SingleSlot;
        d.cost_keys = {d.id};
        d.legality.rarity_mask = kRarityRare;
        d.legality.requires_open_affix = true;
        d.legality.forbidden_flags |=
            kFlagInfluenced | kFlagEldritchImplicit;
        d.sets_flags = kFlagInfluenced;
        add(registry, std::move(d));
    }
}

void add_structural(ActionRegistry& registry) {
    {
        auto d = base_descriptor("fracture", ActionType::Fracture,
                                 TransitionKind::Special, kRarityRare);
        d.legality.forbidden_flags |=
            kFlagInfluenced | kFlagSynthesised | kFlagFractured;
        d.legality.min_total_affixes = 4;
        d.sets_flags = kFlagFractured;
        add(registry, std::move(d));
    }
    {
        /* Synthetic: buy a fresh base and start over. Always legal — it is
         * how the policy leaves corrupted/mirrored dead ends — and it upper-
         * bounds every state's value (see plan, Problem Formalization). */
        ActionDescriptor d;
        d.id = "restart";
        d.display_name = "restart (fresh base)";
        d.synthetic = true;
        d.kind = TransitionKind::Deterministic;
        d.cost_keys = {"base"};
        d.legality.rarity_mask = kRarityAny;
        d.legality.forbidden_flags = 0;
        add(registry, std::move(d));
    }
}

} // namespace

ActionRegistry build_action_registry(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options) {
    ActionRegistry registry;
    add_basic_currency(registry);
    add_essences(session, registry);
    add_fossils(session, registry, options);
    add_bench(session, registry);
    add_veiled(session, registry);
    add_harvest(session, registry);
    add_eldritch(session, registry);
    add_influence_exalts(session, registry);
    add_structural(registry);
    return registry;
}

} // namespace solver
} // namespace poecraft
