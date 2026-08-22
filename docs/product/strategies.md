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
`apps/web/src/app/components/pc-condition-editor.ts`.

## Strategy Builder

Strategy Builder provides:

- a draggable operation/terminal palette;
- an HTML/SVG pan/zoom board with edge creation and selection;
- node and edge inspectors;
- nested condition editing;
- graph validation, auto-layout, and fit-view;
- manual Save/Save As/Duplicate; and
- Simulator and Calculator runner modes over the same graph.

Validation checks ids, one start, operation/terminal fields, edge endpoints,
default-edge uniqueness, condition shape, and reachability to terminal and
success outcomes. Validation is product feedback; native compilation remains
the execution authority.

Large graphs degrade deliberately. Above 220 nodes or 320 edges the board uses
simplified routing/presentation. Above 1,200 nodes or 2,400 edges it shows a
summary until the user explicitly requests rendering. Validation rows are
capped for display while the underlying document remains intact.

Code authority:
`pc-strategy-editor.ts`, `pc-strategy-board.ts`, `pc-edge-layer.ts`,
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
`apps/web/src/app/components/pc-simulator.ts`, `pc-run-trace.ts`,
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
`pc-strategy-editor.ts`.

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

The current v1 schema has no recombinator/feeder node kinds and no item-flow
wires. It has no publishing/account resource contract. Aggregate Simulator
node/edge overlays, empirical focus/trim, recombinator item flow, and
publishing remain deferred in [Product Notes](NOTES.md), the
[solver roadmap](../future/solver-roadmap.md), and other `future/` references.

Historical UI plans and approved visual evidence are in the
[product-design archive](../archive/2026-07-product-design/README.md).
