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
performance pass. Current solving still expands a full reachable closure,
scans a large flat action registry, and is proven mainly on toy real goals plus
a synthetic six-slot fixture. Macro/sub-policy operators, safe action control,
cycle acceleration, memory bounds, and real craft gates are specified in
[solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md).
The parallel live-economy track is planned in
[economy-ingest-plan.md](economy-ingest-plan.md). Its main product decisions are
fully recorded; implementation has not started.

## Direction Of Travel

1. S7 solver depth and performance: real craft benchmarks, existing one-item
   correctness gaps, safe action generation/pruning, exact macro/sub-policy
   operators, compact transition storage, cycle-aware optimization, policy
   compression, and native/WASM end-to-end gates.
   [solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md)
2. Parked mechanic track M1-M5 after S7: trade leaves/corruption/finishers,
   Hinekora's Lock, beast imprint, and recombinators as spec pyramids with an
   auto-planner. These are not active work.
   [solver-mechanic-extensions.md](solver-mechanic-extensions.md)
3. Workspace fluency: craft history tree, cost distributions and materials/
   shopping lists, aggregate board overlays, a live economy service (league
   snapshot fetch with overrides), and recomb/feeder blocks with item-flow
   wiring. Ambient watched-mod odds in the Emulator were skipped entirely.
   [desktop-workspace-ui.md](desktop-workspace-ui.md),
   [strategy-editor-ui.md](strategy-editor-ui.md),
   [economy-ingest-plan.md](economy-ingest-plan.md)
4. Product infrastructure remains later or parallel: Economy E0-E4 may proceed
   independently; accounts Phase 12 is deferred, so publishing Phases 15-16
   stay blocked; recombinator engine Phase 18 remains parked for M4-M5.
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
