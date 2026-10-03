import assert from "node:assert/strict";
import { build } from "esbuild";
import { chromium } from "playwright";

const bundle = await build({entryPoints: ["src/app/workspace/persistence.ts"], bundle: true, write: false, format: "iife", globalName: "storage"});
// Optional local qualification with installed Chrome; CI keeps pinned Chromium.
const browserChannel = process.env.POECRAFT_TEST_BROWSER_CHANNEL;
assert.ok(browserChannel === undefined || browserChannel === "chrome",
    "POECRAFT_TEST_BROWSER_CHANNEL must be unset or chrome");
// launch() uses a fresh temporary profile; no persistent/user profile is supplied.
const browser = await chromium.launch({
    headless: true,
    ...(browserChannel ? {channel: browserChannel} : {}),
});
try {
    const page = await browser.newPage();
    await page.route("https://workspace.test/", route => route.fulfill({contentType: "text/html", body: "<title>Storage contract test</title>"}));
    await page.goto("https://workspace.test/");
    await page.addScriptTag({content: bundle.outputFiles[0].text});
    const result = await page.evaluate(async () => {
        const p = (window as unknown as {storage: any}).storage;
        const a = {id: "donor", name: "Donor", base: "a", itemLevel: 86, createdAt: 1, state: {lifecycle: 0, memory_strands: 20}};
        const b = {id: "receiver", name: "Receiver", base: "b", itemLevel: 1, createdAt: 1, state: {lifecycle: 0}};
        const consumed = {...a, state: {...a.state, lifecycle: 1}};
        const changed = {...b, state: {lifecycle: 0, mods: ["retained"]}};
        const beforeDraft = {docId: "work", state: b.state, spend: 0};
        const afterDraft = {docId: "work", state: changed.state, spend: 7};
        await p.putStash(a); await p.putStash(b); await p.putDraft(beforeDraft);
        await p.commitWorkspaceResources([a,b], [consumed,changed], afterDraft);
        const after = [await p.getStash("donor"), await p.getStash("receiver"), await p.getDraft("work")];
        let staleSave = false;
        try { await p.putStash(a); } catch { staleSave = true; }
        await p.commitWorkspaceResources([consumed,changed], [a,b], beforeDraft);
        const undo = [await p.getStash("donor"), await p.getStash("receiver"), await p.getDraft("work")];
        await p.commitWorkspaceResources([a,b], [consumed,changed], afterDraft);
        const redo = [await p.getStash("donor"), await p.getStash("receiver"), await p.getDraft("work")];
        await p.putStash({...changed, name: "External edit"});
        let staleUndo = false;
        try { await p.commitWorkspaceResources([consumed,changed], [a,b], beforeDraft); } catch { staleUndo = true; }
        const failed = [await p.getStash("donor"), await p.getStash("receiver"), await p.getDraft("work")];
        let alias = false;
        try { await p.commitWorkspaceResources([consumed,consumed], [a,a], beforeDraft); } catch { alias = true; }
        const goalList = {version:"calculator_goal_list_v1",activeGoalId:"life",goals:[
            {id:"clean",name:"Magic clean",goalRarity:"magic",slots:[],allowExtraModifiers:false},
            {id:"life",name:"Life coverage",goalRarity:"rare",slots:[{group:"life",minTier:2}],minSatisfiedSlots:1,
                allowExtraModifiers:true,goalImplicitKeys:["implicit"],goalInfluenceBits:2,goalCorrupted:false}]};
        await p.putCalculatorDraft({docId:"calculator",base:"base",itemLevel:86,state:b.state,
            goalRarity:"rare",slots:[{group:"life",minTier:2}],goalList,actionId:"exalt",fossilKeys:[],updatedAt:1});
        goalList.goals[0].name="Changed after saving";
        const calculator = await p.getCalculatorDraft("calculator");
        return {a,b,after,undo,redo,failed,staleSave,staleUndo,alias,calculator};
    });
    assert.equal(result.after[0].state.lifecycle, 1);
    assert.equal(result.after[0].state.memory_strands, 20);
    assert.equal(result.after[2].spend, 7);
    assert.deepEqual(result.undo.slice(0,2), [result.a, result.b]);
    assert.equal(result.undo[2].spend, 0);
    assert.deepEqual(result.after, result.redo);
    assert.equal(result.failed[0].state.lifecycle, 1);
    assert.equal(result.failed[1].name, "External edit");
    assert.equal(result.failed[2].spend, 7);
    assert.ok(result.staleSave && result.staleUndo && result.alias);
    assert.equal(result.calculator.goalList.goals[0].name,"Magic clean");
    assert.equal(result.calculator.goalList.activeGoalId,"life");
    await page.reload();
    await page.addScriptTag({content: bundle.outputFiles[0].text});
    const reloadedCalculator=await page.evaluate(async()=> (window as unknown as {storage:any}).storage.getCalculatorDraft("calculator"));
    assert.deepEqual(reloadedCalculator,result.calculator,"Complete goal-list state survives an actual IndexedDB reload");
    await page.evaluate(async()=> (window as unknown as {storage:any}).storage.deleteDraft("calculator"));
    assert.equal(await page.evaluate(async()=> (window as unknown as {storage:any}).storage.getCalculatorDraft("calculator")),undefined);
    console.log("Calculator IndexedDB: versioned goal-list payload, ordering, names, selection, reload and deletion passed");
    console.log("Workspace resources: atomic apply, Undo/Redo, memory, spend and stale/alias rejection passed");
} finally { await browser.close(); }
