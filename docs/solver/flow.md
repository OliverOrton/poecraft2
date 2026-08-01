# End-To-End Solver Flow

**Status: stable implemented cross-layer flow reference.** This page describes
the current product and runtime sequence. It does not schedule deferred solver
work or define crafting mechanics.

Parent: [Solver](README.md)

Verified against code and complete non-visual cross-layer tests: 2026-07-28 @
gated root renewal incumbent closure. Scope: Calculator orchestration,
workspace strategy handoff, `EngineClient`, worker protocol, WASM facade,
solver C ABI, native solve lifecycle, policy compilation, exact graph
evaluation, and sampled verification. No rendered review or mechanic ruling
was performed.

Qualification addendum: source inspection and focused native tests on
2026-07-31 verified the shared publication steps through observation
propagation, but the required natural two-goal case stopped before its first
closed-partition class at the unchanged 1 GiB cap. Release WASM and full
cross-layer acceptance were not rerun.

The policy-guided refinement steps below define the retained native
publication authority. Its reconstruct-then-merge production path stopped at
the archived
[two-goal 1 GiB qualification gate](../archive/2026-07-31-policy-guided-exact-refinement/README.md),
so this page does not claim broad availability or changed refusal counts. The
separate
[active structural boundary](../active/proof-carrying-quotient-refinement.md)
will integrate the same authority during solving; implementation has not
begun.

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
  -> JSON summaries or transferable strategy bytes back through the worker
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

The Calculator owns one persistent solver field and opens a separate scoped
solver inside each Solve invocation:

| Handle | Current purpose | Lifetime |
| --- | --- | --- |
| `solver` | Full/current-goal registry and one-action exact outcomes | Reopened when the goal or session changes; closed with the Calculator |
| Scoped Solve solver | Priced `goal_relevant` solve scope, transition closure, and latest result | Opened fresh for one Solve; closed after summary, telemetry, and compiled strategy transfer or any terminal failure |

Goal edits clear result state, replace the ordinary solver, and refresh the
available action descriptors. Base or item-level changes also replace the
session, context, item, and ordinary solver handles. Draft persistence stores
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
   priced action IDs.
6. Calculator opens a fresh scoped solver for that priced goal. It is not
   retained for later repricing or Solve invocations.
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

1. derive, canonicalize, and validate the one engine-owned
   observation/preservation/destruction contract for every candidate action;
   reject incomplete descriptors before admission, then project the concrete
   start item and expand reachable abstract states;
2. admit state-local primitive and automatic operators and build exact sparse
   transition rows within configured work/memory/output caps;
3. price operators against the pinned economy;
4. optimize cyclic components using SCC-based policy iteration with the
   documented fallback;
5. lift any coarse selected-policy region through native policy-guided exact
   refinement, locally re-optimize counterexample classes when required, and
   prove selected-kernel lumpability; and
6. finalize values, executable policy, bounds, diagnostics, telemetry, and
   hashes.

Callers may opt into `goal_progress_gated_reforges`. In that scope, primitive
reforge rows fold goal outcomes to one terminal exit and zero-goal-progress
outcomes to a virtual retry basin while retaining every partial-progress item
exactly. Basin expansion permits only legal destructive reforges independent
of the discarded affixes; ordinary partial states retain the normal complete
action envelope. A completed result is labelled exact only within that
zero-progress-reroll restriction. Omitting the option preserves the
unrestricted globally optimal solve contract.

When a completed gated root destructive-reforge row has positive terminal
mass and every non-goal exit proves the same legal exact action-local kernel,
Solve immediately records the executable fixed policy “repeat that reforge
until goal” at value `cost / terminal_probability`. It retains the entire
competing action envelope and continues discovery, so a cap-stopped result is
bounded within the gated restriction rather than exact. The compiler
independently revalidates the witness and emits a compact goal-or-repeat loop;
it does not enumerate one strategy node per retained partial state.

Goal-progress-gated solves also use a Chaos-anchored incremental action
envelope. A completed Chaos row releases and queues its exact partial
successors before filtered Fossil, corrected Harvest reforge, and
goal-relevant Essence alternatives finish. Bellman may optimize the admitted
subset, then exact delayed Q rows are admitted and reoptimized, proved
non-improving, or retained as unresolved. Compatible outcomes use ordinary
Chaos-created state IDs rather than a stored cross-action structural DAG.
Support-delta states are queued and expanded before the action can be
classified. The envelope is applied at every reached compatible carrier, and
an open or resource-limited envelope blocks an exact result.

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

Policy-guided refinement stays entirely inside the native solve. It starts
from the concrete item, visits only policy-reachable strict refinements, and
derives routing features from the admitted action contracts. Distinguishing
predicates are local to the exact policy classes that select different
decisions; they are not one graph-wide feature union. Cyclic observation
propagation and successor-class partitioning run to a deterministic fixed
point. States merge only when selected action, immediate cost, and exact
probability into every successor class agree. A mismatch is a native
counterexample for witness-local Bellman re-optimization; it is not a signal
for TypeScript to select a recipe or fallback.

Observed-choice fixed programs retain the exact pre-choice carrier on each
choice group. Preference lookup, refinement, and compiled routing require that
observation identity to match; an equal offered modifier or projected
successor from another observation carrier cannot satisfy the branch.

When an action destroys every source feature needed by downstream routing, the
native refinement collapses that path back to the coarse parent. Preserving
actions retain only their declared side/lock/fracture scope, and modifier IDs
with equal exclusion-effect signatures remain merged. Named refinement caps
and bounded counterexample/refusal telemetry cross the ABI as diagnostics.
The publication ledger distinguishes cumulative strict materialization/kernel
work from final retained states and classes, records contract-driven collapse
separately from state/cache reuse, and exposes the fixed-point, lumpability,
class-policy properness, and compiled exact-cost assertions. Its memory fields
distinguish live phase estimates, peak, declared limit, and the retained exact
strategy payload. Feature masks and count arrays use native
`RefinementFeature` bit/declaration order. Separate counters expose locally
scheduled/evaluated state-action rows and accepted policy/value changes. The
frontend gains no observation, preservation, exclusion, or lumpability logic.

## Policy To Editable Strategy

After a solve with `policy_available`:

1. `pc_solver_compile_strategy` expands the exact refined primitive and
   automatic policy classes into ordinary V1 start, router, operation, and
   terminal nodes. Router predicates come from the shared native observation
   vocabulary, not action-name compiler cases. Fixed-program observed-choice
   routers remain scoped to the exact observation carrier that produced the
   offer.
2. The WASM facade exposes the compiled JSON response as raw bytes plus native
   result status and length. `EngineBindings` slices those bytes from linear
   memory, clears the reusable native response string, and transfers the
   resulting `ArrayBuffer` from worker to main thread.
3. `EngineClient` decodes and parses the document once.
   `prepareSolverStrategy` adopts that uniquely transferred object, checks its
   V1 shape, assigns missing board positions, and runs product validation.
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
handle, and opens a fresh scoped solver. After Calculator has obtained the
summary, telemetry, and compiled strategy bytes, terminal cleanup closes the
scoped solver, envelope solver, and economy handle. The transition closure and
latest native result therefore do not remain live merely to support a possible
reprice.

[Browser Repricing Uses Rebuild By Default](../decisions.md#2026-07-18--browser-repricing-uses-rebuild-by-default)
records this implemented owner choice. A retained-cache product mode remains
deferred until it has an enforced live-memory budget.

## Exact Whole-Graph Evaluation

Strategy Builder's Calculator mode is related to solving but does not reuse the
solver handle or solved policy state:

1. A graph edit schedules a debounced evaluation and aborts the superseded
   request.
2. Product validation rejects malformed graphs before native work.
3. The editor opens a session for the strategy base/item level, pins the
   economy, and calls `EngineClient.strategyEvaluate`. The client encodes the
   graph once and transfers its byte buffer to the worker.
4. The worker compiles those bytes, optionally loads the economy, opens a
   stateful evaluation, and steps discovery, SCC solving, fallback, and
   finalization with progress and event-loop yields.
5. Worker cleanup always closes evaluation, economy, and compiled-strategy
   handles; the editor closes its session.
6. The product accepts the result only if the request version still matches the
   current graph and exposes convergence, terminal mass, expected work,
   accounting, and graph flow.

Evaluation refuses unsupported action/condition vocabulary rather than using
sampling. Its exact state is `(strategy node, abstract item state)`, distinct
from the solver's action-selection policy state. The evaluator derives which
source features can cross an operation from the same admitted action contract
used by solving and compilation; it does not maintain an independent list of
full versus side-preserving rerolls.

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
| Coarse selected-policy incompatibility | Feed the native observation witness into exact refinement/local re-optimization; withhold publication only for a named refinement cap or separately named unsupported exact vocabulary. Incomplete action contracts fail admission before search |
| Cap or incomplete solve | Report termination and bounds; compile only an independently certified executable incumbent |
| Stale product request | Ignore its result using request/version checks |
| Cancelled stepped work | Yield, observe cancellation, abandon/destroy native work, and return bounded progress |
| Document/session change | Close solver handles before replacing their owning session |
| Strategy handoff | Transfer bytes, parse once, and validate/adopt the uniquely owned ordinary V1 graph; clone only when creating a separate document owner |
| Mechanic ambiguity | Stop for Oliver's ruling; this flow has no mechanic authority |

## Code And Evidence Map

| Layer | Primary files |
| --- | --- |
| Calculator orchestration | `apps/web/src/app/components/pc-calculator.ts`, `solve-workspace.ts` |
| Strategy evaluation/simulation | `apps/web/src/app/components/pc-strategy-editor.ts` |
| Main-thread RPC | `apps/web/src/app/engine-client.ts`, `engine-protocol.ts` |
| Worker stepping | `apps/web/src/app/engine-worker.ts` |
| WASM calls | `apps/web/src/app/engine-wasm.ts`, `bindings/wasm/wasm_api.cpp` |
| Transfer ownership tests | `apps/web/test/engine-client-transfer.test.ts`, `engine-smoke.test.ts` |
| Public native contract | `engine/include/poecraft/solver.h` |
| Native API/lifetime | `engine/src/solver_api.cpp` |
| Solve shared types and entry | `engine/src/solver_solve_types.hpp`, `solver_solve.cpp` |
| Action observation/refinement contract | `engine/src/solver_internal.hpp`, `solver_registry.cpp`, `solver_refinement.cpp` |
| Expansion and Bellman stepping | `engine/src/solver_solve_expand.cpp`, `solver_sparse_policy.cpp`, `solver_solve_bellman.cpp` |
| Focused, constructive, and heuristic phases | `engine/src/solver_solve_focused.cpp`, `solver_solve_constructive.cpp`, `solver_solve_heuristics.cpp` |
| Quotient, finish, and telemetry phases | `engine/src/solver_solve_quotient.cpp`, `solver_solve_finish.cpp`, `solver_solve_telemetry.cpp` |
| Policy compilation | `engine/src/solver_compile.cpp` |
| Exact graph evaluation | `engine/src/solver_eval.cpp` |
| Focused web checks | `apps/web/test/solve-workspace.test.ts`, `strategy-calculator-mode.test.ts`, `engine-smoke.test.ts` |
| Native checks | `engine/tests/test_solver_api.cpp`, `test_solver_solve.cpp`, `test_solver_compile.cpp`, `test_solver_eval.cpp` |

When these paths change, use the repository-wide
[change-impact map](../foundation/change-impact.md) to identify downstream
bindings, generated WASM, documentation, and final verification obligations.
