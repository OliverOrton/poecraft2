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
