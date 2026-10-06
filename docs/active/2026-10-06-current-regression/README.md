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

## Synthetic qualification

Frozen source `bedb1191166ce02fdd90e11064b811b58b0dd5b4`, based directly on
main `7252027c`, passes **50 tests in 2.00 seconds** in the parent-controlled
retry of `tools/ingest/tests/test_solver_reports.py -q`. The unchanged qualified
process owner, absolute selected Python and project PYTHONPATH enforced the
120-second total test allowance with cleanup. Source and fixture pins remained
unchanged; exact process identity and elevated CIM found no survivors. LOCAL
was released at `2026-10-06T03:37:49.4976483Z`.

The original invocation remains a negative: **47 passed, 3 failed** because the
sparse checkout excluded archived research-series inputs. The repair restored
exactly 14 named small tracked references and the specifically authorized
21,470,786-byte preparation JSON from this source's Git objects. The trace was
copied and hashed on disk, with no contents dumped into the conversation. No
test, expected value, frozen price or runtime input was changed. The original
failure and all 15 restored identities were checked again after the retry.
The [compact receipt](checks/synthetic-20261006.json) pins both invocations and
the fixture repair; full logs remain with the CI evidence owner on KIDS.

This checks symmetric and one-sided missing cases, missing files,
failure/cancel/refusal, partial censoring, duplicates, identity/correctness
mismatch, native expectation misses, unexpected IDs, the valid complete control
and CLI economics. It establishes synthetic reporter behavior. Actual native
Current comparisons, solver economics, exact closure, public activation and
whole-release quality remain unqualified by this batch. Integration requires
passing hosted Windows and Solver knowledge checks on the exact published head.
