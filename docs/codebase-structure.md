# Codebase Structure

## Purpose

This document defines the implementation shape before code begins. The goal is to keep the project split into clear layers:

```text
data ingest -> compiled data/session build -> deterministic engine -> bindings -> UI/tools
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
no React
```

This fits the app well. The simulator is a dense tool UI, not a media-heavy website. It needs good controls, tables, search, mod lists, item panels, and debug views. It does not need a component framework with a virtual DOM.

Use native custom elements for reusable widgets:

```text
<pc-select>
<pc-combobox>
<pc-mod-list>
<pc-item-panel>
<pc-craft-bar>
<pc-weight-table>
```

Use Shadow DOM selectively:

- yes for reusable low-level widgets where style isolation helps
- no for large app panels where global layout/theming is easier

Keep Lit optional. Do not start with it. If native custom elements become too verbose, add Lit later for component rendering only. Lit components are still standard custom elements, so that would not force a React-style app rewrite.

### Frontend State

Use a tiny explicit store instead of a framework store.

```text
AppState
  selectedBase
  selectedItemLevel
  currentItem
  selectedCraft
  lastActionResult
  debugPanels
```

Updates should be event-driven:

```text
component dispatches command event
app controller applies command
store updates
subscribed components render
```

Avoid hidden two-way binding. Custom widgets should dispatch events and receive state through properties.

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

Use three test layers:

```text
ingest tests
engine parity/unit tests
frontend widget tests
```

Engine tests are the most important. They should compare session masks, candidate pools, and weights against golden fixtures from the old implementation.

## Repository Layout

Target layout:

```text
poecraft2/
  README.md
  docs/
    architecture-plan.md
    codebase-structure.md
    data-shapes-and-ingest.md
    engine-bitsets.md
    item-state-flow.md
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
      fixtures/
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
          app-state.ts
          app-controller.ts
          engine-client.ts
        components/
          pc-app.ts
          pc-item-panel.ts
          pc-craft-bar.ts
          pc-mod-list.ts
          pc-select.ts
          pc-combobox.ts
          pc-weight-table.ts
        styles/
          tokens.css
          base.css
          layout.css
        widgets/
          listbox.ts
          popup.ts
          focus.ts

  fixtures/
    old-poecraft/
      session-pools/
      action-results/

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
- generate validation reports and golden fixture data

It may use rich Python objects because it is not performance-sensitive.

### `engine`

Responsibilities:

- load compiled data
- build sessions for selected base/item level/mechanic set
- own static masks, dense mod IDs, and weight arrays
- own compact `ItemState`
- apply deterministic craft actions
- return debug information for validation and UI

It should not parse RePoE directly and should not know about frontend DOM concepts.

### `bindings`

Responsibilities:

- expose the engine to Python for testing/ML
- expose the engine to WebAssembly for browser UI
- keep wrapper-specific memory management out of core engine logic

Python binding can come first for parity tests and ML experiments. WebAssembly can come once the first simulator UI exists.

### `apps/web`

Responsibilities:

- render the simulator
- provide custom widgets
- call a browser-safe engine client
- display item state, mod pools, weights, and action results

The web app should not reimplement mod pool rules. If it needs pool details, it asks the engine for debug data.

## Engine Public API Shape

Keep the first API small.

```c
pc_result pc_load_compiled_data(const char* manifest_path, pc_data_handle* out_data);
pc_result pc_create_session(pc_data_handle data, const pc_session_options* options, pc_session_handle* out_session);
pc_result pc_destroy_session(pc_session_handle session);

pc_result pc_item_init(pc_session_handle session, const pc_item_init_options* options, pc_item_state* out_item);
pc_result pc_apply_action(pc_session_handle session, pc_item_state* item, const pc_action_request* request, pc_action_result* out_result);
pc_result pc_get_debug_pool(pc_session_handle session, const pc_item_state* item, const pc_action_request* request, pc_debug_pool* out_pool);
```

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
    PC_ACTION_ESSENCE,
    PC_ACTION_FOSSIL
} pc_action_type;
```

Every action should accept a seed/RNG state or use the caller-owned RNG state in `pc_action_request`. Do not use global random state.

## First Implementation Slice

Build the first vertical slice in this order:

1. SQLite schema for bases, mods, weights, tags, stats, essences, fossils.
2. Python ingest for a small source subset.
3. Compiled data artifact for that subset.
4. Native engine data loader.
5. Session builder for one base/item level.
6. `ItemState` fixed-slot implementation.
7. Normal explicit candidate mask and weight pool.
8. Chaos/alchemy/exalt actions.
9. Golden tests against old `poeCraft`.
10. Minimal web UI with base selector, item panel, craft buttons, and debug pool.

This proves the entire architecture before adding every special mechanic.

## Frontend Recommendation

The best frontend for this project is:

```text
Vite + TypeScript + native Web Components
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
```

Add Lit only if component boilerplate slows development. If added, keep it as a rendering helper for custom elements, not as an app framework.

Avoid React because:

- the UI is a simulator/tool, not a content app
- custom controls and data tables benefit from direct DOM ownership
- engine state is external and command-driven
- Web Components keep widgets portable and framework-independent

Avoid a large CSS/UI framework because the app needs domain-specific controls and dense layouts. Small helper packages are okay if they solve focused problems, but the core UI should stay ours.

## Open Decisions

Before implementation starts, decide:

1. Whether engine internals are definitely C++20 or plain C.
2. Whether Python binding comes before WebAssembly.
3. Whether compiled engine data is binary-first or JSON-first for the first slice.
4. Whether UI debug views are required in the first web slice.

Recommended answers:

```text
engine internals: C++20 with C ABI
first binding: Python
first compiled data: JSON manifest + simple binary arrays, or JSON-only for the first tiny slice
first UI debug views: yes, at least candidate pool and chosen mod details
```

## Invariants

- Ingest can be rich and slow; engine runtime must be compact and deterministic.
- SQLite is canonical source data, not the hot runtime structure.
- Engine has no frontend dependency.
- Frontend has no crafting-rule authority; it asks the engine.
- Item state is small and copyable.
- Scratch masks and candidate pools are reusable per worker/thread.
- The first implementation should prove one vertical slice before broad mechanic coverage.
