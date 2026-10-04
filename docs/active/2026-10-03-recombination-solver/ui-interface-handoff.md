# Builder presentation handoff — 2026-10-04

Source boundary: `9736e766de959070b0c89fcb71281438ebd6f64c` on
`dot/sol61-recomb-builder-20261004`; engine tree
`149bbc4ce0a555435d358d2f52a4c83f06d18166`, web source tree
`68f0d291b3861695ff00e9690afd73fa438c5c8b`. This is **source-only,
unbuilt and unqualified**. Neither component receipt qualifies the combined
source. This handoff authorizes presentation work within the parent's selected
UI task; it does not qualify or change execution contracts.

## File ownership

All paths below are relative to `apps/web/src/`.

| File | Safe presentation scope | Preserve / coordinate with execution owner |
| --- | --- | --- |
| `styles/app.css`, `styles/continuity.css` | Palette, typography, surfaces, spacing, focus/selection treatment | Node width/port geometry and event hit targets below |
| `app/components/pc-strategy-node.tsx` | Rendered node markup, icons, labels, template explanation | `strategyNodeConnectors`, `strategyResourcePorts`, `data-node-id`, `data-port-id`, pointer events and port placement |
| `app/components/pc-strategy-editor.tsx` | Inspector/palette markup, shared presentation components, help copy | Graph mutations, slot bindings, source-entry edits, saved snapshots, history, run/evaluation handlers |
| `app/components/pc-run-trace.ts` | Trace layout, actual-output/resource cards, readable receipts | Runtime snapshot selection, all identity/cost/provenance fields, selection/highlight events |
| `app/components/pc-strategy-board.ts` | Board chrome/surface styles | Zoom/pan transforms, pointer lifecycle, hit testing, edge create/reconnect payloads and measurement |
| `app/components/pc-edge-layer.ts` | Edge strokes, colors, card/label styling | Port endpoints, preview/reconnect geometry, item/control distinction, rank/default routing and delegated events |
| `app/strategy-eval-presentation.ts` | Existing result label formatting where semantics stay identical | Stale/qualified/incomplete status and the meaning of numerical results |

Execution-owned adapters remain `app/strategy-model.ts`,
`app/engine-protocol.ts`, `app/engine-worker.ts`, the WASM facade and native
providers. Presentation may consume their current fields; schema, validation,
mechanics, cancellation and serialization changes need this owner. The native
planner worker route has not been delivered. Do not imply a working browser
planner or Cancel operation from the presence of draft native APIs.

## Connectors and physical resources

`strategyNodeConnectors` is the single connector source. A recombination node
has `input_a` at y=36 and `input_b` at y=78; ordinary input and `output` are at
y=54. Source-only nodes have no input; terminal nodes have no output. Node width
is 210px, port diameter 14px, rendered top is `port.y - 7`. Edge endpoints use
the same centers and width; selected reconnect handles are 18px outside those
endpoints with radius 7. Preserve these values for this visual migration.
Changing dimensions later requires node, board, edge preview/routing and hit
geometry to change together.

Retain `data-port-id`, `data-node-id`/`dataset.nodeId`, `data-edge-id` and
`data-edge-end`. Keep pointer propagation/capture behavior and existing event
payloads: `strategy-connect-start`, `strategy-edge-create`,
`strategy-edge-reconnect-start`, `strategy-edge-reconnect`, node drag/select
and `trace-highlight`. A/B bindings are typed identities, not vertical or
visual guesses. Item edges keep `kind`, `from_port` and `to_port`. Connected
supplies override configured input bindings through the existing model owner.

Keep the names **Recombination**, **Donor item** and **Saved feeder**. Start and
Stash-derived donor specifications are authored templates, not proof that a
physical live item exists. Suggested Start explanation: "Authored starting
item template"; when `start_item_present === false`, explain "Execution entry;
no starting item is owned." Resource templates are acquired and charged only
when the executed source requests them. Only `initially_owned` declares a live
starting resource, with `initial_cost_chaos` handled by the execution owner.

Source connections, input/output slot bindings, automatic Start entry and
pinned child snapshot edits must retain their coherent Undo/Redo behavior.
Do not reconnect both fresh sources around a recycling loop: the checked
export uses a fresh entry and separate continuation retaining the actual output.

## Actual output and paid provenance

`StrategyTraceEntry.resources` is the native resource snapshot. Its
`active_output` identifies the active physical output; use that resource's
`base_key`, `item_level`, `identity` and `item` for its card. `entry.item` is the
runner's current-item snapshot and is a distinct field. An authored
`strategy.base_state` or donor template must never replace a missing runtime
item. Show the current snapshot and full resource inventory separately when
useful; consumed/unavailable slots must not look like owned live items.

Preserve resource lifecycle/acquisition counts, recombination model/carrier/
input/output identities, and paid feeder receipts: child document ID/revision,
output contract, actual returned resource/child resources, terminal/failure
status, actions, known child and acquisition costs, price key, cost completeness
and acceptance. Equal specifications can be different physical identities;
presentation must not collapse them or imply free reuse after consumption.
Keep both `known_cost` and `child_known_cost`; do not add both again to a total.

Known cumulative cost and `cost_complete` are native authorities. A user-declared
all-in attempt price is an explicit scenario assumption; physical gold/dust
quantities remain unknown. No complete ranking follows from partial known costs.
The existing editor help about unconditional cost-cap refusal predates the
explicit all-in field: presentation can explain the distinction, but must not
invent an editor input, default price, or adapter capability. Advanced v3 results
remain estimated, explicit-scenario analysis only; advanced random Apply is held.

## Pending execution gate

Builder validation/editor currently allow seven explicit resource slots.
The native planner's catalogue allowance is 32; that is not an export-capacity
promise. The Ring witness uses six explicit slots and fits. Larger exports
need an execution-owned bounded solution/refusal; UI must not silently raise
limits to make them appear admitted.

LOCAL has not been granted; no heavy command or owned process is running.
Next requested batch: existing Tests/Engine builds with two compiler jobs,
header/DLL smoke, serial `--random-recombination-only`,
`--recombination-solver-only`, `--strategy-recombination-only` and
`--strategy-feeder-only`, then affected Python tests against the source-matched
isolated DLL and frozen runtime `82fb60a2`. The prepared Ring export witness
checks `1+2/.333`, not fresh-pair `3/.333`; its 1,000 native trials at seed
62667494 are unrun. Preserve all current caps/tolerances and release LOCAL
immediately after deterministic process cleanup. Matching WASM/worker and web
qualification follow that native gate. See the [source checkpoint](sol61-combined-source-checkpoint.json)
for full pins, frozen identity and unrun stages.
