# Strategies

**Status: stable implemented strategy-model and product reference.**

Parent: [Product](README.md)

Verified against code: 2026-08-22 through recovery-scoped Restart at
`1e21260` / release WASM `cfd8904`.
Scope: web strategy document, authoring/validation, native
compile/run/evaluate semantics, board degradation, and current runner
presentation. No rendered or visual review was performed.

## Strategy Document

A saved v1 `StrategyDocument` contains:

- name and description;
- `start_node_id`;
- a stable-key `base_state`;
- start, operation, router, and terminal nodes;
- prioritized guarded edges;
- optional economy identity and UI viewport metadata; and
- optional non-executable `solver_policy_scope` provenance: `unrestricted`,
  `zero_progress_reroll_policy_restriction`,
  `no_economic_restart_policy_restriction`, or
  `zero_progress_reroll_and_no_economic_restart_restrictions`.

`base_state` can preserve base key, item level, rarity, quality, item flags,
generic influence, both Eldritch tiers, and prefix/suffix modifier keys with
crafted/fractured flags. Runtime integer ids are session-local and are never
the saved identity.

Authored legacy documents may omit `solver_policy_scope`. The web model,
clone/persistence path, and comparisons preserve it when present, but native
graph execution never reads it as a condition or crafting rule.

Terminal kinds are `success`, `failure`, and `stop`. Reaching a success
terminal defines success. Restart is an operation/control-flow choice and does
not define a goal. Safety limits belong to each Simulator invocation rather
than individual graph nodes.

Code authority:
`apps/web/src/app/strategy-model.ts`,
`engine/src/simulator.cpp`, and
`engine/include/poecraft/simulator.h`.

## Execution Semantics

The native strategy compiler resolves stable base/mod/action keys for one
session and compiles every condition. During a run:

1. A start or router node evaluates its outgoing edges without applying an
   action.
2. An operation node applies its native action once, records action/material
   accounting, then evaluates outgoing edges.
3. Non-default edges are tested in stable priority/source order; the first
   match wins. One default edge may provide fallback.
4. A terminal ends the run and records its kind/reason.
5. Missing routes, refused actions, cancellation, or configured action/cost/
   graph-step limits remain explicit non-success outcomes.

The web app and Python/WASM bindings call this native implementation; there is
no TypeScript graph interpreter.

## Conditions

The native stored vocabulary at d5e38e3 includes:

- `always`;
- modifier group/family presence with minimum tier and optional
  crafted/fractured requirement;
- exact modifier or family counts over compiler-supplied key sets;
- item flags including corruption, mirror, split, synthesis, crafted,
  fractured, veiled side, metamods, influence, and Eldritch presence;
- exact generic influence bits and Searing/Eater tier ranges;
- current Unveil-offer membership;
- engine-authored versioned `observation_signature` programs for exact policy
  routing;
- rarity;
- open prefix/suffix and occupied prefix/suffix count ranges; and
- nested `all`, `any`, `not`, and `at_least` expressions.

The visual condition editor exposes family, item flag, Eldritch tier, rarity,
open/occupied side counts, `always`, and nested ALL/ANY/AT LEAST/NOT groups.
Advanced compiler conditions (`mod_count`, `mod_family_count`, exact influence
bits, Unveil offers, and `observation_signature`) remain valid stored JSON
without appearing as ordinary leaf choices. The web model preserves an
`observation_signature` payload opaquely; native compilation and evaluation
remain its shape and semantic authority.

Solver-generated fixed-program Unveil routers bind an offer test to the exact
pre-Unveil observation carrier that produced its choice group. The same
modifier offered from another carrier does not match that branch. This
observation identity is engine-authored inside the routing program and is not
editable crafting logic in the visual condition editor.

This corrects the historical statement that the ordinary editor supports only
AND rows and requires separate edges for OR: the current editor has a nested
condition-tree model.

Condition parsing/evaluation authority:
`engine/src/simulator.cpp`. Web authoring authority:
`apps/web/src/app/strategy-model.ts` and
`apps/web/src/app/components/pc-condition-editor.tsx`.

## Strategy Builder

The React continuity pass keeps the existing board workflow and adds explicit
Restart, Transmutation and previously missing supported mechanic palette
entries. Namespaced actions such as `bestiary:imprint` retain their complete
type when placed. Palette entries support both drag/drop and double-click.

Nested conditions show group boundaries, ALL/ANY/N OF logic and connecting
AND/OR labels. Modifier rows show the selected tier's text and “Tn or better”;
Any tier remains a distinct zero threshold. The editor preserves advanced
native JSON conditions. Runner-mode updates are scoped to their toolbar and
cannot overwrite nested condition controls.

Strategy Builder provides:

- a draggable operation/terminal palette;
- an HTML/SVG pan/zoom board with edge creation and selection;
- node and edge inspectors;
- nested condition editing;
- graph validation, auto-layout, and fit-view;
- manual Save/Save As/Duplicate; and
- Simulator and Calculator runner modes over the same graph.

Undo/Redo covers graph edits, conditions, labels, base changes and layout.
A node drag is one edit; panning and zooming do not consume undo steps and
the current viewport is preserved when restoring an edit. Continuous label
typing is grouped until focus leaves the field. History survives draft reload,
and a new edit after Undo discards the redo branch (see the shared
[history limits](workspace.md#emulator-state)).

Select an edge to reveal handles beside its source and destination ports.
Drag either handle onto another node to reconnect it. The existing edge ID,
condition, priority, default flag and label are retained. Escape or releasing
on empty space cancels the gesture. Reconnection is a single undoable edit.

Validation checks ids, one start, operation/terminal fields, edge endpoints,
default-edge uniqueness, condition shape, and reachability to terminal and
success outcomes. Validation is product feedback; native compilation remains
the execution authority.

Large graphs degrade deliberately. Above 220 nodes or 320 edges the board uses
simplified routing/presentation. Above 1,200 nodes or 2,400 edges it shows a
summary until the user explicitly requests rendering. Validation rows are
capped for display while the underlying document remains intact.

Code authority:
`pc-strategy-editor.tsx`, `pc-strategy-board.ts`, `pc-edge-layer.ts`,
`strategy-layout.ts`, and `strategy-model.ts`.

## Simulator Mode

Simulator mode compiles the current graph and runs the native batch simulator
in the worker. Current controls include run once/run N, maximum actions per
run, progress, and cancellation. Current results show:

- completed/success counts and success rate;
- total and average actions plus throughput;
- average known cost and complete/incomplete cost status;
- pinned economy identity;
- sampled per-action and per-material averages;
- retained traces/examples; and
- failure summaries.

The current product does not render median/percentile costs, a cost histogram,
budget probability, or a full exported shopping list. It also aggregates
operation-node action counts rather than every node visit and edge traversal.
Those are open/deferred product items, not stable Simulator promises.

Code authority:
`apps/web/src/app/components/pc-simulator.tsx`, `pc-run-trace.ts`,
`engine-worker.ts`, and the native simulator ABI.

## Exact Calculator Mode

Calculator mode asks the native evaluator to solve the compiled graph as an
absorbing process over `(node, abstract item state)`. It begins immediately on
mode entry and is debounced after structural changes. Node movement, viewport,
display labels, name, and description do not invalidate the exact result.
Structural changes keep the previous result visible but stale until replacement.

The product renders native terminal/failure/unresolved probability, expected
actions and materials, node visits, edge traversals, incoming state classes,
and accounting/review projections. Price edits update displayed cost rows from
the existing quantities and shared economy; they do not define routing.

Unsupported graph vocabulary is refused with the native gap message. Exact
evaluation represents compiler-emitted `mod_count`, `mod_family_count`, and
versioned `observation_signature` routing, including crafted/fractured
requirements and the engine-owned action
observation/preservation/destruction contract. Authored
concrete Unveil-offer selection remains the explicit gap; solver-compiled
ordinary decision DAGs are evaluable.

Code authority:
`engine/src/solver_eval.cpp`,
`apps/web/src/app/components/pc-strategy-odds.ts`,
`strategy-eval-presentation.ts`, and
`pc-strategy-editor.tsx`.

## Persistence And Economy

Strategy content is manually saved to the local Stash. Draft content and the
selected Simulator/Calculator builder mode are persisted separately for
reload recovery. Imported or duplicated strategies are unsaved copies.

Each Simulator run and exact evaluation receives an immutable economy pin.
Changing the workspace economy affects new work; old results retain their
pinned identity. Price arithmetic does not move into graph conditions or
change action legality.

See [Workspace](workspace.md) and [Economy](../economy/README.md).

## Deferred Model Extensions

The feeder extension retains operation nodes and existing control-flow edges,
with explicit resource slot assignments shown as item ports. It has no general
concurrent scheduler or publishing/account resource contract. Executable
recombination stays held pending resource-slot integration of the qualified native pair Apply.
Aggregate Simulator node/edge overlays, empirical focus/trim and publishing
remain deferred in [Product Notes](NOTES.md), the
[solver roadmap](../future/solver-roadmap.md), and other `future/` references.

Historical UI plans and approved visual evidence are in the
[product-design archive](../archive/2026-07-product-design/README.md).

## Explicit resources

An authored graph may contain up to seven resource templates with stable IDs,
base/item-level state and an `acquisition_price_key` (default `resource:<id>`).
Templates are initially unavailable. `acquire_resource` makes one live copy and
charges acquisition once; `awakener` uses explicit donor/receiver roles and
charges its own currency separately. A consumed donor remains absent across
Restart and recipient Imprint restore. Repeated use requires another acquisition;
an absent quote marks cost incomplete, and a cost-capped run stops before that
operation. Unknown cost is never reported as a zero total. Traces include resource
acquisitions, lifecycle and strands. Exact whole-graph evaluation refuses resource
graphs because inventory/control identity is reserved for Pro.

Foulborn operations have exact authored evaluation. Dominance and Vaal are native
sampled operations with authored Simulator support and currency accounting.
Vaal models affixes/implicits on ordinary equipment, retaining the 25% socket-only
branch while ignoring socket changes as Oliver approved. Exact Dominance and
corruption graph evaluation remain unavailable. Memory actions and the other
evidence-held currencies refuse compilation with their missing-law reason.

## Feeder extension source contract (2026-10-03)

**Source implemented; native, web and matching WASM qualification are held.**
The [living feeder record](../active/2026-10-03-strategy-feeder/README.md) owns the
exact source checkpoint and pending acceptance. The preceding qualified WASM
does not implement this vocabulary. Current and Finder producers are unchanged;
this is authored Simulator/Builder work and extends no exact solver authority.

An optional `output_contracts` array names up to 32 contracts, each with `id`,
`base_key` and a native condition `predicate`. A child must reach its success
terminal, return a live item, and satisfy both the actual session base identity
and this predicate. A success label alone is insufficient.

A resource may contain `feeder: {strategy_id, revision, document_json,
output_contract_id}`. The exact embedded saved revision is immutable during a
run; changing a Stash record cannot change it. New saves receive distinct revision
labels; older saved records pin their existing timestamp label and exact JSON.
Native compilation rejects missing contracts, conflicting documents under one
reference/revision, cycles over `(strategy_id, revision)` and nesting beyond 16 invocations.
Referencing an immutable older revision of the same saved strategy is permitted.
Seven resource slots remain the native capacity. Configured cluster feeder
templates remain outside this ordinary resource-session scope.

`invoke_feeder` specifies `params.resource_id`. It requires an empty output slot,
charges its `acquisition_price_key` for the pinned child's starting item, and
executes that child natively with the parent's economy and remaining absolute
action, graph-step and cost budgets. Each invocation is independently seeded
from the parent's native RNG. Acquisition counts as one resource action and
child crafting actions also debit the parent. Unknown quotes remain incomplete;
a cost cap refuses before applying an operation with an unknown price.

The actual returned item/session goes into the slot, including a failed or
mismatched output. Failure/stop, limits, missing routes, refused child actions
and output mismatch terminate the parent explicitly. The v1 runner does not
silently retry a failed child. Costs already incurred remain in the parent;
sampled materials and action counts include nested invocations. Retained traces
include stable-key full resource items and child receipts (reference/revision,
terminal/failure, acquisition and child costs, completeness and actual output
acceptance). Trace retention remains bounded; aggregate failure/censoring and
accounting retain all runs.

`move_resource` specifies distinct `params.from`/`params.to` slots, including
`current`. Source must be live and destination absent. The move copies the actual
item/session and identity, then marks its source consumed. It never acquires a
replacement. Mixed-base slot-to-slot moves preserve their session; a move into
`current` requires the existing compiled base, item level, cluster identity, data
and session modifier mapping.
An incompatible move refuses. `discard_resource` explicitly destroys a live
item without resale credit. Restart remains a separate paid fresh-base action.
Moving/discarding current invalidates its Imprint checkpoint; consumed resources
cannot be resurrected by Restart or restore. Pair inputs require distinct live
identities.

The reversible v1 default is a fresh independently paid child invocation plus
explicit recycling; this is a proposed design, not a historical user preference.
The Builder pins a Stash child and selected output contract from Start, authors
contracts with the existing condition editor, and shows explicit item ports on
the board. Save/import, clone and draft Undo/Redo preserve the complete reference.
No crafting interpreter or probability law is added to TypeScript.

`recombination` nodes can store `input_a`, `input_b` and `output` slot assignments,
but Builder compilation deliberately refuses them pending resource-slot integration
of the pair owner's qualified full Apply. The native pair contract consumes both inputs and creates one
new item with the explicit native A/B `output_session`; no receiver-index
reinterpretation is permitted. Mixed-base outputs require conditions over their
actual session. All inventory/feeder/recycling graphs remain refused by exact
Calculator evaluation pending its complete inventory kernel and properness
contract. There is no profit objective, resale pricing, salvage assumption or
dedicated profit/recombination solver in this source checkpoint.
