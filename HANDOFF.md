# Session Handoff — pre-S6 polish P1 complete; P2 mapping next

Written 2026-07-14 after implementing and gating
[pre-S6 product-polish Phase P1](docs/pre-s6-product-polish-plan.md). Read
[AGENTS.md](AGENTS.md), then [docs/direction.md](docs/direction.md), then this
file. The next task is Phase P2 only. S6 Phase 1 must not start yet, and Phase D
in [docs/strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md)
remains unscheduled.

## Current state

Phase P1 — base ordering and graph auto-labels — is complete:

1. `DataImpl` loads the canonical `base_items.drop_levels` array and validates
   it alongside every other parallel base array. The additive public
   `pc_data_get_base_drop_level` enumeration API returns the stored `int32_t`
   unchanged, including the negative unknown sentinel.
2. `pcw_data_bases` and the web `BaseInfo` contract expose `drop_level`. The
   committed WASM module was rebuilt.
3. Every Emulator, Calculator, and Strategy Builder base selection still uses
   `pc-base-picker`. Its shared model filters supported named bases and orders
   each class/subcategory by known drop level descending, unknown last, display
   name ascending, then metadata path ascending. Class and subcategory order is
   unchanged.
4. Shared strategy presentation code now owns automatic operation-node and edge
   labels. Empty authored `name`/`label` means automatic; any non-empty text is a
   manual override. New operation nodes and edges author empty text. Existing
   non-empty saved/imported text is preserved.
5. Automatic operation labels follow live parameters and resolve keyed choices
   through the loaded catalog/session display text: Essences, Fossils, bench and
   unveil modifiers, Harvest tags, influences, and current Eldritch tiers.
   Automatic edge labels recursively include the complete live condition tree,
   including nested ALL, ANY, NOT, and AT LEAST branches.
6. Board nodes, the edge layer, and inspector placeholders use the same shared
   formatters. Parameter/condition edits relabel automatic text immediately;
   manual overrides remain fixed; clearing an override returns to automatic
   mode without changing graph identity or semantics. The board presents full
   condition chains as compact, wrapped, left-aligned trees on a subdued
   backdrop; hover and the inspector expose the full parenthesized expression.
7. Node IDs, edge IDs, priority/routing order, operation payloads, condition
   payloads, and compiled behavior are unchanged.

## Pinned regression coverage

- Native loader/API tests pin Vaal Regalia at canonical drop level 68 and load
  a memory artifact whose first level is changed to `-1`, proving the unknown
  sentinel survives loading and enumeration unchanged.
- The real WASM worker `BaseInfo` path pins Vaal Regalia at 68.
- Shared picker tests pin level-descending order, unknown-last behavior,
  name/path tie-breaking, and supported/named filtering.
- Strategy tests pin parameter-sensitive catalog labels, live node/condition
  relabeling, manual override behavior, clear-to-auto, stable IDs/priority, and
  save/reopen preservation of both automatic and manual authored states. Nested
  condition coverage pins the full expression and every rendered tree branch.

## Acceptance gates — all green

- `powershell -File scripts/build.ps1`
- `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`
  — 123,409 checks, 0 failures
- `powershell -File scripts/build-wasm.ps1`
- `npx tsc --noEmit` in `apps/web`
- `npm test` in `apps/web` — 22/22 worker smokes plus all presentation/model
  tests
- `npm run build` in `apps/web`
- `powershell -File scripts/test.ps1`

A separate rendered headless Chrome smoke used a clean profile against the
production Vite build. It selected from the shared Body Armour picker (122
supported choices; the level-84 name tie put Conquest Lamellar first), authored
a Harvest Reforge node and a guarded rarity edge, verified live automatic
labels and placeholders, exercised manual overrides and clear-to-auto, and
finished with zero application console errors or uncaught exceptions.

A follow-up rendered smoke seeded a nine-line nested ALL/ANY/NOT/AT LEAST edge,
verified every visible branch, the full hover and inspector expression, the
label backdrop and alignment, and manual override/clear-to-auto behavior. It
also finished with zero console errors or uncaught exceptions.

## Next task — pre-S6 polish Phase P2 only

Implement **Phase P2 — Searing/Eater application as first-class currencies**
from [docs/pre-s6-product-polish-plan.md](docs/pre-s6-product-polish-plan.md).
Before changing code, get Oliver's authoritative row for every in-scope
currency and record it in the plan or a focused mechanic fixture:

```text
stable canonical currency key
display name
Searing or Eater side
native implicit tier produced
legality or replacement rule if it differs from the current implementation
economy price key
```

If any row is missing or ambiguous, stop and ask Oliver. Do not research or
infer PoE mechanics. Then implement only P2 end to end, run its full acceptance
gate including a real rendered browser smoke, commit locally, and rewrite this
handoff for P3a. Do not begin the P3 goal-expression work or S6 Phase 1.

## Gotchas

- The auto-label contract is authored-text based, not ID based: exactly empty
  text is automatic; non-empty saved text is manual. Never rewrite a manual
  override when parameters change or during load/normalization.
- Automatic edge presentation must retain every nested condition branch. Keep
  the full expression and the board tree derived from the same live condition;
  wrapping is presentation-only and must not rewrite authored text.
- Catalog/session display names are the label authority for keyed choices.
  Extend that route for concrete Eldritch currencies in P2; do not add a second
  hard-coded currency-label table.
- P2 must preserve a deterministic legacy normalization path for v1
  `{type: "eldritch_ember"|"eldritch_ichor", tier: N}` operations while new
  documents serialize concrete currency identity. Invalid mappings fail
  explicitly.
- The frontend has no crafting-rule authority. Concrete currency identity must
  flow through the action registry, native engine, bindings, WASM, Strategy
  Builder, Calculator, Emulator, traces, and economy keys.
- The engine WASM module is committed. Rebuild it after C ABI, engine, or
  strategy-vocabulary changes with `scripts/build-wasm.ps1`; the script
  self-activates `C:\emsdk`.
- Exact Calculator evaluation contracts from the earlier OOM work still stand:
  no probability cutoffs, junk coarsening, frontier/epsilon changes, or result
  contract changes.
- Use a separate headless browser process for repository browser smoke; do not
  use Codex's built-in browser.
- Commits are local-only unless Oliver explicitly says to push.
