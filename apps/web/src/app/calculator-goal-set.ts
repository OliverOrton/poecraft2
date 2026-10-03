import type {CalculatorItemGoal, CalculatorGoalSet, CalcResult} from "./engine-protocol";
import type {CalculatorDraftRecord, CalculatorGoalSlot} from "./workspace/persistence";

export const MAX_CALCULATOR_GOALS = 8;
export interface CalculatorGoalDraft {
    id: string;
    name: string;
    goalRarity: "normal" | "magic" | "rare";
    slots: CalculatorGoalSlot[];
    minSatisfiedSlots?: number;
    allowExtraModifiers?: boolean;
    goalImplicitKeys?: string[];
    goalInfluenceBits?: number;
    goalCorrupted?: boolean;
}
export interface CalculatorGoalList {
    version: "calculator_goal_list_v1";
    activeGoalId: string;
    goals: CalculatorGoalDraft[];
}
export function newCalculatorGoal(id: string = crypto.randomUUID(), name = "Goal 1"): CalculatorGoalDraft {
    return {id, name, goalRarity: "rare", slots: [], goalImplicitKeys: []};
}
export function createCalculatorGoalList(): CalculatorGoalList {
    const goal = newCalculatorGoal();
    return {version: "calculator_goal_list_v1", activeGoalId: goal.id, goals: [goal]};
}
/** Legacy scalar drafts migrate once; malformed modern lists refuse intact. */
export function recoverCalculatorGoalList(draft: CalculatorDraftRecord): CalculatorGoalList {
    const list = draft.goalList ?? {
        version: "calculator_goal_list_v1" as const,
        activeGoalId: "legacy-goal",
        goals: [{id: "legacy-goal", name: "Goal 1", goalRarity: draft.goalRarity,
            slots: draft.slots, minSatisfiedSlots: draft.minSatisfiedSlots,
            allowExtraModifiers: draft.allowExtraModifiers,
            goalImplicitKeys: draft.goalImplicitKeys, goalInfluenceBits: draft.goalInfluenceBits,
            goalCorrupted: draft.goalCorrupted}],
    };
    validateCalculatorGoalList(list);
    return structuredClone(list);
}
export function validateCalculatorGoalList(list: CalculatorGoalList): void {
    if (list.version !== "calculator_goal_list_v1" || !Array.isArray(list.goals) ||
        list.goals.length < 1 || list.goals.length > MAX_CALCULATOR_GOALS)
        throw new Error("Calculator requires one to eight goal items.");
    const ids = new Set<string>();
    for (const goal of list.goals) {
        if (!/^[a-zA-Z0-9_.-]{1,64}$/.test(goal.id) || ids.has(goal.id))
            throw new Error("Calculator goal IDs must be unique stable identifiers.");
        ids.add(goal.id);
        if (typeof goal.name !== "string" || goal.name.length > 80 ||
            !["normal", "magic", "rare"].includes(goal.goalRarity) ||
            !Array.isArray(goal.slots) || goal.slots.length > 8)
            throw new Error("Each Calculator goal allows at most eight modifier slots and an 80-character name.");
        for (const slot of goal.slots) if (Boolean(slot.group) === Boolean(slot.familyModKey) ||
            !Number.isSafeInteger(slot.minTier) || slot.minTier < 0)
            throw new Error("Calculator goal slots require one group/family and an integer tier.");
        if (goal.minSatisfiedSlots !== undefined && (!Number.isInteger(goal.minSatisfiedSlots) ||
            goal.minSatisfiedSlots < (goal.slots.length ? 1 : 0) || goal.minSatisfiedSlots > goal.slots.length))
            throw new Error("Calculator goal threshold is outside its slot count.");
    }
    if (!ids.has(list.activeGoalId)) throw new Error("Calculator active goal is missing.");
}
export function calculatorItemGoal(goal: CalculatorGoalDraft): CalculatorItemGoal {
    return {version: "v1", rarity: goal.goalRarity,
        ...(goal.allowExtraModifiers ? {allow_extra_modifiers: true} : {}),
        ...(goal.slots.length ? {min_satisfied_slots: goal.minSatisfiedSlots ?? goal.slots.length} : {}),
        slots: goal.slots.map(slot => slot.group ? {group: slot.group, min_tier: slot.minTier} :
            {family_mod_key: slot.familyModKey!, min_tier: slot.minTier}),
        implicit_mod_keys: [...(goal.goalImplicitKeys ?? [])],
        ...(goal.goalInfluenceBits === undefined ? {} : {influence_bits: goal.goalInfluenceBits}),
        ...(goal.goalCorrupted === undefined ? {} : {corrupted: goal.goalCorrupted})};
}
/** IDs and predicates bind odds; presentation and future values do not. */
export function calculatorGoalSet(list: CalculatorGoalList, action: string): CalculatorGoalSet {
    validateCalculatorGoalList(list);
    return {version: "calculator_goal_set_v1", actions: action ? [action] : [],
        goals: [...list.goals].sort((a,b) => a.id < b.id ? -1 : a.id > b.id ? 1 : 0).map(goal =>
            ({id: goal.id, goal: calculatorItemGoal(goal)}))};
}
/** A frozen native per-goal view; outcome routing is never reconstructed. */
export function selectedCalculatorResult(result: CalcResult | null, id: string): CalcResult | null {
    if (!result?.goal_results) return result;
    const goal = result.goal_results.find(goal => goal.id === id);
    if (!goal) return null;
    return {...result, ...goal, outcomes: result.outcomes.map(outcome => {
        const observation = outcome.goal_observations?.find(goal => goal.id === id);
        return observation ? {...outcome, ...observation} : outcome;
    })};
}
/** Request lifetimes cover edits, deletions, session changes and disposal. */
export class CalculatorRequestLifetime {
    private revision = 0;
    invalidate(): void { ++this.revision; }
    freeze(identity: string): {revision: number; identity: string} {
        return {revision: ++this.revision, identity};
    }
    accepts(token: {revision: number; identity: string}, identity: string, disposed: boolean): boolean {
        return !disposed && token.revision === this.revision && token.identity === identity;
    }
}
