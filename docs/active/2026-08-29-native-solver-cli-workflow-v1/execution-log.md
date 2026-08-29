# Native Solver CLI Workflow v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Gate 0 activation

- HEAD: `d690a954ccb47d2d041b2faaf9b2963b37eba231`
- Parent: `48da3eeaa7fa03d890c3353eb0758d58694d537c`
- Branch: `main`
- Upstream divergence: `origin/main...HEAD = 0/0`
- Status: only protected `?? 0`; no other dirty path
- Installed `poecraft-solver-lab.exe`: present
- Repository MCP executable/module/registration/processes: absent
- Shared catalog: zero active jobs/attempts, queue resumed, dispatcher released

The existing CLI already owns structured profiles, cases, drafts, revisions,
submissions, matrices, jobs, attempts, summaries, comparison, evaluation,
bundles, cancellation, queue control, and supervision through
`SolverLabService`. The selected work composes those owners; it does not
replace them.

## 2026-08-29 — Gates 1–4 implementation checkpoint

- Added `solver_lab_matrix_v1` and `solver_lab_resolved_matrix_v1` to the
  existing schema registry.
- Added a mechanics-neutral RFC 6901 helper with a registered case-path
  vocabulary, typed and finite values, a 24-hour watchdog ceiling, patch and
  Cartesian limits, canonical patch ordering, and duplicate/overlap rejection.
- Added `SolverLabService.derive_case` as a replay-safe composition of the
  existing draft, structural/profile, native validation, and immutable
  revision owners. Derived case IDs bind the complete base and patch identity,
  rather than catalog allocation order.
- Added `SolverLabService.run_matrix_definition`. It derives content-addressed
  revisions, resolves complete execution requests, writes the immutable
  resolved manifest under `build/solver-lab/matrices` before job submission,
  and then creates deterministic experiments and planned jobs.
- Added filtered dispatch to the existing supervisor. A filtered owner can
  claim only its declared job IDs; a competing legitimate owner remains the
  singleton dispatcher.
- Added `derive-case`, `run-matrix-file`, and `run` CLI adapters. Stdout remains
  one `solver_lab_operation_result_v1` JSON document; changed status is emitted
  only to stderr, and existing large artifacts remain on disk.
- Added the checked-in matrix definition
  `experiments/solver-lab/native-cli-workflow-v1-smoke.json`.
- Focused new workflow tests: `6 passed in 3.46s`.

No solver, ladder, fragment, proof, mechanic, C ABI, WASM, GUI, MCP, or catalog
schema behavior changed. Terminal-only native acceptance and the final focused
and repository suites remain pending.
