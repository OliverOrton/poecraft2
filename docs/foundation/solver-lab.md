# Native Solver Lab

**Status: stable local orchestration and research-tool reference.** The Lab
wraps the native benchmark; it does not own crafting mechanics, solver policy,
proof authority, or strategy evaluation.

Parent: [Foundation](README.md)

Verified against the implemented Lab surface: 2026-08-29 @ CLI-first MCP
removal boundary. Checked owners include `solver_lab_catalog.py`,
`solver_lab_service.py`, `solver_lab_supervisor.py`, `solver_lab.py`, and
`solver_lab_unattended_qualification.py` plus their focused contract, CLI,
supervisor, nonvisual-GUI, corpus-runner, and parity tests.

## Purpose And Authority

The Native Solver Lab is a local Windows research workbench for repeatable
native solver experiments. A persistent SQLite catalog, resource-aware
supervisor, PySide6 GUI, and JSON CLI all call the same typed Python service.
Every solve still runs in its own
`poecraft_solver_benchmark.exe` process through the shared corpus-worker
adapter.

The native engine remains authoritative for case validity, mechanics, action
legality, probabilities, Bellman values, bounds, policy compilation,
properness, and independent exact evaluation. The Lab owns only request
identity, durable orchestration, process/resource supervision, bounded reads,
and immutable artifact indexing.

## Install And Launch

Build the engine first if `build/engine/poecraft_solver_benchmark.exe` is not
current:

```powershell
powershell -File scripts/build.ps1
```

Install the optional Lab dependencies into the active Python environment:

```powershell
py -3 -m pip install -e "tools/ingest[solver-lab]"
```

The reliable repository-local launch does not depend on Python's Scripts
directory being on `PATH`:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
py -3 -m poecraft_ingest.solver_lab --root . gui
```

If the Python Scripts directory is on `PATH`, the shorter installed-console
command is equivalent:

```powershell
poecraft-solver-lab --root . gui
```

Installing the package creates that executable but does not necessarily add
its Scripts directory to PowerShell's `PATH`.

By default the catalog is `build/solver-lab/catalog.sqlite3`, attempts are
under `build/solver-lab/attempts/`, and investigation bundles are under
`build/solver-lab/bundles/`. These paths are ignored build evidence, separate
from canonical game SQLite and the compiled runtime artifact.

## Locked v0 Profile

`native_allflame_no_imprint_v1` binds the exact profile and economy recorded in
every job:

- frozen Allflame economy identity;
- native `calculator_product_v1`;
- automatic Imprint programs disabled;
- voluntary/economic Restart disabled;
- native mechanic-owned paid Fracture miss replacement retained;
- goal-progress-gated reforges enabled;
- exact junk-free terminal success;
- complete native Calculator goal-relevant action-family scope; and
- native independent exact strategy evaluation enabled.

Simulator verification is opt-in. Whenever it is required, the repository
acceptance count is 10,000 runs. The profile resolves worker flags through the
typed service; the GUI and CLI do not reproduce those controls.

## Case Authoring And Revision Identity

Frozen repository cases remain read-only. Local authoring has three explicit
states:

1. an editable draft stored in the Lab catalog;
2. a native-validated, content-addressed immutable revision under
   `build/solver-lab/cases/`; and
3. a queued job whose request identity records the revision ID, canonical case
   digest, snapshot paths, corpus digest, profile, and effective native
   command.

Editing a draft after a save cannot change the saved revision or any existing
job. Saving unchanged content reuses its existing revision; changed content
creates the next ordinal. Draft deletion retains saved revisions. Before a
revision can be saved, the service checks the bounded document shape, locks it
to the selected Lab profile, and invokes the native benchmark's genuine
`--validate-only` path. The Lab never infers mechanic validity itself.

Execution request v4 discloses the action-envelope identity through separate
components for explicit Imprint scope, the effective disabled native action
families, allowed mechanic families, the product action envelope, and the goal
action list. Disabled families are validated against the native public family
vocabulary and canonicalized as a sorted unique list before hashing, matching
the engine's order-independent bit-mask interpretation. The Lab does not assign
actions to families; that remains native authority. A profile's Imprint scope
therefore cannot be mistaken for the goal's effective disabled-family list.

The browser Calculator owns graphical item and goal authoring. Its **Copy Lab
case** action exports the current concrete affixes, crafted/fractured/veiled
slot flags, influence and Eldritch state, product goal, diagnostic family
exclusions, pinned Allflame prices, and supported solve targets. The exporter
refuses active Imprint checkpoints and special item flags that the benchmark
start format cannot preserve. It fixes automatic Imprints and voluntary
Restart off to match the current Lab profile.

## GUI Workflow

The GUI has five persistent surfaces:

1. **Cases** browses frozen cases, editable drafts, and immutable revisions.
   It can create from the local template, clone, import the Calculator
   clipboard envelope, edit JSON plus the common watchdog/bounded-finish/
   memory controls, validate, save a revision, copy an export, and submit.
2. **Queue & Run** submits a frozen case or local revision and shows priority, attempt, host
   reservation, phase, bounds, stop reason, artifacts, events, work, and
   memory. It can cancel, retry, clone, reprioritize, and pause/resume new
   dispatch.
3. **Compare** filters immutable attempts and compares two to twenty request
   identities, outcomes, deterministic work, memory, and strategy summaries.
4. **Strategy** shows bounded graph/action/evaluation evidence and exports a
   controlled investigation bundle.
5. **Matrix** previews and submits a canonical frozen-case-by-replicate product.
   Re-submitting the displayed batch is idempotent; **Submit new replicate
   batch** deliberately assigns a new batch identity.

The GUI polls durable state every 1.5 seconds and never performs a solve,
catalog aggregation, report parse, native case validation, or mutation on its
UI thread. Attempt summaries are cached by artifact identity and terminal
summaries are not reopened; selected detail refresh is separate, overlapping
refresh is suppressed, unchanged table rows do not reset the model, and job
selection is preserved by stable ID.

Every visible action has an explicit valid/busy state and explanation. Accepted
and rejected operations append to the persistent **Activity & Errors** dock
with affected identities and state transitions. Complete tracebacks and the
same activity stream are retained in `build/solver-lab/gui-activity.log`;
ordinary health refresh never clears this history.

Live Cancel changes the durable job to `canceling`, lets the worker observe the
request, then uses the supervisor's verified Windows process-tree termination
fallback when graceful completion does not arrive. The final `canceled` state
is not displayed until the process is gone, the attempt and job are terminal,
and the lease and host reservation are released.

## JSON CLI

All commands accept the common `--root`, `--catalog`, `--attempts`,
`--executable`, `--artifact`, `--corpus`, and `--profile` overrides before the
operation name. Examples:

```powershell
poecraft-solver-lab --root . profiles
poecraft-solver-lab --root . cases
poecraft-solver-lab --root . create-case-draft --name local-three-prefix --source-case-id CASE_ID --idempotency-key create-local-three
poecraft-solver-lab --root . validate-case-draft DRAFT_ID
poecraft-solver-lab --root . save-case-revision DRAFT_ID --idempotency-key save-local-three-v1
poecraft-solver-lab --root . submit LOCAL_CASE_ID --revision-id REVISION_ID --idempotency-key run-local-three-v1
poecraft-solver-lab --root . submit CASE_ID --idempotency-key study-a-case-1
poecraft-solver-lab --root . submit-matrix --include-role fast_exact_three_prefix --replicates 2 --idempotency-key study-a
poecraft-solver-lab --root . run-until-idle --max-workers 1
poecraft-solver-lab --root . supervise --max-workers 1
poecraft-solver-lab --root . attempts
poecraft-solver-lab --root . strategy-summary --attempt-id ATTEMPT_ID
poecraft-solver-lab --root . export-bundle --attempt-id ATTEMPT_ID --idempotency-key export-ATTEMPT_ID
```

Mutating operations require an idempotency key and accept `--dry-run`. Matrix
includes are the union of explicit case IDs and roles, exclusions apply last,
and empty programmatic includes mean the full frozen corpus. Expansion order
is sorted case ID followed by replicate ordinal. Limits are 100 cases, 100
replicates, 1,000 listed attempts, 20 compared attempts, and 256 returned bound
samples.

The parity command separates immutable request identity, strict native
semantics, runner classification, and clock-positioned observations:

```powershell
poecraft-solver-lab-parity `
  --direct-ledger build/solver-lab/direct/ledger.json `
  --lab-catalog build/solver-lab/catalog.sqlite3 `
  --direct-wall-seconds 1.0 --lab-wall-seconds 1.0 `
  --output build/solver-lab/qualification.json
```

## CLI Automation And Supervision

Use `supervise` for a durable headless dispatcher, or `run-until-idle` for a
bounded batch that exits when the queue drains. Other terminal processes can
submit, inspect, compare, cancel, and export through the same CLI while the
supervisor owns dispatch. No GUI needs to be open.

The CLI exposes a finite operation vocabulary, including the complete
draft/validation/revision lifecycle. It has no arbitrary shell, SQL,
unrestricted path-write, benchmark argument bag, mechanics override, or remote
worker authority. Mutations use complete canonical-request idempotency, and
every response uses the structured `solver_lab_operation_result_v1` JSON
envelope. Detailed native output remains in immutable attempt artifacts rather
than being truncated into another transport protocol.

`get_run_summary` preserves the bounded native attribution surfaces needed
to diagnose completed and partial work: `action_control`,
`automatic_candidates`, `incremental_action_envelope`,
`action_envelope_ledger`, `operator_lineage`,
`cooperative_scheduler`, `carrier_ladder`, and `missing_frontier`.
These are reads of native evidence, not orchestration authority. Retrying an
immutable terminal job revalidates its complete dispatch identity; if source,
executable, artifact, or another dispatch component no longer matches, the
job becomes `dispatch_refused` without creating a worker or attempt. Clone
or submit a new immutable revision when a new identity is intended.

The catalog has one durable dispatcher owner. A second `supervise` process
detects a verified-live owner, remains control-only, reports the existing
owner, and exits; it cannot multiply the catalog's worker limit. A successor
may replace ownership only after proving that the recorded dispatcher process
identity is absent, and it reconciles the replaced owner's running/finalizing
attempts before new dispatch. `--poll-seconds`, `--max-workers`,
`--memory-budget-bytes`, `--worker-headroom-bytes`, and
`--global-safety-reserve-bytes` are the dispatcher/resource launch options.

## Statuses, Resources, And Recovery

- `queued`: durable work awaiting dispatch.
- `blocked`: host-memory admission or exclusive-drain requirement currently
  prevents dispatch; this is not a solver proof result.
- `running` / `canceling`: one owned native process and lease are live.
- `finalizing`: files are being validated and hashed before atomic terminal
  publication; its reservation remains owned.
- `dispatch_refused`: complete dispatch-time identity no longer matches the
  immutable submitted request; no preclaim worker was started.
- `orphan_quarantined`: the original worker is verified live or cannot yet be
  proved absent. Its lease/reservation is retained and retry/clone-as-retry is
  blocked.
- `completed`: the worker produced its final report. This can contain an exact,
  bounded, state-cap, or solver-owned resource-cap result.
- `partial`: a watchdog/orphan path retained a valid partial observation.
- `canceled`: verified process-tree cancellation completed.
- `failed`: runner error, crash, OS-like OOM, or another terminal failure.

Host accounting exposes the native `max_solver_owned_bytes` cap, per-worker
host headroom (512 MiB by default), their total worker reservation, and the
global safety reserve separately. Only the native cap is solver/bound/proof
authority. Queue pause stops new dispatch only; running pause is intentionally
unavailable.

On restart, the supervisor reconciles stale ownership, sessions, and leases and
verifies PID creation identity before changing attempt state. A proved-absent
worker with a valid final report is truthfully recovered as completed. A
verified-live or possible-live worker is quarantined without releasing its
reservation; periodic reconciliation publishes only after absence is proved.
Retry creates a new attempt ordinal and directory. Do not delete or edit
catalog rows or attempt artifacts to retry work.

Closing the GUI or interrupting `supervise` normally stops new dispatch and
does not cancel live work; the non-daemon supervisor drains each worker within
its immutable watchdog. For a shorter bounded shutdown, first pause the queue,
cancel any selected live job through the CLI, wait for terminal
acknowledgment and released reservation, then close the client. For maintenance,
copy the catalog and its `-wal`/`-shm` siblings together only after dispatcher
ownership reports released.

## Unattended Qualification

The repository-owned qualification harness writes each run to a new immutable,
ignored directory below `build/solver-lab/unattended-hardening/`. The
accelerated mode covers the deterministic crash, idempotency, dispatch,
watchdog, terminal-publication, integrity, quarantine, cancel/retry,
dispatcher-death, and dual-owner matrix in ordinary test time:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
py -3 -m poecraft_ingest.solver_lab_unattended_qualification `
  --root . `
  --output-root build/solver-lab/unattended-hardening `
  --accelerated
```

The low-duty soak links that accelerated result, exercises the isolated
catalog lifecycle plus one real native control, reacquires and releases a
no-work dispatcher at every audit interval, and rechecks provenance, artifact
hashes, process survivors, duplicate attempts, and lease/reservation state:

```powershell
py -3 -m poecraft_ingest.solver_lab_unattended_qualification `
  --root . `
  --output-root build/solver-lab/unattended-hardening `
  --soak `
  --duration-seconds 21600 `
  --interval-seconds 600 `
  --accelerated-evidence <accelerated-result.json>
```

The soak enforces at least 21,600 seconds of wall time. An interrupted or
short rehearsal ledger remains useful integration evidence but cannot set
`passed: true` and is not overnight qualification. Bulky catalogs, attempt
artifacts, logs, and bundles remain ignored; only the harness, bounded tests,
and summarized execution evidence are committed.

## Artifacts And Limitations

Each attempt directory may contain `report.json`, `partial.json`,
`worker.log`, `supervisor-error.json`, and a `strategies/` directory, depending
on its outcome. The controlled Lab root also owns `gui-activity.log`. The
supervisor validates and hashes the required evidence before one catalog
transaction indexes artifacts, terminalizes attempt/job, emits events, and
releases the lease. Every terminal consumer rechecks owned path, size, and
SHA-256 before parsing or export; legacy unindexed terminals are disclosed but
not trusted. Investigation bundles contain bounded summaries, hashes, events,
reproduction argv, and a bounded log tail; they do not copy arbitrary files or
full telemetry.

The Lab does not provide a second graphical modifier editor, live solve checkpoint/resume, running pause, remote or
multi-machine workers, authentication, cloud execution, learned guidance,
full strategy-graph rendering, automatic cap tuning, or another evaluator
backend. It also does not make the PDR control exact: that case remains a
truthful solver-owned resource-cap control.
