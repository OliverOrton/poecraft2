import type { BaseInfo } from "./engine-protocol";

function hasKnownDropLevel(base: BaseInfo): boolean {
    return Number.isFinite(base.drop_level) && base.drop_level >= 0;
}

/**
 * Canonical display order inside a base-picker class/subcategory. The engine
 * exposes unknown levels as a negative sentinel, which stays explicit and is
 * ordered after every known level.
 */
export function compareBasePickerBases(a: BaseInfo, b: BaseInfo): number {
    const aKnown = hasKnownDropLevel(a);
    const bKnown = hasKnownDropLevel(b);
    if (aKnown !== bKnown) return aKnown ? -1 : 1;
    if (aKnown && a.drop_level !== b.drop_level) {
        return b.drop_level - a.drop_level;
    }
    return a.name.localeCompare(b.name) || a.path.localeCompare(b.path);
}

/** Shared input preparation for every consumer of pc-base-picker. */
export function supportedBasePickerBases(bases: BaseInfo[]): BaseInfo[] {
    return bases
        .filter((base) => base.support === 0 && base.name)
        .sort(compareBasePickerBases);
}
