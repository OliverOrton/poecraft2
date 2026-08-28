# Native Solver Lab

**Status: stable local orchestration and research-tool reference.** The Lab
wraps the native benchmark; it does not own crafting mechanics, solver policy,
proof authority, or strategy evaluation.

Parent: [Foundation](README.md)

Verified against the implemented Lab surface: 2026-08-28 @ `5255998`.

## Purpose And Authority

The Native Solver Lab is a local Windows research workbench for repeatable
native solver experiments. A persistent SQLite catalog, resource-aware
supervisor, PySide6 GUI, JSON CLI, and local stdio MCP adapter all call the same
typed Python service. Every solve still runs in its own
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

Launch the GUI from the repository root:

```powershell
poecraft-solver-lab --root . gui
```

The equivalent module command is useful before installing console scripts:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
py -3 -m poecraft_ingest.solver_lab --root . gui
```

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
typed service; UI, CLI, and MCP do not reproduce those controls.

## GUI Workflow

The GUI has four persistent surfaces:

1. **Queue & Run** submits a frozen case and shows priority, attempt, host
   reservation, phase, bounds, stop reason, artifacts, events, work, and
   memory. It can cancel, retry, clone, reprioritize, and pause/resume new
   dispatch.
2. **Compare** filters immutable attempts and compares two to twenty request
   identities, outcomes, deterministic work, memory, and strategy summaries.
3. **Strategy** shows bounded graph/action/evaluation evidence and exports a
   controlled investigation bundle.
4. **Matrix** previews and submits a canonical case-by-replicate product.
   Re-submitting the displayed batch is idempotent; **Submit new replicate
   batch** deliberately assigns a new batch identity.

The GUI polls durable state and never performs a solve on its UI thread. Oliver
owns rendered visual/usability review; automated tests exercise the controller
and widgets offscreen.

## JSON CLI

All commands accept the common `--root`, `--catalog`, `--attempts`,
`--executable`, `--artifact`, `--corpus`, and `--profile` overrides before the
operation name. Examples:

```powershell
poecraft-solver-lab --root . profiles
poecraft-solver-lab --root . cases
poecraft-solver-lab --root . submit CASE_ID --idempotency-key study-a-case-1
poecraft-solver-lab --root . submit-matrix --include-role fast_exact_three_prefix --replicates 2 --idempotency-key study-a
poecraft-solver-lab --root . run-until-idle --max-workers 1
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

## Local MCP

`poecraft-solver-lab-mcp` runs a local stdio MCP server over the same service.
Configure an MCP client to run this command from the repository root after the
optional install:

```json
{
  "mcpServers": {
    "poecraft2-native-solver-lab": {
      "command": "poecraft-solver-lab-mcp",
      "args": ["--root", "."]
    }
  }
}
```

This is a user-local client setting; do not commit client-specific paths or
secrets. The adapter exposes 21 finite typed tools. It has no arbitrary shell,
SQL, write path, benchmark argument bag, mechanics override, or remote worker
authority. Mutation tools use the same dry-run and idempotency rules as the
CLI.

## Statuses, Resources, And Recovery

- `queued`: durable work awaiting dispatch.
- `blocked`: host-memory admission or exclusive-drain requirement currently
  prevents dispatch; this is not a solver proof result.
- `running` / `canceling`: one owned native process and lease are live.
- `completed`: the worker produced its final report. This can contain an exact,
  bounded, state-cap, or solver-owned resource-cap result.
- `partial`: a watchdog/orphan path retained a valid partial observation.
- `canceled`: verified process-tree cancellation completed.
- `failed`: runner error, crash, OS-like OOM, or another terminal failure.

Host reservation is based on case metadata and available system memory. It is
not `max_solver_owned_bytes` and never becomes bound/proof authority. Queue
pause stops new dispatch only; running pause is intentionally unavailable.

On restart, the supervisor reconciles stale sessions and leases, verifies PID
creation identity before touching a process, retains valid partial evidence,
and records an orphan rather than claiming completion. Retry creates a new
attempt ordinal and directory. Do not delete or edit catalog rows or attempt
artifacts to retry work.

Closing the GUI stops new dispatch; a running non-daemon worker is allowed to
drain. To stop live work, cancel the selected job and wait for terminal
acknowledgment before closing. For a clean maintenance shutdown, pause the
queue, let running attempts finish (or cancel them), close the GUI/supervisor,
then copy the catalog and its `-wal`/`-shm` siblings together if a raw backup is
needed.

## Artifacts And Limitations

Each attempt directory may contain `report.json`, `partial.json`,
`worker.log`, and a `strategies/` directory. The catalog indexes existing files
with SHA-256 and size after termination. Investigation bundles contain bounded
summaries, hashes, events, reproduction argv, and a bounded log tail; they do
not copy arbitrary files or full telemetry.

v0 does not provide live solve checkpoint/resume, running pause, remote or
multi-machine workers, authentication, cloud execution, learned guidance,
full strategy-graph rendering, automatic cap tuning, or another evaluator
backend. It also does not make the PDR control exact: that case remains a
truthful solver-owned resource-cap control.
