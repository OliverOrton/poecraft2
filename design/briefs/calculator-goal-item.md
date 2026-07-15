# Calculator Goal Item - Design Brief

Approved by Oliver on 2026-07-14: Variant A, the literal-twin item-frame
direction. Implementation details are frozen in
`design/specs/calculator-goal-item.md`.

## Purpose

Calculator stays a deliberately small one-step tool: one concrete input item,
one authored goal item, one selected crafting action, and the native engine's
exact odds for that action. This pass changes only Goal presentation. The Goal
must use the same item-frame hierarchy as Input item while remaining visibly a
target made of modifier-family tier thresholds, not a fabricated rolled item.

## Fixed product and backend contract

- Goal JSON remains v1 `{ rarity, slots, min_satisfied_slots }`.
- Exactly one registry action is selected and calculated at a time.
- `pc_calc_action_outcomes` remains the one-step API. Strategy Builder's
  `pc_strategy_evaluate` remains its whole-graph API. Both already use the same
  native action registry, `CalcContext`, legality, and transition distributions.
- Calculator and Strategy Builder already share `buildModifierOptions`.
- Calculator continues to use `pc-mod-pool` for both concrete input editing and
  goal-family selection.
- The visual reuse target is `pc-mod-list`: add an explicit target model rather
  than keep a Calculator-only goal list.
- Do not reuse Strategy Builder's recursive `pc-condition-editor`; there is no
  predicate tree in this narrowed Calculator.

## Reference images

- `design/refs/calculator-current-phase-c.png` - current Calculator layout and
  empty concrete Input item frame.
- `design/mockups/item-display/variant-g-unified-six-slot.png` - approved shared
  item-frame hierarchy.
- `design/mockups/calculator/variant-e-stacked-context-rail.png` - approved
  Calculator information hierarchy. The overall layout must not be redesigned.

## Populated-state content

Use these values as layout truth; image-model text is illustrative only:

- base: `Vaal Regalia` / `iLvl 86`
- input rarity: `Rare`
- goal rarity: `Rare`
- action: `Chaos`
- goal success: `All 3`
- exact result: `1.64%`
- target prefix: `P1` / `T1 (best)` /
  `+(91-100) to maximum Energy Shield`
- target prefix: `P2` / `T1 (best)` /
  `(101-110)% increased Energy Shield`
- target suffix: `S1` / `T1 (best)` /
  `+(46-48)% to Cold Resistance`
- target-row odds may appear as `3.99%`, `4.21%`, and `12.08%`, but the engine
  result remains authoritative.

## Required Goal states

1. Empty: Rare Vaal Regalia target with three prefix and three suffix positions;
   every row says `No prefix requirement` or `No suffix requirement`.
2. Populated: three target modifier rows and three unrequired rows.
3. Dense: all six rows contain target modifier families, wrapped without
   clipping at the existing left-rail width.
4. Active: Goal card has the same Calculator context-card selection treatment
   as today and the shared pool says `Editing goal`.
5. Inactive: Goal remains fully legible with the same neutral item-frame body as
   Input item.

Loading, illegal, unsupported, error, cost, coverage, and technical-distribution
states remain in the unchanged Odds inspector and are not redesigned here.

## Interactions

- Clicking either item frame selects whether the shared modifier pool edits the
  concrete input or the goal.
- Clicking a pool tier while Goal is active adds or updates that modifier family
  at the selected tier or better.
- A target row can change its tier threshold and remove the requirement.
- `Finished rarity` and `Success means` retain their current native-backed
  behavior and may be placed in the Goal header or a compact footer.
- Goal rows never offer concrete-item fracture or direct-roll interactions.

## Shared frame requirements

- Match Input item for base and item-level header, rarity-colored border/header,
  prefix/suffix dividers, side rails, fixed `P1-P3` / `S1-S3` positions,
  typography, spacing, and empty-row rhythm.
- Target rows use modifier-family text and `Tn or better`, never pretend to be
  an exact rolled numeric value beyond the engine/catalog tier label.
- Add one restrained `TARGET` or `GOAL` cue. Do not tint the entire card, add an
  ornamental frame, or create a second visual system.
- Keep the accepted compact Path of Exile tool aesthetic: 1 px borders, 3 px
  radii, Segoe UI / Cascadia Mono, no gradients, glow, glass, or large KPI cards.

## Existing palette

- `--pc-bg: #14110d`
- `--pc-bg-raised: #1d1a14`
- `--pc-bg-panel: #211d16`
- `--pc-border: #3a3327`
- `--pc-text: #c8b88f`
- `--pc-text-dim: #8a7d5e`
- `--pc-text-strong: #e8dcae`
- `--pc-accent: #af6025`
- `--pc-prefix: #78d6c4`
- `--pc-suffix: #d8a7f2`
- `--pc-normal: #c6c6c6`
- `--pc-magic: #8888ff`
- `--pc-rare: #ffff77`

## Image-model directions

Generate two structurally distinct treatments while holding the overall
Calculator layout fixed:

- Variant A - literal twin: Goal is almost exactly the Input `pc-mod-list`, with
  tier selectors and remove controls embedded at the right of occupied rows;
  rarity and success controls sit in a compact band above the frame.
- Variant B - quiet target frame: Goal keeps the same header and six-slot ledger,
  but rows use a restrained target marker and the rarity/success controls live in
  a compact footer so the item silhouette dominates.

Flat 16:9 desktop UI, no browser frame, no watermark. Mocks control hierarchy
only; implementation uses real engine/catalog text and values.
