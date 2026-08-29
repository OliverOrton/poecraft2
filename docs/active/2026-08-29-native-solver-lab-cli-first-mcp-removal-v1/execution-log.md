# Native Solver Lab CLI-First MCP Removal v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Gate 0 activation

### Repository boundary

- HEAD: `ecf778c0ab9b8e3ebbeb285ca7748774845c325f`
- Parent: `13a53219f9585b048d1a0156c8debcaa13cef933`
- Branch: `main`
- Upstream divergence: `origin/main...HEAD = 0/0`
- Status: only protected `?? 0`; no other dirty path

### Existing automation surfaces

- CLI entry point: `poecraft-solver-lab`
- CLI schema: `solver_lab_operation_result_v1`
- MCP entry point selected for removal: `poecraft-solver-lab-mcp`
- MCP server version selected for removal: `0.2.0`
- MCP typed tool count selected for removal: `31`
- User-local registration selected for removal:
  `poecraft2-native-solver-lab`
- Registered command:
  `poecraft-solver-lab-mcp.exe --root C:\Users\Oliver\Documents\poecraft2 --with-supervisor --max-workers 1`

### Live MCP process inventory

Four exact repository-specific launcher/process trees were live:

| launcher PID | Python PID | disposition at activation |
| ---: | ---: | --- |
| 8264 | 53128 | catalog dispatcher owner |
| 18448 | 18468 | control-only |
| 12180 | 3500 | control-only |
| 13808 | 25780 | control-only |

The active catalog dispatcher was
`supervisor-e1363676-96b8-486d-bb2c-1050479f1f1e`, PID `53128`. The queue
was resumed. Job counts contained only terminal states: 26 canceled, 71
completed, 34 dispatch-refused, 11 failed, and 12 partial. No active host
reservation or nonterminal job/attempt was observed.

### Existing CLI evidence

Read-only CLI calls returned:

- one `native_allflame_no_imprint_v1` profile;
- exactly seven frozen cases;
- `fragment-clean-one-goal-renewal-control-v1`;
- `fragment-clean-one-goal-renewal-shadow-v1`; and
- the same catalog dispatcher, queue, host-resource, and durable session
  information available through the shared service.

This establishes that removal does not require a replacement serialization
layer. Subsequent gates retain and qualify the existing JSON and artifact
contracts.

## 2026-08-29 — Gate 1 transport-independent coverage

The two tests that had exercised service analysis beneath the MCP filename
were retained under unattended-hardening coverage. They verify:

- deterministic bound-trace downsampling from five samples to three;
- strategy graph/action and recorded independent-evaluation summaries;
- two-attempt identity/result comparison;
- dry-run bundle preview;
- stable bundle replay under the same idempotency key;
- bounded investigation-bundle content; and
- bounded, sorted, idempotent case/replicate matrix selection.

The real Windows cancellation test now qualifies service, CLI, and GUI paths.
Every path reaches terminal `canceled`, removes the grandchild process tree,
observes cancellation, and releases its lease and host reservation. The MCP
leg was removed without weakening the CLI assertion.

The unattended accelerated list now cites direct catalog singleton and
concurrent dispatcher-acquisition tests instead of combined stdio launchers.
The existing JSON CLI identity and complete-payload idempotency tests remain.

Focused result: 7 passed in 10.37 seconds. Complete Solver Lab result after
removal: 67 passed in 44.40 seconds.

## 2026-08-29 — Gate 2 repository MCP removal

Removed:

- `poecraft_ingest/solver_lab_mcp.py`;
- `tests/test_solver_lab_mcp.py`;
- optional dependency `mcp>=2.1,<3`; and
- console entry point `poecraft-solver-lab-mcp`.

Current service/contract docstrings, the frozen-corpus install guide, and the
stable Solver Lab reference now describe the CLI, GUI, typed service, and
supervisor. The stable reference documents headless `supervise` and bounded
`run-until-idle` operation through the existing JSON envelope. Historical
archives and their current index summaries retain truthful historical MCP
qualification wording.

## 2026-08-29 — Gate 3 CLI qualification

The editable ingest package was reinstalled from the changed project. Windows
initially held the removed launcher open because four registered servers were
still live; Gate 4 shut down those exact processes before the successful
installation check. Pip's inert rollback metadata and the obsolete launcher
were moved out of the Python environment and then sent to the Windows Recycle
Bin. They are recoverable there but are no longer installed or runnable.

Installed-surface result:

- `poecraft-solver-lab.exe`: present;
- `poecraft-solver-lab-mcp.exe`: absent;
- `poecraft_ingest.solver_lab`: importable;
- `poecraft_ingest.solver_lab_mcp`: absent; and
- current `poecraft-ingest` entry-point metadata lists only the CLI and parity
  Lab commands.

An isolated installed-CLI exercise returned:

- schema `solver_lab_operation_result_v1`;
- one profile and seven frozen cases;
- both fragment control/shadow case IDs;
- a dry-run submission with core identity
  `293afa7f4961cd46eb505f931aec520132898b834237b3737bae15388b402f7a`;
- zero persisted jobs, attempts, or local revisions after the dry-run;
- successful `run-until-idle` with zero reserved bytes; and
- released dispatcher ownership with the queue resumed.

The accelerated unattended harness selected 13 direct tests and reported 22
passed in 13.27 seconds. Ledger:
`build/solver-lab/unattended-hardening/accelerated-20260829T164523.457225Z-54524`.

## 2026-08-29 — Gate 4 user-local decommission

Pre-decommission CLI audit found zero nonterminal jobs, zero nonterminal
attempts, and a resumed queue. Oliver's user-local
`poecraft2-native-solver-lab` registration was removed with `codex mcp
remove`. The four previously inventoried repository launcher trees (eight
exact wrapper/Python processes) were terminated; no unrelated Python or Codex
process was targeted.

The installed CLI then ran `run-until-idle` on the shared catalog. Supervisor
`supervisor-b0b55db4-3e0e-4d5a-a07d-8f7a712633bd` recovered the dead MCP
owner, reconciled the session, found no work, and released ownership at
`2026-08-29T16:44:37.033+00:00`. Its final status recorded zero running
attempts, zero reserved host bytes, and a resumed queue.

Final decommission checks:

- user-local MCP registration absent;
- repository MCP processes: zero;
- MCP executable: absent;
- MCP Python module: absent;
- dispatcher ownership: released; and
- queue paused: false.
