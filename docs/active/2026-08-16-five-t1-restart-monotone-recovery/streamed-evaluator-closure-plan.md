# Streamed Exact-Evaluator Closure Recovery

**Status: selected by Oliver's 2026-08-17 instruction to continue after
auditing prior attempts. Gate 0 is complete; exact online deterministic
routing is selected for Gate 1.**

Parent: [Five-T1 Exact-Evaluator Scaling And Recovery](successor-plan.md)

## Objective

Make the priced five-natural-T1 candidate reach exact closed partitioning and
independent materialization under the existing one-GiB evaluator budget. Reuse
the retained collision-safe replay partition; reduce the raw evaluator graph
before changing any count cap. Preserve exact probabilities, complete raw pair
identity, fail-closed publication, attribution, and compiled-strategy
semantics.

## Prior-Work Audit

This plan deliberately does not repeat earlier approaches:

1. Policy-guided reconstruct-then-merge is retained only as a small reference
   oracle. Its natural two-goal qualification exceeded one GiB before
   partition initialization, so reconstructing another complete graph is not
   a scalable endpoint.
2. Open-graph early aggregation is unsound. The frozen cyclic witness proves
   that initially identical nodes can require a later split after successor
   closure. No pair or transition may be discarded merely because its current
   observation agrees with another.
3. The proof-carrying quotient milestone already implemented
   `refine_closed_probabilistic_partition_replay`. Its fixed-policy replay and
   collision checks are reusable. Its hard stop came from solver alternative
   candidate breadth before partition initialization, not from the compiled
   strategy evaluator addressed here.
4. The evaluator already contracts deterministic pass-through chains after
   closure and compresses one compiler-generated policy-route root online.
   Moving more contraction before closure must preserve node occupancy, edge
   traversal, route traces, and cycle treatment; it is not assumed safe.
5. Pair-tree replacement, 32-to-24-byte transition compaction, and segmented
   24-byte pair storage are complete. At 18 million transitions the current
   peak is 1,026,151,572 bytes against 1,050,981,759, while discovery remains
   open. Another cap-only probe is not useful.
6. Deterministic checkpoint/replay of whole solver runs remains deferred. It
   would improve iteration time but would not reduce the live proof carrier.

## Invariants

1. No probability truncation, hash-only equality, optimistic pruning,
   representative-state authority, mechanic change, price change, candidate
   scope change, or byte-cap increase.
2. Raw pair identity remains the exact tuple of compiled node, item state,
   checkpoint state, and Unveil offer until a completed split-only partition
   proves a quotient.
3. Transition routing authority must retain exact edge traversal,
   compressed-policy trace and state, deterministic-chain occupancy, terminal
   mass, and operation/material accounting.
4. Use the existing shared closed-partition and exact evaluator authorities.
   Do not create an evaluator-local approximate partition.
5. Witness A, the destructive-cycle raw evaluator, single-step parity, and
   shared-row oracles must remain exact controls.
6. Parent successor Gates 4-8 remain closed until Witness B is independently
   materializable. No full acceptance pipeline runs before the final
   integrated gate.

## Gate 0 - Behavior-Neutral Representation Census

**Complete.** The checked 10-million prefix contains 9,987,873 router pairs
and 10,335 operation pairs. Only 3,965 operation pairs had expanded; no router
pair had expanded. All 9,974,257 retained transition policy states equal their
target-pair states, but only 12,126 transitions use the existing single-root
policy-route compression. See the
[Gate 0 evidence](evidence/streamed-closure-gate0.md).

Add selected-allocation-neutral scalar diagnostics for the stopped discovery
prefix. Record:

- expanded, pending, and total raw pairs by compiled node kind;
- completed deterministic pass-through pairs and shared-row pair reuse;
- exact state count, retained row count, transitions and absorptions;
- transition edge/route presence and whether raw policy state equals the
  retained target pair state; and
- pair, link, row, calculator, and total evaluator bytes.

Pin a focused diagnostic-shape test. Run the native build and focused
evaluator/solver tests only once after the diagnostic is complete, then run
Witness A and the checked 10-million Witness B once. Diagnostics may not alter
values, hashes, cap classification, actions, route defaults, or memory peaks
outside negligible scalar structure size.

## Gate 1 - Select One Pre-Closure Owner

**Active selection: exact online deterministic routing.** Derived transition
routing is not part of this gate. The implementation must retain a
collision-safe exact route trace, report flow on every skipped node and edge,
stop before modifier-offer observation, and leave deterministic cycles raw.

Choose exactly one implementation from Gate 0 evidence:

- **Derived transition routing:** shrink raw transitions to probability,
  target, and exact encoded edge/route authority when policy state is proved
  derivable from the raw target. Allocate a policy-state sidecar only after a
  quotient or rewrite destroys that derivation.
- **Exact online deterministic routing:** skip an additional class of
  non-operation, non-offer deterministic router pairs only if the census shows
  they materially own the frontier. Retain an exact trace authority for every
  skipped node/edge and preserve deterministic cycles.

Do not combine both implementations in this gate. Add a segment/sidecar or
route-trace boundary test plus exact raw/reference value, flow, route, and
accounting parity.

## Gate 2 - Closure Decision

Run Witness A once and Witness B at the checked 10-million cap. Use measured
linear headroom for at most one temporary higher-cap probe. A cap may become
permanent only if raw discovery closes and the subsequent exact phase fits;
otherwise restore 10 million.

Record raw closure count, initial/final partition classes if reached, all
owning bytes, stage time, largest public step, exact cost, success/off-policy
mass, and publication outcome. Stop if discovery still cannot close without a
second broad representation authority.

## Gate 3 - Existing Replay-Partition Density

Run only after raw closure. If the retained graph fits but the existing replay
partition does not, compact that authority rather than introducing another
quotient:

- derive shared row authority on demand instead of retaining per-pair arc
  source caches;
- stream observation/immediate keys from exact pair/node authority;
- release initial assignments once only their count is retained; and
- keep one final class mapping plus the minimum split workspace.

The materialized partition and replay partition must remain byte-for-byte
equivalent on canonical class mappings, projected arcs, lumpability, and
counterexamples in focused tests. Stop if the common partition contract itself
requires a redesign.

## Gate 4 - Rejoin The Scaling Successor

Only after Witness B is independently materializable, resume cooperative
evaluation, truthful publication reasons, permanent five-goal coverage,
remaining action semantics, release-WASM qualification, and final acceptance
from parent successor Gates 4-8.

## Checkpoints

Create coherent local commits with the required co-author line for plan
selection, Gate 0 evidence, the selected Gate 1 representation, the closure
decision, any Gate 3 replay compaction, and final re-entry or stopped handoff.
Do not push unless Oliver asks.
