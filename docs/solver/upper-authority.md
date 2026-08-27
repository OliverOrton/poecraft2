# Executable Upper Authority

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## What An Upper Means

An upper bound is the exact expected cost of one executable proper strategy
inside the stated request scope. A heuristic estimate, incomplete sparse
policy, optimistic renewal calculation, partial transition graph, or sampled
mean is not an upper.

Primary owners: `solver_solve_constructive.cpp`,
`solver_executable_carrier_planner.*`, `solver_solve_finish.cpp`,
`solver_policy_assertion.cpp`, and the incumbent portfolio contracts in
`solver_solve_types.hpp`.

## Candidate Sources

Candidates may come from a complete coarse policy, constructive/renewal
witness, focused or incremental carrier plan, retained strict incumbent, or
strict policy improvement. `IncumbentPortfolio` keeps estimates separate from
verified executable candidates and retains the cheapest compatible authority.

Every published candidate binds goal, economy, action vocabulary, caller
scope, artifact, graph prefix, and source/target generations. A changed
identity invalidates reuse. Cost reconciliation uses the engine's named
absolute-or-relative tolerance contract.

## Recovery And Exact Terminal Success

Product defaults do not voluntarily abandon a live item for a fresh base.
Restart remains available where an engine-owned recovery program requires it,
such as a failed Fracture branch. Cleanup is successor driven: junk need not
be removed before an action that legally replaces it, but exact terminal
success still permits no junk.

## Failure And Telemetry

Compilation, missing prices, improperness, off-policy probability, incomplete
cost accounting, graph/evaluator cap, or cost mismatch rejects promotion.
Inspect `incumbent_portfolio`, upper milestones, candidate kind/stage,
publication stage, exact evaluation, and retained rollback-upper telemetry.
