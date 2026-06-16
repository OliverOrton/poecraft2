# Implementation Plan

## Purpose

This is the build plan for turning the planning docs into code. It assumes the current decisions:

```text
engine internals: C++20
public boundary: C ABI
ingest: Python
source database: SQLite
frontend: Vite + TypeScript + native Web Components
first binding: Python
browser runtime: WebAssembly later
```

The first objective is not to support every crafting mechanic. The first objective is to prove the full vertical slice:

```text
source data -> SQLite -> compiled data -> native engine -> action simulation -> regression tests -> simple web emulator
```

Once that slice is correct, mechanics and UI surfaces can expand safely.

## Build Principles

- Keep SQLite canonical, but never use SQLite in hot simulation loops.
- Keep engine runtime data compact, dense, and deterministic.
- Add mechanics only after the base pool, item state, masks, weights, and action loop are proven.
- Every public engine API returns explicit result codes.
- Every random action uses caller-owned deterministic RNG state.
- The frontend asks the engine for rule answers; it does not reimplement crafting rules.
- Strategy graph editing is UI-authoring; compiled strategies must run without the UI.
- The old app is a design reference, not a compatibility target. Do not require byte-for-byte behavior, saved-strategy compatibility, or old-engine parity.

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
- `scripts/test.ps1` runs ingest tests and engine tests, even if there are only smoke tests.
- CI is optional at this point, but local commands must be the source of truth.

## Phase 1: SQLite Schema And Seed Ingest

Goal: produce a canonical SQLite database from a small but real subset of source/RePoE-shaped data.

Implement schema:

```text
mods
mod_stats
mod_spawn_weights
mod_generation_weights
mod_implicit_tags
mod_adds_tags
mod_flags
mod_groups
base_items
base_tags
essence_guarantees
fossils
fossil_weight_modifiers
fossil_added_mods
fossil_forced_mods
bench_options
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

Start with a narrow data subset:

```text
one normal non-cluster base
normal explicit prefix/suffix mods for that base
mod stats and display text
spawn weights
generation weights
implicit tags
groups
basic essences/fossils only if needed for early tests
```

Acceptance gate:

- Ingest builds `data/sqlite/poecraft.db` deterministically.
- Running ingest twice produces the same schema version and data hash.
- A validation command prints row counts and top-level warnings.
- A query can return all normal rollable prefix/suffix mods for the chosen base.

## Phase 2: Spec And Regression Fixtures

Goal: create small, inspectable fixtures that define the new engine's intended behavior before writing too much action logic.

Create fixture folders:

```text
fixtures/spec/session-pools/
fixtures/spec/action-results/
fixtures/spec/strategy-runs/
```

Initial fixtures:

```text
base mod pool for one base/item level
normal prefix candidate pool
normal suffix candidate pool
chaos/alchemy/exalt candidate pools
weights for selected pools
one fractured reforge case
one prefix-lock or suffix-lock reforge case
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
- No fixture depends on old-app serialized data or old-app RNG behavior.

## Phase 3: Compiled Data Format

Goal: turn SQLite rows into engine-loadable arrays.

Start simple:

```text
manifest.json
mods.json or mods.bin
strings.json or strings.bin
```

Use JSON for the first tiny slice if that makes debugging faster. Move to binary arrays once the data shape stabilizes.

Compiled data should include:

```text
global mod ids
group ids
generation type
domain
required level
flags
spawn weight rows
generation weight rows
implicit tag ids
stat rows
base item ids
base tag signatures
string table
```

Acceptance gate:

- `compile_engine_data.py` reads SQLite and writes compiled artifacts.
- Artifacts have schema version, source hash, generated timestamp, and row counts.
- A validation command can diff SQLite counts against compiled counts.

## Phase 4: Native Engine Foundation

Goal: build the native engine shell with deterministic primitives.

Implement:

```text
result codes
fixed-width public types
data loader
string/id lookup tables
deterministic RNG
bitset word helpers
ItemState and ModSlot
CraftScratch
basic debug printing helpers for tests
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

- Engine tests compile and run.
- RNG gives identical sequences across test runs.
- `ItemState` copy is a plain cheap value copy.
- Basic add/remove/compact side helpers pass unit tests.

## Phase 5: Session Builder, Masks, And Weights

Goal: build a session for one base/item level and match the new engine's spec fixtures.

Implement:

```text
dense session mod ids
session_global_mod_id[]
prefix_mask
suffix_mask
base_spawnable_mask
normal_random_roll_mask
crafted/essence/implicit exclusion masks
group_mask[group_id]
implicit_tag_mask[tag_id]
influence masks
tag_signature interning
base_roll_weight[tag_signature_id][mod_id]
positive_base_weight_mask[tag_signature_id]
```

Implement debug APIs:

```text
get session mod count
dump mask as mod ids
dump weighted pool
compare fixture pool
```

Acceptance gate:

- Session pool matches spec fixture for selected base/item level.
- Normal prefix/suffix pools match spec fixture after group blocking and item state filters.
- Final weights match documented truncation behavior.

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

Core action rules:

- Add-one actions use current groups from the live item.
- Reforge actions clear removed slots before rebuilding group blocking.
- Fractured and locked-side slots are preserved.
- Essence guaranteed mods are direct lookup, then normal mods fill remaining slots.
- Fossil forced mods are added first, then fossil-weighted mods fill remaining slots.

Acceptance gate:

- Seeded basic action tests are deterministic.
- Spec pool tests pass before and after action mutation.
- Reforge tests prove removed groups do not block new rolls.
- Annul/remove tests respect fractured and locked-side behavior.

## Phase 7: Python Binding And Batch Runner

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
session = data.create_session(base_id, item_level)
item = session.create_item(rarity="rare")
result = session.apply(item, {"type": "chaos"}, seed=123)
pool = session.debug_pool(item, {"type": "chaos"})
```

Acceptance gate:

- Python can run the same seeded action tests as native.
- Python can load spec fixtures and compare pools/weights.
- Batch simulation can run many chaos/alchemy/exalt attempts without leaking memory.

## Phase 8: Web Emulator Slice

Goal: build the first browser-facing UI around the engine API.

Use:

```text
Vite
TypeScript
native Web Components
plain CSS
```

First components:

```text
pc-app
pc-item-panel
pc-craft-bar
pc-mod-list
pc-select
pc-combobox
pc-weight-table
```

First UI features:

```text
select base
select item level
create item
apply one craft operation at a time
show item mods
show craft history
show debug candidate pool for selected action
```

Start with a mock engine client if WASM is not ready, but keep the interface shaped like the final engine client.

Acceptance gate:

- User can run the first supported actions one by one.
- UI displays the same item state as engine debug output.
- Debug pool view matches native/Python test expectations.
- No React dependency.

## Phase 9: WebAssembly And Worker Runtime

Goal: run the real engine in the browser.

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
- Keep deterministic seeds visible in debug output.

Acceptance gate:

- Native and WASM replay the same seeded action sequence.
- Browser emulator can use real engine data.
- Long runs do not block the UI.

## Phase 10: Strategy Graph Runner

Goal: implement strategy execution independent of the visual editor.

Implement strategy model:

```text
StrategyGraph
StartNode
OperationNode
TerminalNode
GuardedEdge
ConditionExpression
StrategyTrace
```

Initial conditions:

```text
always
has mod group
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
trace records node, action, attempts, matched edge, item snapshot, cost
```

Acceptance gate:

- Strategy JSON can run without the UI.
- A simple chaos-repeat-until-condition strategy works.
- Trace output explains which edge matched at every step.
- Linear step-style strategies can be represented by graph JSON.

## Phase 11: Strategy Editor UI

Goal: build the Blueprint-style graph editor.

Components:

```text
pc-strategy-editor
pc-strategy-board
pc-strategy-node
pc-edge-layer
pc-condition-editor
pc-run-trace
```

First editor features:

```text
drag operation/start/terminal nodes
connect nodes with edges
edit node operation and max attempts
edit edge conditions
save/load graph JSON
run once
run N simulations
inspect trace and aggregate stats
```

Acceptance gate:

- The editor can create the same simple strategy used in Phase 10.
- Saved JSON round-trips without losing layout or semantics.
- Invalid graphs show warnings.
- Trace highlights taken nodes and edges.

## Phase 12: Mechanic Expansion

Goal: expand coverage after the core loop is proven.

Suggested order:

1. Bench crafts and metamods.
2. Veiled exalt/chaos and unveil options.
3. Harvest reforge/augment/resistance conversion.
4. Eldritch implicits and eldritch currency.
5. Influenced exalts.
6. Corrupted implicits and special fossil effects.
7. Recombinators.

Each mechanic needs:

```text
ingest/schema support
session masks/tables
action implementation
debug pool output
spec fixture
native and Python tests
web UI affordance
```

## Phase 13: Performance Pass

Goal: optimize only after correctness is measured.

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
weighted pool cache
alias tables for hot pools
prewarmed common tag signatures
faster group-block construction
SIMD bitset operations
binary compiled data format
worker pool for browser simulations
```

Acceptance gate:

- Benchmarks show the new engine is meaningfully faster than an object-heavy Python baseline.
- Optimizations do not change seeded replay results.

## Phase 14: ML And Private Training

Goal: add ML tooling after simulator correctness and throughput are stable.

Initial ML path:

```text
Python binding
strategy graph runner
batch simulation
static economy baseline
trajectory export
baseline Monte Carlo strategy evaluation
```

Do not start here. ML depends on a trustworthy engine and strategy runner.

## Immediate Next Task

Start with Phase 0 and Phase 1 together:

1. Create minimal repo scaffolding.
2. Add local build/test scripts.
3. Add SQLite schema draft.
4. Add Python ingest package skeleton.
5. Ingest one small real base/mod subset.
6. Write the first validation report.

The first meaningful milestone is:

```text
Given one selected base and item level,
the new pipeline can list the expected normal rollable prefix/suffix mods from canonical data.
```

That milestone proves the data path before the engine gets complicated.

## Definition Of Done For MVP

The first MVP is complete when:

- Ingest can build the SQLite database from source data.
- Compiled data can be loaded by the native engine.
- A session can be created for a selected base/item level.
- Core actions can mutate `ItemState`.
- Candidate pools and weights match spec fixtures.
- The web emulator can apply supported actions one by one.
- A simple strategy graph can run repeated simulations.
- Native, Python, and WASM seeded runs agree for the covered action set.
