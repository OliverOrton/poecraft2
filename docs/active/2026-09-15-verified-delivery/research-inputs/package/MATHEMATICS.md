# Mathematical research: verified delivery and continuation interfaces

**Status:** conditional mathematics and research design, not native implementation or new certificate authority. The supplied audit is preserved as U2; this document independently derives its main formulas, adds partial-alignment attribution and sharpens scope/implementation premises. Sources and prior-art limits are in [SOURCES.md](SOURCES.md). New tiny exact-rational checks are in `checks/`.

## 1. Common model and authority

Fix a crafting target θ: native mechanics/data, base/item level, exact initial item, required clean terminal predicate, permitted primitives/programmes, observation and controller memory, and original prices. Fix a finite deterministic controller π after all its observation choices are resolved at their legitimate points.

Let N be a represented semantic domain containing every nonterminal state reachable from each queried entry. A state is an item **and** control context, not only a goal mask. Let Q≥0 be the matrix of nonterminal transitions, c≥0 the expected immediate monetary reward, and n≥0 the expected number of primitives in a represented step. Let g_j be immediate exit probability to terminal category j. Categories include clean goal and, where present, failure/refusal/unresolved outcomes. Do not combine them into success.

For a completely supported proper controller on this finite domain, Q is transient, ρ(Q)<1 and

\[
 Z=(I-Q)^{-1}=\sum_{k\ge0}Q^k,\qquad V=Zc,\qquad N_{\rm act}=Zn.
\]

Expected visits from s are \(d_s^T=e_s^TZ\), and \(V(s)=d_s^Tc\). Almost-sure *absorption* in some terminal is not enough for a proper crafting policy if that terminal can be failure. Goal absorption must meet the existing native contract.

All matrix inverses below are notation. Native implementations should solve the required sparse systems and charge fill/storage; the tiny Fraction reference code may construct dense inverses. If an old policy is proper only from its root, an unreachable trap cannot be included casually in a purported globally invertible domain. Newly reached candidate states need real old continuation support before an old/new common-domain formula is applicable. [R10, R11]

## 2. Safe interruption selects an owned witness

Let E_t be the conceptual history of compatible complete proper root-policy certificates committed by finite solver-computation time t. For true mathematical values,

\[
 U_t=\min_{\pi\in E_t}V_\pi(s_0),\qquad V_\theta^*(s_0)\le U_t.
\]

Every candidate is a feasible member of the same optimization problem. Therefore selecting a retained witness at an arbitrary finite computation stop preserves the inequality pointwise. No optional-stopping theorem, admissible search heuristic or completed optimality proof is required.

The invariant is **graph + root/context + complete certificate + evaluated value**, not a scalar minimum. A bounded portfolio may discard old witnesses only while retaining a compatible no-worse executable one according to its actual numerical selection contract. A stale cost-7 scalar cannot describe a retained cost-10 graph. Finishing computation does not cap the future crafting execution of that graph.

Define a finite result-selection point in the existing publication owner. Before sealing, collect compatible publication-eligible evidence whose verification has already completed, including complete strict/private artifacts awaiting transfer. Unfinished proposals do not delay the fallback. Seal the tuple atomically in the logical sense; subsequent packaging cannot change graph and cost independently.

A run ID is separate from θ and from artifact identity. Duplicate finish is idempotent; a late message for an earlier run cannot control a newer run with the same target or reused handle. Cancellation and terminal commitment need one declared precedence. A delivered exact answer is not downgraded by a stale Finish click.

This is safety, not a universal latency theorem. The current numerical contract may be complete-model evaluation with named floating-point reconciliation, rather than rigorously rounded native endpoints. This argument does not promote its decimals to a new enclosure proof.

## 3. Cooperative initialization and work refinement

Write a concrete continuation state as W=(K, cursor, scratch, debit), where K is committed evidence. Let α(W)=K ignore unfinished scratch. A sufficient refinement condition for finer suspension is:

1. Each fine step changes only scratch/cursor, or commits one complete result permitted by the original logical owner.
2. Consumers cannot observe staged rows, unverified lower vectors or half-built artifact/certificate pairs as committed evidence.
3. Interruption destroys staged work through its real rollback owner; cumulative logical debit survives yields and is not refunded by memory release.
4. The owning handle, goal/economy/context and eventual publication target remain fixed.

Induction over fine steps preserves K's invariants. This covers initialization as well as later candidate/evaluator work. Begin may validate cheaply and return a handle owning a staged setup continuation; expensive preparation can publish a lower/model only after its existing complete check. Until then, the ordinary lower must remain a previously valid one, such as the existing universal floor, never an incomplete producer vector.

The current setup has a special reservation before ordinary byte-ledger initialization. Moving it requires a single explicit transfer of that charge; neither double charging nor a gap is valid. The current broad exception-to-retention-refusal handler cannot be used to swallow cancellation and continue. Expensive-model refusal, cancellation, resource stop and cheap invalid arguments need distinct observable outcomes. [R5, R6]

A 0.9 goal-mass prefix and unfinished 0.1 trap branch is not a complete row. Normalizing the prefix invents a proper action. A moved lower snapshot needs its dedicated restoration. If public suspension inside sparse append was previously impossible, a new yield must preserve atomic append or introduce genuine staging.

Equal logical safety does not imply equal scheduling, arithmetic order, completion time or fairness. Native step boundaries influence scheduling. Do not reset a subtask each time it yields or claim a same-trajectory theorem without proving that stronger condition.

A read can be logically passive but consume time, memory or host-quantum calibration. Test passivity at a fixed logical boundary separately from timed overhead. Response latency has multiple terms: dispatch + remainder of noninterruptible work + rollback/transfer + sealing/export + transport/UI. A measured maximum `step` covers only one part; synchronous begin and abandonment must remain visible.

## 4. Control-indexed selected-policy equivalence

Let x include the physical item and required hidden programme memory, and k be a controller node. A family of maps \(\alpha_k(x)\) is exact for this selected controller if whenever \(\alpha_k(x)=\alpha_k(y)\):

- goal/failure classification, selected action legality and allowed observation choice agree;
- the expected immediate reward vectors agree for the queries being preserved;
- the probability of every successor (control node, abstract cell) pair agrees;
- persistent information needed to preserve these statements is retained.

These conditions let the quotient follow the same pushed-forward finite path law; induction on finite path length yields equal terminal probabilities and expected finite-horizon reward. For nonnegative rewards the limit gives equal total expected reward, and transience/properness on the certified domain supplies finiteness. For action-duration distributions, equality of mean n is insufficient: preserve the joint duration/reward/exit law at the queried granularity.

Backward requirements can be expressed as a monotone closure over the existing finite observation vocabulary:

\[
 \mathcal R_k \supseteq \mathcal O_k \cup
 \bigcup_{k\to\ell} \operatorname{Pre}_{k\to\ell}(\mathcal R_\ell).
\]

Here \(\mathcal O_k\) includes native legality, goal, reward and routing observations. The existing refinement contracts implement relevant preservation/preimage operations; reset can kill a downstream requirement, while locked/surviving affixes can preserve it. Cycles need a fixed point, not one backward traversal. This closure is only sound when those native contracts cover every relevant flow and observation. An absent contract does not mean an absent requirement. [R9; P2]

**Counterfactual boundary:** exactness for selected action a does not establish exactness for unselected b. A's law may ignore a hidden modifier tag that B's legality or draw probabilities depend on. Before B is evaluated, retain the union of requirements for the candidate choice set, refine/rebuild, or give B a separate verified context. A selected-policy quotient is not an all-action quotient and cannot retire B.

**Adapter limit:** a smaller \(\mathcal R_k\) does not automatically make today's single-layout CalcContext smaller. The global union across the selected candidate's legitimate paths may restore every distinction. A proposed per-node partition requires an explicit owner/bridge; do not claim it exists after a census.

## 5. Narrow implied-entry predicates

Let C be the kept entry condition and F the junk predicates proposed for removal. If

\[
 \forall x\text{ that can reach the entry},\quad C(x)\Rightarrow F(x),
\]

then C and C∧F route exactly the same entry set. On a side with exactly k occupied affixes, k **distinct satisfying** affixes assigned injectively to goal slots exhaust occupancy. No extra junk can remain. Two overlapping goal tests can match the same affix; two satisfied bits therefore do not establish two distinct acceptable occupants. Below-tier membership is not satisfying coverage.

This is the existing Ring clean-guard implication, not a uniform continuation-value theorem. It neither deletes native positive-mass exits nor makes a mandatory option interior an allowed decision. It is not a generic dirty-state coarsening rule. Whole recurring-controller evaluation stays required. [R8, R10]

## 6. Complete boundary response and reuse

Partition N into boundary B and interior I. B contains every changed decision/route and every required entry; I is an unchanged semantic submodel, not merely unchanged JSON nodes. Put

\[
 A_I=I-Q_{II},\quad T=A_I^{-1}Q_{IB},\quad r_I=A_I^{-1}c_I,
\]
\[
 H=Q_{BB}+Q_{BI}T,\qquad \bar c=c_B+Q_{BI}r_I.
\]

From \(V_I=c_I+Q_{II}V_I+Q_{IB}V_B\), solve for \(V_I=r_I+TV_B\) and substitute:

\[
 V_B=\bar c+HV_B.
\]

Apply the same operation to primitive-count and per-resource reward columns. For each terminal category j,

\[
 \bar g_j=g_{B,j}+Q_{BI}A_I^{-1}g_{I,j}.
\]

For a complete interior that exits to B or a terminal almost surely, \(H\mathbf1+\sum_j\bar g_j=\mathbf1\). Keep each terminal category separately. A boundary visit means the next return after leaving/taking the boundary step, not a zero-time return to the same node.

**Properness is not inherited from the interiors.** Each visit to an interior may terminate, while H=1 repeatedly returns to the same boundary forever. Full global absorption still requires the selected boundary chain to be transient and its eventual terminal mass to satisfy the clean-goal contract.

**Dependency footprint:** reusing T requires unchanged Q_II, Q_IB, semantic cell membership, exit identities, routing, observations, native law and persistent context. Reusing r_I additionally needs c_I unchanged; price-only reuse instead stores price-independent resource response. Recompute Q_BI, Q_BB and c_B if those change. New boundary destinations require new response columns or invalidation. A changed router predicate can invalidate the classification throughout I.

**What may be reused without new state discovery:** only domains whose correspondence is already established. A response optimization for numerical solving does not save the initial native pair construction unless the reused artifact also justifies that construction. This is why phase attribution and overlap precede cache implementation.

**Break-even:** for m actual planned comparisons, require

\[
 T_{build}+\sum_{j=1}^m(T_{match,j}+T_{update,j}+T_{solve,j}+T_{validate,j})
 < \sum_{j=1}^m T_{cold,j}
\]

under the full memory limit. Sparse elimination can create dense boundary fill. The top-three/one-wave pipeline provides a small m; hypothetical unlimited reuse cannot justify its overhead. During qualification, full cold native checking remains acceptance authority, so report reuse-only and all-in pilot times separately. [P1; R10]

## 7. Recurrence-corrected policy change

The existing performance-difference identity uses the new controller's occupancy:

\[
 V'-V=(I-Q')^{-1}(c'+Q'V-V).
\]

For a change to one row i on a common certified finite domain,

\[
 Q'=Q+e_i\delta p^T,\quad c'=c+e_i\delta c,\quad z_i=Ze_i.
\]

Subtract equations and write \(V'-V=z_it\). Then

\[
 t=\delta c+\delta p^T(V+z_it),\qquad
 V'_0-V_0=\frac{d_i(\delta c+\delta p^TV)}{1-\delta p^Tz_i}.
\]

For nonnegative transient Q and Q' on that full domain, the denominator is positive (the determinant ratio of nonsingular M-matrices), but native numerical evaluation must not infer positivity by a convenient tolerance. An invalid/singular candidate does not become proper through this identity.

Example: unit-cost retry Q=0.9 has V=10; changing to 0.8 gives V'=5. Old visits times advantage predict −10; denominator 2 gives the true −5. The attached audit's example is correct.

For k changed rows, let E select the rows, Q'=Q+ED and c'=c+Eδc. Then

\[
 V'-V=ZE(I-DZE)^{-1}(\delta c+DV).
\]

This is ordinary linear algebra, not a new theorem claim. One changed compiled node may alter many item/control rows; a new option may introduce states; a global observation change may alter the whole domain. In those cases the one-row interpretation fails until a proper common-domain or boundary response representation is established.

The current score d_i V_i is exposure to remaining cost, not improvement. In a ten-state cost-one chain it sums to 55 while total spend is ten. Immediate spend \(d_i c_i\) is additive; continuations overlap. A ranking formula can remain a heuristic, but should not be called expected savings. Leave unsupported tails unknown and require complete root evaluation for any retained change. [R3, R10]

## 8. Discrepancy attribution with incomplete alignment — additional derivation

For any finite vector v on a complete native controller domain, let e=c+Qv−v. Then

\[
 V-v=Ze,\qquad V(s)-v(s)=d_s^Te.
\]

An algebraic decomposition is not by itself an explanation of a *source/native* discrepancy. The vector must faithfully represent the source at corresponding native item/control entries. An unaligned or nonuniform source class cannot be filled with a convenient scalar and then treated as explained evidence.

There is a useful stopped alternative. Let K be the aligned subset containing s, U=N\K, and let v_K be the legitimate finite comparison vector on K. Define

\[
 a=c_K+Q_{KK}v_K-v_K,\quad d_K^T=e_s^T(I-Q_{KK})^{-1},\quad h^T=d_K^TQ_{KU}.
\]

Using native equations for V_K gives the exact identity

\[
 \boxed{V(s)-v_K(s)=d_K^Ta+h^TV_U.}
\]

h is the distribution of first arrival at U before terminal exit; \(h\mathbf1\le1\). V_U denotes the full native continuation, which may later revisit K. Thus no residual is counted twice. If compatible componentwise endpoints l_U≤V_U≤u_U are available, h≥0 transports those bounds. Without endpoints, preserve the symbolic remainder hᵀV_U; reporting it as zero would fabricate coverage.

This is especially suitable for the Conquest mismatch preflight: report mapped domain, first-hit unaligned contexts, known residual contributions and unknown remainder. Do not fit 358M/85k as a universal calibration. If s itself is not aligned, there is no source-root decomposition to report; first establish that mapping. [R7, R11; new derivation here]

## 9. Lower-model structural ceiling

For a fixed optimistic model M, every accepted lower h satisfies h≤V*_M under its actual proof contract. An exact solution of M cannot exceed V*_M. A cheap feasible auxiliary policy is an upper on V*_M, so it can demonstrate why solving this unchanged model more accurately cannot reach a proposed stronger threshold.

Synthetic persistent-mode example: mode x costs (1,100) in phases A/B and y costs (100,1). Real total is 101; choosing a new favourable mode at each phase yields a valid optimistic total of two. This is structural relaxation, not inaccurate arithmetic.

A future coupling relation is valid only if projections of **every relevant proper native policy** remain feasible, with original cost counted once. Occupation flows satisfy \(\sum_a x(s,a)-\sum_{t,a}P(s|t,a)x(t,a)=\alpha(s)\), x≥0, but projected tying constraints need their own native bridge. The incumbent's occupancy cannot constrain every possible policy. A new LP cut does not automatically fit today's lower schema. [U2 §5; P3]

No such native mismatch is established by the synthetic example. Freeze and inspect the actual cheapest optimistic winner before selecting a new relation; no lower producer is authorized in this plan.

## 10. Scoped numerical and duration research

For finite Q≥0 and finite w>0 satisfying w≥1+Qw, w≥1 and

\[
 \|Q\|_w=\max_i(Qw)_i/w_i\le1-1/\max_i w_i<1.
\]

Hence Q is transient and \(Z\mathbf1\le w\). If a sound residual check supplies \(|c+Qv-v|\le\epsilon\mathbf1\), then

\[
 v-\epsilon w\le V\le v+\epsilon w.
\]

More generally \(|e|\le b\) yields \(|V-v|\le Zb\). Native enclosures require native coefficient provenance and outward-safe checks. Exact Fraction calculations on rounded coefficients prove only that stored model. The interval's lower endpoint bounds this fixed policy, not the MDP optimum. Existing residual/count mathematics already covers related sensitivity. [R11; P4]

For primitive-action duration τ between interfaces, retain

\[
 H_{ij}(z)=\mathbb E_i[z^\tau\mathbf1\{exit=j\}],\qquad
 g_i(z)=\mathbb E_i[z^\tau\mathbf1\{clean\ goal\}],\qquad F(z)=g(z)+H(z)F(z).
\]

When the relevant series and zero-action control closure are well-defined, coefficients through B give deadline completion probability. At z=1 the response collapses to ordinary exit mass; derivatives can give expectations when finite. Neither exit marginals nor mean duration determine the whole series. Swapping durations 1/3 between goal and return branches preserves mean two and exit marginals but changes one-action success. Unresolved mass, cleanup work and mandatory programme memory remain explicit. A user chance constraint or hard-stop cost objective is a separate product decision; this plan preserves research, not a distributional optimizer. [P5]

## 11. Claims and qualification posture

The derivations explain necessary conditional contracts. Native correspondence, error rounding, task finiteness, actual runtime reachability, and useful performance are separate obligations. Add scoped histories under existing policy/programme/equivalence/reuse/numerical/search claims where applicable; do not allocate new IDs for classical identities or promote broad open statuses because this synthetic suite passes.

The included 48/384/18 checks and finite lifecycle model validate these small reference equations and counterexamples. They do not supply a proof of native code, a public capability claim, or a performance measurement.
