# Conditional mathematics for downstream role computation

These are finite-model arguments, not a claim that an arbitrary native role map
satisfies them. Existing native coefficient/reconciliation, properness, choice and
publication contracts remain acceptance authority. The independent examples in
`checks/` illustrate premises and counterexamples, not PoE mechanics.

## 1. Similarity, symmetry and computation identity

Let x be an exact native state with required item/control memory. A descriptive
projection h(x) can place “A+B held” and “A+C held” together. Equality of h proves
only equality of h. It does not establish equal legal menus, transition laws,
values or required continuation support.

A fixed-instance state-action automorphism has much stronger requirements: native
terminal truth, observed choices, corresponding legal actions, costs and pushed-
forward transition laws must agree. Under a full finite proper-policy correspondence
it preserves optimal values. With unequal parameter assignment, whole-instance
relabeling can give V_(g theta)(g x)=V_theta(x), not V_theta(g x)=V_theta(x).
The existing canonical chapter already preserves these distinctions. [R14]

A computation object C(theta,u) instead preserves a *function*. A and B can reuse
its structure while producing different values, decisions and output states.
The numerical cache key must therefore contain its complete binding/evidence
inputs; a role-shape key alone is insufficient.

## 2. Complete probability support and selective service

For a complete action row P(.|s,a), summing all represented outcomes gives its
native complete law, including observations and mandatory program costs. The set
of successors scheduled for additional work W may be a strict subset of its
nonterminal support without changing P. This is scheduling, not deleting mass.

For example, support masses 0.1,0.2,0.7 remain those masses when only the second
successor receives work next. Normalizing it to probability one changes the action.
An upper controller must eventually route every positive outcome it can encounter
to a complete legal continuation; that route may be an already verified compatible
fallback rather than full optimal exploration. A full-scope lower or exact result
retains the other applicable proof obligations.

The source's nonfocused enqueue, focused selection and scheduling-only grouping
are separate mechanisms [R6,R9–R11]. Their existence is not a general fairness or
convergence theorem for arbitrary admitted program grammars.

## 3. Guarded local decision structure with explicit boundary inputs

For binding b, let I_b be a finite set of native internal states and Z_b the
external successor/control ports. Let a bijection map the relevant internal
states, admissible decisions and ports to a common *structural* pattern. For each
binding, keep its own probabilities, rewards, actual native output identities,
legal/observable choices and terminal classification.

A bound one-step decision expression is

    (T_b(v;u))_i = min over a in A_b(i) of
        [ c_b(i,a) + sum_j Q_b(i,a,j) v_j + sum_z H_b(i,a,z) u_z ].

All positive mass is accounted for by internal successors, correctly identified
goals and ports. The common pattern may include guarded optional branches, but
an absent/illegal action or changed support cannot be silently treated as active.
Source contracts must justify a specialization before its repeated expensive
structural work is skipped.

**Acyclic case.** If native leaves, guards, arithmetic/control operations and output
maps correspond, induction in reverse dependency order establishes equality of
each bound computation with its cold native counterpart. Each min uses that
binding's candidate values and existing exact-tie rule. No equality between
bindings follows.

**Cyclic fixed-controller case.** For a selected policy whose internal block is
transient, solve

    (I-Q_b) v_b = c_b + H_b u_b.

If the bound cold and shared structures express the same matrices and right-hand
side, uniqueness yields the same v_b in the mathematical model. Changing numbers
normally requires new numeric factorization/evaluation even if symbolic ordering
or storage is reusable. The existing stored-number numerical contract is separate.

**Cyclic decision case.** The engine must still establish the applicable SSP or
proper-controller conditions. A min equation alone can admit spurious improper
solutions. This packet does not claim every locally greedy role controller is
proper or that every local value is a full-problem optimum.

## 4. Boundaries can defeat apparent symmetry

One binding has a local recurrence

    v = 1 + 0.25 v + 0.5 u,

with the remaining 0.25 mass reaching the goal. Hence v=(4+2u)/3. With u=0 this
beats a direct cost-10 finish. With u=100 the direct finish wins. The local
structure and probabilities are identical, but its externally supplied value
changes the answer.

Omitting an unaligned port and setting u=0 therefore need not be harmless. An
unsupported continuation stays unknown. Old root cost, a lower from a different
scope or another binding's value cannot supply it.

Two one-state regions that always exit to each other are individually transient
*until exit*, yet their composition may never reach the goal. Therefore local
termination is not global properness. If an outside controller returns, its value
must be the mutually consistent full-controller value or a properly justified
boundary certificate. No independent acyclic truncation is licensed.

An old-policy entry value can price a one-time deviation followed irreversibly by
that frozen policy (with the corresponding control-mode identity). It is not
automatically the tail value after installing a replacement on every revisit.
Repeated replacement requires the consistent newly composed controller.

This observation extends the existing first-exit/response mathematics, not a new
whole-solver response-cache commitment.

## 5. Structural recurrence does not eliminate independent numeric information

For q_b = c_b + sum_j p_bj v_bj, suppose the p_bj and v_bj are arbitrary independent
inputs, with p_bk nonzero. An algorithm that never inspects v_bk cannot distinguish
two inputs differing only there, while the correct output differs by
p_bk * delta. Without an additional validated relation or retained dependency
summary, exact evaluation must process the relevant independent leaves.

Thus “all rows are a sum of weighted values” is not enough for asymptotically
cheaper numeric work. Opportunities include repeated structural dependency
construction, repeated exactly equal subexpressions, native-justified aggregate
sums, or a compressed function whose operations stay compact. They do not include
reusing a numeric answer solely because the role status histogram agrees.

SPUDD is a precedent for manipulating compressed subfunctions, not for declaring
all unequal-valued leaves equal. Its discounted-model and input-factorization
premises differ from this native SSP. [P1]

## 6. Ragged member data and evolving roles

Different-length tier vectors are legitimate parameters. Aggregating their weights
is exact for an output class only when each aggregated member has the same relevant
future action/observation/port meaning. Equal totals do not suffice: future costs
(0,100) with normalized member weights (0.1,0.9) versus (0.9,0.1) give 90 versus 10.

A mapping must move crafted/fractured flags and actual role-related blockers with
the selected family, and preserve context that remains physically fixed. If a new
outcome changes which role is missing, update the inverse map or keep its stable
named role. Suffix, checkpoint or offered-modifier distinctions cannot disappear
because a prefix permutation matches.

Support zeros can destroy termination and topology. A graph-preserving symbolic
pattern is a guarded region, not a proof for every numeric vector [P2]. Native
member IDs stay calculator-scoped; shape-local IDs are not interchangeable with
native state, row or compiled-node IDs.

## 7. Cost-weighted opportunity and finite amortization

For actual distinct binding tasks b with ordinary caches enabled, write disjoint
work as S_b (reusable structure), N_b (necessary numeric work), and O_b (outputs,
scatter, required downstream checks). The cold total is

    T_cold = sum_b (S_b + N_b + O_b).

The new total is

    T_shared = B + sum_b (M_b + N'_b + O'_b),

where B is shared construction and M_b includes admission/matching and any
required guard validation. Repeating a cold native comparison on every production
hit belongs in O'_b; it is not free evidence. A development oracle may be run
separately for validation but must be disclosed as such.

A diagnostic histogram count does not measure S_b. One hundred identical shapes
with one unit of structural setup and 99 units of independent numeric/output work
can lose to cold computation once one unit of per-binding matching is introduced.
At most the actually bypassed terms are saved.

An optimistic upper bound for the observed workload, before overhead, is the sum
of eligible repeated structural costs beyond required first builds. It is useful
for rejecting a hopeless pilot, not guaranteeing a positive result. Sampling
coverage and uncertainty remain explicit; selected examples cannot stand for an
unmeasured full corpus.

Nested parent/child timers must be separated before summing. Search invocations,
expected crafting visits and first-hit probability are different units. A fixed
240-second solver runtime is a budget, so compare time to equal work or a verified
target and final quality separately; a faster operation can alter which later
candidate finishes before the deadline.

## 8. Reuse boundaries and witness authority

A template has no executable upper merely by being reusable. A completed native
controller from a restricted proposal family is an allowed feasible witness, so
its independently established J_pi(root) upper-bounds the original optimum. Its
restricted optimum is not a full-scope lower. Source-selected or unexpanded fringe
values cannot be substituted as executable tails.

Template identity, binding identity and coefficient/boundary evidence generation
are separate. Reuse a structural object only while its assumptions remain true.
Recompute numeric conclusions after relevant coefficient, price, action, domain or
port-value changes; preserve native tie order and current reconciliation meaning.
A returned graph, certificate, context and value remain a single owned bundle.

The shared object plus simultaneous binding arrays, pending outputs and scratch
must fit the existing aggregate cap. Eviction while a computation is suspended
cannot invalidate its live storage. Cancellation releases scratch but does not
refund logical work. Partial output is not a completed ordinary row.

## 9. A scope ladder for honest conclusions

1. H0 counts establish descriptive recurrence in an observed sample/domain.
2. H1 matches establish only the chosen operational descriptors agree.
3. A native source/guard contract plus cold comparisons supports a specialization.
4. A consumed specialization with full accounting supports a measured reuse result.
5. Same-budget original-root evaluation supports a strategy-quality result.
6. Full alternative coverage and native lower/upper proof still own exact closure.

No step is implied merely by passing the previous one. This avoids both treating
G0 as a global rejection and treating another repeated signature as a large gain.

## Source links

- [R6 — Ordinary successor queue](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/engine/src/solver_solve_expand.cpp#L2710-L2893). Read variant pricing, conditional fringe and shared-kernel suppression.
- [R9 — Focused policy fringe](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/engine/src/solver_solve_focused.cpp#L247-L384). Read policy-dependent walk and heuristic priority.
- [R10 — Focused grouping and quotas](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/engine/src/solver_solve_focused.cpp#L1050-L1190). Read per-class/quota work selection; no state/value equivalence.
- [R11 — Actual scheduling signature](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/engine/src/solver_solve_quotient.cpp#L466-L510). Read literal masks, capacity, context, four junk vectors and goal-member tokens.
- [R14 — Canonical representation mathematics](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/docs/solver/mathematics/representations.md#role-parametric-computation). Read existing equivariance, parameter-binding and guarded-specialization argument.
- [P1 — SPUDD: Stochastic Planning using Decision Diagrams (Hoey et al., UAI 1999)](https://www.cs.toronto.edu/~cebly/Papers/spudd.pdf). Primary paper; text on ADD subgraph sharing and discounted-MDP model read. No benchmark speedup transferred to poecraft2.
- [P2 — Parameter Synthesis in Markov Models: A Gentle Survey (Jansen, Junges, Katoen, 2022)](https://arxiv.org/html/2207.06801v1). Primary survey; parametric instantiation and graph-preserving versus topology-changing conditions read.
- [P3 — Relativized Options: Choosing the Right Transformation (Ravindran and Barto, 2003)](https://www.cse.iitm.ac.in/~ravi/papers/ICML03.pdf). Primary paper; relative representations and distinction from approximate equivalence read.
- [P4 — Bounded Real-Time Dynamic Programming (McMahan, Likhachev, Gordon, 2005)](https://www.cs.cmu.edu/~ggordon/mcmahan-likhachev-gordon.brtdp.pdf). Primary SSP paper; statewise bounds, focused work, and explicit policy fallback conditions read. No new BRTDP implementation selected.
