# Native Solver Lab GUI Stabilization Execution Log

**Status: active; Gate UI-0 reproduction in progress.**

Parent: [Plan](plan.md)

## Starting boundary

- Selected from clean local `main` checkpoint `ee18cc2`.
- The prior case-authoring boundary was complete and archived.
- Scope is Python Lab service/catalog/supervisor/GUI, the closed CLI/MCP
  adapters, Calculator handoff parsing, tests, and documentation only.
- No native engine, solver, ABI, generated data, or release-WASM change is
  authorized.

## Required evidence

- Full pre-fix explicit-null traceback.
- Root cause for every repaired null-chain failure.
- Exact cancellation failure owner and acknowledgement/termination mode.
- Null-summary, large-refresh, cancellation, and complete button-matrix tests.
- Rendered manual smoke result and any remaining usability limitations.

## Gate UI-0 pre-fix reproduction — 2026-08-28

The focused reproduction wrote a syntactically valid `partial.json` whose
optional native sections were present with JSON `null`, then called the same
`SolverLabService._run_summary()` used by GUI refresh. The failure reproduced
on clean selection checkpoint `b7b383e` before parser behavior changed.

Complete Python traceback (the stack has exactly the test caller and service
parser frames):

```text
Traceback (most recent call last):
  File "tools/ingest/tests/test_solver_lab_gui_stabilization.py", line 38, in test_partial_report_with_explicit_null_sections_has_bounded_summary
    summary = service._run_summary(
        {
            "attempt_id": "attempt-1",
            "job_id": "job-1",
            "status": "running",
            "directory": str(attempt_directory),
            "result": None,
        }
    )
  File "tools/ingest/poecraft_ingest/solver_lab_service.py", line 1513, in _run_summary
    lower = solve.get("lower_bound", last.get("lower_bound"))
            ^^^^^^^^^
AttributeError: 'NoneType' object has no attribute 'get'
```

Immediate owner: report normalization in the Lab service, not Qt or the native
worker. `dict.get(key, {})` only substitutes the default for an absent key; an
explicit JSON `null` remains Python `None`. The parser repeated this assumption
for solve, telemetry, policy, execution, work, memory, timings, bound trace,
samples/latest sample, and several later summary consumers. The broader audit
remains open until those boundaries and the GUI/case/supervisor surfaces are
normalized.

## Gates UI-1 through UI-5 implementation result — 2026-08-28

### Null and diagnostic ownership

- The reproduced exception had one direct root: `_run_summary()` retained an
  explicit-null `solve_summary` and then called `solve.get`. The same latent
  shape assumption existed at telemetry child, bound-trace, strategy exact-
  evaluation, attempt-result, catalog JSON, supervisor queued-request, case-
  authoring profile, and GUI detail boundaries. No second independent
  `NoneType.get` traceback reproduced.
- Shared `as_mapping`, `as_list`, and `first_mapping` helpers now normalize
  those boundaries. A report whose optional sections are all null produces an
  empty bounded summary; report/cases/case nulls and null latest samples also
  remain usable. A malformed attempt becomes an isolated warning row rather
  than aborting the job snapshot.
- The GUI now has one persistent **Activity & Errors** dock. Every error entry
  records UTC timestamp, operation, complete traceback, and selected job,
  attempt, case, draft, and revision identity. Activity and errors append to
  the controlled `build/solver-lab/gui-activity.log`; health polling does not
  clear either surface.

### Refresh and action ownership

- The prior GUI synchronously listed jobs and reparsed every attempt report on
  Qt every 750 ms, reset the table on every poll, and then reconstructed
  selection. During that reset, action handlers could observe no selection and
  silently return. This is the GUI owner for the reported apparent button and
  Cancel failures.
- Summaries are now cached by attempt ID plus report path, nanosecond mtime,
  size, and attempt status. Unchanged active artifacts reuse the cache;
  terminal artifacts become immutable cache entries. Job aggregation and
  selected detail reads run off the Qt thread, refresh is non-reentrant, the
  default poll is 1.5 seconds, unchanged models are not reset, and job
  selection is restored by stable ID.
- The 100-job witness included one 8 MiB null-bearing partial report and an
  injected 750 ms aggregation delay. A Cases mutation completed in under
  650 ms while refresh remained in flight; the final table contained all 100
  jobs, an unchanged refresh emitted no model reset, and the selected running
  job survived.
- Queue, Cases, Matrix, Compare, and Strategy controls now have explicit
  validity/busy contracts. Conflicting queue, case, and matrix mutations are
  single-flight. Invalid direct handlers append useful rejection text rather
  than returning silently. Mutation feedback includes accepted/rejected,
  identity, previous/requested/current state, and idempotency key where the
  service contract has one.
- Saving a revision requires the exact currently displayed canonical case
  digest, current native-valid result, and unchanged draft name. Submitting
  from Cases requires a selected saved immutable revision. Native validation
  and frozen/revision immutability were not weakened.

### Live cancellation qualification

The supervisor cancellation implementation did not require a behavioral
repair. Four equivalent running jobs were canceled through direct service,
JSON CLI, typed stdio MCP, and the actual GUI button. Each job ran a real
Windows process group containing a Python child and grandchild, exposed a
valid partial report with null nested sections, and completed this lifecycle:

```text
running -> canceling/cancel_requested -> worker observes request
-> CTRL_BREAK attempted -> verified process-tree termination
-> no parent or grandchild survivor -> canceled attempt and job
-> released lease -> zero supervisor host reservation -> GUI canceled
```

All four used `graceful_then_process_tree_termination`. Measured
acknowledgment was 492.250 ms (service), 483.748 ms (CLI), 473.993 ms (MCP),
and 480.118 ms (GUI), below the pinned 5,000 ms limit. The GUI timer remained
active, the selected row survived terminal refresh, and persistent cancellation
feedback remained after another poll.

### Focused automated evidence

- 34 focused Lab tests passed in 23.10 seconds across stabilization, service,
  catalog, supervisor, GUI, CLI/MCP, and contracts.
- The action matrix clicks and counts exactly one intended call for Submit,
  queued/live Cancel, Retry, Clone, priority, Pause, Resume, Refresh, Compare,
  Strategy summary, bundle export, all three Matrix actions, and all Cases
  create/clone/import/update/validate/revision/submit/copy/discard actions.
- Invalid-state coverage checks persistent explanations and disabled-state
  contracts; timer refresh cannot erase selection or feedback.

Rendered smoke and the final consolidated repository acceptance remain open.
