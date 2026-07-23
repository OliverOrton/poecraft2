# Focused-Round Performance Attribution And Scheduling

**Status: completed on 2026-07-23. Historical archive; it does not own current
sequencing.**

Parent: [Documentation archive](../README.md)

This archive preserves the completed
[focused-round performance plan](plan.md). Source commit `f28bbb8` adds
diagnostic-only focused schedule, fallback-validation, owned-byte-ledger, and
solve-step telemetry plus its native/report tests. It changes no mechanic,
certificate, public cap, natural-T1 generation, economy data, or focused
scheduling default.

Fresh instrumentation replay preserved counts, status, caps, certificate
bounds, and deterministic transition/policy hashes. The predeclared
run-local matrix proved that increasing the 256-state global batch reduces
rounds, repeated policy evaluations, and total solve time. No tuple qualified:
all seven variants retained an approximately 11.5-second maximum solve step,
and the larger batches made that existing spike the p95, violating the fixed
responsiveness gates. The accepted result is therefore instrumentation plus an
explicit no-default-change decision.

The tracked
[acceptance summary](../../../fixtures/solver-scaling/v1/evidence/focused-round-performance-summary.json)
contains the baselines, complete matrix, hashes, causal decisions, watchdog
facts, and final native/WASM/web acceptance. Stable conclusions live in
[Solver](../../solver/README.md), [Evidence](../../evidence.md), and the
[solver roadmap](../../future/solver-roadmap.md). The root
[HANDOFF](../../../HANDOFF.md) records the stopping point.
