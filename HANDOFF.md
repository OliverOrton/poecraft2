# Session Handoff — Strategy evaluator Phase C.1 complete

Written 2026-07-14 after completing the exact-evaluation loop acceleration and
progress/cancellation milestone. Read [AGENTS.md](AGENTS.md), then
[docs/direction.md](docs/direction.md), then this file. Strategy Calculator
Mode design and the completed phase record live in
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md).

## Current state

Strategy Calculator Mode Phases A-C.1 are complete in the local commit that
contains this handoff. Phase C.1 replaced whole-graph probability sweeps with a
one-time reachable `(compiled node, abstract state)` transition graph and an
SCC condensation solver:

- acyclic singleton components flow directly;
- singleton self-loops use the geometric closed form;
- identical-row cyclic components use an exact rank-one closed form (the hot
  Alteration/reforge-loop path);
- small general cyclic components use a pivoted dense solve;
- large or ill-conditioned components use a bounded local iterative fallback;
- closed recurrent components become unresolved immediately, with deterministic
  entry-node attribution.

Result reconstruction still produces the existing v1 terminal, failure,
expected action/consumption, node-class, and edge-flow contract. The synchronous
`pc_strategy_evaluate` API drives the same new work object internally. The
additive begin/step/finish/destroy C ABI exposes real discovery/SCC/fallback/
finalization progress and lets WASM abandon obsolete work safely.

The worker now runs adaptive ~16 ms evaluation chunks, limits progress messages
to about 10/s, accepts an `AbortSignal`, and destroys both temporary compiled
strategy and evaluation handles on success, refusal, error, and cancellation.
Strategy Builder structural edits abort the in-flight evaluation immediately;
leaving Calculator mode and document disposal do the same. The last result and
stale annotations remain visible until the replacement finishes.

## Performance result

`npm run benchmark:strategy-eval` is the opt-in pinned Node/WASM worker gate.
Its final warmed medians were:

- T1 `+(91-100) to maximum Energy Shield`: 1,434 ms before, 31.639 ms after,
  **45.32× faster**; callback-enabled median 31.440 ms (-0.200 ms,
  measurement noise/no observed overhead);
- lower-hit-rate T1 hybrid Energy Shield/Life: 3,510 ms before, 31.243 ms
  after, **112.35× faster**; callback-enabled median 31.379 ms (+0.136 ms).

Both cases independently pin terminal probability, expected actions,
consumption, edge flows, and zero fallback sweeps. The target was at least 5×
per graph and callback overhead no more than 2% or 2 ms.

## Verification completed

- `powershell -File scripts/build.ps1` — pass;
- direct native suite — 118,414 checks, 0 failures;
- `powershell -File scripts/build-wasm.ps1` — pass;
- `npx tsc --noEmit` — pass;
- `npm test` — pass, including 21/21 worker smoke checks;
- `npm run build` — pass;
- `npm run benchmark:strategy-eval` — both performance/overhead gates pass;
- `powershell -File scripts/test.ps1` — pass;
- separate headless Chrome process — progress text observed, a burst of
  structural replacements ended on the newest 81.7014428413-action graph,
  final surface reported `direct SCC solve`, and the console/error capture was
  empty. Codex browser tooling was not used.

Native coverage now includes hand-computed geometric and two-node cyclic
systems, test-only high-precision forward-reference parity, retained MC gates,
immediate self/router closed SCCs, a converged near-closed exit, forced
large-SCC fallback conservation, explicit pair-cap failure, stepped progress,
and byte determinism. Worker coverage includes ordered progress, prompt repeated
cancellation, stable live-handle count, and an obsolete-request burst where only
the newest graph completes.

## Next task

Resume **`docs/s6-plan.md` Phase 1**. Phase D in
`docs/strategy-calculator-mode-plan.md` remains an unscheduled follow-up. Do not
silently start Phase D while doing s6 work.

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
- `apps/web/test/strategy-eval-benchmark.ts`

## Rulings and gotchas

- Exactness remains `1e-12` by default. The speedup does not loosen epsilon,
  drop outcome classes, or change simulator-parity absorption semantics.
- `pc_strategy_eval_options.max_pairs` defaults to 1,000,000 and is distinct
  from `max_states`; exceeding it returns `PC_RESULT_CAPACITY_EXCEEDED` with an
  explicit `max_pairs` message.
- The result JSON's existing `sweeps` field now counts local fallback sweeps.
  Direct SCC solves report 0 and the UI labels them `direct SCC solve`.
- Closed recurrent SCC visit counts are mathematically infinite. The finite v1
  node-class surface records their entry snapshot and reports all entering mass
  as unresolved instead of fabricating repeated traversals.
- Do not remove the rank-one solve as an apparent special case: exact reforge
  and Alteration loops produce many pair states with identical internal rows;
  this is the path responsible for the pinned speedup.
- SCC discovery contains every positive-probability reachable transition; it
  never uses a mass cutoff. Epsilon applies only to local fallback termination.
- Web progress is engine phase/count presentation only. Probability, routing,
  legality, and SCC membership remain native-engine authority.
- Cancellation is handle destruction, not a partial result. Always retain the
  worker `finally` cleanup for both temporary handles.
- Do not use Codex's built-in/in-app browser for this repository. Use a separate
  headless browser process when a browser smoke is required.
- PoE1 mechanic ambiguity still goes directly to Oliver; never research or
  guess.
- Commits are local-only unless Oliver explicitly asks to push.
