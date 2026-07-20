# Calculator Goal Item - Approved P3b Implementation Spec

Approved by Oliver on 2026-07-14 from
`design/mockups/calculator-goal-item/variant-a-literal-twin.png`.

The mock controls hierarchy and placement only. Runtime modifier text, tier
choices, requirement counts, legality, probabilities, action identity, and
costs remain catalog/engine owned. In particular, ignore the image-model typo
on the Input item's `S3` row and its incorrect target-section counts.

## Product boundary

Calculator remains:

```text
one concrete input item
+ one authored v1 goal item
+ one selected registry action
-> exact engine-returned odds for that action
```

P3b changes presentation and shared component structure only. It does not add
multi-goal/OR expressions, action comparison, a new goal schema, or any native,
C ABI, Python, WASM, or Strategy Builder evaluation behavior.

## Layout

Keep the approved Calculator shell unchanged:

1. workbench/status strip;
2. mechanic tabs and one selected action;
3. three-pane body with the stacked Input/Goal context rail, shared Modifier
   Pool, and Odds inspector.

Only replace the inside of the Goal context card:

1. context header: `Goal item` plus the existing `Select` / `Editing` state;
2. compact two-control band: `Finished rarity` and `Success means`;
3. one target-mode `pc-mod-list` frame matching the Input item frame.

Do not move the controls to the footer, widen the left rail, or redesign the
pool and Odds regions.

## Shared `pc-mod-list` contract

Make concrete-versus-target intent explicit with a discriminated model. The
exact type names may follow local conventions, but the contract is:

```ts
type PcModListModel = ConcreteModListModel | TargetModListModel;

interface ConcreteModListModel extends ModListModel {
    kind: "concrete";
}

interface TargetModListModel {
    kind: "target";
    baseName: string;
    itemLevel: number;
    rarity: "normal" | "magic" | "rare";
    prefixes: TargetSlotMod[];
    suffixes: TargetSlotMod[];
    otherRequirements: TargetOtherRequirement[];
    maxPrefix: number;
    maxSuffix: number;
}
```

Existing Emulator and Calculator Input callers pass `kind: "concrete"` and
retain their current rendering and events. Target mode shares the same outer
frame, title line, rarity treatment, prefix/suffix section structure, side
rails, fixed slot codes, typography, and empty-row rhythm.

The component stays presentational. It receives already-resolved target rows
and emits edits; it does not read Calculator drafts, reopen solvers, resolve
families, or calculate probabilities.

## Target header

- Primary line: `Vaal Regalia` and `iLvl 86`, from the same base selection as
  Input item.
- Secondary line: goal rarity and a restrained `TARGET` badge.
- Count copy: `N requirements · NP / NS`, not `N explicit`; a goal is not a
  fabricated rolled item.
- Do not infer influence badges from an influenced modifier requirement. Goal
  v1 does not separately author item influence state.
- Rarity colors only the same border/header accents used by concrete mode.

## Target rows

Adapt every non-legacy `CalculatorGoalSlot` through the existing
`ModifierFamilyOption` list:

- stable identity: `familyModKey`;
- side: `option.side`;
- stat text: the selected tier's catalog label when `minTier > 0`, otherwise
  the family display label with an explicit `Any tier` threshold;
- tier choices: the option's existing complete tier list plus `Any tier`;
- visible metadata: existing player-facing tags and `sourceLabel` when useful;
- probability: the matching engine-returned `slot_satisfied[index]`, shown as a
  quiet marginal value only when the current calculation is legal/supported;
- actions: tier selector and a compact remove button.

Rows use fixed `P1-Pn` / `S1-Sn` positions. Feed target rows through the same
stable-slot utility using `familyModKey` as identity so unchanged requirements
do not jump when another row is edited. Preserve Calculator slot-array order
within each side; do not sort targets alphabetically or by probability.

Empty target rows say `No prefix requirement` or `No suffix requirement`.
Section counts report actual requirements over engine/session capacity, for
example `Prefixes 2/3` and `Suffixes 1/3`; do not copy the mock's illustrative
`3/3` values.

Recovered legacy group requirements have no reliable P/S side. Keep them
readable and removable in a compact `Other requirements` block inside target
mode after the suffix ledger. Do not re-enable group-goal authoring and do not
invent a side.

## Target events and Calculator ownership

Target mode emits two bubbling events with stable identities:

```ts
"target-tier-change" -> { familyModKey: string; minTier: number }
"target-remove"      -> { familyModKey: string }
```

Legacy group removal may include the original Calculator slot index or another
unambiguous stable token because it has no `familyModKey`.

`pc-calculator` remains responsible for:

- mutating `slots`;
- preserving the current all-slots-follow behavior when requirements change;
- clamping partial `min_satisfied_slots` exactly as today;
- syncing `pc-mod-pool` selections;
- reopening the native solver and recalculating;
- draft recovery/persistence;
- rendering engine-owned combined and marginal probabilities.

Clicking controls inside the Goal frame must retain Goal as the active shared-
pool context. Concrete-mode right-click fracture behavior remains unchanged;
target rows never emit fracture or direct-item edit events.

## Controls above the frame

Retain the current native-backed controls and semantics:

- `Finished rarity`: `normal | magic | rare` -> goal `rarity`;
- `Success means`: `All N`, then `At least N-1 of N` through
  `At least 1 of N` -> goal `min_satisfied_slots`.

With no requirements, `Success means` remains disabled with `Add modifiers`.
The selected goal rarity drives target-frame rarity styling. Controls stay in
the compact band above the target frame, as approved in Variant A.

## Unchanged surfaces

- `pc-mod-pool` remains the one shared input/goal authoring surface.
- Modifier families continue to come from shared `buildModifierOptions`.
- The mechanic tabs and one-action selection remain unchanged.
- Odds headline, cost, coverage, miss signals, technical distribution, loading,
  illegal, unsupported, and engine-error states remain unchanged.
- Strategy Builder's `pc-condition-editor` and whole-graph Calculator mode are
  not touched.

## CSS and tokens

Reuse the existing item and Calculator tokens only:

- frame/rows: `--pc-bg`, `--pc-bg-raised`, `--pc-bg-panel`;
- borders: `--pc-border`, active context `--pc-accent`;
- text: `--pc-text-strong`, `--pc-text`, `--pc-text-dim`;
- side rails/codes: `--pc-prefix`, `--pc-suffix`;
- rarity: `--pc-normal`, `--pc-magic`, `--pc-rare`.

Remove superseded `.pc-calc-slots` / `.pc-calc-slot*` goal-list styling once no
consumer remains. Add target-mode selectors under `pc-mod-list` rather than
forking a second item-frame stylesheet. Keep 1 px borders, 3 px radii, and the
current compact type scale. Mobile remains out of scope.

## Required verification

- Component tests: unchanged concrete empty/populated/fractured rendering;
  target empty, three-requirement, dense six-requirement, any-tier, marginal-
  odds, and legacy group states; stable placement; tier/remove events.
- Calculator tests: v1 draft recovery, rarity, all/partial success thresholds,
  pool selection, tier updates/removal, one selected action, and verbatim native
  result rendering.
- Visual comparison: capture the implemented populated target next to
  `variant-a-literal-twin.png`; record any engine-truth deviations here.
- Separate-process headless Chrome: edit Input and Goal through the shared pool,
  change a target tier, remove a target, select one action, observe odds, and
  finish with no application console errors or uncaught exceptions.
- Gates: `npx tsc --noEmit`, `npm test`, `npm run build`, and
  `powershell -File scripts/test.ps1`.

Native and WASM rebuilds are not expected because P3b changes no engine or ABI
contract.

## P3b implementation verification

Implemented and verified on 2026-07-14.

Visual records:

- approved hierarchy: `design/mockups/calculator-goal-item/variant-a-literal-twin.png`;
- full populated Calculator: `design/mockups/calculator-goal-item/implemented-p3b.png`;
- focused populated Goal item: `design/mockups/calculator-goal-item/implemented-p3b-goal.png`.

The implementation follows Variant A's literal-twin hierarchy: Input and Goal
now render through separate, explicitly identified `pc-mod-list` instances;
Goal retains its rarity and success-threshold controls above a target-mode item
frame. The captured Vaal Regalia example has one concrete input prefix, a rare
two-requirement goal (one prefix and one suffix), `All 2`, one selected Chaos
action, engine-returned marginal odds on both target rows, and a combined exact
result of 1.4751%.

Engine-truth deviations from the illustrative mock are intentional:

- section counts are the real `1/3` prefix and `1/3` suffix requirements, not
  the mock's fictional full counts;
- modifier text, tier availability, tags, and percentages come from the live
  Vaal Regalia iLvl 86 catalog and exact Calculator result;
- the any-tier requirement is labeled `ANY` / `Any tier`, while the concrete
  tier is labeled `T1` / `T1 or better`;
- target rows include quiet marginal odds and real player-facing tags, so they
  are slightly taller than the mock's invented compact rows;
- Goal does not invent influences or exact rolled values that are absent from
  the v1 goal state.

Separate-process headless Chrome exercised the complete shared-pool flow:
edited Input, authored prefix and suffix Goal requirements, changed one target
to Any tier, removed and re-added a target, switched between partial and All
success thresholds, selected Chaos, observed exact and marginal odds, and
reloaded the recovered v1 draft. The final application console and uncaught-
exception lists were empty.

Acceptance gates passed:

- `npx tsc --noEmit`;
- `npm test`;
- `npm run build`;
- `powershell -File scripts/test.ps1` (123,485 native engine checks, zero
  failures, plus all ingest, artifact, binding, and web checks).

No native, C ABI, binding, WASM, persistence, or solver-goal contract changed.
