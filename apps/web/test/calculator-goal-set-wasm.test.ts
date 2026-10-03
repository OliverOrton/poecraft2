// Run only against the source-matched rebuilt release WASM, through the worker.
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {Worker,type TransferListItem} from "node:worker_threads";
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
    console.log("Source-matched WASM goal-set worker transport passed");
} finally {if(item)await client.closeItem(item);if(session)await client.closeSession(session);client.dispose();await worker.terminate();}
