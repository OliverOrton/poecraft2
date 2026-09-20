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

## M0 retained-pool mutation map

| Existing owner / function | Access found at M0 |
|---|---|
| `solver_solve_types.hpp`, `IncumbentPortfolio` / Impl alias | Public retained vector and mutable `certified_fallback_portfolio` reference. Output and pending aliases remain a distinct scope. |
| `solver_solve_constructive.cpp`, `retain_certified_incumbent` | Provenance validation, identity deduplication, four-entry reserve/replace/push/sort, retained-byte refresh and observations. Noncompetitive fifth entry is handled without storing. |
| `best_current_certified_fallback` | Erases invalid entries and updates counters during lookup, then selects eligible evidence. |
| `commit_output_incumbent` | Retains displaced compiled evidence before replacement; reads the pool for lineage. Failed admission can preserve old output. |
| `solver_solve_finish.cpp`, publication coroutine | Direct min/move/erase transfer, retained candidate checking and three clear/shrink paths after moving the selected fallback. |
| `solver_solve_return_bridge.cpp` | Calls retention and pruning lookup; uses complete private root-only artifacts. |
| `solver_solve_telemetry.cpp` | Reads front/capacity/payload; fast and full ledgers subtract four compatibility pointer shells. |

At M0 these were external writers, including a mutable vector reference held
across verification suspension while fallback lookup could erase entries.
The alternative-shadow coroutine also borrowed certificate entries across
suspension. The completed M2 boundary below addresses both lifetimes. Original
aliases were output, pending candidate, retained vector and finalization upper.

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

## M1 gate disposition

M1 was committed locally as `75504e4ee2d5700629b23e2ed06eec63812cbd78`.
It initially stopped because this Windows host had no actual non-Windows checkout.
On September 20 Oliver explicitly said **“skip non windows”**. That lane is waived,
not passed. The available M1 checks and frozen-data route qualified continuation
into M2/M3. Economy stays frozen; hosted CI remains unobserved and no push is
selected by this programme.

## M2 retained ownership boundary

All selected retained-pool writers now go through the existing
`Impl::IncumbentPortfolio`: admission/deduplication, four-entry replacement,
explicit pruning, verification staging, controlled take and release. The mutable
`certified_fallback_portfolio` alias and external vector mutations are removed.
Storage ordering and the handled-without-storing outcome are unchanged. Admission
uses the existing provenance and original-cost comparator; lazy memory queries
retain their previous decision points. A staged copy protects the displaced
witness if allocation fails.

`retained()` exposes a const borrowed view. The formerly mutating best lookup is
split into explicit pruning and passive selection, with a clearly named combined
operation at the same existing service boundaries. Progress reads neither verify
nor hash whole graphs. No second eligibility cache or result publisher was added.

Verification moves a complete entry into coroutine-owned storage; the vector
keeps its identity slot. Views and ordering resolve the slot to that bundle.
Pruning, replacement and release cannot invalidate graph/certificate references
inside suspended verification or its alternative-shadow callback. Scope exit
restores only an attached slot. Detached payload survives until work completion
or cancellation, remains charged, and cannot be taken for publication while its
verification owner is active. Materialized-but-unverified entries remain distinct
from independently certified/evaluated proper executable entries. Complete private
root-only assertions still use the existing handoff and cannot become statewise
parent values. Final normalization, classification and sealing remain publication's.

Output/pending aliases, proposal construction, scheduling and cooperative setup
remain outside this selected boundary. The dependency surface is narrower because
consumers receive const views or moved complete bundles, not a mutable vector or
an unrestricted new manager. No mechanics, objectives, modes, priorities, caps,
numerical tolerances or browser defaults changed.

### Memory and focused evidence

Windows 64-bit layout changes: `Impl` 75,512 → 75,520 bytes; portfolio 4,496 →
4,512; candidate remains 1,608; pointers are 8. Two active-entry fields add 16
bytes; removing the alias removes 8. Both fast/full ledger offsets change from
four to three pointer shells, so charged structural storage rises by 16 bytes.
Reserved vector slots, staged dynamic payload, real owner/verifier coroutine
frames and copy overlap remain charged. The initial checkpoint publishes frame
charges before verifier admission. Release does not refund work.

The selected-fallback fixture checks deterministic ties, identity deduplication,
four-entry capacity, handled fifth entry, cap refusal preserving a valid fallback,
1,000 passive reads, explicit invalidation, owned transfer and suspended partial,
complete and detached-entry lifetimes. Its graph strings/flags are structural
fixtures, not native certificates. Existing bounded-Finish, return-bridge and
proof-handoff fixtures qualify native complete/partial authority separately.
The initial detached-byte assertion compared the producer's spare capacity with
its copied bundle and failed; it now measures the actual retained allocation.

## M3 qualification

M0–M3 are complete on the selected Windows scope. The selected native suites pass (2,440 retained
selection, 202 bounded Finish, 648 return bridge and 49 proof handoff checks).
The old-header canary preserves the 200-byte progress ABI and adjacent storage;
sequence/trace reads are passive. The full native API selector passes 2,899
checks. That existing selector also invoked its embedded 10,000-run product
fixture; this was not a new policy campaign and was broader than needed for this
boundary. No separate Simulator qualification is selected.

The rebuilt release WASM preserves its finish-TU O1/non-LTO exception and other
flags. All 18 binding tests pass with the wrapper's documented discovery root;
the earlier direct command omitted that root and failed collection. Knowledge
lint passes all 25 claims with the existing open-claim warnings. An intermediate
Windows text-encoding error in the claim edit was caught by lint and corrected
from the original UTF-8 bytes before adding the two scoped history notes.

The serial [qualification adapter](probes/qualify.py) delegates to the existing
corpus/worker owners. It preflighted the frozen CB01 product8/240-second request,
then ran preserved `75504e4` and the changed native executable with independent
original-price evaluation, followed by actual Calculator Finish. Native arms
use the same goal/start/actions/prices, data, 1-GiB solver cap, other work/checker
caps, retention treatment and host reservation; only the executable is the
intentional treatment. Timed runs had fresh host admission and 315-second
external cleanup watchdogs. The existing typed report comparator admits one
pair with no exclusions or reported regressions. This is one serial pair, not a
counterbalanced speedup experiment.

| Observation | Baseline | Retained owner |
|---|---:|---:|
| Original-cost evaluated upper | 85,558.70618560436 | 85,558.70618560436 |
| Expected primitive executions | 8,407.202314771383 | 8,407.202314771383 |
| Independent success / off-policy mass | 1 / 0 | 1 / 0 |
| Compiled graph | 469 nodes / 1,117 edges | Same bytes |
| Solve time | 240.538 s | 240.646 s |
| Native peak owned bytes | 253,288,221 | 253,288,237 |
| Fast/full solve-ledger maximum overestimate | 16 bytes | 16 bytes |
| Publication / invalidation | 1 success / 1 invalidation | Same |

Both reports preserve `bounded_feasible`, `requested_bounded_finish` and the
separate `refused_unsupported_action` status. Expectations pass; neither result
claims exact closure. The selected witness hash is `3fc9a56d2a8c1049` and graph
SHA-256 is `7df8b8be00a5c4513d6d5fb761605b5c399957c714079d8c39a103add036dc02`.
Both independently evaluated reports select the cheapest verified candidate.
Peak memory differs by the expected 16 structural bytes. Live memory and
observational JSON sizes are recorded separately, not treated as process peaks.

Actual Calculator + worker + fresh release WASM delivers a usable policy.
The complete graph equals the previous independently evaluated A7 manual-worker
graph under the existing attached-economy projection; goal, actions and original
prices also match. The raw comparison with native is false because Calculator
adds 469 display positions. The receipt separately checks that positions and
attached economy are the only differences; no executable field is removed.
There is no new graph requiring a Simulator campaign.

| Actual Calculator clock interval | Measurement | Gate disposition |
|---|---:|---|
| UI Finish intent → UI usable | 3.571289 s | Pass ≤10 s; historical 5.483 s remains evidence |
| UI worker request → UI usable | 45.185117 s | Pass ≤65 s in this probe |
| Native begin request → begin complete | 12.598996 s | Fail ≤250 ms |
| Maximum native stepped call | 369.019 ms | Fail ≤250 ms |
| Setup Cancel | Not rerun | Historical 23.081 s / 1 s failure remains |

Each subtraction uses a single clock and the delivery interval starts at the
worker request. This single current 65-second pass does not erase A7's earlier
failure or establish that the structural change fixed latency. No matched WASM
speedup claim is made. The Conquest-four WASM C5218.04094969 versus native
C3746.13194095 gap remains open. Bow/Ring/Amulet, Regalia and historical native
high-water policies were not replaced or rerun. Rendered UI review remains
Oliver's responsibility; no browser defaults changed.

The native batch and Calculator process all completed without survivors. Its
first adapter process exited 1 **after** successful evidence writes because its
final check used `returncode` instead of the supervisor's `exit_code`. The field
was corrected; the read-only [summary](probes/summarize.py) independently checked
all saved exits, evaluation and identity results. No timed case was repeated to
repair the adapter. The full web `npm test` chain and separate `npm run typecheck`
pass against the new release artifact. Bulk logs remain on disk.

[Build identities](M3-build.json), [qualification](qualification.json),
[validation commands and log hashes](M3-validation.json), both preflight/admission
receipts and the Calculator process receipt provide the exact reproducible
inputs and dispositions. Terminal execution used batched work and 300-second
empty-input waits. Outer tool cells can still yield earlier; no solver telemetry
was repeatedly queried through model turns and no new supervisor was added.

## Closeout and next decision

Research disposition: the packet is preserved once; M0–M3 are implemented and
qualified on Windows, with non-Windows explicitly waived. The existing source
map, publication, resource/resumption and mathematical interruption owners now
record the enforced boundary and actual storage/lifetime correspondence.
CLM-0002 keeps its conditional accepted status; CLM-0021 remains open. No new
claim ID, numerical endpoint, general native proof or latency theorem is asserted.

The next recommended decision is the queued two-owner cooperative setup programme,
subject to Oliver selecting it. It is not started. Output/proposal/scheduler
migration, response caches, saved-Ring diagnostics and action-budget optimization
remain unselected. There is no automatic restart or push. Economy remains frozen.
Protected root `0` and unrelated work were not inspected or changed.
