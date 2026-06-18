# Path of Exile Crafting Simulator Architecture Plan

## Goals

1. Build a fast, deterministic Path of Exile crafting simulator suitable for public use.
2. Run public simulations entirely client-side to avoid per-user hosting cost.
3. Preserve a shared simulation substrate that can later support ML strategy search and training.

The project should prioritize the simulator core first. ML strategy suggestion is a downstream consumer of the engine, not a dependency for the first usable version.

## System Shape

The project is split into two systems that share one engine and one data pipeline:

- Public web app: static-hosted, no accounts, runs the native engine compiled to WebAssembly.
- Private training infrastructure: local/private coordinator plus workers, runs the same native engine compiled for local machines.

Both systems consume the same generated engine data artifacts so simulation behavior stays aligned between browser use, native testing, and later ML training.

## Public Web App

The public app should be a static TypeScript frontend with a WebAssembly engine module.

- UI: Vite + TypeScript with native Web Components/custom elements.
- Workspace shell: `dockview-core` for IDE-style tabs, docking, splits, resizing, and layout restoration.
- Engine: native engine compiled to WebAssembly.
- Hosting: Cloudflare Pages, GitHub Pages, Netlify, or similar static hosting.
- User storage: IndexedDB for saved items, crafting history, settings, and optional export/import.
- Public backend: none for the initial simulator.

The WASM API should be coarse-grained. JavaScript should request batches such as "run this strategy N times" or "evaluate these actions from this state" rather than calling into WASM for every individual crafting step.

Long simulations should run in Web Workers with cancellation and progress reporting so the UI stays responsive.

The public app should expose two main crafting workflows:

- Emulator: user performs crafting operations one by one against a live item.
- Strategy simulator: user builds or loads a strategy graph and runs one or many attempts.

The strategy editor should use a Blueprint-style visual graph. Operation nodes mutate the item, and guarded edges decide the next state based on item/simulation conditions. The graph should compile down to deterministic simulator semantics similar to the old `Step` runner rather than becoming a general-purpose visual programming language. See [strategy-editor-ui.md](strategy-editor-ui.md).

Both workflows should live inside a desktop-like workspace. Users can open multiple item, strategy, result, and trace documents; arrange them in tab groups and resizable splits; and restore their workspace later. Saved resources and workspace layouts should use IndexedDB. See [desktop-workspace-ui.md](desktop-workspace-ui.md).

## Engine

The engine is a single native codebase with C++20 internals and a C ABI boundary. It has two build targets:

- Native build for local development, tests, tools, and private workers.
- WASM build for the public browser app.

Important engine properties:

- Deterministic seeded RNG.
- No undefined behavior reliance.
- Stable behavior across native and WASM builds.
- Coarse batch-oriented API.
- Data structures designed for hot simulation loops.
- Small regression tests for core crafting interactions and seeded action sequences.

The initial engine work should emphasize correctness and inspectability over ML-readiness.

## Data Pipeline

SQLite should be treated as a canonical source and build artifact, not as the simulation hot path.

Pipeline:

```text
raw PoE data / old project data
        ↓
canonical SQLite database
        ↓
compiled engine data blob
        ↓
native engine structures
        ↓
native and WASM simulation
```

The SQLite database is useful because the source data is relational and easier to inspect, diff, and regenerate. The engine should load generated C-friendly data structures for fast simulation.

Each generated artifact should carry version metadata:

- engine version
- source data version
- schema version
- generated data hash
- RNG version
- model version, once ML exists

## Runtime Data Loading

For a crafting session, load the relevant item data into engine memory:

- applicable mods for the item
- mod groups
- tags
- weights
- tiers
- item-level restrictions
- domain/generation metadata
- influence, fractured, synthesized, eldritch, essence, fossil, harvest, bench, metamod, and cannot-roll rule context as needed

The engine should generate action-specific mod pools from this item-local universe and cache them for repeated use. Cache keys should include every state component that can affect legality or weighting, such as item level, tags, influence state, metamods, fossil constraints, and blocked prefix/suffix rules.

UI-only data such as display names, icons, descriptions, localization, and change history can stay outside the engine and be lazy-loaded by the frontend.

## Session Mod Indexing

Each crafting session should build a compact session-local mod universe. Mods should be assigned dense integer IDs from `0..N-1`, where `N` is the number of mods relevant to the selected item context. This keeps filtering and random selection cache-friendly and avoids repeatedly scanning global mod tables.

The session mod store should use structure-of-arrays layout for hot fields:

- affix type
- generation type
- domain
- required item level
- tags
- mod group
- spawn weights
- tier/range data
- influence and special-mechanic flags
- prefix/suffix occupancy behavior

The engine should precompute bitset indexes over the dense session IDs:

- all prefix mods
- all suffix mods
- mods by tag
- mods by generation type
- mods by domain
- mods by influence or special mechanic
- mods by item-level threshold
- mods by mod group
- mods affected by fossil, essence, harvest, bench, metamod, or cannot-roll constraints

Action pool filtering should mostly be bitset algebra:

```text
candidate_pool =
    action_base_mask
    & affix_availability_mask
    & item_level_mask
    & required_tag_mask
    & mechanic_allowed_mask
    & ~blocked_group_mask
    & ~cannot_roll_mask
```

After legality filtering, the engine can build or reuse a weighted selection table for that pool. For small pools a simple cumulative weight array may be enough. For hot repeated pools, an alias table or cached prefix-sum table can avoid rebuilding selection data during large simulation batches.

Action pool cache keys should be explicit and conservative. A key should include the action type plus every item/session feature that can change either legality or weights:

- item level
- base tags
- influence state
- eldritch state
- fractured/synthesized state
- existing mod groups
- prefix/suffix occupancy
- metamod state
- fossil and resonator state
- essence/harvest/bench action parameters
- cannot-roll and forced-tag constraints

The first implementation should favor correctness with conservative invalidation. Once behavior is validated, profiling can identify which masks and weighted tables are worth caching more aggressively.

## Validation Strategy

The old implementation can be used as a design reference while reading mechanics, but it is not a compatibility target. Validation should be lean and based on the new engine's documented rules, hand-inspected examples, invariants, and a small set of deterministic regression fixtures.

Validation layers:

- Smoke tests for ingest, session creation, and core actions.
- A small number of spec fixtures for important crafting interactions.
- Seeded native/Python/WASM replay checks for covered action sequences.
- Lightweight data diff reports after source data refreshes.
- Targeted edge-case tests only when implementing a tricky mechanic or fixing a bug.

The simulator's credibility depends on users trusting the crafting rules, but the test suite should stay practical. Avoid exhaustive compatibility matrices and broad coverage goals unless a specific mechanic needs them.

## ML Strategy Suggestion

ML is intentionally deferred until the simulator core is stable.

Likely approaches:

- Monte Carlo strategy evaluation.
- Search over action graphs using the engine as the environment.
- Learned policy/value model trained from generated trajectories.
- Hybrid search guided by a learned model.

Initial ML work can use a static economy baseline. League economy inputs can be added later if strategy recommendations need to become price-sensitive per league.

The public app should eventually ship a bundled model or lightweight strategy data so inference remains client-side.

## Private Training Infrastructure

The private training system is for personal/local machines.

Suggested shape:

- Coordinator on a household machine.
- Postgres for job metadata, summaries, and model registry.
- File storage for large trajectory/model artifacts.
- Native engine workers.
- Thin Python wrapper or standalone worker binary.
- Tailscale for private networking if machines need to coordinate across the network.

Postgres should not be the bulk trajectory store. Use files for large simulation outputs and keep Postgres as the catalog.

Because this infrastructure is private and household-scoped, simple auth is acceptable initially. If collaborators or off-network machines join later, worker tokens and scoped credentials should be added.

## Implementation Phases

1. Repo and project scaffolding.
2. Data pipeline prototype from old project data into canonical SQLite.
3. Generated engine data blob format.
4. Native engine port with lean deterministic tests.
5. WASM build and TypeScript integration.
6. Public emulator UI for one-action crafting flows.
7. Validation expansion against documented rules and known examples.
8. Strategy graph editor and simulator trace UI.
9. Baseline strategy search / Monte Carlo evaluator.
10. Private training coordinator and worker loop.
11. Client-side ML or strategy suggestion prototype.

## Open Questions

- Exact generated binary format for engine-hot data.
- First supported crafting mechanics and item classes.
- First strategy graph condition/operator subset.
- Which mechanics should be redesigned rather than inherited from the previous app.
- Static economy baseline format for future strategy evaluation.
- Release process for league data updates.
