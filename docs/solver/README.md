# Solver

**Status: stable implemented architecture reference.** Historical phase plans
and acceptance narratives are archived and do not control current sequencing.

Parent: [Documentation index](../README.md)

Verified against code: 2026-07-19 @ d5e38e3. Scope: native solver,
calculation/evaluation engines, public C ABI, policy compilation, and the web
worker call path. This is a source audit, not a new performance run or mechanic
ruling.

## Purpose

The solver turns a concrete start item, a goal, an action scope, and an
immutable economy into a minimum-expected-cost policy. Its transition and
legality authority stays in the native engine. The web app selects inputs,
shows diagnostics, and transfers compiled strategy JSON; it does not implement
crafting probabilities or Bellman logic.

The same subsystem provides four related services:

- exact outcomes for one action on one concrete item (Calculator);
- minimum-expected-cost policy solving;
- compilation of a solved policy to an ordinary editable strategy graph; and
- exact whole-graph evaluation and action/material accounting for a compiled
  strategy.

The [product Calculator reference](../product/calculator.md) describes the
current user-facing orchestration. Mechanic behavior belongs to the
[mechanics library](../mechanics/README.md).

## Goal And State Contract

A v1 goal contains one to eight slots. Each slot names either a stable modifier
group or modifier-family key and a minimum tier (`0` means any tier). The goal
also names the finished rarity and may set `min_satisfied_slots`; omission
means every slot. Goal parsing rejects unknown, overlapping, empty, or
out-of-range definitions.

The abstract layout is derived from the resolved goal and the candidate action
set. An abstract state records:

- absent, below-tier, or satisfied status for every goal slot;
- blocked, crafted-goal, and fractured-goal masks;
- rarity and prefix/suffix counts;
- compact junk counts, including crafted/fractured combinations;
- corruption, mirror, split, synthesis, metamod, veiled, influence, and
  Eldritch state needed by admitted actions.

Non-goal affixes share a junk class only when the admitted actions cannot
distinguish them. Classes include side, relevant tag signature, goal-blocking
effects, and—when exact group effects are required—the complete exclusion
effect on later pools. `CalcContext` forces the strict exclusion-effect form
when Unveil, Harvest resistance conversion, Fracture, or remove-crafted-mods
participates. Because automatic product mode admits Fracture, the current
product solve does not match the old claim that strict partitioning is
evaluator-only. The strict-versus-compact product trade remains deferred to
the [solver roadmap](../future/solver-roadmap.md).

Code authority:
`engine/src/solver_internal.hpp`, `engine/src/solver_api.cpp`,
`engine/src/solver_abstract.cpp`, and `engine/src/solver_calc.cpp`.

## Actions And Planner Operators

`build_action_registry` enumerates plannable engine actions for one session.
Descriptors carry a stable id, display name, transition kind, legality facts,
price-key quantity vector, tag discriminators, and preservation effects. The
synthetic `restart` action consumes the `base` price and returns to the clean
base state.

Callers may name an explicit primitive subset and fixed option programs. The
product `action_mode: "goal_relevant"` instead builds a bounded,
price-independent envelope and enables state-local automatic candidates. At
d5e38e3 those candidates cover relevant primitive Fracture, permanent bench
finishes, temporary bench blockers, protected metamod routes, Multimod
finishes, and automatic Imprint attempt/restore programs. Imprint programs are
not user-authored in product mode.

Fixed and automatic options are solver operators over exact primitive
programs. They carry complete exit distributions, resource quantities, choice
groups, and compilation recipes. They never become opaque simulator actions:
a selected option compiles back into ordinary strategy operations and routers.

The goal-relevant product envelope intentionally excludes Veiled and Eldritch
families at this commit even though their primitive actions and exact
evaluators exist for explicit/manual scopes. The registry makes that a bounded
product-scope choice rather than an absence of primitive engine support. It is
recorded as deferred scope work in the
[solver roadmap](../future/solver-roadmap.md).

Code authority:
`engine/src/solver_registry.cpp`, `engine/src/solver_options.cpp`, and
`engine/src/solver_internal.hpp`.

## Exact Transition Provider

`CalcContext` owns the abstract state table, representative materialization,
planner operators, and price-independent transition caches. Runtime solver and
Calculator outcomes are exact engine evaluations:

- deterministic operations emit one successor;
- single-slot operations enumerate the engine-owned weighted pool;
- reforge operations use a sequential abstract roll frontier with exact group
  removal, target-count mixing, and mechanic-specific stages; and
- special evaluators handle implemented Harvest, Veiled, Eldritch, Fracture,
  and other registered one-item mechanics.

Compound Bestiary actions use their own exact calculation API because their
state includes an optional saved checkpoint. Automatic Imprint retry is still
an exact solver operator assembled from those native Bestiary transitions.

There is no Monte Carlo fallback in the calculation engine. Sampling is used
as test or simulator evidence, not to produce `pc_calc_action_outcomes` or
solver rows. There is also no public `pc_calc_batch_outcomes` function; the
implemented public Calculator call is the single-query
`pc_calc_action_outcomes` surface.

Code authority: `engine/src/solver_calc.cpp` and
`engine/src/solver_reforge.cpp`.

## Solve And Reprice

A solve performs these implemented stages:

1. Project the concrete start item and expand reachable abstract states.
2. Admit legal state-local candidates and copy exact outcomes into one compact
   sparse transition graph, subject to state, row, transition, reforge-work,
   diagnostic, output, and owned-byte caps.
3. Price each operator by dotting its resource quantities with the pinned
   economy. Missing prices exclude the affected operator and are diagnosed;
   absent never means free.
4. Optimize cyclic components with SCC-based policy iteration and sparse
   component solves. A prioritized Bellman path remains the explicit fallback
   if policy evaluation fails.
5. Extract deterministic policy choices, observation-owned Unveil choices,
   values, reachability, diagnostics, hashes, and optional solve-log records.

Focused expansion computes disclosed lower and upper bounds and an optimality
gap while extending relevant fringe states. A result is converged only when
the implementation's exactness and gap conditions pass. Resource exhaustion is
a reported boundary, not a numeric solution.

Transition caches are price-independent and can be reused by a solver handle,
but the browser's long-lived transfer/reprice lifecycle is not a settled
stable promise. The current product retains a solve handle; the deferred
browser-lifetime work is recorded in the [solver roadmap](../future/solver-roadmap.md)
and [solver notes](NOTES.md).

Code authority: `engine/src/solver_solve.cpp`.

## Policy Compilation

`pc_solver_compile_strategy` converts the latest solved policy into v1 strategy
JSON. The document contains ordinary start, router, operation, and terminal
nodes, deterministic prioritized edges, `expected_cost` annotations, and
non-executable accounting-role metadata. Fixed and automatic operators expand
to their primitive programs. An explicit off-policy failure terminal catches
states outside the compiled policy-reachable set.

The compiled `base_state` preserves the solve start, not merely the base type:
it serializes rarity, item flags, generic influence bits, both Eldritch tiers,
and every materialized prefix/suffix modifier with crafted/fractured flags.
This matches the simulator's existing start-item parser and corrects the stale
audit claim that compiled verification always started from a fresh normal
base.

Compilation refuses a policy when the ordinary strategy vocabulary cannot
represent it or when configured graph/output caps are exceeded. It does not
invent a second execution format.

Code authority: `engine/src/solver_compile.cpp` and
`engine/src/simulator.cpp`.

## Exact Strategy Evaluation And Accounting

The exact evaluator derives a strict layout from the compiled graph's actions
and condition targets, discovers `(graph node, abstract state)` pairs, and
solves the resulting absorbing graph by SCC. It reports terminal probability,
action-not-applied/no-edge/unresolved attribution, expected actions and
materials, node/edge flow, incoming state classes, and S8.4 accounting and
review projections. Quantities remain price-independent; product code applies
the active price table for display.

Evaluation refuses unsupported graph vocabulary rather than estimating it.
At d5e38e3 compiler-only `mod_count`/`mod_family_count` and concrete Unveil
offer conditions remain gaps for whole-graph evaluation. The stateful API has
begin/step/finish/destroy calls, cooperative progress, owned/output byte caps,
and live/peak memory statistics.

Code authority: `engine/src/solver_eval.cpp` and
`engine/src/solver_api.cpp`.

## Public And Browser Interfaces

The public ABI is declared in `engine/include/poecraft/solver.h`:

| Surface | Implemented contract |
| --- | --- |
| Registry | create/destroy, action count/info/find, candidate indices |
| Calculator | exact `pc_calc_action_outcomes` for one concrete item/action |
| Solve | synchronous solve plus begin/step/finish/abandon |
| Results | state value/policy, concrete projection, compile, solve log |
| Diagnostics | versioned telemetry and selected live/peak memory statistics |
| Exact graph | synchronous evaluate plus begin/step/finish/destroy and memory statistics |

`bindings/wasm/wasm_api.cpp` exposes the same stateful solve and evaluation
surfaces to the web worker. `apps/web/src/app/engine-worker.ts` provides
cooperative stepping, progress, cancellation, and event-loop yields. WASM
build/export/memory details are owned by the [engine WASM reference](../engine/wasm.md).

## Boundaries

- Minimum expected cost is the implemented solve objective; roll-quality
  finishing is not part of this DP state.
- Product optimality is always relative to the admitted and priced action
  scope. Diagnostics must disclose exclusions and caps.
- Whole-graph exact evaluation and sampled simulation are separate evidence
  sources. A sampled cost mean is not automatically proof of Bellman parity.
- Recombinators, publishing, ML use, remaining product-scope decisions, and the
  measured R3A boundary are deferred; see the
  [solver roadmap](../future/solver-roadmap.md).
- Mechanic rules are never decided by this architecture file; see the
  [mechanics library](../mechanics/README.md).

## History And Notes

- [Archived solver architecture and S1-S8 phase record](../archive/2026-07-19-bestiary-solver-s8/solver-plan.md)
- [Solver notes](NOTES.md)
