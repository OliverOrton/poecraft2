# Native Solver Lab Case Authoring Result

**Status: completed.** Source checkpoint: `88d65a9`.

## Outcome

The Lab is no longer limited to its five frozen cases. It now stores editable
local drafts and content-addressed immutable revisions, validates every save
through the native benchmark's `--validate-only` path, and binds submitted
jobs to the revision ID, canonical content digest, case/corpus snapshots,
profile, and effective native request. Frozen fixtures and saved revisions
remain read-only.

The GUI has a fifth **Cases** tab with template creation, frozen/revision
cloning, Calculator clipboard import, JSON plus common run controls, native
validation, revision save, export, and submission. Calculator has a **Copy Lab
case** action that transports the concrete carrier, supported affix flags,
influence/Eldritch state, product goal, diagnostic family exclusions, and
pinned Allflame economy without making TypeScript a crafting authority.

CLI and MCP now expose the same bounded lifecycle. The MCP surface grew from
21 to 31 typed tools without arbitrary filesystem paths, shell, SQL, mechanic
overrides, or native argument bags. The installed stdio executable was
registered as the user-local Codex server `poecraft2-native-solver-lab`, rooted
at this checkout. A real protocol handshake reported the expected server, 31
tools, and five frozen cases. A newly started Codex task or app restart is
required to load a server added while a task is already open.

The current Lab profile remains unchanged: fixed Allflame source identity,
`calculator_product_v1`, goal-progress-gated reforges on, voluntary/economic
Restart off, automatic Imprint programs off, paid native Fracture miss
replacement retained, and exact junk-free terminal success. Unsupported
active Imprint checkpoints and special item flags are refused during browser
export rather than silently discarded.

One directly relevant ownership defect was repaired during acceptance:
read-only SQLite contexts had committed without closing their handles. They
now close deterministically, so temporary case workspaces and MCP/test
lifetimes release the catalog immediately on Windows.

## Acceptance

- Lab unit/integration, GUI-controller, supervisor, parity, contracts, and MCP
  tests: `26 passed`.
- Full web suite and TypeScript type-check: passed, including the new
  Calculator export contract and `28/28` release-WASM worker checks.
- Direct native authoring probe: native validation passed one generated case;
  immutable revision save and dry-run submission resolved as
  `local_revision`.
- Installed MCP stdio probe: expected server identity, `31` tools, and `5`
  frozen cases.
- Full `scripts/test.ps1`: passed on the final rerun, including ingest/DB/data
  validation, `3,417,290` native checks, all `12` solver benchmark
  specifications, web/WASM, and remaining repository layers.

The first full-pipeline attempt encountered a timing-only pre-existing WASM
finalization-cancellation assertion after all native checks passed. The exact
test passed immediately in isolation and again in the complete successful
pipeline. No cancellation or solver change was made.

No compiled-strategy 10,000-run verification was required because this
boundary changes orchestration and authoring only. No native solver,
mechanics, ABI, compiled data schema, strategy vocabulary, or WASM module was
changed.

Oliver still owns rendered GUI and browser usability review.
