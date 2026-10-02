import type { ClusterBaseCatalog, ClusterConfiguration } from "./engine-protocol";

/** Metadata projection only; all crafting eligibility remains native. */
export function readClusterCatalog(bundle: Uint8Array): Record<string, ClusterBaseCatalog> {
    const payload = JSON.parse(new TextDecoder().decode(bundle));
    const c = payload.game_data.cluster_jewels;
    if (!c) return {};
    const strings = payload.strings.strings as string[];
    const offset = payload.strings.string_id_base ?? 0;
    const text = (sid: number): string => strings[sid - offset] ?? "";
    const tagNames = new Map<number, string>(payload.game_data.tags.global_tag_ids.map((id: number, i: number) => [id, text(payload.game_data.tags.name_string_ids[i])]));
    return Object.fromEntries(c.key_string_ids.map((sid: number, index: number) => [text(sid), {
        minPassiveCount: c.min_skills[index], maxPassiveCount: c.max_skills[index],
        passives: Array.from({length: c.passive_offsets[index + 1] - c.passive_offsets[index]}, (_, j) => {
            const p = c.passive_offsets[index] + j;
            return {key: text(c.passive_key_string_ids[p]), name: text(c.passive_name_string_ids[p]),
                tag: tagNames.get(c.passive_tag_ids[p]) ?? "",
                text: JSON.parse(text(c.passive_stat_text_json_string_ids[p])) as string[]};
        }),
    }]));
}

export function validClusterSelection(catalog: ClusterBaseCatalog, config?: ClusterConfiguration): boolean {
    return Boolean(config && Number.isInteger(config.passiveCount) &&
        config.passiveCount >= catalog.minPassiveCount && config.passiveCount <= catalog.maxPassiveCount &&
        catalog.passives.some(p => p.key === config.passiveKey && !p.tag.startsWith("old_do_not_use_")));
}

/** Present native fixed enchantments separately from explicit/implicit slots. */
export function clusterEnchantmentText(value: unknown): string[] {
    if (!value || typeof value !== "object") return [];
    const c = value as {passive_count?: unknown; jewel_socket_count?: unknown; passive_text?: unknown};
    if (typeof c.passive_count !== "number" || typeof c.jewel_socket_count !== "number") return [];
    return [`Adds ${c.passive_count} Passive Skills`,
        `${c.jewel_socket_count} Added Passive Skills are Jewel Sockets`,
        ...(Array.isArray(c.passive_text) ? c.passive_text.filter((line): line is string => typeof line === "string") : [])];
}
