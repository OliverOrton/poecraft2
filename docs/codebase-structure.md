# Codebase Structure

## Purpose

This document defines the implementation shape before code begins. The goal is to keep the project split into clear layers:

```text
data ingest -> compiled data/session build -> native engine -> bindings -> UI/tools
```

The engine should not depend on the frontend, ML tooling, or Python ingest code. The frontend should consume a small simulator API instead of knowing the engine's internal masks.

## Stack Decisions

### Core Engine

Use a native compiled engine with C-compatible boundaries.

Recommended first implementation:

```text
C++20 internally
C ABI exports at the boundary
```

Why:

- C-style structs and arrays map cleanly to the bitset/session design.
- C++ gives safer implementation tools than plain C for builders, tests, and ownership.
- A C ABI is easy to call from Python, Node, and WebAssembly wrappers.
- The hot path can still be written in plain, predictable loops over arrays.

The engine should avoid exceptions across public boundaries. Public calls should return explicit result codes and optional debug/error buffers.

### Data Ingest

Use Python for ingest and compile steps.

Why:

- The old project already used Python and RePoE-shaped structures.
- Python is convenient for messy source normalization, SQLite generation, diffing, and validation.
- Ingest is not the hot path.

SQLite remains the canonical local source database. Compiled engine data should be generated from SQLite into binary/JSON artifacts that the native engine can load quickly.

### Frontend

Use:

```text
Vite
TypeScript
native Web Components / custom elements
plain CSS
dockview-core for workspace layout
no React
```

This fits the app well. The simulator is a dense tool UI, not a media-heavy website. It needs good controls, tables, search, mod lists, item panels, and debug views. It does not need a component framework with a virtual DOM.

`dockview-core` should own the desktop-style workspace shell: tabs, dock groups, splits, resizing, and layout serialization. It supports vanilla TypeScript, so it does not change the no-React decision. Domain panels remain custom Web Components.

Use native custom elements for reusable widgets:

```text
<pc-select>
<pc-combobox>
<pc-mod-list>
<pc-item-panel>
<pc-craft-bar>
<pc-weight-table>
<pc-strategy-board>
<pc-condition-editor>
<pc-run-trace>
<pc-workspace>
<pc-stash>
<pc-simulator>
```

Use Shadow DOM selectively:

- yes for reusable low-level widgets where style isolation helps
- no for large app panels where global layout/theming is easier

Keep Lit optional. Do not start with it. If native custom elements become too verbose, add Lit later for component rendering only. Lit components are still standard custom elements, so that would not force a React-style app rewrite.

### Community Backend

Add the account backend after the local app is usable.

Recommended later stack:

```text
Node.js + TypeScript
Fastify or similarly small HTTP framework
PostgreSQL
Path of Exile OAuth 2.1
```

Use shared TypeScript contracts for resource schemas and API payloads. Path of Exile authentication, account sync, publishing, discovery, and social features belong in the backend; crafting simulation remains in the browser/native engine initially.

See [accounts-publishing-and-discovery.md](accounts-publishing-and-discovery.md).

### Frontend State

Use small explicit services instead of a frontend state framework or one oversized global store.

```text
WorkspaceService:
  open documents, active document, Dockview layout

DocumentService:
  document descriptors, dirty state, local view state

ItemRepository / StrategyRepository:
  manually saved guest resources in IndexedDB

RecoveryRepository:
  local crash-recovery snapshots that are not Stash resources

AccountClient:
  later account sync, publishing, and social API

EngineClient:
  session handles and action calls

SimulationJobService:
  worker jobs, progress, cancellation

CommandService:
  commands, menus, shortcuts, enablement
```

Updates should remain event-driven:

```text
component dispatches command event
service applies command
affected documents/components receive explicit updates
```

Avoid hidden two-way binding. Custom widgets should dispatch events and receive state through properties. Keep workspace layout state separate from item/strategy domain data.

### Styling

Use plain CSS with design tokens:

```text
apps/web/src/styles/tokens.css
apps/web/src/styles/base.css
apps/web/src/styles/layout.css
```

The visual style should be restrained and information-dense:

- compact panels
- stable grid/table dimensions
- clear active/disabled states
- strong keyboard support
- no image dependency
- no decorative hero/landing page

Custom dropdowns/comboboxes should follow accessible keyboard behavior:

- arrow keys move option focus
- enter selects
- escape closes
- typeahead/search for long lists
- visible focus states
- ARIA combobox/listbox roles where appropriate

If a control can be native without hurting the UX, use native. Use custom widgets where Path of Exile data needs search, grouping, filtering, or rich option rows.

### Tests

Use a lean test set:

```text
ingest smoke tests
engine unit/regression tests for core rules
frontend smoke tests for important custom widgets
```

Engine tests are the most important, but they should stay focused. Cover the few rules that can silently break the simulator: session masks, candidate pools, weights, item-state mutation, and simulator control flow. Avoid broad coverage targets or a large fixture matrix.

## Repository Layout

Target layout:

```text
poecraft2/
  README.md
  docs/
    accounts-publishing-and-discovery.md
    architecture-plan.md
    codebase-structure.md
    data-shapes-and-ingest.md
    desktop-workspace-ui.md
    engine-bitsets.md
    implementation-plan.md
    item-state-flow.md
    strategy-editor-ui.md
    weight-calculation-flow.md

  data/
    raw/
      repoe/
      poe_ninja/
      manual/
    sqlite/
      poecraft.db
    compiled/
      manifest.json
      mods.bin
      bases.bin
      strings.bin

  schemas/
    sqlite/
      001_initial.sql
    compiled/
      compiled-data-format.md
    postgres/
      001_accounts.sql
      002_publications.sql

  tools/
    ingest/
      pyproject.toml
      poecraft_ingest/
        __init__.py
        cli.py
        repo_loader.py
        normalize_mods.py
        normalize_bases.py
        normalize_essences.py
        normalize_fossils.py
        write_sqlite.py
        compile_engine_data.py
      tests/

  engine/
    CMakeLists.txt
    include/
      poecraft/
        api.h
        result.h
        item_state.h
        session.h
    src/
      bitset.cpp
      data_loader.cpp
      item_state.cpp
      session_builder.cpp
      weight_pool.cpp
      actions_basic.cpp
      actions_essence.cpp
      actions_fossil.cpp
      actions_harvest.cpp
      rng.cpp
    tests/
      test_session_builder.cpp
      test_basic_actions.cpp
      test_weights.cpp

  bindings/
    python/
      pyproject.toml
      poecraft_engine/
    wasm/
      package.json
      src/

  apps/
    web/
      package.json
      index.html
      vite.config.ts
      tsconfig.json
      src/
        main.ts
        app/
          account-client.ts
          app-controller.ts
          command-service.ts
          document-service.ts
          engine-client.ts
          simulation-job-service.ts
          workspace-service.ts
        persistence/
          database.ts
          item-repository.ts
          recovery-repository.ts
          strategy-repository.ts
          workspace-repository.ts
        components/
          pc-app.ts
          pc-workspace.ts
          pc-stash.ts
          pc-simulator.ts
          pc-item-panel.ts
          pc-craft-bar.ts
          pc-mod-list.ts
          pc-select.ts
          pc-combobox.ts
          pc-weight-table.ts
          pc-strategy-editor.ts
          pc-strategy-board.ts
          pc-strategy-node.ts
          pc-edge-layer.ts
          pc-condition-editor.ts
          pc-run-trace.ts
        styles/
          tokens.css
          base.css
          layout.css
        widgets/
          listbox.ts
          popup.ts
          focus.ts

    api/
      package.json
      src/
        auth/
        resources/
        publications/
        social/

  packages/
    contracts/
      package.json
      src/

  fixtures/
    spec/
      session-pools/
      action-results/
      strategy-runs/

  scripts/
    build.ps1
    test.ps1
```

Do not create every folder immediately. Add folders as the first implementation needs them.

## Layer Responsibilities

### `tools/ingest`

Responsibilities:

- read raw RePoE/source data
- normalize domains, tags, item classes, influence names, groups, stats, and weights
- write SQLite tables
- compile SQLite rows into engine-loadable artifacts
- generate validation reports and spec fixture data

It may use rich Python objects because it is not performance-sensitive.

### `engine`

Responsibilities:

- load compiled data
- build sessions for selected base/item level/mechanic set
- own immutable static masks, dense mod IDs, and source weight rows
- create per-worker action contexts that own random state, scratch space, lazy tag-signature weights, and action-pool caches
- own compact `ItemState`
- apply craft actions
- compile and execute strategy graphs as the simulator core
- return debug information for validation and UI

It should not parse RePoE directly and should not know about frontend DOM concepts.

### `bindings`

Responsibilities:

- expose the engine to Python for validation tooling and ML
- expose the engine to WebAssembly for browser UI
- keep wrapper-specific memory management out of core engine logic

Python binding comes first for validation tooling, batch simulation, and ML experiments. WebAssembly comes next, before the real simulator UI is built, so the UI integrates with the production engine boundary instead of a disposable mock implementation.

### `apps/web`

Responsibilities:

- provide the desktop-like workspace shell
- render the simulator
- render the one-action emulator
- render the visual strategy editor
- render the Stash
- provide custom widgets
- call a browser-safe engine client
- display item state, mod pools, weights, and action results

The web app should not reimplement mod pool rules. If it needs pool details, it asks the engine for debug data.

The strategy editor should be a Blueprint-style graph UI backed by deterministic simulator semantics. See [strategy-editor-ui.md](strategy-editor-ui.md).

The workspace should support multiple Emulator/Simulator/Strategy Builder tabs, resizable splits, a Stash tab, manual resource saves, and layout restoration. See [desktop-workspace-ui.md](desktop-workspace-ui.md).

### `apps/api` (Later Phase)

Responsibilities:

- authenticate Path of Exile users and the admin fallback
- sync manually saved items and strategies
- create immutable published strategy versions
- store 100,000-run publication summaries
- register/archive checksum-verified engine, data, and economy artifact bundles used by publications
- serve public/private/unlisted resources
- support profiles, favorites, forks, follows, and ratings

Reports, moderation, and comments are later API capabilities, not requirements for the initial publishing release. The API should not implement crafting rules or run the engine in the initial account phase.

## Engine Public API Shape

Keep the first API small.

```c
uint32_t pc_abi_version(void);

pc_result pc_data_load_file(
    const char* manifest_path,
    pc_data_handle* out_data,
    pc_error_info* out_error);

pc_result pc_data_load_memory(
    const void* bytes,
    size_t byte_count,
    pc_data_handle* out_data,
    pc_error_info* out_error);

void pc_data_destroy(pc_data_handle data);

pc_result pc_session_create(
    pc_data_handle data,
    const pc_session_options* options,
    pc_session_handle* out_session,
    pc_error_info* out_error);

void pc_session_destroy(pc_session_handle session);

pc_result pc_action_context_create(
    pc_session_handle session,
    const pc_action_context_options* options,
    pc_action_context_handle* out_context,
    pc_error_info* out_error);

void pc_action_context_destroy(pc_action_context_handle context);

pc_result pc_item_init(
    pc_session_handle session,
    const pc_item_init_options* options,
    pc_item_state* out_item,
    pc_error_info* out_error);

pc_result pc_apply_action(
    pc_action_context_handle context,
    pc_item_state* item,
    const pc_action_request* request,
    pc_action_result* out_result,
    pc_error_info* out_error);

pc_result pc_apply_action_batch(
    pc_action_context_handle context,
    pc_item_state* items,
    uint32_t item_count,
    const pc_action_request* request,
    pc_action_result* results,
    pc_batch_summary* out_summary,
    pc_error_info* out_error);

pc_result pc_debug_pool_query(
    pc_action_context_handle context,
    const pc_item_state* item,
    const pc_pool_query_request* request,
    pc_debug_pool_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_strategy_compile_json(
    pc_session_handle session,
    const char* strategy_json,
    size_t strategy_json_size,
    pc_strategy_handle* out_strategy,
    pc_error_info* out_error);

void pc_strategy_destroy(pc_strategy_handle strategy);

pc_result pc_economy_load_json(
    const char* economy_json,
    size_t economy_json_size,
    pc_economy_handle* out_economy,
    pc_error_info* out_error);

void pc_economy_destroy(pc_economy_handle economy);

pc_result pc_simulator_create(
    pc_session_handle session,
    pc_strategy_handle strategy,
    pc_economy_handle economy,
    pc_simulator_handle* out_simulator,
    pc_error_info* out_error);

pc_result pc_simulator_run_chunk(
    pc_simulator_handle simulator,
    const pc_simulation_options* options,
    uint32_t max_completed_runs,
    pc_simulation_progress* out_progress,
    pc_error_info* out_error);

pc_result pc_simulator_get_summary(
    pc_simulator_handle simulator,
    pc_simulation_summary* out_summary,
    pc_error_info* out_error);

pc_result pc_simulator_missing_price_query(
    pc_simulator_handle simulator,
    pc_price_key_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_simulator_get_trace_count(
    pc_simulator_handle simulator,
    uint32_t* out_trace_count,
    pc_error_info* out_error);

pc_result pc_simulator_trace_query(
    pc_simulator_handle simulator,
    uint32_t trace_index,
    pc_trace_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_simulator_get_example_count(
    pc_simulator_handle simulator,
    pc_terminal_kind terminal_kind,
    uint32_t* out_example_count,
    pc_error_info* out_error);

pc_result pc_simulator_example_query(
    pc_simulator_handle simulator,
    pc_terminal_kind terminal_kind,
    uint32_t example_index,
    pc_simulation_example* out_example,
    pc_error_info* out_error);

pc_result pc_simulator_failure_summary_query(
    pc_simulator_handle simulator,
    pc_failure_summary_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

void pc_simulator_destroy(pc_simulator_handle simulator);
```

The implemented declarations and result structs live in
`engine/include/poecraft/simulator.h`. Economy v1 uses chaos-equivalent prices
with basic operation keys, `essence:<metadata-key>`,
`fossil:<metadata-key>`, and `resonator:<socket-count>`.

Public structs should begin with `struct_size` and `abi_version` fields so newer callers and libraries can reject incompatible layouts cleanly.

`pc_error_info` should be caller-owned and contain fixed-size code/message storage rather than engine-owned string pointers.

Ownership and threading rules:

- data and session handles are immutable after construction and may be shared across threads
- action-context handles own random state, scratch masks, lazy tag-signature weights, and action-pool caches; one context belongs to one worker/thread at a time
- item state is caller-owned and must not be mutated concurrently
- simulator handles own an equivalent private action context and belong to one worker/thread at a time
- every created opaque handle has one matching null-safe destroy function
- child handles retain internal references to required parent data, so destroying the caller's data/session/strategy handle does not leave a live child dangling
- variable-length debug output uses a caller-provided buffer and a query-required-count pattern
- if a later API returns engine-allocated memory, it must provide a matching `pc_buffer_free`; do not free across runtimes with the caller's allocator
- failed mutating calls leave the input item unchanged; apply to a temporary copy and commit only on success
- file loading is a native convenience; WASM normally uses the memory-loading API
- strategy compilation is a cold-path engine operation so every binding uses the same validation and execution semantics
- economy handles contain immutable canonical operation costs and remain separate from crafting-rule data; a null economy handle disables market-cost statistics
- browser workers call bounded simulator chunks and yield between them for progress messages and cancellation
- simulation options set bounded retention counts for debug traces and representative success/failure items; the simulator never retains every trace from a large batch
- summaries expose cost status, while `pc_simulator_missing_price_query` returns the canonical missing-price keys so incomplete costs cannot be presented as complete

First action request shape:

```c
typedef enum {
    PC_ACTION_TRANSMUTE,
    PC_ACTION_AUGMENT,
    PC_ACTION_ALTERATION,
    PC_ACTION_REGAL,
    PC_ACTION_ALCHEMY,
    PC_ACTION_CHAOS,
    PC_ACTION_EXALT,
    PC_ACTION_ANNUL,
    PC_ACTION_SCOUR,
    PC_ACTION_ESSENCE,
    PC_ACTION_FOSSIL,
    PC_ACTION_BENCH,
    PC_ACTION_VEILED_CHAOS,
    PC_ACTION_VEILED_EXALT,
    PC_ACTION_UNVEIL,
    PC_ACTION_HARVEST_REFORGE,
    PC_ACTION_HARVEST_AUGMENT,
    PC_ACTION_HARVEST_RESIST,
    PC_ACTION_ELDRITCH_EMBER,
    PC_ACTION_ELDRITCH_ICHOR,
    PC_ACTION_ELDRITCH_EXALT,
    PC_ACTION_ELDRITCH_CHAOS,
    PC_ACTION_ELDRITCH_ANNUL,
    PC_ACTION_INFLUENCE_EXALT,
    PC_ACTION_FRACTURE
} pc_action_type;

typedef struct {
    uint32_t struct_size;
    uint32_t abi_version;
    pc_action_type action_type;
    const char* essence_key;
    uint32_t fossil_count;
    const char* fossil_keys[4];
    const char* mod_key;
    const char* target_tag;
    const char* source_tag;
    const char* influence;
    uint32_t tier;
} pc_action_request;
```

Pool debugging wraps the same action request in `pc_pool_query_request`, adding
the side filter and whether rejected session rows should be returned. This keeps
Python, WASM, UI diagnostics, and native execution on one action-parameter
shape. Direct actions such as bench, unveil, and implicit application expose
their selected rows through the last-action trace rather than pretending they
have a normal weighted explicit pool.

Random state belongs to `pc_action_context_handle` or the simulator's private action context. Do not use process-global random state or hide mutable random state in a shared session. Context options may accept an initial seed for tests/debugging, but exact seeded replay across platforms or engine versions is not a compatibility requirement.

The first Python binding uses the shared C ABI directly and keeps data, session,
and action-context ownership explicit. Its batch call maps to
`pc_apply_action_batch`, so Python does not cross the native boundary once per
item for validation or ML batches. It also exposes session mod resolution and
explicit test-item construction for fixture parity without duplicating pool
rules in Python. `scripts/package-python.ps1` produces a platform-tagged wheel
containing the shared library and required local runtime DLLs.

## First Implementation Slice

Build the first vertical slice in this order:

1. SQLite schema for bases, mods, weights, tags, stats, bench options, essences, fossils, and cluster-jewel records.
2. Python ingest for the complete crafting-relevant RePoE dataset, including preserved cluster-jewel records.
3. Complete compiled runtime artifact for every released base, the global mod catalog, and all normalized mechanic tables.
4. Native engine data loader.
5. Generic session builder for ordinary non-cluster bases at selected item levels.
6. `ItemState` fixed-slot implementation.
7. Normal explicit candidate mask and weight pool.
8. Chaos/alchemy/exalt actions.
9. Lean regression tests against spec fixtures.
10. Minimal workspace shell with real Emulator and Stash tabs, manual local saves, recovery, and the first item emulator panel.

This proves the entire architecture before adding every special mechanic.

Canonical SQLite and the compiled runtime artifact are both full-dataset. Vaal Regalia is only the detailed rule fixture. Cluster records are retained in the artifact, while cluster-jewel session creation returns an explicit unsupported-feature result until its passive-tag, notable-cap, and socket rules are implemented.

The native strategy simulator and editor can come after the emulator slice. Add real Simulator and Strategy Builder tabs with those implementations rather than building placeholder application surfaces that will be discarded.

## Frontend Recommendation

The best frontend for this project is:

```text
Vite + TypeScript + native Web Components
dockview-core for the workspace shell
```

Start without Lit. Build a small internal widget toolkit:

```text
pc-select
pc-combobox
pc-tabs
pc-modal
pc-tooltip
pc-mod-row
pc-mod-table
pc-strategy-board
pc-strategy-node
pc-condition-editor
pc-run-trace
pc-workspace
pc-stash
pc-simulator
```

Add Lit only if component boilerplate slows development. If added, keep it as a rendering helper for custom elements, not as an app framework.

Avoid React because:

- the UI is a simulator/tool, not a content app
- custom controls and data tables benefit from direct DOM ownership
- engine state is external and command-driven
- Web Components keep widgets portable and framework-independent

Avoid a large CSS/UI framework because the app needs domain-specific controls and dense layouts. Small helper packages are okay if they solve focused problems, but the core UI should stay ours.

Dockview is the focused exception: use it for tabs, split resizing, and layout persistence. Keep all domain controls and document contents custom.

## Resolved Decisions

Use:

```text
engine internals: C++20 with C ABI
first binding: Python
second binding: WebAssembly before the real web UI
first compiled data: complete JSON manifest + parallel arrays, then the same complete schema encoded as binary arrays
first UI debug views: yes, at least candidate pool and chosen mod details
first strategy conditions: always, has mod group, rarity, open prefix/suffix
```

## Invariants

- Ingest can be rich and slow; engine runtime must be compact and consistent across bindings.
- SQLite is canonical source data, not the hot runtime structure.
- Engine has no frontend dependency.
- Frontend has no crafting-rule authority; it asks the engine.
- Item state is small and copyable.
- Random state, scratch masks, lazy signature weights, and candidate pools are reusable per worker/thread through an action context.
- Item and strategy resources use stable IDs/schema versions from the first local implementation.
- Item and strategy content saves manually; layout persistence never saves domain content.
- The account backend is a later layer and does not block the local app.
- The first implementation should prove one vertical slice before broad mechanic coverage.
