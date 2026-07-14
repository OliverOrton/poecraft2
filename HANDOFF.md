# Session Handoff — Strategy Calculator Mode Phase C next

Written 2026-07-14 after completing Phases A and B of
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md).
Read [AGENTS.md](AGENTS.md), then [docs/direction.md](docs/direction.md).

## Current state

Strategy Calculator Mode now has an exact native evaluator and a complete
browser transport path. `EngineClient.strategyEvaluate(session, strategy,
options?)` sends one worker request; the worker compiles the strategy, calls
the evaluator, and destroys the temporary compiled handle in `finally`.

Phase B added:

- `pcw_strategy_evaluate` in `bindings/wasm/wasm_api.cpp`, including all four
  evaluation options and the C ABI query-required-count result path;
- the new export in `scripts/build-wasm.ps1` and a locally rebuilt WASM
  module (the generated `bindings/wasm/dist` directory is gitignored);
- `StrategyEvalOptions` and the full `StrategyEvalResult` JSON contract in
  `apps/web/src/app/engine-protocol.ts`;
- typed WASM, worker, and `EngineClient` plumbing;
- worker smoke coverage for a nontrivial one-Chaos exact evaluation, terminal
  mass, edge flows, expected actions, consumption keys, agreement with a
  5,000-run simulation, and a `veiled_chaos` refusal message.

The browser-facing result is the native evaluator's JSON payload returned as
a typed object. No probability, routing, legality, or crafting logic was added
to TypeScript.

## Verification

- `powershell -File scripts/build-wasm.ps1` — pass.
- `npx tsc --noEmit` in `apps/web` — pass.
- `npm test` in `apps/web` — pass; engine smoke 18/18.
- `npm run build` in `apps/web` — pass.
- `powershell -File scripts/test.ps1` — pass, including 118,285 native engine
  checks, 18/18 WASM worker smoke tests, all other web tests, and Python tests.

## Next task

Implement **Phase C only** from
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md).

Phase C must begin with the required image-model design loop before UI code:

1. create `design/briefs/strategy-calculator-mode.md` with real values from a
   Phase B evaluation and capture current-builder references in `design/refs/`;
2. generate 2–4 structurally different mockups in
   `design/mockups/strategy-calculator-mode/`;
3. present them to Oliver for selection and resolve the two explicit plan
   questions about edge-label and node-badge quantities;
4. write the selected design to
   `design/specs/strategy-calculator-mode.md`;
5. only then implement the mode, summary panel, overlays, drill-down,
   persistence, and Phase C tests.

Do not skip Oliver's mock selection or start Phase D. After Phase C, resume
[s6-plan.md](docs/s6-plan.md) Phase 1.

## Important files

- `docs/strategy-calculator-mode-plan.md`
- `engine/src/solver_eval.cpp`
- `engine/include/poecraft/solver.h`
- `bindings/wasm/wasm_api.cpp`
- `apps/web/src/app/engine-protocol.ts`
- `apps/web/src/app/engine-wasm.ts`
- `apps/web/src/app/engine-worker.ts`
- `apps/web/src/app/engine-client.ts`
- `apps/web/test/engine-smoke.test.ts`
- `apps/web/src/app/components/pc-strategy-editor.ts`
- `apps/web/src/app/components/pc-strategy-board.ts`
- `apps/web/src/app/components/pc-strategy-node.ts`
- `apps/web/src/app/components/pc-edge-layer.ts`
- `apps/web/src/app/odds-presentation.ts`
- `apps/web/src/app/workspace/persistence.ts`
- `apps/web/src/app/workspace/prices.ts`

## Rulings and gotchas

- The strict exclusion-effect partition remains evaluator-only. Do not change
  the ordinary DP solver's compact partition as part of Phase C.
- Evaluator support and refusal text come from the engine. Render refusal
  messages verbatim; do not recreate a support list in the UI.
- The worker intentionally owns the compile/evaluate/destroy lifecycle for
  `strategyEvaluate`; UI callers never receive a temporary strategy handle.
- Result costs are price-independent `expected_consumption` vectors. Phase C
  may dot them with live workspace prices for display only.
- The Phase B probability-sum smoke assertion uses the plan's `1e-3`
  serialization tolerance.
- Rebuild WASM after any further C ABI change before running web tests.
- Commits remain local unless Oliver explicitly asks to push.
