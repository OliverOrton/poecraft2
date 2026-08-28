# Resources, Resume, And Replay

Parent: [Solver](README.md) | Verified against current source: 2026-08-27 @
`952524b`.

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

Native development now supports cross-process replay of that completed coarse
closure. `solver_development_checkpoint.cpp` serializes the calculator's
ordered abstract states, generated planner operators, candidate and dependency
ordering, state-local automatic admission, action-envelope evidence, and every
behavior-bearing `SolveTransitionCache` arena. A load reconstructs those
namespaces and must enter the ordinary compatible-cache path; it may not fall
back to rebuilding the graph.

The benchmark harness exposes the intended interface for one selected case:

```text
poecraft_solver_benchmark ... --case CASE \
  --save-development-checkpoint PATH
poecraft_solver_benchmark ... --case CASE \
  --load-development-checkpoint PATH
```

Its caller identity binds the ABI/compiler, compiled artifact manifest,
canonical case JSON, resolved economy, and CLI graph overrides. The binary
also carries format/layout guards, payload length, and checksum. Save refuses
an incomplete or focused graph, an active calculator row/admission cursor, or
a proof-carrying quotient graph. Load requires a fresh solver and exact
identity/options/operator compatibility.

Replay skips coarse state/transition construction only. Bellman optimization,
strict refinement and repair, compilation, and evaluation run normally. The
checkpoint is disposable native-development state: it is not exported by
release WASM and has no correctness, exactness, publication, or evidence
authority.

A strict-partition checkpoint is still unimplemented. It would additionally
need the persistent oracle/session, selected closure, partition generations,
Bellman proof store, obligations and dependencies, row kernels, resumable
cursors, and verified incumbent. Request/result JSON is not a substitute for
either checkpoint.
