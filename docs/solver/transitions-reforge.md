# Transitions And Reforge Work

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page describes row construction and publication. [Policy and program mathematics](mathematics/policies.md) explains observation timing and complete exit mass; [numerical closure](mathematics/numerical-closure.md) distinguishes native probabilities from their stored coefficients.

## Inputs And Outputs

For one interned source and an admitted primitive or planner operator, `CalcContext` supplies a complete successor distribution with any observed-choice sidecar. Transition structure is price-independent. Sparse solve rows attach priced operator variants to the shared kernel later; price changes can still invalidate an earlier pricing or admission proof.

A row includes the semantic action identity, all relevant source observations, successor identities, probabilities, and fixed-program/choice information. A public statement of exact transition calculation does not mean every stored floating coefficient is an exact rational representation of the native law.

Primary owners are `solver_calc.cpp`, `solver_reforge.cpp`, `solver_options.cpp`, `solver_solve_expand.cpp`, and `solver_calc_types.hpp`.

## Exact Reforge Recurrence

The strict destructive-row production path uses the V3 factored terminal recurrence and deterministic canonical accumulation. The coarse V1 path and diagnostic V1/V2 comparisons remain separately identified by the [benchmark contract](benchmarking.md#observation-contract).

Broad row work can suspend during predecessor indexing, expansion, canonicalization, accumulation, and publication. The cursor retains the state needed to continue the same computation; it is not a partial stochastic kernel available for ordinary use.

The synchronous `outcomes()` interface is a completion wrapper for callers that do not schedule the cooperative cursor.

## Rare count-law identity

Ordinary equipment rare reforges use the native shared integer 4/5/6 law,
weights 8/3/1 over 12, approved by Oliver on 2026-10-02. Exact raw, projected
and factored rows use this same mixture, including retained and forced-mod
refill. Fixed-count refill overrides the mixture. Capacity and preserved-count
clamps coalesce probabilities; native pool exhaustion absorbs all remaining
mass. Veiled Chaos clamps the total before subtracting its reserved placeholder.
The shared interface also contains the separately approved configured-cluster
3/4 law (65/35); ordinary/Abyss jewel probabilities are unresolved.

All three ordinary count masses remain positive. For a fixed finite graph and
unchanged conditional kernels, its positive transition support therefore stays
unchanged. Legal graph routes and finite-state properness may be reused as
structure, but its probabilities, expected cost, lower/upper certificates and
proof relations must be reconstructed and checked. Authored graph vocabulary
stays v1. Historical benchmark receipts retain their original law and costs;
new costs are not pure search-improvement comparisons.

Native count-law version 2 binds memo/kernel signatures and lower, quotient and
continuation identities. Action refinement contract 4 and evaluator certificate
version 2 invalidate old assertions/values. Development checkpoint format 5
rejects old-law rows and additionally checks the law version and session kind.
The [execution record](../active/2026-10-02-reforge-law/README.md) owns tests and
remaining native/WASM qualification; this reference alone confers no acceptance.

## Completion is an authority boundary

While suspended, the cursor owns scratch only. It cannot populate the ordinary row cache, seed Bellman values as a completed row, satisfy an action obligation, or certify a proof row. Cancellation discards the unpublished work. Completion installs the immutable distribution transactionally.

A valid lower-only abstraction may represent an unmaterialized action independently, but that is a separately justified relation. It does not make an incomplete native distribution complete. See [CLM-0008](claims.md#clm-0008).

Observed choices retain their native information timing and source observation identity. A fixed policy evaluates its fixed choice rule; a lower model that grants extra choice must justify that relaxation. Neither may silently discard an outcome, retry branch, or mandatory internal program cost.

## Work And Resources

`max_reforge_work` is the V1-equivalent logical work envelope. It is not elapsed time and is not the number of physical V3 operations.

Keep these measurements separate: logical work, raw-equivalent work, projected/factored evaluator effort, active row-build time, row family, resumes/suspensions/cancellations, maximum cooperative slice wall time, and retained cursor bytes. Moving between evaluators does not change the meaning of the logical cap.

The retained recurrence preserves ordering and accumulation across uninterrupted and resumed execution. Native solver builds retain `-ffp-contract=off`; determinism fixtures compare distributions and work identities. This is an implementation reproducibility contract, not a claim that every platform has identical timing.

## Native effects versus relaxed effects

The applied-reforge work distinguishes a native action postcondition from refill success. Legal Alchemy changes rarity even when filling stops early. Minimum occupancy requires additional conditional-history/non-exhaustion evidence. The lower model may preserve underfilled Rare results when that proof is unavailable; it may not infer unsupported Normal rollback from an exhausted pool.

The detailed mathematical distinction is [CLM-0017](claims.md#clm-0017). Do not generalize the Alchemy contract to another action without its own native effect evidence.

## Failure

Inapplicable, unsupported, resource-interrupted, canceled, and invalid are different outcomes. An interrupted row stays absent. Probability, canonical-state, or ownership violations fail before installation.

Cache reuse requires the complete relevant observation identity. A shared action name or source hash without collision-checked semantic equality is insufficient. Selected reforge evaluator controls and versions must also remain visible in comparisons.

The current reforge cache checks the complete preserved-base observation after
locating `(action_index, preserved_base_hash, goal_progress_gated)`. Wiped-only
entry differences therefore already share one completed kernel. The
[cross-binding G0 discovery](../active/2026-09-23-cross-binding/README.md#g0-native-discovery)
found different preserved fractured roles among ordinary A4 misses. Their
coarse Chaos rows use `v1_raw`; equal bucket conflict graphs observed after
construction did not provide a preconstruction guard or a reusable weighted
frontier. No cross-binding template or new cache is retained. The existing
logical work cap and row completion authority are unchanged.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/transitions-reforge.md) and [benchmarking.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md), [2026-09-05-native-applied-reforge-preparation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.

[Scour recovery](../active/2026-09-29-metamod-recovery/README.md) aligns native application, abstract rarity/applicability and refinement version 3 with Oliver's single-lock ruling. Fracture preservation is unconditional; exactly one lock additionally preserves its side. Double-lock native semantics are unchanged and remain open. Nonrenewal add/removal metadata follows native implementation independently; the approved protected-renewal whitelist is unchanged.
