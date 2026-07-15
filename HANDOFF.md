# Session Handoff - S6 Phase 2 complete

Written 2026-07-15 after S6 Phase 2, "Chunked solve with progress and cancel,"
completed. Read [AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md),
this file, then [docs/s6-plan.md](docs/s6-plan.md). S6 Phase 3 is next; it has
not started.

## Product result

Solve remains in Calculator's approved natural lower workspace, spanning the
center and right columns. Its idle state is still minimal: readiness, the
collapsed shared price table, and `Start solve`. While native work is active,
Start is replaced by `Cancel` and one compact row shows only real progress:

- expansion/iteration phase;
- reachable states expanded;
- completed Bellman sweeps;
- current residual;
- the descending `V(start)` upper bound.

Cancel sends an AbortSignal through `EngineClient`, the worker abandons native
partial state, and the panel returns to a restartable idle state. A completed
solve still follows the Phase 1 compile -> auto-layout -> Strategy Board ->
5,000-run verification flow unchanged.

## Native and worker contract

`engine/src/solver_solve.cpp` now exposes one internal `SolveWork` state
machine. Each expansion work item processes one reachable abstract state; each
iteration item performs one deterministic in-place Bellman sweep; `finish()`
alone extracts the policy. Both synchronous `pc_solver_solve` and the stepped
surface drive this same implementation, so native behavior did not fork.

The added C ABI is:

```text
pc_solver_solve_begin
pc_solver_solve_step
pc_solver_solve_finish
pc_solver_solve_abandon
```

Progress uses the usual `struct_size` / `abi_version` convention and reports
`expanding | iterating | done`, state/sweep counts, residual, and the current
start-value bound. Begin snapshots priced actions from the economy; abandon
discards partial work.

`bindings/wasm/wasm_api.cpp` mirrors the four calls. The worker adapts expansion
chunks toward a short slice, keeps iteration at one sweep per chunk, yields a
timer-task turn immediately so AbortSignal messages land, and returns a
`{cancelled:true, progress}` result after abandon. Existing strategy-run
progress remains backward-compatible.

## Verification

- Native build and engine suite pass: `123739 checks, 0 failures`.
- Native tests compare synchronous and stepped summary, values, policy, and
  diagnostics across assorted budgets on the synthetic and Vaal Regalia
  fixtures; the public C ABI test covers abandon and restart.
- WASM rebuilt through `scripts/build-wasm.ps1`.
- `npx tsc --noEmit`, `npm test`, and `npm run build` pass in `apps/web`.
- `scripts/test.ps1` passes end to end.
- The worker smoke uses a multi-slot goal, observes at least two progress
  events, cancels via AbortSignal, and then completes the normal solve flow.

Separate headless Chrome exercised progress -> cancel -> restart -> converged
solve with zero console errors. During the live two-goal solve it showed `109`
states, `5,450` sweeps, residual `2.47e-2`, and `V(start) 733.2208c`; Cancel was
enabled and returned a restartable panel. The next one-goal solve converged at
`17.5403c` (`49` states, `1,341` sweeps). The outer Solve bounds were
`371.109375..1423.984375px`, exactly matching Modifier Pool left and Odds
right. Capture:
`design/mockups/s6-solve-panel/implemented-phase2-progress.png`.

## Worktree caution

This worktree still contains the pre-existing uncommitted Strategy Board
layout/polish changes in `pc-edge-layer.ts`, `pc-strategy-board.ts`,
`pc-strategy-editor.ts`, and `app.css`, plus the uncommitted S6 Phase 1 slice.
Phase 2 preserves those changes and overlaps Calculator/CSS/docs. Inspect and
stage deliberately; do not assume every current diff belongs solely to Phase
2. Commits remain local-only unless Oliver explicitly asks to push.

## Next boundary

S6 Phase 3 is Emulator ambient odds (watched modifiers plus engine-returned
per-action odds). Follow the image-model design loop in `docs/s6-plan.md`
before implementation. The plan explicitly calls out the watched-goal rarity
default as a mechanic/product choice to confirm with Oliver. Do not begin Phase
4 or fold its evaluator/condition-vocabulary work into Phase 3.
