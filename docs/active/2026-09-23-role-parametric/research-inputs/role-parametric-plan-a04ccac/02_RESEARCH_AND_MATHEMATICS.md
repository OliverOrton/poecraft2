# Role-parametric planning: shared structure, instance-specific economics

**Research and conditional derivations, not a new native proof issuer.** See [sources](SOURCES.md), [implementation design](03_DESIGN_AND_MILESTONES.md), and [executed abstract examples](checks/results.json).

## 1. Three different claims

A structural resemblance can support three very different operations:

| Claim | Sufficient basis | Permitted reuse |
|---|---|---|
| Exact state–action symmetry at a fixed native target | Goal/legality/observation, rewards and mapped successor laws agree over the relevant member domain | Quotient or mapped exact numerical result, under the full correspondence contract |
| A common parameterized decision problem | The same structural operations/relations are instantiated with their actual native parameter sets | Programme/query structure and, when proved applicable, symbolic recurrence topology; recompute numerical decisions |
| A useful role-based search sketch | A proposal names achievable subgoals and admissible native capabilities, but its numerical behavior is not shared | Candidate generation/ordering only; complete native construction and checking still decide acceptance |

The second and third rows are the primary programme. Exact symmetry is a special case, not a prerequisite. Different weights or different numbers of tiers need not prevent structural reuse. They do prevent simply identifying the corresponding values unless the required further relation is proved.

## 2. Parameterize the problem, not away its hard parts

Let R be a set of goal roles. A concrete binding b maps roles to actual native goal-slot/family requirements. A parameter record theta_b includes:

- the actual satisfying/member partitions and ragged tier lists for each role;
- native weights after level, pool, group, tags and action-specific modifiers;
- side/capacity constraints and conflict incidence, including the non-goal pool;
- current physical item, requested opposite-side goals, fractures/crafts/locks;
- permitted action/program identifiers, original resource vectors and prices;
- native observation timing, mandatory programme state and output vocabulary.

Writing `(role state z, theta_b)` is a representation of a family, not a proof that z alone is Markov. Any future-observable difference not determined by theta_b and z must stay in the native state or cause refinement/refusal. A template can safely ask a native kernel factory for the needed law without reconstructing crafting rules itself.

For a chosen complete controller topology, numerical specialization has the familiar form

    V_b = c_b + Q_b V_b,
    N_b = n_b + Q_b N_b.

When the native controller is finite and proper, each system has a finite solution. The same code/structural circuit can compute many instances; its coefficients and usually its answers differ. The best action can also differ. There is no reason a policy optimized for one weight vector remains optimal for another.

At a boundary with two permissible choices, the relevant decision is

    min_j { c_b(z,j) + sum_y P_b(y|z,j) V_b(y) }.

Do not select a role by an unqualified “hardest first,” “cheapest first” or arbitrary slot number. Those may be cheap proposal orders, not economic proofs. Native costs and effects have to determine the accepted complete controller.

If the family has different tier counts, keep a variable-length vector `(w_{r,t})_{t in T_r}`. No padding, percentile normalization or merging of all below-tier outcomes is authorized. When the native observation contract does permit aggregation, the exact aggregate must be established per action/member domain, not inferred from the requested T1 label.

## 3. Covariance is not value equality in a heterogeneous item

A coherent renaming sigma moves roles, their complete attributes, actions, state and context together. Under that correspondence,

    V_(sigma theta)(sigma z) = V_theta(z).

This is a statement across consistently relabelled problems. It does **not** imply

    V_theta(sigma z) = V_theta(z)

when theta is held fixed and different missing families have different difficulty. For example, with weights `(1,10,100)`, an identical “two goals present” count can leave either a weight-1 or a weight-100 target missing. The remaining expected work differs.

For exact symmetry at one fixed target, the familiar sufficient conditions are equal terminal truth, corresponding allowed choices at the same information boundary, equal relevant reward and equal probability into every mapped successor class. Preserve concrete resource names even when original scalar prices happen to coincide. State-only canonicalization without action/role lifting is insufficient. [P1; R13]

The implementation should therefore separate a cheap **shape key**, a complete **native binding/kernel key**, and a **cost/certificate context**. A shape hit can return a reusable recipe for construction; it cannot return another binding's cached probabilities or upper value. Canonical hash equality is still only a lookup, not a substitute for its existing equality owner.

## 4. Why the selected pilot can affect strategy quality

A template that says “repeat until named role A succeeds” may throw away outcomes containing a different useful role B. A more general family can consider stopping at a useful attained subset and then adapting its remaining plan to what actually happened. This does not require A and B to have the same probability or cost.

Here is an exact illustrative setting, not a PoE model. Each attempt costs c. It returns mutually exclusive observed outcome i with probability p_i, or an unchanged retry endpoint. Accepting outcome i enters a fixed, complete proper continuation with cost u_i, without reentering the changed decision. In this first toy, rejection returns to **the same full item/control entry at zero extra cost**; this is an explicit toy rule, not a PoE reset assumption. Paid recovery is treated below. For an accepted set T with positive mass,

    J_T = (c + sum_(i in T) p_i u_i) / sum_(i in T) p_i.

Derivation: the complete equation is `J_T = c + sum_T p_i u_i + (1-p_T)J_T`. This is valid only under the identical-return and complete-tail hypotheses.

Take `c=1`, `p_A=0.1`, `p_B=0.4`, `u_A=5`, `u_B=8`. Then:

| Accepted outcomes | Cost |
|---|---:|
| A only | 15 |
| B only | 10.5 |
| A or B | 9.4 |

The combined structure is cheaper despite unequal probabilities and tails. Changing `u_B` to 100 makes B undesirable to accept; “any progress is good” is not valid either. In general,

    J_(T union {j}) - J_T = p_j (u_j - J_T) / (p_T + p_j).

Thus an extra accepted outcome helps in this special setting exactly when its complete tail is cheaper than continuing the old attempt. An opposite-side suffix obligation can change u_j and reverse this conclusion.

This identity motivates a **continuation-sensitive progress frontier**, not a numerical shortcut. In the engine, rejected outcomes may destroy progress, preserve different side states, incur different paid recoveries or lead to different anchors. Then retain the vector first-exit law and solve the actual composed controller. Do not substitute the scalar equation, collapse exit members to a representative, or supply an old root scalar as a missing tail.

A single craft can acquire several goals. Outcome labels must therefore be disjoint native classes such as exact attained subset plus all remaining control-relevant state. Summing individual goal-hit marginal probabilities double-counts simultaneous hits. One toy law with A-only .2, B-only .1 and both .3 has union probability .6, not .9.

## 5. Ordering, preservation and repair are coupled

A second exact example makes the economic need for parameterization clear. Acquire role i by a cost-c_i retry with success p_i. After i is acquired, attempt j at cost c_j with success p_j. A failure of j loses i with conditional probability ell_j; otherwise i remains. The two-state equations yield

    J_(i then j) = c_j/p_j + (c_i/p_i) [1 + (1-p_j) ell_j/p_j].

With unit costs, `p_A=.1`, `p_B=.5`, and all failures erasing the first role, A-then-B costs22 and B-then-A costs30. If the B-first route can retain B when A fails, that latter cost becomes12. The best order is not determined by p_A and p_B alone; preservation and cost matter.

The native role family should encode which roles are protected, attempted, lost or newly attained and the actual native mechanisms realizing that relationship. It must not assume that every family has a targeted action, or that side protection acts per modifier. Actual operator contracts determine applicability.

## 6. What can be shared without assuming equal coefficients

Useful forms of structural reuse include a typed sequence of native capability queries, the dependency pattern connecting setup/attempt/cleanup, a role-indexed exit classifier, and a bounded search over alternative attainment orders. The same skeleton may serve Fire, Cold or asymmetric armour families with different bindings.

Some numerical structure may also be reusable. An expression such as `(c+p_A u_A+p_B u_B)/(p_A+p_B)` can be instantiated repeatedly without deriving the algebra again. Sparse matrix sparsity patterns may be reusable when topology truly matches. But native pair discovery, coefficient construction, elimination fill, conditioning and support changes still have costs. Do not implement a generic symbolic parametric-MDP solver in the pilot or claim a large numerical saving merely from shared syntax. [P2]

A support change from `p>0` to `p=0` can turn a retry into a trap. For a finite fixed controller and identical positive support, almost-sure reachability can be related through graph structure, but there is no uniform finite cost bound as escape probabilities approach zero. The initial implementation does not transfer even this property between native certificates; independent checking remains.

Likewise, sharing a selected-controller kernel does not establish equivalence for every competing action. Adding another comparison action can require more observations and a new native layout. The full native action scope and lower/proof ledger stay untouched by this proposal family. [R13]

### Paid recovery and re-entry change the formula

In a more realistic renewal model, let rejecting outcome i have a complete expected recovery cost r_i to the identical entry, and let neutral failure have mass p_0 and recovery r_0. For fixed tails that do not re-enter the modified stage,

    J_T = [c + sum_T p_i u_i + sum_(i not in T) p_i r_i + p_0 r_0] / p_T.

Adding outcome j changes the value by

    p_j (u_j-r_j-J_T) / (p_T+p_j).

The threshold is therefore recovery-adjusted. The earlier unadjusted example is exactly the r_i=r_0=0 special case. A costly rejected outcome can be worthwhile to accept even when its tail is expensive, because accepting avoids repeated paid recovery. Every native recovery must have a real legal route; setting it to zero is not permitted.

If a tail revisits the edited decision, its value also depends on the changed controller. If rejected outcomes return to different contexts, one scalar J is insufficient. In either case use the complete coupled first-exit equations and re-evaluate the actual original-root graph; do not insert old-tail values into the scalar threshold and certify a recurrent change.

### One native experiment can answer several unequal goal queries

There is a particularly useful exact subcase. If the carrier and complete native setup/attempt/cleanup programme are unchanged, changing which goal outcomes we prefer need not change the physical attempt law at all. A single joint outcome distribution can assign different probabilities to A, B and C, while answering several stopping/continuation queries without reconstructing that physical experiment.

Let K(x,a) be that complete value-independent attempt law. A target set T defines a query/exit classifier h_T on its native outcomes. Compute K once, then apply different h_T and tail evaluations. The resulting option/retry kernels and values generally differ. This is **multi-query reuse of one law**, not a symmetry assertion that A, B and C are equally likely.

The premise fails when T changes actual targeted action, intermediate stopping, hidden observation, cleanup or retained-state layout. The shared law must retain all distinctions needed by every query. The union layout may cost more than the saved builds; account for that. Existing `outcomes`/attempt/template caches may already deliver this reuse in a common context, so the native inventory must locate an actual duplicated construction before claiming a new saving. Cross-context reuse is not authorized by equal shape keys.

This is a practical bridge between the broad role idea and a small pilot: preserve different role probabilities, reuse a genuine common experiment where available, and reoptimize what to do with its outcomes.

## 7. Feasibility is the authority boundary

Let Pi_b be all policies admitted by the original native target and let C_b be the finite set of complete role-generated policies actually verified for binding b. If `C_b subset Pi_b`, then

    V_b* <= min_(pi in C_b) J_pi(root).

This is why a restricted role family can improve an executable upper without being an exact abstraction of the entire solver. It does not establish that the best template is globally optimal, or that omitted templates/actions are inferior. The old cheaper verified policy must survive all failed, incomplete or more expensive proposals.

Properness must be checked for the composition. Two locally terminating fragments can alternate forever globally. A small positive branch into an uncovered or trapping state also prevents an executable claim. A useful role schema is consequently a **proposal plus native binding and complete checking**, not another layer that manufactures value certificates from descriptor similarities.

The smaller representation can remain mathematically exact for each instantiated controller even when only its proposal selection is heuristic. The loss of a global optimality guarantee comes from limiting searched policies, not necessarily from approximating transition probabilities. This distinction is central to the user request.

## 8. Relation to primary research

[P1] Relativized options are directly relevant because they represent a related family of options with relative state/action maps; the paper also discusses approximate transformations. Its discounted learning setup and empirical heuristic case do not authorize native state merging here. We use its distinction between shared subtask structure and a justified mapping, not its learning procedure.

[P2] Parametric Markov-model research treats transition expressions and parameter valuations separately. This supports retaining unequal coefficients while reusing model structure. Solving every parameter region, sharing an optimal policy across regions, or matching support is a separate undertaking, not an automatic consequence of declaring parameters.

[P3] General policies and serializations describe reusable subgoal structures. [P4] PG3 distinguishes using a generalized policy to guide planning from requiring it to solve the task by direct execution. Both are useful architectural antecedents for a sketch that asks native search to complete a candidate. Their classical-planning experiments do not discharge SSP probability, cost or properness premises.

These antecedents make “symmetry,” “options,” or “templates” alone weak novelty claims. A possible contribution is an explicit contract for structural role reuse across heterogeneous native modifier families, with bounded goal-acceptance/continuation composition, complete native grounding and separately measured coverage, construction and policy-quality gains. This review does not establish novelty or claim an exhaustive literature search.

## 9. Native tests and remaining unknowns

The [31 executed abstract tests](checks/results.json) verify the examples and intended distinctions. They do not inspect native family weights, guarantee that the proposed option family is absent, or establish a faster/cheaper actual controller.

The next native evidence must establish: actual unequal tier/weight vectors; a real missing multi-role composition or repeated structural cost; complete observation/control semantics; and an end-to-end same-scope result. Equal names, schematic topology and a lower private estimate cannot substitute for those tests.
