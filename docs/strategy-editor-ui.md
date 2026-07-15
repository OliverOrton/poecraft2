# Strategy Editor UI

## Purpose

The project has two related user-facing workflows:

```text
Emulator:
  user performs crafting operations one by one

Strategy Builder:
  user builds a strategy, then uses Simulator or Calculator runner mode
```

The strategy runner is the Simulator; it is not a separate workspace document.
Calculator is the Strategy Builder's exact whole-graph evaluation mode. The old
project's workflow remains useful reference, but the current product shape is
the implemented four-document workspace.

Strategy Builder is one of the four core workspace areas alongside Emulator,
Calculator, and Stash.

The direction is:

```text
Visual feel:
  Unreal Blueprints-style node graph

Execution model:
  native strategy simulator
```

The graph is for authoring and debugging strategy flow. The simulator is the native engine subsystem that executes the compiled graph. Python and WebAssembly bindings call the same simulator; they do not implement separate graph interpreters. The prior app's simulator is a useful design reference, but new strategies do not need to be backward-compatible with old strategy files.

## Old Simulator Model

The old strategy system used:

```text
Step
  step_id
  name
  method
  conditions
  on_success
  on_failure
  max_attempts
```

Each step could:

1. Execute a crafting method.
2. Check conditions on the item.
3. Follow a success or failure outcome.
4. Repeat until success, failure routing, or max attempts.

`max_attempts` is documented here only as old behavior. The new graph does not carry per-node attempt limits; repetition is explicit graph flow and safety limits belong to the simulator run configuration.

The old simulator tracked:

```text
current step
step attempt count
currency used
step logs
final item
```

The new editor should preserve the useful routing model visually, but it does not need per-node attempt loops.

## New Strategy Model

Use a graph:

```text
StrategyGraph
  start node
  operation nodes
  terminal nodes
  guarded edges
```

State nodes are operation nodes. A state should generally do one thing:

```text
Chaos Orb
Essence of Horror
Bench craft suffixes cannot be changed
Harvest reforge fire
Annul
No-op / condition check
```

Edges decide where to go next.

```text
edge = condition + target state + priority
```

That means conditions live on edges, not inside the operation node. This keeps the graph readable:

```text
operation node:
  "use chaos"

outgoing edges:
  if target mods present -> success
  if item can be rescued -> repair state
  otherwise -> chaos again
```

## Node Types

### Start Node

Defines the base item start state:

```text
base type
item level
rarity
initial mods, if any
influence state
fractured/synthesised/split/corrupted flags
quality/sockets, if relevant
```

The start node should have no crafting operation. It creates the initial item state.

### Operation Node

Runs one crafting operation.

Fields:

```text
id
name
operation type
operation params
position
notes
```

Examples:

```text
operation: chaos
params: {}
```

```text
operation: essence
params:
  essence_id: essence_of_horror
```

```text
operation: fossil
params:
  fossils: [dense_fossil, pristine_fossil]
```

### Terminal Node

Ends a simulation branch.

Types:

```text
success
failure
stop
```

Terminal nodes should capture why the branch ended:

```text
target reached
budget exceeded
cannot continue
manual stop
```

Reaching a success terminal defines the strategy goal. There is no separate global end-condition object. Restart edges only direct execution back through the graph and never define the goal.

### No-op / Router Node

The old strategy system supported condition-only steps through a no-op method.

Keep this available, but treat it as a secondary tool:

```text
operation: condition_check_only
```

Most strategies should not need many router nodes because edge conditions can handle routing directly.

## Edge Types

Edges are guarded transitions.

Fields:

```text
id
from_node_id
to_node_id
priority
condition
label
is_default
```

Execution evaluates outgoing edges in priority order. The first matching edge wins.

Default edges are fallback routes:

```text
if no guarded edge matches:
  take default edge
```

Common default edges:

```text
repeat this node
go to failure
go to repair state
stop
```

A restart is represented as an ordinary transition back to the chosen restart state, optionally after applying a reset/scour operation. It is flow control, not a terminal result.

Only one default edge should be allowed per node.

## Execution Semantics

For an operation node:

```text
apply node.operation once
increment run-wide action/currency counters
evaluate outgoing edges in priority order
take first matching edge, otherwise the default edge
record trace entry
```

Repeating an operation is represented by a self-edge or another cycle in the graph.

The simulator run configuration, not individual nodes, owns safety limits:

```text
maximum total craft actions
optional maximum total cost
cancellation flag
optional wall-clock budget in interactive/browser jobs
```

Reaching a configured limit terminates the run with a non-success limit result. These guards prevent malformed or intentionally cyclic graphs from running forever without adding per-node attempt semantics.

The web simulator exposes the maximum actions per run, defaulting to 100,000
and bounded by the engine maximum. Aggregate results show total actions across
all completed runs. The result inspector defaults to action distribution,
aggregated by operation node across every run, with retained traces available
in a separate tab.

For a no-op/router node:

```text
do not mutate item
evaluate outgoing edges once
take first match or default
```

For terminal nodes:

```text
end simulation branch
record success/failure/stop reason
```

## Conditions

The native condition vocabulary currently used by authored and solver-compiled
strategies is:

```text
always
has mod group at minimum tier
has modifier family at minimum tier (optionally fractured)
item flag (corrupted, mirrored, split, synthesised, fractured, crafted,
           veiled/veiled side, metamods, influenced, eldritch implicit)
eldritch implicit tier range by Searing/Eater side
rarity is
open prefix/suffix count in range
prefix/suffix count in range
all / any / not / at least N of
```

`has modifier family at minimum tier` may also require that the matched modifier
is fractured. In the condition builder, right-clicking a modifier in the picker
adds it with that requirement; right-clicking an already selected modifier
turns the requirement on. Item flags and eldritch tier ranges are also ordinary
visual condition rows.

The S6 policy compiler additionally emits advanced conditions that are valid in
stored JSON but deliberately stay out of the ordinary leaf picker:

```text
mod_count          exact count across a compiler-supplied set of mod keys
influence_bits     exact generic influence bitset
has_unveil_option  whether a currently veiled slot offers one stable mod key
```

Still-planned additions include:

```text
has mod with tag
does not have mod group/id/tag
total affix count in range
has empty affix side
item is craftable
cost spent <= amount
total craft actions <= amount
```

Later condition types:

```text
stat total >= value
open/closed prefix/suffix combinations
influence dominance as a user-facing semantic predicate
crafted mod count
```

Condition expressions should support:

```text
all of
any of
not
at least N of group
```

Keep condition evaluation pure. A condition should inspect item/simulation state and return true or false. It should not mutate the item.

In the first visual editor, conditions listed on one edge are ANDed. OR is
authored as another outgoing edge. The native format retains nested `any`
support for imported or advanced JSON, but the ordinary builder does not expose
a separate OR-group control.

## Condition Editor UI

The condition editor should borrow from Scratch/Blockly only for the condition-building experience.

Do not make the whole strategy editor a block language.

Use rows/cards like:

```text
has modifier
  require [ALL | N OF]
  [maximum Life] [T1+]
  [fire resistance] [T2+]

open suffixes [>=] [1]
```

`Has modifier` remains one condition when several modifier families are
selected. It defaults to requiring all selected modifiers. Selecting `N OF`
reveals the required count. Tier thresholds default to T1.

```text
has modifier
  require [2] of [3]
  fire resistance
  cold resistance
  lightning resistance
```

The right inspector should let the user edit an edge's condition without opening a separate page.

The modifier-family picker uses the same prefix/suffix tabs, source labels,
classification tags, family text, and tier grouping as the Emulator modifier
pool rather than a dense single-line combobox.

Useful controls:

```text
condition type select
emulator-style searchable modifier-family list
operator select
number stepper
negate toggle
```

Conditions should render as short edge labels on the board:

```text
has +2 arrows
open prefix
brick -> restart
else
```

Long conditions can be summarized with a label and expanded in the inspector.

## Board UI

Use an Unreal Blueprints-inspired layout:

```text
left palette:
  operations
  terminal nodes
  saved condition snippets

center board:
  pan/zoom graph canvas
  operation nodes
  terminal nodes
  wires/edges
  selected path highlights

right inspector:
  selected node settings
  selected edge condition editor
  validation warnings

bottom panel:
  action distribution (default)
  retained run trace (optional tab)
  currency/stats
  item snapshots
  selected simulation details
```

Do not use a landing page for the strategy editor. A new strategy may show the
base picker first; confirming it opens the working board directly.

The Strategy Builder itself is a tab inside the desktop workspace described in
[desktop-workspace-ui.md](desktop-workspace-ui.md). It can be split beside
Emulator, Calculator, or Stash. Its Simulator/Calculator runner, inspector, and
condition editor remain part of the Strategy Builder area rather than becoming
unrelated top-level applications.

```text
strategy editor on the left
simulator on the right
```

## Node Visual Design

Nodes should be compact and information-dense.

Operation node contents:

```text
operation name
important params
current validation status
optional run stats from last simulation
```

Example:

```text
Chaos Orb
```

```text
Essence
Horror
```

Terminal nodes should be visually distinct:

```text
Success
Failure
Stop
```

Use color carefully:

- operation nodes: neutral
- success terminal: green accent
- failure terminal: red accent
- warning/invalid state: amber accent
- selected execution path: bright outline

Avoid making every node a different color. The graph should be readable under heavy use.

## Interaction Rules

Core interactions:

```text
drag node
pan board
zoom board
click node to inspect
click edge to inspect
drag from output handle to create edge
delete selected node/edge
duplicate selected node
auto-layout selected region
```

Creating a new strategy first uses the shared Emulator-style base picker, then
opens a blank board. No sample graph or automatic node layout is inserted.

Execution/debug interactions:

```text
run once
run N simulations
step through one trace
highlight active node
highlight taken edges
show item snapshot at trace point
show why an edge matched or failed
```

### Aggregate Result Overlay

After a multi-run simulation, the board replaces the plain visited-node
marking with an aggregate overlay driven by the run's statistics:

```text
node visit counts        color intensity or badge per node
edge take rates          edge thickness/label per edge
expected cost share      per-node share of total expected spend
failure locations        where non-success runs ended
```

Stepping a single retained trace still uses the active-node/taken-edge
highlight; the overlay is the multi-run aggregate view. Toggling between
overlay and plain board is one control in the bottom panel.

Validation interactions:

```text
warn when a node has no outgoing edge
warn when multiple default edges exist
warn when a node cannot reach a terminal
warn when an edge condition references unavailable data
warn when a graph has no reachable success or non-success terminal
```

## Recombinator Blocks And Item Flow

**Status: parked future mechanic work (M4-M5).** This section is retained as
architecture only; do not implement it during active S7 solver work.

The editor model above is control flow: one implicit item walks guarded
edges. Recombinators add item flow: items as values that merge at blocks
(see [solver-mechanic-extensions.md](solver-mechanic-extensions.md) for
the underlying spec-pyramid model). The two flavors must stay visually
and semantically distinct so existing strategies are unaffected:

```text
guard edge   routing decision (existing, unchanged)
item wire    carries an item between blocks; exists only around
             recomb and feeder blocks; distinct color/weight
```

Recomb and feeder blocks live in the same `StrategyDocument` schema as
every other node — new node kinds plus a new wire kind, not a separate
pyramid document type. Mixed strategies stay natural (craft a base
conventionally, then feed it into a recomb tier in the same graph), and
persistence, validation, and compilation extend rather than fork.

### Recomb Block

```text
inputs:  two item ports (A, B), each gated by a spec condition
output:  one item port; outgoing guard edges route by result conditions
```

When both inputs are wired, the block shows a live badge computed by the
exact recomb enumerator: success chance, expected attempts, expected cost
including feeder costs. Hovering expands to the full outcome
distribution — the Calculator's two-item view rendered in place. Item
wires show the spec distribution flowing through them on hover. Users see
the odds change as they edit feeders, before running anything.

### Feeder Block

A feeder block references another strategy document. It displays the
referenced strategy's name, its cached summary (average cost and output
spec distribution), and a staleness indicator when the referenced
strategy has changed since the summary was computed, with one-click
recompute. It is never re-executed during the parent's simulation; the
summary is the contribution.

Fluent creation paths:

```text
double-click feeder block  -> open referenced strategy in a new
                              workspace tab (splits: goal strategy
                              left, feeder strategy right)
drag strategy onto canvas  -> create feeder block referencing it
drag Stash item onto port  -> fixed-price bought feeder instead of a
                              crafted one
```

### Recycling Wires

Dragging a recomb output back to another block's input port creates a
condition-gated return wire. The guard is pre-filled by computing which
failure outcomes of the source block satisfy the target port's spec
condition (the enumerator answers this statically); the user edits from
there. Return wires render as distinct curved back-edges so pyramids
stay readable.

### Palette Template

One palette template, "recomb pair", drops the standard motif pre-wired:
two feeder blocks feeding a recomb block, a success guard edge upward,
and a salvage return wire downward. Most pyramids are this motif
stacked, so authoring becomes drag, drop, retarget.

### Solver Round-Trip

"Plan pyramid" (the auto-planner) emits ordinary recomb/feeder blocks
onto the canvas — fully editable, nothing opaque. After hand-editing,
"re-cost" reruns the spec-level fixed point over the user's structure
and refreshes every badge without replanning the structure. The
plan-tweak-re-cost loop is the intended workflow, not a fallback.

### Run Trace: Item Lineage

For runs containing recomb blocks, the run trace gains a lineage view: a
family tree of items per attempt showing which feeders were produced,
consumed, or recycled where, plus an expected-cost rollup by pyramid
tier (e.g. "62% of expected cost is tier-2 recombs") so users can see
exactly where to optimize.

### Item-Flow Validation

Additional checks in the existing validation-issue system:

```text
warn when recomb inputs have mismatched item classes
warn when a return-wire condition is unsatisfiable by the source
  block's outcome set (checked statically via the enumerator)
warn when a feeder summary is stale
warn when an item wire's source may not produce an item on all paths
```

## Web Implementation Approach

Keep the earlier frontend stack decision:

```text
Vite + TypeScript + native Web Components
```

Implement the board with custom elements and SVG:

```text
<pc-strategy-editor>
<pc-strategy-board>
<pc-strategy-node>
<pc-edge-layer>
<pc-condition-editor>
<pc-run-trace>
```

Recommended rendering:

```text
HTML elements for nodes
SVG layer for wires/edges
absolute positioning for the board
CSS transforms for pan/zoom
```

This keeps node contents easy to build with normal DOM/CSS while preserving smooth wire rendering.

Do not start with React Flow because the project is intentionally avoiding React.

Possible libraries to revisit later:

```text
Rete.js:
  if custom graph editing becomes too expensive

jsPlumb:
  if edge routing/dragging becomes the hard part

Sequential Workflow Designer:
  probably too linear for this graph, but useful inspiration
```

Default recommendation: build the first board custom. The graph model is domain-specific enough that a generic node editor may create more friction than it removes.

## Saving And Stash

Strategy content saves manually.

```text
Save:
  write to the current owned strategy resource

Save As:
  create a new strategy resource

Import/Fork:
  create a new unsaved copy
```

Closing a dirty Strategy Builder tab prompts Save, Discard, or Cancel.

Stash strategy cards show:

```text
title
description
compact success-route summary
publication state
success/cost statistics when available
author and fork attribution
```

Complex success routes should not be rendered as a fabricated exact goal item.

## Publishing

Publishing freezes an immutable strategy version and runs 100,000 browser simulations.

Store the resulting:

```text
success rate
average/median/percentile cost when complete
cost status and missing price keys
average craft actions per run
sample count
engine/data versions
economy snapshot
```

Reaching a success terminal determines success. Restart transitions affect the run but do not count as the goal.

Publishing is unavailable until the full public engine mechanic, validation, packaging, and performance/readiness gates pass.

See [accounts-publishing-and-discovery.md](accounts-publishing-and-discovery.md).

## Strategy JSON Shape

The saved strategy should separate graph authoring data from execution semantics.

Persistent strategy JSON uses stable global base/mod keys. A base key is the RePoE metadata path, not a display name, slug, or runtime integer ID. Session-local dense IDs exist only after the native simulator compiles the strategy for a specific data/session version.

```json
{
  "version": "v1",
  "name": "Example Bow Strategy",
  "description": "",
  "start_node_id": "start",
  "base_state": {
    "base_key": "Metadata/Items/Weapons/TwoHandWeapons/Bows/SpineBow",
    "item_level": 86,
    "rarity": "rare"
  },
  "nodes": [
    {
      "id": "start",
      "kind": "start",
      "position": { "x": 0, "y": 0 }
    },
    {
      "id": "chaos_1",
      "kind": "operation",
      "name": "Chaos until arrows",
      "operation": {
        "type": "chaos",
        "params": {}
      },
      "position": { "x": 320, "y": 0 }
    },
    {
      "id": "success",
      "kind": "terminal",
      "terminal": "success",
      "position": { "x": 680, "y": -120 }
    }
  ],
  "edges": [
    {
      "id": "start_to_chaos",
      "from": "start",
      "to": "chaos_1",
      "priority": 0,
      "condition": { "type": "always" },
      "label": "start"
    },
    {
      "id": "chaos_success",
      "from": "chaos_1",
      "to": "success",
      "priority": 0,
      "condition": {
        "type": "has_mod_group",
        "group": "AdditionalArrows"
      },
      "label": "has arrows"
    },
    {
      "id": "chaos_repeat",
      "from": "chaos_1",
      "to": "chaos_1",
      "priority": 999,
      "condition": { "type": "always" },
      "label": "else",
      "is_default": true
    }
  ]
}
```

The UI can store extra layout metadata, but the simulator should only need:

```text
base state
nodes
edges
conditions
operation params
```

Run-wide limits come from the simulation options, not the saved graph.

The Phase 10 compiler also accepts nested condition expressions:

```json
{
  "type": "all",
  "conditions": [
    { "type": "rarity_is", "rarity": "rare" },
    { "type": "open_suffix_count", "min": 1 }
  ]
}
```

`all`, `any`, and `not` compose conditions directly. `at_least` adds a
`count` field. Count ranges are inclusive.

## Economy Snapshot JSON

The native simulator consumes an optional immutable snapshot:

```json
{
  "version": "v1",
  "id": "settlers-example",
  "prices": {
    "chaos": 1.0,
    "essence:Metadata/Items/Currency/Essence/SomeEssence": 8.5,
    "fossil:Metadata/Items/Currency/CurrencyDelveCraftingSomeFossil": 3.0,
    "resonator:1": 0.5
  }
}
```

Basic operations use the operation name. Essence costs use the stable essence
metadata key. Fossil costs are the sum of each selected fossil key and the
matching `resonator:<socket-count>` key. Missing keys remain explicit and make
cost output incomplete; a configured cost limit cannot continue through a
missing required price.

## Compilation To Simulator

The graph should compile into a simulator-friendly representation:

```text
node_id -> operation
node_id -> ordered outgoing edges
edge -> compiled condition
terminal node ids
```

The compiled simulator should not care how the graph looked on the board.

The simulator implementation lives in the native engine. This lets Python, WebAssembly, and later ML tools run strategies without the web UI while preserving one execution implementation.

## Relationship To Emulator

The emulator and strategy simulator should share:

```text
engine actions
item state rendering
mod display helpers
debug pool views
currency/cost model
```

The simulator supports two item-detail modes:

```text
structural:
  skip numeric stat rolls for maximum throughput
  conditions that require rolled numeric values are unavailable

full_rolls:
  roll and retain numeric stat values
  required for stat-total conditions and representative result items
```

Structural mode is the initial default. Full-roll simulation can be added as an explicit toggle without changing graph structure.

But they have different interaction models:

```text
Emulator:
  user clicks one craft operation at a time
  immediate item mutation
  history/undo matters

Strategy editor:
  user edits graph
  simulator runs graph repeatedly
  trace/stat inspection matters
```

Cross-area imports:

```text
Emulator -> Strategy Builder:
  create a new unsaved strategy
  current Emulator item becomes the start state

Strategy Builder -> Emulator:
  create a new unsaved item from a selected concrete successful result
```

For strategies with multiple success routes, do not synthesize a fake item. If no successful result has been selected, the user must run the strategy or choose a saved representative example before importing the goal into Emulator.

The Strategy Builder should also be able to open any item snapshot from a trace in Emulator as a new unsaved item.

## First UI Slice (Completed Historical Baseline)

First useful strategy editor slice:

1. Start node and one operation node.
2. Terminal success/failure nodes.
3. Drag nodes on board.
4. Draw edges between nodes.
5. Edge inspector with simple conditions:
   - always
   - has mod group
   - open prefix/suffix
   - rarity
6. Manually save/load strategies through Stash.
7. Run one strategy once and show trace.
8. Run N simulations and show aggregate success/cost.

Do not build a full visual scripting language first. Prove that the graph can express common crafting strategies, then expand condition types and editor comfort.

## Invariants

- Strategy graph authoring is a UI concern; execution must be testable without the UI.
- Operation nodes mutate item state; edge conditions inspect item state.
- Reaching a success terminal defines success; restart transitions only control flow.
- Conditions are pure and deterministic.
- Outgoing edges are evaluated in priority order.
- A node may have at most one default edge.
- Simulator traces should record node id, operation, cumulative craft-action count, matched edge, item snapshot, and currency used.
- Per-node attempt limits are not part of the graph; run-wide simulation options provide safety limits.
- Strategy content is manually saved; Import/Fork creates a new unsaved copy.
- The UI should feel Blueprint-like, but the logic should stay strategy-oriented rather than becoming arbitrary visual code.
