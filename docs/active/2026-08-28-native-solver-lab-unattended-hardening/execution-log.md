# Native Solver Lab Unattended Execution and Identity Hardening Log

**Status: Gates 0–4 passed; the mandatory fresh-task Gate 5 restart is next.**

Parent: [Plan](plan.md)

## Selected boundary

- Selected by Oliver: 2026-08-28.
- Stable source base:
  `978b200e8d7993a49ee7991f303cc0823f60914b`
  (`Complete solver lab GUI stabilization`).
- Expected implementation-task HEAD: the single documentation-only planning
  commit whose parent is the stable source base and whose subject is
  `Activate solver lab unattended hardening`.
- Branch: `main`, local-only; do not push.
- Starting tree at selection: clean.
- Prior acceptance retained: 34 focused Lab tests, rendered PySide smoke,
  `3,417,290` native checks, all 12 solver benchmark specifications, 28/28
  release-WASM checks, and the complete repository pipeline.
- No source, catalog, user MCP configuration, or runtime behavior was changed
  while selecting this plan.

## Mandatory next-task startup record

- Fresh task: yes; the configured `poecraft2-native-solver-lab` server was
  loaded before implementation.
- Implementation HEAD:
  `6400f045a97a0389ce2d48e430ae6e55c9465f01` (`Activate solver lab unattended
  hardening`).
- HEAD parent: `978b200e8d7993a49ee7991f303cc0823f60914b`.
- `git status --short`: empty/clean.
- `git show --stat --oneline --decorate HEAD`: the expected single
  documentation-only activation commit; changed only `HANDOFF.md`,
  `docs/README.md`, the active plan/log, and `docs/active/README.md`.
- MCP server/tool inventory: 31 typed tools loaded (`list/get` profile, case,
  revision, draft, job, attempt, summary, trace, strategy, evaluation, and
  supervisor reads; bounded draft/revision, submit/matrix, cancel/retry/clone,
  priority/queue, compare, and export mutations). No shell, SQL, arbitrary
  path-write, mechanic, or native-argument tool was present.
- MCP call health: profile, frozen-case, revision, supervisor, job, and attempt
  inventory calls all returned `ok: true`; no MCP error.
- Profile identities: 1 — `native_allflame_no_imprint_v1`, content SHA-256
  `876824a29d51ef8e87013639a86120315ca13235833261980b3eb28917b6bb56`.
- Frozen cases: 5 —
  `conquest-lamellar-allflame-clean-3-prefix-extended-product8`,
  `conquest-lamellar-allflame-clean-3-suffix-product8`,
  `conquest-lamellar-allflame-clean-4-pdr-product8`,
  `conquest-lamellar-allflame-partial-4-to-5-product8`, and
  `spine-bow-allflame-clean-4-goal-product8`.
- Revisions: 3 —
  `case-rev-dc1e9c206c4402dbeededea1819074db`,
  `case-rev-15ce203781cd7935c4e0326fc1a65ca0`, and
  `case-rev-5e1df8024975909ab780c335b9769ba6`.
- Supervisor status: queue active; job counts `completed=11`, `partial=4`;
  current reported session
  `supervisor-0f222afe-f8f1-46dd-915a-48b22b8d37c0` active with
  `max_workers=1`, `memory_budget_bytes=30161058816`,
  `memory_safety_reserve_bytes=536870912`, and poll interval `0.25s`.
- Jobs: 15 bounded results — 11 `completed`, 4 `partial`; IDs in MCP order:
  `job-2b88e26a-073e-4873-82dd-2c0f2cf55b10`,
  `job-d38c292b-5aa7-42b5-83fd-b3b0a0eb008b`,
  `job-ce9ebbc0-afc7-43b9-b517-fd7668769cfe`,
  `job-535c56a2-726f-41f9-b9b9-8b6c5f1cea8c`,
  `job-4872aa1e-1e95-4b51-a417-7e5d36c5f388`,
  `job-38eb2ea8-0fae-487f-b155-1a78efb9a022`,
  `job-02539671-559e-48c1-a2d4-ff5e20ab5070`,
  `job-8dd22c62-d36e-489b-bdcc-b22e5547241d`,
  `job-f26b63e8-1a13-49b5-81bd-c5e9d3a3adef`,
  `job-b32ebe64-fe87-429c-bdde-48df5f26fd80`,
  `job-bca518be-5ee8-4a75-8468-47a155324179`,
  `job-702cb567-5f06-46ad-ac78-91e4f899eec4`,
  `job-fa0725bc-01ac-427b-a3b8-846050609593`,
  `job-6e9241e7-f86a-413c-bdc2-4c26212e6498`, and
  `job-d585a2d2-3b46-4dd8-895e-b9fb84bd108d`.
- Attempts: 15 bounded results — 11 `completed`, 4 `watchdog`; IDs in MCP
  order: `attempt-af7b1ad4-46bc-4b57-8e19-08a5efedff41`,
  `attempt-ea91cc2a-d59f-4282-a269-b9db3c9aaee6`,
  `attempt-7005e598-0452-4c19-9c75-55f89d97b1e5`,
  `attempt-a7ff820d-6b85-4b6f-888c-fd46ae28f29e`,
  `attempt-7c049c78-b503-4030-b50f-4004a8e66931`,
  `attempt-674f31f9-4648-4bc0-9aa3-a895bc23ec7f`,
  `attempt-eb8fe10b-edcd-47f9-8e9a-003d3ec4ed56`,
  `attempt-3e582bad-3a56-49a7-a730-096f3c555a7b`,
  `attempt-cd5d77e2-172d-4dfe-90f2-38f184fe4285`,
  `attempt-e1350ab0-d661-47e5-a478-1aea0b447fe5`,
  `attempt-d0561a02-4bbb-4308-8b30-867ad6d8edf8`,
  `attempt-59c44de2-6723-47b2-bc49-8b2784caafe7`,
  `attempt-dc6de09e-6e99-41b6-81bb-7341d558d964`,
  `attempt-e9df1231-79c5-41db-a9cb-b2c80485b8d9`, and
  `attempt-3ef589b4-5239-4f39-a7aa-f3648cccd36e`.
- Startup decision: **proceed**.

The inventory above used MCP exclusively. Ordinary repository tools remain the
source inspection, editing, testing, and Git surface.

## Gate ledger

| Gate | Status | Required retained evidence |
| --- | --- | --- |
| 0 — baseline/reproductions | passed | exact defect witnesses and reviewed v2 state/schema design below |
| 1 — idempotency binding | passed | every mutation family, dry-run, legacy, equal/unequal race, JSON CLI, and real stdio MCP tests |
| 2 — dispatch/watchdog/headroom | passed | eight-component identity mutation matrix, local revision mutation, timed child watchdog, and split-resource evidence |
| 3 — atomic publication/recovery | passed | pre/post-transaction crash points, valid-final recovery, tamper rejection, and possible-live quarantine/reconcile tests |
| 0–3 checkpoint | passed | 72 focused tests, clean diff check, local checkpoint commit, and exact HANDOFF continuation |
| 4 — combined MCP/supervisor | passed | one-command real-stdio dispatch, singleton/race/legacy ownership, normal release, and queued/running/finalizing forced-restart tests |
| 5 — fresh-task MCP E2E | not started | revision → submit → partial → cancel → retry → compare → export transcript |
| 6 — overnight qualification | not started | accelerated suite and six-plus-hour soak ledger/invariants |
| 7 — final acceptance | not started | full Lab suite, stdio, full pipeline, docs/link/diff checks |

## Source-confirmed pre-implementation owners

- Watchdog: immutable override is recorded by `solver_lab_service.py`, while
  the shared worker enforces `CaseTask.watchdog_seconds`.
- Terminal ordering: `solver_lab_supervisor.py` calls catalog
  `finish_attempt()` before indexing files.
- Final recovery: stale attempts are always marked orphaned; a valid final
  report is not a completion recovery path.
- Possible-live ownership: orphan marking releases the lease regardless of
  whether original-process absence was proved.
- Idempotency: operation name is compared, complete request identity is not;
  catalog conflict replay is also payload-blind.
- Evidence reads: indexed hashes are returned as metadata but are not checked
  before terminal files are parsed/exported.
- Dispatch identity: case content is checked, but the full queued execution
  identity is not freshly compared.
- Unattended MCP: the stdio server does not own/start a supervisor.
- Memory: host reservation equals the native solver-owned cap; per-worker host
  overhead is not a separate component.

These are hypotheses to reproduce at Gate 0, not permission to skip runtime
evidence.

## Gate 0 baseline and v2 design

Gate 0 used only isolated temporary catalogs/attempt directories plus the
ignored `build/solver-lab/unattended-hardening/gate0_witnesses.py` driver. The
exact command was:

```powershell
$env:PYTHONPATH='tools/ingest;bindings/python'
py -3 build/solver-lab/unattended-hardening/gate0_witnesses.py
```

The clean-source defects reproduced exactly:

```text
IDEMPOTENCY_CHANGED_PAYLOAD {"same_result": true, "stored_priority": 1, "stored_watchdog": 1.0}
WATCHDOG_OVERRIDE {"case": 120, "submitted": 0.05, "worker_received": 120.0}
PUBLICATION_GAP {"artifact_count": 0, "attempt_status": "completed", "error": "injected_after_finish_before_index", "job_status": "completed", "lease_status": "released"}
VALID_FINAL_AND_UNKNOWN_PROCESS_RECOVERY {"attempt_status": "orphaned", "job_status": "failed", "lease_status": "recovered_orphan", "recovery": [{"failure_kind": "stale_supervisor_lease", "partial_observation_available": false, "process_identity_verified": false, "status": "orphaned", "verified_process_tree_terminated": false}]}
TAMPERED_INDEXED_READ {"catalog_hash": "2d9885e7e22c399e5e131230715e55194cf14fb8cfbe587305cd9d93edaeda96", "parsed_changed_lower_bound": 999.0}
MCP_WITHOUT_SUPERVISOR {"attempt_count": 0, "job_status": "queued", "submitted_job": "job-cf37d1a3-1d92-4570-99d5-63e6d3e6c654"}
```

The recovery supervisor UUID is intentionally omitted from the retained line
because it is random; the full local stdout retained it. The stdio witness
used the real MCP initialization and `submit_job`/`list_jobs` tools, not a
direct service substitute.

Reviewed additive v2 schema/state decision:

- Catalog schema advances additively from v3 to v4. Existing rows are never
  rewritten. Commands gain nullable `request_sha256`; legacy null rows remain
  readable but their keys return `legacy_unbound_idempotency_key`. Jobs gain
  nullable dispatch identity/diff plus separate solver-cap, worker-headroom,
  and reservation-policy columns. Attempts gain nullable validated request and
  host-watchdog identity. Leases gain disclosed reservation bytes and use only
  `active`, `quarantined`, or `released` for new rows.
- New canonical operation requests use
  `solver_lab_operation_request_v2` and hash the operation plus the complete
  resolved payload. Service precheck and the same `BEGIN IMMEDIATE` mutation
  transaction both require operation/request-hash equality. Dry-runs neither
  insert nor reserve a key.
- New immutable job requests use `solver_lab_execution_request_v2`. Top-level
  components are source, executable, compiled artifact with every declared
  file verification, corpus, case/revision, profile, economy, complete general
  plus explicit action scope, solver caps, measurement/watchdog, replicate,
  and scheduler reservation policy. Dispatch diffs are bounded per top-level
  component and never rewrite the queued request.
- Job transitions are `queued|blocked -> dispatch_refused` before claim on
  preflight mismatch; otherwise `queued|blocked -> running ->
  finalizing -> terminal`. Attempt transitions are `running|canceling ->
  finalizing -> terminal`. A post-claim identity failure follows finalizing
  and publishes a hashed supervisor error with job `dispatch_refused`.
- Terminal publication prepares and hashes the complete artifact set first,
  then one `BEGIN IMMEDIATE` transaction inserts artifacts, terminalizes the
  attempt/job, appends events, and releases the lease. Completed attempts
  require indexed final report plus worker log; other outcomes require at
  least one truthful indexed partial/log/supervisor-error artifact. Existing
  legacy unindexed terminals remain readable as metadata but are explicitly
  disclosed and never parsed as trusted evidence.
- Recovery classifies process identity as `verified_live`, `proved_absent`, or
  `unknown`. Verified-live and unknown both transition attempt/job to
  `orphan_quarantined`, keep the full lease/reservation quarantined, and block
  retry/clone-as-retry. Only proved absence permits final-report completion or
  truthful partial/error publication and exactly-once release.
- Host scheduling records native solver-owned cap, default 512 MiB per-worker
  headroom, total worker reservation, global safety reserve, and policy
  version separately. Only the native cap retains solver/proof authority.

Gate 0 decision: **pass**. No production behavior had changed when these
witnesses and decisions were recorded.

## Gates 0–3 checkpoint record

- Catalog migration/schema versions: additive catalog v4; command/job/attempt
  contracts v2; operation requests
  `solver_lab_operation_request_v2`; execution requests
  `solver_lab_execution_request_v2`. Nullable migration columns preserve all
  existing rows, artifacts, results, and command payloads.
- Idempotency request contract: every mutating service operation constructs a
  complete resolved canonical request before lookup. Both the service precheck
  and the mutation's `BEGIN IMMEDIATE` transaction require equal operation and
  request SHA-256. Equal concurrent requests return one mutation/result;
  unequal concurrent requests produce one mutation and
  `idempotency_conflict: request_sha256_mismatch`. Legacy null request hashes
  return `legacy_unbound_idempotency_key`; dry-runs do not consume keys. JSON
  CLI and real stdio MCP expose the same conflict.
- Dispatch refusal state and tested components: preclaim mismatch moves the job
  to `dispatch_refused`, records the fresh identity and bounded structured
  component diff, and creates zero attempts/processes. The parameterized test
  independently changed `source`, `executable`, `compiled_artifact`, `corpus`,
  `profile`, `economy`, `action_scope`, and `scheduler`; an actual saved local
  revision file mutation was also refused. Restoring identity plus an explicit
  retry succeeds without rewriting the original request. A second preflight
  immediately before native launch converts a postclaim mismatch into a hashed
  supervisor-error terminal publication.
- Requested/enforced watchdog evidence: a submitted `0.15s` immutable
  watchdog was recorded in the command identity, passed to the real isolated
  child, and expired the sleeping child before its longer case default. The
  result retained `host_watchdog_seconds=0.15` and no survivor.
- Solver cap / worker headroom / global reserve evidence: the witness recorded
  native solver cap `1073741824`, per-worker headroom `536870912`, total worker
  reservation `1610612736`, and separate global reserve `268435456` under
  `solver_lab_host_reservation_v2`. Scheduler admission and lease evidence use
  the total worker reservation; only the native cap is passed to solver-owned
  enforcement.
- Terminal publication transaction: supervisor transitions to `finalizing`,
  validates and hashes files before catalog mutation, then one
  `BEGIN IMMEDIATE` transaction inserts artifact rows, terminalizes attempt and
  job, emits events, and releases the lease. Pretransaction hashing failure
  retains `finalizing` plus the live reservation; an injected postcommit failure
  replays idempotently without duplicate publication or release.
- Required artifact sets: completed publication requires a valid indexed final
  report plus worker log, or the final report plus a hashed recovery error when
  recovering a worker whose log is unavailable. Every other terminal outcome
  requires at least one truthful indexed partial report, worker log, or
  supervisor-error artifact. Strategy files are individually indexed when
  present.
- Final-report recovery evidence: with process identity proved absent, a stale
  `running`/`finalizing` attempt containing a valid final report recovered to
  completed, indexed its evidence, released the lease exactly once, and
  retained its attempt history.
- Possible-live quarantine evidence: both `verified_live` and `unknown` move
  attempt/job to `orphan_quarantined`, keep the lease/reservation quarantined,
  block dispatch/retry/clone, and emit diagnostics. Reconciliation releases
  and truthfully publishes only after the original process is proved absent.
- Artifact tamper evidence: changed report, strategy, and worker-log bytes are
  rejected by the centralized size/SHA/path verifier from run summary, bound
  trace, strategy summary, evaluation, compare, and bundle surfaces. Legacy
  unindexed terminals are disclosed as `legacy_unindexed_terminal` and their
  bytes are not parsed.
- Focused command/result:

  ```powershell
  py -3 -m pytest tools/ingest/tests/test_solver_lab_contracts.py tools/ingest/tests/test_solver_lab.py tools/ingest/tests/test_solver_lab_supervisor.py tools/ingest/tests/test_solver_lab_mcp.py tools/ingest/tests/test_solver_lab_gui_stabilization.py tools/ingest/tests/test_solver_lab_unattended_hardening.py tools/ingest/tests/test_solver_corpus_runner.py tools/ingest/tests/test_solver_lab_parity.py -q --tb=short
  # 72 passed in 38.02s
  git diff --check
  # exit 0, no output
  ```

- Checkpoint commit: local `Harden solver lab unattended execution identity`
  (the commit containing this record), with the required Codex co-author line;
  nothing pushed.
- Retained limitations: the configured MCP executable still serves controls
  only and does not yet own a supervisor. Catalog-scoped singleton dispatcher
  ownership and the combined launch path belong exclusively to Gate 4. No
  native engine, mechanic, ABI, compiled artifact, strategy vocabulary, WASM,
  browser, or rendered-GUI behavior changed.
- Next exact action: implement Gate 4's catalog-scoped dispatcher ownership and
  combined MCP/supervisor launcher, qualify forced restart and dual-server
  behavior, update the user-local registration, then stop for the mandatory
  fresh-task Gate 5 restart.

## Gate 4/5 operator record

Gate 4 retained one combined generic command:

```powershell
poecraft-solver-lab-mcp --root . --with-supervisor --max-workers 1
```

- MCP server version: `0.2.0`; tool count remains 31 with unchanged finite
  typed authority and no shell, SQL, arbitrary output path, native argument
  bag, mechanic control, or remote worker surface.
- Typed bounded launch controls: poll interval `0.01..60s`, workers `1..16`,
  automatic-or-bounded host budget, and nonnegative bounded per-worker
  headroom/global safety reserve. Service/job identity still owns the latter
  resource values; only the native cap has solver authority.
- Catalog migration: additive v5 adds the single-row dispatcher ownership,
  PID-creation identity, heartbeat, configuration, acquisition/release, and
  replacement record. Existing v4 rows are untouched.
- Ownership: simultaneous acquisition produces exactly one owner. A second
  in-process or real stdio combined server reports `control_only` and the live
  owner. Catalog-wide active/quarantined lease count and reservation bytes are
  included in admission, so a successor cannot dispatch around a possible-live
  orphan.
- Legacy transition: a live pre-v5 supervisor session is conservatively
  migrated as owner. The exact registered-command probe found the intentionally
  still-open GUI supervisor
  `supervisor-0f222afe-f8f1-46dd-915a-48b22b8d37c0` at PID `46580`, recorded it
  as the catalog owner, and returned
  `runtime_dispatcher.mode=control_only` with reason
  `verified_live_legacy_supervisor`. It did not start another dispatcher.
- Real combined stdio dispatch: an isolated temporary catalog started only the
  MCP command above, submitted through MCP with a `0.15s` watchdog, created one
  attempt/native process, reached a truthful terminal result, and released
  dispatcher ownership when stdio closed.
- Forced-death fault matrix: proved-dead dispatcher ownership at `queued`,
  `running`, and `finalizing` was replaced transactionally. The successor
  dispatched the queued job once, published the evidence-free running attempt
  as failed with a hashed supervisor error, and recovered the valid finalizing
  report as completed. Every case retained one attempt, no duplicate worker,
  no reservation, and released ownership.
- Normal shutdown: two-server and no-work tests released ownership. Shutdown
  stops new dispatch but does not cancel live work; the non-daemon supervisor
  drains it within the immutable watchdog. The documented shorter procedure is
  pause, typed cancel if desired, wait for terminal/released reservation, then
  close.
- User-local registration: removed and re-added successfully with the installed
  `poecraft-solver-lab-mcp` entry point, repository root, `--with-supervisor`,
  and `--max-workers 1`; `codex mcp get` confirmed enabled stdio transport and
  those arguments. No absolute user path or Codex configuration is committed.
- Registered-command probe: server
  `poecraft2-native-solver-lab`, version `0.2.0`, 31 tools, no MCP error; the
  live-GUI control-only result is recorded above.
- Focused command/result:

  ```powershell
  py -3 -m pytest tools/ingest/tests/test_solver_lab_contracts.py tools/ingest/tests/test_solver_lab.py tools/ingest/tests/test_solver_lab_supervisor.py tools/ingest/tests/test_solver_lab_mcp.py tools/ingest/tests/test_solver_lab_gui_stabilization.py tools/ingest/tests/test_solver_lab_unattended_hardening.py tools/ingest/tests/test_solver_corpus_runner.py tools/ingest/tests/test_solver_lab_parity.py -q --tb=short
  # 81 passed in 45.15s
  ```

- Gate 4 commit: local `Run solver lab MCP with singleton dispatcher` (the
  commit containing this record), with the required Codex co-author line;
  nothing pushed.
- Mandatory stop: do not execute Gate 5 in this task. Oliver should normally
  close the still-open GUI first so its supervisor drains/releases ownership,
  then restart/open a fresh Codex task. If it remains open, the registered MCP
  server must correctly stay control-only and report that owner.

The fresh Gate 5 task must record:

```text
fresh task identity:
MCP server/version/tools:
dispatcher ownership mode:
selected/created revision:
submitted job:
observed partial:
cancellation attempt/mode/reservation release:
retry attempt ordinal:
comparison identity:
bundle identity and verified artifact hashes:
GUI opened: no
```

## Gate 6 overnight record

```text
soak artifact root:
start/end/duration:
restart count and phases:
queued/running/finalizing recovery results:
cancellation/retry results:
idempotency conflict count:
quarantine/reconciliation results:
provenance revalidation results:
terminal artifact count/integrity results:
process survivors:
active/quarantined/released reservations:
unexplained failures:
```

## Final acceptance record

```text
focused Lab suite:
stdio MCP integration:
fresh-task MCP E2E:
overnight evidence audit:
shared corpus-worker/parity tests:
full scripts/test.ps1:
git diff --check:
documentation link/reachability audit:
native/WASM changes: none expected
10,000-run strategy verification: not required unless scope changes
final source checkpoint:
archive/result checkpoint:
```

## Stop/handoff record

If a gate stops, record the first failing invariant, exact reproduction, why it
cannot be fixed safely inside scope, all retained commits/state, and one next
command. Do not mark the boundary complete or release quarantined evidence to
make the tree look clean.
