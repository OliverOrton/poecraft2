# S7 Solver Depth And Performance Plan

Active execution plan for making the existing one-item solver capable of
realistic end-to-end crafts before adding more mechanic families. Read
[AGENTS.md](../AGENTS.md), [direction.md](direction.md), and
[HANDOFF.md](../HANDOFF.md) first. Stable solver architecture remains in
[crafting-solver-plan.md](crafting-solver-plan.md); this file owns S7
sequencing, performance gates, and stop boundaries.

Status 2026-07-15: planning is active; implementation has not started. Oliver
selected solver depth and a large solver-specific performance pass ahead of
trade leaves, corruption, Hinekora's Lock, imprints, recombinators, and other
mechanic expansion. The parked mechanic track is recorded in
[solver-mechanic-extensions.md](solver-mechanic-extensions.md).

## Outcome

S7 is complete when a user can provide:

```text
one concrete start item or clean purchasable base
one multi-modifier target
the permitted one-item crafting mechanics
a frozen price table
```

and receive a complete crafting policy that:

- finds and costs useful intermediate stages without the user hand-authoring
  those stages;
- handles retry, salvage, recovery, and restart branches;
- compiles into an ordinary editable Strategy Board document;
- expands every solver shortcut into real primitive engine operations;
- verifies through the native simulator with realized cost and success
  behavior consistent with the solver result;
- completes inside owner-approved native and browser time/memory budgets.

This is still the one-item solver. Locks, imprints, recombinators, corruption
endgames, trade-leaf intermediates, and ML guidance remain outside S7. Live
economy ingest is useful parallel work but is not a prerequisite: S7 fixtures
use frozen prices.

## Current Baseline And Why It Slows Down

S1-S6 prove a correct vertical slice, not realistic endgame scalability:

- `solver_solve.cpp` forward-expands the complete reachable abstract-state
  closure. Every expanded state scans every priced candidate action.
- Every Bellman sweep scans every expanded state and every candidate action
  again. Policy extraction repeats the Q-value scan.
- The action registry eagerly enumerates every one-to-four-fossil loadout. A
  current Vaal Regalia session has 25 named fossils, 15,275 fossil loadouts,
  and 15,604 total actions.
- Candidate actions determine discriminating tags and junk classes before the
  solve. An oversized action set therefore enlarges both the action dimension
  and the abstract state dimension.
- `max_states` limits expanded states, not all discovered successor states,
  state/action pairs, transition entries, or solver-owned bytes.
- Retry-heavy values descend from a fixed `1e12` ceiling through whole-table
  sweeps. The authored-strategy evaluator already solves comparable cycles by
  SCC decomposition and closed-form/dense component evaluation, but the
  optimal solver does not reuse that machinery.
- Transition distributions, successor vectors, and heap-backed junk counts
  accumulate in memory. Large reforge evaluators can build large internal
  frontiers for each distinct action.
- Calculator Solve creates a temporary solver and closes it after each run, so
  price-only re-solves do not currently retain the transition cache promised by
  the architecture plan.
- The compiler emits routing predicates for each policy-reachable state. A
  numerically successful large solve can still produce an unusably large board.
- Phase 14 benchmarks engine action and simulator throughput. There is no
  native/WASM optimal-solver benchmark or solver-owned memory accounting.
- The six-slot all-T1 acceptance test is a small synthetic session restricted
  to Chaos and Restart. It validates solve/compile/simulate contracts, not a
  real multi-stage endgame craft.

Two registered one-item actions are not calculation-supported today:
Harvest resistance conversion and Fracture. The stable solver doc also lists
scour-plus-alchemy and bench removal even though neither is a primitive solver
descriptor today.

## S7 Invariants

1. **The native engine remains the rule authority.** TypeScript may present
   progress, diagnostics, and prices; it does not infer legality or odds.
2. **The primary result remains exact up to the declared abstraction.** Safe
   pruning requires a proof or an exhaustive-oracle comparison. Deferred
   candidates remain discoverable through bounds rather than being silently
   deleted. A heuristic mode may exist only if Oliver explicitly approves it,
   and it must be labeled separately.
3. **Macros are solver operators, not new crafting rules.** The simulator runs
   primitive actions only. Every selected macro expands into the same ordinary
   graph vocabulary a user can inspect and edit.
4. **Fixed macro kernels stay price-independent.** They report expected
   resource quantities; the active economy supplies the dot product. If two
   setup alternatives differ by price, expose separate fixed options or cache
   the selected option under an economy fingerprint.
5. **Information timing is preserved.** Sampled offers such as Unveil remain
   observe-then-decide choices; no macro may turn them into a preselected random
   result.
6. **No global macro registry explosion.** Goal-relevant primitive actions and
   options are synthesized lazily for the current session, goal, and start
   state.
7. **Performance work is measured on native and WASM.** Optimize the phase that
   the solver benchmark identifies; do not reopen the already-optimized pool
   picker without evidence.
8. **S6 Phase 3 stays skipped.** Ambient Emulator odds are not part of this
   performance or solver-depth work.

## Permanent Benchmark Corpus

S7.0 creates versioned solver benchmark specifications under a dedicated
fixture directory. Each specification pins:

```text
engine/data version and base metadata path
item level and concrete start state
goal slots, tiers, rarity, and success threshold
allowed mechanic families and exhaustive-oracle action mode
frozen resource price table
expected support/refusal status
native and WASM budgets
verification run count and statistical tolerance
```

The permanent matrix must contain at least:

1. **Oracle cases:** synthetic and small real one-/two-modifier goals whose
   optimum is hand-computable or exhaustively solvable.
2. **Ordinary real craft:** at least two target modifiers and meaningful choice
   among rolling, adding, removing, benching, and restarting.
3. **Advanced real craft:** three or four target conditions with at least two
   stages, a preservation/recovery decision, and either metamod or Eldritch
   intent.
4. **Endgame real craft:** five or six finished conditions from an approved
   clean or fractured start, requiring multiple mechanic families and a
   complete compiled policy.
5. **Stress/refusal cases:** intentionally unreachable goals, missing prices,
   cap exhaustion, cancellation, and actions whose exact evaluator is absent.

Oliver approves the concrete ordinary, advanced, and endgame targets before
they become permanent gates. Test fixtures may use random simulation seeds;
policy/value results remain deterministic for identical solve inputs.

### Solver Benchmark Report

Add a native and worker/WASM harness, driven by one PowerShell entry point,
that writes comparable JSON reports under `build/performance`. Report at least:

```text
registry generation and abstract-layout time
actions before/after support, relevance, dominance, and price filters
discriminating tags and junk-class count
states discovered, expanded, frontier, and goal states
state/action rows evaluated and transition entries stored
distribution/reforge cache requests, hits, misses, and build time
expansion, optimization, extraction, compile, and verification time
Bellman backups, sweeps or policy-improvement rounds, and optimality gap
solver-owned bytes, native process working set, and WASM heap growth
maximum worker step duration and cancellation acknowledgement latency
policy-reachable states, compiled nodes/edges, and strategy JSON bytes
V(start), simulated mean cost, success count, and off-policy failures
```

S7.0 records the unoptimized baseline before setting final machine-specific
budgets. Structural gates below apply on every machine; time and memory ceilings
are ratified against Oliver's machine after the baseline is captured.

## Planner Operator Model

Keep the existing primitive `ActionDescriptor` registry as execution
authority. Add a solver-only operator layer:

```text
planner operator
  id and display name
  kind: primitive | option
  initiation predicate
  primitive dependencies
  fixed internal option program
  fixed, expressible exit predicate

evaluated option kernel for one entry abstract state
  legal / supported / terminates almost surely
  expected primitive resource quantities
  expected primitive action count
  exit abstract-state distribution
  observation-owned choice groups
  expansion recipe for StrategyDocument compilation
```

For a fixed option `o`:

```text
Q(s, o) = dot(prices, E[resources | s, o])
          + sum over exits e of P(e | s, o) * V(e)
```

An option program is a small graph of primitive operations and abstract-state
routers. Its evaluator solves the internal absorbing chain exactly. The cache
key includes the option definition and entry abstract state, but not prices.
The chosen policy stores a tagged primitive-or-option reference instead of only
a registry action index.

Options stay alongside primitives. Replacing a primitive or hiding an internal
decision requires transition equivalence or a formal dominance proof. This is
important because an intermediate failure can be salvageable even when the
macro usually looks cheaper.

### First Fixed Options

- **Scour then Alchemy:** valid only when the resulting state makes Alchemy
  legal. Fractures and side locks can leave preserved explicit mods, so this is
  not an unconditional two-action alias.
- **Eldritch prefix/suffix intent:** an explicit dominance-setup sequence plus
  the primitive Eldritch Exalt, Chaos, or Annul. Tiers remain real output state;
  setup is not silently restored. If the goal constrains implicit tiers, exact
  tiers remain in the outer state and no dominance-only collapse is allowed.
- **Protected side operation:** apply the approved prefix/suffix lock, perform
  Scour or a supported reforge, and return every resulting state. If the lock
  must be reapplied on retry, resource use records every application.
- **Deterministic bench finish:** Multimod plus an explicitly selected set of
  finishing bench crafts, with crafted counts and open sides checked exactly.

### Renewal And Local-Policy Options

The highest-value collapse is a repeat loop with a fixed exit predicate:

```text
repeat primitive or fixed option
until one of a finite set of success / salvage / brick exits is reached
```

Candidates include Alteration, Chaos, Essence, Fossil, Harvest reforge,
scour/Alchemy, and protected rerolls. Failures may be hidden only when another
attempt has the same preserved-base transition kernel. The outer solver still
receives every meaningful exit and decides whether to continue, recover, or
restart.

Unveil options retain sampled three-option offers as observation-owned choice
groups. Fracture options wait until the abstraction identifies the actual
fractured carrier; a Boolean "some mod is fractured" flag is insufficient.

### Unsafe Collapses

- "Repeat until useful" without a fixed native-expressible exit predicate.
- Selecting an Unveil result before its offer is sampled.
- Treating all crafted state as one Boolean in Multimod or recrafting flows.
- Treating all fractured state as one Boolean when later actions depend on the
  fractured carrier.
- Removing primitives merely because a macro usually wins on price.
- Hiding or restoring Eldritch implicit tiers after a side-intent option.

## Correctness Substrate Required By S7

Before relying on macro policies, S7 must close these one-item gaps:

1. Pin Oliver's Eldritch dominance rule. The current primitive engine falls
   back to unrestricted ordinary Exalt/Chaos/Annul behavior when neither side
   is dominant; registry legality does not require dominance.
2. Pin which actions honor, remove, or consume each metamod. The native action
   and calculation paths must agree before protected-crafting options are
   summarized.
3. Add a real remove-crafted-modifiers primitive and approved cost if Oliver
   wants blocker/no-attack/no-caster cleanup represented.
4. Implement exact Harvest resistance conversion and Fracture calculation
   evaluators.
5. Refine abstract state only where required by future decisions:
   fractured goal/junk carrier, crafted goal mask and junk counts, total
   crafted count, and side-specific open/removable facts. Prefer option-local
   refinements over globally widening every solve when equivalence permits.
6. Extend compiler conditions only for distinctions a selected policy actually
   requires, then verify them through the ordinary simulator.

## Action-Space Control

Action reduction happens before abstract layout construction:

1. Remove evaluator-unsupported actions and actions illegal for the entire
   session/start/goal envelope.
2. Use symbolic production/destruction/preservation metadata to build a
   backward goal-relevant set.
3. Close that set over structural dependencies: rarity setup, open-slot
   creation, metamods, dominance setup, recovery, and Restart.
4. Generate only fossil loadouts whose signatures remain potentially useful.
   A simple "does not mention the goal tag" rule is unsafe because suppressing
   junk can improve a target family.
5. Within one state, collapse actions with identical abstract transition
   kernels to the cheapest priced representative. Preserve ties and diagnostics.
6. Record one reason for every included, deferred, pruned, unpriced, or
   unsupported action.

An exhaustive-oracle mode remains available for small fixtures. Certified
pruning must choose the same value and policy as exhaustive mode on the oracle
matrix. Potentially useful actions that are not yet expanded are deferred by a
bound; they are not reported as proven irrelevant.

## Large Solver Performance Pass

The performance pass spans S7.2 and S7.5.

### Storage And Hot-Loop Work

- Add independent caps for discovered states, expanded states, state/action
  rows, transition entries, reforge frontier work, compiled graph size, and
  solver-owned bytes.
- Pack variable abstract-state payloads into arenas or compact interned storage
  instead of one heap allocation per state's junk counts.
- During expansion, store legal evaluated state/action rows once in contiguous
  sparse form. Optimization and extraction must not repeat legality checks,
  distribution hash lookups, or successor-value allocations.
- Use calculated proper-policy/restart bounds instead of a universal `1e12`
  initialization where possible.
- Retain a compatible solver context and transition cache for price-only
  re-solves; invalidate on goal, action, engine, data, or abstraction changes.
- Split expansion and optimization into bounded work units. A single full sweep
  may not monopolize the WASM worker or delay cancellation.
- Instrument representative-item caching and reforge-frontier storage before
  trading memory for speed.

### Cycle And Optimization Work

First eliminate direct self-loop contributions algebraically. Then benchmark:

1. residual-prioritized backups as a low-risk improvement;
2. policy iteration using a known proper initial policy;
3. exact fixed-policy evaluation over SCCs using the proven strategy-evaluator
   geometric, rank-one, dense, and bounded-fallback machinery;
4. action improvement until stable.

Policy iteration is the preferred target if it wins the benchmark corpus. It
directly attacks the thousands of numerical sweeps caused by retry loops.

Only if action control, compact storage, macro kernels, and policy iteration
still fail the hard corpus should S7 add LAO*-style focused expansion. That pass
must maintain admissible lower/upper bounds and report the start-state
optimality gap. It may not silently call an unexpanded frontier solved.

### Policy Compression

Before compiling, group policy regions that choose the same continuation and
can be separated by existing native conditions. Reuse selected option subgraphs
instead of cloning their primitive nodes for every state. Compression is
accepted only if exact strategy evaluation and Monte Carlo verification remain
consistent with the raw policy.

## Phasing

Each phase ends with relevant native, binding, WASM, and web gates green, one
local commit, and a rewritten `HANDOFF.md`. Stop at each boundary; do not roll
the next phase into the same change.

### S7.0 - Benchmark corpus and solver telemetry

- Add native and worker/WASM solver benchmark harnesses and report schema.
- Propose the real ordinary, advanced, endgame, and stress fixtures for Oliver
  approval; pin frozen prices and exhaustive oracle subsets.
- Capture baseline runtime, memory, state/action/transition size, graph size,
  cancellation, and simulation agreement without optimizing solver behavior.
- Ratify hard native/WASM time and memory ceilings after the baseline.

Gate: comparable native/WASM JSON exists for every approved case; repeated
runs report stable structural counts; no optimization claim is made without a
saved baseline.

### S7.1 - One-item correctness and state substrate

- Implement approved Eldritch dominance and metamod behavior.
- Add bench removal if approved.
- Add Harvest resistance conversion and carrier-exact Fracture evaluators.
- Add only the crafted/fractured/side-specific abstract features required by
  the approved benchmark crafts and compiler.

Gate: primitive engine versus exact evaluator matrices pass on synthetic and
real artifact fixtures; every new distinction compiles and simulates without
off-policy routing.

### S7.2 - Action control, storage, and first performance pass

- Build the goal-relevance/dependency analyzer and exhaustive-oracle mode.
- Generate fossil loadouts lazily and collapse certified equivalent actions.
- Add complete action-inclusion diagnostics and independent resource caps.
- Store sparse transition rows once, compact state payloads, and chunk Bellman
  work for responsive cancellation.

Gate: reduced and exhaustive modes agree on oracle fixtures; no approved case
exceeds a configured cap without an explicit diagnostic; the phase improves
the benchmark corpus and does not regress any pinned case beyond the ratified
tolerance.

### S7.3 - Fixed solver options

- Land the planner-operator/kernel contract and tagged policy actions.
- Compile options into primitive Strategy Board subgraphs.
- Implement scour/Alchemy, approved Eldritch side-intent, protected-side, and
  deterministic Multimod finishing options.

Gate: every option kernel matches its expanded primitive graph's exit
distribution and expected resource vector; solve/compile/simulate remains
consistent with `V(start)`.

### S7.4 - Renewal and observation-aware options

- Implement fixed-exit repeat options for approved rolling/reforge methods.
- Add protected repeat loops that correctly repay setup costs.
- Preserve observed Unveil choice semantics inside option evaluation.
- Add fracture preparation/retry only after carrier-exact state lands.

Gate: closed-form hand cases and exact expanded graphs match option kernels;
outer policies still see all success, salvage, and brick exits.

### S7.5 - Deep optimization and cache reuse

- Eliminate algebraic self-loops and benchmark prioritized backups.
- Extract/reuse SCC policy-evaluation machinery and implement policy iteration
  if it wins the pinned matrix.
- Reuse transition caches for price-only solves.
- Add bounded focused expansion only if the preceding work still misses the
  hard-corpus budget.
- Compress policy regions before compilation.

Gate: final native/WASM time, memory, worker-slice, cancellation, and graph-size
budgets pass; the hard-corpus geometric-mean speed and peak memory improve by
the ratified large-pass targets; small-policy values remain unchanged within
current numerical tolerance.

### S7.6 - End-to-end product gate

- Solve every approved real craft from its declared start without hand-authored
  intermediate stages.
- Open each generated ordinary strategy in the workspace.
- Run exact strategy evaluation where supported and at least 30,000 Monte Carlo
  verification runs for permanent native gates; keep the UI's interactive
  verification count smaller.
- Present actions considered/deferred/pruned, resource caps, optimality status,
  expected materials, and verification delta.

Gate: every required craft converges without a cap hit, compiles without a
vocabulary gap, has no off-policy simulation route, and matches expected cost
and success behavior within the fixture's statistical tolerance.

## Structural Acceptance Gates

These do not depend on machine speed:

- Existing S1-S6 fixtures preserve expected values and policies.
- Exhaustive and certified-reduced action sets agree on oracle-sized cases.
- Every omitted action has a stable diagnostic reason or a valid deferred
  lower bound.
- Macro kernels equal their expanded primitive graphs on exact fixtures.
- Discovered states, pairs, transitions, bytes, reforge work, and graph size
  obey independent caps.
- The worker yields and cancellation is acknowledged within the ratified bound.
- Raw and compressed policies agree under exact evaluation.
- Compiled-policy simulation remains consistent with `V(start)` and reports no
  unmatched route.

Provisional performance targets, to be confirmed after S7.0, are:

```text
moderate real craft: native <= 10 s, WASM <= 30 s
hard endgame craft:   native <= 2 min, WASM <= 5 min
worker step:          <= 50 ms
cancel acknowledgement: <= 250 ms
final hard-corpus improvement: >= 5x geometric-mean solve speed,
                               >= 2x peak-memory reduction
```

The final owner-approved limits replace these provisional numbers before S7.1.

## Owner Decisions Required Before S7.1

1. Approve the permanent real craft targets and their start states.
2. Set the hardest acceptable browser solve time and memory budget.
3. Confirm whether tied/no Eldritch dominance is illegal and whether a
   prefix/suffix intent option may establish dominance automatically.
4. Pin which actions honor, remove, or consume each metamod used by the corpus.
5. Confirm whether remove-crafted-modifiers should be added and its cost key.
6. Confirm that the primary solver must remain provably optimal up to its
   abstraction; decide separately whether a labeled heuristic mode is wanted.

Mechanic answers come from Oliver and are written into focused fixtures before
implementation. Agents must not research or guess them.

## Stop Boundary

S7 ends after realistic one-item crafts meet the complete performance and
verification gate. Do not begin the parked mechanic track, live economy UI
integration, publishing/accounts, recombinators, or ML work as part of S7.
