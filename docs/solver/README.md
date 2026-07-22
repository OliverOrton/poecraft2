# Solver

**Status: stable implemented architecture reference.** Historical phase plans
and acceptance narratives are archived and do not control current sequencing.

Parent: [Documentation index](../README.md)

Verified against code and action/state-pruning acceptance: 2026-07-21 working tree. Scope:
native solver, calculation/evaluation engines, public C ABI, policy
compilation, and the non-visual WASM/worker path. The pinned measurements live
in [solver-scaling v1](../../fixtures/solver-scaling/v1/README.md); this stamp
does not claim rendered-browser review or a mechanic ruling.

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
[mechanics library](../mechanics/README.md). The complete UI-to-native request,
handle, cancellation, compilation, evaluation, and verification sequence is in
[End-To-End Solver Flow](flow.md).

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
evaluator-only. Automatic-candidate layouts also retain ordinary affix identity
needed to materialize the current carrier exactly, even when the explicit
primitive envelope cannot roll those affixes.

After strict reachability closes, the solver refines a collision-checked
behavioral partition across every admitted action. Legality, automatic
admission, resource identity, exact probability, choices, and projected
successor classes must all match before states merge. The strict layout is the
oracle; unknown or mismatched observations remain distinct. The bounded Chaos
control merges 57,722 strict states to 3 classes, while both accepted complete
product envelopes merge none because their larger action sets observe every
difference. No approximate global compaction exists.

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
price-independent envelope and enables state-local automatic candidates.
Those candidates cover relevant primitive Fracture, permanent bench
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
2. Admit legal state-local candidates. Exact action producibility and setup
   legality reject impossible protected-repeat programs before option-kernel
   construction.
3. Evaluate deterministic goal finishes and Restart before broad stochastic
   kernels. A price-bound constructive state certificate may stop a carrier
   early only when every other admitted operator has an optimistic lower
   bound strictly above an executable row upper bound. The lower bound grants
   an operator every goal slot any constituent primitive could possibly
   produce, then prices the cheapest relaxed primitive cover of the remaining
   goal requirement.
4. Copy required exact outcomes into one sparse
   transition graph, subject to state, row, transition, reforge-work,
   diagnostic, output, and owned-byte caps. Collision-checked observation
   signatures reuse exact kernel payloads without changing strict states.
5. Price each operator by dotting its resource quantities with the pinned
   economy. Missing prices exclude the affected operator and are diagnosed;
   absent never means free.
6. Refine a completed all-action strict graph into the exact quotient, then
   optimize cyclic components with SCC-based policy iteration and sparse
   component solves. A prioritized Bellman path remains the explicit fallback
   if policy evaluation fails.
7. Extract deterministic policy choices, observation-owned Unveil choices,
   values, reachability, diagnostics, hashes, and optional solve-log records.

Focused expansion computes finite constructive upper bounds and global lower
bounds while extending relevant fringe states. A zero gap is an exact closure
proof and may finish directly without a separate outer Bellman phase. A
complete executable incumbent can also survive a resource-cap stop or an
enabled product gap target. In that result, `L` is the certified optimal-cost
lower bound, `U` is the incumbent certificate, and `J_pi` is the exact returned
policy cost with `L <= J_pi <= U`. The gap targets are checked only after a
complete focused lower/upper round; they do not participate in Bellman
comparisons, ties, admission, pruning, or exact closure. Resource exhaustion
without an executable proper fallback reports no finite upper bound.

Exact focused closure uses the absolute numerical proof tolerance
`epsilon * 10`. Separately named value comparisons may retain their historical
value-scaled roundoff allowance. Neither tolerance is a product gap target,
and requested absolute or relative gaps never relax exact closure.

Once a constructive renewal/progressive-fracture fallback has been
synthesized, focused rounds retain it in the atomic incumbent strictly as an
executable upper-bound/output witness. Reuse validates goal, economy, action
vocabulary prefix, referenced row/operator ownership, and properness.
Monotonic graph growth and lazy action-vocabulary extension are allowed while
the complete prefix present at synthesis remains identical. A new focused
round or lower-bound update does not trigger re-synthesis; a missing witness,
changed existing executable dependency, or failed validation does. The
retained witness never guides focus, admission, pruning, ties, or Bellman
comparisons.

The atomic incumbent captures same-round values, selected row IDs, frontier
operators, fallback, and provenance. Policy references and Unveil preferences
are deterministic derived output and are materialized once if that incumbent
is returned. Exact searches therefore do not rebuild full-state output vectors
on every improving upper round.

The constructive state certificate is not compaction and does not infer
equivalence from similarity. Its witness records the executable upper, the
strict minimum competing lower, and the number of kernels avoided. Because
the proof depends on current prices, a partial graph produced by it is never
retained as the price-independent transition cache; a later reprice rebuilds
or safely reuses only a separately completed all-action graph.

Selected-allocation enforcement uses incremental owner ledgers with periodic
full audits. On the accepted two-T1 product, per-state preparation byte audits
fell from the 22.47-second baseline to 7.3 ms (0.04% of expansion); audited
undercount is a hard error.

Transition caches are price-independent and can be reused by a solver handle,
but the browser's long-lived transfer/reprice lifecycle is not a settled
stable promise. The current product retains a solve handle; the deferred
browser-lifetime work is recorded in the [solver roadmap](../future/solver-roadmap.md)
and [solver notes](NOTES.md).

Code authority: `engine/src/solver_solve.cpp`.

## Policy Compilation

`pc_solver_compile_strategy` converts the latest executable policy into v1
strategy JSON. `policy_available`, not exact convergence, is the compilation
precondition. Exact policy regions with the same action and continuation share
operation nodes, and a collision-checked decision DAG routes concrete states
to those regions. The document otherwise contains ordinary start, router,
operation, and terminal nodes, deterministic prioritized edges,
`expected_cost` annotations, and non-executable accounting-role metadata.
Fixed and automatic operators expand to their primitive programs. Exact closed
policies retain the explicit off-policy failure terminal. Bounded policies use
an explicit safe Restart/fallback default for unmatched compiled states; a
frontier heuristic is never emitted as an action.

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

The exact evaluator derives a strict layout from the compiled graph's actions,
family/mod count observations, and condition targets; discovers `(graph node,
abstract state)` pairs; and solves the resulting absorbing graph by SCC. It
contracts compiler-generated policy routing without losing exact node/edge
flow and uses dense, rank-one, or matrix-free preconditioned component solves
as appropriate. It reports terminal probability,
action-not-applied/no-edge/unresolved attribution, expected actions and
materials, node/edge flow, incoming state classes, and S8.4 accounting and
review projections. The evaluator internally retains exact abstract-state,
compiled-node, and action occupancy together with immediate priced reward;
the retained occupancy/reward dot product is reconciled with exact expected
cost. B4 owns its report surface. Quantities remain price-independent; product
code applies the active price table for display.

Evaluation refuses unsupported graph vocabulary rather than estimating it.
`mod_count` and `mod_family_count`, including required crafted/fractured flags,
are exact. Concrete authored Unveil-offer conditions remain the named gap. The
stateful API has
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
| Solve | synchronous solve plus begin/step/finish/abandon, product gap targets, and live `L`/`U`/gap progress |
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
- Recombinators, publishing, ML use, and remaining product-scope decisions are
  deferred; see the
  [solver roadmap](../future/solver-roadmap.md).
- Mechanic rules are never decided by this architecture file; see the
  [mechanics library](../mechanics/README.md).

## History And Notes

- [End-to-end solver flow](flow.md)
- [Archived solver architecture and S1-S8 phase record](../archive/2026-07-19-bestiary-solver-s8/solver-plan.md)
- [Solver notes](NOTES.md)
