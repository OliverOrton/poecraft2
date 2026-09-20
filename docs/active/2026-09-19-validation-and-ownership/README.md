# Reliable validation and retained-artifact ownership

Selected by Oliver through [CODEX_PROMPT.md](research-inputs/package/CODEX_PROMPT.md)
against local `d13c9b835186ce35cb51aef0de6d0027f8b8a1a7`. The imported packet's 23
payload hashes match. Only M0–M3 are selected; queued cooperative setup and the
completed Ring diagnostics are outside this execution. Commits remain local.

## Starting contract and M0

The previous [qualification](../2026-09-15-verified-delivery/qualification.json)
and [A7 build](../2026-09-15-verified-delivery/build-A7.json) remain the controls.
The working tree had no programme changes at entry. Actual native binaries,
release WASM, frozen RePoE source files, canonical SQLite and the complete
compiled artifact are present locally; the ZIP contains none of those payloads.
Local source snapshot is `repoe-e4eaf06c20e1ddb4`; runtime manifest SHA-256 is
`852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`.
At entry the recorded source URLs were mutable. M1 established an immutable
upstream route and reproduced the source and runtime bytes exactly (below).
No current dataset fetch or publication of local data was performed.

| Failure | Starting disposition |
|---|---|
| Windows interpreter | Workflow selects 3.12; script can launch default `py -3` instead. Local launcher currently selects 3.14. |
| Test dependency/collection | pytest is undeclared; unittest misses top-level functions. No `load_tests` hooks found; compiled-data tests import `tests.test_ingest`, so package-root collisions matter. |
| Prerequisites | Native/data consumers run before artifact preparation. Full sources are local; clean hosted provisioning remains unresolved. Tiny compiled-data fixtures do not replace the natural corpus. |
| Raw evidence | One hosted mismatch is explained by LF-to-CRLF conversion; other hash-bound inputs require their own checks. No global normalization. |
| Research lookup | Test selects observation zero although the intended stable ID is `support`. Authored conditional history stays intact. |
| Performance / WASM | Previous begin, setup release, step and total-delivery failures and Conquest-four quality gap remain open. Finish-to-usable 5.483 s is the positive control. |

## Retained-pool mutation map

| Existing owner / function | Current access to migrate if M1 passes |
|---|---|
| `solver_solve_types.hpp`, `IncumbentPortfolio` / Impl alias | Public retained vector and mutable `certified_fallback_portfolio` reference. Output and pending aliases remain a distinct scope. |
| `solver_solve_constructive.cpp`, `retain_certified_incumbent` | Provenance validation, identity deduplication, four-entry reserve/replace/push/sort, retained-byte refresh and observations. Noncompetitive fifth entry is handled without storing. |
| `best_current_certified_fallback` | Erases invalid entries and updates counters during lookup, then selects eligible evidence. |
| `commit_output_incumbent` | Retains displaced compiled evidence before replacement; reads the pool for lineage. Failed admission can preserve old output. |
| `solver_solve_finish.cpp`, publication coroutine | Direct min/move/erase transfer, retained candidate checking and three clear/shrink paths after moving the selected fallback. |
| `solver_solve_return_bridge.cpp` | Calls retention and pruning lookup; uses complete private root-only artifacts. |
| `solver_solve_telemetry.cpp` | Reads front/capacity/payload; fast and full ledgers subtract four compatibility pointer shells. |

M1 must demonstrate intended collection and reproducible required prerequisites
before M2 begins. The map remains a migration target: **no retained-pool mutation
paths have been removed in this revision.** In particular, verification currently
holds a mutable candidate reference across coroutine suspension while fallback
selection can prune the vector. The alternative-shadow coroutine also borrows
certificate entries across suspension. A later M2 implementation must account for
these lifetimes, not just make the vector private. The four current alias shells
are output, pending candidate, retained vector and finalization verified upper;
neither layout nor fast/full memory compensation changed here.

## M1 implementation and qualification

The [compact receipt](validation.json) records exact executable/data hashes,
commands, collection counts, raw-log paths and dispositions. Logs and fresh source,
SQLite and runtime outputs remain under `out/validation-and-ownership`.

- `scripts/python-common.ps1` resolves one exact Python executable for build,
  test, format, packaging, benchmarks and project Harvest generation. Windows CI
  binds its setup-python selection through `POECRAFT_PYTHON`; SDK Python stays
  independent. `tools/ingest[test]` declares pytest without the optional GUI.
- `scripts/test.ps1` prepares and validates required native/data prerequisites
  before consumers, runs mixed pytest/TestCase families in separate processes,
  and makes Web typechecking explicit. Missing selected prerequisites fail.
  Explicit scopes report which layers were not selected. The binding suite's
  `test_stress -> test_bindings` import required preserving its discovery root;
  the initial failing collection log is retained.
- Report tests select observation ID `support` and assert kind and development
  role under authored, reversed and prepended histories. No historical records or
  expected observation fields were rewritten.
- Scoped Git `-text` rules preserve original fixture/evidence bytes, including
  both LF and already-CRLF records. The existing S8 raw-byte tests now also run in
  the Ubuntu knowledge workflow. No mass renormalization occurred.

The new metadata-only source lock points to upstream commit
`b6379c408e4a275f2d0d39655daabe44842c60f7`. All 12 source files match the frozen
snapshot exactly after the declared export encoding. The existing ingest/compiler
rebuilt a separate full SQLite database and reproduced all three runtime files
byte-for-byte, including manifest hash `852279f8…0a46eb`. The archive input/output
hashes are in the lock; the upstream correspondence and reconstruction logs are
in the receipt. The economy remains frozen by Oliver's explicit direction.

| Check | Actual disposition |
|---|---|
| Fresh Python environment | Python 3.14.2 / pytest 9.1.1; declared editable test extra installed with that interpreter's pip |
| Ingest collection | Before: unittest 26 versus pytest 151. After: pytest 156, including 2 extra ordering variants and 3 pinned-fetch cases. Actual IDs retained |
| Negative collection canary | TestCase passes and top-level test deliberately fails; pytest exits 1 |
| Required Python wrapper | 151 ingest + 12 economy + 18 binding tests pass; 2 economy subtests pass; 5 PySide6 tests deliberately optional/skipped |
| Missing prerequisite canaries | Missing binding and missing native executables each fail; no successful skip/header fallback |
| Full frozen source→SQLite→runtime | Pass, all required production bytes reproduced; no tiny-corpus substitution |
| Knowledge workflow commands | 8 knowledge tests, corrected reports/classifier, lint, metadata and byte-identical generated Markdown pass |
| Raw fixture checks | S8 suite: 10 pass. Three selected Git blobs preserve bytes with both `core.autocrlf=true` and `false` in isolated Windows checkouts |
| Web wrapper | Fresh `npm ci`, bundle preparation, full `npm test` and `npm run typecheck` pass with the unchanged release WASM |
| PowerShell / Harvest | 8 changed/new scripts parse; explicit interpreter resolution and project generator work. Generated header text is unchanged; prior CRLF versus new LF bytes are recorded separately |
| Native/WASM rebuild, new discovery, Calculator timing, Simulator | Not run/not reached for this validation-only change; existing native DLL and committed release WASM identities recorded |
| Actual non-Windows checkout | Unavailable on this host: WSL is not installed. Windows Git conversion modes are not labelled a Linux run |
| Hosted CI | Not run on this local revision; no green claim and no push |

## Gate and continuation

M0 is complete. M1 repairs are implemented and pass the available local checks,
but M1 is **not fully qualified**: the selected plan explicitly asks for an actual
non-Windows checkout check, and none is available in this session. A question
about an existing Linux/macOS checkout is pending. The packet says, “Do not begin
M2 while its affected required qualification remains unavailable.” M2 and M3 are
therefore not started/not reached; this is a safe partial delivery, not programme
completion. Existing Ubuntu CI is prepared to run the missing fixture lane when
an authorized corresponding checkout/run is available. No background workflow,
push, restart, public data upload or new host installation was performed.

Canonical build/tooling and data owners describe the implemented M1 route.
Research disposition: the packet is preserved once; its owned-witness, staged
transfer and alias-accounting arguments remain conditional guidance for M2.
There is no mathematical change, new claim ID or promotion of CLM-0002/CLM-0021.
The earlier native policies, graph/certificate authority, 200-byte progress ABI,
browser defaults, 5.483-second Finish control, other failed timing gates and
Conquest-four WASM gap remain unchanged historical evidence.

Next decision: provide/qualify the actual non-Windows checkout, then implement
the mapped retained-pool boundary under the selected M2 contract. Do not select
cooperative setup, repeat the completed Ring experiments, or launch another
policy campaign to test a collector. Work stayed sequential without subagents;
protected root `0` and unrelated work were not inspected or changed.
