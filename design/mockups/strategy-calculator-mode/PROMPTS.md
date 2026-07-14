# Strategy Calculator Mode Mockup Prompts

All prompts use the same references and source brief:

- Image 1: `design/refs/strategy-builder-current-phase-c.png` — current
  Strategy Builder structure and graph.
- Image 2: `design/refs/calculator-current-phase-c.png` — current Calculator
  answer, cost, table, and context density.
- Image 3: `design/refs/emulator-current.png` — accepted restrained visual
  tone and fixed technical rows.
- Content authority: `design/briefs/strategy-calculator-mode.md`.

The images are hierarchy studies. Runtime text and numbers remain engine-owned.

## Variant A — Bottom analysis ledger

Use case: ui-mockup

Asset type: shippable 1280×720 desktop application UI mockup, flat screenshot
inside one Dockview Strategy Builder tab.

Input images: Image 1 is the structural reference and current graph; Image 2
is the compact exact-result and table reference; Image 3 is the accepted
density, typography, border, and item-tool aesthetic. Generate a new mockup;
do not edit the references literally.

Primary request: preserve Image 1's palette, graph board, and right Strategy
inspector across the upper workspace. Add a compact segmented mode control
`Simulator | Calculator` in the Strategy Builder toolbar with Calculator
active. Replace the lower Simulator runner with a two-column analysis ledger:
left side leads with exact Success, expected actions, convergence, and three
editable expected-cost rows; right side is a selected-node drill-down table.
Annotate the existing graph with small probability labels on edges and compact
visit badges on nodes. Make the lower ledger feel connected to the selected
Alteration node, not like a generic dashboard.

Text (verbatim where visible): `Strategy Builder`, `Simulator`, `Calculator`,
`Alteration until T1 Energy Shield, then Regal`, `Success`,
`99.9999999999%`, `81.7014 expected actions`, `Converged · 2,219 sweeps`,
`Expected cost`, `Alteration`, `79.7014`, `Transmute`, `1.0000`, `Regal`,
`1.0000`, `17.0403c total`, `Alteration · 79.701 expected visits`, `Share`,
`Rarity`, `Affixes`, `G1`, `Flags`, `5.5214%`, `Magic`, `1P/0S`, `Absent`,
`…other classes · 76.6401%`. Board labels include `1.24%`, `98.76%`,
`79.70×`, and terminal `100%`.

Style/medium: realistic production product UI, not concept art; plain
CSS-feasible grid, tables, inputs, tabs, nodes, and connectors.

Color palette: #14110d base, #1d1a14 raised, #211d16 panels, #3a3327 borders,
#c8b88f text, #8a7d5e dim text, #e8dcae strong text, #af6025 accent,
#78d6c4 prefix, #d8a7f2 suffix, #8888ff magic, #ffff77 rare.

Constraints: preserve the current graph editor identity and node geometry;
dense 10–13 px visual type; 1 px borders; 3 px radii; no gradients, glow,
glass, oversized KPI cards, generic SaaS dashboard, fantasy ornament,
currency art, graph minimap, browser frame, watermark, invented crafting
controls, or React conventions. The result must be structurally distinct from
Variants B and C.

## Variant B — Calculator inspector

Use case: ui-mockup

Asset type: shippable 1280×720 desktop application UI mockup, flat screenshot
inside one Dockview Strategy Builder tab.

Input images: Image 1 is the current Strategy Builder and graph reference;
Image 2 supplies exact-result/cost/table density; Image 3 supplies the accepted
visual tone. Generate a new mockup; do not edit the references literally.

Primary request: make the graph board the full available height between the
left Palette and a widened right Calculator analysis inspector. Calculator is
active in a compact `Simulator | Calculator` toolbar switch. Stack three
sections in the right inspector: one strong exact Success answer and
convergence line, a compact expected-consumption cost ledger, then the selected
Alteration node's incoming-state table. Remove the large bottom runner and use
only a very slim bottom status strip for `Exact · Converged · 17.0403c`.
Annotate graph edges with compact probabilities and nodes with visit badges;
the selected Alteration node and connected labels have strongest contrast.

Text (verbatim where visible): `Strategy Builder`, `Simulator`, `Calculator`,
`Success`, `99.9999999999%`, `Converged`, `81.7014 actions`, `Expected cost`,
`Alteration · 79.7014 × 0.20c`, `Transmute · 1.0000 × 0.10c`,
`Regal · 1.0000 × 1.00c`, `Total · 17.0403c`, `Incoming states`,
`Alteration · 79.701 visits`, `G1 · T1 +(91-100) to maximum Energy Shield`,
`5.5214%`, `Magic`, `1P/0S`, `Absent`, `…76.6401% other classes`,
`Exact · Converged · 17.0403c`. Board labels include `1.24%`, `98.76%`,
`79.70×`, and `100%`.

Style/medium: realistic production product UI, compact expert tool matching
the references; plain CSS-feasible vertical inspector and board.

Color palette: #14110d, #1d1a14, #211d16, #3a3327, #c8b88f, #8a7d5e,
#e8dcae, #af6025, #78d6c4, #d8a7f2, #8888ff, #ffff77.

Constraints: retain Palette and editing toolbar; the right rail may scroll but
must not shrink the board below practical editing width; dense 10–13 px type;
1 px borders; 3 px radii; no gradients, glow, glass, giant KPI tiles, generic
SaaS styling, fantasy ornament, currency art, minimap, browser frame,
watermark, or invented controls. Structurally different from A and C.

## Variant C — Answer band and tabbed dock

Use case: ui-mockup

Asset type: shippable 1280×720 desktop application UI mockup, flat screenshot
inside one Dockview Strategy Builder tab.

Input images: Image 1 provides the current Strategy Builder graph and chrome;
Image 2 provides compact Calculator data treatments; Image 3 provides accepted
Emulator density and visual restraint. Generate a new mockup; do not edit the
references literally.

Primary request: add a slim full-width exact-answer band directly below the
Strategy Builder toolbar. It contains the `Simulator | Calculator` control,
one strong Success answer, expected actions, total expected cost, and
convergence status in a single restrained strip—not separate KPI cards. Keep
the palette/board/inspector row below it. Replace the old runner with a shallow
tabbed dock using tabs `Summary`, `Expected cost`, and `Node details`, with
`Node details` active and a wide technical table. The band keeps the answer
visible while the dock supports deep inspection. Annotate graph nodes and
edges; reduce unselected label contrast to demonstrate a dense-graph rule.

Text (verbatim where visible): `Strategy Builder`, `Simulator`, `Calculator`,
`Success 99.9999999999%`, `81.7014 expected actions`, `17.0403c expected`,
`Converged · 2,219 sweeps`, `Summary`, `Expected cost`, `Node details`,
`Alteration · 79.701 expected visits`, `G1 · T1 +(91-100) to maximum Energy Shield`,
`Share`, `Rarity`, `Affixes`, `G1`, `Flags`, `5.5214%`, `Magic`, `1P/0S`,
`Absent`, `…other classes · 76.6401%`. Board labels include `1.24%`,
`98.76%`, `79.70×`, and terminal `100%`.

Style/medium: realistic production product UI, not concept art; compact,
plain CSS-feasible tool with one answer strip and one tabbed technical dock.

Color palette: #14110d, #1d1a14, #211d16, #3a3327, #c8b88f, #8a7d5e,
#e8dcae, #af6025, #78d6c4, #d8a7f2, #8888ff, #ffff77.

Constraints: the answer band must be a single restrained ledger/toolbar, not
floating cards; preserve graph editing and right inspector; dense 10–13 px
type; 1 px borders; 3 px radii; no gradients, glow, glass, oversized cards,
SaaS styling, fantasy ornament, currency art, minimap, browser frame,
watermark, or invented controls. Structurally different from A and B.
