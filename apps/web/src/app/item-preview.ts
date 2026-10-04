import type { EngineClient } from "./engine-client";
import type { Catalog } from "./engine-protocol";
import { itemSnapshotCluster, type ItemSnapshot } from "./workspace/persistence";
import type { ConcreteModListModel } from "./components/pc-mod-list";
import { concreteItemFacts } from "./item-card-model";

/** Read a saved resource through its own native session for shared item cards. */
export async function readItemCard(client: EngineClient, data: number, catalog: Catalog | null,
    snapshot: ItemSnapshot, name: string): Promise<ConcreteModListModel> {
    const session = await client.createSession(data, snapshot.base, snapshot.itemLevel, itemSnapshotCluster(snapshot));
    let item = 0;
    try {
        item = await client.importItem(snapshot.state, session);
        const info = await client.itemInfo(item, session);
        const fractured = new Set([...(info.fractured_prefix_mod_ids as number[]), ...(info.fractured_suffix_mod_ids as number[])]);
        const stored = snapshot.state as Record<string, Array<{rolls?: number[]}>>;
        const slots = async (ids: number[], side: string) => Promise.all(ids.map(async (id, index) => {
            const mod = await client.modInfo(session, id);
            return {sessionModId: id, key: mod.key, tierIndex: mod.family_tier_index, textLines: mod.text_lines,
                classificationTags: mod.classification_tags, fractured: fractured.has(id), crafted: mod.reach_kind === 2, veiled: mod.reach_kind === 6,
                rollValues: stored?.[side]?.[index]?.rolls};
        }));
        return {...concreteItemFacts(info, catalog, {baseKey: snapshot.base, baseName: name, itemLevel: snapshot.itemLevel}),
            prefixes: await slots(info.prefix_mod_ids as number[], "prefixes"), suffixes: await slots(info.suffix_mod_ids as number[], "suffixes"),
            enchantments: await slots((info.enchantment_mod_ids as number[]) ?? [], "enchantments"),
            implicits: await slots(info.implicit_mod_ids as number[], "implicits")};
    } finally {
        try {
            if (item) await client.closeItem(item);
        } finally {
            await client.closeSession(session);
        }
    }
}
