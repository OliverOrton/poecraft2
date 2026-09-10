# Proposed canonical addition: selecting a proper controller from completed rows

**Placement:** `docs/solver/mathematics/policies.md`, adjacent to properness and fixed-policy construction. This is authored research text to reconcile, not an automatic accepted claim or repository edit. Source pin: `44fdca8ebf88b9b87b2bd832360cb62128e3968a`.

## Properness can be synthesized before minimizing expected cost

A finite set of complete native-compatible rows can contain a proper policy even when a locally greedy policy is improper or reaches an unfinished frontier. Choosing a proper policy and evaluating its expected cost are separate tasks. The first can use transition support; the second requires the complete probabilities, prices and numerical contract.

Let D be a finite semantic state domain and G its true goals. A construction may additionally terminate at an independently executable, entry-scoped continuation which is committed to completing the original goal. A heuristic value, an unexpanded state, a finite implementation ceiling, or a locally terminating option with unresolved exits cannot serve as that continuation.

For X contained in Y, define APre(Y,X) to contain states having a permitted complete row whose support stays inside Y and has positive probability of reaching X. Almost-sure reachability on a finite fully observable MDP is characterized by the nested fixed point

\[
W=\nu Y.\mu X.(G\cup\operatorname{APre}(Y,X)).
\]

One finite algorithm starts Y at D, grows X from G under this predecessor rule, replaces Y by X, and repeats until stable. The final inner construction provides ranks and selected rows. Unlike a strict outcome-by-outcome descending rank, these rows may have upward or sideways outcomes. They must stay inside the final W and have at least one positive lower-rank outcome at each nonterminal.

**Properness argument.** Suppose the resulting fixed controller has a reachable non-goal bottom SCC. Select a state of minimum rank in that SCC. Its positive lower-rank edge leaves the SCC, contradicting that it is bottom. Every reachable bottom SCC is therefore a goal. The finite chain absorbs almost surely, and with finite immediate costs its expected total cost is finite. This is the existing finite-controller properness argument applied constructively; it is not an optimality argument.

A proper policy's reachable domain cannot be eliminated by the nested fixed point: its selected rows stay inside that domain, and every state has a finite support path to a goal. This gives completeness for the finite supplied fully observable action view. It does not give completeness for an incomplete implicit native graph, a representative with unproved member correspondence, or a controller language with additional unmodeled observational constraints.

**Observed decisions.** Every positive-probability direct outcome must stay in Y. Each positive-probability offer needs a permitted choice that stays in Y. Some direct outcome or offer must permit positive progress into X. Fix the actual offer-local choice as part of the controller. A later numerical choice change invalidates that selected-controller witness unless checked again. Choices at indistinguishable observations cannot be assigned incompatible decisions merely because the construction gave them different IDs.

**Counterexample to a stricter initializer.** At s, a first-listed cost-1 action goes to t; another cost-2 action reaches the goal with probability one-half and t otherwise. At t, a cost-1 action returns to s. No strict rank can assign either state if every non-self successor must already be assigned. The first-listed choices form a closed cycle. Nevertheless, selecting the cost-2 escape and return gives

\[
J(s)=2+\tfrac12J(t),\quad J(t)=1+J(s),\quad J(s)=5,\ J(t)=6.
\]

If SCC repair sets both state values to infinity and demands a finite one-row Q before replacing a decision, it may reject that escape because t is a different infinite successor. Owner-self elimination does not eliminate an entire mutually dependent component. This is an incompleteness of that seed/repair construction, not permission to relax the final evaluator.

**Partial views.** An unknown positive-mass successor is not a goal. A failed pessimistic construction says only that the current complete-row view does not contain the required controller. New completed rows or compatible frontier certificates can change the answer. Unbuilt alternative actions may be omitted from a candidate's search space, but not from the full-scope optimality ledger. A proper policy on a restricted action set supplies an upper after native qualification, never a lower merely because the restricted problem was solved exactly.

**Entry scope.** A losing component unreachable from the selected root need not be repaired to publish the root controller. Conversely, root properness does not establish a continuation from that losing component. Independently certified frontier controllers must retain their initiation, observation, target, pricing and commitment semantics. A generic pair of locally terminating options may still alternate forever.

**Performance limit.** A cost-1 retry succeeding with probability 10^-9 is proper and costs 10^9 in expectation. Qualitative progress does not establish usefulness. Keep native numerical cost evaluation, ordinary policy improvement, and full-scope lower comparisons separate. A new evaluated seed should survive optional speculation through the existing portfolio, but an unevaluated support witness cannot be published as an upper.

## Source correspondence to reconcile

- Current restrictive initializer: `initialize_focused_proper_policy()` in `solver_solve_bellman.cpp`.
- Current repair: `repair_improper_policy()`; nonowner infinity handling in `evaluate_sparse_policy_row_impl`; nonfinite rejection in `sparse_policy_row_precedes`.
- Completed-row upper view and different progress-based seed: `try_install_reachable_incumbent` region in `solver_solve_constructive.cpp`.
- Native controller/observation qualification remains with the existing fixed-policy, compiler, assertion and independent evaluator owners.

The current Ring/Amulet cases have not been shown to instantiate the counterexample. The proposed implementation must make that attribution or preserve a negative finding. Passing a new generic helper's test alone does not establish repair of the ordinary path.

## Knowledge dispositions for the receiving session

| Finding | Basis | Required disposition |
|---|---|---|
| Existing all-outcomes-descend initializer is sufficient but not complete for cyclic properness | Source plus exact two-state example | Link to existing properness claims; add native old/new fixture if selected |
| Temporary infinity inside a closed SCC can hide a mixed escape row from finite-Q repair | Source chain including canonical nonfinite rejection | Do not equate with the full solver failing every cyclic case; trace actual caller |
| Safe closed region plus positive rank descent permits destructive return edges | Finite fixed-controller bottom-SCC proof | Preserve assumptions, complete support and observations; native mapping remains separate |
| Negative finite-view result is not native infeasibility | Missing-row counterexample and action-subset direction | Keep original action obligations; use a bounded dependency witness |
| Proper seed is not an economical policy or an optimum | Rare retry and omitted cheaper-action examples | Report cost and exactness separately; retain normal improvement/evaluation |
| Current cross-base failure is earlier than a threshold-retirement consumer | Completed current cohort, not the older Conquest-only record | Defer threshold native implementation until an actual compatible comparison exists |

The supplied 26 named checks and 512 exhaustive finite-view comparisons are synthetic reference evidence. They do not alter existing CLM status, establish a new theorem about the full native engine, or substitute for complete native evaluation.
