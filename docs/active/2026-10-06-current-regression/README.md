# Current representative comparison

This workflow selects the existing five-case law-3 native Current corpus. It changes
neither the cases nor their frozen prices/runtime. The preserved dirty Bow4 is
existing representative coverage; it is not Oliver's separately improved request.
No Finder work is included.

The declaration at fixtures/solver-regressions/current-representative-v1.json
provides the same explicit case_ids to the existing corpus runner and reporter.
Run each preserved executable into a new output directory. Resolve and check
the actual binary/source, frozen artifact, case bytes, economy, Current activation
and capacity identity before parent-controlled LOCAL execution. Do not refresh
frozen inputs. These commands are invocation instructions, not a run allowance.

PowerShell selection:

```powershell
    $cohortPath = "fixtures/solver-regressions/current-representative-v1.json"
    $cohort = Get-Content -LiteralPath $cohortPath -Raw | ConvertFrom-Json
    $caseArguments = @()
    foreach ($solverCase in $cohort.case_ids) {
        $caseArguments += @("--case", $solverCase)
    }
```

Existing runner, once for each approved preserved executable/output:

```powershell
    & $env:POECRAFT_PYTHON -m poecraft_ingest.solver_corpus_runner --root . --executable EXE --artifact FROZEN_MATCHED_ARTIFACT --corpus $cohort.corpus --solver-mode current --max-workers 1 --host-watchdog-seconds 165 @caseArguments --output NEW_RUN
```

Existing reporter:

```powershell
    & $env:POECRAFT_PYTHON -m poecraft_ingest.solver_reports --run baseline=BASELINE_RUN --run candidate=CANDIDATE_RUN --pair baseline:candidate --expected-cohort $cohortPath --economic-gate --output COMPARISON.json
```

Each case retains 120-second requested Finish, 150-second native watchdog, 1 GiB
solver cap, 1 GiB exact evaluator cap and zero Simulator runs. Five serial
165-second host reservations total 825 seconds (13.75 minutes) per arm, or
27.5 minutes for a fresh pair, before separately admitted setup and cleanup.
These are nominal reservations: main's final communicate() drain is unbounded
after process-tree termination. This patch does not import the experimental
runner or claim that 165 seconds enforces an end-to-end bound.

Admission must include evaluator overlap, process overhead and available host RAM
through the existing worker-headroom and memory-budget options. Use the same
approved reservation/load controls in both arms. No automatic concurrency or
new worker orchestration is added. Respect gaming quiet windows.

The economic gate requires a nonempty predeclared cohort, exactly one report and
a completed eligible raw ledger outcome for every expected case in each arm,
no unexpected cases, matched comparison identity, and complete independently
checked costs with no material increase. Duplicate report IDs and duplicate
JSON keys refuse rather than overwrite evidence. Missing reports remain in
cohort accounting. Failed/canceled/refused and partial watchdog outcomes remain
visible and cannot pass; a native expectation miss remains a completed
measurement but also cannot pass. Legacy ungated comparison remains an
analytical workflow; a missing declaration cannot produce a release gate pass.

Checked cost still requires matched completed/converged evaluation, complete
reconciled spend, zero off-policy mass, success within the existing numerical
tolerance, and finite nonnegative total_expected_cost. The solver scalar is not
a replacement. Input/economy/action/law/checker/cap/runtime/machine identity
checks remain in the existing comparison owner. Existing cost tolerances remain.

This gate establishes matched checked-policy economics only. It does not prove
global optimality, Current exact closure, a capability ceiling, WASM activation,
browser responsiveness or whole-release qualification. Native resource stops
are completed bounded measurements where appropriate; watchdog censoring is
not native exact closure. Existing exact-closure profiles remain separate.

Validation requested: one finite synthetic batch for
tools/ingest/tests/test_solver_reports.py, with a 120-second total deadline,
through the parent/CI owner after LOCAL admission. It exercises symmetric and
one-sided missing cases, missing files, failure/cancel/refusal, partial censoring,
duplicates, identity/correctness mismatch, native expectation misses,
unexpected IDs, valid complete control and CLI economics. No native build,
solver, Simulator, full CI or dependency installation belongs to that batch.
