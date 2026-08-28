# Native Solver Lab v0 Execution Log

**Status: proposed; implementation has not started.**

Parent: [Plan](plan.md)

## Starting boundary

- Proposed from repository HEAD `769c3de` (`Record stopped PDR replay
  boundary`).
- Working tree was clean before the research reports were archived and this
  proposed plan was written.
- Current gate: none; awaiting Oliver's activation.
- No Solver Lab source, dependency, GUI, catalog, supervisor, CLI, or MCP
  implementation exists yet.

## Decisions already made

- First boundary is Native Solver Lab v0 only.
- GUI is part of the first usable vertical slice, not a later polish project.
- JSON CLI and local MCP use the same typed service as the GUI.
- The existing native benchmark and engine remain the only solve/mechanics
  authority.
- Scheduler replay, solver behavior, proof work, options, RCASSP, learned
  guidance, Imprint, and release-WASM changes are excluded.

## Next executable step after activation

Run Gate 0: record the then-current HEAD/dirty paths, validate the current
runner and native smoke, establish optional PySide6/MCP dependencies, and land
the versioned Lab contracts/profile/corpus without beginning a broad matrix.
