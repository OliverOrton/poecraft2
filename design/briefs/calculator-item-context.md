# Calculator Item / Goal Context Brief

## Grounded current behavior

- The Calculator owns one concrete engine item, one abstract goal, and one
  selected action.
- An Emulator item enters Calculator through the Emulator's `Odds` command.
- A saved Stash item enters Calculator through the Stash card's `Odds` command.
- `Change base…` uses the shared `pc-base-picker`; confirming a different base
  and item level creates a fresh item for the new session.
- Calculator has no internal Stash chooser and no clipboard/text item parser.
- The concrete input item is read-only in Calculator today. Modifier-pool
  clicks author goal slots, not item modifiers.

## Reuse inventory

- Reuse `pc-mod-list` for the concrete input item. It already renders rarity,
  influence, implicits, fixed prefix/suffix slots, tier helpers, crafted and
  fractured states from engine item information.
- Reuse `pc-base-picker` for base and item-level changes.
- Reuse `pc-mod-pool` for both contexts, but make its active context explicit:
  input inspection versus goal authoring. Do not duplicate pool or weight
  rules in Calculator.
- Reuse `WorkspaceApi.openCalculator(ItemSnapshot)` for Emulator and Stash
  handoffs. A Calculator-local saved-item chooser would be a new presentation
  component over the existing Stash records, not a second persistence path.
- Do not force the abstract goal into `pc-mod-list`. A goal slot is a family or
  group threshold, not a concrete rolled modifier. Give goal requirements the
  same item-card visual language while preserving that distinction.

## Interaction model

Two visible, selectable surfaces share one Modifier Pool:

1. **Input item** — a concrete engine item. Selecting it shows actual item
   tiers and live engine pool/weight state. The initial design keeps pool
   clicks read-only until direct item editing is explicitly chosen as product
   behavior. Actions remain available for exact calculation, not mutation.
2. **Goal requirements** — abstract success conditions. Selecting it makes
   pool tiers clickable; clicks add or update a family threshold and selected
   tiers stay highlighted. Rarity and `At least N of M modifiers` are part of
   this goal surface.

The active surface needs a clear accent border, a small `Editing` or `Viewing`
label, and matching pool title/instruction. Keyboard focus must be visible.
The pool must never look editable when it is only inspecting the input.

## Content for mockups

- Base: `Vaal Regalia · iLvl 86`
- Input item: `Rare`, empty or realistically populated fixed prefix/suffix
  slots, `Change base…`, `Choose item…`, and `New item` affordances.
- Goal: `Rare`, `At least 2 of 3 modifiers`, with requirements for maximum
  Energy Shield, increased Energy Shield, and Cold Resistance. Tier controls
  are explicit and the active requirement is highlighted in the pool.
- Shared pool: `Modifier Pool · Editing Goal` or
  `Modifier Pool · Viewing Input`, search, Prefix/Suffix/Implicit tabs,
  source sections, family rows, tier expansion, and selected-tier treatment.
- Selected action: `Chaos` in the existing full mechanic/action band.
- Answer: success chance first; then a compact `Goals met` distribution
  (`3 of 3`, `2 of 3`, `1 of 3`, `0 of 3`); raw engine classes collapsed under
  `Raw outcome classes (8780)`.

## Structural variants

### Variant D — Paired cards above the pool

The main authoring workbench (left/center) has Input Item and Goal Requirements
cards side by side above a full-width shared pool. Odds remains a persistent
right inspector. Both context cards remain visible and directly clickable.

### Variant E — Stacked context rail

A left rail stacks Input Item and Goal Requirements cards. The wider center is
the shared pool; Odds remains at right. The active card owns the center pane.
This preserves Variant A's three-pane rhythm while making the input concrete.

## Constraints

- Preserve the existing compact Emulator aesthetic and palette from
  `design/briefs/calculator.md`.
- Desktop-first at approximately 1280×720, Dockview tab, no browser frame.
- Plain CSS/Web Components feasible: simple grids, 1 px borders, 3 px radii,
  10–13 px visual type, no ornamental item frame or oversized dashboard cards.
- No invented crafting authority. Item state, pool availability, weights,
  legality, probability, and outcome dimensions come from the native engine.
- Label the goal as requirements, not as an already-existing finished item.

