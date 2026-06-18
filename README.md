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
session construction, the full crafted/essence/base-implicit/fossil-direct
session universe, materialized group/classification/influence masks, lazy
influence tag-signature weights, cached prefix-sum weighted pools, rich
request-shaped pool debugging, and the initial normal/essence/fossil actions.
Phase 7 provides a shared native library, owning Python wrappers, exact spec
fixture parity checks, explicit test-item construction, a coarse native
batch-action API, bounded batch/handle stress coverage, and a platform wheel
build. Standard fossil multipliers stack in fixed point; Sanctified Fossil
remains explicitly unsupported until its special level/lucky behavior is
implemented.

Python binding smoke example:

```powershell
$env:PYTHONPATH = "$PWD/bindings/python"
py -3 -c "from poecraft_engine import load_data; d=load_data('data/compiled/current'); s=d.create_session('Metadata/Items/Armours/BodyArmours/BodyInt17', 86); c=s.create_action_context(); print(c.run_batch(s.create_item(), {'type':'alchemy'}, 1000).summary)"
```

Build a self-contained Python wheel containing the native library:

```powershell
.\scripts\package-python.ps1
```

This repo is intended to house a native crafting simulation engine, a browser-based public simulator, and later ML tooling for crafting strategy suggestions.

Start with:

- [docs/accounts-publishing-and-discovery.md](docs/accounts-publishing-and-discovery.md)
- [docs/architecture-plan.md](docs/architecture-plan.md)
- [docs/codebase-structure.md](docs/codebase-structure.md)
- [docs/data-shapes-and-ingest.md](docs/data-shapes-and-ingest.md)
- [docs/desktop-workspace-ui.md](docs/desktop-workspace-ui.md)
- [docs/engine-bitsets.md](docs/engine-bitsets.md)
- [docs/implementation-plan.md](docs/implementation-plan.md)
- [docs/item-state-flow.md](docs/item-state-flow.md)
- [docs/mod-data-and-pool-semantics.md](docs/mod-data-and-pool-semantics.md)
- [docs/strategy-editor-ui.md](docs/strategy-editor-ui.md)
- [docs/weight-calculation-flow.md](docs/weight-calculation-flow.md)
