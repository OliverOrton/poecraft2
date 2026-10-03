import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {parseHTML} from "linkedom";
import {calculatorGoalSet, recoverCalculatorGoalList, newCalculatorGoal, validateCalculatorGoalList,
    selectedCalculatorResult, CalculatorRequestLifetime, type CalculatorGoalList} from "../src/app/calculator-goal-set";
import type {CalculatorDraftRecord,ItemStashRecord} from "../src/app/workspace/persistence";
import type {CalcResult, CalculatorGoalSet, ModInfo, SolverGoal, SolverActionInfo} from "../src/app/engine-protocol";

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
calculator.innerHTML='<div class="pc-calc-goal-tabs"></div><input data-goal-name><button data-goal-command="add"></button><button data-goal-command="delete"></button><div class="pc-calc-output"></div><div class="pc-calc-solve-panel"></div><div class="pc-calc-status"></div>';
const access=calculator as unknown as {
    base:string;solver:number;pickerActions:SolverActionInfo[];modCache:ModInfo[];modKeyToFamily:Map<string,string>;
    addGoalFromPool(key:string):void;hasItemRequirements():boolean;itemGoal():unknown;solverGoal(mode:string,actions?:string[]):SolverGoal;
    guard(work:()=>Promise<void>):Promise<void>;goalChanged():Promise<void>;startSolve():Promise<void>;
    goalList:CalculatorGoalList;calc:CalcResult|null;calcError:string;session:number;item:number;actionId:string;
    dataId:number;resourceIdentity:string;donors:ItemStashRecord[];mechanicValues:Map<string,string>;oddsIdentity():string;
    calculateAwakener(solver:number,item:number,request:Record<string,unknown>,donor:ItemStashRecord|undefined,identity:string|undefined,data:number):Promise<CalcResult>;
    busy:boolean;disposed:boolean;client:Record<string,(...args:unknown[])=>Promise<unknown>>;
    renderGoalTabs():void;renderGoal():void;renderResults():void;renderSolvePanel():void;
    persist():Promise<void>;openSolver():Promise<void>;selectGoal(id:string):Promise<void>;
    goalCommand(command:string):Promise<void>;recalc():Promise<void>;currentWork:Promise<void>|null;
};
// Known runtime metadata: reported Shaper Titanium Spirit Shield, Elder on the
// same base, and Crusader Vaal Regalia. No base/influence exception is allowed.
const pickerCases=[
    {base:"Metadata/Items/Armours/Shields/ShieldInt12",key:"GainRandomChargeOnBlockInfluence1",influence:2},
    {base:"Metadata/Items/Armours/Shields/ShieldInt12",key:"AreaOfEffectInfluence1",influence:1},
    {base:"Metadata/Items/Armours/BodyArmours/BodyInt17",key:"EnergyShieldRecoveryRateBodyInfluence2",influence:3}];
const realGuard=access.guard,realGoalChanged=access.goalChanged;
access.guard=async work=>work();access.goalChanged=async()=>{};
access.busy=false;access.solver=5;access.item=1;access.pickerActions=[{id:"scour",cost_keys:[]} as unknown as SolverActionInfo];
for(const fixture of pickerCases){
    access.base=fixture.base;const target=newCalculatorGoal("influenced","Influenced explicit");
    access.goalList={version:"calculator_goal_list_v1",activeGoalId:target.id,goals:[target]};
    access.modCache=[{key:fixture.key,reach_kind:1,reach_influence:fixture.influence,family_tier_index:1} as ModInfo];
    access.modKeyToFamily=new Map([[fixture.key,fixture.key]]);
    access.addGoalFromPool(fixture.key);
    assert.equal(target.goalInfluenceBits,undefined,"An explicit modifier does not author an exact influence-set goal");
    assert.equal(access.hasItemRequirements(),false);
    assert.deepEqual(access.solverGoal("scoped_solve",["scour"]).slots,[{family_mod_key:fixture.key,min_tier:1}]);
    assert.equal((access.itemGoal() as {influence_bits?:number}).influence_bits,undefined);
    access.renderSolvePanel();
    assert.equal(calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="start"]')?.disabled,false);
    assert.doesNotMatch(calculator.querySelector('.pc-calc-solve-panel')!.textContent!,/does not support these requirements/);
}
const explicitOnly=structuredClone(access.goalList);
for(const property of [{goalInfluenceBits:2},{goalInfluenceBits:0},{goalCorrupted:true},{goalCorrupted:false},{goalImplicitKeys:["implicit"]}]){
    access.goalList=structuredClone(explicitOnly);Object.assign(access.goalList.goals[0],property);
    assert.equal(access.hasItemRequirements(),true,"Genuine item-property and implicit requirements remain unsupported");
    access.renderSolvePanel();assert.match(calculator.querySelector('.pc-calc-solve-panel')!.textContent!,/does not support these requirements/);
    await access.startSolve();assert.match(calculator.querySelector('.pc-calc-status')!.textContent!,/Use Odds/);
}
access.goalList=structuredClone(explicitOnly);access.goalList.goals[0].goalInfluenceBits=2;
access.addGoalFromPool(pickerCases[2].key);
assert.equal(access.goalList.goals[0].goalInfluenceBits,2,"Picking a modifier preserves a deliberately authored exact property");
access.renderSolvePanel();assert.match(calculator.querySelector('.pc-calc-solve-panel')!.textContent!,/Any influence/);
const heldSlots=structuredClone(access.goalList.goals[0].slots);
access.goalList.goals[0].goalInfluenceBits=undefined;access.renderSolvePanel();
assert.deepEqual(access.goalList.goals[0].slots,heldSlots,"Clearing only the property retains all modifier targets");
assert.equal(calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="start"]')?.disabled,false);
access.guard=realGuard;access.goalChanged=realGoalChanged;
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
for (const change of [()=>{access.session=3;},()=>{access.dataId=3;},()=>{access.disposed=true;}]) {
    access.disposed=false;access.session=2;access.dataId=2;access.goalList=structuredClone(list);resolveResult=undefined!;
    const waiting=access.recalc();while(!resolveResult) await new Promise(resolve=>setTimeout(resolve,0));
    change();resolveResult(result);await waiting;assert.equal(access.calc,null);
}
// Same resource ID with changed donor payload is a different probability request.
access.disposed=false;access.actionId="awakener";
const donor={id:"donor",base:"donor-base",itemLevel:80,state:{rarity:2},name:"Donor",createdAt:0,updatedAt:0} as ItemStashRecord;
access.donors=[donor];access.mechanicValues.set("awakener-donor","donor");
const donorIdentity=access.oddsIdentity(), frozenDonor=structuredClone(donor);
donor.state={rarity:1};assert.notEqual(access.oddsIdentity(),donorIdentity);
let submittedDonor:unknown;const donorHandles:number[]=[];
access.client={createSession:async(data,base,level)=>{assert.equal(data,2);assert.equal(base,"donor-base");assert.equal(level,80);return 6;},
    importItem:async(state)=>{submittedDonor=structuredClone(state);return 7;},currencyCalc:async()=>result,
    closeItem:async(id)=>{donorHandles.push(Number(id));},closeSession:async(id)=>{donorHandles.push(Number(id));}};
await access.calculateAwakener(5,4,{},frozenDonor,"receiver",2);
assert.deepEqual(submittedDonor,{rarity:2});assert.deepEqual(donorHandles,[7,6]);
await assert.rejects(access.calculateAwakener(5,4,{},frozenDonor,"donor",2),/distinct donor/);
console.log("Calculator goal-list, nonvisual tabs, migration and stale-response checks passed");
