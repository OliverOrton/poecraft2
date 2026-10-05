# CI supervisor lifecycle continuation — October 5, 2026

Selected patch starts from remotely verified main
`7eb16ac3d63834fd5d3256ab42483f47d264b764` on KIDS, in isolated local branch
`dot/sol61-ci-lifecycle-20261005`. The
[prior checkpoint](https://github.com/OliverOrton/poecraft2/blob/d485ff111fc6b3aafa1679bca97b9ff9bd70d2a8/docs/active/2026-10-04-sol61-ci-supervision/README.md)
retains earlier scheduling, worker, hosted and handle-identity negatives.
Only its scheduler-test ownership slice is ported; the worker, production
supervisor, catalog, workflow and native sources remain main's versions.

## Hosted negatives and causal scope

Both complete decoded job logs were retrieved through the GitHub connector:

| Run / job | Failed assertion | Pytest summary | Cancellation |
|---|---|---|---|
| [37209887436 / 111458735834](https://github.com/OliverOrton/poecraft2/actions/runs/37209887436/job/111458735834) | cancel/retry completion, line 363 | 233 passed, 1 failed, 5 skipped; 143.03 s; 14:48:09 UTC | 20:36:46 UTC |
| [37211203187 / 111462601947](https://github.com/OliverOrton/poecraft2/actions/runs/37211203187/job/111462601947) | concurrent dispatch completion, line 312 | 233 passed, 1 failed, 5 skipped; 121.90 s; 15:09:06 UTC | 20:57:59 UTC |

Each failed the unchanged five-second condition wait. Both build steps passed;
the Test step was cancelled. Neither log captures the catalog, preflight
identity or live thread state at the failed wait. No common completion-failure
cause is established. Real payload hashing latency, refusal after an identity
capture/change, and other dispatcher state remain hypotheses, not findings.

There is a separate demonstrated exit defect: these tests start a non-daemon
dispatcher, then call `stop()` only after assertions. An assertion skips that
call, leaving the dispatch loop alive even after all work completes. A forced
main assertion reproduces pytest printing its final summary while the process
stays alive, with the dispatcher and two pool threads still non-daemon. This
explains a failure-to-exit mechanism without claiming the original hosted
five-second failure has been reproduced.

## Selected repair and fixture identity

An autouse finalizer owns every supervisor created in this test module: stop
dispatch, request cancellation of unfinished synthetic work, join for five
seconds, and require the dispatcher to be gone. Test waits remain five seconds.
Ordinary overlap and exclusive drain now use worker-entry/release events, with
explicit active-worker and lease assertions. Cancel/retry retains both attempts,
their distinct directories, first log and indexed artifacts. Timed-out waits
include supervisor, job and attempt diagnostics.

These tests already replace native `_run_case` with synthetic workers. Their
compact fixture uses a 29-byte synthetic executable and a 43-byte schema-1
manifest with `files: {}`. It still calls the production request/provenance
capture and both actual dispatch preflights, and hashes executable, manifest,
source/corpus/profile/case identities. It does not exercise payload-file
hashing, production artifact validity, native execution or binding behavior.
Replacing expensive production payload I/O is appropriate for this scheduler
unit boundary; it is not production integration qualification.

The unchanged `test_solver_lab_unattended_hardening.py` owns the eight changed
identity-component refusals, restored-identity retry, atomic hashed publication
and real-artifact workflows. Its real-artifact identity/service gate passes in
the follow-up below. Full-ingest, current-source native and hosted qualification
remain separate, unrun obligations. No production validation authority is removed.

## Parent-approved finite local batch

Python 3.14 on KIDS; serial, no build, solver or browser. Baseline uses original
main tests with only the same compact fixture, and records actual preflights.
The failure probe is an explicit assertion injection, not a natural timeout.

| Arm | Test result | Outer wall time | Exit / watchdog |
|---|---|---:|---|
| Main scheduler pair | 2 passed, 13 deselected; 2.67 s | 3.409 s | 0; no timeout |
| Main forced concurrency assertion | 1 intentional failure; 1.56 s; live dispatcher/pool after summary | 10.256 s | terminated by declared 10 s watchdog |
| Candidate supervisor suite | **15 passed; 7.61 s** | 8.112 s | 0; no timeout |
| Candidate forced assertion with both workers held | 1 intentional failure; 1.12 s; both jobs canceled, no lease/thread left | 1.616 s | 1; no timeout |
| Candidate forced assertion after canceled-attempt retry completes | 1 intentional failure; 1.19 s; first log still `canceled-first` | 1.693 s | 1; no timeout |

Baseline-pair preflights all pass in 0.116–0.158 s; candidate-suite preflights
all pass in 0.115–0.157 s. These local synthetic observations do not diagnose
hosted timing. All five outer process identities prove absent. Candidate
session-end records contain only the main thread and zero reserved leases.
The final CIM check at 03:25:25 UTC finds no owned process or worktree Python/
native survivor. LOCAL was released immediately; no additional runs are selected.

Test source SHA256:
`b0673f471beef0e94c969c1f4729d4a955adeb2aac56e03a89aada4e6eb4dc77`.
The [qualification index](qualification.json) pins the receipts and fixture
scope. Full local evidence remains under
`out/ci-lifecycle-20261005/`: baseline source, assertion-injection plugin,
five process receipts/logs, complete catalog/thread/preflight observations,
immutable attempt directories, `batch.json` and `survivor-check.json`.

Complete hosted text is preserved outside the checkout under
`../ci-evidence/job111458735834.log` and `../ci-evidence/job111462601947.log`
(UTF-8, normalized line endings, BOM removed). SHA256 respectively:
`e3d1045bc708906298b9eef284603afc3964bfe6699794e2ef9dc0a3200d4413` and
`6824b7be6f0f4b4b46dc92765511b58df943e95cd0b6b5a1013bf47d6e55b881`.

Source and focused local tests are qualified. Full hosted integration is unrun;
the original completion-failure cause stays open. No main merge,
push, deployment, dev-server restart, WASM activation or economic claim follows.
Parent coordinates branch publication and integration after patch review.

## Real-artifact identity/service follow-up

Parent grants the unchanged unattended-hardening module with a 300-second
watchdog, five-second cleanup and first-failure stop. On source `fc652ca6`,
all **26 tests pass in 98.45 s**, outer 98.945 s, using qualified execution
adapter blob `f532376adbde79d9f31da1bc2d97e1b24fe2f5d9` from the prior CI
worktree. No build, native solve, fetch, ingest or data regeneration runs.

The existing historical executable is an immutable identity fixture, not a
current-source build: SHA256
`97fb69b6b61070d601f8b8127634bd3d522ac0368e5d23402d8cef8243807900`,
14,987,785 bytes, receipt source `29d9e666`, engine tree `b6085499`.
Current `fc652ca6` has engine tree `4d11a36e`; the native source mismatch is
explicit. The service contract binds source and executable hash separately;
mocked `_run_case`/validation and `run_soak(run_real_native=False)` require
stable identity, without requiring a freshly compiled native solver. This use
adds no native execution or current-binary compatibility qualification.

The fixture runtime is the existing immutable law-3 snapshot: manifest SHA256
`82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d`
and its declared 16,914,460-byte game-data and 6,740,473-byte strings payloads.
The isolated checkout references the snapshot through its ignored
`data/compiled/current` junction. No runtime file is copied or modified; only
the historical executable is copied into the fresh ignored build directory.
All executable/manifest/payload hashes match before and after the test.

Coverage includes all eight dispatch identity components, restored-identity
retry, local revision mutation, equal/unequal idempotency races, watchdog/resource
contracts and synthetic-child termination, atomic hashed publication,
preparation failure and replay, recovery/quarantine, artifact tampering,
bounded evidence/CLI/matrix workflows and accelerated unattended lifecycle.
Native computation remains mocked. This does not rerun the original scheduler
pair with production payloads or determine its hosted five-second cause.

No timeout, cancellation, unexpected descendant, survivor, pipe-drain or
cleanup error occurs. Parent identity is proved absent, and CIM at 03:49:37 UTC
finds no owned process. LOCAL is released immediately. Complete evidence:
`out/ci-lifecycle-real-artifact-r1/{receipt.json,stage.log,survivor-check.json}`;
prerequisite/source mismatch receipt:
`out/ci-lifecycle-20261005/real-artifact-prerequisites-ready.json`.
The qualification index pins their hashes. Full hosted/native/binding acceptance
and any publication remain parent-coordinated, pending work.
