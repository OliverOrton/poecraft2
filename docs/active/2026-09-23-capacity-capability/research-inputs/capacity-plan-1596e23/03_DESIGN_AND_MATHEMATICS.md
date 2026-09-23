# Native design and mathematical obligations

These are conditional derivations and implementation contracts, not new native certificates. Classical policy/quotient facts already live in the repository. Extend their existing arguments only where this programme changes their application. [R12, R15–R17 in SOURCES.md](SOURCES.md)

## 1. Three resource situations are different

Let K be committed semantic evidence, X staged scratch, D cumulative charged work, and M the currently accounted live footprint. Let B be the unchanged memory allowance and a the conservative additional reservation required by a prospective transaction.

1. **Prospective refusal:** M+a>B before the full allocation is made. This does not imply M>B or that every other task is infeasible.
2. **Actual concurrent retention pressure:** necessary live payload already consumes the budget. Merely deferring the next task may recover little or nothing.
3. **Cumulative work exhaustion:** D reaches its global allowance. Releasing memory cannot refund D or permit additional charged work beyond that allowance.

The first-refusal snapshot must tell these apart. A selected-allocation estimate is not process heap; a post-unwind snapshot is not a pre-refusal peak. Preserve both measured and conservatively reserved amounts.

### Optional-task interruption contract

An attempted task can be locally deferred only if rollback preserves K, removes or consistently retains its staged X, and leaves D at least as large as before the attempt. Any newly committed append-only evidence must remain charged and correctly indexed. All uncompleted coverage remains an explicit obligation.

By induction over completed transactions, K remains valid when each transaction either preserves K or appends a complete result satisfying the old owner's contract. A subsequently chosen complete, proper native controller supplies its usual executable upper. This is a safety argument. It proves neither that another task fits nor that continuing improves cost.

**Service obligation:** do not equate `temporarily unavailable` with `legally inapplicable`, `proved noncompetitive`, or `permanently attempted`. Relevant readiness/headroom changes must reach the consumer under a bounded retry policy. Conversely, repeated unrelated growth is not a reason to retry an identical oversized task indefinitely.

The existing Imprint family exhaustion path demonstrates one special-case separation of optional search work from unresolved exactness. Extending that behavior to another owner requires that owner's actual rollback and coverage argument. A memory refusal is not automatically the same kind of exhaustion.

## 2. Deferred actions do not disappear from optimality

Let A be the original allowed policy/action scope and A' a candidate restriction. For the same native root, goal, costs, observation timing and allowed controller class,

    V*_A <= V*_{A'} <= J(pi),  pi in A' proper and executable.

The first inequality is inclusion of feasible policies. The second follows because an optimum cannot exceed one admissible policy. Thus a restricted private policy can improve an upper for the original request. Its optimum or lower does not become a full-A lower.

Toy negative: two root actions finish for 1 and 10. Deferring the cost-one action leaves a proper cost-ten controller. Reporting 10 as the original optimum or original lower is wrong. Reporting it as a feasible upper while retaining an independently valid lower is sound.

The native implementation must keep omitted/deferred/unconstructed families in the coverage and result-classification contract. Re-optimizing only the admitted rows must not accidentally close the full envelope. In a new private namespace, even an old valid statewise lower needs its transfer relation; retaining the old original-root certificate is a different operation.

## 3. Exact private reduction needs the right quantifier

For a fixed controller decision k and members x,y sharing its proposed cell, require the same terminal truth, selected legality and permitted observed choice, expected native rewards, and probabilities of every successor control/cell. Preserve all persistent information that can affect these facts later.

With these equalities, conditional one-step expectation agrees on cell-constant continuation functions; induction gives agreement for finite horizons. For proper finite chains with finite expected rewards, the limit preserves total cost and absorption. Additional alternatives require their own observations/equality or separate refinement. This is not all-action equivalence just because one selected controller is uniform.

The current global `CalcContext` needs the union of the candidate's required observations. A smaller per-node set need not reduce that global union. Dropping an action without rebuilding the layout need not remove its historical distinctions. Passing the old layout as a refinement parent retains split-only identities rather than coarsening them. These are existing constraints, not missing infrastructure.

A two-member negative: both items have one satisfied goal and the same occupancy, but the proposed action succeeds with probabilities 1 and 1/2 due to a relevant blocker. A representative-only merge is not exact. A later lock/bench/offer observation can create the same problem even when the immediate action agrees.

### Construction savings can be lost at checking

The historical Amulet private layout shrank dramatically, but one small private controller still required roughly 750k native checker pairs and over 1.8 GiB of checker storage. That is a concrete warning against measuring only private state count. [R09]

Admission for a private proposal must include concurrently retained parent, child, graph/parser/economy and checker, not treat the independent evaluator's maximum as additional free memory. If the complete checker cannot fit, the candidate remains unqualified regardless of its source cost.

## 4. Complete mass remains non-negotiable

For a complete action law with successors split into K (known continuation) and U (uncovered),

    E[V(S')] = sum_{t in K} p_t V(t) + sum_{t in U} p_t V(t).

The second sum is not zero because it is unavailable. If every uncovered successor has a valid compatible continuation bounded above by H, its contribution is at most p_U H. But H must be backed at those entries; a finite root incumbent alone provides no such bound. An executable upper also needs actual routes, not just an existential finite number.

Small omitted probability is not harmless under recurrence. With cost 1 per attempt, goal probability p and retry 1-p, J=1/p. Changing or dropping a branch can affect the escape probability and therefore the entire repeated cost. A 0.001 branch into a cost-million trap contributes 1000 even before recurrence. A positive branch into a non-goal absorbing trap invalidates properness entirely.

Streaming or chunking an enormous distribution is therefore not automatically a solution to the memory cap. It needs an end-to-end consumer that preserves every successor/routing obligation without retaining the same structure elsewhere. This plan does not authorize a general streaming-kernel redesign without that contract.

## 5. Cheap local changes do not remove the properness premise

For a complete proper baseline pi with value V_pi, define candidate advantage

    a_mu = c_mu + Q_mu V_pi - V_pi.

If mu is also proper on the complete compatible domain,

    V_mu - V_pi = (I-Q_mu)^(-1) a_mu.

This supports improvement reasoning when the domain and tails are genuinely compatible. It does not certify properness by itself.

Toy counterexample: states x and y each have a cost-one terminal action. Add zero-cost actions x->y and y->x. Against baseline V=(1,1), both switches have zero advantage. Selecting both produces an endless zero-cost cycle, not a proper crafting strategy. The finite vector (1,1) satisfies that cycle's Bellman equations but is not proof of reaching the goal.

The existing full emitted-controller evaluator, scope checks, paid replacement branches and late pairing refusal remain final authority. Do not replace them by a local score, a small residual or a private optimum.

## 6. Memory accounting is a concurrent-lifetime statement

For disjoint actual ownership buckets M_i(t),

    peak = max_t sum_i M_i(t).

Generally this is smaller than sum_i max_t M_i(t). Conversely, after a growth allocation, old and new buffers may overlap; counting only the final new buffer can understate the real peak. Shared immutable payload is charged once, but separate row metadata, references, indices and logical work obligations remain distinct.

Example: phase A holds parent 4 and scratch 6 (total 10). Phase B holds parent 4 and checker 7 (total 11). The simultaneous peak is 11, not 17. In contrast, resizing a payload from 6 to 9 while the old allocation is alive can require 15 before release.

The historical Ring-wide peak is 1,736,778,351 selected bytes. Relative to a 1-GiB cap, the difference is 663,036,527 bytes, about 38.18% of that peak. These are arithmetic facts about a different run, not evidence that a present cache is that large or removable.

Fast/full accounting must agree on the changed ownership inventory at stable checkpoints, and admission remains conservative while storage changes. Existing structural offsets, detached candidates and nested coroutine charges cannot be silently recalibrated to pass a case.

## 7. Separate construction economics from crafting economics

The planner's resource budget is computational; the policy's action count is execution. Making search fit in memory does not cap the number of crafting actions. A low-memory controller can still have a long retry tail.

For a fixed proper native controller, cost and primitive count are separate rewards:

    C=(I-Q)^(-1)c,  N=(I-Q)^(-1)n.

Hard action horizons and completion probability by a horizon require more than N. Similarly, occupancy times remaining cost is exposure, not an additive attribution of removable expense. The next programme continues to optimize original expected Chaos; count and completion distribution remain separate observations/product decisions.

## 8. Proof-status discipline

These arguments justify directions and negative tests. They do not prove current native coverage or establish the cause of the Ring failure. Native correspondence requires the actual first-refusal owner, transaction lifetime, represented domain and compiled graph. Record theorem premises separately from measured outcomes and implementation hypotheses. A no-op or blocked data gate does not turn into a new exactness result.
