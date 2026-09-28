import type { EngineClient } from "./engine-client";
import type { CraftAction } from "./engine-protocol";

export interface CraftSpend {
    counts: Record<string, number>;
    untracked: boolean;
}

export function emptyCraftSpend(): CraftSpend { return {counts: {}, untracked: false}; }

/** Each history frame carries its cumulative counts, including trimmed older steps. */
export function addCraftSpend(previous: CraftSpend, keys: readonly string[] | undefined): CraftSpend {
    const counts = {...previous.counts};
    for (const key of keys ?? []) counts[key] = (counts[key] ?? 0) + 1;
    return {counts, untracked: previous.untracked || keys === undefined};
}

/** Request-shape adapter only. Consumption vectors come from the native registry. */
export function craftRegistryId(action: CraftAction): string {
    switch (action.type) {
        case "essence": return `essence:${action.essence}`;
        case "fossil": return `fossil:${[...new Set(action.fossils)].sort().join("+")}`;
        case "bench": return `bench:${action.mod_key}`;
        case "harvest_reforge": case "harvest_augment": return `${action.type}:${action.target_tag}`;
        case "harvest_resist": return `harvest_resist:${action.source_tag}:${action.target_tag}`;
        case "eldritch_ember": case "eldritch_ichor": return `${action.type}:${action.tier}`;
        case "influence_exalt": return `influence_exalt:${action.influence}`;
        default: return action.type;
    }
}

/** Reuse the native action descriptors without running a search or simulation. */
export class NativeCraftCosts {
    private session = 0;
    private keys = new Map<string, string[]>();
    async forAction(client: EngineClient, session: number, action: CraftAction): Promise<string[] | undefined> {
        if (this.session !== session) { this.session = session; this.keys.clear(); }
        const id = craftRegistryId(action);
        if (!this.keys.has(id)) {
            // The descriptor API requires a well-formed goal, even for metadata.
            // A session-owned modifier supplies that identity; no odds or search runs.
            const anchor = await client.modInfo(session, 0);
            const registry = await client.openSolver(session, {version: "v1", rarity: "rare",
                slots: [{family_mod_key: anchor.key, min_tier: 0}], actions: [id]});
            try {
                for (const entry of await client.solverActions(registry)) this.keys.set(entry.id, entry.cost_keys);
            } finally { await client.closeSolver(registry); }
        }
        return this.keys.get(id);
    }
}
