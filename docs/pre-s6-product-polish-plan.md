# Pre-S6 Product Polish Plan

Execution plan for the product-polish interlude Oliver scheduled on 2026-07-14
before [s6-plan.md](s6-plan.md) Phase 1. Read [AGENTS.md](../AGENTS.md),
[direction.md](direction.md), and [HANDOFF.md](../HANDOFF.md) first. Work
through the active phases below in order. Each implementation phase ends
test-green with one local commit and a rewritten handoff; stop at the stated
boundary instead of rolling the next phase into the same change. Oliver skipped
P2 on 2026-07-14, so it is retained only as a deferred record and is not a gate
for P3 or S6.

This interlude now covers three active product requests:

1. order bases from highest to lowest base level requirement;
2. automatically name Strategy Builder nodes and edges;
3. keep Calculator intentionally simple at one input item, one goal item, one
   selected crafting action, and the odds for that action; make the goal use the
   same item-frame UI as the input item without changing goal semantics.

The proposed Searing Exarch/Eater currency migration is deferred. The proposed
multi-goal/OR/named-outcome expansion is cancelled rather than postponed.

P1 and P3a are complete. Variant A is the approved goal-item direction; P3b is
the next implementation phase. S6 Phase 1 resumes only after P3b is complete.
Strategy Builder calculator mode Phase D remains unscheduled.

## Standing contracts

- The native engine remains the only authority for action legality,
  probability, goal matching, and overlapping-outcome accounting. TypeScript
  may sort display data and render engine results; it must not recreate
  crafting rules or add probabilities together.
- SQLite is canonical and the compiled artifact is derived. Completed P1 loads
  `base_items.drop_levels` into the runtime catalog and exposes it through the
  shared base picker; keep that canonical route rather than deriving a level
  from names or metadata paths in the frontend.
- PoE mechanic details, including the exact Searing/Eater currency-to-tier
  mapping and any edge cases, come from Oliver. If P2 is ever rescheduled, it
  begins by writing the approved mapping into the plan or a focused mechanic
  fixture; never research or guess it.
- Stable graph IDs are not display names. Auto-naming must not rename IDs or
  graph references.
- Existing saved strategies and v1 Calculator goal documents remain readable
  and keep their exact meaning. P3 introduces no new goal schema.
- P1 changes behavior inside existing controls and does not need a new visual
  design. P3 changes a major surface and must use the image-model design loop
  in [s6-plan.md](s6-plan.md) before UI implementation.
- Calculator one-step odds and Strategy Builder whole-graph odds keep separate
  task-shaped entry points. They already share the native action registry,
  `CalcContext`, action legality, and exact transition distributions; do not
  route one through the other or create another crafting-rule backend.

## Phase P1 - Base ordering and graph auto-labels (complete)

**Goal.** Base pickers show the highest-level bases first, and new graph nodes
and edges carry useful labels that stay in sync with their operation or
condition until the user deliberately overrides them.

### Base catalog and ordering

1. Load the canonical base `drop_levels` array from the compiled artifact into
   the immutable native data set and validate its length with the other base
   arrays.
2. Expose that value through the public base-enumeration path and the WASM
   `BaseInfo` contract as `drop_level`. Unknown/sentinel values remain explicit;
   do not manufacture a level.
3. In `pc-base-picker`, sort bases within each class/subcategory by:

   ```text
   known drop_level descending
   unknown drop_level last
   display name ascending
   metadata path ascending
   ```

   Class and subcategory ordering remains unchanged. Because Emulator,
   Calculator, and Strategy Builder share `pc-base-picker`, fix the ordering in
   that component rather than independently in each consumer.

### Node and edge auto-labels

Use an empty authored label as the unambiguous "automatic" state:

- An operation node with an empty `name` renders a label derived from its live
  operation and parameters. Use the engine/catalog display names for keyed
  choices such as Essences, Fossils, bench mods, Harvest tags, influences, and
  later the concrete Eldritch currencies; do not duplicate those names in a
  second hard-coded label table.
- An edge with an empty `label` renders the existing live
  `conditionLabel(edge.condition)` summary. Editing the condition therefore
  updates the board label immediately.
- Typing a non-empty node name or edge label creates a manual override. Clearing
  it returns to automatic mode.
- New nodes and edges start in automatic mode. Existing non-empty imported or
  saved names remain manual and are never rewritten on load.
- Node/edge IDs, priority, routing order, operation payloads, and condition
  payloads are unchanged.

Keep the formatter logic in shared strategy presentation/model code so the
board node, edge layer, inspector placeholder, and any future compiled graph
view agree. Pin parameter-sensitive examples in tests; a generic "Essence" or
"Fossils" label is not sufficient when a concrete selection exists.

### P1 acceptance gate

- Native loader/API tests prove `drop_level` survives artifact load and base
  enumeration, including its unknown sentinel.
- Web tests prove descending level order, deterministic tie-breaking, and the
  same shared picker behavior for supported bases.
- Strategy tests prove live parameter/condition relabeling, manual override,
  clear-to-auto, and save/reopen preservation of overrides.
- A real rendered browser smoke covers one base picker and node/edge edits; the
  console is clean.
- `powershell -File scripts/build.ps1`
- `powershell -File scripts/build-wasm.ps1` if the native/WASM catalog contract
  changes (expected for this phase)
- `npx tsc --noEmit`, `npm test`, and `npm run build` in `apps/web`
- `powershell -File scripts/test.ps1`

P1 completed and was committed locally. Oliver then skipped the planned P2
Eldritch currency migration.

## Phase P2 - Searing/Eater application as first-class currencies (skipped)

**Status.** Oliver skipped this phase on 2026-07-14. It is not an execution
prerequisite for P3 or S6 Phase 1. The specification below is retained only as
a possible future migration; do not implement it unless Oliver schedules it
again and supplies the authoritative currency table.

**Goal.** Every actual Searing Exarch and Eater of Worlds implicit-applying
currency is a distinct catalog/action/economy item. Users choose a named
currency; they never choose an abstract side and a numeric tier.

Before implementation, record Oliver's authoritative table for every in-scope
currency:

```text
stable canonical currency key
display name
Searing or Eater side
native implicit tier produced
legality or replacement rule if it differs from the current implementation
economy price key
```

If any row is ambiguous, stop and ask Oliver. Do not infer it from online
sources.

Then implement the migration end to end:

1. Add distinct action-registry descriptors backed by canonical currency
   identity. It is acceptable for native internals to resolve a currency to the
   existing side/tier operation, but public authoring, display, and cost
   identity must be the concrete currency.
2. Replace the Emulator's generic tier selector plus `Apply Ember` / `Apply
   Ichor` controls with the actual named currency entries in the Eldritch
   panel. Reuse the current grouped-panel interaction language.
3. Offer those same descriptors in Strategy Builder, Calculator, Simulator
   reports, action traces, and economy/cost-key displays. Auto-generated node
   names from P1 must show the concrete currency name.
4. Define a backward-compatible normalization path for existing v1
   `{type: "eldritch_ember"|"eldritch_ichor", tier: N}` craft requests and
   saved strategy operations. New documents serialize the concrete currency
   identity. Invalid or unmapped legacy tiers fail explicitly.
5. Keep native action semantics and tests authoritative. The frontend only
   submits a currency identity and renders the engine's legality/outcome.

### P2 acceptance gate

- The action registry exposes one distinct descriptor and economy key per
  approved currency row; generic numeric-tier choices are absent from all
  authoring surfaces.
- Native, C ABI, Python, WASM, strategy compiler/simulator, Calculator action
  catalog, and Emulator agree on identity and behavior for every row.
- Legacy v1 requests/strategies normalize deterministically, and focused tests
  cover invalid mappings.
- UI and trace labels use the actual currency name and the price table charges
  its own currency key.
- A real browser smoke applies at least one currency from each side, authors
  one in Strategy Builder, selects one in Calculator, and reports no console
  errors.
- Rebuild native and WASM, then run the complete `scripts/test.ps1`, web
  type-check/test/build, and relevant binding tests.

Because P2 is skipped, proceed directly from the completed P1 baseline to P3a.

## Phase P3a - Simple goal-item design and reuse contract (complete)

**Goal.** Freeze the visual shape and reuse boundary for the existing v1
Calculator goal before changing UI code.

The product contract is deliberately narrow:

```text
one concrete input item
+ one authored v1 goal item
+ one selected registry action
-> exact engine-returned odds for that action
```

P3 does not add named goals, OR branches, a predicate tree, action comparison,
or a new native goal contract. Existing `rarity`, modifier-family `slots`, tier
thresholds, and `min_satisfied_slots` retain their current semantics.

Document and design the following reuse boundary:

- native: Calculator's `pc_calc_action_outcomes` and Strategy Builder's
  `pc_strategy_evaluate` stay separate public entry points over the same action
  registry, `CalcContext`, legality checks, and transition distributions;
- catalog/model: Calculator and Strategy Builder continue sharing
  `buildModifierOptions`; Calculator continues using the engine-backed
  `pc-mod-pool` for input and goal authoring;
- presentation: extend the shared `pc-mod-list` item frame with an explicit
  goal/target model instead of maintaining Calculator-only goal-row markup;
- Strategy Builder's recursive `pc-condition-editor` is not reused for this
  simple goal item. Its routing vocabulary and persistence contract solve a
  different problem and would add unnecessary backend/UI surface.

Run the required image-model design loop for two item-frame treatments while
holding the rest of the approved Calculator layout fixed:

1. literal twin item frames, with target rows carrying tier-threshold controls;
2. the same shared frame with a quieter target treatment and compact goal
   controls integrated into the header/footer.

Both treatments must cover empty, populated, and dense six-slot goal items;
loading and illegal/unsupported states remain in the unchanged Odds inspector.
The goal must read as a target rather than a fabricated rolled item, while base,
item level, rarity border, prefix/suffix rails, slot positions, and typography
match the input item.

### P3a acceptance gate

- The design brief records the narrow product contract, realistic content,
  states, interactions, and the backend/component reuse boundary.
- Reference screenshots, prompts, and at least two image-model examples are
  committed under `design/`.
- Oliver selected Variant A.
- The approved implementation spec states exactly how `pc-mod-list` represents
  target rows without changing concrete-item behavior.

Oliver approved Variant A on 2026-07-14. The implementation contract is in
`design/specs/calculator-goal-item.md`. P3a stopped without UI implementation.

## Phase P3b - Shared item-frame Calculator goal (next)

**Goal.** Replace the Calculator-only Goal requirements list with the approved
goal/target mode of the same shared item-frame UI used by the input item.

1. Extend `pc-mod-list` through an explicit concrete-versus-target model. Keep
   concrete Emulator/Calculator rendering and right-click fracture behavior
   unchanged.
2. Adapt each existing `CalculatorGoalSlot` through the already-shared
   `ModifierFamilyOption` catalog data into a target prefix/suffix row with its
   selected tier-or-better threshold. Empty goal positions remain visibly
   stable.
3. Keep goal rarity, `min_satisfied_slots`, add/update/remove, persistence, and
   solver reopen behavior unchanged. The shared modifier pool remains the only
   goal authoring surface.
4. Remove the superseded Calculator-only goal-row rendering and CSS. Do not
   duplicate item-frame markup or modifier-family lookup logic.
5. Do not change native engine, C ABI, bindings, WASM, Strategy Builder graph
   evaluation, or goal JSON.

### P3b acceptance gate

- Component/model tests cover empty, populated, and dense target item frames,
  tier changes/removal, stable slot placement, and unchanged concrete item
  behavior.
- Calculator tests preserve v1 draft recovery, rarity and `All / At least N`
  semantics, one selected action, and verbatim engine-returned odds.
- Screenshot comparison against the approved mock is recorded in the design
  spec.
- A real separate-process headless browser smoke edits input and goal through
  the shared pool, changes a target tier/removes a target, calculates one
  action, and finishes with a clean application console.
- `npx tsc --noEmit`, `npm test`, `npm run build`, and
  `powershell -File scripts/test.ps1` are green. Native/WASM rebuilds are not
  expected because the engine contract does not change.

After P3b, commit locally, rewrite `HANDOFF.md`, and resume
[s6-plan.md](s6-plan.md) Phase 1. Do not begin S6 Phase 2.
