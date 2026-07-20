# Calculator Mockup Prompts

All variants use `design/refs/calculator-current.png` as the current-state
reference and `design/refs/emulator-current.png` as the accepted visual-tone
reference. The complete source of truth is `design/briefs/calculator.md`.

## Variant A — Workbench

Use case: ui-mockup

Asset type: shippable desktop application UI mockup, 1280×720, flat screenshot
inside one Dockview tab.

Input images: Image 1 is the current Calculator layout and content reference.
Image 2 is the accepted Emulator density, tone, border, and typography
reference. Create a new mockup; do not edit either screenshot literally.

Primary request: redesign the Path of Exile 1 crafting Calculator as a compact
three-pane workbench. Keep a narrow goal rail on the left, give the modifier
pool the widest center pane, and use a right answer inspector whose first
content is a prominent exact probability, followed by cost and a compact
outcome table. Preserve the top item band and the two-row mechanic/action band.

Text (verbatim where visible): `poecraft`, `Calculator`, `Vaal Regalia · iLvl
86`, `Basic currency`, `Chaos`, `Goal`, `All 3 mods`, `0.07%`, `Modifier Pool`,
`+(91-100) to maximum Energy Shield`, `T1 (best)`, `Odds`, `All 3 mods in one
Chaos`, `about 1 in 1,429 attempts`, `Cost per attempt`, `Outcome classes
(8780)`, `Chance`, `Rarity`, `Affixes`, `G1`, `Flags`.

Style/medium: realistic product UI, not concept art; compact expert desktop
tool matching Image 2; plain CSS-feasible rectangles, tables, tabs, inputs,
and restrained dividers.

Color palette: #14110d base, #1d1a14 raised, #211d16 panels, #3a3327 borders,
#c8b88f text, #8a7d5e dim text, #e8dcae strong text, #af6025 accent, #78d6c4
prefix, #d8a7f2 suffix, #ffff77 rare.

Constraints: dense 10–13 px visual type scale; 1 px borders; 3 px radii; no
gradients, neon, glow, glass, oversized cards, KPI dashboard tiles, fantasy
ornament, currency art, browser frame, React conventions, or watermark. Make
the layout structurally distinct from Image 1, not merely recolored. Engine
results are the focal hierarchy.

## Variant B — Goal-first

Use case: ui-mockup

Asset type: shippable desktop application UI mockup, 1280×720, flat screenshot
inside one Dockview tab.

Input images: Image 1 is the current Calculator layout and content reference.
Image 2 is the accepted Emulator density, tone, border, and typography
reference. Create a new mockup; do not edit either screenshot literally.

Primary request: redesign the Calculator around a goal-first horizontal
authoring workflow. Below the compact item and action bands, create a shallow
goal strip with three removable modifier chips, tier controls, per-mod odds,
and a combined `0.07%` summary. Below it, use a wide two-pane split: a spacious
modifier browser on the left and an answer ledger on the right. The answer
ledger leads with probability, then price, then an outcome table.

Text (verbatim where visible): `poecraft`, `Calculator`, `Vaal Regalia · iLvl
86`, `Basic currency`, `Chaos`, `Finished rarity`, `Rare`, `Goal`, `All 3 mods
· 0.07%`, `+(91-100) to maximum Energy Shield`, `T1 (best)`, `3.99%`,
`Modifier Pool`, `Search mods, groups, stat text…`, `Prefixes`, `Suffixes`,
`Implicits`, `Odds`, `0.07%`, `about 1 in 1,429 attempts`, `Cost per attempt`,
`Outcome classes (8780)`.

Style/medium: realistic product UI, not concept art; compact expert desktop
tool matching Image 2; plain CSS-feasible flex/grid layout.

Color palette: #14110d base, #1d1a14 raised, #211d16 panels, #3a3327 borders,
#c8b88f text, #8a7d5e dim text, #e8dcae strong text, #af6025 accent, #78d6c4
prefix, #d8a7f2 suffix, #ffff77 rare.

Constraints: desktop-only dense layout; goal chips must support eight rows by
wrapping or scrolling; 1 px borders; 3 px radii; no gradients, neon, glow,
glass, oversized cards, generic SaaS dashboard styling, fantasy ornament,
currency art, browser frame, or watermark. Structurally different from both
Image 1 and Variant A.

## Variant C — Answer-first

Use case: ui-mockup

Asset type: shippable desktop application UI mockup, 1280×720, flat screenshot
inside one Dockview tab.

Input images: Image 1 is the current Calculator layout and content reference.
Image 2 is the accepted Emulator density, tone, border, and typography
reference. Create a new mockup; do not edit either screenshot literally.

Primary request: redesign the Calculator with a persistent answer-first band.
After the compact item and action controls, show one slim full-width result
band: selected action, exact combined chance `0.07%`, reciprocal `about 1 in
1,429 attempts`, and `1.0c per attempt`. Below it, use a two-pane authoring
workspace with a compact goal editor at left and a much wider modifier pool at
right. Put the detailed outcome distribution in a docked lower drawer/table
that is visibly expandable and currently open.

Text (verbatim where visible): `poecraft`, `Calculator`, `Vaal Regalia · iLvl
86`, `Selected: Chaos`, `All 3 mods`, `0.07%`, `about 1 in 1,429 attempts`,
`1.0c per attempt`, `Goal`, `Finished rarity`, `Rare`, `Modifier Pool`,
`+(91-100) to maximum Energy Shield`, `T1 (best)`, `3.99%`, `Outcome classes
(8780)`, `Chance`, `Rarity`, `Affixes`, `G1`, `G2`, `G3`, `Flags`.

Style/medium: realistic product UI, not concept art; compact technical
workbench matching Image 2; plain CSS-feasible grid, tabs, controls, and table.

Color palette: #14110d base, #1d1a14 raised, #211d16 panels, #3a3327 borders,
#c8b88f text, #8a7d5e dim text, #e8dcae strong text, #af6025 accent, #78d6c4
prefix, #d8a7f2 suffix, #ffff77 rare.

Constraints: the answer band is a restrained toolbar/ledger, not four floating
KPI cards; dense 10–13 px visual type; 1 px borders; 3 px radii; no gradients,
neon, glow, glass, oversized cards, fantasy ornament, currency art, browser
frame, or watermark. Structurally different from Image 1 and the other two
variants.

## Variant D — Paired item and goal cards

Use case: ui-mockup

Create a realistic 1280×720 desktop application screenshot based on
`calculator-implemented.png`, following
`design/briefs/calculator-item-context.md`. Keep the full poecraft workspace
chrome and action bands. Replace the left Goal rail with a wider authoring
workbench: two substantial compact cards side by side above one shared
Modifier Pool. The left card is `INPUT ITEM`, showing `Vaal Regalia · iLvl 86`,
`Rare`, fixed prefix/suffix slots, `Change base…`, and `Choose item…`. The right
card is `GOAL REQUIREMENTS`, visibly selected with an accent border, showing
`Rare`, `At least 2 of 3 modifiers`, and three requirement rows with tier
thresholds. The pool below reads `MODIFIER POOL · EDITING GOAL`, and one T2
Energy Shield family/tier is highlighted. Keep a compact Odds inspector at
right with a headline success chance, a four-row `Goals met` summary, and a
collapsed `Raw outcome classes (8780)` disclosure.

Use #14110d, #1d1a14, #211d16, #3a3327, #c8b88f, #8a7d5e, #e8dcae,
#af6025, #78d6c4, #d8a7f2, and #ffff77. Dense 10–13 px type, restrained
1 px borders, 3 px radii. No gradients, glow, glass, fantasy ornament,
currency art, huge KPI tiles, browser frame, watermark, or invented Save
controls. The goal is requirements, not a fabricated finished item.

## Variant E — Stacked context rail

Use case: ui-mockup

Create a realistic 1280×720 desktop application screenshot based on
`calculator-implemented.png`, following
`design/briefs/calculator-item-context.md`. Preserve the existing poecraft
workspace chrome and full mechanic/action bands. Use three vertical panes:
a 300 px left context rail, a wide center Modifier Pool, and a compact right
Odds inspector. Stack two directly clickable cards in the left rail. The top
`INPUT ITEM` card uses Emulator-style fixed slots and reads `Vaal Regalia ·
iLvl 86`, `Rare`, with compact `Change base…` and `Choose item…` actions. The
lower `GOAL REQUIREMENTS` card is visibly selected, says `Rare · At least 2 of
3 modifiers`, and lists three tiered requirements. The center title reads
`MODIFIER POOL · EDITING GOAL`, with an Energy Shield T2 family and tier
highlighted. The Odds inspector leads with exact success chance, then a small
`Goals met` distribution and collapsed `Raw outcome classes (8780)`.

Match the Emulator-like dense dark tool aesthetic: #14110d, #1d1a14,
#211d16, #3a3327, #c8b88f, #8a7d5e, #e8dcae, #af6025, #78d6c4,
#d8a7f2, #ffff77; 10–13 px type, 1 px borders, 3 px radii. No gradients,
neon, glow, glass, fantasy ornament, currency art, oversized dashboard tiles,
browser frame, watermark, or invented Save controls.
