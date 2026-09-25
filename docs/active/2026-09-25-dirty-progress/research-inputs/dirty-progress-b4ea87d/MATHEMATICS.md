# Arguments and counterexamples

These are conditional finite-model arguments and exact toy examples, not new native certificates or evidence of an implemented speedup. Native probabilities, numerical tolerance and observation correspondence keep their existing meanings. Sources are in SOURCES.md.

## 1. Three claims must not be conflated

1. **Goal correctness:** the final physical item must satisfy the unchanged strict request.
2. **Search utility:** which incomplete calculations are worth servicing under a finite budget.
3. **Policy economics:** expected paid cost of a complete proper controller.

A safe search heuristic can be economically misleading. A low expected crafting cost can still be hard to construct computationally. Cost-to-go and search-distance-to-go need not agree (P01). This neither vindicates nor refutes any particular native heuristic.

## 2. The debt tie is real, but local

Let `m` be the required count, `g` the satisfied distinct native goal slots, `k` occupied explicits, and `r` a rarity-mismatch bit. The code's nonterminal quantity is

\[
D(s)=\max(1,\max(m-g,0)+(k-g)+r).
\]

At requested rarity with `g <= m`, this reduces to `max(1,m+k-2g)`. For `m=4`, `(g,k)=(0,0)` and `(3,6)` both give 4. For `m=5` both give 5. A true strict terminal is explicitly zero, overriding this expression. When `g>m`, do not use the simplified expression blindly.

No expected-cost comparison follows. At the same dirty-count state, a native preserving cleanup could cost 1, while another context forces expensive recovery. Conversely, a cheap empty-state route can beat an expensive dirty repair. The arithmetic is neither an admissible monetary lower nor a properness proof.

Moreover, this D is used only by a particular initial-row rule. A scheduler ordering full goal masks first does not equate those two carriers because their D matches. This distinction is source-specific, not a mathematical dispute.

## 3. Strict target and diagnostic coverage

Let `T` be the original native success set. Let `A` be the set that has the original rarity and requested slot threshold, ignoring only unrelated explicit occupancy. Then `T` is contained in `A`.

For an original proper policy `pi`, define `tau` as first entry in `A` at the declared valid observation boundaries, or original success if it occurs first. With a Markov-complete state `X` including controller memory, native item, offer/checkpoint context, and finite expected nonnegative cost:

\[
J_\pi(s)=\mathbb E_s^\pi\left[\sum_{t<\tau}c_t\right]
 +\mathbb E_s^\pi\left[V_\pi(X_\tau)\right].
\]

The second term includes all subsequent loss, repair and reacquisition. It is not merely the bill for actions called “cleanup.” If the root already satisfies the observed coverage, the prefix is zero. True final terminals have continuation zero.

Proof: decompose every path's reward at the stopping time, then apply the conditional remaining-cost definition on the same complete state. Finite expectation/nonnegative monotone sums justify the decomposition. This is not optional stopping applied to a learned estimate.

### Stopped-copy implementation

Make a diagnostic controller identical to `pi` up to `tau`, then stop with no further crafting cost. With identical native prefix laws and prices, its expected cost is exactly the first term. Thus

\[
J_{post}=J_\pi-J_{stopped}.
\]

This subtraction uses two complete compatible evaluations. An unguarded stop, wrong root, changed programme, changed chance law or truncated graph breaks the premise. The stopped copy is not a policy for `T` and has no executable-upper authority for the original problem.

If the observation boundary misses an acquisition inside a mandatory macro, it computes the first **boundary-visible** acquisition instead. Label that result. Never count a later-boundary quantity as the earliest physical event by assumption.

## 4. Matrix form and correct first-hit weighting

Partition the proper controller's nonterminal states into `N` (outside coverage) and `D` (dirty coverage). Strict terminal cost is zero. For an initial state in `N`, define

\[
d_N^\top=e_s^\top(I-Q_{NN})^{-1},\qquad
\mu_D^\top=d_N^\top Q_{ND}.
\]

Then

\[
J_{pre}=d_N^\top c_N,\qquad
J_{post}=\mu_D^\top V_D,\qquad J=J_{pre}+J_{post}.
\]

Direct first hits in strict terminals contribute zero. If another boundary classification distinguishes already-clean but not yet terminal states, include that class with its actual continuation, not zero by label alone. `mu` is a **first-hit** measure. It is not recurrent occupation in the original chain.

Toy: root pays 100 to reach dirty coverage. There, pay 1, finish with probability 1/2, otherwise lose progress and pay 10 to return. Then

\[
V_D=1+\tfrac12(10+V_D)=12,\quad J=112.
\]

Prefix=100 and post=12. The original expected number of visits to `D` is 2; multiplying it by `V_D` gives 24, double-counting the tail. The included exact tests check both representations.

For first-dirty-hit probability `p`, conditional tail is `J_post/p` when `p>0`. Report p separately; do not confuse conditional and unconditional values. An unknown/capped first-hit calculation is not zero.

## 5. Interpretation limits

A large post-acquisition bill can be unavoidable under the action scope. Reclassifying the target as success would remove it by changing the problem, not by improving a policy.

A small post-acquisition bill does not falsify the broader hypothesis. A policy might destroy valuable partial progress many times, and only obtain all goals on its final clean transition. Then post-first-full-coverage cost can be zero despite an enormous preventable acquisition bill. The carrier/service diagnostic addresses that complementary possibility.

A promising dirty state may also be rare, expensive to reach, have incomplete successors, or incur large computation to check. Its local quality and its priority under a finite search budget are not the same fact.

## 6. Reordering preserves authority, not performance or trajectory

Suppose a work change modifies only the order/selection of computations. Native problem semantics, completed-row laws, original prices, every published lower's checking conditions, and every returned policy's full native acceptance remain unchanged. At every finite interruption, any retained fully checked proper policy remains a feasible witness and therefore an upper in its documented numerical sense.

This says nothing about whether it is cheaper, whether the first policy arrives sooner, or whether all proof obligations are eventually served. Existing fairness/coverage owners must not be bypassed. A scheduler-neutral observation must not trigger work or change generation state; measurement overhead can still change a timed run.

The first S ablation uses `old_advances OR goal_count_increases`. It cannot erase an old progress event, but it can change the selected initial policy and therefore properness/work demand. Full evaluation remains mandatory. No universal improvement theorem is claimed.

## 7. Complete cleanup and recovery laws

For a fixed local proper programme with complete expected paid reward `g` and next-boundary law `H`, taking it once and returning to a compatible old policy gives

\[
Q_{once}=g+H V_\pi.
\]

“Cheap cleanup” is established only by its actual complete law and tails. If the new router invokes the repair on every revisit, this is a different controller and needs the appropriate coupled equations or full original-root evaluation.

A simple complementary-choice example: direct completion costs 10; prepare for 1 and use the old tail of 100 costs 101; prepare for 1 and use a newly constructed completion costing 1 costs 2. Immediate improvement of every intermediate edit is not a necessary property of a useful full policy.

Every loss, illegal-action outcome, required reset and mandatory cleanup must remain in the emitted controller's semantics. Local proper fragments can form an improper global cycle. The existing native evaluator, not a role feature or debt decline, decides acceptance.

## 8. Future goal-language change is independent

Under the same actions/prices, optimal cost to a larger allowed terminal set `A` is no greater than cost to `T`. A solution to the relaxed goal need not solve the strict goal. A relaxed-goal value can be a conceptual lower when its exact scope is justified, but this programme builds no new lower producer and changes no public default.

A later explicit occupancy/open-slot API must carry its terminal identity through native goals, compiler/evaluator, hashes, caches, product exports and benchmarks. Historical clean-goal results cannot silently be relabelled as results of that new problem.
