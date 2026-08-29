# Native Solver CLI Workflow v1 Result

**Status: completed on 2026-08-29.** Final implementation checkpoint:
`0e9d907b0088cd7a99d726d45238b0fffee3a882`.

## Outcome

`poecraft-solver-lab` now provides three small service compositions:

- `derive-case` applies a finite JSON-Pointer patch registry to a cloned
  frozen case or revision, invokes existing native validation, and saves an
  immutable content-addressed revision;
- `run` submits one existing immutable request, dispatches only its job when
  it owns the supervisor, streams changed state to stderr, and returns one
  compact structured terminal result; and
- `run-matrix-file` expands `solver_lab_matrix_v1`, writes an immutable fully
  resolved identity manifest before submission, creates deterministic jobs,
  and can target-wait for the batch.

All outputs retain `solver_lab_operation_result_v1`. Large reports,
strategies, telemetry, logs, and exact evaluations remain in the existing
indexed attempt artifacts.

## Acceptance-Found Repair

Native terminal acceptance exposed a pre-existing local-authoring action-
catalogue defect: rebuilding a product envelope from the public goal dropped
the frozen envelope's fossil controls, leaving derived exact controls in phase
0 until watchdog. The authoring synchronizer now retains existing envelope-
only controls and overlays registered goal edits. A versioned derivation
request prevents old broken results from silently replaying as repaired ones.

Before the repair, derived suffix coordinates recorded zero states at phase 0;
the frozen suffix control completed exact. After the repair, both derived
matrix coordinates progressed through native phases and published naturally,
with one exact strategy. No solver, ladder, fragment, proof, mechanic, ABI,
WASM, GUI, or product behavior changed.

## Acceptance

The fresh terminal-only workflow native-validated and replayed a patched
four-mod revision, retained a truthful 60-second partial result, canceled a
genuinely running second job with no survivor or held reservation, compared
the attempts, read a matched native independent exact evaluation, and ran and
replayed the checked-in two-coordinate matrix with stable manifest/revision/
job identities.

The complete focused Lab suite passed 73 tests. The full repository pipeline
passed 3,417,655 native checks with zero failures, all 13 benchmark
specifications, 28/28 release-WASM checks, and every remaining suite. The
untracked file `0` remains untouched. No GUI, MCP, direct SQLite, alternate
runner, push, or visual review was used.

The earlier unattended-hardening six-hour soak remains owner-waived, not
passed, and is not overnight qualification.
