# Native Solver Lab Unattended Execution and Identity Hardening Log

**Status: selected; implementation has not begun.**

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

The implementing task must be a newly opened/restarted Codex task so the
configured `poecraft2-native-solver-lab` MCP server is available. Before edits,
fill in:

```text
implementation HEAD:
HEAD parent:
git status --short:
MCP server/tool inventory:
profile identities:
frozen case count/IDs:
revision count/IDs:
supervisor status:
job count/statuses:
attempt count/statuses:
startup decision: proceed | stop
```

Use MCP for that operator inventory. Use ordinary repository tools for source
inspection, editing, tests, and Git.

## Gate ledger

| Gate | Status | Required retained evidence |
| --- | --- | --- |
| 0 — baseline/reproductions | not started | exact defect witnesses and reviewed v2 state/schema design |
| 1 — idempotency binding | not started | equal replay, changed-payload conflict, transactional race tests |
| 2 — dispatch/watchdog/headroom | not started | identity mutation matrix, timed watchdog, resource-component evidence |
| 3 — atomic publication/recovery | not started | crash points, final recovery, integrity failure, quarantine/reconcile tests |
| 0–3 checkpoint | not started | focused suite, clean checkpoint commit, exact HANDOFF continuation |
| 4 — combined MCP/supervisor | not started | one-command dispatch, singleton ownership, forced-restart tests |
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

## Gates 0–3 checkpoint record

Fill in only after the complete checkpoint is coherent:

```text
catalog migration/schema versions:
idempotency request contract:
dispatch refusal state and tested components:
requested/enforced watchdog evidence:
solver cap / worker headroom / global reserve evidence:
terminal publication transaction:
required artifact sets:
final-report recovery evidence:
possible-live quarantine evidence:
artifact tamper evidence:
focused commands/results:
checkpoint commit:
retained limitations:
next exact action:
```

## Gate 4/5 operator record

After the combined launcher is installed, record the generic command and the
user-local MCP registration verification without committing user-specific
paths. Then restart/open a new Codex task and record:

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
