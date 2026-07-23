# Mechanical Solver Split

**Status: completed on 2026-07-22. Historical archive; it does not own current
sequencing.**

Parent: [Documentation archive](../README.md)

This archive preserves the completed
[mechanical solver split plan](plan.md). Source commit `042a281` split the
solve implementation into exactly nine `solver_solve*.cpp` translation units
plus the private `solver_solve_types.hpp` header. The move passed exact
body/comment-token, native count, transition/policy bit-hash, one-sided
performance, rebuilt WASM, existing web-test, and TypeScript gates without
changing mechanics or adding tests.

Stable ownership now lives in [Solver](../../solver/README.md), the
[end-to-end solver flow](../../solver/flow.md), and [Engine](../../engine/README.md).
The root [HANDOFF](../../../HANDOFF.md) records the commands, hashes, per-run
measurements, watchdog evidence, acceptance boundary, and exact stopping point.

The exact oracle, exact evaluator, simulator verification, Calculator,
`solver_internal.hpp`, and engine-smoke test split were outside this chunk.
Oliver must select any later profiling, product, or structural work.
