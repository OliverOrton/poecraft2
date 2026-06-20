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
    message: string;
}

/** Thrown on the main thread when the worker reports a failed call. */
export class EngineError extends Error {
    readonly code: number;
    constructor(code: number, message: string) {
        super(`poecraft engine error ${code}: ${message}`);
        this.name = "EngineError";
        this.code = code;
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
        | "fossil";
    essence?: string;
    fossils?: string[];
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
    /** pc_session_support: 0 ordinary, 1 cluster, 2 unsupported domain. */
    support: number;
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
