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

export interface StrategyResult {
    cancelled: boolean;
    done: number;
    summary: BatchSummary;
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

export interface ModInfo {
    session_mod_id: number;
    global_mod_id: number;
    key: string;
    generation_type: number;
    reach_kind: number;
    reach_influence: number;
    reach_via: string;
    primary_group_id: number;
    required_level: number;
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
