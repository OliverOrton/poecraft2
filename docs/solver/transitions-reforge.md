# Transitions And Reforge Work

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Inputs And Outputs

For one interned state and one admitted primitive or planner operator,
`CalcContext` returns an exact probability distribution over interned
successors plus any observed-choice sidecar. Prices are not part of a
transition row. Sparse solve rows later attach every equivalent priced
operator variant to the shared kernel.

Primary owners: `solver_calc.cpp`, `solver_reforge.cpp`,
`solver_options.cpp`, `solver_solve_expand.cpp`, and
`solver_calc_types.hpp`.

## Exact Reforge Recurrence

The current V3 destructive-reforge evaluator uses a factored terminal
recurrence and deterministic canonical accumulation. Broad rows are explicit
cooperative work objects: predecessor indexing, expansion, canonicalization,
accumulation, and publication can suspend at deterministic checkpoints.

A suspended cursor owns scratch only. It cannot populate the ordinary cache,
seed Bellman values, satisfy an action obligation, or certify a proof row.
Cancellation discards it. Completion transactionally installs the immutable
distribution. The synchronous `outcomes()` API remains a completion wrapper
for callers that do not schedule cooperatively.

## Work And Resources

`max_reforge_work` is the stable V1-equivalent logical envelope, not a vague
wall-time estimate and not the physical V3 operation count. Telemetry exposes
V1 logical, V1 raw-equivalent, V2 projected, V3 factored, active build time,
row family/owner, resumes, suspensions, cancellations, maximum slice wall, and
maximum retained cursor bytes separately.

The recurrence preserves transition ordering and floating-point accumulation
across uninterrupted and resumed execution. Native builds use
`-ffp-contract=off`; tests require bit-identical distributions and work hashes.

## Failure

Unsupported and inapplicable are distinct. A resource-interrupted row remains
absent. Probability, canonical-state, cap, or retained-byte violations fail
the row before publication. Exact reforge caches may be reused only through
their complete collision-checked observation identity.
