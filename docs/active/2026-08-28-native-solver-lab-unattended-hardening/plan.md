# Native Solver Lab Unattended Execution and Identity Hardening

**Status: selected; implementation has not begun.** Selected by Oliver on
2026-08-28 from the completed GUI-stabilization source checkpoint
`978b200e8d7993a49ee7991f303cc0823f60914b`.

Parent: [Active work](../README.md)

Progress and exact evidence belong in the [execution log](execution-log.md).

## Objective

Make the local Native Solver Lab safe to operate unattended through its typed
MCP surface. A queued job must execute only the immutable source, executable,
artifact, corpus, case/revision, profile, economy, action scope, caps, and host
controls it recorded. Attempt completion must be crash-atomic with hashed
artifacts, recovery must distinguish a finished worker from an unresolved
possible-live orphan, and repeated commands must be idempotent only when their
complete canonical requests are identical.

This is orchestration and evidence hardening. The native benchmark remains the
only solve worker and all crafting, search, proof, compilation, and evaluation
authority stays native-owned.

## Mandatory Fresh-Task Startup Gate

Do not implement this plan in the task that created it. Restart Codex or open a
new Codex task so the configured `poecraft2-native-solver-lab` MCP server is
actually loaded into that task.

Before editing anything, the fresh task must:

1. Read, in order:

   - `AGENTS.md`;
   - `docs/README.md`;
   - `docs/direction.md`;
   - `HANDOFF.md`;
   - this plan; and
   - this boundary's `execution-log.md`.

2. Verify the source boundary and worktree:

   ```powershell
   git status --short
   git rev-parse HEAD
   git rev-parse HEAD^
   git show --stat --oneline --decorate HEAD
   ```

   The tree must be clean. `HEAD^` must be exactly
   `978b200e8d7993a49ee7991f303cc0823f60914b`; `HEAD` must be the single
   documentation-only planning commit titled
   `Activate solver lab unattended hardening`. Record the full `HEAD` in the
   execution log before implementation. Stop if the parent, subject, changed
   paths, or clean-tree condition differs; do not reset, clean, restore, or
   discard anything to manufacture the expected state.

3. Confirm the configured `poecraft2-native-solver-lab` MCP tools are present
   in the new task. If they are absent, inspect the user-local registration
   with `codex mcp get poecraft2-native-solver-lab`, restart Codex/open another
   task, and check again. Do not start implementation in a task that cannot
   call the registered server.

4. Use MCP—not ad hoc SQLite or filesystem inspection—to record the operator
   baseline:

   - list profiles;
   - list frozen cases;
   - list case revisions;
   - read supervisor status;
   - list jobs; and
   - list attempts.

   Record bounded identities/counts and any MCP error in the execution log.
   Do not mutate the catalog during this initial inventory.

5. Keep authority separated for the whole boundary:

   - MCP is the operator-level end-to-end acceptance surface for case/revision,
     submit, observe, cancel, retry, compare, and export workflows.
   - Normal repository tools (`rg`, PowerShell, `apply_patch`, Python tests,
     Git) inspect, edit, build, and test source.
   - MCP must never become a shell, arbitrary path writer, SQL console,
     mechanic override, or untyped native-argument channel.

## Source-Confirmed Starting Defects

Reproduce these before changing their behavior; retain exact outputs in the
execution log.

- `SolverLabService.submit_job()` records a watchdog override in the immutable
  request and job row, but `SolverLabSupervisor._execute_claimed()` passes the
  original `CaseTask` to `_run_case()`. `resolve_case_execution()` therefore
  enforces `task.watchdog_seconds`, not the submitted override.
- `_execute_claimed()` calls `finish_attempt()` before
  `_index_attempt_artifacts()`. A crash between those calls can publish a
  terminal job, release its lease, and leave incomplete or absent artifact
  hashes.
- `recover_stale_attempts()` does not recover a valid final report as
  completed. It publishes `orphaned` and `partial`/`failed` instead.
- `mark_attempt_orphaned()` releases the lease even when the recorded process
  identity cannot prove that the original worker is gone.
- idempotency lookup checks that a key has the same operation, but does not
  require the same complete request arguments. Catalog insertion also replays
  the prior result without comparing request identity.
- terminal artifact consumers use catalog paths/hashes as metadata but do not
  verify the current file bytes before parsing or exporting them.
- dispatch checks the queued case content digest, but it does not compare the
  complete queued source/executable/artifact/corpus/profile/economy/action-
  scope identity against freshly captured dispatch-time identity.
- the MCP executable serves tools but does not start a supervisor; an LLM can
  submit a job that remains queued unless a GUI or separate `supervise`
  process is also running.
- host reservation currently derives directly from
  `caps.max_solver_owned_bytes`. The global host safety reserve is visible,
  but per-worker process overhead is not represented separately from the
  native solver-owned cap.

If any claim no longer reproduces on the exact starting tree, update the plan
and log with the observed owner before implementing. Do not preserve a stale
diagnosis merely because it appears here.

## Locked Contracts

### Immutable execution request

New jobs use a versioned canonical request document whose identity includes:

- source commit, dirty flag, and sorted dirty-path manifest;
- executable resolved path and SHA-256;
- compiled-artifact manifest identity plus verification of every manifest-
  declared file hash and size;
- corpus path, SHA-256, corpus/schema/config identities;
- case ID, source kind, resolved path, file SHA-256, canonical content SHA-256,
  and local revision ID/content identity when applicable;
- profile path, schema, ID, canonical document SHA-256, and resolved native
  bindings;
- pinned economy identity and canonical economy payload digest;
- complete general and explicit action-scope identity from the corpus/profile;
- solver caps, exact-evaluation/verification controls, immutable watchdog,
  replicate, and case source;
- scheduler reservation components as disclosed orchestration identity, while
  keeping them outside solver proof authority.

Paths bind what is executed; hashes and canonical bytes decide equality. At
dispatch, reload and recapture every component from disk/catalog. Compare the
fresh document to the queued request and report a structured per-component
diff. Never “repair” a queued request by replacing it with current inputs.

A mismatch refuses dispatch before a native process starts. Use an explicit
terminal job state such as `dispatch_refused`, with no attempt or lease, a
durable event, and bounded component diagnostics. It must not be treated as a
transient host-memory block or silently retried each poll. An explicit retry
may requeue the same immutable request after the operator restores its inputs;
cloning/resubmission creates a newly resolved request.

Existing terminal history remains readable. Do not rewrite old request or
artifact identities. New queued legacy jobs that lack enough identity for
safe revalidation must refuse dispatch as `legacy_identity_incomplete` rather
than run optimistically.

### Complete idempotency binding

Every mutation constructs one canonical complete operation request before
looking up its idempotency key. Store its SHA-256 beside the command. Replay a
recorded result only when operation and request SHA-256 are both equal. The
catalog transaction must repeat the comparison so concurrent callers cannot
race the service precheck.

The complete request includes resolved defaults and every behavior- or target-
relevant argument: target identity, canonical document digest, profile/case/
revision/job identity, priority, watchdog, replicate/matrix expansion, desired
queue state, and dry-run semantics where applicable. A reused key with any
changed argument is a deterministic conflict and performs no mutation.
Dry-runs remain non-persistent and do not consume a key. Legacy command rows
without a request digest are readable but their keys cannot be replayed; reuse
returns `legacy_unbound_idempotency_key`.

### Terminal publication and evidence integrity

Attempt publication has one durable ordering:

```text
claimed/running lease
  -> process exits or recovery proves it absent
  -> classify result and discover artifacts
  -> validate required report identity and hash every present artifact
  -> one IMMEDIATE catalog transaction inserts the complete artifact set,
     terminalizes attempt and job, appends events, and releases the lease
```

No public terminal attempt/job state may exist without the required indexed
artifact hashes. Hashing and validation happen before the transaction; the
transaction rechecks the attempt/lease precondition and publishes artifacts,
statuses, result, events, and release atomically. If preparation fails, retain
a nonterminal `finalizing`/recovery state and reservation so startup recovery
can retry publication. A supervisor-owned error artifact may be created
atomically when no native report can exist, ensuring every terminal attempt
still has hashed evidence.

All terminal evidence reads resolve through one integrity-checking service:
verify catalog path ownership, existence, size, and SHA-256 before parsing,
comparing, summarizing, or exporting. A modified indexed artifact returns a
structured integrity failure and its bytes do not become evidence. Active
`partial.json` remains a mutable, explicitly `unindexed_live_observation`;
terminal summaries never silently fall back to unindexed files.

### Recovery and quarantine

Recovery must combine supervisor heartbeat, lease state, PID creation identity,
process liveness, final/partial artifact validity, and artifact publication
state.

- If the original process is proved absent and a valid final report exists,
  index it and recover the attempt/job to its truthful completed/native final
  classification.
- If the process is proved absent and only a valid partial or supervisor error
  exists, publish the truthful partial/failed/canceled classification with
  hashes.
- If the process is verified live, or liveness cannot distinguish the original
  process from a possible survivor, quarantine the attempt/job and lease.
  Preserve the full host reservation, forbid retry/clone-as-retry and duplicate
  dispatch, and emit explicit diagnostics. Never release merely because the
  prior supervisor heartbeat is stale.
- Reconciliation periodically rechecks quarantined attempts. It may release
  and publish only after original-process absence is proved. There is no
  untyped “force clear” MCP escape hatch.

### Host resource identity

Expose three different quantities:

1. native `max_solver_owned_bytes`, owned by the solver cap;
2. explicit per-worker host-process headroom, initially a conservative and
   configurable 512 MiB default; and
3. the existing global host safety reserve, also independently configurable.

Scheduler reservation is solver-owned cap plus per-worker headroom. Available-
memory admission additionally retains the global safety reserve. Display and
record all components and their policy version. None is a proof bound or a
claim about native live/peak solver-owned memory. Do not raise native solver
caps in this boundary.

## Target State Vocabulary

Keep existing terminal meanings and add only the orchestration states required
for honesty:

- job: `queued`, transient resource `blocked`, `dispatch_refused`, `running`,
  `canceling`, `orphan_quarantined`, `completed`, `partial`, `canceled`, or
  `failed`;
- attempt: `running`, `canceling`, `finalizing`, `orphan_quarantined`, or an
  existing truthful terminal outcome;
- lease: `active`, `quarantined`, or `released`.

Update the finite CLI/MCP/service/status contracts and nonvisual GUI model
handling where these strings cross a boundary. Do not add GUI features or
perform rendered review in this milestone.

## Gate 0 — Fresh-task baseline and fault reproductions

### Work

Complete the mandatory startup gate, then inspect only the current Lab owners:

- `solver_lab_contracts.py`;
- `solver_worker.py` and `solver_corpus_runner.py`;
- `solver_lab_catalog.py`;
- `solver_lab_service.py`;
- `solver_lab_supervisor.py`;
- `solver_lab_mcp.py` and `solver_lab.py`;
- current Lab tests and stable `docs/foundation/solver-lab.md`.

Create deterministic pre-fix witnesses for every source-confirmed defect. Use
temporary catalogs, artifacts, repositories, and synthetic process trees;
never mutate the live canonical SQLite, compiled artifact, frozen corpus, or
user catalog to manufacture a mismatch.

Define the v2 request/command/publication schemas, migration behavior, state
transition table, required artifact sets by outcome, structured identity diff,
and quarantine/reconciliation decision table before behavioral edits.

### Focused tests/evidence

- Same idempotency key plus changed target/priority/watchdog/document currently
  replays the first result.
- A short submitted watchdog and longer case watchdog prove which value is
  actually enforced.
- Fault injection between `finish_attempt` and artifact indexing demonstrates
  a terminal-without-hashes state.
- Stale recovery after a valid final report demonstrates incorrect orphan
  publication.
- Unknown PID identity demonstrates premature lease release/retry eligibility.
- Mutating a copied indexed artifact demonstrates that a read currently trusts
  changed bytes.
- MCP submit without a separately running supervisor remains queued.

### Pass

The execution log contains exact reproduction commands/results and a reviewed
schema/state-machine decision. No production behavior has changed yet.

### Hard stops

- Stop if HEAD/tree/MCP startup preconditions fail.
- Stop if fixing a witness would require native engine, mechanic, proof,
  search-order, Imprint, or WASM changes.
- Stop if the proposed catalog migration would overwrite or reinterpret
  immutable existing attempts.

### Retained state

Retain only tests, schema design notes, and reproduction evidence. Do not
commit a knowingly half-migrated catalog or behavioral source change at this
gate alone.

## Gate 1 — Canonical mutation requests and idempotency conflicts

### Work

Centralize canonical operation-request construction and transactional replay
checking. Add an additive catalog migration for request identity. Route every
mutating service operation—draft creation/update/discard, revision save, job
submit/matrix, cancel, retry, clone, priority, queue state, and bundle export—
through the same equality rule.

Ensure resolved defaults are included before lookup. Matrix requests bind the
sorted expanded case/revision/replicate set, not only caller filters. Submit
binds the complete immutable job request, not only raw optional arguments.

### Focused tests

- Parameterized same-key/same-request replay for every mutation.
- Parameterized same-key/changed-argument rejection for every mutation,
  including the required submit watchdog/priority/revision witness.
- Concurrent equal requests produce one mutation and one result; concurrent
  unequal requests with one key produce one mutation and one conflict.
- Dry-run does not persist or consume a key.
- Legacy unbound keys refuse replay without deleting their history.

### Pass

No changed payload can reuse an idempotency result, and all equal replay remains
stable across service, JSON CLI, and stdio MCP.

### Hard stops

- Stop if equality is checked only in service code and not inside the catalog
  transaction.
- Stop if canonicalization omits resolved defaults or target identity.
- Stop if migration rewrites old command payloads or makes the catalog
  unreadable by the current service before code and migration are coherent.

### Retained state

Retain the additive migration, shared request identity helper, service routing,
and passing focused tests only as one coherent change. Existing rows and
immutable results remain byte-for-byte untouched.

## Gate 2 — Dispatch identity, watchdog, and host headroom

### Work

Build one dispatch preflight that reloads and verifies the complete immutable
request. Validate both frozen and local-revision sources. Compare structured
components and refuse a mismatch without creating a process. Persist the
fresh dispatch identity and diff event; do not replace the queued request.

Minimize the validation-to-launch window: preflight before claim, record the
validated digest with the claim/command, and recheck immediately before native
process creation. A post-claim mismatch must release through an explicit
hashed supervisor-error terminal publication, never run the changed input.

Make the immutable request watchdog the exact value passed to
`run_isolated_process()`. Add the host-side watchdog to attempt command identity
and final result evidence even though it is not a native argv flag.

Separate native solver cap, 512 MiB default per-worker headroom, and global
safety reserve in the catalog, scheduler math, service/MCP status, events, and
documentation. Provide bounded CLI/MCP launch configuration; do not infer a
larger native cap.

### Focused tests

- Required regression matrix: queue a job, mutate one copied source/executable/
  artifact/corpus/case/revision/profile/economy/action-scope component, and
  prove `dispatch_refused`, zero native starts, zero active leases, and an exact
  component diff. Use isolated copies or injected provenance; never dirty the
  live repository as a test side effect.
- Restore an exact input and explicitly retry; prove the original immutable
  request can dispatch without being rewritten.
- Requested watchdog equals command identity, runner result, elapsed timeout,
  and the value received by the process runner.
- Host admission asserts `reservation = solver cap + per-worker headroom`,
  while the global reserve remains separate and all three are disclosed.
- Existing resource blocking remains transient and distinct from identity
  refusal.

### Pass

Every required identity mismatch refuses before execution, the submitted
watchdog is the enforced watchdog, and host overhead is explicit without
changing solver proof/cap semantics.

### Hard stops

- Stop if any path silently refreshes a queued request to current inputs.
- Stop if a mismatched job can create or launch a native attempt.
- Stop if watchdog equality is demonstrated only in metadata rather than by a
  timed child process.
- Stop if host headroom is reported as solver-owned memory or proof authority.

### Retained state

Retain the complete preflight, refusal state/events, watchdog plumbing, and
resource split together. Do not retain a new status that any service/CLI/MCP/
model consumer cannot parse.

## Gate 3 — Crash-atomic terminal publication, integrity, and recovery

### Work

Replace `finish_attempt()` followed by per-file inserts with a prepared
artifact set and one atomic catalog terminal-publication transaction. Add
explicit finalizing and quarantine state, preconditioned transitions, and a
single recovery classifier shared by startup and periodic reconciliation.

Recover valid final reports after supervisor interruption only when original-
process absence is proved. Quarantine verified-live or unresolved possible-
live workers, retain their lease/reservation, and prohibit retry or duplicate
dispatch until reconciliation proves absence.

Route terminal run summary, trace, compare, strategy/evaluation, and bundle
reads through the central hash-verifying artifact reader. Keep mutable active
partials visibly unindexed and non-authoritative.

### Focused tests

- Kill a supervisor after a valid final report is atomically created but before
  catalog publication; a successor recovers the attempt/job as completed with
  all required hashes.
- Inject failures before hashing, during artifact preparation, and before the
  publication transaction; no public terminal state or released lease may
  appear. Inject a failure after transaction commit; the terminal state must
  be complete and idempotently recoverable.
- Direct catalog attempts to terminalize without the required artifact set are
  rejected.
- Modify the bytes of a copied indexed report/strategy/log and prove every
  evidence surface reports `artifact_integrity_failure` without parsing or
  exporting changed content.
- An unresolved possible-live orphan becomes `orphan_quarantined`, retains its
  lease and full reservation, blocks retry, and cannot spawn a duplicate.
- When the same fixture later proves the original process absent,
  reconciliation publishes truthful hashed evidence and releases exactly once.
- Existing valid terminal artifacts remain readable; legacy unindexed
  terminals are disclosed rather than silently trusted.

### Pass

Terminal state, artifact rows, events, and lease release are atomic; final
report recovery is truthful; and no unresolved possible-live worker can lose
ownership or be duplicated.

### Hard stops

- Stop if any terminal status can be written without required artifact hashes.
- Stop if recovery treats a stale heartbeat as proof that a process is dead.
- Stop if a quarantine can be cleared by retry, clone-as-retry, or an untyped
  force flag.
- Stop if terminal readers retain a direct filesystem bypass around hash
  verification.

### Retained state and checkpoint

Gates 0–3 are the first required coherent checkpoint. Run the consolidated
focused Lab catalog/service/supervisor/CLI/MCP tests once here, update the
execution log and HANDOFF with exact results, and create a local checkpoint
commit with `Co-authored-by: Codex <codex@openai.com>`. The retained tree must
be clean, migration-safe, and useful even if later gates move to another task:
canonical idempotency, dispatch refusal/watchdog/headroom, atomic publication,
hash verification, final-report recovery, and quarantine must all be active
together. Do not run the full repository pipeline at this checkpoint.

## Gate 4 — One unattended MCP-plus-supervisor launch path

### Work

Add one documented command that starts the closed stdio MCP server and its
bounded supervisor together, for example:

```powershell
poecraft-solver-lab-mcp --root . --with-supervisor --max-workers 1
```

Expose poll interval, host budget, per-worker headroom, and global safety
reserve only as typed bounded orchestration options. The configured user-local
`poecraft2-native-solver-lab` registration must use this path. Do not commit a
user-specific absolute path or Codex configuration.

Use a catalog-scoped dispatcher ownership/heartbeat contract so two Codex
tasks cannot silently multiply the intended worker limit. If another verified
live dispatcher owns the catalog, the second MCP server remains a control/read
surface and reports the owner rather than dispatching. Stale ownership uses the
Gate 3 recovery rules.

Normal stdio shutdown stops new dispatch and does not silently cancel running
work. Document the bounded shutdown procedure. Forced MCP death must be handled
by startup recovery, not by pretending the worker completed.

### Focused tests

- Starting only the combined stdio server, submitting through MCP, and polling
  through MCP runs a job without GUI or separate `supervise` command.
- A second combined server does not exceed the catalog's intended dispatcher/
  worker ownership and reports control-only mode.
- Normal shutdown with no running work releases dispatcher ownership.
- Forced server death during queued/running/finalizing states is recovered by
  the successor without duplicate native processes.
- MCP schemas remain finite and expose no shell, SQL, arbitrary output path,
  native argument bag, or mechanic control.

### Pass

One registered command is sufficient for unattended typed operation and
supervisor health is visible through MCP.

### Hard stops

- Stop if enabling the combined path can create competing unbounded
  supervisors for one catalog.
- Stop if MCP shutdown automatically cancels a running solve.
- Stop if convenience requires arbitrary execution or path-write authority.

### Retained state

Retain the combined entry point, typed options, dispatcher ownership, tests,
and generic documentation together. Preserve the existing GUI and standalone
CLI supervisor paths.

## Gate 5 — Fresh-Codex MCP operator qualification

### Work

After Gate 4 is installed and the user-local registration is updated, stop the
current task and open/restart a fresh Codex task again. This is mandatory: an
already-running task may retain the old MCP process and cannot qualify the new
combined launcher.

The new task rereads AGENTS, HANDOFF, this plan, and the execution log; verifies
the Gate 4 checkpoint and clean tree; confirms the configured MCP tools; then
performs the following workflow entirely through MCP without opening the GUI:

1. list profile, frozen case, revision, supervisor, job, and attempt state;
2. select an existing suitable immutable revision, or clone a fast/bounded
   frozen case into a draft, validate it natively, and save a revision;
3. submit the revision with a unique idempotency prefix;
4. observe queued/running state and at least one live partial observation;
5. cancel the running job and observe `canceling -> canceled`, zero survivor,
   released lease, and released host reservation;
6. retry the same immutable job and observe its new attempt;
7. compare the two immutable attempts;
8. export an investigation bundle; and
9. verify the exported evidence reports matching request, command, artifact,
   and integrity identities.

### Focused tests/evidence

- Automated real-stdio test covers the same tool sequence with bounded
  synthetic/native fixtures.
- The actual fresh-task transcript records returned job/revision/attempt/
  bundle identities, partial observation, cancellation mode, retry ordinal,
  comparison, and artifact verification.
- No GUI process is opened.

### Pass

A fresh Codex task completes the required operator workflow using only the
configured MCP surface for Lab operations.

### Hard stops

- Stop if tools are missing or still attached to the pre-Gate-4 server.
- Stop if any operator step requires direct catalog edits, GUI interaction, or
  an arbitrary shell tool exposed by MCP.
- Stop if cancel/retry can race into two live attempts.

### Retained state

Retain immutable attempts and exported evidence. Do not delete acceptance
history to make later runs cleaner; use unique bounded identities.

## Gate 6 — Overnight unattended recovery qualification

### Work

Add an isolated, deterministic unattended qualification harness with a compact
accelerated fault suite and a real low-duty-cycle soak lasting at least six
hours (target eight). Store all output under ignored
`build/solver-lab/unattended-hardening/`.

The soak must exercise:

- queued work surviving MCP/supervisor restart;
- running work across controlled supervisor interruption;
- final-report-before-publication recovery;
- live cancellation with verified tree termination;
- retry producing a new immutable attempt;
- idempotent repeated commands and changed-payload conflicts;
- periodic provenance revalidation;
- resource blocks and explicit headroom accounting;
- quarantine followed by proved-absent reconciliation; and
- terminal artifact verification, comparison, and bundle export.

Use isolated catalog/attempt directories and copied or synthetic inputs for
tamper/fault cases. Include a small real native Lab case to keep the production
worker path represented, but do not spend six hours on continuous expensive
solver work; idle/restart intervals are valid unattended evidence.

### Focused tests/evidence

- The accelerated suite completes in ordinary test time and deterministically
  covers every injected crash boundary.
- The soak ledger records start/end wall time, launcher/supervisor identities,
  every transition and restart, request/command hashes, watchdogs, reservation
  components, artifact hashes, process-survivor checks, and final catalog
  invariants.
- End invariants: no unintended live process, no duplicate active attempt, no
  terminal without hashes, no released quarantined lease, no silent identity
  drift, and no unexplained reservation.

### Pass

The accelerated suite and six-plus-hour soak both satisfy all invariants, and
the evidence is summarized in the execution log without committing bulky
runtime artifacts.

### Hard stops

- Stop on the first leaked process, duplicate dispatch, unhashed terminal,
  premature reservation release, integrity bypass, or unexplained identity
  mismatch.
- Do not weaken time, process, hash, or reservation assertions to finish the
  overnight run.
- Do not convert synthetic fixture success into a claim that the real native
  path passed unless the real control also ran.

### Retained state

Retain the harness, bounded tests, and summarized evidence. Runtime catalogs,
logs, copied artifacts, and bundles remain ignored local evidence.

## Gate 7 — Final acceptance, documentation, and handoff

### Work and acceptance

Run once after all gates are coherent:

1. complete Solver Lab catalog/service/supervisor/CLI/MCP/GUI-nonvisual test
   suite, including additive migration from a v1 fixture;
2. real stdio MCP initialization/tool-schema/combined-supervisor integration;
3. the Gate 5 fresh-task operator workflow and Gate 6 overnight evidence audit;
4. existing corpus-runner and parity tests affected by shared worker watchdog
   plumbing;
5. `powershell -File scripts/test.ps1` exactly once as the complete repository
   pipeline;
6. `git diff --check`; and
7. one-off Markdown link and active/archive reachability audit.

No native build or WASM rebuild is expected because native and WASM changes are
out of scope. If implementation unexpectedly requires C++, C ABI, strategy
vocabulary, or browser-visible native behavior, stop and ask Oliver rather
than broadening this boundary. No 10,000-run compiled-strategy verification is
required because solver policies and evaluation behavior must remain unchanged.

Update `docs/foundation/solver-lab.md` with request identity, dispatch refusal,
watchdog, resource components, atomic publication, integrity, quarantine,
combined MCP launch, recovery, and unattended shutdown contracts. Record exact
tests and evidence in the execution log, write a result, archive the boundary,
set HANDOFF to no active boundary, and create coherent local commits ending
with `Co-authored-by: Codex <codex@openai.com>`. Do not push.

### Pass

Every required regression, operator workflow, overnight invariant, focused
suite, complete pipeline, and documentation audit passes from a clean tree.

### Hard stops

- Stop if final acceptance reveals any solver/mechanic/WASM semantic change.
- Stop if the actual MCP workflow differs from the automated stdio contract.
- Stop if documentation claims terminal integrity or orphan safety beyond the
  tests actually run.

### Retained state

Retain only a fully accepted boundary. If final acceptance fails, keep the
deepest coherent checkpoint, leave the tree buildable, update HANDOFF with the
exact failure and next command, and do not mark the milestone complete.

## Explicit Non-Goals

- crafting mechanics or action legality;
- solver search order, action admission, proof bounds, exactness, or policy
  publication;
- verified option/subgoal systems;
- RCASSP or another lower-bound abstraction;
- learned guidance, training, model inference, or GPU work;
- automatic or manual Imprint behavior changes;
- native solver cap increases or automatic cap tuning;
- scheduler-aware solver graph checkpoint/replay;
- release-WASM, browser worker, Calculator, or product behavior;
- GUI redesign or rendered visual review;
- remote/multi-machine execution, cloud service, authentication, or accounts;
- arbitrary shell, SQL, filesystem-write, mechanic, or native-argument MCP
  authority.

## Cross-Gate Stop Conditions

- Never reset, clean, restore, discard, or overwrite pre-existing user work.
- Never hand-edit canonical SQLite or the compiled runtime artifact.
- Never release an unresolved possible-live worker's reservation.
- Never publish a terminal attempt without its required artifact hashes.
- Never run a queued job against identity that differs from its immutable
  request.
- Never replay an idempotency key for a changed canonical payload.
- Never weaken frozen-case or saved-revision immutability.
- Never relabel partial/crash/orphan evidence as completed to progress a gate.
- Never use an MCP convenience surface to acquire mechanic or arbitrary
  execution authority.
- Keep `execution-log.md` and HANDOFF current at each checkpoint. If blocked,
  preserve exact commands, outputs, retained state, and the next executable
  action.
