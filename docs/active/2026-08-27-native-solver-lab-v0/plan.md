# Native Solver Lab v0

**Status: proposed 2026-08-27; implementation not started.** Oliver requested
a usable GUI at the start of the Lab program and typed LLM controls. Approval
of this plan activates the boundary.

Parent: [Active work](../README.md)

Research basis: [Solver Research Architecture Audits](../../archive/2026-08-27-solver-research-audits/README.md)

## Outcome

Build a local Windows-native experiment workbench around the existing
`solver_corpus_runner.py` and `poecraft_solver_benchmark` execution path. The
first usable vertical slice must let Oliver submit a native solve from a thin
PySide6 GUI, watch its honest partial progress, close and reopen the GUI
without losing the job, and inspect the immutable attempt afterward. The same
typed operations must be available through a JSON CLI and a local MCP adapter
for LLM use.

The Lab is orchestration and measurement infrastructure. It does not become a
second implementation of crafting mechanics, solver policy, proof authority,
or exact evaluation.

## Locked Contracts

### Authority

- The existing native benchmark remains the only solve worker.
- One OS process owns one solve. The Lab never runs multiple solve handles in
  one worker process.
- The C++ engine remains the sole authority for case validity, mechanics,
  action legality, transition probabilities, Bellman values, proof bounds,
  strategy compilation, properness, and independent exact evaluation.
- Python may validate schemas, identities, paths, process outcomes, and
  resource reservations. It may not reproduce crafting rules.
- GUI, CLI, and MCP call one typed Python application-service layer. None owns
  a parallel queue or writes ad hoc solver state.
- The MCP surface is a closed tool vocabulary. It exposes no arbitrary shell,
  SQL, path write, mechanics override, or untyped benchmark-argument tool.

### Research profile

Add one explicit versioned Lab profile:

```text
native_allflame_no_imprint_v1
```

It binds:

- the exact resolved Allflame economy identity and price-table bits used by
  each attempt; no automatic economy refresh during an experiment;
- the native `calculator_product_v1` solver profile;
- generated automatic Imprint programs disabled;
- voluntary economic Restart disabled unless an explicit diagnostic profile
  says otherwise;
- mechanic-owned paid Fracture miss replacement retained even when voluntary
  Restart is disabled;
- goal-progress-gated reforges enabled;
- exact junk-free terminal success;
- otherwise complete Calculator action-family scope;
- native independent exact strategy evaluation enabled; and
- Simulator verification only when explicitly required, using 10,000 runs.

The profile is orchestration identity, not Python-owned mechanic logic. It
must resolve to existing engine/manifest controls and record their native
telemetry. Imprint code remains available to other callers and later explicit
profiles.

### Persistence and identity

- Store the Lab catalog separately from canonical game data, by default under
  ignored `build/solver-lab/catalog.sqlite3`.
- Store every attempt under an immutable attempt-specific directory below
  ignored `build/solver-lab/attempts/`.
- Experiments, resolved jobs, attempts, commands, leases, events, artifacts,
  and comparisons have versioned schemas.
- Retrying creates a new attempt. Raw attempt artifacts are never overwritten.
- Canonical job identity binds source commit and dirty-path manifest,
  executable hash, compiled-data identity, case document, economy, profile,
  action scope, solver caps, watchdog, measurement mode, and replicate.
- Paths are bindings; content hashes and canonical identity bytes are
  authority.
- Partial reports remain partial. Watchdog, cancellation, crash, OS OOM, and
  native resource stops remain distinct outcomes.

### Process and control behavior

- Reuse the existing process-group launch and no-survivor watchdog behavior.
- Host reservation is distinct from `max_solver_owned_bytes` and is never
  reported as solver proof state.
- Queue pause stops new dispatch only.
- Running cancel uses graceful request where available, then verified process-
  tree termination.
- Running pause/resume is explicitly unavailable in v0 because the current
  coarse checkpoint is not a faithful live scheduler continuation.
- A GUI restart, supervisor restart, or machine interruption cannot relabel an
  attempt complete. Startup reconciliation either resumes queue ownership or
  records an honest orphan/crash outcome.

## Product Surface

### Queue GUI

The first usable GUI surface shows:

- queued, blocked, running, canceling, canceled, failed, partial, and completed
  jobs;
- case, profile, priority, attempt, declared solver cap, host reservation,
  elapsed active time, phase, latest lower, latest verified upper, and stop;
- supervisor health and reserved/available host memory; and
- actions to submit, clone, change priority, pause/resume queue dispatch,
  cancel, retry, and open an attempt.

### Run Detail GUI

Show, without inventing authority:

- certified lower and its provenance;
- independently verified executable upper and exact evaluated cost;
- candidate estimates separately;
- absolute and multiplicative gap;
- phase, termination, cap owner, and bounded/exact status;
- states, rows, transitions, carriers, obligations, logical work, live/peak
  memory, and maximum cooperative slice;
- bounded bound trajectory, attempt events, logs, warnings, and artifacts; and
- explicit partial/crash/watchdog classification.

### Compare and Strategy GUI

Compare attempts by immutable request differences, deterministic work to bound
events, upper/lower trajectories, memory, action-family composition, graph
size, exact-evaluation status, and stop reason. Strategy summary shows nodes,
edges, exact cost, success/off-policy mass, properness, resource vector,
pricing completeness, and route failures. v0 does not render the full strategy
graph.

### Typed operations

GUI, CLI, and MCP expose the same application operations:

```text
list_profiles
list_cases
get_case
submit_job
submit_matrix
list_jobs
get_job
pause_queue
resume_queue
cancel_job
retry_job
clone_job
change_priority
get_run_summary
get_bound_trace
compare_runs
get_strategy_summary
evaluate_strategy
export_investigation_bundle
get_supervisor_status
```

Mutations require an idempotency key and support a dry-run response containing
the canonical operation and affected immutable identities. MCP setup is local
development configuration; no user-specific Codex configuration or secrets
are committed.

## Gates

### Gate 0 — Current-tree contract and dependency baseline

1. Record actual HEAD and dirty paths before implementation.
2. Read the current runner, benchmark, reporting, product-profile, and process-
   lifecycle contracts; do not re-read the entire archive.
3. Add optional Solver Lab dependencies rather than making PySide6 and the MCP
   SDK mandatory ingest dependencies. Pin compatible Python 3.11+ ranges and
   document one reproducible local installation command.
4. Define canonical schemas for profile, experiment, job, attempt, command,
   event, artifact, and operation results.
5. Freeze a small v0 corpus containing at least:

   - one fast exact three-prefix case;
   - one fast exact three-suffix case;
   - one bounded four-mod PDR case;
   - one non-armour four/five-goal control; and
   - one dirty/partial carrier control.

6. Run only a fast direct benchmark smoke and existing runner unit tests to
   pin the unwrapped behavior. Do not begin with a broad matrix.

Pass when schemas and profile identity are explicit, dependencies load in the
documented environment, and direct native execution is green.

### Gate 1 — Preserve and factor the existing worker substrate

Refactor `solver_corpus_runner.py` behind behavior-neutral typed components:

- command construction;
- resolved task and attempt paths;
- process launch/termination;
- partial/final result classification;
- provenance capture; and
- memory reservation metadata.

Retain the existing corpus-runner CLI, JSON ledger, watchdog behavior, exit
codes, and tests. The new Lab must call the factored adapter rather than fork a
second subprocess implementation.

Pass when the existing runner suite is unchanged semantically and direct-
versus-factored command/provenance fixtures are canonical-equal.

### Gate 2 — First usable persistent GUI vertical slice

Implement together, as one vertical slice:

- SQLite schema/migration and typed catalog repository;
- immutable experiment/job/attempt insertion;
- append-only commands and events;
- a single-worker supervisor using the Gate 1 adapter;
- a typed application-service layer;
- JSON CLI operations for submit/list/get/cancel; and
- PySide6 Queue and Run Detail windows using the same service.

The GUI must submit one small exact job, show queued/running/final state, poll
honest step-boundary partials, open logs/report/strategy metadata, close, and
reopen without losing history. It must not block the UI thread on a solve.

This is the first required local checkpoint. Pass only after nonvisual Qt
controller/widget tests and one native small-case end-to-end run pass. Oliver
owns rendered visual review.

### Gate 3 — Supervisor durability and resource control

Add:

- deterministic priority and enqueue ordering;
- bounded multi-process dispatch;
- conservative host-memory admission and exclusive oversize drain;
- leases, supervisor sessions, and process identity tokens;
- watchdog, graceful cancel, escalation, and no-survivor verification;
- retry, clone, and priority commands;
- startup recovery for stale leases and orphaned attempts;
- attempt-specific immutable artifact indexing and hashing; and
- durable SQLite transactions suitable for local power/process interruption.

Synthetic subprocess tests must cover crash, watchdog, cancellation, OS-like
OOM, stale lease, supervisor restart, memory refusal, oversize exclusive run,
and retry without artifact overwrite.

### Gate 4 — LLM controls and investigation bundles

Complete the stable JSON CLI and add a local stdio MCP adapter over the same
application service. Tool schemas must be finite and versioned. Every mutation
uses idempotency and dry-run support; read operations return bounded structured
results rather than dumping raw multi-megabyte telemetry.

`export_investigation_bundle` contains:

- request/profile/action-scope identity;
- terminal runner/native status;
- bound and milestone summary;
- dominant work and memory owners already present in native telemetry;
- policy and strategy summary;
- action-family composition;
- warnings and missing-price evidence; and
- immutable artifact IDs and hashes.

Pass when GUI, CLI, and MCP submission produce the same canonical job identity
and when malformed/stale/duplicate mutations fail safely without duplicate
attempts.

### Gate 5 — Compare, Strategy, and Matrix GUI

Add the remaining practical surfaces:

- paired/multi-attempt Compare;
- Strategy Summary;
- deterministic experiment-matrix editor and preview;
- filter/search for jobs and attempts;
- artifact export; and
- profile/action-scope disclosure, including the distinction between disabled
  voluntary Restart and retained paid Fracture miss replacement.

Matrix expansion is a deterministic canonical cross-product with include,
exclude, and replicate rules. Re-submission is idempotent unless the caller
explicitly requests new replicates.

Pass with controller/model tests and small end-to-end matrices. Oliver performs
the visual and usability review; automated agents do not claim rendered UI
acceptance.

### Gate 6 — Baseline freeze and behavior-neutral qualification

Using `native_allflame_no_imprint_v1`, run the small v0 corpus through both
the direct runner and Lab. Compare deterministic semantic projections:

- request/action/profile/economy identities;
- lower, upper, exact evaluated cost, and provenance;
- native status and termination;
- state/row/transition/logical-work counters;
- policy/strategy hashes when produced;
- exact success, properness, pricing, reconciliation, and off-policy mass; and
- honest partial/resource classifications.

Wall time, process IDs, paths, GUI polling times, and OS RSS are instrumentation
and need not be equal. Lab instrumentation overhead must be measured, bounded,
and reported.

The PDR case is a capped orchestration/partial-publication control, not a claim
of exact closure. Its repository-supported reference is upper
`7866.432124027084`, lower `21.772459401271156`, and a solver-owned memory
stop. Any fresh difference must be characterized by exact request/profile and
source identity before changing an expectation.

### Gate 7 — Final acceptance and handoff

Run once, after all selected gates are coherent:

- complete Solver Lab Python unit/integration suite;
- existing corpus-runner suite;
- nonvisual offscreen Qt tests;
- MCP schema/stdio integration tests;
- native small-corpus parity matrix;
- exact-strategy evaluation and required 10,000-run simulator controls;
- fresh native Release build if native files changed;
- the appropriate repository acceptance pipeline once;
- documentation/link validation; and
- `git diff --check`.

Document installation, launch, catalog recovery, profile identity, GUI usage,
CLI examples, MCP configuration, artifacts, statuses, limitations, and safe
shutdown. Archive the completed plan/result and update `HANDOFF.md`.

## Explicit Non-Goals

- scheduler-aware or first-strict-partition checkpoint/replay;
- running solve pause/resume;
- PDR proof-memory attribution or repair;
- verified option/subgoal behavior;
- RCASSP or another lower bound;
- solver carrier/action/obligation ordering changes;
- learned inference, training, GPU use, or model dependencies;
- automatic cap tuning or claims that more RAM fixes PDR;
- automatic Imprint programs;
- release-WASM or Calculator product redesign;
- full strategy-graph visualization;
- remote/multi-machine workers, authentication, or cloud execution; and
- arbitrary shell, SQL, or user-script execution from GUI/CLI/MCP.

## Stop Conditions

- Stop for an unresolved Path of Exile mechanic ruling; do not infer it in
  Python or from external research.
- Stop if the Lab changes deterministic native solver output or action scope
  for a supposedly identical job.
- Stop if GUI, CLI, and MCP cannot share one canonical operation/service layer.
- Stop if watchdog/cancel/recovery can leave an unowned worker or overwrite a
  prior attempt.
- Stop if partial/crashed work would need to be relabeled complete to proceed.
- Stop if MCP requires arbitrary execution authority or mechanics duplication.
- Stop the affected later gate, while preserving earlier qualified
  infrastructure, if a new optional dependency cannot be made reproducible.

Do not weaken identities, tests, exact-evaluation requirements, or process-
survivor checks to pass a gate.

## Long-Horizon Execution Discipline

When Oliver activates implementation, continue gate by gate without asking
for approval on ordinary engineering choices. Maintain
`execution-log.md` and `HANDOFF.md` with the actual current gate, decisions,
files, commands, failures, remaining work, and next executable step. Create
coherent local checkpoint commits with the required co-author line. Do not
push or perform public actions.

Run focused tests while developing and the appropriate complete acceptance
once at the end. If the full boundary cannot fit one session, finish the
deepest coherent passing gate, keep the repository buildable, and leave an
exact continuation point rather than an unqualified partial behavior change.

## Successor Tracks Preserved, Not Selected

After the Lab is stable, the research audits support separate future
boundaries for:

1. verified executable graph fragments as a shadow incumbent generator for
   better four/five-mod strategies;
2. fresh-run PDR proof-memory attribution, adding scheduler-aware replay only
   where identical-prefix continuation materially helps;
3. small action-specific retention/capacity proof patterns after a measured
   strict-obligation consumer is selected; and
4. deterministic scheduling baselines and feature logging before any learned
   guidance.

These tracks do not gain implementation authority from this plan.
