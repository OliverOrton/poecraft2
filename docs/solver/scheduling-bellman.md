# Scheduling And Bellman Search

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Search Flow

`SolveWork` expands reachable abstract states, appends price-independent sparse
rows, prices every retained variant, and advances deterministic Bellman and
policy-evaluation work. Focused mode alternates lower/upper work and freezes
carrier epochs; incremental mode grows delayed action families and generated
programs while maintaining an explicit action-envelope ledger.

Primary owners: `solver_solve.cpp`, `solver_solve_expand.cpp`,
`solver_solve_incremental.cpp`, `solver_solve_focused.cpp`,
`solver_solve_priority.cpp`, `solver_solve_bellman.cpp`, and
`solver_sparse_policy.cpp`.

## Ordering Versus Proof

Carrier scheduling round-robins goal-subset buckets to retain diversity.
Within those buckets it may prefer more progress, useful protection/fracture,
free side capacity, fewer blocked missing slots, or less unrelated occupancy.
These scores are scheduling-only and cannot be converted to a lower bound.

Sparse policy row selection uses strict finite objective order. The separate
policy-stability check is tolerance guarded so roundoff does not oscillate
near-equal actions forever. Exact ties retain stable deterministic row order.

## Bellman Contract

Goal states have value zero. Every nonterminal row has immediate priced cost
plus expected successor value, with observed-choice groups selecting their
best successor. A policy must be proper: positive-probability recurrent classes
without a route to goal are not executable uppers. Restart is an ordinary
scoped recovery action, not a blanket proof of properness.

## Failure And Telemetry

An open graph yields frontier bounds, not fabricated terminal arcs. The solve
may stop for a requested bounded finish, gap target, named cap, numerical
failure, absent executable policy, or exact closure. Inspect focused rounds,
lane work, state/row/transition counts, Bellman sweeps and residuals,
policy-reachable counts, and action-family search cost.
