# Session Handoff — Solver S6: Calculator Tab and Workspace Integration

Written 2026-07-07 at the end of the session that built the crafting solver.
Read [CLAUDE.md](CLAUDE.md) first (commands, constraints, subagents), then
this file. Delete or rewrite this handoff when its work is done.

## Where things stand

Solver phases S1–S5 plus the C ABI and browser plumbing are **done, gated,
and committed** (`9c08637`, `91627f6` on `main`; not pushed). The full
pipeline (`scripts/test.ps1`) and web tests are green: engine suite at
~102k checks / 0 failures, web 15/15 + 12/12. Per-phase status lives
inline in [docs/crafting-solver-plan.md](docs/crafting-solver-plan.md)
under "Phasing" — that section is accurate and names every file.

The stack, end to end: goal-spec JSON → `pc_solver_create` → exact outcome
distributions (`pc_calc_action_outcomes`), value-iteration solve against a
`pc_economy` price table, policy → ordinary strategy JSON (with a new
`restart` strategy operation), verified by simulation — natively, through
the public C API, and inside the WASM worker. The load-bearing invariant,
enforced by tests at every layer: **empirical mean cost from simulating
the compiled policy matches V(start)** (e.g. 6.8750 vs 6.8754 at 50k runs).

## Next task: the Calculator workspace tab (S6 UI)

Everything below the UI is ready. `EngineClient`
([apps/web/src/app/engine-client.ts](apps/web/src/app/engine-client.ts))
already has typed methods: `openSolver(session, goal)`, `solverActions`,
`solverCalc(solver, item, actionId)` (exact odds + per-slot hit
probabilities), `solverSolve`, `solverCompileStrategy`, `solverLog`.
Types are in `engine-protocol.ts` (`SolverGoal`, `CalcResult`, …). The
browser acceptance test at the bottom of
[apps/web/test/engine-smoke.test.ts](apps/web/test/engine-smoke.test.ts)
is a working usage example of the whole flow.

Build the tab per [docs/desktop-workspace-ui.md](docs/desktop-workspace-ui.md)
and the plan's "Workspace Integration" section:

- New `pc-calculator` custom element (no React, plain CSS tokens from
  `src/styles/tokens.css`); register it as a workspace document component
  in [pc-workspace.ts](apps/web/src/app/components/pc-workspace.ts) —
  follow the `emulator`/`strategy`/`stash` registration pattern around
  lines 85–93.
- Inputs: an item (Stash pick, Emulator handoff, or built in place — the
  Emulator import path in the workspace model shows the stable-key item
  handoff), one action (picker fed by `solverActions`; the goal's
  `actions` subset keeps it small), and a goal (slots by group/family —
  the condition editor's mod pickers and the worker `catalog` method
  already expose group keys and mod keys).
- Output: outcome table over goal-relevant classes (probability, rarity,
  counts, slot statuses), per-slot hit odds, expected cost from the
  action's `cost_keys` dotted with the active economy.
- The plan also wants Emulator "odds before you click" — same
  `solverCalc` call against the live emulator item; consider it once the
  tab works.

After the tab, in rough priority order: solver-in-Simulator flow (solve →
open compiled strategy in the Strategy Board; nodes carry `expected_cost`
annotations the board can display), chunked `pc_solver_solve` with
progress/cancel (mirror `runStrategy`'s chunk/yield pattern in
`engine-worker.ts`), veiled/eldritch evaluators (in
`engine/src/solver_calc.cpp` `evaluate()`, currently `supported=false`),
and junk-count/flag condition types (the compiler in `solver_compile.cpp`
throws precise "vocabulary gap" errors listing exactly what's missing).

## Rulings and decisions already made (don't re-ask, don't re-derive)

- **Mechanic rules are decided by Oliver.** Ask him directly; never
  research online unless he points you at the official wiki for a
  specific check.
- Harvest augment's add-then-remove behavior is **intentional** (his
  ruling; noted in `solver_registry.cpp`).
- Harvest resist conversion: fire/cold/lightning ordered pairs only.
- Fossils: all 1–4 loadout combinations are plannable actions; the 420
  nameless `RandomFossilOutcome*` rows are Tangled Fossil internals,
  excluded by their empty display name (an ingest-flag cleanup chip is
  pending with Oliver).
- WASM **is** rebuildable: `scripts/build-wasm.ps1` self-activates the
  emsdk at `C:\emsdk`. The dist is gitignored, so rebuild after pulling
  engine changes.
- Oliver is fine with single big commits; end messages with the Claude
  co-author line. He runs sessions with "continue with whatever is next"
  — pick the plan's next phase and drive to a gated, pipeline-green
  milestone.

## Gotchas that cost time this session

- `tsx` does not type-check — run `npx tsc --noEmit` in `apps/web`.
- Engine tests are white-box: they include `../src/*.hpp` and build
  synthetic 8-mod `SessionImpl`s (the pattern is duplicated per test
  file deliberately; weights 100 except mod 7 at 400).
- Run the engine test binary directly for fast iteration:
  `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`.
- New engine test files must be added in **three** places: CMakeLists,
  `scripts/build.ps1` TestSources, and `tests.hpp`/`test_main.cpp`.
- Goal groups/mods for artifact tests must be positively weighted under
  the base signature (`positive_base_weight_mask`), not merely rollable.
- Metamod locks do NOT close a side for add-actions (`open_side_filter`
  checks caps only); internal `apply_action` may leave partial mutations
  on failure — the C ABI and evaluators treat unapplied as unchanged.
- MC gate conventions: 20–50k samples, per-outcome 5σ + small slack,
  coverage ≥ 99.5% for reforges (evaluator truncation budget).

## Pending with Oliver

- Two task chips: fix stale claims in `docs/direction.md`; mark
  non-socketable fossil rows in ingest.
- Commits are local only — ask before pushing if it comes up.
