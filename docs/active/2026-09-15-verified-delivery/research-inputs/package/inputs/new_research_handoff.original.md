# poecraft2: solver seams, recurrence economics, and research opportunities

**Read-only audit and research handoff — 15 September 2026**  
**Repository:** OliverOrton/poecraft2  
**Pinned revision:** `4594e44b6b851511ae5471e52f232bce57d16856`  
**Audience:** lead researcher and next-plan author

## 0. Verdict and evidence boundary

The next programme should not be another generic solver rewrite, another attempt to discover the already-recovered Ring controller, or an automatic continuation of the previous lower-bound programme. The stronger direction is:

> **Make compiler-defined continuation boundaries the place where search, representation, evaluation reuse, and recurrence-sensitive economics meet; first resolve the activation and responsiveness seams that prevent native successes from becoming reliable product capabilities.**

This is an architecture and mathematics proposal, not implemented work. The current solver has valuable machinery: complete native transition construction, persistent strict proof ownership, candidate-local continuation, independent compiled-policy evaluation, graph-local decision provenance, proof-only lower checking, and experiment supervision. Several apparently new ideas already exist in these owners. Reuse them rather than create a second planner, checker, supervisor, or permanent recipe library. [R1–R8]

### What was actually done

Read current branch metadata, the latest commit comparison, current solver contracts and mathematical chapters, recent positive and negative experiment records, and selected source paths. Examined primary SSP/model-checking literature. Ran an independent, standard-library-only exact-rational test suite: **96 generated transient-chain fixtures, 576 property-check groups, and nine named examples; all passed.** The script and machine-readable results accompany this report.

No repository files were modified; no commits, pushes, issues, comments, or other public actions were made. No native solver, WASM benchmark, or game-mechanics simulation was run in this audit. A local source-download attempt could not resolve its network host, so repository inspection used the GitHub connector. The protected root item `0` was not inspected. This is a targeted architecture/source audit, not a claim to have reviewed every source line or every mechanics implementation.

Use these evidence labels throughout:

- **Recorded:** an existing pinned repository measurement; not remeasured here.
- **Source-confirmed:** directly supported by the inspected implementation.
- **Derived:** arithmetic or mathematics, with stated premises; synthetic tests are not native qualification.
- **Hypothesis:** a proposed causal explanation or improvement requiring a matched experiment.

## 1. The baseline has moved

The pinned commit is “Expose solver progress and recover native Ring continuations.” The previous Ring diagnostic-versus-native-discovery distinction has changed: the native execution-guided treatment now discovers and retains the same cheaper Ring graph. The historical fracture-trigger obstruction must not be presented as still blocking that qualified treatment. It remains relevant when auditing other activation paths. [R1, R2, R9]

| Retained controller | Expected original Chaos | Expected primitive actions | Qualification scope |
|---|---:|---:|---|
| Ring four | 220743.46354691702 | 564673.7576801107 | Native target recovered in the declared execution-count treatment |
| Bow four | 12770.827062219498 | 66914.72854184538 | Previously generated improved graph preserved |
| Amulet | 12541.579648544115 | 25518.491313143768 | Retained graph preserved under its declared profile |
| Conquest four | 3746.1319409485764 | 8608.881793656798 | Reference native graph preserved |
| Conquest five | 85558.70618560436 | 8407.202314771383 | Same graph verified substantially earlier |
| Vaal Regalia | 65.60036144971359 | Separate receipt | Exact control retained |

These are not six default-browser discoveries or six exact optima. The Ring/Bow wide lane uses the declared 600/840/870-second finish/native/host timing and 8-GiB aggregate / 4-GiB checker allowance with 400M logical work. The Conquest product-profile comparison has different limits. Preserve those distinctions. [R2, R9, R10]

Conquest-five verification moved from 244.678/244.900 seconds to 46.644/47.006 seconds in counterbalanced native pairs, without changing the target graph or original cost. The default requested finish remains four minutes. WASM verifies the same graph at about 45.032 source seconds, but that is not a matched WASM speedup experiment. [R2]

Bow's recorded cost fell from 223349.0000393144 to 12770.827062219498, a derived **94.2821% reduction**. More importantly, the relevant return-to-goal probability increased by about **21.74 times**, while the local continuation became slightly more expensive and acquisition itself stayed unchanged. The main result is evidence for changing recurrent loss/escape structure, not for simply making acquisition cheaper. [R9]

## 2. High-confidence findings at the infrastructure boundaries

### F1. Native capability and public activation are not yet the same thing

**Source-confirmed.** The public `PC_SOLVER_FLAG_DIRTY_CONTINUATION_SEARCH` maps to `DirtyGuidedStatic`. The inspected entry-construction code defines `execution` only for `DirtyExecutionCost` and `DirtyExecutionCount`, and the temporary clean late-entry selector tests `execution`. The final Ring preflight explicitly supplies `--native-dirty-guidance execution-count` and `--native-execution-action-price 0.389479993470349`. [R10–R12]

Consequently, “the solver now generates Ring's improved graph” is true for the qualified native treatment, but is not evidence that merely setting the public dirty-search flag exposes the same discovery route. Nor does native qualification establish default-browser capacity: the earlier Ring worker evaluation exceeded the unchanged 100000-state default, and its successful larger allowance is separately identified. [R9]

This is not necessarily an unintended defect. Keeping an expensive treatment private can be appropriate. It is a **capability-delivery seam** that the next plan must explicitly choose to bridge or preserve.

**Next action:** build a compact activation matrix from resolved options: ordinary Calculator, public dirty opt-in, native execution-cost, native execution-count, and actual WASM defaults. Record which candidate owners can run, not just which enum or UI flag exists. Decouple useful original-cost entry service from a diagnostic action-price mode only after bounded qualification. Do not silently change the public cost objective or raise browser limits.

### F2. The 387 ms step metric misses a 13.1-second synchronous begin

**Recorded and source-confirmed.** The M12 worker receipt records `native_begin_requested` at 0.0494 ms and `native_begin_completed` at 13092.8139 ms: about **13.093 seconds**. The worker calls `bindings.beginSolverSolve(...)` synchronously before its stepping/yield loop. Its `max_step_ms` timer surrounds `stepSolverSolve`, not begin. [R13, R14]

The same receipt fails the existing 250 ms solve-step gate with a 387.339 ms maximum. Improving that maximum alone would still leave a much longer worker event-loop blockage at initialization. The UI thread is separate; this is not evidence that rendering itself freezes. It is evidence that queued cancellation/progress work in that worker cannot be serviced during the synchronous begin call.

**Next action:** measure longest uninterrupted worker/native calls across begin, step, export, abandonment, and telemetry cleanup. Attribute begin's time to its actual native owners before choosing a patch. Make expensive setup cooperative inside the native continuation owner; wrapping a synchronous call in JavaScript `await` does not make it interruptible. Preserve immediate cheap validation and deliberately qualify any change to when errors are reported.

The existing 250 ms criterion is a step criterion. Adding an end-to-end initialization/cancellation criterion is a proposed stronger contract, not a claim that the old benchmark already enforced it. Do not infer the cause of the 387 ms outlier from the begin measurement, or attribute all begin time to retention preparation without profiling.

### F3. Entry eligibility is a larger search restriction than the ranking formula

**Source-confirmed.** The current contained wave selects at most three clean Rare, one-goal-missing physical entries, with no hidden offer/checkpoint context and a permitted primitive boundary. The graph-local request path further filters operation kinds to Exalt, Annul, and Scour. The legacy path retains parent-binding and, in some lanes, serialized-graph `"fracture"` tests. [R11]

These gates prevent illegal compositions, but also define what opportunities can even reach the scorer. Replacing the ranking formula without counting excluded boundaries cannot recover opportunities excluded earlier. The new Ring route fixes one real namespace/eligibility obstruction; it is not general post-incumbent policy improvement.

**Next action:** add a bounded eligibility/rejection census through the existing entry owner: real decision boundary, supported return construction, observation context, rarity/occupancy, goal debt, compiler provenance, and cap stop. Start with typed capability requirements, backed by the native action/refinement contracts. A JSON substring is not a durable semantic capability contract. Do not simply delete the guard: first show which invariant it approximates and which native tests replace it.

### F4. The strict path already has observation-liveness machinery that candidate layout construction does not obviously exploit fully

**Source-confirmed.** `CalcContext` constructs `layout_actions` from candidates plus fixed/conditional programme dependencies and builds a layout. Carrier-local option construction already avoids importing every parent descriptor as a dependency. Separately, `solver_refinement_observation_helpers.hpp` implements backward preservation of downstream requirements, selector preimages, affix flows, fresh-item reset, and observation propagation through ordered execution paths. [R15, R16]

This corrects a tempting but inaccurate proposal: **do not invent a second generic observer-liveness engine.** The research opportunity is an adapter from the existing contract machinery into earlier, candidate-local layout selection.

There is unusually strong prior evidence that representation matters: the recorded Amulet experiment reduced 21 junk classes to six and a Chaos row from 4160 outcomes to 85 by rebuilding without goal-neutral conversion observers. Disabling actions on an already-built layout did not provide that reduction. This is historical evidence, not a new result from this audit. [R3]

The present restriction remains conservative and selective. A broader adapter is a hypothesis; it may discover that the current layout is already minimal for a candidate, or that bench/routing observations restore most distinctions.

### F5. Evaluation reuse exists, but changed-controller reuse remains a plausible expensive seam

**Source-confirmed.** The return-bridge path can parse and evaluate related anchor, one-shot, excursion, and repeated graphs. At the same time, selective dirty growth already reuses a root check when the root-reachable selected-row vector is unchanged and only unselected alternatives grow. It is inaccurate to say the evaluator never reuses evidence. [R6, R17]

The open opportunity is narrower: reuse the **unchanged interior of a controller after selected decisions actually change**, using complete boundary-response information and precise invalidation. The compiler's new graph-local declarations provide a much better potential anchor than copied parent state IDs. First measure overlap and checker cost; a large dense boundary or global router change can destroy the value of this optimization.

### F6. The remaining source/native value discrepancy deserves localization before another estimator

**Recorded.** The Conquest-five worker receipt reports source value 358209179.6640102 versus independently evaluated graph cost 85558.70618560436, about **4186.7 times larger**. The graph remains a valid bounded policy under the current evaluator contract; the source value fails reconciliation. [R13]

This is not enough to diagnose a numerical bug. Different abstractions, continuation estimates, or graph transformations may explain the discrepancy. It is enough to reject the idea that a more accurate floating-point solve of the same source system will necessarily improve native candidate ranking or close the proof.

**Next action:** localize the discrepancy by aligned native item/control entries, primitive rewards, programme boundaries, and recurrence, using the residual attribution identity in section 4. Missing or nonuniform mappings remain unknown. Do not fit a correction to one root ratio and call it a native value model.

### F7. More local lower work is not necessarily more proof progress

**Recorded.** The strict record reports 74015 strengthened obligations, zero noncompetitive retirements, and an unchanged checked root interval; the five-goal proof reaches replay-backed partition memory. The lower research also identifies a cheap auxiliary policy and restrictions that leave a low ceiling in a particular model. [R3, R18]

A numerical proposal cannot lift a valid lower past the optimum of the same optimistic model. Before another tightening programme, extract the current optimistic winner and ask which relaxation makes it artificially cheap. Coupled refinement, not a new sweep count, is the candidate intervention.

### F8. Several historical “missing features” are now present or were rejected for good reasons

Keep these distinctions visible:

| Idea that should not be repitched as missing | Current evidence |
|---|---|
| Factored exact reforge recurrence | V3 retained; V1 still defines logical-work comparisons and other paths [R4] |
| Persistent strict proof owner | Existing quotient session, split-only partition, proof store, and dependencies [R18] |
| Root policy surviving optional proof finish | Bounded-finish publication contract retained [R6] |
| Early assertion of a materialized candidate | Current one-slot fix qualifies earlier Conquest verification [R2] |
| Arbitrary numerical proposals checked as lowers | Existing checked-potential/numerical-reuse machinery; native correspondence still required [R5, R7] |
| Rank-one occupancy stabilization | Already implemented and documented [R7] |
| Multi-return expected-cost equations | Already in the policy mathematics [R8] |
| Generic support-only selector | Prior real failures were missing support, not solved by that selector [R19] |
| Pending-node deduplication | Previous mutation changed work/first-policy behavior and regressed Conquest; removed [R19] |
| Full strict disk checkpoint | Still not implemented; completed coarse replay is a different facility [R6] |

## 3. Research programme A: contract-driven candidate representations

### Proposed question

Can the existing backward-observation contracts eliminate unnecessary distinctions **before** expensive candidate rows are constructed, while retaining the complete native universe and leaving the full proof action scope untouched?

Let a fixed finite controller have control state `k` and physical item `x`. Its true state includes any required checkpoint, offer, or programme memory. Use a control-indexed abstraction `alpha_k(x)`, rather than assume one global partition must serve every phase.

For exact selected-policy sharing, two members in a cell must agree on:

1. Native goal classification and the selected action's legality/observable choice rule.
2. Relevant expected reward, and the complete joint reward/exit law when distributional reporting is requested.
3. Probability of entering each successor `(control, abstract-cell)` pair.
4. All persistent information that can affect those facts later.

These are fixed-controller conditions. Equality for a selected controller is not equality for every competing action, and an exact selected-policy quotient is not an all-action optimality proof.

### Why backward liveness matters

A modifier distinction matters now if the current native operation observes it, or if it survives and can influence a downstream operation, goal, price, or router. A destructive operation may kill the need for some old identity; a protected action may preserve it. On cycles the requirement is a fixed point, not a single backward pass. The current refinement helpers already model much of this preservation/preimage calculation. [R16]

The candidate-layout adapter should consume those requirements, not infer relevance solely from action names or from whether an action directly creates a desired modifier. Bow's paid capacity setup is a concrete warning: a goal-neutral craft can strongly change later probability and cost. [R9]

### Minimal native experiment

Use the existing fresh private-context constructor and immutable physical entries. Compare: current public/static reduction; the recorded conversion-only reduction control; and a contract-derived selected-controller layout. Keep the modifier universe, target, original prices, action scope, and budgets fixed. Never copy private IDs, rows, or entry values between layouts.

Measure classes, discriminator causes, support sizes, row work, total candidate construction time, native checker work, peak owned bytes, and the actual returned original-cost graph. First perform the cheap layout/observer census; do not launch a long solve when no distinctions disappear.

**Stop conditions:** stop if the derived layout is no smaller; if savings disappear after necessary bench-conflict/routing refinement; or if the same candidate no longer passes whole native evaluation. A smaller row count without preserved policy quality is not success.

**Critical regressions:** mixed bench-conflict members; same desired-goal mask with different blockers; surviving crafted locks; hidden offer/checkpoint state; opposite-side effects; action dependencies not selectable as standalone primitives; native junk-free goal semantics; and cancellation before row publication.

**Novelty posture:** bisimulation and backward observation analysis are established, and the project already contains core machinery. The possible contribution is proof-separated, compiler/contract-driven representation selection in this implicit SSP pipeline—not “inventing abstraction.”

## 4. Research programme B: continuation response and recurrence-sensitive economics

### 4.1 Exact boundary reduction

For a proper finite fixed controller, let `Q` be its substochastic nonterminal transition matrix and `c` its expected immediate original-cost vector:

\[
V=c+QV.
\]

Partition states into boundary `B` and unchanged interior `I`. Define

\[
R=(I-Q_{II})^{-1},\qquad
\bar Q=Q_{BB}+Q_{BI}RQ_{IB},\qquad
\bar c=c_B+Q_{BI}Rc_I.
\]

Then

\[
V_B=\bar c+\bar QV_B.
\]

Proof: solve `V_I=c_I+Q_IB V_B+Q_II V_I` for `V_I`, substitute into the boundary equations. Goal-absorption probabilities transform by the same stopped-process law. The separate primitive-action reward transforms as another right-hand side. The included rational checks verify reward equality and complete boundary-plus-goal exit mass.

This is the useful electrical-network analogy: eliminate an interior while retaining its exact response at connection points. It does **not** assume that poecraft2's directed chain is reversible or an undirected electrical Laplacian. Schur elimination, Kron reduction, and incremental Markov-chain verification are established antecedents. The repository already states equivalent first-exit expectation identities. [R8, P1, P2]

### Proposed implementation experiment, not a new authority shortcut

Use compiler-declared decision routers as candidate interfaces. An evaluator-owned artifact may retain price-independent transition/exit structure, the required reward response, and native semantic dependencies for unchanged interiors. Avoid explicitly forming a dense inverse; solve the required sparse systems. Charge retained response storage and temporary factorization overlap to the existing evaluator/aggregate budget.

Invalidate on relevant changes to native laws, graph routing, observation rules, goal predicates, scope, member domain, programme memory, or coefficient provenance. Price-only reuse requires genuinely price-independent response information, not an old scalar cost. A textual node-ID match is insufficient.

Keep full independent flattened evaluation as the acceptance authority while qualifying the optimization. Compare against cold evaluation on the exact same candidate graph, including new off-policy successors and mutated router conditions. If a useful response artifact would require a huge or dense boundary, use the old route instead.

**First gate:** measure unchanged interior coverage, boundary size/fill, and evaluation-time attribution on actual related candidates. No cache implementation should be selected merely because the algebra is attractive.

### 4.2 Why the current entry score is not an economic saving estimate

The inspected temporary-entry ranking uses `root_expected_visits * exact_continuation_upper`. This measures repeated exposure to remaining cost, not recoverable local cost. In a ten-state chain with one unit of cost per step, each state is visited once; the total bill is ten, while the sum of visits times continuation value is 55. The scorer can count the same downstream cost repeatedly. This is a heuristic limitation, not a correctness error. [R11]

Let

\[
Z=(I-Q)^{-1},\quad d^T=e_0^TZ.
\]

For a change only to row `i`, write `Q'=Q+e_i delta_p^T`, `c'=c+e_i delta_c`, and `z_i=Z e_i`. Provided both controllers are proper on the common required state domain, the exact root difference is

\[
V'_0-V_0=
\frac{d_i\,[\delta_c+\delta_p^TV]}
{1-\delta_p^Tz_i}.
\]

Derivation: subtract the fixed-policy equations to obtain `(I-Q)(V'-V)=e_i(delta_c+delta_p^T V')`; substitute `V'=V+z_i t`, solve the scalar equation for `t`, then take the root component.

The numerator is an occupancy-weighted local advantage. The denominator accounts for altered recurrence. For cost-one retries, changing return probability from 0.9 to 0.8 changes value from ten to five. The uncorrected old-occupancy calculation predicts a change of minus ten; the feedback denominator is two, giving the correct minus five.

For multiple changed rows with selector matrix `E`, row difference `D`, and reward difference `delta_c`,

\[
V'-V=ZE\,(I-DZE)^{-1}(\delta_c+DV).
\]

The exact-rational suite checks both forms. These are standard linear-system consequences, not claims of a new performance-difference theorem.

**Native use:** compare the current score with complete one-step advantage and a response-based recurrence correction only where all tails have legitimate compatible meanings. An uncovered successor remains unknown. A new physical state without an old continuation cannot be assigned an invented `V` merely to use this formula. Complete composed-controller evaluation remains mandatory.

A bounded second wave could be considered after a genuinely cheaper verified graph changes the relevant interface law. Do not rearm on row growth, elapsed sweeps, or the same refused graph. Preserve a deterministic work limit and first-policy service. The existing second-generation fixture establishes that meaningful successive improvements are possible; it does not qualify an unbounded policy-iteration loop in production. [R2]

### 4.3 Localize the large source/native discrepancy with the same adjoint machinery

For any finite comparison vector `v` on the complete native controller domain, with goal value zero, define

\[
e=c+Qv-v.
\]

Then

\[
V_0-v_0=d^Te.
\]

This attributes root discrepancy to occupancy-weighted local residuals. It can distinguish immediate-cost mismatch, native successor-law mismatch, and tail/recurrence effects when the mapped source vector is legitimate. Group contributions by native operator/option boundary and semantic context, not arbitrary parent IDs.

This is a **diagnostic**, not permission to transport a source vector across incompatible layouts. Any missing entry, nonuniform class value, or changed control meaning is a separate unaccounted category. A partial attribution must disclose its residual remainder. The experiment should first identify whether a small set of recurring boundaries explains most of the Conquest discrepancy. Do not infer that root reconciliation can be repaired by learning one scalar correction.

## 5. Research programme C: refine the optimistic winner, not just its solver

The current lower path has valuable machinery: complete action/family coverage, independently valid outside-domain evidence, exact-binary inequality checking, native event-cap justification, and rebuilding final-vector minimizing allocations. Use those owners. Do not replace them with an unconstrained learned potential or a fixed-policy response presented as an optimum. [R5]

### 5.1 The important failure mode is consistent local optimism with impossible global witnesses

Consider a synthetic two-phase process. A hidden mode `x` or `y` is selected once and persists. Phase A costs one in mode `x` and 100 in mode `y`; phase B costs 100 in mode `x` and one in mode `y`. Every real trajectory costs 101. A relaxation taking a separate best compatible member at each phase reports two: it silently uses mode `x` in A and mode `y` in B.

The value two can satisfy every intended optimistic inequality. More accurate arithmetic cannot make that unchanged relaxation produce 101. Retaining the persistent mode, or an equivalent valid coupling constraint, is what removes the impossible witness. The accompanying exact test checks that the weak bound is valid, not a numerical error.

This is **not** a claim that the native retention producer exhibits this exact defect. It is a specific falsifiable hypothesis for its current cheap optimistic controller: is there a sequence of mutually incompatible blocker, occupancy, retention, or cleanup assumptions supplying cheap local relations?

### 5.2 A concrete lower-model investigation

Freeze the current accepted relation and its cheapest auxiliary policy. Preserve all native/legal residual alternatives. Audit which cheap exits rely on outside-domain zero continuation, generous goal-hit coupling, early cleanup, free change of hidden pool context, or a genuine cheap native route. Choose one observed mismatch, not an arbitrary additional state feature.

A proper finite policy's occupation variables satisfy

\[
\sum_a x(s,a)-\sum_{t,a}P(s\mid t,a)x(t,a)=\alpha(s),
\quad x\ge0.
\]

Projected flow constraints can be tied across features, or the producer can retain a small persistent witness coordinate. A valid lower relaxation must contain the projection of every relevant proper native policy and charge original cost once. It cannot impose the incumbent's occupancy on all possible policies. Occupation-measure heuristics and their cross-projection tying constraints are established precedents, with their own assumptions; they are not a plug-in proof for this native relation. [P3]

The native deliverable should be a strengthened admissible relation/dual witness consumed by the existing lower checker and coverage machinery. Do not claim that an arbitrary new LP cut fits the old checker without an explicit representation and validity bridge. If the producer's conservative schema cannot express the desired coupling, name that as the actual design task.

**Gate:** the strengthened model must change the optimistic root bottleneck or discharge relevant competitive obligations under unchanged budgets. More internal states, more refined inequalities, or a stronger unused component alone do not pass. Preserve the old lower independently if a changed truncated model cannot contain it as one jointly feasible vector.

**Stop:** no missing persistent witness found; the current optimistic winner remains available; new native correspondence cannot be justified; or preparation cost consumes the budget without useful root/retirement effect. In those cases defer this programme rather than adding another lower producer by momentum.

## 6. Numerical credibility: turn existing sensitivity mathematics into a scoped enclosure capability

The numerical chapter already distinguishes stored coefficients from native probabilities, fixed-policy evaluation from optimum certification, and residual size from value error. It explicitly leaves correspondence work open. Do not call this audit evidence that current accepted policies are invalid. [R7]

One small formal target is an evaluator-owned transience/error witness. For a finite nonnegative `Q`, suppose a finite positive vector `w` satisfies

\[
w\ge\mathbf1+Qw.
\]

Then `Q` is transient and `(I-Q)^(-1) 1 <= w`. If a checked native-model residual bound gives

\[
|c+Qv-v|\le\varepsilon\mathbf1,
\]

nonnegativity of the transient inverse gives

\[
v-\varepsilon w\le V\le v+\varepsilon w.
\]

Proof: the first inequality gives a strict contraction in the `w`-weighted sup norm; alternatively sum its iterates. Apply the nonnegative inverse to the two residual inequalities. The interval bounds the **fixed controller's** value. Its lower endpoint is not a lower bound on the full MDP optimum.

Native coefficient enclosures, integer-weight provenance, terminal inclusion, and outward-safe residual arithmetic remain necessary. Exact checking of rounded stored coefficients alone gives a stored-model claim, not a native interval. Near-critical retries can require a large `w`, making an honest interval wide; that is useful information rather than a reason to relax acceptance.

The repository already has closely related count-weighted residual mathematics. The new implementation question is whether its actual evaluator can cheaply issue such a scoped witness, not whether the inequality is novel. Fixed-point certificate research provides a strong precedent for separating proposal algorithms from small trusted acceptance procedures. [R7, P4]

## 7. Execution practicality without changing the optimization contract

Ring's current graph has expected primitive count about 564674. Its 1000 execution trials have 143 successes and 857 censored at 100000 primitive actions, with zero illegal actions, missing prices, or graph-step failures. The retained records correctly do not use the truncated successes as an uncensored cost estimate. [R2]

A cost-only SSP may legitimately prefer a long-running proper controller. Still, expected count is not completion probability at a user-visible cap. A deterministic ten-action policy and a geometric policy with success probability 0.1 have the same mean count ten; their probabilities of finishing within five actions are zero and 0.40951. The exact test demonstrates the difference.

**First deliverable:** a clearly labelled completion-probability diagnostic for already verified fixed controllers at declared primitive-action budgets. Do not start by changing the public objective, claiming a deterministic action cap from an expectation penalty, or publishing a censored conditional cost as the policy's expected cost.

For stopped interfaces, let

\[
H_{ij}(z)=\mathbb E_i[z^\tau\mathbf1\{\text{exit at }j\}],\qquad
g_i(z)=\mathbb E_i[z^\tau\mathbf1\{\text{goal exit}\}].
\]

The fixed controller's completion generating functions satisfy `F(z)=g(z)+H(z)F(z)`. Summing coefficients through budget `B` gives completion probability by `B` primitive actions. Expected duration alone is insufficient: duration and exit context may be correlated. Correct native counting must include mandatory internal actions and preserve control memory; zero-primitive routing must not create a spurious duration loop.

This is a research representation, not a claim of cheap exact evaluation for huge budgets. Begin with native primitive chains, bounded-error distribution propagation, and small exact controls. Measure memory and truncation error. Distributional probabilistic model checking is established and directly relevant; a general distributional solver is not needed before a useful fixed-policy diagnostic. [P5]

Only a separate owner decision should introduce a chance constraint, expected-count constraint, or bounded-horizon objective. Each defines a different optimization problem from minimum infinite-horizon expected Chaos cost.

## 8. Next-plan selection and experimental gates

Do not implement every research branch in one session. Use existing benchmark/corpus/Lab supervision and immutable report owners. The Lab default profile can change action scope, including Imprint; importing a benchmark there is not automatically an equivalent request. Resolve the exact saved manifest and treatment before dispatch. [R20]

| Stage | Selected question | Smallest useful evidence | Advancement rule |
|---|---|---|---|
| 0A | Which public/native modes can reach the proven entry service? | Resolved activation matrix and a small path fixture | Explicit product decision; no silent objective/cap changes |
| 0B | What blocks worker cancellation before normal stepping? | Begin/step/export/abandon durations and native owner attribution | Cooperative repair targets the actual long call |
| 1A | Are candidate layouts paying for dead future observations? | Contract-versus-current discriminator census | At least one real reduction surviving required native distinctions |
| 1B | Are related controller checks mostly recomputing unchanged interiors? | Actual pair/row overlap, boundary size/fill, checker time | Predicted reusable work exceeds response construction/accounting overhead |
| 2 | Does one selected representation or response intervention improve capability? | Same-request baseline/treatment, reversed timing order where timing matters | Earlier same-cost checked policy, cheaper checked policy, or same graph within previously binding budget; no regressions |
| 3 | Can feedback-aware entry choice improve a bounded search? | Matched eligibility and work; score is the only treatment | Better returned original-cost graph, not larger estimated gain |
| 4 | Does a specific persistent witness explain the weak proof ceiling? | Frozen auxiliary winner plus one native-valid refinement | Useful root-bound/retirement effect, not just local counters |
| 5 | Can numerical/tail diagnostics be issued within a useful budget? | Exact small fixtures plus one fixed real graph | Scoped native endpoint/distribution accuracy and bounded overhead |

Stages 1A and 1B are competing cheap diagnoses. Select one implementation branch based on evidence; do not automatically build both. Stage 4 is an independent research spike, not a prerequisite for improving executable policy discovery. The numerical correspondence work should remain visible even if it does not produce a near-term speedup.

### Qualification discipline

For the chosen change, preserve original prices, primitive counting, goal, action vocabulary, modifier universe, build identity, finish/watchdog meaning, logical work, and aggregate/checker limits. Candidate/evaluator memory is inside the total reservation, not extra uncharged capacity. Use existing owner-native cancellation and headroom admission.

Choose tests by changed boundary. A pure rank/latency treatment does not need unchanged-policy simulation. A changed compiled strategy needs the existing independent native evaluation and the relevant execution qualification; censoring stays visible. First-policy time, final returned original cost, policy identity, lower provenance, and first stop owner should be reported together. Preserve known negative controls as negatives, not as omitted data.

Do not run a broad soak to answer a question a source trace or small native fixture can settle. Run timed cases serially on an admitted host and preserve reversed-order confirmation for timing claims. Use `solver_corpus_runner`, `solver_reports`, and the existing Lab matrix/wait route. No new queue, process polling daemon, or all-purpose checkpoint system is selected. [R20]

### Practical stop rules

A candidate representation change is rejected if it improves a storage counter but changes first-policy scheduling enough to worsen the returned controller. A response cache is rejected if fill or invalidation makes it more expensive than cold checking. A new entry wave is rejected if it starves the first policy or repeatedly retries the same graph. A lower refinement is deferred if it cannot change its own current optimistic winner. No intervention earns a pass by increasing a capacity, dropping probability mass, or treating a partial policy as executable.

## 9. Technical-debt disposition

**Repair now when touched:** semantic eligibility encoded through graph strings or unrelated mode prerequisites; long synchronous setup outside step telemetry; duplicate parsing/checking whose measured cost is significant; stale interpretation of historical Ring status; source/native discrepancy without local attribution.

**Consolidate cautiously:** shared payload storage versus logical work/scheduling. Previous deduplication regressed the first-policy path, so preserving stable cursors, order, and policy identity is part of the contract. Moving bytes to shared ownership must not silently merge work obligations or undercount their lifetime. [R19]

**Preserve as useful infrastructure:** V3 factored rows; reference paths needed for work/determinism checks; persistent quotient and ProofStore; complete native evaluation; bounded-finish fallback; graph-local provenance; corpus/Lab supervision and immutable evidence; existing mathematical claims and counterexamples.

**Do not select speculatively:** whole-solver multithreading, a new learned planner, generic permanent option libraries, a universal dynamic quotient, full strict checkpoint serialization, or retirement based on an action being absent from the incumbent. An action unused by the current winner may still create a better controller; Bow's paid setup illustrates why progress-only relevance is unsafe for proof. This audit does not establish that any of these research topics is permanently unhelpful.

## 10. Novelty ledger

| Candidate contribution | Already known or already present | What would still need to be demonstrated |
|---|---|---|
| Compiler/contract-driven early observer reduction | Bisimulation, backward requirements; strict helper implementation already present | A sound adapter that materially reduces native candidate/evaluator work without weakening target scope |
| Reusable continuation response artifacts | Schur/first-exit equations and incremental MC verification; identical-selected-row reuse already present | Safe changed-controller reuse across real compiled routing, with useful sparse boundaries and budgeted invalidation |
| Feedback-sensitive entry selection | Performance difference and matrix inverse identities | Better bounded discovery with the same eligibility, prices, work, and authority |
| Native discrepancy attribution | Occupancy/residual identities | A causal explanation of an actual large source/native mismatch and a useful repair |
| Persistent-witness lower refinement | Coupled projection/occupation heuristics and counterexample-guided abstraction | A new native validity bridge that removes an actual optimistic bottleneck without preparation dominating |
| Fixed-policy action-budget distribution diagnostics | Distributional model checking and semi-Markov duration laws | Accurate, bounded-cost native primitive accounting on these large controllers |
| Native fixed-policy value enclosures | Transience-weighted residual theory and fixed-point certificates | Complete coefficient/provenance-to-endpoint correspondence in the real evaluator |

The plausible publishable unit is not a renamed classical identity. It is a verified, budget-aware method connecting controller-local representation, incremental checking, and implicit SSP search, with an explicit theory of when each artifact has proposal, executable-upper, or lower-proof authority. This audit is not an exhaustive novelty search and makes no claim that this combination is unprecedented.

## 11. Documentation and researcher handoff

Keep mathematical derivations in the existing canonical chapters: selected-policy response and recurrence in `docs/solver/mathematics/policies.md`; observer equivalence in the state/abstraction owner; native-value enclosures in `mathematics/numerical-closure.md`; optimistic witness refinements in `mathematics/lower-bounds.md`; and lifecycle/termination assumptions in the search/resumption owner.

Update mechanism pages beside any selected implementation: `upper-authority.md`, `states-carriers.md`, `resources-resume-replay.md`, `scheduling-bellman.md`, and `lower-pruning.md` as applicable. Link existing claims and state any new premises explicitly; do not allocate a new claim merely to rename known algebra. Research suggestions remain unimplemented until a native correspondence and qualification record exist.

The living programme record should retain the exact baseline, selected/rejected hypotheses, raw receipt identities, failure witnesses, source changes, and stop reasons. Historical statements must remain historical; a compact current-capability table should say which runtime/mode/profile is qualified. Preserve the distinctions between true new capability, earlier verification, earlier final delivery, and stronger optimality proof.

**Recommendation to the next-plan author:** select a small product-activation/latency gate first, then one evidence-driven continuation-interface experiment. Use the Bow result to prioritize recurrent progress retention and actual reachable decision contexts. Run the persistent-witness proof investigation as a bounded independent spike rather than automatically spending the next long session on lower arithmetic.

---

## Source index

All repository references below are pinned to the audited revision. File ranges identify inspected source excerpts; a referenced mechanism page is evidence for its documented contract, not proof that every caller was audited.

[R1]: https://github.com/OliverOrton/poecraft2/commit/4594e44b6b851511ae5471e52f232bce57d16856
[R2]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/active/2026-09-14-progress-and-delivery/README.md
[R3]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/research.md
[R4]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/transitions-reforge.md
[R5]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/lower-pruning.md
[R6]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/resources-resume-replay.md
[R7]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/mathematics/numerical-closure.md
[R8]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/mathematics/policies.md
[R9]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/active/2026-09-14-cross-base-strategy-recovery/README.md
[R10]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/active/2026-09-14-progress-and-delivery/M11-final-ring-preflight.json#L228-L272
[R11]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/engine/src/solver_solve_return_bridge.cpp#L710-L915
[R12]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/engine/src/solver_api.cpp
[R13]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/active/2026-09-14-progress-and-delivery/worker-conquest-M12-qualification.json
[R14]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/apps/web/src/app/engine-worker.ts#L340-L535
[R15]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/engine/src/solver_calc.cpp#L520-L640
[R16]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/engine/src/solver_refinement_observation_helpers.hpp#L120-L370
[R17]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/engine/src/solver_solve_return_bridge.cpp#L430-L680
[R18]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/strict-closure.md
[R19]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/solver/scheduling-bellman.md
[R20]: https://github.com/OliverOrton/poecraft2/blob/4594e44b6b851511ae5471e52f232bce57d16856/docs/foundation/tooling.md

The bracket labels above are audit evidence references. Their descriptions are the named files in the text; the links below make the source list usable outside this conversation.

- [R1 — Pinned commit][R1]; [R2 — Latest qualified programme][R2]; [R3 — Research and prior dispositions][R3].
- [R4 — Transition machinery][R4]; [R5 — Lower authority][R5]; [R6 — Resources and reuse][R6]; [R7 — Numerical mathematics][R7]; [R8 — Policy mathematics][R8].
- [R9 — Bow/Ring cross-base evidence][R9]; [R10 — Actual Ring activation receipt][R10]; [R11 — Entry eligibility and ranking source][R11]; [R12 — Public option mapping][R12].
- [R13 — M12 worker receipt][R13]; [R14 — Worker timing/cancellation source][R14]; [R15 — Candidate layout construction][R15]; [R16 — Existing backward observation machinery][R16]; [R17 — Return evaluation sequence][R17].
- [R18 — Strict closure status][R18]; [R19 — Scheduler and rejected experiments][R19]; [R20 — Existing experiment owners][R20].

### Primary research

[P1] Florian Dörfler and Francesco Bullo. *Kron Reduction of Graphs with Applications to Electrical Networks*. arXiv:1102.2950. Schur-response analogy; not a claim that the native chain is undirected. https://arxiv.org/abs/1102.2950

[P2] Paul Gainer, Ernst Moritz Hahn, and Sven Schewe. *Incremental Verification of Parametric and Reconfigurable Markov Chains*. 2018. Reuse of stable substructure through state elimination. The abstract/search record was available; a full arXiv open failed during this audit. https://arxiv.org/abs/1804.01872

[P3] Felipe Trevizan, Sylvie Thiébaux, and Patrik Haslum. *Occupation Measure Heuristics for Probabilistic Planning*. ICAPS 2017. In particular, flow conservation and cross-projection tying constraints; reviewed the relevant paper pages. https://users.cecs.anu.edu.au/~thiebaux/papers/icaps17.pdf

[P4] Krishnendu Chatterjee, Tim Quatmann, Maximilian Schäffeler, Maximilian Weininger, Tobias Winkler, and Daniel Zilken. *Fixed Point Certificates for Reachability and Expected Rewards in MDPs*. TACAS 2025 / arXiv:2501.11467. Certificate/checker separation; no automatic native implicit-model bridge. https://arxiv.org/abs/2501.11467

[P5] Ingy ElSayed-Aly, David Parker, and Lu Feng. *Distributional Probabilistic Model Checking*. NFM 2024. Fixed-model reward distributions and distributional queries beyond expectations. https://www.prismmodelchecker.org/papers/nfm24dpmc.pdf

A May 2026 rollout-bound preprint by Hansson and Wahlberg, arXiv:2605.22965, was also examined. Its properness and uniform approximation assumptions do not supply a native poecraft2 guarantee; no proposed implementation or novelty claim depends on it.

### Reproducibility files

- `synthetic_checks.py`: exact-rational, dependency-free tests; does not import or emulate native game mechanics.
- `synthetic_results.json`: 576 random property groups and nine named examples, all passing.
- `derived_recorded_metrics.json`: arithmetic on pinned receipt values, explicitly not new measurements.

Run `python synthetic_checks.py --output synthetic_results.json` from this directory. The default seed is fixed. Mathematical tests support the stated identities and examples, not an empirical speedup or production correctness claim.
