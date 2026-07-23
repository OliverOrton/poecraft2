# End-To-End Solver Flow

**Status: stable implemented cross-layer flow reference.** This page describes
the current product and runtime sequence. It does not schedule deferred solver
work or define crafting mechanics.

Parent: [Solver](README.md)

Verified against code and complete non-visual cross-layer tests: 2026-07-22 @
mechanical-split source commit `042a281`. Scope: Calculator orchestration,
workspace strategy handoff, `EngineClient`, worker protocol, WASM facade,
solver C ABI, native solve lifecycle, policy compilation, exact graph
evaluation, and sampled verification. No rendered review or mechanic ruling
was performed.

## Purpose

The [Solver reference](README.md) owns the planning abstraction, exact
transition model, optimization, and output contracts. This page owns the
sequence across product and runtime boundaries: which object calls which
surface, what is retained, where cancellation is observed, and when a result
becomes an editable strategy.

```text
Calculator / Strategy Builder
  -> EngineClient request with an opaque worker handle
  -> engine-worker dispatch and cooperative stepping
  -> EngineBindings JSON call into the WASM facade
  -> pcw_* handle registry and JSON translation
  -> pc_* C ABI
  -> native CalcContext / SolveWork / policy compiler / evaluator
  -> JSON or summary back through worker structured clone
  -> product validation, presentation, and workspace handoff
```

The native engine owns action legality, outcome probabilities, abstract state,
cost optimization, and graph execution. TypeScript owns input selection,
economy pinning, request lifetime, progress presentation, and validation of the
returned editable document.

## Shared Inputs And Identities

All solver-related flows begin from these values:

| Input | Owner and role |
| --- | --- |
| Compiled game data | One worker-local data handle opened by the shared engine service |
| Session | Immutable base metadata path and item level; owns dense session-local mod IDs |
| Concrete item | Worker-local item handle containing the Calculator input state |
| Goal | V1 rarity, one to eight stable group/family slots, tier thresholds, and `min_satisfied_slots` |
| Action scope | Full registry for one-action odds or a bounded `goal_relevant` product envelope for Solve |
| Economy | Immutable price snapshot pinned for one solve, evaluation, or simulation |

Integer handles are meaningful only inside the worker and its WASM module.
Persisted items, goals, economies, and strategies use stable string identities;
they do not persist session-local IDs or native handles.

## Calculator Setup And Handle Roles

When a Calculator document opens, `pc-calculator` obtains the shared
`EngineClient` and data handle, opens a session and action context, and imports
or creates its input item. A goal with at least one slot opens an ordinary
solver handle. That handle supplies the action picker and exact one-action odds;
it is separate from the product Solve handle so Solve can use a narrower priced
scope without removing actions from the Calculator.

The Calculator currently owns two solver fields:

| Handle | Current purpose | Lifetime |
| --- | --- | --- |
| `solver` | Full/current-goal registry and one-action exact outcomes | Reopened when the goal or session changes; closed with the Calculator |
| `solveSolver` | Priced `goal_relevant` solve scope and its transition closure/latest result | Reused while the serialized solve goal is unchanged; closed on scope/session change or Calculator disposal |

Goal edits clear result state, close both handles, open a new ordinary solver,
and refresh the available action descriptors. Base or item-level changes also
replace the session, context, item, and solver handles. Draft persistence stores
stable state, not these runtime objects.

Code authority:
`apps/web/src/app/components/pc-calculator.ts`,
`apps/web/src/app/solve-workspace.ts`, and
`apps/web/src/app/workspace/persistence.ts`.

## Exact One-Action Odds

The ordinary Calculator path answers “what can this selected action do to this
exact item?” without mutating the item:

1. Calculator selects a canonical action ID from the current solver registry.
2. `EngineClient.solverCalc` sends `solver`, `item`, and action ID in a worker
   request.
3. The worker calls `EngineBindings.solverCalc`; the WASM facade resolves the
   action ID and invokes `pc_calc_action_outcomes`.
4. Native `CalcContext` projects the concrete item to its abstract state and
   returns the cached or newly constructed exact sparse successor distribution.
5. The result returns supported/legal state, successor probabilities,
   per-goal-slot marginal satisfaction, and combined goal success probability.
6. Calculator presents the native probabilities and performs only labelled
   display arithmetic such as failure probability and action-cost-per-success.

There is no sampled fallback in this path. Bestiary Imprint/restore uses its
dedicated exact compound-state API because the state includes an optional
checkpoint; it returns beside registry action results but does not pass through
`pc_calc_action_outcomes`.

Prices do not change exact one-action probabilities. The product may update
displayed cost arithmetic when the shared economy changes.

Code authority:
`apps/web/src/app/components/pc-calculator.ts`,
`apps/web/src/app/engine-client.ts`,
`apps/web/src/app/engine-worker.ts`,
`apps/web/src/app/engine-wasm.ts`,
`bindings/wasm/wasm_api.cpp`, and
`engine/src/solver_api.cpp`.

## Solve Preparation

Pressing Solve creates a scoped, priced request before native optimization:

1. Calculator pins the effective workspace economy and its provenance for the
   currently known solver action keys and Imprint creation key.
2. Product readiness checks require an input item, at least one goal slot, and
   at least one priced action. A priced Fracture action also requires `base`
   pricing because miss recovery uses Restart.
3. Calculator opens a short-lived `action_mode: "goal_relevant"` envelope
   solver and reads its native action descriptors.
4. TypeScript keeps only descriptors whose complete cost-key vectors resolve
   in the pinned economy. Missing prices exclude an action; they never make it
   free.
5. The envelope solver closes. Calculator builds a second goal containing the
   priced action IDs and computes a stable serialized key for that exact scope.
6. A previous `solveSolver` is reused only when that key matches. Otherwise it
   closes and a new solver opens.
7. The pinned economy is loaded into a short-lived native economy handle for
   this solve invocation.
8. Positive absolute-chaos and relative-percent product targets are converted
   to the optional native gap fields. Disabled inputs omit those fields.

Candidate generation and descriptors remain native. TypeScript filters by
price completeness but does not decide mechanic legality or synthesize planner
operators.

Code authority:
`apps/web/src/app/components/pc-calculator.ts`,
`apps/web/src/app/solve-workspace.ts`, and
`engine/src/solver_registry.cpp`.

## Cooperative Native Solve

`EngineClient.solverSolve` assigns one request ID and passes an `AbortSignal`
plus a progress callback to the worker. The worker drives the stateful native
surface:

```text
pcw_solver_solve_begin
  -> pc_solver_solve_begin
  -> repeated pcw_solver_solve_step / pc_solver_solve_step
  -> pcw_solver_solve_finish / pc_solver_solve_finish
```

The native stages are:

1. project the concrete start item and expand reachable abstract states;
2. admit state-local primitive and automatic operators and build exact sparse
   transition rows within configured work/memory/output caps;
3. price operators against the pinned economy;
4. optimize cyclic components using SCC-based policy iteration with the
   documented fallback; and
5. finalize values, policy, bounds, diagnostics, telemetry, and hashes.

The worker starts from the requested/default work count, adapts each step to a
roughly 12 ms slice, and clamps Solve to one through four native work items.
It rebases to one item at phase changes, emits progress at phase boundaries or
roughly every 100 ms, and yields after bounded accumulated work so incoming
messages can run.

Cancellation is cooperative, not preemptive. `EngineClient` posts a cancel
message with the same request ID. The worker can observe it only after the
current native step returns and the event loop services that message. If a
solve began but does not finish, worker cleanup calls
`pcw_solver_solve_abandon`; native code discards partial work while retaining
bounded abandoned telemetry for diagnosis.

Resource exhaustion, unsupported input, or a native error is surfaced as a
boundary/error. Termination and policy quality are separate: an exact close is
`exact`, a cap can retain `bounded_feasible`, and an enabled post-round gap can
return `bounded_near_optimal`. Compilation is allowed only when
`policy_available`; a non-converged result without an executable proper
fallback is not compiled.

Calculator renders the returned policy's exact evaluated cost separately from
the optimal-cost lower bound and certified upper bound. It also shows absolute
and multiplicative certificates, policy quality, termination/cap detail,
requested and fired targets, economy identity, and the admitted priced action
IDs. A bounded result uses certificate wording (“within 1.10x” / “at most 10%
more expensive”) and never calls its policy or upper bound exact.

Code authority:
`apps/web/src/app/engine-client.ts`,
`apps/web/src/app/engine-worker.ts`,
`apps/web/src/app/engine-wasm.ts`,
`bindings/wasm/wasm_api.cpp`,
`engine/include/poecraft/solver.h`,
`engine/src/solver_api.cpp`, and
the native `engine/src/solver_solve*.cpp` phase family with its private
`solver_solve_types.hpp` declarations.

## Policy To Editable Strategy

After a solve with `policy_available`:

1. `pc_solver_compile_strategy` expands the chosen primitive and automatic
   operators into ordinary V1 start, router, operation, and terminal nodes.
2. The WASM facade returns the compiled JSON string; `EngineBindings` parses it
   into a structured-cloneable object.
3. `prepareSolverStrategy` checks the V1 shape, clones it, assigns missing board
   positions, and runs product strategy validation.
4. Calculator attaches the pinned economy identity and retains an unsaved
   JavaScript strategy document.
5. “Open strategy” clones that document into Strategy Builder as an unsaved
   copy. The editable graph, not the solver's internal operator representation,
   becomes the execution document.

The graph preserves the concrete solve start item and native `expected_cost`
and accounting-role annotations. Compilation fails rather than inventing a
second execution vocabulary when the policy cannot be represented or output
caps are exceeded.

Code authority:
`engine/src/solver_compile.cpp`,
`bindings/wasm/wasm_api.cpp`,
`apps/web/src/app/engine-wasm.ts`,
`apps/web/src/app/solve-workspace.ts`, and
`apps/web/src/app/components/pc-calculator.ts`.

## Current Repricing And Lifetime

Price changes rerender Calculator cost/readiness information; they do not
change mechanic outcomes and do not automatically rerun Solve. A later Solve
invocation pins the then-current economy, creates a new short-lived economy
handle, and may reuse `solveSolver` when its goal/action-scope key is unchanged.
That retained solver can keep price-independent transition data and its latest
result until the goal/session changes or the Calculator closes.

This retention is current implementation, not the approved destination.
[Browser Repricing Uses Rebuild By Default](../decisions.md#2026-07-18--browser-repricing-uses-rebuild-by-default)
records the owner-approved target: after successful strategy transfer, release
the solved handle/transition closure and rebuild on repricing. Delivery remains
deferred in the [solver roadmap](../future/solver-roadmap.md). Stable documents
must label that behavior as decided-but-unimplemented until code inspection
shows otherwise.

## Exact Whole-Graph Evaluation

Strategy Builder's Calculator mode is related to solving but does not reuse the
solver handle or solved policy state:

1. A graph edit schedules a debounced evaluation and aborts the superseded
   request.
2. Product validation rejects malformed graphs before native work.
3. The editor opens a session for the strategy base/item level, pins the
   economy, and calls `EngineClient.strategyEvaluate` with a cloned graph.
4. The worker compiles the graph, optionally loads the economy, opens a
   stateful evaluation, and steps discovery, SCC solving, fallback, and
   finalization with progress and event-loop yields.
5. Worker cleanup always closes evaluation, economy, and compiled-strategy
   handles; the editor closes its session.
6. The product accepts the result only if the request version still matches the
   current graph and exposes convergence, terminal mass, expected work,
   accounting, and graph flow.

Evaluation refuses unsupported action/condition vocabulary rather than using
sampling. Its exact state is `(strategy node, abstract item state)`, distinct
from the solver's action-selection policy state.

Code authority:
`apps/web/src/app/components/pc-strategy-editor.ts`,
`apps/web/src/app/engine-client.ts`,
`apps/web/src/app/engine-worker.ts`,
`engine/src/solver_eval.cpp`, and
`engine/src/solver_api.cpp`.

## Sampled Verification

Calculator's current “Verify 10,000 runs” path is separate sampled evidence:

1. load the solve's pinned economy;
2. compile the returned strategy through the ordinary simulator compiler;
3. create a native simulator;
4. run 10,000 bounded Monte Carlo invocations with progress; and
5. compare mean known cost with the solver's exact `evaluated_policy_cost`
   after the current completion and cost-status checks.

Simulator, strategy, and economy handles close in `finally`. The button uses
the repository's required verification sample count but is not by itself the
compiled-strategy acceptance gate: it does not yet enforce complete
terminal/off-policy truth or a one-sided confidence check; see
[Calculator](../product/calculator.md#current-verification-button). Exact
evaluation, solver value, and sampled simulation must remain labelled as three
different evidence sources.

## Failure And Ownership Checklist

| Boundary | Required behavior |
| --- | --- |
| Unknown/unpriced action | Exclude or diagnose; never assign zero cost silently |
| Unsupported exact vocabulary | Refuse exact calculation/evaluation; never sample silently |
| Cap or incomplete solve | Report termination and bounds; compile only an independently certified executable incumbent |
| Stale product request | Ignore its result using request/version checks |
| Cancelled stepped work | Yield, observe cancellation, abandon/destroy native work, and return bounded progress |
| Document/session change | Close solver handles before replacing their owning session |
| Strategy handoff | Validate and clone ordinary V1 strategy JSON; do not expose opaque solver operators |
| Mechanic ambiguity | Stop for Oliver's ruling; this flow has no mechanic authority |

## Code And Evidence Map

| Layer | Primary files |
| --- | --- |
| Calculator orchestration | `apps/web/src/app/components/pc-calculator.ts`, `solve-workspace.ts` |
| Strategy evaluation/simulation | `apps/web/src/app/components/pc-strategy-editor.ts` |
| Main-thread RPC | `apps/web/src/app/engine-client.ts`, `engine-protocol.ts` |
| Worker stepping | `apps/web/src/app/engine-worker.ts` |
| WASM calls | `apps/web/src/app/engine-wasm.ts`, `bindings/wasm/wasm_api.cpp` |
| Public native contract | `engine/include/poecraft/solver.h` |
| Native API/lifetime | `engine/src/solver_api.cpp` |
| Solve shared types and entry | `engine/src/solver_solve_types.hpp`, `solver_solve.cpp` |
| Expansion and Bellman stepping | `engine/src/solver_solve_expand.cpp`, `solver_solve_bellman.cpp` |
| Focused, constructive, and heuristic phases | `engine/src/solver_solve_focused.cpp`, `solver_solve_constructive.cpp`, `solver_solve_heuristics.cpp` |
| Quotient, finish, and telemetry phases | `engine/src/solver_solve_quotient.cpp`, `solver_solve_finish.cpp`, `solver_solve_telemetry.cpp` |
| Policy compilation | `engine/src/solver_compile.cpp` |
| Exact graph evaluation | `engine/src/solver_eval.cpp` |
| Focused web checks | `apps/web/test/solve-workspace.test.ts`, `strategy-calculator-mode.test.ts`, `engine-smoke.test.ts` |
| Native checks | `engine/tests/test_solver_api.cpp`, `test_solver_solve.cpp`, `test_solver_compile.cpp`, `test_solver_eval.cpp` |

When these paths change, use the repository-wide
[change-impact map](../foundation/change-impact.md) to identify downstream
bindings, generated WASM, documentation, and final verification obligations.
