import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

import type { SolverGoal } from "../src/app/engine-protocol";

export type BenchmarkModFlag = "fractured" | "crafted" | "veiled";

export interface SolverBenchmarkCase {
    schema_version: "solver_benchmark_case_v1";
    id: string;
    category: string;
    approval_status: string;
    benchmark_enabled: boolean;
    benchmark_mode?: "cancel_after_first_step" | "exact_product_acceptance";
    description?: string;
    comparison_profile?: string;
    watchdog_seconds?: number;
    session: {
        base_metadata_path: string;
        base_name: string;
        item_class_key?: string;
        item_level: number;
    };
    start: {
        rarity: "normal" | "magic" | "rare";
        with_implicits: boolean;
        searing_exarch_tier?: number;
        eater_of_worlds_tier?: number;
        generic_influence_bits?: number;
        mods: Array<{ key: string; flags: BenchmarkModFlag[] }>;
    };
    goal: SolverGoal;
    product_action_envelope?: {
        mode: "calculator_goal_relevant_priced_v1";
        envelope_goal: SolverGoal;
        expected_priced_action_ids?: string[];
        pricing_filter?: string;
    };
    forced_winner_contract?: {
        automatic_candidate_kind: string;
        dependency_action_id: string;
        dependency_must_not_be_primitive_candidate: boolean;
        required_compiled_operation_type: string;
        snapshot_action_price: number;
        forced_action_price: number;
        target_side: "prefix" | "suffix";
        preserved_goal_side: "prefix" | "suffix";
    };
    compiled_operation_contract?: {
        type: string;
        required_string_parameters: Record<string, string>;
    };
    compiled_operation_contracts?: Array<{
        type: string;
        required_string_parameters: Record<string, string>;
    }>;
    material_ratio_contract?: {
        numerator_price_key: string;
        denominator_price_key: string;
        expected_ratio: number;
        require_positive_denominator: boolean;
        require_complete_pricing: boolean;
    };
    market_price_override_contracts?: Array<{
        price_key: string;
        snapshot_chaos_value: number;
        override_chaos_value: number;
        purpose: string;
    }>;
    bounded_best_policy_contract?: {
        allow_exact: boolean;
        bounded_requires_named_stop: boolean;
        allowed_bounded_terminations?: string[];
        allowed_bounded_stop_causes?: string[];
        bounded_requires_strict_gap: boolean;
        bounded_requires_open_obligations: boolean;
        require_evaluated_upper: boolean;
        require_cheapest_independently_evaluated_incumbent: boolean;
    };
    allowed_mechanic_families: string[];
    economy: {
        version: "v1";
        id?: string;
        prices?: Record<string, number>;
        snapshot_path?: string;
        content_sha256?: string;
        source_cutoff_at_utc?: string;
        manual_overrides?: Record<string, number>;
        override_purpose?: string;
        fallback_price?: null;
        missing_price_decisions?: Record<string, string>;
        manual_override_decisions?: Record<string, string>;
        [key: string]: unknown;
    };
    expected?: {
        solve_status: string;
        optimality_status: string;
        compile_status: string;
        verification_status: string;
    };
    caps: {
        max_states: number;
        max_sweeps: number;
        max_absolute_optimality_gap?: number;
        max_relative_optimality_gap?: number;
        solve_step_work_items: number;
        max_discovered_states: number;
        max_expanded_states: number;
        max_state_action_rows: number;
        max_transitions: number;
        max_reforge_work: number;
        max_solver_owned_bytes: number;
        max_compiled_nodes: number;
        max_compiled_edges: number;
        max_strategy_json_bytes: number;
        max_diagnostic_samples?: number;
        max_telemetry_json_bytes?: number;
        full_evidence?: boolean;
        strict_states?: boolean;
        kernel_reuse?: boolean;
        goal_progress_gated_reforges?: boolean;
        high_impact_executable_uppers?: boolean;
        worker_step_ms: number;
        cancel_ack_ms: number;
        [key: string]: number | boolean | undefined;
    };
    verification: {
        runs: number;
        seed: number;
        max_actions_per_run: number;
        max_graph_steps_per_run?: number;
        mean_cost_absolute_tolerance?: number;
        mean_cost_relative_tolerance?: number;
        minimum_success_rate?: number;
        exact_evaluation?: boolean;
        exact_cost_absolute_tolerance?: number;
        exact_cost_relative_tolerance?: number;
        exact_max_states?: number;
        exact_max_pairs?: number;
        exact_max_transitions?: number;
        exact_max_owned_bytes?: number;
    };
    cancellation?: {
        abort_after_progress_events?: number;
    };
    [key: string]: unknown;
}

export interface SolverBenchmarkManifest {
    schema_version:
        | "solver_benchmark_corpus_v1"
        | "natural_t1_benchmark_corpus_v1"
        | "cross_base_reliability_corpus_v1";
    corpus_id: string;
    artifact: {
        manifest_relative_path: string;
        canonical_source?: string;
        engine_abi_version?: number;
        artifact_schema_version?: number;
        source_version?: string;
        source_data_hash?: string;
        game_data_sha256?: string;
        strings_sha256?: string;
    };
    generator?: {
        engine_abi_version?: number;
        [key: string]: unknown;
    };
    comparison_profile?: {
        id: string;
        product_scope?: string;
        exact_required?: boolean;
        maximum_wall_seconds?: number;
        compiled_exact_evaluation_required?: boolean;
        verification_runs?: number;
        [key: string]: unknown;
    };
    owner_decisions?: string[];
    cases: string[];
}

export interface LoadedSolverBenchmarkCorpus {
    path: string;
    manifest: SolverBenchmarkManifest;
    cases: SolverBenchmarkCase[];
}

function readJson(path: string): unknown {
    return JSON.parse(readFileSync(path, "utf8")) as unknown;
}

function requireRecord(value: unknown, label: string): Record<string, unknown> {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
        throw new Error(`${label} must be a JSON object`);
    }
    return value as Record<string, unknown>;
}

function requireString(record: Record<string, unknown>, key: string, label: string): void {
    if (typeof record[key] !== "string" || record[key] === "") {
        throw new Error(`${label}.${key} must be a non-empty string`);
    }
}

function requireBoolean(
    record: Record<string, unknown>,
    key: string,
    label: string,
): void {
    if (typeof record[key] !== "boolean") {
        throw new Error(`${label}.${key} must be boolean`);
    }
}

function finitePrice(value: unknown, positive: boolean): value is number {
    return typeof value === "number" && Number.isFinite(value) &&
        (positive ? value > 0 : value >= 0);
}

function samePrice(left: number, right: number): boolean {
    return Math.abs(left - right) <=
        1e-12 * Math.max(1, Math.abs(left), Math.abs(right));
}

function requireDecision(
    decisions: Record<string, unknown>,
    key: string,
    label: string,
): void {
    if (typeof decisions[key] !== "string" || decisions[key] === "") {
        throw new Error(`${label}.${key} must disclose a non-empty decision`);
    }
}

function validateBoundedBestPolicyContract(
    value: unknown,
    label: string,
): void {
    const contract = requireRecord(value, label);
    for (const key of [
        "allow_exact",
        "bounded_requires_named_stop",
        "bounded_requires_strict_gap",
        "bounded_requires_open_obligations",
        "require_evaluated_upper",
        "require_cheapest_independently_evaluated_incumbent",
    ]) {
        requireBoolean(contract, key, label);
    }
}

function validateCompiledOperationContract(
    value: unknown,
    label: string,
): void {
    const contract = requireRecord(value, label);
    requireString(contract, "type", label);
    const parameters = requireRecord(
        contract.required_string_parameters,
        `${label}.required_string_parameters`,
    );
    for (const [key, parameter] of Object.entries(parameters)) {
        if (key === "" || typeof parameter !== "string" || parameter === "") {
            throw new Error(
                `${label} parameters must be non-empty strings`,
            );
        }
    }
}

function validateCase(value: unknown, path: string): SolverBenchmarkCase {
    const spec = requireRecord(value, path);
    if (spec.schema_version !== "solver_benchmark_case_v1") {
        throw new Error(`${path} has unsupported case schema`);
    }
    requireString(spec, "id", path);
    requireString(spec, "category", path);
    requireString(spec, "approval_status", path);
    if (typeof spec.benchmark_enabled !== "boolean") {
        throw new Error(`${path}.benchmark_enabled must be boolean`);
    }
    if (spec.comparison_profile !== undefined) {
        requireString(spec, "comparison_profile", path);
    }
    if (spec.watchdog_seconds !== undefined &&
        (!finitePrice(spec.watchdog_seconds, true))) {
        throw new Error(`${path}.watchdog_seconds must be a positive number`);
    }
    if (spec.bounded_best_policy_contract !== undefined) {
        validateBoundedBestPolicyContract(
            spec.bounded_best_policy_contract,
            `${path}.bounded_best_policy_contract`,
        );
    }
    if (spec.forced_winner_contract !== undefined) {
        const contract = requireRecord(
            spec.forced_winner_contract,
            `${path}.forced_winner_contract`,
        );
        for (const key of [
            "automatic_candidate_kind",
            "dependency_action_id",
            "required_compiled_operation_type",
            "target_side",
            "preserved_goal_side",
        ]) {
            requireString(contract, key, `${path}.forced_winner_contract`);
        }
        requireBoolean(
            contract,
            "dependency_must_not_be_primitive_candidate",
            `${path}.forced_winner_contract`,
        );
        if (!finitePrice(contract.snapshot_action_price, true) ||
            !finitePrice(contract.forced_action_price, true) ||
            contract.forced_action_price >= contract.snapshot_action_price) {
            throw new Error(
                `${path}.forced_winner_contract prices must disclose a positive lower forced value`,
            );
        }
        const sides = new Set(["prefix", "suffix"]);
        if (!sides.has(contract.target_side as string) ||
            !sides.has(contract.preserved_goal_side as string) ||
            contract.target_side === contract.preserved_goal_side) {
            throw new Error(
                `${path}.forced_winner_contract must use opposite prefix/suffix sides`,
            );
        }
    }
    if (spec.compiled_operation_contract !== undefined) {
        validateCompiledOperationContract(
            spec.compiled_operation_contract,
            `${path}.compiled_operation_contract`,
        );
    }
    if (spec.compiled_operation_contracts !== undefined) {
        if (!Array.isArray(spec.compiled_operation_contracts) ||
            spec.compiled_operation_contracts.length === 0) {
            throw new Error(
                `${path}.compiled_operation_contracts must be a non-empty array`,
            );
        }
        for (const [index, contract] of
            spec.compiled_operation_contracts.entries()) {
            validateCompiledOperationContract(
                contract,
                `${path}.compiled_operation_contracts[${index}]`,
            );
        }
    }
    if (spec.market_price_override_contracts !== undefined) {
        if (!Array.isArray(spec.market_price_override_contracts) ||
            spec.market_price_override_contracts.length === 0) {
            throw new Error(
                `${path}.market_price_override_contracts must be a non-empty array`,
            );
        }
        const keys = new Set<string>();
        for (const [index, raw] of
            spec.market_price_override_contracts.entries()) {
            const label = `${path}.market_price_override_contracts[${index}]`;
            const contract = requireRecord(raw, label);
            for (const key of ["price_key", "purpose"]) {
                requireString(contract, key, label);
            }
            const priceKey = contract.price_key as string;
            if (priceKey === "base" || priceKey.startsWith("beast:") ||
                keys.has(priceKey)) {
                throw new Error(
                    `${label}.price_key must be a unique non-Bestiary market key`,
                );
            }
            keys.add(priceKey);
            if (!finitePrice(contract.snapshot_chaos_value, true) ||
                !finitePrice(contract.override_chaos_value, true)) {
                throw new Error(`${label} prices must be positive numbers`);
            }
        }
    }
    if (spec.material_ratio_contract !== undefined) {
        const label = `${path}.material_ratio_contract`;
        const contract = requireRecord(spec.material_ratio_contract, label);
        requireString(contract, "numerator_price_key", label);
        requireString(contract, "denominator_price_key", label);
        if (contract.numerator_price_key === contract.denominator_price_key) {
            throw new Error(`${label} keys must be distinct`);
        }
        if (!finitePrice(contract.expected_ratio, true)) {
            throw new Error(`${label}.expected_ratio must be positive`);
        }
        requireBoolean(contract, "require_positive_denominator", label);
        requireBoolean(contract, "require_complete_pricing", label);
    }
    if (spec.expected !== undefined) {
        requireRecord(spec.expected, `${path}.expected`);
    }
    if (!spec.benchmark_enabled) {
        return spec as unknown as SolverBenchmarkCase;
    }
    for (const key of ["session", "start", "goal", "economy", "caps", "verification"]) {
        requireRecord(spec[key], `${path}.${key}`);
    }
    const session = spec.session as Record<string, unknown>;
    requireString(session, "base_metadata_path", `${path}.session`);
    if (!Number.isInteger(session.item_level)) {
        throw new Error(`${path}.session.item_level must be an integer`);
    }
    const start = spec.start as Record<string, unknown>;
    if (!Array.isArray(start.mods)) {
        throw new Error(`${path}.start.mods must be an array`);
    }
    const goal = spec.goal as Record<string, unknown>;
    if (!Array.isArray(goal.slots) || goal.slots.length === 0) {
        throw new Error(`${path}.goal.slots must be a non-empty array`);
    }
    const caps = spec.caps as Record<string, unknown>;
    for (const key of [
        "max_states", "max_sweeps", "solve_step_work_items",
        "max_discovered_states", "max_expanded_states",
        "max_state_action_rows", "max_transitions", "max_reforge_work",
        "max_solver_owned_bytes", "max_compiled_nodes", "max_compiled_edges",
        "max_strategy_json_bytes", "worker_step_ms", "cancel_ack_ms",
    ]) {
        if (typeof caps[key] !== "number" || (caps[key] as number) < 0) {
            throw new Error(`${path}.caps.${key} must be a non-negative number`);
        }
    }
    for (const key of [
        "full_evidence",
        "strict_states",
        "kernel_reuse",
        "goal_progress_gated_reforges",
        "high_impact_executable_uppers",
    ]) {
        if (key in caps && typeof caps[key] !== "boolean") {
            throw new Error(`${path}.caps.${key} must be boolean`);
        }
    }
    if (spec.bounded_best_policy_contract !== undefined) {
        const expected = requireRecord(spec.expected, `${path}.expected`);
        if (expected.compile_status !== "compiled") {
            throw new Error(
                `${path}.bounded_best_policy_contract requires compiled output`,
            );
        }
        const verification = spec.verification as Record<string, unknown>;
        if (verification.exact_evaluation !== true) {
            throw new Error(
                `${path}.bounded_best_policy_contract requires exact evaluation`,
            );
        }
    }
    if (spec.compiled_operation_contract !== undefined ||
        spec.compiled_operation_contracts !== undefined ||
        spec.material_ratio_contract !== undefined) {
        const expected = requireRecord(spec.expected, `${path}.expected`);
        if (expected.compile_status !== "compiled") {
            throw new Error(
                `${path} compiled-policy contracts require compiled output`,
            );
        }
    }
    if (spec.material_ratio_contract !== undefined) {
        const verification = spec.verification as Record<string, unknown>;
        if (verification.exact_evaluation !== true ||
            !Number.isSafeInteger(verification.runs) ||
            (verification.runs as number) <= 0) {
            throw new Error(
                `${path}.material_ratio_contract requires exact evaluation and sampled verification`,
            );
        }
    }
    return spec as unknown as SolverBenchmarkCase;
}

export function loadSolverBenchmarkCorpus(path: string): LoadedSolverBenchmarkCorpus {
    const absolute = resolve(path);
    const raw = requireRecord(readJson(absolute), absolute);
    if (
        raw.schema_version !== "solver_benchmark_corpus_v1" &&
        raw.schema_version !== "natural_t1_benchmark_corpus_v1" &&
        raw.schema_version !== "cross_base_reliability_corpus_v1"
    ) {
        throw new Error(`${absolute} has unsupported corpus schema`);
    }
    requireString(raw, "corpus_id", absolute);
    if (!Array.isArray(raw.cases) || !raw.cases.every((entry) => typeof entry === "string")) {
        throw new Error(`${absolute}.cases must be an array of file names`);
    }
    const manifest = raw as unknown as SolverBenchmarkManifest;
    const cases = manifest.cases.map((entry) => {
        const casePath = resolve(dirname(absolute), entry);
        return validateCase(readJson(casePath), casePath);
    });
    const ids = new Set<string>();
    for (const spec of cases) {
        if (ids.has(spec.id)) throw new Error(`duplicate solver benchmark id: ${spec.id}`);
        ids.add(spec.id);
    }
    if (manifest.comparison_profile !== undefined) {
        const profile = requireRecord(
            manifest.comparison_profile,
            `${absolute}.comparison_profile`,
        );
        requireString(profile, "id", `${absolute}.comparison_profile`);
        const boundedProfile = "product_scope" in profile ||
            "maximum_wall_seconds" in profile ||
            "exact_required" in profile;
        if (boundedProfile) {
            requireString(profile, "product_scope", `${absolute}.comparison_profile`);
            requireBoolean(profile, "exact_required", `${absolute}.comparison_profile`);
            requireBoolean(
                profile,
                "compiled_exact_evaluation_required",
                `${absolute}.comparison_profile`,
            );
            if (!finitePrice(profile.maximum_wall_seconds, true)) {
                throw new Error(
                    `${absolute}.comparison_profile.maximum_wall_seconds must be positive`,
                );
            }
            if (!Number.isSafeInteger(profile.verification_runs) ||
                (profile.verification_runs as number) < 0) {
                throw new Error(
                    `${absolute}.comparison_profile.verification_runs must be a non-negative integer`,
                );
            }
        }
        for (const spec of cases) {
            if (boundedProfile && spec.comparison_profile !== profile.id) {
                throw new Error(
                    `${spec.id} comparison profile does not match its corpus`,
                );
            }
            if (boundedProfile &&
                (!finitePrice(spec.watchdog_seconds, true) ||
                    spec.watchdog_seconds >
                        (profile.maximum_wall_seconds as number))) {
                throw new Error(
                    `${spec.id} watchdog must be positive and within its corpus profile`,
                );
            }
            if (boundedProfile && profile.exact_required === true &&
                spec.expected?.optimality_status !== "exact") {
                throw new Error(
                    `${spec.id} must require exact optimality under its corpus profile`,
                );
            }
            if (boundedProfile &&
                profile.compiled_exact_evaluation_required === true &&
                spec.verification.exact_evaluation !== true) {
                throw new Error(
                    `${spec.id} must require independent exact evaluation`,
                );
            }
            if (boundedProfile &&
                spec.verification.runs !== profile.verification_runs) {
                throw new Error(
                    `${spec.id} verification runs do not match its corpus profile`,
                );
            }
        }
    }
    return { path: absolute, manifest, cases };
}

export function materializeSolverBenchmarkEconomy(
    spec: SolverBenchmarkCase,
    repoRoot: string,
): unknown {
    const economy = spec.economy;
    if (!economy.snapshot_path) {
        requireRecord(economy.prices, `${spec.id}.economy.prices`);
        return economy;
    }

    const snapshotPath = resolve(repoRoot, economy.snapshot_path);
    const snapshot = requireRecord(readJson(snapshotPath), snapshotPath);
    if (snapshot.id !== economy.id) {
        throw new Error(`${spec.id} pinned economy snapshot id mismatch`);
    }
    const metadata = requireRecord(snapshot.metadata, `${snapshotPath}.metadata`);
    if (metadata.content_sha256 !== economy.content_sha256) {
        throw new Error(`${spec.id} pinned economy content hash mismatch`);
    }
    if (metadata.source_cutoff_at_utc !== economy.source_cutoff_at_utc) {
        throw new Error(`${spec.id} pinned economy cutoff mismatch`);
    }
    const sourcePrices = requireRecord(snapshot.prices, `${snapshotPath}.prices`);
    if ("base" in sourcePrices) {
        throw new Error(`${spec.id} requires Restart/base to remain unpriced`);
    }
    if (economy.fallback_price !== null) {
        throw new Error(`${spec.id} requires a null price fallback`);
    }
    const overrides = requireRecord(
        economy.manual_overrides,
        `${spec.id}.economy.manual_overrides`,
    );
    const entries = Object.entries(overrides);
    if (entries.some(([, value]) => !finitePrice(value, true))) {
        throw new Error(`${spec.id} has an invalid manual price override`);
    }

    const baseOverride = overrides.base;
    const missingDecisions = economy.missing_price_decisions === undefined
        ? {}
        : requireRecord(
            economy.missing_price_decisions,
            `${spec.id}.economy.missing_price_decisions`,
        );
    const manualDecisions = economy.manual_override_decisions === undefined
        ? {}
        : requireRecord(
            economy.manual_override_decisions,
            `${spec.id}.economy.manual_override_decisions`,
        );
    const legacyBaseDisclosure =
        economy.override_purpose === "r3f_restart_route_gate_not_market_quote";
    const generatedReliability = spec.category === "cross_base_reliability";
    if (baseOverride !== undefined) {
        if (!finitePrice(baseOverride, true)) {
            throw new Error(`${spec.id} has an invalid base override`);
        }
        if (!legacyBaseDisclosure && !generatedReliability) {
            requireDecision(
                missingDecisions,
                "base",
                `${spec.id}.economy.missing_price_decisions`,
            );
        }
    }

    const forced = spec.forced_winner_contract;
    const marketOverrides = entries.filter(([key]) => key !== "base");
    const additionalMarketOverrides = marketOverrides.filter(
        ([key]) => key !== forced?.dependency_action_id,
    );
    const disclosedMarketOverrides =
        spec.market_price_override_contracts;
    if (disclosedMarketOverrides !== undefined) {
        if ((forced === undefined && baseOverride !== undefined) ||
            economy.override_purpose !==
                "synthetic_forced_winner_gate_not_market_quote" ||
            additionalMarketOverrides.length !==
                disclosedMarketOverrides.length) {
            throw new Error(
                `${spec.id} disclosed market overrides must be the complete non-Bestiary override set`,
            );
        }
        const declaredKeys = new Set<string>();
        for (const contract of disclosedMarketOverrides) {
            const key = contract.price_key;
            const sourcePrice = sourcePrices[key];
            const override = overrides[key];
            if (key === "base" || key.startsWith("beast:") ||
                declaredKeys.has(key) || !finitePrice(sourcePrice, true) ||
                !finitePrice(override, true) ||
                !samePrice(sourcePrice, contract.snapshot_chaos_value) ||
                !samePrice(override, contract.override_chaos_value)) {
                throw new Error(
                    `${spec.id} disclosed market override for ${key} does not match its pinned quote and override`,
                );
            }
            declaredKeys.add(key);
            requireDecision(
                manualDecisions,
                key,
                `${spec.id}.economy.manual_override_decisions`,
            );
        }
        if (additionalMarketOverrides.some(
            ([key]) => !declaredKeys.has(key))) {
            throw new Error(
                `${spec.id} contains an undisclosed market-price override`,
            );
        }
    } else if (forced === undefined) {
        if (marketOverrides.length !== 0 || entries.length > 1) {
            throw new Error(
                `${spec.id} contains an undisclosed market-price override`,
            );
        }
    } else if (additionalMarketOverrides.length !== 0) {
        throw new Error(
            `${spec.id} contains an undisclosed market-price override`,
        );
    }
    if (forced !== undefined) {
        const dependency = forced.dependency_action_id;
        const sourcePrice = sourcePrices[dependency];
        const actionOverride = overrides[dependency];
        if (
            economy.override_purpose !==
                "synthetic_forced_winner_gate_not_market_quote" ||
            dependency === "base" ||
            entries.length !== 2 + additionalMarketOverrides.length ||
            baseOverride === undefined || actionOverride === undefined ||
            marketOverrides.filter(([key]) => key === dependency).length !== 1 ||
            !finitePrice(sourcePrice, true) || !finitePrice(actionOverride, true) ||
            !finitePrice(forced.snapshot_action_price, true) ||
            !finitePrice(forced.forced_action_price, true) ||
            forced.forced_action_price >= forced.snapshot_action_price ||
            !samePrice(sourcePrice, forced.snapshot_action_price) ||
            !samePrice(actionOverride, forced.forced_action_price)
        ) {
            throw new Error(
                `${spec.id} forced-winner overrides do not match the disclosed snapshot and dependency prices`,
            );
        }
        requireDecision(
            missingDecisions,
            "base",
            `${spec.id}.economy.missing_price_decisions`,
        );
        requireDecision(
            manualDecisions,
            dependency,
            `${spec.id}.economy.manual_override_decisions`,
        );
    }

    const sources = snapshot.sources === undefined
        ? undefined
        : { ...requireRecord(snapshot.sources, `${snapshotPath}.sources`) };
    return {
        ...snapshot,
        ...economy,
        metadata,
        prices: { ...sourcePrices, ...overrides },
        ...(sources === undefined ? {} : { sources }),
    };
}

export function materializeSolverBenchmarkGoal(
    spec: SolverBenchmarkCase,
    pricedActionIds?: readonly string[],
): SolverGoal {
    const envelope = spec.product_action_envelope;
    if (!envelope) return spec.goal;
    if (envelope.mode !== "calculator_goal_relevant_priced_v1") {
        throw new Error(`${spec.id} has an unsupported product action-envelope mode`);
    }
    if (pricedActionIds === undefined &&
        envelope.expected_priced_action_ids === undefined) {
        return envelope.envelope_goal;
    }
    const actions = pricedActionIds ?? envelope.expected_priced_action_ids;
    if (
        !Array.isArray(actions) ||
        (pricedActionIds === undefined && actions.length === 0) ||
        !actions.every((entry) =>
            typeof entry === "string" && entry.length > 0)
    ) {
        throw new Error(`${spec.id} has an invalid pinned product action envelope`);
    }
    if (pricedActionIds !== undefined && actions.length === 0) {
        return spec.goal;
    }
    if (pricedActionIds !== undefined &&
        envelope.expected_priced_action_ids !== undefined &&
        (pricedActionIds.length !== envelope.expected_priced_action_ids.length ||
            pricedActionIds.some(
                (entry, index) =>
                    entry !== envelope.expected_priced_action_ids?.[index],
            ))) {
        throw new Error(`${spec.id} derived product action envelope changed`);
    }
    return {
        ...spec.goal,
        actions: [...actions],
    };
}

export function validateCorpusArtifactPins(
    manifest: SolverBenchmarkManifest,
    artifactValue: unknown,
    abiVersion: number,
): void {
    const artifact = requireRecord(artifactValue, "artifact manifest");
    const source = requireRecord(artifact.source, "artifact manifest.source");
    const files = requireRecord(artifact.files, "artifact manifest.files");
    const gameData = requireRecord(files["game-data.json"], "artifact game-data file");
    const strings = requireRecord(files["strings.json"], "artifact strings file");
    const pins = manifest.artifact;
    const comparisons: Array<[string, unknown, unknown]> = [
        [
            "engine ABI",
            abiVersion,
            pins.engine_abi_version ??
                manifest.generator?.engine_abi_version,
        ],
        ["artifact schema", artifact.artifact_schema_version, pins.artifact_schema_version],
        ["source version", source.source_version, pins.source_version],
        ["source data hash", source.data_hash, pins.source_data_hash],
        ["game-data sha256", gameData.sha256, pins.game_data_sha256],
        ["strings sha256", strings.sha256, pins.strings_sha256],
    ];
    for (const [label, actual, expected] of comparisons) {
        if (expected === undefined) {
            throw new Error(`solver benchmark corpus is missing ${label} pin`);
        }
        if (actual !== expected) {
            throw new Error(
                `solver benchmark ${label} mismatch: expected ${String(expected)}, got ${String(actual)}`,
            );
        }
    }
}
