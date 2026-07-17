# poecraft2

Path of Exile 1 crafting simulator and strategy research project.

## Build and test

The canonical data pipeline uses Python 3.11+ and the current RePoE fork
snapshot:

```powershell
./scripts/build.ps1
./scripts/test.ps1

$env:PYTHONPATH = "$PWD/tools/ingest"
py -3 -m poecraft_ingest.cli refresh --force
py -3 -m poecraft_ingest.cli validate
py -3 -m poecraft_ingest.cli query-pool --base "Vaal Regalia" --item-level 86
py -3 tools/ingest/validate_spec_fixtures.py
py -3 tools/ingest/compile_engine_data.py compile `
  --database data/sqlite/poecraft.db `
  --output data/compiled/current
py -3 tools/ingest/compile_engine_data.py validate `
  --database data/sqlite/poecraft.db `
  --artifact data/compiled/current
```

`refresh` freezes source files under `data/raw/repoe`, builds
`data/sqlite/poecraft.db`, and writes a validation report. Generated source and
database artifacts are intentionally ignored by Git.

Cluster-jewel definitions, passives, notables, and applicable mod rows are
preserved in SQLite. Cluster session/pool construction is explicitly
unsupported until its later runtime phase.

The Phase 1 ordinary-base smoke query uses Vaal Regalia at item level 86.

Phase 2 fixtures under `fixtures/spec` pin the 212-row session universe,
normal prefix/suffix weights, combined alchemy/chaos side weighting, and one
fractured-reforge invariant. They are checked against the canonical database
and intentionally do not use old-app serialized data or RNG sequences.

Phase 3 produces `data/compiled/current/{manifest,game-data,strings}.json`.
It is a complete parallel-array runtime artifact containing every released
base, the global normalized mod catalog, ordered relationships, and mechanic
lookup tables. Vaal Regalia remains only the detailed Phase 2 rule fixture.

Phases 4-6 now provide the native C ABI, compact item state, generic ordinary
session construction, materialized group/classification/influence masks, lazy
influence tag-signature weights, cached prefix-sum weighted pools, rich
request-shaped pool debugging, and the initial normal/essence/fossil actions.
Phase 7 provides a shared native library, owning Python wrappers, exact spec
fixture parity checks, explicit test-item construction, a coarse native
batch-action API, bounded batch/handle stress coverage, and a platform wheel
build.

Phase 8 provides the WebAssembly facade, worker runtime, progress/cancellation
protocol, and headless cross-target checks. Phase 9 provides the Dockview
workspace, real WASM-backed Emulator slice, Stash/manual-save model, and
recovery persistence. Phase 10 provides the native compiled strategy graph
simulator, immutable economy snapshots, run-wide limits, retained traces and
representative items, aggregated failures, and matching C/Python/WASM access.

Phase 13 is complete. Ordinary sessions now include the direct mechanic
registries needed for bench/metamods, veiled and unveiled modifiers, corrupted
implicits, and Eldritch implicits. The shared action API implements bench
crafting, metamod locks and cannot-roll filters, veiled exalt/chaos and unveil,
Harvest reforge/augment/resistance conversion, Eldritch implicits and currency,
generic influenced exalts, and current special fossil behavior. Sanctified
required-level weighting, Bloodstained/Gilded implicits, and mirrored
Fractured-Fossil output are covered across native, Python, and WASM.
Fracturing Orb is also implemented across the shared action vocabulary, while
the Emulator supports exact-mod fracture editing from item-row right-clicks
and the condition builder supports fractured-family requirements.

Phase 14 is complete. The benchmark harness records native and worker/WASM
session, pool/cache, action, strategy, progress, and memory measurements. The
measured hot-path optimization removes repeated common tag-signature work,
skips a redundant combined-affix mask operation for both-side pools, and reuses
influence-mask storage. Detailed timing probes are opt-in so normal simulations
do not call the clock inside candidate and sampling loops. The benchmark matrix
separates one- and ten-action Alteration and Chaos graphs; the Strategy Builder
uses adaptive worker chunks and frame-throttled progress rendering, and shows
the measured actions per second after each run.
`scripts/package-public-artifacts.mjs` creates a content-addressed,
hash-verified bundle for the native/WASM engines, compiled data, production UI,
and economy snapshot. Phase 12 accounts and standalone Phase 18 recombinators
remain deferred; Phase 15 publishing is not started.

Python binding smoke example:

```powershell
$env:PYTHONPATH = "$PWD/bindings/python"
py -3 -c "from poecraft_engine import load_data; d=load_data('data/compiled/current'); s=d.create_session('Metadata/Items/Armours/BodyArmours/BodyInt17', 86); c=s.create_action_context(); print(c.run_batch(s.create_item(), {'type':'alchemy'}, 1000).summary)"
```

The Python binding also exposes `Session.compile_strategy`,
`load_economy`, and `Strategy.create_simulator`; all graph execution remains in
the native engine.

Build a self-contained Python wheel containing the native library:

```powershell
.\scripts\package-python.ps1
```

Run the Phase 14 benchmarks and package immutable public-engine artifacts:

```powershell
.\scripts\benchmark.ps1
Push-Location apps/web
npm run build
Pop-Location
node .\scripts\package-public-artifacts.mjs
```

This repo is intended to house a native crafting simulation engine, a browser-based public simulator, and later ML tooling for crafting strategy suggestions.

Start with:

- [docs/direction.md](docs/direction.md) — one-page orientation and doc map
- [HANDOFF.md](HANDOFF.md) — exact current implementation boundary
- [docs/README.md](docs/README.md) — documentation lifecycle and subject index
- [docs/active/bestiary-and-solver-capability-plan.md](docs/active/bestiary-and-solver-capability-plan.md) — active execution plan
- [docs/implementation-plan.md](docs/implementation-plan.md)
- [docs/archive/README.md](docs/archive/README.md) — completed execution plans
