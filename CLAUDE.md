# poecraft2

Path of Exile 1 crafting simulator: Python ingest → canonical SQLite →
compiled runtime artifact → native C++20 engine (C ABI) → Python/WASM
bindings → Vite + TypeScript + Web Components app (no React). Read
[docs/README.md](docs/README.md) first for the primary knowledge map, then
[docs/direction.md](docs/direction.md) for product orientation and
[HANDOFF.md](HANDOFF.md) for the current implementation boundary. If no
boundary is active, Oliver must choose the next chunk before implementation.

## Commands

- Full test pipeline: `powershell -File scripts/test.ps1` (ingest tests → DB
  validate → fixture parity → artifact compile/validate → binding tests →
  engine CTest → web smoke tests). Slow; prefer running only the changed
  layer and what's downstream of it.
- Engine build: `powershell -File scripts/build.ps1`
- Web dev server: `npm run dev` in `apps/web` (use the preview tools /
  launch.json config `web`)
- Python layers need `PYTHONPATH=tools/ingest;bindings/python`; invoke via
  `py -3`.

## Hard constraints

- The engine WASM module (`bindings/wasm/dist/poecraft_engine.mjs`) is
  committed and rebuildable: `scripts/build-wasm.ps1` self-activates the
  Emscripten SDK from `C:\emsdk` (emcc is not on PATH in a fresh shell).
  Rebuild it after engine C ABI or strategy-vocabulary changes, then run
  the web tests.
- SQLite (`data/sqlite/poecraft.db`) is canonical, the compiled artifact
  (`data/compiled/current`) is derived — never hand-edit either; recompile
  via `tools/ingest/compile_engine_data.py`.
- The frontend has no crafting-rule authority; it asks the engine. Don't
  reimplement pool/weight rules in TypeScript.

## Subagents — when to delegate

Project agents live in `.claude/agents/`. Delegate when the raw output would
swamp the conversation or the work is self-contained research; do the work
inline when it's a quick single-file check.

- [**test-pipeline**](.claude/agents/test-pipeline.md) — run the test suite (full or one layer) and get back
  only the pass/fail summary and failure excerpts. Use after changes to
  engine, ingest, bindings, or web instead of running `scripts/test.ps1`
  inline.
- [**data-auditor**](.claude/agents/data-auditor.md) — dataset health: manifest row counts, DB↔artifact
  consistency, fixture parity, staleness. Use when data looks wrong or
  after ingest changes.
- [**doc-drift**](.claude/agents/doc-drift.md) — read-only audit of `docs/*.md` against the code with
  file:line evidence. Use after landing a phase of work and before editing
  docs (this repo's docs are load-bearing design specs).
Independent delegations (e.g. doc-drift on two unrelated docs alongside
test-pipeline) can run in parallel.

## Mechanic rules

Path of Exile mechanic questions (how a craft behaves, rules, edge cases)
are decided by Oliver. When a rule is ambiguous, ask him directly — do not
research online or guess. Implemented behavior and unresolved rulings are
indexed from [docs/mechanics/README.md](docs/mechanics/README.md).
