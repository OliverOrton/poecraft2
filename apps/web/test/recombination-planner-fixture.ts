// Shared finite Ring input owner. Admission is native and distinct from
// planner/import qualification, so the ownership test needs no feeder or retry graph.
import assert from "node:assert/strict";
import type {EngineClient} from "../src/app/engine-client";
import {EngineError, type ModInfo} from "../src/app/engine-protocol";
export const RING_FIXTURE_BASE = "Metadata/Items/Rings/Ring1";
export async function prepareRingPlannerFixture(client: EngineClient, data: number) {
    const base = RING_FIXTURE_BASE;
    const session = await client.createSession(data, base, 80), context = await client.createContext(session, 62667494);
    const blank = await client.createItem(session, {rarity: "rare", withImplicits: false});
    const pool = await client.debugPool(context, blank, {action: {type: "exalt"}, side: "prefix"});
    // Same finite fixture rule as the native Ring witness: first ordinary,
    // positive-proxy prefixes in session order with full groups compatible.
    // Admission/group checking stays in native editing and pair owners.
    const selected: ModInfo[] = [];
    let rejectedGroupConflicts = 0;
    const full = await client.createItem(session, {rarity: "rare", withImplicits: false});
    for (const candidate of pool.entries.filter(entry => entry.accepted && entry.generation_type === 0 && entry.spawn_weight > 0)
        .sort((a, b) => a.session_mod_id - b.session_mod_id)) {
        const info = await client.modInfo(session, candidate.session_mod_id);
        if (info.reach_kind !== 0) continue;
        const before = await client.exportItem(full, session);
        try {await client.editItem(full, session, {add_explicit: info.key});}
        catch (error) {
            // pc_item_edit_json checks every canonical group and commits only
            // after success. The legacy raw addMod helper cannot admit this fixture.
            if (!(error instanceof EngineError) || !error.detail.includes("conflicting explicit modifier")) throw error;
            assert.deepEqual(await client.exportItem(full, session), before, "group refusal must preserve the complete item");
            ++rejectedGroupConflicts; continue;
        }
        const checked = await client.exportItem(full, session) as {prefixes: Array<{mod_key: string; flags: number}>};
        assert.deepEqual(checked.prefixes.map(slot => slot.mod_key), [...selected.map(mod => mod.key), info.key]);
        assert.ok(checked.prefixes.every(slot => slot.flags === 0));
        selected.push(info); if (selected.length === 2) break;
    }
    assert.equal(selected.length, 2);
    assert.ok(rejectedGroupConflicts > 0, "retain the frozen fixture's adjacent-tier conflict as negative evidence");
    const a = await client.createItem(session, {rarity: "magic", withImplicits: false});
    const b = await client.createItem(session, {rarity: "magic", withImplicits: false});
    await client.editItem(a, session, {add_explicit: selected[0].key});
    await client.editItem(b, session, {add_explicit: selected[1].key});
    const itemA = await client.exportItem(a, session), itemB = await client.exportItem(b, session), itemAB = await client.exportItem(full, session);
    return {base, session, context, blank, full, a, b, selected, itemA, itemB, itemAB};
}
