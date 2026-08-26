import assert from "node:assert/strict";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";

import {
    benchmarkConsiderImprintPrograms,
    loadSolverBenchmarkCorpus,
    materializeSolverBenchmarkEconomy,
    materializeSolverBenchmarkGoal,
    validateCorpusArtifactPins,
} from "./solver-benchmark-corpus";

const MANIFEST = new URL(
    "../../../fixtures/solver-benchmarks/v1/manifest.json",
    import.meta.url,
);
const RELIABILITY_MANIFEST = new URL(
    "../../../fixtures/solver-reliability/v1/manifest.json",
    import.meta.url,
);
const SOLVER_ANYTIME_MANIFEST = new URL(
    "../../../fixtures/solver-anytime-proof-realignment/v1/manifest.json",
    import.meta.url,
);
const GOAL_REALIGNMENT_MANIFESTS = [
    new URL(
        "../../../fixtures/solver-goal-realignment/v1/manifest.json",
        import.meta.url,
    ),
    new URL(
        "../../../fixtures/solver-goal-realignment/v1/five-natural-t1-manifest.json",
        import.meta.url,
    ),
    new URL(
        "../../../fixtures/solver-goal-realignment/v1/five-mod-manifest.json",
        import.meta.url,
    ),
    new URL(
        "../../../fixtures/solver-goal-realignment/v1/gate6-manifest.json",
        import.meta.url,
    ),
];

test("S7 solver benchmark corpus is versioned, unique, and explicitly gated", () => {
    const corpus = loadSolverBenchmarkCorpus(fileURLToPath(MANIFEST));
    assert.equal(corpus.manifest.schema_version, "solver_benchmark_corpus_v1");
    assert.ok(corpus.cases.length >= 5);
    assert.equal(new Set(corpus.cases.map((entry) => entry.id)).size, corpus.cases.length);
    for (const entry of corpus.cases) {
        assert.equal(typeof entry.benchmark_enabled, "boolean");
        assert.ok(entry.expected?.solve_status.length);
        if (entry.benchmark_enabled) {
            assert.ok(entry.caps.worker_step_ms > 0);
            assert.ok(entry.caps.cancel_ack_ms > 0);
        }
    }
});

test("owner-approved real crafts are benchmark-enabled", () => {
    const corpus = loadSolverBenchmarkCorpus(fileURLToPath(MANIFEST));
    const proposedRealCrafts = corpus.cases.filter((entry) =>
        ["ordinary", "advanced", "endgame"].includes(entry.category),
    );
    assert.ok(proposedRealCrafts.length >= 3);
    assert.ok(proposedRealCrafts.every((entry) => entry.benchmark_enabled));
});

test("general benchmarks disclose Imprint off and retain explicit opt-in", () => {
    const corpus = loadSolverBenchmarkCorpus(fileURLToPath(MANIFEST));
    const ordinary = corpus.cases.find((entry) =>
        entry.id === "oracle-real-one-mod");
    assert.ok(ordinary);
    assert.equal(ordinary.caps.consider_imprint_programs, false);
    assert.equal(benchmarkConsiderImprintPrograms(ordinary), false);
    assert.equal(benchmarkConsiderImprintPrograms({
        ...ordinary,
        caps: { ...ordinary.caps, consider_imprint_programs: true },
    }), true);
});

test("Gate 1 solver-anytime controls disclose exact mechanic-family proofs", () => {
    const corpus = loadSolverBenchmarkCorpus(
        fileURLToPath(SOLVER_ANYTIME_MANIFEST),
    );
    assert.equal(
        corpus.manifest.corpus_id,
        "poecraft2-solver-anytime-proof-realignment-gate1-v1",
    );
    assert.equal(corpus.cases.length, 19);
    const focused = corpus.cases.filter((entry) =>
        entry.mechanic_family_control !== undefined
    );
    assert.deepEqual(
        focused.map((entry) => entry.mechanic_family_control?.id).sort(),
        [
            "eldritch_annul_exalt_side_intent",
            "eldritch_chaos_side_intent",
            "essence",
            "explicit_imprint_retry",
            "fossil_added_mod",
            "fossil_forced_mod",
            "fossil_pure_reweight",
            "fracture_miss_and_ordinary_primitives",
            "harvest_augment_target_tag",
            "harvest_reforge_target_tag",
            "protection_metamod_and_temporary_bench",
        ],
    );
    for (const entry of focused) {
        const control = entry.mechanic_family_control;
        assert.ok(control);
        assert.ok(control.registered_action_ids.length > 0);
        assert.ok(control.synthetic_price_keys.length > 0);
        assert.match(control.synthetic_price_purpose, /not market evidence/);
        assert.equal(entry.verification.exact_evaluation, true);
        assert.equal(entry.expected?.compile_status, "compiled");
        assert.ok(entry.compiled_operation_contract !== undefined ||
            entry.compiled_operation_contracts !== undefined);
    }
});

test("corpus artifact pins reject stale WASM/data combinations", () => {
    const corpus = loadSolverBenchmarkCorpus(fileURLToPath(MANIFEST));
    const artifactPath = fileURLToPath(
        new URL("../../../data/compiled/current/manifest.json", import.meta.url),
    );
    const artifact = JSON.parse(readFileSync(artifactPath, "utf8")) as unknown;
    validateCorpusArtifactPins(corpus.manifest, artifact, 2);
    assert.throws(
        () => validateCorpusArtifactPins(corpus.manifest, artifact, 999),
        /engine ABI mismatch/,
    );
});

test("state-scaling corpus materializes its pinned economy snapshot", () => {
    const scalingManifest = fileURLToPath(new URL(
        "../../../fixtures/solver-scaling/v1/acceptance-manifest.json",
        import.meta.url,
    ));
    const repoRoot = fileURLToPath(new URL("../../../", import.meta.url));
    const corpus = loadSolverBenchmarkCorpus(scalingManifest);
    const product = corpus.cases.find((entry) =>
        entry.id === "solver-scaling-dire-pelt-three-t1-product");
    assert.ok(product);
    const economy = materializeSolverBenchmarkEconomy(
        product,
        repoRoot,
    ) as { id: string; prices: Record<string, number> };
    assert.equal(economy.id, product.economy.id);
    assert.equal(economy.prices.base, 1);
    assert.equal(typeof economy.prices.chaos, "number");
    const goal = materializeSolverBenchmarkGoal(product);
    assert.deepEqual(
        goal.actions,
        product.product_action_envelope?.expected_priced_action_ids,
    );
    assert.equal(goal.actions?.length, 32);
    assert.ok(goal.actions?.includes("restart"));
});

test("goal-realignment corpora validate every current case and economy", () => {
    const repoRoot = fileURLToPath(new URL("../../../", import.meta.url));
    const artifactPath = fileURLToPath(
        new URL("../../../data/compiled/current/manifest.json", import.meta.url),
    );
    const artifact = JSON.parse(readFileSync(artifactPath, "utf8")) as unknown;
    const loaded = GOAL_REALIGNMENT_MANIFESTS.map((manifestUrl) =>
        loadSolverBenchmarkCorpus(fileURLToPath(manifestUrl)));

    assert.deepEqual(
        loaded.map((corpus) => corpus.manifest.corpus_id),
        [
            "poecraft2-solver-goal-realignment-v1",
            "poecraft2-solver-goal-realignment-five-natural-t1-v1",
            "poecraft2-solver-goal-realignment-five-mod-v1",
            "poecraft2-solver-goal-realignment-gate6-v1",
        ],
    );
    assert.deepEqual(loaded.map((corpus) => corpus.cases.length), [5, 3, 1, 2]);

    const cases = loaded.flatMap((corpus) => {
        validateCorpusArtifactPins(corpus.manifest, artifact, 2);
        const profile = corpus.manifest.comparison_profile;
        assert.ok(profile);
        assert.equal(typeof profile.maximum_wall_seconds, "number");
        const maximumWallSeconds = profile.maximum_wall_seconds ?? 0;
        for (const entry of corpus.cases) {
            assert.equal(entry.comparison_profile, profile.id);
            assert.ok((entry.watchdog_seconds ?? 0) > 0);
            assert.ok(
                (entry.watchdog_seconds ?? Number.POSITIVE_INFINITY) <=
                    maximumWallSeconds,
            );
        }
        return corpus.cases;
    });
    const economies = new Map(cases.map((entry) => [
        entry.id,
        materializeSolverBenchmarkEconomy(entry, repoRoot) as {
            id: string;
            prices: Record<string, number>;
            sources?: Record<string, string>;
            manual_overrides?: Record<string, number>;
            missing_price_decisions?: Record<string, string>;
            manual_override_decisions?: Record<string, string>;
        },
    ]));

    assert.equal(
        economies.get("conquest-lamellar-allflame-four-natural-t1")
            ?.prices.base,
        5,
    );
    assert.equal(
        economies.get("conquest-lamellar-allflame-three-prefix-t1-product")
            ?.prices.base,
        undefined,
    );
    assert.equal(
        economies.get("conquest-lamellar-allflame-five-natural-t1-product")
            ?.prices.base,
        undefined,
    );
    assert.equal(
        economies.get("vaal-regalia-allflame-warlord-exalt-goal")
            ?.prices.base,
        1,
    );
    assert.deepEqual(
        cases.find(
            (entry) =>
                entry.id === "vaal-regalia-allflame-warlord-exalt-goal",
        )?.compiled_operation_contract,
        {
            type: "influence_exalt",
            required_string_parameters: { influence: "warlord" },
        },
    );
    const forced = economies.get(
        "vaal-regalia-allflame-automatic-eldritch-exalt",
    );
    assert.equal(forced?.prices.base, 1000);
    assert.equal(forced?.prices.eldritch_exalt, 0.001);
    assert.equal(forced?.prices.eldritch_annul, 0.001);
    assert.equal(forced?.sources?.eldritch_exalt, "quote");
    assert.equal(forced?.sources?.eldritch_annul, "quote");
    assert.equal(forced?.manual_overrides?.eldritch_exalt, 0.001);
    assert.equal(forced?.manual_overrides?.eldritch_annul, 0.001);
    assert.match(
        forced?.missing_price_decisions?.base ?? "",
        /owner_fixture_input/,
    );
    assert.match(
        forced?.manual_override_decisions?.eldritch_exalt ?? "",
        /forced_winner/,
    );
    assert.match(
        forced?.manual_override_decisions?.eldritch_annul ?? "",
        /paired_retry_cleanup/,
    );
    assert.equal(
        economies.get("runic-gauntlets-allflame-partial-five")?.prices.base,
        1,
    );

    const imprint = cases.find(
        (entry) => entry.id === "vaal-regalia-allflame-imprint-retry",
    );
    assert.ok(imprint);
    const publicGateFixture = JSON.parse(readFileSync(
        fileURLToPath(new URL(
            "../../../fixtures/bestiary/v1/solver-product-imprint-current.json",
            import.meta.url,
        )),
        "utf8",
    )) as {
        expected: {
            selected_carrier_mod_key: string;
            selected_target_mod_key: string;
        };
    };
    assert.equal(
        imprint.start.mods[0]?.key,
        publicGateFixture.expected.selected_carrier_mod_key,
    );
    assert.deepEqual(
        imprint.goal.slots.map((slot) =>
            "family_mod_key" in slot ? slot.family_mod_key : null),
        [
            publicGateFixture.expected.selected_carrier_mod_key,
            publicGateFixture.expected.selected_target_mod_key,
        ],
    );
    assert.deepEqual(
        imprint.compiled_operation_contracts?.map((contract) => contract.type),
        ["bestiary:imprint", "bestiary:restore_imprint"],
    );
    assert.deepEqual(imprint.material_ratio_contract, {
        numerator_price_key: "beast:rare",
        denominator_price_key: "beast:craicic-croaker",
        expected_ratio: 3,
        require_positive_denominator: true,
        require_complete_pricing: true,
    });
    const imprintEconomy = economies.get(imprint.id);
    assert.equal(imprintEconomy?.prices["beast:craicic-croaker"], 66);
    assert.equal(imprintEconomy?.sources?.["beast:craicic-croaker"], "quote");
    assert.equal(imprintEconomy?.prices["beast:rare"], 1);
    assert.equal(imprintEconomy?.sources?.["beast:rare"], "owner_default");
    assert.equal(imprintEconomy?.manual_overrides?.["beast:rare"], undefined);
    assert.equal(imprintEconomy?.prices.eldritch_chaos, 1000);
    assert.equal(imprintEconomy?.prices.eldritch_annul, 1000);

    const fiveMod = cases.find(
        (entry) => entry.id === "runic-gauntlets-allflame-partial-five",
    );
    assert.ok(fiveMod);
    assert.deepEqual(fiveMod.bounded_best_policy_contract, {
        allow_exact: true,
        bounded_requires_named_stop: true,
        bounded_requires_strict_gap: true,
        bounded_requires_open_obligations: true,
        require_evaluated_upper: true,
        require_cheapest_independently_evaluated_incumbent: true,
    });

    const fiveNaturalT1 = cases.find(
        (entry) =>
            entry.id === "conquest-lamellar-allflame-five-natural-t1-product",
    );
    assert.ok(fiveNaturalT1);
    assert.deepEqual(fiveNaturalT1.bounded_best_policy_contract, {
        allow_exact: true,
        bounded_requires_named_stop: true,
        allowed_bounded_terminations: ["numerical_stability"],
        allowed_bounded_stop_causes: ["numerical_stability"],
        bounded_requires_strict_gap: true,
        bounded_requires_open_obligations: true,
        require_evaluated_upper: true,
        require_cheapest_independently_evaluated_incumbent: true,
    });

    const fourGoalFinalization = cases.find(
        (entry) =>
            entry.id ===
            "conquest-lamellar-allflame-three-prefix-spell-suppression-t1-product",
    );
    assert.ok(fourGoalFinalization);
    assert.equal(fourGoalFinalization.goal.min_satisfied_slots, 4);
    assert.equal(fourGoalFinalization.goal.slots.length, 4);
    assert.deepEqual(fourGoalFinalization.economy.manual_overrides, {});
});

test("goal-realignment materialization rejects undisclosed market overrides", () => {
    const repoRoot = fileURLToPath(new URL("../../../", import.meta.url));
    const corpus = loadSolverBenchmarkCorpus(
        fileURLToPath(GOAL_REALIGNMENT_MANIFESTS[0]),
    );
    const primary = corpus.cases.find(
        (entry) => entry.id === "conquest-lamellar-allflame-four-natural-t1",
    );
    const eldritch = corpus.cases.find(
        (entry) => entry.id ===
            "vaal-regalia-allflame-automatic-eldritch-exalt",
    );
    assert.ok(primary);
    assert.ok(eldritch);

    const undisclosed = structuredClone(primary);
    undisclosed.economy.manual_overrides = {
        ...(undisclosed.economy.manual_overrides ?? {}),
        chaos: 0.001,
    };
    assert.throws(
        () => materializeSolverBenchmarkEconomy(undisclosed, repoRoot),
        /undisclosed market-price override/,
    );

    const mismatchedForcedValue = structuredClone(eldritch);
    mismatchedForcedValue.economy.manual_overrides = {
        ...(mismatchedForcedValue.economy.manual_overrides ?? {}),
        eldritch_exalt: 0.002,
    };
    assert.throws(
        () => materializeSolverBenchmarkEconomy(mismatchedForcedValue, repoRoot),
        /forced-winner overrides do not match/,
    );

    const missingDecision = structuredClone(eldritch);
    missingDecision.economy.manual_override_decisions = {
        eldritch_annul:
            eldritch.economy.manual_override_decisions?.eldritch_annul ?? "",
    };
    assert.throws(
        () => materializeSolverBenchmarkEconomy(missingDecision, repoRoot),
        /manual_override_decisions\.eldritch_exalt/,
    );
});

test("cross-base reliability corpus covers every class and authored start", () => {
    const corpus = loadSolverBenchmarkCorpus(
        fileURLToPath(RELIABILITY_MANIFEST),
    );
    assert.equal(
        corpus.manifest.schema_version,
        "cross_base_reliability_corpus_v1",
    );
    assert.equal(corpus.cases.length, 49);
    const classCases = corpus.cases.filter(
        (entry) =>
            (entry.corpus as { stratum?: string } | undefined)?.stratum ===
            "class-one-goal",
    );
    assert.equal(classCases.length, 27);
    assert.equal(
        new Set(classCases.map((entry) => entry.session.item_class_key))
            .size,
        27,
    );
    assert.ok(
        corpus.cases.every(
            (entry) =>
                entry.verification.exact_evaluation === true &&
                entry.verification.runs >= 100 &&
                entry.expected?.solve_status ===
                    "reliability_classified" &&
                entry.expected.compile_status ===
                    "compiled_if_policy_available" &&
                entry.expected.verification_status ===
                    "run_if_compiled",
        ),
    );
    assert.deepEqual(
        corpus.cases.find((entry) => entry.id === "reliability-start-crafted")
            ?.start.mods[0].flags,
        ["crafted"],
    );
    assert.equal(
        corpus.cases.find(
            (entry) => entry.id === "reliability-start-influenced",
        )?.start.generic_influence_bits,
        1,
    );
    assert.equal(
        corpus.cases.find(
            (entry) => entry.id === "reliability-start-eldritch",
        )?.start.eater_of_worlds_tier,
        2,
    );
    assert.equal(
        (
            corpus.cases.find(
                (entry) => entry.id === "reliability-start-veiled",
            )?.corpus as { tier?: string } | undefined
        )?.tier,
        "special_start",
    );
    const verificationCases = corpus.cases.filter(
        (entry) =>
            (entry.corpus as { tier?: string } | undefined)?.tier ===
            "verification",
    );
    assert.deepEqual(
        verificationCases.map((entry) => entry.id),
        [
            "reliability-selected-gloves-10k",
            "reliability-selected-ring-10k",
        ],
    );
    assert.ok(
        verificationCases.every(
            (entry) => entry.verification.runs === 10_000,
        ),
    );
});
