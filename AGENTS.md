# poecraft2 — agent instructions

Path of Exile 1 crafting simulator: Python ingest → canonical SQLite →
compiled runtime artifact → native C++20 engine (C ABI) → Python/WASM
bindings → Vite + TypeScript + Web Components app (no React).

Start here, in order:

1. [docs/README.md](docs/README.md) — the primary knowledge map and document
   lifecycle policy. This repo's docs are load-bearing specs.
2. [docs/direction.md](docs/direction.md) — short product orientation and
   durable direction, not an execution plan.
3. [HANDOFF.md](HANDOFF.md) — the exact current implementation boundary when
   one exists. If it says no boundary is active, Oliver must choose the next
   chunk before implementation resumes. Keep it current at every handoff.

## Commands

- Full test pipeline: `powershell -File scripts/test.ps1` (ingest tests →
  DB validate → fixture parity → artifact compile/validate → binding tests
  → engine CTest → web tests). Slow; prefer running only the changed layer
  and what's downstream of it.
- Engine build: `powershell -File scripts/build.ps1`
- Web: `npm test` and `npx tsc --noEmit` in `apps/web` (tsx does not
  type-check); dev server `npm run dev` in `apps/web`.
- Python layers need `PYTHONPATH=tools/ingest;bindings/python`; invoke via
  `py -3`.

## Hard constraints

- The engine WASM module (`bindings/wasm/dist/poecraft_engine.mjs`) is
  rebuildable: `scripts/build-wasm.ps1` self-activates the Emscripten SDK
  from `C:\emsdk` (emcc is not on PATH in a fresh shell). Rebuild after
  engine C ABI or strategy-vocabulary changes. Run web tests only under the
  testing cadence below.
- SQLite (`data/sqlite/poecraft.db`) is canonical, the compiled artifact
  (`data/compiled/current`) is derived — never hand-edit either; recompile
  via `tools/ingest/compile_engine_data.py`.
- The frontend has no crafting-rule authority; it asks the engine. Don't
  reimplement pool/weight rules in TypeScript.

## Mechanic rules

Path of Exile mechanic questions (how a craft behaves, rules, edge cases)
are decided by Oliver. When a rule is ambiguous, ask him directly — do not
research online or guess. Implemented behavior and unresolved rulings are
indexed from [docs/mechanics/README.md](docs/mechanics/README.md).

## Conventions

- Single big commits per milestone are fine. Intermediate plan phases are
  implementation checkpoints, not test gates.
- Do not run routine test suites at the end of each intermediate phase. Run a
  narrowly relevant test only when something is broken and the test is needed
  to diagnose or fix it. Run the appropriate complete acceptance suite once at
  the end of the full plan, unless Oliver explicitly asks for an earlier run.
- Oliver owns rendered and visual UI review. Do not perform browser visual
  checks, screenshots, or rendered UI smoke unless Oliver explicitly asks.
  Non-visual automated web tests follow the same testing cadence above.
- Compiled-strategy verification uses 10,000 simulator runs whenever
  verification is required, unless Oliver explicitly changes that count.
- Commits are local-only unless Oliver says to push.
- End commit messages with your agent's co-author line.
