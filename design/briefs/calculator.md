# Calculator Visual Redesign Brief

## Purpose

The Calculator answers one focused question: for this Path of Exile 1 item,
goal, and crafting action, what exact outcomes and costs does the native engine
report? The redesign must make the answer legible before the raw distribution,
keep goal authoring and action selection fast, and visually belong beside the
Emulator. This phase is arrangement and styling only; it must preserve every
existing behavior, engine call, and result.

## Current-state diagnosis

Reference images:

- `design/refs/calculator-current.png` — current Calculator with a real
  Vaal Regalia, one T1 Energy Shield goal, and Chaos selected.
- `design/refs/emulator-current.png` — accepted baseline aesthetic.

The current three equal-height columns spread attention evenly across tasks
that are not equally important. A short goal leaves a large empty column, the
modifier pool is squeezed despite being the main authoring surface, and the
Odds column leads with cost then immediately becomes a raw 40-row table. The
Emulator works better because it uses restrained borders, lets the modifier
pool dominate the center, and treats secondary history/details as quieter
supporting regions.

## Content inventory

Use these values verbatim in the representative populated-state mockup unless
the layout needs to abbreviate a value. Values and labels come from the running
app on a Vaal Regalia at item level 86.

### Workspace chrome and item input

- Product title: `poecraft`
- Global actions: `+ Emulator`, `+ Strategy`, `+ Calculator`, `Stash`
- Active dock tab: `Calculator`
- Base action: `Change base…`
- Base summary: `Vaal Regalia · iLvl 86`
- Fresh-item rarity: `Rare`
- Fresh-item action: `New item`
- Collapsed item summary: `Item · Rare · 0/3P · 0/3S`
- Busy/status copy: `Calculating…`

### Action selection

- Mechanic tabs: `Basic currency`, `Essences`, `Harvest`, `Fossil`,
  `Eldritch`, `Influenced`, `Veiled`
- Basic actions: `Transmute`, `Augment`, `Alteration`, `Regal`, `Alchemy`,
  `Chaos`, `Exalt`, `Annul`, `Scour`, `Fracture`, `Restart (fresh base)`
- Representative selected action: `Chaos`
- Selection label may read `Selected: Chaos`, but selection must also be
  obvious on the button itself.

### Goal authoring

- Section title: `Goal`
- Rarity label and value: `Finished rarity` / `Rare`
- Empty-state copy: `Click modifiers in the pool to define the goal.`
- Group control: `Or require any mod from a group…`
- Populated representative goal:
  - side `P`
  - `+(91-100) to maximum Energy Shield`
  - threshold `T1 (best)`
  - per-slot exact chance `3.99%`
- Two-or-more-slot headline: `All 3 mods · 0.07%`
- Dense state supports eight rows. Use this realistic label set:
  - `P · +(91-100) to maximum Energy Shield · T1 (best)`
  - `P · (101-110)% increased Energy Shield · T1 (best)`
  - `P · (39-42)% increased Energy Shield / (16-17)% increased Stun and Block Recovery · T1 (best)`
  - `S · +(46-48)% to Cold Resistance · T1 (best)`
  - `S · +(46-48)% to Fire Resistance · T1 (best)`
  - `S · +(46-48)% to Lightning Resistance · T1 (best)`
  - `S · +(31-35)% to Chaos Resistance · T1 (best)`
  - `S · +(51-55) to Intelligence · T1 (best)`
- Each row needs a side marker, readable modifier name, tier selector,
  optional probability, and remove action.

### Modifier pool

- Section title: `Modifier Pool`
- Instruction: `click a tier to require it in the goal (that tier or better)`
- Search placeholder: `Search mods, groups, stat text…`
- Tabs: `Prefixes`, `Suffixes`, `Implicits`
- Source groups and counts: `Base Mod Pool · 8`, `Shaper Mods · 5`,
  `Elder Mods · 4`, `Crusader Mods · 4`, `Warlord Mods · 3`,
  `Redeemer Mods · 4`, `Hunter Mods · 4`, `Crafted Mods · 25`,
  `Essence Mods · 6`, `Fossil Mods · 0`
- Expanded family: `+(91-100) to maximum Energy Shield · 11 tiers`
- Visible tier examples:
  - `T1 · +(91-100) to maximum Energy Shield · iLvl 75 · 1,000`
  - `T2 · +(77-90) to maximum Energy Shield · iLvl 69 · 1,000`
  - `T3 · +(62-76) to maximum Energy Shield · iLvl 60 · 1,000`
- Tag chips: `DEFENCES`, `ENERGY SHIELD`

### Exact answer and diagnostics

- Section title: `Odds`
- The main answer must precede diagnostics: `All 3 mods in one Chaos` /
  `0.07%` and a secondary interpretation `about 1 in 1,429 attempts`.
  The reciprocal is presentation derived from the displayed engine
  probability, not independent crafting logic.
- Single-goal representative answer: `3.99%`
- Cost title: `Cost per attempt`
- Price row: `chaos`, editable `price`, subtotal `—`
- Missing-price total: `set prices above`
- Populated-price example: `1.0 chaos` / `Total · 1.0c`
- Distribution title: `Outcome classes (8780)`
- Columns: `Chance`, `Rarity`, `Affixes`, `G1`, `G2`, `G3`, `Flags`
- Representative rows from the engine result:
  - `0.20% · Rare · 3P/1S · ·`
  - `0.16% · Rare · 3P/1S · ~`
  - `0.15% · Rare · 3P/3S · ·`
  - `0.13% · Rare · 2P/2S · ·`
- Dense table shows 40 rows and ends with
  `…and 8740 more classes totalling 94.8%.`
- Slot marks: `✓` satisfied, `~` present below tier, `×` blocked, `·` absent.

## Required states

1. **Empty:** base exists, no action or goal. Preserve `No action selected`,
   `Click modifiers in the pool to define the goal.`, and
   `Define a goal to compute odds against.`
2. **Loading:** retain the authored goal and selected action, disable mutating
   controls, and show `Calculating…` close to the answer region.
3. **Illegal:** show `This action is not legal on the current item.` with the
   chosen action and item still visible.
4. **Unsupported:** show `No exact evaluator for this action yet.`
5. **Error:** show the engine message without hiding the inputs that caused it.
6. **Populated:** lead with the combined probability, then per-goal odds, cost,
   then the outcome distribution.
7. **Dense:** eight goal rows plus 40 visible outcome rows must remain usable
   without shrinking text below the existing 10–13 px scale. Independent
   region scrolling is acceptable.
8. **Expanded item:** the item summary opens a 320–460 px item-card popover.
9. **Expanded modifier family:** tiers show range, tags, item level, and weight.
10. **Hover/focus:** action buttons, tabs, goal rows, table rows, and editable
    price inputs need visible states that do not rely on glow effects.

## Interactions

- Change the base, fresh rarity, or fresh item from the compact top band.
- Expand/collapse the input item without making it a permanent column.
- Switch mechanic tabs, configure their selectors, and select exactly one
  action; the selected action recalculates immediately.
- Change finished rarity.
- Add a goal by clicking a modifier tier; change its threshold or remove it.
- Add a group goal with the combobox.
- Search and switch prefix/suffix/implicit modifier pools; expand families.
- Edit missing currency prices inline; all tabs share the saved price.
- Scroll long goal, pool, and distribution regions without losing the selected
  base, action, and headline probability.

## Hard constraints

- Desktop-first, dark theme, inside a Dockview tab.
- Plain CSS and Web Components; no React and no UI framework replacement.
- The engine remains the only authority for legality, probabilities, weights,
  outcomes, and costs. This phase changes no behavior or engine calls.
- Keep the Emulator's compact, low-decoration Path of Exile tool aesthetic.
  Avoid oversized cards, dashboard KPI tiles, neon glows, gradients, glass,
  ornamental fantasy frames, currency art, or generic SaaS styling.
- Use restrained 1 px borders, squared 3 px radii, dense typography, and the
  existing Segoe UI / Cascadia Mono pairing.
- Existing palette tokens, supplied to the image model verbatim:
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
  - `--pc-implicit: #aebbd0`
  - `--pc-fractured: #d8a65b`
  - `--pc-normal: #c6c6c6`
  - `--pc-magic: #8888ff`
  - `--pc-rare: #ffff77`
- The mockup should be a flat 16:9 desktop UI at approximately 1280×720,
  showing the Calculator tab only, with no browser frame and no watermark.

## Structural questions for variants

Generate genuinely different information hierarchies, not recolors:

- **Workbench:** compact goal rail at left, wide pool in the center, answer
  inspector at right; a much stronger headline inside the inspector.
- **Goal-first:** upper authoring strip combines item, action, and horizontal
  goal chips; lower split gives the pool most width and keeps an answer ledger
  beside it.
- **Answer-first:** a persistent top answer band summarizes probability and
  cost, with a lower two-pane goal/pool authoring workspace and collapsible
  detailed distribution.

The chosen direction must still support the later S6 Solve panel without
claiming space for it in this restyle phase.
