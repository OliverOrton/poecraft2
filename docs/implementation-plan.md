# Implementation Plan

Modifier vocabulary and action-selection semantics in this plan defer to [mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md).

## Purpose

This is the build plan for turning the planning docs into code. It assumes the current decisions:

```text
engine internals: C++20
public boundary: C ABI
ingest: Python
source database: SQLite
frontend: Vite + TypeScript + native Web Components
workspace: dockview-core tabs/splits
first binding: Python
browser runtime: WebAssembly later
accounts/community backend: later phase
game: Path of Exile 1
```

The first objective is not to support every crafting mechanic. The first objective is to prove the full vertical slice:

```text
source data -> SQLite -> compiled data -> native engine -> action simulation -> lean regression checks -> local workspace/Stash
```

Once that slice is correct, mechanics and UI surfaces can expand safely.

## Build Principles

- Keep SQLite canonical, but never use SQLite in hot simulation loops.
- Keep engine runtime data compact, dense, and consistent across bindings.
- Add mechanics only after the session mod universe, item state, masks, weights, and action loop are proven.
- Every public engine API returns explicit result codes.
- Random state is owned by the action/simulator context; never use process-global random state.
- The frontend asks the engine for rule answers; it does not reimplement crafting rules.
- Strategy graph editing is UI-authoring; the native engine simulator executes compiled strategies for Python and WASM callers.
- Item and strategy resources use stable IDs/schema versions from the first local save implementation.
- Item and strategy content saves manually; workspace layout persistence is separate.
- The old app is a design reference, not a compatibility target. Do not require byte-for-byte behavior, saved-strategy compatibility, or old-engine parity.
- Keep testing practical. Prefer smoke tests, a few rule fixtures, action invariants, and bug regressions over broad coverage goals.

## Phase 0: Scaffolding And Tooling

Goal: create the minimum repo structure and commands needed for repeatable work.

Create:

```text
schemas/sqlite/
tools/ingest/
engine/
fixtures/
scripts/
```

Add root scripts:

```text
scripts/build.ps1
scripts/test.ps1
scripts/format.ps1
```

Add initial toolchain files:

```text
tools/ingest/pyproject.toml
engine/CMakeLists.txt
engine/include/poecraft/api.h
engine/include/poecraft/result.h
```

Acceptance gate:

- `scripts/build.ps1` runs without doing much yet.
- `scripts/test.ps1` runs smoke tests for the pieces that exist.
- A small GitHub Actions workflow runs the same build/test entry points on Windows.
- Add Linux and WASM jobs when those targets exist; do not create a broad CI matrix before there is code to exercise.

## Phase 1: SQLite Schema And Full Dataset Ingest

Goal: produce a canonical SQLite database from the complete current crafting-relevant RePoE dataset.

Canonical ingest is broad from the beginning. It must not discard rows merely because the first engine fixture or currently implemented mechanics do not use them. Runtime support can remain narrow while canonical source preservation is complete.

Implement schema:

```text
data_manifest
source_file
tag
item_class
base_item
base_item_tag
base_item_implicit
mod
mod_stat
mod_spawn_weight
mod_generation_weight
mod_implicit_tag
mod_adds_tag
bench_option
bench_option_item_class
bench_option_cost
fossil
fossil_weight
fossil_mod_link
essence
essence_mod
cluster_jewel
cluster_jewel_passive
cluster_notable
```

Implement ingest modules:

```text
repo_loader.py
normalize_mods.py
normalize_bases.py
normalize_essences.py
normalize_fossils.py
write_sqlite.py
cli.py
```

Ingest all available rows for the implemented canonical source tables:

```text
all released base items and their tags/implicits
all mod rows in the relevant source domains
all mod stats and display text
all ordered spawn and generation weights
all classification and added tags
all exclusivity groups and source metadata
all bench options and costs
all fossil definitions, weights, and mod links
all essence definitions and guaranteed-mod links
all cluster-jewel definitions, passives, and notable records
```

Cluster-jewel records are normalized and retained now, but cluster-specific session construction is deferred. Until cluster enchant/passive-tag selection, notable caps, and socket rules are implemented, attempting to create a cluster-jewel session must return an explicit unsupported-feature result. It must not silently use normal jewel rules.

Acceptance gate:

- Ingest builds `data/sqlite/poecraft.db` deterministically.
- Running ingest twice produces the same schema version and data hash.
- A validation command prints row counts and top-level warnings.
- Source-to-SQLite validation accounts for every row in the crafting-relevant source files; skipped rows require an explicit reason code.
- Cluster-jewel source records are present and internally referentially valid even though cluster session construction is not yet supported.
- A query can return all normal rollable prefix/suffix mods for the chosen base.

## Phase 2: Lean Spec Fixtures

Goal: create a few small, inspectable fixtures that define the new engine's intended behavior before writing too much action logic.

Create fixture folders:

```text
fixtures/spec/session-pools/
fixtures/spec/action-results/
```

Initial fixtures:

```text
session mod universe for one representative base/item level
one normal prefix candidate pool
one normal suffix candidate pool
one chaos or alchemy candidate pool
weights for selected pools
one fractured reforge case
```

Fixtures should be authored from the canonical schema, explicit rule expectations, and hand-inspected examples. The old app can help explain mechanics, but it should not generate the expected results.

Fixture shape:

```json
{
  "fixture_version": 1,
  "base": "...",
  "item_level": 86,
  "effective_tags": ["default", "..."],
  "pool": [
    {
      "mod_id": "...",
      "group": "...",
      "generation_type": "prefix",
      "spawn_weight": 1000,
      "generation_multiplier_pct": 100,
      "final_weight": 1000
    }
  ]
}
```

Acceptance gate:

- Fixture loading and validation is scriptable.
- Fixtures are small enough to inspect in review.
- Each fixture explains the rule being tested.
- No fixture depends on old-app serialized data or exact old-app RNG sequences.
- Do not add fixtures just to increase coverage; add them for core rules, tricky mechanics, or bugs.

## Phase 3: Compiled Data Format

Goal: turn the complete canonical SQLite dataset into one engine-loadable runtime artifact.

Start simple:

```text
manifest.json
mods.json or mods.bin
strings.json or strings.bin
```

Use JSON arrays first if that makes debugging faster. The artifact must still contain the complete runtime dataset; the later binary format is an encoding optimization, not the point where multi-base support appears.

Compiled data should include:

```text
all released base items, item classes, tags, and base implicits
the complete normalized global mod catalog
all exclusivity groups and ordered spawn/generation-weight rows
classification tags, added tags, and stat rows
stable serialized mappings for domains, generation types, influences, and flags
bench options/costs/item-class restrictions
fossil definitions, weights, tags, and linked mods
essence definitions and guaranteed-mod links
cluster definitions/passives/notables, marked runtime-unsupported
string table
```

Acceptance gate:

- `compile_engine_data.py` reads SQLite and writes one complete runtime artifact.
- The manifest declares `complete_dataset: true` and records schema/source hashes, generated timestamp, enum mappings, and row counts.
- A validation command compares every compiled section and relationship count against canonical SQLite and validates parallel-array offsets/IDs.
- Every released base is present. Cluster and other unsupported runtime families remain represented and carry explicit support classification rather than being omitted.
- The full artifact can supply the data needed to build a session for any ordinary non-cluster base without returning to SQLite.
- Vaal Regalia remains a detailed fixture only; it is not a filter on the compiled artifact.

## Phase 4: Native Engine Foundation

Goal: build the native engine shell with fixed-width, testable primitives.

Implement:

```text
result codes
fixed-width public types
ABI version and struct-size fields
data loader
file and memory data-loading entry points
string/id lookup tables
engine-owned RNG state
bitset word helpers
ItemState and ModSlot
CraftScratch
per-worker action-context handle
basic debug printing helpers
opaque-handle create/destroy rules
```

Engine files:

```text
engine/include/poecraft/api.h
engine/include/poecraft/item_state.h
engine/include/poecraft/session.h
engine/src/rng.cpp
engine/src/bitset.cpp
engine/src/data_loader.cpp
engine/src/item_state.cpp
```

Acceptance gate:

- Engine smoke tests compile and run.
- `ItemState` copy is a plain cheap value copy.
- Basic add/remove/compact side helpers have focused unit tests.
- Failed mutating API calls leave item state unchanged.
- Data/session/action-context handles and caller-provided output buffers pass basic lifetime/leak checks.
- Two action contexts can safely share one immutable session while owning independent random state, scratch space, and caches.
- Provisional fixed capacities are checked against the current ingested dataset before the ABI is frozen.
- The native loader loads the complete runtime artifact and can resolve multiple ordinary bases by stable metadata path.

## Phase 5: Session Builder, Masks, And Weights

Goal: build the generic session path for every ordinary non-cluster base represented in the complete artifact, with detailed correctness checks against a few small fixtures.

Implement:

```text
dense session mod ids
session_global_mod_id[]
prefix_mask
suffix_mask
base_explicit_universe_mask
normal_random_roll_mask
crafted/essence/implicit exclusion masks
group_mask[group_id]
implicit_tag_mask[tag_id]
influence masks
tag_signature interning
base_roll_weight[tag_signature_id][mod_id]
positive_base_weight_mask[tag_signature_id]
positive_spawn_weight_mask[tag_signature_id]
```

Include every influence-specific mod reachable for the selected base/item level in the same dense session universe. Keep prefix, suffix, influence, and mechanic masks as separate bitsets over those shared IDs. Sessions are immutable after construction. Build uncommon influence/tag-signature weight arrays lazily in the worker-local action context.

Implemented baseline: the session universe also includes crafted mods available
to the item class, essence direct mods, base implicits, and base-compatible
fossil added/forced mods. Group, classification-tag, mechanic, and influence
masks are materialized over the shared dense IDs. The worker-local context
interns influence signatures and caches exact candidate-mask keys with compact
prefix-sum weighted pools.

The request-shaped pool debug API can return either accepted candidates or all
session rows with the first failing filter, active ordered weight rows,
multipliers, and final weight. The action context also retains a bounded
last-action stage trace with per-side totals, random roll, chosen mod, and
chosen side.

The complete artifact already contains all bases and the global mod catalog. Session construction filters that shared catalog by the requested base, item level, domain/item class, ordered selector weights, influence reachability, and enabled mechanics. The first detailed fixture uses Vaal Regalia, but ordinary armour, weapon, jewel, and abyss-jewel smoke cases must pass through the same generic path. Cluster-jewel session creation remains explicitly unsupported in this phase; cluster records being present in the artifact does not imply that cluster runtime rules are implemented.

Implement debug APIs:

```text
get session mod count
dump mask as mod ids
dump weighted pool
compare a spec fixture pool
```

Acceptance gate:

- Session mod universe matches a spec fixture for selected base/item level.
- Normal prefix/suffix weighted candidate pools match a spec fixture after exclusivity-group blocking and item-state filters.
- Final weights match documented truncation behavior.
- Combined prefix/suffix draws and Harvest spawn-only targeting match their focused fixtures.
- Session creation succeeds through the generic path for representative armour, weapon, jewel, and abyss-jewel bases from the same loaded artifact.
- Cluster-jewel session creation returns the explicit unsupported-feature result until cluster runtime support is implemented.

## Phase 6: Core Action Engine

Goal: implement the first real crafting actions against compact state.

Initial actions:

```text
transmute
augment
alteration
regal
alchemy
chaos
exalt
annul
scour
```

Then add:

```text
essence craft
basic fossil craft
```

Implemented baseline: essence actions resolve their guaranteed item-class mod,
respect the essence item-level restriction, add it directly, then use the normal
filler pool. Basic fossil actions apply added/forced mod links and
classification-tag weight multipliers before sampling from the shared
prefix-sum pool. Fossil-specific side effects such as quality, white sockets,
lucky rolls, and corrupted-essence behavior remain with the later mechanic
expansion.

Core action rules:

- Add-one actions use current groups from the live item.
- Reforge actions clear removed slots before rebuilding group blocking.
- Fractured and locked-side slots are preserved.
- Essence guaranteed mods are direct lookup, then normal mods fill remaining slots.
- Fossil forced mods are added first, then fossil-weighted mods fill remaining slots.

Acceptance gate:

- Basic action tests validate legal outcomes and item-state invariants.
- Spec pool checks pass before and after action mutation.
- Reforge tests prove removed groups do not block new rolls.
- Annul/remove tests respect fractured and locked-side behavior.
- Keep this set small; expand only when a new mechanic or bug needs it.

## Phase 7: Python Binding And Batch API

Goal: expose the native engine to Python for validation tooling, batch simulation, and future ML work.

Implement:

```text
bindings/python/
  pyproject.toml
  poecraft_engine/
```

Minimum Python API:

```python
data = load_data(path)
session = data.create_session(base_key, item_level)
context = session.create_action_context()
item = session.create_item(rarity="rare")
result = context.apply(item, {"type": "chaos"})
pool = context.debug_pool(item, {"type": "chaos"})
```

Implemented baseline: `bindings/python` provides an owning Python wrapper
over the shared C ABI library, including deterministic handle cleanup, item
value copies, rich request-shaped pool debugging, and native batch application.
`pc_apply_action_batch` parses one action request, reuses one worker context and
its weighted-pool cache, and applies the action independently to a contiguous
item array with per-item results plus an aggregate summary.

Standard fossil multipliers now stack in million-scale fixed point and truncate
only after the stacked multiplier is applied to the base roll weight. Sanctified
Fossil level/lucky weighting remains deferred; requests containing Sanctified
return `PC_RESULT_UNSUPPORTED_FEATURE` instead of silently running without its
defining behavior.

The binding can enumerate/resolve session mods and construct explicit test
items, allowing Python to validate the same fractured-reforge and exact
weighted-pool fixtures as native code. The package script builds a wheel with
the shared engine library and any required local runtime DLLs.

Acceptance gate:

- Python runs the same core action/rule checks as native, including failed
  preconditions and fractured reforge preservation.
- Python loads the canonical spec fixtures and compares exact candidate keys,
  weights, side totals, and combined totals.
- Repeated 1,000-item alchemy/chaos/exalt batches and context lifetime churn
  stay within a bounded private-memory envelope.
- An isolated install of the platform wheel loads its bundled native library
  and completes a 1,000-item batch.

## Phase 8: WebAssembly And Worker Runtime

Goal: expose the production engine boundary to the browser before building the real application UI.

Implement:

```text
bindings/wasm/
apps/web/src/app/engine-client.ts
apps/web/src/app/engine-worker.ts
```

Rules:

- Use Web Workers for long-running simulations.
- Keep API coarse-grained for batch strategy runs.
- Support cancellation and progress messages.
- Load compiled data through the C ABI memory-loading path.
- Keep the TypeScript `EngineClient` small and stable so UI components do not depend on WASM memory details.

Acceptance gate:

- Native and WASM pass the same rule-fixture and API-shape smoke checks.
- A headless browser/worker test can load data, create a session, create an item, apply an action, and query a debug pool.
- Long runs do not block the UI.

Implemented baseline: `bindings/wasm/wasm_api.cpp` is a coarse-grained,
JSON-in/JSON-out facade over the existing C ABI. Engine objects (data, session,
context, item) live inside the WASM module behind small integer handles, so the
TypeScript layer never marshals the `pc_item_state` struct or touches linear
memory. Data loads through the `pc_data_load_memory` bundle path (the multi-MB
bundle is copied onto the heap rather than passed as a stack-allocated string).
`scripts/build-wasm.ps1` compiles the engine sources plus the facade with
Emscripten to an ES module + `.wasm` under `bindings/wasm/dist`.

`apps/web/src/app/engine-client.ts` is the small, stable main-thread handle:
promise-returning and handle-based. `engine-worker.ts` owns the module and runs
in both a browser Web Worker and Node `worker_threads`; long operations are
chunked, yielding to the event loop between chunks so cancel/progress messages
flush and the UI stays responsive. Phase 10 now uses this same boundary for
compiled native strategy handles and bounded simulator chunks.
`engine-protocol.ts` defines the shared message envelopes and domain types;
`engine-wasm.ts` is the worker-side ccall marshalling layer (the WASM analogue
of the Python `_binding.py`).

The headless acceptance test (`apps/web/test/engine-smoke.test.ts`, run with
`npm test` via `tsx`) drives the WASM engine inside a real Node worker through
`EngineClient`: it loads the memory bundle, creates a session/item, applies an
action, queries debug pools, and asserts the WASM pool summaries match the same
canonical `fixtures/spec` pools the native and Python suites validate against.
Phase 10 extends the same test to compile and run a real graph strategy, inspect
native traces/examples/cost output, report progress, and cancel a long run
promptly via `AbortSignal`.

## Phase 9: Web Workspace And Emulator Slice

Goal: build the desktop-like browser workspace and the first item emulator document against the real `EngineClient`.

Use:

```text
Vite
TypeScript
native Web Components
plain CSS
dockview-core
IndexedDB
```

First components:

```text
pc-app
pc-workspace
pc-item-panel
pc-craft-bar
pc-mod-list
pc-select
pc-combobox
pc-weight-table
pc-stash
```

First UI features:

```text
open multiple item documents
rearrange tabs and create resizable splits
restore the previous workspace layout
open Emulator and Stash
keep document registration extensible for later real Simulator and Strategy Builder tabs
select base
select item level
create item
apply one craft operation at a time
show item mods
show craft history
show debug candidate pool for selected action
manually save, save as, duplicate, and reopen items
close dirty tabs with Save/Discard/Cancel
recover dirty/unsaved work after a crash or reload
Edit saved resources or Import as a new unsaved copy
```

Do not build a disposable mock application. Small fake `EngineClient` implementations are allowed only in isolated component tests; the real emulator slice integrates with WASM.

Acceptance gate:

- User can open two item documents and arrange them in tabs or a split.
- Workspace layout survives reload independently from domain content.
- Dirty or unsaved work can be recovered without appearing in Stash.
- User can run the first supported actions one by one.
- User can manually save items to Stash and reopen them.
- Saved items can be reopened without depending on the current panel layout.
- UI displays the same item state as engine debug output.
- Debug pool view matches engine debug output for the first supported actions.
- No React dependency.

## Phase 10: Native Strategy Simulator

Goal: implement the simulator in the native engine, independent of the visual editor, and expose it through Python and WASM.

Implement strategy model:

```text
StrategyGraph
StartNode
OperationNode
TerminalNode
GuardedEdge
ConditionExpression
StrategyTrace
SimulationOptions
EconomySnapshot
```

Initial conditions:

```text
always
has mod group
has modifier family at minimum tier
rarity is
open prefix count
open suffix count
prefix count range
suffix count range
```

Execution semantics:

```text
operation node applies action
outgoing edges evaluate in priority order
first matching edge wins
default edge handles fallback
reaching a success terminal defines success
run-wide action/cost/cancellation limits prevent infinite execution
economy snapshot maps each operation/input combination to chaos-equivalent cost
trace records node, action, matched edge, item snapshot, cumulative actions, cost
bounded retained traces and representative success/failure items
aggregated failure summaries
```

Acceptance gate:

- Strategy JSON can compile and run in the native engine without the UI.
- A simple chaos-repeat-until-condition strategy works.
- Trace output explains which edge matched at every step.
- C/Python/WASM callers can query retained traces, representative terminal items, and aggregated failure summaries without retaining every run.
- Linear step-style strategies can be represented by graph JSON.
- Python and WASM call the same native simulator implementation.

Implemented baseline:

- `engine/src/simulator.cpp` compiles `v1` graph JSON into dense native nodes,
  ordered guarded/default edges, resolved operation inputs, and pure compiled
  conditions.
- The start state is resolved against the compile session using stable base,
  mod, and group keys. Initial conditions cover `always`, mod-group presence,
  display-family minimum tiers, rarity, open prefix/suffix ranges,
  prefix/suffix count ranges, and nested `all`/`any`/`not`/`at_least`
  expressions.
- Run-wide action, graph-step, and optional cost limits terminate cyclic or
  unaffordable runs without adding per-node attempt semantics.
- Immutable economy snapshots use chaos-equivalent canonical keys. Basic
  actions use their operation name; essences use `essence:<metadata-key>`;
  fossils sum `fossil:<metadata-key>` entries plus
  `resonator:<socket-count>`.
- Simulator handles retain bounded traces and representative terminal items,
  aggregate non-success outcomes, and expose sorted missing-price keys. They
  never retain every run.
- `engine/include/poecraft/simulator.h` exposes the additive C ABI.
  `bindings/python` and `bindings/wasm` wrap those same handles; the browser
  worker calls `pc_simulator_run_chunk` and yields between chunks for progress
  and cancellation.
- Native, Python, and WASM tests run the same chaos-repeat graph shape and
  verify routing, success summaries, retained traces/examples, economy cost,
  missing-price reporting, and bounded cancellation.

## Phase 11: Strategy Editor UI

Goal: build the Blueprint-style graph editor and connect the real Simulator workspace tab.

Components:

```text
pc-strategy-editor
pc-strategy-board
pc-strategy-node
pc-edge-layer
pc-condition-editor
pc-run-trace
pc-simulator
```

First editor features:

```text
drag operation/start/terminal nodes
connect nodes with edges
edit node operation
edit edge conditions
manual save/load through Stash
import Emulator item as start state
run once
run N simulations
inspect trace and aggregate stats
```

Acceptance gate:

- The editor can create the same simple strategy used in Phase 10.
- Manually saved strategies round-trip without losing layout or semantics.
- Emulator import creates a new strategy with the current item as its start state.
- Invalid graphs show warnings.
- Trace highlights taken nodes and edges.

Implemented baseline:

- `pc-strategy-editor` is a dockview document with a draggable operation/start/
  terminal palette, custom HTML nodes, SVG guarded edges, pan/zoom, node
  movement, connection dragging, selection, deletion, and automatic layout.
- The inspector edits start state, operations and parameters, terminal results,
  edge priority/default routing, ANDed edge conditions, multi-select
  modifier-family ALL/N-of checks, minimum tiers, and advanced nested condition
  JSON. Client
  validation marks unreachable/dead-end nodes, invalid endpoints/defaults,
  missing terminals, and unsupported conditions.
- Strategies use the existing manual Stash policy. Saved graph JSON retains
  stable execution semantics plus node positions and viewport state; Edit
  rebinds the document and Import creates an unsaved copy.
- Emulator documents can open the current item as a new strategy containing
  only that start node; no sample route is inserted. Session-local mod ids are
  resolved back to stable mod keys before the imported start state is persisted.
- `pc-simulator` compiles through the Phase 10 WASM worker boundary for Run
  once/Run N, progress and cancellation. Aggregate results, retained native
  traces, item snapshots, total action counts, and per-operation action
  distribution are shown in the bottom panel. Action distribution is the
  default result view; the trace tab highlights taken nodes/edges and the active
  trace node on the board. The per-run action limit is user-editable and
  defaults to 100,000.
- New strategies use the shared Emulator-style class/base/item-level picker and
  open on a blank board. Modifier conditions use an Emulator-style
  prefix/suffix family browser rather than a single-line selector. Parallel and
  reciprocal edges receive separate visual lanes.
- Web model tests cover the Phase 10 chaos-repeat graph, layout/condition
  round-trip, invalid graph warnings, and Emulator start-state import. The
  worker smoke test continues to exercise the real native simulator and now
  also verifies the editor catalog and modifier-family minimum-tier routing.

## Phase 12: Account And Sync Foundation (Deferred)

Goal: add optional accounts after the local app, Stash, and Strategy Builder are usable.

Status: intentionally skipped for now. Do not implement this phase until it is
explicitly resumed. Phase 13 is the next active phase. Any later phase that
requires accounts or server-backed resources still depends on this phase.

Implement:

```text
TypeScript/Node API
PostgreSQL schema
Path of Exile OAuth 2.1 login
admin-only fallback login
account Stash
manual item/strategy sync
prompted guest-data merge
account export/deletion
minimal Terms/Privacy pages and GGG/RePoE attribution
```

Rules:

- Guests keep full local functionality.
- Existing guest data is merged only after confirmation.
- Manual Save writes account resources; unsaved changes are not uploaded.
- Stable local resource IDs/schema versions map cleanly to server resources.
- Cached account resources may be opened offline, but offline account writes are not queued; offer Save Local Copy/export.
- OAuth credentials are encrypted at rest; browser sessions use secure, HTTP-only, same-site cookies.

Acceptance gate:

- Guest use still works without the API.
- Path of Exile user can sign in and access saved resources on another browser.
- First sign-in asks whether to merge local resources.
- Admin fallback is not exposed as normal public signup.
- User can export owned resources and delete the account.
- The public app displays the required GGG non-affiliation notice and links its privacy/terms pages.

## Phase 13: Mechanic Expansion (Complete)

Goal: expand the currently functional engine with the remaining ordinary
one-item crafting mechanics in this phase's scope.

Suggested order:

1. Bench crafts and metamods.
2. Veiled exalt/chaos and unveil options.
3. Harvest reforge/augment/resistance conversion.
4. Eldritch implicits and eldritch currency.
5. Influenced exalts.
6. Corrupted implicits and special fossil effects.

Recombinators are not part of Phase 13. They are deferred to their own
standalone Phase 18 because they require two-item state, transfer rules, and a
broader session universe.

The old `poeCraft` project at `C:\Users\Oliver\poeCraft\poeCraft` should be
consulted while implementing these crafting options. Its crafting engine,
method definitions, item model, registries, and data-loading code are useful
behavioral and data-flow references. The current `poecraft2` semantics docs,
current source data, and focused fixtures remain authoritative when the old
implementation is incomplete, approximate, or conflicts with the new engine
contracts.

Each mechanic needs:

```text
ingest/schema support
session masks/tables
action implementation
debug pool output
focused spec fixture when needed
native and Python checks when needed
web UI affordance
```

Acceptance gate:

- Every Phase 13 mechanic has ingest/schema support, native implementation, debug output, focused checks, and a usable web affordance.
- Native, Python, and WASM agree on the shared rule/strategy fixtures for the complete Phase 13 mechanic set.
- Recombinators and any other unsupported mechanics are explicitly reported as unsupported rather than silently producing approximate results.
- Publishing remains disabled until the deferred Phase 12 account foundation and the Phase 13 and Phase 14 gates pass.

Implemented baseline:

- Runtime sessions load class-legal bench mods, generic veiled templates and
  unveil outcomes, corrupted implicits, Eldritch implicits by side/tier, and
  fossil sell-price rows as dense session mods with dedicated masks.
- Bench actions enforce crafted-mod limits and metamod state. Prefix/suffix
  locks preserve the correct side during rerolls/removal; cannot-roll attack
  and caster crafts filter classification-tag pools.
- Veiled exalt and veiled chaos add one placeholder with three persisted,
  weighted generic unveil choices. Unveil validates the offered choice, side,
  and live group conflicts before replacing the placeholder.
- Harvest reforge and augment use classification-tag targeting with spawn
  weight only, respect metamod blocking, and share one combined affix
  distribution. Resistance conversion preserves side and required level while
  replacing a removable source resistance.
- Ember/Ichor application maintains independent Exarch/Eater tiers and
  implicits. Eldritch exalt/chaos/annul use the documented dominant-influence
  side, while generic influenced exalts add and roll only the requested
  influence.
- Fossils retain fixed-point weight stacking and forced/added rows. Sanctified
  applies its required-level weighting; Bloodstained and Gilded add their
  implicit effects; Fractured Fossil returns a mirrored result item.
- The C ABI, Python binding, WASM worker, native strategy compiler, Emulator,
  and Strategy Builder use the same action vocabulary and parameter contract.
  Direct mechanics report selected mods through the last-action trace; weighted
  mechanics remain available through request-shaped pool debugging.
- Native engine checks, Python binding/strategy checks, and WASM worker/strategy
  checks cover the Phase 13 mechanics. The Emulator and Strategy Builder expose
  parameter controls for bench, Harvest, Eldritch, influence, veiled, and
  unveil actions.
- The post-Phase-13 UI pass groups Emulator actions into Basic Currency,
  Essences, Harvest, Fossil, Eldritch, Influenced, and Veiled panels. Essence
  selection is type-first with the highest available tier selected by default,
  and mechanic selections persist while crafting.
- Fracturing Orb is a shared native/Python/WASM/strategy action. It fractures
  one random explicit modifier on a rare item with at least four modifiers,
  ignores metamods, rejects generic influence, synthesised state, and an
  existing fracture, and remains legal with Eldritch implicits or split state.
- Emulator item rows can be right-clicked to mark that exact modifier as
  fractured for direct state editing; modifier-pool right-clicking can still
  add a selected modifier already fractured. Strategy family conditions use
  the same gesture in the condition picker or selected row to require the
  matched modifier to be fractured.
- Item displays show influence badges, reserve stable modifier-row space, use
  modifier-specific text colors, and frame the item using its rarity color.

## Interlude P: Pre-S6 Product Polish (Complete)

Before solver S6 Phase 1, complete the active polish phases in
[pre-s6-product-polish-plan.md](pre-s6-product-polish-plan.md):

1. expose canonical base drop level and order base choices high-to-low; add
   live auto-labels with manual overrides for strategy nodes and edges;
2. skipped by Oliver for now: the concrete Searing/Eater currency migration;
3. keep Calculator deliberately narrow at one input item, one goal item, one
   selected action, and engine-returned odds; redesign only the Goal surface so
   it uses the same shared item-frame UI as the input item.

Status 2026-07-15: P1 and P3 are complete; P2 remains skipped. Calculator Goal
now uses the same shared item-frame structure as Input through an explicit
target model, with the v1 goal and native odds contracts unchanged. The full
repository gate and separate-process browser recovery smoke passed. Solver S6
Phases 1-2 and 4 are complete. Oliver skipped Phase 3 ambient Emulator odds
entirely; it is not deferred work. Phase 4 added exact
veiled/eldritch evaluators, policy-selected unveil offers, and the native
condition vocabulary required to compile tag-sensitive, flagged, tiered, and
otherwise ambiguous solver states. Its full repository gate passed. S6 is
complete.

This interlude does not move crafting-rule authority into the frontend. The
Calculator and Strategy Builder keep their task-shaped public APIs, but both
remain backed by the same native action registry and exact transition provider.
The earlier multi-goal/OR contract and shared predicate-editor work are not
scheduled.

## Parallel Track E: Economy Ingest And League Switching (Planned)

The complete economy subsystem is specified in
[economy-ingest-plan.md](economy-ingest-plan.md). It is a parallel
workspace-fluency track, not a continuation of the original RePoE phases and
not a reason to mix volatile league prices into `data/sqlite/poecraft.db`.

The track builds a separate canonical economy SQLite database, dynamically
ingests every PoE1 league exposed by the configured economy provider, publishes
immutable content-addressed snapshots, caches them in the browser, and adds a
workspace-level league selector with per-league overrides. Phases E0-E4 may run
independently on a separate branch. E5-E6 must rebase before touching the
shared workspace price and cost surfaces.

Status: plan written and Oliver's primary decisions recorded: poe.ninja, PoE1
PC, six-hour refresh, 30-day detail plus weekly retention forever, warned
low-confidence prices, manual-only base cost, per-league overrides, pinned
in-flight work, one archived challenge family, zero-cost unveil selection, and
GitHub Actions -> Cloudflare R2 publication. No implementation has started.

## Phase 14: Performance And Public-Engine Readiness

Goal: optimize only after correctness is measured, then package the complete engine for public workloads.

Measure:

```text
session build time
candidate mask build time
weighted pool build time
sampling time
action throughput
batch strategy throughput
native vs WASM throughput
cache hit rates
```

Possible optimizations:

```text
alias tables for hot pools
prewarmed common tag signatures
faster group-block construction
SIMD bitset operations
binary compiled data format
worker pool for browser simulations
versioned engine/data artifact packaging
```

Prefix-sum sampling and the exact-key worker-local weighted-pool cache are
baseline engine behavior rather than deferred Phase 14 work. Phase 14 measures
their hit rate and decides whether alias tables, prewarming, or cache policy
tuning are justified.

Acceptance gate:

- Benchmarks capture a pre-optimization native baseline and show a meaningful
  improvement in the measured hot path. The retired object-heavy Python engine
  is not used as a Phase 14 comparison.
- Optimizations do not change rule fixtures, legality, or weight calculations.
- A representative 100,000-run WASM job has bounded memory, visible progress, cancellation, and an acceptable completion time.
- Engine, compiled game data, UI/cold data, and economy artifacts have immutable version/hash manifests suitable for publication retention.
- Public publishing remains disabled until Phase 15 and the deferred Phase 12
  account foundation are completed.

Status: complete.

- `tools/benchmark_engine.py` and `apps/web/test/performance-benchmark.ts`
  measure native and worker/WASM session build, pool/cache, action, strategy,
  progress, and memory behavior. The strategy matrix covers both one- and
  ten-action runs with Alteration and Chaos actions so fixed per-run overhead
  is not confused with weighted-roll throughput. `scripts/benchmark.ps1` runs
  the full matrix.
- Common non-influenced actions now use a direct tag-signature lookup, skip a
  redundant combined-affix mask operation for both-side pools, and reuse
  influence-mask storage. Detailed clock-based profiling is explicitly opt-in;
  production action contexts retain cheap cache counters without making host
  clock calls in candidate-building and modifier-sampling loops.
- Against commit `7158ca5` with the same current data and direct WASM harness,
  one-action Alteration improved from about 832k to 1.29m actions/s and
  ten-action Alteration from 985k to 1.80m actions/s. One-action Chaos improved
  from about 232k to 384k actions/s and ten-action Chaos from 252k to 456k
  actions/s. Chaos is intentionally slower because each action performs four
  to six weighted modifier draws.
- Worker yielding uses `MessageChannel` instead of timer clamping and adapts
  chunk sizes toward about 16 ms of native work. Strategy Builder progress
  rendering is frame-throttled and no longer rebuilds the graph for every
  chunk. A real warmed browser run of 100,000 one-action Alteration simulations
  reports about 1.0m actions/s. The 100,000-run worker matrix retains visible
  progress, prompt AbortSignal cancellation, and zero WASM memory growth after
  simulator creation.
- `scripts/package-public-artifacts.mjs` builds a content-addressed,
  hash-verified manifest containing native and WASM engines, compiled game
  data, production UI/cold assets, and an immutable no-price economy snapshot.

## Phase 15: Publishing And Discovery

Goal: add strategy sharing only after the complete public engine and readiness gates have passed.

Prerequisite: the deferred Phase 12 account and sync foundation must be resumed
and completed before this phase begins.

Implement:

```text
private/unlisted/public visibility
immutable strategy versions
100,000 browser simulations on publish
publication statistics and economy snapshot metadata
versioned historical artifact bundles
profiles
favorites
forks with attribution
follows
ratings
basic public publication listing
unpublish
```

Published summary:

```text
success rate
average/median/percentile cost when complete
cost status and missing price keys
average craft actions per run
sample count
engine/data versions
economy snapshot
```

Rules:

- Reaching a success terminal defines publication success.
- Restart transitions only control flow.
- Strategy cards show title, description, and a compact success-route summary.
- Complex goals are not converted into invented item previews.
- Unknown prices mark cost statistics incomplete; they are never silently treated as free.
- A strategy using cost-based control flow cannot publish against an economy snapshot missing a required price.
- Unpublishing preserves immutable version/fork attribution records.
- Historical publications use their archived checksum-verified artifact bundle when rerunnable and fall back to view-only when an old runtime is no longer safe/compatible.
- Initial publishing uses safe text rendering, length limits, and rate limits. Reports and admin moderation are a later phase.

Acceptance gate:

- User can publish an immutable strategy version with 100,000-run statistics.
- Another user can favorite, rate, follow the author, and fork the version.
- Public/unlisted/private visibility behaves correctly.
- Published costs remain tied to their economy snapshot and clearly show incomplete status when applicable.
- An old publication never silently runs on replacement engine/data.

## Phase 16: Reports, Moderation, And Comments

Goal: add community governance after initial publishing is operating.

Implement:

```text
report publication/profile
admin hide/unpublish
moderation action audit records
comments after reporting/moderation
activity features later
```

Acceptance gate:

- Users can report public content and administrators can review and hide it.
- Moderation actions are rate-limited and auditable.
- Comments do not launch before reporting and moderation.

## Phase 17: ML And Private Training

Goal: add ML tooling after simulator correctness and throughput are stable.

Before any learned model, the exact DP crafting solver
([crafting-solver-plan.md](crafting-solver-plan.md), phases S1-S6) must
exist: it computes optimal strategies from known transition
probabilities, and its logged (state, value, action) corpus is the
training data and evaluation baseline for everything in this phase.

Initial ML path:

```text
Python binding
native strategy simulator
batch simulation
static economy baseline
trajectory export
baseline Monte Carlo strategy evaluation
```

Do not start here. ML depends on a trustworthy engine and native strategy simulator.

## Phase 18: Recombinators (Deferred)

Goal: implement recombinators as a dedicated two-item crafting system after the
ordinary one-item mechanic expansion is stable.

Implement:

```text
two-item recombination session
input-side and output-base state
transfer eligibility and exclusion masks
prefix/suffix selection and retention rules
special modifier and item-state handling
debug transfer-candidate output
native, Python, and WASM actions
strategy and Emulator UI support
focused recombination fixtures
```

Use the existing recombinator architecture notes in
`architecture-plan.md`, `data-shapes-and-ingest.md`, and `engine-bitsets.md` as
the starting contract. Re-check the live Path of Exile 1 rules and current
source data when this phase begins; do not infer recombination behavior from
ordinary one-item rolling.

The solver/editor side of recombinators — the spec-pyramid model,
recomb/feeder strategy blocks, recycling wires, and the pyramid
auto-planner — is specified in
[solver-mechanic-extensions.md](solver-mechanic-extensions.md) (solver
phases S10-S11) and [strategy-editor-ui.md](strategy-editor-ui.md). This
phase provides the engine substrate those phases consume; the exact
outcome enumerator here and solver phase S10 are the same effort viewed
from the engine and solver sides.

Acceptance gate:

- Recombination uses an explicit two-item session and does not broaden every ordinary crafting session.
- Transfer candidates, exclusions, item-state changes, and output-base behavior are inspectable through debug output.
- Native, Python, and WASM agree on focused recombination fixtures.
- Unsupported recombinator variants fail explicitly rather than using approximate transfer rules.

## Immediate Next Task

Phase 14 is complete. Phase 15 publishing remains blocked until the deferred
Phase 12 account and sync foundation is explicitly resumed and completed.
Solver S6 is complete: Phases 1, 2, and 4 landed, and Oliver skipped Phase 3
entirely. No new primary boundary is active until Oliver chooses one. Parallel
Economy Track E is planned with its main decisions recorded; its E0-E4
implementation may proceed independently on a dedicated branch. Recombinators
remain deferred to Phase 18.

## Definition Of Done For MVP

The first MVP is complete when:

- Ingest can build the SQLite database from source data.
- Complete compiled data can be loaded by the native engine.
- Sessions can be created generically for ordinary non-cluster bases at selected item levels.
- Core actions can mutate `ItemState`.
- Candidate pools and weights match the small spec fixture set.
- The web workspace can open multiple item documents and manually save/reopen them through Stash.
- Item emulator documents can apply supported actions one by one.
- A simple strategy graph can run repeated simulations.
- Native, Python, and WASM expose the same covered rules and simulator behavior.
