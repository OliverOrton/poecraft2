import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {parseHTML} from "linkedom";
import type {CraftAction, Catalog, HinekoraInfo} from "../src/app/engine-protocol";
import type {CraftControlsModel} from "../src/app/components/pc-craft-controls";
import type {CraftHistoryEntry} from "../src/app/workspace/persistence";
const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.defineProperty(globalThis,"navigator",{value:{userAgent:"linkedom"},configurable:true});
Object.assign(globalThis,{window:dom.window,document:dom.document,HTMLElement:dom.HTMLElement,
    customElements:dom.customElements,CustomEvent:dom.CustomEvent,BroadcastChannel:undefined});
globalThis.fetch=async()=>new Response(readFileSync(new URL("../public/game-assets/catalog.json",import.meta.url)));
const {PcEmulator}=await import("../src/app/components/pc-emulator");
const emulator=new PcEmulator();
const configured: CraftAction={type:"essence",essence:"selected-original-essence"};
const observations: CraftAction[]=[], actual: CraftAction[]=[], entries: CraftHistoryEntry[]=[];
const catalog: Catalog={groupKeyById:[],groupNameById:[],essences:[],fossils:[],bench:[],harvestTags:[],influences:[]};
const access=emulator as unknown as {
    client: unknown; catalog: Catalog; item: number; session: number; context: number; busy: boolean;
    lockInfo: HinekoraInfo | null; lockPreview: unknown;
    pendingHistoryEntry: CraftHistoryEntry | null; currentWork: Promise<void>|null; craftCosts: unknown;
    renderShell():void; renderMechanicControls():void; markChanged():Promise<void>;
    applyLock():Promise<void>; applyConfiguredAction(action:CraftAction,commit?:boolean):Promise<void>;
};
Object.assign(access,{catalog,item:1,session:2,context:3,busy:false,
    client:{applyHinekoraLock:async()=>({active:true,cost_keys:["hinekora_lock"]}),
        observeHinekoraLock:async(_context:number,_item:number,_session:number,currency:CraftAction)=>{
            observations.push(structuredClone(currency));return{active:true,currency,cost_keys:[]};},
        apply:async(_context:number,_item:number,currency:CraftAction)=>{actual.push(structuredClone(currency));return{applied:true,added:1,removed:0};}},
    craftCosts:{forAction:async()=>["original-essence-price-key"]}});
access.markChanged=async()=>{if(access.pendingHistoryEntry)entries.push(access.pendingHistoryEntry);access.pendingHistoryEntry=null;};
access.renderShell();
const ordinaryLockButton = emulator.querySelector<HTMLButtonElement>('.pc-craft-options [data-simple-action="hinekora_lock"]')!;
assert.ok(ordinaryLockButton);
assert.equal(ordinaryLockButton.disabled,false);
ordinaryLockButton.click(); await access.currentWork;
assert.deepEqual(observations,[]);assert.deepEqual(actual,[]);
assert.deepEqual(entries[0].costKeys,["hinekora_lock"]);
access.lockInfo={active:true,cost_keys:[],model:"independent-cached-lock-v1",approximate:true};
await access.applyConfiguredAction(configured);
assert.deepEqual(observations,[configured]);assert.deepEqual(actual,[]);
assert.deepEqual(entries[1].costKeys,[]);
access.lockInfo={active:true,currency:configured,cost_keys:[],preview:{}};
access.lockPreview={kind:"concrete",readOnly:true,itemFlags:0,rarity:"rare",influences:[],implicits:[],suffixes:[],
    prefixes:[{sessionModId:0,key:"Strength1",tierIndex:1,textLines:["+10 Strength"],classificationTags:[],fractured:false,crafted:false,rollValues:[10]}],maxPrefix:3,maxSuffix:3};
access.renderShell();
const model=()=>(emulator.querySelector("pc-craft-controls") as unknown as {model:CraftControlsModel}).model;
assert.match(emulator.textContent!,/Hinekora's Lock/);
assert.match(emulator.textContent!,/Roll values: 10/);
assert.equal(emulator.querySelector<HTMLInputElement>('[data-simple-action="hinekora_lock"]')!.disabled,true);
assert.equal(emulator.querySelector('[aria-label="Hinekora\'s Lock preview"] .pc-item-remove-mod'),null);
assert.ok(emulator.querySelector('.pc-craft-options [data-simple-action="hinekora_lock"]'));
assert.equal(emulator.querySelector('[data-lock-apply]'),null);
// Inspection/render and changing UI settings never execute the cached action.
model().onValue("essence-key","a-different-current-selection");
assert.deepEqual(actual,[]);
model().onLockCommit?.();await access.currentWork;
assert.deepEqual(actual,[configured]);assert.deepEqual(entries[2].costKeys,["original-essence-price-key"]);
assert.equal(observations.length,1,"Commit cannot reserve/pay another Lock");
console.log("Lock controls: Apply Lock, free independent-request preview, read-only numerical rolls and separate payment/commit passed");

const {setFallbackPrice,setPrice}=await import("../src/app/workspace/prices");
setFallbackPrice(null);setPrice("hinekora_lock",null);
const {PcCraftSpend}=await import("../src/app/components/pc-craft-spend");
const spendElement=new PcCraftSpend();
spendElement.setModel({spend:{counts:{hinekora_lock:1},untracked:false},catalog});
assert.match(spendElement.textContent!,/Unpriced/);
assert.match(spendElement.textContent!,/1 material price missing/);
assert.match(spendElement.querySelector("[data-spend-total]")!.textContent!,/\+ unpriced/);
assert.match(spendElement.querySelector('[data-spend-key="hinekora_lock"]')!.textContent!,/1 ×/);
console.log("Lock spending: exact one-unit quantity remains visibly unpriced without a quote");
