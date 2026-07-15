import { Catalog } from "./engine-protocol";

/**
 * RePoE implicit tags include both broad labels players recognize from advanced
 * modifier descriptions and narrow implementation tags such as
 * `flat_life_regen` or `caster_damage`. Keep the complete set in the engine;
 * concrete UI surfaces only show this conservative player-facing subset.
 */
const PLAYER_FACING_MOD_TAGS = new Set([
    "ailment",
    "attack",
    "attribute",
    "aura",
    "caster",
    "chaos",
    "cold",
    "critical",
    "curse",
    "defences",
    "elemental",
    "fire",
    "flask",
    "gem",
    "life",
    "lightning",
    "mana",
    "minion",
    "physical",
    "resistance",
    "speed",
]);

export function visibleModTags(tags: readonly string[]): string[] {
    const visible: string[] = [];
    const seen = new Set<string>();
    for (const tag of tags) {
        const normalized = tag.toLowerCase();
        if (!PLAYER_FACING_MOD_TAGS.has(normalized) || seen.has(normalized)) {
            continue;
        }
        seen.add(normalized);
        visible.push(normalized);
    }
    return visible;
}

export interface StableSlotLayout<T, TId = number> {
    slots: Array<T | undefined>;
    ids: Array<TId | undefined>;
}

/**
 * Retain surviving modifiers in their previous visual slots. New modifiers
 * fill the first available positions, so removing P1 does not make P2 jump.
 */
export function placeStableSlots<T, TId = number>(
    mods: readonly T[],
    capacity: number,
    previousIds: readonly (TId | undefined)[],
    idOf: (mod: T) => TId,
): StableSlotLayout<T, TId> {
    if (capacity <= 0) {
        return { slots: [], ids: [] };
    }

    const remaining = new Map(mods.map((mod) => [idOf(mod), mod]));
    const slots = new Array<T | undefined>(capacity).fill(undefined);

    for (let index = 0; index < capacity; index += 1) {
        const previousId = previousIds[index];
        if (previousId === undefined) continue;
        const mod = remaining.get(previousId);
        if (mod === undefined) continue;
        slots[index] = mod;
        remaining.delete(previousId);
    }

    for (const mod of mods) {
        const id = idOf(mod);
        if (!remaining.has(id)) continue;
        const openIndex = slots.findIndex((slot) => slot === undefined);
        if (openIndex < 0) break;
        slots[openIndex] = mod;
        remaining.delete(id);
    }

    return {
        slots,
        ids: slots.map((mod) => (mod === undefined ? undefined : idOf(mod))),
    };
}

/** Engine/catalog-backed influence labels shared by concrete item views. */
export function influenceLabels(
    genericBits: number,
    searingExarchTier: number,
    eaterOfWorldsTier: number,
    catalog: Catalog | null,
): string[] {
    const labels: string[] = [];
    for (const influence of catalog?.influences ?? []) {
        const code = influence.code ?? 0;
        if (code > 0 && (genericBits & (1 << (code - 1))) !== 0) {
            labels.push(influence.name);
        }
    }
    if (searingExarchTier > 0) {
        labels.push(`Searing Exarch T${searingExarchTier}`);
    }
    if (eaterOfWorldsTier > 0) {
        labels.push(`Eater of Worlds T${eaterOfWorldsTier}`);
    }
    return labels;
}
