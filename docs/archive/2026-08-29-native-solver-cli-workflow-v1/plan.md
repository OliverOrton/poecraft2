# Native Solver CLI Workflow v1

**Status: completed on 2026-08-29.** Selected by Oliver from source checkpoint
`d690a954ccb47d2d041b2faaf9b2963b37eba231` following Native Solver Lab
CLI-First MCP Removal v1.

Parent: [Active work](../README.md)

Progress and exact evidence belong in the [execution log](execution-log.md).

## Objective

Add only three high-leverage workflows to the existing
`poecraft-solver-lab` JSON CLI:

1. versioned JSON matrix-definition files;
2. one bounded JSON-Pointer case-derive operation; and
3. one submit → targeted wait → compact summary command.

Every workflow composes `SolverLabService`, the existing catalog, native
validation, immutable revisions, `SolverLabSupervisor`, and current attempt
artifacts. This boundary creates no second runner, identity, validation,
artifact, or mechanic authority.

## Starting State

- Branch `main` is synchronized with `origin/main` at
  `d690a954ccb47d2d041b2faaf9b2963b37eba231`.
- The only dirty path is the protected untracked three-byte file `0`.
- The repository-specific MCP surface is absent. The installed JSON CLI,
  service, catalog, supervisor, GUI, and saved artifacts are qualified.
- The CLI already exposes the complete low-level draft, revision, submission,
  queue, attempt, comparison, evaluation, and bundle lifecycle.
- Meaningful case edits still require a complete replacement document, matrix
  selections live in shell arguments, and ordinary runs require several
  separate commands.

## Locked Boundaries

- Do not change solver, ladder, fragment, proof, mechanic, C ABI, WASM,
  product, GUI, or catalog-schema behavior.
- Do not restore MCP, add direct SQLite access, or launch native work outside
  the existing supervisor/worker path.
- Do not add table output, JSONL, a response-envelope replacement, a broad
  case editor, semantic editing aliases, automatic cap tuning, YAML, or a new
  dependency.
- Frozen cases remain immutable. Derived documents must pass the existing
  structural/profile checks and native `--validate-only` authority before an
  immutable revision is saved.
- Case patch paths are a small registered vocabulary. Reject unknown paths,
  duplicate or overlapping patches, invalid JSON Pointer escapes, and bounded
  collection overflows before mutation.
- Matrix definitions use JSON and `solver_lab_matrix_v1`. Expansion is
  deterministic, capped, and content-addressed. Resolved manifests live only
  under the controlled Solver Lab build root and bind definition, case,
  revision, execution, source, executable, artifact, profile, economy,
  coordinate, job, and replicate identities.
- Replaying unchanged matrix inputs is idempotent. Changed source,
  executable, artifact, base case, or definition produces a visibly distinct
  resolved matrix identity.
- A one-shot wait observes only its submitted job. When it owns dispatch, its
  supervisor may dispatch only that job; when another legitimate dispatcher
  owns the catalog, it polls only that job's durable state.
- Stdout remains one stable JSON result. Compact progress may go to stderr.
  Large evidence remains in existing files and targeted reads.
- Preserve the unattended-hardening limitation: its six-hour soak was
  owner-waived, not passed, and is not overnight qualification.
- Preserve untracked `0`; do not read, delete, clean, edit, stage, rename, or
  commit it. Stop on any other unexpected dirty path.
- Keep commits local and end each commit message with
  `Co-authored-by: Codex codex@openai.com`.

## Gate 0 — Activate And Map The Workflow

1. Activate this plan in `docs/README.md`, `docs/active/README.md`, and
   `HANDOFF.md`.
2. Record the exact branch, source, upstream, protected dirt, installed CLI,
   absent MCP surface, released dispatcher, and existing service owners.
3. Commit the selected boundary before implementation.

## Gate 1 — Versioned Patch And Matrix Contracts

1. Add mechanics-neutral workflow helpers and schema constants for
   `solver_lab_matrix_v1` and `solver_lab_resolved_matrix_v1`.
2. Implement RFC 6901 decoding for a registered, bounded set of case paths:
   watchdog/bounded finish, disabled families, minimum goal satisfaction,
   goal slots or slot minimum tiers, and approved solver caps.
3. Validate and canonicalize values, reject path overlap, and apply patches to
   a deep copy before the existing case/profile validators run.
4. Validate matrix name, base case/revision selector, unique axes, values,
   replicates, priority, and Cartesian limits. Expansion order must be stable.

## Gate 2 — Derive Case

1. Add `SolverLabService.derive_case` as a composition of create draft,
   bounded patch, update draft, optional native validation, and optional
   immutable save.
2. Bind the complete base identity and canonical patch set to the composite
   idempotency key. Give each child mutation a complete derived key so a
   partial interruption can replay safely.
3. Add CLI `derive-case` with one required base selector, repeated `--set` and
   `--set-json`, `--validate`, `--save`, and required idempotency key.
4. Return draft, validation, revision ID, revision digest, and applied patch
   identities through `solver_lab_operation_result_v1`.

## Gate 3 — Matrix File

1. Add `run-matrix-file FILE` to parse one bounded JSON definition and call a
   typed service operation.
2. Deterministically derive and save every coordinate revision.
3. Resolve every replicate's complete execution request and planned job ID,
   then atomically create an immutable resolved manifest before any planned
   job becomes dispatchable.
4. Submit the planned jobs with complete idempotency keys under one catalog
   experiment. Replay the same resolved identity without duplicate revisions,
   experiments, jobs, or files.
5. `--wait` may use the existing supervisor and must return compact per-job
   terminal summaries plus the resolved-manifest path and SHA-256.

## Gate 4 — One-Shot Run

1. Add `run` for a frozen case or immutable revision. Derive a default
   idempotency key from a dry-resolved complete request, while permitting an
   explicit key.
2. Without `--wait`, return the ordinary submitted job. With `--wait`, acquire
   filtered dispatch when possible or observe the verified existing owner.
3. Poll only the selected job, stream status changes to stderr, and enforce a
   bounded wait timeout without changing the native watchdog.
4. Emit one stdout JSON result with job/attempt IDs, artifact directory,
   terminal status, selected compact summary fields, bounds, stop,
   request/core identities, strategy identity, and exact-evaluation summary.

## Gate 5 — Terminal-Only Acceptance

A fresh isolated CLI catalog must complete the required workflow without GUI,
MCP, SQLite editing, full-document replacement, or an alternate runner:

1. derive a frozen four- or five-mod case;
2. patch a goal field, disabled family, solver cap, and watchdog;
3. native-validate and save the revision;
4. run it to a bounded or completed terminal result;
5. inspect the compact summary;
6. cancel a genuinely running second attempt and verify process-tree and
   reservation release;
7. compare two attempts and read recorded native exact evaluation when a
   strategy is available; and
8. run and replay a versioned matrix file, verifying stable resolved and job
   identities.

Run the complete focused Lab suite and then one final
`powershell -File scripts/test.ps1`. Update the stable Solver Lab reference,
execution log, evidence, and handoff; archive the boundary; commit locally;
do not push.

## Completion Contract

This boundary passes only when the three requested workflows are small,
bounded compositions of existing authority; changed inputs are identity-
visible; unchanged replays are stable; real cancellation and artifacts remain
safe; the terminal-only acceptance workflow and full repository pipeline pass;
and no forbidden solver, product, GUI, MCP, database, or alternate-runner
surface appears.
