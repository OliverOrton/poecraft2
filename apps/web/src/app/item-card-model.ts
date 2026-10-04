import type { Catalog } from "./engine-protocol";
import type { ConcreteModListModel } from "./components/pc-mod-list";
import { clusterEnchantmentText } from "./cluster-configuration";
import { influenceLabels } from "./item-display";

/** Native facts shared by live views and imported previews. No session or mutation ownership. */
export function concreteItemFacts(info: Record<string, unknown>, catalog: Catalog | null,
    identity: {baseKey: string; baseName: string; itemLevel: number}): Pick<ConcreteModListModel,
        "kind" | "baseKey" | "baseName" | "itemLevel" | "rarity" | "itemFlags" | "memoryStrands" |
        "lifecycle" | "influences" | "clusterEnchantmentText" | "maxPrefix" | "maxSuffix"> {
    return {
        kind: "concrete", ...identity,
        rarity: String(info.rarity),
        itemFlags: Number(info.item_flags ?? 0),
        memoryStrands: Number(info.memory_strands ?? 0),
        lifecycle: Number(info.lifecycle ?? 0),
        influences: influenceLabels(Number(info.generic_influence_bits), Number(info.searing_exarch_tier ?? 0),
            Number(info.eater_of_worlds_tier ?? 0), catalog),
        clusterEnchantmentText: clusterEnchantmentText(info.cluster),
        maxPrefix: (info.max_prefix as number) ?? (info.prefix_mod_ids as number[]).length,
        maxSuffix: (info.max_suffix as number) ?? (info.suffix_mod_ids as number[]).length,
    };
}
