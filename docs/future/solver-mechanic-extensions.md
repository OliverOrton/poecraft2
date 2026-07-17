# Parked Mechanic And Recombinator Extensions

**Status: parked future work.** Bestiary expansion and the focused one-item
solver capability/reviewability pass are active in
[bestiary-and-solver-capability-plan.md](../active/bestiary-and-solver-capability-plan.md).
Do not begin recombinators or the independent mechanic backlog in this file
until Oliver accepts S8 and explicitly selects the next milestone.

This is a future companion to
[crafting-solver-plan.md](../solver/crafting-solver-plan.md). It covers trade leaves,
corruption/finishing work, Hinekora's Lock, and recombinators. Selected
Bestiary mechanics, including any approved imprint work, now belong to the
active B1 plan rather than an M phase here.
Vocabulary defers to
[mod-data-and-pool-semantics.md](../engine/mod-data-and-pool-semantics.md). Mechanic
rules come from Oliver; agents must not research or infer them.

## Recombinators

The text below is an architecture sketch, not an authoritative mechanic
fixture. When this track resumes, Oliver must pin the supported PoE1 variant,
outcome tables, transfer rules, base selection, and crafted/fractured/
influence/enchant behavior before implementation.

Recomb outcomes are exact-mod dependent in ways most mechanics are not:
crafted mods used as pool padding, exclusive-mod retention, and per-side
counts all hinge on mod identity. Recomb math therefore never uses the
junk abstraction — intermediate specs track exact mod multisets. This is
affordable because recomb intermediates are low-mod-count by design; the
abstracted item-level solver is used only *inside* "craft this feeder"
sub-solves, whose goals are exact specs.

### No Main Item: The Spec Pyramid

A recomb project has no main item. It is a pyramid — a DAG of intermediate
item specs converging on the goal, where every node is produced and
consumed. The solver models it at the spec level:

```text
spec       exact mod multiset for an intermediate item (+ base class)

C(spec) = min over productions of expected cost, where a production is:
  buy        trade-leaf price for the spec
  craft      item-level SSP solve with the spec as goal (cached)
  recombine  C(spec A) + C(spec B) + recomb cost vector,
             taken in expectation over the exact outcome distribution,
             where non-goal outcomes are credited at C(outcome spec)
             salvage value instead of written off
```

Because salvage values feed back into production costs, C is a fixed
point over the spec space — the same value-iteration machinery as the
item-level solver, run over an AND/OR spec graph instead of item states.

Auto-planning (v1): candidate specs are enumerated from subsets of goal
mods plus crafted pad mods, pruned by dominance (a spec strictly harder to
produce and no more useful is dropped), then the fixed point is solved.
The chosen productions *are* the pyramid; the solver emits it directly in
the block vocabulary below.

### Strategy Graph Blocks

Two node kinds extend the strategy model:

```text
recomb block   two item inputs, each gated by a condition;
               output edges route by conditions on the result

feeder block   references another strategy document; not re-executed
               during simulation of the parent — it contributes a cached
               summary (average cost vector + output spec distribution)
               computed from the referenced strategy's own solve or
               simulation runs, invalidated when that strategy changes
```

Recycling is graph wiring, not a separate pool system: a recomb output
edge whose condition matches a lower-tier spec wires back into another
recomb block's input, so failed outcomes feed the pyramid instead of
being discarded. The simulator runs the parent strategy as item flow —
a recomb block fires when both inputs are satisfied, drawing either a
live routed item (no new cost) or a feeder summary (cost += summary
average). This keeps the single-item runner semantics inside each
strategy document; multi-item behavior exists only at recomb blocks.

Summary-cost valuation is an expected-cost approximation (it ignores
variance coupling between sub-strategies). The verification gate for
recomb fixtures therefore also runs a nested-execution mode — feeders
actually simulated, not summarized — to bound the approximation error.

Editor UX for these blocks — item ports and wires, live enumerator
badges, recycling gestures, feeder summaries and staleness, the recomb
pair template, lineage tracing, and item-flow validation — is specified
in [strategy-editor-ui.md](../product/strategy-editor-ui.md).

### Engine Work

- Recomb outcome enumerator as a `special` transition kind operating on
  exact mod multisets: pooled-affix selection is small and discrete, so
  exact enumeration is feasible; MC cross-check like every other
  distribution. The mechanic's finicky special cases (crafted retention,
  exclusive mods, base selection) live here and nowhere else, pinned by
  fixtures against in-game outcome tables.
- Session universe must span both donor bases; the bitset plan already
  budgets for this.
- Calculator tab support: pick two items, see the exact result
  distribution. This is also the fixture surface for pinning the outcome
  tables against in-game data.

## Trade-Leaf Actions

"Buy an item matching spec S" as a deterministic acquire action with cost
from the price table. Needed for realistic recombinator strategies (feeders
are usually bought) and generally lets the solver answer "cheaper to buy
the intermediate than craft it." No engine mechanics involved; it is a
registry descriptor plus price-table entries keyed by spec. The restart
action becomes a special case of this.

## Hinekora's Lock (Foresight)

Lock foresees the outcome of the next currency use, and the user may then
decline to apply it. This is observe-then-decide, a new transition
structure rather than a new pool rule:

```text
locked(a) from state s:
  pay lock cost, sample o ~ P(. | s, a)
  then choose: apply (pay cost(a), move to o)  or  decline (stay in s)

V contribution:
  c_lock + E over o [ min( cost(a) + V(o),  V(s) ) ]
```

One new evaluation mode in the solver sweep (an expectation over a min
instead of a plain expectation), reusing the unlocked action's already
cached outcome distribution. Locked variants are auto-generated for
eligible actions rather than hand-registered. Foresight massively changes
optimal play for slam/annul decisions, so this is high value for its size.

## Bestiary Ownership Moved To B1

The earlier draft treated Beast Imprint as one future macro. That is too narrow
to define the selected Bestiary expansion and is no longer sequencing
authority. B1.0 first classifies Oliver's requested recipes as ordinary
one-item, checkpoint/restore, multi-output/multi-item, or unsupported. The
approved contract then decides whether a selected imprint technique uses a
specific fixed option or requires explicit checkpoint state. Do not infer that
choice from this historical sketch.

## Corruption Endgames

Vaal orb and temple double-corrupt as terminal gamble actions: outcome
distributions include brick/no-change/implicit results, and corrupted
states restrict the legal action set to tainted currency. The registry's
legality predicates and state flags already express all of this; the work
is descriptors plus outcome fixtures. Tainted currency descriptors
(legal only when corrupted) land in the same pass.

## Finishing-Cost Items (Outside The DP)

Deterministic or value-only steps that never change a DP decision are
folded into the terminal finishing cost alongside divines:

- blessed orbs / implicit rolls
- anointments (deterministic enchant; a trade-leaf-priced add)
- catalysts and other quality (affects magnitudes, not tiers)

They appear in the reported total cost and the compiled strategy's final
segment, but not in the abstract state.

## Explicitly Out Of Scope

Mirror/reflecting-mist copies, synthesis implicit crafting, and removed
league mechanics (e.g. Necropolis corpse crafting) are not planned.

## Future Sequencing

The former M1-M5 bundle is retired. Trade leaves, corruption/tainted actions,
finishing-cost reporting, and Hinekora's Lock remain independent backlog items;
none is scheduled. The minimum trade-leaf acquisition contract needed to price
recombinator feeders lands with recombinator foundations instead of preceding
them as a general product phase.

```text
R1  minimum feeder acquisition contract; recomb outcome enumerator and
    owner-approved fixtures;
    recomb/feeder blocks in the strategy model; item-flow simulation
    with summary-cost feeders, condition-gated recycling edges, and
    nested-execution verification mode; Calculator two-item support
    gate: enumerator matches MC and owner-approved outcome tables;
          a hand-authored pyramid simulates end to end with recycling;
          summary-mode cost agrees with nested-mode within tolerance;
          feed acquisition chooses craft or buy from pinned costs

R2  pyramid auto-planner: spec enumeration + dominance pruning,
    fixed-point spec-level DP with salvage credits over cached
    craft/buy sub-costs, emit pyramid as recomb/feeder block graph;
    "re-cost" of user-edited pyramids ships alongside as the
    fixed-structure subset of the same solve
    gate: on a pinned recomb goal, the auto-planned pyramid's simulated
          cost matches C(goal spec) within tolerance and does not lose
          to a hand-authored reference pyramid
```

Possible independent later milestones retain their existing design notes above:

- Hinekora's Lock observe-then-decide evaluation;
- corruption and tainted-currency endgames; and
- blessed/anoint/catalyst finishing-cost reporting.

They do not block R1/R2 unless Oliver explicitly makes one a prerequisite.
