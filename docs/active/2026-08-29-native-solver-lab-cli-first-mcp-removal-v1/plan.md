# Native Solver Lab CLI-First MCP Removal v1

**Status: active.** Selected by Oliver on 2026-08-29 from source checkpoint
`ecf778c0ab9b8e3ebbeb285ca7748774845c325f` after completion of Generated
Planner-Envelope Qualification And Ladder-Service Repair v1.

Parent: [Active work](../README.md)

Progress and exact evidence belong in the [execution log](execution-log.md).

## Objective

Remove the Native Solver Lab MCP transport and make the existing JSON CLI the
sole supported automation interface. Preserve the Lab's typed service,
persistent catalog, immutable case revisions, idempotency, process-tree
cancellation, host reservation policy, supervisor ownership, saved attempt
artifacts, investigation bundles, and PySide6 GUI.

The CLI already exposes the required bounded operations and emits the stable
`solver_lab_operation_result_v1` JSON envelope. Completed work already stores
full reports, logs, strategies, hashes, identities, events, and bundles. This
boundary therefore does not add another output format, live-event protocol,
or parallel orchestration layer.

## Starting State

- Branch `main` is synchronized with `origin/main` at
  `ecf778c0ab9b8e3ebbeb285ca7748774845c325f`.
- The only dirty path is the protected untracked three-byte file `0`.
- The repository provides `poecraft-solver-lab-mcp`, depends optionally on
  `mcp>=2.1,<3`, and documents a version `0.2.0` server with 31 typed tools.
- The user-local Codex configuration registers
  `poecraft2-native-solver-lab` with an embedded supervisor.
- Four MCP launcher/process trees were live at activation. Catalog ownership
  correctly resolved to one active dispatcher; the other servers were
  control-only.
- The existing CLI independently returned one profile, all seven frozen
  cases, the fragment control/shadow pair, and current supervisor state as
  structured JSON. The catalog contained no queued, blocked, running,
  canceling, or finalizing job at activation.

## Locked Boundaries

- Do not change native solver, crafting mechanics, action envelopes, planner,
  graph-fragment behavior, C ABI, strategy vocabulary, WASM, or product UI.
- Do not change catalog schemas, persisted identity definitions, frozen cases,
  immutable revisions, or artifact formats merely to remove a transport.
- The CLI must continue to call `SolverLabService`; it gains no arbitrary
  shell, SQL, unrestricted path-write, or mechanic authority.
- Preserve the existing JSON response contract and saved raw evidence. Do not
  create JSONL, tables, or a replacement RPC protocol in this boundary.
- Preserve the GUI and independent supervisor. Removing MCP must not make the
  GUI the only practical way to run or cancel work.
- Do not rewrite archived documents to erase historical MCP qualification.
  Update current contracts and indexes while leaving archives truthful.
- Do not uninstall a shared Python `mcp` distribution merely because this
  repository no longer depends on it. Remove the repository dependency,
  entry point, adapter, registration, and live repository-specific servers.
- Preserve the unattended-hardening wording: its six-hour soak was
  owner-waived, not passed, and is not overnight qualification.
- Preserve the untracked file `0`: do not read, delete, clean, edit, stage,
  rename, or commit it. Stop on any other unexpected dirty path.
- Keep commits local and end each commit message with
  `Co-authored-by: Codex codex@openai.com`.

## Gate 0 — Activate And Inventory

1. Activate this plan in `docs/README.md`, `docs/active/README.md`, and
   `HANDOFF.md`.
2. Record the exact source, branch, upstream, protected dirt, registration,
   process trees, catalog owner, queue state, profile count, case count, and
   CLI response schema.
3. Commit the selected boundary before implementation.

Gate 0 changes no runtime behavior.

## Gate 1 — Preserve Transport-Independent Coverage

1. Move service-level matrix, bound-trace, strategy, comparison, evaluation,
   and investigation-bundle tests out of the MCP-named test module.
2. Retain real CLI cancellation coverage, including terminal process-tree
   removal and reservation release, while removing only the redundant MCP leg.
3. Replace unattended qualification references to combined MCP launchers with
   existing direct supervisor singleton, recovery, and cancellation owners.
4. Confirm the existing CLI regressions still cover complete idempotency
   binding and canonical action-envelope identity disclosure.

No behavior should be weakened merely because its original regression lived
beside transport tests.

## Gate 2 — Remove The Repository MCP Surface

1. Delete `solver_lab_mcp.py` and every MCP transport test.
2. Remove the `mcp` optional dependency and
   `poecraft-solver-lab-mcp` entry point.
3. Update current module descriptions, fixture guidance, and the stable Solver
   Lab contract to describe the typed service, JSON CLI, supervisor, and GUI.
4. Verify current non-archive source and current-contract documentation contain
   no live MCP installation, registration, invocation, or dependency surface.
   Historical archive evidence may retain accurate MCP wording.

## Gate 3 — CLI And Lab Qualification

1. Reinstall the editable ingest package so removed entry points cannot remain
   as stale repository tooling.
2. Exercise the existing CLI against isolated test state for profiles, seven
   cases, draft/revision reads, dry-run submission, jobs, attempts, supervisor
   status, comparison, evaluation, and bundle behavior as applicable.
3. Run focused Solver Lab service, CLI, supervisor, GUI-stabilization, case
   authoring, fragment-isolation, and unattended-hardening tests.
4. Require stable JSON envelopes, complete idempotency conflicts, exact
   identities, verified cancellation, artifact integrity, and singleton
   dispatcher ownership.

## Gate 4 — Decommission User-Local MCP

1. Recheck through the CLI that the shared catalog has no nonterminal job or
   attempt and that the queue is resumed.
2. Remove the user-local `poecraft2-native-solver-lab` Codex registration.
3. Terminate only the exact repository-specific MCP process trees identified
   by executable and command line. Do not kill unrelated Python or Codex
   processes.
4. Run the CLI supervisor once against the idle catalog so any stale ownership
   is safely recovered and released.
5. Verify the registration is absent, no repository MCP process remains, no
   reservation remains, and dispatcher ownership is released.

## Gate 5 — Acceptance And Closeout

1. Run `powershell -File scripts/test.ps1` once after the complete change.
2. Confirm source and generated package metadata contain no Solver Lab MCP
   adapter or entry point, while the CLI and GUI remain available.
3. Update the stable contract, evidence index if needed, execution log, and
   `HANDOFF.md`; archive this boundary with a result.
4. Commit locally. Do not push.

## Completion Contract

This boundary passes only when:

- the repository no longer contains or installs the Solver Lab MCP adapter;
- Codex no longer registers or runs the repository MCP server;
- the existing JSON CLI exposes the preserved Lab operations without a new
  output protocol;
- the catalog, supervisor, cancellation, identity, and artifact guarantees
  retain direct automated coverage;
- the full repository pipeline passes; and
- no solver, mechanic, fragment, product, ABI, or WASM behavior changes.
