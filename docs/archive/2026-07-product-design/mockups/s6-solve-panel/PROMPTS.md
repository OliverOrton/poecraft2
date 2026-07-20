# S6 Phase 1 Solve Panel - Image Model Prompts

All variants use `design/briefs/s6-solve-panel.md` as content authority. Values
are grounded in the live browser runtime; image text is illustrative and must
not replace engine truth.

## Shared references

- Image 1: `design/refs/s6-solve-calculator-current.png` - live current
  Calculator at 1600x900.
- Image 2: `design/refs/emulator-current.png` - accepted visual density and
  restrained item-tool tone.
- Image 3: `design/refs/strategy-builder-current-phase-c.png` - current Strategy
  Board and Simulator layout.
- Image 4: `design/refs/strategy-calculator-phase-c-preview.png` - existing
  board annotation and shared price-row language.
- Image 5: `design/refs/s6-solve-calculator-tall-current.png` - Oliver's tall
  current Calculator screenshot showing the selected empty lower workspace.

Generate new flat product mockups; do not literally edit or collage the
references.

## Selected refinement - Calculator natural lower workspace

```text
Use case: precise-object-edit
Asset type: shippable tall desktop Calculator UI placement mockup
Input images: Image 5 is the edit target and layout authority; Image 4 is a supporting reference for the existing compact price-row and result language
Primary request: change only the currently empty black lower area beneath the Modifier Pool and Odds columns in Image 5. Add the Phase 1 `SOLVE TO STRATEGY` surface there. Preserve every existing pixel-level layout decision above it and preserve the full left Input Item and Goal Item rail unchanged.
Placement: the new Solve surface begins below the existing Modifier Pool content and spans the center plus right columns only, from the left edge of the Modifier Pool to the right edge of the Odds column. It is part of the normal vertical Calculator page flow, not a fixed viewport dock. Do not shrink, move, replace, or tab the current Modifier Pool or Odds content. Do not cover or extend under the left item rail.
Solve layout: one continuous bordered region with a slim header `SOLVE TO STRATEGY`; two practical columns separated by a 1px divider. Left column is `PRICE READINESS` with `10 / 354 actions priced`, orange `340 missing price keys`, compact existing-style editable rows `chaos 1.00c`, `alteration 0.20c`, and one raw missing key, plus `Show all 340 missing`, `Unpriced actions are excluded`, and a closed `Advanced` disclosure. Right column is `SOLVE RESULT`, led by `Expected cost 2.9319c`, followed by `Converged · 35 states · 73 sweeps · residual 0`, prominent `344 actions skipped`, primary button `Open in Strategy Board`, secondary `Verify 5,000 runs`, and compact comparison `Exact 2.9319c · Empirical 2.9424c · Delta +0.0105c`.
Text (verbatim where legible): `SOLVE TO STRATEGY`, `PRICE READINESS`, `10 / 354 actions priced`, `340 missing price keys`, `chaos`, `1.00c`, `alteration`, `0.20c`, `Show all 340 missing`, `Unpriced actions are excluded`, `Advanced`, `SOLVE RESULT`, `Expected cost`, `2.9319c`, `Converged`, `35 states`, `73 sweeps`, `residual 0`, `344 actions skipped`, `Open in Strategy Board`, `Verify 5,000 runs`, `Exact 2.9319c`, `Empirical 2.9424c`, `Delta +0.0105c`
Style/medium: precise realistic edit of the existing production UI, dense Path of Exile expert tool, plain CSS and native Web Components feasible
Color palette: match Image 5 exactly; #14110d #1d1a14 #211d16 #3a3327 #c8b88f #8a7d5e #e8dcae #af6025
Constraints: change only the empty center/right lower workspace; preserve all existing Calculator content, text, proportions, scrollbars, tabs, controls, item frames, pool rows, Odds sections, and column widths; no full-width dock, no inspector tabs, no Strategy Builder mode, no solve progress bar or solve Cancel button; one continuous technical panel, not floating cards; dense 10-13px typography, 1px borders, 3px radii; no gradients, glow, glass, fantasy ornament, currency art, oversized KPI cards, generic SaaS dashboard, browser frame, watermark, or invented crafting mechanics
```

## Variant A - Calculator Solve inspector (recommended)

```text
Use case: ui-mockup
Asset type: shippable 1600x900 desktop application UI mockup, flat screenshot inside one Dockview Calculator tab
Input images: Image 1 is the live Calculator structure and palette; Image 2 is the accepted restrained visual tone; Image 4 shows the existing compact price rows and annotation vocabulary
Primary request: preserve Image 1's Calculator workbench exactly: mechanic/action bands at top, stacked Input and Goal item rail on the left, Modifier Pool in the center, and the existing right inspector width. Add a compact `Odds | Solve` switch at the top of the right inspector with `Solve` active. The Solve inspector is in its successful-result state and must fit without becoming a generic dashboard.
Solve inspector hierarchy: kicker `SOLVE TO STRATEGY`; source line `Vaal Regalia · iLvl 86 · Rare` and `All 2 goal modifiers`; one compact price-readiness block `10 of 354 actions priced`, orange warning `344 actions excluded · 340 missing price keys`, two existing-style editable price rows (`chaos` value `1.00`, `alteration` value `0.20`) and a bounded `Show all 340 missing` disclosure; closed `Advanced` disclosure; a compact result led by `Expected cost` and `2.9319c`; metrics `Converged`, `35 states`, `73 sweeps`, `Residual 0`; prominent warning row `344 actions skipped`; primary button `Open in Strategy Board`; secondary button `Verify 5,000 runs`; comparison row `Exact 2.9319c · Empirical 2.9424c · Delta +0.0105c`; quiet link `Download solve log`.
Text (verbatim where legible): `CALCULATOR WORKBENCH`, `Odds`, `Solve`, `SOLVE TO STRATEGY`, `Vaal Regalia`, `iLvl 86`, `Rare`, `All 2 goal modifiers`, `10 of 354 actions priced`, `344 actions excluded`, `340 missing price keys`, `Unpriced actions are excluded`, `Show all 340 missing`, `Advanced`, `Expected cost`, `2.9319c`, `Converged`, `35 states`, `73 sweeps`, `Residual 0`, `344 actions skipped`, `Open in Strategy Board`, `Verify 5,000 runs`, `Exact 2.9319c`, `Empirical 2.9424c`, `Delta +0.0105c`, `Download solve log`
Style/medium: realistic production product UI, dense expert crafting tool, plain CSS-feasible, not concept art
Color palette: #14110d #1d1a14 #211d16 #3a3327 #c8b88f #8a7d5e #e8dcae #af6025; prefix #78d6c4; suffix #d8a7f2; rare #ffff77
Constraints: the placement decision must be obvious: Solve lives in Calculator's right inspector; preserve the current workbench and existing one-step Odds context; reuse compact key/value/missing-price rows; no progress bar or Cancel control for the solve itself; no new badge system; dense 10-13px typography, 1px borders, 3px radii; no gradients, glow, glass, fantasy ornament, currency art, oversized KPI cards, generic SaaS dashboard, browser frame, watermark, or invented crafting controls
```

## Variant B - Calculator bottom Solve dock

```text
Use case: ui-mockup
Asset type: shippable 1600x900 desktop application UI mockup, flat screenshot inside one Dockview Calculator tab
Input images: Image 1 is the live Calculator structure; Image 2 supplies accepted density and border rhythm; Image 4 supplies the existing compact price-row treatment
Primary request: preserve Image 1's top mechanic/action bands and three-column Calculator workbench, including the right Odds inspector with the live `0.1329%` Chaos answer. Compress the workbench vertically just enough to add a shallow full-width `SOLVE TO STRATEGY` dock across the bottom. Make the bottom dock a continuous four-part expert-tool ledger, not floating cards.
Bottom dock structure: first region is source and goal summary `Vaal Regalia · iLvl 86 · Rare` and `All 2 modifiers`; second and widest region is shared price readiness with `10 / 354 actions priced`, `340 missing keys`, three compact existing-style price rows and `Show all`; third region is solve result with headline `2.9319c expected`, `Converged · 35 states · 73 sweeps · residual 0`, and prominent `344 skipped`; fourth region is action/verification with buttons `Solve again`, `Open in Strategy Board`, `Verify 5,000 runs`, plus `2.9424c empirical · +0.0105c delta`. Include a small closed `Advanced` disclosure without adding a second toolbar.
Text (verbatim where legible): `CALCULATOR WORKBENCH`, `ODDS`, `0.1329%`, `SOLVE TO STRATEGY`, `Vaal Regalia`, `iLvl 86`, `Rare`, `All 2 modifiers`, `PRICE READINESS`, `10 / 354 actions priced`, `340 missing keys`, `Unpriced actions are excluded`, `Show all`, `2.9319c expected`, `Converged`, `35 states`, `73 sweeps`, `residual 0`, `344 skipped`, `Advanced`, `Solve again`, `Open in Strategy Board`, `Verify 5,000 runs`, `2.9424c empirical`, `+0.0105c delta`
Style/medium: realistic shippable product UI, compact technical ledger, plain CSS-feasible, not concept art
Color palette: #14110d #1d1a14 #211d16 #3a3327 #c8b88f #8a7d5e #e8dcae #af6025; prefix #78d6c4; suffix #d8a7f2; rare #ffff77
Constraints: placement must be obvious: Solve remains in Calculator but uses a bottom dock while Odds stays visible; one continuous dock with dividers, not KPI cards; price rows visibly match the existing UI; no solve progress bar or solve Cancel control; preserve dense tool hierarchy and practical Modifier Pool width; 10-13px type, 1px borders, 3px radii; no gradients, glow, glass, fantasy ornament, currency art, oversized cards, generic SaaS dashboard, browser frame, watermark, or invented mechanics
```

## Variant C - Simulator/Strategy Builder Solver mode (alternative)

```text
Use case: ui-mockup
Asset type: shippable 1600x900 desktop application UI mockup, flat screenshot inside one Dockview Strategy Builder tab
Input images: Image 3 is the current Strategy Builder and Simulator structure; Image 4 is the existing Calculator-mode board annotation and cost-ledger reference; Image 1 supplies the authored Vaal Regalia goal context; Image 2 supplies accepted visual restraint
Primary request: show the meaningful alternative placement from the solver plan: Solver lives in Strategy Builder beside the existing `Simulator | Calculator` modes. Add a compact three-way mode switch `Simulator | Calculator | Solver` with `Solver` active. Keep the generated ordinary strategy graph visible in the upper board and use the existing lower work surface for source/goal, shared price readiness, solve result, and verification. This is a placement study, not a new visual system.
Upper board: ordinary editable dark graph with start/router/operation/success nodes; several operation nodes show compact existing-style expected-cost annotations such as `~2.93c to go`, `~1.40c to go`, and `~0.50c to go`; edge labels remain in the existing annotation style; right inspector still edits the selected operation node.
Lower Solver surface: left source block `Vaal Regalia · iLvl 86 · Rare` and one-family goal `Any tier increased Energy Shield`; center shared price readiness `10 / 10 chosen actions priced` with existing compact rows for `alteration 0.20c`, `scour 0.50c`, and `base 5.00c`; right result `Expected cost 2.9319c`, `Converged · 35 states · 73 sweeps`, buttons `Re-solve`, `Verify 5,000 runs`, and comparison `Exact 2.9319c · Empirical 2.9424c · Delta +0.0105c`.
Text (verbatim where legible): `Strategy Builder`, `Simulator`, `Calculator`, `Solver`, `SOLVE FROM GOAL`, `Vaal Regalia`, `iLvl 86`, `Rare`, `Any tier increased Energy Shield`, `PRICE READINESS`, `10 / 10 chosen actions priced`, `alteration`, `0.20c`, `scour`, `0.50c`, `base`, `5.00c`, `Expected cost`, `2.9319c`, `Converged`, `35 states`, `73 sweeps`, `Re-solve`, `Verify 5,000 runs`, `Exact 2.9319c`, `Empirical 2.9424c`, `Delta +0.0105c`, `~2.93c to go`
Style/medium: realistic production product UI, dense Blueprint-like crafting graph and technical bottom ledger, plain CSS/Web Components feasible, not concept art
Color palette: #14110d #1d1a14 #211d16 #3a3327 #c8b88f #8a7d5e #e8dcae #af6025; prefix #78d6c4; suffix #d8a7f2; magic #8888ff; rare #ffff77
Constraints: make the Simulator/Strategy Builder placement unmistakable; preserve existing graph, palette, inspector, and lower work-surface identity; expected-cost labels must look like the existing node annotation badge rather than a second badge layer; no graph minimap, no floating KPI cards, no solve progress/cancel controls, no gradients, glow, glass, fantasy ornament, currency art, generic SaaS dashboard, browser frame, watermark, React conventions, or invented mechanics; dense 10-13px type, 1px borders, 3px radii
```
