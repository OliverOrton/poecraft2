# Resources, Resume, And Replay

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Resource Dimensions

States, expanded states, sparse rows, transitions, logical reforge work,
strict states/kernels/transitions, refinement rounds/classes, compiler graph,
evaluator pairs, strategy bytes, and solver-owned memory are separate limits.
Wall time and cooperative slice latency are measured independently. Raising
one limit may merely reveal the next; telemetry must name the first owner.

Primary owners: `solver_solve_contracts.hpp`, `solver_calc_types.hpp`,
`solver_solve_telemetry.cpp`, `solver_policy_refinement.cpp`,
`solver_eval.cpp`, and the public option parsing in `solver_api.cpp`.

## Cooperative Work

`SolveWork`, automatic admission, exact reforge rows, strict refinement,
compilation assertion, and exact evaluation retain explicit progress. A public
step advances bounded logical work and may suspend with owned-byte telemetry.
Cancellation discards unpublished work and preserves the last verified
incumbent when the owning contract permits bounded publication.

## Development Replay Boundary

The existing price-independent `SolveTransitionCache` reuses a completed
reachable closure inside one compatible `CalcContext`. Compatibility binds the
start state, operator order, graph-affecting limits/options, and action scope;
prices are intentionally excluded and rows are repriced on reuse.

Cross-process disk replay is not yet a truthful stable contract. A real coarse
checkpoint must serialize both the calculator's ordered state/operator and
state-local admission authority and the sparse transition cache. A strict
checkpoint must additionally serialize the persistent oracle/session,
selected closure, partition generations, Bellman proof store, obligations,
dependencies, row kernels, resumable cursors, and verified incumbent. Saving
only the request or final JSON would not avoid graph construction and must not
be called replay.

Until that representation is implemented, benchmark artifacts are evidence,
not resumable engine state. Any future format must version and validate source,
compiler/FP contract, canonical and compiled artifact, goal/start carrier,
prices, action vocabulary, options/caps, layouts, stable identities, payload
checksums, and completeness before mutation.
