# Mathematical obligations and useful counterexamples

These are conditional derivations and abstract examples, not native PoE measurements. Primary background is in [SOURCES.md](SOURCES.md); the project's existing policy/lower/numerical chapters remain the implementation authority.

## 1. Two meanings of “exact”

Exact selected-policy evaluation concerns a declared transition/reward/control model. Optimality closure concerns all policies in a declared scope. Neither mathematically requires a terminal item with no extra explicit modifiers.

A terminal predicate is part of the optimisation problem. PRISM's reachability-reward definition, for example, accumulates cost until the declared target is reached; changing that target changes the queried expected reward. Its nontermination convention is a separately specified property, not something to copy implicitly into this engine. [P1]

## 2. Target inclusion and the bound direction

Let T be the clean terminal and C the coverage terminal, T subset C, in the same underlying native decision process, with nonnegative costs and compatible allowed stopping at C. A complete proper T-policy, truncated at its first C hit, is a feasible C-policy. Pathwise accumulated cost to C is no greater than cost to T. Taking expectations and infima gives:

    V*_C(s) <= V*_T(s).

The inequality can be strict by any factor and T can lack a proper finite policy even when C has one. It does not imply a monotone runtime/state-count relationship for a heuristic implementation under finite caps.

A valid lower for C is therefore also a lower for T within the same model/scope. A T lower need not be valid for C. A proper C-policy need not reach T and is not a T upper. A T-policy can supply a C upper after the permissible stopping interpretation is checked. These transfers require unchanged native law, prices, scope and control memory; strings such as “same base” do not establish them.

For lower certificates, newly terminal states require zero continuation. Rechecking only those entries is insufficient: predecessor constraints can fail after terminal values change. For `root --cost 1--> covered_dirty --cost 100--> clean`, the clean values `(101,100)` cannot certify loose value 1, even after manually changing the dirty value to zero while retaining root 101.

Finite-MDP fixed-point certificate research supports separating proposed values from a complete target-bound check. It does not discharge this engine's native relation or make a stale certificate portable. [P2]

## 3. Exact equivalence of occupancy formulations

Assume a valid native domain in which satisfying requested slots are disjoint, one slot represents at most one coexisting affix, and each has a fixed side. With all requested slots required, let r_p and r_s be the number of requested prefix and suffix slots. Then coverage implies:

    n_p >= r_p and n_s >= r_s.

Under coverage, `n_p=r_p && n_s=r_s` holds exactly when every occupied explicit is a satisfying requested affix. Hence the explicit-count target equals the legacy clean target.

For A4 these counts are 3 and 1; for A5, 3 and 2, subject to native validation. Extra-below-tier members cannot fit while all requested satisfying members are also present. Implicit affixes are not counted by this argument.

For any-k thresholds with k less than the listed-slot count, fixing total occupancy to k is not equivalent. Example: three requested distinct affixes all present and no extras satisfies the old threshold-two clean goal, but violates an exact-two occupancy target. Therefore the general clean relation remains `n=g`, not `n=k`.

Open-slot constraints normalise using the actual cap. `open_suffixes>=2` is `n_s<=cap_s-2`, not `n_s=1` unless the cap and required suffix lower bound make that equality follow.

The formula equality makes the optimal values equal under the same model. It does not itself accelerate search. If E is normalised to L and all consumers use the same contract, identical logical execution is the expected control result, not a disappointing research outcome.

## 4. A useful historical transfer theorem

Suppose an old loose-target optimum J is certified for the SAME underlying model and scope as the proposed clean comparison. Suppose that certified policy is also complete/proper for the clean target and its first loose successes are already clean, with the same cost J. Then:

    J = V*_C <= V*_T <= J,

so V*_T=J. This is a sufficient sandwich argument. It fails if the historical result is only an upper, the scope/prices/data differ, dirty successes occur, or the claimed old closure is unsupported.

A historical result labelled “exact evaluation” is not necessarily an optimality certificate. Classify the actual status and numerical correspondence before applying this theorem.

## 5. Coverage is a reversible state predicate, not an irreversible phase

Let C(x) indicate current requested coverage. Removing a desired modifier can move a state from C back to its complement. Appending an `ever_covered` bit and then assuming coverage persists can allow false clean success or suppress necessary reacquisition.

For same-target search, calculate current coverage and current occupancy from the actual state. A phase label used only for diagnostics is harmless; a phase that changes allowed actions or terminal semantics needs its own proof and is not a representation-only change.

## 6. Decomposition needs the correct boundary value

For a fixed proper clean policy pi and first permitted coverage time tau_C, the strong Markov/tower argument gives:

    J_pi,T(s) = E_pi[sum_{t<tau_C} c_t + V_pi,T(X_tau_C)].

The tail includes cleanup, losses, returns and reacquisition. Using zero at coverage instead produces the cost of an easier stopping problem. If the controller contains mandatory programmes, the stopping boundary must respect actual control/observation timing; a primitive event inside a commitment is not automatically an intervention port.

For optimal clean search, decomposition before C uses V*_T on C, not zero. Knowing the coverage-optimal policy alone is insufficient. Example: route A reaches dirty coverage for 1 and then requires 100; route B reaches clean coverage for 5. The coverage optimum selects A at cost 1; the clean optimum selects B at 5. Appending cleanup to the coverage-optimal route gives 101, not 5.

Thus `V*_T - V*_C` is the semantic cost difference between two optimisations, not generally the cleanup cost of either fixed policy.

## 7. Complete cleanup/loss recurrence

An illustrative cycle has root R and dirty covered D. Acquisition costs a and reaches D with probability p, otherwise returning to R. Cleanup costs b; it succeeds with probability q, loses coverage and returns to R with probability l, and otherwise stays at D. For p,q>0 and q+l<=1:

    V_R = a + (1-p)V_R + p V_D
    V_D = b + l V_R + (1-q-l)V_D

so:

    V_R = ((q+l)*a/p + b)/q,
    V_D = V_R - a/p.

Loose value is a/p. Clean cost includes losing acquired work. The example also shows that a lower post-coverage holding cost does not follow from its affix count.

A local programme's almost-sure exit does not imply global goal properness: two locally terminating programmes can deterministically hand control to each other forever. Final acceptance must check the complete root controller, not only programme kernels.

## 8. Same-law one-action probabilities

For a complete choice-free native distribution p over successors:

    P(T) = sum_x p(x) 1[T(x)],
    P(C) = sum_x p(x) 1[C(x)],
    P(C and not T) = P(C)-P(T) >= 0.

A positive marginal for one requested slot does not imply all-slot coverage. Even full coverage can have zero clean probability when every covered outcome includes an unmatched explicit affix. Capped/partial enumeration cannot certify that zero. Per-slot probabilities cannot generally be multiplied because joint outcomes are dependent.

## 9. Proof and performance status

Unknown action coverage remains open even when a candidate policy is complete. A restricted candidate search can produce an upper for the original task if the whole candidate is legal and checked, but its restricted optimum is not an original-scope lower.

The safe-zero fallback is a mathematical lower for nonnegative costs, not proof that a run will make progress or close the gap. Removing unsupported clean producers from BOTH comparative arms may make the experiment valid but changes the algorithm profile relative to ordinary defaults; it must be disclosed.

Counters for newly nonterminal covered-dirty states help locate the additional work induced by the task. They do not prove that all such work is avoidable. Nor does a looser target's smaller value guarantee fewer enumerated states in a finite heuristic solve.

## 10. Native evidence required

All abstractions above need native slot/domain, target/compiled-predicate, programme stopping, complete action scope, price and identity correspondence. The tests in [tests/goal_contract_checks.py](tests/goal_contract_checks.py) exercise the algebra and counterexamples only. They are not a substitute for native differential tests or original-root policy evaluation.
