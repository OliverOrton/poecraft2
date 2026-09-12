# Executable policies, properness, and upper bounds

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


A policy upper is a witness of what can actually be executed. It is not a promise that the action sequence is optimal, and it is not the value of a convenient projection unless that projection has been connected to native execution.

<a id="fixed-policy"></a>
## 1. Fixed-policy evaluation

Fix a deterministic policy on a finite semantic graph, including any controller memory. Let \(N\) be the nonterminal states reachable from the entry under that policy. Let \(Q\) be the transition matrix restricted to \(N\), and let \(r\) be the expected immediate cost vector. Goal transitions are not included in \(Q\), so it is substochastic.

If the policy reaches the goal almost surely, the nonterminal chain is transient and

\[
Q^n\to0,\qquad
(I-Q)^{-1}=\sum_{n\ge0}Q^n.
\]

Its expected remaining cost is

\[
J_\pi=(I-Q)^{-1}r,
\quad\text{equivalently}\quad
J_\pi=r+QJ_\pi.
\]

The series explanation is useful: \(Q^nr\) is the expected cost paid at the \(n\)-th nonterminal step. Nonnegative costs allow the expected sum to be taken term by term. Finiteness follows from finite transient \(Q\) and finite immediate costs.

The same inverse gives expected visits and resource uses. If \(e_s\) selects the start, the row vector \(e_s^\top(I-Q)^{-1}\) records expected visits to nonterminal states. Multiplying by per-visit resource expectations yields totals whose price-weighted sum should reconcile with monetary cost. That is a consistency check, not an optimality proof.

### Cost attribution and a changed controller

For the evaluated controller, let \(d_\pi^\top=e_s^\top(I-P_\pi)^{-1}\).
Then \(J_\pi(s)=\sum_t d_\pi(t)c_\pi(t)\). Aggregate by priced action or
retry region only after retaining the operation, item and controller-memory
identity. Compiled node counts and source `expected_cost` annotations are not
independent occupancy or entry-cost evaluations. The old controller's largest
cost contributions can guide which alternative to investigate; they do not
certify how much a changed controller saves.

For a complete legal action at an entry whose every positive-mass exit has a
compatible route and value under the same proper controller,
\(Q_\pi(t,a)=c(t,a)+\sum_u P(u\mid t,a)J_\pi(u)\) prices taking that action
once and then following \(\pi\). This is an executable upper candidate, not
a lower on all possible continuations. Uncovered exits remain unknown. A saved
lower or the old root scalar cannot supply an absent entry value. Choices must
remain at their native observation boundary. Replacing the action on every
revisit describes a different controller and requires its own evaluation.

For two proper controllers on a common finite represented domain, define
\(A_\mu^\pi=c_\mu+P_\mu J_\pi-J_\pi\). Subtracting their fixed-policy
equations gives

\[
J_\mu-J_\pi=(I-P_\mu)^{-1}A_\mu^\pi.
\]

The inverse is nonnegative. Nonpositive advantage throughout the relevant
domain therefore suffices for non-increase; a negative term reached with
positive expected visits under \(\mu\) gives strict root improvement. This
condition is sufficient, not necessary: complete candidate evaluation may
accept changes with compensating local advantages. Exact root change uses
the **new** controller's visits. For example, the supplied two-state calculation
has \(J_\pi=(12,10)\), \(J_\mu=(6,5)\), and \(A=(-1,-5)\). Old visits
\((2,1)\) predict -7; new visits \((1,1)\) correctly give -6.

Properness is an explicit premise. A zero-cost self loop has zero advantage
against a cost-10 finish and never terminates. Complete transitions, observed
choices, pricing, entry compatibility and independent evaluation of the actual
emitted controller remain necessary. A better upper may help existing certified
pruning; full competitor coverage still owns optimality. This elementary
derivation is imported from the [post-incumbent review](../../active/2026-09-11-post-incumbent-cost/imported/review_and_knowledge.md),
without a new theorem ID or native correspondence claim from its synthetic tests.

Because \(\pi\) is one allowed proper policy,

\[
V^*(s)\le J_\pi(s).
\]

This subset argument is the entire reason a fixed-policy evaluation supplies an upper. No greediness premise is required. [CLM-0002](../claims.md#clm-0002).

### Why an equation solution is insufficient

For a zero-cost non-goal self-loop, the equation is \(J=J\). Every finite number solves it, but the controller never finishes. Algebra does not establish properness. A numerical solver returning a finite vector cannot turn this into an executable upper.

The implementation must check support and absorption separately. The current publication contract specifies properness, complete pricing, off-policy accounting, and exact evaluation of the actual compiled graph. [Publication and Evaluation](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md).

<a id="properness"></a>
## 2. Properness is an entry-scoped support property

In a finite fixed-policy graph, a non-goal bottom strongly connected component reachable with positive probability traps some execution mass. Such a component makes goal absorption fail. Conversely, if every reachable bottom component is an absorbing goal component, a finite chain reaches a goal almost surely.

The graph used here must contain **all positive-probability outcomes**. A missing low-probability edge may be the only edge to a failure component. A default route or evaluator refusal is not an absorbing success.

The entry qualifier matters. A compiled graph may contain an improper component unreachable from its original root. Root evaluation can still be proper. Asking for a continuation upper at an entry inside that component requires a new entry-specific argument and should fail.

A simple graph demonstrates the distinction: root \(r\) goes to the goal for cost 1; a separate node \(z\) loops forever. The policy is proper from \(r\), not from \(z\). The existence of both nodes in one serialized strategy does not extend root authority to \(z\).

Likewise, the fact that a strategy router refuses an off-policy item establishes a limitation of *that strategy*. It does not establish the native item's infeasibility and does not forbid lower-only analysis at the item. [Lower-only frontier proof](lower-bounds.md#frontier).

A finite vector saved beside a root policy does not expand that policy's entry
domain. For example, a cost-3 action from r to a true goal is a complete policy;
a separate unresolved state x may carry a lower estimate 0. A cost-1 alternative
from r to x then has optimistic one-step value 1, but supplies no executable
improvement until x has a complete proper continuation. Upper-policy reuse must
mark the unselected domain unknown and retain the first verified artifact while
testing that alternative. The [goal-reaching delivery application](../../active/2026-09-10-goal-reaching-row-delivery/README.md)
exercises this boundary with native rows and independent emitted-graph evaluation.

<a id="proper-seed"></a>
### Constructing a proper seed from complete rows

On a finite fully observable domain D, let G contain true goals and only those
additional entries with compatible, committed, independently executable proper
continuations. Unknown states, heuristic values and implementation ceilings are
not terminals. For X contained in Y, APre(Y,X) contains a state with an allowed
complete row whose positive support stays in Y and has a positive edge into X.
The almost-sure winning region of this supplied action view is

\[
W=\nu Y.\mu X.(G\cup\operatorname{APre}(Y,X)).
\]

Start Y at D, grow X from G using that predecessor rule, replace Y by X and
repeat until stable. The final inner construction records a selected row and
rank for each nonterminal. Outcomes may return to the same or a higher rank;
all remain in W and at least one positive outcome reaches a lower rank.
If the selected controller had a non-goal bottom SCC, a minimum-rank member
would have a lower-rank edge leaving it, a contradiction. Its finite chain is
therefore proper. Conversely, a proper controller's closed reachable region
cannot be removed: each of its states has a finite positive-support path to G.
This completeness argument applies to the supplied finite fully observable
view, not an incomplete native graph or an unproved abstraction.

For post-observation choices, every positive offer needs a permitted safe
decision, and some direct outcome or offer must allow progress. Retain the
actual owner-scoped choices; shared sparse group storage does not identify
independent controller decisions. Indistinguishable observations cannot acquire
different decisions from an unavailable distinction. A later numerical choice
or row change needs another properness check. No tiny positive trap edge may
be discarded. Native routing, numerical evaluation and pricing remain separate
obligations, and unbuilt alternatives remain in the full-scope proof ledger.

A stricter initializer can miss mutual retries. Suppose s has a first cost-1
row to t and another cost-2 row to the goal with probability one-half and t
otherwise; t returns to s for cost 1. Requiring every non-self successor to
have an earlier rank assigns neither state. First-row completion cycles, and
repair that temporarily makes the whole rejected SCC infinite rejects the
mixed escape's infinite one-row Q. Yet the escape/return controller has
J(s)=2+J(t)/2 and J(t)=1+J(s), hence costs 5 and 6.

The [native caller fixture](../../../engine/tests/test_solver_solve.cpp) confirms
that narrow initializer/repair limitation, while the complete existing numerical
seed path and the joint progress seed handle the same example. This does not
attribute every missing policy to that helper. In the September 10
[Ring/Amulet experiment](../../active/2026-09-10-proper-policy-recovery/README.md),
the initial and bounded later complete-row views contained no true goals or
proper committed frontiers. A selector cannot recover a proper controller from
those views. That negative is not native infeasibility.

Native terminal debt is a separate construction question. Satisfying all target
slots can leave extra explicit affixes or the wrong rarity. A work preference
using missing goals, extra affixes and rarity can request cleanup or acquisition,
but it is neither an admissible cost nor a monotone property of all outcomes.
Only the native goal predicate creates a goal leaf. A complete candidate still
retains failures that remove acquired goals, every positive stochastic exit and
the permitted observed choices, then passes the existing properness, compiler
and independent evaluator. The later native delivery application found such
continuations; it does not overturn the earlier goal-free-view falsification
or introduce the removed generic support selector.

Finally, a cost-1 retry succeeding with probability 10^-9 is proper but costs
10^9 in expectation. A support witness is neither an economical policy nor an
upper issuer. The existing numerical/native publication chain must qualify it,
and ordinary cost improvement and all-action proof must remain available.
The nested fixed point may require many passes; no linear-time claim follows
from the graph formulation. The imported [argument and synthetic oracle](../../active/2026-09-10-proper-policy-recovery/research-inputs/research_report.md)
remain research evidence, with their native limits recorded above.

### Saved decisions and continuation authority

A completed operation, its observed-choice payload, prices and immutable row
identity may be retained from one candidate even when a coarse root walk did not
visit that entry. This preserves a candidate decision, not a value theorem at the
entry. Every newly reached continuation still needs complete routing, properness
and evaluation under the original final goal. Copying a later greedy decision
instead changes the controller and invalidates the saved candidate's identity.
Two individually terminating continuations that alternate forever are a concrete
counterexample to treating preserved decisions as an executable upper.

The September 9 preservation-only mutation passed a focused snapshot fixture but
failed its ordinary no-retention qualification at the 90-second finalization
watchdog. It was removed; this conditional argument is retained for the next
bounded continuation repair. See the [failure and patch](../../active/2026-09-09-empty-start-partial-continuation/README.md).

The resumed repair instead discovers missing siblings in a selected candidate's
known prefix and publication kernel. It asks the ordinary work owner to complete
their continuations and retries assembly; it does not fill a saved policy with
new greedy actions. The proof obligation is unchanged: every positive-mass
branch and required observed choice must have a compatible route, then the
assembled controller must pass properness and independent compiled evaluation.
Discovering several missing branches together grants no upper while any remains.
The current [matched evidence](../../active/2026-09-09-empty-start-partial-continuation/README.md)
demonstrates an improved empty-five executable upper, with unchanged lower.

An exact structural parent outside a saved policy's table has no decision in
that table. Any separate fallback witness needs its own domain and identity
check. The parent can still be an entry for **new** continuation construction:
keep the saved controller fixed, obtain a legal complete native row at the
missing entry through the existing strict action owner, and form a candidate
on the resulting reachable closure. The fixed-policy equations apply to this
new controller only after its full nonterminal chain is proper and priced; the old
root value cannot be assigned to the added entry. Complete competing-action
proof remains necessary for any optimality claim. This is construction and
reevaluation, not extension of the old certificate. Fixed-policy lifting may
still refuse the absent decision, while quotient completion can explicitly
discharge it. A parent absent from the calculator requires a separate
correspondence check. A demanded native exact successor can be registered
through the existing complete goal-member and junk-class projection, preserving
rarity, occupancy, fractures, flags and observations under the same caps. This
registers a new coordinate, with no old selected action or value. It does not
establish state-local automatic-action coverage at that coordinate, so the
current implementation keeps global closure open after such growth. A missing
member map, changed terminal/control semantics or a synthetic retry coordinate
cannot use this physical-parent route. [CLM-0021](../claims.md#clm-0021).

<a id="choices"></a>
## 3. Respect when a choice becomes available

Suppose a random observation \(O\) is revealed before an allowed decision \(a\). Optimizing that decision has the form

\[
\mathbb E_O\!\left[\min_{a\in A(O)}q(O,a)\right].
\]

If the decision must instead be committed before observing \(O\), the corresponding expression is

\[
\min_a\mathbb E_O[q(O,a)].
\]

In the simple common-action case, the first is no larger than the second. Extra information cannot worsen an optimal minimization decision.

For equiprobable observations, let action A have costs \((0,10)\) and B have costs \((10,0)\). Choosing after observation costs zero; choosing beforehand costs five. Moving a minimum through an expectation changes the problem.

A lower may grant extra information deliberately, with an optimism argument. An upper must use the actual compiled decision rule and the information available at that point. If newer solver values would choose another offer, evaluating the old fixed strategy must still follow the old rule. Otherwise the evaluator has silently constructed a different policy. [CLM-0003](../claims.md#clm-0003).

The exact evaluator's documented product includes choice and checkpoint state. Its observed-choice representation should be mapped to this timing contract rather than inferred from a grouped row's appearance. [Publication and Evaluation](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md); [GAP-03](../research.md#gap-03).

<a id="programs"></a>
## 4. Programs and options: a stopped-process equation

Let an allowed mandatory program \(o\) start at \(s\), execute primitive work, and stop at the next legitimate decision boundary at time \(\tau\). Its exit distribution and expected internal cost are

\[
P_o(t\mid s)=\Pr_s(S_\tau=t),
\qquad
C_o(s)=\mathbb E_s\!\left[\sum_{n<\tau}C_n\right].
\]

If \(\tau<\infty\) almost surely and the expected internal cost is finite, then for a fixed compatible continuation \(\pi\),

\[
J_{o;\pi}(s)=C_o(s)+\sum_tP_o(t\mid s)J_\pi(t).
\]

This is the law of total expectation, not a new crafting action. Internal resource use must include repeated setup when retries actually reapply it. Every positive-probability exit needs the corresponding continuation. Exit-specific cost/resource correlations must remain consistent with whatever accounting the compiler and evaluator check.

The stop boundary cannot skip a decision the native caller is entitled to take. It may summarize mandatory internal operations of one allowed operator. It must not turn an optional cleanup into a forced one merely to obtain a convenient continuation law.

### Proper options do not imply a proper composition

Option 1 moves \(s\) to \(t\) in one step; option 2 moves \(t\) back to \(s\) in one step. Each option terminates, but alternating them never reaches the goal. Composition needs a proper **global option policy**, not just local exit proofs.

For a finite option graph, complete normalized exit kernels, proper absorption at the goal, and finite expected internal cost per visited option suffice to evaluate the composed policy. An independent flattened primitive evaluation checks the actual artifact rather than trusting informal multiplication of local summaries. [CLM-0004](../claims.md#clm-0004).

### Failed mass cannot be normalized away

Suppose an attempted macro exits to success with probability \(0.9\) and enters a non-goal trap with probability \(0.1\). Dividing the successful exits by \(0.9\) proves something about a conditional execution, not the original program. Neither a finite local estimate nor a label such as “verified fragment” repairs that lost mass.

The benchmark-private fragment work has its own restricted verification and flattening boundary. This chapter states the mathematical obligation for any composition; it does not activate that subsystem or recommend a permanent recipe library. [Source map](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md).

<a id="retry"></a>
## 5. A retry equation must pay for failure

For independent identical trials with cost \(c\), success probability \(p>0\), and an immediate retry after failure,

\[
J=c+(1-p)J=c/p.
\]

If failure first incurs a mandatory recovery cost \(d\), the equation becomes

\[
J=c+(1-p)(d+J)
=\frac{c+(1-p)d}{p}.
\]

The assumptions are substantial: every failure must really reach the same priced retry state after recovery, the trial law must remain the same, and every required branch must be proper. A destructive action whose failure changes blockers, rarity, or retained progress is not an identical retry until that state change is represented or reset at its actual cost.

With \(c=1\), \(p=1/128\), and \(d=2\), free rollback gives 128 while paid recovery gives 382. This is an exact synthetic example, not a native price prediction. The applied-reforge archive uses this kind of distinction to test its lower relation. [Applied-reforge evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md).

A retry formula may also describe a proper policy of an *optimistic lower model*. Its result is then an upper on that auxiliary optimum, not an executable native upper. The model label travels with the number.

<a id="compilation"></a>
## 6. Why the emitted strategy is the artifact that must be evaluated

A solver policy and its compiled router can disagree in ways that a policy-value equation will not catch. The router might omit a physical member, merge distinct conditions, choose a different default, or lose checkpoint/offer information.

Therefore the certificate chain has two separate obligations:

1. The abstract or strict policy used for proof has the stated semantics and value.
2. The **actual emitted strategy** follows that policy over every execution state reached from the certified entry, with the same priced primitive actions and terminal predicate.

The existing pipeline compiles, parses, and independently evaluates the returned graph. The documented output is the evaluated artifact, not a new recompilation of an older policy after the check. [Publication and Evaluation](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md).

Strict proof can establish an optimum while a bad router fails to execute it. Conversely, a beautifully executable policy may be suboptimal. Both directions must be tested at their own boundary.

Exact evaluation also does not imply that its decimal rendering is an exact rational answer. The [numerical chapter](numerical-closure.md#layers) separates graph-level completeness from arithmetic representation.

<a id="improvement"></a>
## 7. A local deviation is a proposal until its full policy is checked

When compatible fixed-policy continuations are available at every successor, one can evaluate

\[
Q_\pi(s,a)=c(s,a)+\mathbb E[J_\pi(S')].
\]

If this is below \(J_\pi(s)\), it identifies a potentially useful deviation. Replacing decisions in a cyclic policy can change visits and recurrence. The new controller still needs an allowed observation/memory interpretation, complete routes, properness, and independent evaluation. A one-time deviation can require an extra “already deviated” memory bit; it is not necessarily the same as choosing \(a\) on every future visit.

If a successor has no compatible fixed-policy route, \(Q_\pi\) has not been established. That is not evidence that \(a\) is worse. An executable continuation can be sought independently, or a lower can reason about the successor without an incumbent route.

Any new qualified strategy changes policy-tied entry values and certificate identities. Old optimality arguments about \(J_\pi\) cannot silently become arguments about \(J_{\pi'}\).

<a id="return-bridges"></a>
### Constructive return domains and complete excursions

Fix a finite native controller \(\beta\) which stops at the true goal or a
declared entry \(z\) of a fixed, independently proper old policy \(\pi\).
An entry includes the exact item, operation, controller phase, checkpoint and
observed-choice context. A root certificate or a node's cost annotation is not
an arbitrary-entry certificate. Every positive-mass exit must have a compatible
executable route, complete prices and finite continuation cost.

If the stopping time \(\tau\) is almost surely finite with integrable internal
cost, define \(r_\beta(x)=\mathbb E_x[\sum_{t<\tau}c_t]\) and
\(K_\beta(x,z)=\Pr_x(X_\tau=z)\). Conditioning on the first exit gives

\[
J_{\beta;\pi}(x)=r_\beta(x)+\sum_z K_\beta(x,z)J_\pi(z).
\]

Goal exits contribute zero. For a finite transient interior \(R\), the existing
fixed-policy equations give \(r=(I-P_{RR})^{-1}c_R\) and
\(K=(I-P_{RR})^{-1}P_{RD}\). These summarize one complete selected controller;
changing any action or observed choice invalidates its stopped law. A physical
visit to an entry during mandatory program work is not a control return.
This is an application of [CLM-0002](../claims.md#clm-0002),
[CLM-0003](../claims.md#clm-0003) and [CLM-0004](../claims.md#clm-0004), with no
lower, action-retirement or all-action optimality authority.

One constructive native domain is an ordinary Rare item with no fractured
affix, locked side, unresolved offer/checkpoint or incompatible persistent
context, together with an independently certified identical empty-Rare entry.
The native Annul application and calculator both choose uniformly among
eligible affixes; `pc_item_remove_at` removes one slot without changing rarity.
Thus stop at a true goal or an approved entry, otherwise pay for Annul and
retain every removal outcome. Remaining affix count strictly decreases, so
the fallback reaches that anchor in at most the initial affix count. Losing a
desired modifier remains a paid branch. A concrete six-affix input has at most
64 deletion subsets; this is not a bound on the union of a broad reforge's
outputs. Every represented class member still needs coverage or a proved
common law under [CLM-0006](../claims.md#clm-0006).

This argument does not admit raw Annul salvage on compressed zero-progress
reforge carriers. Existing retry restrictions and mandatory first-exit programs
remain in force. Fractures, locks, differing flags/influence/implicit context,
or an unavailable exact empty-Rare route refuse this witness. Normal-item
Annul and double-lock Scour are outside the argument. The supplied
[research and native application](../../active/2026-09-11-first-return-improvement/README.md)
separate this conditional construction from its measured native eligibility.

<a id="first-return-improvement"></a>
### One-shot and repeated return controllers

Choose one exact decision entry \(s\). Begin the proposed trial, complete its
bridges, then follow frozen old decisions until the first return to precisely
\(s\) or the true goal. The initial trial is not an immediate time-zero return.
Suppose this complete excursion is transient, has finite expected cost \(r\),
return probability \(q\), and goal probability \(1-q\). With
\(J=J_\pi(s)\), taking it once and then using the old policy costs

\[
Q_{\mathrm{once}}=r+qJ.
\]

The one-shot graph stores the phase distinction: after entering old \(\pi\),
revisiting the physical item does not restart the trial. A separately constructed
controller which repeats the same excursion at each return is proper when
\(q<1\), and has

\[
J_{\mathrm{repeat}}=\frac{r}{1-q},\qquad
J-J_{\mathrm{repeat}}=\frac{J-Q_{\mathrm{once}}}{1-q}.
\]

The multiplier is the new excursion's return law, not old-policy occupancy.
For example, \(J=100,r=0.5,q=0.99\) gives one-shot cost 99.5 and repeated cost
50. A material-improvement threshold applied only to the one-shot gain would
miss this candidate. For at most \(k\) attempts followed by old \(\pi\), the
cost is \(r(1-q^k)/(1-q)+q^kJ\); the emitted graph must actually remember the
attempt count. A bounded exact entry set instead has \(u=r+Ku\), requiring
transience of its complete selected return matrix.

At \(q=1\), a one-shot trial may terminate through old \(\pi\) while repetition
never reaches the goal. A cost-one trial followed by cost-one return and an old
cost-ten finish has one-shot cost 12 and improper repetition. Unknown positive
mass, even tiny, cannot be dropped or normalized away. An uncompetitive chosen
bridge rejects that controller, not the underlying action. Near-one return
probability amplifies numerical error; no convenient epsilon establishes
\(q<1\). The existing coefficient, mass, properness, residual and full emitted
controller reconciliation remain necessary. A private absorbing return used
to measure \(q\) is not crafting success and must not alter the published goal.

The supplied exact rational checks cover finite synthetic compositions and a
separate deletion-order oracle. They test these formulations, not native
mechanics, whole-engine correctness or a performance gain; no new accepted
claim is introduced.

The native Ring application distinguishes an unknown masked deviation from an
unroutable physical tail. Its one uncovered coarse successor has one desired
FireResist8 suffix. An actual-item request independently certifies the old graph
there at \(J+9.69\): the existing router already pays Annul, loses that desired
modifier and returns to the identical empty Rare. All eligible one-affix members
share this deterministic removal law; this common law, rather than the one
requested representative, establishes the bridge's class coverage.

With the same frozen current-run old entry, the complete native excursion gives
\(r=11.597499232757386\), \(q=0.9999226716107862\), and
\(Q_{\mathrm{once}}=938003.6652244877\) against
\(J=938064.6067502735\). Its separately emitted repeated controller evaluates
to 149977.25092497544 with complete cost and zero off-policy mass. This is a
native example of amplification, not a lower-bound or exact-closure result.
The material comparison still uses the stronger frozen Ring reference
204763.14825000268. Amulet's Exalt repeats its old policy and offers no gain;
the additional admitted gated-reforge construction reaches the unchanged
200,000-state checker cap on both families. Complete bridge rows alone therefore
do not establish a finite excursion law or a better reforge controller.

<a id="mapping"></a>
## 8. Correspondence and limits

<a id="dirty-guidance"></a>
### Dirty continuation entries and complete response costs

Candidate availability precedes guidance. If a builder presents only
\(C(s)\subseteq A(s)\), even a perfect ranking can choose only inside \(C(s)\).
Expanding that set can improve a restricted controller without changing any
certified lower. A completed fixed-controller equation \(J=c+PJ\) evaluates
that controller; it does not label the full-scope optimum \(V^*\).

A private prediction may be calibrated against an independent native check of
the **same frozen controller and entry**. A composed root result cannot label
an earlier local entry, and resource-censored work is not a high-cost sample.
Bounded residual correction can order the next construction/check obligation;
it cannot supply a boundary tail, justify permanent pruning or mutate a graph
while it is being checked. Static and adaptive comparisons need identical
candidate eligibility and machinery. These distinctions and the exact-rational
counterexamples are preserved in the [v2 mathematical input](../../active/2026-09-12-adaptive-dirty-guidance/research-inputs/mathematical_handoff.md).

For spend attribution, use expected visits times the native immediate priced
action cost. Visits times continuation cost overlap across successive entries.
Exact policy savings use the new-controller occupancy in the
[policy-difference identity](#first-return-improvement); old-controller visits
are only an ordering proxy. None of these observations changes CLM-0002's
entry, properness, scope or complete-cost preconditions.

A useful partial item may remain dirty between actions. Only the original
terminal predicate determines success; an intermediate clean subgoal is an
additional policy restriction unless its equivalence has been established.
Paid removal can lose useful progress. Continued acquisition can exhaust space
or change the native pool, so neither cleanliness nor goal count alone gives an
economic ordering.

For a fixed local controller on internal states \(R\), with actual executable
boundary entries \(B\), assume almost-sure exit and finite expected internal
cost. Its Bellman equations give
\[
g=(I-P_{RR})^{-1}c,\qquad H=(I-P_{RR})^{-1}P_{RB},\qquad J(v_B)=g+Hv_B.
\]
These expressions denote sparse linear solves, not a dense inverse to construct.
Complete boundary coverage gives row exit mass one. A changed boundary value may
reuse unchanged \(g,H\); changed internal decisions require reevaluation. If new
decisions control returns among several boundaries, their complete joint system
must also be proper: locally exiting components can form a non-goal cycle.

An exact nonempty item/control entry with independently evaluated continuation
is sufficient for that entry. A goal mask or one materialized class member is
not a uniform class certificate. A private-layout controller can instead be
compiled, evaluated over its complete native operation/item graph and retained
as an original-scope root artifact. This grants no parent statewise values or
private state-ID correspondence. Reuse must bind the complete graph, actual
entry, terminal semantics, scope, vocabulary, mechanics and prices.

One-shot advantage also need not rank repeated controllers. At old cost 10,
an excursion with success probability .9 and repeated cost 8 gains 1.8 in a
one-shot comparison; one with probability .01 and repeated cost 1 gains only .09.
The second repeated controller is cheaper. This follows directly from the
[existing identity](#first-return-improvement), not a new accepted claim.
The [supplied argument and rational checks](../../active/2026-09-11-dirty-state-continuation/research-inputs/review_and_mathematics.md)
also cover the complete multi-entry response; native correspondence and current
qualification remain in the programme's living record.

`IncumbentPortfolio` separates estimates from executable candidates. The compiler, policy assertion, and evaluator establish different parts of the upper chain. The source contract binds target, economy, action scope, graph identity, and relevant generations. [Executable Upper Authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/upper-authority.md).

The candidate-continuation lifecycle can retain partial construction work without granting it upper authority. The archive of released-candidate reclamation records a case where a refused candidate retained memory and suppressed ordinary work; fixing that lifecycle restored useful policy discovery. That is a progress/performance finding, not a different upper theorem. [Reclamation evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md).

[CLM-0002](../claims.md#clm-0002), [CLM-0003](../claims.md#clm-0003), and [CLM-0004](../claims.md#clm-0004) hold the reusable statements. [GAP-03](../research.md#gap-03) and [GAP-05](../research.md#gap-05) retain the general native correspondence and numerical reconciliation obligations. The linked native applications establish only their recorded request and controller scopes; they do not close those general obligations.
