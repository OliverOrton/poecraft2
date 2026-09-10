# Audit 2 — Computation reuse, resumption, and research survival

**Repository:** OliverOrton/poecraft2  
**Audited snapshot:** `be553608ecabde28a3dec07856255911459ec97d`  
**Date:** September 9, 2026  
**Scope:** Combined response to the two supplied Audit 2 briefs. Advisory findings, not an implementation gate.

## Verdict

**The solver is not repeatedly starting from nothing. It already retains substantial expensive work within a live solve, and numerical reuse has demonstrated a real preparation-time benefit. The important gaps are narrower: live strict-proof work does not survive process loss, and persistent proof storage does not eliminate all whole-partition reconstruction.**

The September 9 proof experiments establish more admitted and evaluated work, but not a material improvement in the four-goal root interval or a new four/five-goal exact closure. They do not establish the same outcome on other bases. The earlier useful partial-continuation repair did improve the independently evaluated five-goal policy; that successful result must not be conflated with the later unsuccessful proof campaign. [Living record][S8]

The three recommended actions, in priority order, are:

1. **When the recorded strict-partition failure is next investigated, capture the smallest deterministic input to that failing owner.** Extend existing benchmark/evidence ownership, not the completed-graph checkpoint into a general live-solver snapshot.
2. **Consider one-pass, generation-local class membership and representative metadata.** This removes a specific repeated scan without changing quotient authority. Do not claim it fixes the recorded memory failure before locating that failure relative to the scan.
3. **Use the existing selected-context exporter plus one report with an embedded knowledge delta.** Preserve the original argument and a few pinned input/result/provenance references; do not introduce another mandatory packet or store.

This ranking reflects the identified investigation and computation boundaries, **not measured savings forecasts**. Every recommendation requires reconciliation with Codex's eventual newer implementation. No later branch state is silently included here.

### Evidence posture

“Source” below means the pinned implementation or contract. “Recorded measurement” means a historical run retained in the repository, under that run's own provenance—not a run performed for this audit. “Inference” and “proposal” are identified explicitly. This audit did not build, run tests, solve, simulate, mutate a queue, or access local build artifacts or the Lab database. Existing acceptance records are evidence of the reported checks, not evidence of frequent adoption. Archive manifests/provenance were inspected through their committed records; archive bytes were not independently expanded or rehashed. The decisive September 9 material is available as the [comparison JSON][S19], [provenance][S27], and [original proof-time evidence bundle][S28].

## 1. What can be resumed today, and what does it skip?

**An existing live handle can continue its retained work. A development checkpoint can reload an eligible completed coarse graph. A job retry, ledger resume, or reproduction bundle does not restore a live strict solve.** The public stepped API explicitly distinguishes `step`, bounded finish, and `abandon`; abandon discards retained partial refinement/certification work. [Public API][S2] [Resume contract][S1]

### State-survival matrix

Here, **kernel evidence** means exact transition structure for a compatible semantic query. It is distinct from a **controller decision**, a **value-dependent probability allocation**, a **cost/value vector**, and a **proof use-site**.

| Boundary | What actually survives | What is recomputed, invalidated, or unsupported | Entry point, eligibility, and observed use |
|---|---|---|---|
| Another cooperative step on the same live solver | Live calculator/graph, completed rows, active cooperative work, numerical continuation, candidate snapshots, and any live strict session; already verified incumbent artifacts remain owned. | Newly demanded work still runs. Partial row scratch cannot masquerade as a completed kernel. Cancellation/abandon or an owner's terminal resource refusal is not a resumable yield. | `pc_solver_solve_step` after `pc_solver_solve_begin`; same live handle and unchanged problem. Current short/long stepped runs exercise this lifecycle. First unsupported case: process loss or destroyed/abandoned work. [S1][S2] |
| Next missing continuation of the same candidate | `ResumableJointPolicyContinuation` retains selection/boundary snapshots, fixed decisions, processed/reachable state sets, walk, cursor, and the specifically missing state. | Resume validates the fixed prefix; this is not “no repeated work.” Unrelated value/proof improvements alone do not invalidate structural compatibility. Changed semantic authority or invalid fixed rows do. `ResourceInterrupted` releases the candidate rather than durably pausing it. | Internal candidate continuation, not a disk-resume CLI. C3's recorded sibling servicing is an actual consumer. First unsupported case: treating a newly chosen controller as the old snapshot. [S5][S8] |
| Next partition/frontier generation within one strict candidate session | `PersistentQuotientSession`, selected raw-row payloads, locator/descriptor interning, discovery cursor, installed recipes, compatible published rows, proof store, Bellman state, and stable cell identities. | New carriers are discovered; observation/exact partition inputs are reconstructed; changed class projections and generation-dependent proof uses are revalidated. A distinct candidate does not automatically inherit this session's authority. | Internal `lift_policy_quotient_pass_task`; observed F4–F8 frontier/proof activity. First unsupported case: reusing old source/target coverage after an incompatible split. [S6] |
| A new solve on a compatible calculator | Compatible calculator state/operator namespace and exact kernel templates can remain; an eligible completed coarse transition cache can be reused. | Public result and compiled-strategy cache are reset at replacement begin. Prices, numerical values, selection, strict proof, compilation, and evaluation are not an automatically resumed result. State-local automatic decisions may invalidate their derived graph. Enabled retention preparation still runs in new `SolveWork` setup. | `pc_solver_solve` or a new `pc_solver_solve_begin`; compatibility is checked, not inferred from “same base.” First unsupported case: assuming a previous policy/result survives a failed replacement solve. [S2][S4][S18] |
| Fresh process loading a development checkpoint | Completed coarse rows/probabilities, ordered states, operators, candidate/dependency/admission namespace, and supporting graph metadata. | Not live controller execution, strict session/proof quotient, numerical solve state, current minimized allocations, compiler work, or evaluator work. Bellman, strict refinement, compilation, and evaluation rerun. | Benchmark `--save-development-checkpoint PATH` / `--load-development-checkpoint PATH`; native checkpoint API. Completed reusable closure and a fresh compatible loader are required. A historical cross-process Eldritch control exists. First unsupported case: incomplete/focused graph, active admission cursor, or proof-carrying quotient. [S1][S3][S9] |
| Supervisor recovery, job retry, or corpus-ledger resume | Attempt identity, reports/partial reports, logs, immutable inputs, reservations and process-ownership records. A still-live child may continue owning its own RAM. Corpus resume preserves completed-case records. | A dead child is not reconstructed from these records. Retry is a new attempt. Corpus resume skips compatible recorded-completed cases whose report file exists; remaining cases run again. Queue pause affects dispatch, not live mathematical state. | Existing Lab recovery/retry and corpus runner. `run_corpus` checks ledger provenance/configuration before reuse. Actual local retry frequency and recovery outcomes are unavailable here. First unsupported case: interpreting `skipped_completed` or recovered artifacts as restored numerical state. [S10][S11] |
| Exported reproduction bundle | Bounded input/configuration/provenance, report and event evidence, reproduction arguments/log excerpts, and artifacts actually included in the export. | No implicit native heap, coroutine, SCC workspace, selected-proof session, or unexported local file. Reproducing output/checksums is not proof that computation was resumed. | Lab `export-bundle`; committed comparison/provenance and evidence bundles also support remote investigation. The September 9 input and report hashes are accessible independently of old chat context. First unsupported case: expecting a bundle to restore arbitrary live solver ownership. [S10][S19] |
| Changed binary, prices, action scope, or target | An old artifact remains historical evidence. Price-independent kernel structure can remain useful only inside its actual compatibility contract; it is not a price-independent policy or lower certificate. | Changed prices require cost/selection/certificate revalidation and may change admission. Changed actions, targets, terminal semantics, or pools change proof scope and potentially structure. Benchmark checkpoint identity also binds build/compiler/artifact/case/economy and relevant overrides. | There is no general cross-version or cross-target checkpoint migration contract. The native serializer checks format/layout/payload and the caller-supplied identity; the harness owns the stronger external identity. Do not weaken that identity to force a hit. [S1][S3][S4][S12] |
| New research/Codex session with repository artifacts | Committed mathematical arguments, claim statements/preconditions/history, original research, inputs, comparison summaries and available evidence bundles. | Old conversational context, unpushed changes, local binaries/databases and unexported work do not appear automatically. A selected context export is not the entire transitive proof library or live runtime state. | `solver_knowledge context`, followed by its pinned references. A real redirected export and its Unicode repair are recorded. First unsupported case: treating local remote-tracking visibility or an old chat citation token as verified external availability. [S13][S14][S15] |

### Development replay: the concrete restriction

`validate_cache` refuses focused/incomplete graphs and proof-carrying quotients. Save refuses active admission cursors and a missing reusable closure. Load requires a fresh context and checks format, layout, payload integrity and caller identity. A required replay mismatch throws rather than silently taking a cold path. Those are source-established restrictions, not merely documentation warnings. The historical checkpoint acceptance record reports round-trip/refusal checks and a cross-process run; I did not rerun those tests. [Serializer, validation and load/save][S3] [Solve setup][S4] [Acceptance record][S9]

The usable command fragments are:

```text
--save-development-checkpoint <checkpoint-file>
--load-development-checkpoint <checkpoint-file>
```

They belong on the corresponding original **single-case benchmark invocation** with the required compatible inputs—not on a generic “resume this failed Lab job” command. Native callers have `pc_solver_development_checkpoint_save` and `pc_solver_development_checkpoint_load`. The feature is not a release-WASM/live-strict checkpoint. [S1][S9][S20]

**Counterargument:** completed-closure replay can still be worthwhile for a stable, expansion-heavy case or a downstream numerical investigation. Correct. In the recorded `eldritch-annul-exalt-side-intent-forced-winner` control, expansion fell from **44.69 ms to 0.07 ms**, with matching cost, transition/policy identities and strategy. But total time rose from **719.12 ms to 1,315.29 ms**. That establishes functioning reuse, not an end-to-end speedup. Keep the feature; do not sell it as recovery for a different phase. [S9]

## 2. Two expensive investigations: was the repeated cost avoidable?

### Example A — Four-goal demanded-continuation/proof work, F6/F7

The final four-goal F7 observation took **240.690 s**, including **178.287 s of strict work**. It retained the same selected candidate identity as F6, strengthened **74,015** obligations, and returned the unchanged interval **198.8334996747695–5,218.040949685988**. The lower lookup itself took **1.654 ms**; there were **zero noncompetitive retirements**. [Useful-proof-time record][S8]

**Existing coarse replay could not preserve those 178.287 strict seconds.** Its format does not contain the selected strict closure, proof-store generations, frontier, or numerical/proof continuations. The live proof-handoff boundary also need not be an eligible completed coarse closure. No compatible checkpoint for this boundary is established by the inspected evidence.

**Derived arithmetic, not a new native measurement:** subtracting the recorded strict time from total leaves **62.403 s**. Even the unrealistic removal of *every* non-strict second could not save more than that while retaining the same strict workload. This is arithmetic on recorded stages, **not a predicted checkpoint saving**: the remainder includes work that coarse replay does not skip, including setup and final publication.

The earlier missing parent was not simply proof that an old decision had been lost. Parent 4741 existed structurally but lay beyond the selected policy's table. F4–F7 built new demanded continuations rather than recovering an already-owned action. Across earlier treatments, wall-triggered handoff could also choose a different candidate. Different candidates and newly reached carriers are genuinely new semantic work; they are not interchangeable repetitions. [S8][S5][S7]

**Avoidable portion:** reconstructing the same failure boundary merely to rediscover its identity is a plausible research cost, but its measured prefix cost is not separated here. A fixed candidate/frontier witness could address that; loading a completed coarse graph cannot. **Smallest next check:** use the existing candidate identity and failure records to establish that the next proposed experiment targets the *same* boundary before requesting another run. This is urgent only when that investigation is actually selected again.

### Example B — Five-goal partition-memory stop, F8

F8 took **184.099 s**, including **117.089 s of strict work**, and stopped at the existing memory cap during replay-backed closed-partition reconstruction. It retained the verified interval **405.3694021063399–85,558.70618560436**. The retained upper was useful bounded behavior, not a new exact result. [S8]

A completed-graph checkpoint cannot restore this failed strict partition pass or remove its peak. Replaying the same workload under the same cap does not itself create memory headroom. The analogous derived, all-non-strict arithmetic remainder is **67.010 s**; again, that is an intentionally loose outside-strict ceiling, not an attainable saving.

There is source-established pass-local reconstruction worth investigating: observation nodes, exact nodes and materialized states are populated for the current locator population, and later partition/stabilization work overlaps retained session structures. However, source also shows existing releases and sharing. The cap does **not** prove every overlapping buffer is redundant, nor that the per-class scan identified below caused this particular stop. [Partition pass][S6]

**Avoidable portion:** repeatedly reaching an unchanged partition input just to inspect the same allocation failure. **Smallest next check:** a deterministic input/ownership witness for the failing partition invocation. The exact time already spent before that invocation and the byte-owning allocation are the missing evidence that would change the expected payoff. A whole live-solver checkpoint is substantially more state than this consumer needs.

**Strongest objection to both examples:** capture/replay engineering can cost more than another bounded run, especially while the implementation changes. Therefore capture is conditional on a repeated, still-relevant boundary—not a prerequisite for Codex's current programme and not a reason to rerun either case now.

## 3. Persistent machinery is real; repeated reconstruction is more specific

### What the candidate cursor already avoids

`ResumableJointPolicyContinuation` does not discard a fixed prefix merely because global values or proof generations improved. It records immutable semantic context, selection and boundary snapshots, fixed decisions and a walk cursor. When its named missing state becomes ready, it resumes from that position after checking the prefix. Already processed states are not resolved again as new decisions. [S5]

There are two important limits. First, `validate_prefix` still walks fixed decisions on a dependency resume; preservation does not make validation free. Second, terminal resource interruption calls `release`. This is retained in-process continuation for a compatible candidate, **not durable pause/crash recovery and not permission to splice in current greedy decisions**. No recommendation to remove these checks is justified by a cache-hit-rate target.

### What the strict quotient already avoids

Within one `PersistentQuotientSession`, selected raw rows, collision-checked identities, shared descriptor payloads, strict locators, the discovery cursor and installed-recipe count persist. The next pass processes newly appended locators rather than rebuilding every native selected kernel. Published rows are reused only when their source cell/generation, selected payload and proof use-site remain valid. The Bellman graph/proof store survive compatible frontier growth. [S6]

Partition changes are not merely housekeeping. If a target class splits, the old probability into that class does not establish the probabilities into its children. Source splits similarly affect member coverage. `stabilize_quotient_partition_state` and the explicit source/target invalidations preserve this distinction. **Retaining a row's physical transition evidence can be sound while reusing its old quotient proof is not.** [S6]

### A concrete remaining duplicate: class metadata scans

After closed partition construction, `class_coverage(class_by_node, class_id)` scans the entire node-to-class vector for each class, while `representative_for` uses a linear `find`; representatives are requested again by downstream consumers. This is distinct from the expensive native kernel work already retained. [S6] (source lines 2860–3340)

**Inference:** for N nodes and C classes, coverage construction has an O(NC) scan component. The same first representative and ordered contiguous membership ranges can be produced in one O(N) pass, with O(C) representative metadata and at most O(N) total ranges. Existing canonical coverage construction and proof validation can remain the authority.

That is a concrete small-change candidate, **not a measured hot spot**. It may run after the allocation that fails in F8. Its earliest beneficiary would be a many-class pass that actually reaches class/row publication, not automatically the memory-capped five-goal case. The strongest objection is therefore lack of owner-level timing and the possibility of adding memory to an already tight pass. Recommendation 2 below makes that constraint explicit.

### Two real pathways, including their publication boundary

| Stage | Useful partial continuation: C3 | Escaping alternative: F4–F8 |
|---|---|---|
| Demand | Missing selected-prefix states and required publication-kernel siblings are batched through the existing refinement requests. | A selected or competitive alternative reaches an exact carrier not covered by the saved selected table/closed partition. |
| Descriptor/admission | Native candidate scope and runtime contracts still govern the requested rows. | Primitive legality is checked cheaply; nonprimitive descriptors conservatively retain vocabulary without eagerly constructing every option kernel. |
| Native kernel | Existing native row owners produce complete transitions and program semantics; servicing one sibling is not completion of the candidate. | A demanded descriptor invokes cooperative native certification. Legality, costs, observations, complete mass and termination are checked at the demanded row, not inferred from descriptor presence. |
| Selected continuation | The fixed candidate cursor preserves decisions; named missing work is serviced. C3 records 367 serviced continuations and one completed candidate. | Missing old ownership is not filled with invented old decisions. New native continuation work is performed, subject to the full candidate/publication contract. |
| Partition and obligation | The completed selected/publication walk must remain representable and eligible for normal validation. | Raw successors outside the current locator partition become a frontier. The pass returns to growth instead of certifying a truncated projected row. Source/target generations govern reuse. |
| Numerical/proof work | Existing properness and numerical/evaluation owners still determine whether the assembled candidate is publishable. | Persistent Bellman solving evaluates certified rows; unresolved actions retain sound floors. Compatible prepared lowers can strengthen those floors without certifying the alternatives. |
| Root publication | Independent evaluation qualified the empty-five upper improvement from roughly 16.998 million to **85,558.70619**, with the same **405.36940** lower. | No material four-goal root gain and no new four/five exact closure were recorded; F8 retained its prior verified artifact despite the optional proof failure. |

Sources: [C3 and F-series records][S8]; [`ResumableJointPolicyContinuation`][S5]; [`quotient_alternative_operator_admitted`][S7]; [`lift_policy_quotient_pass_task`, obligation accounting and frontier return][S6].

The deferred option-kernel repair is already present. Its source contract covers nonprimitive descriptors at that admission owner; it is not evidence that every other eligibility check throughout the engine is now cheap. I did not establish another specific eager-kernel defect in this bounded review. F6 also added a guarded physical-parent registration path, but its recorded long run made zero such registrations; focused branch checks do not establish an end-to-end benefit from that path. [S8]

## 4. Numerical machinery and heuristic consumers: keep authority separate from speed

### Four reuse contracts, not one cache

| Reuse type | Existing producer → consumer | Authority retained / necessary invalidation |
|---|---|---|
| Price-independent transitions | Compatible native kernel/template and sparse-row owners → broad solve and strict selected/alternative construction | Native state, pool, action, observation and layout compatibility. Price-independent probabilities do not imply reusable price-dependent admission or control. |
| Candidate/controller/properness | Captured selection/boundary snapshot and fixed decisions → candidate continuation and publication | Compatible exact entry, terminal/action/economy scope and unchanged fixed prefix. A newly added decision needs its own complete validation; properness is not inherited from one unrelated entry. |
| Partition/proof dependencies | Persistent raw evidence and cells → projected rows, ProofStore use-sites, Bellman graph | Source/target membership, generations, observation coverage, action/vocabulary and prices. Unchanged evidence can survive while changed projections are checked again. |
| Prepared lower/numerical initialization | Native retention/phase preparation → lower query and typed strict completion lookup | Current query/coordinates/boundaries and final complete checking. Geometry/capacities can be reused; current-value minimizing allocations cannot simply be replayed. |

These are current source/contract distinctions. They do not establish cross-base hit rates. [S1][S4][S5][S6][S12][S16]

### The numerical backends are not merely unused inventory

The shared sparse-policy layer supplies canonical row/observed-choice evaluation, deterministic tie rules, cooperative SCC discovery, and fixed-policy component solving. The component interface supports previous values and retained `SparsePolicyResume`, with WideFloat dense or resumed BiCGSTAB/Gauss–Seidel work. Its memory accounting explicitly includes overlap between retained resume state and fresh work vectors. The broad solver calls the shared SCC machinery; lower-query proposal generation calls the shared sparse evaluator. [Sparse interface/implementation][S16] [Broad caller][S21] [Lower-query consumer][S12]

The lower query also has live checking consumers, not just unused arithmetic helpers: `ExactMass` checks dyadic stored mass and `ExactDot` checks final represented inequalities, including observed choices and normalized-reference semantics. It validates source/target/price generations. **WideFloat policy evaluation, exact binary-coefficient checking, and a claim about underlying native mathematical coefficients are different levels of authority.** An external solver returning a small residual would not replace these contracts. [S12]

The September 7 numerical-reuse implementation already accepts compatible checked vectors or bounded **untrusted** current-coordinate initializers. It never adds “x must be at least the old lower” as a new constraint. Final checking and native value-dependent minimization remain. Its matched compact pair reduced preparation from **43.1828114 s to 31.3143940 s (27.48%)**, numerical solve time from **19.1891742 s to 6.8069396 s**, and increased peak by **102,176 bytes**. The development pair still missed its chosen verified-upper target. This supports retaining scoped reuse, not extrapolating its old stage fraction to today's F7/F8 workloads or enabling it everywhere. [Numerical-reuse archive][S15]

### F7 is a cheap source floor, not the missing successor expectation

The current strict consumer computes

\[
b_C=\min_{s\in C}h(s),\qquad
\ell(C,a)=\max\{c^-(C,a),b_C\}.
\]

With complete member coverage and the same target/action/policy scope, `h(s) <= V*(s) <= Q*(s,a)` makes this sound. Unsupported members retain their valid fallback. Neither a representative value nor the maximum over members can replace the minimum. This argument and its actual consumer are now in the canonical lower chapter. [S6][S17]

It is **not** the action-specific quantity `c(s,a) + E[h(S')]`. F7's 74,015 strengthened obligations and 1.654 ms lookup cost established consumption but produced no retirements or root movement in the recorded four-goal run. Removing this cheap consumer is unsupported; claiming it already supplies stronger successor-conditioned reasoning is also unsupported. A future expectation consumer would still need complete native mass, compatible successor bounds and current-value allocation checks. No such redesign is recommended by this audit. [S8][S17]

### External numerical tools and native parallelism: no change recommended yet

A bounded comparison is possible: a frozen nonsymmetric fixed-policy system `(I-P_pi)x=c` could be handed to a conventional sparse solver as an **untrusted numerical proposal or independent comparison**, while native policy extraction, properness, full action coverage and final checking remain. Eigen's versioned 3.3.9 documentation lists BiCGSTAB/SparseLU under MPL2 and separates symbolic-pattern analysis from numerical factorization; changed coefficients do not justify reusing old numeric factors. This is a concrete possible consumer, not a recommendation to add an LP/SMT/model-checking dependency. [Eigen sparse-solvers reference][E1]

The adoption costs are real engineering questions: matching the existing scalar/coefficient interpretation, avoiding duplicate sparse matrices or direct-factorization fill-in, retaining cooperative cancellation and deterministic decisions, and qualifying the native and WASM builds. Current CMake explicitly disables FP contraction on the relevant GNU/Clang/WASM paths. A generic solver replacement does not remove the partition/native-kernel work that dominates the identified failure boundaries. Without an attributed current numeric bottleneck, a dependency change has no credible demonstrated net saving. [S16][S22]

**Process-level case throughput already has a boundary:** the corpus runner uses isolated case processes dispatched through a worker pool and explicit memory reservations. This is different from making one solve faster. Within a solve, frozen matrix operations or already-independent SCC work are plausible candidates, but active calculators, state interning, option synthesis, proof generations and shared memory ledgers are not automatically independent. A safe design would require immutable shared inputs, worker-private scratch, deterministic commit/reduction and an aggregate—not per-worker-only—memory bound. [S11][S16]

No within-solve Amdahl estimate is warranted from “178 seconds were strict”: strict is not a measured parallelizable fraction. The smallest discriminating observation, only during a selected future investigation, is an owner-level split for its dominant phase with matrix/component sizes, peak scratch and dependency width. Do not launch a broad cohort or speed up a loop merely to reach the same state cap sooner.

### Cross-family evidence and its limit

| Family / workload | Accessible evidence | What can and cannot be concluded |
|---|---|---|
| Conquest destructive-renewal/Fracture-retention, mixed four/five-goal product scope | Current C/F records, detailed case/economy/cap fields, strict timing and root outcomes | Strongest current evidence for the continuation/partition investigation. It is not representative of all bases or all supported actions. |
| Eldritch same-side annul/exalt forced-winner control | Historical completed-closure replay, matching cost/strategy identities, expansion and total timings | Proves that eligible cross-process graph reuse works in a materially different control. It does not profile today's hard mixed-goal strict workload. |
| Veiled observed-choice/deferred-option fixture | Current F5 record reports focused native checks and the 8.333333333333 expected-cost fixture | Evidence that deferred descriptors did not simply erase this observed-choice contract in the tested fixture. It is not a current non-Conquest performance benchmark or a broad pool result. |

Sources: [S8][S9][S19]. Only Conquest has the current hard-case stage evidence inspected here. The smallest missing cross-family observation would be one already-authorized non-Conquest case with the same owner timings/peak-memory fields and its actual action vocabulary—not a rerun of the cohort. Supported features need not be active in every case.

## 5. Research handoffs: useful mathematics did survive

### Handoff A — September 7 numerical reuse

**Input and disposition:** the original `poecraft_c774_review_package.zip` is retained under the numerical-reuse archive. Its receipt distinguishes incorporated findings, duplicate obligations, rejected lookup caching and a failed checked-only gate. The manifest/hash matching is the implementation session's recorded verification, not a ZIP verification repeated here. [S15]

**Argument and counterexample:** current-query feasibility is separate from prior native admissibility. A native lower of 10 at `s` can be invalid as an iterate in a newly truncated model imposing `x(s) <= 1`. Likewise, a minimizing allocation for values `(0,10)` can become wrong when values swap to `(10,0)`: the old allocation gives 6 while the correct minimum remains 4. The canonical chapter and CLM-0013/0014 preserve these arguments, not merely the recommendation “warm start.” [S17][S23]

**Implementation:** the pinned `solver_quotient_lower.cpp` checks current coordinates, request/scope, model and price generation, coefficient mode, finite/nonnegative values and exact terminal/boundary values before accepting an untrusted initializer. The complete final inequality check remains. The measured outcome, unchanged target attainment and 102,176-byte cost are retained. This is observable source adoption plus recorded measurement—not an assumption that the workflow was used successfully every time. [S12][S15]

A fresh remote reader can retrieve the canonical argument, current source, original archive link and measurement receipt without the old conversation. The archived binaries themselves are not present in this audit environment.

### Handoff B — September 9 useful-proof-time review

The imported `useful-proof-time-review.md` retains the original distinction between statewise heuristics, root lower, executable upper and ordering scores; its `(400,40) -> (800,40)` example explains why one stronger floor need not improve a complete minimum. It also distinguishes “start checking this candidate” from a real bounded-finish request, and old selected ownership from a newly constructed continuation. [Original review][S24]

Those ideas reached the canonical lower chapter, search/resumption chapter, research record and existing claim destinations. The native implementation has the diagnostic handoff, demanded beyond-snapshot rows, conservative descriptors and the typed whole-cell lower consumer. The living record preserves failed F3 variants, F4's eager-cap diagnosis, later fixes, unchanged F7 root and F8 memory stop. The implementation result therefore does **not** silently turn the original proposal into a claimed exact-closure success. [S6][S7][S8][S13][S17][S25]

The original imported review still contains old chat citation tokens/placeholders. Those are not portable evidence links. Nevertheless, the current canonical pages and living record provide the necessary pinned arguments and result references. The comparison JSON includes actual case, economy, action and cap information; provenance identifies source/executable/report and archive hashes. A reader does not need the old thread to recover the main conclusion and failure. [S19][S24][S27]

### What the existing exporter does—and does not do

`solver_knowledge.export_context` binds the export to the actual committed revision, rejects selected or directly linked dirty/uncommitted sources, converts relative links to pinned GitHub links, preserves historical links, and refuses oversized output rather than truncating it. It includes selected claims' fields and dependency warnings. It does **not** inline the full arguments of every transitive dependency or verify network visibility; remote-ref presence is only a local observation. [S14]

A successful real redirected export is documented, including the Windows Unicode failure and its focused repair. That is stronger than an untested workflow description, but not evidence of routine use by every research chat. The minimum useful handoff is already close to the implemented workflow. [S15]

## 6. Three interventions, with bounded consumers and stop conditions

### 1 — Narrow deterministic strict-partition witness

**Type:** genuinely missing boundary-specific capability, using existing evidence infrastructure.

**Owner/consumer:** capture input to the specific `refine_closed_probabilistic_partition_replay` invocation inside `lift_policy_quotient_pass_task`; consume it in a small benchmark/test-side replay of that same owner. Existing `carrier_ladder_exact_boundary_v1` record/recover serves a selected-prefix diagnostic and is not a serialized live strict partition. Reuse its identity/evidence discipline and the existing attempt artifacts; do not create a second supervisor or database. [S6][S26]

**Minimum saved state:** ordered replay-node/locator semantics, terminal and immediate/observation keys, arc sources and complete transition payloads, the incoming partition seed, stable candidate/context identities, algorithm/build/input identities, limits, and the externally retained owner-byte baseline at the call. Capture ordered work position only when the chosen reproduction depends on it. For a logical memory-cap witness, state explicitly what is reproduced; it is not automatically an RSS/allocator-peak reproduction.

**Eliminated work / retained authority:** a developer studying this *same* partition input need not rediscover the upstream candidate and native selected closure. Replayed data remain diagnostic input, not a public lower, upper or exactness certificate. Changed semantic input invalidates the witness; a changed implementation can be compared against the same input only as a declared experiment, not by pretending the old binary identity still matches.

**Cost and memory:** a bounded serializer/driver plus identity checks is smaller than production strict checkpointing, but payload size can be substantial. Stream through existing artifact ownership rather than materializing another full copy during a memory-capped run. Full live recovery would additionally need active oracle/calc/admission state, proof/store generations, reverse dependencies, scheduler/frontier, numerical continuation, candidate lifecycle, compiler/evaluator state and portable compatibility—all outside this proposal.

**Strongest objection / cheapest check:** an evolving or once-only failure does not justify a new witness. First reconcile the current Codex code and name the exact unresolved owner. Capture only when that owner must be revisited anyway. Stop if the input cannot be bounded, the old path is gone, or no repeated investigation will consume it. No new run is requested by this report.

### 2 — One-pass partition metadata, not weaker proof reuse

**Type:** small source-level optimization candidate.

**Owner/consumer:** the `class_coverage`/`representative_for` construction in `solver_policy_refinement.cpp`, consumed by `build_cells`, row publication and source accounting. Build first representative and ordered membership ranges once per `class_by_node` generation. [S6]

**Eliminated work / retained authority:** repeated whole-population scans; retain identical representatives, coverage ranges, canonical identities, source/target generations, complete member checks and final proof behavior. No new cross-generation cache is required. Rebuild the index whenever that mapping changes.

**Cost and memory:** O(C) representative/index metadata, with ranges preferably written into already-needed coverage storage rather than copied into a second retained structure. Charge actual capacity/overlap. This is a contained data-flow change, not a quotient redesign.

**Earliest beneficiary:** a large completed partition pass reaching cell/row publication. **Strongest objection:** the F8 cap may occur earlier, and kernel construction or refinement may dominate wall time. The cheapest discriminating check is attribution at this block and a focused comparison of the old/new representatives, ordered coverage and proof identities on an existing partition witness. Stop if the block is immaterial or memory worsens. Do not describe it as a demonstrated root improvement or F8 cap fix.

### 3 — Existing exporter + one report + embedded delta

**Type:** use existing tooling; no new schema or mandatory packet.

From a separate committed checkout of the revision actually being handed off, with `tools/ingest` on `PYTHONPATH`, a selected export can use:

```text
python -m poecraft_ingest.solver_knowledge --root . context --claim CLM-0013 --claim CLM-0014 --claim CLM-0021 --max-chars 60000
```

Select the claims needed for the actual question rather than exporting the ledger by default. Include the one original report and its embedded delta, plus pinned references to the decisive source, exact input, comparison and provenance. Add only genuinely unavailable evidence needed for that decision. Codex's normal completion message can record dispositions and canonical destinations; no separate receipt form is necessary. [S13][S14]

**Benefit and cost:** avoids rediscovering arguments or mistaking superseded experimental conclusions for current contracts, at little additional machinery cost. No saved-hours estimate is supported. **Strongest objection:** an existing report may already contain everything necessary; then it needs no wrapper. **Stop condition:** do not make routine changes produce research paperwork, and do not treat traceability lint as mathematical or native acceptance.

## 7. Missing evidence that could change these recommendations

The decision-changing gaps are narrow: the exact F8 allocation/partition-call input and owner-level timing; one matched non-Conquest profile before generalizing performance or selecting a parallel/backend change; and actual local use frequency if deciding whether workflow adoption rather than implementation deserves priority. Source/binary compatibility must also be reconciled against Codex's newer work before an old witness is reused.

No absence claim is made about Oliver's ignored checkpoints, local Lab database, shell history, live Codex thread, or unpushed work. None was available to this audit. A missing current non-Conquest measurement is not evidence that its solver paths are unused or ineffective.

## Codex knowledge delta

These are advisory propositions to reconcile, **not new official claim IDs or automatic acceptances**.

| Proposition | Preconditions / argument to preserve | Evidence or counterexample | Canonical destination | Unresolved premise / intake disposition |
|---|---|---|---|---|
| Live continuation, completed-graph replay and job recovery are different capabilities. | Preserve the actual owner and eligibility boundary; native caller identity and harness compatibility are separate responsibilities. | Serializer restrictions; stepped API; corpus ledger; expansion-only Eldritch replay with slower total time. | `resources-resume-replay.md`; `search-and-resumption.md`; existing CLM-0021/0022 context, not a new theorem. | Whether a future investigation has a compatible completed graph. Incorporate terminology; do not claim live strict recovery. |
| A fixed candidate can survive unrelated value changes without acquiring new controller decisions. | Same semantic context and fixed prefix; prefix validation remains; resource refusal is not a retained yield. | `ResumableJointPolicyContinuation`; beyond-table parent 4741 and the distinction between C3/F-series outcomes. | `search-and-resumption.md`; CLM-0021; entry/properness context CLM-0002/0004. | Reconcile any newer candidate ownership changes. Preserve the argument and explicit refusal semantics. |
| Persistent physical rows do not justify stale quotient coverage. | Complete source/target classes, observations, prices and generations. | Source/target invalidation and valid use-site checks in the persistent strict pass. | `strict-closure.md`; CLM-0005/0006/0008. | Exact F8 allocation cause; one-pass class metadata remains an unmeasured proposal. |
| The strict prepared lower is a whole-cell source/action floor, not a successor expectation or a root-gain guarantee. | Minimum over every compatible member, including valid unsupported fallbacks; same target/action/policy scope. | F7 formula; 74,015 strengthened obligations, zero retirements, unchanged root. | `lower-bounds.md#coverage`; CLM-0006/0008/0011; research RQ-002. | Whether a newer workload turns these floors into useful retirements/root progress. No deletion or default-activation conclusion. |
| Numerical seeds may be reusable while probability minimizers are not. | Current identity/coordinates/boundaries; complete final inequality checking and final-value native minimization. | Truncated-model `10 > 1` counterexample; swapped values produce stale expectation 6 versus true minimum 4; September 7 matched reuse result. | `lower-bounds.md#initialization` and `#events`; CLM-0013/0014, retaining their actual statuses. | Current workload's numeric share; no general convergence or cross-base speedup claim. |
| Remote knowledge survival already works for selected handoffs, but export is not full offline proof closure. | Committed selected sources; follow pinned dependencies; keep original evidence and explicit dispositions. | Numerical-reuse receipt/actual export and September 9 imported review → canonical argument/source/outcome. | `research.md#handoff`; `solver_knowledge.py`; existing claims above. | External visibility of any newly exported local revision. Use existing report/delta workflow, without a new mandatory packet. |

[S1]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/resources-resume-replay.md
[S2]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/include/poecraft/solver.h#L615-L746
[S3]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_development_checkpoint.cpp#L520-L950
[S4]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_solve.cpp#L495-L710
[S5]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_joint_policy_continuation.hpp#L72-L430
[S6]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_policy_refinement.cpp#L2100-L4130
[S7]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_policy_oracle_setup.inc#L690-L800
[S8]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/README.md
[S9]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-08-27-solver-development-checkpoint-replay/result.md
[S10]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/foundation/solver-lab.md
[S11]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_corpus_runner.py#L571-L720
[S12]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_quotient_lower.cpp#L450-L640
[S13]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/research.md
[S14]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_knowledge.py#L226-L360
[S15]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-09-07-checked-numerical-reuse-v1/README.md
[S16]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_sparse_policy.hpp
[S17]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/mathematics/lower-bounds.md
[S18]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_calc.cpp
[S19]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/proof-time-comparison.json
[S20]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/benchmarks/solver_benchmark.cpp
[S21]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_solve_bellman.cpp
[S22]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/CMakeLists.txt#L87-L143
[S23]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/claims.md#clm-0013
[S24]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/research-inputs/useful-proof-time-review.md
[S25]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/mathematics/search-and-resumption.md
[S26]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/foundation/solver-internals.md
[E1]: https://libeigen.gitlab.io/eigen/docs-3.3/group__TopicSparseSystems.html
[S27]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/proof-time-provenance.json
[S28]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/proof-time-evidence.zip
