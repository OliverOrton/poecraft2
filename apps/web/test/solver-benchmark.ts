import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { cpus } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, type EngineTransport } from "../src/app/engine-client";
import {
    EngineError,
    type SolveProgress,
    type SolverSolveResult,
    type SolverTelemetry,
} from "../src/app/engine-protocol";
import { prepareSolverStrategy } from "../src/app/solve-workspace";
import { isStrategyDocument } from "../src/app/strategy-model";
import {
    benchmarkConsiderImprintPrograms,
    loadSolverBenchmarkCorpus,
    materializeSolverBenchmarkEconomy,
    materializeSolverBenchmarkGoal,
    validateCorpusArtifactPins,
    type SolverBenchmarkCase,
} from "./solver-benchmark-corpus";
import type { ClientMessage, WorkerMessage } from "../src/app/engine-protocol";

const REPO_ROOT = fileURLToPath(new URL("../../../", import.meta.url));
const ARTIFACT_DIR = resolve(REPO_ROOT, "data/compiled/current");

interface CliOptions {
    corpus: string;
    output?: string;
    caseId?: string;
    skipVerification: boolean;
    verificationRuns?: number;
    solveStepWorkItems?: number;
    highImpactExecutableUppers?: boolean;
    watchdogSeconds?: number;
    solveOnly: boolean;
}

type CompletedSolverSolveResult = Extract<
    SolverSolveResult,
    { cancelled: false }
>;

interface CaseReport {
    id: string;
    category: string;
    approval_status: string;
    benchmark_enabled: boolean;
    expected: SolverBenchmarkCase["expected"] | null;
    actual_status: string;
    workflow_status: {
        solve_result_class: string;
        compile: string;
        exact_evaluation: string;
        simulation: string;
    };
    expectation_met: boolean;
    verification_skipped: boolean;
    input: Record<string, unknown>;
    phase_wall_ms: {
        registry_layout: number | null;
        solve: number | null;
        compile: number | null;
        verification: number | null;
        total: number | null;
    };
    execution: {
        solve_steps: number | null;
        max_solve_step_ms: number | null;
        worker_max_slice_ms: number | null;
        finalization_ms: number | null;
        cancellation_ack_ms: number | null;
        cancellation_mode: string | null;
        cooperative_abandon_ms: number | null;
        watchdog_seconds: number | null;
        watchdog_mode: "cooperative_abort_signal" | null;
        watchdog_expired: boolean;
    };
    progress_trace: Array<Record<string, unknown>>;
    memory: {
        measurement_kind: string;
        process_working_set_before_bytes: number | null;
        process_working_set_after_bytes: number | null;
        process_working_set_delta_bytes: number | null;
        process_working_set_peak_bytes: number | null;
        wasm_heap_before_bytes: number | null;
        wasm_heap_after_bytes: number | null;
        wasm_heap_growth_bytes: number | null;
    };
    solve_summary: Record<string, unknown> | null;
    solver_telemetry: SolverTelemetry | null;
    compiled_graph: {
        nodes: number;
        edges: number;
        strategy_json_bytes: number;
    } | null;
    product_action_ids: string[];
    compiled_operation_contract: Record<string, unknown> | null;
    compiled_operation_contracts: Array<Record<string, unknown>>;
    market_price_override_contracts: Record<string, unknown> | null;
    material_ratio_contract: Record<string, unknown> | null;
    forced_winner_contract: Record<string, unknown> | null;
    bounded_best_policy_contract: Record<string, unknown> | null;
    mechanic_family_control: Record<string, unknown> | null;
    prepared_strategy: {
        sha256: string;
        json_bytes: number;
        document: unknown;
    } | null;
    exact_evaluation: Record<string, unknown> | null;
    value: { start: number | null };
    verification: Record<string, unknown> | null;
    cap_checks: {
        all_passed: boolean;
        checks: Record<string, boolean>;
    };
    errors: string[];
}

function parseArgs(args: string[]): CliOptions {
    const parsedOptions: Partial<CliOptions> = {};
    let corpus = resolve(REPO_ROOT, "fixtures/solver-benchmarks/v1/manifest.json");
    let output: string | undefined;
    let caseId: string | undefined;
    let skipVerification = false;
    let verificationRuns: number | undefined;
    let solveOnly = false;
    for (let index = 0; index < args.length; index += 1) {
        const value = args[index + 1];
        if (args[index] === "--corpus" && value) {
            corpus = resolve(value);
            index += 1;
        } else if (args[index] === "--output" && value) {
            output = resolve(value);
            index += 1;
        } else if (args[index] === "--case" && value) {
            caseId = value;
            index += 1;
        } else if (args[index] === "--skip-verification") {
            skipVerification = true;
        } else if (args[index] === "--verification-runs" && value) {
            const parsed = Number(value);
            if (!Number.isSafeInteger(parsed) || parsed <= 0) {
                throw new Error("--verification-runs must be a positive integer");
            }
            verificationRuns = parsed;
            index += 1;
        } else if (args[index] === "--solve-step-work-items" && value) {
            const parsed = Number(value);
            if (!Number.isSafeInteger(parsed) || parsed <= 0) {
                throw new Error(
                    "--solve-step-work-items must be a positive integer",
                );
            }
            parsedOptions.solveStepWorkItems = parsed;
            index += 1;
        } else if (
            args[index] === "--high-impact-executable-uppers" && value
        ) {
            if (value !== "true" && value !== "false") {
                throw new Error(
                    "--high-impact-executable-uppers must be true or false",
                );
            }
            parsedOptions.highImpactExecutableUppers = value === "true";
            index += 1;
        } else if (args[index] === "--watchdog-seconds" && value) {
            const parsed = Number(value);
            if (!Number.isFinite(parsed) || parsed <= 0) {
                throw new Error("--watchdog-seconds must be a positive number");
            }
            parsedOptions.watchdogSeconds = parsed;
            index += 1;
        } else if (args[index] === "--solve-only") {
            solveOnly = true;
        } else {
            throw new Error(`unknown or incomplete argument: ${args[index]}`);
        }
    }
    Object.assign(parsedOptions, {
        corpus,
        output,
        caseId,
        skipVerification,
        verificationRuns,
        solveOnly,
    });
    return parsedOptions as CliOptions;
}

function applyDiagnosticOverrides(
    spec: SolverBenchmarkCase,
    options: CliOptions,
): SolverBenchmarkCase {
    return {
        ...spec,
        watchdog_seconds: options.watchdogSeconds ?? spec.watchdog_seconds,
        caps: {
            ...spec.caps,
            solve_step_work_items:
                options.solveStepWorkItems ?? spec.caps.solve_step_work_items,
            high_impact_executable_uppers:
                options.highImpactExecutableUppers ??
                spec.caps.high_impact_executable_uppers,
        },
    };
}

function readText(path: string): string {
    return readFileSync(path, "utf8");
}

function buildBundle(): Uint8Array {
    const manifest = readText(resolve(ARTIFACT_DIR, "manifest.json"));
    const strings = readText(resolve(ARTIFACT_DIR, "strings.json"));
    const gameData = readText(resolve(ARTIFACT_DIR, "game-data.json"));
    return new TextEncoder().encode(
        `{"manifest":${manifest},"strings":${strings},"game_data":${gameData}}`,
    );
}

function spawnClient(): { client: EngineClient; worker: Worker } {
    const worker = new Worker(new URL("./worker-bootstrap.mjs", import.meta.url));
    const transport: EngineTransport = {
        postMessage: (message: ClientMessage, transfer?: Transferable[]) =>
            worker.postMessage(
                message,
                (transfer ?? []) as unknown as TransferListItem[],
            ),
        onMessage: (handler: (message: WorkerMessage) => void) =>
            worker.on("message", handler),
        onError: (handler: (error: Error) => void) => worker.on("error", handler),
        terminate: () => void worker.terminate(),
    };
    return { client: new EngineClient(transport), worker };
}

function roundMs(value: number): number {
    return Number(value.toFixed(3));
}

function errorDetail(error: unknown): string {
    if (error instanceof EngineError) return `[${error.code}] ${error.detail}`;
    return error instanceof Error ? error.message : String(error);
}

function disabledReport(spec: SolverBenchmarkCase, status?: string): CaseReport {
    return {
        id: spec.id,
        category: spec.category,
        approval_status: spec.approval_status,
        benchmark_enabled: false,
        expected: spec.expected,
        actual_status:
            status ??
            (spec.execution_backend === "native_unit_fixture"
                ? "covered_by_native_unit_gate"
                : "not_run_approval_pending"),
        workflow_status: {
            solve_result_class: "not_run",
            compile: "not_attempted",
            exact_evaluation: "not_requested",
            simulation: "not_requested",
        },
        expectation_met: true,
        verification_skipped: false,
        input: {
            comparison_profile:
                spec.comparison_profile ??
                (spec.execution_backend === "native_unit_fixture"
                    ? "native-unit-only"
                    : "native-wasm-solver-v1"),
            watchdog_seconds: spec.watchdog_seconds ?? null,
            requested_bounded_finish_seconds:
                spec.requested_bounded_finish_seconds ?? null,
            session: spec.session ?? null,
            start: spec.start ?? null,
            goal: spec.goal ?? null,
            product_action_envelope: spec.product_action_envelope ?? null,
            allowed_mechanic_families: spec.allowed_mechanic_families ?? null,
            compiled_operation_contract:
                spec.compiled_operation_contract ?? null,
            compiled_operation_contracts:
                spec.compiled_operation_contracts ?? null,
            market_price_override_contracts:
                spec.market_price_override_contracts ?? null,
            material_ratio_contract: spec.material_ratio_contract ?? null,
            forced_winner_contract: spec.forced_winner_contract ?? null,
            bounded_best_policy_contract:
                spec.bounded_best_policy_contract ?? null,
            mechanic_family_control: spec.mechanic_family_control ?? null,
            economy: spec.economy ?? null,
            caps: spec.caps ?? null,
            verification: spec.verification ?? null,
        },
        phase_wall_ms: {
            registry_layout: null,
            solve: null,
            compile: null,
            verification: null,
            total: null,
        },
        execution: {
            solve_steps: null,
            max_solve_step_ms: null,
            worker_max_slice_ms: null,
            finalization_ms: null,
            cancellation_ack_ms: null,
            cancellation_mode: null,
            cooperative_abandon_ms: null,
            watchdog_seconds: spec.watchdog_seconds ?? null,
            watchdog_mode: spec.watchdog_seconds === undefined
                ? null
                : "cooperative_abort_signal",
            watchdog_expired: false,
        },
        progress_trace: [],
        memory: {
            measurement_kind: "not_measured",
            process_working_set_before_bytes: null,
            process_working_set_after_bytes: null,
            process_working_set_delta_bytes: null,
            process_working_set_peak_bytes: null,
            wasm_heap_before_bytes: null,
            wasm_heap_after_bytes: null,
            wasm_heap_growth_bytes: null,
        },
        solve_summary: null,
        solver_telemetry: null,
        compiled_graph: null,
        product_action_ids: [],
        compiled_operation_contract: null,
        compiled_operation_contracts: [],
        market_price_override_contracts: null,
        material_ratio_contract: null,
        forced_winner_contract: null,
        bounded_best_policy_contract: null,
        mechanic_family_control: null,
        prepared_strategy: null,
        exact_evaluation: null,
        value: { start: null },
        verification: null,
        cap_checks: { all_passed: true, checks: {} },
        errors: [],
    };
}

function startRssSampler(): { before: number; stop: () => { after: number; peak: number } } {
    const before = process.memoryUsage().rss;
    let peak = before;
    const timer = setInterval(() => {
        peak = Math.max(peak, process.memoryUsage().rss);
    }, 5);
    timer.unref();
    return {
        before,
        stop: () => {
            clearInterval(timer);
            const after = process.memoryUsage().rss;
            return { after, peak: Math.max(peak, after) };
        },
    };
}

async function safeTelemetry(
    client: EngineClient,
    solver: number,
    errors: string[],
): Promise<SolverTelemetry | null> {
    try {
        return await client.solverTelemetry(solver);
    } catch (error) {
        errors.push(`telemetry: ${errorDetail(error)}`);
        return null;
    }
}

function statusFrom(
    solve: SolverSolveResult | null,
    error: string | null,
    telemetry: SolverTelemetry | null,
    pricedProductEnvelope = false,
): string {
    if (solve?.cancelled) return "cancelled";
    if (solve && !solve.cancelled) {
        if (solve.stop_cause === "state_cap") return "refused_state_cap";
        if (solve.cap_hit_mask !== 0) return "refused_resource_cap";
    }
    const stateCapHit = nestedBoolean(telemetry, "optimization", "state_cap_hit");
    const resourceCapHit = nestedBoolean(
        telemetry,
        "optimization",
        "resource_cap_hit",
    );
    const missingPrice = nestedNumber(telemetry, "actions", "missing_price") ?? 0;
    const unsupported =
        (nestedNumber(telemetry, "actions", "unsupported_requested") ?? 0) +
        (nestedNumber(telemetry, "actions", "unsupported_observed") ?? 0);
    const fullRequestStatus = nestedString(
        telemetry,
        "optimization",
        "full_request_status",
    );
    if (stateCapHit) return "refused_state_cap";
    if (resourceCapHit) return "refused_resource_cap";
    if (missingPrice > 0 && unsupported > 0 && !pricedProductEnvelope) {
        return "refused_missing_price_and_unsupported_action";
    }
    if (missingPrice > 0 && !pricedProductEnvelope) {
        return "refused_missing_price";
    }
    if (unsupported > 0) return "refused_unsupported_action";
    if (
        fullRequestStatus === "incomplete_action_subset" &&
        !pricedProductEnvelope
    ) {
        return "incomplete_action_subset";
    }
    if (solve && !solve.cancelled && solve.converged) {
        return !pricedProductEnvelope &&
            (solve.skipped_actions > 0 || missingPrice > 0 || unsupported > 0)
            ? "converged_filtered_actions"
            : "converged";
    }
    if (solve?.policy_status === "bounded_near_optimal") {
        return "bounded_near_optimal";
    }
    if (solve?.policy_status === "bounded_feasible") {
        return "bounded_feasible";
    }
    if (
        solve && !solve.cancelled &&
        solve.termination === "numerical_stability"
    ) {
        return "numerical_stability";
    }
    if (
        solve && !solve.cancelled &&
        solve.termination === "no_executable_policy"
    ) {
        return "no_executable_policy";
    }
    const detail = error?.toLowerCase() ?? "";
    if (detail.includes("max_states") || detail.includes("state cap")) {
        return "refused_state_cap";
    }
    if (detail.includes("unsupported")) return "refused_unsupported_action";
    if (detail.includes("price")) return "refused_missing_price";
    if (solve && !solve.cancelled && !solve.converged) {
        const optimization = nestedString(telemetry, "optimization", "status");
        const rawStart = nestedNumber(telemetry, "value", "raw_start_bound");
        const goalStates = nestedNumber(telemetry, "states", "goal");
        if (
            optimization === "not_converged" &&
            rawStart !== null &&
            rawStart >= 1e12 &&
            solve.residual === 0 &&
            goalStates === 0
        ) {
            return "refused_unreachable_goal";
        }
        return "not_converged";
    }
    return error ? "error" : "not_run";
}

function expectationMet(expected: string, actual: string): boolean {
    if (expected === actual) return true;
    if (expected === "refused_state_cap_with_filtered_actions") {
        return actual === "refused_state_cap";
    }
    if (expected === "baseline_cap_or_compile_refusal_allowed") {
        return [
            "converged",
            "refused_state_cap",
            "refused_resource_cap",
            "not_converged",
        ].includes(actual);
    }
    if (expected === "reliability_classified") {
        return [
            "converged",
            "bounded_near_optimal",
            "bounded_feasible",
            "no_executable_policy",
            "numerical_stability",
            "refused_state_cap",
            "refused_sweep_cap",
            "refused_resource_cap",
            "refused_unsupported_action",
            "refused_unreachable_goal",
        ].includes(actual);
    }
    return false;
}

function nestedNumber(value: unknown, ...path: string[]): number | null {
    let current = value;
    for (const key of path) {
        if (!current || typeof current !== "object") return null;
        current = (current as Record<string, unknown>)[key];
    }
    return typeof current === "number" ? current : null;
}

function nestedString(value: unknown, ...path: string[]): string | null {
    let current = value;
    for (const key of path) {
        if (!current || typeof current !== "object") return null;
        current = (current as Record<string, unknown>)[key];
    }
    return typeof current === "string" ? current : null;
}

function nestedBoolean(value: unknown, ...path: string[]): boolean | null {
    let current = value;
    for (const key of path) {
        if (!current || typeof current !== "object") return null;
        current = (current as Record<string, unknown>)[key];
    }
    return typeof current === "boolean" ? current : null;
}

function nestedRecord(
    value: unknown,
    ...path: string[]
): Record<string, unknown> | null {
    let current = value;
    for (const key of path) {
        if (!current || typeof current !== "object" || Array.isArray(current)) {
            return null;
        }
        current = (current as Record<string, unknown>)[key];
    }
    return current && typeof current === "object" && !Array.isArray(current)
        ? current as Record<string, unknown>
        : null;
}

function finiteNumber(value: unknown): value is number {
    return typeof value === "number" && Number.isFinite(value);
}

function costsReconcile(
    left: number | null,
    right: number | null,
    spec: SolverBenchmarkCase,
): boolean {
    if (!finiteNumber(left) || !finiteNumber(right)) return false;
    const absolute = Math.abs(left - right);
    const relative = Math.abs(right) > 1e-12
        ? absolute / Math.abs(right)
        : absolute;
    return absolute <=
        (spec.verification.exact_cost_absolute_tolerance ?? 1e-7) ||
        relative <=
        (spec.verification.exact_cost_relative_tolerance ?? 1e-9);
}

function nonnegativeCount(value: unknown): number {
    return finiteNumber(value) && value > 0 ? value : 0;
}

function boundedBestPolicyReport(
    spec: SolverBenchmarkCase,
    solve: CompletedSolverSolveResult | null,
    telemetry: SolverTelemetry | null,
): Record<string, unknown> | null {
    const contract = spec.bounded_best_policy_contract;
    if (!contract) return null;

    const exactPath = solve?.policy_available === true &&
        solve.policy_status === "exact" && solve.termination === "exact_closed";
    if (exactPath) {
        return {
            checked: true,
            passed: contract.allow_exact,
            path: "exact",
            checks: {
                named_stop: null,
                strict_gap: null,
                open_obligations: null,
                evaluated_upper: null,
                cheapest_independently_evaluated_incumbent: null,
            },
            open_work: {
                incremental_actions: 0,
                refinement_obligations: 0,
            },
            failure_reason: contract.allow_exact
                ? null
                : "the contract does not permit the exact completion path",
        };
    }

    const bounded = solve?.policy_available === true && [
        "bounded_feasible",
        "bounded_near_optimal",
    ].includes(solve.policy_status);
    const capStops = new Set([
        "state_cap",
        "transition_cap",
        "memory_cap",
        "sweep_cap",
        "reforge_work_cap",
        "state_action_row_cap",
        "compiled_output_cap",
    ]);
    const allowedTerminations = contract.allowed_bounded_terminations;
    const allowedStopCauses = contract.allowed_bounded_stop_causes;
    const namedStop = allowedTerminations === undefined &&
            allowedStopCauses === undefined
        ? (solve?.termination === "refused_resource_cap" &&
                capStops.has(solve.stop_cause)) ||
            (solve?.termination === "numerical_stability" &&
                solve.stop_cause === "numerical_stability") ||
            (solve?.termination === "requested_bounded_finish" &&
                solve.stop_cause === "requested_bounded_finish")
        : solve !== null &&
            solve !== undefined &&
            (allowedTerminations === undefined ||
                allowedTerminations.includes(solve.termination)) &&
            (allowedStopCauses === undefined ||
                allowedStopCauses.includes(solve.stop_cause));
    const strictGap = finiteNumber(solve?.lower_bound) &&
        finiteNumber(solve?.upper_bound) &&
        solve.lower_bound < solve.upper_bound;
    const evaluatedUpper = costsReconcile(
        solve?.upper_bound ?? null,
        solve?.evaluated_policy_cost ?? null,
        spec,
    );

    const envelope = nestedRecord(telemetry, "incremental_action_envelope");
    const incremental = nonnegativeCount(
        envelope?.remaining_action_envelope,
    );
    const incrementalOpen = envelope?.enabled === true &&
        envelope.closed === false && incremental > 0;
    const certification = nestedRecord(
        telemetry,
        "policy_refinement",
        "certification_work",
    );
    const refinement = [
        "unresolved_alternative_obligations",
        "competitive_alternatives_remaining",
        "obligations_resource_interrupted",
    ].reduce(
        (total, key) => total + nonnegativeCount(certification?.[key]),
        0,
    );
    const openObligations = incrementalOpen || refinement > 0;

    const fallbackPortfolio = nestedRecord(
        telemetry,
        "policy_refinement",
        "fallback_portfolio",
    );
    const cheapestEvaluated =
        fallbackPortfolio?.cheapest_independently_evaluated_selected === true;
    const passed = bounded &&
        (!contract.bounded_requires_named_stop || namedStop) &&
        (!contract.bounded_requires_strict_gap || strictGap) &&
        (!contract.bounded_requires_open_obligations || openObligations) &&
        (!contract.require_evaluated_upper || evaluatedUpper) &&
        (!contract.require_cheapest_independently_evaluated_incumbent ||
            cheapestEvaluated);
    return {
        checked: true,
        passed,
        path: "bounded",
        checks: {
            named_stop: namedStop,
            strict_gap: strictGap,
            open_obligations: openObligations,
            evaluated_upper: evaluatedUpper,
            cheapest_independently_evaluated_incumbent: cheapestEvaluated,
        },
        open_work: {
            incremental_actions: incremental,
            refinement_obligations: refinement,
        },
        failure_reason: passed
            ? null
            : !bounded
              ? "the result was neither exact nor a bounded policy"
              : "one or more bounded-result proof obligations failed",
    };
}

function matchingOperationNodes(
    strategy: unknown,
    operationType: string,
    requiredStringParameters: Readonly<Record<string, string>> = {},
): number {
    const root = nestedRecord(strategy);
    if (!Array.isArray(root?.nodes)) return 0;
    return root.nodes.filter((value) => {
        const node = value && typeof value === "object" && !Array.isArray(value)
            ? value as Record<string, unknown>
            : null;
        const operation = nestedRecord(node?.operation);
        return node?.kind === "operation" && operation?.type === operationType &&
            Object.entries(requiredStringParameters).every(
                ([key, expected]) => operation[key] === expected,
            );
    }).length;
}

function compiledOperationContracts(
    spec: SolverBenchmarkCase,
): Array<{ type: string; required_string_parameters: Record<string, string> }> {
    return [
        ...(spec.compiled_operation_contract === undefined
            ? []
            : [spec.compiled_operation_contract]),
        ...(spec.compiled_operation_contracts ?? []),
    ];
}

function mechanicFamilyControlReport(
    spec: SolverBenchmarkCase,
    telemetry: SolverTelemetry | null,
    operationContracts: Array<Record<string, unknown>>,
    compileStatus: string,
    exactEvaluationStatus: string,
): Record<string, unknown> | null {
    const contract = spec.mechanic_family_control;
    if (contract === undefined) return null;
    const ledger = nestedRecord(
        telemetry,
        "incremental_action_envelope",
        "typed_ledger",
    );
    const lifecycles = Array.isArray(ledger?.action_lifecycles)
        ? ledger.action_lifecycles
        : [];
    const authoritative = ledger?.authoritative_obligation_owner === true;
    let registered = authoritative;
    let scheduled = authoritative;
    let admitted = authoritative;
    let materialized = authoritative;
    for (const actionId of contract.registered_action_ids) {
        const lifecycle = lifecycles.map((value) => nestedRecord(value)).find(
            (value) => value?.action_id === actionId,
        ) ?? null;
        registered = registered && lifecycle?.registered === true;
        materialized = materialized &&
            lifecycle?.exact_row_complete === true;
        scheduled = scheduled && lifecycle?.scheduled === true;
        admitted = admitted &&
            lifecycle?.exact_registry_legality === true &&
            lifecycle.exact_option_kernel === true;
    }
    const syntheticPriceDisclosed = contract.synthetic_price_purpose.includes(
        "not market evidence",
    ) && contract.synthetic_price_keys.every((key) => {
        const price = spec.economy.prices?.[key] ??
            spec.economy.manual_overrides?.[key];
        return finiteNumber(price) && price > 0;
    });
    const selected = syntheticPriceDisclosed &&
        operationContracts.length > 0 && operationContracts.every(
            (operation) => operation.present === true,
        );
    const stages: Record<string, boolean> = {
        registered,
        legal_carrier_admitted: admitted,
        scheduled,
        exact_row_materialized: materialized,
        selected_under_disclosed_synthetic_price: selected,
        compiled: compileStatus === "compiled",
        independently_exact_evaluated: exactEvaluationStatus === "matched",
    };
    const failureStage = Object.entries(stages).find(([, passed]) => !passed)
        ?.[0] ?? null;
    return {
        id: contract.id,
        registered_action_ids: [...contract.registered_action_ids].sort(),
        synthetic_price_keys: [...contract.synthetic_price_keys].sort(),
        synthetic_price_purpose: contract.synthetic_price_purpose,
        synthetic_price_disclosed: syntheticPriceDisclosed,
        stages,
        passed: failureStage === null,
        failure_stage: failureStage,
    };
}

function materialRatioContractReport(
    spec: SolverBenchmarkCase,
    exactEvaluation: Record<string, unknown> | null,
    verification: Record<string, unknown> | null,
): Record<string, unknown> | null {
    const contract = spec.material_ratio_contract;
    if (contract === undefined) return null;
    const exactResult = nestedRecord(exactEvaluation, "result");
    const expectedConsumption = exactResult?.expected_consumption;
    let exactNumerator = 0;
    let exactDenominator = 0;
    if (Array.isArray(expectedConsumption)) {
        for (const value of expectedConsumption) {
            const row = nestedRecord(value);
            if (!row || typeof row.quantity !== "number") continue;
            if (row.key === contract.numerator_price_key) {
                exactNumerator = row.quantity;
            } else if (row.key === contract.denominator_price_key) {
                exactDenominator = row.quantity;
            }
        }
    }
    const exactPricingComplete =
        nestedString(exactResult, "accounting", "pricing", "status") ===
            "complete" &&
        Array.isArray(
            nestedRecord(exactResult, "accounting", "pricing")
                ?.missing_price_keys,
        ) &&
        (nestedRecord(exactResult, "accounting", "pricing")
            ?.missing_price_keys as unknown[]).length === 0;
    const exactExpected = contract.expected_ratio * exactDenominator;
    const exactTolerance = 1e-10 * Math.max(
        1,
        Math.abs(exactNumerator),
        Math.abs(exactExpected),
    );
    const exactChecked = exactResult !== null;
    const exactPassed = exactChecked &&
        (!contract.require_positive_denominator || exactDenominator > 0) &&
        Math.abs(exactNumerator - exactExpected) <= exactTolerance &&
        (!contract.require_complete_pricing || exactPricingComplete);

    const sampledAccounting = nestedRecord(
        verification,
        "simulation",
        "sampled_accounting",
    );
    let sampledNumerator = 0;
    let sampledDenominator = 0;
    if (Array.isArray(sampledAccounting?.materials)) {
        for (const value of sampledAccounting.materials) {
            const row = nestedRecord(value);
            if (!row || typeof row.count !== "number") continue;
            if (row.price_key === contract.numerator_price_key) {
                sampledNumerator = row.count;
            } else if (row.price_key === contract.denominator_price_key) {
                sampledDenominator = row.count;
            }
        }
    }
    const summary = nestedRecord(verification, "simulation", "summary");
    const sampledPricingComplete = summary?.cost_status === "complete" &&
        summary.missing_price_run_count === 0 &&
        summary.missing_price_action_count === 0;
    const sampledExpected = contract.expected_ratio * sampledDenominator;
    const sampledTolerance = 1e-10 * Math.max(
        1,
        Math.abs(sampledNumerator),
        Math.abs(sampledExpected),
    );
    const sampledChecked = sampledAccounting !== null;
    const sampledPassed = sampledChecked &&
        (!contract.require_positive_denominator || sampledDenominator > 0) &&
        Math.abs(sampledNumerator - sampledExpected) <= sampledTolerance &&
        (!contract.require_complete_pricing || sampledPricingComplete);
    return {
        numerator_price_key: contract.numerator_price_key,
        denominator_price_key: contract.denominator_price_key,
        expected_ratio: contract.expected_ratio,
        checked: exactChecked && sampledChecked,
        passed: exactPassed && sampledPassed,
        exact_evaluation: {
            checked: exactChecked,
            numerator_quantity: exactNumerator,
            denominator_quantity: exactDenominator,
            complete_pricing: exactPricingComplete,
            passed: exactPassed,
        },
        sampled_simulation: {
            checked: sampledChecked,
            numerator_count: sampledNumerator,
            denominator_count: sampledDenominator,
            complete_pricing: sampledPricingComplete,
            passed: sampledPassed,
        },
    };
}

function buildCapChecks(spec: SolverBenchmarkCase, report: CaseReport): CaseReport["cap_checks"] {
    const checks: Record<string, boolean> = {};
    const optimization = report.solver_telemetry?.optimization;
    const reportedCaps =
        optimization && typeof optimization === "object" &&
        Array.isArray((optimization as Record<string, unknown>).cap_hits)
            ? (optimization as Record<string, unknown>).cap_hits as unknown[]
            : [];
    const check = (name: string, actual: number | null, cap: number): void => {
        if (actual !== null) {
            checks[name] =
                actual <= cap || reportedCaps.includes(name);
        }
    };
    check("max_discovered_states", nestedNumber(report.solver_telemetry, "states", "discovered"), spec.caps.max_discovered_states);
    check("max_expanded_states", nestedNumber(report.solver_telemetry, "states", "expanded"), spec.caps.max_expanded_states);
    check("max_state_action_rows", nestedNumber(report.solver_telemetry, "work", "state_action_rows"), spec.caps.max_state_action_rows);
    check("max_transitions", nestedNumber(report.solver_telemetry, "work", "transition_entries"), spec.caps.max_transitions);
    check("max_reforge_work", nestedNumber(report.solver_telemetry, "work", "reforge_work"), spec.caps.max_reforge_work);
    check("max_solver_owned_bytes", nestedNumber(report.solver_telemetry, "memory", "solver_owned_bytes_estimate"), spec.caps.max_solver_owned_bytes);
    check("max_compiled_nodes", report.compiled_graph?.nodes ?? null, spec.caps.max_compiled_nodes);
    check("max_compiled_edges", report.compiled_graph?.edges ?? null, spec.caps.max_compiled_edges);
    check("max_strategy_json_bytes", report.compiled_graph?.strategy_json_bytes ?? null, spec.caps.max_strategy_json_bytes);
    check("worker_step_ms", report.execution.worker_max_slice_ms, spec.caps.worker_step_ms);
    if (spec.watchdog_seconds !== undefined) {
        check(
            "watchdog_seconds",
            report.phase_wall_ms.total === null
                ? null
                : report.phase_wall_ms.total / 1000,
            spec.watchdog_seconds,
        );
        checks.watchdog_not_expired = !report.execution.watchdog_expired;
    }
    if (spec.expected?.solve_status === "cancelled") {
        check("cancel_ack_ms", report.execution.cancellation_ack_ms, spec.caps.cancel_ack_ms);
    }
    return { all_passed: Object.values(checks).every(Boolean), checks };
}

function caseExpectationMet(
    spec: SolverBenchmarkCase,
    report: CaseReport,
    skipVerification: boolean,
): boolean {
    if (report.errors.length > 0) return false;
    if (report.solver_telemetry?.version !== "solver_telemetry_v1") return false;
    if (!report.cap_checks.all_passed) return false;
    if (spec.bounded_best_policy_contract !== undefined &&
        report.bounded_best_policy_contract?.passed !== true) {
        return false;
    }
    if (spec.mechanic_family_control !== undefined &&
        report.mechanic_family_control?.passed !== true) {
        return false;
    }
    if (report.compiled_operation_contracts.some(
        (contract) => contract.present !== true,
    )) {
        return false;
    }
    if (spec.market_price_override_contracts !== undefined &&
        report.market_price_override_contracts?.checked !== true) {
        return false;
    }
    if (spec.material_ratio_contract !== undefined) {
        const exact = nestedRecord(
            report.material_ratio_contract,
            "exact_evaluation",
        );
        if (exact?.passed !== true ||
            (!skipVerification &&
             report.material_ratio_contract?.passed !== true)) {
            return false;
        }
    }
    if (
        report.solve_summary?.policy_available === true &&
        spec.verification.exact_evaluation === true &&
        report.workflow_status.exact_evaluation !== "matched"
    ) {
        return false;
    }
    if (spec.expected === undefined) {
        const solve = report.solve_summary;
        if (solve === null) return false;
        if (solve.policy_available === true) {
            if (!report.compiled_graph) return false;
            if (
                spec.verification.exact_evaluation === true &&
                report.workflow_status.exact_evaluation !== "matched"
            ) {
                return false;
            }
            if (
                !skipVerification &&
                spec.verification.runs > 0 &&
                (report.workflow_status.simulation !== "completed" ||
                    report.verification?.verification_passed !== true ||
                    report.verification.runs !== spec.verification.runs ||
                    report.verification.requested_runs !==
                        spec.verification.runs)
            ) {
                return false;
            }
        } else if (
            solve.stop_cause === "none" &&
            solve.termination !== "no_executable_policy"
        ) {
            return false;
        }
        return true;
    }
    if (!expectationMet(spec.expected.solve_status, report.actual_status)) return false;
    for (const section of [
        "availability",
        "actions",
        "policy_compatibility",
        "abstraction",
        "states",
        "work",
        "cache",
        "optimization",
        "timings_ns",
        "memory",
        "compilation",
        "value",
    ]) {
        const value = report.solver_telemetry[section];
        if (!value || typeof value !== "object" || Array.isArray(value)) {
            return false;
        }
    }
    if (!report.cap_checks.all_passed) return false;
    if (report.execution.worker_max_slice_ms !== null &&
        report.execution.worker_max_slice_ms > spec.caps.worker_step_ms) {
        return false;
    }
    if (spec.expected.solve_status === "cancelled" &&
        (report.execution.cancellation_ack_ms === null ||
            report.execution.cancellation_ack_ms > spec.caps.cancel_ack_ms)) {
        return false;
    }
    if (spec.expected.optimality_status === "exact" &&
        nestedString(report.solver_telemetry, "optimization", "status") !==
            "exact_abstract") return false;
    if (spec.expected.compile_status === "compiled" && !report.compiled_graph) {
        return false;
    }
    if (!skipVerification && spec.expected.verification_status === "run") {
        if (!report.verification ||
            report.verification.verification_passed !== true ||
            report.verification.runs !== spec.verification.runs ||
            report.verification.requested_runs !== spec.verification.runs) {
            return false;
        }
    }
    if (!skipVerification &&
        spec.expected.verification_status === "run_if_compiled" &&
        report.compiled_graph &&
        (!report.verification ||
            report.verification.verification_passed !== true ||
            report.verification.runs !== spec.verification.runs ||
            report.verification.requested_runs !== spec.verification.runs)) {
        return false;
    }
    return true;
}

async function runCase(
    client: EngineClient,
    data: number,
    spec: SolverBenchmarkCase,
    skipVerification: boolean,
    verificationRunsOverride?: number,
    progressOutputPath?: string,
    solveOnly = false,
): Promise<CaseReport> {
    const totalStarted = performance.now();
    const errors: string[] = [];
    let session = 0;
    let item = 0;
    let solver = 0;
    let economy = 0;
    let strategyHandle = 0;
    let simulator = 0;
    let solve: SolverSolveResult | null = null;
    let telemetry: SolverTelemetry | null = null;
    let compiledGraph: CaseReport["compiled_graph"] = null;
    let exactEvaluation: CaseReport["exact_evaluation"] = null;
    let verification: CaseReport["verification"] = null;
    let productActionIds: string[] = [];
    let preparedStrategy: CaseReport["prepared_strategy"] = null;
    let forcedPriceChecked = false;
    let dependencyCandidateChecked = false;
    let registryLayoutMs: number | null = null;
    let solveMs: number | null = null;
    let compileMs: number | null = null;
    let verificationMs: number | null = null;
    let cancellationAckMs: number | null = null;
    let solveError: string | null = null;
    const progressTrace: Array<Record<string, unknown>> = [];
    const persistProgressTrace = (reason: string): void => {
        if (!progressOutputPath) return;
        mkdirSync(dirname(progressOutputPath), { recursive: true });
        writeFileSync(
            progressOutputPath,
            `${JSON.stringify({
                schema_version: "solver_progress_trace_v1",
                case_id: spec.id,
                reason,
                captured_at_utc: new Date().toISOString(),
                trace: progressTrace,
            }, null, 2)}\n`,
        );
    };
    let memoryBefore = 0;
    let memoryAfter = 0;
    let wasmBefore = 0;
    let wasmAfter = 0;
    let rssPeak = 0;
    let rssSampler: ReturnType<typeof startRssSampler> | null = null;
    let compileStatus = "not_attempted";
    let exactEvaluationStatus = spec.verification.exact_evaluation
        ? "not_attempted"
        : "not_requested";
    const verificationRuns =
        verificationRunsOverride ?? spec.verification.runs;
    let simulationStatus = verificationRuns > 0
        ? "not_attempted"
        : "not_requested";
    const caseAbort = new AbortController();
    let watchdogExpired = false;
    // The worker consumes this abort at its next event-loop yield. The separate
    // worker_step_ms contract bounds any one in-flight native/WASM slice.
    const watchdogTimer = spec.watchdog_seconds === undefined
        ? null
        : setTimeout(() => {
            watchdogExpired = true;
            persistProgressTrace("cooperative_watchdog_fired");
            caseAbort.abort("solver benchmark watchdog expired");
        }, spec.watchdog_seconds * 1000);
    watchdogTimer?.unref();

    try {
        wasmBefore = (await client.memoryStats()).wasm_memory_bytes;
        rssSampler = startRssSampler();
        memoryBefore = rssSampler.before;
        const registryStarted = performance.now();
        session = await client.createSession(
            data,
            spec.session.base_metadata_path,
            spec.session.item_level,
        );
        item = await client.createItem(session, {
            rarity: spec.start.rarity,
            withImplicits: spec.start.with_implicits,
        });
        for (const mod of spec.start.mods) {
            await client.addMod(item, session, {
                key: mod.key,
                fractured: mod.flags.includes("fractured") || undefined,
                crafted: mod.flags.includes("crafted") || undefined,
                veiled: mod.flags.includes("veiled") || undefined,
            });
        }
        if ((spec.start.searing_exarch_tier ?? 0) !== 0 ||
            (spec.start.eater_of_worlds_tier ?? 0) !== 0 ||
            (spec.start.generic_influence_bits ?? 0) !== 0) {
            const exported = await client.exportItem(item) as Record<string, unknown>;
            exported.searing_exarch_tier =
                spec.start.searing_exarch_tier ?? 0;
            exported.eater_of_worlds_tier =
                spec.start.eater_of_worlds_tier ?? 0;
            exported.generic_influence_bits =
                spec.start.generic_influence_bits ?? 0;
            const imported = await client.importItem(exported);
            await client.closeItem(item);
            item = imported;
        }
        const economySpec = materializeSolverBenchmarkEconomy(
            spec,
            REPO_ROOT,
        );
        forcedPriceChecked = spec.forced_winner_contract !== undefined;
        const economyPrices = nestedRecord(economySpec, "prices");
        if (economyPrices === null) {
            throw new Error("materialized benchmark economy has no prices object");
        }
        if (spec.product_action_envelope !== undefined) {
            const envelopeSolver = await client.openSolver(
                session,
                spec.product_action_envelope.envelope_goal,
            );
            try {
                const candidates = await client.solverActions(envelopeSolver);
                productActionIds = candidates
                    .filter((candidate) => candidate.cost_keys.every(
                        (key) => Object.hasOwn(economyPrices, key),
                    ))
                    .map((candidate) => candidate.id);
            } finally {
                await client.closeSolver(envelopeSolver).catch(() => {});
            }
        }
        solver = await client.openSolver(
            session,
            materializeSolverBenchmarkGoal(spec, productActionIds),
        );
        await client.solverActions(solver);
        const forcedWinner = spec.forced_winner_contract;
        dependencyCandidateChecked = forcedWinner !== undefined &&
            forcedWinner.dependency_must_not_be_primitive_candidate;
        if (forcedWinner?.dependency_must_not_be_primitive_candidate &&
            productActionIds.includes(forcedWinner.dependency_action_id)) {
            throw new Error(
                "forced winner dependency unexpectedly appeared as a primitive product candidate",
            );
        }
        economy = await client.loadEconomy(
            economySpec,
        );
        registryLayoutMs = roundMs(performance.now() - registryStarted);

        const solveStarted = performance.now();
        const recordSolveProgress = (progress: SolveProgress): void => {
            progressTrace.push({
                elapsed_ms: roundMs(performance.now() - solveStarted),
                ...progress,
            });
        };
        try {
            if (spec.benchmark_mode === "cancel_after_first_step") {
                let abortStarted: number | null = null;
                solve = await client.solverSolve(
                    solver,
                    item,
                    economy,
                    {
                        solve_profile: spec.caps.solve_profile,
                        max_states: spec.caps.max_states,
                        max_sweeps: spec.caps.max_sweeps,
                        max_absolute_optimality_gap:
                            spec.caps.max_absolute_optimality_gap,
                        max_relative_optimality_gap:
                            spec.caps.max_relative_optimality_gap,
                        max_discovered_states: spec.caps.max_discovered_states,
                        max_expanded_states: spec.caps.max_expanded_states,
                        max_state_action_rows: spec.caps.max_state_action_rows,
                        max_transitions: spec.caps.max_transitions,
                        max_reforge_work: spec.caps.max_reforge_work,
                        max_solver_owned_bytes: spec.caps.max_solver_owned_bytes,
                        max_compiled_nodes: spec.caps.max_compiled_nodes,
                        max_compiled_edges: spec.caps.max_compiled_edges,
                        max_strategy_json_bytes: spec.caps.max_strategy_json_bytes,
                        max_diagnostic_samples: spec.caps.max_diagnostic_samples,
                        max_telemetry_json_bytes: spec.caps.max_telemetry_json_bytes,
                        max_policy_refinement_states:
                            spec.caps.max_policy_refinement_states,
                        full_evidence: spec.caps.full_evidence,
                        strict_states: spec.caps.strict_states,
                        kernel_reuse: spec.caps.kernel_reuse,
                        goal_progress_gated_reforges:
                            spec.caps.goal_progress_gated_reforges,
                        allow_economic_restart:
                            spec.caps.allow_economic_restart,
                        high_impact_executable_uppers:
                            spec.caps.high_impact_executable_uppers,
                        consider_imprint_programs:
                            benchmarkConsiderImprintPrograms(spec),
                    },
                    {
                        chunkSize: spec.caps.solve_step_work_items,
                        yieldEveryStep: true,
                        signal: caseAbort.signal,
                        onProgress: (progress) => {
                            recordSolveProgress(progress);
                            if (abortStarted === null && !progress.done) {
                                abortStarted = performance.now();
                                caseAbort.abort("cancel-after-first-step benchmark");
                            }
                        },
                    },
                );
                if (abortStarted !== null) {
                    cancellationAckMs = roundMs(performance.now() - abortStarted);
                }
            } else {
                solve = await client.solverSolve(
                    solver,
                    item,
                    economy,
                    {
                        solve_profile: spec.caps.solve_profile,
                        max_states: spec.caps.max_states,
                        max_sweeps: spec.caps.max_sweeps,
                        max_absolute_optimality_gap:
                            spec.caps.max_absolute_optimality_gap,
                        max_relative_optimality_gap:
                            spec.caps.max_relative_optimality_gap,
                        max_discovered_states: spec.caps.max_discovered_states,
                        max_expanded_states: spec.caps.max_expanded_states,
                        max_state_action_rows: spec.caps.max_state_action_rows,
                        max_transitions: spec.caps.max_transitions,
                        max_reforge_work: spec.caps.max_reforge_work,
                        max_solver_owned_bytes: spec.caps.max_solver_owned_bytes,
                        max_compiled_nodes: spec.caps.max_compiled_nodes,
                        max_compiled_edges: spec.caps.max_compiled_edges,
                        max_strategy_json_bytes: spec.caps.max_strategy_json_bytes,
                        max_diagnostic_samples: spec.caps.max_diagnostic_samples,
                        max_telemetry_json_bytes: spec.caps.max_telemetry_json_bytes,
                        max_policy_refinement_states:
                            spec.caps.max_policy_refinement_states,
                        full_evidence: spec.caps.full_evidence,
                        strict_states: spec.caps.strict_states,
                        kernel_reuse: spec.caps.kernel_reuse,
                        goal_progress_gated_reforges:
                            spec.caps.goal_progress_gated_reforges,
                        allow_economic_restart:
                            spec.caps.allow_economic_restart,
                        high_impact_executable_uppers:
                            spec.caps.high_impact_executable_uppers,
                        consider_imprint_programs:
                            benchmarkConsiderImprintPrograms(spec),
                    },
                    {
                        chunkSize: spec.caps.solve_step_work_items,
                        boundedFinishAfterMs:
                            spec.requested_bounded_finish_seconds === undefined
                                ? undefined
                                : spec.requested_bounded_finish_seconds * 1000,
                        signal: caseAbort.signal,
                        onProgress: recordSolveProgress,
                    },
                );
            }
        } catch (error) {
            solveError = errorDetail(error);
            errors.push(`solve: ${solveError}`);
        }
        solveMs = roundMs(performance.now() - solveStarted);
        telemetry = await safeTelemetry(client, solver, errors);

        if (!solveOnly && solve && !solve.cancelled && solve.policy_available) {
            const compileStarted = performance.now();
            try {
                const raw = await client.solverCompileStrategy(solver);
                if (!isStrategyDocument(raw)) {
                    throw new Error(
                        "The solver returned an invalid strategy document.",
                    );
                }
                const strategyJson = JSON.stringify(raw);
                compiledGraph = {
                    nodes: raw.nodes.length,
                    edges: raw.edges.length,
                    strategy_json_bytes: Buffer.byteLength(strategyJson),
                };
                const strategy = prepareSolverStrategy(raw);
                const preparedJson = JSON.stringify(strategy);
                const reloaded = JSON.parse(preparedJson) as unknown;
                if (!isStrategyDocument(reloaded)) {
                    throw new Error(
                        "The saved compiled strategy did not reload as a "
                        + "Strategy Board document.",
                    );
                }
                preparedStrategy = {
                    sha256: createHash("sha256")
                        .update(preparedJson)
                        .digest("hex"),
                    json_bytes: Buffer.byteLength(preparedJson),
                    document: reloaded,
                };
                const forcedWinner = spec.forced_winner_contract;
                if (forcedWinner !== undefined &&
                    matchingOperationNodes(
                        reloaded,
                        forcedWinner.required_compiled_operation_type,
                    ) === 0) {
                    throw new Error(
                        "compiled strategy omitted the forced winner operation type",
                    );
                }
                for (const operationContract of
                    compiledOperationContracts(spec)) {
                    if (matchingOperationNodes(
                        reloaded,
                        operationContract.type,
                        operationContract.required_string_parameters,
                    ) === 0) {
                        throw new Error(
                            "compiled strategy omitted the required operation "
                            + `contract: ${operationContract.type}`,
                        );
                    }
                }
                strategyHandle = await client.compileStrategy(
                    session,
                    reloaded,
                );
                compileStatus = "compiled";
                if (spec.verification.exact_evaluation) {
                    exactEvaluationStatus = "running";
                    const exact = await client.strategyEvaluate(
                        session,
                        reloaded,
                        {
                            max_states:
                                spec.verification.exact_max_states,
                            max_pairs:
                                spec.verification.exact_max_pairs,
                            max_transitions:
                                spec.verification.exact_max_transitions,
                            max_owned_bytes:
                                spec.verification.exact_max_owned_bytes,
                        },
                        {
                            economy: economySpec,
                            signal: caseAbort.signal,
                        },
                    );
                    const offPolicy =
                        exact.terminals.failure +
                        exact.terminals.stop +
                        exact.terminals.action_not_applied +
                        exact.terminals.no_matching_edge +
                        exact.terminals.unresolved;
                    const exactCost =
                        exact.accounting.totals.per_invocation
                            .total_expected_cost;
                    const solverCost = solve.evaluated_policy_cost;
                    const absolute =
                        exactCost !== null && solverCost !== null
                            ? Math.abs(exactCost - solverCost)
                            : Number.POSITIVE_INFINITY;
                    const relative =
                        solverCost !== null && Math.abs(solverCost) > 1e-12
                            ? absolute / Math.abs(solverCost)
                            : absolute;
                    const absoluteTolerance =
                        spec.verification
                            .exact_cost_absolute_tolerance ?? 1e-7;
                    const relativeTolerance =
                        spec.verification
                            .exact_cost_relative_tolerance ?? 1e-9;
                    const reconciled =
                        absolute <= absoluteTolerance ||
                        relative <= relativeTolerance;
                    const matched =
                        exact.converged &&
                        exact.accounting.totals.per_invocation
                            .cost_complete &&
                        exact.terminals.success >= 1 - 1e-10 &&
                        offPolicy <= 1e-10 &&
                        reconciled;
                    exactEvaluationStatus = matched
                        ? "matched"
                        : "mismatch";
                    exactEvaluation = {
                        result: exact,
                        converged: exact.converged,
                        cost_complete:
                            exact.accounting.totals.per_invocation
                                .cost_complete,
                        success_probability: exact.terminals.success,
                        off_policy_mass: offPolicy,
                        total_expected_cost: exactCost,
                        solver_policy_cost: solverCost,
                        cost_delta_absolute: Number.isFinite(absolute)
                            ? absolute
                            : null,
                        cost_delta_relative: Number.isFinite(relative)
                            ? relative
                            : null,
                        cost_reconciled: reconciled,
                    };
                    if (!matched) {
                        errors.push(
                            "exact evaluation: compiled policy did not "
                            + "reconcile with the solver policy",
                        );
                    }
                }
                telemetry = await safeTelemetry(client, solver, errors);
                const engineStrategyBytes = nestedNumber(
                    telemetry, "compilation", "strategy_json_bytes");
                if (compiledGraph && engineStrategyBytes !== null) {
                    compiledGraph.strategy_json_bytes = engineStrategyBytes;
                }
            } catch (error) {
                if (compileStatus !== "compiled") {
                    compileStatus = "compile_failed";
                } else if (exactEvaluationStatus === "running") {
                    exactEvaluationStatus = "evaluation_failed";
                }
                errors.push(`compile: ${errorDetail(error)}`);
                telemetry = await safeTelemetry(client, solver, errors);
            }
            compileMs = roundMs(performance.now() - compileStarted);
            if (strategyHandle && verificationRuns > 0 &&
                !skipVerification) {
                simulationStatus = "running";
                const verifyStarted = performance.now();
                try {
                    const verificationStartValue =
                        nestedNumber(telemetry, "value", "start") ??
                        solve.start_value;
                    if (verificationStartValue === null) {
                        errors.push(
                            "verification: executable policy has no evaluated cost",
                        );
                    } else {
                        verification = await verifyStrategy(
                            client,
                            session,
                            economy,
                            strategyHandle,
                            verificationStartValue,
                            spec,
                            verificationRuns,
                            (handle) => { simulator = handle; },
                            caseAbort.signal,
                        );
                        simulationStatus =
                            verification.verification_passed === true
                                ? "completed"
                                : "mismatch";
                    }
                } catch (error) {
                    simulationStatus = "simulation_failed";
                    errors.push(`verification: ${errorDetail(error)}`);
                }
                verificationMs = roundMs(performance.now() - verifyStarted);
            }
        }
    } catch (error) {
        errors.push(`setup: ${errorDetail(error)}`);
    } finally {
        if (simulator) await client.closeSimulator(simulator).catch(() => {});
        if (strategyHandle) await client.closeStrategy(strategyHandle).catch(() => {});
        if (solver) await client.closeSolver(solver).catch(() => {});
        if (economy) await client.closeEconomy(economy).catch(() => {});
        if (item) await client.closeItem(item).catch(() => {});
        if (session) await client.closeSession(session).catch(() => {});
        wasmAfter = (await client.memoryStats().catch(() => null))?.wasm_memory_bytes ?? 0;
        if (rssSampler) {
            const sampled = rssSampler.stop();
            memoryAfter = sampled.after;
            rssPeak = sampled.peak;
        }
        if (watchdogTimer !== null) clearTimeout(watchdogTimer);
        persistProgressTrace(
            watchdogExpired ? "case_cleanup_completed_after_watchdog" : "case_completed",
        );
    }

    const totalMs = roundMs(performance.now() - totalStarted);
    if (spec.watchdog_seconds !== undefined &&
        totalMs > spec.watchdog_seconds * 1000) {
        watchdogExpired = true;
    }
    if (watchdogExpired) {
        errors.push(
            `watchdog: exceeded ${spec.watchdog_seconds} seconds`,
        );
    }

    const actualStatus = statusFrom(
        solve,
        solveError,
        telemetry,
        spec.product_action_envelope !== undefined,
    );
    const worker = solve?.worker;
    const complete = solve && !solve.cancelled ? solve : null;
    const boundedContract = boundedBestPolicyReport(spec, complete, telemetry);
    if (boundedContract !== null && boundedContract.passed !== true) {
        errors.push(
            `bounded best-policy contract failed: ${String(
                boundedContract.failure_reason,
            )}`,
        );
    }
    const forced = spec.forced_winner_contract;
    const forcedOperationNodes = forced !== undefined && preparedStrategy !== null
        ? matchingOperationNodes(
            preparedStrategy.document,
            forced.required_compiled_operation_type,
        )
        : 0;
    const operationContract = spec.compiled_operation_contract;
    const operationContractNodes = operationContract !== undefined &&
        preparedStrategy !== null
        ? matchingOperationNodes(
            preparedStrategy.document,
            operationContract.type,
            operationContract.required_string_parameters,
        )
        : 0;
    const operationContractReports = compiledOperationContracts(spec).map(
        (contract) => {
            const matching = preparedStrategy === null
                ? 0
                : matchingOperationNodes(
                    preparedStrategy.document,
                    contract.type,
                    contract.required_string_parameters,
                );
            return {
                type: contract.type,
                required_string_parameters:
                    contract.required_string_parameters,
                checked: preparedStrategy !== null,
                matching_node_count: matching,
                present: matching > 0,
            };
        },
    );
    const mechanicFamilyControl = mechanicFamilyControlReport(
        spec,
        telemetry,
        operationContractReports,
        compileStatus,
        exactEvaluationStatus,
    );
    if (mechanicFamilyControl !== null &&
        mechanicFamilyControl.passed !== true) {
        errors.push(
            `mechanic family control ${String(mechanicFamilyControl.id)} lost stage ${String(mechanicFamilyControl.failure_stage)}`,
        );
    }
    const materialRatio = materialRatioContractReport(
        spec,
        exactEvaluation,
        verification,
    );
    if (!skipVerification && materialRatio !== null &&
        materialRatio.passed !== true) {
        errors.push(
            "material ratio contract failed exact or sampled accounting",
        );
    }
    const report: CaseReport = {
        id: spec.id,
        category: spec.category,
        approval_status: spec.approval_status,
        benchmark_enabled: true,
        expected: spec.expected ?? null,
        actual_status: actualStatus,
        workflow_status: {
            solve_result_class:
                complete === null
                    ? "not_available"
                    : complete.cap_hit_mask !== 0
                      ? complete.policy_available
                          ? "capped_with_policy"
                          : "capped_without_policy"
                      : complete.policy_status === "exact"
                        ? "exact"
                        : complete.policy_available
                          ? "bounded"
                          : complete.termination === "no_executable_policy"
                            ? "no_executable_policy"
                            : "no_policy",
            compile: complete?.policy_available
                ? compileStatus
                : "not_applicable_no_policy",
            exact_evaluation: complete?.policy_available
                ? exactEvaluationStatus
                : "not_applicable_no_policy",
            simulation: complete?.policy_available
                ? simulationStatus
                : "not_applicable_no_policy",
        },
        expectation_met: false,
        verification_skipped: skipVerification,
        input: {
            comparison_profile:
                spec.comparison_profile ?? "native-wasm-solver-v1",
            watchdog_seconds: spec.watchdog_seconds ?? null,
            requested_bounded_finish_seconds:
                spec.requested_bounded_finish_seconds ?? null,
            session: spec.session,
            start: spec.start,
            goal: spec.goal,
            product_action_envelope: spec.product_action_envelope ?? null,
            allowed_mechanic_families: spec.allowed_mechanic_families,
            mechanic_family_control: spec.mechanic_family_control ?? null,
            compiled_operation_contract:
                spec.compiled_operation_contract ?? null,
            compiled_operation_contracts:
                spec.compiled_operation_contracts ?? null,
            market_price_override_contracts:
                spec.market_price_override_contracts ?? null,
            material_ratio_contract: spec.material_ratio_contract ?? null,
            forced_winner_contract: spec.forced_winner_contract ?? null,
            bounded_best_policy_contract:
                spec.bounded_best_policy_contract ?? null,
            economy: spec.economy,
            caps: spec.caps,
            verification: spec.verification,
        },
        phase_wall_ms: {
            registry_layout: registryLayoutMs,
            solve: solveMs,
            compile: compileMs,
            verification: verificationMs,
            total: totalMs,
        },
        execution: {
            solve_steps: worker?.step_count ?? null,
            max_solve_step_ms: worker ? roundMs(worker.max_step_ms) : null,
            worker_max_slice_ms: worker ? roundMs(worker.max_step_ms) : null,
            finalization_ms: worker
                ? roundMs(worker.finalization_ms)
                : null,
            cancellation_ack_ms: cancellationAckMs,
            cancellation_mode:
                spec.benchmark_mode === "cancel_after_first_step"
                    ? "abort_signal_after_worker_progress"
                    : null,
            cooperative_abandon_ms: null,
            watchdog_seconds: spec.watchdog_seconds ?? null,
            watchdog_mode: spec.watchdog_seconds === undefined
                ? null
                : "cooperative_abort_signal",
            watchdog_expired: watchdogExpired,
        },
        progress_trace: progressTrace,
        memory: {
            measurement_kind: "node_process_rss_sampled_5ms_and_wasm_heap_snapshots",
            process_working_set_before_bytes: memoryBefore || null,
            process_working_set_after_bytes: memoryAfter || null,
            process_working_set_delta_bytes:
                memoryBefore && memoryAfter ? memoryAfter - memoryBefore : null,
            process_working_set_peak_bytes: rssPeak || null,
            wasm_heap_before_bytes: wasmBefore || null,
            wasm_heap_after_bytes: wasmAfter || null,
            wasm_heap_growth_bytes:
                wasmBefore && wasmAfter ? wasmAfter - wasmBefore : null,
        },
        solve_summary: complete
            ? { ...complete, progress: undefined, worker: undefined }
            : null,
        solver_telemetry: telemetry,
        compiled_graph: compiledGraph,
        product_action_ids: productActionIds,
        compiled_operation_contract: operationContract === undefined
            ? null
            : {
                type: operationContract.type,
                required_string_parameters:
                    operationContract.required_string_parameters,
                checked: preparedStrategy !== null,
                matching_node_count: operationContractNodes,
                present: operationContractNodes > 0,
            },
        compiled_operation_contracts: operationContractReports,
        market_price_override_contracts:
            spec.market_price_override_contracts === undefined
                ? null
                : {
                    checked: true,
                    declared_override_count:
                        spec.market_price_override_contracts.length,
                },
        material_ratio_contract: materialRatio,
        forced_winner_contract: forced === undefined
            ? null
            : {
                required_compiled_operation: {
                    type: forced.required_compiled_operation_type,
                    checked: preparedStrategy !== null,
                    matching_node_count: forcedOperationNodes,
                    present: forcedOperationNodes > 0,
                },
                synthetic_action_price: {
                    snapshot_quote: forced.snapshot_action_price,
                    forced_value: forced.forced_action_price,
                    checked: forcedPriceChecked,
                },
                dependency_primitive_candidate: {
                    action_id: forced.dependency_action_id,
                    must_be_absent:
                        forced.dependency_must_not_be_primitive_candidate,
                    checked: dependencyCandidateChecked,
                    absent: !productActionIds.includes(
                        forced.dependency_action_id,
                    ),
                },
            },
        bounded_best_policy_contract: boundedContract,
        mechanic_family_control: mechanicFamilyControl,
        prepared_strategy: preparedStrategy,
        exact_evaluation: exactEvaluation,
        value: {
            start: complete
                ? nestedNumber(telemetry, "value", "start") ?? complete.start_value
                : null,
        },
        verification,
        cap_checks: { all_passed: true, checks: {} },
        errors,
    };
    report.cap_checks = buildCapChecks(spec, report);
    report.expectation_met = caseExpectationMet(
        spec, report, skipVerification);
    return report;
}

async function verifyStrategy(
    client: EngineClient,
    session: number,
    economy: number,
    strategyHandle: number,
    startValue: number,
    spec: SolverBenchmarkCase,
    verificationRuns: number,
    setSimulator: (handle: number) => void,
    signal: AbortSignal,
): Promise<Record<string, unknown>> {
    const simulator = await client.createSimulator(session, strategyHandle, economy);
    setSimulator(simulator);
    const run = await client.runStrategy(simulator, {
        target_runs: verificationRuns,
        seed: spec.verification.seed,
        max_actions_per_run: spec.verification.max_actions_per_run,
        max_graph_steps_per_run:
            spec.verification.max_graph_steps_per_run ?? 0,
    }, {
        // Qualification owns the complete deterministic sample and has a
        // case-level watchdog. One native chunk avoids paying worker yields
        // and repeated result/progress serialization inside a 10,000-run
        // release comparison; interactive product simulations retain their
        // adaptive cancellable chunks.
        chunkSize: verificationRuns,
        signal,
    });
    if (run.cancelled) throw new Error("simulation verification was cancelled");
    const completed = run.summary.completed_runs;
    const meanCost = completed > 0
        ? run.summary.known_total_cost / completed
        : Number.NaN;
    const absolute = Math.abs(meanCost - startValue);
    const relative = startValue !== 0 ? absolute / Math.abs(startValue) : absolute;
    const successRate = completed > 0 ? run.summary.success_count / completed : 0;
    const offPolicy = run.summary.no_matching_edge_count +
        run.summary.action_not_applied_count;
    const fullFixtureRun = completed === spec.verification.runs &&
        verificationRuns === spec.verification.runs &&
        run.summary.target_runs === spec.verification.runs &&
        run.summary.seed === spec.verification.seed;
    const completeCostAccounting = run.summary.cost_status === "complete" &&
        run.summary.missing_price_run_count === 0 &&
        run.summary.missing_price_action_count === 0;
    const meanWithinTolerance = spec.verification.mean_cost_absolute_tolerance !== undefined
        ? absolute <= spec.verification.mean_cost_absolute_tolerance
        : spec.verification.mean_cost_relative_tolerance !== undefined
          ? relative <= spec.verification.mean_cost_relative_tolerance
          : null;
    return {
        simulation: {
            summary: run.summary,
            sampled_accounting: run.sampled_accounting,
        },
        runs: completed,
        requested_runs: verificationRuns,
        success_count: run.summary.success_count,
        failure_count: run.summary.failure_count,
        mean_cost: meanCost,
        off_policy_failures: offPolicy,
        cost_delta_absolute: absolute,
        cost_delta_relative: relative,
        success_rate: successRate,
        mean_cost_within_tolerance: meanWithinTolerance,
        success_rate_within_tolerance:
            spec.verification.minimum_success_rate === undefined
                ? null
                : successRate >= spec.verification.minimum_success_rate,
        verification_passed:
            fullFixtureRun &&
            completeCostAccounting &&
            offPolicy === 0 &&
            meanWithinTolerance !== false &&
            (spec.verification.minimum_success_rate === undefined ||
                successRate >= spec.verification.minimum_success_rate),
    };
}

const options = parseArgs(process.argv.slice(2));
const corpus = loadSolverBenchmarkCorpus(options.corpus);
if (options.caseId && !corpus.cases.some((entry) => entry.id === options.caseId)) {
    throw new Error(`unknown solver benchmark case: ${options.caseId}`);
}

const artifactManifestPath = resolve(
    REPO_ROOT,
    corpus.manifest.artifact.manifest_relative_path,
);
const artifactManifest = JSON.parse(readText(artifactManifestPath)) as unknown;
const { client, worker } = spawnClient();
let data = 0;
try {
    await client.whenReady();
    validateCorpusArtifactPins(
        corpus.manifest,
        artifactManifest,
        client.getAbiVersion(),
    );
    const dataLoadStarted = performance.now();
    data = await client.loadData(buildBundle());
    const dataLoadMs = roundMs(performance.now() - dataLoadStarted);
    const reports: CaseReport[] = [];
    for (const spec of corpus.cases) {
        if (options.caseId && spec.id !== options.caseId) continue;
        if (!spec.benchmark_enabled) {
            reports.push(disabledReport(spec));
        } else {
            process.stdout.write(`solver benchmark: ${spec.id}\n`);
            reports.push(await runCase(
                client,
                data,
                applyDiagnosticOverrides(spec, options),
                options.skipVerification,
                options.verificationRuns,
                options.output ? `${options.output}.progress.json` : undefined,
                options.solveOnly,
            ));
        }
    }
    const report = {
        schema_version: "solver_benchmark_report_v1",
        runner: "wasm_worker",
        all_expectations_met: reports.every(
            (entry) => !entry.benchmark_enabled || entry.expectation_met,
        ),
        corpus: {
            id: corpus.manifest.corpus_id,
            schema_version: corpus.manifest.schema_version,
            manifest_path: corpus.path,
            manifest: corpus.manifest,
        },
        artifact: {
            manifest_path: artifactManifestPath,
            manifest: artifactManifest,
        },
        environment: {
            platform: process.platform,
            architecture: process.arch,
            node: process.version,
            cpu: cpus()[0]?.model ?? "unknown",
            logical_cpu_count: cpus().length,
            abi_version: client.getAbiVersion(),
            runtime: "node-worker_threads-wasm",
            data_load_ms: dataLoadMs,
        },
        diagnostic_overrides: {
            solve_step_work_items: options.solveStepWorkItems ?? null,
            high_impact_executable_uppers:
                options.highImpactExecutableUppers ?? null,
            watchdog_seconds: options.watchdogSeconds ?? null,
            solve_only: options.solveOnly,
        },
        cases: reports,
    };
    const payload = `${JSON.stringify(report, null, 2)}\n`;
    if (options.output) {
        mkdirSync(dirname(options.output), { recursive: true });
        writeFileSync(options.output, payload);
        process.stdout.write(`WASM solver benchmark report: ${options.output}\n`);
    } else {
        process.stdout.write(payload);
    }
    if (!report.all_expectations_met) process.exitCode = 1;
} finally {
    if (data) await client.closeData(data).catch(() => {});
    client.dispose();
    await worker.terminate();
}
