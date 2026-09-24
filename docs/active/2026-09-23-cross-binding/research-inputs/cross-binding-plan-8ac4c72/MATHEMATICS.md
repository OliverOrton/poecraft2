# Mathematical basis and limits

These derivations concern finite explicitly specified stochastic systems under their stated premises. Native correspondence and numerical acceptance are separate obligations. The accompanying exact-rational examples are illustrative, not a PoE mechanics implementation.

## 1. Four different meanings of 'symmetry'

### 1.1 Fixed-instance automorphism

For a fixed model theta, a bijection g of semantic states and legal actions that preserves the goal, observations, immediate rewards and transition law maps complete policies to equal-valued policies. Under finite proper-policy assumptions, corresponding trajectories have equal probability/reward and corresponding terminal behavior. If the full allowed policy class is preserved in both directions, optimal values correspond as well.

Three-label permutation orbits contain at most 3! = 6 elements. This is not a promise of sixfold reduction and does not cover arbitrary unequal coefficient models.

### 1.2 Equivariance of a family of instances

A consistent relabeling of the whole instance, including native parameter assignment, can establish

    V_(g theta)(g s) = V_theta(s).

It does not imply V_theta(g s)=V_theta(s) when theta is asymmetric. One cannot rename the currently missing goal without also carrying the weights, blockers, prices, action mapping and required suffix/control relations.

### 1.3 Common parameterized computation

A template C has a binding theta and computes C(theta). Different instances share C, not its evaluated number. For a fixed bound policy, its equations are

    V_theta = c_theta + Q_theta V_theta.

When Q_theta is transient, V_theta is uniquely (I-Q_theta)^(-1)c_theta. The same sparsity/control graph can support different matrices, values and optimal action choices. Retaining only the shape and one old V loses essential information.

A parameterized representation can share many structural operations without being a value-preserving quotient of the union of instances. No exact state-count compression follows merely from a common function signature.

### 1.4 Proposal transfer

An analogous policy sketch can be rebound and evaluated even when exact structural correspondence is unavailable. This grants no inherited probability, value or properness. The previous programme mostly tested a candidate-vocabulary extension and a same-entry memo; neither discharges the cross-binding computation contract selected now.

## 2. An exact parametric computation can preserve unequal answers

Consider three distinct weighted choices A,B,J, sampled twice without replacement, with weights a,b,j>0 and W=a+b+j. The event 'pick A and B in either order' has probability

    f(a,b,j) = (a/W)(b/(W-a)) + (b/W)(a/(W-b)).

This is one arithmetic graph with different input leaves. It gives f(3,2,1)=7/12, but f(1,2,3)=3/20. Sharing the graph is exact; sharing either output across these bindings is not.

This toy has simple without-replacement semantics, not the native game's multigroup exclusions, side caps, guarantees, retries or variable draw count. The native implementation must obtain its structure and leaves from its own recurrence. The example explains what the computation-reuse claim means, not which formula can replace native reforge.

## 3. A sufficient specialization contract

Let C be a finite acyclic arithmetic/control dependency graph with guarded operations and binding-specific native leaves. For each admitted binding b suppose:

1. Each leaf equals the native input or exact aggregate justified for b.
2. Each internal operation represents the same native recurrence step for b.
3. Every branch/support guard used by that recurrence is checked for b.
4. The output map scatters the resulting weights/rewards to the correct actual native successors, resources and observation/control contexts.
5. No omitted outcome changes the completed row or its selected continuation.

Topological induction then proves that every output of C(b) equals its corresponding native recurrence output in the mathematical model. Sharing C across b does not require equal leaves or equal outputs.

For cyclic continuation equations, the analogous argument is equality of the bound equations plus a justified transient/proper solution. A known sparsity pattern alone is insufficient: the bound coefficients, active support and terminal classification must be correct. A numerical factorization usually changes with coefficients, even when its symbolic allocation/order is reusable.

A sampled pair of matching rows is a valuable implementation test, not proof of a universal guard. The source contract must justify every allowed specialization; unknown cases fall back to the existing cold path.

## 4. Ragged tiers and conditional aggregation

Suppose a family contains members t with native weights w_t. Replacing that list by sum_t w_t is exact for one next-step outcome class only when all those members have the same relevant exclusion, effects, observation class and continuation meaning for the selected computation. Then linearity gives the class mass as that sum divided by the actual total eligible weight.

If a later action distinguishes a required level or offered literal ID, members with equal immediate class appearance can have different futures. Their distribution must survive or the representation must refine. Equal total weight does not repair that lost correlation.

For example, two member mixtures can both total 10 but assign weights (1,9) versus (9,1) to future costs (0,100). Their continuation expectations are 90 and 10. Treating the entire family as one unparameterized member is unsound.

Variable-length tier vectors are therefore legitimate binding data, not permission to force each family into an arbitrary universal tier bucket. Support zeros are also semantic: an escape probability changing from positive to zero can destroy properness.

## 5. Policy decisions must remain binding-dependent

An attempt costing 1, with identical-state retry and success probability p, has expected completion cost 1/p for p>0. Compared with a guaranteed cost-20 finish, the preferred action is retry at p=1/5 and guaranteed finish at p=1/100. Both use the same decision skeleton. Reusing the first instance's chosen action for the second would be suboptimal.

More generally, even if each fixed-policy value is a rational function of parameters, choosing a policy introduces minima and changes of optimal region. A whole parameter-space optimizer is not required: instantiate actual native coefficients and use the current finite selection/evaluation machinery for that binding.

Zero-cost nonterminating cycles illustrate another limit. A formally self-consistent value is not enough; properness and actual clean-goal reachability remain explicit.

## 6. Explain the previous stop-family collapse without overclaiming it

Fix a complete physical attempt, its reward and observation rule, and a retry-equivalence set R. For a stop set T define

    n_T(x) = RETRY if x in R and x not in T; otherwise actual(x).

If T1 intersect R = T2 intersect R, then n_T1(x)=n_T2(x) for every outcome. The pushforward transition law, forced retry probability and rewards are identical, provided other entry/terminal/admission conditions are also unchanged.

This explains why naming extra 'success' outcomes can add no new kernel when those outcomes were already non-equivalent outer exits. It fits the inspected native normalization rule. The aggregate 828-collapse observation does not prove this was the exact reason for every individual candidate; an individual mapping would be needed for that stronger empirical statement.

## 7. Exactness of a restricted family is not full optimality

If F_b is a bounded family of permitted proper controllers for the fixed original request, selecting the cheapest independently evaluated member gives

    V_original*(root) <= min_(pi in F_b) J_pi(root).

A restricted family's own optimum supplies an upper, not a lower for the original action space. Missing alternatives stay open. A different binding's old scalar, a partial item's root value or an unverified template cannot replace a complete original-root witness.

All positive outcomes, mandatory costs and native control timing survive composition. Two locally terminating fragments can form a nonterminating global cycle; the whole composed controller remains subject to native properness/evaluation.

## 8. Quantifying the actual opportunity

For naturally encountered distinct bindings i, decompose cold work as

    T_cold = sum_i (S_i + N_i + O_i),

where S is structural construction, N numerical computation and O actual output/scatter/other required downstream work. A shared route costs

    T_shared = B + sum_i (M_i + N'_i + O'_i),

with B template construction/retention and M validation/mapping. Only measured saved terms count. A complete native cold comparison required on every hit belongs in O'; it cannot be omitted from end-to-end acceptance.

If only S changes, the asymptotic speedup is limited by its fraction of the real workload. A tiny 3-second phase in a fixed 244-second computation cannot justify a large ordinary speed claim. Deadline-constrained strategy quality can still change discontinuously near a useful milestone, so fixed-work speed and final cost must be reported separately.

Memory follows simultaneous lifetime:

    M_live = M_parent + M_templates + M_bindings + M_outputs + M_active_scratch,

with shared allocations charged once and all live owners accounted. Summing unrelated measured maxima is not the actual peak; comparing only per-binding peaks can miss concurrent overlap. Releasing storage does not refund logical work.

## 9. What would be genuinely new evidence

The key witness is not a uniform synthetic probability or a second call with the same item. It is one native structural template serving different actual family assignments, with correct unequal bound laws/values and a measured reduction in operations that the existing caches do not already eliminate.

The strongest later outcome is better same-budget search, not merely shared code. It could arise from lower structural memory or earlier completion of relevant binding-specific decisions. It must be demonstrated on the real original-root request and cannot be inferred from a shape count.

No novelty claim is attached to the algebra above. The possible research contribution is the native dependency/observation contract and useful integration of parameter-bearing computation into this implicit exact-transition/verified-upper pipeline. That remains to be established empirically and against broader literature.
