# Session Handoff — Solver S6 continues: Solve-in-Simulator flow

Written 2026-07-07 after the session that shipped the Calculator tab.
Read [CLAUDE.md](CLAUDE.md) first (commands, constraints, subagents), then
this file. Delete or rewrite this handoff when its work is done.

## Where things stand

Solver phases S1–S5 plus the C ABI, browser plumbing, and now the
**Calculator workspace tab** are done and committed on `main` (not
pushed). Web tests are 16/16 + 12/12 green and `npx tsc --noEmit` is
clean in `apps/web`. Per-phase status lives inline in
[docs/crafting-solver-plan.md](docs/crafting-solver-plan.md) under
"Phasing" — accurate, names every file.

The new tab: `pc-calculator`
([apps/web/src/app/components/pc-calculator.ts](apps/web/src/app/components/pc-calculator.ts)),
registered in `pc-workspace.ts` beside emulator/strategy/stash, opened
from the titlebar "+ Calculator", the Emulator craft bar "Odds" button
(item handoff via snapshot → calculator draft), or "Odds" on Stash item
cards. Supporting pieces added this session:

- `apps/web/src/app/modifier-options.ts` — `buildModifierOptions` moved
  out of pc-strategy-editor so the goal editor shares the family picker
  options.
- `apps/web/src/app/workspace/prices.ts` — localStorage price table
  (manual-override layer of the planned Economy service); the cost
  section's inline price inputs write here and all tabs share it.
- `CalculatorDraftRecord` in `workspace/persistence.ts` (same drafts
  store; calculators are never dirty/saved resources).
- `EngineClient.solverActions(solver, { omitFossilCombos })` — the
  worker drops multi-fossil combo ids from the picker payload (a full
  registry has ~15k of them from 25 named fossils). The tab reassembles
  loadout ids client-side: `"fossil:" + sortedKeys.join("+")`, cost keys
  `fossil:<key>` each + `resonator:<n>` (mirrors `solver_registry.cpp`).
- The tab opens its solver with **no** `actions` subset so any registry
  action id calculates; the picker list is fetched once per session
  (registry depends on session only, not goal).

## Next task, in priority order

1. **Solver-in-Simulator flow**: goal editor + "Solve" in the Simulator
   or Strategy Builder context → `solverSolve` (needs a `pc_economy`
   from the price table; `loadEconomy({version:"v1", prices})`) →
   `solverCompileStrategy` → open the strategy JSON in the Strategy
   Board. Nodes carry `expected_cost` annotations the board can display.
   The end of the browser acceptance test in
   [apps/web/test/engine-smoke.test.ts](apps/web/test/engine-smoke.test.ts)
   is the exact call sequence.
2. Chunked `pc_solver_solve` with progress/cancel (engine + WASM facade
   change; mirror `runStrategy`'s chunk/yield pattern in
   `engine-worker.ts`). Long goals currently block the worker.
3. Emulator ambient odds ("odds before you click", plan §Watched
   Modifiers): same `solverCalc` against the live emulator item.
4. Veiled/eldritch calc evaluators (`solver_calc.cpp` `evaluate()`,
   currently `supported=false`) and junk-count/flag condition types
   (`solver_compile.cpp` throws precise "vocabulary gap" errors).

## Rulings and decisions already made (don't re-ask, don't re-derive)

- **Mechanic rules are decided by Oliver.** Ask him directly; never
  research online unless he points you at the official wiki.
- Harvest augment add-then-remove is intentional (noted in
  `solver_registry.cpp`); harvest resist conversion is fire/cold/
  lightning ordered pairs only; all 1–4 fossil loadouts are plannable
  actions; nameless `RandomFossilOutcome*` rows are excluded by empty
  display name.
- WASM **is** rebuildable: `scripts/build-wasm.ps1` self-activates the
  emsdk at `C:\emsdk`. No rebuild was needed this session (no C ABI or
  facade change).
- Oliver is fine with single big commits; end messages with the Claude
  co-author line; "continue with whatever is next" means pick the plan's
  next phase and drive to a gated, pipeline-green milestone.

## Gotchas that cost time (this session's additions at top)

- `pcw_*` JSON serializes doubles with `std::to_string` (6 decimals):
  outcome probabilities over many classes sum to ~1 ± 1e-3, and reforge
  evaluators carry a ≥ 99.5% coverage budget — write assertions
  accordingly (see the calculator picker test).
- Dockview detaches inactive panels' DOM (`document.querySelector` on a
  background tab's content returns null); activate via
  `panel.api.setActive()` in tests.
- A goal family picked from the full option list can be bench/veiled-
  only — 0% odds under pool actions is correct engine behavior, not a
  bug (Jun veiled families burned 20 minutes this session).
- `tsx` does not type-check — run `npx tsc --noEmit` in `apps/web`.
- Engine tests: add new files in **three** places (CMakeLists,
  `scripts/build.ps1` TestSources, `tests.hpp`/`test_main.cpp`); run the
  binary directly for iteration:
  `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`.
- Goal groups/mods for artifact tests must be positively weighted under
  the base signature (`positive_base_weight_mask`).
- Metamod locks do NOT close a side for add-actions; MC gate
  conventions: 20–50k samples, per-outcome 5σ + small slack.

## Pending with Oliver

- Two task chips: fix stale claims in `docs/direction.md`; mark
  non-socketable fossil rows in ingest.
- Commits are local only — ask before pushing if it comes up.
