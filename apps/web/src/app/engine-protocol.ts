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
    /** Price provenance pinned for the engine-listed action keys in this work. */
    price_sources?: Record<
        string,
        "override" | "quote" | "recipe" | "zero" | "fallback"
    >;
    /** User fallback at pin time; null means unquoted actions stayed excluded. */
    fallback_price?: number | null;
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
        | "fracture"
        | "remove_crafted_modifiers";
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

export interface BestiaryActionInfo {
    index: number;
    global_action_id: number;
    global_recipe_id: number;
    id: "bestiary:imprint" | "bestiary:restore_imprint";
    recipe_id: "bestiary:imprint";
    family_display_name: string;
    display_name: string;
    operation: "create" | "restore";
    transition_kind: "deterministic";
    emulator_available: boolean;
    calculator_available: boolean;
    strategy_builder_available: boolean;
    solver_available: boolean;
    checkpoint_requirement: "absent" | "present";
    checkpoint_effect: "create" | "consume";
    identity_requirement: "current_item" | "same_item";
    /** Repeated keys are exact consumed quantities. */
    cost_keys: string[];
}

export interface BestiarySolverOptionInfo {
    index: number;
    id: "imprint_retry";
    display_name: string;
    goal_restriction_key: "complete_magic_item_goal";
    goal_restriction: string;
    goal_rarity: "magic";
    requires_complete_goal: true;
    min_program_actions: 1;
    max_program_actions: 3;
}

export interface BestiaryPresentation {
    actions: BestiaryActionInfo[];
    solver_options: BestiarySolverOptionInfo[];
}

export interface BestiaryActionResult {
    action_id: BestiaryActionInfo["id"];
    applied: boolean;
    refusal_code: number;
    refusal_key: string;
    refusal_reason: string;
    cost_keys: string[];
    consumed_price_keys: string[];
    output_item_count: number;
    output_checkpoint_count: number;
    consumed_checkpoint_count: number;
    checkpoint_present: boolean;
}

export interface BestiaryCompoundSuccessor {
    /** Saved checkpoint is nested state, never a second live item handle. */
    bestiary: {
        checkpoint_present: boolean;
        checkpoint?: unknown;
    };
    [key: string]: unknown;
}

export interface BestiaryCalculation {
    deterministic: true;
    probability: number;
    result: BestiaryActionResult;
    /** Complete deterministic compound successor serialized by the engine. */
    successor: BestiaryCompoundSuccessor;
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
    /** Candidate primitive ids; omitted means full registry, [] means options only. */
    actions?: string[];
    /** Engine-owned bounded generation for useful 1-4 fossil signatures. */
    fossil_mode?: "exhaustive" | "goal_relevant";
    /** Engine-owned product relevance/dependency action envelope. */
    action_mode?: "goal_relevant";
    /** Native S8.3 carrier-aware candidates; implies the bounded product envelope. */
    automatic_candidates?: boolean;
    /** Additional hand-selected loadouts to materialize without scoping actions. */
    requested_fossil_actions?: string[];
    /** Explicit, price-independent fixed planner programs. */
    options?: SolverFixedOption[];
}

export type SolverFixedOption =
    | { type: "scour_alchemy" }
    | {
          type: "eldritch_side_intent";
          side: "prefix" | "suffix";
          action: "eldritch_exalt" | "eldritch_chaos" | "eldritch_annul";
          setup: string[];
      }
    | {
          type: "protected_side";
          side: "prefix" | "suffix";
          action: string;
      }
    | {
          type: "multimod_finish";
          bench_crafts: string[];
      }
    | {
          type: "renewal";
          /** One approved roll/reforge, Scour+Alchemy, optionally followed by Unveil. */
          actions: string[];
          until: SolverOptionExit;
      }
    | {
          type: "protected_repeat";
          side: "prefix" | "suffix";
          action: string;
          until: SolverOptionExit;
      }
    | {
          type: "fracture_prepare";
          /** Approved roll/reforge used until the exact carrier is ready. */
          preparation: string[];
          carrier_goal_slot: number;
      }
    | {
          type: "imprint_retry";
          /** One to three exact ordinary primitives; not a generic macro. */
          actions: string[];
          until: SolverOptionExit;
      };

export interface SolverOptionExit {
    /** Zero-based indices into SolverGoal.slots. */
    goal_slots: number[];
    min_satisfied: number;
}

export interface SolverActionInfo {
    index: number;
    id: string;
    display_name: string;
    /** 0 deterministic, 1 single-slot, 2 reforge, 3 special. */
    transition_kind: number;
    synthetic: boolean;
    cost_keys: string[];
    preservation: {
        can_preserve: CarrierProperty[];
        can_destroy: CarrierProperty[];
        can_create: CarrierProperty[];
        can_make_unreachable: CarrierProperty[];
        destructive_renewal: boolean;
        preserves_fractured_affixes: boolean;
        protection: {
            prefix_lock: boolean;
            suffix_lock: boolean;
            cannot_roll_attack: boolean;
            cannot_roll_caster: boolean;
        };
    };
}

export type CarrierProperty =
    | "goal_families"
    | "satisfied_goal_subset"
    | "junk_blockers"
    | "crafted_state"
    | "fractured_state"
    | "prefix_side"
    | "suffix_side"
    | "active_protection";

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
    max_discovered_states?: number;
    max_expanded_states?: number;
    max_state_action_rows?: number;
    max_transitions?: number;
    max_reforge_work?: number;
    max_solver_owned_bytes?: number;
    max_compiled_nodes?: number;
    max_compiled_edges?: number;
    max_strategy_json_bytes?: number;
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

/** Benchmark telemetry emitted by the native optimal solver.  The versioned
 * payload deliberately remains forward-compatible so S7 can add counters
 * without forcing the worker protocol to duplicate the C ABI schema. */
export interface SolverTelemetry {
    version: "solver_telemetry_v1";
    [key: string]: unknown;
}

/** Worker-owned measurements around bounded pc_solver_solve_step calls. */
export interface SolverWorkerMetrics {
    step_count: number;
    yield_count: number;
    max_step_ms: number;
    total_step_ms: number;
}

export type SolverSolveResult =
    | (SolveSummary & {
          cancelled: false;
          progress: SolveProgress;
          worker: SolverWorkerMetrics;
      })
      | {
          cancelled: true;
          progress: SolveProgress;
          worker: SolverWorkerMetrics;
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
    /** Engine-owned compound Imprint checkpoint presence. */
    checkpoint_present: boolean;
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
