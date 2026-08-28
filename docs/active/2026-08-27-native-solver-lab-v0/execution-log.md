# Native Solver Lab v0 Execution Log

**Status: active; Gates 0-4 complete, Gate 5 next.**

Parent: [Plan](plan.md)

## Starting boundary

- Proposed from repository HEAD `769c3de` (`Record stopped PDR replay
  boundary`) and activated from clean checkpoint `bd86b46` (`Plan native
  solver lab v0`).
- Working tree was clean at activation.
- Current gate: Gate 5 — Compare, Strategy, and Matrix GUI.
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

Add GUI tabs for matrix submission, immutable attempt comparison, bounded bound
trajectories, and strategy/evaluation summaries over the Gate 4 service.

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

## Gate 3 result — 2026-08-27

- Catalog schema v2 migrates Gate 2 catalogs in place and adds supervisor
  sessions, active leases, queue controls, blocked/cancel state, native process
  identity, and release/recovery records.
- Dispatch order is deterministic by priority, enqueue timestamp, and job ID.
  The supervisor supports bounded native concurrency, explicit host-memory
  reservations, a conservative live-memory safety reserve, blocked admission,
  and exclusive drain/run behavior for a job larger than the concurrency
  budget. Host reservation remains orchestration metadata, never solver proof.
- Native processes now carry PID plus creation-time identity tokens. Live
  cancellation first requests the available process-group signal, then
  escalates to verified tree termination and records acknowledgment time/mode.
  Running pause/resume remains honestly unavailable.
- Startup reconciliation detects stale supervisor/lease heartbeats, checks
  process identity before termination, retains a valid partial observation,
  and records an orphaned attempt without relabeling it complete.
- Retry requeues the same immutable job identity but creates the next attempt
  ordinal and a fresh directory. Clone creates a new job. Priority changes are
  pre-dispatch only. Queue pause stops new dispatch and does not affect running
  work. All mutations are idempotent and support dry-run through the service.
- Final/partial reports, worker logs, and compiled strategies are indexed by
  attempt with content hash and byte size after completion. Prior attempt
  files are never overwritten.
- The Queue GUI gained live cancel, retry, clone, priority, pause/resume, and
  supervisor concurrency/reserved-memory status. The JSON CLI exposes the
  same controls plus bounded supervisor configuration.
- Synthetic coverage includes actual child-process crash and cancellation,
  the existing real watchdog/no-survivor test, distinct OS-like OOM,
  insufficient host memory, exclusive oversize drain, two-worker dispatch,
  live cancel, stale lease/supervisor restart, retry artifact preservation,
  priority/clone/pause controls, and v1-to-v2 catalog migration.
- Focused Gate 0-3 tests: 32 passed in 4.83 seconds. No native matrix or full
  acceptance pipeline was run.

## Gate 4 result — 2026-08-27

- Completed the stable JSON CLI vocabulary for profiles/cases, individual and
  matrix submission, durable job controls, run/bound/strategy reads,
  comparison, exact-evaluation disclosure, bundle export, and supervisor
  status.
- Added a local stdio MCP 2.1 adapter with 20 finite typed tools over the same
  application service. It exposes no arbitrary shell, SQL, write path,
  benchmark-argument bag, or mechanics override. All nine mutating tools
  require idempotency and expose dry-run.
- Read results are bounded: job/event limits are capped, bound traces are
  deterministically downsampled to at most 256 samples, strategy reads return
  census/action/pricing/terminal/route-failure summaries, and worker logs are
  available only as a bounded tail inside an investigation bundle.
- Native run summaries now include lower authority, independently evaluated
  upper, gaps, latest deterministic work, native owned memory, and the largest
  top-level timing owners without returning full multi-megabyte telemetry.
- `evaluate_strategy` discloses the independent native exact evaluation already
  required during every published-profile solve; v0 does not introduce a
  second evaluator backend.
- Investigation export writes only under ignored
  `build/solver-lab/bundles/<content-derived-id>/`. The bounded bundle includes
  request/profile/action-scope identity, terminal status, bound milestones,
  work/memory/timing owners, strategy/evaluation summary, hashed artifact
  index, events, bounded log tail, and exact reproduction argv.
- Matrix submission is bounded to 100 cases x 100 replicates, uses one durable
  experiment, and gives every child job its own derived idempotency identity.
- The MCP schema test pins the complete 20-tool vocabulary and verifies every
  mutation schema. The real Gate 2 exact suffix attempt was read successfully
  through the new CLI: 78 nodes / 219 edges, complete pricing, exact cost
  `1101.15648683309`, success mass 1, off-policy mass 0, and automatic Imprint
  false. Bundle dry-run produced no file.
- Focused Gate 0-4 tests: 35 passed in 7.11 seconds. No native matrix or full
  acceptance pipeline was run.
