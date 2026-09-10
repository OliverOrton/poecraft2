# First-policy recovery after the cross-base audit

**Repository:** OliverOrton/poecraft2  
**Reviewed main:** `44fdca8ebf88b9b87b2bd832360cb62128e3968a`  
**Review date:** September 10, 2026  
**Posture:** read-only source/literature research and independent small-model computation. No repository edit, native build, native solve, Simulator, queue operation, or public action was performed.

## Decision

Select **Complete-Row Proper-Policy Synthesis and First-Incumbent Recovery v1**.

The next question is not another global lower-bound refinement. It is whether the already completed native rows contain a proper executable policy which current value/progress-based initialization fails to assemble. Use a bounded qualitative reachability calculation as a candidate constructor, followed by the existing fixed-policy, compiler, and independent evaluation chain. If the completed view contains no such policy, identify the precise closure deficit rather than reporting that the native problem is unsolvable.

This is a source-grounded hypothesis, not a claim that the Ring/Amulet failures have already been solved. A mathematical counterexample exposes a limitation of two existing helpers. Whether that limitation causes the current native failures is the first implementation experiment. The usual ladder and numerical/proof owners stay; no new general planner, permanent basin library, SAT solver, or heuristic framework is selected.

Source references `[Rxx]` and primary literature `[Lxx]` resolve in `source_register.json`. The companion reference script and JSON are synthetic mathematics, not current engine qualification.

## 1. What changed since the previous review

### 1.1 Broad evidence now exists; do not run the baseline programme again

The completed cross-base work used the existing corpus/worker/reporting machinery, validated all twelve requests, measured the ten missing ordinary cases, and preserved the two short Conquest imports. Its derived current scope is Allflame/base-1, product work step eight, retention reuse enabled, no diagnostic proof handoff, generated Imprint and voluntary economic Restart off, and the original goal-progress-gated semantics. Long cases use a 240-second requested finish, 300-second watchdog, 315-second outer cleanup allowance, and 1 GiB total solver ownership. Other caps remain explicit. `[R01, R03, R17]`

| Case | Checked lower | Returned independently evaluated upper | Native time | Recorded obstruction |
|---|---:|---:|---:|---|
| CB01 Conquest empty five, imported short | 405.3694 | 85,558.7062 | 66.618 s | Requested finish |
| CB02 Conquest empty four, imported short | 198.8335 | 5,218.04095 | 62.176 s | Requested finish |
| CB03 Conquest partial three-to-five | 405.11165 | 794,067.4530 | 252.562 s | No useful strict window |
| CB04 Spine Bow four | 341.03456 | 223,349.0000 | 242.438 s | Selected continuation incomplete |
| CB05 Spine Bow five | 297.8000 | 87,200,457.4280 | 50.426 s | Valid graph, but unnamed resource stop fails bounded contract |
| CB06 Amethyst Ring two | 10.16985 | None | 0.921 s | Missing selected Normal successor, no cap |
| CB07 Amethyst Ring four | 201.85524 | 3,725,358,919.4984 | 18.173 s | Admission scratch/owned-byte refusal |
| CB08 Onyx Amulet three | 174.79327 | None | 4.748 s | Missing selected Normal successor, no cap |
| CB09 Amethyst Ring three | 172.49527 | None | 8.922 s | Admission memory before first policy |
| CB10 Jewelled Foil four | 329.5400 | 45,635.96043 | 119.394 s | Strict state cap |
| CB11 Dire Pelt three | 40.20377 | 2,313.66960 | 242.427 s | Ordinary discovery consumes allowance |
| CB12 Vaal Regalia one | 65.60036 | 65.60036 | 8.536 s | Existing exact capability measured |

All nine returned core graphs independently evaluate properly. CB05 still fails its delivery contract; a valid graph does not erase that failure. These are committed observations, not this review's measurements. CB01/02 are short controls and must not be relabelled current long measurements. A refusal before a scratch allocation need not appear in the realized retained-byte peak. `[R01]`

### 1.2 The historical partial strategy is now a verified current-scope reference

A0 checked the old partial-Conquest graph under the current engine and matching start, goal, action scope, economy and compiled data. Its cost is **80,720.78955245294**, versus today's **794,067.4530398862**. This establishes a roughly 9.84-fold policy-quality opportunity under the target, not a matched runtime regression: historical search controls differ. Keep that high-water mark and do not repeat its completed evaluation or inject the graph into search. `[R01]`

### 1.3 Two queue changes are already refuted as sufficient fixes

B1 serviced the omitted selected fringe. Ring then failed on memory and Amulet on reforge work, still without a policy. B2 prioritized that service ahead of delayed alternatives. It expanded 5,535/6,477 states, avoided the earlier caps, but still returned no policy at the finish request. Amulet still lacked a selected Normal-state operator even though its Transmute and Alchemy rows had been materialized. The roughly 68 million step calls per run are an attribution lead, not proof of a specific computational defect. Both variants were removed and the engine restored. `[R01]`

This rules out “just expand the missing Normal state” as a sufficient new plan. It does not rule out carefully selected dependency service as part of a different, actual controller-construction mechanism.

### 1.4 The advisory cleanup and mathematical intake have landed

The TestAll benchmark prerequisite, single knowledge-lint invocation, and registered-ledger wording are fixed. Threshold-query, auxiliary-ceiling, and stopping-line arguments are now in canonical mathematical documents. The probe remains compiled by default, the Veiled sampling split remains deferred, and neither the post-partition scan rewrite nor a general checkpoint was built. Do not repeat that migration or relabel it a new solver improvement. `[R02]`

## 2. Why the next direction changes

The previous threshold-directed lower proposal remains valid **when there is a compatible source upper, a live comparison, and useful boundary strength**. The completed audit did not find that situation in the earliest failing cases: CB06/08/09 have no returned upper, and CB04 spends most extraction work constructing a selected continuation rather than answering a named root-binding alternative. `[R01, R02]`

A qualitative feasibility constructor answers an earlier and potentially cheaper question:

> Given the completed rows we already own, can we select a finite controller that reaches the original goal almost surely?

For an upper candidate, not every alternative action needs materialization. Every outcome of each **selected** action does need a valid route. This asymmetry is already part of fixed-policy upper authority; the proposal exploits it before asking for all-action optimality. `[R08, R12]`

The plan does not switch the objective from minimum expected cost to arbitrary goal reachability. It constructs a proper starting controller, evaluates its cost, and then uses the existing cost improvement and proof machinery. A proper but astronomically expensive fallback is a coverage result, not a policy-quality success.

## 3. What current source actually does

### 3.1 A deliberately restrictive rank seed

`initialize_focused_proper_policy()` selects a row only when all non-self successors are already assigned, true goals, or permitted finite frontiers. It requires some exit to that lower rank. This is useful for acyclic progress plus self retries, but it cannot orient a mutually dependent retry component. The code says so and, in its supported no-incumbent mode, assigns the first admitted row to the remaining states for later SCC checking/repair. `[R04]`

This helper also participates in a lower-mode path, whose frontier treatment is not interchangeable with an executable upper. A new proper-seed constructor must be used in the upper-candidate owner; do not globally reinterpret a heuristic frontier as an executable terminal.

### 3.2 SCC repair has a concrete finite-exit blind spot

`repair_improper_policy()` sets every state in a rejected SCC to infinity in a temporary vector. It seeks an admitted row with an exit outside the SCC and evaluates that row numerically. The shared evaluator eliminates the owner's self-loop, but any other infinite-valued successor makes the row infinity. The canonical row selector rejects nonfinite candidate values. `[R05, R06, R07]`

Thus a valid probabilistic escape which also returns to **another member of the same SCC** can be unavailable to this repair. The SCC has an escape in the controlled graph, yet no row has a finite repair Q under that temporary assignment. This is a limitation of the constructive method, not an unsound published certificate.

Other callers and seed paths can avoid the bad SCC. The source finding therefore does not prove the full solver fails every such problem, nor that CB06/08 instantiate it. Native-path falsification remains mandatory.

### 3.3 Joint assembly uses a different, local progress seed

The ordinary joint attempt includes admitted rows and fully materialized alternative rows even when their optimization classification remains unresolved. It then ranks possible initial rows using goal-progress probability and cost per progress. After selecting a row it walks its successors; missing routes are queued. `[R08, R09]`

Local progress preference does not imply that all chosen rows form a closed proper policy. A more expensive/non-progress row might close a controller already present in that same completed-row set. Choosing it requires a controller-level compatibility calculation, not merely earlier queue service.

### 3.4 There is a candidate-preservation question, not yet a proven artifact loss

Joint assembly evaluates a proper numerical candidate, then may improve its selected rows before installation. Physical/observation publication closure can reveal additional required routes. The first proper numerical candidate is not yet an independently verified native artifact. `[R10, R11]`

The plan checks whether a useful candidate can be retained with its exact choices and then qualified before speculative improvement creates a new dependency. It must not claim that a valid published policy was lost when only a coarse numeric candidate existed. It must not publish a value before physical route/choice and pricing checks.

## 4. A decisive synthetic counterexample

Three states: start `s`, return state `t`, and true goal `g`.

| Source/action | Cost | Complete outcomes |
|---|---:|---|
| s / first-listed cycle | 1 | t with probability 1 |
| s / try-exit | 2 | g with probability 1/2; t with probability 1/2 |
| t / return | 1 | s with probability 1 |

The strict rank seed cannot assign either s or t. Its first-row completion selects the closed cycle. On repair, `value(t)=infinity`; the try-exit row's finite escape competes with a positive return to t, so its Q remains infinity and the finite-row selector refuses it.

Yet selecting try-exit and return is proper, with

\[
J(s)=2+\tfrac12J(t),\qquad J(t)=1+J(s),
\]

so **J(s)=5 and J(t)=6**.

No additional action, lower table, state expansion, or mechanic is needed in this example. The available rows need to be combined differently. The example matches the inspected helpers' restricted semantics; the provided Python reproduction does not call compiled native code. The plan requires a regression which does.

## 5. A support-based proper-seed construction

### 5.1 Finite view and authority

Let D be a finite view of completed semantic rows. Let G be the true goal entries, optionally augmented for construction by independently proper, fully executable continuation frontiers. Such a frontier is a committed controller, not a scalar heuristic or a locally terminating option with unresolved exits.

Unknown positive-mass successors are unavailable in this pessimistic view. Unbuilt actions remain outside this **candidate search**, but still remain open in the original lower/exactness action ledger. A negative result means “no proper controller was found in this completed view,” not “the full native request is infeasible.”

The cost calculation later uses the complete original coefficients and compatible frontier costs. The qualitative step does not invent probabilities, merge physical members, or issue an upper itself.

### 5.2 Safe support and progress

For an ordinary finite-MDP row define

\[
\operatorname{APre}(Y,X)=\{s:\exists a,\;\operatorname{Supp}(P(s,a))\subseteq Y
\;\land\;\operatorname{Supp}(P(s,a))\cap X\ne\varnothing\}.
\]

Compute

\[
W=\nu Y.\;\mu X.\;(G\cup\operatorname{APre}(Y,X)).
\]

Operationally: initialize Y to the available domain. Within Y, grow X from G by choosing rows which stay inside Y and have a positive chance to reach X. Remove states not reached by that inner construction, and repeat until stable. Keep a chosen action and the inner-iteration rank for each surviving nonterminal.

This is standard almost-sure reachability reasoning, not a novel graph algorithm. A production implementation may use an equivalent existing SCC/worklist formulation. The independent reference is intentionally simple; no claim of asymptotically optimal runtime is made. `[L01, L02]`

### 5.3 Why it permits destructive retries

At the final fixed point, every selected successor remains inside W and every non-goal has some positive-probability successor of lower rank. Other outcomes may go to the same or higher rank.

Assume a reachable non-goal bottom SCC exists under the selected fixed controller. Choose a member with minimum rank within that SCC. Its positive edge to a lower-ranked state leaves the SCC, contradicting bottomness. Thus all reachable bottom SCCs are goals and the finite chain reaches a goal almost surely.

A finite proper chain with finite immediate costs has finite expected cost. The rank argument does not say it is economical. With maximum rank K and smallest selected positive transition probability epsilon, a crude finite bound is E[T] <= K/epsilon^K: there is a descending path of length at most K, with probability at least epsilon^K, from each surviving state. This can be enormous; it is not a proposed numerical upper implementation.

The nested fixed point also captures all almost-sure winning states of the finite fully observable supplied MDP. A proper policy's closed reachable region cannot be removed: all its states have finite support paths to the goal using its selected safe rows. This completeness statement does not extend automatically to an incomplete native row view, a policy language with additional hidden constraints, or an arbitrary abstract representative.

### 5.4 Observed choices must be selected at the correct information boundary

For each positive-probability observed offer, at least one permitted successor must stay in Y. Fix one such choice for every offer. At least one positive direct outcome or positive offer must permit progress into X. Choices may differ between actually distinguishable observations; they cannot depend on an unavailable physical distinction or a future random result.

The implementation must retain the exact action and per-offer choices. If an evaluator or improvement pass later chooses something else from numerical values, that is a different controller which needs rechecking. Do not enumerate the product of all offer policies when the current representation permits factored per-offer checking; the reference demonstrates that distinction.

When a native option has more internal decisions than this simple representation, use its existing complete semantics or refuse the unsupported candidate. Do not erase the observation context to make the graph calculation convenient. `[R12, R13]`

### 5.5 A useful output need not cover every stored state

The selected controller only needs properness at its actual entry and reachable domain. An unrelated losing SCC elsewhere in the calculator must not block publication of an otherwise proper root controller. Conversely, a root certificate cannot be transplanted to that losing component. The current mathematical reference already states this entry distinction. `[R12]`

### 5.6 Qualitative proof, numerical candidate, executable upper, exact optimum

These are four different outcomes:

1. The finite completed-row view has a proper selected controller.
2. The controller's represented transition equations have a valid finite cost.
3. Its native compiled artifact routes all actual outcomes and independently evaluates properly.
4. Every competing full-scope alternative is proved no better.

The proposed helper supplies only the first, as evidence/proposal for the existing chain. The ultimate goal remains the fourth. Omitted unselected rows are harmless for finding a feasible upper candidate, but cannot justify a lower or an exactness label. A tiny positive trap probability must never be rounded away for qualitative analysis.

## 6. What the independent computation establishes

`proper_seed_reference.py` uses only the Python standard library. It implements the nested fixed point, an independent fixed-controller graph test, exhaustive enumeration of small stationary observation-resolved controllers, and exact rational policy evaluation for selected examples.

The retained output reports:

- **26 named checks passed.**
- **512 finite models** checked against exhaustive enumeration.
- **24,412 deterministic observation-resolved controllers** enumerated across that comparison.
- Reproducible seed `20260910`.

Additional cases include an available path to the goal with a hidden trap, an arbitrarily small positive trap edge, an unreachable losing state, observed-offer choices that require different decisions, unknown frontiers, a new row invalidating an old refusal, zero-cost non-goal cycles, locally terminating options that cycle globally, and a proper retry costing one billion.

The exhaustive oracle checks the qualitative winning set. This is not 512 complete native solves or 24,412 evaluated PoE strategies. The “old helper” illustrations are restricted independent reproductions, not linked C++ calls. The reference's counts and exact values support falsification of the proposal; they do not predict native throughput, policy quality, or first-policy coverage.

## 7. What the literature changes about the recommendation

| Source | Relevant result/idea | Transfer and limit |
|---|---|---|
| PRISM algorithm documentation | Probability-zero/one preprocessing is separate from quantitative MDP solving. | A qualitative proper seed can precede numeric optimization. It does not provide a native projection or a new code owner. `[L01]` |
| Junges, Jansen, Seshia, CAV 2021 | Productive, safe regions and incremental almost-sure synthesis. | Supports closed-region plus goal-accessibility thinking and not requiring the maximal region before useful output. Their POMDP/SAT machinery is not needed for a small completed finite MDP view. `[L02]` |
| T. and H. Geffner, ICAPS 2018 | Compact strong-cyclic policies for FOND planning. | Proper controllers can contain retries and need not orient every transition toward the goal. Fairness and actual policy observation constraints still matter; no SAT planner or cost-optimality claim is imported. `[L03]` |
| Kolobov et al., ICAPS 2011 | Trap handling and generalized SSP heuristic search. | Reinforces separating a finite greedy estimate from a proper controller. It is not a reason to replace the solver with FRET or assume its GSSP hypotheses match this product. `[L04]` |
| Rodriguez et al., ICAPS 2021 | Explicit fairness distinguishes nondeterministic planning variants. | Use finite native positive-probability support, not an invented fairness guarantee for adversarial or observed decisions. `[L05]` |
| Heck et al., AAAI 2026 | Integrated combinatorial policy constraints and probabilistic model checking, including partial-policy analysis. | Motivates rejecting or completing coherent controller fragments rather than blindly retrying one greedy vector. A direct qualitative graph method is the smaller first implementation here; adding SMT is not justified. `[L06]` |
| Hansson and Wahlberg, 2026 preprint | Rollout error depends on expected time to absorption as well as value error. | A nearly closed proper retry can be very expensive. This motivates evaluating/optimizing the seed; assumptions about uniform approximation error are not available for claiming a rollout guarantee here. `[L07]` |
| Fraga Pereira et al., ICAPS 2022 | Depth-first strong-cyclic planning is a memory-oriented alternative. | Retain as an alternative if candidate construction itself needs a different search order. Do not introduce another FOND planner before the completed-row feasibility test. `[L08]` |

The practical synthesis is deliberately modest: **separate existence of a proper controller from optimization of its expected cost** inside the current machinery. No publication novelty claim is made. Primary-source metadata and access levels are recorded; unavailable PDF screenshots were not used as visual evidence.

## 8. Ranked directions after reconciling current evidence

### First: proper-policy construction on completed rows

Highest immediate leverage on CB06/08 because it distinguishes missing rows from failure to choose a valid combination. Native acceptance is unchanged. The changed premise relative to B1/B2 is a controller-level success criterion and seed, not a third queue preference. A source-pattern counterexample justifies a bounded native test; the actual captured row view decides usefulness.

### Second: one dependency/commitment repair if the qualitative candidate cannot reach native publication

If the view is incomplete, identify the smallest coherent missing dependency using the existing queue. If a proper coarse candidate exists but physical publication asks for more routes, identify whether they are genuinely reachable under the frozen observed choices. Never discard a possible observation; also do not assume every alternative offered within one observation must be selected. The existing compiler/observation owner decides the legal representation.

Retain candidate evidence before speculative cost improvement only with its actual status. A previously independently evaluated artifact remains the safety net; a proper coarse candidate is not yet that artifact. Limit this follow-through to the failure exposed by the new seed.

### Third: the existing memory/threshold work once its consumer is actually reached

CB09 has an admission-memory failure and CB04 has a selected-closure failure. A qualitative graph method cannot recover absent native kernels for free. Threshold lower queries become competitive after a compatible upper and a specific alternative exist. Partition layout changes become competitive when the run reaches the affected owner. The current lower import and partition-scan proposal should not be repeated merely because they are available research ideas. `[R01, R02]`

### Other directions are not selected now

A permanent basin library duplicates the unresolved producer if no reusable entries exist. More wall time has already failed B2. Generic threading can reach the same memory cap sooner. A learned progress score does not supply missing properness or observations. A new universal lower cannot compensate for a policy which is never constructed. These remain future hypotheses, not prohibited forever.

## 9. Strongest objection and the decisive experiment

**Objection:** there may be no closed proper controller in the completed rows, so the new qualitative selector produces another correct but unhelpful answer.

That is why the first experiment freezes the available row/choice/entry semantics before generating more work. On the original early failing epoch, unknown successors remain unknown. On a later bounded epoch reached through existing owners, repeat only after relevant new completed evidence appears. Classify the result as:

- proper root controller exists in the view;
- current view cannot close, with the relevant unavailable dependencies;
- candidate exists but native observation/route validation fails;
- view analysis itself was capped or interrupted.

Only the first justifies promoting the selector into the ordinary candidate path directly. The second permits one targeted completion experiment, not an unbounded closure search. The third targets the actual observation/identity bridge. A generation with no new evidence must not trigger millions of identical feasibility scans.

For a positive view, compare the old and new candidate construction over the **same** rows and decisions. A native fixture must reproduce the mixed-return SCC limitation using actual native helpers. Then compare the two real previously failing families with the original request, caps, activation and independent evaluator. A passing synthetic fixture is not enough.

The best result is new exact closure. A more immediate meaningful result is a proper, independently evaluated deliverable controller on **both** the previously no-policy Ring and Amulet requests, followed by the existing quantitative improvement and cross-base non-regression gate. Such a result expands verified-policy coverage; it must not be reported as satisfying the preceding programme's separate 20% finite-gap/new-exact criterion automatically.

## 10. What should survive this review

Import this report once and put the actual proper-seed argument under the existing policy/properness mathematical owner. Map it to existing properness, observations and snapshot obligations; do not automatically assign an accepted new claim. Preserve the native applicability result, whether positive or negative. The accompanying implementation plan selects the work, retains the existing cohort and holdouts, and gives a concrete stopping boundary.

**Bottom line:** the new cross-base results make “construct the first proper controller from what we already know” a better immediate question than “prove the current good controller more strongly.” The proposed structural calculation is small enough to falsify before committing to another architecture.
