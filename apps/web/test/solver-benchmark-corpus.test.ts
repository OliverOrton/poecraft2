import assert from "node:assert/strict";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";

import {
    loadSolverBenchmarkCorpus,
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
