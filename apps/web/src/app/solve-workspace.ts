import type { SolverActionInfo } from "./engine-protocol";
import {
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
    missingFractureBasePrice: boolean;
}

function objectRecord(value: unknown): Record<string, unknown> | null {
    return value !== null && typeof value === "object"
        ? (value as Record<string, unknown>)
        : null;
}

/** Explain an incomplete native solve from its bounded telemetry. */
export function incompleteSolveDetail(telemetry: unknown): string {
    const root = objectRecord(telemetry);
    const optimization = objectRecord(root?.optimization);
    const capHits = Array.isArray(optimization?.cap_hits)
        ? optimization.cap_hits.filter(
              (value): value is string => typeof value === "string",
          )
        : [];
    if (capHits.length > 0) {
        return `The exact solve reached the ${capHits.join(", ")} resource boundary before it could produce a complete policy. This is a solver-capacity limit, not a pricing error; the incomplete policy was not compiled.`;
    }
    const status = optimization?.full_request_status;
    if (typeof status === "string" && status !== "incomplete_solve") {
        return `The exact solve stopped with status ${status} before it could produce a complete policy. The incomplete policy was not compiled.`;
    }
    return "The native optimizer did not produce a complete policy. The incomplete policy was not compiled.";
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
    const fracture = actions.find((action) => action.id === "fracture");
    const pricedFracture =
        fracture !== undefined &&
        fracture.cost_keys.every((key) => priceFor(key) !== undefined);
    return {
        totalActions: actions.length,
        pricedActions,
        costKeys,
        missingKeys,
        missingFractureBasePrice:
            pricedFracture && priceFor("base") === undefined,
    };
}

/** Validate, adopt, and auto-layout a uniquely transferred solver policy. */
export function prepareSolverStrategy(value: unknown): StrategyDocument {
    if (!isStrategyDocument(value)) {
        throw new Error("The solver returned an invalid strategy document.");
    }
    const strategy = value;
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
