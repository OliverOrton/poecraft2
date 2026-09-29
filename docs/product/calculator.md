# Calculator

**Status: stable implemented product reference.**

Parent: [Product](README.md)

Current-scope audit: verified against source on 2026-08-25 @ `a1449fa` for
goal-progress gating, economic Restart, automatic Imprint defaults, result
scope disclosure, and the native finalization phases. No web suite or rendered
review was run for this documentation-only audit.

Verified against code, complete non-visual R4 acceptance, the final Solver Goal
Realignment native/release-WASM acceptance, selected-policy cooperative
finalization acceptance, and recovery-scoped Restart native/release-WASM/web
acceptance: 2026-08-22 @ `1e21260` / `cfd8904`. Scope:
`pc-calculator`, goal/draft models, solver worker orchestration, exact-outcome
presentation, and shared economy access. No rendered or visual review was
performed; that review remains Oliver's.

## Contract

The 2026-09-27 continuity migration adds **Allow extra modifiers** to Goal item.
It defaults off, including recovered drafts that lack the optional field.
When enabled, the native goal still requires its requested rarity, modifier
tiers and satisfaction count, but unrelated final explicit modifiers are
allowed. The setting persists and travels with odds, Solve and diagnostic
goal requests as `allow_extra_modifiers: true`.

Coverage goals select the native `target_neutral_zero` proof profile. Exact
one-action odds and checked executable policies remain available; positive
clean-target lower bounds, gap stopping and global optimality certification
are not available. The UI disables gap controls and discloses this scope.
This does not activate the private selective service or qualify new timings.

For registered one-item actions, Calculator combines:

```text
one concrete input item
+ one authored v1 goal
+ one selected engine action
-> exact native, goal-aware outcomes for that action
```

Bestiary's exact Calculator surface operates
on compound item-plus-checkpoint state and does not require goal slots merely
to show the deterministic action result. Goal slots are required to open the
ordinary solver handle and to run Solve.

Awakener uses the current item as receiver and a selected Stash donor. Calculate
enumerates its retained pairs and weighted refill without modifying either
resource. Dominance and Vaal also have native single-action odds, separate from
strategy search. Vaal exposes all four branch probabilities and a weighted
implicit table even before an explicit goal is selected. The table distinguishes
rolling an implicit from having it on the final item, including survival of an
existing implicit. Socket changes retain their 25% mass but are otherwise ignored.

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

Input and Goal use the normal shared item ledger, with Prefixes above Suffixes.
Input/Goal buttons switch the active item column and modifier-pool context
without scrolling between two stacked cards. Craft controls and the modifier
pool remain visible in their own editing column. Only Odds and Strategy finder
share the compact results pane. Craft choices, pool rows and reports scroll
within their own areas without displacing the item. Views remain mounted when
switched, preserving edits and results.

Harvest choices show the complete material recipe, including Crystallised
Rancour or Sacred Lifeforce where required, and the selected economy's craft
price and source. Recipes come from the existing versioned economy manifest;
the UI does not derive new crafting rules or reprice a complete recipe from
incomplete component quotes.

Code authority:
`apps/web/src/app/components/pc-calculator.tsx`,
`apps/web/src/app/calculator-goal-model.ts`,
`apps/web/src/app/components/pc-mod-list.tsx`, and
`apps/web/src/app/workspace/persistence.ts`.

## Exact One-Action Result

Calculator opens a native solver handle for the current goal and uses
`pc_calc_action_outcomes` through the WASM worker. A hand-selected Fossil
loadout is explicitly requested when needed so it remains queryable outside
the bounded automatically generated Fossil set.

Awakener, Dominance and Vaal use `pc_calc_currency_outcomes_json`. This native
endpoint reuses the physical weighted-refill DP from concrete prepared items and
returns terminal structural observations, native success and slot marginals.
Its response-local state IDs are not strategy state handles. The goal-free
`pc_calc_create_inspector` is inspection-only and refuses strategy solving.
The existing execution subset and unresolved-law refusals still apply; numeric
roll-value goals and automatic multi-item/corruption search are not introduced.

The engine result owns:

- supported/legal state;
- sparse abstract successor probabilities;
- per-slot satisfied probability; and
- combined success probability for finished rarity, the requested slot
  threshold and selected extra-modifier policy. With the default clean setting,
  a covered item with an unmatched explicit affix is not success.

The Odds inspector presents that result, groups returned classes by goal
coverage, exposes overlapping miss signals, and retains a capped raw technical
distribution. TypeScript does not recompute the success predicate. It does
perform display-only arithmetic for failure probability, independent-repeat
expected attempts, action cost, and `action cost / success probability`.

Each current WASM outcome includes native `is_goal`; row highlighting uses it
instead of guessing terminal success from coverage alone. Probabilities use
full double precision across the WASM JSON facade. The display suppresses only
tiny floating-point excursions outside [0,1]. Tier labels follow the actual
selected threshold. An already-satisfied input can compile a zero-action
goal-guarded strategy and still undergo ordinary native artifact checking.
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
`apps/web/src/app/components/pc-calculator.tsx`.

## Solve To Strategy

The Solve surface is distinct from one-action odds:

1. Pin the effective workspace economy, including action price provenance.
2. Build a product-envelope goal that is independent of the action currently
   selected in the Odds inspector. Exact selected-action odds may explicitly
   materialize that Fossil on its own scoped handle; that request-only detail
   never enters the ordinary Solve envelope.
3. Open a native `action_mode: "goal_relevant"` envelope, which enables the
   current automatic-candidate substrate. Native action classification retains
   automatic dependencies separately; the descriptor list returned to the web
   app contains independently selectable candidates only.
4. Keep only candidates whose complete price vectors resolve. If priced
   Fracture is relevant, require an explicit `base` price because miss recovery
   owns an exact fresh-base replacement branch.
5. Open a fresh scoped solve handle with the versioned native
   `calculator_product_v1` solve profile, then run the stateful native
   begin/step/finish API in the worker with progress and cancellation.
   The profile owns goal-progress-gated reforges, high-impact executable-upper
   work, voluntary economic Restart off, generated Imprint programs off, zero
   gap targets, and a 200,000-state policy-refinement allowance. The web app
   sends only positive gap targets and explicit user overrides; it does not
   recreate those defaults field by field.
   Calculator therefore does not abandon an ordinary carrier merely because a
   fresh start is cheaper. An unchecked “Allow abandoning this item and buying
   a fresh base” control explicitly restores that economic action. The native
   engine default remains unrestricted for backward compatibility.
   “Consider automatic Imprint checkpoint/retry programs” is a separate,
   unchecked-by-default control. Selecting it explicitly adds generated
   Imprint programs; other automatic families remain eligible in either
   scope. Results, telemetry, and compiled strategies disclose the profile,
   named overrides, and selected action scope and make no optimality claim
   over an excluded family.
   An advanced diagnostic panel can also disable native action families for a
   single solve. All families are enabled by default. The Calculator presents
   engine-provided primitive family metadata for pricing/readiness and sends a
   typed disabled-family list to both product-envelope and scoped Solve goals;
   exact Odds inspection is unaffected. The result repeats the disabled names
   and states that bounds and exactness apply only within the restricted
   envelope. The existing Imprint and economic-Restart controls remain the
   ordinary focused controls rather than duplicate family checkboxes.
   The refinement allowance affects only optional attempts to certify a
   cheaper broad/strict policy after an executable fallback is already
   independently verified. A proper, zero-off-policy, completely priced
   direct graph that improves the verified portfolio publishes its exact
   evaluated cost as a bounded upper without a second strict-lift traversal;
   solver/exact cost mismatch still blocks exactness. Main solving and the
   fallback's exact evaluation retain their normal caps.
6. Compile whenever the result has `policy_available`, including bounded cap
   and target-gap results. Transfer the compiled document as one byte buffer,
   decode/parse it once on the main thread, assign missing board positions,
   attach the economy identity, and allow an unsaved copy to open in Strategy
   Builder. A non-converged result without a proper executable policy is not
   compiled.
7. Release the scoped solve handle and its transition closure after summary,
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

A named numerical-stability stop is likewise bounded, never exact. It means
the selected policy stopped changing across complete fixed-policy evaluations
while strict comparisons remained unresolved inside the numerical tolerance.
Calculator may publish the independently evaluated executable policy as an
upper bound, keeps the certified lower and open obligations visible, and does
not relabel the policy value as the optimum.

That stable selected policy is not silently replaced by the previous fallback.
Native finalization retains it as an unverified candidate, independently
compiles/evaluates it, and publishes it only if it earns executable proof and
beats every other independently evaluated candidate. Calculator displays
native expanding, iterating, refining, compiling, and certifying progress; it
does not manufacture a synthetic finalization phase. Cancellation remains
available until native `Done`, after which result transfer is packaging-only.

Automatic options remain native planner operators and compile into primitive
strategy nodes; the web app does not execute opaque macros.

Near policy quality, the result discloses the actual product scope without
listing every action: goal-relevant action discovery, the zero-progress
destructive-reforge retry restriction, whether economic Restart was admitted,
admitted priced counts grouped by family, missing-price exclusions, bounded
automatic Veiled dependencies, and any unresolved action obligations left by
a resource stop. Detailed admitted action IDs stay in the existing collapsible
section. Solver telemetry is therefore retrieved for successful exact results
as well as bounded or refused results.

Every solver-generated document records its non-executable scope as optional
`solver_policy_scope` metadata. Default Calculator results use
`zero_progress_reroll_and_no_economic_restart_restrictions`; opting into
economic Restart uses `zero_progress_reroll_policy_restriction`. Engine callers
can also produce `unrestricted` or
`no_economic_restart_policy_restriction`. Legacy authored documents may omit
the field. The metadata is provenance for presentation and persistence, never
simulator routing authority.

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

### Native Solver Lab handoff

**Copy Lab case** reuses the current Calculator product-goal construction and
copies a versioned `solver_lab_calculator_export_v1` envelope. It includes the
concrete start carrier, canonical explicit-modifier keys and supported slot
flags, influence/Eldritch state, native product goal, diagnostic family
exclusions, supported gap targets, and the pinned effective Allflame price
snapshot. It does not serialize the currently inspected Odds action into Solve
scope.

The current Lab profile fixes goal-progress gating on and both voluntary
economic Restart and automatic Imprint programs off. Calculator requires those
two UI controls to be off before export. It also refuses an active Bestiary
checkpoint or special item flags that the benchmark's start-item contract
cannot reproduce. These refusals prevent a case from silently changing before
native validation. Import, editing, validation, immutable revision save, and
submission are owned by the [Native Solver Lab](../foundation/solver-lab.md).

The browser worker starts ordinary product solves conservatively and adapts
toward a responsive slice. The explicit 1,024-work-item qualification request
retains that complete native batch from its first and subsequent calls because
step boundaries affect incremental scheduling; its separate 20-second guard
remains the responsiveness contract. Calculator does not retain the scoped
solve handle after transfer. Strategy inputs are encoded once and transferred
to the worker for compilation or exact evaluation instead of structured-
cloning the full graph. A retained transition-cache product mode remains
deferred in the
[solver roadmap](../future/solver-roadmap.md).

Code authority:
`apps/web/src/app/solve-workspace.ts`,
`apps/web/src/app/solver-lab-export.ts`,
`apps/web/src/app/components/pc-calculator.tsx`,
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
goal rarity/slots/threshold, selected action, Awakener donor identity, and Fossil loadout. They are
crash-recovery state rather than Stash resources. Emulator and Stash item cards
open Calculator through their `Odds` handoff.

Every calculation or solve uses the shared workspace economy facade. Price
changes update display calculations and may trigger a new solve, but they do
not change mechanic legality or one-action outcome probabilities. See
[Economy](../economy/README.md).
