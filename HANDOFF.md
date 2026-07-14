# Session Handoff — Solver S6 Phase 1 next

Written 2026-07-13 after the Calculator Variant E, shared item display, and
native success-threshold milestone. Read [AGENTS.md](AGENTS.md), then
[docs/direction.md](docs/direction.md). The remaining ordered plan is
[docs/s6-plan.md](docs/s6-plan.md).

## Current state

Solver S1–S5 and the Calculator S6 foundation are complete. The current
workspace also contains the approved Calculator/UI milestone and is not yet
committed.

Calculator now uses Variant E:

- a stacked left context rail for the concrete Input item and authored Goal;
- one shared `pc-mod-pool`, switching between direct item editing and goal
  selection when a context card is clicked;
- the shared `pc-mod-list` item card, also used by Emulator, with implicits,
  stable prefix/suffix slot positions, actual mod text once, tier labels, and
  player-facing tags;
- selected family/tier highlighting in every modifier-pool instance;
- no detached Current item disclosure and no new Add modifier group control.

The success-definition dropdown is engine-backed, not presentation-only:

- goal JSON accepts optional `min_satisfied_slots` (default all; validated
  `1..slots.length`);
- `CalcContext::is_goal_state` uses finished rarity + the slot threshold, so
  value iteration uses the same success predicate;
- `pc_calc_summary` and WASM JSON return `success_probability`;
- compiled policies use ordinary `all` for all-slot goals and native
  `at_least` for partial thresholds;
- Calculator drafts persist `minSatisfiedSlots`; old drafts load as all.

Odds now leads with the native combined probability, then exact modifier-
coverage buckets and overlapping miss signals. The raw abstract outcome table
is retained under a collapsed Technical distribution drawer with goal-column
labels and success-first sorting. Percentages preserve engine precision; the
headline also shows raw `p`, failure chance, and expected attempts. The price
panel distinguishes action cost per attempt from estimated action spend per
success (`cost / success_probability`) and states that reset/recovery spend is
excluded.

## Verification

- `powershell -File scripts/build.ps1` — pass.
- `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`
  — 102,031 checks, 0 failures.
- `powershell -File scripts/build-wasm.ps1` — rebuilt successfully.
- `npx tsc --noEmit`, `npm test`, and `npm run build` in `apps/web` — pass;
  engine smoke is 16/16.
- Standalone headless Chrome smoke on Vaal Regalia iLvl 86 with two T1 goals
  and Chaos: `All 2` 0.18%, `At least 1 of 2` 8.49%; coverage/miss sections
  render, Technical distribution defaults closed and expands to 40 rows.
  No application page errors; the existing missing favicon is the only 404.
- Precision/cost smoke: 8.4933%, `p = 0.084933`, 91.5067% failure, 11.774
  expected attempts; at 1c per Chaos the panel reports 11.774c estimated
  action spend per success.

## Next task

Implement the Strategy Builder simulator/calculator mode switch per
[docs/strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md)
(scheduled 2026-07-14, ahead of s6 Phase 1). Phase A (engine evaluator +
C ABI) first; the plan records Oliver's scope decisions — do not relitigate
them.

After that, resume [docs/s6-plan.md](docs/s6-plan.md) Phase 1: Solve in the
workspace, then open the compiled policy as a Strategy Board document and
provide its verification run. The engine call sequence is already exercised
in `apps/web/test/engine-smoke.test.ts`. Run the Phase 1 solve panel/board
integration through the same image-model design loop before implementation.

Do not start Phase 2 (chunked progress/cancel) as part of Phase 1. Until Phase
2, the solve call blocks the worker and the UI should state that clearly.

## Important files from this milestone

- `engine/src/solver_internal.hpp`, `solver_calc.cpp`, `solver_api.cpp`,
  `solver_compile.cpp`, `engine/include/poecraft/solver.h`
- `bindings/wasm/wasm_api.cpp` and rebuilt
  `bindings/wasm/dist/poecraft_engine.mjs`
- `apps/web/src/app/components/pc-calculator.ts`
- `apps/web/src/app/components/pc-mod-list.ts`, `pc-mod-pool.ts`
- `apps/web/src/app/item-display.ts`
- `apps/web/src/app/odds-presentation.ts`
- `apps/web/src/app/workspace/persistence.ts`, `engine-protocol.ts`
- `apps/web/src/styles/app.css`
- `design/specs/calculator.md`, `design/specs/item-display.md`

## Rulings and gotchas

- Mechanic rules are decided by Oliver. Ask him directly; do not research or
  guess ambiguous Path of Exile behavior.
- The native engine remains the only crafting-rule authority. Frontend
  coverage/miss summaries may aggregate returned outcome fields, but the
  success predicate and combined probability come from the engine.
- `pcw_*` JSON serializes doubles with `std::to_string` (6 decimals), so large
  distributions sum near 1 rather than bit-exactly 1.
- Rebuild WASM after C ABI or strategy-vocabulary changes with
  `scripts/build-wasm.ps1`.
- `tsx` does not type-check; keep `npx tsc --noEmit` in the web gate.
- Dockview detaches inactive panels; component state must survive reconnect.
- Commits are local only unless Oliver explicitly asks to push.
