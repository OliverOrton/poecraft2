# Review and mathematical handoff

**Source pin:** `bf3b980041821acee1f1f8149afc912a22021884` (parent `b1d7436c37e2cf2dec5149713b685fe6ecffaad6`). This is a read-only source/retained-evidence review. No native checkout, build, solve, tests, simulation, queue mutation or public action was performed. The attached Python examples are exact synthetic SSP calculations, not PoE qualification. Use `source_register.json` for durable pinned links and reading scope.

## 1. Verdict and what changed

The current pass made a real but tiny Ring improvement, preserved the current Amulet policy, and left its larger objective unmet. Selective construction and row reuse now exist. One-Essence diversity was checked using native metadata and is absent on the chosen primaries. Those are resolved findings, not recommendations to run the same experiments again.

The most concrete current failure is a TemporaryBenchRepeat blocker candidate. A proper-looking private model estimates 12,482.58479255575, but independent native execution reaches only 0.27177507343351093 goal mass. The 3481.1716716110686 accounted partial cost is not a strategy upper. The compact report records `converged=1,cost_complete=1,failure=0,stop=0,failures=1`; it does not identify enough detail here to attribute the missing mass to a particular action or route category. Do not infer that the native checker is wrong, that the policy is cheap, or that increasing time solves this semantic failure. Trace the first exact mismatch.

The rejected branch is not one of the returned policies sampled successfully. The current independently evaluated fallbacks remain valid. A working blocker might improve cost, but its private estimate is only about 5.15% below the Amulet fallback, below the full programme's 25% goal. Treat correspondence recovery as a useful checkpoint and then look at the resulting strategy again.

The new source imports complete native option kernels, prices `expected_resources`, maps the native self marker to the source, and routes the emitted controller through the existing compiler/evaluator. These interfaces—not a new mechanic database—are the exact place to compare native exit/control semantics. The present review does not settle which interface causes the failure.

## 2. Keep two feedback loops, without a second planner

**Economic loop:** inspect the current policy's immediate spend, expected execution, lost-progress behaviour, and unrealized alternatives. A dominant Annul share once led to a useful change; after that improvement repeated Essence acquisition became the expensive behaviour. The explanatory unit is an actual semantic entry and decision, not a permanent currency blacklist.

For a complete proper finite policy with initial distribution mu and substochastic transient matrix P, expected visits are `d^T=mu^T(I-P)^(-1)`. Root spend is `d^T c`. Grouping immediate cost terms by action yields an additive accounting. Summing `d(s)*J(s)` does not: those values include overlapping future costs. Even r->q->goal with costs 1 and 1 has root cost 2 but sum of visited continuation values 3. Use entry value/occupancy as a heuristic lead, not additive saved-cost evidence.

**Proof loop:** inspect where an aggressive potential violates an actual complete lower relation. A violation may identify a real cheaper action, an artificial free continuation in the relaxation, a poor estimate, a stale coefficient/minimizer or a missing native bridge. These require different follow-throughs. Neither a high policy cost nor a failed estimate inequality proves a mechanic should be deleted.

The loops can share evidence and hypotheses, but not authority. The existing native upper chain validates controllers. The existing lower chain validates all-action optimistic relations. Reusing the same row bytes does not make every use of that row mathematically identical.

## 3. Three different values

`L(s) <= V*(s) <= U(s)` when endpoints have compatible native scope and entries. An adaptive `Vhat(s)` may lie outside the interval. It may still be valuable for proposing work. A private restricted-action policy value can seed that estimate; it is neither a full-scope lower nor a promise that a root-only graph executes from every source.

Do not call `max(L,Vhat)` a lower, or assume multiplying a guess by 0.5 makes it safe. A predictor of the *current controller's* native cost is not necessarily a predictor of optimal cost. Resource censoring, missing routes and finite-action simulation stops must not become cost labels.

The previous adaptive experiment calibrated three acquisition-family buckets. A shared positive multiplier preserves the order of same-family scores; that narrow experiment had no observed selection effect. It does not refute state-conditioned estimates or a certifying producer. Conversely, the present plan does not justify a new neural/LP framework merely because those are possible.

## 4. A concrete way to test estimates as certificate proposals

### 4.1 Existing theorem and native boundary

For finite bounded nonnegative h, zero at true goals, if

`h(s) <= c(s,a) + sum_t P(t| s,a) h(t)`

for every allowed action on the relevant domain, repeated conditioning under any proper policy bounds h by that policy's expected cost. The bounded residual vanishes as the probability of not terminating tends to zero. Taking the infimum gives h<=V*. A finite lower-only region can stop at independent compatible boundary lowers; it does not need an incumbent route at every exit. This argument is already in the repository's CLM-0007/0008/0012 material.

Complete action/family coverage, native domain/member validity, positive mass, observation timing, coefficient interpretation and current semantic generations remain required. A successful numerical check on a restricted or falsely declared model is not a native lower. An accepted finite vector is not proof of a proper policy or model infeasibility.

### 4.2 Largest feasible step along a proposed direction

Let l be feasible for the **current complete query**. Keep goals and independent boundary values fixed. On internal variable states set

`d(s)=max(0,Vhat(s)-l(s))`,  `h_alpha=l+alpha*d`,  `0<=alpha<=1`.

An ordinary row gives

`l(s)+alpha*d(s) <= c + P*l + alpha*P*d`.

Equivalently,

`alpha * a_sa <= b_sa`,

where `a_sa=d(s)-P*d`, `b_sa=c+P*l-l(s)>=0`.

Rows with a_sa<=0 do not limit an upward nonnegative ray. For a_sa>0, alpha<=b_sa/a_sa. Thus for fixed affine rows and a fixed feasible base:

`alpha_star=min(1, min_{a_sa>0} b_sa/a_sa)`.

A scalar unresolved-family constraint `h(s)<=F(s,family)` gives the same calculation with slope d(s) and slack F-l(s). Such a weak floor can cap the result; it cannot be removed merely to improve alpha.

This is a one-dimensional constrained feasibility calculation, not a new SSP solver or an assertion of research novelty. The estimate supplies a shape, while the complete inequalities determine how much is justified. A raw estimate need not be accurate to yield a valid checked result; poor shape can make it useless.

For raw stored coefficients use those coefficients exactly. For normalized-reference semantics use the query's exact positive row mass/cross multiplication. Do not mix coefficient models. Floating native code proposes a vector; the existing exact-dot complete checker accepts/refuses the actual stored vector. A slightly rounded alpha is not an acceptance tolerance or a proof that it is exactly maximal.

### 4.3 Worked example

There is one nonterminal state s:

- finish costs 40 and reaches goal;
- retry costs 12, reaches goal with probability 1/2 and returns to s with probability 1/2.

The true optimum is 24. Suppose l(s)=4 and Vhat(s)=100, so d(s)=96.

For finish, `b=36,a=96`, giving alpha<=3/8. For retry, `b=10,a=48`, giving alpha<=5/24. The limiting retry row produces

`h(s)=4+(5/24)*96=24`.

An untrustworthy guess of 100 has supplied a direction from which a checked 24 is recovered. That illustrates feasibility, not a forecast of the Ring/Amulet improvement. If l is already 24 on this unchanged model, the available alpha is zero.

### 4.4 Choices and value-dependent probability envelopes

A lower must respect the minimum over real decisions at their native observation time. Along a ray, an observed-choice RHS is generally a minimum of affine functions. Its minimizing option may change. A fixed allocation valid at l need not minimize at h_alpha.

One safe route is explicit native observation/decision nodes and complete constraints. Another is a bounded feasibility search which recomputes the actual minima and uses the existing checker at each proposed alpha. For a fixed complete concave RHS, the feasible alpha set containing 0 is an interval, but that fact does not justify freezing a changing native event allocation. The producer must regenerate native envelope minima at the final vector just as it does now.

The synthetic example with x increasing from 1 to 21 and y fixed at 10 shows why freezing x as the initially cheaper choice would falsely accept root 15; rechecking the other permitted choice limits the root to 10.

### 4.5 Ray limit, model ceiling and native optimum are different

Take two independent states with terminal costs 100 and 1. An estimate (100,100) scaled uniformly from 0 is limited at alpha=0.01, giving (1,1). The model nevertheless supports (100,1). A different direction recovers it. Therefore failure of one direction is not proof of an intrinsically weak model.

In contrast, an explicitly proper auxiliary policy below threshold tau proves that **the unchanged optimistic model** cannot supply a feasible potential at least tau at its entry. This is the existing threshold-ceiling argument. It says nothing about a refined model or the native optimum, because the auxiliary policy may use intentionally artificial exits.

Example: root has a 10-cost finish or pays 2 and either finishes or reaches q, each with probability 1/2. At q, pay 1 and either retry q or exit to d, each with probability 1/2. Boundary b(d) gives `V(q)=2+b` and deviation cost `3+b/2`. With b=4 the auxiliary route costs5; with b=40 the route costs23 and root optimum is10. Running more iterations on the b=4 model cannot certify root10. Changing a nonbinding action cannot fix that escape.

The current old405-style relaxation has a recorded cheap outside-domain route. It is not automatically the applicable model for Ring/Amulet; inspect the current query. We do not prescribe another unchanged solve of that old model.

### 4.6 Shape refinement, if the ray is the problem

At most a small number of existing state/goal-side/control features can form

`h_theta(s)=l(s)+sum_j theta_j f_j(s)`.

For fixed complete affine constraints the shared budget is

`sum_j theta_j [f_j(s)-P f_j] <= c+P l-l(s)`.

A scalar-family floor and fixed boundary/terminal constraints must be included too. Requiring nonnegative increments and checking the complete resultant vector is a simple conservative subset. This can avoid letting one unrelated bottleneck scale every entry down, but may cost more than it saves. Use one changed direction first; do not build a general feature-search or LP package as a prerequisite.

Different component tables may charge the same operation. Their sum is not admissible just because each is a lower. Checking the combined potential under the original cost budget, or a genuine cost partition, addresses double charging. The pointwise maximum of separately valid native lowers remains safe on compatible domains.

## 5. What current code already does

`solver_quotient_lower.cpp` accepts checked or untrusted coordinate-compatible inputs. It validates exact request/scope/model/price/coefficient identities and fixed boundary values. The numerical path currently tries a tiny contraction of internal values by `1-1e-10` when the proposal fails; if it still fails, it checks a zero-modelled-state fallback. A supplied direct candidate can instead be refused. Every accepted vector passes the raw inequality checker.

The proposed ray producer is not an excuse to remove that checker or to describe seeds/checking as missing. It is a bounded alternative between 'tiny correction' and 'give up useful shape', preferably outside the trusted checker. The complete native model may already be tight or too weak, so the real first result could be a zero ray or a model ceiling. A numeric-only diagnostic should not add dormant runtime machinery if no native consumer follows.

`QuotientLowerCertificate` deliberately has no conversion to a public lower. Preserve that design. The existing native producer/domain proof must authorize consumption. In particular, private dirty layout IDs and selected-row subsets cannot be cast into full-scope proof coordinates.

## 6. Native experiment that can change a decision

Choose one source/action question from an expensive repeated phase or a newly improved incumbent, and one contrasting family if useful. Recover current complete lower constraints and independent boundaries from existing owners. Before a large solve, check whether an obvious valid scalar constraint already prevents the useful threshold.

Then compare the present lower proposal with a repaired aggressive current-coordinate vector on the **same** complete query. Retain every violated/limiting constraint's source, action/family, kind, slack, slope and scope. Do not just compare two output numbers. If a repair is useful, consume it where that source/action enters the root proof; otherwise inspect one named artificial escape or poor direction. Stop when the unchanged model has a cheap checked auxiliary policy, when native correspondence is missing, or when the experiment would duplicate the whole graph.

Policy work continues independently. A failing lower attempt can still identify an action worth trying or a free-boundary assumption worth researching. It must not become permanent ownership of the whole solve budget.

## 7. Literature read and transfer limits

**P1 — Chatterjee, Quatmann, Schaeffeler, Weininger, Winkler and Zilken, TACAS 2025.** Read the publisher's expected-reward certificate and generation sections. Finite co-inductive values can certify expected-reward lowers, with explicit treatment of nonreachability/infinity. This supports separating fast proposals from trusted checking. It does not supply poecraft's native abstraction or scope proof. No Isabelle/Storm dependency is proposed.

**P2 — Hartmanns and Kaminski, Optimistic Value Iteration, CAV 2020.** Read the publisher's method discussion. Its guess-and-verify strategy constructs candidate uppers from numerical iterates, then proves them. It is precedent for speculative numerical proposals, not a drop-in lower-repair algorithm for our proper-policy SSP semantics. Direction, fixed-point and termination assumptions must not be inverted casually.

**P3 — Kloessner, Pommerening, Keller and Roeger, ICAPS 2022.** Read the SSP/LP and potential-heuristic sections. It connects weighted features, full Bellman constraints and cost partitioning. A small potential basis is therefore a principled alternative to hand-authoring one global table. Coverage and the cost of separation remain; merely fitting values on sampled states is not that result. PDF screenshot retrieval failed; the parsed equations/text, not any unseen figure, support this discussion.

**P4 — Schmalz and Trevizan, AIJ 2026.** Read the selective-constraint, consistency/error and action-elimination sections of the author PDF. Relevant constraints can be introduced without expanding every action first. The paper also makes approximation/consistency assumptions explicit; its epsilon statements are not our strict acceptance tolerance. Native zero-cost/observed/abstract-program semantics require our own bridge. PDF screenshot retrieval failed; no graph/chart interpretation is used.

These are established ingredients. The simple ray calculation is derived here as a candidate implementation tactic, not claimed novel. The missing science is whether the available native evidence and actual economic problem admit a stronger useful potential cheaply.

## 8. Checks and limits

`checked_estimate_examples.py` passed **32 named checks**, including **128 generated finite three-state models and 1,024 enumerated complete deterministic policies**. Generated actions have positive goal probability, so their policies are transient. The repaired values are compared with separately solved policy systems; limiting rays are tested for maximality on their fixed direction.

Named counterexamples cover omitted actions, infeasible old bases, fixed-boundary mutation, a ray limit that is not a model ceiling, a changed observed minimizer, zero-cost cycles, rare-retry residual amplification, incomplete probability mass, double-counted continuation accounting, and one-shot/repeated/cooperating decisions.

The reference is intentionally small and exact-rational. It does not prove all theorem premises for native kernels, evaluate a PoE strategy or estimate performance. No native test/build/solve/Simulator was performed in this review. The first native correspondence and consumer measurements belong to the implementation session.
