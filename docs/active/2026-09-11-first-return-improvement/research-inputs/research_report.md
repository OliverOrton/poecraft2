# Incumbent-backed first-exit policy improvement

## Recommendation

Select **Certified Return Bridges and First-Return Policy Improvement v1**. Keep the recovered first-policy path unchanged. Work after a real, independently evaluated incumbent exists: complete a small, explicit native route from an alternative's uncovered outcomes back to that incumbent's certified entry domain, then evaluate a narrowly changed controller. Where worthwhile, summarize the complete excursion to the original entry or goal and price repetition rather than repeatedly reconstructing a speculative global policy.

This is a hypothesis about the missing producer of useful continuation evidence, not a claim of an already demonstrated Ring/Amulet speedup. The immediate native witness is the recorded Ring root Exalt with one uncovered successor, subject to checking the actual selected snapshot. The larger goal is materially cheaper policies on both existing non-Conquest primaries, including one useful reforge-led attempt if its scope and bridge are supportable. A successful small bridge alone does not complete that goal.

The methods are established first-exit and policy-improvement reasoning. No algorithmic novelty is claimed. The proposed change is their application to the existing native candidate, compiler, evaluator, and continuation owners.

## Evidence boundary and current state

The reviewed remote commit is `84f02ee3b603fe3879ac2c4a885d1b81ba67772f`, rechecked on September 11, 2026. The new research was read-only. Repository records are retained measurements, not native runs repeated for this report. The independent Python calculations are synthetic exact rational examples, not crafting experiments or a machine-checked proof of the implementation. There were no source edits, native builds, solver runs, simulations, queue operations, or public actions. [R01]

M0 tooling is done. First-policy recovery is done. Neither is the next implementation task. The current ordinary upper pass also already performs improvement; its mere existence is not a missing feature. The key remaining issue is complete, economically useful continuations after an alternative leaves the incumbent domain. [R02–R04]

| Current evidence | What it establishes | What it does not establish |
|---|---|---|
| Ring-two now returns 582,192.807187538; Amulet-three returns 40,215,428.995558396 | Real policy availability on formerly failing requests | Practical policy quality or exact closure |
| Ring memory-only 4 GiB arm returns 204,763.14825000268 | Extra native capacity enables useful search | A code speedup or a need for the entire 4 GiB |
| Amulet 100M work arm returns the same 40,215,428.995558396 | Additional work alone failed to improve this controller | Every possible work allocation would fail |
| T1 queue deduplication saves about 62.9 MB on Ring but regresses Conquest-five badly | The tested mutation is not a safe retained change | All deduplication is mathematically wrong |
| T2 preserves compatible old decisions and prefers reducing-affix rows but gains no cost | A removal preference alone does not complete the required new controller | An explicit complete stopping/return construction cannot work |
| 13 of 14 current core/exposed cases have evaluated graphs | Broader delivery coverage | All expectations pass: CB09 and CB05 remain explicit failures |

The Ring 4 GiB run's next barrier is 50M reforge work. The one-GiB refusal is recorded as 456,471,865 already-owned bytes plus a proposed 790,419,257-byte scratch allocation. The actual larger run peaks at 1,118,120,640 solver-owned bytes. These are different quantities. Repeating a memory-only sweep is not the selected next experiment. [R03–R05]

Independent cost attribution identifies 98.90% of the original Ring cost in Harvest Augment Defences and 80.90% of Amulet cost in Annul, mostly in zero-goal regions. Those are costs of the existing controller, not lower bounds or guaranteed savings from replacing the action. Source node `expected_cost` annotations and masked policy-pass Q values are not independent arbitrary-entry certificates. [R03–R04]

## 1. The bottleneck has changed

The previous goal-free-view diagnosis was correct at its captured epoch. Later native work found full-goal-but-dirty states, completed cleanup, fixed a real no-incumbent scheduler yield, and retained the first properly evaluated graph before speculative improvement could displace it. Those repairs changed the starting point for this programme. [R02]

Current competing action rows can be legal, scheduled and kernel-complete, yet still have unknown continuation Q. The recorded Ring root Chaos exposes 2,478 uncovered routes; Amulet root Chaos exposes 3,361. In a different recorded Ring snapshot, root Exalt has one uncovered successor with probability approximately 0.006103143118706134. Its actual semantic identity and current incumbent must be recovered together: do not combine that old mass with a later controller's cost vector. [R04]

The current upper-domain masking is correct. It prevents treating unrelated lower estimates as executable tails. Removing that mask would hide the problem by constructing a false upper. The useful question is instead whether a small, explicitly costed legal controller can get the missing item into a domain whose completion policy is actually known. [R06–R07]

The plan does not infer that the cheapest-looking reforge is best. A current estimate may have omitted its most expensive tail. Conversely, failure to construct a tail does not prove the action uncompetitive.

## 2. Existing machinery to reuse

The native evaluator already represents arbitrary-entry requests, exact item/entry identities, globally routable policy entries, fixed decision-to-decision selected kernels, properness status, costs, and memory. `StrategyPolicyEntryResult::globally_routable()` requires more than a finite value: the global router must select the same compiled decision. `StrategyEvalOptions::continuation_entries` can seed actual new entry requests through the ordinary compiled start route; `policy_decision_entries` exposes genuine reached decisions. [R07]

Use those contracts. A new table of cached doubles is not a substitute. A source state ID, compiled node, exact item, and controller phase are not interchangeable. A router condition that appears to match is not an independently verified continuation; the actual entry must have complete execution support and properness.

The fixed-policy mathematics already supplies transient-system evaluation and stopped-program cost/exit equations. The current policy chapter also separates one-time deviation, persistent replacement, and performance difference under the new controller's occupancy. The work here specializes that reasoning to a small, constructible return boundary rather than inventing a second Bellman solver. [R08]

The supplied reuse audit correctly distinguishes live candidate continuation from a disk checkpoint. Neither job retry nor completed-coarse replay restores this live trial's evaluator/product state. Use an existing in-process fixture or narrow recorded input if needed, not a general checkpoint project. [A02]

## 3. A small candidate bridge suggested by current native semantics

Consider an actual ordinary Rare item with n explicit affixes. Assume the exact item/control context is admitted, no affix is fractured, neither side is locked, no unresolved offer/checkpoint is active, and the persistent base/influence/implicit/item context is compatible with an independently verified empty-Rare entry of the incumbent.

The inspected native `do_annul` selects one eligible affix uniformly and calls `pc_item_remove_at`. That removal changes the slot/count, not rarity. Under the stated unlocked/nonfractured premises, each application removes exactly one affix. Thus repeated legal Annul reaches the identical empty-Rare anchor in at most n steps, or can stop earlier at a true goal or a separately certified compatible entry. This follows from source behavior, not a new game-mechanic ruling. The exact calculator/registry/application correspondence still needs its native fixture. [R09–R10]

This provides a constructive fallback for a particular class of otherwise-uncovered real items:

> Stop if an approved incumbent entry or true goal is reached; otherwise pay for one native Annul and continue on every outcome.

The bridge deliberately allows losing desired modifiers. Those losses are included in the transition law and recovery cost. It does not assert that deleting all modifiers is the best craft, and it does not voluntarily buy another base. Empty Rare and Normal are different states; do not silently use Scour to claim the same return.

For n distinct initial affixes, every deletion-only state is a subset of those affixes. At most 2^n subsets are possible, versus enumerating removal orders. For n ≤ 6 this is at most 64 per concrete entry, before early stopping. This is a local combinatorial bound, not a bound on the whole reforge distribution or on the evaluator's incumbent state space. Across many input items, the union may remain large. Retaining a pooled representative without proving its removal/router equivalence is forbidden.

A simple recurrence for a fixed stop-first deletion controller is

\[
B(S)=
\begin{cases}
0,&S\text{ is an actual goal},\\
J_\pi(S),&S\text{ is an approved stop entry},\\
c_{\rm annul}+\sum_{S'}P_{\rm annul}(S'\mid S)B(S'),&\text{otherwise}.
\end{cases}
\]

Every recursive deletion reduces the rank n, so this local calculation is acyclic even if the eventual old policy has retries. A richer, still bounded deletion-only choice may compare stopping against another deletion only where both are completely supported. The first implementation need not optimize every such choice.

### Cases this lemma does not cover

A fracture or lock can prevent reaching the proposed anchor. Corruption, influence or implicit differences may survive deletion and defeat compatibility. A virtual zero-progress retry carrier is not a physical item. The product's reroll-only restriction must still be obeyed: a zero-goal reforge outcome cannot be diverted into arbitrary Annul salvage just because the raw action is legal. Some raw ordinary-mechanic questions remain open in the repository; the selected bridge deliberately avoids normal-item Annul and double-lock Scour. [R11]

For broad reforges, preserve the existing admitted zero-progress renewal controller and its complete first-exit law. Only its genuine permitted decision exits may enter a cleanup bridge. If no such finite, compatible route exists, that proposed reforge bridge is unsupported. A declined bridge is not a global action retirement.

## 4. First-exit composition gives a complete continuation, not a guessed value

Fix a finite native controller β that starts at x and stops at a true goal or an entry z in a verified continuation domain D. It must retain every positive-probability outcome and the actual observation/checkpoint decisions. Assume it stops almost surely with finite expected internal cost and that the selected old continuation is proper at every positive-mass exit.

Define

\[
r_\beta(x)=\mathbb E_x\!\left[\sum_{t<\tau}c_t\right],\qquad
K_\beta(x,z)=\Pr_x(X_\tau=z).
\]

The combined controller has cost

\[
J_{\beta;\pi}(x)=r_\beta(x)+\sum_{z\in D}K_\beta(x,z)J_\pi(z).
\]

Conditioning on the finite first exit gives the equation. All goal exits have zero remaining cost. A finite set of proper finite-cost continuations and an integrable bridge make the full controller proper and finite-cost. The construction needs a real route to the identified operation/item/control entry, not just the scalar J.

For a fixed finite transient interior R, the existing sparse equations are

\[
r=(I-P_{RR})^{-1}c_R,\qquad K=(I-P_{RR})^{-1}P_{RD}.
\]

These are already the stopped-program equations in the backbone. They do not require re-solving every action at every interior state. They require the selected controller to be fixed and its entire kernel to be represented. If a policy or observation choice changes, its r/K must be recomputed or re-certified. [R08]

This produces an executable upper candidate only after native composition and independent evaluation. It supplies no admissible lower, general state equivalence, or proof that unchosen actions cannot help. It may be a bad upper.

## 5. One-shot deviation and first-return improvement

A one-shot trial makes the controller boundary unambiguous:

\[
\text{Trial}\ \longrightarrow\ \text{Bridge}\ \longrightarrow\ \text{old }\pi.
\]

Once control enters old π, it does not jump back into the trial merely because the same physical item reappears. Controller phase is part of the state. This provides a complete, easily interpreted first check. But one-shot gain may be too small for a material policy improvement, so it is not the programme's finish line.

### A complete first-return calculation

Choose one exact semantic entry s. Start with the proposed action/controller there, complete its bridges, and follow the frozen old π elsewhere **until the first return to exactly s or true goal**. Let this excursion have finite expected cost r, return probability q, and total goal probability 1−q. These quantities include all internal costs, old-policy moves before the return, and control/observation distinctions. A return to an abstractly similar item is not a return to s.

Let J = Jπ(s). Taking the trial once and then using π costs

\[
Q_{\rm once}=r+qJ.
\]

If the same complete excursion is repeated at each return and q<1, the new controller is proper, with

\[
J_{\rm repeat}=r+qJ_{\rm repeat}=\frac{r}{1-q}.
\]

Therefore

\[
\boxed{J-J_{\rm repeat}=\frac{J-Q_{\rm once}}{1-q}}.
\]

This identity is the important extra problem-solving result. A small local improvement may be amplified by repeated returns. Conversely, old-policy occupancy is not the multiplier for the new strategy: the new complete excursion supplies q. With nonnegative r and a complete finite excursion, strict Qonce<J rules out q=1. Nevertheless, production must independently verify the emitted repeated controller; the equation is not permission to ignore a hidden trap or numerical ambiguity.

For a fixed maximum k attempts and then old π, the exact value is

\[
J_k=r\frac{1-q^k}{1-q}+q^kJ\quad(q<1).
\]

At q=1 it is kr+J and never improves with nonnegative r. Finite attempt memory is a safe fallback only when the scope permits it and the actual graph stores the counter/phase; continually replanning the counter at k is not the same controller. Do not introduce a large depth sweep. Prefer a native verified repeated excursion when its first-return law is complete.

### Example: why a one-step gate can miss the useful gain

Synthetic old policy cost J=100; complete excursion cost r=0.5; return probability q=0.99. The one-shot cost is 99.5, only 0.5% better. The repeated controller costs 50, a 50% gain. A requirement that the one-shot improvement itself be 20% would reject this useful candidate prematurely.

A second example has J=10, trial cost 1, half immediate goal, and half a cost-2 cleanup back to s. Here r=2 and q=1/2. One-shot costs 7 and repetition costs 4. Both are proper; they are different policies and must have different identities.

This does not say that any measured Ring/Amulet trial has favorable r or q. The next native experiment must obtain them. It also does not claim a new theorem: it is a direct renewal calculation specialized to a current controller boundary.

### More than one return entry

A complete finite entry set D has the corresponding relation u=r+Ku. If K is transient, u=(I−K)−1r. First use one entry, not an expanding universal basin. A multi-entry construction is allowed only as a bounded follow-through for a witnessed control-memory or routing need, not an excuse to rebuild the full state graph.

## 6. Counterexamples and refusal conditions

**A one-shot controller can terminate while its repeated form does not.** Trial s→t costs 1, cleanup t→s costs 1, and old π at s finishes for 10. One-shot costs 12. Repetition never reaches the goal. q=1 must refuse a claimed proper repeated controller.

**An unknown small tail is still unknown.** Probability 0.01 leading to a continuation costing 10,000 contributes 100. Treating it as free can reverse the comparison. Positive trap mass of any size defeats properness.

**A row average is not a reusable state value.** A bridge whose expected exit cost under one producing row is 11 need not cost at least or at most 11 from each member. Reuse requires the full member or producer-specific contract.

**A root certificate is not an arbitrary-entry certificate.** Returning to the same base/rarity or finding a matching node label is insufficient. The global router, exact item, controller memory, selected entry and price/scope must agree. The August 31 Scour experiment explicitly failed at arbitrary-entry routing; it never reached its proposed SCC calculation. This plan adds a paid route to a valid entry rather than overriding that refusal. [R12]

**A cheap local trial does not certify a whole permanent rewrite.** The first-return identity changes one declared decision boundary with a fully evaluated excursion. It does not license T2's wider set of speculative selections or replacing several actions without re-evaluating the resulting controller.

**A failed bridge does not retire the action.** If one cleanup controller costs 12 but a different tail would cost 3, failing a 10-cost comparison only rejects the first proposal. Original alternatives remain open.

**Numerical acceptance is separate.** When q is close to one, error is amplified by 1/(1−q). Do not declare q<1 from an arbitrary epsilon or publish an optimistic scalar quotient. Use the existing coefficient, mass, properness, residual and full native reconciliation contracts. Any interval/proposal used to select work remains distinct from a verified value.

## 7. Why this differs from the failed experiments

| Earlier work | Established failure or limit | Changed premise here |
|---|---|---|
| July high-impact upper waves | Exact rows reused, but per-carrier completion covered too little; no qualifying upper change | A finite native return bridge supplies complete tails without solving an unrestricted continuation at every exit |
| August coarse carrier planner | Coarse composition lost native mass and failed publication | Exact item/entry identities and full compiled evaluation; no scalar basin substitution |
| August 31 fixed-policy Scour expansion | Existing router refused new entry | A new, paid, scope-valid route reaches a genuinely certified entry before old policy reuse |
| September proper-selector investigation | Views lacked goals; full native toy caller already succeeded | First incumbent now exists; its executable domain is the boundary |
| Latest T1 deduplication | Memory saving but no primary cost gain and severe Conquest regression | Do not touch first-policy walk/order; new work is post-verification only |
| Latest T2 old-decision/removal preference | Thousands of unknown tails still required ordinary construction | Construct and cost a complete stopping/return controller, rather than merely rank cleanup rows |

The current `PrimitiveRenewalWitness` and existing fixed-program/first-exit machinery must be inspected before implementing a new internal object. Existing signatures may prove the selected repeat directly. Extend the narrow owner if the native contract fits; do not create another public upper issuer. [R06, R08, R13]

## 8. Literature: use and limits

**Bertsekas and Castañon, 1999, “Rollout Algorithms for Stochastic Scheduling Problems.”** The publisher abstract describes lookahead built on a base heuristic and policy improvement. It supports the general base-policy concept. Only metadata/abstract were reviewed; no detailed theorem from the inaccessible full article is assumed. The proposal's first-return proof is supplied above, not imported from this abstract. [L01]

**Hansson and Wahlberg, 2026 preprint, “Performance Bounds for Rollout Policies in Stochastic Shortest Path Problems.”** Its full HTML separates approximate guidance from properness and makes error amplification depend on the new policy's expected hitting time. We lack its uniform value-error premise, so its performance bound cannot certify adaptive scores in this project. It strengthens the case for complete fixed-controller evaluation. [L02]

**Watanabe, van der Vegt, Junges and Hasuo, CAV 2024, “Compositional Value Iteration with Pareto Caching.”** The full publisher text motivates reusable component analysis against boundary queries. Its reachability setting assumes compositional interfaces; it does not discover a cheap native crafting separator or establish cost-optimality here. Use a small actual return interface, not a Pareto cache or imported model-checking dependency. [L03]

**Bertsekas, 2017/2018, “Proper Policies in Infinite-State Stochastic Shortest Path Problems.”** The abstract highlights differences between proper-policy and unrestricted Bellman solutions. This programme stays in finite, independently checked controller domains and does not infer properness from a finite algebraic solution. Only abstract/metadata were reviewed. [L04]

The existing audits already supplied first-exit lower and query-integration arguments. Those remain separate tools for different consumers. This programme is upper-side completion: it should not republish the audits' threshold-lower machinery as a new implementation prerequisite. [A03]

## 9. Independent calculations performed for this report

`first_exit_reference.py` uses Python's standard library and exact `Fraction` arithmetic. The final run passes 30 named checks. It compares 256 transient blocks with separately assembled full one-shot and repeated controllers, and 48 deletion problems with explicit removal-order enumeration totaling 11,786 orders. The general arguments are written above; these finite checks test the formulations and counterexamples, not all native cases or numerical performance.

The deletion example has two desired labels A/B and one junk label, with certified stopping costs 100 at empty, 20 at A, 30 at B, and true goal at exactly A/B. Expected internal deletion cost is 2; exits are empty with probability 1/3, A and B with 1/6 each, and goal with 1/3. Total continuation cost is 131/3. The initial handwritten expected-value assertion was incorrect; the recurrence and independent removal-order oracle exposed it and the assertion was corrected. This was a reference-draft arithmetic error, not a native solver failure.

The script does not model native locks, fractures, raw item legality or compiler routing. Those are required native fixtures, not claims covered by its synthetic count. The same rational linear-system routine evaluates both differently assembled full controllers; this cross-check tests composition, not an independent numerical-backend implementation. The deletion-order oracle is a separate combinatorial computation.

## 10. Decision and boundaries

The strongest objection is that the native bridge is either unavailable under the existing controller scope or too expensive to improve the incumbent. A second objection is that verifying the composed artifact recreates the same large raw/evaluator graph despite the compact proposal. Both are explicit stopping conditions, not reasons to keep adding abstractions.

First establish one useful complete tail. Then test a complete first-return law and a current-run repeated candidate. Only then scale to one selected reforge family or high-spend Amulet entry. If current complete candidate values lack useful headroom, retain the negative result and choose another concrete obstruction; do not add lower refinements, larger caps or learned estimates merely to make this proposal appear successful.

Retain existing optimistic lowers. They remain the safe pruning/closure foundation. Adaptive estimates may order the candidate queue, but this first implementation should use native probability, completed fallback cost and known missing-work information before introducing a new estimator. A better executable upper can subsequently unlock existing certified pruning without changing L.

There is no source-based case for a subsystem purge in this programme. M0/build/lint fixes are done; no GUI/service/verifier deletion is selected. The prior retirement audit also warns against treating unknown use or failed root improvement as dead-code evidence. [A01]

The companion plan preserves the existing material targets and resource profiles, protects the recovered first-policy path, and requires actual returned native and affected WASM results. It supplies one fresh-session prompt; the next session need not recover old chat context.

## Sources

The full paths, pinned URLs, coverage notes and literature metadata are in `source_register.json`. Repository references below use the reviewed commit unless their own historical text explicitly says otherwise. No temporary chat citation tokens are required to follow this report.

[R01]: https://github.com/OliverOrton/poecraft2/commit/84f02ee3b603fe3879ac2c4a885d1b81ba67772f
[R02]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/active/2026-09-10-goal-reaching-row-delivery/README.md
[R03]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/active/2026-09-11-post-incumbent-cost/README.md
[R04]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/active/2026-09-11-post-incumbent-cost/m1-cost-and-continuation.json
[R05]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/active/2026-09-11-post-incumbent-cost/implementation-profile.json
[R06]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/solver/upper-authority.md
[R07]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/engine/src/solver_eval_types.hpp#L60-L400
[R08]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/solver/mathematics/policies.md
[R09]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/engine/src/actions_basic.cpp#L632-L675
[R10]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/engine/src/item_state.cpp#L87-L111
[R11]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/mechanics/ordinary-currency.md
[R12]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/archive/2026-08-31-clean-five-policy-potential-scc-cegar-v1/README.md
[R13]: https://github.com/OliverOrton/poecraft2/blob/84f02ee3b603fe3879ac2c4a885d1b81ba67772f/docs/archive/2026-07-29-high-impact-executable-uppers/report.md
[L01]: https://link.springer.com/article/10.1023/A:1009634810396
[L02]: https://arxiv.org/html/2605.22965v1
[L03]: https://link.springer.com/chapter/10.1007/978-3-031-65633-0_21
[L04]: https://arxiv.org/abs/1711.10129

A01: supplied **Audit 3 — Retirement, duplication, and carrying cost**, September 9, 2026, pinned be553608; particularly the scope and preserved-owner conclusions. Its small implemented fixes are superseded by current M0/current source.

A02: supplied **Audit 2 — Computation reuse, resumption, and research survival**, September 9, 2026, pinned be553608; state-survival table and distinction between physical kernels, controller decisions and proof use-sites.

A03: supplied **Whole-solver capability and falsifiable abstractions**, September 9, 2026, pinned be553608; stopped-region and query-specific arguments, particularly complete probability and entry-scoped authority. Its old result numbers are not current baselines.
