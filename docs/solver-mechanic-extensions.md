# Solver Mechanic Extensions

Short companion to [crafting-solver-plan.md](crafting-solver-plan.md). It
covers the mechanics that plan deferred — recombinators above all — and the
smaller gaps not yet in the action registry. Vocabulary defers to
[mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md).

## Recombinators

Mechanics summary (3.25 Settlers rules, exact tables pinned during
implementation): two items of the same item class are consumed and produce
one item. Prefixes from both inputs form one pool, suffixes another; the
result's affix count per side comes from a probability table keyed on pool
size; affixes are then drawn from the pool without same-group duplicates;
the surviving base is chosen between the two inputs; crafted, fractured,
influence, and enchant mods follow special retention rules.

### Keeping It Tractable: Hierarchical Decomposition

The naive model — abstract state = (main item, feeder item) jointly — squares
the state space and is not needed. Feeder production is independent of the
main item, so recombination decomposes:

```text
recombine(main, feeder spec)
  cost      = recombinator cost vector
            + acquisition cost of feeder spec (see below)
  successor = recomb outcome distribution over the pooled affixes
```

The feeder is described by a spec (its goal-relevant mods), not tracked as
live state. Its acquisition cost is either:

1. a market price (trade-leaf action, below), or
2. V_feeder(clean base) from a recursive sub-solve with the feeder spec as
   the goal — the solver calling itself with a smaller goal.

Sub-solve results are cached per feeder spec. The main DP then treats
"recombine with a feeder of spec F" as one action per candidate F, with a
price-independent outcome distribution and a cost that already contains the
expected feeder production cost. Candidate feeder specs are enumerated from
the goal split (subsets of goal mods split across two donor items), pruned
aggressively.

This reuses the plan's existing machinery end to end; the reserved
`companion state` slot in the action descriptor is only needed if a future
technique requires *reacting* to a partially built feeder mid-strategy,
which v1 explicitly does not.

### Engine Work

- Recomb outcome enumerator as a `special` transition kind: pooled-affix
  selection is small and discrete, so exact enumeration is feasible; MC
  cross-check like every other distribution.
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

## Beast Imprint (Checkpoint/Restore)

Imprint copies a magic item; restoring returns to the copy. Rather than
storing a snapshot in state, model the known techniques as macro-actions
with closed-form expected cost, e.g. "imprint, regal, restore-and-repeat
until the regal hits" folds the geometric retry loop into a single
descriptor cost/distribution. Avoids snapshot references in the abstract
state entirely. If a technique ever needs arbitrary checkpointing, revisit
with the companion-state slot; do not build general snapshot state for the
known use cases.

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

## Phasing

Continues the main plan's numbering; S7 is cheap wins first.

```text
S7  trade-leaf actions; corruption/tainted descriptors; finishing-cost
    reporting (blessed/anoint/catalyst)
    gate: solver prefers buying an intermediate when the price table
          makes crafting it dominated

S8  Hinekora's Lock evaluation mode, auto-generated locked variants
    gate: locked-exalt fixture matches hand-computed E[min(...)] values;
          policy flips on a slam decision when lock price crosses the
          analytic break-even

S9  beast imprint macro-actions
    gate: imprint-regal loop cost matches the closed-form geometric
          expectation

S10 recombinators: outcome enumerator + fixtures, feeder spec
    enumeration/pruning, recursive sub-solve with caching,
    Calculator two-item support
    gate: enumerator matches MC and pinned in-game outcome tables;
          end-to-end recomb goal passes the simulate-vs-V(start) check
          with feeder costs included
```
