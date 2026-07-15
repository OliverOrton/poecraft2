import type { SolverActionInfo } from "./engine-protocol";
import {
    cloneStrategy,
    isStrategyDocument,
    validateStrategy,
    type StrategyDocument,
} from "./strategy-model";
import { ensureStrategyPositions } from "./strategy-layout";

export interface SolvePriceReadiness {
    totalActions: number;
    pricedActions: number;
    costKeys: string[];
    missingKeys: string[];
}

/** Action ids that can enter a scoped solve with the current economy. */
export function pricedSolverActionIds(
    actions: readonly SolverActionInfo[],
    priceFor: (key: string) => number | undefined,
): string[] {
    return actions
        .filter((action) =>
            action.cost_keys.every((key) => priceFor(key) !== undefined),
        )
        .map((action) => action.id);
}

/** Build the solve checklist from native action cost keys and shared prices. */
export function solvePriceReadiness(
    actions: readonly SolverActionInfo[],
    priceFor: (key: string) => number | undefined,
): SolvePriceReadiness {
    const costKeys = Array.from(
        new Set(actions.flatMap((action) => action.cost_keys)),
    ).sort((a, b) => a.localeCompare(b));
    const missingKeys = costKeys.filter((key) => priceFor(key) === undefined);
    const pricedActions = pricedSolverActionIds(actions, priceFor).length;
    return {
        totalActions: actions.length,
        pricedActions,
        costKeys,
        missingKeys,
    };
}

/** Validate, clone, and auto-layout a native solver policy for the workspace. */
export function prepareSolverStrategy(value: unknown): StrategyDocument {
    if (!isStrategyDocument(value)) {
        throw new Error("The solver returned an invalid strategy document.");
    }
    const strategy = cloneStrategy(value);
    ensureStrategyPositions(strategy);
    const errors = validateStrategy(strategy).filter(
        (issue) => issue.severity === "error",
    );
    if (errors.length) {
        throw new Error(
            `The compiled strategy is not board-valid: ${errors
                .map((issue) => issue.message)
                .join("; ")}`,
        );
    }
    return strategy;
}
