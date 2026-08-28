# Native Solver Lab GUI Stabilization Result

**Status: completed.** Source checkpoint: `60bd13f`.

## Outcome

The Native Solver Lab GUI is now a reliable base for further work. Valid
partial reports with explicit-null optional sections produce bounded summaries
instead of `NoneType.get` failures, and one malformed attempt no longer aborts
the complete job list. Typed normalization now covers report, attempt, job,
catalog, supervisor, case-validation, and GUI-detail boundaries.

The GUI no longer reparses every report synchronously every 750 ms. Cached
summary aggregation and selected-detail reads run off the Qt thread, terminal
reports are not reopened, refresh is single-flight, unchanged rows avoid model
resets, and selection is retained by stable job ID. A 100-job test with an
8 MiB partial and 750 ms injected aggregation delay kept an unrelated Cases
mutation below 650 ms while refresh was still running.

Every visible Queue, Compare, Strategy, Matrix, and Cases action now has an
explicit enabled/busy contract. Invalid direct invocation produces a useful
persistent rejection instead of a silent return. Accepted and rejected
operations append state, identity, and idempotency evidence to the visible
**Activity & Errors** dock and `build/solver-lab/gui-activity.log`; complete
tracebacks and selected job/attempt/case/revision context are retained.

## Cancellation ownership

The supervisor's cancellation path was already behaviorally sound. The GUI
made it appear unreliable because synchronous polling reparsed reports and
reset the table, temporarily destroying selection; handlers then silently
returned. Background refresh, stable selection, explicit state, and durable
feedback repaired the actual owner.

Equivalent real Windows parent/grandchild process trees were canceled through
direct service, JSON CLI, stdio MCP, and the actual GUI button. Every path
completed `running -> canceling -> canceled`, removed the complete process
tree, terminalized attempt and job, released the lease and host reservation,
and used `graceful_then_process_tree_termination`. Acknowledgments were
492.250 ms, 483.748 ms, 473.993 ms, and 480.118 ms respectively, below the
5-second contract.

## Acceptance

- Final focused Lab suite: `34 passed in 21.20s`.
- Rendered PySide smoke: live cancellation, stable selection, correct terminal
  controls, zero released reservation, Cases action state, Matrix dry-run, and
  persistent activity history all passed.
- Full `scripts/test.ps1`: passed on the single final run, including
  `3,417,290` native checks, all `12` solver benchmark specifications,
  `28/28` release-WASM checks, and all remaining layers.

Native validation and frozen/revision immutability were preserved. No native
solver, crafting mechanic, ABI, generated-data contract, strategy vocabulary,
or release-WASM behavior changed, so no WASM rebuild or compiled-strategy
verification was required.

No blocking GUI limitation remains. Intentional Lab constraints are unchanged:
local execution only, no running Pause, and closing the GUI does not silently
cancel live work.
