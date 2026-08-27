# Strict Closure

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Purpose And Inputs

Strict closure checks whether the selected broad policy and every competitive
alternative are sound over exact carriers. Inputs are the coarse result,
exact start, prices, action scope, refinement limits, and any independently
verified rollback upper.

Primary owners: `solver_policy_refinement.cpp`,
`solver_policy_oracle_*.inc`, `solver_refinement_*.cpp`,
`solver_quotient_partition.cpp`, `solver_quotient_bellman.cpp`, and
`solver_quotient_proof.cpp`.

## Persistent Session

One `PersistentQuotientSession` owns the strict calculator, selected closure,
stable split-only partition, Bellman graph, proof store, published rows,
alternative obligations, reverse dependencies, and verified incumbent. New
frontier carriers grow this session in place; generation stamps invalidate
only affected sources and targets.

Selected rows are materialized first. Every other admitted semantic action in
a reachable quotient cell becomes either a carrier-wide certified row or an
explicit lower-only obligation. An immediate-price lower is sound but usually
weak. A partially evaluated alternative cannot publish.

When an alternative row exposes a successor outside the closed partition, the
scheduler returns that frontier immediately to grow/repartition before
replaying unrelated old-generation obligations. This avoids starving the
existing grow-in-place boundary behind expensive broad rows.

## Closure Conditions

Exact closure requires a proper selected policy, complete action accounting,
every competitive alternative certified or carrier-wide dominated at the
current Q generation, closed frontier and action envelopes, and a compiled
exact-evaluation assertion that reconciles with the final strict Q value.

## Failure And Telemetry

Strict work may retain a bounded executable upper after a proof cap. Inspect
strict states/cells/kernels/transitions, frontier insertions, source/target
splits, obligation lifecycle, selected versus alternative work, row
attribution, proof-memory categories, policy improvements, and exact-envelope
closure. A large partially-evaluated count with no frontier insertion signals
a partition/action-uniformity boundary rather than successful proof work.
