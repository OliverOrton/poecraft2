# Session Handoff — S6 Phase 1 next

Written 2026-07-14 after completing Phases A-C of
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md).
Read [AGENTS.md](AGENTS.md), then [docs/direction.md](docs/direction.md).

## Current state

Strategy Builder Calculator mode is complete across the native evaluator,
WASM/worker/client transport, and the web UI. The selected Phase C layout is a
hybrid of the design mockups: Variant B's compact node/result treatment lives
in the existing bottom runner boundary, while the right panel remains the
actual selected graph-node/edge inspector.

Calculator mode now provides:

- automatic exact evaluation on entry and about 300 ms after structural
  changes;
- no re-evaluation for node movement, viewport changes, strategy/node names,
  descriptions, or edge display labels;
- exact success/failure/stop and simulator-parity miss signals, expected
  actions, convergence/residual detail, and per-node attribution;
- engine-reported expected consumption with shared live price inputs and an
  explicit incomplete total when prices are missing;
- operation expected-visit badges, terminal absorb-probability badges, and
  conditional edge shares with absolute traversal counts on hover;
- a selected-operation incoming-state table with resolved target labels,
  rarity/affix counts, slot statuses, flags/blocked masks, and truncated mass;
- stale/evaluating/invalid/unresolved/refusal states, with engine refusal text
  rendered verbatim;
- optional `builderMode` draft persistence (legacy drafts open in Simulator)
  and retained instance state across Dockview detach/reconnect.

The reusable presentation pieces are in
`apps/web/src/app/strategy-eval-presentation.ts`,
`apps/web/src/app/odds-presentation.ts`, and the annotation inputs on
`PcStrategyBoard` / `PcStrategyNode` / `PcEdgeLayer`. The standalone
Calculator now uses the same expected-consumption price-row renderer.

The complete image-model design record is under
`design/briefs/strategy-calculator-mode.md`,
`design/mockups/strategy-calculator-mode/`, and
`design/specs/strategy-calculator-mode.md`. The implemented 1280×720 preview is
`design/refs/strategy-calculator-phase-c-preview.png`.

## Verification

- `powershell -File scripts/build.ps1` — pass.
- `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`
  — pass, 118,285 checks and 0 failures.
- `npx tsc --noEmit` in `apps/web` — pass.
- `npm test` in `apps/web` — pass, including 18/18 WASM worker smoke checks and
  the new Phase C presentation/change-classification tests.
- `npm run build` in `apps/web` — pass.
- `powershell -File scripts/test.ps1` — pass across ingest, artifact,
  bindings, native engine, WASM, and web tests.
- Separate headless Chrome preview at 1280×720 — real Transmute strategy exact
  evaluation rendered successfully; right operation inspector remained intact;
  only the existing favicon 404 appeared in the console.

## Next task

Resume **Phase 1 only** from [s6-plan.md](docs/s6-plan.md): solve in the
workspace (`solve -> compiled strategy opened in the Strategy Board`) with
expected-remaining-cost annotations and a one-click verification run.

Phase 1 is a UI phase, so begin with its required image-model design loop.
The placement question in the plan (Calculator solve section versus Simulator
workflow) still goes to Oliver during mock review. Do not implement before his
selection. Do not begin Phase 2.

The engine/client call sequence already works and is pinned by the last
`engine-smoke.test.ts` solver test:

```text
openSolver -> solverSolve -> solverCompileStrategy
           -> compileStrategy -> createSimulator -> runStrategy
```

## Important files

- `docs/s6-plan.md`
- `docs/strategy-calculator-mode-plan.md`
- `apps/web/src/app/components/pc-calculator.ts`
- `apps/web/src/app/components/pc-strategy-editor.ts`
- `apps/web/src/app/components/pc-strategy-board.ts`
- `apps/web/src/app/components/pc-strategy-node.ts`
- `apps/web/src/app/strategy-eval-presentation.ts`
- `apps/web/src/app/odds-presentation.ts`
- `apps/web/src/app/workspace/prices.ts`
- `apps/web/src/app/workspace/persistence.ts`
- `apps/web/test/engine-smoke.test.ts`
- `apps/web/test/strategy-calculator-mode.test.ts`

## Rulings and gotchas

- Do not use Codex's built-in/in-app browser for this repo; Oliver reports it
  crashes the Codex app. Use a separate headless browser process when a later
  UI phase reaches its preview gate.
- The right Strategy Builder inspector is for actual graph authoring. Analysis
  belongs in the bottom inspector; preserve this layout decision.
- Exact-evaluation node class `share` values are normalized within each node;
  `expected_visits` and edge traversals are absolute expected counts.
- Edge percentages are presentation arithmetic only:
  `edge traversals / sum(sibling traversals)`. Crafting/routing authority stays
  native.
- Structural re-evaluation is keyed by `strategyStructuralSignature`; keep
  positions, viewport, names/descriptions, and display labels out of it.
- The evaluator is a synchronous sequential-worker call. Chunking belongs to
  later solver work, not a Phase C UI workaround.
- Phase 1's compiled-policy `expected_cost` badge must reuse the current board
  annotation mechanism rather than introducing parallel node markup.
- Expected consumption is price-independent. Price edits only recompute the
  UI dot product and never re-run evaluation.
- Phase D bounds mode and reach-probability-on-demand were not started.
- PoE1 mechanic ambiguity still goes directly to Oliver; never research or
  guess.
- Commits remain local unless Oliver explicitly asks to push.
