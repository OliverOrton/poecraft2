# Change Impact Map

**Status: stable repository process reference.** This page maps implementation
changes to downstream code, generated artifacts, documentation, and final
verification. It does not select work or replace the testing cadence in
`AGENTS.md`.

Parent: [Foundation](README.md)

Verified against repository: 2026-07-20 @ 8f6ea61. Scope: build/test scripts,
native CMake targets, public headers, Python/WASM bindings, web protocol and
package scripts, data/economy schemas and tooling, checked workflows, and
documentation lifecycle rules. No build or test suite was run.

## Purpose

poecraft2 has several deliberate derivation and binding boundaries. A local
edit can compile in one layer while leaving a committed WASM module, a parser,
a type union, a fixture, or a stable reference stale. Use this map before
implementation to identify the complete change surface, then use it again at
acceptance to confirm that every required downstream artifact was refreshed.

This document answers “what else changes if I change this?” It does not decide
Path of Exile behavior. Any ambiguous mechanic rule still requires Oliver's
ruling before implementation or documentation.

## Dependency Overview

Game data:

```text
source snapshot and normalization
  -> schemas/sqlite + canonical poecraft.db
  -> compile_engine_data.py
  -> data/compiled/current
  -> native file loader or web poecraft-data.json bundle
  -> immutable data/session handles
```

Runtime and product:

```text
engine/src + engine/include/poecraft
  -> native static/shared libraries and C ABI
  -> Python ctypes subset
  -> WASM JSON facade + generated poecraft_engine.mjs/.wasm
  -> EngineBindings -> engine-worker -> EngineClient
  -> Emulator / Calculator / Strategy Builder / Simulator / Stash
```

Economy:

```text
provider adapter + economy schema/catalogs
  -> canonical economy SQLite
  -> immutable league snapshots + league index
  -> browser cache, overrides, and fallback
  -> pinned economy JSON
  -> native solve/evaluation/simulation pricing
```

## Rules Before Editing

1. Identify the authority. Mechanics belong to
   [Mechanics](../mechanics/README.md); runtime representation belongs to
   [Engine](../engine/README.md); solver contracts belong to
   [Solver](../solver/README.md); product behavior belongs to
   [Product](../product/README.md); and prices belong to
   [Economy](../economy/README.md).
2. Distinguish canonical input from derived output. Never hand-edit canonical
   SQLite or the compiled runtime artifact; regenerate both through their
   owning tools. Generated WASM is rebuildable output but is tracked and must
   match the engine source used by the web product.
3. Check every exposed surface. Python wraps a subset of the C ABI; WASM wraps
   a different, broader product subset. Update a binding only when it exposes
   the changed contract, but never assume a native change is invisible to WASM
   merely because the facade function name stayed the same.
4. Preserve authority boundaries. TypeScript may validate request shape and
   present native results, but it must not acquire pool, weight, transition, or
   mechanic authority.
5. Plan one final acceptance gate. Intermediate implementation phases are not
   routine test gates. Use a narrow test only to diagnose a failure, then run
   the appropriate complete suite once after the selected work is complete.

## Change Matrix

| Change | Authority to edit first | Downstream surfaces to inspect or update | Derived output / final verification |
| --- | --- | --- | --- |
| Documentation only | Owning stable area, note, future page, decision, or archive | Area README, parent/back-links, verification stamp, glossary/evidence if terms or results changed | One-off Markdown link/reachability audit; no product suite merely for prose |
| RePoE source normalization or ingest behavior | `tools/ingest/poecraft_ingest/`, ingest fixtures | Canonical schema assumptions, validation reports, compiled-data projection, engine loader/session fixtures, engine/data docs | Regenerate canonical SQLite and compiled artifact through tools; ingest tests, DB validation, fixture parity, then downstream binding/engine/web checks as affected |
| Canonical game-data schema | `schemas/sqlite/001_initial.sql`, writer and validation code | Queries, normalizers, compiled-data serializer, fixture validators, economy joins using game data, engine data loader | Rebuild SQLite and `data/compiled/current`; validate both before engine consumers |
| Compiled runtime artifact shape | `tools/ingest/poecraft_ingest/compiled_data.py`, compiler/validator | `engine/src/data_loader.cpp`, public summaries if exposed, web data-bundle builder, fixture and loader tests, `docs/engine/data.md` | Recompile/validate artifact; rebuild web data bundle through its npm pre-script; run loader and downstream consumer acceptance |
| Internal native engine behavior with unchanged ABI | Owning `engine/src/*.cpp` implementation and private header | Native tests, mechanic/engine/solver stable reference, WASM behavior, Python behavior if its exposed call reaches the changed code | Native build; rebuild WASM before browser acceptance whenever browser semantics changed; run affected native/binding/web layers at final gate |
| Public C ABI symbol, struct, enum, or lifetime | `engine/include/poecraft/*.h` plus owning `engine/src` facade | Header smoke, ABI validation, every exposing binding, native call sites/benchmarks, WASM facade/export list, web protocol/types, stable docs | Native build and binding tests; mandatory WASM rebuild for browser-exposed changes; web/WASM acceptance after rebuild |
| Primitive action enum or mechanic request shape | `session.h`, `engine_internal.hpp`, native parser/action implementation after Oliver's ruling | Simulator parser, solver registry/exact evaluator, Bestiary boundary if relevant, WASM request parser, `CraftAction`, product pickers/panels, mechanic coverage matrix | Native build, mandatory WASM rebuild, complete changed-layer acceptance; update the owning mechanic page and `mechanics/README.md` completeness table |
| Mechanic legality or transition behavior | Owning action/session/pool implementation after a recorded Oliver ruling | Exact calculation, sampled action path, strategy simulator, solver operators, Python/WASM exposure, all relevant product surfaces and fixtures | Validate native/exact/sampled parity as appropriate; rebuild WASM; record ruling provenance and explicit unsupported boundaries |
| Strategy JSON operation or condition vocabulary | `engine/src/simulator.cpp` and public simulator contract; `strategy-model.ts` for authored shape | Solver compiler, exact graph evaluator, Python strategy compiler/evaluator, WASM parser, worker protocol, Strategy Builder authoring/validation, persistence and fixtures | Native build, mandatory WASM rebuild, binding/web acceptance; update `docs/product/strategies.md`, solver flow/boundaries, and mechanics vocabulary when applicable |
| Solver goal, option, result, cap, or telemetry contract | `engine/include/poecraft/solver.h`, `solver_internal.hpp`, and owning solver implementation | `solver_api.cpp`, WASM facade, `engine-wasm.ts`, `engine-protocol.ts`, worker/client, Calculator/solve helpers, native and web fixtures, solver docs | Native build and solver tests; rebuild WASM; run Node worker/WASM and product-model acceptance at final gate |
| Solver algorithm with stable request/output | Owning `solver_*.cpp` files | Exact transition assumptions, policy compiler/evaluator, telemetry/evidence, native benchmark corpus, browser work-step behavior and caps | Native build; use focused diagnostics only when needed; rebuild WASM before web acceptance; run required 10,000-run compiled-strategy verification only when the selected acceptance plan requires it |
| WASM facade, exports, memory, or marshalling | `bindings/wasm/wasm_api.cpp`, `scripts/build-wasm.ps1` | `engine-wasm.ts`, worker/client protocol, generated release wrapper, engine smoke test, `docs/engine/wasm.md` | Mandatory `scripts/build-wasm.ps1`; inspect generated `.mjs/.wasm` diff; run web/WASM acceptance at final gate |
| Python binding exposure | `bindings/python/poecraft_engine/_binding.py` and high-level package API | ctypes signatures/structs, owning C ABI lifetime, package data/build, binding tests, foundation capability statement | Native shared-library build, Python binding tests, package smoke/build when release scope requires it |
| Worker RPC or structured-clone type | `engine-protocol.ts`, `engine-worker.ts`, `engine-client.ts`, `engine-wasm.ts` as applicable | Every component caller, progress/cancel handling, Node worker bootstrap and smoke tests, solver/WASM flow docs | Typecheck plus web/WASM tests at final gate; rebuild WASM only if native/facade code also changed |
| Workspace persistence or saved strategy/item shape | `workspace/persistence.ts`, strategy/item models | Draft recovery, Stash records, handoffs, legacy parsing/migration, affected components and model tests, product docs | Web typecheck/tests at final gate; rendered review belongs to Oliver and runs only when explicitly requested |
| Economy schema, provider, price catalog, or snapshot envelope | `schemas/economy/`, `tools/economy/`, checked catalogs/fixtures | Refresh workflow, publication/retention/checkpoint code, browser economy service, native economy parser and solver/simulator pricing if envelope changed, economy docs | Economy tests and validation/publish checks; native/WASM rebuild only when their parsed envelope or pricing contract changed |
| Checked workflow or packaging | `.github/workflows/`, `scripts/package-*`, build scripts | Required secrets/resources, artifact inputs, cache headers, output manifests, deployment docs | Exercise the narrow packaging/workflow validation available locally; full product tests only when implementation inputs changed |

## Rebuild Triggers

### Native engine

Run `powershell -File scripts/build.ps1` after native source/header changes.
The script compiles the ingest and economy Python packages, regenerates the
Harvest allowlist header, discovers the installed Visual Studio CMake/Ninja
tools even when they are absent from `PATH`, then configures/builds the checked
UCRT64 GCC Release preset. CMake builds the object library, static/shared
engines, tests, and solver benchmark. A prominent direct-g++ fallback remains
for portability, but it recompiles all sources and is not the development path.

Native and WASM source discovery has one owner:
`engine/engine-sources.txt`. CMake, `scripts/build.ps1`, and
`scripts/build-wasm.ps1` consume that inventory and reject missing, duplicate,
or unlisted translation units. Add, move, or remove an engine translation unit
there once. `scripts/dev-engine.ps1` exposes incremental engine-only,
tests-only, benchmark-only, selected-suite, parallel native CTest,
benchmark-validation, rerun-failed, and explicit clean-rebuild workflows.

### Compiled game data

`data/sqlite/poecraft.db` is canonical and `data/compiled/current` is derived.
Use `tools/ingest/compile_engine_data.py` to compile and validate the artifact;
never edit either output manually. Changes to the artifact shape must be
coordinated with the native loader and `scripts/build-data-bundle.mjs` used by
the web build.

### WASM

`scripts/build-wasm.ps1` compiles every `engine/src/*.cpp` file plus
`bindings/wasm/wasm_api.cpp` and rewrites the tracked release module. Rebuild
before web acceptance whenever native behavior visible in the browser changes.
It is mandatory after engine C ABI or strategy-vocabulary changes. A fresh
shell does not have `emcc` on `PATH`; the script activates the SDK from
`$env:EMSDK` or `C:\emsdk`.

After adding or renaming a public facade export, check all three inventories:

- `EMSCRIPTEN_KEEPALIVE` declarations in `wasm_api.cpp`;
- `$Exported` in `scripts/build-wasm.ps1`; and
- calls/bindings in `engine-wasm.ts` plus the generated release wrapper.

### Web and public artifacts

The web `predev` and `prebuild` hooks run `npm run build:data`. `npm run build`
type-checks before Vite builds; `npm test` uses `tsx` and therefore does not
replace `npx tsc --noEmit`. Public packaging requires current native/WASM,
compiled-data, web, and economy inputs; use
`scripts/package-public-artifacts.mjs` only when packaging is in the selected
scope.

## Verification Selection

The full pipeline is `powershell -File scripts/test.ps1`, in this order:

1. ingest unit tests;
2. economy unit tests;
3. canonical database validation when the local database exists;
4. spec fixture parity;
5. compiled artifact compile and validation;
6. Python binding tests;
7. native CTest or fallback engine tests and solver-corpus validation; and
8. web/WASM tests when npm and the generated module are available.

Use that complete pipeline when the finished change crosses most layers. For a
narrow change, run the changed layer and everything downstream that consumes
its contract. Do not run routine suites after every intermediate checkpoint.

Additional acceptance rules from `AGENTS.md` remain binding:

- rebuild release WASM before web tests when its source inputs changed;
- run both `npm test` and `npx tsc --noEmit` for a completed web change;
- compiled-strategy verification uses 10,000 simulator runs when verification
  is required unless Oliver explicitly changes the count; and
- Oliver owns rendered/visual UI review, so agents do not perform it unless he
  explicitly asks.

Documentation-only changes require a proportional one-off Markdown link and
reachability audit, not the product pipeline.

## Documentation Consequences

| Change introduces or alters | Documentation destination |
| --- | --- |
| Implemented mechanic behavior or surface coverage | Owning `docs/mechanics/*.md` family and coverage index |
| Stable runtime/solver/product/economy contract | Owning area reference with a fresh verification stamp |
| Cross-layer flow or lifetime | Existing flow page; avoid duplicating the same lifecycle in several areas |
| Durable owner-approved engineering choice | `docs/decisions.md`, clearly labelled implemented or deferred |
| Measurement, fixture, target, pass, miss, or stopped gate | `docs/evidence.md` and linked raw evidence |
| Unselected possibility | `docs/future/` or area `NOTES.md`; never stable current sequencing |
| Completed execution plan | Extract durable knowledge, then move it with final evidence to a dated archive |

If a document describes code-dependent behavior, verify the relevant paths and
update its date/commit/scope stamp. If the code was not checked, mark the claim
unverified rather than copying a stale plan assertion.

## Worked Change Traces

### Add a primitive action kind

1. Obtain and record Oliver's mechanic ruling.
2. Update the public/internal enum and native action implementation.
3. Update request/simulator parsers, exact calculation, solver registry, WASM
   parsing, TypeScript action union, and each intended product surface.
4. Update the mechanic family page and complete-coverage matrix.
5. Build native, rebuild WASM, and run the appropriate final downstream suite.

### Add a solver telemetry field

1. Update native telemetry ownership/serialization and its cap accounting.
2. Update the WASM JSON facade, TypeScript protocol type, and any product
   presentation or diagnostic parser.
3. Update native/API fixtures and worker/WASM checks.
4. Build native, rebuild WASM, and run the final solver plus web/WASM acceptance
   selected for the chunk.

### Restructure documentation only

1. Preserve authority and history; update area indexes and parent links.
2. Move completed plans only after extracting decisions, evidence, glossary
   terms, stable contracts, deferred work, and open notes.
3. Run the one-off Markdown target, area-index, and reachability audit.
4. Do not run engine, ingest, binding, or web tests merely because prose moved.
