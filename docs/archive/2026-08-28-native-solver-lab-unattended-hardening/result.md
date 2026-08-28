# Native Solver Lab Unattended Execution And Identity Hardening Result

**Status: completed with an explicit owner-approved qualification exception.**
Final source checkpoint: `4886594eab7e499669dabdbeb674fcefd7fa84b0`.

## Outcome

The Native Solver Lab now binds every mutating idempotency key to its complete
canonical request, revalidates immutable execution identity immediately before
dispatch, enforces the submitted watchdog, and accounts separately for native
solver memory, per-worker host headroom, and the global reserve. Terminal
publication is crash-atomic and hash-indexed; consumers recheck artifact
ownership, size, and SHA-256 before reading or export. Restart recovery can
publish a proved-valid final report, while a possible-live worker is
quarantined without releasing its reservation until absence is proved.

The combined version `0.2.0` stdio MCP launcher exposes 31 bounded typed tools
and owns the singleton catalog dispatcher when available. A second launcher
remains control-only, and normal shutdown drains work and releases ownership.
No GUI, separate supervisor, arbitrary shell/SQL/path-write authority, or
ownership force-clear is required for unattended operation.

## MCP Operator Qualification

Gate 5 ran entirely through the configured MCP surface without opening the
GUI. It inventoried the retained catalog, selected immutable revision
`case-rev-15ce203781cd7935c4e0326fc1a65ca0`, submitted job
`job-4d55bb58-097e-4882-8a01-831a078ef550`, observed a substantive live
partial, canceled ordinal 1 with verified process-tree termination and
reservation release, retried to completed ordinal 2, compared identical
request identities, and exported a verified investigation bundle.

The accepted cancellation acknowledged in 486.729 ms with no survivor. The
retry completed truthfully at its native state cap, and the seven exported
artifacts verified. Bundle `bundle-a8536d99e6149e3dbf771e8b` had content
SHA-256 `792e27c404960d9342f775a732d90d2603371cc322e49ac205183779592e2e92`;
same-key export replay was stable.

## Qualification Exception

The deterministic Gate 6 harness is retained. Its accelerated matrix passed
22 cases in 17.89 seconds, and a 56.324-second real-native rehearsal passed all
invariants with 18 verified artifacts, zero survivors, zero held reservations,
and exact provenance equality.

A real six-hour soak began at `2026-08-28T13:53:58.133-07:00`. Oliver first
requested that it stop, then explicitly decided to skip the six-hour
requirement and close the boundary. The immutable partial ledger remains
`passed: false`, has no end time, and is not overnight evidence. Before it was
stopped, two audits completed; the last at 654.971 seconds found zero process
survivors, held bytes, active or quarantined leases, identity mismatches,
integrity failures, duplicate active attempts, or unhashed terminals.

This milestone is therefore accepted with the duration requirement waived by
its owner. It does **not** establish six-hour or overnight reliability. The
harness still enforces the full 21,600-second minimum if that evidence is
wanted later.

## Acceptance

- Complete Lab catalog/service/supervisor/CLI/MCP/nonvisual-GUI,
  corpus-runner, parity, migration, and unattended-hardening suite: `82 passed
  in 60.84s`.
- Real stdio initialize/tool-schema, combined dispatch, second-owner
  control-only behavior, and ownership release: passed inside that suite.
- Fresh-task MCP operator workflow: passed exclusively through MCP.
- Full `powershell -File scripts/test.ps1`: passed once from clean source
  checkpoint `4886594`, including `3,417,290` native checks with zero failures,
  all 12 solver benchmark specifications, existing 10,000-run strategy
  controls, 28/28 release-WASM checks, and all web suites.
- `git diff --check`: passed.
- Final Markdown target and root-reachability audit: 437 Markdown files, 1,800
  local targets, zero missing, and zero unreachable non-template documents
  after archival.

No native mechanics, search behavior, C ABI, canonical or compiled data,
strategy vocabulary, browser behavior, or WASM source changed. No push was
performed.

## Retained Limitation

The only omitted evidence is sustained six-hour operation. Future work must
not cite this result as overnight qualification; it may cite the accelerated
fault matrix, real-native rehearsal, two healthy partial-soak audits, complete
regression suite, and owner-approved waiver exactly as recorded here.
