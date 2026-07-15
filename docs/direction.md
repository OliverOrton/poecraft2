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

Engine phases 0-13 are complete: data pipeline, native C++ engine with
Python and WASM bindings, compact item state and weighted pools, the
compiled strategy graph simulator, the Dockview workspace with Emulator,
Strategy Builder, Simulator, and Stash, and the Phase 13 mechanic
expansion (bench, metamods, veiled, harvest, eldritch, influenced exalts).
See [implementation-plan.md](implementation-plan.md) for phase detail and
the current next task. Before S6 Phase 1, a short product-polish interlude is
scheduled for base ordering, graph auto-labels, and making the Calculator's one
goal look and edit like its input item. The Searing/Eater currency migration is
skipped for now, and the earlier multi-goal/OR expansion is no longer part of
this interlude. The detailed sequence is in
[pre-s6-product-polish-plan.md](pre-s6-product-polish-plan.md).

## Direction Of Travel

1. Calculation engine and DP solver (solver phases S1-S6): exact
   action-outcome distributions, a Calculator workspace tab, value
   iteration over goal-derived abstract states, policy-to-strategy-graph
   compilation with an end-to-end verification gate.
   [crafting-solver-plan.md](crafting-solver-plan.md)
2. Solver mechanic extensions (S7-S11): trade-leaf buy actions,
   corruption endgames, Hinekora's Lock, beast imprint macro-actions,
   and recombinators as spec pyramids with an auto-planner.
   [solver-mechanic-extensions.md](solver-mechanic-extensions.md)
3. Workspace fluency: watched-mod odds in the Emulator, craft history
   tree, cost distributions and materials/shopping lists, aggregate
   board overlays, a live economy service (league snapshot fetch with
   overrides), and recomb/feeder blocks with item-flow wiring.
   [desktop-workspace-ui.md](desktop-workspace-ui.md),
   [strategy-editor-ui.md](strategy-editor-ui.md)
4. Engine track continues in parallel: performance baselining
   (Phase 14), then publishing/accounts (Phases 12, 15-16) and the
   recombinator engine substrate (Phase 18, consumed by S10-S11).
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
[mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md),
[weight-calculation-flow.md](weight-calculation-flow.md),
[item-state-flow.md](item-state-flow.md),
[engine-bitsets.md](engine-bitsets.md)

Solver:
[crafting-solver-plan.md](crafting-solver-plan.md),
[solver-mechanic-extensions.md](solver-mechanic-extensions.md),
[pre-s6-product-polish-plan.md](pre-s6-product-polish-plan.md)

UI and workspace:
[desktop-workspace-ui.md](desktop-workspace-ui.md),
[strategy-editor-ui.md](strategy-editor-ui.md)

Community (later):
[accounts-publishing-and-discovery.md](accounts-publishing-and-discovery.md)
