# Strategy Editor UI

## Purpose

The project has two related user-facing workflows:

```text
Emulator:
  user performs crafting operations one by one

Simulator:
  user loads or builds a strategy, then runs many attempts
```

The old project already had this split. The new project should keep it, but the strategy editor should become a visual graph editor instead of a mostly linear list of steps.

The direction is:

```text
Visual feel:
  Unreal Blueprints-style node graph

Execution model:
  deterministic strategy graph runner
```

The graph is for authoring and debugging strategy flow. The simulator should still execute a deterministic strategy model that can be tested without the UI. The prior app's simulator is a useful design reference, but new strategies do not need to be backward-compatible with old strategy files.

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

The simulator tracked:

```text
current step
attempt count
currency used
step logs
final item
```

The new editor should not throw this away. It should expose the same control flow visually.

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
max attempts
position
notes
```

Examples:

```text
operation: chaos
params: {}
max_attempts: 1000
```

```text
operation: essence
params:
  essence_id: essence_of_horror
max_attempts: 500
```

```text
operation: fossil
params:
  fossils: [dense_fossil, pristine_fossil]
max_attempts: 300
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

Only one default edge should be allowed per node.

## Execution Semantics

For an operation node:

```text
attempts = 0

while attempts < node.max_attempts:
  apply node.operation
  attempts += 1
  evaluate outgoing edges in priority order

  if matching edge exists:
    move to edge.target
    record trace entry
    return

  if default edge exists:
    if default target is this node:
      continue
    move to default target
    record trace entry
    return

  continue

fail max attempts
```

This preserves the useful control-loop shape where a state can repeat until conditions pass or max attempts are reached.

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

The first condition system should preserve the old condition vocabulary:

```text
has mod group
has exact mod id
has metamod
rarity is
open prefix count >= N
open suffix count >= N
has influence
has fractured mod
prefix count in range
suffix count in range
```

Add these soon after:

```text
has mod with tag
does not have mod group/id/tag
total affix count in range
has empty affix side
item is craftable
item is corrupted/mirrored/split/synthesised
cost spent <= amount
attempts at node <= amount
```

Later condition types:

```text
stat total >= value
explicit mod tier at least N
open/closed prefix/suffix combinations
influence dominance
eldritch implicit tier
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

## Condition Editor UI

The condition editor should borrow from Scratch/Blockly only for the condition-building experience.

Do not make the whole strategy editor a block language.

Use rows/chips like:

```text
ALL of
  has mod group [IncreasedLife]
  open suffixes [>=] [1]
  NOT has tag [caster]
```

Or:

```text
AT LEAST [2] of
  has fire resistance
  has cold resistance
  has lightning resistance
```

The right inspector should let the user edit an edge's condition without opening a separate page.

Useful controls:

```text
condition type combobox
mod/group/tag searchable combobox
operator select
number stepper
AND/OR segmented control
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
  run trace
  currency/stats
  item snapshots
  selected attempt details
```

Do not use a landing page for the strategy editor. The first screen should be the working board.

## Node Visual Design

Nodes should be compact and information-dense.

Operation node contents:

```text
operation name
important params
max attempts
current validation status
optional run stats from last simulation
```

Example:

```text
Chaos Orb
max 1000
```

```text
Essence
Horror
max 500
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

Validation interactions:

```text
warn when a node has no outgoing edge
warn when multiple default edges exist
warn when a node cannot reach a terminal
warn when an edge condition references unavailable data
warn when max attempts is missing or too high
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

## Strategy JSON Shape

The saved strategy should separate graph authoring data from execution semantics.

```json
{
  "version": "v1",
  "name": "Example Bow Strategy",
  "description": "",
  "start_node_id": "start",
  "base_state": {
    "base_id": "spine_bow",
    "item_level": 86
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
      "max_attempts": 1000,
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
        "group": "AdditionalArrow"
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
max attempts
```

## Compilation To Runner

The graph should compile into a runner-friendly representation:

```text
node_id -> operation
node_id -> ordered outgoing edges
edge -> compiled condition
terminal node ids
```

The compiled runner should not care how the graph looked on the board.

This also lets Python/native/ML tools run strategies without the web UI.

## Relationship To Emulator

The emulator and strategy simulator should share:

```text
engine actions
item state rendering
mod display helpers
debug pool views
currency/cost model
```

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

The strategy editor should be able to open an item snapshot from a trace in emulator mode. That will make debugging strategies much easier.

## First UI Slice

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
6. Save/load strategy JSON.
7. Run one strategy once and show trace.
8. Run N simulations and show aggregate success/cost.

Do not build a full visual scripting language first. Prove that the graph can express common crafting strategies, then expand condition types and editor comfort.

## Invariants

- Strategy graph authoring is a UI concern; execution must be testable without the UI.
- Operation nodes mutate item state; edge conditions inspect item state.
- Conditions are pure and deterministic.
- Outgoing edges are evaluated in priority order.
- A node may have at most one default edge.
- Simulator traces should record node id, operation, attempts, matched edge, item snapshot, and currency used.
- The UI should feel Blueprint-like, but the logic should stay strategy-oriented rather than becoming arbitrary visual code.
