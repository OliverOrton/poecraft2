# Mathematical contracts and counterexamples

These are conditional arguments for the proposed implementation, not proofs that every native caller already satisfies the hypotheses. The exact-rational examples are abstract models, not PoE mechanics. The canonical documentation must retain the distinction between mathematical law, native correspondence, stored floating-point model, numerical acceptance, and optimality closure.

## M1. Changing the target changes the problem

Let L be the clean target and R the requested-coverage target, with L contained in R. Fix the physical controlled process, nonnegative paid rewards and compatible allowed decision/programme boundaries. Any proper L controller reaches R by the time it reaches L. Where early stopping is legal, truncating at R cannot increase pathwise cost. Thus the optimal R cost is at most the optimal L cost.

This does not imply that a finite-budget search returns a cheaper R incumbent. A weak R search may return a controller worse than a strong L incumbent. A policy that reaches L is feasible for R only with the corresponding request/scope/entry semantics; its *old numerical claim* is not automatically reused under a different stopping controller. A generated R controller is generally not feasible for L.

If target-bound programmes change their own termination/gating, audit that semantic transformation. An allowed programme must still execute required internal cleanup; user-target attainment does not authorize stopping midway through mandatory native work. Comparisons must specify the same legal decision boundaries and permitted programme grammar.

## M2. Zero global lower is valid; stale clean lower is not

For nonnegative costs, h(s)=0 is an admissible lower even if the optimum is infinite. At target states its boundary value is zero; for every action, 0 <= c(s,a)+sum P(s'|s,a)0. This elementary validity says nothing about discovery efficiency.

Example: root pays 1 to a covered dirty state, then pays 100 to reach a clean goal. L has value 101; R has value 1. Reusing the clean root potential 101 as an R lower is invalid. Setting only the new dirty terminal to zero leaves the predecessor inequality 101 <= 1 violated. The entire consumed witness must be target-valid, not cosmetically normalized at its terminal or final report.

A profile which reports zero but used 101 earlier to retire a candidate is not target-neutral. The proposed poison tests therefore inspect retirement/publication consumers and not just output scalars.

## M3. Fixed-row value must survive neutral global proof

A complete row with immediate expected cost c, self-return p<1 and otherwise only target exits satisfies J=c+pJ, so J=c/(1-p). This is the exact cost of repeating this selected row. It is a feasible upper if the row/programme is valid and proper in that entry domain. It is not a global lower when other actions exist.

Example: c=2,p=1/2 gives J=4. Another deterministic allowed action costing 1 makes the MDP optimum 1. A neutral global lower of zero can coexist with both positive policy values. Turning every positive `candidate.lower_q`-named intermediate into zero would break the selected-row calculation if that field was also carrying J. Separate the semantic channels before changing consumers.

p=1 does not become a finite policy by choosing a numerical ceiling. A zero-cost self loop is still nonterminating. The independent evaluator's properness and unresolved-mass checks remain necessary.

## M4. Executable evidence and global proof are distinct

For a proper finite selected controller with nonnegative substochastic Q and expected immediate c, its value satisfies V=c+QV. A checked finite supersolution u >= c+Qu bounds that controller's value from above when the appropriate transience/properness conditions hold. A controller-specific lower bounds J_pi, not necessarily V*.

A global MDP lower needs inequalities over the full relevant action scope and native member domain, with the right target boundary. A restricted policy value or a selected-controller quotient cannot be promoted because no more candidates were searched. Frontier exhaustion, a stable proposal beam, an exhausted attempt cap and an all-action optimality certificate are different facts.

The new neutral profile preserves selected-policy evaluation and properness while withholding global closure. The ordinary clean profile retains its existing all-action proof path. Certificate/checker separation has established precedent [P1]; the new work is the native consumer/identity discipline, not new fixed-point theory.

## M5. Complete decision programmes are semi-Markov objects

For a programme starting at x, let g(x) be its expected paid resource cost until a legitimate exit and H(x,y) its probability of exit y. Its execution may contain several primitives, observations and loops. If it terminates almost surely with finite expected cost and all exit mass is accounted for, a fixed outer controller satisfies V(x)=g(x)+sum_y H(x,y)V(y).

Primitive-count reward is a separate expected reward accumulated over the same native programme, not one per macro. For duration-tail questions, joint duration/exit distributions would be needed; they are not selected here.

Initiation, mandatory internal execution and termination boundaries are part of the contract [P2]. A real operation inside a programme is not thereby independently admissible, and a reached example does not prove the whole proposed initiation set.

## M6. Selective completion can change recurrence economics

An invented two-state example has root R and a valuable held state H. A root attempt costs 2, directly succeeds with probability 0.1, enters H with probability 0.2 and retries at R with probability 0.7. The old H action pays 1 to return to R:

V_R=2+0.7 V_R+0.2 V_H,    V_H=1+V_R.

This gives V_R=22. A new H fill action costs 1, succeeds with probability 0.5 and stays at H otherwise, so V_H=2 and V_R=8. All acquisition, retries and recovery are charged.

If the same fill costs 100, V_H=200 and the root value becomes 140. Retaining progress is then worse than the old root policy. Hence a branch based only on goal count or a slogan that junk is harmless is inadequate. The programme must compare complete native candidates.

The native hypothesis is the branch identified in source: intact valuable holdings can still return to destructive acquisition when target occupancy is low. A preserving alternative is worth testing, but this toy proves no PoE gain.

## M7. Boundary continuation is not zero and can change the decision

For a local programme with cost g, internal transition Q and external-port law H, V=g+QV+Hu, where u contains compatible continuation values at the *actual* external ports. For a scalar example v=1+(1/4)v+(1/2)u, v=(4+2u)/3. A cheap exit and an expensive exit with the same local topology imply different local decisions.

An unknown port is not zero, the base root value, or another binding's tail. If the candidate is a full original-root controller, explicitly represent its recovery branches instead. If it is a later local patch, obtain proper exact-entry continuations and complete composition through existing owners.

A one-shot deviation followed by the old controller is not the same as a recurring replacement. For an old cost-one p=0.1 retry, J_old=10. A new cost-two p=0.5 action used once then old gives 2+0.5*10=7, but repeated new actions give 4. Final root checking evaluates the actual emitted controller, not a local advantage estimate.

## M8. Piecewise choosing proper fragments can be improper

Each of two fragments can terminate at the other's entry with probability one; their composition then cycles forever without a goal. Similarly, two proper complete policies can induce an improper controller if one naively switches between their local decisions without a valid improvement/properness contract.

Selecting the cheaper of two **whole root policies at invocation** is safe when each complete policy is compatible and checked. Arbitrarily copying their best-looking state actions is not the same operation. Current's root-only proposal service uses the first interpretation. Existing proper fixed-policy improvement must keep its own hypotheses.

## M9. Heterogeneous roles share construction, not value

A common controller shape may use V_b=c_b+Q_b V_b for binding b. Different weights, tier vectors, blockers, prices, persistent item state or target requirements change its coefficients and may change the best action. A lossy role feature can rank proposals; it cannot identify exact native cache keys or policy certificates.

The selected shared factory is a common programme-construction schema. It is not an exact state quotient, and the prior failed micro-template/memo experiments are not reopened. The same actual native programme may have reusable structural information, but all matching, numerical, output and validation work must be counted before a reuse claim.

## M10. Budget and identity laws

Logical reforge work spent on failed/rejected/cancelled construction remains spent. Releasing scratch reduces live memory, not cumulative work. Concurrent parent+candidate+checker ownership is the relevant cap; adding unrelated historical peaks is not a live peak, and ignoring allocation overlap is not sound accounting.

A checked result is bound to exact root/control, target, data, action/program scope, prices and its numerical/evaluation contract. Price-only re-evaluation may use price-independent quantities when actually owned; an old scalar cost does not survive repricing automatically. L/E normal forms may share semantic identity only when their equivalence is proved; R and neutral/ordinary proof capabilities remain distinguishable.

## M11. Search quality and finite candidate families

A finite controller sketch with holes describes a candidate family. Oracle-guided synthesis gives a useful precedent for combining proposal construction with formal checking [P3], but does not make our bounded search exhaustive. Eight or 24 checked candidates are not an optimality proof. Feedback can generate useful coordinated changes without demanding every isolated partial edit improve; each completed selected controller still receives complete checking.

Base-policy lookahead and rollout provide relevant policy-improvement foundations [P4]. This plan does not import their approximation guarantees without their hypotheses, nor does it assume a heuristic failure proves no good completion exists.

## Canonical integration

Integrate M1–M3 and profile contracts into representation/lower/search-resumption owners; M4–M8 into policy/upper/numerical owners; M9 into representation/search notes; M10 into resources/identity; M11 into finder mechanism/research disposition. Cite the native fixtures that establish each implementation premise. Do not create a new broad theorem ID merely to rename these arguments.
