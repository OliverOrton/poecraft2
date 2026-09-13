# Review and mathematical handoff

## Recommendation

Use **selective action coverage and incremental continuation improvement** inside the existing native policy-search owners. Preserve the successful static dirty mode. Do not repeat the three-bucket learning campaign, first-policy investigation, coarsening diagnostic, checker-headroom implementation, or no-idle-polling milestone.

The latest implementation finds substantially cheaper policies. Its remaining problem is not simply an inaccurate scalar estimate: valuable choices are represented at an uneven granularity. It selects one globally cheapest goal-forcing Essence, yet can build protection alternatives at every dirty state in a large closure before evaluating any changed controller. The next experiment should put additional useful actions into small, complete candidate changes, guided by expected completion cost and actual recurrence.

This recommendation is source-grounded; its performance benefit remains to be measured. It does not establish that an untested protection or alternate Essence is cheaper in the native cases.

## 1. Reviewed state and actual outcome

Pinned remote main: `b1d7436c37e2cf2dec5149713b685fe6ecffaad6`, direct successor to `23e03ba9d3fef2f67f2bf1597f69c2c36422e5a7`. The review read the commit, current HANDOFF/AGENTS, the new living record, native action/candidate records and relevant implementation. No repository mutation, native build, solve, test or Simulator run was performed. Synthetic arithmetic is separate below. [R1], [R2], [R3], [R4]

| Request | Reviewed predecessor | Current static / adaptive U | Current L | Expected actions |
|---|---:|---:|---:|---:|
| CB08 Onyx Amulet, three goals | 1,973,847.6160810417 | 13,159.724084639676 / same | 174.79327321907147 | 26,655.7344 |
| CB07 Amethyst Ring, four goals | 10,251,920.398984343 | 228,023.27997140243 / same | 201.85524142944053 | 592,244.4726 |
| CB04 Spine Bow, contrast | 223,349.0000393144 | unchanged in both arms | separately scoped | 1,404,492.0070 |

The Amulet and Ring-four reductions are 99.3333% and 97.7758% under the matched 600/840/870-second profile. These are independently evaluated controller costs, not new native exact closures. The current public native opt-in chooses static expansion. Browser defaults remain unchanged; preservation of browser defaults is not delivery of the large native improvements in the browser. [R2], [R3], [R4]

Paid redraw at positive progress first produced Amulet 72,112.40 and Ring-four 228,023.28. Correct protected-program construction and compilation then produced Amulet 13,159.72. Ring's further protected candidates remained incomplete. These causal stages matter more than attributing the final result to “AI guidance.” [R4]

The original-limit 1,000-trial checks recorded Amulet 971 successes/29 action-limit stops and Ring-four 149/851, with no other execution failures. Eventual success under a proper controller does not promise completion within 100,000 actions. Truncated trial spend is not the unlimited expected cost. Expected Chaos remains the optimization objective, while action count and capped completion remain important diagnostics. [R4]

## 2. The tooling amendment worked and is done

Current AGENTS makes no routine LLM polling, deterministic batching, typed identity preflight and compact output durable rules. The living record reports long awaited batches, zero survivors and no idle model status polls. The worker's internal cancellation/deadline checks remain appropriate. Reuse this operating path; no new M0 or waiting configuration project is selected. [R3], [R4]

The reporter's zero-exclusion result is not sufficient by itself: the record says its generic regression flags do not automatically flag a higher nonexact U or a new bounded-result contract error. New comparisons must explicitly inspect these fields. This can be a small assertion using existing reports, not another comparison framework. [R4]

Original-profile CB02 first returned its old graph without an expected open-obligation counter; a targeted repeat passed on both arms. CB03's saved 536,407.1454 graph was not reproduced by either current arm in the targeted repeat; both returned 794,067.4530. This is not established as a treatment regression, but the earlier high-water graph and initial failures remain evidence. CB05's named-stop/proof-contract issue and CB09's absent policy remain open. [R4]

## 3. What the adaptive experiment actually tested

`DirtyGuidance` has three acquisition-family buckets: Exalt, Chaos and Essence. It multiplies a private whole-controller estimate by `exp(log_residual[family])`. A bucket updates only after an independent evaluation of the same complete frozen root controller, using a bounded log residual and damping. This is sensible authority separation, but is not a learned statewise optimal completion value. [R5]

The proposal scheduler starts with preserved whole-controller candidates. For expanded candidates, its static prior is a previously completed private family cost times 0.5 for redraw expansion or 0.6 for protected expansion. Both static and adaptive modes share the same action set and an age path. [R6]

For two candidates in the same family with finite positive costs `x,y`, multiplying both by `k>0` preserves their order:

`x < y  iff  kx < ky`.

In particular, at the same decision point, a family scalar cannot make the protected 0.6 candidate outrank its 0.5 redraw counterpart. Different families can change order, and completed evidence may change later priors; the impossibility statement is only about a common multiplier at one fixed comparison.

Measured correction never changed the next selected obligation on the three tested families. Final policies matched. The small sampled timing differences failed the declared 10% adaptation threshold. Keep static as the default. Do not infer that all state/action estimates or online learning are useless; this was a narrow, late-feedback family calibration. Conversely, do not launch another predictor sweep without a native decision whose order can actually change. [R4], [R5], [R6]

## 4. The Ring bottleneck is now repeated acquisition

Current Ring-four policy spends about 87.52% on 582,176 expected Suffering4 Essences, 5.71% on 1,342 Annuls, and the remainder mainly on 8,726 Exalts. The preceding “96% Annul” diagnosis is stale. About 547,614 Essence uses occur with one goal present and 34,560 with two. [R7]

These counts do not prove every redraw destroys valuable progress. In a recorded one-goal entry the same Essence reintroduces its forced goal, and its current private continuation cost is lower than the available Exalt/Annul alternatives. A better protection, acquisition or salvage continuation must win a complete native comparison; goal count alone cannot decide it. [R7]

The most informative unfinished candidate is not a numerical loser. Essence expansion 2 inherited only **9,140,737 remaining logical work units**. It consumed 9,140,730, reached 8,679 private states and installed zero rows before refusal. Two later Chaos candidates inherited seven units. An Exalt-protection candidate then spent 173.85 seconds constructing 17,780 rows, including 8,519 protected alternatives and 7,969 refusals, without reaching its first improvement round. These are the report's actual per-candidate records, not a new timing experiment. [R7]

This distinguishes two mechanisms: early proposals consume the shared budget, and broad protection expansion admits a large amount of continuation work before a single changed controller can be priced. More aggregate RAM is not established as the remedy for either. More reforge budget may answer the first question; selective expansion targets the second.

## 5. Two structural opportunities need separate tests

### 5.1 One cheapest forced-goal action is not enough in general

The current root proposal loop retains Exalt, Chaos and **one** Essence minimizing price divided by the number of satisfied goal slots it forces. That is a proposal heuristic, not dominance evidence. [R6]

A synthetic two-goal renewal example shows the risk. An action costing 1 forces the common goal and obtains the rare remaining goal with probability 1/1,000; its proper repeat value is 1,000. An action costing 5 forces the rare goal and obtains the other with probability 1/2; its repeat value is 10. Both force one goal. Price-per-forced-goal prefers the wrong one by a factor of 100 in total cost.

This is not PoE mechanics or proof that a different admissible Essence exists for CB07. First enumerate the actual native, priced, satisfying-tier forced-goal masks. If all valid candidates force the same relevant identity with equivalent laws, this route has no useful diversity. If they differ, a small representative set should reflect *which* goals are forced and what remains, not merely how many. Equal masks do not prove equal exclusion/weight/cost laws; untested variants remain eligible rather than globally deleted.

The same principle applies to targeted and protected acquisition: an action family name or cheapest price does not identify its economically best member.

### 5.2 Broad action registration is not selective exploration

Inside the current private builder, adding an alternative appends all its positive successors to the walk. For expansion 2, each eligible dirty state asks the option-kernel owner for each protected alternative, extending the closure before fixed-policy evaluation and improvement begin. This is explicit in the source. [R6]

The ordinary solver already has delayed actions, candidate-local continuations and sparse policy iteration. The proposed work is to give the **private dirty owner** the same useful granularity: a closed seed controller, a small set of pending source/operator changes, complete native rows for selected changes, and sparse improvement before automatically expanding the next layer of alternatives.

This is not permission to truncate a selected stochastic row. Every selected action's outcomes still need an executable continuation. Unselected alternatives may remain unmaterialized and unresolved. If a proposed action needs a new continuation, complete a small coherent dependency patch, not just the first edge; two individually unattractive edits can jointly be useful.

## 6. Mathematics for selecting and checking a local improvement

The existing policy chapter already contains the fixed-policy and performance-difference identities. They are not new discoveries here. The following corollary and counterexamples sharpen their use for this work. [R8]

### 6.1 Local advantage and actual root improvement

On a common finite semantic decision graph (physical item plus required controller/observation state), let proper policy pi have substochastic matrix P and finite cost vector c:

`J = c + P J`.

For a proposed proper policy mu define `A_mu^pi = c_mu + P_mu J - J`. Subtracting equations yields

`J_mu - J = (I-P_mu)^(-1) A_mu^pi`.

A nonpositive advantage at every changed reachable state is sufficient for non-increase when the candidate is proper; it is not necessary. The exact root difference uses the **new** policy's expected visits. Current occupancy times estimated advantage is only a ranking proxy. Unknown successor values cannot be filled with the root U or a lower and still be called executable evidence.

The existing `ProductionPolicyOracle::shared_bellman_prefers_candidate` already consumes complete kernels and compatible policy/continuation values and refuses absent tails. Do not add an identical second Bellman comparator. Adapt existing construction/ownership to reach an actual private candidate consumer. [R9]

### 6.2 A one-row change has a useful renewal correction

Suppose only row s changes, on the same complete finite domain. Write delta_p for the row difference and delta_c for the price difference. Let

`z = (I-P)^(-1) e_s`,

so z(i) is the old policy's expected visits to s starting from i. With

`b = delta_c + delta_p J`,

subtracting the two policy equations gives `delta_J = z (b + delta_p delta_J)`. Hence, when the changed chain is proper and the denominator is valid,

`delta_J = z b / (1 - delta_p z)`.

This is the elementary rank-one linear-system identity applied to a local controller change. It is a **design calculation**, not a selected new numerical backend or permission to construct a dense inverse. Use the existing sparse solver for z only if an actual small frozen witness needs it; ordinary full native evaluation remains publication authority. A near-zero denominator requires the existing numerical safeguards, not relaxed tolerances.

Counterexample: the old policy pays 1 and returns with probability 0.99, so J=100 and old visits=100. New choice A finishes immediately for 80: true gain 20, but old-visits times local advantage predicts gain 2,000. New choice B retains the 0.99 retry with cost 0.1: true gain 90. The naive occupancy score ranks A ahead of B; the complete return correction ranks B correctly. This is why prioritizing high spend alone does not solve the search problem.

For several changed rows, the corresponding small response system is `delta_J = Z (I - DeltaP Z)^(-1) b`. Its usefulness depends on the number of changed entries and compatible boundary coverage; it is not a mandate to build a Schur-complement framework now.

### 6.3 An interpretable test for a candidate retention opportunity

For a declared renewal controller, let a be the paid acquisition cost, p0 direct goal probability, and wi the disjoint probabilities of optional useful exits. At exit i, a complete local continuation costs ki, finishes with probability pi, and otherwise returns to the *same actual renewal decision*. Then

`J = (a + sum_i wi ki) / (p0 + sum_i wi pi)`.

Relative to returning immediately to an old controller costing J0, a local continuation is attractive when `ki < pi J0`. This is an opportunity threshold, not proof that the currently allowed native region has that response. Any other boundary must remain explicit.

Synthetic example: a=1, p0=0.01; an event of mass 0.1 permits local cost 2 and conditional goal escape 0.5, otherwise paid/context-correct return already included. Old J=100; new J=20. This suggests measuring the cost/probability response of preserving a promising partial item rather than pricing a reset-free fantasy or ranking purely by number of goals.

### 6.4 Small patches must be allowed to contain cooperating changes

An old root can finish for 10. Another action costs 1 and goes to t, where the old continuation costs 100. Merely testing that root action gives 101. A new legal continuation at t costing 1 makes the complete alternative cost 2. Improving t alone does not change the old root's result. A greedy rule requiring each intermediate edit to strictly improve the current root misses this pair.

The work unit should therefore be a **small coherent candidate plus its missing selected dependencies**, not necessarily one greedy row at a time. This also prevents the new plan from repeating the already falsified “service one missing state and stop” approach.

### 6.5 What is not certified by these calculations

A private restricted optimum is not a full-action lower. Locally terminating options can compose into a non-goal cycle. An entry-specific average is not a uniform value for every represented member. A cheaper private estimate is not a verified native graph. Observed choices must remain at their original decision time. A state with zero satisfied goals may still contain required control/setup information. All such obligations remain with the existing native owners.

## 7. Literature assessment and its actual contribution

**Schmalz and Trevizan, Artificial Intelligence 2026, Efficient Constraint Generation for Stochastic Shortest Path Problems.** The author-hosted accepted manuscript's sections 2–5 distinguish permanent action elimination from selective addition, and describe reconsideration driven by value changes. This is the closest precedent for action-granular work. Its finite-SSP, cost, heuristic, coverage and termination conditions do not automatically hold for our procedural private views. We propose neither a CG-iLAO* replacement nor its optimality theorem. The transferable idea is selective action work with dependency-directed reconsideration, not the paper's reported speedups. Intermediate restricted values must remain separate from native proof bounds. [P1]

**Hansson and Wahlberg, 2026 preprint, Performance Bounds for Rollout Policies in SSPs.** Its performance-difference and hitting-time analysis explains why long recurrent controllers can magnify small prediction errors. The theorem needs properness and a uniform approximation bound not established for our estimator. It motivates preserving full evaluation and accounting for changed visits, not a numerical guarantee for three learned family buckets. [P2]

**Thayer, Dionne and Ruml, ICAPS 2011, Learning Inadmissible Heuristics During Search.** The publication establishes online correction as a legitimate search-guidance approach in its tested domains. It does not establish that a scalar family calibration will alter this solver's choices or transfer deterministic path-search guarantees to native stochastic control. We should keep estimates available as revisitable priorities without treating this failed pilot as a refutation of the whole idea. [P3]

**Smith and Simmons, AAAI 2006, Focused RTDP.** Its published abstract emphasizes targeting uncertainty relevant to good policies rather than spreading work uniformly. That supports measuring the root relevance of the chosen work. The reward/bound conventions and algorithmic details differ; no FRTDP speedup is claimed for this programme. [P4]

No novel algorithm or theorem is claimed. The contribution of this review is the source-grounded placement of selective action work, a specific budget failure to discriminate, and mathematically valid ways to reason about a changed recurrent controller.

## 8. Falsification and limits

The strongest objection is that promising protected or alternative acquisition actions may genuinely lose, or their newly required continuation closure may remain huge. A focused native comparison should establish that before building general incremental machinery. If the first small policy patch has no useful response, test a materially distinct native acquisition choice rather than enlarge the same patch indefinitely.

A second objection is that incremental mutation can compromise candidate identity or change a good first-policy schedule. Preserve the original fast path, compatible immutable verified artifacts, and child namespace. A selected work order is not permission to attach old costs to changed rows. Any larger-representation reconciliation must be explicit and measured.

The review did not fetch local Codex transcripts, ignored raw result files, native binaries or a working checkout. Repository records are retained measurements, not fresh execution. The primary PDF was read through its parsed text; screenshot attempts failed, and no chart/table inference depends on unseen images. The supporting Python script performs exact arithmetic in small synthetic models only: 27 named checks plus 256 generated transient three-state models. It does not certify native correspondence or predict runtime.

## Sources

[R1]: https://github.com/OliverOrton/poecraft2/commit/b1d7436c37e2cf2dec5149713b685fe6ecffaad6
[R2]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/HANDOFF.md
[R3]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/AGENTS.md
[R4]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/docs/active/2026-09-12-adaptive-dirty-guidance/README.md
[R5]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/engine/src/solver_dirty_guidance.hpp
[R6]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/engine/src/solver_solve_return_bridge.cpp#L880-L1520
[R7]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/docs/active/2026-09-12-adaptive-dirty-guidance/final-ring-four.json
[R8]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/docs/solver/mathematics/policies.md
[R9]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/engine/src/solver_policy_oracle_improve.inc#L1-L185
[R10]: https://github.com/OliverOrton/poecraft2/blob/b1d7436c37e2cf2dec5149713b685fe6ecffaad6/docs/solver/resources-resume-replay.md#candidate-checker-and-native-headroom
[P1]: https://schmlz.github.io/downloads/cgilao/cgilao_aij26_paper.pdf
[P2]: https://arxiv.org/html/2605.22965v1
[P3]: https://ojs.aaai.org/index.php/ICAPS/article/view/13474
[P4]: https://aiinternational.org/Library/AAAI/2006/aaai06-192.php

Full titles and access scope are in section 7 and `source_register.json`. These are references for the reasoning, not a mandatory recursive reading list.
