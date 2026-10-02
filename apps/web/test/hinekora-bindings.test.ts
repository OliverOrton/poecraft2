import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {Worker, type TransferListItem} from "node:worker_threads";
import {EngineClient, type EngineTransport} from "../src/app/engine-client";
import type {ClientMessage, WorkerMessage} from "../src/app/engine-protocol";
import {EditHistory} from "../src/app/edit-history";
import {addCraftSpend, emptyCraftSpend} from "../src/app/craft-costs";

const root = new URL("../../../", import.meta.url);
const lock = JSON.parse(readFileSync(new URL("apps/web/runtime.lock.json", root), "utf8"));
const artifact = new URL(lock.runtime_directory + "/", root);
const text = (name: string) => readFileSync(new URL(name, artifact), "utf8");
const bundle = new TextEncoder().encode(`{"manifest":${text("manifest.json")},"strings":${text("strings.json")},"game_data":${text("game-data.json")}}`);
const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
const transport: EngineTransport = {
    postMessage: (message: ClientMessage, transfer?: Transferable[]) => worker.postMessage(message, (transfer ?? []) as unknown as TransferListItem[]),
    onMessage: handler => worker.on("message", handler), terminate: () => void worker.terminate(),
};
const client = new EngineClient(transport);
const items: number[] = [], contexts: number[] = [];
const physical = (value: unknown) => {const {bestiary, foresight, ...fields} = value as Record<string, unknown>; return fields;};
try {
    await client.whenReady();
    const data = await client.loadData(bundle);
    const session = await client.createSession(data, "Metadata/Items/Amulets/Amulet3", 86);
    const context = await client.createContext(session,17); contexts.push(context);
    const otherSession = await client.createSession(data, "Metadata/Items/Amulets/Amulet3", 86);
    const otherContext = await client.createContext(otherSession,17); contexts.push(otherContext);
    const item = await client.createItem(session,{rarity:"rare"}); items.push(item);
    const veiled = await client.createItem(session,{rarity:"rare"}); items.push(veiled);
    const veilContext = await client.createContext(session,8); contexts.push(veilContext);
    for (const type of ["veiled_chaos","veiled_exalt"] as const)
        await assert.rejects(client.createHinekoraLock(veilContext,veiled,session,{type}),/does not support/);
    assert.equal((await client.apply(veilContext,veiled,{type:"veiled_exalt"})).applied,true);
    const hidden = await client.exportItem(veiled,session);
    await assert.rejects(client.createHinekoraLock(veilContext,veiled,session,{type:"exalt"}),/pending Unveil offers/);
    assert.deepEqual(await client.exportItem(veiled,session),hidden);
    await assert.rejects(client.createHinekoraLock(otherContext,item,otherSession,{type:"exalt"}),/exact session/);
    const locked = await client.createHinekoraLock(context,item,session,{type:"exalt"});
    assert.equal(locked.active,true); assert.deepEqual(locked.cost_keys,["hinekora_lock"]);
    assert.deepEqual((await client.hinekoraInfo(context,item,session)).preview,locked.preview);
    assert.deepEqual((await client.hinekoraInfo(context,item,session)).cost_keys,[]);
    assert.ok(Number((await client.itemInfo(item,session)).item_flags) & 16);
    await assert.rejects(client.createHinekoraLock(context,item,session,{type:"exalt"}),/Modify the item/);
    const refused = await client.apply(context,item,{type:"transmute"});
    assert.equal(refused.applied,false);
    assert.deepEqual((await client.hinekoraInfo(context,item,session)).preview,locked.preview);
    await assert.rejects(client.apply(context,item,{type:"chaos"}),/cross-currency correlations/);
    assert.equal((await client.hinekoraInfo(context,item,session)).active,true);
    const inspector = await client.openCalcInspector(session);
    await assert.rejects(client.currencyCalc(inspector,item,"exalt"),/foresight/);
    await client.closeSolver(inspector);
    const paid = await client.exportItem(item,session);
    const staleSession=await client.createSession(data,"Metadata/Items/Amulets/Amulet3",85);
    const staleContext=await client.createContext(staleSession,17);contexts.push(staleContext);
    await assert.rejects(client.importItem(paid,staleSession,staleContext),/base\/level/);
    const missing=structuredClone(paid) as Record<string,unknown>;delete missing.foresight;
    await assert.rejects(client.importItem(missing,session,context),/lacks its paid Lock checkpoint/);
    assert.equal((await client.hinekoraInfo(context,item,session)).active,true);
    const initialSpend = addCraftSpend(emptyCraftSpend(),locked.cost_keys);
    const history = new EditHistory<{state: unknown; spend: typeof initialSpend}>();
    history.reset({state:paid,spend:initialSpend});
    const next = await client.createContext(session,41); contexts.push(next);
    const bare = await client.importItem(paid,session); items.push(bare);
    await assert.rejects(client.apply(next,bare,{type:"exalt"}),/restored/);
    const restored = await client.importItem(JSON.parse(JSON.stringify(paid)),session,next); items.push(restored);
    assert.deepEqual((await client.hinekoraInfo(next,restored,session)).preview,locked.preview);
    const bad = structuredClone(paid) as Record<string, unknown>; bad.quality=1;
    await assert.rejects(client.importItem(bad,session,next),/item identity mismatch/);
    assert.equal((await client.hinekoraInfo(next,restored,session)).active,true);
    assert.equal((await client.apply(next,restored,{type:"exalt"})).applied,true);
    assert.deepEqual(physical(await client.exportItem(restored,session)),physical(locked.preview));
    assert.equal((await client.hinekoraInfo(next,restored,session)).active,false);
    const finished = await client.exportItem(restored,session);
    history.record({state:finished,spend:addCraftSpend(initialSpend,["exalt"])});
    const reloaded = new EditHistory<{state: unknown; spend: typeof initialSpend}>();
    reloaded.restore(JSON.parse(JSON.stringify(history.export())),history.at(1)!);
    const undone = reloaded.go(0)!;
    const undoContext = await client.createContext(session,73); contexts.push(undoContext);
    const undoItem = await client.importItem(undone.state,session,undoContext); items.push(undoItem);
    assert.deepEqual((await client.hinekoraInfo(undoContext,undoItem,session)).preview,locked.preview);
    assert.deepEqual(undone.spend.counts,{hinekora_lock:1});
    assert.equal((await client.apply(undoContext,undoItem,{type:"exalt"})).applied,true);
    assert.deepEqual(physical(await client.exportItem(undoItem,session)),physical(locked.preview));
    const redone = reloaded.go(1)!;
    const redoContext = await client.createContext(session,74); contexts.push(redoContext);
    const redoItem = await client.importItem(redone.state,session,redoContext); items.push(redoItem);
    assert.equal((await client.hinekoraInfo(redoContext,redoItem,session)).active,false);
    assert.deepEqual(redone.spend.counts,{hinekora_lock:1,exalt:1});
    // Structural six-fracture fixture qualifies consumed-vs-refused semantics;
    // it does not assert that six fractures are a reachable in-game item.
    const frozenContext=await client.createContext(session,7);contexts.push(frozenContext);
    const seedItem=await client.createItem(session,{rarity:"normal"});items.push(seedItem);
    let six: Record<string, unknown> | undefined;
    for(let attempt=0;attempt<100;attempt++) {
        await client.apply(frozenContext,seedItem,{type:"scour"});
        await client.apply(frozenContext,seedItem,{type:"alchemy"});
        const value=await client.exportItem(seedItem,session) as Record<string, unknown>;
        if((value.prefixes as unknown[]).length+(value.suffixes as unknown[]).length===6){six=value;break;}
    }
    assert.ok(six,"Finite structural fixture must fill all affix slots");
    for(const side of ["prefixes","suffixes"]) for(const slot of six[side] as Array<{flags:number}>) slot.flags|=1;
    const frozen=await client.importItem(six,session);items.push(frozen);
    const beforeFrozen=physical(await client.exportItem(frozen,session));
    const identical=await client.createHinekoraLock(frozenContext,frozen,session,{type:"chaos"});
    assert.deepEqual(physical(identical.preview),beforeFrozen);
    assert.equal((await client.apply(frozenContext,frozen,{type:"chaos"})).applied,true);
    assert.equal((await client.hinekoraInfo(frozenContext,frozen,session)).active,false);
    assert.deepEqual(physical(await client.exportItem(frozen,session)),beforeFrozen);
    assert.equal((await client.createHinekoraLock(frozenContext,frozen,session,{type:"chaos"})).active,true);
    await client.apply(frozenContext,frozen,{type:"chaos"});
    const lifetimeContext=await client.createContext(session,93);contexts.push(lifetimeContext);
    const retired=await client.createItem(session,{rarity:"rare"});
    await client.createHinekoraLock(lifetimeContext,retired,session,{type:"exalt"});
    await client.closeItem(retired);
    const fresh=await client.createItem(session,{rarity:"rare"});items.push(fresh);
    assert.equal((await client.createHinekoraLock(lifetimeContext,fresh,session,{type:"exalt"})).active,true,
        "Closing an item cannot transfer its identity or no-refresh record to a fresh item");
    // Closing a context preserves paid information for an explicit later import.
    await client.closeContext(context); contexts.splice(contexts.indexOf(context),1);
    const detached = await client.exportItem(item,session);
    const reopened = await client.createContext(session,85); contexts.push(reopened);
    const reopenedItem = await client.importItem(detached,session,reopened); items.push(reopenedItem);
    assert.deepEqual((await client.hinekoraInfo(reopened,reopenedItem,session)).preview,locked.preview);
    await assert.rejects(client.editItem(reopenedItem,session,{memory_strands:101}),/integer from 0 to 100/);
    assert.equal((await client.hinekoraInfo(reopened,reopenedItem,session)).active,true);
    await client.editItem(reopenedItem,session,{memory_strands:44});
    assert.equal((await client.hinekoraInfo(reopened,reopenedItem,session)).active,false);
    assert.equal((await client.itemInfo(reopenedItem,session)).memory_strands,44);
    console.log("Lock worker: paid preview, exact restore, failed import, Undo/Redo spend, context cleanup and native strand editing passed");
} finally {
    for (const item of items) await client.closeItem(item).catch(()=>{});
    for (const context of contexts) await client.closeContext(context).catch(()=>{});
    await worker.terminate();
}
