# Algorithm literature and falsification notes

Research date: 2026-09-04. Repository reference supplied by the coordinating researcher: `c9530ac941b95e68a1e3d890caf00d53c9e6d774`. This workstream was read-only on the main worktree and did not access protected untracked `0`. No production changes, native solve, simulation, or tests were run here. The mathematical examples below are derivations, not measured native results. They are mechanics-independent.

## Result that matters

The finite-domain, free-value proposal has a sound interpretation and survives the central objection: an outside state needs an independently valid lower bound, not an executable continuation in the incumbent controller. It is standard finite-domain SSP/Bellman reasoning, not a new optimality theorem. The strongest objections are (a) coverage and boundary provenance, (b) placeholders that cap every possible improvement, (c) inconsistent boundary values breaking claimed monotonic extension, and (d) exact expectation/dependency queries costing as much as the rejected physical expansion.

Free values remove a logical restriction of frozen policy potential, but cannot strengthen an unchanged relaxed model beyond its optimum. An LP, SCC solve, and repaired candidate face the same cheapest optimistic actions. A lower-optimal relaxed policy is the right source of abstraction flaws; incumbent occupancy is a different quantity.

## A precise finite-domain certificate

Let the original model have nonnegative costs, absorbing success states of value zero, and a finite set of legal actions at each state. Use the objective that minimizes expected cost among proper policies, equivalently assigns infinite cost to paths that never succeed. Let `R` be finite, and let every modeled candidate `x(s)` be finite and nonnegative. For each legal action at each `s in R`, retain one or more sound coverage witnesses:

* An exact row: `x(s) <= c(s,a) + sum_(t in R) P(t|s,a)x(t) + sum_(t outside R) P(t|s,a)b(t)`.
* A scalar row: `x(s) <= L(s,a)`, where `L(s,a)` is independently certified no greater than the original action's optimal cost-to-go, including its immediate cost and all mandatory work.
* A whole-family scalar row `x(s) <= L_F(s)`, provided `L_F(s) <= inf_(a in F) Q*(s,a)` and the coverage witness identifies the canonical set `F`.
* A whole-kernel optimistic replacement with an explicit cost/probability or simulation relation proving it lower than the original action for the continuation class being used.

Here `b(t) <= V*(t)` must have authority independent of the candidate being checked. Having been evaluated by an upper-policy router is neither necessary nor sufficient. An unbounded/unknown state can conservatively have boundary zero under nonnegative costs. Infinity needs separate qualitative authority; a router refusing a continuation supplies none.

### First-exit proof, including scalar rows

Fix any proper original policy from a modeled start. Stop on the first success, exit from `R`, or selection of an action represented only by a scalar placeholder. Until this stopping event, apply the exact-row inequalities conditionally and telescope. At success the terminal potential is zero. At an outside state it is `b(t)`, bounded by the expected remaining cost of the proper policy's continuation. At a scalar stop it is `L(s,a)`, bounded by that continuation's complete action-plus-future cost. Properness makes the stopping event almost surely finite. In the truncation at time `n`, the remaining modeled potential is bounded by `max_R x`; its expectation vanishes as the probability of not stopping vanishes. Nonnegative accumulated costs permit monotone convergence. Therefore `x(s)` is no greater than the cost of every proper original policy, hence `x(s) <= V*(s)`.

This proof handles arbitrary cycles inside `R` simultaneously. It does not require the outside `b` vector to satisfy Bellman inequalities, nor `x=J_policy`, nor a policy entry at an outside state. It does require all original choices to be covered. It also separates **soundness of a finite candidate** from **a particular algorithm reaching the greatest feasible candidate**.

For a fixed finite model, maximize `x(root)` or a positive weighted sum of variables subject to all covered rows and success anchors. A finite feasible solution is a lower certificate after independent arithmetic validation. An optimizer's success status alone is not a certificate. A global sum objective can be unbounded due to an irrelevant state; that is not evidence that the root value is infinite.

## Explicit counterexamples and boundaries

| Question | Mechanics-independent construction | Exact conclusion |
|---|---|---|
| Frozen incumbent vs free lower | `r --10-->g` is incumbent; another action `r --1-->t`, `t --4-->g`. Take `R={r}` and `b(t)=4`. | True optimum and free lower are 5. Frozen `J(r)=10` fails `10<=1+4`. Failure is desirable: incumbent is suboptimal. |
| Missing incumbent continuation | `r --10-->g`; deviation `r --1-->t`, `t --20-->g`. Incumbent has no route at `t`; use `b(t)=9`. | `x(r)=10` satisfies both rows, proving the root incumbent optimal. Lower 9 at `t` is sufficient although true value is 20. |
| Simultaneous stochastic cycle | At `s`, pay 1 then reach `t` with probability 1/2 or goal with 1/2. At `t`, pay 2 then reach `s` with probability 1/2 or goal with 1/2. | Equations give `x(s)=8/3`, `x(t)=10/3`. One-step evaluation against zero gives only `(1,2)`. Cycle is proper: probability of remaining two steps is 1/4. |
| Missing action and duplicate coverage | At root, legal terminal actions cost 10 and 5. Put the cost-10 inequality into a partial LP twice. | Constraint count 2 still misses the second action and falsely permits 10. Replacing the missing action by explicit scalar cap 3 yields sound lower 3; refining its cap to exact 5 yields 5. Set equality is the coverage test. |
| Destructive cleanup/rebuild | State `A+B_dirty`; cleanup costs 1 and leaves `B_clean`, losing A. Restoring A costs 7 and preserves clean B. Goal is `A+B_clean`. | True recovery cost 8. A relaxation that silently preserves A during cleanup gives 1. This is a valid weak lower if explicitly an optimistic relaxation, but cannot justify an exact row or strong preservation certificate. Refinement must track the lost property/rebuild obligation. |
| Zero-cost improper cycle | At `s`, exit to goal costs 10; self-loop costs zero. | Proper-policy optimum 10; ordinary accumulated-cost optimum 0. All `0<=x(s)<=10` satisfy the Bellman inequalities. Maximizing the finite subsolution gives 10; bottom-start VI stays at 0. A greedy tie at `x=10` may choose the improper loop, so a lower certificate does not by itself compile a proper policy. |
| Small residual, large error | One action costs 1 and reaches goal with probability `p=10^-12`, otherwise retries. Candidate `x=(1+10^-8)/p`. | True value `10^12`; candidate is too high by `10^4`, yet its Bellman inequality violation is only `10^-8`. A small absolute residual cannot be accepted as an exact lower inequality. |
| Admissible but inconsistent lower | `r --1-->t --9-->g`; choose `h(r)=10,h(t)=0`. | Both entries are admissible. But `h(r)>1+h(t)`. Admissibility does not supply a common Bellman certificate or monotone-backup invariant. |
| Domain extension can weaken | `r --0-->t --0-->u --10-->g`; admissible boundary has `b(t)=10,b(u)=0`. First `R={r}`, later `R={r,t}`. | First model allows root lower 10. Second forces `x(t)<=0`, so root lower becomes 0. Both models are sound. Merely adding states does not imply stronger values when boundary is inconsistent. |

These exact examples should precede any native experiment. The source of the long-horizon residual amplification is `(I-P_pi)^-1`: a residual bound is multiplied by an expected remaining-visit bound. A floating-point tolerance must never silently stand in for that bound.

## Monotonicity and candidate repair

For a fixed domain and exact rows, increasing an independently sound outside boundary enlarges the feasible set of subsolutions and cannot lower the optimum. Increasing a valid scalar cap likewise weakens its inequality and can only improve the attainable lower. The direction is easy to confuse: a stronger lower on a minimized action means a **larger** scalar cap in the Bellman subsolution LP.

Replacing a scalar cap with an exact row is monotone only if the new row dominates the old optimistic action on the permitted continuation vectors, or the old solution has a feasible extension to the new model. It is insufficient that both replacements are separately admissible. A sufficient condition is a Bellman-consistent base and refinement that removes only optimism while admitting a lift of the old certificate. More generally, check the feasible lift directly.

An independently certified old lower can always remain in a publication portfolio: the pointwise maximum of independent admissible bounds is admissible. But that maximum need not be feasible for this particular finite model and must not be mislabeled as its Bellman certificate. Imposing `x>=h` can make a model infeasible when `h` is admissible but inconsistent or the placeholders are weaker than `h`.

`J_policy` can initialize a candidate. Repair may decrease values to satisfy missing inequalities, including at the root when the incumbent is suboptimal. An overestimating `J` is not an admissible iterate merely because a later repair might succeed. Repairs must propagate through incoming dependencies and terminate with a complete check. Decreasing all entries by a fixed epsilon is not a universal repair: zero-cost deterministic edges and retry amplification defeat arbitrary constant slack. The repaired vector can be weak or fail to settle without SCC/end-component handling; that does not refute the lower model.

## Zero costs, failure, and DP/LP reuse

Three objectives must remain distinct:

1. Expected accumulated cost of all paths, permitting a zero-cost nonterminating policy to have cost zero.
2. Expected cost restricted to proper policies.
3. Expected cost with infinite penalty on every path that does not reach success.

For the finite, nonnegative setting, 2 and 3 have the relevant equivalence; 1 can differ. The finite-candidate first-exit proof addresses 2/3. Arbitrary Bellman fixed-point selection addresses none of them automatically. In particular, unrestricted extended-real greatest fixed points can be spuriously infinite even on a proper retry state: `infinity=1+(1/2)infinity`. The finite-candidate requirement prevents this false certificate. Unsolvability or infinite lower claims require additional qualitative certificates.

SCC decomposition is a graph organization, not a semantic cure for zero-cost end components. Existing sparse Bellman/Howard machinery is reusable only after its terminal/failure convention, policy properness rules, tie handling, finite anchors, and numeric acceptance match the relaxed model. Positive-cost stochastic cycles are routine simultaneous systems. Zero-cost end components may require quotienting, a progress witness, proper initialization and tie rules, or another correctly justified procedure. If the reuse fails here, compare LP and DP on the exact same toy rows before changing representation.

## Temporary inactivity and the procedural-kernel bill

For an exact row define slack `d(s,a)=c+sum P*x-x(s)`. Increasing a source value can turn its inactive outgoing rows into violations. Decreasing a successor value can violate incoming rows, including rows that were not materialized. A mutation to costs, probabilities, action scope, automatic-program phase, or boundary proof identity invalidates the relevant cached witnesses even without a value update.

Temporary inactivity therefore needs complete canonical action enumeration plus a sound separation method. It is different from permanent retirement, which requires a lower on an action exceeding an applicable upper bound (with the correct equality/tie contract). A partial LP that simply omits a cheaper row can overestimate. It cannot publish its current objective as a lower by appeal to eventual constraint generation.

For procedural kernels the useful metrics include: exact expectation queries, predecessor-query work, conservative invalidation fanout, duplicate suppression, rechecked rows, newly interned canonical states, retained row/dependency bytes, transient construction peak, and cache reuse under a second source shape. Knowing the mathematical predecessor set is not the same as obtaining it cheaply. Storing dependencies for all currently omitted actions may reconstruct the full rejected expansion.

Scalar placeholders offer a deliberately simpler invariant: keep `x(s)<=L(s,a)` until refinement. When `L` is an immutable independently certified scalar, that coverage has no dependency on mutable successor values. The cap may be weak, but its bookkeeping is cheap and explicit. A whole-family placeholder can avoid listing all physical successors, provided membership/coverage is certified. A later stronger projected function of `x` needs its own dependency support or a conservative complete recheck.

Compute the remaining placeholder ceiling **before** building expensive rows. Any outgoing cap `L` implies `x(s)<=L`. Propagate those caps through cheap known rows to get a source ceiling; a cheap optimistic policy gives another upper bound on what the relaxed model can certify. Refine the minimum-owning optimistic action/phase, then measure the shift in the same source/scopes. Fixing a high-occupancy incumbent deviation cannot help while another untouched cheap placeholder continues to own the minimum.

## Mandatory programs and projected expectations

Primitive inequalities can be composed over a mandatory automatic program by the same stopping-time argument: retain the initial setup cost, every internal action cost, and the joint distribution at the first external decision point. A program phase that changes available actions is part of the modeled Markov state. Setup paid only before the program is not paid again on every retry; setup omitted entirely is a weaker relaxation and cannot be treated as an exact macro.

If the original semantics offers a decision after observing an intermediate outcome, collapsing across it with a fixed continuation removes a legal choice and can increase a claimed lower unsoundly. Either preserve that decision point, quantify over all legal internal choices, or prove that continuation is mandatory. Conversely, granting earlier/more observation-dependent choices may form an admissible relaxation, but not an exact program equivalence. A phase/entry-sensitive composition key must retain scope, prices, checkpoint/resources, observation timing, and terminal/failure behavior; a compiled controller node identifier alone does not define the original state.

For a potential `phi(t)` depending only on retained statistics, an exact query needs `E[phi(T)]`, not necessarily all physical successors. For additive `phi`, exact marginal expectations can suffice by linearity without independence. Interaction terms need suitable joint statistics. A projected Markov kernel requires stronger pushforward/distribution and action-availability conditions than one expectation query. Build the weakest query sufficient for the proof, and prove its relationship to the engine's complete physical kernel. This is a representation alternative to a shared full successor DAG, not a reason to recreate that DAG first.

## Primary literature: exact transfer and limits

The following summaries intentionally emphasize the needed contracts rather than importing performance claims into this repository. No Path of Exile mechanic sources were consulted.

### Schmalz and Trevizan, AAAI 2024

[Published paper](https://ojs.aaai.org/index.php/AAAI/article/download/30005/31764), DOI [10.1609/aaai.v38i18.30005](https://doi.org/10.1609/aaai.v38i18.30005), pp. 20247–20255; especially Algorithm 2 and the constraint-generation discussion. It connects partial SSP search to LP constraint generation and uses heuristic values to avoid repeatedly evaluating unpromising actions. The distinction between temporary inactivity and permanently justified elimination is directly relevant. The paper is historical grounding, not the version to implement from: the authors' [project page](https://schmalz.cc/cgilao/) explicitly says the AIJ extension subsumes previous versions and fixes minor issues. It does not establish that an arbitrary partial LP is an anytime lower certificate, or that source/successor dependencies are inexpensive for an implicitly generated kernel.

### Schmalz and Trevizan, AI Journal 2026

[Accepted manuscript, arXiv 2604.01855](https://arxiv.org/pdf/2604.01855), DOI [10.1016/j.artint.2026.104505](https://doi.org/10.1016/j.artint.2026.104505). Pinpoints: Section 2, Assumptions 1–3 and Figure 2 (PDF pp. 4–5); Definitions 8–9 (pp. 6–7); partial SSP LP/Figure 6 (pp. 11–12); Algorithm 2 and Definition 13 (pp. 13–14); `Ext-Succs`/`Preds` and violation bookkeeping (pp. 14–15); Figure 8, Theorem 3 (pp. 16–18). Assumptions include existence of a proper policy and infinite cost for improper policies. Consistency is stronger than admissibility. Partial expansion can temporarily overestimate, requiring violation repair. Notably, full-SSP consistency is not guaranteed by the returned partial-SSP solution. Its epsilon result involves an expected-step factor; it is not exact arithmetic certification. The transferable idea is reactivation-aware lazy row selection. The repository must supply complete separation/dependency access and independently validate the final covered candidate; no procedural-kernel complexity benefit follows automatically.

### Chatterjee et al., TACAS 2025

[Publisher full text](https://link.springer.com/chapter/10.1007/978-3-031-90653-4_7), DOI [10.1007/978-3-031-90653-4_7](https://doi.org/10.1007/978-3-031-90653-4_7), pp. 130–151. Most relevant are Section 5 (explicit nonreachability-as-infinity expected rewards), Lemma 6 and Proposition 7, and Section 6 on certificate generation. Lemma 6 combines a Bellman subsolution with finiteness on the appropriate almost-sure-reachability region. This explains why finite lower potentials can be sound even with zero costs, while arbitrary infinity-valued fixed points are not. The paper distinguishes ordinary accumulated reward in its appendix. It does not require lower candidates to equal incumbent policy values. Its finite explicit-MDP certificates do not certify this repository's state abstraction, macro coverage, or query probabilities; those must be established before the checker receives rows. Section 6 also explains how ordinary rounding can break inductivity even when the underlying iteration is theoretically sound.

### Klößner, Seipp and Steinmetz, ECAI 2023

[Cartesian Abstractions and Saturated Cost Partitioning in Probabilistic Planning](https://journals.sagepub.com/doi/pdf/10.3233/FAIA230405), DOI [10.3233/FAIA230405](https://doi.org/10.3233/FAIA230405), pp. 1272–1279. Pinpoints: Sections 3.1–3.5, Algorithms 1–3, Theorems 1–2; Section 3.2 uses FRET-pi to handle zero-cost operators. CEGAR seeks a flaw in an **optimal abstract policy**: false goal, inapplicable action, or outcome transition inconsistent with the concrete state. Refinement separates the state facts responsible for that flaw. This supports selecting a retention/capacity or phase distinction from the current optimistic lower policy, not expanding the incumbent domain by default. The efficient transition rewiring relies on the paper's Cartesian/factored operator representation; its at-most-two rewired transitions per split is not a promise for arbitrary procedural kernels. Termination by eventually reaching identity is a completeness statement, not a tractable-memory guarantee.

### Klößner et al., ICAPS 2022

[Cost Partitioning Heuristics for Stochastic Shortest Path Problems](https://ojs.aaai.org/index.php/ICAPS/article/download/19802/19561/23815), DOI [10.1609/icaps.v32i1.19802](https://doi.org/10.1609/icaps.v32i1.19802), pp. 193–202. Relevant sections are SSPs with Negative Costs, Abstraction Heuristics, Cost Partitioning, and Relationship to Occupation Measure Heuristics; Theorems 1–2 qualify LP semantics when negative cycles exist. Cost partitioning can combine multiple projected lower problems without charging the same original cost repeatedly. The transferable obligation is a compatible cost allocation and abstraction mapping, not simply summing independent lowers. The paper also relates occupation-measure tying constraints to cost partitions. Its negative-cost subtleties matter if saturated partitions introduce negative residual costs even though original prices are nonnegative. It does not supply missing state-dependent automatic-program identities or justify discarding retention/capacity interactions when a joint recovery obligation dominates the real cost.

### Klößner et al., ECAI 2024

[Merge-and-Shrink Heuristics for SSPs with Prune Transformations](https://ai.dmi.unibas.ch/papers/kloessner-et-al-ecai2024.pdf), DOI [10.3233/FAIA240618](https://doi.org/10.3233/FAIA240618). Pinpoints: Section 3, Definition 1 and Theorem 1 (PDF pp. 3–4), and Section 4's pruning/composition conditions. The paper permits weaker heuristic guarantees outside states relevant to proper solutions, under explicit alive-state properties. That is not permission to classify an off-incumbent state as dead or irrelevant because the incumbent router fails there. Its alive notion considers general proper policies, not just the incumbent or a single stationary controller. Useful transfer: prove structural pruning conditions once and compose compatible transformations. Limit: the factored transition-system and preservation hypotheses must be supplied, and theorem scope must match the requested root/action family; an ad hoc state deletion has no authority merely because it resembles a prune transformation.

### Hartmanns, TACAS 2022

[Correct Probabilistic Model Checking with Floating-Point Arithmetic](https://ris.utwente.nl/ws/portalfiles/portal/295755740/978_3_030_99527_0_3.pdf), DOI [10.1007/978-3-030-99527-0_3](https://doi.org/10.1007/978-3-030-99527-0_3), pp. 41–59. Pinpoints: Sections 3.1–3.4 and 4, especially safely rounded probability conversion and Bellman sums, compiler behavior, and termination at a representational fixed point. Correct numeric enclosure needs safe rounding throughout relevant operations, including input probabilities, not a tolerance on an ordinary float result. The method can terminate with an enclosure wider than requested. This motivates exact-rational micro-oracles and an independently directed final inequality check. It does not certify a model with omitted actions or the original probabilities after they have already been irreversibly rounded. Its compiler/runtime observations are version-specific evidence, so local C++/WASM numeric guarantees need their own validation.

### Hartmanns et al., STTT 2026

[The revised practitioner's guide to MDP model checking algorithms](https://publications.rwth-aachen.de/record/1032364/files/1032364.pdf), DOI [10.1007/s10009-026-00848-y](https://doi.org/10.1007/s10009-026-00848-y). Pinpoints: Section 2.1 (objective semantics; PDF p. 4), Section 2.2, Section 3 (LP), and Sections 6–7 (evaluation/conclusions). Its expected reward is ordinary accumulated nonnegative reward, with infinite-value states preprocessed; this is not automatically the repository's nontermination-as-infinity objective. Undiscounted Bellman operators lack the discounted contraction guarantee. Empirically no one solver dominates every model; polynomial LP theory does not establish a practical advantage over VI/PI. Transfer: use a small matched LP reference to distinguish incorrect solving from weak modeling, then reuse existing machinery if semantics and arithmetic are checked. Benchmark speedups on explicit models do not account for the repository's procedural row construction and retained physical-state costs.

### Established closest prior art: LRTDP, i-dual, occupation measures

[Bonet and Geffner, Labeled RTDP, ICAPS 2003](https://cdn.aaai.org/ICAPS/2003/ICAPS03-002.pdf), especially model assumptions and Section 4's labeling procedure, already focuses Bellman work on relevant greedy-envelope states and labels solved regions. Its positive-cost/properness assumptions and epsilon consistency are not a license for exact numeric certification. The idea of keeping lower-guided solved subgraphs is established prior art.

[Trevizan et al., Heuristic Search in Dual Space for Constrained SSPs, ICAPS 2016](https://cdn.aaai.org/ojs/13768/13768-40-17286-1-2-20201228.pdf), Section on i-dual, Algorithm 1, Theorem 1; also the [IJCAI 2017 presentation](https://www.ijcai.org/proceedings/2017/0701.pdf), DOI [10.24963/ijcai.2017/701](https://doi.org/10.24963/ijcai.2017/701). The algorithm treats frontier states as artificial terminals with heuristic costs and expands those carrying positive occupation flow. This is especially close to boundary-anchored free-value search. Its full action expansion and LP/occupation representation differ from cheap scalar-covered native proof queries; constrained-policy randomization is not an automatically required feature here.

[Trevizan, Thiébaux and Haslum, Occupation Measure Heuristics for Probabilistic Planning, ICAPS 2017](https://users.cecs.anu.edu.au/~thiebaux/papers/icaps17.pdf), Definition 3 and Theorem 1, construct lower bounds by relaxing the occupation-measure LP and tying projected action counts. Tying counts does not enforce consistent action/state sequencing across projections, a stated limitation. This is a concrete explanation for why separately plausible goals can ignore destructive rebuild interactions. The useful comparison is one interaction-preserving projected model, not a claim that a production LP dependency is necessary.

## Standard theory versus possible repository contribution

Bellman subsolutions, frontier heuristic terminals, SCC solution, partial expansion/constraint generation, counterexample-guided refinement, cost partitioning, occupation measures, and directed arithmetic are established methods. A repository integration can still be useful: exact engine-owned procedural expectation queries, immutable action-scope identities, compositional automatic-program certificates, persistent lower-only ownership, and measured reuse across source shapes. Combining familiar ingredients does not by itself justify novelty.

The smallest changed invariant worth proposing is: **every retained modeled state has a finite candidate value and complete canonical legal-choice coverage by exact rows or explicitly authoritative optimistic placeholders; outside dependencies bind to independent lower proofs, never to incumbent routability.** Numeric promotion checks this invariant after all repairs/reactivations. Whether an existing retained owner already satisfies it must be answered from source inspection, not from these papers.

## Suggested theory contract for an implementation boundary

1. Fix proper-policy/failure-infinity semantics, original Markov state, and caller action/program/price scope before creating the proof model.
2. Admit canonical lower-only states independently of the incumbent controller.
3. Cover the canonical action set completely; record exact rows, family membership, placeholders, and retired actions as distinct evidence types.
4. Keep candidate values and certified publication values separate. Preserve previous independent lowers without claiming they are feasible in a newly refined model.
5. Require exact/directed final inequality checking and finite candidates; treat positive lower infinity and executable policy properness as separate qualitative proofs.
6. Compare DP/SCC, LP reference, and repair on identical rows including the zero-cycle and inconsistent-boundary fixtures above.
7. Measure placeholder ceilings and dependency/query cost before adding exact rows. If physical expansion dominates, stop scheduling work and choose a projected expectation representation problem.

