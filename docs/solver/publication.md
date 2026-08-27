# Publication, Compilation, And Evaluation

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Pipeline

Publication classifies bounds and termination, chooses only a compatible
verified incumbent, compiles its native policy to ordinary strategy JSON,
parses that JSON through strategy authority, and exact-evaluates the resulting
operation graph. The returned strategy is the evaluated graph, not a separate
recompilation of an older coarse policy.

Primary owners: `solver_solve_finish.cpp`, `solver_compile.cpp`,
`solver_compile_conditions.hpp`, `solver_compile_serialization.hpp`,
`solver_policy_assertion.cpp`, `solver_eval.cpp`,
`solver_eval_resolve.cpp`, and `solver_eval_report.cpp`.

## Compiler Contract

Policy routers express observable distinctions between exact carriers whose
selected continuation differs. Operation nodes execute native actions;
infrastructure nodes own start, success, off-policy failure, and any scoped
Restart default. Equivalent route signatures and identical operation regions
may be shared, but compiler compaction cannot merge semantically distinct
conditions or change the certified default.

## Evaluation Contract

Exact graph evaluation constructs the reachable `(operation node, item,
choice/checkpoint)` product, proves properness, solves success probability and
expected resource equations, and reports off-policy mass and cost
completeness. Simulator execution is sampled evidence over the same strategy
vocabulary and does not replace exact evaluation.

## Failure And Telemetry

Node/edge/JSON/pair/memory caps, invalid operations, missing prices,
improperness, nonzero off-policy mass, incomplete accounting, or cost mismatch
reject exact promotion. Inspect compiler nodes/edges/condition density,
evaluation phases, pair counts and memory, success/off-policy probability,
exact cost, and reconciliation deltas.

