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
    benchmark_mode?: "cancel_after_first_step";
    description: string;
    session: {
        base_metadata_path: string;
        base_name: string;
        item_level: number;
    };
    start: {
        rarity: "normal" | "magic" | "rare";
        with_implicits: boolean;
        searing_exarch_tier?: number;
        eater_of_worlds_tier?: number;
        mods: Array<{ key: string; flags: BenchmarkModFlag[] }>;
    };
    goal: SolverGoal;
    allowed_mechanic_families: string[];
    economy: {
        version: "v1";
        id?: string;
        prices: Record<string, number>;
    };
    expected: {
        solve_status: string;
        optimality_status: string;
        compile_status: string;
        verification_status: string;
    };
    caps: {
        max_states: number;
        max_sweeps: number;
        solve_step_work_items: number;
        worker_step_ms: number;
        cancel_ack_ms: number;
        [key: string]: number;
    };
    verification: {
        runs: number;
        seed: number;
        max_actions_per_run: number;
        mean_cost_absolute_tolerance?: number;
        mean_cost_relative_tolerance?: number;
        minimum_success_rate?: number;
    };
    cancellation?: {
        abort_after_progress_events?: number;
    };
    [key: string]: unknown;
}

export interface SolverBenchmarkManifest {
    schema_version: "solver_benchmark_corpus_v1";
    corpus_id: string;
    artifact: {
        manifest_relative_path: string;
        canonical_source: string;
        engine_abi_version?: number;
        artifact_schema_version?: number;
        source_version?: string;
        source_data_hash?: string;
        game_data_sha256?: string;
        strings_sha256?: string;
    };
    owner_decisions: string[];
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
    requireRecord(spec.expected, `${path}.expected`);
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
    return spec as unknown as SolverBenchmarkCase;
}

export function loadSolverBenchmarkCorpus(path: string): LoadedSolverBenchmarkCorpus {
    const absolute = resolve(path);
    const raw = requireRecord(readJson(absolute), absolute);
    if (raw.schema_version !== "solver_benchmark_corpus_v1") {
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
    return { path: absolute, manifest, cases };
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
        ["engine ABI", abiVersion, pins.engine_abi_version],
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
