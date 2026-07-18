# Project Direction

**Status: current orientation and sequencing summary.** `HANDOFF.md` remains
the sole exact next-work pointer.

The complete subject/lifecycle index is [docs/README.md](README.md).

One-page orientation: what poecraft2 is, where it stands, and where it is
going. Details live in the linked docs; this page only sets the direction.

## Vision

A fast, trustworthy Path of Exile 1 crafting simulator that runs entirely
client-side, plus a solver that turns a goal item into the cheapest expected-cost
crafting strategy. The same native engine powers manual crafting (Emulator),
strategy simulation (Simulator), exact odds and graph evaluation (Calculator),
optimal planning (the DP solver), and eventually recombinator and ML tooling.

```text
engine correctness -> simulator -> calculator/solver -> recombinators
                                              -> ML on top of the solver
```

The solver is dynamic programming over engine-owned transitions, never a black
box. Generated policies compile to ordinary editable strategy graphs, carry
expected-cost annotations and action/material accounting, and verify through
the same native simulator.

## Where Things Stand

Engine phases 0-11, 13, and 14 are complete: canonical ingest and compiled
data, the native C++ engine with Python/WASM bindings, one-item mechanic
coverage through the Phase 13 expansion, the compiled strategy simulator, and
the Dockview workspace with Emulator, Calculator, Strategy Builder/Simulator,
and Stash. Phase 12 accounts remains deferred.

Solver S1-S7 is complete. S1-S6 delivered the exact calculation/solve/compile/
simulate vertical slice. S7 added the approved real craft corpus, exact
Fracture and Harvest support, action-envelope diagnostics and lazy Fossils,
fixed and renewal options, compact sparse transitions, SCC policy iteration,
focused expansion, cache reuse, policy compression, deterministic native/WASM
parity, responsive worker execution, exact whole-graph evaluation, and much
faster compiled simulation. Final native/WASM reports agreed across the corpus
and the complete automated suite passed.

Oliver closed S7 on 2026-07-17 and directed work to move forward. The final
endgame simulator sample remains recorded at `0.9942` success against the
former `0.995` target; the miss was not relabelled as a numeric pass and no
replacement sample was run. The completed plan and evidence are preserved in
[the S7 archive](archive/2026-07-solver-s7/).

The active product chunk is:

```text
B1  owner-selected Bestiary expansion
S8  one-item solver capability, exact accounting, review, and trimming
then recombinators
```

The authoritative execution plan is
[bestiary-and-solver-capability-plan.md](active/bestiary-and-solver-capability-plan.md).
B1.0-B1.4 are complete: Oliver selected Imprint, approved its exact
checkpoint/restore contract, and parked both prefix/suffix conversion recipes.
Oliver waived B1.5 as a separate checkpoint based on focused B1.3/B1.4
validation; it is waived/deferred, not complete, and its full acceptance,
10,000-run Imprint verification, and rendered review were not backfilled. S8.0
now versions the exact solver before-state and the review, accounting, and
trimming contracts. S8.1 derived review sections is the immediate boundary.
Mechanic rules are never researched or inferred by agents.

The parallel economy track is implemented: canonical ingest/publishing,
immutable league snapshots, browser cache and selector, per-league overrides,
and pinned cost identities are present. Production activation only needs the
documented R2 resources and repository secrets. Imprint and beast price
identities and mappings are present from B1.

## Direction Of Travel

1. **B1 Bestiary expansion.** Pin the selected recipes, then carry them through
   canonical data/prices, native actions, exact calculation, strategy
   execution, solver descriptors where appropriate, Python/WASM, and the shared
   product action surfaces. Stateful or multi-output recipes receive an exact
   selected representation or remain explicitly unsupported.
[bestiary-and-solver-capability-plan.md](active/bestiary-and-solver-capability-plan.md)
2. **S8 practical one-item solver capability.** Keep minimum expected cost as
   the sole objective while making the action space carrier-aware, considering
   relevant Fracture/bench/metamod routes automatically, extending exact
   action/material accounting, deriving a compact review projection, and
   offering optional simulator-informed trimming with disclosed impact.
[bestiary-and-solver-capability-plan.md](active/bestiary-and-solver-capability-plan.md)
3. **Recombinators after S8.** First add exact two-item outcomes and hand-authored
   recomb/feeder item-flow graphs, then automatic spec-pyramid planning. The
   minimum trade-leaf support needed to value feeders belongs with this work,
   not in a preceding M1 bundle. Hinekora's Lock, corruption/tainted currency,
   and finishing-cost items remain independently parked.
   [solver-mechanic-extensions.md](future/solver-mechanic-extensions.md)
4. **Workspace fluency remains later or parallel.** Remaining product work
   includes craft history, richer materials/shopping views, aggregate board
   overlays, and eventually recomb/feeder blocks. Ambient watched-mod odds in
   Emulator were skipped entirely.
   [desktop-workspace-ui.md](product/desktop-workspace-ui.md),
   [strategy-editor-ui.md](product/strategy-editor-ui.md),
   [economy-ingest-plan.md](economy/economy-ingest-plan.md)
5. **Infrastructure and ML remain later.** Economy production activation is an
   external operational step. Accounts Phase 12 is deferred, so publishing
   Phases 15-16 remain blocked. ML stays last, using the exact solver's logged
   state/value/action corpus as ground truth.

## Doc Map

Foundations:
[architecture-plan.md](foundation/architecture-plan.md),
[codebase-structure.md](foundation/codebase-structure.md),
[implementation-plan.md](implementation-plan.md)

Data and engine internals:
[data-shapes-and-ingest.md](engine/data-shapes-and-ingest.md),
[economy-ingest-plan.md](economy/economy-ingest-plan.md),
[mod-data-and-pool-semantics.md](engine/mod-data-and-pool-semantics.md),
[weight-calculation-flow.md](engine/weight-calculation-flow.md),
[item-state-flow.md](engine/item-state-flow.md),
[engine-bitsets.md](engine/engine-bitsets.md)

Solver and active execution:
[crafting-solver-plan.md](solver/crafting-solver-plan.md),
[bestiary-and-solver-capability-plan.md](active/bestiary-and-solver-capability-plan.md),
[solver-mechanic-extensions.md](future/solver-mechanic-extensions.md),
[ml-strategy-planning.md](future/ml-strategy-planning.md)

UI and workspace:
[desktop-workspace-ui.md](product/desktop-workspace-ui.md),
[strategy-editor-ui.md](product/strategy-editor-ui.md)

Community (later):
[accounts-publishing-and-discovery.md](future/accounts-publishing-and-discovery.md)

Completed execution plans and historical evidence:
[archive/README.md](archive/README.md)
