# poecraft2

Path of Exile 1 crafting simulator and exact strategy planner. Python ingest
builds canonical SQLite data, a C++20 engine owns crafting behavior, Python and
WASM expose it, and a Vite + TypeScript + Web Components app provides the
client-side product.

Start with the [documentation map](docs/README.md), then read
[project direction](docs/direction.md). [HANDOFF](HANDOFF.md) names the current
implementation boundary when one exists.

## Quickstart

Build the native engine:

```powershell
powershell -File scripts/build.ps1
```

For focused native development, use the incremental Ninja wrapper:

```powershell
powershell -File scripts/dev-engine.ps1 -Task Engine
powershell -File scripts/dev-engine.ps1 -Task Test -Suite refinement
powershell -File scripts/dev-engine.ps1 -Task TestAll
powershell -File scripts/dev-engine.ps1 -Task BenchmarkValidate
```

`dev-engine.ps1` also supports `Configure`, `Tests`, `Benchmark`, `Full`,
`RerunFailed`, and the explicitly destructive `CleanRebuild` task. It uses the
checked-in UCRT64 GCC/Ninja Release preset and optional ccache when installed.
The canonical engine translation-unit inventory is
`engine/engine-sources.txt`.

Run the full acceptance pipeline:

```powershell
. ./scripts/python-common.ps1
$projectPython = Get-PoeCraftPython
& $projectPython.Command -m pip install -e './tools/ingest[test]'
powershell -File scripts/test.ps1 -FetchPinnedData
```

Set `POECRAFT_PYTHON` to a Python 3.11+ executable to select an interpreter.
The shared resolver otherwise resolves the local launcher/PATH once and binds
nested project scripts to that exact executable. Install dependencies with its
own `-m pip`; the optional `solver-lab` GUI extra remains separate. Emscripten's
SDK interpreter is independent.

The full pipeline prepares native/data prerequisites before pytest collects
ingest, economy and binding tests, then runs engine CTest, web tests and explicit
TypeScript checking. `-FetchPinnedData` provisions the existing frozen game
snapshot when needed, through the [pinned ingest route](docs/engine/data.md#frozen-validation-inputs).
It does not refresh economy prices. Unexpected existing data fails validation
and is preserved. Missing selected prerequisites fail instead of silently
skipping integration.

Use `-Scope Python`, `-Scope Native` or `-Scope Web` for the affected lane;
`-SkipBuild` explicitly reuses an existing native build. Other lanes are reported
as not selected. During development, prefer the changed layer and its downstream
consumers; run the appropriate complete suite once at the end of the selected plan.

Run the web app from `apps/web`:

```powershell
npm install
npm run dev
npm test
npx tsc --noEmit
```

Direct Python commands use the selected executable and the needed package roots:

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
