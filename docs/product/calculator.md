# Calculator

**Status: stable implemented product reference.**

Parent: [Product](README.md)

Verified against code and complete non-visual R4 acceptance: 2026-07-26 @
browser-transfer/lifetime R4 closure. Scope:
`pc-calculator`, goal/draft models, solver worker orchestration, exact-outcome
presentation, and shared economy access. No rendered or visual review was
performed; that review remains Oliver's.

## Contract

For registered one-item actions, Calculator combines:

```text
one concrete input item
+ one authored v1 goal
+ one selected engine action
-> exact native, goal-aware outcomes for that action
```

Bestiary is the deliberate exception: its exact Calculator surface operates
on compound item-plus-checkpoint state and does not require goal slots merely
to show the deterministic action result. Goal slots are required to open the
ordinary solver handle and to run Solve.

Selecting an action does not mutate the input item. Input modifiers and goal
requirements share the engine-backed modifier pool but use different modes.
New goals author stable modifier families; recovered legacy group slots remain
readable/removable. The goal also contains finished rarity and
`min_satisfied_slots` (`All N` or at least a selected count). Goal slots are
bounded by the native maximum of eight.

The concrete Input and target Goal frames share `pc-mod-list`. The component
renders already-resolved catalog/engine data; it does not decide modifier
families, tiers, legality, or probability. Direct input editing retains the
engine-backed modifier and fracture gestures. Goal rows express tier-or-better
requirements and show native marginal slot probability when available.

Code authority:
`apps/web/src/app/components/pc-calculator.ts`,
`apps/web/src/app/calculator-goal-model.ts`,
`apps/web/src/app/components/pc-mod-list.ts`, and
`apps/web/src/app/workspace/persistence.ts`.

## Exact One-Action Result

Calculator opens a native solver handle for the current goal and uses
`pc_calc_action_outcomes` through the WASM worker. A hand-selected Fossil
loadout is explicitly requested when needed so it remains queryable outside
the bounded automatically generated Fossil set.

The engine result owns:

- supported/legal state;
- sparse abstract successor probabilities;
- per-slot satisfied probability; and
- combined success probability for finished rarity plus the slot threshold.

The Odds inspector presents that result, groups returned classes by goal
coverage, exposes overlapping miss signals, and retains a capped raw technical
distribution. TypeScript does not recompute the success predicate. It does
perform display-only arithmetic for failure probability, independent-repeat
expected attempts, action cost, and `action cost / success probability`.
Those last two values are explicitly not a full strategy forecast: they omit
reset, recovery, cleanup, and base spend unless the selected action itself
contains those inputs.

Action cost comes from the descriptor's complete price-key quantity vector and
the pinned workspace economy. Missing prices remain missing. Bestiary compound
outcomes use the dedicated exact Bestiary surface but are presented alongside
the registry actions.

Code authority:
`engine/include/poecraft/solver.h`, `engine/src/solver_api.cpp`,
`apps/web/src/app/odds-presentation.ts`, and
`apps/web/src/app/components/pc-calculator.ts`.

## Solve To Strategy

The Solve surface is distinct from one-action odds:

1. Pin the effective workspace economy, including action price provenance.
2. Open a native `action_mode: "goal_relevant"` envelope, which enables the
   current automatic-candidate substrate. Native action classification retains
   automatic dependencies separately; the descriptor list returned to the web
   app contains independently selectable candidates only.
3. Keep only candidates whose complete price vectors resolve. If priced
   Fracture is relevant, require an explicit `base` price because miss recovery
   uses Restart.
4. Open a fresh scoped solve handle with
   `goal_progress_gated_reforges: true` and run the stateful native
   begin/step/finish API in the worker with progress and cancellation. This is
   the Calculator product scope; the native/WASM option remains optional and
   other callers retain their explicit scope or the unrestricted default.
5. Compile whenever the result has `policy_available`, including bounded cap
   and target-gap results. Transfer the compiled document as one byte buffer,
   decode/parse it once on the main thread, assign missing board positions,
   attach the economy identity, and allow an unsaved copy to open in Strategy
   Builder. A non-converged result without a proper executable policy is not
   compiled.
6. Release the scoped solve handle and its transition closure after summary,
   telemetry, and strategy handoff. A later solve or reprice rebuilds.

Two optional product stopping targets are available: absolute chaos-equivalent
gap and relative percent gap. Either positive target can stop only after a
complete lower/upper round. The inputs do not change Bellman comparisons,
ties, admission, pruning, or eventual exact results.

The result identifies policy quality and termination separately, and shows the
exactly evaluated returned-policy cost, optimal-cost lower bound, certified
policy upper bound, absolute gap, certified multiplicative factor, requested
target/firing criterion, pinned economy, and the exact admitted priced action
IDs. Cap hits remain visible even when an executable bounded policy survives.
The result also names the first deterministic stopping cause and reports every
cap in a bit mask. Registry, candidate, evaluator-supported, supported-priced,
missing-price, and unsupported-vocabulary action counts remain separate; the
UI does not collapse them into an ambiguous unavailable or skipped total.
All configured caps stay visible before solving even when policy availability
or the current action envelope makes a particular cap unlikely to fire.
Bounded certificates use only wording such as “Certified within 1.10x of
optimal” and “At most 10% more expensive than optimal.” They never say the
policy is 10% suboptimal or call the upper bound the optimum; a weak lower
bound can make the certificate pessimistic.

Automatic options remain native planner operators and compile into primitive
strategy nodes; the web app does not execute opaque macros.

Near policy quality, the result discloses the actual product scope without
listing every action: goal-relevant action discovery, the zero-progress
destructive-reforge retry restriction, admitted priced counts grouped by
family, missing-price exclusions, deferred Veiled crafting, and any unresolved
action obligations left by a resource stop. Detailed admitted action IDs stay
in the existing collapsible section. Solver telemetry is therefore retrieved
for successful exact results as well as bounded or refused results.

Every solver-generated document records its non-executable scope as optional
`solver_policy_scope` metadata. Calculator results use
`zero_progress_reroll_policy_restriction`; unrestricted callers use
`unrestricted`. Legacy authored documents may omit the field. The metadata is
provenance for presentation and persistence, never simulator routing
authority.

The product publishes a Strategy Board document only when the selected policy
has exact executable identity. When a coarse-parent action or downstream route
needs discarded modifier/exclusion identity, the native solver now treats
that compatibility witness as a request for lazy exact refinement. It visits
only policy-reachable strict carriers, emits class-local exact routers when
one decision remains sound, and locally re-optimizes affected subclasses when
their exact action values differ. The returned artifact must compile,
exact-evaluate as a proper absorbing policy, and reconcile with the displayed
policy cost before `policy_available` remains true. A named refinement cap or
a witnessed renewal whose expected action count exceeds the product
Simulator's 100,000-action run limit can still withhold publication. An action
with an unsupported or incomplete observation/preservation/destruction
contract is rejected during native admission before solving, rather than
becoming a post-solve publication refusal. These are native proof boundaries,
not frontend crafting rules.

For a fixed program with an observed choice, the returned exact strategy keeps
each offer ordering scoped to the native pre-choice observation carrier. An
equal modifier offer reached from another carrier does not reuse that choice
group, and Calculator does not reconstruct or broaden the match.

The complete Calculator-to-worker-to-native sequence, including handle
ownership, cooperative cancellation, compilation, repricing, and verification,
is documented in [End-To-End Solver Flow](../solver/flow.md).

The browser worker adaptively steps solves but caps a call at four work items.
Calculator does not retain the scoped solve handle after transfer. Strategy
inputs are also encoded once and transferred to the worker for compilation or
exact evaluation instead of structured-cloning the full graph. A retained
transition-cache product mode remains deferred in the
[solver roadmap](../future/solver-roadmap.md).

Code authority:
`apps/web/src/app/solve-workspace.ts`,
`apps/web/src/app/components/pc-calculator.ts`,
`apps/web/src/app/engine-worker.ts`, and the [Solver](../solver/README.md).

## Current Verification Button

`Verify 10,000 runs` compiles the generated document and samples it through the
native strategy simulator with the same pinned economy. The current UI
requires all 10,000 runs to complete and the aggregate cost status to be
complete, then compares sampled mean known cost with the exact
`evaluated_policy_cost` of the returned policy.

That button is sampled evidence, not yet a truthful end-to-end verification
gate. At the B2 boundary it does not require a success threshold, zero
failure/stop/limit counts, or zero action-not-applied/no-edge/off-policy
outcomes before displaying the cost delta, and it does not display sampled
variance/confidence. These are open repair items, not implemented contracts;
see [Product Notes](NOTES.md) and the
[solver roadmap](../future/solver-roadmap.md).

The compiled graph does preserve the solve's complete materialized start item,
including rarity, flags, influences, Eldritch tiers, and crafted/fractured
explicit modifiers. Verification is not implicitly reset to a fresh normal
base.

## Economy And Persistence

Calculator drafts in IndexedDB preserve the base, item level, input state,
goal rarity/slots/threshold, selected action, and Fossil loadout. They are
crash-recovery state rather than Stash resources. Emulator and Stash item cards
open Calculator through their `Odds` handoff.

Every calculation or solve uses the shared workspace economy facade. Price
changes update display calculations and may trigger a new solve, but they do
not change mechanic legality or one-action outcome probabilities. See
[Economy](../economy/README.md).
