# poecraft2 structural audit: make safe changes local again

**Decision:** schedule a bounded structural-consolidation programme after the current Codex programme reaches an explicit terminal handoff. Do not redirect its active work. Do not defer all structural work until another solver feature has enlarged the same boundaries. First restore the trustworthy change/validation loop; then finish one existing ownership migration. Broader solver, cache, header and compiler changes follow only as separately justified slices.

**Reviewed main:** `4594e44b6b851511ae5471e52f232bce57d16856`. **Status:** read-only audit and future plan, not an implemented change. References Rxx/Px resolve in SOURCE_INDEX.md. Claude's uploaded structure report includes uncommitted changes; its approximate size/churn figures are not measurements of this exact snapshot.

## Executive assessment

The concern is justified, but the problem is more specific than “the repository is too large for an LLM.” The current code requires too many local changes to be reasoned about as changes to a global state machine. Some intended owners are real typed interfaces; others are named containers with public mutable aliases and compatibility paths still bypassing the owner. Tools and tests then impose a second nonlocal burden: obtaining the right environment, collector, artifact, mode, clock, comparison and historical context.

This combination makes the cost of *understanding and validating a change* increase faster than the size of the change. Large functions and headers amplify that cost, but are not its sole cause. More file splitting or more agent instructions cannot finish an ownership migration.

There are concrete symptoms, not just style preferences: hosted validation failures on the reviewed main; tests whose meaning depends on metadata list position; an exact raw-hash failure reproduced solely by newline conversion; deliberate legacy aliases around the portfolio; an action ledger explicitly not yet authoritative for the scheduler; a service importing its case loader from a runner; an authoring-only template required during general service construction; multiple native/WASM/test pathways with different prerequisites and build settings; and a toolchain exception for one large coroutine translation unit. [R06–R24]

The correct response is neither a rewrite nor indefinite caution. It is to pay down the debt that most increases regression risk and research turnaround, with a clean baseline and one fully closed boundary at a time. The fixed-point equations, factored native rows, properness checks, strict proof sessions, actual graph evaluator, shared worker/supervision and mathematical record remain valuable assets. [R03–R07, R23–R30, R35–R36]

## 1. Scope and confidence

This review follows important paths across core solver ownership, candidate/publication lifecycle, proof and observation contracts, Python runner/Lab/reporting, native/WASM build, hosted CI, test selection, byte identity and canonical documentation. Source excerpts and actual hosted run logs were read. One exact byte-level reproduction was executed in a scratch directory.

It is not a whole-repository AST census, CPU profile, full security review or fresh solver qualification. The most recent local Codex edits were unavailable. The post-programme first step must classify each finding as still present, already repaired, superseded or requiring a small reproducer. The user is explicitly not dispatching this now.

Evidence categories used below:
- **Source-confirmed:** inspected code/configuration establishes the stated structure.
- **Recorded execution:** a committed receipt or hosted job log records an outcome; not reexecuted here.
- **Reproduced locally:** the particular byte check included in this package.
- **Inference/proposal:** a causal interpretation or architectural change whose benefit still needs qualification.

The report's recommendations are engineering judgments. It makes no quantitative prediction of future LLM success, compile speed or native solver gain.

## 2. The most immediate debt: the change-validation loop is not reliable from a clean checkout

### D01 — The reviewed main's Windows CI does not reach native/web qualification

The push run `34929659638`, job `104255050536`, builds successfully, then fails in the Python portion of Test. Its log records 35 unittest tests, four failures and twelve errors. Errors include missing pytest imports and missing `data/compiled/current/manifest.json`. The Python selected by setup-python is not the interpreter actually used by `py -3`; the log shows the latter resolving Python 3.14.7 despite a 3.12 setup. These are recorded hosted outcomes, not a claim that the native engine failed its local qualification. [R11–R14, evidence/ci-observations.json]

More importantly, installing pytest alone is incomplete. The wrapper uses unittest discovery, while many repository tests are top-level pytest functions or use pytest fixtures. Import success does not mean those functions will be collected by unittest. A runner must demonstrate actual collection of the intended test styles. Pytest can collect conventional unittest cases too, but compatibility exceptions and expensive integration tests should be audited before blindly changing the global collector. [R11, R20, P5]

The wrapper also prepares compiled game data after the initial Python suite that contains data-dependent tests. A development machine can hide this ordering error with a previously generated artifact. The clean-checkout path exposes it. [R11, R18]

**Repair:** one explicitly resolved Python executable, declared test dependencies, tested collection, and named unit/artifact/native/worker lanes with prerequisites prepared before consumers. Reuse scripts/test.ps1 and existing workflows rather than introduce a new orchestration layer. A selected required lane must fail clearly when unavailable; optional local lanes may remain optional, but their skip is not a pass.

### D02 — A specific archived hash failure is byte portability, not changed strategy semantics

The S8 tests hash raw bytes. I fetched the exact base64 Git blob of `examples/evaluator-straight.strategy.json`, verified its Git blob SHA, and obtained the expected SHA-256 `71bd8aea...`. Replacing its 35 LF line endings with CRLF produces exactly the Windows failing hash `fd46a2e6...`. The parsed JSON objects are equal. This reproduces one observed failure without changing any solver or strategy semantics. It does not prove all four hash failures have the same cause. [R18–R19, scripts/check_checkout_bytes.py]

**Repair:** declare path-appropriate checkout byte rules for hash-bound fixtures and raw evidence. Preserve raw identity and semantic identity as different contracts. Do not recompute historical expected hashes merely to accommodate the machine, globally normalize every report, or bulk-renormalize archives. Inspect which paths contain canonical LF text versus immutable recorded bytes before choosing attributes. The root `.gitattributes` read returned Not Found at the pin; this does not rule out nested attributes or local Git configuration. Git's documented EOL rules explain the reproduction. [P4]

### D03 — The knowledge pipeline fails for a positional assumption, not a rejected theorem

The knowledge push run `34929659641`, job `104255050860`, reports zero knowledge-lint errors and expected open-claim warnings; its eight knowledge unit tests pass. A later reporter test fails with `KeyError: sources` at `report['observations'][0]['sources'][0]`.

The metadata's first observation is now the conditional `out-of-snapshot-continuation-failures-20260909` record. The `support` observation follows it. The test names the intended support semantics but addresses list position zero. This is a concrete schema/identity drift: adding a valid research observation broke a test that accidentally treated ordering as identity. It is not evidence that a mathematical claim is false, nor that the support archive is missing. Later generated-view checks in that shell block were not reached. [R15–R17]

**Repair:** address observations by stable IDs and discriminate their declared kinds. Test ordering changes, supported/conditional/unavailable observations and missing evidence explicitly. Separate hermetic report-shape tests from actual archived-evidence integration. Do not fix this by reordering history or filling in fabricated `sources` fields.

### Why these three findings precede invasive refactoring

A structural change often returns the same successful graph while breaking an exceptional lifetime or portable evidence path. If its validation route cannot run consistently, the implementer compensates with manual commands and longer instructions. That makes the next session even more dependent on local knowledge. Repairing this loop improves development confidence without changing solver mathematics or launching another expensive cost-search campaign.

## 3. Core structural debt: named ownership is not yet enforced ownership

### D04 — The portfolio refactor is visibly unfinished

`IncumbentPortfolio` exists and owns output, pending and retained candidate storage. The implementation then exposes mutable aliases in `SolveWork::Impl` back to those containers. The accompanying comment explicitly calls this a behaviour-neutral compatibility stage while later work migrates callers to portfolio methods. [R06]

This is stronger evidence than a large-method count. The source has moved fields into an owner but has not yet made that owner the exclusive place where transitions are validated. An agent making an apparently local update must still understand who else can mutate the same candidate, flags, graph, values and history.

**Target:** complete one existing lifecycle, preferably verified-artifact adoption, best-selection and observation. The owner should admit a complete checked graph/certificate/context/value combination, preserve the existing tie and replacement rules, expose read-only information, and make retirement/pruning explicit. Do not introduce another portfolio or a new “manager” while keeping every mutable escape hatch alive. Unknown/unverified candidates remain legitimate separate stages, not forcibly converted to verified types.

**Gate:** all callers in the selected lifecycle use the owner, the bypass they replaced is gone or explicitly unavailable for that lifecycle, and an invalid/stale artifact cannot be made available by setting a Boolean. If that cannot be completed safely in the selected slice, leave a precise boundary and do not claim encapsulation was achieved.

### D05 — A typed action ledger and a legacy scheduling view coexist

The source records that the typed ledger is currently an observational lifecycle while the proven completed-pair scheduler view remains active pending wider qualification. The conceptual source map describes the ledger as the owner. Both statements can be reconciled, but only if the implementation status is explicit. [R06, R29]

This is not an invitation to flip the new ledger into authority as cleanup. That changes search scheduling and may repeat known regressions. **Structural maintenance should document effective ownership and prevent new dual writes; selecting a different scheduler consumer remains an algorithm treatment.**

The general debt is incomplete migration with no narrowly scoped completion criterion, not the existence of a deliberately retained fallback. Keep migration status near the owner and test. Do not create a separate global migration database.

### D06 — Candidate states require too much nonlocal reasoning

The retained incumbent record carries graph, root, numerical values, materialization status, properness, independent verification, executable status, root-only status, provenance and generation metadata. Several combinations are necessarily meaningful during construction; others must never be published. Today many of those distinctions are represented by fields that can be manipulated together outside their final issuer. [R06]

The remedy is not one giant enum that loses information. Use the existing unverified-candidate and compiled-artifact boundaries, narrow creation/transition routines, and immutable views of accepted evidence. Only add a type when it makes an actual invalid operation unavailable or a required dependency explicit. Avoid virtual dispatch or copy-heavy wrappers in hot loops.

A user-visible cost cannot outlive the certificate and graph supporting it. A root-only controller cannot acquire parent-state authority because another field is finite. Refactoring this invariant is a valuable safety and context-locality gain even if expected Chaos does not change. [R35–R36]

### D07 — Observation and mutation are too easily confused

The current progress implementation deliberately avoids an existing service accessor because that accessor can prune storage; it then reconstructs an observational choice from retained data. This is a telling structural pressure: a “read” and an “act on candidates” path are not cleanly separated, so reporting risks reimplementing selection logic. It is not proof that the displayed selection is currently wrong.

**Target:** a pure, owner-provided view of availability and selected artifact, plus explicit mutation operations. Keep emission bounded and outside decision authority. A snapshot need not copy the whole graph or scan all states. Separate native-source event time from host observation time and active computation from coroutine lifetime. The previous phase-write and persistence regressions demonstrate that diagnostics can change behaviour if this boundary is not respected. [R03, R06–R07, R37, R39]

### D08 — The large coordinator is costly because it owns unrelated lifetimes

The user-provided report estimates roughly 260 methods and long-lived state spread across many files. Those approximate counts include local changes. Independently of the exact numbers, the inspected `Impl` exposes search arrays, numerical modes, candidate lifetimes, publication, strict proof, private work and diagnostics through a common mutable context. [R06; user report]

A function moved into another file still reaches the same shared state. Real decomposition gives each selected stage a small input contract, its own staged storage, a complete output, a commit point and a release rule. Existing lower/order types already distinguish authorities; extend that style where an actual misuse is prevented. Do not turn every phase into an object that holds an unrestricted `Impl&`, because that merely renames the same coupling.

## 4. Tooling architecture needs attention as much as C++

### D09 — General Lab operations depend on authoring-specific setup

The inspected service imports its case-task loader from `solver_corpus_runner`, and its constructor eagerly looks up a particular Conquest authoring template. The documented CLI accepts a corpus override and includes operations unrelated to authoring. A corpus lacking that template therefore exposes a hidden construction prerequisite unless another supported layer guarantees it. [R21, R28]

This is tooling coupling, not evidence of base-hardcoded solver logic. The initial repair should be a small custom-corpus/lifecycle fixture. If general listing/running should work without authoring, move template acquisition to the explicit authoring operation or give it a separately declared required input. Put shared immutable case resolution beneath both service and runner; retain command compatibility, not an extra loader above them.

### D10 — Large Python modules contain several reasons to change

The repository metadata reports approximately 114 KB for `solver_lab_service.py` and 117 KB for `runtime_report_parity.py`. These are source-byte measurements, not complexity or CPU scores. Their inspected roles nevertheless span case resolution, orchestration and evidence operations on one side, and multiple typed comparison/normalization families on the other. [R21–R26]

Splitting by CLI command alone is not necessarily useful. Prefer stable boundaries: resolve an immutable request; execute/supervise it; publish an immutable attempt; read/project its evidence. The shared worker already handles command identity, paths, resource reservations and process concerns. Preserve it rather than construct a third pipeline. The Lab's persistent catalogue and the runner's isolated job path have different operational roles; being different is not automatically duplication. [R23, R28]

### D11 — Comparison meanings must remain separate, but their implementation should be explicit

Raw artifact equality, same crafting target, same experiment treatment, same resource profile, ordinary solver-behaviour parity and descriptive report similarity are different relations. Current tools correctly contain several of these distinctions, including strict missing-value checks and explicit mode identities. They also rely on manually maintained nested dictionaries, field lists and name-based projection rules. [R22–R26]

The danger is not that there are multiple comparators. It is that a caller can use a convenient projection as if it established a stronger relation. A “universal normalized JSON” function would make this worse.

**Target:** named relation-specific functions with typed/validated inputs and explicit mismatch explanations. Preserve original field availability and schema versions. Unknown is not zero; malformed input is not valid empty data; a diagnostic omission is not a removed native action. Use hermetic fixtures for partial reports and archived evidence for integration. Do not add a parallel evidence database.

### D12 — Source, binary and browser qualification have distinct identities

The worker already hashes executables, artifacts, requests and machine configuration. This is a strong foundation. Source provenance based on commit and dirty paths, however, is not enough by itself to reconstruct arbitrary local source bytes. Some programme build receipts add the missing hashes, but that correspondence is not uniformly guaranteed by every convenience route. [R23]

Likewise, a fresh native build does not produce a fresh WASM build, and a web test against an existing committed WASM module does not qualify newly edited C++. The existing change-impact doc says this correctly; the build/test wrappers should make the selected qualification's actual identities and unavailable layers unavoidable in their result. [R08–R12, R30, R38]

Do not require a WASM rebuild for every Python reporter edit. Require source-matched evidence when claiming browser-visible native behaviour, and distinguish an intentionally reused compatible binary from a newly measured one.

## 5. Build and code-layout debt

### D13 — The compiler workaround is a real constraint, not a diagnosis by line count

The build isolates `solver_solve_finish.cpp` at O1 without LTO and records optimizer/lowering failures. The rest of the main WASM build uses O3/LTO. This is source-confirmed; no compiler failure was reproduced in this audit. [R08]

A coherent coroutine-stage extraction may reduce compile pressure or improve lifetime clarity, but a function-size threshold does not prove that. Preserve the existing compiler settings first while qualifying a structural change. Test stronger optimization/toolchain changes separately and retain a reproducible failure command when available. Do not remove the fallback because one run compiled successfully. [P6]

The alternate CMake WASM target has different settings from the PowerShell release route. Decide which route is supported and make intentional differences explicit; do not silently claim both qualify identical release artefacts. Share the declared source inventory and relevant build settings rather than create another wrapper.

### D14 — Header fan-out and private definitions can raise build/context cost

The user report points to large anonymous-namespace headers with multiple includers. This implies repeated front-end processing; it does not measure final duplicated binary code. Some hot code benefits from visible definitions, and some local statics/identities change meaning when definitions are centralized. [R09, R29, user report]

Measure the selected include closure and one incremental edit first. Move cold non-template definitions with narrow declarations when it removes real fan-out. Preserve templates/small hot helpers where useful. Use compiler remarks and actual build settings instead of assuming same-TU extraction is free or cross-TU extraction is always slow. Do not reorganize the documented single-TU oracle and evaluator lifecycle merely to reach a file-size target.

### D15 — Utilities should retire duplication, not precede every task

A small saturation helper is a good candidate for touched callers; the currently cited helper lives in a heavy refinement header and should not become a universal include. Preserve overflow versus refusal semantics and integer/sentinel meaning.

A bounded JSON writer may improve a cold telemetry section, but key order, integer strings, finite/null values, escaping and output-cap semantics matter. Do not migrate executable graph identity serialization in the same initial change. A timer spanning suspension measures lifetime, not active work. A child-task wrapper can add a frame and alter cancellation, exceptions and accounting; it is not just replacing boilerplate. [R07, R31–R34, R37]

Success is deleting the replaced special-case implementation and making its semantics testable. A helper used only once, with more configuration and dependencies than the old code, may not be an improvement. No all-utility migration is selected.

## 6. Mathematics, diagnostics and agent-facing knowledge

### D16 — Canonical mathematics is an asset; correspondence and comments need maintenance

The mathematical chapters already preserve fixed-policy equations, properness, complete outcome requirements, observation timing, stopped laws, recurrent policy differences and numerical qualifications. The change-impact map has sensible ownership and proportional-validation rules. Do not replace these with another framework or drown them in new generated summaries. [R30, R35–R36]

But a low-level types header still presents a legacy S4 narrative in which restart yields finite upper initialization. That wording conflicts with the modern explicit distinction between working estimates, ceilings and independently executable uppers. This is a source-comment finding, not a newly demonstrated runtime numerical bug. An LLM can be misled by an authoritative-looking local comment even when the correct argument exists elsewhere. [R06, R35]

The ownership map similarly needs to distinguish intended ledger authority from the documented retained legacy scheduler consumer. A clean claim-reference lint cannot establish this correspondence; the actual hosted lint passed while later reporter tests failed. Fix touched semantic comments, stable links and actual ownership descriptions. Do not expand a universal startup reading list.

### D17 — Diagnostics/reference implementations need a retirement test, not a purge

Names such as shadow, audit or legacy are insufficient evidence of dead code. Some support differential tests, proof-discrepancy diagnosis, reproducibility or logical-work baselines. Others may be obsolete experiments still compiled into every build or stored in the large coordinator. [R03, R29, user report]

Before removal, identify product/benchmark activation, callers, build inventory, tests, evidence reproduction and distinct truth it provides. If genuinely unnecessary, remove live code and update affected inventories while preserving the historical receipt and rejection reason. If a reference is useful only in tests, isolate it deliberately. Do not keep unreachable production code just because an archive once mentioned it, but do not treat source inactivity as proof of no consumer.

### D18 — Long instructions are compensating for weak boundaries

The previous implementation requests needed extensive lists of forbidden transfers, protected scopes, old failed paths and exact profile details. These requirements are substantive, but a future feature should not need to reread every failure to avoid recreating it. The relevant rule should become a narrow API, a negative fixture or an executable command contract, linked to the mathematical premise. [R03, R06–R07, R27–R30]

The architecture objective is to reduce a task's **reasoning surface**: the independent facts and source bodies that must remain simultaneously in context to make a correct change. File count and line count are proxies at best. Splitting a method into ten helpers capturing the same mutable universe can increase the reading burden.

Primary long-context research supports concern about retrieval/use of information in large contexts, but does not establish a codebase-size limit for the user's current agent. A 2026 benchmark of repository context files found no general resolution gain and increased costs in its tested settings. That cautions against solving this by endlessly extending AGENTS.md; it does not say documentation or native safety rules are worthless. [P2–P3]

## 7. What a better structure would allow

Keep the current solver; narrow the contracts around it:

1. **A resolved request is an immutable value.** Public mode, private treatment and capacities are visible before execution. Service and runner share resolution without depending on each other's CLI.
2. **A candidate is not a verified artifact.** The existing verifier/portfolio transition owns adoption. Consumers see only the appropriate stage and root domain.
3. **A phase owns its staged work.** Complete rows/evidence commit atomically; interruption and release are explicit; reports do not mutate it.
4. **A read is not service.** Observation uses a pure bounded owner view. Selection/pruning has a separate mutation contract.
5. **A report is a projection, not proof.** The native certificate, immutable artifact and declared comparison relation remain the source of acceptance.
6. **A build/test claim identifies what ran.** Interpreter, collector, prerequisite artifact, executable/WASM hash and test lane are named. Missing required layers fail; optional skips remain visible.
7. **A mathematical argument has one canonical owner.** Source and tests refer to its premises; historical failures remain in their original evidence rather than becoming mandatory prose everywhere.

These are target properties of existing owners, not seven new services/classes. Adopt one complete slice at a time. Information hiding is about decisions and dependencies, not moving statements to different files. [P1]

## 8. Recommended timing and programme shape

**Do now in this conversation:** retain the audit and future plan only. No redirect, patch, parallel edit or workflow rerun.

**After the current Codex handoff:** schedule structural consolidation rather than another major search feature by default. Its entry gate is an explicit terminal checkpoint with source/build identities, qualified capabilities and known failures—not a demand that every open solver research problem be solved. Unresolved responsiveness may be carried as an explicit limitation if the structural work does not conceal it.

**First selected implementation:** restore the trusted clean-checkout validation and reporting loop. This has independently reproducible failures and low mathematical risk. No expensive solver search is necessary to repair test collection, interpreter selection, positional metadata or byte portability.

**Next selected slice:** finish the current portfolio's read/adoption/selection ownership, or—if the active programme already closed it—separate one stable read-only diagnostic projection from coordinator internals. Do not automatically migrate the action ledger's scheduling authority or decompose the whole `Impl`.

**Then resume a major solver experiment** once the initial foundations and one meaningful ownership slice are qualified. Structural improvement continues when touching remaining modules. There is no “make the repository perfect” gate.

Subsequent choices are defined in POST_PROGRAMME_ROADMAP.md. Coroutine/LTO, cold header movement, case-resolution direction and serializer decomposition each have their own benefit/risk gate. They are not bundled into the first run.

## 9. Measure success without gaming the metrics

For each selected slice record a compact before/after table using existing tools:

- required test styles actually collected, required lanes executed, and clean-checkout prerequisites;
- unique mutable entry points to the selected evidence/lifecycle;
- obsolete aliases or duplicate implementations removed;
- source/header bodies rebuilt by one representative edit, with the same build setup;
- named units and inputs needed for a representative maintenance task;
- active and total task timing, source/host clocks and instrumentation overhead where touched;
- actual policy, status, root certificate, probabilities, resource debits and stop behaviour preserved under matched controls.

Do not replace one 5000-line function with 20 functions all taking unrestricted `Impl&` and count that as success. Do not count a new facade as closure while external mutation continues. Do not change test expectations, proof tolerances, prices, runtime caps or graph semantics to make a cleanup qualify. Faster wall-time to the same verified target, lower rebuild cost and smaller reasoning surface are different benefits; label the one achieved.

Three useful future maintenance probes are adding an observational progress field, rejecting a stale verified artifact, and resolving a diagnostic treatment through runner/Lab. They should be assessed when genuinely needed, not implemented as fake product work to manufacture an LLM benchmark. Compare context and changed-boundary requirements, not a single noisy token count.

## 10. Preservation and limits

The reviewed root policy references and exact Regalia are preserved as correctness/performance controls where relevant. The later current programme may improve them; use its final compatible receipts rather than roll back to this pin. Do not rerun unchanged-policy simulation for tooling/prose changes. Structural changes to shared native lifecycle require scoped cap/cancel/exception and original-root checks, not merely a normal-path graph hash.

This audit does not prove a hard LLM ceiling, a numerical flaw in all accepted policies, a need for a new solver, or the futility of lower-bound research. It identifies avoidable engineering friction and credible risk. The best next move is to make the existing architecture enforce the distinctions its mathematics already explains.

**Bottom line:** a structural-consolidation window is justified after the current run. Start with trustworthy validation, then close one ownership boundary. That is more likely to clarify the solver's true limits than another algorithm layer built on the same implicit assumptions—and far safer than a broad rewrite.
