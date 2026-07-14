# Session Handoff — Strategy evaluator Phase C.1 next

Written 2026-07-14 after Oliver scheduled the exact-evaluation loop
acceleration/progress pass. Read [AGENTS.md](AGENTS.md), then
[docs/direction.md](docs/direction.md), then
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md).

## Current state

Strategy Calculator Mode Phases A-C are complete in local commit `41bb721`:
native whole-graph evaluation, WASM/worker/client transport, and the approved
Strategy Builder UI with exact summary/costs, node and edge annotations,
selected-node incoming classes, persistence, stale/refusal/unresolved states,
and tests. The worktree was clean before this planning update.

The evaluator in `engine/src/solver_eval.cpp` is exact but currently performs
whole-graph forward mass propagation one graph hop per sweep. Action outcomes
are cached, yet low-probability exits still need thousands of sweeps to drive
the remaining transient tail below the default `1e-12` epsilon. The worker
calls it synchronously, so the UI cannot receive real progress or cancel an
obsolete evaluation.

Measured through the same Node/WASM worker path as the web tests:

- T1 `+(91-100) to maximum Energy Shield` Alteration loop:
  1,434 ms, 2,219 sweeps, 81.7014428412 expected actions;
- lower-hit-rate T1 family with the same graph shape:
  3.51–3.64 s, 4,448 sweeps, 162.4028856824 expected actions;
- session creation: about 15 ms.

## Next task

Implement **Phase C.1 only** from
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md):

1. discover the reachable `(compiled node, abstract state)` transition graph
   once;
2. condense it into SCCs and solve acyclic flow directly, singleton loops by
   geometric closed form, small cyclic SCCs as linear systems, and large/
   ill-conditioned SCCs with a local iterative fallback;
3. detect closed recurrent SCCs as unresolved immediately;
4. reconstruct the existing exact result contract from solved pair visits;
5. add a stepped native C ABI while preserving synchronous
   `pc_strategy_evaluate`;
6. drive adaptive WASM worker chunks with real progress and cancellation, and
   cancel obsolete Strategy Builder evaluations on structural changes;
7. meet the numerical, MC, cancellation, leak, and performance gates written
   in Phase C.1.

Stop after C.1. Do not begin Phase D or `s6-plan.md` Phase 1. End with one
local commit and no push.

## Important files

- `docs/strategy-calculator-mode-plan.md`
- `engine/src/solver_eval.cpp`
- `engine/src/solver_api.cpp`
- `engine/include/poecraft/solver.h`
- `engine/src/solver_internal.hpp`
- `engine/tests/test_solver_eval.cpp`
- `bindings/wasm/wasm_api.cpp`
- `scripts/build-wasm.ps1`
- `apps/web/src/app/engine-protocol.ts`
- `apps/web/src/app/engine-wasm.ts`
- `apps/web/src/app/engine-worker.ts`
- `apps/web/src/app/engine-client.ts`
- `apps/web/src/app/components/pc-strategy-editor.ts`
- `apps/web/src/app/components/pc-strategy-odds.ts`
- `apps/web/test/engine-smoke.test.ts`
- `apps/web/test/strategy-calculator-mode.test.ts`

## Rulings and gotchas

- Exactness remains `1e-12` by default; do not make the feature faster by
  loosening epsilon, dropping outcome classes, or changing simulator-parity
  absorption semantics.
- Transition probabilities and routes are engine authority. Web progress is
  phase/count presentation only.
- Keep the existing synchronous ABI as a wrapper over the new stepped engine;
  WASM/web use begin/step/finish/destroy.
- Result JSON fields stay compatible. Numerical ordering may change, but each
  converged value must match the high-precision reference within the planned
  tolerance and identical input must remain byte-deterministic.
- New graph edits must abort/abandon the in-flight evaluation rather than wait
  for it and then queue another. Always destroy the temporary compiled handle
  and evaluation-work handle on success, refusal, error, and cancel.
- The existing Phase C evaluating state is the UI design. No image-model loop
  is required for its progress text; do not move analysis into the right graph
  inspector.
- Do not use Codex's built-in/in-app browser; Oliver reports it crashes the
  app. Use a separate headless browser process for the final UI smoke.
- PoE1 mechanic ambiguity goes directly to Oliver; never research or guess.
- Commits are local-only unless Oliver explicitly asks to push.

## Last green gate

At `41bb721`:

- `powershell -File scripts/build.ps1` — pass;
- direct native suite — 118,285 checks, 0 failures;
- `npx tsc --noEmit`, `npm test`, `npm run build` — pass;
- `powershell -File scripts/test.ps1` — pass;
- separate headless-Chrome Phase C preview — pass, apart from the existing
  favicon 404.
