# 4. Validation, experiment selection and evidence contracts

This is a validation plan, not a claim that the commands below have already passed on a modified repository. Commands marked “existing” were checked in current source. Uppercase values are unresolved execution-time parameters, not guessed paths, case IDs or native flags.

## 4.1 Validation begins with collection and prerequisites

At the reviewed pin, the first Python discovery command runs before artifact preparation and uses unittest for a tree containing top-level pytest tests. The workflow-selected interpreter is not bound to the wrapper's `py -3` fallback. Repair those facts in the existing owner, not by telling every future agent to memorize a special command. [R20](SOURCES.md#r20), [R21](SOURCES.md#r21).

For the selected test families record: actual executable/version; collected test IDs and count; needed imports/binding; dataset/economy identity; whether a runtime is built or reused; and final outcome. Separate **passed, failed, unavailable, deliberately optional, and not reached**. Do not treat a process exiting zero after skipped native work as full qualification.

A future collection check can use the installed interpreter's `-m pytest --collect-only`, but inspect custom hooks, import-time binding loads and duplicate `tests` package roots first. Pytest interoperability does not support `load_tests`. The isolated demonstration included here validates that specific risk; it does not replace collection of the real repository. [P01](SOURCES.md#p01).

Use existing temporary source → database → compiled artifact fixtures for self-contained data tooling. Real native corpus tests still need their actual pinned data. The M1 gate cannot be passed by substituting arbitrary synthetic mechanics or excluding those tests without changing the reported coverage. [R26](SOURCES.md#r26), [R27](SOURCES.md#r27).

## 4.2 Known existing focused commands

### Research-report and traceability repair

The current knowledge workflow supplies these operations; run them with the one explicitly resolved Python and the appropriate package root, after dependencies are prepared:

```powershell
$env:PYTHONPATH = "tools/ingest"
& $env:POECRAFT_PYTHON -m pytest tools/ingest/tests/test_solver_reports.py tools/ingest/tests/test_ci_changes.py -q
& $env:POECRAFT_PYTHON -m unittest discover -s tools/ingest/tests -p test_solver_knowledge.py
& $env:POECRAFT_PYTHON -m poecraft_ingest.solver_knowledge lint --base BASE_REF
& $env:POECRAFT_PYTHON -m poecraft_ingest.solver_knowledge check-metadata experiments/solver-research/backbone-pilot-v1.json
& $env:POECRAFT_PYTHON -m poecraft_ingest.solver_reports --research-series experiments/solver-research/backbone-pilot-v1.json --output NEW_REPORT_JSON --markdown NEW_REPORT_MD
```

The first command currently fails on the positional support lookup. After fixing it, reach the later metadata/render comparison rather than assuming the earlier failed CI exercised it. Compare generated Markdown through its established deterministic contract. Temporary report paths must be new and outside immutable evidence. The script wrapper should check each exit code; these snippets are individual command examples, not a complete PowerShell runner. [R22](SOURCES.md#r22), [R23](SOURCES.md#r23).

### Native ownership and delivery contracts

After building the affected current test target through the existing incremental build path, these selectors are present in `engine/tests/test_main.cpp`:

```powershell
build/engine/poecraft_engine_tests.exe --solver-selected-fallback-only
build/engine/poecraft_engine_tests.exe --solver-bounded-finish-only
build/engine/poecraft_engine_tests.exe --solver-return-bridge-only
build/engine/poecraft_engine_tests.exe --solver-joint-policy-continuation-only
build/engine/poecraft_engine_tests.exe --solver-compile-metadata-only
build/engine/poecraft_engine_tests.exe --solver-proof-handoff-only
```

Select from these by the changed boundary; do not run all six automatically after each edit. The following existing selectors require an artifact argument in their dispatch:

```powershell
build/engine/poecraft_engine_tests.exe --solver-api-only ARTIFACT_DIRECTORY
build/engine/poecraft_engine_tests.exe --solver-native-continuation-api-only ARTIFACT_DIRECTORY
```

The existing `--solver-eldritch-side-fixtures-only` explicitly includes a 1000-trial simulation and is not a cheap proof-only substitute. Do not invent a new flag and assume the binary selected the intended subset. Inspect dispatch and the reported suite/check count. [R28](SOURCES.md#r28).

Retain the old-header canary and relevant ABI/lifetime checks through their existing implementation/evidence route. This package does not invent a canary command that was not read. Check the actual local selector before running it. [R09](SOURCES.md#r09).

### Web and release artifact

From `apps/web`, existing commands are:

```text
npm ci
npm test
npm run typecheck
```

Use `npm ci` when a clean selected web environment needs dependencies. Current `npm test` includes Calculator delivery and worker Finish-control tests; typechecking remains separate. This does not automatically build fresh WASM or prepare the production data bundle. [R30](SOURCES.md#r30).

The release module is owned by `scripts/build-wasm.ps1`, which activates its configured SDK. Preserve its current O1/non-LTO finish translation unit, O3/LTO remaining sources, exception mode, `-ffp-contract=off`, memory and stack settings. The alternate CMake WASM target does not establish identical release flags. Do not mix a compiler experiment into M2. [R19](SOURCES.md#r19), [R29](SOURCES.md#r29).

### Actual Calculator qualification probe

From `apps/web`, the current positional interface is:

```text
npx tsx test/calculator-delivery-probe.ts EXACT_FROZEN_CASE_ID NEW_ABSOLUTE_OUTPUT_JSON finish
npx tsx test/calculator-delivery-probe.ts EXACT_FROZEN_CASE_ID NEW_ABSOLUTE_OUTPUT_JSON cancel_setup
```

Resolve the exact case ID from the probe's existing fixed corpus at `docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json`; do not derive it from “Conquest five” or a filename guess. The probe validates artifact pins and exercises actual component/client/worker/WASM with linkedom. It does not constitute rendered UI acceptance. [R36](SOURCES.md#r36).

**Its built-in assertions check terminal status and graph usability, not the latency gates.** Inspect the recorded UI/worker/source milestones and explicitly compute the intended intervals. Include graph identity, frozen request/prices and any omitted event coverage. The 65-second contract begins at the worker request; the 75.925-second old Calculator figure includes 60 ms of earlier preparation. Do not subtract timestamps from different clocks. [R05](SOURCES.md#r05).

## 4.3 Minimum evidence per milestone

| Stage | Default evidence | Expensive work that is not justified merely by the stage |
|---|---|---|
| M0 | Current refs, explicit failure/prerequisite table, retained-pool writer map | New full solver cohort, reprofile all code |
| M1 | Real collection and negative canary; interpreter/dependency evidence; byte identity checks; report fixtures; reproducible required data setup | Policy discovery/simulation to test a Python collector |
| M2 | Focused lifecycle/role/tie/cap tests, passive-read checks, completed/partial handoff negatives | All-mechanics campaign after every method extraction |
| M3 | Selected native/downstream suites once; fixed-graph validation plus a small justified same-request discovery/control comparison; actual Finish delivery probe | Repeated 10000-trial runs of unchanged policies or rerunning the completed Ring research probes |

A retained native code change must receive current-source validation. Existing historical costs are controls, not evidence that a new executable is correct. Conversely, a pure M1 report/environment repair has no reason to rerun long cost search.

## 4.4 Matching and acceptance relations

Use four explicit relations rather than one normalized-JSON comparison:

**Raw artifact identity.** Compare bytes where immutable evidence or graph-transfer integrity requires it. Preserve source bytes and their expected hashes. A newline fix changes checkout rules, not the archived object.

**Same semantic request.** Match exact start item, ordered goal and required satisfaction, allowed actions/programmes, native mechanics/data and original prices. Root-only versus statewise authority is part of the meaning, not a cosmetic tag.

**Same experiment envelope.** Record executable/treatment as the intentional difference; match the declared time, memory, state/pair/transition/work bounds, activation and host reservations. The wide Ring treatment is not a default browser comparison. A host cleanup deadline is not the solver's requested finish.

**Qualified consumer projection.** Only remove or adapt fields explicitly justified by that consumer contract, such as the already-declared attached economy identity in the Calculator comparison. Keep missing/nonfinite/contradicted fields visible. Do not normalize away an input, refusal or graph difference to get a pass. [R05](SOURCES.md#r05), [R08](SOURCES.md#r08), [R11](SOURCES.md#r11).

For structural changes, use deterministic fixture event/selection traces and complete evidence comparisons. When timing matters, preserve a matched baseline executable and reverse arm order where a speedup claim is intended. A single noisy wall-clock miss is not automatically a semantic regression; a new graph/certificate mismatch is not explainable by timing noise. Investigate the actual difference.

## 4.5 Resource and interruption qualification

Exercise cap refusal before candidate copying, during old/new overlap and before transfer. Check the retained compiled artifact, certificate, frame and result storage populations. Validate that releasing scratch reduces live bytes but not consumed work, and that the compatibility-offset adjustment follows only actually removed fields. Repeated reads must neither prune nor change query/Finish eligibility.

Exercise Finish with an existing verified fallback, with a completed private/strict assertion awaiting transfer, and with an incomplete assertion. Add stale invocation and pre-aborted request cases through the existing worker-control tests. Preserve one terminal response and Cancel precedence. The pending proof may be interrupted, but a mixed graph/value bundle is never returned.

Test graph-prefix mutation, generation rewind, goal/pricing/scope change, changed certification bytes, missing root coverage and root-only contamination. Keep supported partial/unverified candidates distinct from invalid candidates; both are unavailable for delivery, but only the latter necessarily require removal.

## 4.6 Numerical and performance disposition

Do not change the existing numerical reconciliation tolerance, compiler floating-point flags, action cost objective or cap to pass the refactor. A fixed-policy cost check is not an optimum certificate. A repeated graph with a new stored-value mismatch requires investigation, not a rewritten golden result.

Current required timing gates are: begin ≤250 ms, setup cancel-to-release ≤1 s, each measured stepped call ≤250 ms, Finish intent-to-usable ≤10 s, usable graph within 65 s of worker start. Only the Finish gate is qualified for the relevant Conquest A7 actual-Calculator case. M3 preserves that positive contract and keeps the other baseline failures explicit; the queued performance programme targets them. [R05](SOURCES.md#r05).

The native/worker/DOM probe's success does not certify every browser/hardware profile. Report environment and clock coverage. Rendered UI remains Oliver's responsibility unless separately requested.

## 4.7 Closeout receipt

Use one living programme record with the baseline and final source/build identities, resolved commands, actual selected tests, pass/fail/unavailable/not-reached outcomes, graph/cost/authority comparisons, removed mutation paths, memory reconciliation, and exact remaining blockers. Preserve raw logs outside model context with reachable paths and hashes when actually retained.

No background monitoring, workflow rerun, push or public artifact upload is implicitly authorized by this package. Do not claim hosted CI is green until a run on the relevant pushed commit has actually completed. Local reproducibility and current hosted status are separate fields.
