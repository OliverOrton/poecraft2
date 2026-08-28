# Native Solver Lab GUI Stabilization Plan

**Status: active.** Selected on 2026-08-28.

## Objective

Make the current Native Solver Lab GUI a reliable base for later research:
partial and malformed reports must degrade to bounded summaries, refresh work
must not stall Qt, every visible action must have an explicit state and durable
feedback, and live cancellation must close the real worker/process lifecycle
through service, CLI, MCP, and GUI surfaces.

## Invariants

- The native benchmark remains validation and solve authority.
- Frozen cases and saved local revisions remain immutable.
- One typed service continues to own GUI, JSON CLI, and MCP operations.
- Terminal attempt artifacts remain immutable; UI caches may not alter them.
- No solver, mechanic, ABI, strategy-vocabulary, or release-WASM behavior is
  changed by this boundary.

## Gates

### UI-0 — Preserve diagnostics

Reproduce the inherited `NoneType.get` failure with the complete traceback,
then add one persistent GUI operation/error history with timestamp, full
traceback, selected identity context, and a controlled Lab log file.

### UI-1 — Normalize report boundaries

Introduce typed mapping/list normalization at every native report, attempt,
job, and case-validation boundary. Explicit-null optional sections must yield
a useful bounded summary, and one malformed attempt must not abort the job
list.

### UI-2 — Keep refresh off the Qt hot path

Cache summaries by attempt artifact identity, avoid rereading immutable
terminal reports, aggregate in a background worker, prevent overlapping
refresh, preserve stable selection, and avoid unchanged model resets. Qualify
the design with at least 100 jobs and a deliberately large partial report.

### UI-3 — Make action state and feedback explicit

Define valid and invalid state for every visible button. No handler silently
returns; every mutation records accepted/rejected outcome, affected identity,
previous/requested/current state, and operation or idempotency identity in the
persistent activity history. Draft save and submit remain validation- and
revision-safe.

### UI-4 — Qualify live cancellation end to end

Exercise direct service, JSON CLI, typed MCP, and actual GUI cancellation on
equivalent running jobs. The GUI witness uses an actual child process, active
timer, null-bearing partial report, stable selection, bounded acknowledgement,
verified process termination, canceled attempt/job, and released lease and
host reservation.

### UI-5 — Exercise the complete GUI action matrix

Use offscreen Qt tests to prove every Queue, Compare, Strategy, Matrix, and
Cases action is connected once, responsive, state-correct, and durably
observable in valid and invalid states. Finish with one rendered manual smoke.

### UI-6 — Accept and hand off

Run one consolidated changed-layer/full acceptance pass, update stable Lab
documentation and the execution log, archive this boundary, and create local
checkpoint commits with `Co-authored-by: Codex <codex@openai.com>`.

## Stop conditions

- Do not weaken native validation or revision/frozen-case immutability.
- Do not hide a supervisor/process failure behind GUI-only state.
- Do not resume solver research or add unrelated case-authoring features until
  UI-0 through UI-5 pass.

