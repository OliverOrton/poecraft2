import type {
    BaseInfo,
    SolveOptions,
    SolverGoal,
} from "./engine-protocol";
import type {
    EconomySnapshot,
    PinnedEconomy,
} from "./workspace/economy-service";

export const SOLVER_LAB_CALCULATOR_EXPORT_VERSION =
    "solver_lab_calculator_export_v1" as const;

interface ExportedSlot {
    mod_id?: unknown;
    flags?: unknown;
}

interface ExportedItemState {
    item_flags?: unknown;
    generic_influence_bits?: unknown;
    searing_exarch_tier?: unknown;
    eater_of_worlds_tier?: unknown;
    implicits?: unknown;
    prefixes?: unknown;
    suffixes?: unknown;
}

export interface SolverLabCalculatorExportInput {
    name: string;
    description?: string;
    base: BaseInfo;
    itemLevel: number;
    itemRarity: string;
    itemState: unknown;
    checkpointPresent: boolean;
    goal: SolverGoal;
    economy: PinnedEconomy;
    options?: SolveOptions;
    watchdogSeconds?: number;
    requestedBoundedFinishSeconds?: number;
    modKeyForId: (modId: number) => string | undefined;
}

export interface SolverLabCalculatorExport {
    schema_version: typeof SOLVER_LAB_CALCULATOR_EXPORT_VERSION;
    name: string;
    suggested_case_id: string;
    calculator: {
        description: string;
        session: {
            base_metadata_path: string;
            base_name: string;
            item_class_key: string;
            item_level: number;
            minimum_item_level: number;
        };
        start: {
            rarity: "normal" | "magic" | "rare";
            with_implicits: boolean;
            generic_influence_bits: number;
            searing_exarch_tier: number;
            eater_of_worlds_tier: number;
            mods: Array<{ key: string; flags: string[] }>;
        };
        goal: SolverGoal;
        economy: EconomySnapshot;
        solve: {
            watchdog_seconds: number;
            requested_bounded_finish_seconds: number;
            options: SolveOptions;
        };
    };
}

function record(value: unknown, label: string): Record<string, unknown> {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
        throw new Error(`${label} is not a JSON object.`);
    }
    return value as Record<string, unknown>;
}

function finiteInteger(value: unknown, fallback = 0): number {
    return typeof value === "number" && Number.isFinite(value)
        ? Math.trunc(value)
        : fallback;
}

function exportedSlots(value: unknown, label: string): ExportedSlot[] {
    if (value === undefined) return [];
    if (!Array.isArray(value)) {
        throw new Error(`${label} is not an array.`);
    }
    return value.map((slot, index) =>
        record(slot, `${label}[${index}]`) as ExportedSlot,
    );
}

function startMods(
    state: ExportedItemState,
    modKeyForId: (modId: number) => string | undefined,
): Array<{ key: string; flags: string[] }> {
    return [
        ...exportedSlots(state.prefixes, "item prefixes"),
        ...exportedSlots(state.suffixes, "item suffixes"),
    ].map((slot, index) => {
        if (typeof slot.mod_id !== "number" || !Number.isInteger(slot.mod_id)) {
            throw new Error(`Item affix ${index + 1} has no session modifier id.`);
        }
        const key = modKeyForId(slot.mod_id);
        if (!key) {
            throw new Error(
                `Item affix ${index + 1} cannot be mapped to a canonical modifier key.`,
            );
        }
        const rawFlags = finiteInteger(slot.flags);
        if ((rawFlags & ~7) !== 0) {
            throw new Error(
                `Item affix ${index + 1} uses flags the native Lab case format cannot preserve.`,
            );
        }
        const flags: string[] = [];
        if ((rawFlags & 1) !== 0) flags.push("fractured");
        if ((rawFlags & 2) !== 0) flags.push("crafted");
        if ((rawFlags & 4) !== 0) flags.push("veiled");
        return { key, flags };
    });
}

function normalizedRarity(value: string): "normal" | "magic" | "rare" {
    if (value === "normal" || value === "magic" || value === "rare") {
        return value;
    }
    throw new Error(`Unsupported input rarity: ${value || "unknown"}.`);
}

function positiveSeconds(value: number, label: string): number {
    if (!Number.isFinite(value) || value <= 0) {
        throw new Error(`${label} must be positive.`);
    }
    return value;
}

function slug(value: string): string {
    const result = value
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9._-]+/g, "-")
        .replace(/^[-._]+|[-._]+$/g, "")
        .slice(0, 128);
    return result || "calculator-case";
}

export function buildSolverLabCalculatorExport(
    input: SolverLabCalculatorExportInput,
): SolverLabCalculatorExport {
    const name = input.name.trim();
    if (!name) throw new Error("Give the Lab case a name.");
    if (!input.goal.slots.length) {
        throw new Error("Define at least one goal modifier before exporting.");
    }
    if (input.checkpointPresent) {
        throw new Error(
            "The Lab start format cannot preserve an active Imprint checkpoint. Restore or remove it before exporting.",
        );
    }
    const state = record(input.itemState, "exported Calculator item") as ExportedItemState;
    const itemFlags = finiteInteger(state.item_flags);
    if (itemFlags !== 0) {
        throw new Error(
            "The Lab start format cannot preserve corrupted, mirrored, split, or synthesised item flags.",
        );
    }
    const economy = input.economy.snapshot;
    if (economy.metadata.league_key !== "allflame") {
        throw new Error(
            "The current Native Solver Lab profile requires the Allflame economy.",
        );
    }
    if (!economy.metadata.content_sha256) {
        throw new Error("The selected Allflame economy has no pinned source digest.");
    }
    const watchdog = positiveSeconds(input.watchdogSeconds ?? 300, "Watchdog");
    const bounded = positiveSeconds(
        input.requestedBoundedFinishSeconds ?? 240,
        "Bounded finish",
    );
    if (bounded >= watchdog) {
        throw new Error("Bounded finish must precede the watchdog.");
    }
    const options: SolveOptions = {
        ...(input.options ?? {}),
        solve_profile: "calculator_product_v1",
        goal_progress_gated_reforges: true,
        allow_economic_restart: false,
        consider_imprint_programs: false,
    };
    const implicits = exportedSlots(state.implicits, "item implicits");
    return {
        schema_version: SOLVER_LAB_CALCULATOR_EXPORT_VERSION,
        name,
        suggested_case_id: slug(name),
        calculator: {
            description: input.description?.trim() || name,
            session: {
                base_metadata_path: input.base.path,
                base_name: input.base.name,
                item_class_key: input.base.item_class_key,
                item_level: input.itemLevel,
                minimum_item_level: input.itemLevel,
            },
            start: {
                rarity: normalizedRarity(input.itemRarity),
                with_implicits: implicits.length > 0,
                generic_influence_bits: finiteInteger(
                    state.generic_influence_bits,
                ),
                searing_exarch_tier: finiteInteger(state.searing_exarch_tier),
                eater_of_worlds_tier: finiteInteger(
                    state.eater_of_worlds_tier,
                ),
                mods: startMods(state, input.modKeyForId),
            },
            goal: structuredClone(input.goal),
            economy: structuredClone(economy),
            solve: {
                watchdog_seconds: watchdog,
                requested_bounded_finish_seconds: bounded,
                options,
            },
        },
    };
}
