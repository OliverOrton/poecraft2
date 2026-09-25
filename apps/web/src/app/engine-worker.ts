/*
 * Engine worker: owns the WASM module and runs all engine work off the UI
 * thread. It speaks the message protocol in engine-protocol.ts and runs in both
 * a browser Web Worker and Node's worker_threads (used by the headless test).
 *
 * Native graph simulations are run in bounded chunks. Between chunks the
 * worker yields to the event loop so queued cancel/progress messages are
 * delivered, keeping the UI responsive and cancellation prompt.
 */

import type { EngineBindings } from "./engine-wasm";
import {
    Catalog,
    CatalogEntry,
    ClientMessage,
    CraftAction,
    EngineError,
    SimulationOptions,
    SimulationProgress,
    SolveOptions,
    SolveProgress,
    SolverWorkerMetrics,
    SolverGoal,
    SolverSolveResult,
    StrategyEvalOptions,
    StrategyEvalProgress,
    StrategyResult,
    WorkerMessage,
} from "./engine-protocol";
import { buildHarvestCatalog } from "./harvest-crafts";
import {
    buildGenericInfluenceCatalog,
    buildInfluenceExaltCatalog,
} from "./influence-presentation";

const DEFAULT_CHUNK_SIZE = 1000;
const compactSolvePhases: Array<SolveProgress["phase"] | undefined> = [
    undefined, "expanding", "iterating", "done", "refining", "compiling", "certifying",
];
const compactSolveOwners: Array<SolveProgress["phase_owner"] | undefined> = [
    undefined, "setup", "planner_construction", "temporary_effect_precompile",
    "dependency_preparation", "primitive_rows", "state_local_automatic_synthesis",
    "ladder_scheduling", "bellman_optimization", "policy_assembly",
    "compilation", "exact_evaluation", "done", "strategy_finder",
];

let bindings: EngineBindings;
let post: (message: WorkerMessage, transfer?: ArrayBuffer[]) => void = () => {};
const cancelled = new Set<number>();
const activeRequests = new Set<number>();
const activeSolves = new Set<number>();
const finishIntents = new Set<number>();
const solveClocks = new Map<number, number>();

// Raw bundle bytes retained until a catalog is distilled from them, then the
// compact catalog is cached and the bytes are dropped to reclaim memory.
const dataBundles = new Map<number, Uint8Array>();
const catalogCache = new Map<number, Catalog>();

interface BundleShape {
    manifest: { enums: { influence: Record<string, number> } };
    strings: { strings: string[]; string_id_base?: number };
    game_data: {
        groups: { key_string_ids: number[]; display_name_string_ids: number[] };
        essences: {
            key_string_ids: number[];
            name_string_ids: number[];
            is_corruption_only?: number[];
        };
        fossils: { key_string_ids: number[]; name_string_ids: number[] };
        mods: {
            global_mod_ids: number[];
            key_string_ids: number[];
            text_line_offsets: number[];
            text_line_string_ids: number[];
        };
        bench_options: { global_mod_ids: number[] };
        tags: { global_tag_ids: number[]; name_string_ids: number[] };
    };
}

/**
 * Distil the UI-authoring catalog from the compiled data bundle. Pure JS over
 * the same JSON the engine loaded — no WASM call, so it works without rebuilding
 * the engine. Mod groups are returned indexed by group id (matching the
 * `primary_group_id` mods report); essences and fossils are filtered to named,
 * craftable rows and sorted for display.
 */
function buildCatalog(bundle: Uint8Array): Catalog {
    const json = JSON.parse(new TextDecoder().decode(bundle)) as BundleShape;
    const strings = json.strings.strings;
    const base = json.strings.string_id_base ?? 0;
    const s = (id: number): string => strings[id - base] ?? "";
    const g = json.game_data;

    const groupKeyById = g.groups.key_string_ids.map(s);
    const groupNameById = g.groups.display_name_string_ids.map(s);

    const essences: CatalogEntry[] = [];
    for (let i = 0; i < g.essences.key_string_ids.length; i += 1) {
        if (g.essences.is_corruption_only?.[i]) continue;
        const name = s(g.essences.name_string_ids[i]);
        if (!name) continue;
        essences.push({ key: s(g.essences.key_string_ids[i]), name });
    }
    const fossils: CatalogEntry[] = [];
    for (let i = 0; i < g.fossils.key_string_ids.length; i += 1) {
        const name = s(g.fossils.name_string_ids[i]);
        if (!name) continue;
        fossils.push({ key: s(g.fossils.key_string_ids[i]), name });
    }
    essences.sort((a, b) => a.name.localeCompare(b.name));
    fossils.sort((a, b) => a.name.localeCompare(b.name));
    const modIndex = new Map(
        g.mods.global_mod_ids.map((id, index) => [id, index]),
    );
    const bench: CatalogEntry[] = [];
    const seenBench = new Set<string>();
    for (const globalId of g.bench_options.global_mod_ids) {
        if (globalId < 0) continue;
        const index = modIndex.get(globalId);
        if (index === undefined) continue;
        const key = s(g.mods.key_string_ids[index]);
        if (!key || seenBench.has(key)) continue;
        seenBench.add(key);
        const lineOffset = g.mods.text_line_offsets[index];
        const lineEnd = g.mods.text_line_offsets[index + 1];
        const name =
            lineEnd > lineOffset
                ? s(g.mods.text_line_string_ids[lineOffset])
                : key;
        bench.push({ key, name: name || key });
    }
    bench.sort((a, b) => a.name.localeCompare(b.name));
    const tagNames = new Set(g.tags.name_string_ids.map(s));
    const harvestTags = buildHarvestCatalog(tagNames);
    const genericInfluences = buildGenericInfluenceCatalog(
        json.manifest.enums.influence,
    );
    const influences = buildInfluenceExaltCatalog(genericInfluences);
    return {
        groupKeyById,
        groupNameById,
        essences,
        fossils,
        bench,
        harvestTags,
        influences,
        genericInfluences,
    };
}

function catalogFor(data: number): Catalog {
    const cached = catalogCache.get(data);
    if (cached) return cached;
    const bundle = dataBundles.get(data);
    if (!bundle) {
        throw new EngineError(1, "catalog unavailable: data not loaded");
    }
    const catalog = buildCatalog(bundle);
    catalogCache.set(data, catalog);
    dataBundles.delete(data);
    return catalog;
}

const yieldToEventLoop = (() => {
    if (typeof MessageChannel !== "undefined") {
        const channel = new MessageChannel();
        const pending: Array<() => void> = [];
        channel.port1.onmessage = () => {
            pending.shift()?.();
        };
        return (): Promise<void> =>
            new Promise((resolve) => {
                pending.push(resolve);
                channel.port2.postMessage(undefined);
            });
    }
    return (): Promise<void> =>
        new Promise((resolve) => setTimeout(resolve, 0));
})();

const yieldToTimerTask = (): Promise<void> =>
    new Promise((resolve) => setTimeout(resolve, 0));

async function runStrategy(
    id: number,
    params: Record<string, unknown>,
): Promise<StrategyResult> {
    const simulator = params.simulator as number;
    const options = params.options as SimulationOptions;
    let chunkSize = (params.chunkSize as number) || DEFAULT_CHUNK_SIZE;
    let yieldCount = 0;
    const runChunk = (): SimulationProgress => {
        const started = performance.now();
        const progress = bindings.runSimulatorChunk(
            simulator,
            options,
            chunkSize,
        );
        const elapsedMs = Math.max(0.1, performance.now() - started);
        const scale = Math.min(4, Math.max(0.5, 16 / elapsedMs));
        chunkSize = Math.min(
            10_000,
            Math.max(1, Math.round(chunkSize * scale)),
        );
        return progress;
    };

    if (cancelled.has(id)) {
        const progress = bindings.runSimulatorChunk(simulator, options, 0);
        return {
            cancelled: true,
            progress,
            ...bindings.simulatorResult(simulator),
        };
    }
    let progress = runChunk();
    post({
        kind: "progress",
        id,
        done: progress.completed_runs,
        total: progress.target_runs,
    });
    while (!progress.finished) {
        // MessageChannel is the low-latency yield, but some worker runtimes can
        // repeatedly service its private queue before incoming cancel messages.
        // Periodically use a timer task to guarantee external messages a turn.
        ++yieldCount;
        await (yieldCount % 4 === 0
            ? yieldToTimerTask()
            : yieldToEventLoop());
        if (cancelled.has(id)) {
            return {
                cancelled: true,
                progress,
                ...bindings.simulatorResult(simulator),
            };
        }
        progress = runChunk();
        post({
            kind: "progress",
            id,
            done: progress.completed_runs,
            total: progress.target_runs,
        });
    }
    return {
        cancelled: false,
        progress,
        ...bindings.simulatorResult(simulator),
    };
}

function solveProgressCounts(
    progress: SolveProgress,
): { done: number; total: number } {
    if (progress.phase === "expanding") {
        return {
            done: progress.expanded_states,
            total: progress.expanded_states + 1,
        };
    }
    if (progress.phase === "iterating") {
        return { done: progress.sweeps, total: progress.sweeps + 1 };
    }
    if (progress.phase === "refining") {
        return {
            done: progress.refinement_states,
            total: progress.refinement_states + 1,
        };
    }
    if (progress.phase === "certifying") {
        return {
            done: progress.certification_discovered_pairs,
            total:
                progress.certification_discovered_pairs +
                progress.certification_pending_pairs,
        };
    }
    if (progress.phase === "compiling") {
        return {
            done: progress.finalization_work_items,
            total: progress.finalization_work_items + 1,
        };
    }
    return { done: 1, total: 1 };
}

async function solveSolver(
    id: number,
    params: Record<string, unknown>,
): Promise<SolverSolveResult> {
    const solver = params.solver as number;
    let begun = false;
    /* Start ordinary product calls conservatively before any slice
     * measurement. The 1024-unit qualification contract deliberately starts
     * with its complete native batch; an artificial micro-boundary there can
     * select a much more expensive incremental scheduling trajectory. */
    const requestedWorkItems = Math.max(
        1, (params.chunkSize as number) || 8);
    const maxWorkItems = Math.min(1024, requestedWorkItems);
    /* The 1024-unit qualification harness has a 20-second slice guardrail.
     * Preserve that stable batch there: incremental action admission makes a
     * scheduling decision at each native step boundary, so timing-adaptive
     * micro-batches can cause repeated Bellman reoptimization and change the
     * amount of work needed to reach the same exact fixed point. Product calls
     * and the 50 ms legacy probes retain responsive 12 ms calibration. */
    const qualificationRequest = requestedWorkItems >= 1024;
    const qualificationWorkItems = qualificationRequest
        ? maxWorkItems
        : null;
    // The explicit diagnostic is supplied only by the supervised probe. It is
    // scoped to this invocation and does not alter Calculator's normal request.
    const fixedEightDiagnostic = params.diagnosticWorkPolicy === "fixed_eight" &&
        requestedWorkItems === 8;
    let workItems = fixedEightDiagnostic ? 8 :
        qualificationWorkItems ?? Math.min(4, maxWorkItems);
    let observedPhase: SolveProgress["phase"] = "expanding";
    let emittedProgress = false;
    let observedOwner = "setup";
    let traceSequence = 0;
    let traceError: string | undefined;
    let lastProgressAt = -Infinity;
    let yieldCount = 0;
    let lastTimerYieldAt = -Infinity;
    let unyieldedStepMs = 0;
    const diagnosticThresholds = [0, 12735, 14378, 24844, 42681, 83570, 117634, 135519];
    let nextDiagnosticThreshold = 0;
    const ownerThresholds = [24844, 42681];
    let nextOwnerThreshold = 0;
    const boundedFinishAfterMs =
        typeof params.boundedFinishAfterMs === "number" &&
        Number.isFinite(params.boundedFinishAfterMs) &&
        params.boundedFinishAfterMs > 0
            ? params.boundedFinishAfterMs
            : null;
    let boundedFinishRequested = false;
    const finderMode = (params.options as SolveOptions | undefined)?.solver_mode === "strategy_finder";
    /* Match the native benchmark contract: the bounded wall window owns
     * synchronous solve-model setup as well as stepped search. Starting this
     * after beginSolverSolve silently granted release WASM an additional full
     * search window and changed the graph selected by the same fixture. */
    const solveStartedAt = performance.now();
    solveClocks.set(id, solveStartedAt);
    const worker: SolverWorkerMetrics = {
        work_policy: fixedEightDiagnostic ? "fixed_eight" : "adaptive",
        step_transport: "json",
        requested_quantum_histogram: {},
        ...(params.diagnosticTrace === true ? {
            diagnostic_events: [], diagnostic_events_omitted: 0,
            diagnostic_row_checkpoints: [], diagnostic_owner_checkpoints: [],
        } : {}),
        step_count: 0,
        yield_count: 0,
        max_step_ms: 0,
        max_setup_step_ms: 0,
        max_ordinary_step_ms: 0,
        total_step_ms: 0,
        finalization_ms: 0,
        progress_observations: [],
        progress_observations_omitted: 0,
        milestones: [],
    };
    if (params.diagnosticTrace === true) bindings.enableDiagnosticStepTiming();
    const milestone = (stage: string): void => {
        worker.milestones!.push({stage, worker_elapsed_ms: performance.now()-solveStartedAt});
    };
    const captureProgress = (stage = "native_work"): void => {
        const readStarted = performance.now();
        if (compactStep && worker.step_count > 0)
            progress = bindings.solverCachedProgress(solver);
        // Diagnostic failure cannot refuse a valid solve, including an older
        // loaded module. Preserve the error and avoid repeated failed reads.
        let trace: SolveProgress["trace"];
        if (!traceError && !finderMode) {
            try {
                trace = bindings.solverProgressTrace(solver, traceSequence);
                traceSequence = trace.sequence;
            } catch (error) {
                traceError = error instanceof Error ? error.message : String(error);
            }
        }
        progress = {...progress, trace, trace_error: traceError,
            worker_observed_ms: performance.now()-solveStartedAt, delivery_stage: stage};
        const observations = worker.progress_observations!;
        if (observations.length === 4096) {
            observations.shift(); worker.progress_observations_omitted! += 1;
        }
        observations.push(progress);
        const readMs = performance.now()-readStarted;
        worker.max_progress_read_ms = Math.max(worker.max_progress_read_ms ?? 0, readMs);
        worker.total_progress_read_ms = (worker.total_progress_read_ms ?? 0) + readMs;
        if (params.diagnosticTrace === true) {
            worker.diagnostic_dropped_before_cursor = Math.max(
                worker.diagnostic_dropped_before_cursor ?? 0, trace?.dropped_before_cursor ?? 0);
            for (const event of trace?.events ?? []) {
                if (worker.diagnostic_events!.length < 1024) worker.diagnostic_events!.push(event);
                else worker.diagnostic_events_omitted! += 1;
            }
            while (nextDiagnosticThreshold < diagnosticThresholds.length &&
                progress.state_action_rows >= diagnosticThresholds[nextDiagnosticThreshold]) {
                worker.diagnostic_row_checkpoints!.push({
                    threshold_rows: diagnosticThresholds[nextDiagnosticThreshold],
                    observed_rows: progress.state_action_rows,
                    worker_observed_ms: progress.worker_observed_ms!,
                    native_call_wall_ms: worker.total_step_ms,
                    progress_read_wall_ms: worker.total_progress_read_ms,
                    step_count: worker.step_count, phase: progress.phase,
                    phase_owner: progress.phase_owner,
                    lifecycle_sequence: progress.lifecycle_sequence,
                    trace_current: trace?.current ?? null,
                });
                nextDiagnosticThreshold += 1;
            }
            if (nextOwnerThreshold < ownerThresholds.length &&
                progress.state_action_rows >= ownerThresholds[nextOwnerThreshold]) {
                const readStarted = performance.now();
                try {
                    const telemetry = bindings.solverTelemetry(solver);
                    worker.diagnostic_owner_checkpoints!.push({
                        threshold_rows: ownerThresholds[nextOwnerThreshold],
                        observed_rows: progress.state_action_rows,
                        worker_observed_ms: progress.worker_observed_ms!,
                        read_wall_ms: performance.now() - readStarted,
                        timings_ns: telemetry.timings_ns, work: telemetry.work,
                        binding_step_ccall_ms: bindings.solverStepTiming().ccall_ms,
                        binding_step_parse_ms: bindings.solverStepTiming().parse_ms,
                    });
                } catch (error) {
                    worker.diagnostic_owner_checkpoints!.push({
                        threshold_rows: ownerThresholds[nextOwnerThreshold],
                        observed_rows: progress.state_action_rows,
                        worker_observed_ms: progress.worker_observed_ms!,
                        read_wall_ms: performance.now() - readStarted,
                        error: error instanceof Error ? error.message : String(error),
                    });
                }
                nextOwnerThreshold += 1;
            }
        }
    };
    const acknowledgeCancellation = (): SolverSolveResult => {
        milestone("cancel_acknowledged");
        if (begun) captureProgress("abandon_cleanup");
        const result: SolverSolveResult = {
            cancelled: true,
            progress,
            worker,
        };
        // Acknowledge intent as progress; the one terminal response follows
        // finally, so its receipt also contains measured resource release.
        post({kind: "progress", id, done: 0, total: 1, solve: {...progress, delivery_stage: "cancel_acknowledged"}});
        return result;
    };
    /* A caller-supplied chunk size controls native work granularity; it does
     * not request one cross-thread progress message per chunk. Large exact
     * solves can execute hundreds of thousands of tiny chunks, and flooding
     * the host queue with every intermediate snapshot can dominate release
     * WASM wall time. The explicit cancellation probe still requests and
     * receives every step; ordinary UI and benchmark runs retain the first,
     * phase-change, completion, and 100 ms progress cadence below. */
    const reportEveryChunk = params.yieldEveryStep === true ||
        (params.reportProgress === true && requestedWorkItems === 1);
    const compactStep = params.diagnosticStepTransport !== "legacy_json" &&
        qualificationWorkItems === null && !reportEveryChunk;
    worker.step_transport = compactStep ? "compact" : "json";
    let progress: SolveProgress = {
        phase: "expanding",
        phase_owner: "setup",
        done: false,
        expanded_states: 0,
        sweeps: 0,
        residual: 1e12,
        start_value_bound: 1e12,
        lower_bound: null,
        upper_bound: null,
        absolute_optimality_gap: null,
        relative_optimality_gap: null,
        focused_round: 0,
        incumbent_kind: "none",
        discovered_states: 0,
        frontier_states: 0,
        state_action_rows: 0,
        transition_entries: 0,
        reforge_work: 0,
        live_owned_bytes: 0,
        peak_owned_bytes: 0,
        finalization_work_items: 0,
        refinement_states: 0,
        refinement_kernels: 0,
        refinement_transitions: 0,
        refinement_rounds: 0,
        refinement_classes: 0,
        certification_discovered_pairs: 0,
        certification_pending_pairs: 0,
        certification_solved_sccs: 0,
        certification_total_sccs: 0,
    };

    try {
        if (cancelled.has(id)) {
            return acknowledgeCancellation();
        }
        milestone("native_begin_requested");
        bindings.beginSolverSolve(
            solver,
            params.item as number,
            params.economy as number,
            params.options as SolveOptions | undefined,
        );
        begun = true;
        milestone("native_begin_completed");
        // Let a cancel queued during synchronous begin reach its owner before
        // any search. This yield does not change the first native work quantum.
        await yieldToEventLoop();

        do {
            if (cancelled.has(id)) {
                return acknowledgeCancellation();
            }
            if (finishIntents.delete(id) && !boundedFinishRequested) {
                milestone("manual_finish_intent_observed");
                // An unreported step replaces progress and has no trace. Read
                // the retained owner at this control boundary, never infer
                // availability from the presence of a throttled JS snapshot.
                captureProgress("manual_finish_intent");
                if (finderMode
                    ? progress.upper_bound !== null && Number.isFinite(progress.upper_bound)
                    : progress.trace?.current.verified_artifact_available === true) {
                    bindings.requestSolverSolveBoundedFinish(solver);
                    boundedFinishRequested = true;
                    milestone("finish_acknowledged");
                } else milestone("finish_refused_no_verified_artifact");
            }
            /* S7.2 splits Bellman sweeps into bounded sparse-row units.
             * Expansion and iteration both adapt toward a 12 ms worker slice,
             * so cancellation is serviced inside a large sweep rather than
             * only between whole-table sweeps. */
            const stepWorkItems = workItems;
            const inputOwner = progress.phase_owner;
            const inputCursor = compactStep ? undefined : progress.lifecycle_sequence;
            const started = performance.now();
            if (compactStep) {
                const status = bindings.stepSolverSolveCompact(solver, stepWorkItems);
                const phase = compactSolvePhases[status & 7];
                const phaseOwner = compactSolveOwners[(status >> 3) & 15];
                if (!phase || !phaseOwner || (status & ~255) !== 0)
                    throw new EngineError(-1, "invalid compact solver step status");
                progress.phase = phase;
                progress.phase_owner = phaseOwner;
                progress.done = (status & 128) !== 0;
            } else {
                progress = bindings.stepSolverSolve(solver, stepWorkItems);
            }
            const measuredMs = Math.max(0, performance.now() - started);
            const elapsedMs = Math.max(0.1, measuredMs);
            worker.step_count += 1;
            const quantumKey = String(stepWorkItems);
            worker.requested_quantum_histogram[quantumKey] =
                (worker.requested_quantum_histogram[quantumKey] ?? 0) + 1;
            const stepContext = {
                input_owner: inputOwner, output_owner: progress.phase_owner,
                input_cursor: inputCursor,
                output_cursor: compactStep ? undefined : progress.lifecycle_sequence,
                quantum: stepWorkItems, duration_ms: measuredMs,
            };
            if (measuredMs > worker.max_step_ms) worker.max_step_context = stepContext;
            worker.max_step_ms = Math.max(worker.max_step_ms, measuredMs);
            if (inputOwner === "setup") {
                if (measuredMs > (worker.max_setup_step_ms ?? 0)) worker.max_setup_step_context = stepContext;
                worker.max_setup_step_ms = Math.max(worker.max_setup_step_ms ?? 0, measuredMs);
            } else {
                if (measuredMs > (worker.max_ordinary_step_ms ?? 0)) worker.max_ordinary_step_context = stepContext;
                worker.max_ordinary_step_ms = Math.max(worker.max_ordinary_step_ms ?? 0, measuredMs);
            }
            worker.total_step_ms += measuredMs;
            unyieldedStepMs += measuredMs;
            const phaseChanged = progress.phase !== observedPhase;
            if (fixedEightDiagnostic) {
                workItems = 8;
            } else if (qualificationWorkItems !== null) {
                workItems = qualificationWorkItems;
            } else if (phaseChanged) {
                workItems = 1;
            } else {
                const scale = Math.min(
                    2, Math.max(0.5, 12 / elapsedMs));
                workItems = Math.min(
                    maxWorkItems,
                    Math.max(1, Math.round(stepWorkItems * scale)),
                );
            }
            observedPhase = progress.phase;

            if (
                !progress.done &&
                !boundedFinishRequested &&
                boundedFinishAfterMs !== null &&
                performance.now() - solveStartedAt >= boundedFinishAfterMs
            ) {
                milestone("finish_requested");
                bindings.requestSolverSolveBoundedFinish(solver);
                boundedFinishRequested = true;
                milestone("finish_acknowledged");
            }

            const now = performance.now();
            if (!progress.done && (
                reportEveryChunk ||
                !emittedProgress ||
                phaseChanged ||
                progress.phase_owner !== observedOwner ||
                now - lastProgressAt >= 100
            )) {
                captureProgress();
                observedOwner = progress.phase_owner;
                const counts = solveProgressCounts(progress);
                post({
                    kind: "progress",
                    id,
                    ...counts,
                    solve: progress,
                });
                emittedProgress = true;
                lastProgressAt = now;
            }

            if (
                !progress.done &&
                (reportEveryChunk || unyieldedStepMs >= 8)
            ) {
                ++yieldCount;
                worker.yield_count = yieldCount;
                unyieldedStepMs = 0;
                const timerYieldDue =
                    params.yieldEveryStep === true ||
                    lastTimerYieldAt === -Infinity ||
                    performance.now() - lastTimerYieldAt >= 100;
                if (timerYieldDue) {
                    await yieldToTimerTask();
                    lastTimerYieldAt = performance.now();
                } else {
                    await yieldToEventLoop();
                }
            }
        } while (!progress.done);

        // A queued cancel wins until terminal commitment, including natural
        // exact completion and a Finish that completed in the last work unit.
        // Explicit control probes need a timer turn: a chain of MessageChannel
        // tasks can outrun their inbound control messages in a warmed worker.
        await (params.yieldEveryStep === true
            ? yieldToTimerTask() : yieldToEventLoop());
        if (cancelled.has(id)) {
            return acknowledgeCancellation();
        }
        captureProgress("native_done");
        if (params.diagnosticTrace === true)
            worker.diagnostic_binding_step_timing = bindings.solverStepTiming();
        milestone("native_done_observed");
        const finalizationStarted = performance.now();
        milestone("result_export_start");
        const summary = bindings.finishSolverSolve(solver);
        milestone("result_export_completed");
        worker.finalization_ms = Math.max(
            0,
            performance.now() - finalizationStarted,
        );
        begun = false;
        progress = {
            ...progress,
            phase: "done",
            done: true,
            expanded_states: summary.expanded_states,
            sweeps: summary.sweeps,
            residual: summary.residual,
            start_value_bound: summary.start_value,
            lower_bound: summary.lower_bound,
            upper_bound: summary.upper_bound,
            absolute_optimality_gap: summary.absolute_optimality_gap,
            relative_optimality_gap: summary.relative_optimality_gap,
        };
        post({
            kind: "progress",
            id,
            done: 1,
            total: 1,
            solve: progress,
        });
        return { ...summary, cancelled: false, progress, worker };
    } finally {
        if (begun) {
            milestone("abandon_cleanup_start");
            bindings.abandonSolverSolve(solver);
            milestone("abandon_cleanup_completed");
        }
    }
}

function evaluationProgressCounts(
    progress: StrategyEvalProgress,
): { done: number; total: number } {
    if (progress.phase === "discovery") {
        return {
            done: progress.discovered_pairs,
            total: progress.discovered_pairs + progress.pending_pairs,
        };
    }
    if (progress.phase === "solving") {
        return {
            done: progress.solved_sccs,
            total: progress.total_sccs,
        };
    }
    if (progress.phase === "fallback") {
        return {
            done: progress.fallback_sweeps,
            total: progress.fallback_sweeps + 1,
        };
    }
    return { done: progress.done ? 1 : 0, total: 1 };
}

async function evaluateStrategy(
    id: number,
    params: Record<string, unknown>,
): Promise<unknown> {
    let strategy = 0;
    let evaluation = 0;
    let economy = 0;
    let workItems = Math.max(1, (params.chunkSize as number) || 16);
    let yieldCount = 0;
    let lastProgressAt = -Infinity;
    let lastTimerYieldAt = -Infinity;
    let emittedProgress = false;
    let observedPhase: StrategyEvalProgress["phase"] = "discovery";
    let pendingDiscovery: number | undefined;
    const reportProgress = params.reportProgress === true;
    try {
        if (cancelled.has(id)) {
            throw new EngineError(1, "strategy evaluation cancelled");
        }
        strategy = bindings.compileStrategy(
            params.session as number,
            params.strategyJson as Uint8Array,
        );
        if (params.economy !== undefined) {
            economy = bindings.loadEconomy(params.economy);
        }
        evaluation = bindings.beginStrategyEvaluation(
            strategy,
            params.options as StrategyEvalOptions | undefined,
            economy || undefined,
            params.reviewProjection,
        );
        let progress: StrategyEvalProgress;
        do {
            if (cancelled.has(id)) {
                throw new EngineError(1, "strategy evaluation cancelled");
            }
            // Do not let a discovery-calibrated budget spill across the
            // discovery/solve boundary, and enter each SCC one at a time so
            // an iterative fallback is observable before any sweeps run.
            const stepWorkItems =
                observedPhase === "discovery" && pendingDiscovery !== undefined
                    ? Math.min(workItems, Math.max(1, pendingDiscovery))
                    : observedPhase === "solving"
                      ? 1
                      : workItems;
            const started = performance.now();
            progress = bindings.stepStrategyEvaluation(evaluation, stepWorkItems);
            const elapsedMs = Math.max(0.1, performance.now() - started);
            const phaseChanged = progress.phase !== observedPhase;
            if (phaseChanged) {
                // One fallback work item is already 32 native sweeps. Rebase
                // at every phase boundary, then adapt from that phase's cost.
                workItems = 1;
            } else {
                const scale = Math.min(4, Math.max(0.5, 16 / elapsedMs));
                workItems = Math.min(
                    16_384,
                    Math.max(1, Math.round(stepWorkItems * scale)),
                );
            }
            observedPhase = progress.phase;
            pendingDiscovery = progress.pending_pairs;
            const now = performance.now();
            if (
                reportProgress &&
                (!emittedProgress ||
                    phaseChanged ||
                    progress.done ||
                    now - lastProgressAt >= 100)
            ) {
                const counts = evaluationProgressCounts(progress);
                post({
                    kind: "progress",
                    id,
                    ...counts,
                    evaluation: progress,
                });
                emittedProgress = true;
                lastProgressAt = now;
            }
            if (!progress.done) {
                ++yieldCount;
                // MessageChannel keeps fine-grained evaluator stepping cheap,
                // while a timer task at least every 100 ms guarantees incoming
                // abort messages a turn even in runtimes that favor the private
                // channel queue. The first continuation always uses the timer.
                const timerYieldDue =
                    lastTimerYieldAt === -Infinity ||
                    performance.now() - lastTimerYieldAt >= 100;
                if (timerYieldDue) {
                    await yieldToTimerTask();
                    lastTimerYieldAt = performance.now();
                } else {
                    await yieldToEventLoop();
                }
            }
        } while (!progress.done);
        if (cancelled.has(id)) {
            throw new EngineError(1, "strategy evaluation cancelled");
        }
        return bindings.finishStrategyEvaluation(evaluation);
    } finally {
        if (evaluation) bindings.closeStrategyEvaluation(evaluation);
        if (economy) bindings.closeEconomy(economy);
        if (strategy) bindings.closeStrategy(strategy);
    }
}

async function dispatch(
    id: number,
    method: string,
    params: Record<string, unknown>,
): Promise<unknown> {
    switch (method) {
        case "loadData": {
            const bundle = params.bundle as Uint8Array;
            const data = bindings.loadData(bundle);
            dataBundles.set(data, bundle);
            return { data };
        }
        case "catalog":
            return { catalog: catalogFor(params.data as number) };
        case "dataSummary":
            return bindings.dataSummary(params.data as number);
        case "listBases":
            return { bases: bindings.listBases(params.data as number) };
        case "bestiaryPresentation":
            return bindings.bestiaryPresentation(params.data as number);
        case "closeData":
            bindings.closeData(params.data as number);
            dataBundles.delete(params.data as number);
            catalogCache.delete(params.data as number);
            return {};
        case "createSession":
            return {
                session: bindings.createSession(
                    params.data as number,
                    params.base as string,
                    (params.itemLevel as number) ?? 0,
                ),
            };
        case "closeSession":
            bindings.closeSession(params.session as number);
            return {};
        case "modCount":
            return { count: bindings.modCount(params.session as number) };
        case "modInfo":
            return bindings.modInfo(
                params.session as number,
                params.modId as number,
            );
        case "createContext":
            return {
                context: bindings.createContext(
                    params.session as number,
                    (params.seed as number) ?? 0,
                ),
            };
        case "closeContext":
            bindings.closeContext(params.context as number);
            return {};
        case "memoryStats":
            return bindings.memoryStats();
        case "createItem":
            return {
                item: bindings.createItem(
                    params.session as number,
                    (params.rarity as string) ?? "normal",
                    (params.withImplicits as boolean) ?? true,
                ),
            };
        case "cloneItem":
            return { item: bindings.cloneItem(params.item as number) };
        case "closeItem":
            bindings.closeItem(params.item as number);
            return {};
        case "itemInfo":
            return bindings.itemInfo(
                params.item as number,
                (params.session as number) ?? 0,
            );
        case "exportItem":
            return { state: bindings.exportItem(params.item as number) };
        case "importItem":
            return { item: bindings.importItem(params.state) };
        case "addMod":
            bindings.addMod(params.item as number, params.session as number, {
                key: params.key as string,
                side: params.side as string | undefined,
                fractured: params.fractured as boolean | undefined,
                crafted: params.crafted as boolean | undefined,
                veiled: params.veiled as boolean | undefined,
            });
            return {};
        case "removeMod":
            bindings.removeMod(params.item as number, {
                modId: params.modId as number,
                side: params.side as "prefix" | "suffix",
            });
            return {};
        case "setModFractured":
            bindings.setModFractured(params.item as number, {
                modId: params.modId as number,
                side: params.side as "prefix" | "suffix",
            });
            return {};
        case "apply":
            return {
                result: bindings.apply(
                    params.context as number,
                    params.item as number,
                    params.action as CraftAction,
                ),
            };
        case "bestiaryApply":
            return {
                result: bindings.bestiaryApply(
                    params.data as number,
                    params.item as number,
                    params.actionId as string,
                ),
            };
        case "bestiaryCalculate":
            return bindings.bestiaryCalculate(
                params.data as number,
                params.item as number,
                params.actionId as string,
            );
        case "debugPool":
            return bindings.debugPool(
                params.context as number,
                params.item as number,
                {
                    action: params.action as CraftAction,
                    side: params.side as string | undefined,
                    include_rejected: params.includeRejected as boolean | undefined,
                },
            );
        case "compileStrategy":
            return {
                strategy: bindings.compileStrategy(
                    params.session as number,
                    params.strategyJson as Uint8Array,
                ),
            };
        case "closeStrategy":
            bindings.closeStrategy(params.strategy as number);
            return {};
        case "strategyEvaluate": {
            return evaluateStrategy(id, params);
        }
        case "loadEconomy":
            return { economy: bindings.loadEconomy(params.economy) };
        case "closeEconomy":
            bindings.closeEconomy(params.economy as number);
            return {};
        case "createSimulator":
            return {
                simulator: bindings.createSimulator(
                    params.session as number,
                    params.strategy as number,
                    params.economy as number | undefined,
                ),
            };
        case "closeSimulator":
            bindings.closeSimulator(params.simulator as number);
            return {};
        case "runStrategy":
            return runStrategy(id, params);
        case "openSolver":
            return {
                solver: bindings.openSolver(
                    params.session as number,
                    params.goal as SolverGoal,
                ),
            };
        case "closeSolver":
            bindings.closeSolver(params.solver as number);
            return {};
        case "solverActions": {
            let actions = bindings.solverActions(params.solver as number);
            if (params.omitFossilCombos) {
                // Multi-fossil loadout ids are "fossil:<key>+<key>..." with
                // sorted keys (solver_registry.cpp); keep only the singles.
                actions = actions.filter(
                    (action) =>
                        !action.id.startsWith("fossil:") ||
                        !action.id.includes("+"),
                );
            }
            return { actions };
        }
        case "solverCalc":
            return bindings.solverCalc(
                params.solver as number,
                params.item as number,
                params.action as string,
            );
        case "solverSolve":
            return solveSolver(id, params);
        case "solverStateValue":
            return bindings.solverStateValue(
                params.solver as number,
                params.state as number,
            );
        case "solverProject":
            return {
                state: bindings.solverProject(
                    params.solver as number,
                    params.item as number,
                ),
            };
        case "solverCompileStrategy":
            return {
                strategyJson: bindings.solverCompileStrategy(
                    params.solver as number,
                ),
            };
        case "solverLog":
            return { log: bindings.solverLog(params.solver as number) };
        case "solverTelemetry":
            return bindings.solverTelemetry(params.solver as number);
        default:
            throw new EngineError(1, `unknown method: ${method}`);
    }
}

async function handle(message: ClientMessage): Promise<void> {
    if (message.kind === "cancel") {
        if (activeRequests.has(message.id)) cancelled.add(message.id);
        return;
    }
    if (message.kind === "finish") {
        if (activeSolves.has(message.id)) finishIntents.add(message.id);
        return;
    }
    if (message.kind !== "request") {
        return;
    }
    const { id, method, params } = message;
    if (activeRequests.has(id)) return;
    activeRequests.add(id);
    // Bind an already-aborted signal to this invocation atomically. A fast
    // synchronous operation can finish before a following cancel message.
    if (message.cancelled) cancelled.add(id);
    if (method === "solverSolve") activeSolves.add(id);
    try {
        const result = await dispatch(id, method, params);
        const strategyJson =
            method === "solverCompileStrategy" &&
            result !== null &&
            typeof result === "object"
                ? (result as { strategyJson?: unknown }).strategyJson
                : undefined;
        const transfer =
            strategyJson instanceof Uint8Array
                ? [strategyJson.buffer as ArrayBuffer]
                : undefined;
        if (method === "solverSolve") {
            const solve = result as SolverSolveResult;
            solve.worker.milestones?.push({stage: "terminal_response_committed",
                worker_elapsed_ms: performance.now() - solveClocks.get(id)!});
        }
        post({ kind: "response", id, ok: true, result }, transfer);
    } catch (error) {
        const info =
            error instanceof EngineError
                ? { code: error.code, detail: error.detail }
                : {
                      code: -1,
                      detail:
                          error instanceof Error ? error.message : String(error),
                  };
        post({ kind: "response", id, ok: false, error: info });
    } finally {
        activeRequests.delete(id);
        activeSolves.delete(id);
        finishIntents.delete(id);
        solveClocks.delete(id);
        cancelled.delete(id);
    }
}

async function main(): Promise<void> {
    const isBrowserWorker =
        typeof (globalThis as { postMessage?: unknown }).postMessage ===
            "function" &&
        typeof (globalThis as { document?: unknown }).document === "undefined";
    if (
        isBrowserWorker &&
        typeof (globalThis as { WorkerGlobalScope?: unknown })
            .WorkerGlobalScope === "undefined"
    ) {
        // Some embedded Chromium shells omit the WorkerGlobalScope constructor
        // even though the module is running in a real worker. Emscripten uses
        // this marker to choose its fetch-based worker loader.
        (
            globalThis as { WorkerGlobalScope?: unknown }
        ).WorkerGlobalScope = Object;
    }
    const { createEngineBindings } = await import("./engine-wasm");
    bindings = await createEngineBindings();
    if (isBrowserWorker) {
        const scope = globalThis as unknown as {
            postMessage: (
                message: unknown,
                transfer?: ArrayBuffer[],
            ) => void;
            onmessage: ((event: { data: ClientMessage }) => void) | null;
        };
        post = (message, transfer) =>
            scope.postMessage(message, transfer ?? []);
        scope.onmessage = (event) => {
            void handle(event.data);
        };
    } else {
        const { parentPort } = await import(
            /* @vite-ignore */ "node:worker_threads"
        );
        if (!parentPort) {
            throw new Error("engine-worker started without a parent port");
        }
        post = (message, transfer) =>
            parentPort.postMessage(message, transfer ?? []);
        parentPort.on("message", (data: ClientMessage) => {
            void handle(data);
        });
    }
    post({ kind: "ready", abiVersion: bindings.abiVersion() });
}

void main();
