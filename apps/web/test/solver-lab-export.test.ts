import assert from "node:assert/strict";

import {
    buildSolverLabCalculatorExport,
    SOLVER_LAB_CALCULATOR_EXPORT_VERSION,
} from "../src/app/solver-lab-export";
import type { PinnedEconomy } from "../src/app/workspace/economy-service";

const economy: PinnedEconomy = {
    snapshot: {
        version: "v1",
        id: "economy:allflame:test:pin:case",
        metadata: {
            schema_version: 1,
            game: "poe1",
            realm: "pc",
            league_key: "allflame",
            league_name: "Allflame",
            source: "test",
            source_cutoff_at_utc: "2026-08-09T18:34:48Z",
            created_at_utc: "2026-08-09T18:34:48Z",
            game_data_hash: "game",
            price_count: 2,
            missing_keys: [],
            low_confidence_keys: [],
            content_sha256: "source-digest",
        },
        prices: { chaos: 1, exalt: 10 },
    },
    profile: "allflame",
    sourceSnapshotId: "economy:allflame:test",
    sourceContentSha256: "source-digest",
    sourceCutoffAtUtc: "2026-08-09T18:34:48Z",
    priceSources: { chaos: "quote", exalt: "override" },
    fallbackPrice: null,
    identity: {
        profile: "allflame",
        effective_snapshot_id: "economy:allflame:test:pin:case",
        source_snapshot_id: "economy:allflame:test",
        source_content_sha256: "source-digest",
        source_cutoff_at_utc: "2026-08-09T18:34:48Z",
        league_name: "Allflame",
        status: "fresh",
        low_confidence_keys: [],
        price_sources: { chaos: "quote", exalt: "override" },
        fallback_price: null,
    },
};

const result = buildSolverLabCalculatorExport({
    name: "Conquest Lamellar 4 → 5",
    base: {
        path: "Metadata/Items/Armours/BodyArmours/BodyStrDex20",
        name: "Conquest Lamellar",
        item_class_key: "Body Armour",
        drop_level: 86,
        support: 0,
    },
    itemLevel: 86,
    itemRarity: "rare",
    itemState: {
        item_flags: 0,
        generic_influence_bits: 0,
        searing_exarch_tier: 1,
        eater_of_worlds_tier: 2,
        prefixes: [
            { mod_id: 4, flags: 1 },
            { mod_id: 8, flags: 0 },
        ],
        suffixes: [{ mod_id: 12, flags: 2 }],
        implicits: [{ mod_id: 99, flags: 0 }],
    },
    checkpointPresent: false,
    goal: {
        version: "v1",
        rarity: "rare",
        action_mode: "goal_relevant",
        min_satisfied_slots: 2,
        slots: [
            { family_mod_key: "family-a", min_tier: 1 },
            { family_mod_key: "family-b", min_tier: 1 },
        ],
        disabled_action_families: ["fossil"],
    },
    economy,
    options: {
        max_absolute_optimality_gap: 3,
        allow_economic_restart: true,
        consider_imprint_programs: true,
    },
    modKeyForId: (id) => ({ 4: "prefix-a", 8: "prefix-b", 12: "suffix-a" })[id],
});

assert.equal(result.schema_version, SOLVER_LAB_CALCULATOR_EXPORT_VERSION);
assert.equal(result.suggested_case_id, "conquest-lamellar-4-5");
assert.equal(result.calculator.start.with_implicits, true);
assert.deepEqual(result.calculator.start.mods, [
    { key: "prefix-a", flags: ["fractured"] },
    { key: "prefix-b", flags: [] },
    { key: "suffix-a", flags: ["crafted"] },
]);
assert.deepEqual(result.calculator.goal.disabled_action_families, ["fossil"]);
assert.equal(result.calculator.solve.options.max_absolute_optimality_gap, 3);
assert.equal(result.calculator.solve.options.allow_economic_restart, false);
assert.equal(result.calculator.solve.options.consider_imprint_programs, false);
assert.equal(result.calculator.solve.options.goal_progress_gated_reforges, true);

assert.throws(
    () =>
        buildSolverLabCalculatorExport({
            ...({
                name: "checkpoint",
                base: {
                    path: "base",
                    name: "Base",
                    item_class_key: "Body Armour",
                    drop_level: 1,
                    support: 0,
                },
                itemLevel: 86,
                itemRarity: "rare",
                itemState: { prefixes: [], suffixes: [] },
                checkpointPresent: true,
                goal: {
                    slots: [{ family_mod_key: "family-a" }],
                },
                economy,
                modKeyForId: () => undefined,
            }),
        }),
    /cannot preserve an active Imprint checkpoint/,
);

assert.throws(
    () =>
        buildSolverLabCalculatorExport({
            name: "unknown affix",
            base: {
                path: "base",
                name: "Base",
                item_class_key: "Body Armour",
                drop_level: 1,
                support: 0,
            },
            itemLevel: 86,
            itemRarity: "rare",
            itemState: {
                prefixes: [{ mod_id: 7, flags: 0 }],
                suffixes: [],
            },
            checkpointPresent: false,
            goal: { slots: [{ family_mod_key: "family-a" }] },
            economy,
            modKeyForId: () => undefined,
        }),
    /cannot be mapped/,
);

console.log("solver lab Calculator export tests passed");
