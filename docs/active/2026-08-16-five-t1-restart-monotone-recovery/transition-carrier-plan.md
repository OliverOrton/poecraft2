# Five-T1 Pre-Closure Transition Carrier Recovery

**Status: Gates 0-1 complete; Gate 2 compact segmented pair-carrier work is
active.**

Parent: [Five-T1 Exact-Evaluator Scaling And Recovery](successor-plan.md)

## Objective

Reach the exact evaluator's closed-partition stage for the priced five-T1
certificate under the existing one-GiB byte budget. Reduce raw transition
retention before treating a higher transition count as useful. Preserve exact
pair identity, double probabilities, fail-closed certification, and every
published strategy semantic.

## Frozen Boundary

- The compact collision-safe pair index cleared the former byte stop.
- With the current representation, 10M, 12M, and 13M transition caps all stop
  during discovery. The 13M run peaks at 1,042,815,196 bytes, only 8,166,595
  below its evaluator budget.
- The transition count is a configurable safety policy, not a mathematical
  limit. A cap-only increase is not useful in the current representation.
- `EvalTransition::via` is always `kNoId` during raw discovery and pair
  refinement. Deterministic pass-through contraction creates it only after
  refinement, yet structure alignment charges eight bytes for it in every raw
  retained transition.

## Invariants

1. Do not reduce probability precision, hash identities without full equality,
   prune reachable pairs, or raise the byte cap.
2. A transition-cap increase may become permanent only after the compact
   representation demonstrates useful additional exact progress within the
   byte budget.
3. Post-contraction `via` authority must remain exact for chain occupancy,
   edge traversal, compressed-policy routing, and attribution.
4. Witness A and all small raw/reference evaluator oracles must remain
   identical in values, terminal mass, edge traversal, and operation
   accounting.
5. Do not start the prior successor's Gates 4-8 until Witness B reaches a new
   precise decision point. Do not run the full acceptance pipeline before the
   final integrated gate.

## Gate 0 - Split Discovery And Post-Contraction Transition Storage

Shrink the raw transition record by moving `via` to a row sidecar allocated
only when pass-through contraction actually rewrites a transition. Reorder the
remaining record so its exact double plus four 32-bit authorities occupy 24
bytes rather than 32. Include sidecar capacity in every fast, audited,
projected, conversion, attribution, and diagnostic memory ledger.

Add focused tests for structure size, no discovery sidecar, contraction with
mixed via/non-via transitions, single-step parity, raw forward-reference
parity, and cap classification.

## Gate 1 - Measured Closure Probe

Run Witness A once. Run the priced witness first at the checked 10M cap to
measure byte savings without changing policy, then use the smallest scoped
cap probe justified by measured headroom. Never cross the byte budget merely
to discover a count.

If raw discovery closes, record raw pairs/transitions, compact payload, initial
and final quotient classes, partition memory, stage timings, and the next exact
result. A permanent cap change is allowed only when it enables closure or a
strictly later exact phase under budget. Otherwise restore the checked cap.

## Gate 2 - Conditional Next Representation

If compact transitions still cannot close the graph, select one next owning
change from evidence: compact pair flags, segmented pair storage that avoids a
capacity-doubling cliff, streamed/shared transition targets, or a sound
pre-closure quotient. Stop if the required change combines multiple broad
architectural authorities without an independently testable intermediate.

**Selected result:** the one allowed 16-million probe still stopped during
raw discovery at 15,998,209 pairs and a 1,011,645,812-byte evaluator peak
against 1,050,981,759 bytes. The checked cap was restored to 10 million.
The next owner is the pair carrier: replace contiguous geometric growth for
raw pairs and discovery links with fixed-size segments, and remove only the
operation/action metadata derivable exactly from the compiled node. Preserve
full four-word raw identity and checked random access. See the
[Gate 1 decision](evidence/transition-carrier-gate1.md).

## Gate 3 - Rejoin The Scaling Successor

Only after Witness B is independently materializable, return to cooperative
evaluation, publication-reason truth, permanent five-goal coverage, remaining
action semantics, release-WASM qualification, and final acceptance in the
parent successor plan.

## Checkpoints

Use coherent local commits with the required co-author line for plan
selection, compact transition storage, the measured decision point, any
conditional next representation, and final re-entry or stop evidence. Do not
push unless Oliver asks.
