# Anytime Benchmark Completion

**Status: completed on 2026-07-26.** This folder is historical; it does not
select current work.

Parent: [Documentation archive](../README.md)

This milestone made incomplete bounded-solver runs durable and analyzable.
The native benchmark writes atomic step-boundary snapshots, the isolated
runner preserves valid snapshots through watchdog cleanup without relabelling
them completed, and the reporter distinguishes administrative censoring,
completed resource-cap measurements, and explicit failures.

It also froze future normalized-gap and horizon semantics without choosing a
primary metric, strengthened experiment identity with the natural-T1
generator-config hash, and assigned whole corpus strata to development,
validation, and frozen-test roles. Pure trajectory analytics remain deferred
until a second real candidate exists. Accumulated-gap racing was rejected.

Contents:

- [Plan](plan.md) — the reduced Gates 0, 1, 2, and 5 contract.
- [Report](report.md) — implementation, fresh baseline, acceptance, and
  limitations.

Durable behavior is extracted into
[Solver Benchmark Trajectories](../../solver/benchmarking.md),
[Decisions](../../decisions.md), [Evidence](../../evidence.md), and the
[natural-T1 corpus guide](../../../fixtures/solver-natural-t1/v1/README.md).
