# Finder construction, resource ownership and complete-policy admission

Research and planning packet, 5 October 2026. Lane: Finder. No implementation or runtime experiment.

## 1. Decisions supported by this investigation

**Repair Finder's own cumulative work ownership before widening its candidate grammar.** The independent native default-product witness consumed 40 ordinary plus 48 automatic work units against cap 50, while reporting 40. That is a measured defect at production source `2ce0a75b4e98b2c5fff59be60f269df745bd63bc`, not a conjecture about a slow run. The Finder implementation has exactly the same Git blob in that review, released main and the observed causal checkout: `81265fce7a2ecb196af0e5200972cbcf26e4a617`. Relevant current source still omits automatic generation work from its counter and does not install an aggregate owner. No new native reproduction was run here. [E1, S1, S3]

**Account for committed generation work exactly once on capacity refusal.** The `std::length_error` catch in `generate_retention_candidate` destroys the producer and terminates Finder without calling its per-advance `charge()` closure. Work committed earlier in that same advance can therefore be omitted from Finder's reported cumulative work and timing; the underlying calculator telemetry is not erased. Conversely, if `charge()` commits its debit and then `update_peak()` throws, blindly calling the same closure in the catch can count the delta twice. These are separately verified source-derived failure-path obligations, not newly measured failures. Section 7.4 and cases A20/A21 specify bounded falsifiers. [S1]

**Keep complete graph checking, native programme admission and optimality separate.** Finder prepares the emitted graph against the original request, evaluates it from the original root, and validates native programmes at every positive reached entry before adoption. An authored evaluator can correctly report success for an authored success terminal that does not satisfy a separate Calculator goal. A checked root graph is not a uniform statewise continuation table, a global lower, or an all-action optimum. [S1, S4, S5, M1]

**Preserve the cheapest complete incumbent through failed, unfinished and more expensive candidates.** Finder already moves the winning graph buffer at candidate adoption and compares complete checked cost. Its API finalization copies the best bundle while the Finder remains live; this creates a distinct ownership/accounting audit item. Fixing adoption alone does not account for final export overlap. [S1, S2]

**Separate computational Finish from crafting termination and from capacity censoring.** Source increments `censored` when requested Finish interrupts an active checker, then sets `PC_SOLVE_CAP_OTHER` whenever `censored > 0`. Thus an interrupted checker can produce requested-Finish termination together with a cap bit despite no demonstrated resource refusal. This is a source-derived reporting consequence, not a newly executed witness. [S1, S2]

**Do not attribute Current's Conquest result to Finder's defect.** Matched Current control and treatment both return 101311.35474896732 with identical strategy bytes, success one, complete prices, zero off-policy mass and requested Finish/cap mask zero. The treatment made 33 upper requests, started none, and made no joint attempt. Finder accounting is an independent safety issue; these measurements do not connect it to that Current economic result. The conditional 179-check, three-case 13-to-3 service fixture remains conditional, not normal product activation or Conquest recovery. [E6]

Recommended next decision: select a narrow, source-pinned Finder resource/lifetime repair with the original default constructor falsifier and complete-tail finite cases. Grammar exploration and real quality comparisons should follow only after this engineering gate. This packet authorizes neither implementation nor runs.

## 2. Scope, evidence classes and provenance

Labels used throughout:

| Label | Meaning |
|---|---|
| Observed source | Direct reading of pinned native code or a named documentation owner |
| Measured receipt | A retained prior native result with its own source/build/request identity |
| Derived mathematics | An argument under explicit premises; toy values are synthetic |
| Source consequence | A conclusion about a code path; no claim of new runtime reproduction |
| Hypothesis | A plausible unresolved mechanism with a falsifier |
| Proposal | A bounded future change or experiment, not current capability |

At start, connected GitHub read confirmed remote main at **`7252027c80856628ed16734583bfc9d6e166458b`**. The normal checkout was **`7eb16ac3d63834fd5d3256ab42483f47d264b764`**. The complete pinned main object was locally readable. The eight-commit comparison from normal HEAD to remote main changed presentation, CI test lifecycle and documentation, not the Finder engine files examined here. Source readings use `git show` at full main SHA rather than treating the normal checkout as current.

The causal research branch remote head was independently read as **`23dcc5aea8828dc535466613479493bc379d4e7b`**. Its local HEAD at intake was **`fb59476f54601f81dc18a8bf2457d8eb7d3eb36b`**, with later local documentation changes and a new checked-graph-service evidence directory. Those are distinct revisions and ownership states. No file there was changed. No qualification is transferred from a similarly named source or another validator implementation.

The task has its own sparse documentation checkout based on pinned main. Only this report is intended for its research branch. Shared canonical documents, native code, data, prices and binaries are outside the mutation scope. Protected path `0` was not inspected or materialized. No trace dump, recursive archive read, build, test, solve, Simulator invocation, dependency installation, deployment or main merge occurred. A failed local clone was replaced with a sparse local object-reference checkout; that setup did not execute engine code.

Bootstrap read: `AGENTS.md`, `docs/solver/research-standards.md`, current `HANDOFF.md` and `current-status.md`, `research.md` sections 4/5, and `docs/_templates/solver-research-handoff.md`. No additional tracked nested AGENTS/SKILL instructions were found in the explicitly relevant engine/docs/.agents trees. This does not assert a recursive filesystem scan. The delegation mentions attached KIDS, but exposes no KIDS file identifier or payload; a title lookup did not resolve it. Its unseen contents are not claimed as evidence. The explicit delegated constraints were followed, and this access limitation is retained for parent reconciliation.

The four prior Pro reports are reused as research inputs, not independent corroborating experiments. Focused excerpts from candidate construction and complete validation were read against their primary cited source/receipts. Their imported README records byte-preserved materialization and Library identities. The present source tracing adds the actual current Finder/API correspondence, Finish classification and finalization overlap audit. Old broad reports' suggested 3.1 GiB attempt and run proposals are superseded by the present no-run instruction; they are not recycled allowances. [E7]

The gaming exclusions begin at 02:00 UTC on October 6 and again at 13:00 UTC. Local research reads in this task began at 20:32 UTC on October 5, outside those windows. No heavy process was started.

## 3. Actual native Finder construction

### 3.1 Entry and separation from Current

`pc_solver_solve_begin` selects `PolicyFinderWork` for `PC_SOLVER_MODE_STRATEGY_FINDER`; Current instead constructs `SolveWork`. Finder does not create a hidden Current solve to obtain its answer. Its public progress and summary expose checked upper/policy availability and NaN lower, gap and residual fields. A bounded feasible status describes its best checked controller, not an exact result. [S2]

Finder receives the original concrete item, session, native `CalcContext`, prices and `SolveOptions`. It rejects unavailable selected Fossil dependencies instead of silently certifying a narrowed request. A root already satisfying the goal gets an ordinary guarded zero-operation candidate and still undergoes independent graph checking. Empty graph bytes or a missing session refuse preparation. [S1]

Its default grammar transformation is specific: Conditional plus CalculatorProductV1 plus eight attempts, with no explicit goal-progress-gating override, becomes ConditionalProtectedScour and enables product continuation proposals. A diagnostic Conditional request with 24 attempts does not inherit that exact default transformation. Attempt values are restricted to 8 or 24; protected Scour retains eight. This distinction belongs in every comparison identity. [S1]

### 3.2 Descriptor filtering and seeds

Finder excludes synthetic, companion-state and goal-disabled actions from ordinary ranked primitives. Every cost key must have a finite nonnegative price; all required keys contribute to the descriptor cost. Root legality qualifies a seed only at the root. It does not prove legality at later graph entries. [S1]

Satisfying Essence guarantees come from native satisfying masks, including tier requirements, rather than family membership or similar display text. Finder orders a guaranteed seed by price per guaranteed goal and preserves distinct guarantee masks within its initial seed beam, while retaining a Chaos seed. Ordinary goal roles are ordered by stable group/family/tier identity. Guaranteed progress features order proposals; they do not create a probability law or a cost upper.

The retained grammar includes primitive loops, two-stage renewal/recovery, Normal-root Alchemy/Scour reset, conditional repeated Annul, Scour/Alchemy fallback, native held-side continuation variants, Dominance eligibility/recovery and Foulborn acquisition/add/reset. These are generic native descriptor/control families, not dispatch by base name, historical graph or saved state ID. They are nevertheless a bounded grammar and do not enumerate every possible programme.

The initial ordinary seeds are bounded near four, but special families can add further complete proposals. The live frontier is bounded at 16 in relevant expansion loops, seen identities at 256 there, and the attempt ceiling counts considered candidates including compile refusals and censored checks. These are not interchangeable lifetime limits. Pending holes have no checkable graph; generated, served, checked, accepted and unserved counts mean different things. [S1, D1]

### 3.3 Ordering and proposal exposure

The heuristic sketch score is first price plus second price plus 0.01 times goal slots, with small renewal/recovery adjustments. It is neither expected crafting cost nor a lower bound. Native completion proposals can be reserved before ordinary frontier service. A completed or capacity-censored feedback parent can create a deeper structural opportunity; this does not accept its child. Deduplication compares complete structural/semantic sketch identities. The FNV graph digest is diagnostic; complete graph bytes remain identity authority. [S1]

Hypothesis: a good continuation may remain unserved because construction, queue order or earlier checks exhaust the budget. This is not established merely by observing expensive accepted policies. Its falsifier is an immutable generated/served/unserved trace under a matched grammar and resource envelope, showing the relevant complete candidate was served and fully rejected or was never constructible. Existing capture can supply this evidence; no new queue database is needed.

## 4. Complete candidate acceptance and authored boundaries

The native acceptance chain has separate gates:

1. Assemble an ordinary graph from a complete sketch. An unresolved `Hole` refuses compilation.
2. Parse the actual bytes. Reconstructed start item must equal the exact original start.
3. Bind every success ingress to the original native goal. A default success edge is rejected even if raw JSON decorates it with a goal condition, because defaults execute as unconditional fallback.
4. Bind every operation node to caller scope. Ordinary primitives must be selected; dependency steps need compiler-owned native-control provenance passed with byte-matching graph. JSON declarations cannot self-authorize a native programme.
5. Evaluate the complete emitted graph from the original root with pinned prices and declared checker limits.
6. For native programmes, obtain the full reached-entry census and independently rederive native admission at every strict-positive entry.
7. Adopt only after all required gates complete, and only when complete evaluated cost beats the previous eligible winner. [S1, S4, S6]

Preparation is explicitly not evaluation or certification. Source checks all operation nodes for scope, including graph-unreachable nodes; root-only properness alone therefore cannot waive an out-of-scope operation. Conversely, authored evaluation on a graph's own terms is not automatically subject to the Finder request-preparation contract.

`finder_evaluation_accepted` requires convergence, complete prices, finite nonnegative total expected cost, success at least `1 - 1e-9`, and failure, stop, action-not-applied, no-matching-edge and unresolved probabilities each at most `1e-9`. The evaluator independently checks mass conservation and uses its own convergence contract. These are the existing native numerical acceptance rules. They are not exact rational equality, not a new zero-error theorem, and not permission to remove small positive branches before evaluation. Continuation certificates have additional residual/status obligations; a root scalar does not replace them. [S1, S5, D2]

The evaluator constructs the reachable operation/item product with relevant offer/checkpoint context. Its observation universe comes from graph conditions. Executed terminal kinds determine success/failure/stop mass (`solver_eval.cpp`, lines 8997–9030). Derived target slots do not constitute a separate Calculator request. An authored graph routing directly to a success terminal can therefore have success mass one under authored semantics while being inadmissible as a Finder answer. This is expected separation of contracts. A consumer must not relabel authored success as request satisfaction without the request guard. [S5, D3]

A root-unreachable improper component does not invalidate a certificate for a root that reaches the true goal without it. It does invalidate a proposed certificate at that component's entry. Native graph-local provenance identifies an observable programme occurrence, not a correspondence between private and parent state classes. All-zero or missing continuation values remain unavailable. [M1]

For native programme entries, current main requires: census requested and complete enough to contain reached decisions, no refused census entries; entry available with `root_expected_visits > 0`; no active checkpoint/offer; exact physical rematerialization; trusted node occurrence; full selected operator identity; supported programme intent; legal supported almost-surely terminating kernel; eligibility; nonempty complete exits; exact expected resources; held-goal satisfaction at entry and every exit. A resource-deferred admission is not a passed entry. The cursor advances only after admission. [S6]

The source validator explicitly handles supported Eldritch, protected Scour and narrow temporary-attempt intents. It does not imply arbitrary Bestiary, persistent Lock, donor, inventory, recycling or external-entry support. Persistent Lock's emulator qualification did not activate adaptive exact Finder or authored evaluation. Recombination's separate evaluator/solver contracts must not be borrowed into the ordinary Finder owner. [D4]

## 5. Failure, retry and paid-tail obligations

| Family / boundary | Complete obligations | Invalid shortcut or failure interpretation |
|---|---|---|
| Already-goal root | Exact original root and goal guard, independently checked zero-operation graph | A raw success label is not request binding |
| Full renewal loop | Native full output law, correct rarity/blockers, true terminal after each attempt, every non-goal continuation | Goal family presence or a cheap descriptor is not clean success |
| Normal Alchemy retry | Alchemy payment; every miss's legal paid Scour/reset; exact reset state; new Alchemy payment | Repeating Alchemy on reached Rare rarity can be illegal |
| Conditional Annul | Uniform native removal law, all goal-loss and junk-removal outcomes, goal tested first, paid reacquisition on held loss | A held-goal router is not physical protection |
| Any-k clean target | Native required threshold, selected satisfying goals and clean extras predicate after every removal | `slots.size()` is not always the required count; all selected goals need not be retained |
| Scour/Alchemy fallback | Both selected primitives and both payments, after-fallback exact item/rarity, complete loop properness | Treating Scour as free or assuming it reaches the root despite locks/fractures |
| Protected Scour | Legal lock/setup, paid Scour, actual preserved union, required crafted cleanup and subsequent acquisition | Local preservation is not a whole-root proper policy |
| Eldritch growth / repair | Correct dominance/tier setup, exact side/capacity, paid add/removal, all lost-progress and reroll tails | A first primitive cost is not the complete programme cost |
| Temporary blocker attempt | Bench, Exalt, mandatory remove-all-crafts, exact resource quantities and complete admission at every reached occurrence | Stopping at an intermediate goal before required cleanup changes the word |
| Dominance | Native eligibility before applying, all loss outcomes, cleanup if crafted, Normal-only renewal's paid recovery | Root legality cannot establish later Dominance eligibility |
| Foulborn add | Correct acquisition rarity/capacity, exactly scoped add count, every post-add miss and paid reset/reacquisition | Foulborn law or additive progress is not automatically an optimal controller |
| Failed primitive / unmatched edge | Preserve action-not-applied / no-matching-edge mass and first failure; refuse executable Finder upper | Normalize the surviving success mass or erase the failed route |
| Failure/stop terminal | Report its actual absorbed probability separately from crafting success | A controlled stop is not requested goal attainment |
| Non-goal recurrent class | Preserve unresolved/improper status; zero cost does not prove success | A finite accumulated-cost estimate alone is not properness |
| Checker or entry cap | Censor unfinished certification, retain prior complete best, retain committed work | No policy yet is not evidence of a high-cost policy |
| Generation capacity exception | Preserve all work committed before `std::length_error`, classify capacity refusal, release dependencies safely, retain prior best | Destroying producer scratch must not omit the advance's debit or reset lifetime authority |
| Finish / Cancel | Release active validator before borrowed checker/census, preserve spent work; Finish returns only completed best, Cancel abandons work | Computational interruption does not mean the crafting controller stops there |

The compiler flattens `RunNativeProgram` into all mandatory primitive operation nodes linked by defaults before routing back to a goal test. This preserves operation order and prevents an invented control decision between mandatory setup and cleanup. `RunScourAlchemy` similarly contains both operations. A two-stage ordinary candidate checks the goal after each stage, while native word boundaries are deliberately different. Do not interchange these stopping times. [S4]

The 2026-10-02 finite Finder follow-up already demonstrates the clean two-goal Annul threshold issue: the legacy fixed-four guard had zero success in that fixture; the current required-count-plus-one guard removes the third unwanted affix. Held-loss outcomes pay reacquisition. Its exact toy checks and graph are useful semantic evidence, not present-law real economics. A native-law qualification hold in that record must remain attached to its reported compact cost. [E3]

## 6. Mathematical premises, proofs and counterexamples

### 6.1 Fixed-policy upper is entry scoped

Let the finite nonterminal states be complete semantic execution states, including controller phase and any required checkpoint/offer memory. Let `Q` be the full nonterminal transition matrix of the emitted fixed controller and `r` its finite immediate expected cost vector. If all reachable nonterminal states are transient and goal absorption is almost sure, then the Neumann series converges:

`J = sum(t >= 0, Q^t r) = (I-Q)^(-1) r`.

For the certified entry, the same allowed controller is one feasible policy, so its cost upper-bounds the optimum over compatible proper policies. This proves neither minimum over omitted actions nor universal entry properness. A root that immediately succeeds plus an unreachable zero-cost self-loop is the exact counterexample to promoting one root certificate to every entry. This is CLM-0002's existing conditional argument. Native coefficient and numerical acceptance remain separate. [M1, X1]

Root occupation is `d^T = e_root^T (I-Q)^(-1)`. Expected resource consumption is occupation times each complete resource reward, not a count of distinct visited items. Repeated visits can exceed one. A control node can represent many physical rows; node identity alone cannot authorize merging their continuation values.

### 6.2 Identical retry must pay reset and preserve identity

Suppose an attempt costs `a`, succeeds with probability `p > 0`, and every failure pays recovery `b` returning to the identical priced semantic entry, with the same future trial law and no other tails. Then:

`J = a + (1-p)(b+J)`, hence `J = [a+(1-p)b]/p`.

Synthetic exact example: `a=2`, `b=3`, `p=1/4` gives `J=17`; omitting recovery gives 8 and prices a different controller. If recovery reaches a different blocker, rarity, held tier or phase, the scalar equation is invalid; solve the complete multi-entry chain instead. Native Normal-root Alchemy/Scour tests must establish exact state equality, not only the same goal mask. No toy arithmetic here was executed as an engine measurement. [M1]

### 6.3 Proper local words do not imply proper global composition

For a mandatory word stopping at a legitimate decision boundary `tau`, let `g(s)` be its complete expected internal cost and `H(s,z)` its full exit law. Under almost-sure finite exit and integrable cost, `J(s)=g(s)+sum_z H(s,z)J(z)` by total expectation. Setup, repeated setup, cleanup, lost-goal recovery and observed-choice timing belong inside the appropriate word or continuation.

Two deterministic one-step words `s -> t` and `t -> s` each exit; alternating them never reaches a goal. Likewise a word with 0.9 success and 0.1 non-goal trap cannot normalize 0.9 to one. Both are existing CLM-0004 counterexamples. A programme admission proves its local semantic word; the complete flattened root evaluation proves the global selected controller. [M1]

### 6.4 Positive mass is an obligation, not a thresholded sample

Consider an exact synthetic native-shaped word with success probability `1-2^-40` and an unsupported entry of probability `2^-40`. Dropping the entry is a semantic change even though its mass is below Finder's numerical acceptance tolerance. The separate native-entry census/admission must visit it if its exact stored root occupancy is positive. Numerical tolerances for evaluating complete coefficients do not authorize deleting support.

Expected visits also do not certify feasibility. A tiny occupied entry with an illegal reset can be decisive; a commonly visited entry with a fully admitted tail can be safe. All entries need semantic identity and complete admission, while occupation is useful for ordering work. The existing original-root-check-then-all-entries sequence is appropriate; skipping entry validation when root mass appears good is not.

### 6.5 Root upper versus statewise values

A valid root cost `U` of a graph supplies no values at unrelated or heterogeneous states. For an intervention with complete local cost `g` and exit law `H`, one-use value `g+H V_old` requires compatible certified `V_old(z)` at every positive exit. Alternatively, construct the entire recurring controller and independently check it from the original root, retaining its exact old router/new recovery and phase. That route does not require inventing statewise scalars, but still requires every paid tail, properness, native scope and the full output artifact. [M1]

This distinction explains why the conditional service fixture can admit a root candidate while normal product calls still refuse unavailable statewise authority. It does not establish that a representation-independent extension in progress is correct, active or economically useful. That owner's source and qualification are outside this lane's changes. [E6]

### 6.6 Exact sharing and representation

An exact quotient must preserve enabled actions, terminal labels, observations, complete successor-class probabilities, resources and required control context for every member of each equivalence class. Goal masks, equal ranker features and similar stat text are insufficient. Below-tier members can carry different exclusions; identical current routes can differ after an additive action. Existing source sometimes selects physical identity, a qualified uniform-removal carrier or further closed probabilistic refinement. Those are proved domain gates, not a universal license to use features as states. [S5, D5]

A minimal counterexample has two items with the same satisfied goal mask but different blocker exclusions: a desired additive action has probability 1/2 on one and 0 on the other. Merging their rows fabricates a kernel. Another has equal side counts and goal status but a fracture/lock that changes Scour. Any future performance work must either refine the state or reject the quotient. This packet proposes no new representation implementation and does not interfere with the active eligibility-extension owner.

### 6.7 Properness and numerical endpoints

Zero-cost non-goal looping can have finite raw accumulated cost while violating proper goal-reaching policy semantics. The policy class must be explicit. The PRISM primary manual separately defines reachability reward as infinite when target reachability is below one; a raw total-reward property has different semantics. External tool labels cannot substitute for the native contract. [X1, X2]

For a proper finite chain, residual `r_hat=c+Q v_hat-v_hat` gives `J-v_hat=(I-Q)^(-1) r_hat`. Near recurrence, a small residual can imply a large value error. Reuse the repository's existing coefficient, properness, mass and residual authority; do not widen tolerance to rescue a candidate. Formal fixed-point certificate literature motivates checking witnesses but does not certify the native state/domain bridge automatically. [D2, X3]

## 7. Resource ownership audit

### 7.1 Measured generation defect and current correspondence

The independent R3 witness uses the actual default product constructor with no private budget owner. It steps the Finder until automatic admission is reached and overspend appears. It checks both actual work <= cap and reported work == actual; both fail, while cancellation/no-refund controls pass. The receipt pins request digest, binary hashes and four assertions. Actual 88 is not a hypothetical count reconstructed from rows. [E1, E2]

Current source has the corresponding mechanism: automatic-scope `CalcContext::consume_reforge_work` forwards to an owner only if present, updates its automatic ledger and returns before the ordinary local cap check. `generate_retention_candidate` observes only the ordinary ledger delta and clamps its addition to the remaining Finder counter. A late clamp cannot stop previously executed work. The public Finder construction path and class do not install a lasting aggregate work owner. A private Current attachment or successful helper test does not change that path. [S1, S2, S3]

### 7.2 Separate unowned entry validator

The historical R3 validator witness completed six positive entries and consumed 48 units under local cap one with no owner. An exhausted shared owner correctly refused and retained its already committed unit. At released main the validator creates a calculator and inherits `problem_.reforge_work_budget_owner()`, but does not itself establish the missing local work cap; Finder passes the full `limits_` into validation and later clamps the validator's measured work at release. This is a separate native cap-enforcement gap under unowned use. [E1, S6]

The private `040993ec9bb8109cc1970df91c8bc9ec7ef97744` implementation adds a local/chained budget owner and live memory ledger. Its focused receipt passes limits 1/47/48, exhausted/shared and stricter-local paths, discovery/memory refusal and suspended cleanup (347 checks). It is scoped repaired evidence at that private source. The examined causal local HEAD and main contain the simpler validator path; the private qualification must not be relabelled as current main or whole-Finder acceptance. Reconcile the intended owner/source explicitly before any future integration. [E4, E5]

### 7.3 Required work invariant

Proposal: preserve one cumulative authority `W`, initially zero, for the declared invocation. Before a native unit costing `delta`, require `delta <= B-W`; only if admitted does it execute and commit `W += delta`. Forward generation, nested automatic admission and entry validation through the existing native owner chain. Checker work may remain in its own native calculator with the genuine remainder and transfer consumed units exactly once. Never attach a work calculator to itself, form an owner cycle, attach a borrowed short-lived owner, double-charge already forwarded events, or reset `W` when scratch is freed.

Inductive proof: initial `0 <= W <= B`; an admitted unit preserves the inequality; a refused unit does not execute and leaves `W` unchanged. Destruction/cancellation cannot refund committed events. Thus every prefix stays within B. Reading telemetry at release or clamping it afterward does not establish the induction premise and is insufficient.

This is an invariant proposal using current owners, not a request for a second supervisor or resource database. Report actual ordinary, automatic and checker components plus cumulative authority distinctly; saturating a diagnostic is not proof that the total was bounded.

### 7.4 Generation capacity refusal skips the release debit

Direct source observation: `generate_retention_candidate` snapshots the ordinary telemetry ledger at lines 794-797. Its `charge()` closure at 798-810 transfers that delta, elapsed search time and a peak update. An incomplete advance calls the closure at 841; normal completion calls it at 871; the general `std::exception` catch calls it at 886. In contrast, the `std::length_error` catch at 872-880 sets the capacity status/refusal, increments censoring, resets `retention_producer_`, clears pending work and sets `done_`, with no `charge()` call. These are exact paths at pinned main, independently inspected after review feedback. [S1]

Conditional source consequence: if the producer commits ordinary native work during this call before a later capacity exception, that call's ordinary delta is absent from `counters_.logical_reforge_work`; its search time and explicit closure peak update are also skipped. Calculator telemetry may still retain those events. Automatic work is already omitted by the independent mechanism in section 7.1. A persistent external budget owner, if genuinely installed, could prevent a refund in its own authority despite the reporting omission; the missing closure alone does not prove that all ledgers refund or that the exception path was reached in an actual run. No receipt or new measurement is claimed for this path.

The closure itself can throw after partial accounting: it first increments work, then search time, then calls `update_peak()`. That function at 1061-1072 records live/peak memory and throws `std::length_error` when the selected memory limit is exceeded. A later peak/capacity exception enters the same generation catch. Calling the unchanged closure again there recomputes the same `after-before` delta against the unchanged snapshot; if at least that much counter headroom remains, the work is added a second time. The clamp can hide a duplicate at the cap rather than make it correct. A proposed repair must distinguish refusal before debit from failure after debit. This is a source-derived repair trap, not evidence that current code already double-charges on this path. [S1]

Required repair boundary: debit every actually committed event exactly once through the persistent authority, including an advance that later refuses capacity; reconcile diagnostic components on every exit, without charging a refused/unexecuted event. Separate a nonthrowing, idempotent debit commitment from fallible memory/telemetry sampling, or establish an equivalent explicit commit-state invariant using existing owners. If a debit is already committed, the catch must not repeat it. Any cleanup accounting guard must not throw while another exception unwinds. Capture any producer-owned live/peak information before destroying its scratch, with dependency-safe release. Scope-exit accounting or explicit path accounting is an implementation choice for a later authorized owner; inserting the existing closure alone would still omit automatic work and would not establish pre-execution enforcement. Preserve refusal provenance and the prior complete best. A20 forces refusal after positive work but before accounting; A21 forces failure during accounting after its debit, and both check exact lifetime totals through Finish/destruction and a hypothetical successor candidate.

### 7.5 Memory, borrow lifetime and handoff overlap

Finder counts the request calculator, economy, ranked/frontier/pending/seen structures, candidate receipts, parsed active graph, old best graph/control, producer and validator; it adds the live checker. This is a declared solver-owned estimate, not process RSS or a complete allocator census. It preserves one live checker, but generation and retained pending native cursors have separate ownership. [S1, D1]

The validator borrows the checker result census, problem, control and prices. Keep all owners alive until validation is released. Current Finder Finish and check catches release validation before checker; preserve that order. The default destructor and producer-reset paths merit an explicit cancellation/lifetime audit because state-local admission can hold price/cursor dependencies in the persistent problem. The independent witness cancels pending admission while Finder prices are still alive. This identifies a test boundary, not a demonstrated dangling read in the released product. [E2]

Two source-derived memory gaps deserve finite falsifiers:

* `pc_solver_solve_finish` copies `finder_work->best()` into `finder_result`, then serializes telemetry, and only afterward resets the Finder. The graph/control copy and temporary serialization coexist with retained Finder storage. The recorded peak uses Finder's prior peak and does not explicitly measure that whole finalization overlap. An allocation/cap failure at this point can leave summary filled before result commit; callers must honor the non-OK return. Source establishes the copy, not an observed cap overshoot. [S2]
* Final retained-memory stats add `finder_result.strategy_json.capacity()` but do not explicitly add its optional native control's vector payloads in that branch. The control is still retained for programme provenance. This is a source accounting omission candidate; native memory statistics and cap impact need a finite native fixture. [S2]

Existing `update_peak` adds current retained bytes to a checker's historical peak. That can conservatively combine owners that did not coexist at the historical instant; it is not an exact process peak. Conversely, current snapshots alone can miss compile/copy transients. Name live, conservative selected peak, reserved transient and process memory separately. Do not assert a memory saving merely from fewer states or free the checker while a borrowed census survives without its own owner. [D1, E7]

## 8. Negative evidence and dispositions

| Input / finding | Disposition and exact scope |
|---|---|
| Default Finder cap50 / actual88 / report40 | Incorporated measured defect at 2ce source; current source correspondence verified; no new main runtime test |
| Unowned validator cap1 / work48 | Incorporated distinct measured defect; main's unowned path remains unqualified |
| Private 347-check local-owner/live-ledger repair | Retain scoped repaired evidence at 040993ec; not equivalent to examined main/causal validator |
| Generation capacity refusal / fallible `charge()` | Verified source-derived omitted same-advance debit if refusal precedes charge; naive catch retry can double-charge after a committed debit; no new measured execution; A20/A21 remain proposed |
| Finish censor -> cap bit | Open source-derived reporting issue; finite execution needed |
| Finalization copy/control memory accounting | Open source-derived audit items; no measured overshoot or process-memory claim |
| N1 clean Essence/Annul state-cap censor | Construction/checker negative, not economic rejection; all seven captured graphs were served |
| N6 compact 51-state accepted graph versus physical 200k censor | Historical unchanged-graph result with native-law hold; not current-law speedup or fresh Finder portfolio result |
| Held-side Finder historical A3/A5 and selective A5 | Checked old-law improvements within Finder, still much more expensive than matched Current; no ranker/default promotion |
| Failed Exalt-fill at occupied 3/1 entry | Negative for those controller guards, not a prohibition on native Exalt or all fill policies |
| Current matched Conquest control/treatment tie | Scoped economic negative for selected treatment; both original-root slots spent; no retry here |
| Historical 85970.67 graph | Repriced historical artifact, not matched old-source run or automatic product-envelope admission |
| Private ~210k candidate | Inferior diagnostic/incomplete context; not a recovered checked current policy |
| 13 -> 3 tiny service fixture | Conditional active/open internal service acceptance; normal constructor inactive/closed; no Conquest or Finder quality result |
| New eligibility extension | In progress under another owner; no code edits or qualification transfer from this packet |

The current source architecture gives no basis to claim that more attempts, a ranker/model, cheap acquisition, a component-time speedup, a rebuilt WASM bundle or a generic helper will improve best complete policy by budget. Old censors are unknown policy costs. The narrow shared recurrence checks in E3 also were not an independent verification of native rolling law: physical and compact paths shared the recurrence. Preserve the native-law hold and projection negatives even when another layer passes. [E3, E7, D6]

## 9. Minimal falsification experiments and bounded milestones

All work below is **proposed for a later explicitly selected execution**, with source-pinned existing owners and serial native fixtures. This task spends zero runtime allowances. Do not execute the tests below here, reopen a 3.1 GiB run, or renew previous P/IC/MM/root slots.

| Milestone | Bounded deliverable | Gate / stop rule |
|---|---|---|
| M0: source selection | Pin intended integration source, compare exact Finder/validator blobs to main and private 040 repair, record actual default profile/grammar/options | Stop any claim of inherited qualification when implementation or request differs; preserve all originals |
| M1: cumulative owner repair | Use existing native work-owner lifecycle for actual Finder generation, checker transfer and entry validation; reconcile every exit including capacity exceptions exactly once before producer destruction | Original default cap50 witness must no longer execute beyond cap; reported aggregate must equal actual components; A20 preserves debit on refusal-before-charge and A21 rejects double debit on failure-during-charge through Finish/destruction; no cap/tolerance increase |
| M2: interruption and handoff | Separate Finish censor from resource refusal; account/move existing final bundle and programme control; dependency-ordered cancellation | Force every active phase, success/refusal/throw and retry; best bytes/cost remain intact; no dangling dependencies or hidden final copy peak |
| M3: complete-tail finite gate | Native original-root candidates for reset, Annul loss, protected cleanup and tiny positive unsupported entry; all-entry certificate coverage | Any omitted positive branch, illegal reset or incomplete word blocks dependent acceptance; do not weaken the failing assertion |
| M4: optional grammar cut | Only after M1–M3, choose one descriptor-derived missing continuation family with smallest exact witness and bounded proposal count | Generated/served/evaluated/native-admitted are separate; no base-specific dispatch or historical seed masquerading as generation |
| M5: actual consumers | If selected, source-matched native default Finder and matching real WASM/client worker Finish/Cancel/export and authored semantic controls | Functional acceptance is separate from economics, rendered review and Current lower/closure |
| M6: quality selection | Parent chooses frozen cohort and new aggregate budget; measure best complete root policy over time, no-policy intervals, censors, counts and maximum step | A component-only win or one special fixture cannot close representative quality; no fresh timed run under this packet |

Minimal causal first experiment: reuse E2's actual default Finder construction, native synthetic session, original request/price vector and cap 50. Stop at the first refused work unit, print owner path plus ordinary/automatic/checker committed work, then request Finish and destroy pending work without refund. Compare repaired versus unmodified source under the same semantics. This resolves one safety question without a real-data solve. M1 also needs the separate cap1/47/48 validator controls; a generation pass alone does not discharge entry work.

Avoid binding a hand-built service fixture to assumed constructor activation. E6 already exposed an inactive/closed outer guard before inner refusal. For every consumer test, record outer mode/profile/override/envelope conditions and whether the candidate actually reaches compilation/checking. An explicit internal active/open phase can qualify conditional service; label it as such rather than adding an unrelated action to force a product gate.

### Exact small adversarial cases

The numerical expected outcomes below are synthetic specifications, not completed checks.

| Case | Exact construction / expected result | Native consumer boundary |
|---|---|---|
| A1 generation cap | Original E2 default constructor cap50; actual committed work never exceeds50 and report equals components | Finder -> selective producer -> automatic admission |
| A2 validator cap | Original six-positive-entry model at cap1,47,48: first two refuse before overspend; cap48 completes48, all6 | Entry validator and aggregate owner |
| A3 shared remainder | Parent budget50 with40 already committed; child request11 refuses with committed parent<=50; cancel preserves40 or actual admitted remainder | Owner chain, partial steps, no refund |
| A4 exact retry payment | Native-shaped normalized trial p=1/4, attempt2, reset3 -> cost17, trials4, resets3 | Flattened reset/reacquisition graph and material totals |
| A5 illegal reset | Same goal mask on two entries, one fractured/locked or nonreset rarity; illegal reset mass must refuse | Root legality versus all-reached legality |
| A6 repeated Annul | Two desired goals plus one junk: each removal1/3; junk -> true clean goal, either goal loss -> paid reacquisition | Goal-first router, threshold3, loss tail |
| A7 any-k ordering | Any2-of3 clean goal; all3 selected goals must reach true goal before cleanup; two selected plus junk enters cleanup | Native threshold and terminal semantics |
| A8 below-tier blocker | Same family present below requested tier and another satisfying-tier entry; only latter satisfies goal; exclusions remain distinct | Satisfying masks, carrier equivalence |
| A9 mandatory cleanup | Bench/Exalt produces a goal-looking item before remove-all-crafts; require cleanup payment and native terminal after word | RunNativeProgram flattening |
| A10 tiny positive entry | Word exits to supported goal with1-2^-40 and unadmitted occurrence with2^-40; all-entry native admission refuses | Complete census independent of numerical root tolerance |
| A11 local proper/global bad | Deterministic words s->t and t->s, no goal: unresolved/improper, never accepted | Whole-controller properness |
| A12 authored semantics | Authored start->success terminal on non-goal item: evaluator follows authored success; Finder preparation refuses unguarded request ingress | Authored evaluation versus Calculator answer |
| A13 decorated default | Default edge carries raw goal condition but points to success from a non-goal item; compiler request guard refuses | Executed fallback semantics |
| A14 preserve best | First complete root policy cost10; later complete20, improper, unsupported, cap, and Finish variants: best remains same10 bytes | Adoption, error handling, final transfer |
| A15 Finish classification | Active checker, sufficient caps, requested Finish: requested-Finish stop with no capacity bit merely for interruption | `censored_finish` versus cap reason |
| A16 handoff memory | Native-program winner with large bounded graph/control; final transfer at exactly sufficient/one-byte-short declared allowance | Copy/move overlap, retained stats, failure atomicity |
| A17 price completeness | Required reset or setup key missing, negative, NaN or infinite -> no accepted upper; repeated identical cost keys retain multiplicity | Descriptor filters and evaluator resource pricing |
| A18 dependency authority | Same ordinary JSON with spoofed programme declaration versus matching native control; only actual trusted occurrence can authorize dependency | Compiler-owned scope, byte identity |
| A19 cancellation lifetime | Suspend admission after committed work, Finish/abandon/destroy/rebegin, ensure borrowed prices/census/control stay valid and counters never refund | Destructor, persistent calculator cursor, validator before checker |
| A20 mid-generation capacity refusal | Synthetic lifetime cap20 with3 already committed; in one producer advance, commit exactly3 ordinary and2 automatic native units, then force `std::length_error` at a later capacity boundary before completion. Exact total is8, remainder12, never3 or6. Finish/destruction preserve8; a hypothetical next candidate cannot recover the spent5. Refused/unexecuted work adds0; prior best bytes/cost survive | Actual `generate_retention_candidate` length-error catch872-880; persistent owner, component reconciliation, release order |
| A21 failure during accounting | Same cap20/pre-call total3/native ordinary3+automatic2 fixture as A20; reach accounting, commit the complete five-unit debit once, then deterministically fail `update_peak()` with `std::length_error`. Final total remains8 and remainder12, never11 or13; Finish/destruction and any catch recovery cannot repeat either component or timing interval | Fallible `charge()` at798-810 inside the generation try; capacity catch after debit commitment |

A20/A21 are proposed finite native refusal-injection specifications, not fabricated telemetry or executed cases. Use existing native event paths for the five committed units and deterministic test-only failure boundaries; select a fixture where both counters change in the same `advance` invocation, not an earlier invocation already charged. Each future receipt must expose the pre-call total3, ordinary delta3, automatic delta2, post-refusal total8 and unchanged total after cleanup. A20 fails before debit commitment; A21 fails after that commitment but inside the fallible peak update, with sufficient headroom to expose duplication rather than hide it in saturation. Match normal-completion, incomplete, non-capacity-exception and pre-first-unit refusal controls, and repeat A21 at both the incomplete-return and completed-generation closure call sites. Record elapsed timing once without asserting an invented exact wall-clock value. These controls distinguish missing release accounting, automatic-work omission and double charging. The cap remains20 throughout the invocation; a fresh lifetime owner is allowed only for a genuinely new declared invocation. Both cases are unrun and no native fault-injection code is added here.

Every fixture should use existing native synthetic builders and independent native outcome enumeration where feasible, rather than fabricating an acceptance flag or census JSON. A selected exact-rational reference may check a tiny finite model, but its exporter and native correspondence remain separate obligations. No new test harness framework is proposed.

## 10. Complexity and resource consequences

Let `A` be ranked descriptors, `G` retained complete sketches, `K` considered attempts, `N` reachable execution pairs, `E` stored transitions, and `V` positive programme entries. Descriptor ranking is ordinarily O(A log A), plus goal-mask construction. The finite frontier and attempt limits bound proposal retention/check count, not the size of one native transition distribution or the total number of underlying physical states. Candidate identity construction scales with control/programme payload; graph parsing and byte comparison scale with emitted bytes. Diagnostic capture compiles generated graphs even if unserved, consumes real wall time and memory, and changes exposure if activated. [S1]

Reachable graph discovery requires at least O(N+E) storage/work for explicit retained topology. SCC decomposition is linear in topology in the standard graph model, but native row generation, observation propagation, exact refinement and occupancy solves can dominate; no end-to-end linearity claim follows. A general dense solve for an n-state SCC would cost O(n^3) arithmetic/O(n^2) storage, but the actual evaluator uses specialized/contraction/sparse paths. This is a cautionary generic upper model, not a timing estimate for its implementation. Near-renewal components can amplify numerical and iteration costs. [S5, D2]

All-entry admission costs O(V) entry service at minimum plus native state-local programme construction and retained caches. V is a count of distinct physical decision entries, not expected visits or unique root policies. One graph may have a huge V even if its control JSON is small. Disabling validation to save work changes authority. Sharing exact immutable results can reduce repetition only when complete request/occurrence/entry/law/prices and semantic domains match; root cost or a mask is not a cache key.

Memory proposals must include parent retained request, old winner, active graph/parsed strategy, evaluator/result/census, validator/owner/cursor scratch, compiler/options copies, diagnostic capture and export. Moving graph bytes helps one boundary, but saving the checker does not save its census for free. Source accounting repairs establish safety/observability; they do not promise faster construction or better policy cost. Any quality experiment must retain no-policy time and maximum uninterrupted native work item, not just search/compile/check totals.

## 11. Current, Finder, authored and WASM applicability

| Surface | What applies | Qualification still required / no inferred authority |
|---|---|---|
| Native default Finder | Actual constructor owner gap, ordinary/nested generation, complete graph check, native-entry gate, best preservation | M1–M3 on actual default exposure; no lower/gap/optimality claim |
| Native diagnostic Finder | Same complete admission; grammar8/24, ranking, capture and private continuations bind distinct identity | Diagnostic passes do not establish default proposal exposure or economics |
| Native Current | Fixed-policy, complete tails and paid resource mathematics; existing parent selective-service owner | Finder repair does not repair Current proof/search; conditional service fixture remains separate; no statewise promotion |
| Authored evaluation | Native graph semantics, product state, complete costs/mass/properness for supplied graph | Request binding requires its own guard; no claim of optimizing alternatives; external entries need their own certificates |
| WASM Finder/worker | Same native engine mechanisms when the exact source is compiled and actually invoked | Matching module/source/runtime/economy, defaults, Finish/Cancel/transport, step responsiveness and memory; native-only receipt is insufficient |
| WASM authored Builder | Same graph semantic separation and handle/version lifetime | Current request identity, stale-result rejection, unsupported vocabulary, actual source/module checks |
| Current proof/lower | None gained from heuristic ranking or checked Finder graph | All-action coverage, lower provenance, strict closure and target profile remain their existing authorities |

A shared helper can be reached by several consumers without all consumers having the same candidate activation, resource owner, usable incumbent representation or retained best. An explicit checked-artifact handoff could be designed later, but shared construction/mode labels alone do not qualify a hybrid. No product activation is proposed in this report.

## 12. Canonical documentation integration map and ready text

No canonical edit is made here. Later coordinated integration should insert the following precise deltas into existing owners, retain this report once, and update statuses only after evidence changes.

| Canonical owner / exact section | Proposed delta | Evidence / condition |
|---|---|---|
| `docs/solver/resources-resume-replay.md`, "Candidate checker and native headroom"; `#measurement-boundary` | Actual default Finder cumulative-owner requirement; committed automatic work; exactly-once/no-refund debit reconciliation on every generation exit including refusal-before-charge and failure-during-charge; final export overlap; separate estimate/process peak | E1/E2 source correspondence; S1 lines798-810/872-880 source-derived obligations; M1/A20/A21 and M2 receipts needed before marking fixed |
| `docs/solver/publication.md`, “Compiler Contract”, “Evaluation Contract”, “Classification and returned evidence”, “Failure And Telemetry” | Prepared versus checked versus native-admitted; authored terminals versus request binding; Finish censor versus resource cause; atomic best transfer | S1/S2/S4/S5/S6; reporting fix still proposed |
| `docs/solver/mathematics/policies.md`, `#fixed-policy`, `#programs`, `#properness`, `#graph-local-boundaries` | Preserve entry-scoped upper and every strict-positive programme entry; fully recurring graph route without fabricated statewise scalars | Existing CLM0002–4 application, no new theorem/status promotion |
| `docs/solver/mathematics/numerical-closure.md`, “Four layers of numerical meaning”, `#bounded-quality` | Existing tolerances do not permit dropping support; engineering cap repair separate from quality; no-policy/censored periods | S1/S5, E1/E3/E6, future matched quality evidence |
| `docs/solver/request-action-scope.md`, “Candidate, dependency, and materialization roles”; “Ordinary product Foulborn/Finder continuation (October 2)” | All operation nodes bind scope; only compiler-owned matched programme dependencies authorize extra steps | S4; no relaxation for hand-built or historical graphs |
| `docs/solver/flow.md`, “Cooperative Native Solve”, “Exact Whole-Graph Evaluation”, “Failure And Ownership Checklist” | Actual peer owners and finalization overlap; authored terminal semantics; Finish versus crafting stop | S2/S5 |
| `docs/solver/current-status.md`, isolated solver/Finder rows | Link report and retain accounting negative as source-corresponding open; private repairs scoped by exact source | No main-source/runtime fix is claimed by this research |
| `docs/solver/research.md`, §4 disposition / §5 owner convention | Incorporate source audit and reuse originals; open new reporting/handoff hypotheses; reject recycled run allowances | This packet only, no second evidence database |
| `docs/solver/claims.md`, CLM0001–4 histories if a reviewed application lands | Add only scoped acceptance event after responsible review; preserve original premises/history | Do not claim universal native correspondence from finite fixtures |
| `HANDOFF.md`, selected programme pointer | Short recommendation, owned evidence/source, next gate, zero runs spent, no automatic selection | Parent decides coordinated integration and future execution |

Doc-ready resources paragraph:

> Finder's declared cumulative work envelope includes ordinary generation, nested automatic admission, complete checker work and every native programme-entry validation. An owner must refuse the next native unit before execution; release, cancellation, capacity exceptions and candidate replacement retain committed work. Every advance's committed debit must be reconciled exactly once, including a `std::length_error` raised after work but before producer destruction. If accounting commits its debit before a fallible peak update, exception recovery must not repeat that debit. Component telemetry and post-hoc clamps are not enforcement. The default constructor's cap50/actual88/reported40 negative remains open at the reviewed Finder implementation; the separate exception-path obligations are source-derived and await their finite falsifiers. A private validator or Current-service pass is not whole-Finder qualification.

Doc-ready publication paragraph:

> A Finder candidate is prepared, then evaluated from its original root, then admitted at every positive reached native programme entry before adoption. Ordinary authored evaluation follows executed terminal labels; it does not bind the graph to an independent Calculator goal. Request-scoped publication additionally binds the exact root, every success ingress, operations, trusted dependency occurrences, prices and complete programme scope. Root checking does not supply arbitrary-entry statewise values or optimality closure.

Doc-ready termination/handoff paragraph:

> Requested Finish censors incomplete candidate work while retaining the cheapest complete compatible policy. Censoring caused solely by Finish is distinct from a resource refusal and does not by itself establish a cap hit. Final graph/control/telemetry transfer must account for all concurrently retained copies or move a complete owned bundle with dependency-safe lifetime and failure atomicity. A filled summary is not a successful result commit when the API returns an error.

Doc-ready quality paragraph:

> Source-correct resource enforcement is an engineering gate, not measured economic recovery. Generated, served, independently checked, fully native-admitted and adopted candidates remain separate stages. Report best complete original-root cost by the declared time/resource budget, periods without a policy, interrupted/capped work and unserved proposals. Current's matched Conquest tie and Finder's independent ownership negative answer different causal questions.

## 13. Evidence and sources

All repository links below are pinned. Line numbers refer to inspected source at that pin; private historical receipts keep their original revision. Local-only later causal material is explicitly identified, not presented as accessible remote content. External statements use primary author publications or official documentation; mathematical proofs and synthetic cases above are this packet's derivations under the stated premises.

* **S1 - Finder source:** [solver_finder.cpp at main](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_finder.cpp), [header](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_finder.hpp). Preparation 58-113; acceptance 115-127; constructor 145-562; retained bytes 566-623; validation 626-673; generation work 794-888, especially [capacity-exception exit872-880](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_finder.cpp#L872); [fallible peak update1061-1072](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_finder.cpp#L1061); service/limits 1075-1257; checker debit/adoption/Finish 1260-1450.
* **S2 — API owner:** [solver_api.cpp](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_api.cpp). Progress/summary 1285–1342; begin 2379–2404; finish 2505–2525; abandon 2547–2560; export 2830 onward; retained stats 3070–3104.
* **S3 — Native work debit:** [solver_calc.cpp](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_calc.cpp#L2235), [owner API](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_calc_types.hpp#L965). Automatic bypass/forwarding 2235–2271; owner lifetime comments 981–988.
* **S4 — Compiler:** [solver_compile.cpp](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_compile.cpp), [contracts](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_compile_contracts.hpp). Success/default guard 62–113; all-node scope 116–147; ordinary sequence 246–331; control/flattened words 412–623.
* **S5 — Evaluator:** [solver_eval.cpp](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval.cpp), [types](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_types.hpp), [operation/condition resolver](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_resolve.cpp). Caps 1386–1392; closed-component attribution 6200 onward; continuation statuses 6813–6872; terminals 8997–9030; pricing 9298–9307; numerical finalization 9456–9488.
* **S6 — Native programmes:** [solver_selective_completion.cpp](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_selective_completion.cpp). Temporary-attempt resource predicate 43–73; generic product scope 102–129; construction 693–750; validator 753–963.
* **M1 — Existing mathematics:** [policies.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/policies.md), [claims CLM0001–4](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/claims.md#clm-0002). Arguments inspected: fixed policy, observed choices, complete programmes, paid retry, exact entries, graph-local boundaries and policy difference.
* **D1 — Resource contract:** [resources-resume-replay.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/resources-resume-replay.md#measurement-boundary).
* **D2 — Numerical/quality contract:** [numerical-closure.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/numerical-closure.md#bounded-quality).
* **D3 — Publication and authored scope:** [publication.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/publication.md), [flow.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/flow.md).
* **D4 — Current capability boundaries:** [current-status.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/current-status.md), [HANDOFF.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/HANDOFF.md).
* **D5 — Representation contract:** [representations.md](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/representations.md#unprotected-uniform-removal).
* **D6 — Prior quality/disposition:** [research.md §4/5](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/research.md#handoff), [Finder H0–H4](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-09-24-strategy-finder/README.md#h0h4--reviewed-finder-continuation).
* **E1 — Original measured defects:** [finite-r3-summary.json](https://github.com/OliverOrton/poecraft2/blob/4c9f078d95e2c0c837ab7a408c4846420bac94db/docs/active/2026-10-04-sol61-independent-review/finite-r3-summary.json). Production2ce, reviewf442, request `3dadb93fe71f6d49bd1db7ffad34f005ef039edf361e58f26e7d9804dc6ff9f9`; generation four assertions/two failures, validator three/one.
* **E2 — Actual falsifier:** [review-native-witness.cpp](https://github.com/OliverOrton/poecraft2/blob/4c9f078d95e2c0c837ab7a408c4846420bac94db/docs/active/2026-10-04-sol61-independent-review/review-native-witness.cpp#L126). Default Finder construction, per-step ordinary/automatic telemetry, no-owner assertion and no-refund cleanup.
* **E3 — Finder Essence and negative qualification:** [2026-10-02-finder-essence README](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-02-finder-essence/README.md), [qualification.json](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-02-finder-essence/qualification.json). README, not a fresh run, establishes the scoped historical N1/N6 account used here.
* **E4 — Private repaired implementation:** [040993ec validator](https://github.com/OliverOrton/poecraft2/blob/040993ec9bb8109cc1970df91c8bc9ec7ef97744/engine/src/solver_selective_completion.cpp#L1140). Local/chained work owner and live ledger are distinct from current main.
* **E5 — Private finite repair receipt:** [entry-observer-r2 exact-entry work/peak safety log](https://github.com/OliverOrton/poecraft2/blob/4c9f078d95e2c0c837ab7a408c4846420bac94db/docs/active/2026-10-04-sol61-independent-review/entry-observer-r2-exact-entry-work-observer-and-selected-peak-safety.log). 347 passing checks, six-entry complete cap48, 36 observer checkpoints, suspended cleanup and no-refund controls.
* **E6 — Later Current continuation:** The locally read committed `fb59476f54601f81dc18a8bf2457d8eb7d3eb36b` programme README and local `checks/root-treatment-20261005/summary.json` record implementation `5a74619339b5283a22074b79026ea3c0177cc0ca`, Benchmark SHA256 `57ae44df5212fd2becfef122329e96b8de9524160178574af18008a260bbd0f7`, matched strategy SHA256 `d65787d11ad895d5925ce9dd4d0aefde483e53bc25df3a80a6a9dfe7a0d7e053`. Observed remote [causal branch](https://github.com/OliverOrton/poecraft2/tree/23dcc5aea8828dc535466613479493bc379d4e7b) is earlier; later local receipts are not claimed published by this task. Delegation independently supplies the same latest measured scope.
* **E7 — Prior imported reports:** consumer-local `docs/active/2026-10-04-sol61-armour-recovery/research-inputs/README.md` records causal report Library `libfile_15b9cf673c388191853b867e5d40b75b`, economics `libfile_f41959d282d48191bd75f47cf145fc99`, complete validation `libfile_f0d21fa811788191acf82f3642e02c37`, construction audit `libfile_4f4809787f288191a027e3b5108526c9`. Relevant excerpts were reused; original reports were not reuploaded or duplicated on this lane's branch.
* **X1 — Primary SSP theory:** [Bertsekas and Yu, Stochastic Shortest Path Problems Under Weak Conditions, §1](https://web.mit.edu/dimitrib/www/SSP_Weak_Conditions.pdf). Used only for finite proper/improper policy distinction and conditional fixed-policy equations; no native performance or software authority.
* **X2 — Official reward semantics:** [PRISM Reward-based Properties](https://www.prismmodelchecker.org/manual/PropertySpecification/Reward-basedProperties), “Reachability reward” and “Total reward.” Used only for explicit nontermination/property contrast.
* **X3 — Primary certificate literature:** [Chatterjee et al., Fixed Point Certificates for Reachability and Expected Rewards in MDPs, TACAS2025](https://arxiv.org/abs/2501.11467). General witness/checker motivation only; no claim that its verified model bridge exists in poecraft2.

## 14. Delivery and remaining gates

Deliverable: this standalone report, preserved in task-local output and as the same bytes on its unique documentation-only research branch and owned Library report. The independently requested correction updates only that branch and replaces the same Library item, preserving version lineage rather than creating a duplicate. It adds the verified generation capacity-exception exit and fallible-accounting obligations and unrun A20/A21 falsifiers; no implementation or runtime qualification is added. Publication receipt, exact remote branch SHA, local byte/hash identity, start/delivery main checks and Library identity belong in the task's final outcome. No shared canonical document changes or engine/product artifacts are included.

Completed here: source/contract/receipt investigation, explicit mathematical premises and counterexamples, source-derived open issues, bounded future milestones, exact adversarial specifications, applicability and canonical integration proposals. Runtime qualification, economic improvement, main integration and matching new WASM remain unrun. The missing separately exposed KIDS payload is an intake limitation; the parent can reconcile that file without interpreting this report as having read it.

Prepublication remote-main recheck again confirmed `7252027c80856628ed16734583bfc9d6e166458b`. The final task receipt will also verify branch publication and recheck main after publication. Neither a research commit nor Library retention changes any qualification above.

Next two recommended actions: reconcile the exact intended Finder/validator source with the private040 repair, then select M1/M2's finite actual-consumer safety gate under a new explicit parent execution grant. No next programme, implementation or allowance is implicitly selected.
