// Run only against the source-matched rebuilt release WASM, through the worker.
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {Worker,type TransferListItem} from "node:worker_threads";
import {buildModifierKeyIndex} from "../src/app/modifier-options";
import {EngineClient,type EngineTransport} from "../src/app/engine-client";
import type {CalculatorGoalSet,CalculatorItemGoal,ClientMessage,WorkerMessage} from "../src/app/engine-protocol";
const root=new URL("../../../",import.meta.url);
const lock=JSON.parse(readFileSync(new URL("apps/web/runtime.lock.json",root),"utf8"));
const dataDir=new URL(`${lock.runtime_directory}/`,root);
const payload=(name:string)=>readFileSync(new URL(name,dataDir),"utf8");
const bundle=new TextEncoder().encode(`{"manifest":${payload("manifest.json")},"strings":${payload("strings.json")},"game_data":${payload("game-data.json")}}`);
const worker=new Worker(new URL("./worker-bootstrap.mjs",import.meta.url));
const transport:EngineTransport={postMessage:(message:ClientMessage,transfer?:Transferable[])=>worker.postMessage(message,(transfer??[]) as unknown as TransferListItem[]),
    onMessage:(handler:(message:WorkerMessage)=>void)=>worker.on("message",handler),terminate:()=>void worker.terminate()};
const client=new EngineClient(transport);let session=0,item=0;
try {
    await client.whenReady();const data=await client.loadData(bundle);
    // Real native modifier/session identity for the generic picker regression.
    for(const fixture of [
        {base:"Metadata/Items/Armours/Shields/ShieldInt12",key:"GainRandomChargeOnBlockInfluence1",influence:2},
        {base:"Metadata/Items/Armours/Shields/ShieldInt12",key:undefined,influence:1},
        {base:"Metadata/Items/Armours/BodyArmours/BodyInt17",key:"EnergyShieldRecoveryRateBodyInfluence2",influence:3}]){
        const frame=await client.createSession(data,fixture.base,86);let rootItem=0,admitted=0;
        try{
            const count=await client.modCount(frame);
            const mods=await Promise.all(Array.from({length:count},(_,id)=>client.modInfo(frame,id)));
            const mod=fixture.key ? mods.find(mod=>mod.key===fixture.key) : mods.find(mod=>mod.reach_kind===1 && mod.reach_influence===fixture.influence && mod.family_tier_index>0);
            assert.ok(mod,fixture.key ?? ("Influence "+fixture.influence+" on "+fixture.base));
            console.log("Native influenced-family fixture",fixture.base,mod.key,mod.reach_influence);
            assert.equal(mod.reach_kind,1);assert.equal(mod.reach_influence,fixture.influence);
            const family=buildModifierKeyIndex(mods).get(mod.key);assert.ok(family);
            rootItem=await client.createItem(frame,{rarity:"rare",withImplicits:false});
            await client.editItem(rootItem,frame,{influence_bits:1<<(fixture.influence-1)});
            admitted=await client.openSolver(frame,{version:"v1",rarity:"rare",slots:[{family_mod_key:family,min_tier:mod.family_tier_index}],actions:["scour"]});
            const before=await client.exportItem(rootItem,frame);
            const outcome=await client.solverCalc(admitted,rootItem,"scour");
            assert.ok(outcome.supported && outcome.legal);
            assert.deepEqual(await client.exportItem(rootItem,frame),before);
        }finally{if(admitted)await client.closeSolver(admitted);if(rootItem)await client.closeItem(rootItem);await client.closeSession(frame);}
    }

    session=await client.createSession(data,"Metadata/Items/Armours/BodyArmours/BodyInt17",86);
    item=await client.createItem(session,{rarity:"magic",withImplicits:true});
    const before=await client.exportItem(item,session);
    const goal:CalculatorItemGoal={version:"v1",rarity:"normal",slots:[]};
    const set:CalculatorGoalSet={version:"calculator_goal_set_v1",actions:["scour"],goals:[{id:"normal",goal},
        {id:"rare",goal:{...goal,rarity:"rare",allow_extra_modifiers:true}}]};
    const solver=await client.openCalcGoal(session,set);
    try {
        const result=await client.currencyCalc(solver,item,"scour");
        assert.equal(result.any_goal_probability,1);
        assert.equal(result.goal_results?.find(goal=>goal.id==="normal")?.success_probability,1);
        assert.equal(result.goal_results?.find(goal=>goal.id==="rare")?.success_probability,0);
        assert.equal(result.outcomes.reduce((sum,row)=>sum+row.probability,0),1);
        assert.deepEqual(result.outcomes[0].matched_goal_ids,["normal"]);
        assert.deepEqual(await client.exportItem(item,session),before);
        await assert.rejects(client.currencyCalc(solver,item,"unveil"),/observed choice|common policy/);
    } finally {await client.closeSolver(solver);}
    const single=await client.openCalcGoal(session,goal), one=await client.openCalcGoal(session,{...set,goals:[set.goals[0]]});
    try {assert.equal((await client.currencyCalc(single,item,"scour")).success_probability,(await client.currencyCalc(one,item,"scour")).goal_results?.[0].success_probability);}
    finally {await client.closeSolver(single);await client.closeSolver(one);}
    await assert.rejects(client.openCalcGoal(session,{...set,goals:Array.from({length:9},(_,i)=>({id:`g${i}`,goal}))}),/eight/);
    await assert.rejects(client.openCalcGoal(session,{...set,goals:[set.goals[0],set.goals[0]]}),/unique/);
    // Nested implicit/property goals use the same chosen final item law.
    await client.closeItem(item); item=await client.createItem(session,{rarity:"rare",withImplicits:true});
    const rarityGoal:CalculatorItemGoal={version:"v1",rarity:"rare",slots:[],allow_extra_modifiers:true,corrupted:true};
    const probe=await client.openCalcGoal(session,rarityGoal);
    let implicitId:number, implicitProbability:number;
    try {
        const vaal=await client.currencyCalc(probe,item,"vaal");
        const implicit=vaal.implicit_outcomes?.find(implicit=>implicit.present_probability>0);
        assert.ok(implicit);implicitId=implicit.mod;implicitProbability=implicit.present_probability;
    } finally {await client.closeSolver(probe);}
    const implicitKey=(await client.modInfo(session,implicitId)).key;
    const nested=await client.openCalcGoal(session,{version:"calculator_goal_set_v1",actions:[],goals:[
        {id:"property",goal:rarityGoal},{id:"implicit",goal:{...rarityGoal,implicit_mod_keys:[implicitKey]}}]});
    try {
        const vaal=await client.currencyCalc(nested,item,"vaal");
        assert.equal(vaal.any_goal_probability,1);
        assert.ok(Math.abs(vaal.goal_results!.find(goal=>goal.id==="implicit")!.success_probability-implicitProbability)<1e-12);
        assert.ok(Math.abs(vaal.goal_results!.find(goal=>goal.id==="implicit")!.implicit_satisfied[0]-implicitProbability)<1e-12);
        const temple=await client.currencyCalc(nested,item,"double_corruption");
        assert.ok(Math.abs(temple.any_goal_probability!-0.5)<1e-12);
    } finally {await client.closeSolver(nested);}
    // Bestiary is a deterministic native successor followed by shared observation.
    await client.closeItem(item);item=await client.createItem(session,{rarity:"magic",withImplicits:true});
    const beast=await client.openCalcGoal(session,{version:"calculator_goal_set_v1",actions:[],goals:[
        {id:"magic",goal:{version:"v1",rarity:"magic",slots:[],allow_extra_modifiers:true}},
        {id:"rare",goal:{version:"v1",rarity:"rare",slots:[],allow_extra_modifiers:true}}]});
    try {
        const before=await client.exportItem(item,session);
        const result=await client.bestiaryGoalCalc(data,beast,item,"bestiary:imprint");
        assert.equal(result.any_goal_probability,1);
        assert.equal(result.goal_results!.find(goal=>goal.id==="magic")!.success_probability,1);
        assert.equal(result.goal_results!.find(goal=>goal.id==="rare")!.success_probability,0);
        assert.deepEqual(await client.exportItem(item,session),before);
    } finally {await client.closeSolver(beast);}
    // Configured cluster law remains in the selected native output session.
    const cluster=await client.createSession(data,"Metadata/Items/Jewels/JewelPassiveTreeExpansionSmall",84,
        {passiveKey:"affliction_maximum_life",passiveCount:2});
    let clusterItem=0,clusterGoal=0;
    try {
        clusterItem=await client.createItem(cluster,{rarity:"normal"});
        clusterGoal=await client.openCalcGoal(cluster,{version:"calculator_goal_set_v1",actions:["alchemy"],goals:[
            {id:"rare",goal:{version:"v1",rarity:"rare",slots:[],allow_extra_modifiers:true}},
            {id:"normal",goal:{version:"v1",rarity:"normal",slots:[]}}]});
        const before=await client.exportItem(clusterItem,cluster);
        const result=await client.currencyCalc(clusterGoal,clusterItem,"alchemy");
        assert.equal(result.any_goal_probability,1);
        assert.equal(result.goal_results!.find(goal=>goal.id==="rare")!.success_probability,1);
        assert.equal(result.goal_results!.find(goal=>goal.id==="normal")!.success_probability,0);
        assert.deepEqual(await client.exportItem(clusterItem,cluster),before);
        await assert.rejects(client.currencyCalc(clusterGoal,clusterItem,"vaal"),/not yet approved|not yet.*qualified/);
    } finally {
        if(clusterGoal)await client.closeSolver(clusterGoal);if(clusterItem)await client.closeItem(clusterItem);await client.closeSession(cluster);
    }
    console.log("Source-matched WASM goal-set worker transport passed");
} finally {if(item)await client.closeItem(item);if(session)await client.closeSession(session);client.dispose();await worker.terminate();}
