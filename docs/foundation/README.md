# Foundation

**Status: stable architecture and layer-ownership reference.** This document
describes the implemented system. It does not select future work.

Parent: [Documentation](../README.md)

Verified against code: 2026-07-19 @ d5e38e3

## Purpose

poecraft2 is a Path of Exile 1 crafting simulator and exact one-item planning
system. A Python ingest pipeline produces canonical data, a native C++20 engine
owns crafting behavior, and a Vite/TypeScript application runs that engine as
WebAssembly in a worker. The frontend does not reproduce crafting rules.

The implemented flow is:

```text
source data
  -> canonical SQLite
  -> compiled JSON runtime artifact
  -> immutable engine data and sessions
  -> native C ABI
  -> Python binding or WASM JSON facade
  -> browser worker and Web Components UI
```

The engine is shared by the Emulator, compiled-strategy Simulator, exact graph
Calculator, and dynamic-programming Solver. The browser application runs
locally; no account or publishing backend is present.

## Layer Ownership

### Canonical data and compilation

`tools/ingest/` reads normalized source data, writes
`data/sqlite/poecraft.db`, validates it, and compiles
`data/compiled/current/`. SQLite is canonical. The compiled artifact is derived
and must be regenerated rather than edited.

The current runtime artifact is JSON, not a binary blob: `manifest.json`,
`strings.json`, and `game-data.json`. Native callers load the sibling files;
the web build packages them into one `poecraft-data.json` document for the
engine's memory-loading path. See [Engine data](../engine/data.md).

### Native engine

`engine/` contains C++20 implementation code and C-compatible public headers.
It owns:

- compiled-data validation and loading;
- immutable base/item-level sessions with dense session-local mod IDs;
- item representation, weighted candidate pools, random state, and actions;
- Bestiary compound state;
- strategy compilation and sampled execution;
- exact action and strategy evaluation;
- solver registry, abstraction, transition construction, solve, policy
  compilation, accounting, and telemetry.

The public boundary is `engine/include/poecraft/*.h`. Public calls return
`pc_result` and write caller-owned `pc_error_info`; exceptions do not cross the
C ABI. Native CMake builds an object library, static engine, shared engine,
tests, and the opt-in solver benchmark.

### Bindings

`bindings/python/` uses the shared C ABI through `ctypes`. Its high-level
binding covers data, sessions, item/action work, Bestiary, strategy evaluation,
and simulation. It does not currently wrap the solver C ABI.

`bindings/wasm/wasm_api.cpp` provides a coarse JSON facade over the same native
engine. It owns integer handle registries so TypeScript never marshals native
struct layouts directly. The release module and its browser use are documented
in [WASM](../engine/wasm.md).

### Web product

`apps/web/` is Vite + TypeScript + native Web Components; there is no React
layer. `dockview-core` provides the docked workspace. A process-wide service
creates one engine worker and loads one data handle per browser tab. UI panels
communicate with the worker through `EngineClient` and structured-cloneable
messages.

The implemented workspace includes Emulator, Calculator, Strategy
Builder/Simulator, and Stash documents. IndexedDB owns local saved resources
and recovery state; local storage owns layout and settings. Economy snapshots
and overrides remain separate from mechanic data.

### Economy

`tools/economy/`, `schemas/economy/`, `data/economy/`, and the web economy
service own league snapshots, canonical price keys, cache/fallback behavior,
and immutable price snapshots passed to the engine. Prices can change action
costs and solver selection; they do not change crafting legality or outcomes.

## Ownership and Lifetime

- Loaded data and sessions are immutable after construction and may be shared
  across threads.
- An action context owns mutable RNG, scratch storage, lazy weight tables, and
  candidate-pool caches; it belongs to one thread or worker at a time.
- Item state is caller-owned in the C ABI. The WASM facade stores item values
  behind worker-local integer handles.
- Strategy, economy, simulator, solver, and exact-evaluation handles each have
  a matching null-safe destroy/close path.
- Child engine handles retain the immutable parent state they need, so closing
  the caller's parent handle does not dangle a live child.
- Random state is never process-global. A seed is accepted for repeatable
  diagnostics, but replay across targets or engine versions is not a public
  compatibility guarantee.

## Authority Boundaries

- [Mechanics](../mechanics/README.md) owns implemented action behavior and
  dated Oliver rulings.
- [Engine](../engine/README.md) owns runtime representation and execution
  contracts.
- [Solver](../solver/README.md) owns the exact planning abstraction and output
  contract.
- [Product](../product/README.md) owns workspace and UI behavior.
- [Economy](../economy/README.md) owns price identity and snapshot behavior.
- [Decisions](../decisions.md) owns durable engineering decisions.
- [Future](../future/README.md) owns deferred proposals. They are not current
  implementation commitments.
- [Archive](../archive/README.md) preserves completed plans and evidence with
  no current sequencing authority.

## Invariants

- Path of Exile 1 is the only current game target.
- SQLite is canonical; compiled runtime data is derived.
- The engine has no frontend dependency.
- The frontend has no crafting-rule authority.
- Native, Python, and WASM paths meet at the C ABI, but a capability is not
  assumed available through every high-level binding unless that binding
  exposes it.
- Sessions are selected by stable base metadata path and item level; artifact
  integer IDs are not persistent product identities.
- Ordinary non-cluster sessions are implemented. Cluster sessions and
  two-item recombination remain explicit future work.
- Current history and deferred designs stay outside stable references.

## Current Entry Points

| Concern | Entry point |
|---|---|
| Repository orientation | `docs/README.md`, `docs/direction.md` |
| Data schema and compilation | `schemas/sqlite/001_initial.sql`, `tools/ingest/poecraft_ingest/compiled_data.py` |
| C ABI | `engine/include/poecraft/` |
| Native source inventory and build | `engine/engine-sources.txt`, `engine/CMakeLists.txt`, `engine/CMakePresets.json`, `scripts/dev-engine.ps1`, `scripts/build.ps1` |
| Native solver phases and private source ownership | [Solver internals](solver-internals.md) |
| WASM build | `scripts/dev-wasm.ps1` (incremental), `scripts/build-wasm.ps1` (direct release fallback) |
| WASM facade | `bindings/wasm/wasm_api.cpp` |
| Browser boundary | `apps/web/src/app/engine-client.ts`, `engine-worker.ts`, `engine-wasm.ts` |
| Product shell | `apps/web/src/app/components/pc-app.ts`, `pc-workspace.ts` |

Commands and testing cadence are maintained in the repository `AGENTS.md`.
Before changing a cross-layer contract, use the
[Change Impact Map](change-impact.md) to identify downstream bindings,
generated artifacts, documentation, and final verification.
