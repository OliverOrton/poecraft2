# Full-Primitive Fossil Setup And Carrier-Ladder Qualification v1 Execution Log

**Status: active.** This log records chronological execution evidence for the
[plan](plan.md).

Parent: [Active boundary](README.md)

## 2026-08-29 — Boundary Activation

Oliver selected the full-primitive Fossil setup and carrier-ladder
qualification boundary for an unattended long-running goal.

Starting repository state was verified before mutation:

- HEAD: `f975c0cb4eb2b4069473e6f9281c208d40c8f29d`;
- branch: `main`;
- upstream `origin/main`:
  `f975c0cb4eb2b4069473e6f9281c208d40c8f29d`;
- tracked worktree: clean; and
- sole status entry: protected untracked `0`.

The protected file was not read or modified. No implementation, build, test,
CLI submission, service restart, or run mutation occurred during activation.

The retained starting Fossil witness from the prior generated-envelope
boundary is:

- revision `case-rev-c1b3c87d43e16122687521d348cf38b5`;
- content SHA-256
  `c1b3c87d43e16122687521d348cf38b5db2a1c8ba748a130ef15f364ec4aae88`;
- job `job-709e39e0-5ead-4fce-9a9c-dca35cd726aa`;
- attempt `attempt-2db95f4b-ebc7-4fab-a17c-e4e55a246cb8`;
- watchdog terminal after 60.161 seconds;
- no native partial report or phase owner;
- zero progress samples;
- empty worker log identity
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`;
  and
- supervisor cleanup reported no surviving process.

This evidence selects setup attribution first. It does not yet authorize a
Fossil, action-envelope, ladder, mechanic, cap, ABI, strategy, or product
behavior change.

## Evidence Recording Contract

For every persistent run, append a bounded record containing:

- case and immutable revision IDs;
- request/content/core/component identities;
- job and attempt IDs;
- artifact paths, IDs, sizes, and SHA-256 hashes;
- selected setup, action, ladder, work, memory, bound, policy, and terminal
  fields;
- cancellation, process-tree, and reservation disposition;
- before/after comparison and conclusion; and
- the source commit and executable/artifact identities used by the run.

Raw JSON reports and bundles remain in their structured artifact locations and
must not be pasted wholesale into this log or agent context.
