# Calculator Implementation Spec

Approved direction: Variant E (`variant-e-stacked-context-rail.png`), approved
by Oliver on 2026-07-13. The concrete item follows
[item-display.md](item-display.md). Mocks control hierarchy only; runtime
labels, actions, legality, probabilities, pools, and outcome data come from the
native engine.

## Implemented layout

1. Compact workbench strip: title, one-line instruction, status.
2. Existing mechanic tabs and action controls. Selecting an action calculates;
   it does not mutate the input item.
3. Three-pane body:
   - left `minmax(300px, 0.95fr)`: stacked Input item and Goal requirements;
   - center `minmax(440px, 1.55fr)`: the shared modifier pool;
   - right `minmax(340px, 1.15fr)`: Odds.

The body uses 1 px token gutters. Below 1050 px it keeps a 1030 px minimum
working width and scrolls horizontally. Mobile is out of scope.

## Context rail and shared pool

- Input item and Goal requirements are both always visible and selectable.
- The active card has an accent border/inset and an `Editing` state badge.
- Selecting Input item puts `pc-mod-pool` in its engine-backed direct mode:
  tier clicks add/remove exact modifiers and the existing fracture gesture is
  retained.
- Selecting Goal requirements puts the same pool in goal mode: a tier click
  adds or updates a family requirement at that tier or better.
- New group-goal authoring is removed. A recovered legacy group slot remains
  readable and removable.
- Selected goal families show a compact `Tn`/`ANY TIER` family badge; the exact
  tier row has the same highlight treatment in every `pc-mod-pool` consumer.

## Input item

- Uses shared `pc-mod-list` rather than a Calculator-only summary.
- Header shows base, item level, rarity, influences, and explicit counts.
- Implicits are a compact block above explicit slots.
- Prefix and suffix slots have stable visual positions so modifiers do not move
  vertically when an action changes the item.
- Each occupied row shows actual rolled text once, a family tier, and only
  player-facing broad tags. Internal classification labels are filtered.
- Rarity colors the border/header accent; the body stays neutral.
- Footer contains Change base, fresh rarity, New item, and the grounded
  Emulator/Stash handoff note.

## Goal requirements

- `Finished rarity` maps to solver goal `rarity`.
- `Success means` maps to native `min_satisfied_slots`:
  - default and first option: `All N`;
  - remaining options: `At least N-1 of N` through `At least 1 of N`.
- A legacy draft without the field loads as `All N`.
- If an all-slots goal gains or loses a slot, it continues to mean all. A
  partial numeric threshold is otherwise clamped to the new slot count.
- Changing rarity, threshold, family, or tier reopens the native solver. The
  threshold is not a display-only calculation.
- With two or more slots, the compact combined strip shows the engine-returned
  `success_probability` for finished rarity and threshold together.
- Each requirement row keeps side, modifier text, remove action, tier selector,
  and marginal per-slot probability in stable positions.

## Odds inspector

### Exact result

The headline is the native `success_probability`. Target copy is grounded in
the configured predicate, for example `Rare + all 2 modifiers` or
`Rare + at least 1 of 2 modifiers`. No TypeScript recomputation defines
success. The primary percentage preserves the engine/WASM precision rather
than rounding to two significant digits. A compact detail strip also shows:

- the unit-interval engine value (`p = 0.084933`);
- exact failure percentage (`91.5067%`);
- geometric expected attempts (`11.774`).

Expected attempts is explicitly scoped to repeated independent tries that
start from the same input item.

### Cost

Cost per attempt remains the selected action's engine registry cost vector
dotted with the shared editable chaos-equivalent price table. The panel also
shows `Estimated action spend per success = action cost / success_probability`.
This is not presented as full-strategy cost: its note excludes base, reset,
cleanup, and recovery spend unless those inputs are already part of the
selected action. Missing prices remain explicit; zero probability reports no
finite estimate.

### Goal coverage

- Returned abstract outcomes are grouped by exact number of satisfied goal
  slots: `N of N` through `0 of N`.
- Rows at or above `min_satisfied_slots` receive a small `threshold` marker.
- The section explicitly says it is modifier coverage only; finished rarity is
  included in the exact result above.
- `Miss signals` reports overlapping marginals derived from returned outcome
  fields: below modifier threshold, a goal below required tier, a goal absent,
  a blocked goal family, and wrong finished rarity. Zero rows are omitted and
  the section says the signals can overlap.

### Technical distribution

- Raw engine outcome classes are preserved in a collapsed `details` drawer.
- The drawer names each `G1`/`G2` column, sorts success classes first and then
  by coverage/probability, and retains the 40-row rendering cap plus remainder
  probability.
- Status vocabulary remains engine-grounded: satisfied, below tier, blocked,
  absent; flags are the native abstract mechanic flags.

## States

- Empty: no goal -> `Define a goal to compute odds against.`
- No action: `Pick an action.`
- Loading: `Calculating…`
- Illegal: `This action is not legal on the current item.`
- Unsupported: `No exact evaluator for this action yet.`
- Error: surface the engine message without replacing the authoring context.

## Tokens

- backgrounds: `--pc-bg`, `--pc-bg-panel`, `--pc-bg-raised`, `--pc-bg-deep`
- borders: `--pc-border`; active workbench accents: `--pc-accent`
- text: `--pc-text-strong`, `--pc-text`, `--pc-text-dim`
- sides: `--pc-prefix`, `--pc-suffix`
- rarity: `--pc-normal`, `--pc-magic`, `--pc-rare`

No new global palette token is required.

## Verification (2026-07-13)

- Native build and engine binary: 102,031 checks, 0 failures.
- WASM rebuilt with `scripts/build-wasm.ps1`.
- `npx tsc --noEmit`, `npm test` (16/16 engine smoke plus item/workspace/
  strategy suites), and `npm run build` pass.
- Headless Chrome, Vaal Regalia iLvl 86, empty Rare input, T1 prefix + T1
  suffix goal, Chaos:
  - `All 2` = 0.18%; changing to `At least 1 of 2` = 8.49%;
  - target copy updates to `Rare + at least 1 of 2 modifiers`;
  - coverage and miss signals render; Technical distribution is collapsed by
    default and expands to the capped 40 rows.
- Precision/cost follow-up: a rendered 8.4933% result exposes
  `p = 0.084933`, 91.5067% failure, 11.774 expected attempts, and with Chaos
  priced at 1c reports 1c per attempt / 11.774c estimated action spend per
  success.
- The only observed failed request is the existing missing favicon (404); no
  application page errors were reported.
