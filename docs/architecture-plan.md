# Path of Exile Crafting Simulator Architecture Plan

## Goals

1. Build a fast Path of Exile 1 crafting simulator suitable for public use.
2. Run public simulations entirely client-side to avoid per-user hosting cost.
3. Preserve a shared simulation substrate that can later support ML strategy search and training.

The project should prioritize the simulator core first. ML strategy suggestion is a downstream consumer of the engine, not a dependency for the first usable version.

This project targets Path of Exile 1. Path of Exile 2 mechanics and data are out of scope unless a separate design is added later.

## System Shape

The project is split into three systems that share one engine/data model where applicable:

- Public web app: local-first, runs the native engine compiled to WebAssembly, and supports guests without accounts.
- Community backend: added later for accounts, sync, publishing, discovery, and social features.
- Private training infrastructure: local/private coordinator plus workers, runs the same native engine compiled for local machines.

Simulation systems consume the same generated engine data artifacts so behavior stays aligned between browser use, native testing, and later ML training.

## Public Web App

The public app should be a static TypeScript frontend with a WebAssembly engine module.

- UI: Vite + TypeScript with native Web Components/custom elements.
- Workspace shell: `dockview-core` for IDE-style tabs, docking, splits, resizing, and layout restoration.
- Engine: native engine compiled to WebAssembly.
- Hosting: Cloudflare Pages, GitHub Pages, Netlify, or similar static hosting.
- Guest storage: IndexedDB for manually saved items/strategies, settings, and export/import.
- Account storage: later backend sync; login remains optional.
- Initial public backend: none for the first local simulator milestone.

The WASM API should be coarse-grained. JavaScript should request batches such as "run this strategy N times" or "evaluate these actions from this state" rather than calling into WASM for every individual crafting step.

Long simulations should run in Web Workers with cancellation and progress reporting so the UI stays responsive.

The public app should expose two main crafting workflows:

- Emulator: user performs crafting operations one by one against a live item.
- Strategy simulator: user builds or loads a strategy graph and runs one or many simulations.

The strategy editor should use a Blueprint-style visual graph. Operation nodes mutate the item, and guarded edges decide the next state based on item/simulation conditions. The graph should compile down to deterministic simulator semantics similar to the old `Step` runner rather than becoming a general-purpose visual programming language. See [strategy-editor-ui.md](strategy-editor-ui.md).

Emulator, Simulator, Strategy Builder, and Stash should live inside a desktop-like workspace. Users can open multiple items/strategies in tab groups and resizable splits. Item and strategy content saves manually; layout restoration is only a UI preference. See [desktop-workspace-ui.md](desktop-workspace-ui.md).

Saved resources should use stable IDs/schema versions from the beginning. Account login and Stash sync may be added after the local app is stable. Public publishing waits until the intended launch mechanics are implemented, native/Python/WASM behavior is validated, and representative browser simulation workloads pass the performance/readiness gate. See [accounts-publishing-and-discovery.md](accounts-publishing-and-discovery.md).

## Engine

The engine is a single native codebase with C++20 internals and a C ABI boundary. It has two build targets:

- Native build for local development, tests, tools, and private workers.
- WASM build for the public browser app.

Important engine properties:

- Engine-owned random state with no global process RNG.
- No undefined behavior reliance.
- Shared crafting rules across native and WASM builds.
- Coarse batch-oriented API.
- Data structures designed for hot simulation loops.
- Small regression tests for core crafting interactions.

The strategy simulator is part of the native engine, not a second TypeScript or Python interpreter. The visual editor produces a compiled strategy graph, and native, Python, and WASM callers all invoke the same simulator implementation.

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
- model version, once ML exists

## Runtime Data Loading

The authoritative definitions of mod rows, tag channels, groups, families, session universes, and action candidate sets are in [mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md). This architecture document uses those terms without redefining them.

At startup, load the complete compiled runtime artifact into engine memory. It contains all released bases, the normalized global mod catalog, ordered weight/tag/stat rows, and special-mechanic lookup tables. For a crafting session, select the relevant subset:

- session-reachable mod rows
- exclusivity groups and UI-family metadata
- base tags, ordered weight-selector tags, classification tags, and added tags as separate channels
- ordered spawn/generation-weight rows and action-specific weight tables
- tier/stat metadata
- item-level restrictions
- domain/generation metadata
- influence, fractured, synthesized, eldritch, essence, fossil, harvest, bench, metamod, and cannot-roll rule context as needed

For any supported ordinary non-cluster base and selected item level, include all normal and influence-specific mods that the supported actions can reach. Influence changes then select another tag signature and mask instead of rebuilding the session. Sessions remain immutable after construction. Per-worker action contexts build uncommon tag-signature weight arrays and action-pool caches lazily, so shared sessions require no cache mutation or synchronization. Recombinators may use a dedicated two-item session because their universe depends on both input items and possible output bases.

The engine should generate action-specific weighted candidate pools from this item-local universe and cache them in the worker-local action context for repeated use. The session identity already fixes base and item level. Within a session, cache keys should include every mutable/contextual component that can affect legality or weighting, such as effective tag signature, influence state, metamods, fossil constraints, open affix sides, and blocked exclusivity groups.

UI-only data such as display names, icons, descriptions, localization, and change history can stay outside the engine and be lazy-loaded by the frontend.

## Session Mod Indexing

Each crafting session should build a compact session-local mod universe. Mods should be assigned dense integer IDs from `0..N-1`, where `N` is the number of mods relevant to the selected item context. This keeps filtering and random selection cache-friendly and avoids repeatedly scanning global mod tables.

The session mod store should use structure-of-arrays layout for hot fields:

- affix type
- generation type
- domain
- required item level
- ordered spawn/generation-weight rows
- classification tags and added tags as separate channels
- exclusivity group
- spawn weights
- tier/range data
- influence and special-mechanic flags
- prefix/suffix occupancy behavior

The engine should precompute bitset indexes over the dense session IDs:

- all prefix mods
- all suffix mods
- mods by classification tag
- mods referenced by spawn-weight selector tag
- mods referenced by generation-weight selector tag
- mods by generation type
- mods by domain
- mods by influence or special mechanic
- mods by exclusivity group
- mods affected by fossil, essence, harvest, bench, metamod, or cannot-roll constraints

Item level is baked into session construction. Action filtering should mostly be bitset algebra using the action-specific formula:

```text
candidate_pool =
    action_universe_mask
    & open_affix_side_mask
    & positive_weight_mask_for_this_action
    & influence_allowed_mask
    & mechanic_allowed_mask
    & ~blocked_group_mask
    & ~metamod_block_mask
```

The exact masks and weight rule differ by action. Normal explicit rolls use spawn × generation. Harvest-targeted draws use classification-tag membership and spawn weight only while still respecting metamods. If both prefix and suffix are legal, sample one combined weighted distribution rather than choosing a side 50/50.

After legality filtering, the engine can build or reuse a weighted selection table for that pool. For small pools a simple cumulative weight array may be enough. For hot repeated pools, an alias table or cached prefix-sum table can avoid rebuilding selection data during large simulation batches.

Action-pool cache keys should be explicit and conservative. A key should include the action type plus every item/context feature that can change either legality or weights:

- effective tag signature
- influence state
- eldritch state
- fractured/synthesized state
- existing exclusivity groups
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
- Native/Python/WASM smoke checks against the same rule fixtures and result shapes.
- Lightweight data diff reports after source data refreshes.
- Targeted edge-case tests only when implementing a tricky mechanic or fixing a bug.

The simulator's credibility depends on users trusting the crafting rules, but the test suite should stay practical. Avoid exhaustive compatibility matrices and broad coverage goals unless a specific mechanic needs them.

## ML Strategy Suggestion

ML is intentionally deferred until the simulator core is stable. Before any
learned model, an exact dynamic-programming solver computes optimal
strategies from the engine's known transition probabilities and doubles as
the training-data generator and evaluation baseline for later ML work. See
[crafting-solver-plan.md](crafting-solver-plan.md).

The research-backed target architecture is specified in
[ml-strategy-planning.md](ml-strategy-planning.md). The model should guide a
symbolic, cyclic stochastic planner and a separate Strategy Builder graph
compiler rather than directly replacing mechanics or emitting unverified graph
JSON.

Likely approaches:

- Monte Carlo strategy evaluation.
- Search over action graphs using the engine as the environment.
- Learned policy/value model trained from generated trajectories.
- Hybrid search guided by a learned model.

Initial ML work can use a static economy baseline. League economy inputs can be added later if strategy recommendations need to become price-sensitive per league.

The public app should eventually ship a bundled model or lightweight strategy data so inference remains client-side.

## Community Backend

The community backend is a later phase and should not block the local simulator.

Suggested shape:

- TypeScript/Node HTTP API.
- PostgreSQL for users, saved resources, immutable strategy versions, publications, statistics, and social relationships.
- Path of Exile OAuth 2.1 login with a separate admin-only fallback.
- Optional account sync; guests remain fully supported locally.
- Browser-generated 100,000-run statistics for initial publishing.
- Public/private/unlisted visibility.
- Profiles, favorites, forks, follows, and ratings first.
- Comments, reports, and moderation later.

Publishing freezes a strategy version. Published costs remain tied to the economy snapshot used for the run. Unpublishing removes public access without breaking fork attribution/version history.

Publishing is not an early engine milestone. It begins only after the complete intended public mechanic set, cross-target validation, public data packaging, and browser throughput/cancellation work are release-ready.

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

This architecture no longer owns sequencing. The original vertical slice,
native/WASM engine, workspace, strategy simulator/editor, mechanic expansion,
public-engine throughput pass, and solver S1-S6 are complete. Accounts remain
deferred and the active milestone is solver S7 depth/performance. See
[implementation-plan.md](implementation-plan.md) for portfolio status and
[HANDOFF](../HANDOFF.md) for the sole current next-work pointer.

## Open Questions

- Exact generated binary format for engine-hot data.
- Which mechanics should be redesigned rather than inherited from the previous app.
- Release process for league data updates.
