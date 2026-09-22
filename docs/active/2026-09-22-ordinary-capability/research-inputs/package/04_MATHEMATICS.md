# Mathematical research for a capability-oriented programme

These are scoped mathematical observations and independently tested toy examples,
not new PoE semantics, native proofs or novelty claims. Core fixed-policy,
properness, first-exit and performance-difference arguments already belong in the
repository's canonical policy chapter. Extend it only where the current application
adds a useful distinction. See [SOURCES.md](SOURCES.md), R12–R13 and P1–P3.

## 1. Feasibility, discovery and checking are distinct

Fix the native task theta. Let E(t) be the currently owned compatible proper policies
whose complete original-root evaluation has committed by time t. A verified result
selects an artifact with cost

    U(t) = min_{pi in E(t)} J_theta,pi(root),

with infinity for an empty set. Under the native acceptance interpretation, every
such graph is a feasible witness; mathematically V*(root) <= J_pi(root).

A known feasible graph pi_ref only proves existence. It does not prove that a
particular implementation, runtime, candidate representation, scheduler or resource
profile will insert it into E(t). Conversely, a historical failure to find it does
not prove current failure after pipeline repairs. This is why C0 measures the
actual current unattended run.

First-policy delivery observes U(t_first). A default run observes U(T) with later
search permitted. The test that voluntarily stops at t_first cannot measure the
second. A cheaper graph returned by an externally seeded diagnostic is not evidence
of discovery by the ordinary product.

## 2. An upper estimate is not a candidate-rejection lower

For a fixed complete candidate pi and incumbent cost U, exclusion as non-improving
is mathematically justified by a valid candidate lower L_pi >= U (with applicable
ties/tolerances), or its reconciled exact cost. A candidate upper H_pi >= J_pi is
not such evidence: H_pi >= U permits J_pi < U.

Example: incumbent cost 10, candidate true cost 6, candidate upper 20. The upper
is valid, but rejecting every candidate whose upper exceeds 10 discards a useful
policy. A heuristic source value has even less exclusion authority unless its
native correspondence and direction are established.

This does not make using a cheap score to limit finite search a correctness bug.
It changes what may be claimed: “not selected for checking” is not “proved unable
to improve.” Relaxing that heuristic creates additional proposals, not native
acceptance, lower-bound or retirement authority. The code-screening hypothesis in
this packet remains unproved until a current source/native candidate is compared.

## 3. Complementary decisions can hide a useful continuation

Finite deterministic toy states r, t and goal g. The incumbent chooses r->g for
cost 10 and, if started at t, t->g for cost 100. Alternatives are r->t for cost 1
and t->g for cost 1. All four complete deterministic policies are proper.

| Selected changes | Root cost |
|---|---:|
| None | 10 |
| Root bridge only | 101 |
| Cheap tail only | 10 |
| Both | 2 |

The tail is unreachable under the incumbent, so its old occupancy is zero. A rule
that demands positive immediate root improvement for every accepted intermediate
proposal can reject the tail, while the bridge alone looks terrible. Completing
and checking the pair exposes the actual gain.

This is not a counterexample to full-domain policy iteration: full-domain value
improvement at t can enable later improvement at r. It is a counterexample to a
narrow root-only/occupied-entry proposal filter. The repository already owns joint
policy machinery; the task is to establish whether a relevant joint opportunity
reaches it after an incumbent, not to implement another policy-iteration solver.

The analogue in native crafting must be a complete legal family with all outcomes,
not a favourable sampled path. Setup, failed attempts and cleanup remain paid.

## 4. A complete closed continuation model is sufficient; a partial prefix is not

For a finite candidate region D and actual boundary B, a selected proper local
controller with complete native transition/reward laws has

    v_D = c_D + Q_DD v_D + Q_DB b_B,

where b_B denotes compatible executable tail values only on certified boundaries.
Solving this describes a one-time local excursion followed by those tails. If the
modified controller is re-entered on returns, the full recurring controller has
its own transient matrix Q and equation V=c+QV. It must be evaluated as that
controller, not priced repeatedly with one-shot or old-root scalars.

A complete zero-cost self-loop can satisfy v=Qv while never reaching a goal.
Properness is a separate premise. A prefix with 0.9 goal mass and unfinished 0.1
trap mass cannot be renormalized into a complete success row. Reachable bottom
components, native observation timing and the true terminal predicate remain the
existing native checker obligations.

An old policy proper only at root r may include an unreachable trapping state t.
Giving a new action access to t invalidates using r's finite certificate there.
This is why reference graph inspection never licenses copying an arbitrary entry
value or private state ID into a new candidate.

## 5. Spending and recurrence: what the fingerprint comparison establishes

For a proper finite fixed controller Q with immediate reward c, expected visits
are d^T=e_root^T(I-Q)^(-1), and J(root)=d^T c. Summing immediate expected resource
spending is valid accounting for that fixed controller. Differences between two
such spending tables are a decomposition of their total difference, not causal
estimates for independently editing each row.

For two proper controllers on a common finite semantic domain:

    J_mu - J_pi = (I-Q_mu)^(-1) (c_mu + Q_mu J_pi - J_pi).

Use the new controller's visits. A cost-one retry with old return .9 and new return
.8 changes value from 10 to 5. Multiplying the local advantage -1 by the old visits
10 predicts -10 instead of the actual -5. Therefore the lower recurring Alteration/
Exalt/Annul totals in the richer C4 graph motivate investigation; they do not prove
that a cheap local modification produces the full 1471.91-Chaos difference.

For source/native residual attribution, v must have a legitimate value and meaning
at every mapped native state. The existing stopped-remainder derivation handles
unmapped entries. No root-ratio correction or new residual analyser is selected.

## 6. Pending evidence, finite budgets and retries

A producer state “pending” is neither an economic negative nor successful closure.
A bounded consumer should retain a compatible obligation until the relevant support
commits, is refused/capped/cancelled, or is legitimately superseded. This progress
condition is already illustrated by the current constructive-certificate repair.

It does not imply immediate global retry after every row: that can starve ordinary
search. The selected experiment has one exceptional complete-check opportunity,
semantic identity and the existing work owner. A row-count or timer change alone
cannot repeatedly rearm it. The model makes no unlimited completeness/fairness or
bounded response theorem; finite computation may leave useful work unresolved.

## 7. Expected cost remains the objective

Preserve original costs and primitive counts as distinct rewards. A capacity or
execution-count constraint changes the optimisation problem and may require policy
memory and joint duration/exit laws. It is a separate future feature, not a shortcut
for obtaining cheaper policies under the current request.

## Research relation and limits

LAO* establishes the precedent of searching for cyclic solutions without enumerating
the entire state space, under its own modelling and heuristic conditions. The native
solver already uses analogous partial-graph machinery; this packet does not propose
replacing it. [P1]

Fixed-point certificate research reinforces the separation of proposals, support/
reachability and independently checked quantitative results. Its finite explicit
model guarantees do not discharge this repository's native abstraction/entry
correspondence or current numerical representation. No new certificate issuer is
selected. [P2]

The recent trajectory-driven online policy-iteration work is mainly deterministic
finite-horizon with a consistency condition and a discussed stochastic counterpart.
A trajectory-only improvement promise is not imported into this cyclic implicit SSP;
full positive support and original-root checking remain required. [P3]

The practical contribution sought is a generic capability at an existing native
selection/completion/checking boundary. It is not a new Schur complement, LAO*,
policy-improvement theorem or proof of global optimality.
