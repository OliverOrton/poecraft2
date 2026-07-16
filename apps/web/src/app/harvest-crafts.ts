import harvestRecipes from "../../../../fixtures/economy/harvest-recipes-v1.json";

import type { CatalogEntry } from "./engine-protocol";

export const HARVEST_REFORGE = 1;
export const HARVEST_AUGMENT = 2;

/**
 * The economy recipe manifest is the owner-approved Harvest craft allowlist.
 * Native CMake generates its allowlist from this same file.
 */
export function buildHarvestCatalog(knownTags: ReadonlySet<string>): CatalogEntry[] {
    const reforge = new Set(Object.keys(harvestRecipes.reforge));
    const augment = new Set(Object.keys(harvestRecipes.augment));
    return Array.from(new Set([...reforge, ...augment]))
        .filter((key) => knownTags.has(key))
        .sort((left, right) => left.localeCompare(right))
        .map((key) => ({
            key,
            name: key.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase()),
            code:
                (reforge.has(key) ? HARVEST_REFORGE : 0) |
                (augment.has(key) ? HARVEST_AUGMENT : 0),
        }));
}

export function harvestTagsFor(
    entries: readonly CatalogEntry[],
    capability: typeof HARVEST_REFORGE | typeof HARVEST_AUGMENT,
): CatalogEntry[] {
    return entries.filter((entry) => ((entry.code ?? 0) & capability) !== 0);
}
