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

test("S7 solver benchmark corpus is versioned, unique, and explicitly gated", () => {
    const corpus = loadSolverBenchmarkCorpus(fileURLToPath(MANIFEST));
    assert.equal(corpus.manifest.schema_version, "solver_benchmark_corpus_v1");
    assert.ok(corpus.cases.length >= 5);
    assert.equal(new Set(corpus.cases.map((entry) => entry.id)).size, corpus.cases.length);
    for (const entry of corpus.cases) {
        assert.equal(typeof entry.benchmark_enabled, "boolean");
        assert.ok(entry.expected.solve_status.length > 0);
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
    validateCorpusArtifactPins(corpus.manifest, artifact, 1);
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
