# The mathematical problem

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


This page defines the problem whose answer the solver tries to certify. It does not prescribe an algorithm. For the reasoning from this problem to an output certificate, use the [mathematical reading guide](mathematics/README.md). The current request fields and their implementation owners remain in [Request and Action Scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/request-action-scope.md).

<a id="target"></a>
## 1. One target, several representations

For a fixed semantic configuration \(\theta\), write the target stochastic shortest-path problem as

\[
\mathcal M_\theta=(S_\theta,A_\theta,P_\theta,c_\theta,G_\theta,s_0,\Pi_\theta).
\]

The policy class \(\Pi_\theta\) is written explicitly because the product can restrict what happens after an outcome, not just disable a button. The same physical item under a different program or retry obligation need not be the same decision state.

| Object | Meaning |
|---|---|
| \(S_\theta\) | Semantic decision states, with all information needed to determine the next legal decisions and their distributions |
| \(A_\theta(s)\) | Complete allowed decisions at \(s\), including the caller's program and dependency rules |
| \(P_\theta(t\mid s,a)\) | Native probability of the next semantic state |
| \(c_\theta(s,a)\) | Expected immediate cost under the pinned prices, including mandatory work represented by the decision |
| \(G_\theta\) | The exact native goal predicate |
| \(s_0\) | Requested starting state |
| \(\Pi_\theta\) | Allowed, nonanticipating policies and their control-memory restrictions |

The configuration binds native behavior, the compiled data artifact, session, start, goal, action scope, price interpretation, and any policy restriction. Its concrete components already have experiment identities; a mathematical alias must refer to those identities rather than replace them with a second hashing service.

A new numerical algorithm is normally a treatment of the same target. A change to the goal predicate, a price, a legal recovery, or what the policy may do after a zero-progress outcome can change the target even when a familiar case name stays the same.

The following are different mathematical objects:

* the native target problem;
* a policy-restricted problem;
* an optimistic lower model;
* a fixed executable strategy and its induced Markov chain;
* a stored floating-point coefficient model used in a numerical calculation.

Their values cannot share an unqualified label such as “the exact answer.” [Numerical acceptance and closure](mathematics/numerical-closure.md#layers) gives the translation obligations.

<a id="state"></a>
## 2. State means sufficient information, not a particular C++ struct

A physical item is part of a state. Where allowed behavior observes them, checkpoint contents, offered choices, program position, and retry restrictions are also part of the state. A Markov state must make the conditional distribution of the next state independent of earlier history, once the current decision is given.

For example, two identical current items with different saved checkpoints can have different restoration outcomes. Merging them because their affixes match changes the problem. Likewise, a controller that has already seen an offer can choose differently from one that has not seen it.

The native architecture distinguishes `pc_item_state`, `AbstractState`, strict carriers, quotient cells, and compiled strategy nodes. The compiled evaluator works over operation/item/control-state combinations. These namespaces are representations of related objects, not interchangeable identities. [States and Carriers](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md); [Publication and Evaluation](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md).

The [representation chapter](mathematics/representations.md) states sufficient conditions for merging, projection, and reuse. It deliberately does not assume that every coarse carrier is an exact behavioral equivalence class before strict validation.

No broad completeness theorem in these documents assumes the entire implicit product graph has already been enumerated. Proofs using a finite graph say so locally.

<a id="goal"></a>
## 3. Success is the native terminal predicate

At the reviewed revision, the documented terminal contract combines required rarity, requested slot/tier coverage, and equality between occupied explicit-affix count and the native satisfied-goal count. Empty explicit slots are allowed; unrelated affixes, temporary blockers, metamods, and below-tier members are not terminal success. [States and Carriers](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md).

For a request with disjoint requirements, an intuitive description is “enough requested affixes of acceptable tiers, and no occupied affix outside that accepted set.” This description is not a replacement for the native goal resolver when goal groups overlap, requirements allow alternatives, or crafted/natural membership differs. In those cases, retain the resolver's counting and assignment semantics. A proof using one bit per distinct affix must establish that premise for its own domain.

We may make \(G_\theta\) absorbing with zero subsequent cost for analysis. That is an analysis convention after native success, not permission to turn a non-goal frontier into success.

A mask containing all required goal bits is not sufficient when extra junk remains. A caller demanding any \(k\) of \(n\) requirements is also not necessarily asking for one fixed subset. Both distinctions matter to lower-table indexing and compiler conditions. The source identifies `CalcContext::is_goal_state` and `exact_goal_condition` as the corresponding native and compiled predicates; their full equivalence remains an implementation obligation, not something established by naming them. [Compiler condition owner](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/engine/src/solver_compile_conditions.hpp).

<a id="scope"></a>
## 4. Scope is larger than the materialized graph

\(A_\theta(s)\) is what the request permits, not the rows currently in memory. An enabled but unevaluated action remains a proof obligation. A disabled action is outside this particular target; a missing price must retain the source's explicit scope qualification rather than becoming a fictitious free action.

The reviewed product contract disables generated Imprint programs and voluntary economic Restart, enables goal-progress-gated reforges, and requires junk-free success. Generated programs must be covered by exact enumeration, suitable dominance, or a proved positive-price argument; a construction-depth cutoff alone does not close their envelope. [Request and Action Scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/request-action-scope.md).

These settings must be disclosed when saying “optimal.” They do not prove optimality over every physically possible crafting policy. In particular:

* A primitive used inside a mandatory program is not automatically an independently selectable product action.
* A policy restriction following a stochastic outcome must survive in state/control memory or in an equivalent policy-class definition.
* Mechanic-owned recovery and discretionary abandonment are different permitted behaviors.
* An auxiliary lower model can deliberately allow extra Restart or extra observations if that is proved optimistic. This does not enable those actions in the native caller.

[CLM-0009](claims.md#clm-0009) explains restriction and relaxation directions. No proof may silently switch between the primitive-expanded problem and a program-restricted problem.

<a id="cost"></a>
## 5. Costs and stopping time

Let \(T=\inf\{n\ge0:S_n\in G_\theta\}\). For an admissible execution with finite-cost termination, define

\[
J_\pi(s)=\mathbb E_s^\pi\!\left[\sum_{n=0}^{T-1} C_n\right],
\qquad
c_\theta(s,a)=\mathbb E[C_n\mid S_n=s,A_n=a].
\]

Costs are nonnegative under the model considered here. They are monetary/resource costs, not CPU time. An action that summarizes mandatory primitive work must include all setup, retries, and cleanup that really belong to that action. If costs depend on the random exit, the unconditional immediate expectation plus the exit-weighted continuation expectation remains valid by linearity; resource accounting may still need the joint exit/resource data.

With a pinned price vector \(p\) and resource-use vector \(R_n\), the cost is \(p\cdot R_n\). Repricing is safe only where resource/transition evidence is price-independent and the relevant optimization or pruning is redone. A graph pruned under old prices is not automatically an action-complete reprice cache.

Starting at a goal gives cost zero. An unavailable price is not zero. An implementation ceiling, infinity, absent incumbent, and unknown value are distinct states of knowledge.

<a id="properness"></a>
## 6. Properness and the nontermination decision

This reference uses the following explicit **working target**:

\[
V_{\rm proper}^*(s)=\inf_{\pi\in\Pi_\theta^{\rm proper}(s)}J_\pi(s),
\]

where a candidate policy must reach the native goal almost surely from the entry being certified and have finite expected cost. The infimum of an empty feasible policy set is infinity. This matches the documented requirement for a finite executable upper; it is not a claim that every internal numerical path already implements precisely this optimization convention. Confirm its application through [GAP-01](research.md#gap-01) before treating it as a fully reconciled project-wide specification.

For a finite fixed-policy graph, almost-sure absorption, finite expected number of steps, and transience of the nonterminal submatrix coincide. Outside that setting they need not coincide. A history-dependent controller with growing memory can terminate almost surely and still have infinite expected duration. Thus the finite-graph equations in the policy chapter state their own assumptions.

The distinction from raw total-cost minimization is concrete. At a non-goal state, allow a zero-cost self-loop and a five-cost finish. Minimizing unrestricted accumulated cost permits looping forever at cost zero; minimizing over proper policies gives five. Bellman's equation alone has multiple solutions in this example. It does not select the intended one.

Bertsekas explicitly separates unrestricted and proper-policy optima in general nonnegative SSPs. PRISM also distinguishes total-reward and reachability-reward properties, with its own convention for failing to reach the target almost surely. These are reasons to specify the target—not to import a tool's convention without reconciliation. [Bertsekas, 2018](https://arxiv.org/abs/1711.10129); [PRISM reward semantics](https://www.prismmodelchecker.org/manual/PropertySpecification/Reward-basedProperties). See [CLM-0001](claims.md#clm-0001).

Zero-cost loops do not prevent a *finite sound lower* from existing. They do prevent casual claims that iteration from zero must find the proper-policy optimum. The [lower proof](mathematics/lower-bounds.md#subsolution) compares against proper policies directly.

<a id="bellman"></a>
## 7. What the Bellman notation commits us to

For a compatible continuation function \(v\), use

\[
Q_v(s,a)=c(s,a)+\sum_tP(t\mid s,a)v(t),
\qquad (Tv)(s)=\inf_{a\in A(s)}Q_v(s,a).
\]

These formulas assume the state includes decision-relevant memory and the policy class permits the required continuation choices. An observed offer may require an expectation containing a minimization *after* the offer, not a minimization before the offer. A fixed policy does not reoptimize that choice during evaluation. [CLM-0003](claims.md#clm-0003).

Do not infer existence of an optimal stationary policy, uniqueness of a Bellman fixed point, or convergence of a numerical method from this notation. Each requires its own assumptions. For this draft, the useful local guarantees are proved directly: proper fixed-policy evaluation, finite lower subsolutions, complete coverage, and compatible bound comparison.

The simple action-family floor result does not require invoking a strong Bellman theorem: policies whose first action is \(a\) form a subset of the full proper policy class, so their infimum cannot be smaller than \(V^*\). [CLM-0011](claims.md#clm-0011).

<a id="output"></a>
## 8. Four different output claims

| Claim | Required interpretation |
|---|---|
| A verified strategy costs \(U\) | A permitted proper executable policy has that evaluated cost under the declared numerical contract |
| A certified lower is \(L\) | Every allowed proper solution costs at least \(L\), with scope and native validity established |
| An auxiliary model has value \(v\) | A statement about that model; transfer to the target needs an explicit relation |
| The native request is exactly closed | Complete compatible proof and executable evidence satisfy the project's exactness/numerical contract |

When \(L\le V^*\le U\), the interval bounds suboptimality. Mathematical equality \(L=U\) proves the optimum. Equality after formatting or a floating-point tolerance test alone does not. The distinction between a mathematically exact number and the source's “exact” policy classification is addressed in [numerical closure](mathematics/numerical-closure.md#exactness).

Memory caps, cancellation, bounded finish, and CPU watchdogs terminate an *algorithm*. Unless explicitly part of the modeled task, they do not change \(T\), the native action scope, or the infinite-horizon objective. A capped solve can still return a valid bound; a cap is not proof of infeasibility.

<a id="correspondence"></a>
## 9. Implementation correspondence and remaining work

The current mechanism contracts point to `solver_api.cpp` and the registry/options layer for request semantics, `CalcContext` for transition and goal calculation, the strict quotient for complete alternative accounting, and the compiler/evaluator for execution. [Source map](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md); [Strict Closure](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md).

This draft recovers a connected specification from those contracts. It does not prove every native-to-abstract relation or every classifier branch. The named integration obligations are in [research.md](research.md#open-obligations), especially improper-policy semantics, goal/representation parity, generated-program completeness, and numerical exactness. A discrepancy is evidence to investigate, not permission to silently change either mechanics or the mathematics.
