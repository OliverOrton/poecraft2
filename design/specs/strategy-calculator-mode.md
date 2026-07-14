# Strategy Builder Calculator Mode — Approved Phase C Spec

Approved by Oliver on 2026-07-14 after review of the three image-model
directions in `design/mockups/strategy-calculator-mode/`.

## Selected structure

Use a hybrid of Variants A and B:

- preserve the existing left palette, central strategy board, and right
  graph inspector;
- the right inspector remains the authoring surface for the selected actual
  node or edge in both modes;
- replace the existing bottom Simulator/trace region when Calculator mode is
  active with a bottom calculator inspector;
- carry Variant B's compact exact-result treatment and node annotation style
  onto the board;
- lay the bottom calculator inspector out as summary, expected consumption,
  and selected-node incoming-state details, with independent scrolling where
  necessary.

The implementation must not move graph editing controls into the calculator
inspector. Selecting a node continues to populate the right authoring
inspector and additionally drives the selected-node analysis in the bottom
calculator inspector.

## Approved quantities and interactions

- Edge annotations show conditional share of the source node's evaluated
  outflow. Hover text shows the absolute expected traversal count.
- Operation-node badges show expected visits (`79.70×`). Terminal badges show
  their absorb probability (`100%`). These are distinct quantities and must
  be labelled in hover text.
- Evaluation starts automatically on entering Calculator mode and roughly
  300 ms after a structural graph change. Status feedback is sufficient; no
  manual Evaluate button is required.
- Positions, viewport, node/edge display labels, and strategy name or
  description do not make the evaluation stale or trigger another call.
- Structural edits keep the previous result visible but dimmed with a
  `Stale · graph changed` marker until the replacement result arrives.
- Price edits recompute displayed subtotals and total only. They never invoke
  the strategy evaluator.

## Bottom calculator inspector

The left summary column contains:

- exact success as the strongest value;
- failure and stop probabilities;
- action-not-applied, no-matching-edge, and unresolved miss signals, with
  node attribution when nonzero;
- expected actions;
- converged/unresolved status, sweep count, and residual mass.

The middle expected-consumption column contains every engine-reported price
key and expected quantity, a chaos-equivalent price input, row subtotal, and
the total expected cost per strategy run. Missing prices remain explicit and
the known subtotal remains visible.

The right selected-node column contains the selected operation node's expected
visits, target legend, top incoming abstract-state classes, and truncated
remainder. The table shows share, rarity, prefix/suffix counts, one status
column per target, flags, and blocked mask. With no operation selected it asks
the user to select one; router/start/terminal selections do not fabricate an
operation distribution.

## State treatment

- **Evaluating:** preserve any previous result and show
  `Evaluating exact graph…`.
- **Invalid:** do not call the engine; show
  `Complete the graph to evaluate exact odds.` while existing validation stays
  in the right inspector.
- **Refusal:** frame the state with
  `This strategy uses actions or conditions Calculator mode cannot evaluate
  exactly yet.` and render the engine error verbatim in a preformatted block.
- **Unresolved:** retain absorbed results, emphasize the unresolved
  probability, and list its per-node mass.
- **Missing prices:** render `Set price`/missing state for each affected row
  and `Incomplete · <known subtotal> known` for the total.
- **Dense graphs:** badges stay compact; edge annotations use subdued backing
  and selected graph elements retain priority. The bottom class table scrolls
  instead of reducing type size.

## Visual rules

Reuse the current Strategy Builder and Calculator tokens, typography, compact
uppercase section labels, one-pixel dividers, and technical table treatment.
The bottom panel should read like the Calculator inspector in Variant B turned
sideways into the existing runner boundary, not like a dashboard of cards.

## Reference hierarchy

- Current structure:
  `design/refs/strategy-builder-current-phase-c.png`
- Calculator visual language:
  `design/refs/calculator-current-phase-c.png`
- Selected node annotation treatment:
  `design/mockups/strategy-calculator-mode/variant-b-calculator-inspector.png`
- Selected bottom placement:
  `design/mockups/strategy-calculator-mode/variant-a-bottom-analysis-ledger.png`

The generated mockups contain illustrative text. Engine results, the authored
graph, and this spec are the implementation authority.

## Preview notes

The 1280×720 implementation preview is captured in
`design/refs/strategy-calculator-phase-c-preview.png`. It follows the approved
hybrid hierarchy directly: the right operation inspector is unchanged, the
bottom region uses three simultaneous columns instead of tabs, and node badges
sit in the existing node header rather than adding a second node band. Edge
annotations retain the authored condition label above the conditional share.
These are deliberate adaptations to the existing component geometry, not
content changes. The preview used a real exact Transmute evaluation and showed
the expected `1.00×` operation badge, `100%` terminal badge, `100%` edge shares,
one expected Transmute, and the missing-price state. The only console error was
the pre-existing favicon 404.
