import type { CatalogEntry } from "./engine-protocol";

interface InfluencePresentation {
    name: string;
    influenceExaltKey?: string;
}

/* RePoE uses internal names for three Conqueror modifier pools. This table is
 * presentation/compatibility metadata only: compiled data supplies the
 * numeric codes, and the native engine remains the authority that accepts or
 * rejects an Influence Exalt operation. */
const PRESENTATION_BY_INTERNAL: Readonly<
    Record<string, InfluencePresentation>
> = {
    adjudicator: { name: "Warlord", influenceExaltKey: "warlord" },
    basilisk: { name: "Hunter", influenceExaltKey: "hunter" },
    crusader: { name: "Crusader", influenceExaltKey: "crusader" },
    elder: { name: "Elder" },
    eyrie: { name: "Redeemer", influenceExaltKey: "redeemer" },
    shaper: { name: "Shaper" },
};

const GENERIC_DISPLAY_ORDER = [
    "shaper",
    "elder",
    "crusader",
    "adjudicator",
    "basilisk",
    "eyrie",
];

const INFLUENCE_EXALT_DISPLAY_ORDER = [
    "crusader",
    "warlord",
    "redeemer",
    "hunter",
];

function titleCase(value: string): string {
    return value
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
}

export function genericInfluenceDisplayName(value: string): string {
    return PRESENTATION_BY_INTERNAL[value.toLowerCase()]?.name ?? titleCase(value);
}

export function canonicalInfluenceExaltKey(value: string): string {
    return (
        PRESENTATION_BY_INTERNAL[value.toLowerCase()]?.influenceExaltKey ??
        value
    );
}

export function buildGenericInfluenceCatalog(
    codesByInternalName: Readonly<Record<string, number>>,
): CatalogEntry[] {
    const remaining = Object.keys(codesByInternalName)
        .filter((key) => key !== "none" && !GENERIC_DISPLAY_ORDER.includes(key))
        .sort();
    return [...GENERIC_DISPLAY_ORDER, ...remaining].flatMap((key) => {
        const code = codesByInternalName[key];
        return Number.isInteger(code) && code > 0
            ? [{ key, code, name: genericInfluenceDisplayName(key) }]
            : [];
    });
}

export function buildInfluenceExaltCatalog(
    genericInfluences: readonly CatalogEntry[],
): CatalogEntry[] {
    const byPublicKey = new Map<string, CatalogEntry>();
    for (const influence of genericInfluences) {
        const publicKey =
            PRESENTATION_BY_INTERNAL[influence.key]?.influenceExaltKey;
        if (!publicKey) continue;
        byPublicKey.set(publicKey, {
            key: publicKey,
            code: influence.code,
            name: genericInfluenceDisplayName(influence.key),
        });
    }
    return INFLUENCE_EXALT_DISPLAY_ORDER.flatMap((key) => {
        const entry = byPublicKey.get(key);
        return entry ? [entry] : [];
    });
}
