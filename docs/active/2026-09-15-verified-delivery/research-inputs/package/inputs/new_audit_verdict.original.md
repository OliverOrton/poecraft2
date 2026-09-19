## Audit verdict

**The strongest opportunities are at the boundaries between existing subsystems—not in replacing the SSP solver.** The repository already contains much of the machinery needed for better search, smaller representations, and safer reuse. The next programme should connect that machinery more effectively, while fixing the gap between a successful native experiment and a capability that the ordinary product can actually use.

I audited `main` at **`4594e44b6b851511ae5471e52f232bce57d16856`**, “Expose solver progress and recover native Ring continuations.” This matters because the previous Ring discovery limitation has changed: the qualified native treatment now generates the cheaper controller. Continuing a plan whose main goal is to recover that same controller would be working from an outdated baseline.

[Read the research handoff](sandbox:/mnt/data/poecraft2_audit/RESEARCH_HANDOFF.md) · [Download the complete audit packet and reproducible mathematical checks](sandbox:/mnt/data/poecraft2_solver_audit_4594e44.zip)

The findings below distinguish existing measurements, source-confirmed limitations, and proposed research. I did not modify the repository or run new native/WASM benchmarks. I did run independent exact-rational mathematical checks: **96 generated chain fixtures, 576 property-check groups, and nine named examples, all passing.**

---

## 1. Two concrete delivery gaps should change the next plan

### The newly qualified Ring capability is not the same as the public dirty-search mode

This is one of the most important distinctions I found.

The public `PC_SOLVER_FLAG_DIRTY_CONTINUATION_SEARCH` selects **`DirtyGuidedStatic`**. The new late-entry construction code defines its `execution` condition using **`DirtyExecutionCost`** **or** **`DirtyExecutionCount`**, and the temporary clean-entry selector tests that condition. Meanwhile, the final successful Ring preflight explicitly uses:

```text
--native-dirty-guidance execution-count
--native-execution-action-price 0.389479993470349
```

Therefore, setting the public dirty-search flag does **not** select the treatment that produced the newly qualified Ring result. This does not prove that no other public search path could ever discover an equivalent policy; it does mean that the successful native treatment is not automatically exposed by that flag.

The capacity distinction also remains important. The earlier Ring worker evaluation exceeded its unchanged default 100,000-state allowance; a separately admitted larger evaluator succeeded. A native result under generous allowances is not a default-browser discovery or evaluation result.

**Recommendation:** before developing another search technique, produce a small activation matrix covering ordinary Calculator, public dirty opt-in, execution-cost, execution-count, and actual WASM defaults. Record which candidate owners can actually run.

Then make an explicit product decision: should useful original-cost late-entry service remain tied to a diagnostic execution-guided treatment, or should a bounded version become independently available? That decision must not silently change the public cost objective or increase browser limits.

### The reported 387 ms maximum step hides a 13.1-second synchronous initialization

The M12 worker receipt records:

| EventWorker elapsed time    |                |
| --------------------------- | -------------- |
| `native_begin_requested`    | 0.0494 ms      |
| `native_begin_completed`    | 13,092.8139 ms |
| Maximum measured solve step | 387.339 ms     |

The initialization interval is therefore approximately **13.093 seconds**. The worker source calls `beginSolverSolve(...)` synchronously, before entering its stepping and event-loop-yielding loop. Its step timer measures `stepSolverSolve`, not initialization.

That is a larger responsiveness problem than the already-failed 250 ms step gate.

The UI thread is separate, so this is not evidence that rendering itself freezes. It is evidence that queued cancellation and progress work in the **solver worker** cannot run during that synchronous call.

**Recommendation:** measure the longest uninterrupted calls across initialization, stepping, result export, abandonment, and telemetry cleanup—not just ordinary steps. Attribute the initialization time to its native owners before choosing the patch.

A smaller JavaScript batch size cannot fix synchronous initialization. Nor does wrapping a synchronous call in `await` make it interruptible. Expensive setup has to move into a genuinely cooperative native continuation, while preserving cheap validation and deliberately qualifying any change to when errors are reported.

The existing 250 ms requirement is a *step* requirement. Adding an initialization/cancellation requirement would be a stronger explicit contract, not evidence that the old test already covered this gap.

---

## 2. Bow changes the economic emphasis—but not by proving a new solver architecture

The Bow result is substantial and informative. The recorded original-cost comparison is:

| Bow controllerExpected ChaosExpected primitive actions |            |              |
| ------------------------------------------------------ | ---------- | ------------ |
| Previous incumbent                                     | 223,349.00 | 1,404,492.01 |
| Improved native controller                             | 12,770.83  | 66,914.73    |

That is approximately **94.28% cheaper**. But the most useful explanation is not simply “the new option is better.”

The recorded return-law comparison shows that acquisition stayed at approximately **22.2372 Chaos**, while the probability of reaching the goal before returning to the acquisition anchor increased from approximately

```math
0.0001315760 \quad\text{to}\quad 0.0028604082.
```

That is roughly a **21.74-fold increase in escape probability**. The subsequent tail actually became slightly more expensive, from approximately 6.88703 to 8.56469 Chaos. Paying more locally reduced repeated loss of accumulated progress.

For a compatible single-anchor renewal structure,

```math
J=\frac{a+b}{p},
```

where `a` is acquisition cost, `b` is the complete subsequent excursion cost, and `p` is its probability of finishing rather than returning. Improving `p` can dominate a small increase in `b`. The full original-root result still needs its actual prefix and return contexts; this scalar equation must not replace the repository’s vector treatment where several contexts exist.

In plumbing terms, the expensive problem was repeated circulation through the return line—not merely the cost of one pass through the pump.

**The direction this supports is searching for changes to recurrent loss and escape structure.** It does not support preferentially optimizing acquisition cost, counting desired modifiers as sufficient progress, or removing actions because they do not directly create a goal modifier. Bow’s paid capacity setup is a particularly strong counterexample to that last shortcut. The recorded gain is attributed to capacity/blocker coverage and entry service, not to action-count weighting.

---

## 3. The best representation opportunity is to reuse machinery that already exists

A tempting recommendation would be “add backward observation analysis so the solver forgets irrelevant item distinctions.”

**That would be misleading: significant backward-observation machinery already exists.**

The strict/refinement path has code for downstream observation requirements, selector preimages, surviving affix features, fresh-item resets, and propagation through ordered execution paths. Separately, candidate `CalcContext` construction builds an abstract layout from candidate actions and programme dependencies. Carrier-local construction already avoids importing every parent descriptor indiscriminately.

The research question should instead be:

> **Can the candidate-construction path consume the existing semantic observation contracts early enough to avoid building distinctions that the candidate’s future execution will never need?**

There is meaningful historical evidence for the importance of this representation choice. The recorded Amulet investigation reduced **21 junk classes to six**, and a Chaos row from **4,160 outcomes to 85**, by constructing a fresh layout without certain conversion observers. Merely disabling actions on the old layout did not obtain that reduction. This is prior repository evidence, not a new measurement from this audit.

### What a sound extension would require

For a fixed controller, consider a partition that depends on the current control node rather than one global partition serving every possible action.

Two physical states can share a selected-policy cell only when they agree on the native goal, selected-action legality and observations, relevant reward, and probability of entering each successor control/cell pair. Features that survive and affect future operations or routing must remain distinguishable. Cyclic controllers require a fixed point of these observation requirements.

This is **selected-controller equivalence**, not permission to merge states for every competing action or to retire those actions from the optimality proof.

The immediate experiment should be cheap: compare the current reduced layout with the requirements derived from the existing backward-analysis machinery. Count which distinctions would actually disappear before building expensive rows.

**Stop immediately if no useful distinctions disappear**, or if necessary bench-conflict and routing observations restore them. The repository already contains a warning against representative-only reasoning: a bench operation could be legal for one coarse member but conflict for another, requiring additional observation/refinement.

This is a promising integration target precisely because it reuses an existing semantic owner rather than introducing another abstraction engine.

---

## 4. Continuation interfaces can unify reuse, ranking, and diagnosis

This is the mathematical direction I would prioritize after the activation and latency checks.

### Reuse unchanged controller interiors—not just unchanged complete policies

The repository already has some useful reuse. Selective dirty growth can retain a root check when the root-reachable selected-row vector is unchanged and only unselected alternatives have grown. Conversely, the return-bridge path can independently evaluate related anchor, one-shot, excursion, and repeated graphs. So the correct finding is not “there is no reuse”; it is that **reuse across changed selected controllers remains a plausible expensive boundary**.

For a proper finite controller,

```math
V=c+QV.
```

Partition its nonterminal states into a small boundary `B`, where decisions may change, and unchanged interior `I`. Eliminating the interior gives

```math
\bar Q = Q_{BB}+Q_{BI}(I-Q_{II})^{-1}Q_{IB},
```

```math
\bar c = c_B+Q_{BI}(I-Q_{II})^{-1}c_I,
```

and therefore

```math
V_B=\bar c+\bar QV_B.
```

The same reduction applies to the separate primitive-action reward. Complete goal-absorption mass must also be retained.

This is the useful electrician’s analogy: eliminate a circuit’s interior while preserving its response at the connection points. The underlying Schur/Kron algebra is established, as is reuse of stable substructure in incremental Markov-chain verification. It does not assume poecraft2’s directed chain is an undirected electrical network. ([arXiv](https://arxiv.org/html/1102.2950v1 "https://arxiv.org/html/1102.2950v1"))

The repository already documents equivalent first-exit mathematics. **The proposed contribution is an evaluator-owned, dependency-checked response artifact**, not a new theorem or a renamed recipe library.

A useful pilot would retain responses for unchanged interiors, rebuild affected boundaries, and compare against cold full evaluation of the same emitted graph. It must invalidate on relevant routing, observation, native-law, goal, context, and coefficient changes. Sparse solves should be used rather than explicitly forming a dense inverse.

Most importantly, **measure overlap and boundary size first**. A response cache is a bad optimization when interfaces become dense, router changes invalidate most of the graph, or memory accounting exceeds the work saved.

### Current entry ranking can count the same downstream expense repeatedly

The inspected selector ranks temporary entries using

```text
root_expected_visits × exact_continuation_upper
```

and retains at most three eligible entries. That is a measure of repeated exposure to remaining cost—not a measure of achievable local savings.

A simple counterexample makes the distinction clear. In a ten-state chain with one unit of cost per step, the true total cost is ten. Every state is visited once, but summing visits times remaining cost gives

```math
10+9+\cdots+1=55.
```

That does not make the current heuristic incorrect as a heuristic. It means it should not be interpreted as a spend decomposition or expected improvement.

For a more precise comparison, let

```math
Z=(I-Q)^{-1},\qquad d^T=e_0^TZ.
```

Suppose a candidate changes only row `i`:

```math
Q'=Q+e_i\delta p^T,\qquad c'=c+e_i\delta c.
```

When both controllers are proper on the required common state domain,

```math
\boxed{ V'_0-V_0 = \frac{ d_i\left(\delta c+\delta p^TV\right) }{ 1-\delta p^TZe_i } }
```

The numerator is an occupancy-weighted local advantage. The denominator corrects for changed recurrence.

For cost-one retries, reducing return probability from `0.9` to `0.8` changes expected cost from ten to five. An uncorrected old-occupancy calculation predicts a reduction of ten; the recurrence correction gives the actual reduction of five.

I verified this identity, its multi-row counterpart, and boundary elimination with exact rational arithmetic in the included tests. These are mathematical derivations, not native speedup measurements.

**Proposed experiment:** compare the existing score with advantage/response-based scores under identical eligibility and work budgets. Where a successor lacks a compatible continuation, leave the score’s missing contribution unknown—do not invent a value to make the formula usable. Full composed-controller evaluation remains the acceptance authority.

### Use the same machinery to explain the enormous source/native value discrepancy

The Conquest-five receipt reports a source value of approximately **358.2 million** versus an independently evaluated graph cost of **85,558.71**—a ratio of about **4,186.7**. The resulting graph remains a bounded feasible policy; the source value does not reconcile. This alone does not establish a numerical bug.

Before adding another learned correction or estimator, localize the discrepancy.

For any legitimate finite comparison vector `v` on the complete native controller domain, define

```math
e=c+Qv-v.
```

Then

```math
V_0-v_0=d^Te.
```

This attributes the root discrepancy to occupancy-weighted local residuals. With valid source/native alignment, it could reveal whether a few recurring boundaries dominate the mismatch, or whether the discrepancy is distributed across immediate costs, transition laws, and continuation estimates.

Missing mappings or nonuniform source-class values must remain explicitly unaccounted for. A root ratio is not a substitute for that analysis.

---

## 5. The lower-bound programme needs a structural hypothesis, not more momentum

The strict record reports **74,015 strengthened obligations, zero noncompetitive retirements, and an unchanged checked root interval** in that investigation. The five-goal proof reaches replay-backed partition memory. These results do not make lower-bound research a dead end, but they do show why “strengthen more inequalities” is not a sufficient next objective.

The key mathematical limitation is:

> **More accurate solution of an unchanged optimistic model cannot raise its lower beyond that model’s optimum.**

A small synthetic example shows what can go wrong structurally.

A mode `x` or `y` is selected once and persists through two phases. Phase A costs one in mode `x` and 100 in mode `y`. Phase B costs 100 in mode `x` and one in mode `y`. Every real trajectory costs 101.

A relaxation taking an independent favourable member at each phase reports two: it uses `x` for A and silently changes to `y` for B.

The bound of two can be mathematically valid. Perfect arithmetic does not remove the impossible combination of favourable assumptions. Preserving the mode, or an equivalent valid coupling constraint, does.

**This is a proposed diagnostic model for poecraft2, not a claim that its current producer has this exact defect.** The native investigation should extract the current cheapest optimistic controller and identify whether its advantage depends on mutually incompatible pool, blocker, capacity, retention, or cleanup assumptions.

Occupation-measure heuristics provide established precedent for coupling projected flows rather than treating each projection independently. Their theory is relevant, but their assumptions do not automatically prove a new native relation sound. ([ANU CECS Users](https://users.cecs.anu.edu.au/~thiebaux/papers/icaps17.pdf "https://users.cecs.anu.edu.au/~thiebaux/papers/icaps17.pdf"))

The next lower-model experiment should therefore retain **one specifically justified persistent distinction or coupling constraint**, using the existing native correspondence and lower-checking owners. It must preserve coverage of all relevant alternatives and charge original cost once.

The advancement criterion is a useful change at the root or in competitive-action retirement—not merely more refined states or higher local counters. If the same cheap optimistic controller remains available, stop that treatment instead of adding another layer of arithmetic.

---

## 6. Keep numerical credibility and execution practicality separate

### Numerical acceptance still has a worthwhile formal target

The numerical documentation already distinguishes native probabilities, stored coefficients, fixed-policy evaluation, and optimality proof. It also explains retry-amplified residual error. This is not missing theory, and it is not evidence that current accepted policies are invalid.

A concrete implementation target is an evaluator-owned transience/error witness. For finite nonnegative `Q`, suppose a finite positive vector `w` satisfies

```math
w\ge \mathbf 1+Qw.
```

If a checked residual bound gives

```math
|c+Qv-v|\le\varepsilon\mathbf 1,
```

then

```math
v-\varepsilon w\le V\le v+\varepsilon w.
```

The important work is establishing the coefficient provenance and outward-safe native residual checks—not merely writing down this familiar inequality. The lower endpoint here bounds the **fixed policy’s cost**, not the full MDP optimum.

The recent fixed-point certificate literature supports keeping numerical proposal algorithms separate from small acceptance procedures; it does not supply poecraft2’s implicit native-model bridge automatically. ([arXiv](https://arxiv.org/abs/2501.11467 "https://arxiv.org/abs/2501.11467"))

### Ring’s long execution tail is not disproved by properness

The current Ring graph has expected primitive count around **564,674**. In its 1,000 execution trials, **143 completed and 857 were censored at 100,000 actions**, with zero reported illegal actions, missing prices, or graph failures. That sampling result is not an uncensored estimate of policy cost.

An expected-action penalty does not imply a hard execution cap or a particular probability of finishing before that cap. A deterministic ten-action policy and a geometric policy with success probability `0.1` both have expected count ten, but their probabilities of finishing within five actions are zero and `0.40951`.

My recommendation is initially diagnostic: report completion probability at declared primitive-action budgets for already verified controllers. Do not silently turn the public objective into a chance-constrained or finite-horizon problem.

Distributional probabilistic model checking is directly relevant here, but a whole new distributional optimizer is unnecessary before a useful fixed-policy diagnostic. ([Prism Model Checker](https://www.prismmodelchecker.org/papers/nfm24dpmc.pdf "https://www.prismmodelchecker.org/papers/nfm24dpmc.pdf"))

---

## 7. What I would preserve, and what I would stop automatically revisiting

The audit argues strongly against throwing away the current architecture.

The repository already has factored reforge recurrence, a persistent strict proof session, explicit proof dependencies, bounded-finish publication, independent compiled-policy evaluation, and selective reuse. Those are useful assets. The first-exit mathematics and rank-one occupancy stabilization also already exist.

Some rejected experiments should remain rejected until their failure mechanism is specifically addressed. For example, pending-node deduplication saved memory but changed the first-policy path and regressed Conquest; “less duplicated storage” was not enough to make it a good change.

I would not select whole-solver multithreading, a generic permanent option library, a new learned planner, or full strict checkpoint serialization as the next default programme. That is a priority judgment, not a claim those subjects can never help.

The experiment infrastructure also does not need replacement. The existing corpus runner, report comparison, Lab matrix/wait path, and native supervision should remain the owners. A particularly important warning in the tooling documentation is that the Lab default profile can change action scope, including Imprint; importing a diagnostic there is not automatically an identity-preserving shortcut.

---

## 8. Recommended next-plan sequence

| PriorityQuestion to answerGate before substantial implementation |                                                                                           |                                                                                       |
| ---------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| **First**                                                        | Which proven candidate services are reachable in each actual product/native mode?         | Resolved activation matrix and a small path fixture                                   |
| **First**                                                        | What causes the long synchronous initialization and remaining step outlier?               | Native owner attribution, with begin/step/export/cancellation measured separately     |
| **Next**                                                         | Can existing observation contracts reduce candidate layouts earlier?                      | A real discriminator reduction that survives native legality and routing requirements |
| **Alternative next branch**                                      | Do changed controllers share enough interior structure to make response reuse worthwhile? | Actual overlap, boundary size/fill, checker cost, and memory accounting               |
| **Then**                                                         | Can recurrence-aware ranking improve the returned policy under the same work budget?      | Identical eligibility, prices, and limits; full original-cost evaluation              |
| **Independent research spike**                                   | What assumption makes the current optimistic winner unrealistically cheap?                | One native-valid refinement that changes the relevant root/retirement bottleneck      |

The two representation/reuse branches should begin as **competing cheap diagnoses**, not as two large implementation commitments. Select the one supported by actual measurements.

For mathematical maintenance, the handoff identifies the existing owners for each derivation: policy/return equations, abstraction conditions, lower relations, numerical closure, and resource/resumption contracts. New implementation work should update those chapters and their existing claims, while leaving proposals visibly unimplemented until native correspondence and qualification exist.

### Where the novelty could be

I found promising contribution targets, but the defensible novelty is not “we discovered Schur complements,” “we invented occupancy measures,” or “we added abstraction.”

The stronger research possibility is a **verified, budget-aware connection between compiler-defined continuation boundaries, candidate-local representations, and incremental policy checking in an implicit SSP**—with explicit separation between proposal guidance, executable-upper authority, and lower-proof authority.

That is a concrete research direction with measurable failure conditions. It also follows the strongest repository evidence: **Bow improved by changing recurrent loss, Ring improved when a reachable decision finally received the right service, and Conquest became useful much earlier when an already-materialized candidate was checked promptly.**

**My recommendation to the lead researcher is to close the activation and initialization seams first, then use the existing semantic contracts to make economically important continuations cheaper to construct and verify. Keep lower-proof work active, but require it to attack an identified structural source of optimism rather than simply continuing the previous programme.**