# Crafting Solver And Calculation Engine Plan

**Status:** S1-S6 are implemented. This file is the stable architecture and
baseline record; active scalability, macro-action, and performance execution is
owned by [solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md).

Modifier vocabulary, pool semantics, and weight rules in this plan defer to
[mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md) and
[weight-calculation-flow.md](weight-calculation-flow.md). Item state layout
defers to [item-state-flow.md](item-state-flow.md). Strategy graph vocabulary
defers to [strategy-editor-ui.md](strategy-editor-ui.md).

## Purpose

Given a start item and a goal item split, produce the cheapest crafting
strategy automatically. The solver outputs a full policy — a best action for
every reachable item state — so "should I annul, keep going, or restart"
is answered by lookup instead of user intuition. The policy compiles into
the existing strategy graph format and is verified by the existing simulator.

Two user-facing surfaces come out of the same machinery:

```text
Calculator:
  new workspace tab alongside Emulator/Simulator/Strategy Builder.
  given the current item and one action, show the exact outcome
  distribution, per-goal-mod hit odds, action cost per attempt, and
  same-input action-spend estimate per success.

Solver:
  baked into the Simulator workflow.
  given a goal split, generate an optimal strategy graph, annotated
  with expected remaining cost at every node.
```

The Calculator is the transition engine exposed directly; the Solver is the
transition engine plus dynamic programming. Building them together means the
solver's math is user-inspectable one action at a time.

## Decisions

```text
problem class:        stochastic shortest path MDP (minimize expected cost)
goal semantics:       tier-perfect (mod present at required tier or better)
roll quality:         outside the DP; post-process expected divine cost
action scope:         full PoE1 mechanic set modeled as data from v1;
                      engine implements mechanics incrementally
implementation:       C++20 in the native engine, C ABI boundary,
                      same source for native / Python / WASM
solve budget:         minutes are acceptable; run in workers with
                      progress + cancellation like long simulations
prices:               action costs are currency-quantity vectors,
                      dotted with a user-editable price table at solve
                      time; never baked into cached artifacts
ml readiness:         solver logs (state, value, action) tuples in a
                      durable format from the first working version
```

Tier-perfect covers "make perfect items": a goal split of six mods all at
T1 is just a goal spec with tier thresholds on every slot. Numeric roll
perfection within tier does not change any decision the DP makes for
pool-driven actions, so it is reported as a separate expected divine cost
attached to the finished-goal terminal rather than tracked in state.

## Problem Formalization

Crafting is not a path problem. An action from a state is a probability
distribution over successor states, so the solver does not output a route;
it outputs a policy over every reachable state:

```text
V(goal state)  = 0
V(s)           = min over legal actions a of
                   cost(a) + sum over s' of  P(s' | s, a) * V(s')
policy(s)      = the action achieving the minimum
```

Restarting is not a special case. Every state has a synthetic action
"restart" whose cost is a fresh base (plus scour where applicable) and whose
successor is the clean-base state with probability 1. Value iteration then
decides salvage-versus-restart automatically by comparing expected
costs-to-go. The same holds for "annul the blocker", "reforge over it",
and every other recovery idea — they are ordinary actions competing in the
same minimization.

Because reforge-class actions can move the item backward, the state graph
has cycles and Dijkstra-style settling does not apply. Value iteration (or
policy iteration) over the reachable state set is the base algorithm. The
restart action bounds every value — no state can be worth more than
restart cost plus V(clean base) — which keeps iteration stable and gives a
cheap initial upper bound.

## Goal Specification

A goal split is a list of goal slots:

```text
goal slot
  mod group or mod family        (same vocabulary as strategy conditions)
  minimum tier                   (tier-perfect: usually 1)
```

plus the required finished rarity and `min_satisfied_slots`. The threshold
defaults to the number of slots (all requirements), but can express goals such
as "at least 2 of these 3 modifiers." The same predicate terminates value
iteration, defines Calculator success, and compiles to the strategy
vocabulary's native `at_least` condition. Invalid thresholds are rejected at
goal parse time.

## Abstract State Model

The concrete item space is far too large to enumerate. The solver operates
on abstract states: projections of the compact item state onto only the
features that change transition probabilities or action legality for this
goal and this action set.

```text
abstract state
  per goal slot:    absent | present below tier | satisfied
  per goal slot:    blocked flag (a non-goal mod occupies the same
                    mod group, or an exclusion mask forbids it)
  prefix count, suffix count
  junk composition: counts per junk equivalence class (see below)
  mechanic flags:   rarity, crafted-mod slot, prefix/suffix-cannot-
                    be-changed, fractured slot, influence signature,
                    corrupted, split/mirrored
```

### Junk Collapsing Rule

A non-goal mod is collapsible with another non-goal mod if and only if every
action in the candidate action set treats them identically. Three things
break equivalence, and only these three:

1. Mod group / exclusion overlap with a goal slot (tracked as the blocked
   flag, not as a junk class).
2. Tag signature, restricted to tags that some candidate action
   discriminates on (fossil weight tags, harvest reforge-more/less tags,
   tag-targeted removal).
3. Nothing else. Under chaos/alt/annul, junk is fully described by
   prefix/suffix counts.

The junk equivalence classes are therefore derived, not designed: prune the
action set for the goal first, collect the set of tags those actions
discriminate on, and partition the session mod universe by
(generation type, restricted tag signature, blocks-a-goal-group). The
engine's existing group/classification masks and lazy tag-signature weights
are exactly the primitives this derivation needs. A coarser action set
automatically yields a coarser, smaller state space.

### Soundness Check

The abstraction is approximately sound, not exactly sound: two concrete
items in the same abstract state can have slightly different pools because
non-goal mods perturb weights. This is accepted and then verified
empirically: the end-to-end gate (below) simulates the compiled policy and
compares realized cost against V(start). If a goal shows material drift,
the fix is refining the offending feature into the abstraction, not
abandoning it.

## Action Model

The solver consumes an engine-owned action registry rather than hardcoding
pool rules. A mechanic is plannable only when its engine action, descriptor,
legality model, and exact calculation evaluator are all present; registering an
engine action alone is not sufficient.

```text
action descriptor
  action id, display name
  cost vector                (currency quantities, price-independent)
  legality predicate         (over abstract mechanic flags: rarity,
                              metamod flags, corrupted, open affixes...)
  transition kind:
    single-slot              exact enumeration from the weighted pool
                             (exalt, annul, aug, regal, veiled add...)
    reforge                  sequential multi-mod roll
                             (chaos, alch, essence, fossil,
                              harvest reforges, veiled chaos)
    deterministic            probability-1 successor
                             (scour, bench craft, metamod application,
                              restart/buy base)
    special                  bespoke enumerator
                             (beast split/imprint, awakener orb,
                              eldritch currency, fracture, recombinator)
  discriminating tags        (which tags this action's weights or
                              targeting depend on — drives junk classes)
  state-flag effects         (sets/clears crafted slot, metamod flags,
                              influence, fracture...)
```

Current one-item coverage:

```text
engine + solver:   transmute/aug/alt/regal/alch/chaos/exalt/annul/scour,
                   essences, fossils, bench crafts, metamod locks,
                   veiled chaos/exalt + unveil, harvest reforge/augment,
                   harvest resistance conversion, eldritch implicits/currency,
                   influenced exalts, Fracture, remove-crafted-modifiers
S7 operators:      solver-only scour/Alchemy and macro/sub-policy operators
later:             corruption, beastcrafting, recombinators (implementation plan
                   Phase 18; session-universe implications already
                   handled by the bitset plan)
```

The deferred mechanics, plus trade-leaf actions, Hinekora's Lock,
corruption endgames, and finishing-cost items, are planned in
[solver-mechanic-extensions.md](solver-mechanic-extensions.md).

Recombinators and beast imprints stress the single-item state assumption
(they involve a second item or a saved copy). Recombinators resolve this
outside the item-level DP: a spec-level pyramid solver plus recomb/feeder
strategy blocks, per the extensions doc. The registry schema still
reserves a `companion state` slot in the descriptor for any future
technique that must track a second live item inside the item-level solver
itself; nothing planned uses it.

## Calculation Engine (Transition Provider)

The component that answers: from abstract state s, applying action a, what
is the exact distribution over abstract successor states? It is both the
solver's inner loop and the Calculator tab's backend.

Three evaluation paths by transition kind:

1. Deterministic: emit the single successor. Trivial.
2. Single-slot: one pass over the cached prefix-sum weighted pool for a
   representative concrete item; sum weights per abstract successor class.
   Exact.
3. Reforge: exact enumeration over full multi-mod outcomes is
   combinatorial, but the solver only needs probabilities over abstract
   classes. Two evaluators, cross-checked:
   - sequential-roll DP in abstract feature space: roll slots one at a
     time, tracking (goal hits, junk class counts) with weight updates for
     group removal between rolls. Exact up to the abstraction.
   - Monte Carlo via the engine batch API from a representative concrete
     item, histogrammed into abstract classes. Ground truth for tests and
     fallback for mechanics whose sequential structure resists the DP
     (some fossil combinations, harvest more/less stacking).

### Strategy Evaluation

The same transition provider also evaluates an already-authored strategy graph
exactly. `pc_strategy_evaluate` derives an evaluation layout from the compiled
graph's referenced families/groups and used action descriptors, then propagates
probability mass over `(graph node, abstract state)` until it reaches a
success/failure/stop terminal, a simulator-parity failure bucket, or the
explicit unresolved remainder. Routing matches the native simulator's stable
priority/source ordering and default-edge fallback. Illegal actions absorb as
`action_not_applied`; no matching edge absorbs separately.

Whole-graph evaluation uses a strict exclusion-effect junk partition so
concrete states that remove different weighted families from later pools do
not collapse together. This refinement is evaluator-only: the ordinary DP
solver retains the compact approximately-sound abstraction described above.
Every used action is checked through the calculation evaluator's support
dispatch, and unsupported actions/conditions return one element-level gap list
instead of an estimate.

The result is price-independent and includes terminal probability by node,
failure/unresolved attribution, expected actions, expected consumption by
price key, expected node visits, expected edge traversals, condition targets,
and a top-K incoming abstract-state mixture per node. The Strategy Builder's
Calculator mode renders those values directly; its only arithmetic is
conditional edge-share presentation and the workspace-price dot product.

Supporting pieces:

```text
representative item materialization
  abstract state -> one concrete compact item consistent with it
  (needed for pool construction and MC sampling)

distribution cache
  key: (session id, abstract state signature, action id)
  value: sparse successor distribution
  price-independent by construction; survives price edits and is
  shared between Calculator queries and solver sweeps
```

### C ABI Surface

```text
pc_calc_action_outcomes(session, item_or_abstract_state, action id)
  -> sparse distribution over abstract successors, plus per-goal-slot
     hit probabilities and the combined goal-success probability

pc_calc_batch_outcomes(...)         same, for many (state, action) pairs

pc_strategy_evaluate(compiled strategy, options)
  -> exact graph terminal/failure mass, expected work and consumption,
     node/edge flows, and incoming state classes
```

The Calculator tab calls the first form with its live input item. The solver
calls the batch form during expansion. An earlier proposal to expose the same
calls as ambient Emulator odds was skipped entirely by Oliver and is not a
scheduled consumer of this API.

## DP Solver

```text
pc_solve(session, start item, goal spec, action registry subset,
         price table, budget/config)
  -> value table, policy, diagnostics
```

Algorithm:

1. Derive junk classes and abstract state layout from goal spec + pruned
   action set.
2. Forward-expand the reachable abstract state set from the start state,
   requesting transition distributions from the calculation engine
   (cache-first). Restart makes the clean base reachable from everywhere;
   expansion is over the closure.
3. Value iteration to convergence (max residual below epsilon in cost
   units), initialized from the restart upper bound. Asynchronous /
   prioritized sweeps ordered by residual are a drop-in speedup if plain
   sweeps are slow; measure first.
4. Extract policy = per-state argmin. Tie-break deterministically
   (lower variance, then action id) so identical inputs yield identical
   strategies.
5. Attach the post-process expected divine/roll-finishing cost to the goal
   terminal for display.

Action costs stay quantity vectors internally; the price table is applied
inside the sweep as a dot product. Re-solving after a price edit reuses the
entire distribution cache and only reruns steps 3–5.

Long solves run like long simulations: in a worker (native thread or web
worker) with progress reporting (states expanded, residual, current
V(start) bound) and cancellation. Minutes-long budgets are acceptable;
the WASM build gets correctness first and optimization only if profiling
demands it.

### Action Pruning

The implemented S1-S6 solver accepts an explicit action subset, but does not
automatically perform the goal-relevance pruning described below. Calculator
Solve currently includes every priced registry action. S7 implements the safe
version of this contract before abstraction, because the candidate set defines
the junk classes:

- keep actions that can produce, remove, or protect a goal slot, plus
  structural actions (scour, restart, rarity changes, metamods that gate
  them);
- generate and retain only actions proved relevant, while keeping structural
  dependencies and deferring uncertain candidates behind valid bounds;
- always keep the restart action.

Every action receives an included, deferred, pruned, unpriced, or unsupported
diagnostic reason. Exhaustive-oracle mode verifies certified pruning on small
fixtures. The detailed contract is in the active S7 plan.

### Solver Options And Macro Actions

S7 adds a solver-only operator layer above primitive actions. A fixed option is
an internally exact primitive strategy fragment with an initiation predicate,
expected resource vector, finite exit-state distribution, observation-owned
choice groups, and an expansion recipe. Its Bellman value is the resource
price dot product plus the value of its exits. The simulator never executes an
opaque macro: selected options compile back into ordinary operation/router
subgraphs and pass the same simulation verification gate.

Initial options include valid scour/Alchemy sequences, explicit-side
Eldritch intent, protected-side operations, deterministic Multimod finishes,
and fixed-exit renewal loops. Options may not hide salvage exits, preselect
Unveil choices, or collapse crafted/fractured carrier distinctions that affect
future mechanics.

## Policy To Strategy Graph

The policy compiles into a `StrategyDocument`:

- each abstract state that the policy can actually visit under itself
  (policy-reachable set, much smaller than solve-reachable) becomes a
  router node whose condition tree tests membership using existing
  condition types (mod group presence/tier, affix counts, rarity,
  metamod flags);
- the policy action becomes the operation node; edges route outcomes back
  into the router layer;
- terminals: goal reached (success), budget/safety limits (stop);
- every node is annotated with V(s) so the strategy board can display
  expected remaining cost at each point — this is the "baked into the
  simulator" integration.

Condition vocabulary gaps discovered here (e.g. a junk-class count that no
existing condition can express) are closed by adding condition types to the
strategy model, not by inventing a second execution format.

### End-To-End Verification Gate

The sole plan-level correctness check is performed once at the end of the full
active solver plan:

```text
solve goal -> compile policy -> simulate compiled strategy exactly 10,000 times
           -> empirical mean cost within tolerance of V(start)
           -> empirical success rate consistent with policy semantics
```

This one loop catches abstraction unsoundness, transition math errors,
compiler bugs, and condition-vocabulary gaps at once. It becomes a fixture
test for a small set of pinned goals (including one all-T1 "perfect item"
goal). Separate evaluator/oracle matrices are not required plan acceptance
gates; a narrowly targeted internal test may be used only when something is
broken and it is needed to diagnose or fix it. Rendered or visual UI checking
is left to Oliver unless he explicitly asks for it.

Exact policy compression may merge equivalent regions. A later, owner-controlled
readability mode may also trim choices with negligible value impact, but must
report the discarded delta and honest bounded/heuristic status; it is not part
of S7.1 or the core S7 acceptance gate.

## ML Data Logging

From the first working solver version, every solve can emit a corpus:

```text
solve record
  session/goal fingerprint, price table hash
  per state: feature vector, V(s), policy action, visit reachability
```

Durable, versioned, append-only. This is the training set for later value
function distillation and search guidance, and the evaluation baseline for
any RL experiment. No model work is in scope for this plan; the logging is
so that corpus accumulation starts on day one.

## Workspace Integration

```text
Calculator tab (new, alongside Emulator/Simulator/Strategy Builder):
  input: item (from Stash, Emulator handoff, or built in place),
         one action
  output: outcome distribution over goal-relevant classes,
          per-mod hit odds, expected cost, top concrete outcomes

Simulator integration:
  input: start item + goal spec (condition editor)
  action: run solver (worker, progress, cancel)
  output: generated strategy document opened in the Strategy Board,
          nodes annotated with expected remaining cost,
          expected materials list (inputs and expected quantities,
          priced by the active economy snapshot),
          one-click verification run (the end-to-end gate)
```

## Phasing

Solver phases are numbered independently of the main implementation plan.
S1 is complete: `engine/src/solver_internal.hpp` defines the registry
schema, goal spec, and abstract state layout; `solver_registry.cpp`
enumerates descriptors for every implemented mechanic plus the synthetic
restart action; `solver_abstract.cpp` derives junk classes from the session
masks; `engine/tests/test_solver_abstract.cpp` is the gate.

S2's transition provider is complete in `solver_calc.cpp`: `CalcContext`
owns the state table and price-independent distribution cache,
materializes representative items, and evaluates exact deterministic
(scour, bench, restart) and single-slot (augment, regal, exalt, annul,
influenced exalt) distributions by enumerating the same weighted pool the
engine samples. `engine/tests/test_solver_calc.cpp` gates it against
hand-computed pool sums and engine Monte Carlo histograms. The
`pc_calc_action_outcomes` C ABI landed with the S5 surface and the
Calculator tab with S6. S6 Phase 4 completed the veiled and eldritch
evaluators described below, so every currently registered veiled/eldritch
mechanic has an exact calculation path.

S3's core reforge evaluator is complete in `solver_reforge.cpp`: a
forward-frontier sequential-roll DP over roll buckets — per-goal-slot
satisfied/below-tier buckets plus (side, junk class, block mask, family
weight) junk buckets with family multiplicities — giving exact group
removal between rolls, target-count mixing (1-2 magic, 4-6 rare), and
early-stop absorption. It covers transmute, alteration, alchemy, chaos,
essence (guaranteed direct adds), and fossils (fossil-weighted pools,
forced adds, Bloodstained/mirror flag effects). The S3 gate in
`test_solver_calc.cpp` pins hand-computed sequential probabilities on the
synthetic session and Monte Carlo agreement for chaos, essence, and
fossil on the Vaal Regalia fixture. Harvest reforge is evaluated exactly
via a two-phase roll DP (guaranteed spawn-only tag pick, then normal
fills, with dual-weight buckets); harvest augment — intentionally
add-then-remove per the project owner's ruling — enumerates its two
stages exactly. Both are MC-gated on the synthetic and Vaal Regalia
fixtures. S6 Phase 4 added exact veiled chaos/exalt transitions, weighted
three-option unveil offers with policy-owned Bellman choice, tiered eldritch
ember/ichor setters, and dominance-aware eldritch exalt/chaos/annul. Synthetic
and Vaal Regalia 20k-sample Monte Carlo matrices gate the special evaluators.

S4's solver core is complete in `solver_solve.cpp`: reachable-closure
expansion through the calculation engine, in-place value iteration
descending monotonically from a finite ceiling (the restart bound makes
every goal-connected value finite), price-vector costs with explicit
missing-price/unsupported-action exclusion diagnostics, deterministic
policy extraction (variance then action-id tie-breaks), policy-reachable
marking, and `serialize_solve_log` producing the per-state ML corpus
records. The S4 gate in `engine/tests/test_solver_solve.cpp` matches
analytic alt-spam costs (V = 1/p), verifies restart optimality in
forced-bad (corrupted) states and under price flips, and solves a toy
one-mod goal on the Vaal Regalia fixture deterministically. Worker/
progress/cancel plumbing arrives with the C ABI.

S5's compiler and the end-to-end verification gate are complete.
`solver_compile.cpp` emits ordinary strategy JSON: a master router whose
prioritized edges test policy-reachable state membership with existing
condition types, operation nodes annotated with `expected_cost` (V(s)),
a success terminal, and a failure terminal that makes off-policy leaks
fail loudly. The strategy vocabulary gained a `restart` operation
(simulator resets to a fresh base, price key `base`). Vocabulary gaps throw
instead of mis-compiling. S6 Phase 4 closed the original tag-discriminating-
layout, flagged-state, group-tier, and ambiguous-signature gaps with exact
junk-member counts, item flags, influence bits, eldritch tiers, group minimum
tiers, and unveil-option conditions. The gate in
`engine/tests/test_solver_compile.cpp` runs the full loop —
solve -> compile -> simulate — and empirical mean cost matches V(start)
on the synthetic alt-spam and restart policies and on the Vaal Regalia
toy goal. It also verifies one six-slot all-T1 policy and a sampled-unveil
policy for 30k successful runs each.

Goal terminals preserve `min_satisfied_slots`: all-slot goals compile as
`all`, while partial thresholds compile as the simulator's native
`at_least` composite. The compiler gate simulates a one-of-two policy to pin
that contract.

The C ABI surface lives in `engine/include/poecraft/solver.h` and
`engine/src/solver_api.cpp`: `pc_solver_create` takes a goal-spec JSON
(slots by group key or family mod key, rarity, optional
`min_satisfied_slots`, optional candidate action subset),
`pc_calc_action_outcomes` is the Calculator backend (exact successor
distribution plus per-slot hit odds and combined goal-success probability for
a concrete item),
`pc_solver_solve` runs synchronous value iteration against a `pc_economy`
price table, and `pc_solver_compile_strategy` / `pc_solver_solve_log`
return the strategy JSON and ML corpus records. The public-ABI gate in
`engine/tests/test_solver_api.cpp` exercises the whole surface end to
end, finishing with the compiled policy verified through the public
simulator.

The browser runtime is wired: `pcw_solver_*` facade exports in
`bindings/wasm/wasm_api.cpp`, worker methods and `EngineClient` calls in
`apps/web/src/app`, and a headless acceptance test that runs Calculator
odds, a solve, and the compiled policy's simulator verification inside
the WASM worker.

The Calculator workspace tab is built: `pc-calculator`
(`apps/web/src/app/components/pc-calculator.ts`) is a workspace document
alongside Emulator/Strategy/Stash, seeded from the Stash ("Odds" on item
cards), an Emulator handoff (the craft bar's "Odds" button), or a base
picked in place. Its selection surfaces are the Emulator's own: goal
mods come from the same modifier-pool browser (`pc-mod-pool` mounted
with `select-goal` — clicking a tier requires that tier or better, with
per-slot tier thresholds), and the action comes
from the same craft-panel band (basic/essence/harvest/fossil/eldritch/
influenced/veiled), whose buttons select a registry action id instead of
applying a craft — fossil loadout ids are reassembled from single-fossil
keys, and the worker's `omitFossilCombos` filter keeps the once-per-
session candidate fetch (used for cost-key lookups) small. Variant E uses a
stacked left context rail: clicking the concrete input item or authored goal
switches what the shared modifier pool edits. The goal's `Success means`
selector changes `min_satisfied_slots` in the native solver. The Odds
inspector leads with `success_probability`, then groups the returned outcome
classes by exact modifier coverage and shows overlapping miss signals; the
raw abstract distribution (rarity, affix counts, slot status, blocked flags,
mechanic flags) is retained in a collapsed technical drawer. Cost per attempt
still comes from the action's cost keys dotted with the workspace price table
(`workspace/prices.ts`, the manual-override layer of the planned Economy
service). S6 Phases 1, 2, and 4 are complete. Oliver skipped the planned Phase
3 Emulator ambient odds-before-you-click surface entirely, so S6 is complete.

```text
S1  action registry schema + descriptors for implemented mechanics;
    goal spec type; abstract state layout + junk-class derivation
    gate: junk classes derived from masks match hand-computed cases

S2  calculation engine: deterministic + single-slot exact paths,
    representative materialization, distribution cache,
    pc_calc_action_outcomes; Calculator tab MVP over it
    gate: exact single-slot distributions match brute-force pool sums
          and MC histograms within tolerance

S3  reforge evaluators: sequential-roll DP + MC cross-check
    gate: DP vs MC agreement on pinned fixtures (chaos, essence,
          fossil on the Vaal Regalia rule fixture)

S4  DP solver core: reachability expansion, value iteration, policy
    extraction, price-vector costs, worker/progress/cancel plumbing,
    solve logging
    gate: hand-checkable toy goals (1–2 mods) match analytic expected
          costs; restart behavior correct in forced-bad states

S5  policy -> strategy graph compiler + end-to-end verification gate
    gate: pinned goals pass the simulate-vs-V(start) check, including
          one synthetic all-T1 perfect-item goal

S6  simulator/workspace integration, diagnostics UI, WASM validation
    gate: browser solve of a representative goal completes within
          budget in a worker with live progress

S7  realistic end-to-end one-item capability and solver performance
    gate at the end of the full plan: approved real multi-stage crafts solve,
          compile, and run in the simulator after native/WASM gains are
          reported for Oliver to evaluate against owner-set criteria
```

New engine mechanics (bench, metamods, harvest, ...) land as registry
descriptors plus engine actions and exact calculation evaluators, gated by
S3-style distribution fixtures per mechanic. Parked future mechanics use the
M1-M5 sequence in [solver-mechanic-extensions.md](solver-mechanic-extensions.md).

## Risks

- Abstraction drift on weight-perturbing junk: accepted and measured by
  the S5 gate; refine features when a goal shows material error.
- Sequential-roll DP correctness under group removal and tag-weight
  updates mid-roll: mitigated by the mandatory MC cross-check in S3.
- State explosion on all-T1 goals with many tier-tracked slots: the current
  solver expands the full reachable closure; policy reachability only reduces
  compilation afterward. S7 addresses action generation, compact storage,
  exact macro kernels, cycle-aware policy optimization, and—only if still
  required—bounded LAO*-style focused expansion.
- Metamod/flag interactions multiply legality edge cases: legality lives
  in one predicate per descriptor, tested per mechanic, never inline in
  solver code.
- Recombinator/imprint two-item states: explicitly deferred; schema
  reserves the extension point so deferral does not become redesign.
