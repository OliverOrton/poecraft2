import assert from "node:assert/strict";
import {parseHTML} from "linkedom";
import {calculateAuthoredRecombination, recombinationOdds} from "../src/app/recombination-calculator";
import type {EngineClient} from "../src/app/engine-client";
import type {CalcResult, CalculatorGoalSet} from "../src/app/engine-protocol";
import {newCalculatorGoal, type CalculatorGoalDraft} from "../src/app/calculator-goal-set";
import type {ItemSnapshot} from "../src/app/workspace/persistence";

const result: CalcResult = {pair_version: 1, model_projection_exact: true, game_odds_estimated: true,
    supported: true, legal: true, success_probability: .35, slot_satisfied: [.35],
    carriers: [{carrier: 0, probability: .5, base_metadata_path: "base-a", item_level: 82},
        {carrier: 1, probability: .5, base_metadata_path: "base-b", item_level: 82}],
    outcomes: [
        {state: 0, carrier: 0, base_metadata_path: "base-a", probability: .25, is_goal: true},
        {state: 1, carrier: 0, base_metadata_path: "base-a", probability: .25, is_goal: false},
        {state: 2, carrier: 1, base_metadata_path: "base-b", probability: .1, is_goal: true},
        {state: 3, carrier: 1, base_metadata_path: "base-b", probability: .4, is_goal: false},
    ].map(row => ({rarity: 2, prefixes: 1, suffixes: 0, flags: 0, blocked: 0, slots: [2], ...row})) as CalcResult["outcomes"]};
assert.equal(recombinationOdds(result).success, .35);
assert.equal(recombinationOdds(result, "base-a").success, .25, "Base filtering keeps unconditional native mass");
assert.equal(recombinationOdds(result, "base-b").success, .1);
const equalBases = structuredClone(result);
equalBases.carriers![1].base_metadata_path = "base-a";
equalBases.outcomes.filter(row => row.carrier === 1).forEach(row => {row.base_metadata_path = "base-a";});
assert.equal(recombinationOdds(equalBases, "base-a").success, .35, "Equal bases count both random carriers");
assert.throws(() => recombinationOdds({...result, supported: false}), /unsupported/);
assert.throws(() => recombinationOdds({...result, outcomes: result.outcomes.slice(0, 1)}), /Incomplete/);
assert.throws(() => recombinationOdds({...result, outcomes: result.outcomes.map(row => ({...row, is_goal: undefined}))}), /Incomplete/);
assert.throws(() => recombinationOdds({...result, model_projection_exact: false}), /complete/);

const inputs: [ItemSnapshot, ItemSnapshot] = [
    {base: "base-a", itemLevel: 80, state: {rarity: 2}}, {base: "base-b", itemLevel: 84, state: {rarity: 2}}];
const goals: CalculatorGoalSet = {version: "calculator_goal_set_v1", actions: [], goals: [{id: "goal", goal: {slots: []}}]};
const closed: string[] = [];
let handle = 0, supplied: unknown;
const fake = {createSession: async () => ++handle, importItem: async () => ++handle,
    openRecombinationPair: async (request: unknown) => {supplied = request; return ++handle;},
    recombinationCalculate: async () => result,
    closeRecombinationPair: async (id: number) => {closed.push(`pair:${id}`);},
    closeItem: async (id: number) => {closed.push(`item:${id}`);}, closeSession: async (id: number) => {closed.push(`session:${id}`);}};
await calculateAuthoredRecombination(fake as unknown as EngineClient, 1, inputs, goals);
assert.deepEqual(supplied, {resources: [
    {identity: "calculator-input-a", role: "input_a", session: 1, item: 2},
    {identity: "calculator-input-b", role: "input_b", session: 3, item: 4}]});
assert.deepEqual(closed, ["pair:5", "item:2", "item:4", "session:1", "session:3"]);
for (const failure of ["importItem", "openRecombinationPair", "recombinationCalculate"] as const) {
    closed.length = 0; handle = 0;
    const failing = {...fake, [failure]: async () => {throw new Error(failure);}};
    await assert.rejects(calculateAuthoredRecombination(failing as unknown as EngineClient, 1, inputs, goals), new RegExp(failure));
    assert.ok(closed.includes("session:1"), "Failures release partially constructed sessions");
    if (failure !== "importItem") assert.ok(closed.includes("item:2"));
    if (failure === "recombinationCalculate") assert.ok(closed.includes("pair:5"));
}
closed.length = 0; handle = 0;
await assert.rejects(calculateAuthoredRecombination({...fake,
    closeRecombinationPair: async () => {throw new Error("release");}} as unknown as EngineClient, 1, inputs, goals), /release/);
assert.ok(closed.includes("item:2") && closed.includes("session:3"), "Pair-release failure still releases every input owner");

const dom = parseHTML("<!doctype html><html><body></body></html>");
Object.assign(globalThis, {window: dom.window, document: dom.document, HTMLElement: dom.HTMLElement,
    customElements: dom.customElements, CustomEvent: dom.CustomEvent, BroadcastChannel: undefined});
Object.defineProperty(globalThis, "navigator", {value: {userAgent: "linkedom"}, configurable: true});
const {PcRecombinationCalculator} = await import("../src/app/components/pc-recombination-calculator");
const calculator = new PcRecombinationCalculator();
const access = calculator as unknown as {client: EngineClient; data: number; busy: boolean; disposed: boolean;
    goal: CalculatorGoalDraft; inputs: Map<string, {snapshot: ItemSnapshot}>; result: CalcResult | null;
    baseCare: boolean; requiredBase: "a" | "b"; calculations: Set<Promise<void>>;
    calculate(): void; invalidate(): void; renderOdds(): void; identity(): string};
access.busy = false; access.data = 1;
access.inputs = new Map([["a", {snapshot: structuredClone(inputs[0])}], ["b", {snapshot: structuredClone(inputs[1])}]]);
access.renderOdds = () => {};
const delivery: {resolve?: (value: CalcResult) => void} = {};
const readDelivery = (): ((value: CalcResult) => void) | undefined => delivery.resolve;
let frozenGoal: CalculatorGoalSet | undefined;
access.client = {...fake, recombinationCalculate: async (_pair: number, goal: CalculatorGoalSet) => {
    frozenGoal = structuredClone(goal);
    return new Promise<CalcResult>(resolve => {delivery.resolve = resolve;});
}} as unknown as EngineClient;
for (const change of [
    () => {access.goal.slots.push({familyModKey: "new-goal", minTier: 1}); access.invalidate();},
    () => {access.inputs.get("a")!.snapshot.state = {changed: "a"}; access.invalidate();},
    () => {access.inputs.get("b")!.snapshot.state = {changed: "b"}; access.invalidate();},
    () => {access.baseCare = true; access.invalidate();},
    () => {access.requiredBase = "b"; access.invalidate();},
    () => {access.invalidate();}, // explicit Cancel
    () => {access.disposed = true;},
]) {
    access.disposed = false; access.baseCare = false; access.requiredBase = "a"; access.goal = newCalculatorGoal("goal");
    delete delivery.resolve; closed.length = 0; handle = 0;
    access.calculate();
    for (let i = 0; i < 20 && !delivery.resolve; ++i) await Promise.resolve();
    const release = readDelivery();
    assert.ok(release, "Native calculation started");
    change(); release(result); await Promise.all(access.calculations);
    assert.equal(access.result, null, "Edited, cancelled or disposed requests reject stale native odds");
    assert.equal(frozenGoal?.goals[0].goal.slots.length, 0, "Submitted goal is frozen");
    assert.ok(closed.includes("pair:5") && closed.includes("session:3"), "Stale calculation releases its owners");
}
console.log("recombination-calculator: terminal base filtering, resource cleanup and stale-request controls passed");
