/*
 * Shared message protocol and domain types for the engine worker boundary.
 *
 * The main thread (`EngineClient`) and the worker (`engine-worker.ts`) exchange
 * only these plain structured-cloneable messages. Engine objects live entirely
 * inside the worker's WASM module and are referenced by small integer handles,
 * so nothing here depends on WASM linear-memory layout.
 */

export interface EngineErrorInfo {
    code: number;
    detail: string;
}

/** Immutable economy identity attached by the workspace to completed work. */
export interface EconomyIdentity {
    profile: string;
    effective_snapshot_id: string;
    source_snapshot_id: string;
    source_content_sha256: string | null;
    source_cutoff_at_utc: string | null;
    league_name: string;
    status: "loading" | "fresh" | "stale" | "offline" | "manual-only";
    low_confidence_keys: string[];
}

/** Thrown on the main thread when the worker reports a failed call. */
export class EngineError extends Error {
    readonly code: number;
    readonly detail: string;
    constructor(code: number, detail: string) {
        super(`poecraft engine error ${code}: ${detail}`);
        this.name = "EngineError";
        this.code = code;
        this.detail = detail;
    }
}

/** A crafting action request. Essence/fossil actions carry their keys. */
export interface CraftAction {
    type:
        | "transmute"
        | "augment"
        | "alteration"
        | "regal"
        | "alchemy"
        | "chaos"
        | "exalt"
        | "annul"
        | "scour"
        | "essence"
        | "fossil"
        | "bench"
        | "veiled_chaos"
        | "veiled_exalt"
        | "unveil"
        | "harvest_reforge"
        | "harvest_augment"
        | "harvest_resist"
        | "eldritch_ember"
        | "eldritch_ichor"
        | "eldritch_exalt"
        | "eldritch_chaos"
        | "eldritch_annul"
        | "influence_exalt"
        | "fracture";
    essence?: string;
    fossils?: string[];
    mod_key?: string;
    target_tag?: string;
    source_tag?: string;
    influence?: string;
    tier?: number;
}

export type AffixSide = "both" | "prefix" | "suffix";

export interface ActionOutcome {
    applied: boolean;
    added: number;
    removed: number;
}

export interface BatchSummary {
    item_count: number;
    applied_count: number;
    total_added: number;
    total_removed: number;
}

export interface SimulationOptions {
    target_runs: number;
    seed?: number;
    max_actions_per_run?: number;
    max_graph_steps_per_run?: number;
    max_cost_per_run?: number;
    retained_trace_count?: number;
    max_trace_entries?: number;
    retained_success_count?: number;
    retained_failure_count?: number;
}

export interface SimulationProgress {
    completed_runs: number;
    target_runs: number;
    finished: boolean;
}

export interface SimulationSummary {
    completed_runs: number;
    success_count: number;
    failure_count: number;
    stop_count: number;
    total_actions: number;
    action_limit_count: number;
    cost_limit_count: number;
    step_limit_count: number;
    no_matching_edge_count: number;
    action_not_applied_count: number;
    missing_price_run_count: number;
    costed_action_count: number;
    missing_price_action_count: number;
    known_total_cost: number;
    cost_status: "disabled" | "complete" | "incomplete";
}

export interface StrategyTraceEntry {
    step_index: number;
    node_id: string;
    node_kind: number;
    action_type: number;
    action_applied: boolean;
    matched_edge_id: string;
    cumulative_actions: number;
    known_cumulative_cost: number;
    cost_complete: boolean;
    terminal_kind: "success" | "failure" | "stop" | null;
    failure_reason: number;
    item: unknown;
}

export interface StrategyTrace {
    entries: StrategyTraceEntry[];
}

export interface SimulationExample {
    terminal_kind: "success" | "failure" | "stop";
    failure_reason: number;
    terminal_node_id: string;
    action_count: number;
    known_total_cost: number;
    cost_complete: boolean;
    item: unknown;
}

export interface FailureSummary {
    failure_reason: number;
    node_id: string;
    detail: string;
    count: number;
}

export interface ActionDistributionEntry {
    node_id: string;
    action_type: number;
    count: number;
}

export interface StrategyResult {
    cancelled: boolean;
    progress: SimulationProgress;
    summary: SimulationSummary;
    action_distribution: ActionDistributionEntry[];
    traces: StrategyTrace[];
    examples: {
        success: SimulationExample[];
        failure: SimulationExample[];
        stop: SimulationExample[];
    };
    failure_summaries: FailureSummary[];
    missing_prices: Array<{ key: string; missing_count: number }>;
    economy?: EconomyIdentity;
}

export interface StrategyEvalOptions {
    epsilon?: number;
    max_sweeps?: number;
    max_states?: number;
    max_pairs?: number;
    max_transitions?: number;
    top_classes_per_node?: number;
}

export interface StrategyEvalProgress {
    phase: "discovery" | "solving" | "fallback" | "finalization" | "done";
    done: boolean;
    discovered_pairs: number;
    pending_pairs: number;
    solved_sccs: number;
    total_sccs: number;
    fallback_sweeps: number;
    residual: number;
}

export interface StrategyEvalClass {
    share: number;
    rarity: number;
    prefixes: number;
    suffixes: number;
    flags: number;
    blocked: number;
    /** Per target: 0 absent, 1 present below tier, 2 satisfied. */
    slots: number[];
}

export interface StrategyEvalResult {
    version: "v1";
    converged: boolean;
    sweeps: number;
    residual_mass: number;
    terminals: {
        success: number;
        failure: number;
        stop: number;
        action_not_applied: number;
        no_matching_edge: number;
        unresolved: number;
        by_node: Array<{
            node_id: string;
            kind: "success" | "failure" | "stop";
            p: number;
        }>;
    };
    unresolved_by_node: Array<{ node_id: string; mass: number }>;
    failures_by_node: Array<{
        node_id: string;
        reason: "action_not_applied" | "no_matching_edge";
        p: number;
    }>;
    expected_actions: number;
    expected_consumption: Array<{ key: string; quantity: number }>;
    targets: Array<
        | { kind: "family"; family_id: number; min_tier: number }
        | { kind: "group"; group_id: number }
    >;
    nodes: Array<{
        id: string;
        expected_visits: number;
        classes: StrategyEvalClass[];
        classes_truncated_share: number;
    }>;
    edges: Array<{ id: string; expected_traversals: number }>;
    economy?: EconomyIdentity;
}

export interface PoolEntry {
    session_mod_id: number;
    global_mod_id: number;
    key: string;
    generation_type: number;
    accepted: boolean;
    first_failure: number;
    spawn_weight: number;
    generation_multiplier_pct: number;
    special_multiplier_pct: number;
    final_weight: number;
}

export interface PoolSummary {
    tag_signature_id: number;
    cache_hit: boolean;
    candidate_count: number;
    prefix_total_weight: number;
    suffix_total_weight: number;
    combined_total_weight: number;
}

export interface PoolDebug {
    entries: PoolEntry[];
    summary: PoolSummary;
}

export interface BaseInfo {
    path: string;
    name: string;
    item_class_key: string;
    /** Canonical base drop-level requirement; negative means unknown. */
    drop_level: number;
    /** pc_session_support: 0 ordinary, 1 cluster, 2 unsupported domain. */
    support: number;
}

// --- solver / calculation engine ---------------------------------------------

/** Goal specification consumed by pc_solver_create (see poecraft/solver.h). */
export interface SolverGoal {
    version?: "v1";
    rarity?: "normal" | "magic" | "rare";
    /** Minimum goal slots that must be satisfied together; defaults to all. */
    min_satisfied_slots?: number;
    slots: Array<
        | { group: string; min_tier?: number }
        | { family_mod_key: string; min_tier?: number }
    >;
    /** Candidate action ids; omitted means the full registry. */
    actions?: string[];
}

export interface SolverActionInfo {
    index: number;
    id: string;
    display_name: string;
    /** 0 deterministic, 1 single-slot, 2 reforge, 3 special. */
    transition_kind: number;
    synthetic: boolean;
    cost_keys: string[];
}

/** One abstract successor class from the calculation engine. */
export interface CalcOutcome {
    state: number;
    probability: number;
    rarity: number;
    prefixes: number;
    suffixes: number;
    flags: number;
    blocked: number;
    /** Per goal slot: 0 absent, 1 present below tier, 2 satisfied. */
    slots: number[];
}

export interface CalcResult {
    supported: boolean;
    legal: boolean;
    /** Rarity and configured slot threshold satisfied together. */
    success_probability: number;
    /** Per goal slot: probability the slot is satisfied after the action. */
    slot_satisfied: number[];
    outcomes: CalcOutcome[];
}

export interface SolveSummary {
    converged: boolean;
    start_state: number;
    start_value: number;
    expanded_states: number;
    sweeps: number;
    residual: number;
    skipped_actions: number;
    economy?: EconomyIdentity;
}

export interface SolveOptions {
    epsilon?: number;
    max_states?: number;
    max_sweeps?: number;
}

export interface SolveProgress {
    phase: "expanding" | "iterating" | "done";
    done: boolean;
    expanded_states: number;
    sweeps: number;
    residual: number;
    /** Monotonically descending upper bound; 1e12 means iteration is pending. */
    start_value_bound: number;
}

export type SolverSolveResult =
    | (SolveSummary & {
          cancelled: false;
          progress: SolveProgress;
      })
    | {
          cancelled: true;
          progress: SolveProgress;
      };

export interface SolverStateValue {
    value: number;
    /** Policy action id, or null for goal/terminal states. */
    action: string | null;
}

export interface ModInfo {
    session_mod_id: number;
    global_mod_id: number;
    key: string;
    generation_type: number;
    reach_kind: number;
    reach_influence: number;
    reach_via: string;
    primary_group_id: number;
    /** Display family: exclusion group + stat signature + side + source. */
    family_id: number;
    required_level: number;
    group_display_name: string;
    family_tier_index: number;
    text_lines: string[];
    classification_tags: string[];
}

/** A catalog entry that pairs a stable engine key with a human-readable name. */
export interface CatalogEntry {
    key: string;
    name: string;
    code?: number;
}

/**
 * UI-authoring catalog derived from the compiled data bundle (not engine state).
 * Lets the editor offer dropdowns for mod groups, essences and fossils instead
 * of requiring raw keys/JSON. `groupKeyById` is indexed by the same group id
 * mods report as `primary_group_id`.
 */
export interface Catalog {
    groupKeyById: string[];
    groupNameById: string[];
    essences: CatalogEntry[];
    fossils: CatalogEntry[];
    bench: CatalogEntry[];
    harvestTags: CatalogEntry[];
    influences: CatalogEntry[];
}

export interface ItemInfo {
    rarity: string;
    prefix_mod_ids: number[];
    suffix_mod_ids: number[];
    implicit_mod_ids: number[];
    fractured_prefix_mod_ids: number[];
    fractured_suffix_mod_ids: number[];
    prefix_count: number;
    suffix_count: number;
    /** Session-aware max prefix slots; absent if the call omitted the session. */
    max_prefix?: number;
    /** Session-aware max suffix slots; absent if the call omitted the session. */
    max_suffix?: number;
    item_flags: number;
    generic_influence_bits: number;
    searing_exarch_tier: number;
    eater_of_worlds_tier: number;
    veiled_option_mod_ids: number[];
}

// --- worker message envelopes ----------------------------------------------

export interface RequestMessage {
    kind: "request";
    id: number;
    method: string;
    params: Record<string, unknown>;
}

export interface CancelMessage {
    kind: "cancel";
    id: number;
}

export interface ReadyMessage {
    kind: "ready";
    abiVersion: number;
}

export interface ProgressMessage {
    kind: "progress";
    id: number;
    done: number;
    total: number;
    evaluation?: StrategyEvalProgress;
    solve?: SolveProgress;
}

export interface ResponseMessage {
    kind: "response";
    id: number;
    ok: boolean;
    result?: unknown;
    error?: EngineErrorInfo;
}

export type ClientMessage = RequestMessage | CancelMessage;
export type WorkerMessage = ReadyMessage | ProgressMessage | ResponseMessage;
