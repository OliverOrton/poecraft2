# Representations, equivalence, and optimistic abstraction

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


The central question is not whether two states look similar. It is what statement remains true after we treat them alike. Exact execution, a uniform lower, and a uniform executable continuation have different quantifiers.

The target notation is in [the model](../mathematical-model.md). Native state fields and namespaces remain described by [States and Carriers](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md).

<a id="namespaces"></a>
## 1. A representation diagram is not an equality chain

A useful conceptual route is

```text
physical item + required control/observation memory
    → solver observation / coarse carrier
    → exact refinement and quotient cell
    → compiled strategy operation
    → evaluator operation–item–memory pair
```

Each arrow needs a relation. None means that the identifiers on either side are interchangeable.

The experimental finder stores distinct finite control nodes for different
partial-goal observations before compiling an ordinary strategy. Sharing a
role feature or action index does not equate the corresponding operation–item
states or their values. A bound native program contributes its mandatory
steps and control phase; the Scour→Alchemy finder branch emits those
steps explicitly rather than treating a macro label as a primitive transition.
The experimental retained-side branch likewise keeps the actual source
binding and emits an admitted Eldritch-side programme's steps. Its held-goal
and tier guards restrict initiation; every reached exact item is rechecked
after evaluation. Sharing these role predicates does not identify states,
kernels or values, and the branch is not a state quotient.

A coarse cell can represent several physical items. One physical item can occur at several strategy nodes with different remaining programs. A strict-state identifier belongs to a particular graph generation. A compiled router selects an operation using conditions that must be sufficient for the policy being executed. The evaluator's product graph is needed precisely because an operation node alone does not determine its transition law. [Publication and Evaluation](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md).

The same warning applies to a lower-table coordinate such as rarity × goal mask × side occupancy. It is a question asked of many possible items, not a complete description of any one of them.

<a id="equivalence"></a>
## 2. Sufficient conditions for an exact quotient

Consider a finite-state, finite-action SSP with terminal states made absorbing. Let \(\phi:S\to\bar S\) partition semantic states. A sufficient exact quotient contract is:

1. Terminal truth is constant within each class.
2. Every member has corresponding allowed actions, with the same information/choice timing.
3. Corresponding actions have equal expected immediate costs.
4. For every target class \(C\),
   \[
   \sum_{t\in C}P(t\mid s,a)
   =\sum_{t\in C}P(t\mid s',a)
   \quad\text{whenever }\phi(s)=\phi(s').
   \]
5. Any control memory needed to make those statements true is included in the relation.

For a class-constant vector \(v=\bar v\circ\phi\), the concrete action expectation becomes

\[
c(s,a)+\sum_tP(t\mid s,a)\bar v(\phi(t))
=\bar c(\phi(s),a)+\sum_{\bar t}\bar P(\bar t\mid\phi(s),a)\bar v(\bar t).
\]

Thus Bellman evaluation on class-constant vectors commutes with the projection. A class-policy can be lifted by choosing its corresponding concrete action. Its projected path law and terminal hitting behavior match. Conversely, for the ordinary fully observed finite-MDP policy class, a quotient policy can use the conditional distribution of a concrete policy’s chosen action given the class history. Because every member has the same action/cost/class-transition law, this reproduces the projected cost and absorption law. The two optimal costs therefore agree. If the caller imposes an additional program or policy restriction, its preservation under both mappings is a separate premise; equal kernels alone do not prove it.

This is a **sufficient exact reduction theorem**, not evidence that the initial native projection satisfies it for every admitted action. The strict pipeline's obligation is to establish carrier-wide rows or refine when a counterexample violates uniformity. [Strict Closure](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md); [CLM-0005](../claims.md#clm-0005).

### A minimal counterexample

Two items have the same satisfied mask and occupancy. Under the same named action, one reaches the goal with probability one; the other cannot reach it. Merging them by mask/count alone either gives the second an impossible transition or loses the first's real one. The representation is not exact.

A hidden blocker can create precisely this *kind* of discrepancy. Whether a particular native blocker does so is a source/model fact to inspect, not a rule to infer from a generic example.

Preserving one frozen continuation expectation has weaker requirements than
this universal quotient contract. The [stopping-line argument and label
counterexample](lower-bounds.md#stopping-line) show how a probability query can
omit later draws when their conditional continuation value is already proved
uniform. A changed potential, hidden conditional law or moved decision can
invalidate that query even though its prefix key is unchanged.

<a id="optimism"></a>
## 3. An optimistic abstraction need not be an exact quotient

For a lower, equality of kernels is stronger than necessary. Let \(\gamma(q)\) be the concrete members represented by abstract coordinate \(q\). Suppose an abstract Bellman expression \(\bar B(q,a;h)\) obeys

\[
\bar B(q,a;h)
\le c(s,a)+\mathbb E[h(\phi(S'))\mid s,a]
\quad\text{for every }s\in\gamma(q)
\]

for the candidate potential being checked. If \(h(q)\le\bar B(q,a;h)\) for every required action, then the concrete pullback satisfies the desired one-step lower inequalities. The proper-policy stopping argument can then establish a native lower.

This relation may grant the abstract controller favorable outcomes, cheaper setup, or more choices. Those weaken a lower. They do not produce an executable policy. A probability-box row minimizing expectation for one frozen vector is an example of a value-specific optimistic relation, not an exact quotient row valid for every future vector. [Lower proof](lower-bounds.md#events).

The universal quantifier over \(\gamma(q)\) is the part that a representative experiment cannot supply. A single exact kernel demonstrates one member; it does not establish the minimum continuation cost over an entire class.

The current retention consumer explicitly relies on retained state fields and complete modifier-member masks, refusing ambiguous or unsupported classes. Preserve that refusal until the universal relation has been established. [Lower and Pruning Authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

<a id="members"></a>
## 4. Uniform member bounds: minimum and maximum mean different things

For each member \(s\in C\), suppose \(\ell_s\le V^*(s)\). Then

\[
\ell_C=\inf_{s\in C}\ell_s
\]

is a lower for every member. Using the maximum generally is not: if two optima are 2 and 100, the class-wide lower cannot be 100 merely because the second member supports it.

For upper-side evidence, suppose every member has a proper permitted policy \(\pi_s\) with evaluated cost \(u_s\). Then

\[
V^*(s)\le u_s\le\sup_{t\in C}u_t.
\]

This proves a scalar upper for every member. To claim **one executable class continuation**, an additional implementation condition is needed: the controller must identify the member or choose a common policy whose coverage is established. The existence of different member-specific policies is not an executable selector by itself.

If the same fixed strategy is proper from every member, the maximum of its member costs supplies a uniform upper directly. If even one represented member is uncovered, no such complete class certificate has been established. Unknown members can receive independent fallback lowers; they cannot disappear from the minimum. [CLM-0006](../claims.md#clm-0006).

This result explains why a private physical-item lower may be stronger than the value safely returned for its coarse class. A low coarse hit rate can be a representation-coverage limitation, not a numerical weakness. Changing that requires a better member-domain argument, not removing the check.

<a id="identity"></a>
## 5. Identity makes proofs applicable; hashes merely locate them

A certificate is a statement about a target, a domain, a relation, and usually a specific value or policy object. Reusing it requires those semantic inputs to agree.

A digest can quickly reject different payloads or locate candidates. A hash match alone is not a mathematical equality proof. Where collision-free identity is required, compare the canonical fields or invoke the exact equality owner after the lookup.

Typical distinctions include:

| Changed fact | What must be reconsidered |
|---|---|
| Price vector | Cost values, price-based pruning, and cost reconciliation |
| Action scope | Completeness and optimality; a disabled-family result is a different target |
| Goal/terminal predicate | Goal labels, properness-to-goal, all value certificates |
| Native transition semantics or artifact | Row/projection validity and all dependent numerical evidence |
| Class membership | Uniform bounds and carrier-wide rows, even if the representative is unchanged |
| Compiled strategy/control memory | Policy coverage, properness, and continuation costs |
| Only search order | Performance and eventual service; existing immutable witnesses may remain valid |

Not every change invalidates every artifact. A completed price-independent transition cache can be reused with new prices under its documented contract, while an incumbent's old cost cannot. [Resources, Resume, and Replay](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md).

A generation counter is an implementation mechanism for expressing invalidation. It is useful only if every relevant semantic change advances the generation or is otherwise checked. Conversely, append-only graph growth need not invalidate a frozen row if its complete transition support and identities remain valid. [CLM-0021](../claims.md#clm-0021).

<a id="state-memory"></a>
## 6. Observations and program memory are part of the relation

Two operation–item pairs with different saved checkpoints cannot share a restore row. Two states before and after observing an offer cannot share the same allowed choice rule. Two items in different program phases can have different mandatory next operations even when all visible affixes match.

An optimistic lower may intentionally grant additional observation or early choice if the resulting policy class contains every native possibility in the right direction. It must label that relaxation. An executable upper cannot exploit information unavailable to its controller. [Choice timing](policies.md#choices).

For a macro action, hiding intermediate mandatory steps is not ordinary state equivalence. It is a stopped-process reduction with its own exit law and accumulated cost. That construction is in [program composition](policies.md#programs).

<a id="counterexamples"></a>
## 7. What refinement is trying to fix

A refinement should name the false advantage given to the optimistic model or the exact distinction missing from a candidate equivalence. Examples include:

* sharing a continuation across members with different action legality;
* preserving a goal that a destructive action can remove;
* ignoring a crafted blocker that occupies a needed slot;
* treating an applied Rare result as the original Normal source;
* dropping a filter before calculating the follow-up pool;
* treating an unfinished program family as absent.

These are distinct missing premises. Adding arbitrary state bits without a measured failing relation can create cost without proof strength. Conversely, a measured null improvement does not prove the new distinction mathematically useless: another independent optimistic exit may still cap the model. [Ceilings and tied constraints](lower-bounds.md#ceilings).

The strict implementation uses split-only partitions and generation-bound rows according to its current contract. This draft does not establish that every current split corresponds to one of these specific mechanisms. [Strict Closure](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md).

<a id="mapping"></a>
## 8. Correspondence and open obligations

<a id="selected-controller-requirements"></a>
### Selected-controller requirements

An exact quotient for all compared actions is stronger than sharing for one
selected controller. The latter requires equal selected legality, immediate
reward and successor control-cell probabilities over the entire member domain,
including goal truth and hidden offer/checkpoint state. Adding an alternative
requires its observations too, refinement, or a separate context. A representative
transition cannot prove whole-member equality.

Existing backward observation contracts propagate requirements for legality,
rewards, native goals, routing, surviving locks/affixes and selector preimages.
For cycles, iterate their monotone requirement union to a fixed point on the
finite observation vocabulary. Missing contracts mean unknown dependence, not
permission to remove a distinction. Compare per-node requirements with the global
union required by today's single CalcContext. A smaller per-node set may leave
that union and the actual class count unchanged.

The narrow [implied clean-entry predicate](policies.md#implied-entry-predicates)
argument already exists: distinct goals exhausting exact side occupancy imply
zero junk. It does not permit dropping predicates on dirty states, overlapping
goals, below-tier members, bench conflicts or surviving control. A general
contract-derived layout adapter remains research.

### Action-scoped private policy search

For a legal candidate subset \(A'\subseteq A\),
\(V_A^*\leq V_{A'}^*\). A proper compiled controller using \(A'\), independently
evaluated under the original mechanics, prices and terminal predicate, therefore
provides an upper for the original request. A private lower or optimum under
\(A'\) does not provide a full-scope lower or retire omitted actions.

Observer removal can permit merging only after recomputing fixed/conditional
program dependencies and retaining the original modifier universe. Passing an
old layout as `refinement_parent_layout` retains its class IDs as split-only
distinctions; genuine coarsening requires a fresh calculator namespace. Existing
state IDs, selected rows and incumbent vectors cannot be transplanted into it.
Projection is a coarsening only when every old class's complete member set maps
to one new class with all goal/tier, occupancy, blocker, four junk-count
categories, persistent flags, retry and observation facts preserved.

The [dirty-state native measurement](../../active/2026-09-11-dirty-state-continuation/README.md#actual-cb08-action-layout-measurement)
reproduces CB08's 21-class full layout. Removing either selected resistance
conversion yields 15 classes; removing both yields six, and removing Annul leaves
21. Whole-member projection and fresh native row construction agree to numerical
precision: Chaos support falls from 4160 to 85 and native recurrence work from
10447875 to 2128588. The same-layout subset retains the original support and work.
This measured search-row saving does not establish equivalent checker carriers,
whole-policy speedup or full-scope equivalence. The imported report predicted a
benefit but did not perform this measurement.

| Mathematical duty | Documented implementation boundary |
|---|---|
| Retain future-observable fields | `AbstractLayout`, `AbstractState`, native exact item/control identity |
| Establish/refine uniformity | Strict observation/features/partition and carrier-wide row validation |
| Preserve semantic namespaces | Calculator, strict carrier, quotient cell, compiled-node mappings |
| Issue a uniform lower | Native phase producer plus consumer member-domain checks |
| Compile distinguishable continuations | Router conditions and evaluated operation–item product |

These mappings come from [the source map](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md), [state contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md), and [publication contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md). [GAP-02](../research.md#gap-02) asks Codex to attach the decisive current producer/checker/test evidence to the universal claims. It does not ask for a blanket reread of every archive.

The retired carrier-planner projection had no planner/proof consumer at `215654f`;
its sole unit fixture checked descriptor field copies. The header and that fixture
were removed after the local inventory audit. Its [Gate 4 counterexample](../../archive/2026-08-25-solver-anytime-proof-realignment/gate4-evidence.md)
remains: attractive coarse compositions failed compiled probability-mass checking.
The [old header](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/engine/src/solver_executable_carrier_planner.hpp)
and [old fixture](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/engine/tests/test_solver_solve.cpp)
remain reproducible from that Git revision; no native equality theorem followed
from the descriptors. The separately used fragment verifier was retained.

<a id="role-parametric-computation"></a>
## 9. Cross-binding computation retains the binding

Fixed-instance symmetry, equivariance across relabeled instances, and a shared
parameterized computation make different claims. A fixed-instance automorphism
must preserve the actual goal, observations, legal actions, rewards and transition
law. Relabeling the *whole* instance, including its coefficients, can instead
give \(V_{g\theta}(gs)=V_\theta(s)\). It does not give
\(V_\theta(gs)=V_\theta(s)\) when \(\theta\) is asymmetric. A common computation
\(C(\theta)\) shares operations while retaining different numerical outputs.
Neither relation makes states with different native bindings one scalar class.

For a fixed proper finite controller, \(V_\theta=c_\theta+Q_\theta V_\theta\).
Its sparse equation pattern can stay fixed while coefficients, values and best
actions change. For example, two draws without replacement from three distinct
choices of weights \(a,b,j>0\) give probability
\[
\frac{a}{a+b+j}\frac{b}{b+j}
+\frac{b}{a+b+j}\frac{a}{a+j}
\]
of drawing both first choices. The arithmetic pattern is common, while weights
\((3,2,1)\) and \((1,2,3)\) give \(7/12\) and \(3/20\). This toy is not the
native reforge recurrence. The [received derivation](../../active/2026-09-23-cross-binding/research-inputs/cross-binding-plan-8ac4c72/MATHEMATICS.md)
states its finite-model premises and limits.

Ragged tier vectors are legitimate binding inputs. Summing member weights for a
chosen outcome class is valid only if every aggregated member has the same
relevant exclusions, effects, observation and continuation meaning for that
computation. Equal totals alone lose future distinctions: with future costs
\((0,100)\), weight mixtures \((1,9)\) and \((9,1)\) both total ten but have
expectations 90 and 10. Required level, literal identity, persistent flags and
support zeros can force separate structure or refusal. The [selected-controller
requirements](#selected-controller-requirements) and [restricted action-scope
upper](#mapping) still apply.

<a id="guarded-specialization"></a>
### Guarded specialization argument

Let a finite acyclic template contain the native recurrence's operations,
support guards and output ports. For each admitted binding, require actual native
leaf coefficients, the same recurrence step at each internal node, every active
support and branch guard, and an inverse output map to the correct physical
successors, rewards and control observations. Topological induction then makes
each specialized output equal to the corresponding native recurrence output in
the mathematical model. For cyclic continuation, the bound equations and a
proper transient solution are additional premises. A matching graph observed
after two cold builds does not prove the guard for future inputs. Stored-double
rounding and publication remain under [numerical closure](numerical-closure.md).

The [G0 native discovery](../../active/2026-09-23-cross-binding/README.md#g0-native-discovery)
found unequal A4 fractured-role coefficients and matching bucket conflict graphs
for two cache misses. It did not establish a preconstruction guard, a reusable
template, equal completed laws or material amortization. The prior pair-stop and
same-entry memo negatives remain scoped to their own implementations.

<a id="downstream-role-signatures"></a>
### Descriptive roles and a guarded continuation computation

A role-status signature (H0) records a pattern such as two attained prefixes and
one missing prefix. It groups observations for measurement; it does not equate
states, actions or values. An operation-specific signature (H1) must additionally
retain the native legal menu, side capacities, exclusions, member and control
observations, setup order and every external exit port needed by that operation.
Even an H1 match is descriptive until a pre-work native guard (H2) establishes
the same dependency structure and an actual binding map for every leaf, action
and output port. This is a conditional application of the
[guarded-specialization argument](#guarded-specialization), not a new quotient.

For a finite local decision region, a bound computation may use different
coefficients \(c_b,Q_b,H_b\) and external values \(u_b\) for each binding:
\[
(T_b(v;u))_i=\min_{a\in A_b(i)}
  \left(c_b(i,a)+\sum_j Q_b(i,a,j)v_j+
                 \sum_z H_b(i,a,z)u_z\right).
\]
The structural map must preserve the decision and observation point, active
support, goal classification and inverse native output map. Unknown external
continuations remain typed unknowns. For a cyclic fixed controller, equality of
the bound equations plus transience gives a unique local response; sharing its
sparsity pattern does not share a numeric factorization. Identical source loop
text also saves no work by itself: independent coefficient and successor-value
leaves still need binding-specific evaluation unless another justified relation
compresses them. The [policy boundary contract](policies.md#bound-role-ports)
keeps global properness separate.

<a id="strict-preparation-sharing"></a>
### Sharing preparation while preserving physical answers

Suppose a finite native preparation has the form
\(F(d,x)=\operatorname{Apply}(\operatorname{Prepare}(d),x)\). A shared
\(\operatorname{Prepare}(d)\) is valid only when its key covers every immutable
dependency it reads, including selected runtime and observation contracts,
successor dependencies and the relevant generation. The application still
uses each actual state's features, coarse parent and current continuation.
If feature values are read during preparation, those values belong in the key
or the computation cannot be split at that boundary.

A required-field set is weaker than an observed value. With canonical
requirement \(C(d)\), an observation key has the form
\(K(x,p,d)=\operatorname{Encode}(p,C(d),\operatorname{Observe}(x,C(d)))\).
Two states can share \(C(d)\) yet have different observed features and native
probabilities. Reusing the complete key, value, row or certificate by the
requirement identity alone would violate their separate authorities. The
[S0 strict-preparation measurement](../../active/2026-09-24-strict-preparation/README.md)
did not retain such a sharing implementation; this is a conditional contract.

<a id="cost-only-role-proposals"></a>
### Role-relative proposals without value transfer

A completion constructor may identify the missing requested goal at a real
physical decision and bind that role to native options for the actual item.
Its tier/member pool, blockers, suffix context, fractures, resource prices and
control state remain binding-specific. A shared construction intention supplies
neither equal transition laws nor equal values, a state quotient or a checked
policy. Each emitted controller still needs its own complete original-root
evaluation. The [cost-only L0 application](../../active/2026-09-24-cost-only-continuation/README.md)
reached the native entry query but found no eligible clean entry in A5 or its
one A4 contrast; it did not exercise a role-relative constructor. This leaves
the earlier cross-binding and strict-preparation gates at their original scope.

The [A5 Eldritch repair](../../active/2026-09-24-native-boundary-repair/README.md)
used the exact native physical source item and control state. A private abstract
representative differed from that item and was refused as candidate initiation;
matching its projected role was insufficient. The Exarch tier, occupied sides,
below-tier requested modifier and dirty intermediate outcomes remain distinct
from the clean terminal predicate. An Eldritch-side intention proposes a native
option only for this binding and actual action envelope; it supplies no shared
probability law, value, state quotient or authorization for another binding.
