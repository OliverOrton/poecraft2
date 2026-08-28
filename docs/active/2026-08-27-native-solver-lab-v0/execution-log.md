# Native Solver Lab v0 Execution Log

**Status: active; Gate 0 in progress.**

Parent: [Plan](plan.md)

## Starting boundary

- Proposed from repository HEAD `769c3de` (`Record stopped PDR replay
  boundary`) and activated from clean checkpoint `bd86b46` (`Plan native
  solver lab v0`).
- Working tree was clean at activation.
- Current gate: Gate 0 — current-tree contract and dependency baseline.
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

## Next executable step

Inspect current runner/benchmark/report contracts, validate the existing runner
tests and one direct native smoke, then land optional dependency groups plus
the versioned Lab contracts/profile/corpus without beginning a broad matrix.
