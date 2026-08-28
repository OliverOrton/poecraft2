# Native Solver Lab Case Authoring Plan

**Status: active.** Selected by Oliver on 2026-08-28.

## Objective

Turn Native Solver Lab v0 from a frozen-corpus runner into a practical local
workbench without creating a second source of crafting truth. Oliver should be
able to construct a goal in the existing browser Calculator, export it, import
or clone it into the Lab, revise local run metadata and supported diagnostic
controls, validate it, save an immutable revision, and submit it through the
same native worker used by frozen cases. Codex should be able to perform the
same bounded operations through the closed local MCP adapter after a client
restart.

## Invariants

- Frozen repository fixtures remain read-only and retain their current case
  identities.
- A saved local revision is immutable. Further edits create another revision;
  existing jobs and attempts continue to identify the exact submitted bytes.
- The native engine and benchmark remain authoritative for mechanics, goal
  legality, action admission, prices, bounds, and strategy evaluation.
- The Lab may validate document shape and supervise work, but does not
  reimplement pools, weights, transitions, or crafting rules.
- Profile-owned behavior such as automatic Imprint and economic Restart is not
  silently changed by editing a case.
- Existing v0 catalog data remains readable through an additive migration.

## Gates

### Gate 0 — Contract and migration map

Map the frozen corpus, case document, request identity, catalog, worker,
Calculator request, GUI, CLI, and MCP paths. Freeze a versioned local-case
contract and the distinction between an editable draft, immutable revision,
and submitted attempt.

### Gate 1 — Persistent local cases

Implement additive catalog/storage support for local case records and
immutable revisions. Support new draft creation, cloning frozen or local
revisions, JSON import/export, validation, revision listing, and deletion of an
unsaved draft only. A submitted revision may never be overwritten or removed
through the Lab.

### Gate 2 — Native execution identity

Allow the existing supervisor/worker path to run a local revision snapshot.
Request identity and reproduction metadata must include its source kind,
revision identifier, content digest, profile identity, and effective native
arguments. Invalid or stale revisions must fail before dispatch.

### Gate 3 — GUI Cases surface

Add a persistent Cases tab that can browse frozen and local cases, create or
clone drafts, edit the versioned JSON document with focused metadata/run
controls, validate, save a new revision, export, and submit. Existing Queue,
Compare, Strategy, and Matrix behavior remains intact. Oliver owns rendered
visual/usability review.

### Gate 4 — Calculator export bridge

Add an explicit Calculator action that exports/copies the already constructed
native solve request in the Lab import envelope. TypeScript owns presentation
and shape only; it must reuse the existing solve-request construction and may
not acquire mechanic authority.

### Gate 5 — CLI and closed MCP parity

Add finite typed operations for listing local cases/revisions, importing,
cloning, validating, saving, exporting, and submitting revisions. Preserve
bounded payloads, dry-run/idempotency rules for mutations, and the MCP ban on
arbitrary paths, SQL, shell, or native argument bags.

### Gate 6 — Local Codex connection

Configure a project-scoped STDIO MCP entry that launches the installed Lab
adapter from the repository root with explicit Python/module paths. Verify the
server protocol and Codex configuration/listing. Disclose that an already-open
task cannot acquire newly configured tools until the Codex client starts a new
task or restarts.

### Gate 7 — Acceptance and handoff

Run the complete changed-layer acceptance once: Lab unit/integration tests,
MCP stdio integration, web tests and TypeScript type-check, plus the
proportional repository pipeline if the final change surface requires it.
No compiled-strategy 10,000-run verification is required unless solver output
semantics change. Update stable Lab/product documentation, evidence, active
state, and HANDOFF; create coherent local checkpoint commits with
`Co-authored-by: Codex <codex@openai.com>`.

## Non-goals

- A second graphical item/mod editor inside the Lab.
- Solver, mechanic, exactness, bound, scheduler, compiler, or strategy changes.
- Mutating committed fixtures, canonical SQLite, compiled game data, or old
  attempt artifacts.
- Arbitrary benchmark arguments, remote workers, cloud execution, or live
  running-solve checkpoint/resume.
- Starting the four-mod PDR exactness milestone in this boundary.

## Stop conditions

- Stop before implementation if a local case cannot be reduced to the same
  native request already constructed by Calculator/corpus paths without
  introducing a second mechanic authority.
- Stop before publication if a saved revision or existing attempt can change
  identity after editing.
- Stop before MCP activation if the server exposes arbitrary filesystem,
  shell, SQL, or low-level benchmark argument authority.

