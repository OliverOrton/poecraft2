# Successor Gate 3 - Compact Pair Index And Transition-Cap Stop

**Status: representation improvement passed; plan stopped at the next exact
count boundary.**

This evidence was produced natively on 2026-08-16. All declared solver and
evaluator caps remained unchanged. Release WASM, later behavior gates, and the
full repository acceptance pipeline were not run.

## Selected Representation Change

The raw-pair discovery index was an ordered map whose node duplicated the full
four-word `(node, state, unveil_offer, checkpoint_state)` identity plus tree
links for every raw pair. Its only authority was collision-safe lookup while
the reachable graph was still open.

Gate 3 replaces that map with one compact chained index:

- bucket heads and one next link are the only retained index payload;
- complete keys remain in the authoritative `EvalPair` vector;
- every hash hit is checked by full four-word equality, so collisions cannot
  merge identities;
- a bounded average chain avoids a late full-table expansion near this
  witness; and
- after a cap check records the true closed-discovery peak, the pair and
  shared-row discovery indexes are retired before partition work.

No probability, transition, pair identity, cap, or partition rule changed.
The evaluator now reports the discovery-index peak independently.

## Focused Proof

The destructive refinement oracle still discovers 76 raw pairs, refines to 57
classes, and matches the raw forward evaluator on terminal probabilities,
expected actions, every operation consumption, and every edge traversal. Its
compact index is smaller than the prior ordered-map accounting. Single-item
and 1,024-item cooperative stepping produce the same result and index peak.

A cap set exactly to the accounted compact discovery peak reaches pair
refinement; discovery-only index storage can no longer cause the next phase to
be mislabeled. The focused evaluator suite passed 16,822 checks and the solver
suite passed 96,113 checks.

## Frozen Witnesses

Witness A remains unchanged after the representation change:

- evaluated upper 624,800.9519118543;
- 184 nodes / 666 edges;
- success 1 / off-policy mass 0;
- paired certification/product defaults remain sound; and
- largest native step 232.95 ms.

Witness B clears the old byte boundary. At the larger newly reached carrier:

| Evidence | Result |
| --- | ---: |
| raw pairs discovered | 9,998,209 |
| retained transitions at stop | 10,000,000 limit reached |
| compact discovery-index peak | 75,497,472 bytes |
| evaluator owned / peak | 935,469,660 / 938,125,764 bytes |
| evaluator byte cap | 1,050,981,791 bytes |
| pair discovery / interning active time | 6,843.55 / 4,610.33 ms |
| pair refinement | not reached |
| stable classification | `exact_eval_pair_discovery_transition_cap` |
| owning cap | `max_transitions` |
| published upper | 37,279,857.73995944 |
| largest native step | 2,799.90 ms |

The stale generic memory classification was corrected: `max_transitions` now
publishes as `exact_eval_pair_discovery_transition_cap`, while an actual
`max_pairs` partition stop has its own
`exact_eval_pair_refinement_class_cap` classification.

## Stop Decision

The selected smallest representation change succeeded: memory is now about
112.9 MB below the byte cap even after discovery grows by another 1.60 million
pairs. It exposes a distinct exact count boundary before the quotient can run.

The 10,000,000 value is a configurable resource-policy guard, not a semantic
or mathematical limit. It is the documented engine default and is explicitly
pinned by this fixture; other checked cases use different values, and this
case's separate external evaluator permits 20,000,000. It remains meaningful
as a bound on retained work and memory. With only about 112.9 MB left at 10
million transitions, raising it to 20 million would not make the current raw
representation fit under the unchanged byte cap.

Continuing now requires either raising the unchanged 10,000,000-transition
limit or a materially broader evaluator architecture that quotients or streams
transitions before the raw graph is fully retained. Both are outside the one
small Gate 3 representation change authorized by the plan. The plan's explicit
stop condition therefore fires.

Gates 4-8 were not started. In particular, the release-WASM cooperative-step
boundary, publication-reason protocol, five-goal permanent corpus coverage,
remaining action semantics, and full acceptance remain open. The priced
five-T1 strategy is not recovered and must not be described as complete.

Raw diagnostic reports are
`build/qualification/successor-gate3-witness-a.json` and
`build/qualification/successor-gate3-witness-b.json`.
