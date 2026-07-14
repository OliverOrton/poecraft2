# Strategy Builder Calculator Mode — Design Brief

## Purpose

Calculator mode answers the whole authored strategy graph exactly: where runs
finish, how often every edge is traversed, how many actions and currency units
the strategy consumes on average, and which abstract item states arrive at a
selected node. It lives inside the existing Strategy Builder and must make the
exact result readable without hiding the graph that produced it. Simulator
mode remains the current Monte Carlo runner and trace workflow.

## Current-state references and diagnosis

- `design/refs/strategy-builder-current-phase-c.png` — current populated
  Strategy Builder at 1280×720.
- `design/refs/calculator-current-phase-c.png` — current Calculator workbench
  and its compact context/pool/odds hierarchy.
- `design/refs/emulator-current.png` — accepted density and item-slot visual
  baseline.

The Strategy Builder currently gives the graph the central workspace, with a
palette at left and the selected graph/node inspector at right. A fixed 286 px
bottom runner splits Simulator controls and result tabs. Before a run, most of
that lower area is empty and its nine same-weight metric boxes do not establish
an answer hierarchy. Calculator mode should preserve the legible board and
editor chrome, replace only the runner/result region or deliberately repurpose
the inspector, and let edge/node annotations connect the exact result back to
the graph.

The current Calculator and Emulator establish the target visual language:
restrained 1 px dividers, compact uppercase section labels, fixed technical
rows, small inline controls, and one strong answer rather than a field of
dashboard cards.

## Representative exact result

These values came from a real Phase B `strategyEvaluate` call through the
WASM/web binding on a Vaal Regalia at item level 86. The graph is named
`Alteration until T1 Energy Shield, then Regal` and starts from a Normal item.
It Transmutes once, repeats Alterations until the T1
`+(91-100) to maximum Energy Shield` family is present, Regals once, then
reaches Success.

### Exact summary

- Status: `Converged · 2,219 sweeps`
- Success: `99.9999999999%` (`p = 0.9999999999989616`)
- Failure: `0%`
- Stop: `0%`
- Action not applied: `0%`
- No matching edge: `0%`
- Numerical residual: `9.8784e-13`
- Expected actions: `81.7014`

### Expected consumption and sample live prices

Consumption quantities are engine results. Prices below are explicit example
user inputs in chaos-equivalent units, not market claims.

| Currency | Expected quantity | Example price | Subtotal |
| --- | ---: | ---: | ---: |
| Alteration | `79.7014` | `0.20c` | `15.9403c` |
| Transmute | `1.0000` | `0.10c` | `0.1000c` |
| Regal | `1.0000` | `1.00c` | `1.0000c` |
| **Total expected cost** |  |  | **`17.0403c`** |

The missing-price state uses the same rows but shows `Set price` for Regal and
`Incomplete · 16.0403c known` for the total.

### Board annotations

Node badges use the engine's v1 expected-visit quantity unless Oliver chooses
to hide them pending a later reach-probability feature:

- Start: `1.00×`
- Transmute: `1.00×`
- Alteration: `79.70×`
- Regal: `1.00×`
- Success terminal: `100%`

Recommended edge labels show the conditional share of the source node's exits;
hover text shows the absolute expected traversals:

| Edge | Board label | Hover detail |
| --- | ---: | ---: |
| begin | `100%` | `1.0000 expected traversals` |
| transmute-hit | `1.24%` | `0.01239 expected traversals` |
| transmute-miss | `98.76%` | `0.98761 expected traversals` |
| alteration-hit | `1.24%` | `0.98761 expected traversals` |
| alteration-repeat | `98.76%` | `78.71383 expected traversals` |
| regal-done | `100%` | `1.0000 expected traversals` |

### Selected-node drill-down

Representative selected node: `Alteration`.

- Header: `Alteration · 79.701 expected visits`
- Target legend: `G1 · T1 +(91-100) to maximum Energy Shield`
- Status vocabulary: `Satisfied`, `Below tier`, `Absent`, `Blocked`
- Columns: `Share`, `Rarity`, `Affixes`, `G1`, `Flags`
- Top incoming state classes:
  - `5.5214% · Magic · 1P/0S · Absent · —`
  - `5.0967% · Magic · 1P/0S · Absent · —`
  - `4.6720% · Magic · 0P/1S · Absent · —`
  - `4.2473% · Magic · 1P/0S · Below tier · —`
  - `3.8225% · Magic · 0P/1S · Absent · —`
- Remainder: `…other classes · 76.6401%`

## Content inventory

### Persistent Strategy Builder chrome

- Active dock tab: `Alteration until T1 Energy Shield, then Regal`
- Toolbar title: `Strategy Builder`
- Save state: `Saved` or `Unsaved`
- Actions: `Save`, `Save As`, `Duplicate`, `Change base…`,
  `Delete selected`, `Auto layout`
- New mode control: `Simulator | Calculator`
- Base summary remains in the graph inspector: `Vaal Regalia`, `Item level 86`
- Palette remains available in both modes.

### Exact summary panel

- Mode/state line: `Calculator · Exact graph evaluation`
- Primary answer: `Success · 99.9999999999%`
- Secondary results: `Failure 0%`, `Stop 0%`, `Unresolved <0.000001%`
- Miss-signal rows when nonzero: `Action not applied`, `No matching edge`
- Attribution rows name the node, for example
  `No matching edge · chaos-check · 2.30%`.
- Work metric: `81.7014 expected actions`
- Convergence detail: `Converged · 2,219 sweeps`
- Cost section uses the three consumption rows and editable prices above.
- Evaluation is price-independent; price changes update only subtotals and
  total cost.

### Board

- Existing graph nodes, ports, edge labels, zoom/pan, drag, and selection.
- Calculator annotations add compact node badges and edge-probability labels.
- Terminal nodes show absorb probability instead of expected visits.
- Edge hover exposes absolute expected traversals.
- A selected node and its connected edges may receive stronger annotation
  contrast in dense graphs.
- Stale results keep the last annotations visible but dimmed, with a visible
  `Stale · graph changed` chip.

### Node details

- No selection: `Select an operation node to inspect its incoming states.`
- Operation selection: selected-node header, target legend, top-K class table,
  and truncated remainder.
- Router/start/terminal selection may show visits/absorb detail but does not
  fabricate an operation distribution.
- The technical table must remain usable with eight target columns and at
  least 40 rows through scrolling, not smaller text.

## Required states

1. **Valid and converged:** use all representative values above. The graph,
   summary, costs, annotations, and selected-node class table are visible.
2. **Evaluating/busy:** retain the graph and last result if one exists; show
   `Evaluating exact graph…`; disable conflicting graph execution controls.
3. **Empty/invalid graph:** preserve validation messages and show
   `Complete the graph to evaluate exact odds.` No engine call is made.
4. **Refusal:** frame the error with
   `This strategy uses actions or conditions Calculator mode cannot evaluate exactly yet.`
   Then render this captured engine message verbatim in a technical block:
   `poecraft engine error 4: strategy evaluation unsupported:`
   `- node 'veiled-reforge' operation 'veiled_chaos' has no exact calculator evaluator`
5. **Unresolved mass:** warning title `Evaluation did not converge`; example
   `100.0000% unresolved after 5 sweeps`; attribution
   `start · 100.0000% unresolved`; preserve any absorbed terminal results.
6. **Missing prices:** quantities remain visible, missing inputs say
   `Set price`, and total says `Incomplete · 16.0403c known`.
7. **Stale:** after a structural edit, dim annotations and result panel, show
   `Stale · graph changed`, then transition to Evaluating after the debounce.
8. **Dense graph:** 30+ nodes and edges. Annotation collisions must be
   controlled by prioritizing the selected node/connected edges, using compact
   labels, or lowering unselected annotation contrast. Do not make the board
   unreadable by showing every label at equal emphasis.
9. **Dense drill-down:** eight target columns and 40 state rows remain
   scrollable at the existing 10–13 px visual scale.

## Interactions

- Switch `Simulator | Calculator`. Simulator restores the current run controls
  and trace; Calculator restores the last exact result and annotations.
- Entering Calculator evaluates a valid graph.
- Structural changes evaluate again after roughly 300 ms. Node movement,
  viewport changes, labels, and graph name edits do not evaluate again.
- The mock review should decide whether this automatic behavior needs a small
  visible `Evaluate again` affordance or status-only feedback.
- Select an operation node to open its incoming-state drill-down.
- Hover an annotated edge to see absolute expected traversals.
- Edit prices inline; totals update immediately without re-evaluating.
- Move/zoom/pan the graph exactly as today. Calculator annotations must not
  block ports, edge creation, node drag, or selection.

## Hard constraints

- Desktop-first, dark theme, inside one Dockview Strategy Builder tab.
- Plain CSS and Web Components; no React and no UI framework replacement.
- The native engine remains the only crafting-rule, routing, legality, and
  probability authority. UI arithmetic is limited to edge-share presentation
  and price dot-products.
- Match the Emulator and current Calculator: compact expert tool, restrained
  1 px borders, squared 3 px radii, Segoe UI plus Cascadia Mono, low
  decoration, dense scan-friendly tables.
- Avoid oversized KPI cards, generic SaaS dashboards, neon/glow, gradients,
  glass, fantasy frames, currency art, graph minimaps, browser chrome, and
  invented crafting controls.
- Existing palette tokens, supplied verbatim:
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
- Flat 16:9 desktop mockup at approximately 1280×720, no browser frame or
  watermark.

## Structural directions for mockups

Generate three genuinely different hierarchies:

- **Variant A — Bottom analysis ledger:** preserve the current palette/board/
  inspector row. Replace the Simulator runner with a two-column exact summary
  and node-details ledger. This is the safest component-aligned direction.
- **Variant B — Calculator inspector:** let the board use the full height
  beside a wider right analysis rail containing success, costs, and selected
  node details; retain only a slim status/cost strip below the board.
- **Variant C — Answer band + tabbed dock:** place a restrained full-width
  exact-answer band between the toolbar and board. The lower dock uses tabs
  `Summary`, `Expected cost`, and `Node details`, with Node details active.

## Questions for Oliver's mock review

1. Edge labels: use conditional share on the board with absolute expected
   traversals on hover (recommended), or show absolute traversals directly?
2. Node badges: show v1 expected visits such as `79.70×` (recommended), or
   omit operation badges until Phase D can compute reach probability?
3. Evaluation feedback: automatic after structural edits with status only
   (recommended), or automatic plus a small manual `Evaluate again` action?
