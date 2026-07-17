# S7 Solver Depth And Performance Plan

> **Archived 2026-07-17.** This is the completed historical S7 execution plan,
> not current sequencing authority. Oliver directed work to move forward after
> the final endgame sample recorded `0.9942` against the former `0.995` target;
> the miss remains part of the record. See
> [Project Direction](../../direction.md), the current
> [HANDOFF](../../../HANDOFF.md), and the active
> [B1/S8 plan](../../bestiary-and-solver-capability-plan.md).

Active execution plan for making the existing one-item solver capable of
realistic end-to-end crafts before adding more mechanic families. Read
[AGENTS.md](../../../AGENTS.md), [direction.md](../../direction.md), and the
[archived final S7 handoff](HANDOFF-s7.6-final.md) first. Stable solver
architecture remains in
[crafting-solver-plan.md](../../crafting-solver-plan.md); this file owned S7
sequencing, performance measurement, owner-set acceptance, and stop boundaries.

Status 2026-07-16: S7.0-S7.5 are implemented; S7.3 landed ahead of S7.2 and
S7.2 was then completed without redoing the fixed-option checkpoint. The
versioned native/worker-WASM corpus, telemetry contract, unoptimized reports,
and cross-backend comparison tooling are in place. The one-item correctness/state
substrate now includes the approved Eldritch fallback fixture, one-Scour crafted
modifier removal, exact Harvest resistance/Fracture calculation, and
carrier-specific compilation. Fixed Scour/Alchemy, explicit Eldritch side
intent, protected-side operations, and deterministic Multimod finishes now
evaluate as price-independent exact kernels and compile to primitive strategy
subgraphs. Explicit action control, lazy fossil generation, exact-kernel action
collapsing, independent caps, compact states, interned sparse transitions, and
bounded Bellman units now provide the first performance layer. Oliver approved
the permanent corpus and numeric criteria below on 2026-07-15. The fresh
pre-optimization full-corpus command was recorded as a timeout when its native
report had not completed after roughly 49 minutes, and Oliver directed the
phase to proceed to performance work. The S7.2R product repair, S7.4
renewal/observation-aware option checkpoint, and S7.5 deep optimization/cache
checkpoint are complete; S7.6 acceptance is next. Trade
leaves, corruption, Hinekora's Lock,
imprints, recombinators, and other mechanic expansion remain parked in
[solver-mechanic-extensions.md](../../solver-mechanic-extensions.md).

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
- completes after a measured native/worker-WASM optimization pass, reports its
  time and memory use, and remains bounded and cancellable. Oliver set no
  solve-time or memory completion ceiling; S7 should make both as good as
  practical.

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

At the S7.0 baseline, two registered one-item actions were not
calculation-supported: Harvest resistance conversion and Fracture. S7.1 added
both exact evaluators plus primitive one-Scour crafted-modifier removal.
Scour-plus-alchemy remains a later solver option rather than a primitive.

## S7 Invariants

1. **The native engine remains the rule authority.** TypeScript may present
   progress, diagnostics, and prices; it does not infer legality or odds.
2. **Make the result as optimal as practical and expose its status.** Safe
   pruning uses a proof where available; hard cases may use bounds, focused
   expansion, or heuristic prioritization when exhaustive work is impractical.
   Never call an unproven result exact, silently delete an uncertain candidate,
   or hide a reported optimality gap. Oliver evaluates the resulting strategies
   and performance gains; an agent does not turn its own benchmark target into
   acceptance authority.
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
8. **Correctness is accepted through the simulator.** At the end of the full S7
   plan, compile the produced policies into ordinary strategy graphs and run
   them in the native simulator. Exact-evaluator matrices, oracle comparisons,
   and other internal checks are optional diagnostics when fixing a broken
   path, not separate plan acceptance gates.
9. **Testing is plan-level, not phase-level.** Do not run routine native,
   binding, WASM, web, or full-repository suites after intermediate phases. Run
   a narrowly relevant test only when something is broken and it is needed to
   diagnose or fix it; run the appropriate complete suite at S7.6.
10. **Visual review belongs to Oliver.** Do not run rendered browser checks,
   screenshots, or visual UI smoke unless Oliver explicitly asks.
11. **S6 Phase 3 stays skipped.** Ambient Emulator odds are not part of this
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
Oliver-approved native and WASM comparison objectives and safety caps
Oliver-approved simulator run count and statistical tolerance
```

The permanent matrix must contain at least:

1. **Oracle cases:** synthetic and small real one-/two-modifier goals whose
   optimum is hand-computable or exhaustively solvable.
2. **Ordinary real craft:** at least two target modifiers and meaningful choice
   among rolling, adding, removing, benching, and restarting.
3. **Advanced real craft:** three or four target conditions with at least two
   stages, meaningful ordinary-currency recovery/restart choices, and a
   deterministic bench finish. The approved advanced gate does not use
   Eldritch side-intent crafting.
4. **Endgame real craft:** five or six finished conditions from an approved
   clean or fractured start, requiring multiple mechanic families and a
   complete compiled policy.
5. **Stress/refusal cases:** intentionally unreachable goals, missing prices,
   cap exhaustion, cancellation, and actions whose exact evaluator is absent.

Oliver approved the concrete oracle, ordinary, advanced, endgame, and
stress/refusal cases plus the numeric criteria below on 2026-07-15. Test
fixtures may use random simulation seeds; policy/value results remain
deterministic for identical solve inputs.

### Owner-Approved Permanent Corpus

The versioned corpus is
`fixtures/solver-benchmarks/v1/manifest.json`. All artifact-backed permanent
cases are owner-approved and enabled; the synthetic six-slot case remains a
native-unit fixture.

| Tier | Approved start and target | Permitted choices | Current status |
| --- | --- | --- | --- |
| Oracle | Clean ilvl 86 Vaal Regalia to one T1 flat-ES prefix; clean Regalia to both T1 flat ES and T1 fire resistance | Transmute, Alteration, Augment where applicable, Restart | Approved and enabled |
| Ordinary | Empty rare ilvl 86 Vaal Regalia to T1 flat ES, T1 increased ES, and the selected cold-resistance bench finish | Alchemy, Chaos, Exalt, Annul, Scour, selected bench craft, Restart | Approved and enabled |
| Advanced | Empty rare ilvl 86 Vaal Regalia to T1 flat ES, T1 increased ES, T2-or-better fire resistance, and the selected cold-resistance bench finish | Chaos, Exalt, Annul, selected bench craft, Restart | Approved and enabled; no Eldritch side-intent craft |
| Endgame | Rare ilvl 86 Vaal Regalia with fractured T1 flat ES to T1 increased ES, T1 ES/stun recovery, T1 fire and cold resistance, and the selected Intelligence bench finish | Chaos, Exalt, Annul, Harvest defence reforge, Dense Fossil, selected bench craft, Restart | Approved and enabled |
| Stress/refusal | Missing price, one-state exhaustion, the historical unsupported-Fracture row (now unreachable-goal after S7.1 evaluator support), unreachable Restart-only goal, cancellation, and the S7.0 full 15,604-action registry | Frozen case-specific action/economy envelopes | Approved and enabled |

The approved advanced case uses ordinary currency plus a deterministic bench
finish and deliberately excludes Eldritch side-intent setup/crafting. The
endgame ES/stun goal uses the local T1
`LocalIncreasedEnergyShieldPercentAndStunRecovery6` family. None of the approved
cases currently needs an unsettled metamod ruling.

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

S7.0 records the unoptimized baseline first. Oliver approved the permanent
performance criteria and per-case safety caps after reviewing the proposal.
Solve time and memory have no completion ceiling, and meeting the directional
minimum does not end optimization while material safe gains remain.

### Recorded S7.0 Baseline

Run `powershell -File scripts/benchmark-solver.ps1`. The stable reports are:

- `build/performance/native-solver-s7.0-unoptimized-v1.json`
- `build/performance/wasm-worker-solver-s7.0-unoptimized-v1.json`
- `build/performance/solver-s7.0-unoptimized-v1-comparison.json`

The comparison report recorded 421 cross-backend checks across all eight
enabled cases with zero mismatches. Those fields establish benchmark
comparability for corpus/artifact identity, case status, structural telemetry,
compiled graph counts, and `V(start)` reporting. Compiled-strategy simulator
results are the correctness evidence. Phase time, native/WASM memory
representation, and worker scheduling measurements remain visible but are not
cross-backend equality or performance-acceptance gates.

| Enabled exact oracle | Native solve | Worker/WASM solve | States / rows / transitions | Sweeps | `V(start)` | Compiled nodes / edges / JSON | 10k simulation mean |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| One T1 ES mod | 1.168 ms | 17,605.552 ms | 8 / 12 / 48 | 2,297 | 8.020144334354436 | 10 / 15 / 27,861 B | 8.14047, 100% success |
| Pre-approval any-tier ES + fire resistance | 0.955 ms | 9,234.950 ms | 9 / 20 / 80 | 1,184 | 4.145081724989235 | 12 / 19 / 36,496 B | 4.14427, 100% success |

Oliver replaced the second oracle with T1 flat ES plus T1 fire resistance after
this report was captured. Preserve the row above as historical evidence, but
capture a fresh unoptimized T1/T1 measurement before S7.2 changes solver
performance. That benchmark measurement is not an intermediate test-suite run.

The full-registry refusal records 15,604 candidates, 15,597 evaluator-
supported actions, 11 priced actions, 42 discriminating tags, 122 junk
classes, 14,909 discovered states from one expansion, four state/action rows,
and 14,909 transitions. Its maximum worker slice was 11.822 ms. Across enabled
cases the maximum observed worker slice was below the approved 50 ms worker
budget; cancellation acknowledgement was 8.149 ms against the approved 250 ms
budget. The one-mod
compiled policy's selected solver allocation estimate was 21,213,823 native bytes and
17,344,082 WASM bytes; the different pointer widths make memory directional
within each backend, not an equality check between them.

The baseline exposes the current worker bottleneck without changing it: the
worker yields between individual Bellman sweeps to keep cancellation
responsive. S7.0 made no solver, action-space, value-iteration, or worker-
scheduling optimization.

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

1. Preserve Oliver's Eldritch dominance rule: tied/no dominance uses the
   ordinary Exalt/Chaos/Annul behavior. Prefix/suffix intent is a separate
   explicit option that may establish the required dominance and must charge
   for its setup currency.
2. Pin which actions honor, remove, or consume each metamod. The native action
   and calculation paths must agree before protected-crafting options are
   summarized.
3. Add a real remove-crafted-modifiers primitive costing one Scour so blocker/
   no-attack/no-caster cleanup can be represented.
4. Implement exact Harvest resistance conversion and Fracture calculation
   evaluators.
5. Refine abstract state only where required by future decisions:
   fractured goal/junk carrier, crafted goal mask and junk counts, total
   crafted count, and side-specific open/removable facts. Prefer option-local
   refinements over globally widening every solve when equivalence permits.
6. Extend compiler conditions only for distinctions a selected policy actually
   requires, then verify the compiled result through the ordinary simulator at
   the final S7 gate.

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

An exhaustive-oracle mode may remain available as a diagnostic for small
fixtures, but it is not a required phase matrix. Certified pruning needs a
documented proof or bound. Potentially useful actions that are not yet expanded
are deferred by a bound; they are not reported as proven irrelevant. The
plan-level correctness result still comes from running the compiled strategy in
the simulator.

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
instead of cloning their primitive nodes for every state. At the final S7 gate,
compile the raw and compressed policies and run both through the simulator;
that simulator result is the correctness comparison.

Later, after the exact S7 work, add an owner-controlled readability option that
can omit branches or choices with negligible value impact. It must disclose the
discarded value delta and the resulting bounded/heuristic status. This is not
S7.1 work and is not required for the S7 gate.

## Phasing

Each intermediate phase ends with an implementation checkpoint, one local
commit, and a rewritten `HANDOFF.md`. It does not require routine native,
binding, WASM, web, simulator, or full-repository test runs. Run a narrowly
relevant test only if something is broken and the test is needed to diagnose or
fix it. Benchmark runs remain available when needed to measure performance;
they report data and do not give the agent authority to accept its own targets.
Run the complete acceptance suite and compiled-strategy simulator verification
once at S7.6. Stop at each boundary; do not roll the next phase into the same
change. Do not perform rendered or visual UI checking unless Oliver asks.

### S7.0 - Benchmark corpus and solver telemetry

Status: complete in the S7.0 baseline commit.

- Add native and worker/WASM solver benchmark harnesses and report schema.
- Propose the real oracle, ordinary, advanced, endgame, and stress fixtures for
  Oliver approval; pin frozen prices and diagnostic oracle subsets.
- Capture baseline runtime, memory, state/action/transition size, graph size,
  cancellation, and simulation agreement without optimizing solver behavior.
- Record candidate comparison targets and per-case harness guardrails after the
  baseline for Oliver to set, revise, or approve. Those criteria are now
  approved as recorded below. There is no solve-time or memory completion
  ceiling; keep both measurements visible and strive for the fastest practical
  result.

Checkpoint: comparable native/WASM JSON exists for every approved case; repeated
runs report stable structural counts; no optimization claim is made without a
saved baseline. The S7.0 comparison report records 421 checks with zero
mismatches. Owner-approved permanent cases are enabled. Preserve the existing
pre-approval rows, and record the approved T1/T1 oracle plus the newly enabled
real cases before S7.2 performs optimization.

### S7.1 - One-item correctness and state substrate

Status: complete in the S7.1 correctness/state-substrate commit.

- Preserve and fixture the approved tied/no-dominance ordinary-currency
  behavior. No approved corpus case currently needs another metamod ruling.
- Add remove-crafted-modifiers at a cost of one Scour.
- Add Harvest resistance conversion and carrier-exact Fracture evaluators.
- Add only the crafted/fractured/side-specific abstract features required by
  the approved benchmark crafts and compiler.

Checkpoint: the implementation and compiler distinctions required by the
approved crafts are present. Routine evaluator matrices and simulator suites
are deferred to S7.6; if a path breaks during implementation, use only the
narrow test needed to diagnose and fix it.

### S7.2 - Action control, storage, and first performance pass

Status: complete in the S7.2 action-control/storage checkpoint. The required
pre-optimization command was attempted from the clean S7.3 commit and recorded
as an owner-stopped timeout after 3,036 seconds; it never completed the native
report and never started WASM. Existing S7.0 reports remain preserved.

- Build the goal-relevance/dependency analyzer. Keep exhaustive-oracle mode only
  if it materially helps diagnose pruning work.
- Generate fossil loadouts lazily and collapse certified equivalent actions.
- Add complete action-inclusion diagnostics and independent resource caps.
- Store sparse transition rows once, compact state payloads, and chunk Bellman
  work for responsive cancellation.

Checkpoint: the benchmark report captures the reduced-mode structural and
performance measurements plus explicit cap diagnostics. Exhaustive comparisons
may be used to debug a broken pruning path, but are not a routine phase gate.
Use the owner-approved per-case caps and report every cap hit explicitly.

Checkpoint result: explicitly selected cases materialize only requested fossil
loadouts and report their complete envelope/dependency reasons; exact equal
transition kernels collapse with price ties retained; per-solve distributions
are copied into compact interned sparse rows; state payloads no longer allocate
four junk-count vectors; discovered/expanded state, row, transition,
reforge-work, solver-byte, and compiled-output caps report their names; and
Bellman work is bounded by row/transition units with adaptive worker scheduling.
The final one-mod and ordinary native/WASM reports agree on statuses, structural
counts, compiled graphs, values, and 10,000-run outcomes. Cycle-heavy endgame
work remains too slow and is intentionally left for S7.5.

### S7.2R - Product action and price repair

Status: complete in the S7.2R product-action/price-repair commit. Calculator now requests engine-owned,
goal-relevant 1-4 fossil generation so useful loadouts remain available without
passing the full fossil powerset. Remove fake tag-derived Harvest actions and
share one owner-approved real-craft allowlist with economy recipe keys. For a
real action with no market quote, resolve price in this order: explicit
per-action override, certified derived recipe, then an explicit user-set
non-zero fallback; disclose and pin the source, and never price invalid actions
or silently treat a missing price as free.

Checkpoint result: the displayed and solved action envelopes agree, a focused
multi-fossil craft retains its useful loadout without registry explosion, fake
Harvest crafts are absent, and genuinely unquoted real actions use the visible
fallback policy or remain explicitly excluded. The follow-up product envelope
is applied natively before abstract-layout construction: the Calculator keeps
its exhaustive odds catalog, Solve retains direct goal mechanics, and missing
prices remove only their individual actions. Standalone metamod and
crafted-mod cleanup primitives remain in the exhaustive catalog; Fracture and
side locks enter product planning only through the bounded S7.4 options.
Pinned Mirage
replays converged through 13 priced actions with five missing prices for a
one-slot goal (49 states) and three missing prices for the reported two-prefix
goal (109 states). Incomplete solves are not compiled and the internal infinity
sentinel is not presented as an expected cost.

### S7.3 - Fixed solver options

Status: complete in the out-of-sequence S7.3 fixed-option commit. S7.2,
S7.2R, S7.4, and S7.5 are also complete; S7.6 is the next boundary.

- Land the planner-operator/kernel contract and tagged policy actions.
- Compile options into primitive Strategy Board subgraphs.
- Implement scour/Alchemy, explicit Eldritch side-intent, protected-side, and
  deterministic Multimod finishing options.

Checkpoint: selected options expand into ordinary primitive Strategy Board
subgraphs. Their plan-level correctness check is the compiled-strategy
simulator run at S7.6, not a required phase-level kernel matrix.

### S7.4 - Renewal and observation-aware options

Status: complete in the S7.4 renewal/observation-aware option checkpoint.

- Implement fixed-exit repeat options for approved rolling/reforge methods.
- Add protected repeat loops that correctly repay setup costs.
- Preserve observed Unveil choice semantics inside option evaluation.
- Add fracture preparation/retry only after carrier-exact state lands.

Checkpoint: option definitions preserve expressible success, salvage, and brick
exits in the compiled strategy. Simulator verification is deferred to S7.6
unless a broken path needs a narrow diagnostic run.

Checkpoint result: explicit goal-slot exit predicates bound renewal synthesis
to approved rolling/reforge programs and Scour/Alchemy. Only outcomes whose
next-attempt kernels are exact-equal normalize to the option entry self-loop;
all changed-carrier, illegal, salvage, and brick states remain outer exits.
Protected repeats execute the selected side lock on every normalized attempt,
so setup is repaid. Veiled Chaos plus Unveil retains sampled offer groups and
compiles state-local `has_unveil_option` choice routers. Fracture preparation
repeats only while the selected satisfying goal carrier remains exact-equivalent,
then executes primitive Fracture; every correct and wrong fractured carrier is
an outer exit for ordinary recovery/Restart policy. Fractured goal carriers
continue to satisfy their slots. Ordinary influence and fractures are mutually
exclusive in engine and solver legality, while Eldritch implicit tiers remain
compatible and explicit in routing state.

### S7.5 - Deep optimization and cache reuse

Status: complete in the S7.5 cycle-optimization/cache-reuse checkpoint.

- Eliminate algebraic self-loops and benchmark prioritized backups.
- Extract/reuse SCC policy-evaluation machinery and implement policy iteration
  if it wins the pinned matrix.
- Reuse transition caches for price-only solves.
- Add bounded focused expansion only if the preceding work still misses the
  hard-corpus budget.
- Compress policy regions before compilation.

Checkpoint: final native/WASM time, memory, worker-slice, cancellation, and
graph-size reports show the cumulative changes against the applicable
unoptimized baseline. The approved directional minimum is at least 5x
geometric-mean solve speed on native and WASM plus at least 2x peak-memory
reduction where the baseline leaves enough headroom. These are not stop goals:
continue while profiling identifies a material, safe S7 bottleneck, and present
the final gains to Oliver for evaluation.

Checkpoint result: sparse rows remove direct self transitions and solve both
ordinary and observed-choice self-loop contributions algebraically. Residual-
prioritized backups remain the bounded fallback; fixed-policy SCC evaluation
with policy improvement won the pinned matrix and reduced the applicable S7.2
native solve measurements from 0.365/33.051/688.372/12671.034 ms to
0.149/0.183/12.352/55.580 ms for one-mod, two-mod, ordinary, and advanced.
Compatible price-independent transition closures are retained on the solver
handle and repriced without transition-provider work. A bounded optimistic
focused-expansion path is enabled only for large pending closures and reports
its lower/upper bound and proof gap honestly. It still did not close the
endgame fractured corpus within a five-minute diagnostic attempt, so that
case remains the explicit performance risk for S7.6 rather than being reported
as converged. Exact state-independent policy programs are compressed into
canonical regions of at most eight states; the four measured cases compile
6/15/135/402 working states into 2/5/41/133 regions. Native and worker WASM
matched all 83 structural/value/compiled-size checks per case, with worker
slices at or below 18.688 ms. These were solver-only reports with simulator
verification deliberately skipped under the intermediate-phase cadence.

### S7.6 - End-to-end product gate

- Solve every approved real craft from its declared start without hand-authored
  intermediate stages.
- Compile each result into an ordinary editable strategy document.
- Run every compiled strategy through the native simulator exactly 10,000 times
  using the approved per-case statistical tolerance.
- Do not perform workspace/browser visual review unless Oliver explicitly asks;
  deliver the strategies for his visual evaluation instead.
- Run the relevant complete native, binding, WASM, web, and repository
  acceptance suites once here, at the end of the full S7 plan.
- Present actions considered/deferred/pruned, resource caps, optimality status,
  expected materials, simulator results, and the baseline/final native/WASM
  performance comparison for Oliver's evaluation.

Full-plan gate: every required craft compiles without a vocabulary gap and its
compiled strategy runs in the simulator without an unmatched/off-policy route.
Record empirical cost and success behavior against the solver forecast using
the approved per-case tolerance. The complete automated suite is green, and the
reported performance meets the criteria Oliver set or approved. Visual UI
acceptance is not part of the agent gate unless Oliver asks for it.

## End-Of-Plan Acceptance And Owner-Set Criteria

The S7 correctness gate is deliberately narrow:

- every approved real craft produces an ordinary compiled strategy;
- the native simulator can run that compiled strategy without an unmatched or
  off-policy route;
- the report records empirical cost and success behavior against the solver
  forecast using exactly 10,000 runs and the approved per-case tolerance.

Cross-backend structural counts, exhaustive-oracle comparisons, exact option
kernel matrices, and raw-versus-compressed exact evaluation remain useful
diagnostics. They are not additional correctness gates and are not run
routinely after intermediate phases. Use one only when a broken path requires
it, or as part of the complete final suite when already covered there.

Oliver approved the following values on 2026-07-15:

| Decision | Approved criterion |
| --- | --- |
| Required native and WASM performance gain | At least 5x geometric-mean solve speed; continue optimizing beyond it while material safe gains remain |
| Required peak-memory change | At least 2x reduction where the baseline leaves enough headroom |
| Worker-step and cancellation responsiveness budgets | 50 ms worker step; 250 ms cancellation acknowledgement |
| State, row, transition, byte, reforge, and compiled-graph caps | The per-case values stored in the approved fixture corpus |
| Simulator repetition count and statistical tolerance | Exactly 10,000 runs for every compiled-strategy verification; retain the existing per-case tolerances |

Future reports compare each backend to its applicable unoptimized baseline:
S7.0 for unchanged cases, and a fresh pre-S7.2 measurement for the approved
T1/T1 oracle and newly enabled real cases. Preserve every baseline rather than
rewriting it around later results. Native/WASM solve time, memory, worker
scheduling, cancellation, and graph size are always reported. Oliver evaluates
the gains and decides whether S7 performance is accepted.

## Recorded Owner Decisions And Remaining Approval

Recorded 2026-07-15:

1. Tied/no Eldritch dominance acts as the corresponding ordinary currency.
   Prefix/suffix intent remains an explicit setup-and-craft option.
2. Remove-crafted-modifiers costs one Scour.
3. Make the solver as optimal as practical and report exact/bounded/heuristic
   status honestly; Oliver will evaluate strategy quality and performance
   gains.
4. There is no owner-set solve-time or memory completion ceiling. Optimize both
   as far as practical, keep the measurements visible, and do not stop merely
   because the 5x/2x directional minimum was reached.
5. The approved responsiveness budgets are 50 ms worker steps and 250 ms
   cancellation acknowledgement. The approved operational/safety limits are
   the per-case caps in the permanent corpus.
6. Correctness is accepted by compiling the resulting strategy and running it
   through the native simulator exactly 10,000 times at the end of the full S7
   plan, using the existing per-case statistical tolerances.
7. Routine tests do not run at intermediate phase boundaries. Run a narrow test
   only to diagnose/fix breakage, then run the complete suite once at S7.6.
8. Oliver performs visual UI review unless he explicitly asks the agent to do
   it.
9. A later owner-controlled readability pass may trim choices with negligible
   value impact if it reports the discarded delta and honest optimality status;
   it is not S7.1 work or an S7 completion requirement.

The permanent corpus, numeric criteria, and manifest-backed real Harvest
allowlist are implemented and owner-authoritative. S7.6 is next. Ask Oliver
only if implementation reveals a new mechanic ambiguity.

Mechanic answers come from Oliver and are written into focused fixtures before
implementation. Agents must not research or guess them.

## Stop Boundary

S7 ends after realistic one-item crafts meet the complete performance and
verification gate. Do not begin the parked mechanic track, live economy UI
integration, publishing/accounts, recombinators, or ML work as part of S7.
