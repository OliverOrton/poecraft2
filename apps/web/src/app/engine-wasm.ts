/*
 * Low-level typed wrapper over the Emscripten module. This runs on the worker
 * thread only. It centralises the one untyped import of the generated module
 * and the JSON-in/JSON-out marshalling so the rest of the worker deals in plain
 * objects. Mirrors the Python `_binding.py` role for the WASM target.
 */

// @ts-ignore - generated Emscripten ES module; no type declarations are shipped.
import createPoecraftEngine from "../../../../bindings/wasm/dist/poecraft_engine.mjs";

import {
    ActionOutcome,
    BaseInfo,
    BatchSummary,
    CalcResult,
    CraftAction,
    EngineError,
    ModInfo,
    PoolDebug,
    SimulationOptions,
    SimulationProgress,
    SolveSummary,
    SolverActionInfo,
    SolverGoal,
    SolverStateValue,
    StrategyResult,
} from "./engine-protocol";

interface EngineModule {
    ccall(
        name: string,
        returnType: string | null,
        argTypes: string[],
        args: unknown[],
    ): unknown;
    _malloc(size: number): number;
    _free(ptr: number): void;
    HEAPU8: Uint8Array;
}

interface OkEnvelope {
    ok: boolean;
    code?: number;
    message?: string;
    [key: string]: unknown;
}

export class EngineBindings {
    private readonly module: EngineModule;

    constructor(module: EngineModule) {
        this.module = module;
    }

    abiVersion(): number {
        return this.module.ccall("pcw_abi_version", "number", [], []) as number;
    }

    private callJson(
        name: string,
        argTypes: string[],
        args: unknown[],
    ): OkEnvelope {
        const json = this.module.ccall(name, "string", argTypes, args) as string;
        const parsed = JSON.parse(json) as OkEnvelope;
        if (!parsed.ok) {
            throw new EngineError(parsed.code ?? -1, parsed.message ?? "unknown error");
        }
        return parsed;
    }

    loadData(bundle: Uint8Array): number {
        const ptr = this.module._malloc(bundle.length);
        try {
            this.module.HEAPU8.set(bundle, ptr);
            const json = this.module.ccall(
                "pcw_data_open",
                "string",
                ["number", "number"],
                [ptr, bundle.length],
            ) as string;
            const parsed = JSON.parse(json) as OkEnvelope;
            if (!parsed.ok) {
                throw new EngineError(
                    parsed.code ?? -1,
                    parsed.message ?? "unknown error",
                );
            }
            return parsed.data as number;
        } finally {
            this.module._free(ptr);
        }
    }

    dataSummary(data: number): Record<string, unknown> {
        const { ok, ...rest } = this.callJson("pcw_data_summary", ["number"], [data]);
        void ok;
        return rest;
    }

    listBases(data: number): BaseInfo[] {
        return this.callJson("pcw_data_bases", ["number"], [data])
            .bases as unknown as BaseInfo[];
    }

    closeData(data: number): void {
        this.module.ccall("pcw_data_close", null, ["number"], [data]);
    }

    createSession(data: number, base: string, itemLevel: number): number {
        return this.callJson(
            "pcw_session_open",
            ["number", "string"],
            [data, JSON.stringify({ base, item_level: itemLevel })],
        ).session as number;
    }

    closeSession(session: number): void {
        this.module.ccall("pcw_session_close", null, ["number"], [session]);
    }

    modCount(session: number): number {
        return this.callJson("pcw_session_mod_count", ["number"], [session])
            .count as number;
    }

    modInfo(session: number, modId: number): ModInfo {
        const { ok, ...rest } = this.callJson(
            "pcw_session_mod_info",
            ["number", "number"],
            [session, modId],
        );
        void ok;
        return rest as unknown as ModInfo;
    }

    createContext(session: number, seed: number): number {
        return this.callJson(
            "pcw_context_open",
            ["number", "string"],
            [session, JSON.stringify({ seed })],
        ).context as number;
    }

    closeContext(context: number): void {
        this.module.ccall("pcw_context_close", null, ["number"], [context]);
    }

    memoryStats(): { wasm_memory_bytes: number } {
        return { wasm_memory_bytes: this.module.HEAPU8.byteLength };
    }

    createItem(
        session: number,
        rarity: string,
        withImplicits: boolean,
    ): number {
        return this.callJson(
            "pcw_item_create",
            ["number", "string"],
            [session, JSON.stringify({ rarity, with_implicits: withImplicits })],
        ).item as number;
    }

    cloneItem(item: number): number {
        return this.callJson("pcw_item_clone", ["number"], [item]).item as number;
    }

    closeItem(item: number): void {
        this.module.ccall("pcw_item_close", null, ["number"], [item]);
    }

    itemInfo(item: number, session = 0): Record<string, unknown> {
        const { ok, ...rest } = this.callJson(
            "pcw_item_info",
            ["number", "number"],
            [item, session],
        );
        void ok;
        return rest;
    }

    exportItem(item: number): unknown {
        return this.callJson("pcw_item_export", ["number"], [item]).state;
    }

    importItem(state: unknown): number {
        return this.callJson("pcw_item_import", ["string"], [
            JSON.stringify(state),
        ]).item as number;
    }

    addMod(
        item: number,
        session: number,
        spec: { key: string; side?: string; fractured?: boolean },
    ): void {
        this.callJson(
            "pcw_item_add_mod",
            ["number", "number", "string"],
            [item, session, JSON.stringify(spec)],
        );
    }

    removeMod(
        item: number,
        spec: { modId: number; side: "prefix" | "suffix" },
    ): void {
        this.callJson(
            "pcw_item_remove_mod",
            ["number", "string"],
            [item, JSON.stringify({ mod_id: spec.modId, side: spec.side })],
        );
    }

    setModFractured(
        item: number,
        spec: { modId: number; side: "prefix" | "suffix" },
    ): void {
        this.callJson(
            "pcw_item_set_mod_fractured",
            ["number", "string"],
            [item, JSON.stringify({ mod_id: spec.modId, side: spec.side })],
        );
    }

    apply(context: number, item: number, action: CraftAction): ActionOutcome {
        return this.callJson(
            "pcw_apply",
            ["number", "number", "string"],
            [context, item, JSON.stringify(action)],
        ).result as unknown as ActionOutcome;
    }

    runBatch(
        context: number,
        item: number,
        action: CraftAction,
        count: number,
    ): BatchSummary {
        return this.callJson(
            "pcw_run_batch",
            ["number", "number", "string", "number"],
            [context, item, JSON.stringify(action), count],
        ).summary as unknown as BatchSummary;
    }

    compileStrategy(session: number, strategy: unknown): number {
        return this.callJson(
            "pcw_strategy_compile",
            ["number", "string"],
            [session, JSON.stringify(strategy)],
        ).strategy as number;
    }

    closeStrategy(strategy: number): void {
        this.module.ccall("pcw_strategy_close", null, ["number"], [strategy]);
    }

    loadEconomy(economy: unknown): number {
        return this.callJson("pcw_economy_open", ["string"], [
            JSON.stringify(economy),
        ]).economy as number;
    }

    closeEconomy(economy: number): void {
        this.module.ccall("pcw_economy_close", null, ["number"], [economy]);
    }

    createSimulator(
        session: number,
        strategy: number,
        economy?: number,
    ): number {
        return this.callJson(
            "pcw_simulator_open",
            ["number", "number", "number"],
            [session, strategy, economy ?? 0],
        ).simulator as number;
    }

    closeSimulator(simulator: number): void {
        this.module.ccall("pcw_simulator_close", null, ["number"], [simulator]);
    }

    runSimulatorChunk(
        simulator: number,
        options: SimulationOptions,
        maxCompletedRuns: number,
    ): SimulationProgress {
        return this.callJson(
            "pcw_simulator_run_chunk",
            ["number", "string", "number"],
            [simulator, JSON.stringify(options), maxCompletedRuns],
        ).progress as unknown as SimulationProgress;
    }

    simulatorResult(simulator: number): Omit<StrategyResult, "cancelled" | "progress"> {
        const { ok, ...result } = this.callJson(
            "pcw_simulator_result",
            ["number"],
            [simulator],
        );
        void ok;
        return result as unknown as Omit<
            StrategyResult,
            "cancelled" | "progress"
        >;
    }

    debugPool(
        context: number,
        item: number,
        query: { action: CraftAction; side?: string; include_rejected?: boolean },
    ): PoolDebug {
        const { ok, ...rest } = this.callJson(
            "pcw_debug_pool",
            ["number", "number", "string"],
            [context, item, JSON.stringify(query)],
        );
        void ok;
        return rest as unknown as PoolDebug;
    }

    openSolver(session: number, goal: SolverGoal): number {
        return this.callJson(
            "pcw_solver_open",
            ["number", "string"],
            [session, JSON.stringify(goal)],
        ).solver as number;
    }

    closeSolver(solver: number): void {
        this.module.ccall("pcw_solver_close", null, ["number"], [solver]);
    }

    solverActions(solver: number): SolverActionInfo[] {
        return this.callJson("pcw_solver_actions", ["number"], [solver])
            .actions as unknown as SolverActionInfo[];
    }

    solverCalc(solver: number, item: number, actionId: string): CalcResult {
        const { ok, ...rest } = this.callJson(
            "pcw_solver_calc",
            ["number", "number", "string"],
            [solver, item, actionId],
        );
        void ok;
        return rest as unknown as CalcResult;
    }

    solverSolve(
        solver: number,
        item: number,
        economy: number,
        options?: { epsilon?: number; max_states?: number; max_sweeps?: number },
    ): SolveSummary {
        const { ok, ...rest } = this.callJson(
            "pcw_solver_solve",
            ["number", "number", "number", "string"],
            [solver, item, economy, JSON.stringify(options ?? {})],
        );
        void ok;
        return rest as unknown as SolveSummary;
    }

    solverStateValue(solver: number, state: number): SolverStateValue {
        const { ok, ...rest } = this.callJson(
            "pcw_solver_state_value",
            ["number", "number"],
            [solver, state],
        );
        void ok;
        return rest as unknown as SolverStateValue;
    }

    solverProject(solver: number, item: number): number {
        return this.callJson(
            "pcw_solver_project",
            ["number", "number"],
            [solver, item],
        ).state as number;
    }

    /** Compiled policy as a parsed strategy document (editor/simulator format). */
    solverCompileStrategy(solver: number): unknown {
        const strategy = this.callJson(
            "pcw_solver_compile",
            ["number"],
            [solver],
        ).strategy as string;
        return JSON.parse(strategy);
    }

    solverLog(solver: number): string {
        return this.callJson("pcw_solver_log", ["number"], [solver])
            .log as string;
    }
}

export async function createEngineBindings(): Promise<EngineBindings> {
    const module = (await createPoecraftEngine()) as EngineModule;
    return new EngineBindings(module);
}

export { EngineError };
