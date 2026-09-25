import type {
    SolverActionFamily,
    SolverActionInfo,
    SolverGoal,
    SolveOptions,
    SolveSummary,
    SolveProgress,
    SolverWorkerMetrics,
} from "./engine-protocol";
import type { PinnedEconomy } from "./workspace/economy-service";
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

export interface CalculatorSolverGoalFields {
    rarity: NonNullable<SolverGoal["rarity"]>;
    minSatisfiedSlots: number;
    slots: SolverGoal["slots"];
}

export type CalculatorSolverGoalMode =
    | "odds"
    | "product_envelope"
    | "scoped_solve";

export interface CalculatorSolveRequest {
    run_id: string;
    submitted_at_utc: string;
    base_path: string;
    item_level: number;
    goal_envelope: SolverGoal;
    solve_options: SolveOptions;
    bounded_finish_after_ms: number;
    economy: PinnedEconomy;
    identity: {
        abi_version: number;
        source_revision: string | null;
        runtime_wasm_sha256: string | null;
        data_hash: string | null;
        data_hash_source: string;
    };
}

/** One bounded UI-observed history; worker/native clocks remain in each sample. */
export interface CalculatorDeliveryTrace {
    schema_version: "solver_delivery_trace_v2";
    request: CalculatorSolveRequest;
    resolved: { start_item: unknown; goal: SolverGoal | null };
    observations: SolveProgress[];
    observations_omitted: number;
    worker: Omit<SolverWorkerMetrics, "progress_observations"> | null;
    ui_milestones: Array<{ stage: string; ui_elapsed_ms: number }>;
    status: "preparing" | "running" | "completed" | "cancelled" | "error";
    error: string | null;
    /** Result and native checking record retained with the frozen request. */
    outcome?: {
        summary: SolveSummary;
        telemetry: unknown;
        compiled_strategy: StrategyDocument | null;
    };
}

export function createCalculatorDeliveryTrace(
    request: CalculatorSolveRequest,
): CalculatorDeliveryTrace {
    return {
        schema_version: "solver_delivery_trace_v2",
        request: structuredClone(request),
        resolved: { start_item: null, goal: null },
        observations: [],
        observations_omitted: 0,
        worker: null,
        ui_milestones: [],
        status: "preparing",
        error: null,
    };
}

export function retainCalculatorProgress(
    trace: CalculatorDeliveryTrace, progress: SolveProgress,
): void {
    if (trace.observations.length === 4096) {
        trace.observations.shift();
        trace.observations_omitted += 1;
    }
    trace.observations.push(progress);
}

export function retainCalculatorWorkerMetrics(
    trace: CalculatorDeliveryTrace, worker: SolverWorkerMetrics | undefined,
): void {
    if (!worker) return;
    // The UI already retained every delivered sample. Do not retain a second
    // full history from the final worker response, or resend one on each update.
    const { progress_observations: _workerHistory, ...metrics } = worker;
    trace.worker = metrics;
}

/** Build one of Calculator's three solver-goal contracts. */
export function buildCalculatorSolverGoal(
    fields: CalculatorSolverGoalFields,
    mode: CalculatorSolverGoalMode,
    inspectedActionId: string,
    actions?: readonly string[],
    disabledActionFamilies?: readonly SolverActionFamily[],
): SolverGoal {
    const scopedActions =
        mode === "scoped_solve" ? [...(actions ?? [])] : null;
    return {
        version: "v1",
        rarity: fields.rarity,
        ...(mode !== "odds" ? { action_mode: "goal_relevant" as const } : {}),
        min_satisfied_slots: fields.minSatisfiedSlots,
        slots: fields.slots,
        ...(mode !== "odds" && disabledActionFamilies?.length
            ? { disabled_action_families: [...disabledActionFamilies] }
            : {}),
        ...(scopedActions
            ? { actions: scopedActions }
            : {
                  fossil_mode: "goal_relevant" as const,
                  requested_fossil_actions:
                      mode === "odds" &&
                      inspectedActionId.startsWith("fossil:")
                      ? [inspectedActionId]
                      : [],
              }),
    };
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
