import assert from "node:assert/strict";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";

import {
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
