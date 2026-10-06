# Executable policies, properness, and upper bounds

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


A policy upper is a witness of what can actually be executed. It is not a promise that the action sequence is optimal, and it is not the value of a convenient projection unless that projection has been connected to native execution.

A generated finite controller is still just a candidate. Its predicates must
use observable native item facts, its success route must test the original goal
with executable edge semantics, and every positive-mass operation outcome must
have a complete continuation. A default edge with a decorative goal predicate
does not test that goal. Only independent evaluation of the complete paid graph
from the original root supplies its executable upper; a search score or
successful local program check does not.

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

The transition law must belong to the selected executable programme throughout
construction and publication. A primitive followed by mandatory paid recovery
has the programme's exits, including the recovery endpoint, rather than the raw
primitive's intermediate failed items. Recovery costs and primitive executions
remain in its rewards and compiled operations. Mixing the programme's priced row
with the raw primitive's exits describes a different controller. The native
[product Fracture publication repair](../upper-authority.md) applies this
correspondence at its existing hit/replacement boundary; it supplies neither
missing continuation values nor a new properness theorem.

<a id="policy-difference"></a>
### Cost attribution and a changed controller

For the evaluated controller, let \(d_\pi^\top=e_s^\top(I-P_\pi)^{-1}\).
Then \(J_\pi(s)=\sum_t d_\pi(t)c_\pi(t)\). Aggregate by priced action or
retry region only after retaining the operation, item and controller-memory
identity. Compiled node counts and source `expected_cost` annotations are not
independent occupancy or entry-cost evaluations. The old controller's largest
cost contributions can guide which alternative to investigate; they do not
certify how much a changed controller saves.

Expected visits are not probabilities of ever using an operation: retries can
make an occupancy count exceed one. A stage's root-weighted expenditure is not
its conditional entry cost. Low observed spend on an action does not bound its
effect on later costs. Cheaper acquisition can increase visits to an expensive
completion stage, so replacing acquisition alone need not improve root cost.
Any changed controller needs its own full paid continuations, properness and
occupancy evaluation. The [October decomposition](https://github.com/OliverOrton/poecraft2/blob/040993ec9bb8109cc1970df91c8bc9ec7ef97744/docs/active/2026-10-04-sol61-armour-recovery/PRO-HANDOFF.md) illustrates
this distinction, but its incomplete experimental entry admission supplies no
publishable upper or measured recovery of the released incumbent.

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

There is also a first-disagreement form when two complete proper controllers
share a truly aligned native prefix, including item and controller memory.
Stop that common process at the goal or the first legitimate decision
disagreement \(D\). If \(\mu(d)\) is the first-entry subprobability of a
disagreement boundary, conditioning on that first exit cancels the common
prefix cost:

\[
J_A(\mathrm{root})-J_B(\mathrm{root})=
\sum_d\mu(d)\bigl(V_A(d)-V_B(d)\bigr).
\]

For a cyclic common prefix, the existing proper finite stopped-chain equations
give its first-exit law. This is not old-controller occupancy times a tail
difference; mass reaching the goal first contributes zero. Mandatory programme
interiors and misaligned observation memory cannot be treated as free decision
boundaries. This conditional attribution neither constructs a new native policy
nor establishes common search traces from equal row counts.

Because \(\pi\) is one allowed proper policy,

\[
V^*(s)\le J_\pi(s).
\]

This subset argument is the entire reason a fixed-policy evaluation supplies an upper. No greediness premise is required. [CLM-0002](../claims.md#clm-0002).

<a id="complementary-decisions"></a>
### Complementary decisions in a bounded proposal search

Consider complete deterministic states r, t and goal g. The incumbent chooses
r->g at cost 10 and t->g at cost 100. Alternatives r->t at cost 1 and t->g at
cost 1 give these root costs:

| Changed decisions | Root cost |
|---|---:|
| None | 10 |
| Bridge only | 101 |
| Tail only | 10 |
| Bridge and tail | 2 |

All four controllers are proper. The cheaper tail has zero incumbent occupancy,
so demanding immediate strict root improvement from each isolated edit can reject
both useful ingredients. Joint completion exposes the gain. This is a limitation
of that bounded proposal filter, not of full-domain policy iteration, which can
improve t before r. It authorizes neither arbitrary family expansion nor copying
the old root value onto an unknown tail. Native applications still need every
positive outcome, observed decision, paid setup/cleanup and compatible entry
under [properness](#properness), then complete original-root evaluation. The
[policy-difference identity](#policy-difference) uses the new controller's visits.
The imported [exact toy checks](../../active/2026-09-22-ordinary-capability/research-inputs/package/checks/check_concepts.py)
illustrate the argument; they establish no native crafting correspondence.

<a id="paid-root-selective-add"></a>
### A fixed selective add before paid root renewal

Fix one exact original root and a complete legal renewal law. The roll costs c,
succeeds with probability p0 > 0, and otherwise reaches observable miss classes
with positive masses wi, where p0 + sum(wi) = 1. From every miss, a legal paid
reset of finite cost r returns deterministically to the **same exact root**.
For a selected class i, one legal add costs ai and has a complete native law:
it succeeds with probability pi, and every positive failure legally resets to
that same root for r. Costs are finite and nonnegative. Selection is fixed before
execution, uses exact observable classes, and permits at most one add per roll.
Root identity includes persistent item context and controller phase; returning
to a similar goal mask or borrowing the old root scalar is insufficient.

The baseline satisfies U0 = c + (1-p0)(r+U0), hence
U0 = [c+(1-p0)r]/p0. For a fixed subset S, first-cycle conditioning gives

\[
U(S)=\frac{c+\sum_{i\notin S}w_i r+
                 \sum_{i\in S}w_i[a_i+(1-p_i)r]}
                {p_0+\sum_{i\in S}w_i p_i}.
\]

Every cycle succeeds with probability at least p0, so this finite controller is
proper under the stated reset laws, including when an add costs zero. Comparing
its numerator with U0 times its denominator yields

\[
U(S)-U_0=
\frac{\sum_{i\in S}w_i[a_i-p_i(r+U_0)]}
     {p_0+\sum_{i\in S}w_i p_i}.
\]

Selecting only positive-mass classes satisfying ai < pi(r+U0) therefore strictly
improves the baseline whenever the selected subset is nonempty. This sufficient
fixed-subset rule does not assert the optimal subset or the unrestricted MDP
optimum. Incomplete laws, hidden observations, unpaid or wrong-root recovery,
and repeated adds invalidate this derivation.

The [private Foulborn application](../../active/2026-10-02-foulborn-sprint/README.md)
uses Alchemy, one selected Foulborn Exalt, and paid Scour. The equation proposes
a graph; the independent original-root checker must still establish complete
native execution, original-goal success, properness and reconciled cost. It
supplies no non-root statewise value or lower authority. Its supplementary
grammar lies outside the old zero-progress-reroll-only policy restriction.

#### No direct roll success

The same fixed-subset equation remains valid with p0 = 0 when the complete
selected cycle has q = p0 + sum(i in S, wi*pi) > 0. Keep all the exact-root,
observable-selection, complete native law and paid-recovery premises above.
Each failed cycle returns to the identical root and phase, so the probability
of surviving k cycles is (1-q)^k and the expected number of cycles is 1/q.
The controller is proper and its cost is the displayed cycle numerator divided
by q. A roll that cannot directly meet the requested rarity can therefore have
a valid paid-root controller whose add supplies the positive success mass.

There is no finite reset-only baseline when p0 = 0. Do not divide by p0 or apply
the improvement comparison to a fictional U0. The v2 proposal selects all legal
positive-success add classes with verified paid recovery in this case, and
refuses an empty selection or q = 0. This constructs one proper proposal; it
asserts neither the best subset nor an unrestricted optimum. When p0 > 0 the
original sufficient improvement test above remains unchanged.

The [ordinary completion](../../active/2026-10-02-foulborn-completion/README.md)
applies this extension to descriptor-selected Transmute/Foulborn Augment or
Regal/Scour controllers, alongside Alchemy/Foulborn Exalt/Scour. It versions the
grammar, cache and checkpoint identity. The independent original-root checker
still verifies the executable graph, complete law, original goal and reconciled
cost before publication; the equation supplies no lower or non-root statewise
authority. Native correspondence and the extension are subject to integration
owner review of the passing checkpoint.

<a id="primitive-execution-reward"></a>
### Primitive execution count is a separate reward

Fix the same complete, proper finite controller and transient nonterminal
matrix P. Let c be expected native monetary cost per represented decision and
n the expected number of primitive executions before that decision's exit.
Then C=c+PC and N=n+PN, so C=(I-P)^(-1)c and N=(I-P)^(-1)n. This is the
fixed-policy and stopped-program argument applied to two rewards, under
[CLM-0002](../claims.md#clm-0002) and [CLM-0004](../claims.md#clm-0004).

An option can execute many primitives. Its n follows native
`OptionKernel::expected_primitive_actions` and exactly the first-exit/retry
normalization of its successor law and resource reward. One option invocation,
one graph traversal and one currency unit are different quantities. Correlation
between internal work and exit does not invalidate the stopped expectation;
linearity suffices for this same committed controller and complete law.
Independent physical evaluation owns the published controller's actual count
and cost under the existing numerical contract.

At entry s, d^T=e_s^T(I-P)^(-1) gives C(s)=d^T c and N(s)=d^T n. These immediate
contributions may be aggregated by action or compatible native region.
Continuation values and counts already include future work; adding them as
separate contributions double-counts overlapping tails.

A one-state retry with success probability p has expected count 1/p. Structure
and properness therefore do not imply short execution, and a long mean does
not prove that a shorter allowed controller exists. The mean alone also does
not determine finite-limit completion. Preserve capped sampling trials in the
denominator and keep them separate from native unlimited expected totals.

For finite-action-budget distributions, retain the joint duration/exit law:
with generating functions H(z) for nonterminal exits and g(z) for goal exits,
first-exit conditioning gives F(z)=g(z)+H(z)F(z). Separate means do not determine
this law or its completion CDF. This is research mathematics, not an implemented
issuer, a deadline guarantee or a change to the original-cost objective.

The [execution-aware application](../../active/2026-09-13-execution-aware-proposals/README.md)
uses accounted private cost/count rewards and the existing sparse policy
owner. Complete rows survive changed proposal weights; numerical results do
not. The supplied [derivation and counterexamples](../../active/2026-09-13-execution-aware-proposals/research-inputs/review_and_mathematics.md)
are explanatory evidence, not whole-engine correspondence or a new claim.

For the selected fractured-Magic acquisition family, the native redraw retains
one satisfying fracture. In the [native reforge owner](../../../engine/src/solver_reforge.cpp),
`base_satisfied_count >= 1`, so every roll has nonzero total goal progress and
the zero-progress retry mass is zero. The composed domain still uses the
caller's gated law, including terminal aggregation and scope metadata. Native
physical/gated mass checks cover this retained-fracture case. This conditional
argument neither removes the caller restriction nor permits entering a
mandatory program midway through its execution.

### Why an equation solution is insufficient

For a zero-cost non-goal self-loop, the equation is \(J=J\). Every finite number solves it, but the controller never finishes. Algebra does not establish properness. A numerical solver returning a finite vector cannot turn this into an executable upper.

The implementation must check support and absorption separately. The current publication contract specifies properness, complete pricing, off-policy accounting, and exact evaluation of the actual compiled graph. [Publication and Evaluation](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md).

<a id="properness"></a>
## 2. Properness is an entry-scoped support property

In a finite fixed-policy graph, a non-goal bottom strongly connected component reachable with positive probability traps some execution mass. Such a component makes goal absorption fail. Conversely, if every reachable bottom component is an absorbing goal component, a finite chain reaches a goal almost surely.

The graph used here must contain **all positive-probability outcomes**. A missing low-probability edge may be the only edge to a failure component. A default route or evaluator refusal is not an absorbing success.

The entry qualifier matters. A compiled graph may contain an improper component unreachable from its original root. Root evaluation can still be proper. Asking for a continuation upper at an entry inside that component requires a new entry-specific argument and should fail.

A numerical root evaluation also does not replace the native correspondence
checks required by the producer's acceptance contract. When every strictly
positive reached programme entry must be admitted, a capped census or validator
leaves that obligation incomplete even if the root equations are proper and
converged. Do not infer admission of unchecked entries, drop tiny positive mass,
or pair a diagnostic scalar with an unqualified artifact. This is an acceptance
refusal, not proof that the proposed native policy is mathematically invalid.

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

Within an immutable native row/price/control domain, an unchanged complete
root-reachable selected row vector defines the same transient fixed-policy
equations and the same entry cost. Adding unselected alternatives or unreachable
router nodes does not invalidate that root check. The reachable support must be
recomputed from every positive selected transition; equal estimates or goal
masks alone do not establish equality. No entry beyond the checked domain gains
upper authority. The [selective application](../../active/2026-09-13-selective-continuation-improvement/README.md)
uses this narrow identity to avoid repeated checks of unchanged controllers.

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

The native temporary-bench option can include paid Multimod before a Cannot
Roll blocker, followed by Exalt and removal of both crafts. Capacity and pool
filtering interact: filling the opposite side can change the draw even when
Multimod supplies no desired modifier. Four paid primitive operations remain
four executions, regardless of the number or quantities of price keys. This
program has a distinct option/template identity from a single blocker. Native
attempt legality, complete exits and cleanup still decide admission; a promising
pool census alone supplies no executable upper. The
[cross-base application](../../active/2026-09-14-cross-base-strategy-recovery/README.md)
records the full-root comparisons and their differing economic results.

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

To distinguish acquisition rarity from later loss, stop the fixed controller at
exact compatible decision entries. If acquisition A to B costs a, B's complete
tail costs b and returns only to identical A with probability 1-p (otherwise
the true goal), then C(A)=(a+b)/p. The same equation holds for the separate
primitive reward. With multiple physical or controller contexts, retain the
return matrix and solve the vector equations instead. A changed recurring
continuation changes that matrix and the original-root occupancy; old visits
times a local saving is not an acceptance calculation. Diagnostic STOP nodes
are never true goals or executable tails.

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

A bounded constructor may cover fewer entries than the native mechanics permit.
Requiring every goal slot on a held side before constructing an Eldritch growth
word can miss a legal partial-held continuation. The [October causal review](https://github.com/OliverOrton/poecraft2/blob/4c9f078d95e2c0c837ab7a408c4846420bac94db/docs/active/2026-10-04-sol61-independent-review/causal-research-handoff.md)
demonstrates one positive original-root carrier whose historical Ember/Exalt word
is legal and preserves its incoming affixes, while the released bounded grammar
routes it to acquisition. This proves a local composition omission, not absence
from all search, a missing primitive, whole-policy economic recovery or the
introducing change. A generic extension must retain exact item/tier/blocker and
capacity guards, every positive exit and paid setup/recovery. Modifier-family
presence or a goal mask alone cannot establish that a desired tier is rollable.

### A native word needs an economically complete complement

The [four-report synthesis](../../active/2026-10-04-sol61-armour-recovery/README.md)
separates a legal partial-held word from a useful complete controller. At the
positive mask-9 Conquest carrier, paid Ember tier 1 then Eldritch Exalt preserves
all four incoming affixes. The below-tier hybrid still excludes its desired
same-family tier, and the released router still sends missing T1 suppression to
Chaos. Returning all 33 native exits to that router therefore does not establish
a saving. Native word legality, source-local admission, compatible exit tails,
proper full-root evaluation and retained export are separate premises.

The matched October 5 Current control finds that exact carrier's parent
projection and its admitted cached native word, while the checked returned
graph still routes it to Chaos. Thus native enumeration/admission and useful
composition are distinct observed boundaries. The one-use value is
\(g+\sum_i p_i V_\pi(s_i)\) only when every \(V_\pi(s_i)\) certifies the
actual item/control entry after the paid word. The checked root scalar
\(J_\pi(r)\) cannot fill those ports. Persistent implicit context can survive
Chaos; reaching the same first paid operation does not prove equal tails.

Current's root-only selective graph deliberately supplies no parent statewise
policy/value certificate. In the matched control, 34 aggregate upper-seed
requests start none, and the joint-assembly counter is zero. Source requires
statewise values or a separate focused fallback for that seed; its exact
historical refusing branches and P0-specific row service were not observed.
The source-a9aadac6 finite checker now certifies the original root, P0 and all
33 exact physical exits with complete priced Current tails. Paid word cost
3.7463 plus those tails is 101314.94475391766, versus P0's checked Current
continuation 101311.35474896753: a loss of 3.59000495013606. All exits retain
the persistent Ember context and have checked tail 101311.19845391765; the
root scalar was not substituted for those ports. This preserves the negative
one-word graft hypothesis and requires a useful complete complementary tail
before an economic treatment. It is not a recurring policy value or complete
product coarse-domain certificate.

A separate full-identity role fixture names the existing refusal
seed_root_only_incumbent_without_focused_fallback: one request, no started pass,
one rejection, checked fallback preserved. It does not reconstruct the product
control's 34 unrecorded refusing branches or prove whole-search cause. The
earlier build negatives remain in the evidence history.

An ordinary fill versus reroll comparison must use each action's complete exit
law and complete compatible tails, including lost goals, mandatory cleanup and
paid recovery. Primitive price, goal-mask gain or old-policy visit counts alone
do not determine the new controller's cost. A bounded producer may select a few
alternatives without claiming full action coverage; all unserved branches and
failed hypotheses remain visible.

<a id="inevitable-renewal"></a>
### Deleting work before an inevitable renewal is conditional

Let A be a mandatory paid Annul, followed by a mandatory Chaos C. If every
positive post-A physical/control state has exactly the same native C exit
kernel K, C resources and compatible continuation context as C at the original
entry, then K_A K_C=K: the A probabilities sum to one, and all rows of K_C
in that support equal K. Removing A leaves the complete exit law unchanged and
removes its nonnegative paid cost and one primitive execution.

This premise also requires no intermediate true goal, retained caller choice,
checkpoint, observation-dependent route, or cleanup/recovery obligation.
Fractures, locks, rarity changes, preserved modifiers, affix-dependent draw
rules and changed control context can invalidate it. A generic Annul followed
eventually by Chaos is not sufficient. The private full-junk finisher in the
economics report is a candidate application; current-native kernel equality
and complete full-root economics remain unestablished. This is a conditional
application of CLM-0002/CLM-0004, not a new runtime issuer or activation.

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
[policy-difference identity](#policy-difference); old-controller visits
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

<a id="boundary-response"></a>
For a complete selected controller Q on N=B union I, eliminate the transient
interior through sparse solves:

\[
T=(I-Q_{II})^{-1}Q_{IB},\quad r_I=(I-Q_{II})^{-1}c_I,\quad
H=Q_{BB}+Q_{BI}T,\quad \bar c=c_B+Q_{BI}r_I,\quad V_B=\bar c+HV_B.
\]

Transform each primitive/resource reward and terminal category through the same
stopped law. Interior termination does not imply global properness: H=1 can
repeat forever. Reuse needs unchanged native rows, whole member domains,
observations/routes, hidden context and coefficients. Equal node text is
insufficient; price-only reuse needs price-independent resource responses.
Boundary fill, matching, invalidation, discovery and cold validation count in
the all-in cost. This first-exit application is research, not an implemented cache.

For a proper old/new pair on a common finite semantic domain, write Z=(I-Q)^-1
and d^T=e_root^T Z. A one-row change at i gives, by subtracting the policy
equations and solving the scalar feedback term,

\[
\Delta V_{root}=\frac{d_i(\Delta c+\Delta p^T V)}{1-\Delta p^TZe_i}.
\]

For Q'=Q+ED and c'=c+E Delta c, substitution gives
V'-V=ZE(I-DZE)^-1(Delta c+DV). Both policies must be proper and all old tails
legitimately known. One compiled node may change many physical rows. Expected
visits times remaining cost is exposure, not additive spend or certified savings.
These identities select neither a new ranking nor a numerical issuer.

<a id="rooted-policy-reuse"></a>
An exact nonempty item/control entry with independently evaluated continuation
is sufficient for that entry. A goal mask or one materialized class member is
not a uniform class certificate. A private-layout controller can instead be
compiled, evaluated over its complete native operation/item graph and retained
as an original-scope root artifact. This grants no parent statewise values or
private state-ID correspondence. Reuse must bind the complete graph, actual
entry, terminal semantics, scope, vocabulary, mechanics and prices.

<a id="graph-local-boundaries"></a>
Graph-local decision provenance supplies an observable intervention boundary,
not a state-space correspondence. A compiler declaration remapped with its graph
can let the evaluator discover actual physical entries without importing private
numeric state IDs. Their occupancy ranks proposals; it does not certify a saving.

There are two distinct ways to assess a proposed replacement. The local response
equations above require compatible finite values for every positive-mass boundary.
Alternatively, compile the complete recurring controller and evaluate its full
native item/control graph from the original root. That second route need not
assign a scalar value to a heterogeneous local return class: actual native
outcomes follow the executable old router or complete new recovery. It still
requires the unchanged terminal predicate, all paid setup/cleanup, complete
probability and price, almost-sure goal completion and finite expected cost.
Local option-prefix costs used for compilation bookkeeping are stripped from
composition annotations and have no bound or acceptance authority. Only the
independent whole-controller result enters the verified portfolio.

<a id="implied-entry-predicates"></a>
For a clean intervention whose distinct satisfied goal slots exhaust both exact
side counts, zero junk counts follow from those observations. Exporting separate
private junk-class predicates adds no routing distinction there and can enlarge
the evaluator's global observation partition. The single-pass option compiler
omits only those implied predicates: overlapping goal masks, mixed-side slots,
below-tier or extra affixes refuse the simplification. It emits the unchanged
complete native program, with no hidden offer or mandatory retry, then returns
all actual outcomes through the old global router. Full original-root evaluation
still establishes the recurring controller's properness and cost; this argument
does not justify deleting guards at arbitrary dirty or interior states.

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

<a id="binding-dependent-policy"></a>
### Binding-dependent decisions and stopped attempts

A common decision graph does not carry one policy choice across unequal
bindings. An attempt costing one with identical-state retry probability
\(1-p\) costs \(1/p\) to finish when \(p>0\). Against a guaranteed cost-20
finish, \(p=1/5\) favors retry and \(p=1/100\) favors the guaranteed action.
Each actual binding needs its own probabilities, rewards, action choice and
properness check. Expected primitive count is a separate reward from priced
cost; changing weights need not change both in the same way.

For one complete physical attempt and retry-equivalence set \(R\), let a stop
set \(T\) map an outcome \(x\) to retry exactly when \(x\in R\setminus T\).
If \(R\cap T_1=R\cap T_2\), both stop sets produce the same normalized
outcome map, provided entry, reward, terminal and admission rules are otherwise
unchanged. This sufficient relation explains how extra named stops can leave a
kernel unchanged. The prior A4 aggregate of 828 additional collapses does not
identify each candidate's cause. The [received argument](../../active/2026-09-23-cross-binding/research-inputs/cross-binding-plan-8ac4c72/MATHEMATICS.md)
keeps that observation distinct from parameterized transition reuse.

<a id="bound-role-ports"></a>
### External values in a bound role region

For a fixed selected local controller with internal states \(I\) and explicit
outside ports \(Z\), the binding-specific equation is
\((I-Q_b)v_b=c_b+H_bu_b\). The familiar first-exit response applies only when
the internal block is transient and each value in \(u_b\) belongs to a
compatible actual continuation. A port with no established continuation stays
unknown; an old root scalar or another binding's value cannot fill it.

If an outside controller can return to the local region, independently solving
the pieces with fixed terminal tails can misprice a recurring controller. The
whole composed policy must use mutually consistent values and pass the native
properness and independent evaluation checks. Local exit with probability one
does not ensure global goal reachability: two regions can exit to each other
forever. A shared structural region therefore remains a computation input until
its complete bound policy and output graph pass the existing upper authority.
Sharing a required-field preparation step under the separate
[dependency contract](representations.md#strict-preparation-sharing) does not
transfer any selected action, value or executable upper between bindings.

<a id="root-only-joint-service-prediction"></a>
### Root-only fallback beside a proposed complete native joint policy

A checked original-root controller supplies a feasible fallback at that root.
It supplies no value for another parent state. A separate joint policy may
nevertheless be constructed from complete native rows on every selected
positive successor, then independently checked as a complete controller while
the old graph remains owned under the same cap. Missing support or refused
ownership cannot be filled by assigning the old root cost to the missing port.

For the prepared two-modifier test, predicted native Annul success mass is
one half. If its loss pays Scour 10 and Alchemy 1 to return to the start, then
V = 1 + (11 + V)/2 gives V = 13. If a complete native Exalt cost 1 instead
returns that loss to the start, V = 1 + (1 + V)/2 gives V = 3. This algebra
is conditional on the native support and properness assertions. The source
fixture at 073bb92a is unbuilt and unrun after a startup-guard cancellation;
neither cost is a measured native result. The existing focused seed refusal
is retained and the old P0 word/Current-tail economic negative is unchanged.
The [selected source and receipt](../../active/2026-10-04-sol61-armour-recovery/README.md#selected-root-only-joint-service-source-prepared-build-canceled)
own subsequent build and finite validation status. No matched Current root,
product coarse-domain authority or release saving follows from this toy.

CI later builds the pinned fixture, but its sole finite selector stops before
the baseline graph is independently evaluated. OriginalRootController requires
length-n policy/reachability tables with invalid/zero placeholders; the fixture
instead clears them while retaining n values. That role-contract violation
is a source-determined CompilationFailure, separate from whether the native
recovery graph is proper or economical. Zero of three cases completes and no
cost-13/cost-3 requirement is reached. The
[failure diagnosis](../../active/2026-10-04-sol61-armour-recovery/checks/root-only-joint-native-20261005/diagnosis.md)
proposes correcting placeholder shape while preserving the independent checker,
root-only value role and all previous negatives. No corrected fixture is built
or tested here; the conditional algebra remains a prediction.

Parent subsequently approves the fixture-only placeholder correction at
fdf043ba. Correctly sized invalid/zero tables restore the explicit input role
without granting any nonroot value or action authority. The supplied graph,
independent checking requirement and conditional algebra are unchanged. This
source correction is unbuilt/untested; the original 0/3 failure remains retained.

CI later qualifies the placeholder correction; its sole selector passes the
initial role guard but captures a scope refusal before exact evaluation. A
bounded proof constructed with default options can emit a synthetic Restart
node even when its actual work excludes Restart and its router defaults fail
closed. Caller scope applies to every operation node, including unreachable
defaults. The fixture must carry the actual work options to the existing
compiler; deleting an unauthorized node or relaxing admission is not a scope
proof. The [full contract audit](../../active/2026-10-04-sol61-armour-recovery/checks/root-only-joint-corrected-native-20261005/fixture-contract-audit.md)
also bounds compiler overlap under the original shared cap. Both corrections
remain proposals; 0/3 cases and unproved economic predictions stay retained.

After the approved option/accounting correction, the qualified toy passes its
independent baseline certificate and cost-13 assertion but still completes 0/3
cases. Its four-action constructor has no delayed Essence/Fossil/HarvestReforge
operator, so it closes the incremental envelope. The outer upper-pass guard
returns before seed-role checking or census changes; the joint checkpoint also
requires generation active and the envelope open. [Activation audit](../../active/2026-10-04-sol61-armour-recovery/checks/root-only-joint-options-native-20261005/activation-guard-audit.md)
keeps source-derived predicates separate from uncaptured runtime fields. A manually
opened internal test envelope would establish a conditional service test only,
not normal product activation, treatment economics or whole-search cause.

The explicitly initialized internal service fixture at qualified 5a746193 later
passes all 179 checks: native complete Annul/Exalt checking adopts cost 3 from
checked baseline 13 while retaining the old artifact. Missing positive successor
and shared ownership refusal preserve 13 without queuing a checker. The same test
first proves its actual four-action constructor inactive/closed; it opens the
envelope only in the labeled conditional phase. [Retained receipt](../../active/2026-10-04-sol61-armour-recovery/checks/root-only-joint-conditional-native-20261005/summary.json)
therefore establishes conditional service, full native checking and preservation,
not normal product activation, Conquest economics, whole-search cause or a new
lower authority. Prior losing P0 composition and rejected fixture evidence remain.

The subsequent matched original Conquest treatment at the same native source
does not activate this service economically: checked cost 101311.35474896732 and
the exported strategy are byte-identical to control, with no cap. The 33 upper
requests all reject, final reason
seed_rejected_statewise_values_without_focused_fallback, and no joint attempt or
new joint admission is observed. [Matched negative](../../active/2026-10-04-sol61-armour-recovery/checks/root-treatment-20261005/summary.json)
therefore leaves the conditional theorem/example at its actual internal scope.
An independently checked root graph and its copied statewise value table carry
distinct authority: the sticky reconciliation veto must survive any proposed
graph-only service. Exact active-object lineage and safe graph compatibility are
the selected source audit; the negative is not proof of whole-search cause.

That [source/receipt audit](../../active/2026-10-04-sol61-armour-recovery/checks/root-treatment-20261005/incumbent-lineage-audit.md)
finds two unavailable-statewise roles: an explicitly root-only graph, and an
ordinary independently checked graph whose copied table fails reconciliation.
The primitive graph checks at 627313592.5067186 although its copied estimate
differs by 14.414810538291931. Retained root-only service separately supplies the
101311.35474896732 export winner without replacing the preferred output object.
The final seed refusal identifies the second role; the selected checkpoint
requires the first format. Individual object/compatibility payloads for all 33
refusals were not captured, so a whole-run universal causal claim is unavailable.

Graph-only scheduling may use either representation **only** after the existing
current graph/payload, goal, economy, vocabulary, caller-scope, artifact and
generation checks and independent proper/executable/cost-complete authority.
It does not repair the rejected table or grant arbitrary-entry continuation
values. The native complete-policy builder must keep its existing statewise
frontier guard closed for that table and check every positive successor before
adoption. Relabeling an ordinary graph as root-only would violate the latter's
placeholder/binding contract. At that audit the predicate/queue alternative was
unapplied and untested; any claimed economic gain must beat the
best compatible checked 101311 graph, not merely the much dearer preferred graph.

Oliver subsequently selects this capability-based extension. Source fb59476f
implements it at the same scheduling/checker owner, with fixed owned observations
of every eligibility subguard and current-versus-retained identities. Its new
ordinary-role fixture obtains sticky rejection from actual independent checking
of a native graph against a perturbed **unverified** estimate, rather than setting
verification flags by hand. All prior root-only controls and normal-constructor
negative remain required. [Source/prerequisite review](../../active/2026-10-04-sol61-armour-recovery/checks/checked-graph-joint-service-20261005/fixture-prerequisite-review.md)
was initially unbuilt/untested. Its matching native selector subsequently passes
429 checks across six cases: complete native tails check 13-to-3 in both roles,
missing-support/cap controls preserve 13 without a complete candidate, and the
independently obtained sticky veto and separate checked fallback survive. The
normal-constructor negative remains conditional fixture evidence.

The matching native Current root at fb59476f/51855004 then demonstrates actual
graph-only checkpoint compatibility and 36 joint attempts, but no complete new
joint candidate or economic improvement: checked 101311.35474896732 remains
byte-identical to control. [Boundary audit](../../active/2026-10-04-sol61-armour-recovery/checks/root-checked-graph-treatment-20261005/continuation-boundary-audit.md)
records the last missing outer state11735/mask16 with no owned completed row or
certified frontier. The rejected-table guard correctly supplies no numerical
boundary there. A proper checked root graph proves only its bound entry; an
arbitrary successor requires an exact physical/control entry mapping and a
complete checked paid controller, or newly completed native rows. Mask equality
or a root scalar does not prove that mapping. This receipt has no physical state
or incoming controller cursor for 11735, so no reusable tail at that entry is
accepted.

Source distinguishes scheduling from proof: `continue_initial_candidate()`
bypasses direct missing-entry refinement after a checked-graph build fails,
whereas its no-incumbent branch invokes that existing owner. Actual last-cohort
requests remain open during automatic synthesis until bounded Finish. The
`service_completions` diagnostic counts selection/retirement, not completed rows
or entry certificates. A second source distinction concerns retry: the graph-only
checkpoint counts `incremental_alternative_rows`, and missing-support failure sets
its next checkpoint to that count plus one. New ordinary completed rows can supply
the required entry evidence without increasing this separate count. Exact service
dispatch therefore does not alone demonstrate a subsequent assembly/check.

The [finite native falsification](../../active/2026-10-04-sol61-armour-recovery/checks/root-checked-graph-treatment-20261005/finite-service-falsification-plan.md)
keeps every positive successor, paid recovery, sticky veto, one-shot checking,
fallback ownership and TargetNeutralZero fixed. Test-only source 87bf8020 preserves
all six controls and adds two real native missing-entry counterparts. It asserts
actual ordinary ledger row completion, separately observes retry refusal, then
uses explicit existing assembly/checking and complete checks at both physical
entries. The direct interventions grant no production scheduling authority.
The predicted 24-to-14.5 composed cost and 13.5 entry tails remain unbuilt/untested.
[Source prerequisites and exact CI pins](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-missing-entry-source-20261005/fixture-prerequisite-review.md)
grant no numerical authority or useful product tail. Production is unchanged;
earlier negatives, original pins, whole-search/introducing-commit uncertainty and
general claim status remain.

The 87bf8020 finite control subsequently demonstrates baseline 24, both actual
Regal ledger completions and withheld retry, then refuses explicit complete
assembly; it never reaches challenger or entry checking. [Prerequisite audit](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-missing-entry-diagnostic-source-20261005/assembly-prerequisite-audit.md)
distinguishes that refusal from two-row availability. The ordinary owner admits
all legal actions at each entry, including Annul toward nonterminal Magic-empty.
The builder may improve its initial policy over those rows before publication;
each selected new positive successor still requires owned complete rows or a
compatible certified frontier. Neither zero working value nor the checked root
graph supplies that frontier. This graph-only role also fails the statewise
precondition for frozen resumable-prefix capture. A fixed paid-controller cost
prediction therefore does not imply closure or delivery by this builder.

The exact final refusal was not recorded. Diagnostic-only source 63bb3b23 preserves
the acceptance assertion and logs its actual inputs/failure; it is unbuilt/unrun.
14.5 and both 13.5 entry claims remain unverified, case 7 remains unrun, and no
production defect or correction is inferred merely from the fixture's failure.

The subsequently granted 63bb3b23 diagnostic observes an additional positive
Magic-empty successor with no completed row/frontier after both Regal rows are
complete. Paid Annul from each original Magic entry reaches it with probability
1. [Exact native negative](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-missing-entry-diagnostic-native-20261005/summary.json)
therefore falsifies the two-row closure premise in this fixture. No checker or
cost-14.5 result is reached. The general obligation is closure of all selected
positive physical/control successors, including those introduced by improvement
over other completed legal native rows.

The parent-selected [corrected finite source](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-frontier-closure-source-20261005/fixture-prerequisite-review.md)
retains this as an intermediate negative, then services actual named native
frontiers through existing owners under fixed bounds. Full materialized physical
and coarse keys, controller scope, incoming paid rows and actual ledger completion
bind each request; selection counters and a zero working value supply no proof.
Only complete assembly plus independent cost/properness checking at the root and
both original entries can establish a delivered feasible improvement. Exhausted
bounds or unavailable support remain unresolved and cannot pass that gate.

For the declared complete native baseline, R=1+0.5(11+R) gives reference R=13.
The proposed paid Regal entry is 0.5+R=13.5; the original Annul composition is
1+0.5(13.5)+0.5(13.5)=14.5. Additional legal Magic-empty continuation can change
the selected controller: its equal-weight Regal reference is 0.5+0.5*24=12.5,
making either entry's Annul route 1+12.5=13.5. These source-derived references are
not checked arbitrary-entry authority. Test-only f1ddac84 remains unbuilt/unrun;
its final captured cost and both physical entry costs require complete native
independent reconciliation and a checked strict improvement over retained 24.
No production correspondence theorem, whole-search cause or product economics
is accepted from the new source.

The subsequent [f1dd finite negative](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-frontier-closure-native-20261005/summary.json)
completes additional native Regal/Scour rows but fails a compound authority
assertion before final assembly/checking. It establishes no 14.5 upper. The
[source-only contract audit](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-owner-contract-source-20261005/owner-contract-audit.md)
separates active candidate identity from compatible checked root authority. A
new selected candidate is not an evaluated policy; retained nonworsening authority
must survive while complete paid checking is pending. Native row certificates
also need the actual resource-variant ownership consumed by repricing: injecting
a direct fixture price does not preserve captured graph-prefix compatibility.
This prerequisite applies to test construction, not a new upper issuer. The
failed run did not record after-prefix/compatibility values, so its exact failed
component remains unmeasured. Broad focused fallback may subsequently enqueue
missing strict states; the isolated immediate call does not prove eventual
absence. General native correspondence, whole-search cause and checked consumer
economics remain open. The draft correction is uncommitted/unbuilt/untested and
awaits parent selection of the natural-service control; all previous negatives
remain evidence. Native append also owns positive fringe enqueueing that the
policy-only fixture omitted. The physical root key and actual checked baseline
cost, current compatibility and retained/active checked graph authority now define
preservation; an exact old portfolio slot or active identity does not.


<a id="cost-only-entry-service"></a>
### A cost-only proposal at an actual verified entry

A verified proper controller is a feasible root-cost witness while a proposed
replacement is not. At an actual reached item/control entry, an almost-surely
exiting local program with full paid cost \(g\) and exit law \(H\) has one-use
then-old value \(g+HV_\pi\) only for compatible old continuations. Replacing
the decision on every return changes the recurrence: a cost-one base attempt
with self-return probability 0.9 costs 10, while a cost-two attempt with
self-return probability 0.5 costs 7 for one use then the base but 4 when
repeated. Old visits times local advantage does not evaluate that new policy.

The emitted paid program, old router and every positive-mass exit must form a
complete recurring original-root controller. Its independent native evaluation
decides properness, cost and adoption; an upper estimate or an omitted proposal
does not prove non-improvement. The [L0 cost-only query](../../active/2026-09-24-cost-only-continuation/README.md)
found no eligible clean entry in its A5/A4 controllers, so no new composed
policy or native correspondence was tested there. This is a conditional
selection argument, not an additional upper or exactness claim.

The [N0–N2 native boundary repair](../../active/2026-09-24-native-boundary-repair/README.md)
separates a safe, actually reached compiler decision from the old clean-entry
recipe. A candidate still needs the actual source item/control, an admitted
complete native option law and compatible continuations at every positive-mass
exit. A whole option includes setup, mandatory repetitions and cleanup; a
primitive first action is not its complete cost or exit law. A finite prefix
followed permanently by the old policy requires a real stage control wherever
the same physical state can recur. The emitted Eldritch candidate instead
replaces one decision on every return, so only whole-root recurring evaluation
can price it. Its C37.9778 A5 improvement is a scoped executable upper witness,
not a lower bound, action retirement, or an exactness result.

The [K2–K4 finder application](../../active/2026-09-25-seed-retention/README.md#k2k3--retained-side-native-finder)
uses this whole-controller principle for a held-side Eldritch programme. The
native option must be admitted at its bound source; the emitted primitive steps
retain their actual order, prices and possible loss/return routes. The finder
restricts initiation to checked held-goal and tier contexts and verifies every
positive-root-visit exact item after whole-root evaluation. Root Chaos
acquisition and Chaos recovery remain paid. This is a recurring replacement,
not the one-use then-old value, and its local preserved goals do not prove
properness of the global loop. The independent exact evaluator supplies that
separate conclusion. The packet's [paid acquisition example](../../active/2026-09-25-seed-retention/research-inputs/seed-retention-238a771/MATHEMATICS.md)
illustrates the same cost accounting; its toy numbers do not confer native
admission. The older fixed-donor first-hit comparison remains unmeasured at
its required compiler boundary.

### Narrow cleanup correspondence

[Recovery](../../active/2026-09-29-metamod-recovery/README.md) reuses complete-programme/first-exit correspondence: Protected Scour pays Bench then Scour, observing after both; terminal crafted cleanup pays native remove-all crafts once. Complete exit mass/resources are retained. A nonterminal Bow row needs its compatible tail before supplying a root upper. Finder re-enters acquisition with the persistent item, checks held goals and side occupancy/craft capacity, and independently re-admits every positive exact reached programme entry.

Parent now selects natural owner observation BEFORE the first explicit dispatch.
[Frozen test-only checkpoint c4dffb18/09e3b8bd](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-natural-owner-source-20261005/fixture-prerequisite-review.md)
uses native resource/enqueue/binding ownership, records actual queue/check events,
preserves compatible checked nonworsening authority for the original physical root,
and captures a valid checked active/retained complete controller without forcing an
explicit-only path. Both actual positive entries still independently check complete
paid recurring policies. Native resource variants and ordinary enqueueing omitted
by the former policy-only fixture materially weaken the previous causal premise.
Natural checked improvement, if observed, establishes existing capability; all
costs/activation remain unmeasured until source-matched execution. Production and
six original bodies remain fixed; source is frozen/unbuilt/untested, with no build,
native or root grant. [Exact CI review handoff](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-natural-owner-source-20261005/ci-handoff.json)
and [passive future product identity outline](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-natural-owner-source-20261005/passive-conquest-identity-outline.md)
preserve state11735 identity as unresolved. The outline is unimplemented/unrun;
no additional Conquest root or handoff-defect claim follows. Prior negatives remain.

CI's c4dffb18 prerequisite review stopped before any build because matching a spent
candidate identity incorrectly permitted re-emplacement after refusal. Production
leaves that attempted slot spent when clearing the failed task/proof scratch.
[Minimal corrected test checkpoint 7a02cec4/2d4c0ad0](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-owner-admission-source-20261005/fixture-prerequisite-review.md)
drains existing work, starts a new owner check only when the slot is exactly zero,
and treats a completed refusal/spent slot as a decisive negative. Pre/post-check
identities, actual refusals/repeated starts and proof/reset lifecycle are observed;
independent positive-entry proofs remain separate. All native ownership, normal-
before-explicit service, exact-root authority and complete paid gates remain fixed.
Source is frozen/unbuilt/untested pending [another CI prerequisite review](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-owner-admission-source-20261005/ci-handoff.json).
No build, native/root run, production change or publication occurred. Earlier source
reviews/native negatives and general mathematical/economic claim status remain.

The [preserved7a02/2d4c native negative](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-authority-diagnostics-source-20261006/preserved-7a02-negative.json)
uses matching Tests3df271e4 after25 CI review/build gates. All six controls complete;
case6 independently checks baseline24 and exposes two positive missing ports.
The normal-owner authority assertion then fails before explicit service, but its
after-step state, exact invalid reason and failing unit index were not recorded.
Latest snapshot is pre-service valid. Certificate loss and a production defect
remain unproved; complete challenger, both entry checks and case7 are unreached.
Native grant1/1 is spent; LOCAL released2026-10-06 00:16:17.2724105UTC with original
PID88224/token absence and empty escalated CIM. Existing receipt bytes stay fixed.

Parent pauses functional correction and selects only observability. Frozen
[Tests-only4c036208/48d5370b diagnostic diff and control-flow checklist](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-authority-diagnostics-source-20261006/control-flow-checklist.md)
emit/flush read-only candidate/certificate/current-binding and owner snapshots
immediately after each post-baseline native owner step, before event/guard failure.
The unchanged asserted root predicate is captured once, flushed and reused;
all89 original acceptance expressions, setup, owner assignments/event order,
six controls and limits remain.22 source gates and5 predecessor-artifact gates
pass; source is **unbuilt, untested, unactivated** pending [CI prerequisite review](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-authority-diagnostics-source-20261006/ci-handoff.json).
Private detached-evaluation identity is not directly exposed; logical retained
views and actual pipeline status are recorded without inventing that identity.
No build/native/root, production correction, assertion weakening or publication
occurred. Reference14.5 remains unverified; checked Current Conquest economics
remain101311.35474896732. General mathematical/economic claim status stays open.

CI [stops4c036208 before build](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-diagnostic-purity-source-20261006/preserved-4c036208-CI-rejection.md):
the diagnostic emitter's const fast_estimated_owned_bytes query increments mutable
owned_byte_ledger_requests. This disproves the previous emitter-purity claim;
its original frozen packet is preserved. CI otherwise verifies after-step flush,
captured acceptance predicate, owner/event order and all six controls.
[Minimal three-line Tests-only correction 7db9b98f/a0be05db](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-diagnostic-purity-source-20261006/transitive-purity-checklist.md)
removes only that query and records live_owned_bytes=unavailable. Remaining calls
are audited transitively as field reads/local key/hash/snapshot construction.
No counter reset, production/acceptance/setup/event-order change or assumed
ownership value hides the issue.16 source/artifact gates pass; source is
**frozen/unbuilt/untested/unactivated**, pending [CI prerequisite review](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-diagnostic-purity-source-20261006/ci-handoff.json).
Private detached identity remains unresolved.4c pre-build rejection,7a native
negative and prior economic findings remain. No heavy/native/root or publication
occurred; Current checked101311.35474896732 and unverified14.5 status stay fixed.

Matching7db9/a0be Tests7c23d298 passes30 CI purity/build gates. Its sole
[diagnostic native invocation](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-diagnostic-purity-native-20261006/summary.json) completes all six controls,
checks case6 baseline24, then records the actual normal-owner failure:
**graph_prefix_changed** at both active and retained-logical location0. Five
returned-step snapshots/predicates show four1s then0. Captured prefix11032646483254399659
remains; current prefix changes to6843342162333667524 as owned row0 variant_count
changes1-to-2. This is an observed metadata/compatibility transition, not exclusive
attribution among all hashed fields. Graph/payload identity, exact certification
payload, available original-root member, checked cost24 and goal/economy/caller/
vocabulary/artifact bindings remain. The physical root still matches. No pending
checker, attempted slot or shared proof bytes appear; live owned bytes are explicitly
unavailable and private detached identity remains unresolved. The predicate's0
is flushed before its same-boolean assertion fails. Certificate loss, a production
defect, whole-search cause and introducing commit remain unproved.

Explicit service, complete challenger, both positive-entry checks and case7 are
unreached; reference14.5 remains unverified. Native exits3221226505 in423.7954ms,
without timeout/cancellation/survivor. PID33740/token33740:134357222284687543 is
proved absent; escalated ErrorActionStop CIM is empty and LOCAL releases
2026-10-06 01:04:19.1134820UTC. Native allowance1/1 is spent; no retry, root,
Benchmark, production/source correction, publication or consumer activation
occurred. Canonical docs retain this scoped fixture negative and all prior
receipts, including7a's still-unobserved original after-state. Checked Current
Conquest economics remain101311.35474896732 with no new measured gain.

<a id="root-artifact-parent-prefix-lifetime"></a>
### Root artifact authority and parent prefix lifetime

The [October 6 source review](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-prefix-dependency-source-20261006/causal-review.md)
separates two implemented obligations. A root-only continuation certificate is
bound to the exact checked graph, full native context and exact physical entry.
The retained fallback wrapper additionally binds the captured parent graph
prefix. Passing the first obligation does not imply passing the second.
Source requires both; native7db observes the first pass and the second reject
with graph_prefix_changed as row0 variant_count grows1-to-2.

The prefix includes row variant count/offset/capacity and selected price/choice
fields. Equivalent native kernels can carry different resource variants, so
kernel or total-cost equality alone cannot authorize a graph-relative reuse.
An exact duplicate resource variant would change count without changing the
paid controller semantics, but the actual second payload is not recorded.
The root-only checker uses a separate exact model and copied graph/economy,
not that parent arena. Conservative overbinding is a possible availability
concern; the synthetic cache/expanded/queue arrangement does not prove an
actual supported producer loses a required checked bound at retention/export.

The unrun finite probe records full payload/prefix differences and directly
rechecks the same root graph, preserving the current rejection predicate.
No parent statewise/lower authority follows, no predicate relaxation is accepted,
and no native economic or general program-correspondence claim is promoted.
All original negatives and the unverified14.5 reference remain.

The [ce4 native counterpart](../../active/2026-10-04-sol61-armour-recovery/checks/graph-only-prefix-probe-native-20261006/README.md)
now records full exact duplicate Scour payloads and unchanged native kernel,
routing, typed context and physical root. Only row0 variant_count changes among
all captured prefix fields; the wrapper rejects while a fresh independent root
checker returns Complete at24 with full paid proper support. This establishes
the distinction between checked root artifact and broader wrapper binding in
the synthetic fixture. The full selector is still a negative: its new assertion
incorrectly negates the owner's intentional paired_default_only flag for an
identical certification graph. Later guards are unrun and original receipts are
not backfilled. Neither the measurements nor an eventual test correction proves
a supported production lifetime defect or sound removal of the prefix guard.
No parent statewise/lower authority or economic claim follows.

<a id="independent-root-artifact-quotient"></a>
### Independently checked graph through parent quotienting

The [resumed normal Current check](../../active/2026-10-04-sol61-armour-recovery/README.md#resumed-current-repair---october-6)
demonstrates a different lifetime boundary from the earlier injected duplicate
variant fixture. Ordinary discovery issues a checked graph at31984.615384614921;
its completed all-action behavioral quotient then replaces458 rows with193.
The active and retained ordinary wrappers remain present but fail their captured
prefix identity. This is a supported normal-construction compatibility loss in
that finite model, not proof of the Conquest whole-search cause or introducing
commit. The unchanged original failed receipts remain retained.

An exact quotient preserves the abstract optimization problem; it does not keep
old row indices, prefixes or copied statewise policies valid. Conversely, a
fixed independently checked paid controller from its exact physical root has no
dependency on a later parent row arena, provided graph bytes and the full native
goal, economy, action vocabulary, caller scope, data and terminal context remain
compatible. Those are distinct authority roles. No root scalar supplies an
uncovered continuation or lower bound.

Sourceb200aec9 requests the existing checker's physical-root continuation entry,
then may retain the same checked graph in the existing root-only role. That role
has no parent decisions, nonroot finite values, row binding or parent generation;
the original statewise output keeps its original prefix and sticky rejection
provenance. Exact graph/certification bytes, full typed context, the available
physical-root member and paid checked cost remain required. The shared byte
ledger must admit both temporary ownership and retention. This does not relax
the ordinary wrapper's prefix predicate or issue a certificate from mere cost
equality. Five focused selectors pass12,754 checks with no failures. Publication
may move the checked artifact out of the retained container; the normal check
follows its physical-root certificate through that transfer and early Finish.
The closed finite model has no missing continuation and proves no positive
service handoff or real-request saving. TargetNeutralZero remains fixed.

The fresh matched original Conquest pair now exports the same checked
101311.35474896732 graph in both arms, cap mask0. The treatment completes eight
native support cells/88 committed rows, then still lacks a complete continuation
at its next assembly boundary (37 attempts/0 successes). Local paid row delivery
is not a complete proper controller or checked economic advantage. The living
record binds those measured native identities and preserves all negatives.

A later source inspection finds that first-policy seed ordering considers
unavailable successors only while no incumbent object exists. An object whose
value table is rejected or root-only still has no nonroot continuation authority.
Source `353d93ee` therefore uses the assembly completion census to prefer rows
with eligible completed/priced immediate successors in that role. An owner row
count alone is insufficient, and every positive exit stays in the comparison.
This is one-step proposal ordering, not a closed-policy proof: a completed
successor row may itself lack a tail or belong to an improper cycle. Complete
closure, properness and independent native checking still own acceptance. The
repair's five focused selectors later pass12,829 checks. Its original matched
Conquest pair ties at101311.35474896732 despite3 handoffs/8 complete cells/187
counted committed rows;36 assemblies still have no complete challenger. Thus
one-step successor support does not establish an executable tail or economic
gain. The later source-only `9063c5fd` uses continuation capability for delivery
before further improvement after proper fixed-policy evaluation. Publication
still requires full native closure/checking and comparison with the checked
fallback; a proper coarse policy is not an independently executable strategy.
The9063 follow-up's focused control later rejects admission of an unchanged
24-cost baseline. Correction83906a6c uses the fixed-policy estimate only to
order early delivery of potentially cheaper proper proposals; equal/expensive
estimates continue improvement. It passes12,829 focused checks but its matched
Conquest pair again ties101311.35474896732, with unchanged353d93ee construction
metrics and no complete challenger. This validates a dispatch contract, not
new economics or proof of the first failed attempt stage. Neither source fact
proves the whole Conquest cause or supplies a lower bound.

Sourceaf483998 (proposal01109141 plus its all-required guard) applies the finite
Annul-to-inevitable-Chaos proposal in the existing complete-held RerollVersusRepair
controller when all goals are required and no requested target goal is present.
Subset-goal contracts retain their original repair, since cleanup without target
progress can otherwise suffice. Requested-goal, native held-side/frame and paid
setup/cleanup/recovery semantics must agree; full native kernel/controller
correspondence must justify any claim that direct Chaos replaces post-Annul
Chaos. Root scalar equality or coarse goal-mask equality cannot establish it.
Full graph checking and every positive programme entry remain mandatory.

The matching build and12,906 focused checks pass. One/two-target component
fixtures pass native original-root and reached-entry validation, with retained
synthetic-price costs942.20959795000726 and942.37761739992402. There is no matched
component control, so these are validity evidence rather than demonstrated
savings or a general stochastic dominance theorem. The original matched
Conquest pair exports identical checked101311.35474896732 strategies/cap0.
Both native selective-completion owners report3 checks/statusretained, while
joint assembly36/0 still lacks complete paid support at7941/mask27. The normal
receipt does not expose the changed branch's reachability or that entry's full
physical/controller identity. No new domain, historicalP0 admission, global
cause, numerical lower authority, consumer activation or private210 activation
follows. [The living checkpoint](../../active/2026-10-04-sol61-armour-recovery/README.md#bounded-repair-tail---qualified-no-conquest-saving)
keeps exact pins, checker ownership, unchanged gates and every prior negative.

The [saved-event relevance audit](../../active/2026-10-04-sol61-armour-recovery/checks/current-support-handoff-source-20261006/normal-frontier-relevance-audit.md)
adds a narrower negative: mask27 records four satisfied of five required goals.
A three-held/two-target programme cannot have zero satisfied target goals there;
its modified direct-Chaos guard is false at that entry. This cardinality argument
supplies no physical frame, incoming cursor or inference about later reroll
visits. The late missing request occurs after all eight bounded service cells
were spent elsewhere. No owned row/statewise frontier is available. Because
closure is rebuilt before and after evaluation, the exact invocation remains
unidentified. Checked native programme-entry certificates and root graph validity
also do not imply the supported primitive/global-routing predicates of the
existing actual-entry owner; copying absent retained provenance alone is
insufficient. These are scoped necessary-condition refusals, not a global cause,
new cost theorem, permission to reuse a root scalar, or justification to widen
service. A bounded passive physical/certificate/branch census is the next gate.

<a id="bounded-checked-graph-support-service"></a>
### Bounded checked-graph support service

The [parent-selected source change](../../active/2026-10-04-sol61-armour-recovery/checks/current-support-handoff-source-20261006/README.md)
`0ae56bc5ed60dd3968a968d46bfd77051b221dfa` changes scheduling correspondence only. A just-attempted, compatible,
independently checked graph checkpoint may transfer its actual named unexpanded
nonterminal continuations to the existing exact row owner, within cumulative
eight-cell / six-handoff limits. No unrelated uncertainty padding is selected by
that bounded caller. A committed native row can make the next ordinary joint
assembly checkpoint due; geometric cadence otherwise remains and the complete
checker slot is not reset. Existing wrapper/prefix and all typed-context,
positive-support, properness, executable cost, ownership and scope gates remain.

This neither proves a continuation at selection nor makes a root-only scalar a
statewise upper. Passive coarse keys and first native row/variant/cost evidence
are observational; unavailable physical/control entries remain unavailable.
TargetNeutralZero and every-positive-entry checking obligations carry forward.
The root probe now expects the owner's successful identical certification graph
pairing flag, with all its other guards intact. The original failed selector
receipt is unchanged. Normal production tests and a matched Current comparison
are still unrun; there is no new mathematical endpoint or economic gain, no
supported production lifetime defect, and no safe binding-removal conclusion.

The0ae CI build fails before native execution on two new test API names.
[Correction `075b402e5da3d795c153b48ddb5390fccd5ad7f6`](../../active/2026-10-04-sol61-armour-recovery/checks/current-support-api-fix-source-20261006/README.md)
uses actual ForbidUnmatched (reject unmatched extras) and finish (move finalized
result after completion), preserving the intended predicate, result ownership,
all assertions and production code. This is source correspondence only, unbuilt
and unrun; no mathematical/economic endpoint or native acceptance is added.

075 qualification now completes the corrected scoped prefix contract, then the
normal producer selector aborts before any normal-support result. Its original
exact throw site is unproved from the log. [Source repair `0f29fe169f75958e87d316ec2af1dee4329ad71d`](../../active/2026-10-04-sol61-armour-recovery/checks/current-support-registry-fix-source-20261006/README.md)
corrects the independently demonstrated absent augmentation registry lookup to
actual augment (same native action and price1), with a named fail-closed lookup
guard. All acceptance/authority gates and production code remain; new source
is unbuilt/unrun and adds no normal service, retention/export or economic proof.
