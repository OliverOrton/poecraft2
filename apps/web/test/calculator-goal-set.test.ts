import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {parseHTML} from "linkedom";
import {calculatorGoalSet, recoverCalculatorGoalList, newCalculatorGoal, validateCalculatorGoalList,
    selectedCalculatorResult, CalculatorRequestLifetime, type CalculatorGoalList} from "../src/app/calculator-goal-set";
import type {CalculatorDraftRecord} from "../src/app/workspace/persistence";
import type {CalcResult, CalculatorGoalSet} from "../src/app/engine-protocol";

const legacy: CalculatorDraftRecord = {docId:"draft",base:"base",itemLevel:86,state:{},goalRarity:"magic",
    slots:[{group:"life",minTier:2}],goalImplicitKeys:["implicit"],goalInfluenceBits:2,goalCorrupted:false,
    actionId:"regal",fossilKeys:[],updatedAt:0};
const migrated = recoverCalculatorGoalList(legacy);
assert.equal(migrated.goals.length,1);
assert.equal(migrated.goals[0].allowExtraModifiers,undefined);
assert.equal(calculatorGoalSet(migrated,"regal").goals[0].goal.min_satisfied_slots,1);
assert.equal(calculatorGoalSet(migrated,"regal").goals[0].goal.corrupted,false);
migrated.goals[0].slots[0].minTier=1;
assert.equal(legacy.slots[0].minTier,2,"Recovery does not mutate the stored scalar draft");
const list: CalculatorGoalList = {version:"calculator_goal_list_v1",activeGoalId:"a",goals:[
    {...newCalculatorGoal("a","Life"),slots:[{group:"life",minTier:1}],allowExtraModifiers:true},
    {...newCalculatorGoal("b","Magic clean"),goalRarity:"magic"}]};
const request=calculatorGoalSet(list,"exalt");
const renamed=structuredClone(list);renamed.goals.reverse();renamed.goals[0].name="Renamed";renamed.activeGoalId="b";
assert.deepEqual(calculatorGoalSet(renamed,"exalt"),request,"Presentation does not change probability identity");
const restored=recoverCalculatorGoalList({...legacy,goalList:renamed});
assert.deepEqual(restored,renamed,"Versioned list, ordering, names and active selection survive recovery");
assert.throws(()=>validateCalculatorGoalList({...list,goals:Array.from({length:9},(_,i)=>newCalculatorGoal(String(i)))}),/one to eight/);
assert.throws(()=>validateCalculatorGoalList({...list,goals:[list.goals[0],list.goals[0]]}),/unique/);
assert.throws(()=>validateCalculatorGoalList({...list,activeGoalId:"missing"}),/missing/);
assert.throws(()=>recoverCalculatorGoalList({...legacy,goalList:{...list,goals:[{...list.goals[0],slots:Array(9).fill({group:"life",minTier:1})}]}}),/eight/);
const result: CalcResult={supported:true,legal:true,success_probability:0.7,any_goal_probability:0.7,
    slot_satisfied:[0.4],implicit_satisfied:[],goal_results:[
        {id:"a",success_probability:0.4,slot_satisfied:[0.4],implicit_satisfied:[]},
        {id:"b",success_probability:0.5,slot_satisfied:[],implicit_satisfied:[]}],
    outcomes:[{state:0,probability:0.2,rarity:2,prefixes:1,suffixes:0,flags:0,blocked:0,slots:[2],is_goal:true,
        matched_goal_ids:["a","b"],goal_observations:[{id:"a",slots:[2],is_goal:true,goal_properties_satisfied:true},
            {id:"b",slots:[],is_goal:true,goal_properties_satisfied:true}]}]};
assert.equal(selectedCalculatorResult(result,"b")?.success_probability,0.5);
assert.deepEqual(selectedCalculatorResult(result,"b")?.outcomes[0].slots,[]);
assert.equal(selectedCalculatorResult(result,"gone"),null);
const lifetime=new CalculatorRequestLifetime();const first=lifetime.freeze("one"),second=lifetime.freeze("two");
assert.equal(lifetime.accepts(first,"one",false),false);
assert.equal(lifetime.accepts(second,"different-session",false),false);
assert.equal(lifetime.accepts(second,"two",true),false);
lifetime.invalidate();assert.equal(lifetime.accepts(second,"two",false),false);

const dom=parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis,"navigator",{value:{userAgent:"linkedom"},configurable:true});
Object.assign(globalThis,{window:dom.window,document:dom.document,HTMLElement:dom.HTMLElement,
    customElements:dom.customElements,CustomEvent:dom.CustomEvent,BroadcastChannel:undefined});
globalThis.fetch=async()=>new Response(readFileSync(new URL("../public/game-assets/catalog.json",import.meta.url)));
const {PcCalculator}=await import("../src/app/components/pc-calculator");
const calculator=new PcCalculator();
calculator.innerHTML='<div class="pc-calc-goal-tabs"></div><input data-goal-name><button data-goal-command="add"></button><button data-goal-command="delete"></button><div class="pc-calc-output"></div>';
const access=calculator as unknown as {
    goalList:CalculatorGoalList;calc:CalcResult|null;calcError:string;session:number;item:number;actionId:string;
    busy:boolean;disposed:boolean;client:Record<string,(...args:unknown[])=>Promise<unknown>>;
    renderGoalTabs():void;renderGoal():void;renderResults():void;renderSolvePanel():void;
    persist():Promise<void>;openSolver():Promise<void>;selectGoal(id:string):Promise<void>;
    goalCommand(command:string):Promise<void>;recalc():Promise<void>;currentWork:Promise<void>|null;
};
Object.assign(access,{goalList:structuredClone(list),busy:false,calc:result,session:2,item:1,actionId:"exalt"});
access.renderGoalTabs();assert.equal(calculator.querySelectorAll('[role="tab"]').length,2);
assert.equal(calculator.querySelector('[aria-selected="true"]')?.textContent,"Life");
let persisted:CalculatorGoalList|undefined;
access.persist=async()=>{persisted=structuredClone(access.goalList);};
access.openSolver=async()=>{access.calc=null;};access.renderGoal=()=>access.renderGoalTabs();access.renderResults=()=>{};access.renderSolvePanel=()=>{};
await access.selectGoal("b");assert.equal(access.goalList.activeGoalId,"b");assert.equal(access.calc,result,"Tab selection retains shared odds");
await access.goalCommand("left");assert.deepEqual(access.goalList.goals.map(g=>g.id),["b","a"]);
assert.deepEqual(calculatorGoalSet(access.goalList,"exalt"),request);
assert.equal(persisted?.activeGoalId,"b");

let resolveResult!:(value:CalcResult)=>void;let submitted:CalculatorGoalSet|undefined;
const closed:number[]=[];
access.client={cloneItem:async()=>4,exportItem:async()=>({}),openCalcGoal:async(_session,goal)=>{submitted=goal as CalculatorGoalSet;return 5;},
    solverActions:async()=>[],currencyCalc:()=>new Promise<CalcResult>(resolve=>{resolveResult=resolve;}),
    closeItem:async id=>{closed.push(Number(id));},closeSolver:async id=>{closed.push(Number(id));}};
const pending=access.recalc();
while (!resolveResult) await new Promise(resolve=>setTimeout(resolve,0));
access.goalList.goals=access.goalList.goals.filter(goal=>goal.id!=="a");
resolveResult(result);await pending;
assert.equal(access.calc,null,"Deleting a goal rejects an older frozen response");
assert.equal(submitted?.goals.length,2,"Worker request remains frozen after deletion");
assert.deepEqual(closed,[4,5],"Stale responses still release both native handles");
for (const change of [()=>{access.session=3;},()=>{access.disposed=true;}]) {
    access.disposed=false;access.session=2;access.goalList=structuredClone(list);resolveResult=undefined!;
    const waiting=access.recalc();while(!resolveResult) await new Promise(resolve=>setTimeout(resolve,0));
    change();resolveResult(result);await waiting;assert.equal(access.calc,null);
}
console.log("Calculator goal-list, nonvisual tabs, migration and stale-response checks passed");
