# Mathematical boundary of a heuristic policy finder

These arguments are conditional, independent of game-specific mechanics. Main's native correspondence and numerical acceptance contracts remain explicit. No new theorem ID or broad exact-closure claim is implied.

## 1. Arbitrary proposal scores do not invalidate a checked policy

Fix the native problem theta: original root, complete control state, allowed program class, goal and costs. Let a proposed finite controller pi induce a reachable substochastic nonterminal matrix Q_pi and expected paid reward c_pi. If the native goal is reached almost surely, the finite nonterminal chain is transient and

    J_pi = (I - Q_pi)^(-1) c_pi.

An arbitrary proposal algorithm may use samples, aliased features, an inaccurate neural predictor, beam eviction or a restricted grammar. If acceptance independently establishes that pi is a legal proper controller for theta and its value contract, then pi remains a feasible witness for theta. The proposal method has no role in that implication.

In ideal exact mathematics V*(root) <= J_pi(root). The current implementation may report a tolerance-checked numerical estimate rather than an outward-safe endpoint. Preserve that documented distinction; do not rename the reported float into a rigorously rounded bound. [R12, P08]

For a compatible collection of fully owned accepted artifacts, selecting the least evaluated cost under the existing reconciliation rule preserves the best available reported strategy. This is not a theorem that all future learned proposals are better, and no artifact can be discarded while only its scalar cost survives.

## 2. Search omission and probability omission are different

A beam may discard an entire candidate policy or action alternative. That restricts discovery; it does not prove the omitted alternative uncompetitive.

After choosing a candidate action/program, its native outcomes are chance branches, not additional free choices. Missing a rare adverse outcome changes the controller's law. A proposed tree containing only favorable rollouts cannot be accepted as an SSP policy.

For an illustrative action with probability p of entering a bad absorbing class, n independent samples miss it with probability (1-p)^n. With p=10^-6 and n=10000 this is about 0.99005. Native support/properness checking is therefore valuable even when sampled scores look excellent. This is arithmetic on an invented model, not a measured PoE event.

## 3. Controller state is part of the stochastic state

If a plan says “try once, then follow the base,” its stage bit must be represented. A stationary replacement repeats on each return and generally has another value.

Example: old cost-one retry with success 0.1 costs 10. A cost-two attempt with success 0.5 followed by the old policy on failure costs 7. Repeating the new attempt costs 4. Neither number can stand in for the other graph.

A macro with finite expected local duration can still participate in a globally improper cycle. Whole-controller properness is necessary. Expected macro duration cannot replace the distribution of primitive execution counts if a future hard budget/chance objective is introduced.

## 4. Heterogeneous roles generalize proposals, not fixed-point values

A feature map phi may identify different native states. Equal phi does not imply equal kernels, legal actions, goal truth or values. The native source and context remain separate from phi.

Example: a cost-one retry succeeds with probability 1/5 or 1/100 in two bindings. Its cost is 5 or 100; a cost-20 direct completion is worse in the first and better in the second. A shared role pattern must allow different selected decisions.

A pure reordering of role records, with corresponding action/condition indices remapped, is different: it changes representation order rather than physical facts. A permutation-invariant pooled global feature and equivariant per-role score are natural model properties. They are not proofs that exchanging unequal native parameters leaves the problem unchanged. [P06]

## 5. Complementary decisions justify non-greedy structure search

In the toy r,t,g system, incumbent r→g costs10 and t→g costs100. Alternatives r→t costs1 and t→g costs1. Root costs for neither edit, bridge only, tail only, both edits are10,101,10,2.

Rejecting each intermediate edit because it does not independently improve the old root prevents discovering the complete cost-two candidate. A beam of partial controller structures can keep both components using heuristic promise; final original-root evaluation decides whether the combined candidate is useful.

This is not a counterexample to full-domain policy iteration. It explains why the new finder should not inherit the old “one exact late entry / immediate strict improvement” gate as a universal search rule.

## 6. Bounds inside the finder

Suppose a complete candidate's true cost is bounded below by ell under the SAME root/problem/domain and a compatible incumbent has a justified upper U. Then ell>=U can certify that this candidate cannot improve the incumbent. An arbitrary optimistic score has no such authority; using it to prune is a heuristic choice.

For a fixed prefix with known complete first-exit law H and cost g, valid boundary lowers L give g + H L as a lower for continuations in their justified scope. Unknown exit probabilities, an entry-root mismatch or a restricted model optimum may invalidate that use. The finder may still use a corresponding estimate but cannot label it certified.

An upper estimate above U is not a proof of non-improvement. Full candidate support/closure and valid inequality direction matter.

## 7. Original goal authority

An evaluator that faithfully reports a program's self-declared success evaluates that program, not necessarily a separate requested goal. The new acceptance relation is Check(theta, pi), not Check(pi) alone.

It must verify original target truth over ALL reached success-terminal states/classes and retain the observations needed to make that truth uniform. A sample of final items or the proposer repeating the expected goal hash is not this check. Binding and native semantics, not duplicated strings, establish the relationship.

## 8. Later proof handoff

A checked original-root controller can later be offered to the old exact optimizer as an executable incumbent after full compatibility validation. It does not bring statewise values for arbitrary coarse states, a new admissible lower, action retirement, or an exactness label. Root-only evidence remains root-only. The proof lane independently covers all competitors and its own native relation.

Candidate-local action restriction is legitimate for finding pi in a subset of allowed policies. A lower on that restricted optimum can exceed the full problem optimum and therefore is not a transferable full-scope lower. Heuristic feature classes cannot be reused as proof quotient cells.

## 9. Claim scope of the included tests

[checks/toy_contracts.py](checks/toy_contracts.py) uses finite exact-rational toy models, rebuilt from a trusted toy action table. It demonstrates these failure cases and identities. It does not emulate native mechanics, qualify compiler observations, test production cancellation or show a solver speedup. Native tests in [VALIDATION.md](VALIDATION.md) remain mandatory for the implementation.
