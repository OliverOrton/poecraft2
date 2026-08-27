/*
 * Session mods → display-family picker options, shared by the Strategy
 * Builder's condition editor and the Calculator's goal editor. The
 * representative T1 mod key is the stable persisted family identity, and each
 * option carries its complete T1..Tn threshold list.
 */

import { Catalog, ModInfo } from "./engine-protocol";
import { visibleModTags } from "./item-display";
import {
    genericInfluenceDisplayName,
    genericInfluenceDisplayRank,
} from "./influence-presentation";

export interface ModifierTierOption {
    tier: number;
    label: string;
    requiredLevel: number;
}

export interface ModifierFamilyOption {
    value: string;
    label: string;
    side: "prefix" | "suffix";
    sourceKind: "base" | "influence" | "crafted" | "essence" | "fossil";
    sourceLabel: string;
    tags: string[];
    tiers: ModifierTierOption[];
}

const REACH_INFLUENCE = 1;
const REACH_CRAFTED = 2;
const REACH_ESSENCE = 3;
const REACH_FOSSIL = 5;

function modSourceLabel(mod: ModInfo): string {
    switch (mod.reach_kind) {
        case REACH_INFLUENCE: {
            const parts = mod.reach_via.split(":");
            const key = (parts[parts.length - 1] || "influenced").toLowerCase();
            return genericInfluenceDisplayName(key);
        }
        case REACH_CRAFTED:
            return "Bench";
        case REACH_ESSENCE:
            return "Essence";
        case REACH_FOSSIL:
            return "Fossil";
        default:
            return "";
    }
}

function modSourceKind(
    mod: ModInfo,
): ModifierFamilyOption["sourceKind"] {
    switch (mod.reach_kind) {
        case REACH_INFLUENCE:
            return "influence";
        case REACH_CRAFTED:
            return "crafted";
        case REACH_ESSENCE:
            return "essence";
        case REACH_FOSSIL:
            return "fossil";
        default:
            return "base";
    }
}

export function titleCase(value: string): string {
    return value
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
}

/** Group explicit session mods into display families; each group is sorted by
 * tier so index 0 is the representative (best-tier) mod. */
function groupFamilies(mods: ModInfo[]): ModInfo[][] {
    const families = new Map<string, ModInfo[]>();
    for (const mod of mods) {
        if (mod.generation_type !== 0 && mod.generation_type !== 1) continue;
        const familyId = Number.isFinite(mod.family_id)
            ? mod.family_id
            : mod.primary_group_id;
        const influence =
            mod.reach_kind === REACH_INFLUENCE ? `:${mod.reach_influence}` : "";
        const key = `${mod.reach_kind}${influence}:${mod.generation_type}:${familyId}`;
        const slot = families.get(key);
        if (slot) slot.push(mod);
        else families.set(key, [mod]);
    }
    const out = Array.from(families.values());
    for (const tiers of out) {
        tiers.sort((a, b) => a.family_tier_index - b.family_tier_index);
    }
    return out;
}

/** Map every explicit mod key to its family's representative (persisted) key,
 * so a click on any tier resolves to the same family identity the options
 * from buildModifierOptions use. */
export function buildModifierKeyIndex(mods: ModInfo[]): Map<string, string> {
    const index = new Map<string, string>();
    for (const tiers of groupFamilies(mods)) {
        const rep = tiers[0];
        for (const tier of tiers) {
            index.set(tier.key, rep.key);
        }
    }
    return index;
}

/**
 * Group session mods into the same display families as the Emulator. The
 * representative T1 mod key is the stable persisted family identity, and each
 * option carries its complete T1..Tn threshold list.
 */
export function buildModifierOptions(
    mods: ModInfo[],
    catalog: Catalog,
): ModifierFamilyOption[] {
    const options: ModifierFamilyOption[] = [];
    for (const tiers of groupFamilies(mods)) {
        const rep = tiers[0];
        const text =
            rep.text_lines.join(" / ") ||
            catalog.groupNameById[rep.primary_group_id] ||
            rep.key;
        const side = rep.generation_type === 0 ? "P" : "S";
        const source = modSourceLabel(rep);
        options.push({
            value: rep.key,
            label: text,
            side: side === "P" ? "prefix" : "suffix",
            sourceKind: modSourceKind(rep),
            sourceLabel: source,
            tags: visibleModTags(
                tiers.flatMap((tier) => tier.classification_tags),
            ).sort(),
            tiers: tiers.map((tier) => ({
                tier: tier.family_tier_index,
                label: tier.text_lines.join(" / ") || tier.key,
                requiredLevel: tier.required_level,
            })),
        });
    }
    options.sort(
        (a, b) =>
            sourceOrder(a) - sourceOrder(b) ||
            influenceOptionOrder(a) - influenceOptionOrder(b) ||
            a.label.localeCompare(b.label),
    );
    return options;
}

function sourceOrder(option: ModifierFamilyOption): number {
    return ["base", "influence", "crafted", "essence", "fossil"].indexOf(
        option.sourceKind,
    );
}

function influenceOptionOrder(option: ModifierFamilyOption): number {
    if (option.sourceKind !== "influence") return 0;
    return genericInfluenceDisplayRank(option.sourceLabel);
}
