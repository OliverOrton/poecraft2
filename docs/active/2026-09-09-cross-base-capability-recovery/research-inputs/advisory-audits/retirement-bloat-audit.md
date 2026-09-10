# Audit 3 — Retirement, duplication, and carrying cost

**Verdict: no significant production solver subsystem qualifies for deletion on the evidence examined.** The strongest cleanup opportunities are narrower: make build prerequisites explicit, remove a duplicate CI scan, and stop maintaining the same experiment narrative in multiple live documents. Preserve independent verifiers, frozen candidate ownership, and complete proof coverage. “No demonstrated root gain” is not the same proposition as “no useful consumer.”

**Repository:** `OliverOrton/poecraft2`  
**Pinned audit snapshot:** `be553608ecabde28a3dec07856255911459ec97d`  
**Review date:** September 9, 2026  
**Status:** advisory, read-only findings; no proposed change below has been applied.

## Three ranked interventions

1. **Align builds with their consumers.** Repair `dev-engine.ps1 -Task TestAll` so it builds the benchmark that its CTest selection invokes. Separately, make the quotient lower probe an explicitly built research target, conditional on confirming that its inclusion in every full build is not an intentional compile check. Keep its source and target name.
2. **Run knowledge lint once per CI path.** A base-aware lint invocation already performs the current-tree checks. Select that invocation when a base ledger exists, otherwise run ordinary lint; retain all other checks and failure behavior.
3. **Consolidate live experiment narration and correct the stale ledger description.** Keep detailed outcomes in the existing living record, derived series in the generated view, propositions in the ledger, and arguments in mathematics. No new registry, receipt, or documentation hierarchy.

These are independent small changes, not gates for Codex’s cross-base capability programme. Any implementation must first reconcile this pinned finding with the eventual newer source. The pinned build graph, CI implementation, and documentation policy support these recommendations; they do not establish a numerical speedup. [Build definitions][build], [development wrapper][dev], [knowledge workflow][knowledge-ci], [research ownership][research].

## Evidence posture and scope

Repository evidence was retrieved through the connected GitHub tools at the stated commit. Searches were used to locate files, not to prove absence on a moving branch. The audit examined selected build/CI definitions, native probe and fragment consumers, candidate capture, Lab entry points and dependencies, knowledge tooling, canonical documents, and the September 9 living record. It was not an exhaustive traversal of every source, script, or archive.

**Source-established** means visible code or configuration. **Historical observation** means a result reported in a retained repository record, not a measurement repeated here. **Inference** and **proposal** are explicitly identified below. There were no native builds, test executions, solver runs, simulations, queue operations, repository edits, or public actions. Protected path `0` was not opened. No new synthetic numerical result is needed for these findings.

I did not inspect Oliver’s local catalogue, shell history, ignored binaries, live Codex thread, or uncommitted changes. Missing access is not evidence of non-use. Official CMake 3.20 documentation is used only to establish build-target behavior, not game mechanics. The two supplied Audit 3 briefs define the review scope; the repository sources below supply its findings.

## Ranked candidate table

Cost categories are **R** runtime memory/CPU, **B** build/dependency/CI, **M** reading/manual maintenance, and **C** correctness risk from inconsistent or conflated ownership. Rankings prioritize actionable confidence, not invented monetary savings.

| Rank / path or symbol | Current consumers | Established cost or risk | Disposition | Strongest counterargument | Minimum safe migration or check |
|---|---|---|---|---|---|
| **1.** `engine/CMakeLists.txt`, `CMakePresets.json`, `scripts/dev-engine.ps1::TestAll`; `poecraft_quotient_lower_probe` | Native CTest corpus validation uses the benchmark; probe performs native lower/reference queries | **B/C:** `TestAll` omits one executable prerequisite; full builds include the probe; an archived relink is observed, duration unknown | **Consolidate prerequisites; make probe explicit-only after check** | Full-build inclusion may deliberately keep research code compilable | Add the existing benchmark preset only to `TestAll`; retain probe target/source and explicit invocation; check reproduction and CI expectations. [Sources][build] [presets][presets] [wrapper][dev] [log][probe-log] |
| **2.** `.github/workflows/solver-knowledge.yml` | Every applicable push/PR knowledge check | **B/M:** valid-base branch performs two current-tree lint scans | **Consolidate** | Both current validity and history preservation are necessary | Preserve both within the existing base-aware call; keep missing-base behavior and nonzero exits. [Workflow][knowledge-ci] [checker][knowledge] |
| **3.** `docs/solver/research.md`; `docs/foundation/solver-internals.md` | Research readers and implementation navigation | **M/C:** duplicate hand-maintained empirical narration; obsolete “companion draft ledger” description | **Consolidate/correct prose** | Research questions need concise empirical orientation | Keep conclusions and counterexamples; link detailed results to their existing owner; preserve anchors and histories. [Research][research] [internals][internals] [ledger][claims] |
| **4.** `engine/benchmarks/solver_executable_fragment*`; `tests/test_solver_fragment.cpp` | Benchmark shadow path, native tests/CTest, compiler-only fallback | **B:** two fragment translation units are listed in benchmark and test targets; **R:** no production engine inventory inclusion | **Keep as an independent verifier/test lane** | Its production policy benefit is unproved | No deletion. Any later target split must preserve the verifier, type-boundary tests, fallback build, and shadow/reproduction entry points. [Build][build] [tests][fragment-tests] [prior consumer audit][backbone] |
| **5.** `solver_lab.py`, `solver_lab_service.py`, supervisor/GUI adapters; `pyproject.toml` | CLI and GUI share the service; service uses existing corpus/worker contracts | **B:** core package dependencies are empty; GUI extra is optional. **M:** multiple adapters, but no demonstrated duplicate numerical authority | **Keep/use existing; GUI already optional** | Surface maintenance can be costly despite optional installation | Do not infer GUI abandonment. A retirement decision requires an actual user/automation consumer inventory, not commit sampling. [CLI][lab-cli] [service][lab-service] [dependencies][pyproject] [contract][lab-doc] |
| **6.** `capture_incumbent_policy`, saved candidate vectors, strict coverage scratch | Publication, fixed-policy materialization, observed choices; strict whole-cell lower lookup | **R/C:** owned copies exist; removable duplicate byte cost is not established | **Insufficient evidence for storage removal** | Immutable payload sharing might reduce memory | First identify identical simultaneously live payloads and their mutation/lifetime constraints; preserve extent, accounting, and whole-member coverage. [Capture][capture] [F7 lifetime evidence][proof-record] |
| **7.** `request_solver_proof_handoff`, demanded continuation rows, deferred option kernels, prepared strict lower consumption | Benchmark-private handoff and existing native continuation/strict owners; focused fixtures and F-series investigations | **R:** long treatments consumed work without a new root result; attribution to a disposable subsystem is unproved | **Retain scoped capability; freeze unsupported benefit claims** | Failed outcome experiments can leave lasting complexity | Reconcile current consumers with Codex; retain only for named proof questions or regression value, with the review criterion below. No automatic activation or deletion. [Hook][diagnostics] [record][proof-record] |

## Finding 1 — Build ownership both overbuilds and underbuilds

### Evidence and consequence

**Source-established:** `TestAll` first builds the `tests-only` preset, then runs CTest with `-L native`. That preset builds only `poecraft_header_smoke` and `poecraft_engine_tests`. However, the same native-labelled selection includes four corpus validation tests whose command is `poecraft_solver_benchmark --validate-only`. CMake substitutes the target’s executable path for such a test command; this is not a request to build that executable. [Development wrapper][dev], [presets][presets], [CTest declarations][build], [CMake command contract][cmake-test].

**Inference, not a reproduced failure:** on a fresh configured build directory, `TestAll` can reach those tests without the required benchmark executable. An earlier full build can mask the missing prerequisite. The Windows workflow uses `build.ps1` before `test.ps1`, so that workflow does not establish that the narrower `TestAll` entry point is self-sufficient. [Windows workflow][windows-ci].

The reverse problem is explicit in `CMakeLists.txt`: `poecraft_quotient_lower_probe` is an unconditional executable target, outside `BUILD_TESTING`, without exclusion from the default build. `build.ps1` performs an untargeted CMake build; the `all-native` preset likewise names no narrower target set. The retained September 7 `build-warm-fixed.log` actually records the probe being relinked alongside engine/test work. **This establishes build fan-out, not how many seconds it costs.** [Build][build], [normal wrapper][normal-build], [presets][presets], [historical log][probe-log].

The probe is not dead. Its `micro()` path constructs a bounded reference from native legality and kernels, checking the archived eight-state, 21-row, 56-transition scope. Its opening comment also distinguishes bounded lower queries from ordinary stepped observations without final extraction. Removing the source would discard a concrete research/reference consumer. [Probe source][probe].

### Strongest objection

The full build may intentionally compile every maintained diagnostic so source drift is caught early. Exclusion would reduce that coverage, not merely improve performance. Existing `engine-only`, `tests-only`, and `benchmark-only` presets already avoid incidental probe compilation during focused work; adding another wrapper is unnecessary. The probe’s incremental link cost may also be small. [Existing presets][presets].

### Smallest change and dependency closure

For `TestAll`, add `Invoke-BuildPreset "benchmark-only"` before CTest. **Do not add the benchmark to `tests-only` globally:** that would charge every focused test build for a prerequisite needed by the broader selection.

For the probe, the smallest proposed build-only retirement is:

```cmake
add_executable(poecraft_quotient_lower_probe EXCLUDE_FROM_ALL
    benchmarks/solver_quotient_lower_probe.cpp)
```

This removes default inclusion, not the named target, source, or native lower implementation. The explicit build remains:

```text
cmake --build build/engine --target poecraft_quotient_lower_probe
```

CMake documents `EXCLUDE_FROM_ALL` specifically as exclusion from default `all` targets. [CMake property contract][cmake-exclude].

**Affected closure:** the target declaration in `engine/CMakeLists.txt`; the `TestAll` branch in `scripts/dev-engine.ps1`; and any current reproduction instruction that promises an untargeted build produces the probe. `engine/engine-sources.txt`, the benchmark target, fragment tests, solver implementation, bindings, and archived evidence remain unchanged. No probe test is registered in the inspected CMake file. Compiler-only fallback already builds tests/shared library/benchmark without this probe. [Build inventory][inventory], [fallback][normal-build].

**Checks still needed before optionalization:** inspect direct or dynamically composed probe invocations in current scripts, fixtures, workflow configuration, and reproduction instructions; confirm whether maintaining probe compilation in every full native CI run is intentional. The repository searches were not a complete dependency proof, and external invocations remain unknown. If universal compile coverage is wanted, keep the default inclusion and use the already available focused presets instead.

A pinned, read-only starting dependency query is:

```text
git grep -n -E 'poecraft_quotient_lower_probe|solver_quotient_lower_probe' be553608ecabde28a3dec07856255911459ec97d -- engine scripts tools .github fixtures docs
```

Review indirect/dynamically constructed invocations as well; this query alone does not prove their absence. It deliberately excludes protected root path `0`.

**Minimum validation after selection, not executed here:** in an isolated build directory, inspect the native CTest command list with `ctest --show-only=json-v1 -L native`; build the two named presets; exercise only the four corpus-validation entries to verify the missing prerequisite is resolved. For probe isolation, verify the default build plan omits the probe and its explicit target still compiles. No solver run, browser rebuild, or Simulator is needed for this build-only distinction.

**Urgency:** the missing prerequisite is a medium-priority reliability fix. Probe isolation is low-priority and conditional; it must not delay solver work.

## Finding 2 — One CI lint scan can preserve both responsibilities

**Source-established:** `solver-knowledge.yml` unconditionally invokes ordinary lint and then invokes lint again with `--base` when the previous ledger exists. In `solver_knowledge.py`, both modes call `check()`. That function always parses the current ledger, traverses logical dependencies, checks solver-document links and claim references, and scans the declared source namespaces for annotations. Supplying `base_text` adds historical identity/statement/history comparisons; it does not replace the current-tree work. `main()` returns failure when the report contains errors. [Workflow][knowledge-ci], [`check()` and `main()`][knowledge].

**Consequence:** the valid-base path performs two overlapping current-tree passes. The bounded saving is one such pass on that path, not half of total CI time. No timings or installation costs were measured. The metadata check and report generator also share report-building machinery, but this audit does not establish that their contracts are interchangeable, so they are not in the deletion set.

**Strongest objection:** current correctness and append-only history checks are distinct safeguards. Removing the wrong invocation could skip one or hide a failure. That objection is answered by choosing the existing superset invocation, not weakening the checker.

**Proposed replacement for only the lint block:**

```bash
if git cat-file -e "$KNOWLEDGE_BASE:docs/solver/claims.md" 2>/dev/null; then
    python -m poecraft_ingest.solver_knowledge lint --base "$KNOWLEDGE_BASE"
else
    echo 'No previous claim ledger is available; append-only comparison is not applicable.'
    python -m poecraft_ingest.solver_knowledge lint
fi
```

**Deletion closure:** one redundant command in `.github/workflows/solver-knowledge.yml`, with the ordinary command moved into the missing-base branch. Keep the knowledge unit tests, report/classifier tests, metadata validation, generation, and committed-output comparison. No parser API, claim status rule, report schema, or required job name needs to change. [Full workflow][knowledge-ci].

**Cheapest discriminating check:** a tiny workflow-shell fixture with stubbed `git`/`python` can cover valid-base, absent-base, and failing-lint paths. Require exactly one lint invocation, the correct `--base` selection, and preservation of nonzero failure. Existing focused knowledge tests remain the implementation check; no native qualification is relevant.

**Urgency:** low risk and ready to select independently. Its value is eliminating unneeded repeated work, not unlocking solver capability.

## Finding 3 — Most workflow ceremony is already removed; residual prose still drifts

**Source-established:** AGENTS and CLAUDE already use task-directed navigation, proportionate validation, optional helpers, and one living record. The research handoff template is a convenience; an adequate imported report is retained once; its ordinary completion response is the receipt. A small fix needs no new plan, claim, packet, or archive. The Lab soak is explicitly not routine qualification, and the Windows workflow already skips native validation for recognized documentation-only changes. Recommendations to introduce these policies would duplicate existing work. [AGENTS][agents], [CLAUDE][claude], [research handoff][research], [Lab qualification][lab-doc], [Windows CI][windows-ci].

Two residual issues remain. First, `research.md` contains detailed chronological RQ-002/RQ-003 narratives also retained in the September 9 living record. For example, the prepared-floor counts and unchanged root result are narrated in both places. These are useful facts, but the exact same changing result story need not have two handwritten owners. Second, `solver-internals.md` describes a “companion draft ledger,” while the ledger explicitly records registered CLM-0001–CLM-0025 and responsible status histories. [Research][research], [living record][proof-record], [internals][internals], [claims][claims].

**Consequence, inferred:** duplicated live prose creates opportunities for inconsistent “current,” “next,” or “still open” statements and repeated editing. The stale ledger phrase obscures the distinction between a registered proposition and an unresolved native application. This is not evidence that every agent must read those files, nor a measured token/time saving.

**Strongest objection:** the research page should explain why a question remains open without forcing readers through a long experiment log. Keep that interpretive paragraph, causal caveat, and relevant claim dependencies. Remove redundant chronology, not the reasoning. Generated `research-state.md` is a declared historical series view, not a promise of exhaustive latest-state coverage; old observation caveats must not be rewritten to pretend they were known later. [Generated view][research-state], [research contract][research].

**Short replacement for the stale internals paragraph:**

> These claim numbers refer to the registered ledger. They map implementation responsibilities to propositions, not completed native verification. Read each claim’s final history event and the scoped correspondence obligations; neither a source annotation nor a passing lint check supplies runtime authority.

**Short replacement for the latest repeated RQ-002 outcome narration:**

> The September 9 Conquest work improved the verified upper in its earlier continuation phase. The subsequent useful-proof-time experiments established pre-finish strict work and compatible whole-cell lower consumption, but not material root improvement or new four-/five-goal exact closure. The [living record](../active/2026-09-09-empty-start-partial-continuation/README.md#useful-proof-time) owns the versioned controls, values, failures, and scope. The remaining question is whether the stronger scoped lower evidence changes a complete root proof; these Conquest results do not establish outcomes on other bases.

This passage is proposed for the repository document, where its relative link resolves. The report’s [pinned evidence link][proof-record] provides the same destination outside the checkout.

**Consolidation closure:** edit only the selected redundant current prose and obsolete phrase. HANDOFF keeps sequencing; the existing living record keeps experiment detail; mathematical chapters keep arguments; claim histories keep proposition status; generated views remain generated. Preserve incoming anchors, original imported reports, immutable comparisons, failed patches, and evidence archives. No historical rewrite, new index, or separate retirement register.

**Minimum check:** review the edited passages and their incoming/outgoing anchors; use existing traceability lint when relevant. No fresh solver qualification. **Urgency:** the misleading ledger phrase is an inexpensive correction; larger narration trimming can wait until those paragraphs next change.

## Finding 4 — The failed root-gain experiment has useful consumers, but not an unlimited research mandate

**Source-established:** the private diagnostic header already declares `request_solver_proof_handoff` as a benchmark-only request into the existing stable-candidate proof path. The current living record documents demanded native rows beyond saved policy ownership, deferred option kernels, and compatible prepared lower consumption in strict alternatives. These are implemented mechanisms, not missing proposals. [Private hook][diagnostics], [current mechanism/qualification record][proof-record].

**Historical observations, not rerun here:** the matched F6/F7 four-goal pair retained the same captured candidate identity. F7 strengthened 74,015 alternative obligations, reported 1.654 ms for the lookup work, and still returned **198.8334996747695–5218.040949685988**, with zero noncompetitive retirements and 90,810 unresolved obligations. That lookup time is not the total cost of preparation, strict work, or the experiment. F8 five-goal retained **405.3694021063399–85558.70618560436** but stopped at the existing partition-reconstruction memory cap. The final short native/WASM strategies were preserved; the existing three-suffix exact anchor was not a new exact result. [F-series evidence][proof-record].

The structured comparison also records the long profile’s 240-second finish request, 300-second native watchdog, Conquest target identity, and 1 GiB solver cap. The living record distinguishes these from the outer cleanup allowance and short regression requests. Only the beginning of that comparison file was directly inspected; detailed F-series outcome claims here are attributed to the living record, not to a complete independent rehash of its raw bundle. [Comparison identity][proof-comparison].

**Disposition:** retain the bounded research capability while its consumer remains concrete. Do not infer current non-Conquest behavior, activate a benchmark-only control as product policy, or delete the native continuation work while Codex may depend on it.

**Strongest case for retirement:** the branch missed its predeclared outcome objective, while implementation, tests, telemetry, and repeat experiments add maintenance cost. Indefinite “might help later” is not sufficient.

**Smallest useful retention criterion:** in the existing living record—not a new register—name the consumer as testing useful pre-finish proof service under complete action/continuation obligations. A further experiment is justified only by a changed dependency capable of affecting the measured frontier or memory obstruction, or by a distinct predeclared cross-base question. Repeating unchanged timing/settings is not a new hypothesis. At the next relevant reconciliation, a diagnostic with neither that consumer nor independent regression/reproduction value can become an archive-only tool after tracing its callers.

**Why not remove now:** F3c distinguished two real discovery-reopening gates; demanded-row and whole-cell fixtures establish behavior beyond a mock lookup. Conversely, F6/F7 recorded no use of the new parent-registration branch, so its real-case benefit must remain unclaimed. The evidence supports differentiated retention, not blanket success or blanket dead-code status. [Allocation, continuation, and zero-use qualifications][proof-record].

**Urgency:** immediate restraint on unsupported benefit claims and repeated no-progress runs; no immediate source deletion. Existing CLM-0022 already separates validity, progress, and bounded performance. [Ledger][claims].

## Finding 5 — Preserve the boundaries that make removal safe

### Fragment verification is an independent check, not a duplicate upper issuer

The build explicitly links the fragment implementation into the benchmark and native test executable, including the compiler-only fallback. The test file uses compile-time assertions to prevent proposals, structural control, verified fragments, flattened candidates, numerical values, public assertions, and bounded incumbents from being interchangeable. The structural-control view specifically does not expose the probability-bearing interface. [CMake][build], [fallback][normal-build], [type-boundary tests][fragment-tests].

The prior consumer audit also records `run_fragment_shadow_only` and a Python shadow workflow. Its implementation map places fragment translation units outside `engine/engine-sources.txt`, C ABI and release WASM. **Do not claim production runtime savings from deleting code already outside that production inventory.** The independent oracle/reproduction value is enough to defeat immediate deletion; general policy benefit remains a separate question. [Prior audit][backbone], [inventory][inventory], [implementation boundary][internals].

**Counterargument:** compiling the same two files for benchmark and tests is a real duplication of build inputs. A shared tooling library might avoid that, but no timing or flag-equivalence analysis establishes a worthwhile payoff here. Do not create another library target merely to reduce a source-list repetition. The smallest next check, only if build cost becomes material, is whether those two target compile configurations and measured compile work actually match.

### Lab layers are adapters with distinct responsibilities

`solver_lab.py` invokes `SolverLabService` and the supervisor. The service uses existing corpus-runner, case, identity, catalogue and worker-provenance contracts. The package has no mandatory dependencies and puts PySide6 in its optional GUI extra. This is evidence of shared ownership and an optional surface, not multiple solver engines. [CLI][lab-cli], [service][lab-service], [package][pyproject].

The documented process identity, immutable evidence, reservation, and quarantine responsibilities are not decorative wrappers. Removing them would change orchestration safety and evidence semantics. GUI use cannot be inferred from accessible commits. **No GUI/service/supervisor deletion is justified here.** MCP is already documented as removed and is absent from the inspected package entry/dependency declaration; it is not a new cleanup opportunity. [Lab contract][lab-doc].

### Frozen candidates and full coverage must not become live aliases

`capture_incumbent_policy` sizes the captured policy from `candidate.values.size()`, respects captured reachability, and materializes selected rows and observation choices from the candidate. Later materialization checks the saved extent. This is not equivalent to consulting the latest greedy policy or enlarging the table to current calculator size. [Capture implementation][capture].

The retained CLM-0021 counterexample is directly relevant: changing a selected row while keeping its old evaluated value invalidates reuse. The September 9 record also reports that F7’s first fixture failed because a new consumer read exact-state storage after release. The correction retained only required parent IDs and charged their scratch ownership. Removing that remaining coverage merely because it resembles another mapping would lose the whole-cell premise. [CLM-0021][claims], [F7 lifetime correction][proof-record].

**Counterargument:** immutable structural sharing could still remove genuinely identical overlapping payloads. That remains a proposal, not a measured opportunity. Before pursuing it, identify each overlapping allocation, its owner, mutation boundary, lifetime, charged bytes, and all consumers. Preserve typed authority, complete member coverage and memory accounting. The five-goal partition cap does not establish that candidate copies caused it.

**Already settled:** the former executable-carrier planner header returned not found at the pinned path, consistent with the backbone archive’s recorded removal of that header, its self-descriptor fixture, and unused include. The archive/current internals also record removal of the measured projection lookup cache. Preserve their historical counterexamples and original evidence; do not “delete” these again. [Retirement record][backbone], [current internals][internals].

**Urgency:** no deletion now. Any renewed storage or oracle cleanup needs a specific cost attribution and a complete consumer/lifetime argument, not another broad audit matrix.

## Limits that could change the recommendations

**Newer implementation:** Codex’s eventual diff could change these consumers or already fix the small issues. Reconcile before editing; this audit deliberately did not chase newer revisions.

**Probe build expectation:** a documented external reproduction script or deliberate CI compile-coverage requirement could justify keeping the probe in `all`. No further native experiment is required to answer that ownership question.

**Storage or surface retirement:** attributed simultaneously live allocation costs could make immutable sharing worthwhile; an explicit GUI/user-automation consumer inventory could support surface retirement. Neither is needed to perform the small CI or prose changes. No whole-repository deletion safety proof, local usage census, timing distribution, or native speedup is claimed.

The report recommends neither fresh qualification of unchanged strategies nor a re-run of the unsuccessful F-series. It introduces no mechanic ruling, scope change, theorem acceptance, or automatic implementation programme.

[agents]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/AGENTS.md
[claude]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/CLAUDE.md
[build]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/CMakeLists.txt
[presets]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/CMakePresets.json
[dev]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/scripts/dev-engine.ps1
[normal-build]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/scripts/build.ps1
[inventory]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/engine-sources.txt
[probe]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/benchmarks/solver_quotient_lower_probe.cpp#L1-L150
[probe-log]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-09-07-checked-numerical-reuse-v1/checks/build-warm-fixed.log
[windows-ci]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/.github/workflows/windows.yml
[knowledge-ci]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/.github/workflows/solver-knowledge.yml
[knowledge]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_knowledge.py
[research]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/research.md
[research-state]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/research-state.md
[internals]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/foundation/solver-internals.md
[claims]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/claims.md
[proof-record]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/README.md#useful-proof-time
[proof-comparison]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/proof-time-comparison.json#L1-L140
[diagnostics]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_diagnostic_options.hpp
[fragment-tests]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/tests/test_solver_fragment.cpp#L1-L160
[backbone]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-09-06-solver-mathematical-backbone-v2/README.md
[lab-cli]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_lab.py#L1-L150
[lab-service]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_lab_service.py#L1-L130
[pyproject]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/pyproject.toml
[lab-doc]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/foundation/solver-lab.md
[capture]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_solve_constructive.cpp#L870-L1020
[change-impact]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/foundation/change-impact.md
[cmake-exclude]: https://cmake.org/cmake/help/v3.20/prop_tgt/EXCLUDE_FROM_ALL.html
[cmake-test]: https://cmake.org/cmake/help/v3.20/command/add_test.html

## Codex intake — knowledge delta, not accepted claims

| Proposition to incorporate | Preconditions | Evidence or counterexample | Canonical destination | Unresolved premise / disposition |
|---|---|---|---|---|
| `TestAll` must build the benchmark used by its native-labelled tests; a research target can remain available without default inclusion | Preset/CTest ownership still matches the pinned source | Tests-only omits benchmark; four CTest commands invoke it. Probe has a real native-reference consumer and an observed historical relink | Existing build files; [change-impact][change-impact] only if its operating contract changes | Reconcile newer diff. Confirm probe compile-coverage/reproduction expectation. Engineering correction; no new CLM required |
| One base-aware lint call preserves current validation plus history checks | Existing `check()` still performs both within one invocation; failure exits retained | `solver_knowledge.py::check/main` and duplicated workflow commands | Existing knowledge workflow; implementation tests | Validate valid-base/missing-base/failure branches. No theorem or claim-status change |
| Register identity, mathematical acceptance, and native correspondence are different states | Read each claim’s final history and scoped premises | Registered ledger contradicts “companion draft ledger”; lint explicitly disclaims proof authority | `solver-internals.md`; existing research/claim owners | Editorial correction only; do not change histories or promote open claims |
| Local activity and recovered old policies do not establish a new root outcome | Same target, prices, action scope, resource envelope, and observation identity | F6/F7 matched candidate, stronger local floors, unchanged four-goal root; F8 unchanged five-goal root and memory cap | Existing RQ-002/RQ-003, living record; **CLM-0022**, **CLM-0024** and relevant **CLM-0006/0008/0011** correspondence | Retain scoped research consumer; future frontier/cap or cross-base benefit remains unproved. No status promotion |
| Structural membership or a duplicate-looking array does not authorize reuse/removal of saved policy or whole-member evidence | Exact semantic dependencies, immutable selected decisions, complete cell coverage, valid lifetime/accounting | Saved-extent capture; **CLM-0021** changed-row counterexample; F7 released-storage fixture failure | Existing **CLM-0021**; `mathematics/search-and-resumption.md#snapshots`, `mathematics/policies.md#properness`, and the whole-member argument in `mathematics/lower-bounds.md` under `docs/solver/`; current source map | Sharing needs attributed identical payloads and complete lifetime/consumer proof. No removal now |

**Oliver’s only additional decision needed for the proposed optionalization:** whether a full native build is intended to compile the probe every time. GUI retirement or loss of a supported fallback would require separate explicit selection; neither is recommended. This report does not pause or replace the ongoing programme.
