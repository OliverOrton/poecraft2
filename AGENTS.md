# poecraft2 — agent instructions

Path of Exile 1 crafting simulator: Python ingest → canonical SQLite →
compiled runtime artifact → native C++20 engine (C ABI) → Python/WASM
bindings → Vite + TypeScript + Web Components app (no React).

Start here, in order:

1. [docs/direction.md](docs/direction.md) — orientation; the doc map at its
   bottom links every design doc. This repo's docs are load-bearing specs.
2. [HANDOFF.md](HANDOFF.md) — where the current work stands, what's next,
   and the gotchas that cost previous sessions time. Keep it current: when
   you finish its work, rewrite it for the next session.

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
  engine C ABI or strategy-vocabulary changes, then run the web tests.
- SQLite (`data/sqlite/poecraft.db`) is canonical, the compiled artifact
  (`data/compiled/current`) is derived — never hand-edit either; recompile
  via `tools/ingest/compile_engine_data.py`.
- The frontend has no crafting-rule authority; it asks the engine. Don't
  reimplement pool/weight rules in TypeScript.

## Mechanic rules

Path of Exile mechanic questions (how a craft behaves, rules, edge cases)
are decided by Oliver. When a rule is ambiguous, ask him directly — do not
research online or guess.

## Conventions

- Single big commits per milestone are fine; drive each task to a gated,
  test-green state before committing.
- Commits are local-only unless Oliver says to push.
- End commit messages with your agent's co-author line.
