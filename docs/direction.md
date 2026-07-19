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
versions the exact solver before-state and the review, accounting, and trimming
contracts. S8.1 now derives deterministic display-only review sections over
those frozen strategies with complete raw graph traceability and no executable
authority. S8.2 corrected Fossil/Essence metamod behavior and added exact
preservation-aware action control without changing the cost objective or raw
strategy authority. S8.3 automatic Fracture, bench, and metamod candidates is
complete: price-independent native generation now admits only legal,
goal-relevant, complete exact kernels; selected options compete by downstream
expected cost and compile into the existing primitive strategy vocabulary.
S8.4 exact action and material accounting is complete: the existing evaluator
occupancy result now reports stable action descriptors, exact material and
native-priced contributions, technique/retry work, optional display-only review
section totals, reconciliation, and explicitly labelled independent-retry
success normalization. Simulator descriptor/material averages remain a
separate sampled evidence source with run count and seed.

A post-S8.4 audit found blocking regressions in the path Calculator actually
uses. The now-repaired R3 drift treated magic as a final-goal restriction
instead of a checkpoint-creation condition. The now-repaired R2 path eagerly
created a large `goal_relevant` fixed-option cross product and widened one
global abstraction for all dependencies; a one-state ordinary product
diagnostic discovered 63,479 states and took about 30 seconds. The now-repaired
R1 path retained unbounded per-row diagnostics, omitted finalized automatic
witnesses from its byte estimate, and checked caps before finalization. The
real S8.3/S8.4 comparisons disabled automatic candidates, so they did not cover
this product path. Large strategy
serialization, repeated browser copies, retained repricing closure, a 64 MiB
product compile cap below current ordinary/advanced graph sizes, and
non-actionable UI diagnostics compound the problem. Calculator's 5,000-run
“verification” also averages costs across failed/stopped runs without checking
or showing terminal and off-policy counts, so a broken compiled policy can look
artificially cheaper than its solver value.

The detailed findings and acceptance gates are recorded in S8.4R of the active
plan. Oliver has pinned automatic, state-local solver discovery of useful
Imprint stages with no user-authored retry program. For browser memory, S8.4R
will release the solved native handle and transition closure after strategy
transfer and rebuild on repricing; retained-cache mode stays out until live-byte
telemetry can enforce a product budget. S8.4R is split into R1 diagnostic/cap
ownership, R2 state-local automatic generation, R3 automatic Imprint discovery,
R3F primitive Fracture product planning,
R3A carrier-relative automatic-kernel scaling/product usability, R4 browser
transfer/lifetime, R5 verification truth, and R6 integrated acceptance. R1 is
complete: its exact Conquest Lamellar/Mirage case is pinned,
diagnostic retention and final output are bounded, evaluator byte caps are in
place, and native/release-WASM selected live-memory telemetry is available.
R2's eager-construction repair is complete: automatic Fracture, bench/blocker,
protected-metamod, and Multimod candidates are now discovered from the current
carrier, rejected or exact-kernel/resource-deduplicated within its transient
exact local context, and only then admitted with their minimal structural
dependencies. Product construction no longer widens the shared layout for the
eager global cross product; the
pinned and ordinary/advanced bounded cases now construct 17/7/5 candidates
with 13-14 junk classes instead of 1,785/1,318/1,773 candidates with 44-45.
R3 is complete: Imprint is now automatic and state-local. Native discovery
runs only at reachable carriers where checkpoint creation is legal, constructs
bounded goal-relevant exact attempt programs, derives useful intermediate
exits, and passes complete attempt/restore kernels through the R2 admission and
deduplication path before adding minimal dependencies. Magic is a checkpoint
creation condition, not a final-goal restriction; exits continue from their
actual successor through ordinary Bellman values, while non-exits restore and
retry exactly. Authored Imprint programs/exits are gone from the product
contract, and depth/work exhaustion is reported as a solver resource boundary.
The focused rare-final compiled fixture passed 64 deterministic runs; its
required 10,000-run verification remains deferred to R6. Release WASM was
rebuilt for the corrected C ABI metadata.

A post-R3 bounded product diagnostic then isolated a separate retained-kernel
scaling defect on the otherwise-correct R2 substrate. The pinned Conquest case
expanded 223 states, admitted exactly 223 fixed options, retained 2,891 rows and
9,168,904 transitions, reached about 433 MB of selected owned data, and never
entered Bellman optimization. Primitive reforge sharing was already effective
(1,992 hits from 2,001 requests, only 9 builds). Fracture preparation is
discovered correctly per carrier, but complete-kernel comparison is confined to
that carrier's transient batch and absolute entry/retry state IDs make the
mapped closure unique. Oliver's fracture-usage direction (2026-07-18) resolves
the Fracture half structurally: fracturing is an early-craft technique —
prepare a cheap carrier, often with several goal mods to cut the cost of
missing — so R3F makes goal-relevant primitive Fracture an ordinary selectable
product candidate, removes product-path preparation closures, and requires a
priced `base` so miss recovery routes through Restart. That implementation,
focused `23.75` boundary evidence, Calculator warning, and release-WASM rebuild
are complete. The pinned normal-cap Conquest attempts were stopped at Oliver's
direction while still expanding, and on 2026-07-19 Oliver transferred the
normal-cap Bellman-entry requirement to R3A: the remaining boundary is
expansion throughput and state-space size, which are R3A's scaling levers, so
R3F is closed on its structural evidence. R3A now normalizes entry-relative
self/retry mass, shares exact transition templates and planner/resource routes
across carriers, emits per-family/per-automatic-kind scaling telemetry,
tightens exact Essence relevance, and proves physical affix/junk ordering does
not split abstract identity. Its final 1,024-state sample bounded
temporary-bench retention at 12 templates/rows per carrier and reduced live
selected bytes to about 22.7 MB, but Oliver stopped the unchanged-cap run
before report emission and Bellman entry was not established. No cap was
  raised. A subsequent incremental selected-byte ledger removed the measured
  quadratic accounting path. Nested protected telemetry then found 13.10
  seconds of retry normalization and 66,693 complete-program fallbacks in a
  16.49-second 2,200-state sample. Exact vector comparison removes those
  fallbacks; an isolated comparison context preserves parent state ordering and
  shares the primitive reforge kernel. The same sample now completes in 2.40
  seconds with identical counts and hashes. A completed 4,096-state diagnostic
  takes 11.55 seconds, uses about 184.13 MB selected memory, and retains at most
  four protected and 12 temporary-bench rows per carrier. Protected kernel and
  outcome-mapping work still consumes 5.87 seconds. The unchanged normal-cap
  request did not reach Bellman in a 30-second hard time box. This does not
  reopen the resolved eager global cross product.
**The measured R3A expansion-time boundary is the immediate and sole boundary;
R4 and S8.5 remain blocked.** The required stop-and-plan point is now the
remaining protected kernel/mapping repetition beyond 4,096 and the 38,613-state
observable-state audit. If the exact full envelope remains unusable, the
fallback is a disclosed mechanic-neutral focused/custom action scope, never a
global optimality claim. Mechanic rules are never researched or inferred by
agents.

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
