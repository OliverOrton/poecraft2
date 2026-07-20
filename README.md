# poecraft2

Path of Exile 1 crafting simulator and exact strategy planner. Python ingest
builds canonical SQLite data, a C++20 engine owns crafting behavior, Python and
WASM expose it, and a Vite + TypeScript + Web Components app provides the
client-side product.

Start with the [documentation map](docs/README.md), then read
[project direction](docs/direction.md). [HANDOFF](HANDOFF.md) names the current
implementation boundary when one exists; there is no active chunk at present.

## Quickstart

Build the native engine:

```powershell
powershell -File scripts/build.ps1
```

Run the full acceptance pipeline:

```powershell
powershell -File scripts/test.ps1
```

The full pipeline covers ingest, database validation, fixture parity, artifact
compile/validation, bindings, engine CTest, and web tests. During development,
prefer the changed layer and its downstream consumers; run the appropriate
complete suite once at the end of the selected plan.

Run the web app from `apps/web`:

```powershell
npm install
npm run dev
npm test
npx tsc --noEmit
```

Python layers use `py -3` with both package roots when required:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
```

## Data And Runtime Rules

- `data/sqlite/poecraft.db` is canonical. Do not hand-edit it.
- `data/compiled/current` is derived. Rebuild it with
  `tools/ingest/compile_engine_data.py`.
- `bindings/wasm/dist/poecraft_engine.mjs` is rebuildable with
  `scripts/build-wasm.ps1`, which activates Emscripten from `C:\emsdk`.
- The native engine owns pool, weight, transition, and mechanic rules. The
  frontend consumes engine answers rather than duplicating that authority.

## Reference Map

- [Architecture and ownership](docs/foundation/README.md)
- [Implemented mechanics](docs/mechanics/README.md)
- [Engine internals and WASM](docs/engine/README.md)
- [Solver architecture](docs/solver/README.md)
- [Product surfaces](docs/product/README.md)
- [Economy data and operations](docs/economy/README.md)
- [Deferred designs](docs/future/README.md)
- [Historical plans and evidence](docs/archive/README.md)

See [AGENTS.md](AGENTS.md) or [CLAUDE.md](CLAUDE.md) for contributor-specific
operating constraints.
