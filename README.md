# poecraft2

Path of Exile 1 crafting simulator and strategy research project.

## Phase 0-1 commands

The canonical data pipeline uses Python 3.11+ and the current RePoE fork
snapshot:

```powershell
./scripts/build.ps1
./scripts/test.ps1

$env:PYTHONPATH = "$PWD/tools/ingest"
py -3 -m poecraft_ingest.cli refresh --force
py -3 -m poecraft_ingest.cli validate
py -3 -m poecraft_ingest.cli query-pool --base "Vaal Regalia" --item-level 86
```

`refresh` freezes source files under `data/raw/repoe`, builds
`data/sqlite/poecraft.db`, and writes a validation report. Generated source and
database artifacts are intentionally ignored by Git.

Cluster-jewel definitions, passives, notables, and applicable mod rows are
preserved in SQLite. Cluster session/pool construction is explicitly
unsupported until its later runtime phase.

The Phase 1 ordinary-base smoke query uses Vaal Regalia at item level 86.

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
