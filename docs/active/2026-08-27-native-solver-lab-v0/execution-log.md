# Native Solver Lab v0 Execution Log

**Status: active; Gates 0-2 complete, Gate 3 next.**

Parent: [Plan](plan.md)

## Starting boundary

- Proposed from repository HEAD `769c3de` (`Record stopped PDR replay
  boundary`) and activated from clean checkpoint `bd86b46` (`Plan native
  solver lab v0`).
- Working tree was clean at activation.
- Current gate: Gate 3 — supervisor durability and resource control.
- Gate 0 added only versioned contracts, the frozen profile/corpus, and
  optional dependencies. No catalog, supervisor, GUI, CLI, or MCP service
  implementation exists yet.

## Decisions already made

- First boundary is Native Solver Lab v0 only.
- GUI is part of the first usable vertical slice, not a later polish project.
- JSON CLI and local MCP use the same typed service as the GUI.
- The existing native benchmark and engine remain the only solve/mechanics
  authority.
- Scheduler replay, solver behavior, proof work, options, RCASSP, learned
  guidance, Imprint, and release-WASM changes are excluded.

## Next executable step

Add bounded multi-process dispatch, deterministic priority/memory admission,
live cancellation, leases/recovery, retry/clone/priority operations, and
attempt artifact hashing to the working Gate 2 vertical slice.

## Gate 0 result — 2026-08-27

- Activation checkpoint: `d893db9` (`Activate native solver lab v0`), with no
  pre-existing dirty paths.
- Existing corpus-runner tests before implementation: 9 passed.
- Direct native smoke:
  `conquest-lamellar-allflame-clean-3-prefix-extended-product8` completed in
  77.092 seconds, closed exact at `1618.2138946963837`, matched independent
  exact strategy evaluation, and compiled 154 nodes / 432 edges. Simulator
  verification was intentionally skipped.
- Optional dependency group:
  `py -3 -m pip install -e "tools/ingest[solver-lab]"` installed and imported
  PySide6 6.11.2 and MCP 2.1.1 on Python 3.14. Compatible project ranges are
  PySide6 `>=6.11,<7` and MCP `>=2.1,<3`; project Python remains `>=3.11`.
- Versioned profile: `native_allflame_no_imprint_v1`. It binds the fixed
  Allflame identity, native `calculator_product_v1`, automatic Imprint off,
  voluntary economic Restart off, native paid Fracture miss replacement,
  goal-progress gating, exact junk-free success, and exact strategy
  evaluation. Python validates bindings but owns no mechanic rule.
- Frozen five-case Lab corpus includes exact three-prefix and three-suffix
  controls, bounded four-mod PDR, non-armour four-goal Bow, and partial
  four-to-five carrier roles.
- Canonical JSON identity plus explicit v1 profile, experiment, job, attempt,
  command, event, artifact, and operation-result schemas landed.
- Focused post-change tests: 13 passed. No broad matrix or full acceptance
  pipeline was run.

## Gate 1 result — 2026-08-27

- Added one shared typed native-worker adapter for command construction,
  attempt paths, process-group execution, final/partial classification,
  provenance capture, and host-only memory reservation disclosure.
- The existing corpus runner still owns the same CLI, v2 ledger, watchdog,
  exit-code behavior, memory admission, completion resume rule, and legacy
  output layout.
- Retries in the Lab can now use immutable attempt-local report, partial,
  strategy, and log paths without creating another subprocess implementation.
- Canonical fixtures pin exact benchmark argv, command identity, provenance
  resume shape, native expectation-miss classification, attempt isolation,
  and memory authority.
- Focused Gate 1 + Gate 0 tests: 18 passed in 0.89 seconds. No native matrix or
  full acceptance pipeline was run.

## Gate 2 result — 2026-08-27

- Added the WAL-backed SQLite catalog with migrations and immutable
  experiments, jobs, attempts, commands, events, and artifact schema.
- Added one typed application service over the frozen profile/corpus. Canonical
  job identity binds source dirty manifest, executable, compiled artifact,
  corpus, case bytes, Allflame economy, native profile/action scope, caps,
  watchdog, measurement mode, and replicate.
- Added idempotent/dry-run submission, durable queued cancellation, case/job
  discovery, and bounded run summaries to the JSON CLI.
- Added a nonblocking single-worker supervisor using the Gate 1 adapter and
  immutable attempt-specific report, partial, strategy, and log paths.
- Added the first PySide6 Queue / Run Detail GUI. It submits jobs, polls durable
  queued/running/final state and native step-boundary partials, shows bound
  provenance/work/memory/artifact metadata, cancels queued work, and reopens
  catalog history. Oliver still owns rendered visual review.
- The first real dispatch failed honestly in 0.283 seconds because the new
  aggregate manifest omitted the benchmark-required `explicit_imprint_scope`.
  The failed attempt and log were retained. The manifest/profile now bind that
  field explicitly; a fresh job and attempt were created rather than
  overwriting evidence.
- Real corrected native run:
  `conquest-lamellar-allflame-clean-3-suffix-product8` completed in 49.869
  seconds (native total 49.496 seconds), closed exact at
  `1101.15648683309`, matched independent exact evaluation, retained 49 bound
  samples, and compiled 78 nodes / 219 edges. No sampled Simulator verification
  was selected.
- Nonvisual Qt, service, catalog, supervisor, contract, and legacy runner
  tests: 23 passed in 1.93 seconds. No broad matrix or full acceptance pipeline
  was run.
