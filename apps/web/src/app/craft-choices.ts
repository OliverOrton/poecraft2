import type { Catalog } from "./engine-protocol";
import { HARVEST_AUGMENT, HARVEST_REFORGE, harvestTagsFor } from "./harvest-crafts";

/*
 * Shared craft-panel option helpers: the Emulator's craft bar and the
 * Calculator's action selector present the same essence/harvest/resistance
 * choices, so the grouping and labelling live here.
 */

export function craftActionLabel(type: string): string {
    return type
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
}

export function resistanceEntries(): Array<{ key: string; name: string }> {
    return ["fire", "cold", "lightning"].map((key) => ({
        key,
        name: craftActionLabel(key),
    }));
}

/** Normalize presentation selections once; controllers never read a hidden/select DOM value. */
export function resolveCraftValues(catalog: Catalog, input: ReadonlyMap<string, string>): Map<string, string> {
    const values = new Map(input);
    const choose = (name: string, keys: string[], fallback = keys[0] ?? "") => {
        if (!keys.includes(values.get(name) ?? "")) values.set(name, fallback);
    };
    const groups = groupEssences(catalog.essences);
    if (!values.has("essence-type")) {
        const storedGroup = groups.find(group => group.tiers.some(tier => tier.key === values.get("essence-key")));
        if (storedGroup) values.set("essence-type", storedGroup.type);
    }
    choose("essence-type", groups.map(group => group.type));
    const group = groups.find(group => group.type === values.get("essence-type"));
    choose("essence-key", group?.tiers.map(tier => tier.key) ?? []);
    choose("influence", catalog.influences.map(entry => entry.key));
    choose("harvest-reforge-tag", harvestTagsFor(catalog.harvestTags, HARVEST_REFORGE).map(entry => entry.key));
    choose("harvest-augment-tag", harvestTagsFor(catalog.harvestTags, HARVEST_AUGMENT).map(entry => entry.key));
    choose("resist-from", ["fire", "cold", "lightning"], "fire");
    choose("resist-to", ["fire", "cold", "lightning"], "cold");
    choose("eldritch-tier", ["1", "2", "3", "4"], "1");
    return values;
}

/** Restore the selected material alongside a Calculator's persisted registry action. */
export function craftValuesFromAction(catalog: Catalog, action: string): Map<string, string> {
    const [kind, ...parameters] = action.split(":");
    const fields: Record<string, string[]> = {
        essence: ["essence-key"], influence_exalt: ["influence"],
        harvest_reforge: ["harvest-reforge-tag"], harvest_augment: ["harvest-augment-tag"],
        harvest_resist: ["resist-from", "resist-to"],
        eldritch_ember: ["eldritch-tier"], eldritch_ichor: ["eldritch-tier"],
    };
    return resolveCraftValues(catalog, new Map((fields[kind] ?? []).map((field, index) => [field, parameters[index] ?? ""])));
}

export interface EssenceTierChoice {
    key: string;
    tier: string;
    rank: number;
}

export interface EssenceGroup {
    type: string;
    tiers: EssenceTierChoice[];
}

const ESSENCE_TIER_RANK: Record<string, number> = {
    Whispering: 1,
    Muttering: 2,
    Weeping: 3,
    Wailing: 4,
    Screaming: 5,
    Shrieking: 6,
    Deafening: 7,
};

export function groupEssences(
    entries: Array<{ key: string; name: string }>,
): EssenceGroup[] {
    const groups = new Map<string, EssenceTierChoice[]>();
    for (const entry of entries) {
        const tiered = entry.name.match(
            /^(Whispering|Muttering|Weeping|Wailing|Screaming|Shrieking|Deafening) Essence of (.+)$/,
        );
        const special = entry.name.match(/^Essence of (.+)$/);
        if (!tiered && !special) continue;
        const type = tiered?.[2] ?? special?.[1] ?? "";
        if (!type) continue;
        const tier = tiered?.[1] ?? "Special";
        groups.set(type, [
            ...(groups.get(type) ?? []),
            {
                key: entry.key,
                tier,
                rank: ESSENCE_TIER_RANK[tier] ?? 1,
            },
        ]);
    }
    return Array.from(groups, ([type, tiers]) => ({
        type,
        tiers: tiers.sort((a, b) => b.rank - a.rank),
    })).sort((a, b) => a.type.localeCompare(b.type));
}
