# Audit 1 — Experiment tools and developer workflow

**Repository:** OliverOrton/poecraft2  
**Review snapshot:** `be553608ecabde28a3dec07856255911459ec97d`  
**Evidence:** pinned source, recorded experiments and retained research dispositions; no new runtime measurements. Historical experiment revisions remain distinct from the review snapshot. Findings need reconciliation against Codex’s eventual newer implementation.

## Verdict

**Keep the existing workbench. It is helping the project reject misleading explanations and choose useful follow-throughs. The evidence does not establish that its Lab catalogue, matrices and comparison interface are the normal route for solver research.** The observed workflow is a mixture of shared native tooling, direct diagnostic commands, campaign-specific comparison scripts and manual interpretation. Some of that mixture is appropriate; forcing every investigation through the Lab would sometimes lose the intended request.

The strongest demonstrated benefits are preserving experiment identity, independently evaluating policies, retaining failed treatments, and refusing to call local activity a root improvement. There is no defensible estimate of developer hours or model tokens saved. Nor does this audit establish that infrequently observed Lab features should be removed. [F-log] [F-summary] [Reuse] [Historical]

### Three ranked actions

1. **Build nothing: use the existing corpus runner and focused readers for ordinary comparisons.** The implementation session is the consumer; `solver_corpus_runner.py`, `solver_reports.py` and the Lab CLI already own the routine work. Use the already-selected cross-base programme’s cases rather than commission another cohort. Expected benefit is less repeated request assembly and fewer lost identity/failure guarantees; the cost is learning the existing invocation. Strongest objection: private diagnostics do not fit that route. Preserve their direct benchmark path. The cheapest check is whether the next already-authorized ordinary comparison can use the commands below without changing its resolved controls. **Useful now; not a new gate.**
2. **Small test change: separate Veiled proof checks from the embedded 10,000-trial sample.** Owners are `engine/tests/test_solver_s8_3.cpp` and the selectors in `test_main.cpp`; the consumer is the next session changing the relevant kernel/proof behavior. Expected benefit is avoiding an inseparable sampling workload when only deterministic/exact checks are selected. Keep explicit Simulator qualification available. Strongest objection: the sample may be cheap and can catch a different class of execution error. First inspect or record this fixture’s own duration during its next necessary invocation; no broad benchmark is needed. **Correct the selector boundary at the next relevant edit; urgency depends on actual cost.**
3. **Improve evidence access using existing exports, not a new service.** Research authors and the receiving Codex session should put the complete decisive argument, source pin and compact result directly in the retained report/context; optional ZIPs should carry supporting bulk, not the only copy of required directions. Reuse `solver_knowledge context`, existing Lab bundles and the campaign’s saved projections. Expected benefit is fewer incomplete handoffs and repeated evidence reconstruction; maintenance cost is small editorial discipline. Strongest objection: the latest continuation already does much of this. Preserve that improvement rather than mandate extra paperwork. The cheapest check is whether a fresh reader can understand the next decision from the accessible report and pins without opening its optional package. **Useful at the next handoff; no reason to pause current work.** [CLI] [Runner] [Reporter] [Veiled] [Selectors] [Context-receipt] [Research]

## Three reconstructed evidence chains

### A. Useful proof time: an experiment that correctly stopped without a new root result

**Question and controls.** Could additional time be spent on useful exact proof instead of more discovery? The F campaign preserved qualified `3174383` binaries and derived separate four/five-goal Conquest cases with a 240-second requested finish, 300-second native watchdog, 315-second outer safeguard and the existing 1 GiB cap. The original short fixtures stayed unchanged. Both the matched long control and the known short-run high-water mark constrained success before treatment results were observed. [F-log]

**Run and observation.** The saved F1 process receipt contains the actual benchmark command, native `reuse` activation, case, report/partial/strategy paths, process identity, timestamps and cleanup outcome. Its trajectory has no verified upper near 60 or 120 seconds. F1/F2 reported no strict rows. F3 and F3b exposed two separate reopening paths; F3c finally created a real proof window. This was not inferred from the mock fixture alone. [F-data] [F-log]

**Comparison and decision.** F4b named eager option-kernel admission as the discovered-state cap owner, enabling F5’s targeted deferral. F6/F7 then spent roughly 178 seconds in strict work. F7 strengthened 74,015 distinct alternative obligations but left the four-goal interval at approximately **198.83350–5,218.04095**. Its absolute gap, **5,019.20745**, missed the **4,015.36596** historical-artifact ceiling. F8 preserved the five-goal bounds but reached a partition-reconstruction memory stop. The branch stopped rather than counting kernel work, stronger local floors or recovery of the old upper as success. [F-log]

**Retained conclusion.** `summarize_proof_time.py` asserts declared input/action compatibility, independent evaluation, graph identity for short regressions and process outcomes; it preserves actual sample timestamps and computes the materiality result. Its special historical gate is genuinely campaign-specific. The canonical benchmarking and research pages retain the negative root result. I inspected the summarizer and selected comparison records, not every underlying log or ZIP member. [F-summary] [Benchmarking] [Research]

### B. Checked numerical reuse: a preparation improvement that did not attain the policy target

**Question and identity.** Starting from reviewed `c77422c…`, could current-query checked or untrusted numerical initialization reduce preparation without weakening the final native check? The recorded gate was at least 15% compact preparation reduction with unchanged checked strength, 32 MiB proof space and 1 GiB total. Provenance distinguishes compact, ordinary and development executable hashes, and explicitly separates measured binaries from later acceptance-only edits. [Reuse] [Reuse-provenance]

**Observation and comparison.** Checked-only reuse improved preparation by 10.4%, below the gate. That justified one bounded follow-through using untrusted vectors with final checking. The accepted compact comparison reduced **43.1828114 to 31.3143940 seconds**, or 27.48%. Numerical time fell, relation construction did not; all 26,259 final relations and the two scoped source values remained unchanged. This was a matched sequential observation, not a timing distribution. [Reuse]

**Decision and retained outcome.** The ordinary runs retained no verified upper. On the separately declared fractured four-to-five development case, both variants missed the preselected **2,698.874796** policy target and returned the same roughly **10.401-million** verified fallback. Thus the result justified optional preparation reuse, not target attainment or broader exact closure. The existing probe verifier and research-series reporting retained the distinction. [Reuse] [Reuse-provenance] [Research]

**External-report intake.** This campaign also records a successful import of an original research package and subsequent reconciliation. Its first real redirected context export exposed a Windows encoding error, which was fixed and tested; the actual RQ-002 export then succeeded while warning that the local revision was not observed in remote refs. This is real use evidence for the context exporter, not just an advertised feature. I read the receipt and provenance; the raw compact ZIP was not independently rehashed or replayed in this audit. [Context-receipt]

### C. Historical five-goal regression: a justified bypass of the Lab

**Question and mismatch.** Was a worse current policy a solver regression or a different experiment? The record identifies four mismatched generation controls: 60 versus 10 seconds, 1,024 versus eight work items, Imprint enabled versus disabled, and base override five versus one. [Historical]

**Actual interface choice.** The Lab import normalized the historical work step to eight and disabled Imprint. The draft was never frozen or run and was discarded through the CLI. The investigation therefore used the archived manifest directly with the native benchmark. That preserved the experiment better than insisting on the current product profile. [Historical]

**Observation, comparison and decision.** The matched current request still produced a roughly 14.45-million policy. Historical source reproduced the byte-identical roughly 87,361 policy in a longer observation window. Matched 60-second source comparisons then isolated `1f68497`/`b6fb861` as the last-good/first-bad scheduling boundary. Restoring interleaving recovered the search behavior; a subsequent 120-second run generated a cheaper **85,408.643621** independently evaluated strategy. Different observation windows do not establish a same-budget speedup, and an exactly evaluated policy is not automatically a certified optimal solution. [Historical]

**Evidence limit.** The pinned archive directory contains the narrative README, including result hashes and the reported disposition; I did not read the original process logs behind those historical statements. This chain therefore has lower direct-artifact coverage than F1’s saved receipt. It still establishes an observed Lab-profile incompatibility and a documented decision to use the direct interface—not a general argument against immutable Lab cases. [Historical]

## Five decisive findings

### 1. The core tools are productively used; routine Lab adoption remains unestablished

**Evidence and consequence.** Case derivation, native validation, immutable revisions, matrices, targeted dispatch, comparison and cancellation are implemented. The CLI acceptance log records their exercise, including an authoring defect where lost product-envelope controls broadened catalogue work. Native qualification exposed that defect and led to a repair. That is a concrete benefit, but it is qualification evidence, not proof that later researchers routinely use matrices. [CLI] [Lab-service] [CLI-acceptance]

Recent F commands instead use the native benchmark plus a campaign summarizer. The corpus runner itself reuses the shared `solver_worker` process/identity machinery; the active record says its wrapper reuses the isolated-process owner. I found no basis for alleging an independently reinvented scheduler or cleanup system. The visible overlap is mundane glue—hashing, input checks, selected projections and packaging—not duplicate numerical authority. [Runner] [F-log] [F-summary]

**Strongest counterargument.** A small script answering a new scientific question can be cheaper and clearer than extending a generic schema. F’s historical gate and non-backdated samples are examples. The summarizer prints only its gate JSON: large retained reports are not evidence that full reports were routinely dumped into model context.

**Smallest change and urgency.** Route ordinary cohorts and Lab-owned attempts through existing commands; retain private diagnostic scripts where they add a distinct assertion. Do not delete or rebuild the Lab based on inaccessible catalogue history. This is an access/usage improvement, not an urgent architecture defect.

### 2. Identity and high-water marks influence decisions; cross-base generalization is still a separate obligation

**Evidence and consequence.** F’s success calculation requires beating both the matched long control and the known short-run result. F6’s wall-triggered handoff captured a different candidate from F5, and the report explicitly declines causal credit for a parent-registration path that had zero observed use. F6/F7 share the recorded candidate identity `4ff45baf2099fb44`, supporting a narrower comparison. Earlier B1–B5 intermediate hashes were not captured; later provenance does not retroactively make those builds byte-exact. [F-log] [F-summary]

The Lab comparison exposes full/core/component identity equality, not an automatic causal verdict. `solver_reports` exposes mismatch exclusions and distinct completed/partial/failure accounting. Its legacy rates are not the all-planned-cases exact-closure denominator; the explicit outcome profile serves that question, with supported, unsupported and contradicted proof evidence separated. Failed/no-policy cases are therefore not inherently lost by the platform, but consumers must choose the right view. [Lab-service] [Reporter] [Benchmarking]

**Strongest counterargument.** Matching identifiers does not neutralize wall-clock scheduling effects or establish repeatable performance. Likewise, multiple Conquest goal shapes are useful controls but not multiple item bases. The frozen Lab manifest includes a Spine Bow control; its existence alone establishes neither current performance nor use as a decision gate. [Lab-corpus]

**Smallest check and urgency.** Preserve declared candidate/graph identity, actual activation and failure/cap outcomes in the already-selected cross-base work. Do not extrapolate F’s Conquest result to non-Conquest cases. This is important before claiming generality; it does not justify another independent cohort or blocking Codex’s programme.

### 3. Current diagnostics answer several causal questions; neither a profiler platform nor full strict checkpointing is yet justified

**Evidence and consequence.** Phase/work observations distinguished no proof allocation from slow proof, named F4b’s offending option/parent, and showed F7’s lookup component took only 1.654 ms even though frontier work remained unfinished after the long proof window. The preparation study separated relation construction, numerical work and checking. These measurements changed experiment choices. [F-log] [Reuse]

The F1 record explicitly calls its process-memory values working-set snapshots, not peak RSS; native owned-memory peaks are separate. Owned bytes, transient overlap, repeated query calls and unique strengthened obligations must remain distinct. Existing action-level reporting also exposes cache requests/hits, time and retained-byte aggregates. None of these is an allocation-stack profile or a complete automatic first-divergence explanation. I did not establish such a turnkey capability at the inspected interfaces, and do not infer its absence from the whole repository. [F-data] [Reporter]

**Replay boundary.** The documented development checkpoint saves a completed coarse transition closure and namespace; it refuses incomplete/focused graphs, active cursors and the proof-carrying quotient graph. It reruns Bellman, strict refinement, compilation and evaluation. It cannot pause F8’s live strict partition rebuild or guarantee reproduction of F5/F6’s different wall-selected candidate. [Replay]

**Strongest counterargument.** Full checkpointing or profiling could eventually save substantial repeated work. That benefit has not been measured here. A smaller same-candidate witness or a narrow owner trace may answer the immediate question more cheaply.

**Smallest next check and urgency.** Before a new measurement, state which unresolved decision remains: for example, whether F8’s first refusal comes from retained partition capacity or transient reconstruction overlap. Inspect the already-retained owner/cap evidence first; request only the missing owner-specific slice if needed. No broad profiling run, checkpoint programme, dashboard or new numerical backend is warranted by this audit.

### 4. Validation is mostly proportionate, but one narrow selector couples proof work to sampling

**Evidence and consequence.** Current selectors already isolate phase-lower, selected-fallback, joint-continuation, policy-refinement, bounded-finish and proof-handoff checks. The later campaign deliberately avoided the full acceptance suite and case-level Simulator runs. That is evidence of focused validation, not a pervasive habit of rerunning everything. [Selectors] [F-log]

However, `--solver-automatic-veiled-only` invokes a fixture that independently evaluates the compiled graph and then unconditionally calls `run_compiled(..., 10000, 8315)`. F5 records exercising this embedded sample. Calling it “focused” does not make it proof-only. The directly supported avoidable quantity is **10,000 sampled trials per invocation when sampling is not the selected question**; elapsed savings are unknown. [Veiled] [Selectors] [F-log]

**Strongest counterargument.** Independent evaluation and simulation can expose different failures. Moreover, native and WASM qualification are not interchangeable: the earlier C8 WASM run failed the policy-improvement target despite the native result, and C15 passed after preparation work changed. Removing those checks merely because an ABI stayed unchanged would have hidden a delivery failure. Historical native workflow qualification also found a real envelope-preservation bug. [F-log] [CLI-acceptance]

**Smallest change and urgency.** Separate the optional sampling portion from Veiled’s exact/kernel assertions, preserving an explicitly selected sampling route and existing broader coverage. Reuse existing selectors elsewhere. Inspect this fixture’s duration during its next necessary run before prioritizing the split as a major performance project. Do not retrospectively judge older owner-approved trial counts by a later validation policy.

### 5. The evidence exchange is improving; inaccessible packages were a real boundary, not a missing dashboard

**Evidence and consequence.** The September 9 record repeatedly distinguishes retrieved review text from generated packages/scripts that the receiving session could not obtain. It retained the text, independently wrote and checked replacement mathematical examples where needed, and did not claim to execute the unavailable originals. Later self-contained directions superseded the unavailable package. These are documented handoff limitations; their developer-time cost was not measured. [F-log]

The existing context exporter pins committed source and warns about local-only visibility. The Lab bundle already includes identities, summaries, reproduction arguments, bounded events and a log tail. But a bundle is not a portable copy of every raw artifact, and its 64-sample trace is a bounded projection. `get_bound_trace` uses uniform index subsampling when truncating: a small returned trace must not be treated as preserving every first event. [Context-receipt] [Lab-service] [Benchmarking]

**Strongest counterargument.** The latest record already retains complete review texts once, with scoped dispositions and canonical destinations. That substantially addresses the historical problem; a new mandatory packet or receipt would add friction.

**Smallest change and urgency.** Keep decisive arguments and the compact result in the directly accessible report/context, with optional bulk evidence linked separately. Preserve source, candidate and actual observation time when extracting a diagnostic slice. Continue the existing normal completion-message receipt. No MCP reinstatement, remote workers or new evidence database is justified. [Research]

## Capability/use matrix

“Qualification” means exercised to establish tool behavior; “research” means observed in an actual solver investigation. Neither implies widespread adoption.

| Capability | Implemented owner | Real-use evidence | Decision it enabled | Limitation | Verdict |
|---|---|---|---|---|---|
| Derivation, validation, immutable revisions | `solver_lab.py`; `SolverLabService.derive_case`; case/profile owners | CLI qualification; historical draft import/discard | Caught envelope-control loss; refused an unsuitable historical route | Routine recent research adoption not established | **Use existing; improve access** |
| Matrices, queue supervision, cancellation | `solver_lab_workflow.py`; service; supervisor | Resolved matrix, bounded/no-strategy case and cancellation in qualification | Demonstrated reproducible submission and safe lifecycle | Not evidence matrices drove recent F decisions | **Use existing when appropriate** |
| Native execution, identities and partial failure retention | Benchmark; `solver_worker`; corpus runner | F1 command/receipt; F failures and native cap results | Separated process failure, bounded delivery and proof stops | Direct wrappers must retain resolved private controls | **Productively used** |
| Focused summaries and trajectories | Lab `get_run_summary`/`get_bound_trace`; native trace; F summarizer | Native trajectories and selected observations used in F | Established missing proof allocation; prevented backdating | Truncated traces can omit events; Lab-reader adoption unknown | **Use existing; preserve sample limits** |
| Paired comparisons | `solver_reports`; Lab comparison; campaign assertions | Reuse and F matched assertions | Rejected causal overclaims and false wins | Generic comparison is not a scientific decision rule | **Use existing plus narrow assertions** |
| Exact-result classification and failure denominators | Reporter outcome profile; native publication/evaluator | Failed targets/no upper retained; F closure gate false | Prevented local progress being called exact closure | Legacy rates and supported strict-proof profile have different scope | **Use the appropriate existing view** |
| High-water marks and diverse cases | Campaign gates; frozen manifest; research references | Historical 87k, reuse 2,698.875 and F short-policy targets | Changed continuation and stop decisions | Current non-Conquest decision-gate use unestablished here | **Keep; do not infer generality** |
| Portable context and investigation bundle | `solver_knowledge`; Lab bundle export | Actual RQ-002 export; review-text intake; bundle implemented | Source reconciliation and honest unavailable-evidence disposition | No recent productive bundle adoption demonstrated; no arbitrary-file copying | **Improve access, not infrastructure** |
| Development replay | `SolveTransitionCache` and documented benchmark checkpoint contract | Recent campaign use not established | No demonstrated saving in these three investigations | Completed coarse closure only; not a live strict checkpoint | **Existing narrow use; strict replay unimplemented** |
| Phase/query/memory diagnosis | Native telemetry/probes; action aggregates; focused attribution | Reuse stage split; F4b, F7 and F8 | Chose targeted changes and stopped unproductive work | Not equivalent to CPU/allocation stacks or causal replay | **Use existing before adding instruments** |

Source owners and receipts: [CLI] [Lab-service] [Runner] [Reporter] [CLI-acceptance] [Lab-corpus] [Replay].

## Shortest existing command paths

**These are proposed commands, not executed audit work.** They require the authorized local checkout, current data artifact and already prepared binaries. They do not install, build or start work from this report automatically.

### 1. Compare a solver change on a small diverse set

For an ordinary native comparison, there is no need to author complete case JSON or a new matrix. Select existing cases and run two preserved executables into separate ledgers. The following three IDs exist in the pinned Lab manifest: a same-side Conquest anchor, a Spine Bow control and a partial Conquest case. They are illustrative existing development controls, not an unseen evaluation cohort or a replacement for Codex’s selected population. [Lab-corpus]

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
$baselineExe = "PATH_TO_PRESERVED_BASELINE.exe"
$candidateExe = "PATH_TO_PRESERVED_CANDIDATE.exe"

$common = @(
  "--root", ".",
  "--artifact", "data/compiled/current",
  "--corpus", "fixtures/solver-lab/v1/manifest.json",
  "--max-workers", "1",
  "--goal-progress-gated-reforges",
  "--case", "conquest-lamellar-allflame-clean-3-suffix-product8",
  "--case", "spine-bow-allflame-clean-4-goal-product8",
  "--case", "conquest-lamellar-allflame-partial-4-to-5-product8"
)

py -3 -m poecraft_ingest.solver_corpus_runner @common `
  --executable $baselineExe --output build/selected-comparison/baseline
py -3 -m poecraft_ingest.solver_corpus_runner @common `
  --executable $candidateExe --output build/selected-comparison/candidate
py -3 -m poecraft_ingest.solver_reports `
  --run baseline=build/selected-comparison/baseline `
  --run candidate=build/selected-comparison/candidate `
  --pair baseline:candidate `
  --output build/selected-comparison/comparison.json
```

**Unavoidable checks:** supply the two preserved binary paths; declare the executable/source change as the treatment; keep other relevant controls/build conditions compatible; and use fresh output directories for different identities. Exact evaluation defaults on and `--run-verification` is deliberately absent. Review ledger failures, exclusions and cap outcomes even when a comparison file is produced. A runner exit code of two is not a passed cohort. For a planned-cohort exact-closure question, use the existing explicit `--outcome-profile` contract rather than interpreting legacy terminal rates as that score. [Runner] [Reporter]

This is the ordinary native route. The runner parser does not expose F’s `--proof-handoff-seconds` or `--native-retention-diagnostic` switches. Do not imply the command reproduces F’s private activation or a WASM default. For that investigation, preserve the directly recorded benchmark request and its process/identity owner instead of silently dropping the diagnostic. [Runner] [F-data]

### 2. Inspect one expensive/incomplete run without its full report

For an existing Lab attempt:

```powershell
py -3 -m poecraft_ingest.solver_lab --root . `
  run-summary --attempt-id ATTEMPT_ID
py -3 -m poecraft_ingest.solver_lab --root . `
  bound-trace --attempt-id ATTEMPT_ID --max-samples 32
```

Common overrides such as `--catalog` go **before** the operation. `run-summary` has no `--summary-fields` option; that selector belongs to `run`/`run-matrix-file`. Inspect status, stop owner, verified policy, phase and memory before requesting more. A 32-sample trace is an overview, not a first-event certificate. For a research handoff, the existing optional export is:

```powershell
py -3 -m poecraft_ingest.solver_lab --root . `
  export-bundle --attempt-id ATTEMPT_ID --idempotency-key EXPORT_KEY
```

Use an actual attempt ID and an export key for that intended export. These commands cannot read F’s direct run merely by inventing a Lab ID. For its already-retained comparison, this small local read selects one observation without rewriting artifacts or running the solver:

```powershell
$p = "docs/active/2026-09-09-empty-start-partial-continuation/proof-time-comparison.json"
$r = (Get-Content $p -Raw | ConvertFrom-Json).runs.'F7-four-heuristic'
[ordered]@{
  summary = $r.summary
  strict = $r.refinement.strict_lift
  heuristic = $r.refinement.completion_heuristic
  memory = $r.memory
  observations = $r.observations_at_seconds
} | ConvertTo-Json -Depth 20
```

This is a projection of an existing campaign artifact, not a proposed new wrapper, report schema or native measurement. Source syntax and keys are established by the current parsers/service and F summarizer. [CLI] [Lab-service] [F-summary]

## Evidence that could change these recommendations

A representative recent **research** Lab attempt/matrix/bundle could change the adoption assessment; qualification logs and source alone cannot. A measurement of Veiled’s own sampling duration could change the priority of splitting it; no timing saving is claimed here. Additional original historical receipts would improve confidence in the August 30 causal chain, whose archived directory currently provides a narrative rather than the full process record. Newer Codex results could change the cross-base assessment and must be reconciled separately. None of those unknowns supports declaring a tool unused, deleting it or pausing current implementation.

## Codex intake — compact knowledge delta

| Proposition to retain | Preconditions and evidence/counterexample | Canonical destination | Unresolved premise / disposition |
|---|---|---|---|
| The existing measurement stack supports useful experiment decisions; Lab adoption is a separate empirical question | F receipts/assertions, reuse comparison, CLI qualification and the historical profile mismatch | `docs/foundation/solver-lab.md`; `docs/solver/benchmarking.md` | Local research-use frequency unknown. Workflow observation; no new theorem/claim ID needed. |
| Local lower consumption and extra strict work do not establish root progress | F7: same F6 candidate identity, 74,015 strengthened obligations, unchanged root gap; F8: preserved upper with a new memory stop | RQ-002/RQ-003 and the already-recorded useful-proof-time applications of **CLM-0006/0008/0011** | Keep existing preconditions/statuses. Do not prescribe handoff, demanded rows, deferral or prepared-lower consumption as absent. |
| Stopping optional proof must remain distinct from losing an already verified policy | Bounded-finish evidence and F8’s retained artifact; compatibility, independent evaluation and relative verified cost still matter | `docs/solver/resources-resume-replay.md`; `docs/solver/upper-authority.md` | Does not certify unresolved alternatives or new exact closure. No status promotion. |
| A focused selector is not necessarily proof-only | `run_automatic_veiled_program` performs exact evaluation and then 10,000 sampled trials; F5 exercised it | `engine/tests/test_solver_s8_3.cpp`, `engine/tests/test_main.cpp`; affected validation guidance | Split only if selected; preserve explicit sampling coverage. Duration remains unmeasured here. |
| A portable handoff needs accessible decisive content, not merely a package link | Successful committed-context export; repeated unavailable September 9 packages; later complete review-text imports | `docs/solver/research.md#handoff`; existing context/bundle references | Current self-contained intake already mitigates the problem. No extra receipt form, framework or automatic claim acceptance. |

**Decision for Oliver:** whether to select the small Veiled test split when that fixture is next touched. The other recommendations reuse existing practices and are advisory inputs to the ongoing implementation session, not additional gates.

[F-log]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/README.md
[F-summary]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/summarize_proof_time.py
[F-data]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/proof-time-comparison.json#L190-L345
[Reuse]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-09-07-checked-numerical-reuse-v1/README.md
[Reuse-provenance]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-09-07-checked-numerical-reuse-v1/provenance.json
[Historical]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-08-30-historical-five-goal-quality-regression-v1/README.md
[CLI]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_lab.py#L53-L230
[Runner]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_corpus_runner.py
[Reporter]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_reports.py
[Veiled]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/tests/test_solver_s8_3.cpp#L600-L720
[Selectors]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/tests/test_main.cpp#L10-L210
[Context-receipt]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-09-07-checked-numerical-reuse-v1/README.md#final-committed-context-acceptance
[Research]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/research.md
[Lab-service]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/tools/ingest/poecraft_ingest/solver_lab_service.py
[CLI-acceptance]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/archive/2026-08-29-native-solver-cli-workflow-v1/execution-log.md
[Lab-corpus]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/fixtures/solver-lab/v1/manifest.json
[Replay]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/resources-resume-replay.md#development-replay-boundary
[Benchmarking]: https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/benchmarking.md
