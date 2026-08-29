# Native Solver Lab CLI-First MCP Removal v1 Result

**Status: completed on 2026-08-29.** Final implementation checkpoint:
`48da3eeaa7fa03d890c3353eb0758d58694d537c`.

## Outcome

The Native Solver Lab now has one supported automation interface: the existing
`poecraft-solver-lab` JSON CLI over `SolverLabService`. The PySide6 GUI and
independent supervisor use that same service and catalog. Completed attempts
continue to retain their complete reports, logs, strategies, finalization
records, hashes, identities, events, and controlled investigation bundles.

No replacement output protocol was added. The CLI's
`solver_lab_operation_result_v1` envelope is sufficient for terminal
automation, while targeted `job`, `run-summary`, `bound-trace`, `compare`,
`strategy-summary`, `evaluate-strategy`, and `export-bundle` commands avoid
unnecessarily dumping the full historical catalog.

## Removed Surface

- Solver Lab MCP adapter module and all transport tests;
- `mcp>=2.1,<3` project dependency;
- `poecraft-solver-lab-mcp` console entry point;
- current installation and registration guidance;
- user-local `poecraft2-native-solver-lab` Codex registration; and
- four live repository MCP launcher trees.

The obsolete executable and pip rollback metadata created by an interrupted
Windows reinstall were removed from the Python environment and sent to the
Windows Recycle Bin. They remain recoverable there but are not installed,
registered, importable, or running. The shared `mcp` Python distribution was
not uninstalled because it is environment-wide and may serve unrelated tools.

Historical archives retain accurate descriptions of their MCP-era evidence;
they do not describe the current supported surface.

## Preserved Guarantees

Service-level matrix, trace, comparison, strategy, exact-evaluation, and
bundle tests were retained outside the deleted transport module. The real
Windows cancellation regression continues to qualify service, CLI, and GUI
paths through process-tree removal, terminal state, cancellation
acknowledgment, lease release, and zero remaining host reservation.

Direct supervisor tests own singleton dispatch, concurrent acquisition,
crash recovery, watchdog, memory admission, and cancel/retry guarantees. The
updated accelerated unattended harness selected 13 direct tests and reported
22 passing tests. The complete focused Lab suite reported 67 passed.

An installed-CLI isolated exercise returned one profile, exactly seven frozen
cases, both fragment control/shadow IDs, stable dry-run solve identity, no
persisted dry-run work, zero reservation, released ownership, and a resumed
queue.

## Runtime Decommission

Before shutdown, the CLI found zero nonterminal jobs and attempts. Codex
registration was removed, then only the eight exact wrapper/Python processes
belonging to the four repository MCP trees were terminated. CLI supervisor
`supervisor-b0b55db4-3e0e-4d5a-a07d-8f7a712633bd` recovered the dead owner,
found no work, and released the catalog reservation cleanly.

Final checks found no registration, MCP process, MCP console executable, or
importable MCP adapter. The CLI executable remains installed.

## Acceptance

The final `powershell -File scripts/test.ps1` passed with 3,417,655 native
checks and zero failures, all 13 solver benchmark specifications, 28/28
release-WASM checks, every ingest/database/artifact/binding/web suite, and the
required Simulator controls. No tracked artifact changed.

This boundary changed no solver, mechanic, action envelope, planner,
graph-fragment behavior, catalog schema, identity definition, C ABI, strategy
vocabulary, WASM behavior, or product UI. No rendered/visual review and no
push were performed. The pre-existing untracked file `0` remains untouched.

The earlier unattended-hardening six-hour soak remains owner-waived, not
passed, and is not overnight qualification.
