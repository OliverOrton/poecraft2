import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { cpus } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { Worker, type TransferListItem } from "node:worker_threads";

import { EngineClient, type EngineTransport } from "../src/app/engine-client";
import {
    EngineError,
    type SolverSolveResult,
    type SolverTelemetry,
} from "../src/app/engine-protocol";
import { prepareSolverStrategy } from "../src/app/solve-workspace";
import {
    loadSolverBenchmarkCorpus,
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
}

interface CaseReport {
    id: string;
    category: string;
    approval_status: string;
    benchmark_enabled: boolean;
    expected: SolverBenchmarkCase["expected"];
    actual_status: string;
    expectation_met: boolean;
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
        cancellation_ack_ms: number | null;
        cancellation_mode: string | null;
        cooperative_abandon_ms: number | null;
    };
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
    value: { start: number | null };
    verification: Record<string, unknown> | null;
    cap_checks: {
        all_passed: boolean;
        checks: Record<string, boolean>;
    };
    errors: string[];
}

function parseArgs(args: string[]): CliOptions {
    let corpus = resolve(REPO_ROOT, "fixtures/solver-benchmarks/v1/manifest.json");
    let output: string | undefined;
    let caseId: string | undefined;
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
        } else {
            throw new Error(`unknown or incomplete argument: ${args[index]}`);
        }
    }
    return { corpus, output, caseId };
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
        expectation_met: true,
        input: {
            comparison_profile:
                spec.execution_backend === "native_unit_fixture"
                    ? "native-unit-only"
                    : "native-wasm-solver-v1",
            session: spec.session ?? null,
            start: spec.start ?? null,
            goal: spec.goal ?? null,
            allowed_mechanic_families: spec.allowed_mechanic_families ?? null,
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
            cancellation_ack_ms: null,
            cancellation_mode: null,
            cooperative_abandon_ms: null,
        },
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
): string {
    if (solve?.cancelled) return "cancelled";
    const stateCapHit = nestedBoolean(telemetry, "optimization", "state_cap_hit");
    const resourceCapHit = nestedBoolean(
        telemetry,
        "optimization",
        "resource_cap_hit",
    );
    const missingPrice = nestedNumber(telemetry, "actions", "missing_price") ?? 0;
    const unsupported = Math.max(
        nestedNumber(telemetry, "actions", "unsupported_observed") ?? 0,
        nestedNumber(telemetry, "actions", "unsupported_requested") ?? 0,
    );
    const fullRequestStatus = nestedString(
        telemetry,
        "optimization",
        "full_request_status",
    );
    if (stateCapHit) return "refused_state_cap";
    if (resourceCapHit) return "refused_resource_cap";
    if (missingPrice > 0 && unsupported > 0) {
        return "refused_missing_price_and_unsupported_action";
    }
    if (missingPrice > 0) return "refused_missing_price";
    if (unsupported > 0) return "refused_unsupported_action";
    if (fullRequestStatus === "incomplete_action_subset") {
        return "incomplete_action_subset";
    }
    if (solve && !solve.cancelled && solve.converged) {
        return solve.skipped_actions > 0 || missingPrice > 0 || unsupported > 0
            ? "converged_filtered_actions"
            : "converged";
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
        if (optimization === "not_converged" && rawStart === 1e12 && goalStates === 0) {
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

function buildCapChecks(spec: SolverBenchmarkCase, report: CaseReport): CaseReport["cap_checks"] {
    const checks: Record<string, boolean> = {};
    const check = (name: string, actual: number | null, cap: number): void => {
        if (actual !== null) checks[name] = actual <= cap;
    };
    check("max_discovered_states", nestedNumber(report.solver_telemetry, "states", "discovered"), spec.caps.max_discovered_states);
    check("max_expanded_states", nestedNumber(report.solver_telemetry, "states", "expanded"), spec.caps.max_expanded_states);
    check("max_state_action_rows", nestedNumber(report.solver_telemetry, "work", "state_action_rows"), spec.caps.max_state_action_rows);
    check("max_transitions", nestedNumber(report.solver_telemetry, "work", "transition_entries"), spec.caps.max_transitions);
    check("max_reforge_work", nestedNumber(report.solver_telemetry, "cache", "reforge", "frontier_work"), spec.caps.max_reforge_work);
    check("max_solver_owned_bytes", nestedNumber(report.solver_telemetry, "memory", "solver_owned_bytes_estimate"), spec.caps.max_solver_owned_bytes);
    check("max_compiled_nodes", report.compiled_graph?.nodes ?? null, spec.caps.max_compiled_nodes);
    check("max_compiled_edges", report.compiled_graph?.edges ?? null, spec.caps.max_compiled_edges);
    check("max_strategy_json_bytes", report.compiled_graph?.strategy_json_bytes ?? null, spec.caps.max_strategy_json_bytes);
    check("worker_step_ms", report.execution.worker_max_slice_ms, spec.caps.worker_step_ms);
    if (spec.expected.solve_status === "cancelled") {
        check("cancel_ack_ms", report.execution.cancellation_ack_ms, spec.caps.cancel_ack_ms);
    }
    return { all_passed: Object.values(checks).every(Boolean), checks };
}

function caseExpectationMet(spec: SolverBenchmarkCase, report: CaseReport): boolean {
    if (!expectationMet(spec.expected.solve_status, report.actual_status)) return false;
    if (report.errors.length > 0) return false;
    if (report.solver_telemetry?.version !== "solver_telemetry_v1") return false;
    for (const section of [
        "availability",
        "actions",
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
    if (spec.expected.verification_status === "run") {
        if (!report.verification || report.verification.verification_passed !== true) {
            return false;
        }
    }
    if (spec.expected.verification_status === "run_if_compiled" &&
        report.compiled_graph &&
        (!report.verification || report.verification.verification_passed !== true)) {
        return false;
    }
    return true;
}

async function runCase(
    client: EngineClient,
    data: number,
    spec: SolverBenchmarkCase,
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
    let verification: CaseReport["verification"] = null;
    let registryLayoutMs: number | null = null;
    let solveMs: number | null = null;
    let compileMs: number | null = null;
    let verificationMs: number | null = null;
    let cancellationAckMs: number | null = null;
    let solveError: string | null = null;
    let memoryBefore = 0;
    let memoryAfter = 0;
    let wasmBefore = 0;
    let wasmAfter = 0;
    let rssPeak = 0;
    let rssSampler: ReturnType<typeof startRssSampler> | null = null;

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
            if (mod.flags.includes("crafted") || mod.flags.includes("veiled")) {
                throw new Error(
                    `benchmark start modifier ${mod.key} requires unsupported worker import flags`,
                );
            }
            await client.addMod(item, session, {
                key: mod.key,
                fractured: mod.flags.includes("fractured") || undefined,
            });
        }
        if ((spec.start.searing_exarch_tier ?? 0) !== 0 ||
            (spec.start.eater_of_worlds_tier ?? 0) !== 0) {
            throw new Error("non-zero Eldritch start tiers are not worker-importable yet");
        }
        solver = await client.openSolver(session, spec.goal);
        await client.solverActions(solver);
        economy = await client.loadEconomy(spec.economy);
        registryLayoutMs = roundMs(performance.now() - registryStarted);

        const solveStarted = performance.now();
        try {
            if (spec.benchmark_mode === "cancel_after_first_step") {
                const controller = new AbortController();
                let abortStarted: number | null = null;
                solve = await client.solverSolve(
                    solver,
                    item,
                    economy,
                    {
                        max_states: spec.caps.max_states,
                        max_sweeps: spec.caps.max_sweeps,
                        max_discovered_states: spec.caps.max_discovered_states,
                        max_expanded_states: spec.caps.max_expanded_states,
                        max_state_action_rows: spec.caps.max_state_action_rows,
                        max_transitions: spec.caps.max_transitions,
                        max_reforge_work: spec.caps.max_reforge_work,
                        max_solver_owned_bytes: spec.caps.max_solver_owned_bytes,
                        max_compiled_nodes: spec.caps.max_compiled_nodes,
                        max_compiled_edges: spec.caps.max_compiled_edges,
                        max_strategy_json_bytes: spec.caps.max_strategy_json_bytes,
                    },
                    {
                        chunkSize: spec.caps.solve_step_work_items,
                        signal: controller.signal,
                        onProgress: (progress) => {
                            if (abortStarted === null && !progress.done) {
                                abortStarted = performance.now();
                                controller.abort();
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
                        max_states: spec.caps.max_states,
                        max_sweeps: spec.caps.max_sweeps,
                        max_discovered_states: spec.caps.max_discovered_states,
                        max_expanded_states: spec.caps.max_expanded_states,
                        max_state_action_rows: spec.caps.max_state_action_rows,
                        max_transitions: spec.caps.max_transitions,
                        max_reforge_work: spec.caps.max_reforge_work,
                        max_solver_owned_bytes: spec.caps.max_solver_owned_bytes,
                        max_compiled_nodes: spec.caps.max_compiled_nodes,
                        max_compiled_edges: spec.caps.max_compiled_edges,
                        max_strategy_json_bytes: spec.caps.max_strategy_json_bytes,
                    },
                    { chunkSize: spec.caps.solve_step_work_items },
                );
            }
        } catch (error) {
            solveError = errorDetail(error);
            errors.push(`solve: ${solveError}`);
        }
        solveMs = roundMs(performance.now() - solveStarted);
        telemetry = await safeTelemetry(client, solver, errors);

        if (solve && !solve.cancelled && solve.converged) {
            const compileStarted = performance.now();
            try {
                const raw = await client.solverCompileStrategy(solver);
                const strategy = prepareSolverStrategy(raw);
                const strategyJson = JSON.stringify(raw);
                compiledGraph = {
                    nodes: strategy.nodes.length,
                    edges: strategy.edges.length,
                    strategy_json_bytes: Buffer.byteLength(strategyJson),
                };
                strategyHandle = await client.compileStrategy(session, strategy);
                telemetry = await safeTelemetry(client, solver, errors);
            } catch (error) {
                errors.push(`compile: ${errorDetail(error)}`);
            }
            compileMs = roundMs(performance.now() - compileStarted);
            if (strategyHandle && spec.verification.runs > 0) {
                const verifyStarted = performance.now();
                try {
                    verification = await verifyStrategy(
                        client,
                        session,
                        economy,
                        strategyHandle,
                        nestedNumber(telemetry, "value", "start") ?? solve.start_value,
                        spec,
                        (handle) => { simulator = handle; },
                    );
                } catch (error) {
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
    }

    const actualStatus = statusFrom(solve, solveError, telemetry);
    const worker = solve?.worker;
    const complete = solve && !solve.cancelled ? solve : null;
    const report: CaseReport = {
        id: spec.id,
        category: spec.category,
        approval_status: spec.approval_status,
        benchmark_enabled: true,
        expected: spec.expected,
        actual_status: actualStatus,
        expectation_met: false,
        input: {
            comparison_profile: "native-wasm-solver-v1",
            session: spec.session,
            start: spec.start,
            goal: spec.goal,
            allowed_mechanic_families: spec.allowed_mechanic_families,
            economy: spec.economy,
            caps: spec.caps,
            verification: spec.verification,
        },
        phase_wall_ms: {
            registry_layout: registryLayoutMs,
            solve: solveMs,
            compile: compileMs,
            verification: verificationMs,
            total: roundMs(performance.now() - totalStarted),
        },
        execution: {
            solve_steps: worker?.step_count ?? null,
            max_solve_step_ms: worker ? roundMs(worker.max_step_ms) : null,
            worker_max_slice_ms: worker ? roundMs(worker.max_step_ms) : null,
            cancellation_ack_ms: cancellationAckMs,
            cancellation_mode:
                spec.benchmark_mode === "cancel_after_first_step"
                    ? "abort_signal_after_worker_progress"
                    : null,
            cooperative_abandon_ms: null,
        },
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
            ? {
                  converged: complete.converged,
                  start_state: complete.start_state,
                  start_value: complete.start_value,
                  expanded_states: complete.expanded_states,
                  sweeps: complete.sweeps,
                  residual: complete.residual,
                  skipped_action_count: complete.skipped_actions,
              }
            : null,
        solver_telemetry: telemetry,
        compiled_graph: compiledGraph,
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
    report.expectation_met = caseExpectationMet(spec, report);
    return report;
}

async function verifyStrategy(
    client: EngineClient,
    session: number,
    economy: number,
    strategyHandle: number,
    startValue: number,
    spec: SolverBenchmarkCase,
    setSimulator: (handle: number) => void,
): Promise<Record<string, unknown>> {
    const simulator = await client.createSimulator(session, strategyHandle, economy);
    setSimulator(simulator);
    const run = await client.runStrategy(simulator, {
        target_runs: spec.verification.runs,
        seed: spec.verification.seed,
        max_actions_per_run: spec.verification.max_actions_per_run,
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
    const meanWithinTolerance = spec.verification.mean_cost_absolute_tolerance !== undefined
        ? absolute <= spec.verification.mean_cost_absolute_tolerance
        : spec.verification.mean_cost_relative_tolerance !== undefined
          ? relative <= spec.verification.mean_cost_relative_tolerance
          : null;
    return {
        runs: completed,
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
            reports.push(await runCase(client, data, spec));
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
