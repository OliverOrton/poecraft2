# Pre-S6 Product Polish Plan

Execution plan for the product-polish interlude Oliver scheduled on 2026-07-14
before [s6-plan.md](s6-plan.md) Phase 1. Read [AGENTS.md](../AGENTS.md),
[direction.md](direction.md), and [HANDOFF.md](../HANDOFF.md) first. Work
through the phases below in order. Each phase ends test-green with one local
commit and a rewritten handoff; stop at the stated boundary instead of rolling
the next phase into the same change.

This interlude covers four product requests:

1. order bases from highest to lowest base level requirement;
2. automatically name Strategy Builder nodes and edges;
3. represent Searing Exarch and Eater of Worlds application as the actual
   currency items instead of a generic side plus numeric tier selector;
4. make Calculator goals look like goal items and support multiple goals, OR
   expressions, and multiple named outcomes for a one-step calculation, using
   condition-authoring UI shared with Strategy Builder where the semantics
   overlap.

The phases are ordered from the smallest independent correction to the largest
contract change. S6 Phase 1 resumes only after P3c is complete. Strategy
Builder calculator mode Phase D remains unscheduled.

## Standing contracts

- The native engine remains the only authority for action legality,
  probability, goal matching, and overlapping-outcome accounting. TypeScript
  may sort display data and render engine results; it must not recreate
  crafting rules or add probabilities together.
- SQLite is canonical and the compiled artifact is derived. The current
  compiled artifact already carries `base_items.drop_levels`; the runtime
  currently discards that field. Plumb it through the runtime catalog rather
  than deriving a level from names or metadata paths in the frontend.
- PoE mechanic details, including the exact Searing/Eater currency-to-tier
  mapping and any edge cases, come from Oliver. Phase P2 begins by writing the
  approved mapping into the plan or a focused mechanic fixture; never research
  or guess it.
- Stable graph IDs are not display names. Auto-naming must not rename IDs or
  graph references.
- Existing saved strategies and v1 goal documents remain readable. New
  contracts may normalize legacy input, but they must not silently reinterpret
  it.
- P1 changes behavior inside existing controls and does not need a new visual
  design. P2 reuses the approved grouped craft-panel language. P3 changes a
  major surface and must use the image-model design loop in
  [s6-plan.md](s6-plan.md) before UI implementation.

## Phase P1 - Base ordering and graph auto-labels (next)

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

Stop after the P1 gate, commit locally, rewrite `HANDOFF.md` for P2, and do not
begin the Eldritch currency migration.

## Phase P2 - Searing/Eater application as first-class currencies

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

Stop after the P2 gate, commit locally, rewrite `HANDOFF.md` for P3a, and do not
begin the goal-expression UI.

## Phase P3a - Goal expression semantics and approved design

**Goal.** Freeze the meaning and visual shape of multi-goal Calculator input
before changing engine or UI contracts.

Write a focused goal-expression contract that answers:

- how named goal outcomes are represented;
- how `ALL`, `ANY`/OR, `NOT`, and `AT LEAST N` compose leaf predicates;
- whether rarity is a leaf predicate or a branch property;
- how an item satisfying more than one named outcome is reported;
- how the combined success probability is an exact union without double
  counting overlaps;
- which expression shapes the optimal solver supports initially, versus the
  one-step Calculator only;
- how v1 `{rarity, slots, min_satisfied_slots}` goals migrate without changing
  meaning;
- which primitives are truly shared with Strategy Builder conditions and which
  need goal-specific adapters.

Recommended data direction: a versioned predicate tree with stable named goal
branches and engine-returned membership/coverage information. Do not implement
"OR" by running several Calculator calls and summing the results in TypeScript;
one successor may satisfy several branches.

Run the required image-model design loop for these states:

- one simple goal;
- several named goals joined by OR;
- nested `ALL` / `ANY` / `AT LEAST N` editing;
- a normal item-shaped goal card that visually reuses the accepted item frame
  and modifier rows while clearly remaining a target, not a rolled concrete
  item;
- one-step results with several goal outcomes, including overlapping matches;
- empty, loading, illegal/unsupported, and dense states.

The design should factor a reusable predicate-expression editing shell that
can host both Calculator goal leaves and Strategy Builder condition leaves. It
must not force the two domains to share invalid leaf types or execution
semantics. Oliver approves the semantics contract and mock before P3b begins.

### P3a acceptance gate

- The versioned goal/predicate schema and overlap semantics are documented with
  concrete JSON and truth-table examples.
- Backward compatibility and Calculator-versus-solver support boundaries are
  explicit.
- The design brief, references, structurally different image-model variants,
  Oliver's selection, and implementation spec are committed under `design/`.

Stop after approval and documentation, commit locally, rewrite `HANDOFF.md` for
P3b, and do not implement the engine contract in the design phase.

## Phase P3b - Native multi-goal calculation contract

**Goal.** The native engine can evaluate the approved expression and return
exact combined and per-named-outcome probabilities for one action.

Implement the P3a schema in the native goal parser, abstract-state/goal
projection, Calculator outcome evaluator, C ABI, Python binding, and WASM
worker. Preserve the v1 adapter. Return enough engine-owned membership data to
render named outcome probabilities and overlap honestly. Keep capacity limits
and unsupported-vocabulary failures explicit.

If P3a deliberately limits the optimal solver to a subset, validate and reject
unsupported expression shapes at solver creation with a precise message; the
Calculator may support the broader approved one-step subset. Do not silently
weaken an expression.

### P3b acceptance gate

- Native truth-table and overlap tests pin named probabilities and combined
  union probability, including one successor satisfying multiple goals.
- C ABI, Python, and WASM return identical results and preserve the old v1
  simple-goal results.
- Capacity/error behavior stays bounded and code-aware.
- Native build, WASM rebuild, binding tests, web worker tests, and
  `scripts/test.ps1` are green.

Stop after the P3b gate, commit locally, rewrite `HANDOFF.md` for P3c, and do
not implement the redesigned Goal tab in the engine phase.

## Phase P3c - Shared expression editor and item-shaped Calculator goals

**Goal.** Implement the approved P3a design on top of the P3b engine contract.

1. Extract a reusable expression-tree editor shell from the current Strategy
   Builder condition editor. Supply domain adapters for available leaf types,
   labels, validation, and modifier selection; do not fork a second recursive
   editor in Calculator.
2. Preserve Strategy Builder condition behavior and saved strategy documents.
   Sharing UI infrastructure does not itself expand strategy routing
   vocabulary.
3. Replace the Calculator goal summary with the approved item-shaped target
   presentation and allow multiple named goals/OR composition.
4. Render per-named-outcome and combined success results directly from the
   engine. Show overlaps according to the approved design and keep the existing
   technical distribution available for diagnosis.
5. Keep the modifier-family picker and item display primitives shared with the
   Emulator; do not build parallel modifier data or item-frame systems.

### P3c acceptance gate

- Component/model tests cover expression editing, multiple named outcomes,
  serialization/reopen, v1 migration, validation, and Strategy Builder
  regression behavior.
- Worker/component tests prove the UI renders engine-returned combined and
  overlapping outcome values without client-side probability arithmetic.
- Screenshot comparison against the approved mock is recorded in the design
  spec; empty, dense, error, and overlap states remain usable.
- Real browser smoke covers Calculator authoring and Strategy Builder condition
  editing, with a clean console.
- `npx tsc --noEmit`, `npm test`, `npm run build`, and
  `powershell -File scripts/test.ps1` are green.

After P3c, commit locally, rewrite `HANDOFF.md`, and resume
[s6-plan.md](s6-plan.md) Phase 1. Do not begin S6 Phase 2.
