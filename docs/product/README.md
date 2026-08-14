# Product

**Status: stable implemented product index.** This area describes the current
browser product; it does not schedule future work.

Parent: [Documentation index](../README.md)

Verified against code and the final non-visual Solver Goal Realignment
acceptance: 2026-08-13. Scope: Vite/TypeScript/Web Components workspace,
document persistence, Calculator, Strategy Builder, Simulator presentation,
economy integration, and the release-WASM worker path. No rendered or visual
review was performed.

## Product Shape

poecraft2 is a desktop-oriented browser workspace with four document kinds:

- Emulator: mutate one native item with engine actions;
- Calculator: inspect one exact action and run the one-item solver;
- Strategy Builder: author a strategy graph and switch its runner between
  Simulator and exact Calculator modes; and
- Stash: open manually saved items and strategies.

Docked tabs and splits are workspace behavior, not separate crafting
implementations. All mechanic legality, transition probability, strategy
execution, exact evaluation, and solving remain native-engine authority.

## References

- [Workspace And Persistence](workspace.md) — tabs, drafts, manual saves,
  Stash, handoffs, and the economy selector.
- [Calculator](calculator.md) — exact one-action odds, goal authoring, product
  solve flow, and the current sampled verification boundary.
- [Strategies](strategies.md) — v1 graph model, conditions, validation, board,
  Simulator mode, and exact whole-graph Calculator mode.
- [Product Reliability Coverage](reliability.md) — automated non-visual
  coverage and Oliver's manual visual checklist.
- [Product Notes](NOTES.md) — non-authoritative open and deferred product work.

Mechanic-specific availability and behavior are indexed by the
[mechanics library](../mechanics/README.md). Economy selection, caching, and
price pinning are owned by [Economy](../economy/README.md).

## Ownership

| Concern | Current code owner |
| --- | --- |
| Workspace shell and documents | `apps/web/src/app/components/pc-workspace.ts` |
| Draft/Stash persistence | `apps/web/src/app/workspace/persistence.ts` |
| Emulator | `apps/web/src/app/components/pc-emulator.ts` |
| Calculator | `apps/web/src/app/components/pc-calculator.ts` |
| Strategy model/validation | `apps/web/src/app/strategy-model.ts` |
| Strategy authoring and runners | `apps/web/src/app/components/pc-strategy-editor.ts` |
| Native worker bridge | `apps/web/src/app/engine-client.ts`, `engine-worker.ts` |
| Shared economy | `apps/web/src/app/workspace/economy-service.ts` |

Historical visual briefs, approved variants, prompts, captures, and
implementation comparisons live in the
[product-design archive](../archive/2026-07-product-design/README.md). They are
evidence of past choices, not a second current product specification.
