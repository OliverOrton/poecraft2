# Solver tooling: choose the existing owner

Use this map before adding experiment infrastructure. Choose an existing route
when it preserves the intended request; otherwise identify the specific gap and
prefer a small extension or faithful adapter. Native C++ owns computation and
evaluation. The Lab and corpus runner share the isolated worker and supervision
contracts; they are different entry points, not competing solvers.

Run from the repository root with
`$env:PYTHONPATH = "tools/ingest;bindings/python"`. The commands below use
`py -3 -m poecraft_ingest.<module>`; replace capitalized placeholders with actual
IDs or paths. Common Lab options such as `--root .`, `--executable EXE`,
`--corpus MANIFEST` and `--profile PROFILE` go before the operation.

| Task | Existing command or owner |
|---|---|
| Build the benchmark | `powershell -File scripts/dev-engine.ps1 -Task Benchmark` |
| Select frozen cases and an executable | `solver_corpus_runner --executable EXE --artifact data/compiled/current --corpus MANIFEST --case CASE --output NEW_RUN`; repeat `--case` for a cohort |
| Compare preserved builds | Run each executable into its own directory, then `solver_reports --run base=BASE_RUN --run candidate=CANDIDATE_RUN --pair base:candidate --output REPORT.json` |
| Summarize a corpus run | `solver_reports --run current=RUN --output SUMMARY.json`; runner outcome and resolved command are also in `RUN/ledger.json` |
| Discover Lab profiles and cases | `solver_lab profiles`; `solver_lab case CASE` for one known case; use catalogue listing only when selecting a case |
| Derive a Lab case | `solver_lab derive-case --source-case-id CASE --name NAME --set-json /POINTER=JSON --validate --save --idempotency-key KEY`; use registered bounded patches |
| Run a Lab case or revision | `solver_lab run CASE --wait`; or `run --revision-id REVISION --wait --summary-fields status,phase,lower,upper,states,rows,memory` |
| Diagnose a Lab attempt | `solver_lab run-summary --attempt-id ATTEMPT`; `bound-trace --attempt-id ATTEMPT --max-samples 32` |
| Compare or inspect Lab strategies | `solver_lab compare ATTEMPT_A ATTEMPT_B`; `strategy-summary --attempt-id ATTEMPT` |
| Independently evaluate a Lab strategy | `solver_lab evaluate-strategy --attempt-id ATTEMPT` |
| Run a declared Lab matrix | `solver_lab run-matrix-file MATRIX.json --wait`; existing matrix validation and immutable resolution own expansion |
| Reproduce a Lab attempt | `solver_lab export-bundle --attempt-id ATTEMPT --idempotency-key KEY`; this is an evidence bundle, not necessarily the smallest context |
| Send selected mathematical context | `solver_knowledge context --claim CLM-0012 --max-chars 20000`; use the relevant claim and preserve its argument/dependencies |
| Use a native-only diagnostic | The benchmark's existing flags or shared worker adapter; preserve exact activation when Lab normalization would change the request |

Inspect operation `--help` for less common flags. Detailed contracts remain in
[benchmarking](../solver/benchmarking.md), [Lab operations](solver-lab.md) and
[research handoffs](../solver/research.md#handoff). The Lab's default profile
changes scope, including Imprint; importing a diagnostic there is not an identity
preserving shortcut. Changed executable, scope, activation, prices or caps must
remain visible in the resolved command and comparison.

## Actual Ring/Amulet workflow

Reuse the frozen CB06/CB08 requests. For an already-needed experiment:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
$solverCohort = "docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json"
py -3 -m poecraft_ingest.solver_corpus_runner `
  --root . --executable build/engine/poecraft_solver_benchmark.exe `
  --artifact data/compiled/current --corpus $solverCohort `
  --case cb06-cross-base-product8-long240 --case cb08-cross-base-product8-long240 `
  --native-retention-diagnostic reuse --host-watchdog-seconds 315 `
  --max-workers 1 --output out/proper-policy-recovery/candidate
py -3 -m poecraft_ingest.solver_reports `
  --run candidate=out/proper-policy-recovery/candidate `
  --output out/proper-policy-recovery/candidate-summary.json
```

Select a preserved baseline executable and a separate output for comparisons.
The case files retain 240-second requested finish, 300-second native watchdog,
1 GiB solver cap and exact evaluation controls. The 315-second override is the
host cleanup deadline only. Exact evaluation remains on; sampling remains off.
Each ledger case records `resolved_command`, its identity and the host reservation.
Changed native activation or host deadline refuses reuse of an incompatible
ledger. Omitted options retain the legacy invocation and configuration identity.
The old campaign adapter remains reproduction evidence; its command-building
workaround is no longer needed for these controls. Other native modes, such as
`checked` with its required target lower, still use their explicit native path.

## Read compactly; distinguish recovery mechanisms

Begin with status/stop reason, verified policy, bounds, time, memory and unfinished
work. Programs may parse large raw reports to extract a named detail; keep bulk
reports out of model context. Preserve truncation and sample limits: a sparse
trace can miss the first important event. Experiment-specific preparation,
analysis and assertions may stay local; reuse execution, supervision and
comparison owners. Add telemetry through the native owner when measurement is
missing. Consolidate only duplication encountered in the selected work.

Live stepping retains the running solver. Compatible candidate continuation
retains fixed decisions and a cursor. Development replay restores an eligible
completed coarse graph. Corpus ledger resume skips compatible completed cases;
Lab retry creates another attempt. Bundles support reproduction. None of the
latter three restores a dead process's live strict-partition workspace.
