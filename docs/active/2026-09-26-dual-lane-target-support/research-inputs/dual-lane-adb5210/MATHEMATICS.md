# Mathematical contracts: target, observation, policy, and proof

These are conditional arguments and explicit counterexamples. They do not establish that every native implementation premise is already satisfied. Native coefficients, complete action/programme scope, observed choices, identity and the existing numerical acceptance contract remain required. Tests in `checks/` use exact rational toy models, not PoE mechanics.

## M1. A graph's observation basis is not its objective

Let a controller use physical/control states x, routing tests O, and a requested target G. O may contain predicates not in G. Exact interpretation requires enough state information to evaluate O and the selected native actions. Request correctness separately requires every reached success terminal to satisfy G.

A projection that retains only G can be invalid if two physical states agree on G but disagree on an O-test choosing different actions. Conversely, treating all predicates in O as conjunctive goals can reject a correct controller. Neither direction follows from the fact that O is stored in a type called `GoalSpec`.

For a supported mapping, let sigma_G be the native requested-slot status vector and let n_P,n_S be actual occupancy. The request predicate is evaluated from sigma_G, original rarity, original k, and original terminal constraints. Unrelated observer slots never enter its satisfaction count. An any-k threshold and a condition union are not interchangeable.

## M2. Request-bound policy acceptance

For the controller-induced process with a complete represented state domain, suppose:
1. the starting physical/control state is the original request root;
2. every executed operation/programme occurrence is permitted and uses the original native law and resource accounting;
3. all positive reachable graph-success support is contained in G;
4. all positive reachable continuations are represented, and the graph reaches permitted success almost surely with finite expected nonnegative original cost under the existing numerical contract.

Then the graph is an executable feasible controller for G, with evaluated cost J_pi. This does not prove optimality. A generic graph evaluator that only establishes faithful execution of graph-defined terminals lacks premise 3 unless its compiler/input contract supplies it.

A finite list of sampled terminal representatives is insufficient for premise 3. A sound abstraction must make G constant in each aggregated success cell or prove that every member satisfies G; otherwise refine or refuse. Compiler guard entailment is another route if it is tied to effective compiled routing and the original goal, not raw JSON decoration.

## M3. Target inclusion and nonnegative cost

For clean target T and coverage target C with T subset C, on a common underlying legal process with nonnegative costs, a proper controller reaching T can be stopped when it first legally reaches C. Its accumulated cost cannot increase. Therefore

    V*_C(s) <= V*_T(s).

This is an optimal-value statement, not a monotonicity guarantee for finite-budget heuristic algorithms. A solver can find a worse upper on an easier task. Independent policies found in different runs need not have costs in the same order.

All stopping points must be legal in the controller/programme semantics. If macro interiors are not decision points, either formulate the process at native primitive control states with the appropriate memory or compare at the admitted complete-decision boundaries. Do not drop mandatory paid work by silently treating an internal snapshot as a free stop.

## M4. Direction of bound transfer

If L_C <= V*_C, then L_C <= V*_T. A lower for T generally does not lower-bound C. A coverage-feasible controller generally is not a feasible T controller. Even a T graph used without an earlier stop need not be the optimal or cheapest C controller.

Toy: root --1--> covered-but-dirty --100--> clean. V*_T=101 and V*_C=1. Reusing the clean root lower 101 is invalid for C. Setting only the newly terminal dirty state's value to zero still leaves an invalid predecessor inequality. Rebuild or recheck the whole consumed vector against the changed Bellman boundary.

Checking |L <= J_pi <= U| is not a substitute: with optimum 4, incumbent10 and bad lower12, clamping the lower to10 creates a superficially consistent interval but still an invalid optimum lower.

## M5. Why the selected zero profile is sound, and what it does not prove

For nonnegative immediate costs and nonnegative successor values, v=0 satisfies the lower subsolution inequalities, with v=0 on all targets. Thus zero is a valid lower seed/authority for either target. It remains valid if a target is unreachable; it supplies no finite upper.

However, emitting zero **after** unqualified pruning does not retroactively justify the removed work, a solved label, or an exact claim. The new profile must prevent such authority from being consumed before decisions happen. Its partial search can still produce valid uppers after complete independent checking. Lack of a positive lower is not permission to infer optimality from a stable policy.

The selected TargetNeutralZero profile intentionally withholds general exact closure. A future full proof implementation can use zero as an initial admissible seed and perform valid all-action Bellman/closure work, but that is a different capability qualification. A complete selected-policy calculation does not establish all-action coverage.

## M6. Uniform finite-prefix isolation test

Let an unavailable clean proof producer yield arbitrary values h. A correctly isolated zero profile's certified outputs and proof-retirement decisions must be independent of h, including stale cache contents. Holding all actual graph/work choices fixed, changing h to large positive values cannot change its published lower from zero or discharge an obligation. This is a falsifiable implementation property; it is stronger than checking the final displayed scalar alone.

Non-authoritative search scores may deliberately influence proposal order. Record that dependency separately. To isolate target effects in L/R experiments, use the same declared scoring/proof policy; do not invisibly switch ranking or action vocabulary in only one arm.

## M7. First-coverage decomposition applies to a fixed controller

For a proper controller pi with clean completion time tau_T, and a legal first-coverage boundary tau_C <= tau_T, integrable nonnegative cost gives

    J_pi,T(s) = E_pi[cost before tau_C] + E_pi[V_pi,T(X_tau_C)].

The continuation term can include cleanup, loss and reacquisition. It is not zero. A stopped copy of the **same** graph at C can identify the first term if the observation/control boundary is faithfully represented. The difference between independently optimized clean and coverage controllers is not that fixed-policy cleanup bill.

Counterexample: path A costs1 to C then100 to T; path B reaches T for5. Coverage optimum1, clean optimum5; difference4, but the continuation after coverage on A is100. Therefore a successful R solve does not prescribe “solve acquisition then append cleanup” as an optimal clean algorithm.

## M8. Progress is a current-state property, not irreversible phase memory

A state with all requested families plus junk is covered but unclean. A removal can make it uncovered. `GoalAssessment.requested_coverage` must reflect the current item after every operation; it must not latch forever after the first hit. Similarly, a prefix completed before suffix work can be lost or have its native control context changed. The final target predicate is independent of a heuristic label like “cleanup phase.”

## M9. Properness and zero-cost cycles

Zero lower values and finite graph size do not establish properness. A non-goal zero-cost self-loop may have finite accumulated reward but does not satisfy the requested almost-sure completion condition. Keep the native improper-component and unresolved-mass rules. A cheap local fragment cannot create a valid upper if its returns form a non-goal recurrent component.

## M10. L/E semantic equivalence is specific

For validated all-required, disjoint, fixed-side slots, covering all slots and constraining P/S counts exactly to their requested totals admits no unmatched affix. E can normalize to L with the same semantic identity. For an any-k goal, a clean item may satisfy more than k requested slots; setting total occupancy to k would strengthen the task. Range and extras constraints are separate. “At least two open suffixes” is a capacity-relative upper count bound, not a universal exact-one-suffix rule.

## M11. Provenance-aware reuse

Sharing an immutable evaluated graph is safe only with the required context: native data, exact root/control state, effective operation/programme scope, target predicate and reward/economy. A hash collision or a shared lossy role description is not semantic equality. L/E authored syntax may differ while proved canonical meaning agrees; R does not share that goal identity.

A cached numeric policy value can be reused under repricing only if the retained complete resource expectation permits correct recomputation; a scalar from a previous economy is insufficient. No general cross-economy caching is selected here.

## Research placement

PRISM's target-based reward queries distinguish model transitions, stopping properties and rewards. Fixed-point certificate work distinguishes candidate numerical computation from checked target-specific proof. BRTDP is relevant precedent for retaining proper upper policies and lower bounds while selectively investigating a state space. None requires a second native solver, grants PoE-specific correspondence, or predicts that the proposed diagnostic will be faster. See [SOURCES.md](SOURCES.md), P1–P3.
