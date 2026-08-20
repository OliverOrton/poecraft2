# Cooperative Exact-Reforge And WASM Graph Provenance

**Status: superseded before implementation by the
[condition-efficient compilation plan](condition-efficient-strategy-compilation-plan.md).**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](README.md)

Predecessor result:
[bounded route-compaction decision](evidence/gated-route-compaction-result.md).

## Objective

Close the remaining browser-responsiveness boundary without changing solver
authority, exact arithmetic, action selection, prices, mechanics, or
publication truth. In parallel, explain the large number of Fracture and
`_fracture_route` nodes seen in the browser by comparing the same request and
compiled-policy role in current native code and the checked release WASM.

The primary implementation target is the measured atomic exact-reforge leaf
beneath cooperative attempt execution. Fracture graph work is diagnostic and
conditional: the existing complete-behavior sharing is retained, and no new
compiler compaction is authorized unless the current native compiler still
emits provably equivalent duplicate behavior for the exact browser artifact.

This plan ends with a release-WASM parity decision. It does not repair the
separate strict coarse-mapping failure, claim an exact optimum, run the priced
primary, run the Warlord/automatic matrix, or run the full acceptance
pipeline.

## Binding Evidence And Prior-Work Audit

1. Current native Witness B materializes a proper bounded executable policy
   at `16226566.773294946` Chaos, success one, zero off-policy mass, 92 nodes,
   338 edges, and three primitive operation regions. Its graph contains 84
   policy routes, one local gated route, four infrastructure nodes, and no
   selected Fracture operation.
2. Exact evaluation closes 35,828 raw pairs to 1,094 refined pairs. The old
   pair-count and memory barriers are no longer the current boundary.
3. Direct certification remains fail-closed as `cost_mismatch` against the
   solver's stored `37279651.842345364`; strict lifting separately stops
   because carrier 5983 maps outside the solved coarse graph. Scheduling work
   cannot promote either result.
4. The largest remaining native public step is 1,254.159 ms in
   compiling/strict lift. Removed owner tracing localized it to the documented
   atomic `CalcContext::outcomes()` call inside cooperative attempt execution.
   Exact evaluator pair discovery has a separate 265.345 ms largest item.
5. `CalcContext::evaluate_reforge()` is one synchronous authority with exact
   V1/V2/V3 branches, deterministic sorted frontiers and candidates,
   long-double recurrence, delayed successor sorting and normalization,
   resource preflights, state interning, and two memo layers. These details
   make a naïve coroutine conversion unsound.
6. `CooperativeTask<T>` and the state-local automatic-admission cursor already
   establish resumable, single-flight, rollback-capable patterns. The current
   attempt coroutine intentionally treats `calc.outcomes()` as an atomic leaf.
7. Product-local Fracture complete-behavior sharing already reduced the frozen
   four-goal native graph from 767 Fracture route/operation pairs to seven
   behaviors, and the later fixed-point compiler sharing is also retained.
   Generic route deduplication, JSON-only equality, and a second Fracture key
   are not new work.
8. The checked release WASM was last rebuilt at `f3b9080`, before the current
   native graph-compaction checkpoints; current source is at least
   `4c418ab`. A browser graph with hundreds of Fracture nodes may therefore be
   stale WASM, a different compiler role, or a different request. It is not
   evidence for another compiler rewrite by itself.
9. Canonical Fracture-Q attribution found 214 eligible rows and no evaluated
   cheaper-Q witness: all final Q values were unresolved because required
   successor values were nonfinite. This plan does not alter Fracture
   admission, prices, preference, or Bellman comparisons.

## Invariants

1. One exact reforge implementation remains authoritative. Synchronous
   callers drain the same resumable cursor used by cooperative callers; no
   second evaluator or approximate browser path is introduced.
2. Pool, family, bucket, exclusion, availability, ordered-frontier,
   predecessor, candidate, recurrence-term, outcome-commit, successor-sort,
   and normalization order remain identical. Exact distribution bits, values,
   hashes, flows, accounting, and failure mass may not drift.
3. Cache keys and lifetime remain unchanged. Cache hits stay synchronous and
   cheap; an incomplete row is never published to either the distribution or
   preserved-base reforge memo.
4. Resource work is charged before the same semantic unit as today. Existing
   work and memory caps, failure classes, and one-GiB envelope remain
   unchanged. Cursor frames and nested retained buffers are fully accounted.
5. Cancellation destroys incomplete work without leaving new public states,
   cache entries, ledger charges, or telemetry that claims completion. Any
   state interning performed while constructing a row must be staged or
   exactly rolled back before a cancellation checkpoint can follow it.
6. Active-time telemetry excludes time suspended back to the caller. RAII
   timers spanning `co_await` are not acceptable; completed and interrupted
   counters must remain truthful.
7. Yield boundaries are semantic work indices, not elapsed-time branches, and
   cannot change evaluation order or results across different resume batch
   sizes.
8. Compiler sharing uses collision-free complete behavior. Operation,
   accounting roles, hit condition, miss/retry continuation, acceptable mask,
   and product/certification default mode must match before Fracture regions
   share. Hashes are only bucket selectors.
9. Native and release WASM receive the same request, economy, action scope,
   solver options, and graph role before their node counts are compared.
10. Certification remains fail closed and the lower bound remains zero until
    an independent proof closes it.

## Gate 0 - Same-Artifact Provenance And Baseline

Freeze the existing five-natural-T1 priced-base request and the exact browser
case that shows excess Fracture nodes. For each artifact, record:

- engine/source version, request and economy identity, options, action scope,
  result kind, publication reason, and whether the graph is product,
  certificate, strict lift, or fallback;
- native and checked-WASM node/edge counts split into infrastructure, master
  policy routes, local gated routes, primitive operations, special recipes,
  Fracture operations, and `_fracture_route` routers;
- selected action IDs and complete Fracture behavior-signature
  multiplicities; and
- result value, bounds, termination, success/off-policy mass, transition and
  policy hashes where available, largest public step, cancellation latency,
  and per-phase active time.

Use the checked release module first; do not rebuild it before capturing the
stale/current comparison. Then run the same request through current native
source. If current native is compact and only the checked WASM is large,
classify the node report as `stale_release_wasm` and make no compiler edit. If
the graphs have different roles or inputs, align them before drawing a
conclusion. If the exact user artifact cannot be reconstructed from the
checked manifest or saved request, stop and request that serialized artifact.

Add behavior-neutral subphase telemetry around the owning exact-reforge row
only as needed to place safe yield points. Separate setup/pool construction,
family and bucket construction, exclusion/availability, frontier construction
and sort, frontier expansion, V3 predecessor/candidate construction,
last-pick recurrence, final successor construction/normalization, and cache
commit. Telemetry must obey the invariants and be removed if it is not useful.

## Gate 1 - Single-Authority Resumable Exact Reforge

Refactor exact reforge evaluation into an owned cursor with explicit phase and
loop indices. Preserve the cache-first synchronous facade. A miss creates one
cursor; synchronous callers drain it, while cooperative attempt execution
resumes it within the existing public work budget.

Place checkpoints only between deterministic semantic units that leave the
cursor internally valid. Preserve allocation preflights and charge work at
the same logical boundary. Stage successor state publication, or introduce a
precise transaction/rollback boundary, so cancellation cannot leak partially
interned state. Commit a complete immutable distribution and its memo entries
atomically at the end.

Account the coroutine frame, vectors, maps, recurrence buffers, and nested
task retention. Replace wall-clock RAII that crosses suspension with explicit
active spans. Cache hits must not allocate a cursor or yield.

## Gate 2 - Exact Parity, Cancellation, And Resource Oracles

Before a real witness, add focused controls that exercise:

- V1, V2, and V3; gated and ungated evaluation; cache hit and miss;
- Chaos, Fossil, Harvest, Essence, Eldritch, and other existing destructive
  reforge families represented by current tests;
- preserved bases, exclusions, fractured modifiers, locked affix sides,
  zero/partial/complete goal progress, and retry-heavy rows;
- deterministic resume schedules ranging from one work unit to synchronous
  drain;
- cancellation at every newly reachable checkpoint; and
- work, memory, reserve, and nested-resource cap failure at phase boundaries.

Compare the old synchronous oracle captured before the refactor with the new
synchronous drain and every cooperative schedule. Require byte-identical
distributions and identical non-time telemetry, state/cache ledgers, failure
class, and hashes. Prove that cancellation publishes no partial row and that a
fresh retry matches a never-cancelled evaluation.

## Gate 3 - Conditional Fracture Graph Decision

This gate is a decision, not an assumed implementation.

If Gate 0 shows only stale WASM, skip compiler changes. If current native still
emits many Fracture nodes for the same graph role, group them by the complete
emitted behavior contract and compare the groups with the existing
`product_fracture_region_key` and fixed-point operation partition. Locate the
specific provenance boundary that bypasses existing sharing.

Only repair that boundary if complete equality is mechanically provable.
Positive tests must collapse identical regions; negative tests must keep
different acceptable masks, restart/miss targets, default modes, accounting
roles, and operation recipes separate. The independently evaluated cost,
success/off-policy mass, transition/policy hashes, and operation accounting
must remain identical. Do not change Fracture candidacy or selection to make a
graph smaller.

If the executable graph changes, run the required 10,000 simulator-run
verification at the final release gate. Pure provenance and scheduling changes
do not require simulation as a substitute for exact bit parity.

## Gate 4 - Native Responsiveness Decision

After implementation, run one native build and the complete focused
calculation, evaluator, compiler, refinement, and solver suites. Then run
Witness A once and Witness B once with independent exact evaluation.

Witness A must remain exact at `624800.9519118543`, success one, zero
off-policy mass, 184 nodes / 666 edges, paired defaults only, transition hash
`284ff325a96fe0d7`, and policy hash `cee2bf6579b1857a`.

Witness B must remain a truthful bounded result at exact compiled-policy cost
`16226566.773294946`, success one, zero off-policy mass, with unchanged action
mix and no new proof authority. If Gate 3 is skipped, its current-native graph
must remain 92 nodes / 338 edges / three operation regions.

Require every public work item in both native witnesses to be at most 250 ms.
If exact-reforge passes but the measured 265.345 ms evaluator pair-discovery
item remains above the boundary, split only its owning deterministic loop and
repeat the exact parity controls. Do not reopen evaluator representation or
numeric-order experiments.

## Gate 5 - Release WASM And Product-Path Parity

Only after Gate 4 passes, rebuild the tracked release WASM through
`scripts/build-wasm.ps1`. Run TypeScript checking and the focused worker/WASM
and Calculator solver tests affected by scheduling, telemetry, serialization,
and graph provenance. Do not perform visual browser review unless Oliver asks.

Run the exact same frozen request natively and through release WASM. Require
matching result truth, values, bounds, termination, selected actions, graph
role and census, complete Fracture behavior multiplicities, hashes where
exposed, and exact compiled-policy evaluation. Require public WASM steps and
cancellation acknowledgement to be at most 250 ms on the qualification
machine. If Gate 3 changed an executable graph, run 10,000 simulator runs and
require the existing verification contract.

Do not run the Warlord/automatic matrix, priced primary, unrelated portfolio,
or `scripts/test.ps1` in this plan.

## Gate 6 - Handoff To Proof Closure

Record the native/WASM result and update `HANDOFF.md`. If responsiveness and
graph provenance close, the next separately selected plan owns strict carrier
5983's coarse mapping failure and the stored-cost mismatch. It must decide
proof closure and lower-bound authority; this plan must not conceal those
issues behind a fast bounded publication.

## Stop Conditions

Stop and write a precise handoff if:

1. any exact probability, distribution, value, non-time telemetry, hash,
   action, cap classification, or graph behavior changes without an explicitly
   authorized representation-only proof;
2. exact state/cache rollback cannot be guaranteed across a checkpoint;
3. cursor retention exceeds the existing resource accounting or one-GiB
   envelope;
4. active-time telemetry cannot distinguish running from suspended time;
5. the current native compiler does not reproduce the reported Fracture graph
   and the exact browser artifact is unavailable;
6. a Fracture merge needs an assumption rather than complete behavior
   equality;
7. either frozen witness retains a public step above 250 ms after its measured
   owner is split; or
8. the strict coarse-mapping or cost-reconciliation defect becomes the
   blocking owner. Record it, but do not repair it in this plan.

## Checkpoints

Create coherent local commits for plan selection/baseline, the resumable
reforge authority and focused oracles, any independently justified Fracture
compiler correction, the bounded native decision, and release-WASM evidence.
End every commit with `Co-authored-by: Codex <codex@openai.com>`. Do not push.
