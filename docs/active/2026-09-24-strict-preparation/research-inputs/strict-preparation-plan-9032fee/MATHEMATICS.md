# Mathematical contracts and counterexamples

These are conditional finite-model/program arguments, not proofs of every native
implementation premise or new novelty claims. [SOURCES.md](SOURCES.md) identifies
existing canonical arguments and the primary background.

## 1. Three equivalences that must not be conflated

A native state-action symmetry preserves a complete transition/reward/goal relation
under a state/action mapping. A policy-value quotient additionally has the required
continuation correspondence. A shared observation computation is weaker: it can
return the same set of required fields for states whose probabilities and optimal
costs differ.

For example, two retry controllers have reward 1 per attempt and success probability
1/5 and 1/100. They require the same observation (“is the target satisfied?”), but
have expected costs 5 and 100. Sharing the requirement does not justify sharing
either cost, transition law, primitive count or chosen controller.

## 2. Exact factoring of a pure preparatory computation

Let a deterministic, valid-domain calculation be

    y = F(d, x) = Apply(Prepare(d), x),

where d is the complete immutable contract/dependency input and x is the actual
state-dependent input. Reusing Prepare(d) is sound if equality of its key implies
equality of every dependency used by Prepare, invalid-domain behavior is preserved,
and Apply still consumes the correct x. Substitution gives the same y as the cold
path. No equality between different x is needed.

If observed state features are part of Prepare in the old implementation, they
cannot be omitted from the key merely because the new function is named “prepare”.
Dependency separation is the proof obligation. A second independent cold result
is a test, not a universal dependency proof.

This is the ordinary specialization/memoization idea [P2], used inside existing
native contracts rather than a universal partial evaluator.

## 3. Grouped synchronous observation propagation

Use a finite observation domain with canonical join ⊔. For node i let D_i be its
initial direct/selected/routing requirements, S_i its successor dependencies and
P_i the actual backward transfer through its selected native program. A schematic
synchronous iteration is

    R_i^0 = D_i
    R_i^(n+1) = R_i^n ⊔ (⊔_{j in S_i} P_i(R_j^n)).

The native helper has richer selectors and ordered paths; the equation explains
the sharing condition, not a replacement mechanics implementation.

If two nodes have equal D, equal complete P and the same successor dependencies,
then R_i^n = R_k^n at every round by induction. Compute their common right-hand side
once, scatter it to both original assignment IDs, and retain the same synchronous
sequence and round-cap result. More general successor renaming requires equality
of their evolving requirements, not merely equal role counts.

Counterexamples: the same operation but different direct/routing requirements;
the same local role mask but a successor that observes a fractured flag instead
of a tier; the same required-feature total with different selected fields. All
can invalidate a shared result.

For deterministic transfer memoization a simpler sufficient key is the full pair
(P_i, R_j^n). Equal pairs have equal outputs even when S_i differs. This requires
no distributivity of P over joins. Do not assume P(X⊔Y)=P(X)⊔P(Y) without checking
native selector/ordered-path semantics.

## 4. Capped prefixes matter, not only the eventual fixed point

Finite monotone dataflow analysis can be computed by worklists [P1], but that does
not prove a new worklist has the same bounded behavior as synchronous rounds.
A two-node cycle with an initial observation at node 0 may need two synchronous
rounds (one changing, one confirming) while an immediate update order reports a
different sequence. Different max-round outcomes are not a transparent refactor.

The selected first treatment preserves synchronous epochs. A frozen-epoch dirty
scheme may skip a node only when its own prior state and all transferred inputs
are unchanged; its output then equals the previous round by determinism. Process
changed dependencies for the next epoch and preserve the completion-confirmation
rule. An asynchronous scheme is a separately justified treatment, not mandatory.

## 5. A prepared requirement is not an observed value

Let C(d) be the canonical required-field set. An observation key has the form

    K(x,p,d) = Encode(p, C(d), Observe(x,C(d))).

Preparing C once preserves K for each actual x,p. Reusing all of K by d alone is
unsound: two states may be required to expose the same field while exposing different
values. Coarse parent identity p is also independent. Such keys can control exact
partitioning and native routing, so the mistake is more than stale telemetry.

Reusing observation work changes no native transition or action cost. A resulting
complete controller still requires its own original-root evaluation, properness,
probability and scope. Restricted or selected-policy evidence is not full-scope
optimality authority.

## 6. Exact inventory updates and ownership transfer

For a container with capacity C and nested payload charges b_i,

    B = C*sizeof(T) + sum_i b_i.

An append with new capacity C' and new payload b has

    B' = B + (C'-C)*sizeof(T) + b.

Replacing/moving an element updates its old/new nested charge at the ownership
boundary. If source and destination overlap in lifetime, both actual allocations
remain charged; shared allocations count once plus each owner's metadata. Storage
release does not refund logical work. A saturated arithmetic sentinel loses
information, so arbitrary subtract-after-saturation is not an exact update rule.

Repeated prefix full scans inspect n(n+1)/2 elements; maintained deltas inspect n
new elements plus explicit reconciliations. This proves an operation-count saving
for that inventory under the stated stable-size assumptions. It does not predict
wall time, prove the inventory includes every allocation, or waive cap preflight.

## 7. Coroutines and evidence-prefix preservation

Write live work W=(K,S,cursor,debit), with K committed native evidence and S staged
scratch. Every optimized step either preserves K or commits a complete result
permitted by the old owner. Induction gives safe evidence after every suspension;
child destruction and rollback preserve the retained original-root artifact.
No incomplete observation fixed point is exposed as ready, no partially built
row is normalized, and no pending operation is recorded as permanently failed
merely because it yielded.

This is a safety argument. Timing, fairness, unchanged numeric ordering and future
policy discovery remain separate tests. A faster stage can change how much useful
work fits before the same deadline even when every completed object is identical.

## 8. Interpreting D1 and future performance

The current reported carrier-discovery time is E(t)-E(session_start), not an
exclusive kernel timer. If A covers initialization, B the selected-locator loop
and C partition-node preparation, measuring B does not identify A+C. Parent and
child times cannot be added. Successive persistent-pass cumulative values cannot
be summed as independent phases.

For one prepared operation with naturally occurring calls b,

    T_old = sum_b [S(d_b)+A(d_b,x_b)]
    T_new = sum_unique_d S(d) + sum_b [Match(d_b)+A(d_b,x_b)] + RetentionCost.

Only the actually bypassed S terms count. If matching scans the same expensive
input as cold construction, there may be no gain. A diagnostic using one coarse
family per key does not upper-bound a future valid common operation across keys;
conversely, combining unlike keys without a guard creates no valid saving.

Existing lower/upper/properness/checker costs remain inside the same aggregate
budget. A percentage reduction of a phase is not the same percentage of the full
solve, and a forced-duration run does not end sooner just because a stage is faster.
Measure first verified U, stronger verified U and Finish-to-usable separately.

## 9. Research disposition

No claim of novel fixed-point theory, partial evaluation, symmetry or memory
accounting is made. The possible contribution is useful exact integration in the
native implicit-SSP pipeline, measured with distinct dataflow, physical-state and
policy-value identities. Broader heterogeneous-role planning remains open, but
this programme does not change its status merely by sharing exact metadata.
