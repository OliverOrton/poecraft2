# Project Direction

One-page orientation: what poecraft2 is, where it stands, and where it is
going. Details live in the linked docs; this page only sets the direction.

## Vision

A fast, trustworthy Path of Exile 1 crafting simulator that runs entirely
client-side, plus a solver that turns a goal item into an optimal crafting
strategy. The same native engine powers manual crafting (Emulator),
strategy simulation (Simulator), exact odds queries (Calculator), optimal
planning (the DP solver), and — eventually — ML strategy tooling built on
the solver's output.

```text
engine correctness -> simulator -> calculator/solver -> recombinators
                                              -> ML on top of the solver
```

The solver is exact dynamic programming over known transition
probabilities, never a black box: every generated strategy compiles to an
ordinary editable strategy graph, is annotated with expected remaining
cost, and is verified by simulating it in the same engine.

## Where Things Stand

Engine phases 0-11, 13, and 14 are complete: data pipeline, native C++ engine with
Python and WASM bindings, compact item state and weighted pools, the
compiled strategy graph simulator, the Dockview workspace with Emulator,
Calculator, Strategy Builder, and Stash (the Strategy Builder owns the
Simulator runner mode), and the Phase 13 mechanic
expansion (bench, metamods, veiled, harvest, eldritch, influenced exalts),
plus the public-engine throughput pass. Phase 12 accounts remains deferred.
See [implementation-plan.md](implementation-plan.md) for phase detail and
the current next task. Solver S1-S6 is a complete vertical slice: Calculator
can solve into an annotated Strategy Board, long solves report progress and
cancel, veiled/eldritch transitions are exact, policies compile to the ordinary
strategy vocabulary, and the compiled graph is simulation-verified. Oliver
skipped S6 Phase 3 ambient Emulator odds entirely; it is not deferred work.
Completed S6-era execution plans are preserved under [archive](archive/).

The active milestone is S7: make the one-item solver handle realistic,
multi-stage end-to-end crafts and complete a dedicated native/WASM solver
performance pass. S7.0 is complete: versioned native/worker-WASM benchmarks,
read-only solver telemetry, an unoptimized baseline, and structural comparison
reports now quantify the current full-closure/value-iteration behavior. S7.1 is
next: Oliver approved the permanent real craft corpus and the performance,
safety, responsiveness, and simulator-verification criteria on 2026-07-15.
The old any-tier two-mod row remains historical; measure its approved T1/T1
replacement and the newly enabled real cases before S7.2 optimization.
Macro/sub-policy operators, safe action control, cycle acceleration, memory
bounds, and real craft gates are specified in
[solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md).
Benchmarks measure and report gains; Oliver sets or approves the performance
targets, operational caps, responsiveness budgets, and simulation tolerances,
then evaluates whether the gains are sufficient. S7 correctness is accepted by
compiling the produced strategies and running each one exactly 10,000 times in
the native simulator at the end of the complete plan. The approved directional
minimums are 5x solve speed and 2x lower peak memory where feasible, but the
solver should be pushed as fast as practical beyond them. Intermediate S7
phases do not carry routine test or visual-browser gates.
The parallel live-economy track in
[economy-ingest-plan.md](economy-ingest-plan.md) is implemented: the separate
canonical ingest/publisher, immutable league snapshots, browser cache and
selector, per-league overrides, and pinned cost identities are in place.
Production activation only needs the documented R2 resources and repository
secrets.

## Direction Of Travel

1. S7 solver depth and performance: real craft benchmarks, existing one-item
   correctness gaps, safe action generation/pruning, exact macro/sub-policy
   operators, compact transition storage, cycle-aware optimization, policy
   compression, and a final native/WASM benchmark plus compiled-strategy
   simulator gate. S7.0 baseline/telemetry is complete; S7.1 correctness/state
   substrate is the exact next boundary.
   [solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md)
2. Parked mechanic track M1-M5 after S7: trade leaves/corruption/finishers,
   Hinekora's Lock, beast imprint, and recombinators as spec pyramids with an
   auto-planner. These are not active work.
   [solver-mechanic-extensions.md](solver-mechanic-extensions.md)
3. Workspace fluency: the live economy service, league snapshot cache,
   overrides, and pinned cost identities are complete. Remaining work includes
   craft history, cost distributions and materials/shopping lists, aggregate
   board overlays, and recomb/feeder blocks with item-flow wiring. Ambient
   watched-mod odds in the Emulator were skipped entirely.
   [desktop-workspace-ui.md](desktop-workspace-ui.md),
   [strategy-editor-ui.md](strategy-editor-ui.md),
   [economy-ingest-plan.md](economy-ingest-plan.md)
4. Product infrastructure remains later or parallel: economy production
   activation is an external operational step; accounts Phase 12 is deferred,
   so publishing Phases 15-16 stay blocked; recombinator engine Phase 18
   remains parked for M4-M5.
5. ML last, on purpose (Phase 17): value-distillation and search
   guidance trained on the solver's logged (state, value, action)
   corpus, with the exact solver as ground truth.

## Doc Map

Foundations:
[architecture-plan.md](architecture-plan.md),
[codebase-structure.md](codebase-structure.md),
[implementation-plan.md](implementation-plan.md)

Data and engine internals:
[data-shapes-and-ingest.md](data-shapes-and-ingest.md),
[economy-ingest-plan.md](economy-ingest-plan.md),
[mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md),
[weight-calculation-flow.md](weight-calculation-flow.md),
[item-state-flow.md](item-state-flow.md),
[engine-bitsets.md](engine-bitsets.md)

Solver:
[crafting-solver-plan.md](crafting-solver-plan.md),
[solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md),
[solver-mechanic-extensions.md](solver-mechanic-extensions.md),
[ml-strategy-planning.md](ml-strategy-planning.md)

UI and workspace:
[desktop-workspace-ui.md](desktop-workspace-ui.md),
[strategy-editor-ui.md](strategy-editor-ui.md)

Community (later):
[accounts-publishing-and-discovery.md](accounts-publishing-and-discovery.md)

Completed execution plans:
[archive/README.md](archive/README.md)
