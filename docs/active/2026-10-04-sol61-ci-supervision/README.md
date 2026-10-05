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
and real-artifact workflows. Real-artifact/full-ingest and hosted qualification
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

Source and focused local tests are qualified. Hosted/full-artifact integration
is unrun; the original completion-failure cause stays open. No main merge,
push, deployment, dev-server restart, WASM activation or economic claim follows.
Parent coordinates branch publication and integration after patch review.
