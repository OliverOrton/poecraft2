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
        | "override"
        | "quote"
        | "recipe"
        | "zero"
        | "owner_default"
        | "fallback"
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
        | "foulborn_augment"
        | "foulborn_regal"
        | "foulborn_exalt"
        | "vaal"
        | "remembrance"
        | "unravelling"
        | "dominance"
        | "tempering"
        | "tailoring"
        | "double_corruption"
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

export interface HinekoraInfo {
    active: boolean;
    model?: "independent-cached-lock-v1";
    approximate?: boolean;
    currency?: CraftAction;
    preview?: unknown;
    cost_keys: string[];
}

export type AffixSide = "both" | "prefix" | "suffix";

export interface ActionOutcome {
    cost_keys?: string[];
    applied: boolean;
    added: number;
    removed: number;
}

export interface MultiItemRequest {
    action: "awakener";
    resources: Array<{identity: string; role: string; session: number; item: number}>;
}
export interface RecombinationPairRequest {
    resources: [{identity: string; role: string; session: number; item: number},
                {identity: string; role: string; session: number; item: number}];
}
export interface RecombinationApplyRequest {
    output_identity: string;
    resources: Array<{identity: string; role: string; session: number; item: number}>;
}
/** Native atomic receipt; costs remain incomplete until gold/dust are known. */
export interface RecombinationApplyResult {
    pair_version: 1;
    model_id: string;
    game_odds_estimated: true;
    carrier: 0 | 1;
    output_session: number;
    output_item: number;
    base_metadata_path: string;
    item_level: number;
    gold_cost: null;
    dust_cost: null;
    cost_complete: false;
    cost_keys: string[];
    resources: Array<{identity: string; effect: number; before: unknown; after: unknown}>;
}
export interface MultiItemResult {
    cost_keys: string[];
    resources: Array<{identity: string; effect: number; before: unknown; after: unknown}>;
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
    checkpoint_restriction_key: "magic_checkpoint_carrier";
    checkpoint_restriction: string;
    checkpoint_rarity: "magic";
    automatic_discovery: true;
    state_local_discovery: true;
    resource_bounded: true;
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
    seed: number;
    target_runs: number;
}

export interface StrategyTraceEntry {
    resources?: Array<{resource_id: string; acquisitions: number; lifecycle: number; memory_strands: number;
        identity?: string; base_key?: string; item_level?: number; item?: unknown; active_output?: boolean;
        recombination?: {model_id: string; carrier: number; input_a: string; input_b: string; output_identity: string;
            gold_cost_complete: false; dust_cost_complete: false; game_odds_estimated: true};
        feeder?: {returned_resource_id?: string; child_resources?: StrategyTraceEntry["resources"]; strategy_id: string; revision: string; output_contract_id: string; terminal_kind: number;
            failure_reason: number; terminal_node_id: string; detail: string; actions: number;
            known_cost: number; child_known_cost: number; acquisition_price_key: string;
            acquisition_cost_complete: boolean; cost_complete: boolean; output_accepted: boolean};
    }>;
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
    resources?: StrategyTraceEntry["resources"];
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
    sampled_accounting: {
        evidence_source: "simulator_sample";
        sample_count: number;
        seed: number;
        actions: Array<{
            action_id: string;
            count: number;
            average_per_invocation: number;
        }>;
        materials: Array<{
            price_key: string;
            count: number;
            average_per_invocation: number;
        }>;
    };
    economy?: EconomyIdentity;
}

export interface StrategyEvalOptions {
    epsilon?: number;
    max_sweeps?: number;
    max_states?: number;
    max_pairs?: number;
    max_transitions?: number;
    top_classes_per_node?: number;
    include_success_normalized?: boolean;
    max_owned_bytes?: number;
    max_output_json_bytes?: number;
}

export interface StrategyAccountingMaterial {
    price_key: string;
    expected_quantity: number;
    price_status: "priced" | "missing";
    unit_price: number | null;
    cost_contribution: number | null;
}

export interface StrategyAccountingAction {
    id: string;
    display_name: string;
    expected_visits: number;
    expected_applied: number;
    classifications: string[];
    materials: StrategyAccountingMaterial[];
    raw_nodes: Array<{
        node_id: string;
        expected_visits: number;
        expected_applied: number;
    }>;
}

export interface StrategyAccountingWorkTotals {
    expected_actions: number;
    known_expected_cost: number;
    total_expected_cost: number | null;
    cost_complete: boolean;
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
    accounting: {
        version: "s8.4_v1";
        semantics: {
            primary: "per_strategy_invocation";
            terminal_mass_separate: true;
            success_normalized_basis:
                | "independent_whole_strategy_retries"
                | null;
            success_normalized_is_conditional_path_expectation: false;
        };
        pricing: {
            status: "disabled" | "complete" | "incomplete";
            economy_id: string | null;
            missing_price_keys: string[];
        };
        totals: {
            per_invocation: StrategyAccountingWorkTotals;
            success_normalized: {
                basis: "independent_whole_strategy_retries";
                success_probability_denominator: number;
                expected_invocations: number;
                work: StrategyAccountingWorkTotals;
            } | null;
        };
        actions: {
            per_invocation: StrategyAccountingAction[];
            success_normalized: StrategyAccountingAction[] | null;
        };
        materials: {
            per_invocation: StrategyAccountingMaterial[];
            success_normalized: StrategyAccountingMaterial[] | null;
        };
        techniques: {
            per_invocation: Record<string, number>;
            success_normalized: Record<string, number> | null;
        };
        review_sections: {
            enabled: boolean;
            items: Array<{
                id: string;
                label: string;
                role: string;
                raw_references: {
                    node_ids: string[];
                    edge_ids: string[];
                };
                per_invocation: StrategyAccountingWorkTotals;
                expected_edge_traversals: number;
                actions: StrategyAccountingAction[];
                materials: StrategyAccountingMaterial[];
                techniques: Record<string, number>;
                success_normalized: {
                    work: StrategyAccountingWorkTotals;
                    expected_edge_traversals: number;
                    actions: StrategyAccountingAction[];
                    materials: StrategyAccountingMaterial[];
                    techniques: Record<string, number>;
                } | null;
            }>;
        };
        reconciliation: {
            action_descriptor_visits_difference: number;
            action_descriptor_applied_difference: number;
            node_operation_visits_difference: number;
            material_quantity_differences: Record<string, number>;
            cost_dot_product_difference: number;
            section_actions_difference: number;
            section_material_differences: Record<string, number>;
        };
    };
    memory: {
        owned_bytes_estimate: number;
        peak_owned_bytes_estimate: number;
        max_owned_bytes: number;
        max_output_json_bytes: number;
    };
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

/** Stable configured input identity. The engine validates membership and bounds. */
export interface ClusterConfiguration {
    passiveKey: string;
    passiveCount: number;
}
export interface ClusterBaseCatalog {
    minPassiveCount: number;
    maxPassiveCount: number;
    passives: Array<{key: string; name: string; tag: string; text: string[]}>;
}

export interface BaseInfo {
    path: string;
    name: string;
    item_class_key: string;
    /** Canonical base drop-level requirement; negative means unknown. */
    drop_level: number;
    cluster?: ClusterBaseCatalog;
    /** pc_session_support: 0 ordinary, 1 configured cluster, 2 unsupported domain. */
    support: number;
}

// --- solver / calculation engine ---------------------------------------------

/** Goal specification consumed by pc_solver_create (see poecraft/solver.h). */
export interface SolverGoal {
    version?: "v1";
    rarity?: "normal" | "magic" | "rare";
    /** Require goal slots while allowing other explicit mods. Current uses
     * zero global lower and checked policies, never exact closure. */
    allow_extra_modifiers?: boolean;
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
    /** Native carrier-aware candidates, including state-local Imprint discovery. */
    automatic_candidates?: boolean;
    /** Additional hand-selected loadouts to materialize without scoping actions. */
    requested_fossil_actions?: string[];
    /** Engine-owned restricted solve envelope. Omitted/empty keeps defaults. */
    disabled_action_families?: SolverActionFamily[];
    /** Explicit, price-independent fixed planner programs. */
    options?: SolverFixedOption[];
}

/** Terminal item requirements for single-action odds; never sent to strategy solving. */
export interface CalculatorItemGoal extends SolverGoal {
    implicit_mod_keys?: string[];
    influence_bits?: number;
    corrupted?: boolean;
}

/** Calculator terminal goals, never a SolverGoal or strategy objective. */
export interface CalculatorGoalSet {
    version: "calculator_goal_set_v1";
    goals: Array<{id: string; goal: CalculatorItemGoal}>;
    /** Shared requested primitive materializes a selected fossil loadout. */
    actions: string[];
}
export interface CalculatorGoalResult {
    id: string;
    success_probability: number;
    slot_satisfied: number[];
    implicit_satisfied: number[];
}

export interface ItemEdit {
    memory_strands?: number;
    add_explicit?: string;
    fractured?: boolean;
    rarity?: "normal" | "magic" | "rare";
    influence_bits?: number;
    corrupted?: boolean;
    add_implicit?: string;
    remove_implicit?: string;
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
    /** Engine-owned solve-envelope family; never inferred from the id. */
    family: SolverActionFamily;
    /** 0 deterministic, 1 single-slot, 2 reforge, 3 special. */
    transition_kind: number;
    synthetic: boolean;
    /** Native cluster capability bits: 1 craft, 2 terminal odds, 4 continuation. */
    cluster_support?: number;
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

export type SolverActionFamily =
    | "foulborn"
    | "memory"
    | "currency"
    | "essence"
    | "fossil"
    | "harvest"
    | "bench"
    | "eldritch"
    | "influence"
    | "fracture"
    | "veiled"
    | "cleanup"
    | "temporary_bench"
    | "metamod"
    | "imprint"
    | "restart";

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
    carrier?: 0 | 1;
    base_metadata_path?: string;
    item_level?: number;
    matched_goal_ids?: string[];
    goal_observations?: Array<{id: string; slots: number[]; blocked?: number; is_goal: boolean; goal_properties_satisfied: boolean}>;
    /** Property-only refills need no explicit enumeration; counts/affix flags are omitted. */
    affixes_unobserved?: boolean;
    goal_properties_satisfied?: boolean;
    influence_bits?: number;
    /** Terminal failure observations have no live item to inspect. */
    terminal?: "destroyed" | "bricked";
    state: number;
    /** Native goal predicate, including rarity and clean/coverage semantics. */
    is_goal?: boolean;
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
    pair_version?: 1;
    model_id?: string;
    projection_id?: string;
    goal_projection_id?: string;
    game_odds_estimated?: boolean;
    model_projection_exact?: boolean;
    apply_supported?: boolean;
    cost_complete?: boolean;
    gold_cost?: number | null;
    dust_cost?: number | null;
    data_identity?: string[];
    unobserved_properties?: string[];
    carriers?: Array<{carrier: 0 | 1; probability: number; base_metadata_path: string; item_level: number}>;
    /** Native union probability, counted once per shared terminal outcome. */
    any_goal_probability?: number;
    goal_results?: CalculatorGoalResult[];
    implicit_satisfied?: number[];
    /** Vaal probabilities over final implicit identities, calculated natively. */
    implicit_outcomes?: Array<{mod: number; weight: number; added_probability: number; present_probability: number}>;
    vaal_branches?: {implicit: number; sockets: number; reforge: number; unchanged: number};
    double_corruption_branches?: {implicit: number; sockets: number; reforge: number; destroyed: number};
    /** Unordered pairs and unconditional probabilities, supplied by native code. */
    implicit_pairs?: Array<{mods: [number, number]; probability: number}>;
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
    start_value: number | null;
    expanded_states: number;
    sweeps: number;
    residual: number | null;
    /** Compatibility aggregate; prefer the two categorized counts below. */
    skipped_actions: number;
    stop_cause:
        | "none"
        | "exact_closed"
        | "target_gap"
        | "state_cap"
        | "transition_cap"
        | "memory_cap"
        | "sweep_cap"
        | "reforge_work_cap"
        | "state_action_row_cap"
        | "compiled_output_cap"
        | "other_resource_cap"
        | "no_executable_policy"
        | "numerical_stability"
        | "requested_bounded_finish"
        | "finder_complete"
        | "bounded_envelope_incomplete";
    cap_hit_mask: number;
    registry_actions: number;
    candidate_actions: number;
    evaluator_supported_actions: number;
    supported_priced_actions: number;
    skipped_missing_price_actions: number;
    skipped_unsupported_actions: number;
    policy_available: boolean;
    policy_status:
        | "none"
        | "bounded_feasible"
        | "bounded_near_optimal"
        | "exact";
    termination:
        | "none"
        | "refused_resource_cap"
        | "target_gap"
        | "exact_closed"
        | "no_executable_policy"
        | "numerical_stability"
        | "requested_bounded_finish"
        | "finder_complete"
        | "bounded_envelope_incomplete";
    lower_bound: number | null;
    upper_bound: number | null;
    evaluated_policy_cost: number | null;
    absolute_optimality_gap: number | null;
    relative_optimality_gap: number | null;
    requested_absolute_optimality_gap: number;
    requested_relative_optimality_gap: number;
    target_met: boolean;
    target_fired: "none" | "absolute" | "relative" | "both";
    economy?: EconomyIdentity;
}

export interface SolveOptions {
    /** Versioned native-owned defaults used by normal Calculator solves. */
    solve_profile?: "calculator_product_v1";
    solver_mode?: "current" | "strategy_finder";
    epsilon?: number;
    max_states?: number;
    max_sweeps?: number;
    max_discovered_states?: number;
    max_expanded_states?: number;
    max_state_action_rows?: number;
    max_transitions?: number;
    /** Stable V1-equivalent logical reforge search envelope. */
    max_reforge_work?: number;
    candidate_max_owned_bytes?: number;
    candidate_max_states?: number;
    candidate_max_pairs?: number;
    candidate_max_transitions?: number;
    max_solver_owned_bytes?: number;
    max_compiled_nodes?: number;
    max_compiled_edges?: number;
    max_strategy_json_bytes?: number;
    max_diagnostic_samples?: number;
    max_telemetry_json_bytes?: number;
    /** Optional post-solve certification/lift state budget. */
    max_policy_refinement_states?: number;
    max_absolute_optimality_gap?: number;
    max_relative_optimality_gap?: number;
    full_evidence?: boolean;
    strict_states?: boolean;
    kernel_reuse?: boolean;
    goal_progress_gated_reforges?: boolean;
    /** Include carrier-local automatic Imprint checkpoint/retry programs. */
    consider_imprint_programs?: boolean;
    /** Permit ordinary policy choice to abandon the carrier for a fresh base. */
    allow_economic_restart?: boolean;
    /** Exact operator-major delayed-action scheduler used by Calculator. */
    high_impact_executable_uppers?: boolean;
}

export interface EngineMemoryStats {
    wasm_memory_bytes: number;
    live_handles: number;
    native_live_owned_bytes: number;
    native_peak_owned_bytes: number;
    native_serialized_output_bytes: number;
    scope: "facade_registries_plus_solver_and_evaluator_owned_allocations";
}

export interface SolverProgressTrace {
    schema_version: "solver_progress_trace_v1";
    clock: "native_solve_constructor_ms";
    native_elapsed_ms: number;
    sequence: number;
    dropped_before_cursor: number;
    current: {
        active_work_owner: string;
        working_value_role: string;
        numerical_generation: number;
        reforge_source: string;
        candidate_identity: string;
        candidate_source: string;
        candidate_stage: string;
        missing_continuations: number;
        candidate_attempts: number;
        verified_identity: string;
        verified_replacements: number;
        finish_requested: boolean;
        [key: string]: unknown;
    };
    events: Array<{sequence: number; native_elapsed_ms: number;
        kind: string; reason: string; candidate_identity: string;
        [key: string]: unknown}>;
}

export interface SolveProgress {
    lifecycle_sequence?: number;
    trace?: SolverProgressTrace;
    /** Worker observation clock, distinct from source event time. */
    worker_observed_ms?: number;
    delivery_stage?: string;
    trace_error?: string;
    /** Native retained-work phase. `done` means finish is packaging-only. */
    phase:
        | "expanding"
        | "iterating"
        | "refining"
        | "compiling"
        | "certifying"
        | "done";
    /** Narrow native owner inside the broad phase; observational only. */
    phase_owner:
        | "setup"
        | "planner_construction"
        | "temporary_effect_precompile"
        | "dependency_preparation"
        | "primitive_rows"
        | "state_local_automatic_synthesis"
        | "ladder_scheduling"
        | "bellman_optimization"
        | "policy_assembly"
        | "compilation"
        | "exact_evaluation"
        | "strategy_finder"
        | "done";
    done: boolean;
    expanded_states: number;
    sweeps: number;
    residual: number | null;
    /** Active numerical workspace value; role/generation are in trace.current. */
    start_value_bound: number | null;
    lower_bound: number | null;
    /** Monotone independently verified executable upper, when available. */
    upper_bound: number | null;
    absolute_optimality_gap: number | null;
    relative_optimality_gap: number | null;
    focused_round: number;
    incumbent_kind:
        | "none"
        | "constructive_fallback"
        | "progressive_fracture"
        | "destructive_renewal"
        | "direct_executable_row"
        | "partial_upper_plus_fallback"
        | "partial_upper_plus_progressive_fracture"
        | "partial_upper_plus_destructive_renewal"
        | "other";
    discovered_states: number;
    frontier_states: number;
    state_action_rows: number;
    transition_entries: number;
    /** Consumed V1-equivalent logical reforge envelope. */
    reforge_work: number;
    live_owned_bytes: number;
    peak_owned_bytes: number;
    finalization_work_items: number;
    refinement_states: number;
    refinement_kernels: number;
    refinement_transitions: number;
    refinement_rounds: number;
    refinement_classes: number;
    certification_discovered_pairs: number;
    certification_pending_pairs: number;
    certification_solved_sccs: number;
    certification_total_sccs: number;
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
    work_policy: "adaptive" | "fixed_eight";
    step_transport: "json" | "compact";
    requested_quantum_histogram: Record<string, number>;
    step_count: number;
    yield_count: number;
    max_step_ms: number;
    max_setup_step_ms?: number;
    max_ordinary_step_ms?: number;
    max_setup_step_context?: SolverWorkerMetrics["max_step_context"];
    max_ordinary_step_context?: SolverWorkerMetrics["max_step_context"];
    total_step_ms: number;
    total_progress_read_ms?: number;
    diagnostic_events?: unknown[];
    diagnostic_events_omitted?: number;
    diagnostic_dropped_before_cursor?: number;
    diagnostic_owner_checkpoints?: Array<{
        threshold_rows: number; observed_rows: number; worker_observed_ms: number;
        read_wall_ms: number; timings_ns?: unknown; work?: unknown;
        binding_step_ccall_ms?: number; binding_step_parse_ms?: number;
        error?: string;
    }>;
    diagnostic_binding_step_timing?: {ccall_ms: number; parse_ms: number};
    diagnostic_row_checkpoints?: Array<{
        threshold_rows: number; observed_rows: number; worker_observed_ms: number;
        native_call_wall_ms: number; progress_read_wall_ms: number;
        step_count: number; phase: string; phase_owner: string | undefined;
        lifecycle_sequence: number | undefined; trace_current: unknown;
    }>;
    /** Packaging-only public-result transfer after native bounded stepping. */
    finalization_ms: number;
    max_step_context?: { input_owner: string; output_owner: string; input_cursor: number | undefined;
        output_cursor: number | undefined; quantum: number; duration_ms: number };
    max_progress_read_ms?: number;
    progress_observations?: SolveProgress[];
    progress_observations_omitted?: number;
    milestones?: Array<{stage: string; worker_elapsed_ms: number}>;
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
    /** Public currency choices for Influence Exalt operations. */
    influences: CatalogEntry[];
    /** Complete generic influence-bit display catalog, including Elder and
     * Shaper. Optional for persisted/test catalogs created before the split. */
    genericInfluences?: CatalogEntry[];
}

export interface ItemInfo {
    memory_strands?: number;
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
    cancelled?: boolean;
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

export type ClientMessage = RequestMessage | CancelMessage | {kind: "finish"; id: number};
export type WorkerMessage = ReadyMessage | ProgressMessage | ResponseMessage;

/** Read-only bounded planner DTO. item_state is the native version-3 stable-key
 * export, including actual base/level, every slot, roll and resource flag. */
export interface RecombinationPlannerRequest {
    version: "recombination-planner-request-v2";
    base_key: string;
    item_level: number;
    data_identity: [string, string, string, string];
    model_id: string;
    price_identity: string;
    goal_set: unknown;
    acquisitions: Array<{
        id: string;
        source_kind: "purchase" | "completed_feeder" | "checked_feeder";
        quote_identity: string;
        item_state: unknown;
        total_cost_chaos: number;
        cost_complete: boolean;
        feeder?: {strategy_id: string; revision: string; document_json: string;
            output_contract_id: string; paid_start_cost_chaos: number};
    }>;
    initial_items: Array<{item_state: unknown; paid_cost_chaos: number}>;
    all_in_attempt_cost_chaos: number | null;
    allow_incomplete_costs: boolean;
    feeder_economy?: unknown;
    scenario?: {id: string; prefix_first_a: number; prefix_first_b: number};
    limits?: {items?: number; states?: number; policy_iterations?: number; work?: number};
    export_checked: boolean;
}
export interface RecombinationPlannerResponse {
    result: {
        version: "random_recombination_inventory_v2";
        model_id: string; price_identity: string; scenario_id: string | null;
        data_identity: [string, string, string, string];
        cost_complete: boolean; fully_priced_ranking: boolean;
        game_odds_estimated: true; global_optimality_claim: false; robust_cost_bound: false;
        expected_cost_chaos: number; expected_recombinations: number;
        acquisitions: Array<{id: string; expected_invocations: number; source_kind: string;
            native_feeder_checked: boolean; [key: string]: unknown}>;
        [key: string]: unknown;
    };
    checked_export: null | {
        version: "checked-recombination-builder-v1";
        strategy: import("./strategy-model").StrategyDocument;
        economy: unknown;
        identity_receipt: RecombinationPlannerResponse["result"];
        [key: string]: unknown;
    };
}
