# Cross-Base Strategy Comparison: What Conquest Can Tell Us About Bow and Ring

**Repository:** OliverOrton/poecraft2  
**Reviewed main:** `3268d1a9f73e8baa156d6daa366bd4f4cc86c29f`  
**Research date:** September 13, 2026  
**Status:** Read-only research and proposed experiments. No repository changes or new native execution.

Source labels refer to `SOURCES.md`. Exact transcribed numbers and derived ratios are in `evidence_snapshot.json`. The implementation sequence is in `02_implementation_plan.md`.

## 1. Recommendation

Oliver’s proposed comparison is a strong next direction, but the comparison should be **how strategies acquire, preserve, lose and reacquire progress**, not simply how many modifiers, graph nodes or currency applications they have.

The most useful question is:

> Why can the good Conquest strategies convert acquired progress into completion relatively efficiently, while Bow and Ring spend so much execution repeatedly acquiring progress—and which of those differences is a native crafting difficulty versus a solver capability gap?

The evidence already establishes a large execution-length contrast. It also identifies a narrow Bow candidate family, fragmented entry selection, substantial repeated acquisition in Ring, a demonstrated native observation failure on Amulet, and major historical gains from completing previously unavailable strategies. It does **not** yet establish that another scheduler, a broader reforge action, an additional blocker or a particular fracture choice will improve Bow or Ring. [R02–R07, R11, R15, R17]

My strongest new recommendation is to decompose **acquisition difficulty from retry amplification caused by later progress loss** before choosing the next implementation. The action on which most money is spent need not be the decision causing that spending. A low-cost cleanup or retention decision can cause hundreds of expensive reacquisition cycles. The exact illustrative example in Section 5 demonstrates this distinction without assuming anything about Path of Exile mechanics.

Start with existing evaluated policies, their native entry/occupancy evidence, and a small number of complete first-exit comparisons. Reuse the existing native continuation, sparse-policy, compiler, evaluator and corpus-runner owners. Do not begin with a new generic option planner, a permanent recipe library, another adaptive-weight sweep, or whole-solver parallelization. [R13–R16, R19]

## 2. What was actually inspected

The review follows the current handoff, current execution-aware programme, retained native and browser receipts, core case definitions, living research record, upper-authority and policy-mathematics contracts, tooling owner, and the relevant current portions of `solver_solve_return_bridge.cpp` and `solver_dirty_guidance.hpp`. It also follows the recent native bench-conflict failure and checked-potential disposition. This is a targeted architecture/evidence review, not an end-to-end audit of every compiler/evaluator source file. [R01–R21]

The previous native-research chat’s older recommendations cannot be treated as current unfinished tasks. Main already records the failure of a generic proper-policy-selector explanation, the successful terminal/continuation and first-retention repairs, dirty continuation improvements, narrow action-dependent layout reductions, static/adaptive comparisons, selective options, observed bench-conflict recovery and execution-aware proposals. Their outcomes constrain the next programme. [R11–R17]

No local owner checkout or unpushed work was available. No native solver, Simulator, test suite, build or browser qualification was run. The numerical tables below are **committed receipts**, not fresh measurements on a newly built `3268d1a9` executable. Large files were read in relevant ranges. The complete current native CB02 policy action/phase profile still needs recovery from its actual graph; the browser count below must not be assigned to the cheaper native artifact.

## 3. The useful comparisons already available

### 3.1 Keep the Conquest references separate

| Retained policy reference | Evaluated expected cost, chaos | Expected primitive actions | What it establishes |
|---|---:|---:|---|
| Conquest four, original native CB02 | 3,746.131941 | Not in the inspected native receipt | A comparatively cheap verified policy; larger native construction/graph metrics are available |
| Conquest four, browser E10 | 5,218.040950 | 14,152.339715 | A separate verified four-goal policy with execution totals |
| Conquest five, browser E9 | 85,558.706186 | 8,407.202315 | Higher goal count and cost need not imply more executions |
| Bow four, execution-aware B reference | 223,349.000039 | 1,404,492.006968 | Current retained cheap Bow controller |
| Ring four, execution-aware T result | 227,377.065450 | 583,110.349960 | Latest cheaper retained Ring controller |
| Amulet three, latest retained control | 12,541.579649 | 25,518.491313 | Useful changed-representation/control comparison, not a four-goal peer |

Sources: Conquest native [R04]; both Conquest browser rows [R07]; Bow [R05]; Ring [R06]; Amulet [R02, R03, R17]. Values are rounded here; the JSON retains source precision.

The native CB02 reference has the original 240-second finish profile. Browser E10 uses the historical 60-second finish / 90-second watchdog profile. The latest hard-base development programme uses the larger explicitly identified envelope. These are **policy-behaviour reference comparisons**, not matched timing measurements or evidence that one code change caused the cross-base differences. [R03, R04, R07–R10]

Relative to the inspected Conquest-four browser policy, Bow executes about **99.24 times** as many primitives, and current Ring about **41.20 times** as many. Those ratios describe these specific evaluated controllers; they do not estimate how much slower the planner must be, the intrinsic optimal cost ratio, or how much improvement is available.

Conquest-five is a useful warning against a one-dimensional difficulty measure: its retained browser policy costs much more than Conquest-four while using fewer primitives. Do not optimize an action-count proxy in place of the project’s minimum-expected-chaos objective. [R07, R14]

### 3.2 Conquest’s good native policy is not the smallest compiled graph

The inspected CB02 native receipt reports 811 compiled nodes and 2,200 edges. Bow’s current baseline has 105 nodes / 317 edges, and Ring’s new graph 161 / 528. Therefore the cheapest of these references is not the one with the fewest compiled nodes. [R04–R06]

CB02’s historical native construction also reports:

| Quantity | Recorded value |
|---|---:|
| Coarse expanded states | 7,213 |
| Coarse frontier | 0 |
| Coarse policy-reachable states | 815 |
| Native peak owned bytes | 911,402,146 |
| State-action rows | 135,375 |
| Transition entries | 213,825 |
| Reforge work | 7,169,352 |
| Remaining refinement obligations | 147,503 |
| Certified lower | 198.8334996747695 |
| Evaluated upper | 3,746.1319409485764 |

These fields do not all count the same objects. The coarse frontier being empty does not discharge the remaining strict/native refinement obligations. The final result remains bounded, not globally exact. The policy is a useful **successful construction reference**, not a known optimal strategy against which to declare all other costs excessive. [R04, R13]

Similarly, Ring’s region rollup reports 120,204 reachable states in its one-goal Essence region and 53,322 in its two-goal Essence region, despite only 161 compiled nodes. Preserve the evaluator’s exact state/pair definitions before comparing these numbers with coarse states or unique physical items. The basic warning already holds: a small router can stand for a large native execution domain. [R06]

### 3.3 Bow’s actual execution concentration

The latest Bow baseline’s total cost is 223,349.000039 chaos. Alteration contributes 138,973.638844 overall. More specifically, the one-fracture Magic Alteration region contributes 138,607.176346, and its corresponding Augment entry contributes 30,397.270612. Together these two fractured-Magic acquisition categories account for approximately **75.67% of total cost and 97.29% of primitive execution**. [R05, R20; arithmetic in evidence JSON]

The source-backed interpretation is that execution is heavily concentrated in an earned-fracture Magic acquisition phase. It is **not** yet evidence that the acquisition operator alone is the cause. The missing causal measurements are how often that phase is entered, how hard a single successful departure is, where those departures go, and which later branches return the controller to it.

Current source selects only actual compiler-bound, globally routable primitive entries. The added Magic family requires one satisfying fracture, no mutable goal progress, no fractured junk, no active offer/checkpoint and no incompatible persistent context. It does not reopen arbitrary mandatory-program interiors. [R15]

The selected Bow entry has one fractured goal and no mutable progress. The programme records 16 eligible Alteration entries with combined immediate spend around 138,607, but one Augment entry around 30,397. Ranking by **largest individual entry** chooses that Augment entry. The code also records the aggregate amounts, so the fragmentation is directly observable rather than a guess. [R03, R15]

This motivates a diagnostic comparison of entry-group scheduling, not a conclusion that the current selection is wrong. Several entries may already lead to a common bottleneck; patching one can affect many visits. Conversely, exact items with similar progress may require different legal continuations. First measure the actual first-exit/re-entry law and routing relationship between these entries.

### 3.4 What the latest Bow alternatives do—and do not—show

The latest programme found complete, independently evaluated Bow alternatives near:

| Controller | Expected cost | Expected primitive actions |
|---|---:|---:|
| Retained cheapest Bow | 223,349.000039 | 1,404,492.006968 |
| Alternative, cost-selected within the new family | 405,589.692463 | 331,010.580055 |
| Alternative, count-guided within that family | 405,922.951785 | 327,477.870157 |

The new family cuts executions by roughly 76% but costs roughly 82% more. Keeping these as diagnostic tradeoffs and retaining the cheaper original policy is correct under the product objective. A reduced execution count is not evidence of reduced expected currency cost. [R03]

The new family pays the legal Regal/Chaos/acquisition/recovery work and retains the earned fracture. There was no admitted Essence at the selected entry that guaranteed a **satisfying mutable goal**. That limited result is not equivalent to “Essences are all illegal,” “all Essences were tested,” or “no unguaranteed acquisition can help.” [R03, R15]

The source also skips protected expansions for `nonempty_handoff` proposals. The Magic family therefore does not test a fresh general set of protection/blocker programmes inside that phase. This is a **coverage boundary**, not proof that protection is useful or supported there. Preserve the narrow entry guards while investigating any extension. [R15]

A second potential boundary is earlier strategy choice: a continuation that keeps the current earned fracture cannot determine whether paying for a different fracture/progress ordering from the original root is cheaper. That is a distinct, full-root experiment, not another parameter setting of the existing fixed-fracture patch.

### 3.5 Ring’s dominant repetition and the misleading cleanup shortcut

The current Ring controller spends approximately:

| Native action | Expected applications | Expected spend, chaos |
|---|---:|---:|
| Suffering4 Essence | 573,117.520168 | 196,464.685914 |
| Exalt | 8,636.122394 | 15,285.936637 |
| Annul | 1,346.579358 | 13,048.353975 |
| Suffix-lock bench operation | 6.076824 | 2,576.573365 |

These selected rows are not an exhaustive spend table; use the native report to reconcile all operations. Essence alone contributes **86.40% of cost and 98.29% of actions**. The policy is not accurately described as currently dominated by Annuls, and protection is not entirely absent: the paid suffix-lock operation has positive occupancy. [R06]

Most Essence applications occur in the report’s one-goal and two-goal regions. That locates the repeated acquisition, but it does not tell us whether acquisition is intrinsically rare, useful partial results are discarded too aggressively, late failures force costly restarts within the live item, or the candidate builder lacks a better complete continuation.

The new count-guided programme improves Ring cost from 227,989.251597 to 227,377.065450, about 0.2685%. It is a real checked improvement, but not a practical-cost breakthrough and not a successful two-family major-improvement result. The next programme should not simply relabel another small parameter-sweep gain as a solved direction. [R02, R03]

## 4. What main’s recent history already tells us

### 4.1 Coverage recovery was important; support-selection alone was not enough

The earlier proper-policy-selector investigation encountered complete-row views without true native goals or proper committed frontiers. A better selector could not manufacture a goal-reaching policy out of that supplied view. Later work corrected concrete discovery/delivery problems: automatic epoch service, real cleanup to clean terminals, completion of losing branches, first-candidate verification and retention before further improvement. [R11, R12]

Do not re-open the removed generic selector as the default explanation for today’s expensive feasible policies. Re-open it only with a fresh native counterexample showing that the complete available view now contains a useful proper controller that the current owner actually misses.

### 4.2 Large gains came from completing different strategies

The living record reports successive large upper improvements from native dirty acquisition/recovery and expanded static continuation coverage, followed by much smaller selective and execution-aware gains. Static and adaptive arms did not establish a causal adaptive-selection benefit in their matched comparison. [R11]

This makes **missing or under-served executable policy structure** a better first hypothesis than “the scoring weights need another adjustment.” It remains a hypothesis for the current Bow and Ring plateaus; historical success cannot certify the next missing strategy.

### 4.3 A real representation failure explains why naive transfer is dangerous

On Amulet, a private source class mixed native items that did and did not conflict with a proposed crafted Lightning modifier. A representative-only bench admission was valid for one member and invalid for another. The emitted controller then lost goal-reaching mass. The correction required an observation of the actual conflict group, a fresh dependency-derived layout and the physical source carried into its new namespace. [R17]

The matched 400M follow-through eventually lowered the verified Amulet upper from 13,159.724085 to 12,541.579649. The initial 200M observed-layout attempt capped before installing rows. Therefore “the idea was rejected at 200M” would be false; the earlier result was a construction limit, and the eventual gain depended on actual completed work under a separately matched larger budget. [R17]

For the new comparison, group items into report categories freely, but do not turn those categories into executable equivalence classes without preserving native legality, conflicts, probabilities, observations and control state. This matters especially when attempting to transplant a Conquest-like blocker or preservation pattern to another base.

### 4.4 Some abstraction reductions already exist

The living research records a private action-observer reduction that changed a 21-junk-class view to six and a Chaos support from 4,160 to 85. Merely deleting actions while keeping the old layout did not accomplish the same reduction. Those are scoped historical observations, not measured current Conquest/Bow/Ring class counts. [R11]

The current code preserves the modifier universe and rebuilds dependencies, and it retains resistance conversions that can actually satisfy a goal. An action unused by the incumbent is not automatically irrelevant to a better policy or to a full-scope proof. Any new leave-one-action-out layout diagnostic belongs first to private upper discovery, with the full action/proof ledger unchanged. [R13, R15]

### 4.5 Proof weakness is a separate problem from expensive policy execution

The checked-potential pilot found cheap optimistic escapes in its auxiliary lower model. Better predictions could not overcome those model ceilings; limited native-support refinement still did not improve the published root lower. Those results are not arguments that native crafting is cheap or that a new upper strategy is impossible. They are evidence about a different model and a different authority. [R17]

Keep the comparative strategy programme focused. Preserve admissible lower bounds and all full-scope proof obligations, but do not attach a new lower-learning or proof-basis campaign before the policy-level discrepancy is understood.

## 5. The most important causal distinction: acquisition rarity versus lost-progress amplification

### 5.1 Exact synthetic example

Consider a two-phase controller, unrelated to any particular PoE mechanic:

- Phase A pays 10 to acquire an intermediate item and enters B.
- Phase B pays 1. It finishes with probability 0.01; otherwise it loses the useful progress and returns to A.

Its expected cost from A is

`J_A = 10 + 1 + 0.99 J_A = 1,100`.

Expected acquisition spend is 1,000, or 90.91% of the total. An analysis that only ranks action spend would strongly emphasize A.

Now change only B: it costs 2, but finishes with probability 0.5 and otherwise returns to A. The total becomes

`J'_A = 10 + 2 + 0.5 J'_A = 24`.

The cheap completion phase got more expensive per visit, yet total expected cost fell by 97.82%. Changing only acquisition price from 10 to 9 would instead reduce 1,100 to 1,000. The acquisition bill was primarily amplified by the completion/retention failure.

`checked_examples.py` verifies these equalities with exact rational arithmetic. It also checks that the changed controller’s occupancy—not the old occupancy—gives the correct performance difference. This is a mathematical illustration, not a native benchmark or evidence that Ring has these probabilities.

### 5.2 The native measurements that distinguish these explanations

For each high-occupancy phase, recover the complete native distribution over the next legitimate decision boundaries. Record the first entrance, repeated returns, expected internal cost/count, acquired goals, lost goals, retained fracture/locks and true clean-terminal absorption.

The key distinction is between:

**Hard acquisition:** even one attempt to move from the actual entry to useful additional progress is expensive.

**Hard retention/completion:** useful progress is reachable relatively cheaply, but later branches repeatedly destroy it and send the controller back.

**Missing continuation coverage:** the kernel can produce useful progress, but the builder cannot complete/route its positive-mass exits within the current policy family and budget.

**Representation burden:** a useful complete controller may exist within the selected family, but required native distinctions produce too many rows, states, evaluator pairs or rejected mixed-class actions.

These explanations can coexist. The study should quantify their contributions rather than select one label based on total cost alone.

### 5.3 Why a scalar retry probability is often insufficient

If every failed excursion returns to the identical native entry and control context, a paid renewal formula is valid. The existing mathematics already derives the relevant cost and recovery identities. [R14]

But Bow’s 16 Alteration entries are not automatically one identical retry state. Returns may change junk, occupied sides, group conflicts, partial progress or routing context. Then retain a vector of exact compatible boundary entries and the complete return kernel. For a fixed controller, write

`V = g + R V`,

where `g` includes paid work before the next boundary and any genuinely committed outside continuation, and `R` records returns to those exact entries. Properness and finite expected work are required. On the finite transient domain, `V = (I - R)^(-1) g` is an explanatory identity; implementation should reuse existing sparse fixed-policy owners rather than form a new dense inverse.

The same stopped process gives primitive counts with a separate immediate-count reward. A region chart can aggregate the results for humans without becoming an optimized abstract MDP. If the aggregation is not Markov, keep the underlying exact boundary solution and label the chart descriptive. [R14; E01]

## 6. What to compare across bases

### 6.1 A strategy fingerprint, not a single complexity score

Each reference should have a compact, consistently defined fingerprint covering:

| Dimension | Required comparison | Why it matters |
|---|---|---|
| Goal/native structure | Actual goal identities, side occupancy, tags, conflicts, satisfying pools and available guaranteed acquisitions | Identifies native structural differences without guessing from base names |
| Policy stages | Paid setup, actual retained anchor, acquisition, useful dirty progress, preservation, cleanup, true completion | Shows which kind of strategy the solver has actually built |
| Repetition and retention | Expected entries/returns, first-exit distributions, cost/count per excursion, goal losses | Separates rarity from reacquisition amplification |
| Representation | Coarse classes, action observers, exact/evaluator domains, mixed legality, complete outcome support | Explains row/checker burden and invalid representative-only proposals |
| Capability delivery | Legal/admitted, requested, row-complete, continuation-complete, compiled, verified, retained | Identifies the first actual missing owner, not just the last cap |
| Proof state | Certified lower, verified upper, remaining competitor/refinement work | Keeps feasible policy progress distinct from optimality |

The supplied Conquest goal names suggest concentrated defence-related structure, while Bow’s goal names span gem level and multiple damage-related families, and Ring spans several different modifier families. Treat that as a **question for native metadata**, not an already established tag/weight/side comparison. The reviewed core fixtures specify goals, but the compiled native pools were not inspected here. [R08–R10]

A claim such as “Conquest is easier because all goals share one tag” requires actual native metadata and state-local action laws. Do not infer it solely from English modifier names, and do not source it from live external game-mechanics pages.

### 6.2 The most informative comparisons are not all root solves

First compare actual continuations under the unchanged final goal. Useful entries include first useful progress, the earned-fracture Magic phase, dirty multi-goal states, and full-goal-but-dirty cleanup. Keep physical items and legitimate control boundaries explicit.

An entry diagnostic can show that a particular tail is cheap or expensive. It does not grant that entry for free in a root policy. Any root-improvement claim must include the original paid acquisition, fracture attempts, failure recovery and repeated re-entry of the composed controller. [R13–R15]

Only then use a small goal ladder or leave-one-goal-out test to locate a discontinuity. Such variants change the request: formerly desired modifiers may become junk, goal-relevant action generation may change, and progress-gated redraw semantics may change. Do not assume the resulting costs must be monotone or call a variant a one-factor comparison until these differences are accounted for. Hold the full four-goal action envelope where the existing fixture mechanism supports it, and explicitly distinguish any remaining terminal/gating changes.

## 7. Ranked hypotheses and falsification tests

### H1 — Later progress loss amplifies the dominant acquisition bill

**Why investigate first:** Both hard controllers concentrate executions in acquisition, but that alone does not identify the causal decision. Conquest supplies a useful comparison for conversion of progress to completion. [R05–R07]

**Test:** Preserve acquisition rows while changing one measured recovery/retention continuation, then perform the opposite comparison—change acquisition while preserving compatible recovery. Complete all exits and evaluate each whole original-root controller. Measure how first-exit probabilities and returns changed, not just the headline upper.

**Negative result:** A fully represented, verified alternative is more expensive, or recovery changes barely affect returns while single-excursion acquisition remains dominant. A cap or unsupported kernel is inconclusive about economics.

### H2 — Single-entry selection under-serves a fragmented native opportunity

**Evidence:** Bow’s aggregate Alteration spend spans 16 entries while the code selects the largest single-entry contribution, the Augment entry. [R03, R15]

**Test:** Compare the present single-entry scheduling with a bounded group of semantically compatible actual entries, under the same work/retention budget. First determine which entries already funnel through the selected Augment boundary. Keep execution identities distinct unless equivalence is independently justified.

**Negative result:** The current entry already intercepts the useful paths; additional entries contribute no different profitable continuation; or the grouped complete controller is more costly. Summed spend alone is not a proof of a gain.

### H3 — Current candidate coverage excludes the needed combination

**Evidence:** The fractured-Magic family is narrow, chooses one qualifying Essence if available plus the documented broad path, and does not receive the general protected expansion used by some root proposals. [R15]

**Test:** At a high-impact actual entry, create an admission/coverage table for native actions and existing programs. Select one missing family supported by native metadata and the measured phase law. Possible branches include preserving useful partial progress, a different legal acquisition, a blocker with fully observed conflicts, or a valid protected cleanup. These are candidates, not a prescribed crafting recipe.

**Negative result:** The complete native family is illegal, unsupported, economically worse, or redundant. Record which result occurred. Do not generalize a result for one entry to all entries or all Essences.

### H4 — The existing anchor/progress order is the wrong economic commitment

**Reasoning:** Improving a tail while holding the current earned fracture fixed cannot compare alternative paid root constructions.

**Test:** After one good within-anchor contrast, compare one other natively supported anchor/order, selected from current metadata and completed laws rather than case IDs. Include every paid setup/failure branch and evaluate from the original root.

**Negative result:** The alternative root acquisition cost outweighs its better continuation, or no different legal complete construction is available within the selected envelope. Never compare a free pre-fractured start with a paid empty-start controller as a product improvement.

### H5 — Representation/dependency breadth prevents the useful family from completing

**Evidence:** Historical fresh-layout reductions and the native mixed bench-conflict counterexample show both excess distinctions and missing necessary distinctions can matter. [R11, R17]

**Test:** A cheap observer/dependency census followed by one controlled private-layout comparison on the same physical entry and intended action family. Count changed junk classes, completed support, rows and checker burden. A necessary conflict observation may increase classes while enabling a valuable controller; smaller is not automatically better.

**Negative result:** The layout shrinks without delivering any useful row/continuation or the required distinction prevents safe merging. No full-scope action retirement follows from incumbent non-use.

## 8. Safety, mathematical and evidential boundaries

The original objective remains minimum expected chaos cost. Primitive count is a diagnostic and a proposal reward, not a replacement product objective. Retain the cheapest compatible independently verified root policy, including when a count-guided or locally promising candidate is worse. [R02, R14–R16]

Every positive-probability exit, paid recovery, lost goal, temporary craft and legitimate observation remains represented. A full goal mask is not necessarily the native clean terminal. Failed or off-policy mass cannot be normalized away, however small. A complete local option is not proof of a proper global composition. [R12–R14]

Policy entries bind physical item, operation, controller phase, checkpoint/offer context and immutable generation. A root certificate is not an arbitrary-entry certificate. Private layouts can receive a physical item but not copied foreign state IDs, rows or scalar uppers. The Amulet counterexample is a required regression witness for any proposal that changes observed conflicts. [R13–R17]

Old-policy occupancy is valid for attributing that policy’s immediate spend. Replacing a recurring decision changes occupancy. The performance-difference identity uses the new controller’s occupancy, and the actual emitted whole graph remains the authority. Do not multiply old visits by a guessed local saving and call it a verified root improvement. [R14]

An abstract/native action ledger stays open unless its real proof obligations are discharged. A private subset optimum is not a global lower or an exact result. An auxiliary model’s policy upper is not a native executable upper. Candidate estimates and proposed rankings supply no transition, equivalence, pruning or lower-bound authority. [R11, R13–R17]

The latest programme’s 1,000-trial results were heavily action-capped: Ring 141 completions / 859 action-limit stops; Bow alternatives 252 / 748 and 253 / 747. They are not uncensored mean estimates, quantiles or a substitute for independent complete evaluation. Keep capped trials in the denominator; do not run another large simulation merely to repeat this observation. [R03]

## 9. What the external literature adds

The options literature supports treating a stage as a closed-loop temporally extended policy with an appropriate duration and exit model, rather than as one cheap macro invocation. That is exactly the distinction needed when comparing physical execution counts and first-exit behaviour. The repository already has the relevant stopped-process mathematics; this research does not recommend importing a new options framework. [E01, R14]

The state-aggregation literature supports checking action-conditioned transition behaviour, not merging states because they share a goal count, action name or human-readable phase. This matters for both action-dependent private views and multi-entry comparison. Native legality, terminal status and reward/control distinctions must also be preserved for the intended use. [E02, E03, R13–R17]

Only official/publisher abstracts were available to this web review. These citations support the method’s framing; the repository evidence and explicit finite calculations carry the concrete recommendations. No claim about novelty, implementation speedup or a solved open problem is being made.

## 10. What is established now, and what remains open

**Established from retained evidence:** substantial execution-length differences; a good Conquest graph need not be the smallest; Bow’s earned-fracture Magic acquisition concentration; Ring’s repeated Essence concentration with some paid protection; current narrow entry/candidate gates; known distinction between actual economic rejection and capped/invalid construction; and the lack of new four-goal exact closure. [R02–R07, R15, R17]

**Strong direction, not yet a native causal result:** compare preservation of useful progress and paid return amplification first. Use the largest spending regions to locate the investigation, not to decide in advance which action family must be optimized.

**Still needed:** a uniform fingerprint of the actual native CB02 graph; native pool/tag/side/conflict data for all three four-goal requests; complete phase entry/return laws; a matched acquisition-versus-recovery contrast; and one candidate whose full-root result tests the selected explanation.

The next deliverable should therefore be a small number of **causal, complete-controller comparisons** with named outcomes, followed by one justified implementation branch. It should not be another broad campaign that produces many row counts, small upper improvements and no explanation of why the good strategies transfer—or fail to transfer—to the hard bases.
