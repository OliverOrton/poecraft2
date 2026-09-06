# Lower bounds and proof models

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


A lower is a statement about every allowed proper solution, not just the current policy or the current sparse graph. Its value is useful only after the native scope and the relation supplying the inequality have been established.

This chapter gives explicit sufficient arguments. It does not claim that every native producer already proves every premise. The corresponding current mechanisms are in [Lower and Pruning Authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

<a id="subsolution"></a>
## 1. A finite subsolution is a lower: the stopping argument

Let \(h:S\to[0,B]\) be bounded, with \(h(g)=0\) for every true goal. Suppose, for every allowed decision at every relevant nonterminal state,

\[
h(s)\le c(s,a)+\sum_tP(t\mid s,a)h(t).
\tag{1}
\]

Choose any allowed proper policy \(\pi\) with finite expected cost. Condition on its history before each decision. Because (1) holds for every available action, it also holds for the action chosen by \(\pi\), including randomized or history-dependent choices consistent with the semantic state.

Applying the inequalities repeatedly until time \(n\) or the first goal gives

\[
h(s_0)\le
\mathbb E^\pi\!\left[\sum_{k=0}^{(T\wedge n)-1}C_k\right]
+\mathbb E^\pi[h(S_n)\mathbf1_{\{T>n\}}].
\]

The last term is at most \(B\Pr(T>n)\), which tends to zero because the policy is proper. The nonnegative partial cost sums converge monotonically to the total cost. Hence \(h(s_0)\le J_\pi(s_0)\). Taking the infimum over allowed proper policies gives

\[
h(s_0)\le V^*(s_0).
\]

The argument applies at any entry whose continuation region satisfies the same conditions. It needs neither an incumbent strategy nor current greediness. [CLM-0007](../claims.md#clm-0007).

**Scope of the result.** Boundedness is a convenient sufficient condition. For unbounded potentials, one needs a justified transversality/integrability replacement; “all values are finite” at individual states is insufficient. If no proper finite-cost policy exists, a finite lower is vacuously below infinity but does not prove that infeasibility. A finite subsolution is not a convergence theorem or a greatest-fixed-point claim.

### Zero-cost components do not select their own solution

A zero-cost self-loop plus a five-cost finish satisfies

\[
(Th)(s)=\min(h(s),5).
\]

Every \(h(s)\in[0,5]\) is a fixed point. The proper-policy optimum is five, while iteration from zero stays zero. The lower at zero is sound but weak. Inferring optimality merely from a zero residual would be wrong. [Nontermination semantics](../mathematical-model.md#properness).

<a id="coverage"></a>
## 2. Action completeness is a quantifier, not a row count

Equation (1) must cover every action in the requested scope. Removing an inconvenient inequality makes the certificate easier to satisfy and can make a claimed lower false.

At one state, suppose action A finishes for 10 and action B finishes for 1. Solving a graph containing only A gives 10, which is not a lower for the original problem. The correct unrestricted optimum is 1.

There are three safe ways to account for an unbuilt action:

* establish exact inapplicability on the entire represented source domain;
* retain a complete optimistic relation for it;
* retain an independently valid scalar floor \(\ell(s,a)\le Q^*(s,a)\).

In the scalar case, impose \(x(s)\le\ell(s,a)\). It may be very weak, but the action has not vanished. A residual-family floor must hold for **every** member of the unresolved family. A sample, count, or finite generated prefix does not prove that universal property. [CLM-0008](../claims.md#clm-0008).

Complete canonical sets or a proved disjoint family partition establish coverage. Two duplicate action constraints cannot replace a missing different action just because the counts match. The current lower-only quotient and repaired coverage mechanisms are documented as checking that distinction. [Lower and Pruning Authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

When a cheap placeholder is replaced by a complete row, keep the native action represented once in the model's intended relaxation. Leaving an old, cheaper fictitious escape alongside the refined row is safe as a lower but may permanently hide the benefit. Deleting a still-required unresolved action is unsafe. Those are opposite mistakes.

<a id="frontier"></a>
## 3. Lower-only regions do not require an incumbent route

Let \(R\) be a finite region with unknown lower variables \(x(s)\). At an exit state \(t\notin R\), let \(b(t)\) be an independently valid lower for the original continuation problem. For a complete modeled action require

\[
x(s)\le c(s,a)
+\sum_{t\in R}P(t\mid s,a)x(t)
+\sum_{t\notin R}P(t\mid s,a)b(t).
\tag{2}
\]

Keep scalar action placeholders as in the previous section. Goals have value zero.

To prove validity, follow any proper native policy until it reaches a goal, first leaves \(R\), or first chooses an action represented only by an independent scalar placeholder. Repeated inequalities bound the cost accumulated before that stopping point. At an outside exit, \(b\) is no greater than the actual policy's remaining cost. At a placeholder, \(\ell\) is no greater than the actual cost of choosing that action and continuing. The bounded interior remainder tends to zero. Thus \(x\) lower-bounds every permitted proper continuation.

This proof assumes the boundary expectations used are well-defined and that their native scope matches; finite nonnegative boundary certificates suffice in the small finite model. It does **not** require the outside lower \(b\) to satisfy Bellman inequalities outside \(R\). We stop there and invoke its independent validity. [CLM-0012](../claims.md#clm-0012).

### Root closure without an outside policy route

At \(s\), choose either a direct finish for 10 or pay 1 and go to \(t\). At \(t\), finishing costs 20. A root policy that directly finishes may have no router for \(t\).

Nevertheless, the independent bound \(b(t)=9\) gives

\[
x(s)\le10,\qquad x(s)\le1+9=10.
\]

The lower 10 and the executable direct-finish upper 10 prove the root optimum. No incumbent continuation or exact value at \(t\) was needed.

This example explains why a failed arbitrary-entry policy evaluation is not a universal barrier to proof exploration. The lower and upper ask different questions.

### Coupled regions are one simultaneous proof

Several lower regions may use each other's variables if the complete joint inequalities are checked. For example,

\[
x\le1+\tfrac12 y,\qquad y\le2+\tfrac12 x
\]

have maximal jointly feasible values \(x=8/3\), \(y=10/3\). A cycle of equations is not circular evidence when the entire vector is independently verified against the native relation. Circularity occurs when one provisional claim is called accepted solely because another provisional claim assumes it.

A proof that closes a fixed policy's values on an SCC is a special case, not the only way to obtain useful lower values. Lower variables can remain below the incumbent while still improving the proof.

<a id="composition"></a>
## 4. Maximum, addition, and action floors

If \(h_i(s)\le V^*(s)\) are independently valid for the same target and source, then

\[
\max_i h_i(s)\le V^*(s).
\]

No independence assumption is needed. This is why maximum composition is a safe default. If each \(h_i\) is also a subsolution of the **same monotone Bellman operator** \(T\), their pointwise maximum is a subsolution: \(h_i\le Th_i\le T(\max_jh_j)\), then take the maximum. [CLM-0010](../claims.md#clm-0010).

Addition needs more. Two bounds can charge the same action twice. If two goal subproblems each cost at least 5, but one five-cost action satisfies both, adding them produces an invalid lower of 10.

A sufficient additive contract is a valid cost partition: nonnegative component costs \(c_i\) satisfy \(\sum_i c_i\le c\) for every original action, with each component model covering every allowed policy. For any such policy, sum the component lower inequalities; the total remains below that policy's original cost. Taking the infimum gives an admissible sum. There must be an explicit allocation for setup, repeats, and compound actions; distinct table names do not establish cost partitioning.

For a complete native action and compatible successor lowers,

\[
c(s,a)+\mathbb E[h(S')]
\]

is an action lower, not automatically a whole-state lower. The state can choose a cheaper different action.

Conversely, a valid whole-state lower \(L(s)\) lower-bounds every legal action: the proper policies whose first decision is \(a\) form a subset of all allowed proper policies. Thus

\[
L(s)\le V^*(s)\le Q^*(s,a).
\]

The same holds for an infimum over any action family. This inference requires the same original scope. A restricted-action optimum or one program's lower does not meet its premise. [CLM-0011](../claims.md#clm-0011).

<a id="initialization"></a>
## 5. Admissible is not the same as feasible in this local model

An old native lower can be valid but inconsistent with a new, more optimistic truncated model.

At \(s\), one action costs 1 and reaches \(t\); from \(t\), finishing costs 9. Native values are 10 and 9. The native lower \(h(s)=10\) is valid. A new region containing only \(s\) with boundary \(b(t)=0\) imposes \(x(s)\le1\). Forcing \(x(s)\ge10\) makes that local model infeasible.

Nothing refuted the old lower. The new model simply discarded information that supported it. Preserve the old independently valid result outside the numerical feasibility problem and take a compatible maximum at consumption; do not silently insert it into an incompatible local iterate. [CLM-0013](../claims.md#clm-0013).

This does not contradict maximum closure for common-operator subsolutions. The hypotheses differ: being admissible under the target is weaker than satisfying every inequality of this truncated operator.

A numerical warm start can be an untrusted proposal when the solving algorithm permits it. It receives authority only after the right final relation is checked. Being “previously accepted” is not a substitute for current scope and feasibility.

<a id="events"></a>
## 6. Native probability evidence and expectation minimization

Suppose the native outcomes form a complete disjoint event partition indexed by \(i\). Let their true probabilities be \(p_i\). For a frozen potential \(h\), let \(m_i\) be no greater than the potential of every native outcome in event \(i\). For example, \(m_i\) may be the minimum over all compatible abstract successors in that event.

Let \(\mathcal P\) be a nonempty normalized set of distributions containing every native distribution represented by the source class. Then

\[
\inf_{p\in\mathcal P}\sum_i p_i m_i
\le \sum_i p_i^{\rm native}m_i
\le\mathbb E[h(\phi(S'))].
\tag{3}
\]

Equation (3) is the transfer argument. It requires complete mass, true membership in \(\mathcal P\), safe event values, and the **minimum**, or a certified lower on that minimum. A feasible distribution supplies an upper on a minimization optimum; its expectation can be too large for a lower proof.

For independent upper capacities \(0\le p_i\le u_i\), \(\sum_i p_i=1\), feasibility requires \(\sum_i u_i\ge1\). Sort events by increasing \(m_i\), then fill the cheapest to capacity until the mass is exhausted. An exchange argument proves optimality: moving mass from a dearer event to a cheaper unsaturated event cannot increase cost. If extra aggregate constraints are added, this greedy argument may no longer apply.

Rounded-up probability capacities enlarge the feasible distribution set and therefore can only decrease its minimum. Restricting the native probabilities to an unjustified discrete grid is a different operation and is not covered by that argument. With dyadic capacities, a dyadic greedy vertex can solve the continuous box problem exactly; the true distribution need not itself be dyadic.

### The minimizing witness belongs to the value vector

With two capacities \(u_1=u_2=3/5\) and values \((0,10)\), the minimum is 4 at distribution \((3/5,2/5)\). Swap the values. The minimum is still 4, now at \((2/5,3/5)\). Reusing the first distribution gives 6. A candidate value 5 would pass the stale expression and violate the actual minimum.

Cache compatible native support and capacities if useful. Recompute choices and allocations after the values change, or independently establish a valid lower on the new minimum. These rows are not native transition kernels reusable for arbitrary vectors. [CLM-0014](../claims.md#clm-0014).

The current phase producer and applied-reforge optimization explicitly preserve this distinction. The measured speedup reuses value-independent geometry/caps while rebuilding minima and allocations. [Applied-reforge evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md).

### A common two-exit bound

If mandatory cost is at least \(c^-\), true-goal probability is at most \(p^+\), and every nonterminal exit has compatible continuation lower at least \(d^-\ge0\), then

\[
Q^*(s,a)\ge c^-+(1-p^+)d^-.
\]

This is sufficient but may discard useful variation among failure exits. It becomes invalid if an uncovered outcome is silently omitted, if the success label is only an abstract goal, or if \(d^-\) is only the cost of one chosen cleanup policy.

<a id="conditional"></a>
## 7. Conditional probabilities, not independence guesses

For events involving sequential draws, an upper on each unconditional marginal is not enough to multiply probabilities. If all three “goal events” are perfectly correlated with probability \(q\), their intersection has probability \(q\), not \(q^3\).

A valid product uses conditional bounds. If for every relevant earlier history \(\mathcal H_j\),

\[
\Pr(E_j\mid\mathcal H_j)\le q_j,
\]

then the probability of the corresponding ordered conjunction is at most \(\prod_jq_j\), by repeated conditioning. Summing these bounds over every possible injective assignment of required distinct goals to draw positions gives a union upper. No independence is asserted. [CLM-0015](../claims.md#clm-0015).

The native premises include which goals are already retained or forced, whether one draw may satisfy several requirements, how many draws can occur on each side, intervening blockers on either side, guaranteed pools, and active filters. A factor derived before conditioning on an earlier pool-changing action may not apply afterward.

A useful weight inequality is

\[
\frac{N'}{N'+B'}\le\frac{N}{N+\max(0,B-D)}
\]

when \(N'\le N\), \(B'\ge\max(0,B-D)\), and denominators are handled correctly. Here \(N\) bounds satisfying weight, \(B\) is baseline other weight, and \(D\) bounds its possible exclusion. Overcounting exclusion overlap is conservative for an upper probability. The native producer still has to prove the history-specific values and correctly handle empty pools.

The reviewed implementation uses native integer statistics and conditional assignment bounds. That does not authorize using its witness in a changed filtered pool; the context identity and all-member argument remain premises. [Lower and Pruning Authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

<a id="coordinates"></a>
## 8. Acquisition tables and continuation potentials have different coordinates

An acquisition table \(A(S)\) asks about producing set \(S\) in a specified optimistic acquisition model. A continuation table \(h(M)\) asks what remains when \(M\) is already satisfied. In a relaxation where the acquisition model has the required compatible meaning, the coordinate conversion is

\[
h(M)=\min_{S:\,|M\cup S|\ge k}A(S).
\]

The union form handles any-\(k\) goals. It is not generally \(A(\mathrm{full}\setminus M)\), and it is not \(A(M)\).

For two independent abstract acquisitions with costs 3 and 5, acquisition values \([0,3,5,8]\) correspond to remaining costs \([8,5,3,0]\). Passing the first array as the second gives the wrong terminal and monotonicity meaning.

This conversion fixes a table's role; it does **not** prove that constructing a subset from an arbitrary empty native item is a lower for every progressed native item. Blockers, protection, retained goals, and action reach can make the progressed state cheaper. The acquisition relaxation's favorable-context/native coverage argument remains necessary. [CLM-0016](../claims.md#clm-0016); [documented proposal repair](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

<a id="retention"></a>
## 9. Retention, capacity, and destructive progress

A monotone mask relaxation can remember every goal ever acquired and never charge for losing it. This is often optimistic and hence useful as a weak lower, but it can erase the dominant cost of a destructive problem.

A current-progress model can retain rarity, side occupancy, crafted/fractured categories, selected goal identities, and protection where needed. Native effect arguments determine which facts survive each action. More dimensions alone do not prove anything; they must remove a specific false advantage while preserving complete action coverage.

**Application versus refill.** A native action can change rarity even if its random filling loop stops early. Proving the rarity change and proving minimum occupancy are different statements. The applied-reforge archive records Alchemy's applied Rare postcondition and a separate conditional non-exhaustion argument: every relevant pre-target history retains positive eligible weight. Nonempty initial weight alone does not imply later nonempty weight after exclusions. [CLM-0017](../claims.md#clm-0017); [native evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md).

**Removal law.** Treating Annul as a favorable choice of affix to remove can be a weak relaxation. A stochastic category law is stronger only when categories represent all eligible occurrences, respect side/protection/fracture rules, and carry the right probability direction. A crafted filter is not ordinary junk if its removal changes the next pool.

**Context cannot be discarded gratuitously.** A filtered item may be cheaper to finish than an unfiltered one. Deleting the filter and looking up the unfiltered lower does not follow from monotonicity. Likewise, the cost of cleaning up and then finishing is the cost of one policy, not a lower on every continuation. [CLM-0025](../claims.md#clm-0025).

**Programs.** A lower relation can telescope through mandatory primitive steps when all their intermediate semantic states and observed choices are covered. A support-only “ever acquired goals” proof does not automatically validate a different current-progress/retention potential. Each relation must use the potential's actual meaning.

<a id="ceilings"></a>
## 10. Use the optimistic policy to identify a ceiling

A proper executable policy in an auxiliary optimistic model gives an upper on **that model's optimum**. It may therefore prove that no amount of better numerical iteration can raise the model's lower beyond a certain value without changing the model.

If an abstract action costs \(\varepsilon\) and jumps to an abstract goal with certainty, every feasible potential at that source satisfies \(h(s)\le\varepsilon\). A new candidate vector, cache, or Bellman solver cannot overcome that row. The documented support-only phase relaxation exhibited this kind of 0.01165 ceiling. [CLM-0018](../claims.md#clm-0018); [phase history](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

At the applied-reforge snapshot, an exact policy of the **optimistic** model used a 212-cost metamod exit whose remaining continuation was zero. It bounded that model even after minimum-occupancy information became stronger. That is why the occupancy proof produced no additional numerical gain in that experiment. It does not prove a native 212-cost completion policy. [Applied-reforge evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md).

Ties also matter. If two unresolved actions both impose \(h(s)\le10\), strengthening only one to 50 leaves the other ceiling at 10. A zero isolated gain does not establish that the first refinement is useless; a useful unit may be a set of tied constraints. Conversely, improving a row from 400 to 800 cannot help a state still constrained by another row at 40.

Separate a genuine semantic exit from a temporary immediate-price shortcut. An action represented only by cost 10 may really continue for another certified 90. Keeping the shortcut forever can hide a 100 lower. It is safe but incomplete refinement. Recompute the changed model and distinguish why each cheap constraint remains. [CLM-0019](../claims.md#clm-0019).

<a id="mapping"></a>
## 11. Source correspondence and unresolved guarantees

The lower-only quotient owns numerical candidate/checking work; the native phase producer owns the physical domination argument; `ProofPatternManager` and the ordinary retention lookup own compatible consumption. These are separate premises. A coefficient checker cannot establish the native relation, and a native relation cannot turn an arbitrary numerical iterate into a lower. [Lower and Pruning Authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

This draft does not add a producer, a new abstraction, or a runtime authority. It supplies the arguments those owners need to cite. [GAP-02](../research.md#gap-02), [GAP-03](../research.md#gap-03), and [GAP-05](../research.md#gap-05) name the class, action/program, and numerical bridges that still need current-source correspondence review.

A result should be reported at the level it establishes: a stronger local relation, a stronger complete model, a valid native lower, a consumed public lower, or reduced exact-proof work. Those are different achievements. [Research interpretation](../research.md#outcomes).
