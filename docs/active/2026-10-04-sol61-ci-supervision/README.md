# CI supervision and policy capability gates — 2026-10-04

Selected scope: diagnose the cancelled Windows run37155615109/job111298272328,
make deterministic capability misses visible, and retain bounded release evidence.
Start main29d9e666b9d11dbe9ba58959f2598df9495dc49d. Existing released sentinel
repair, Bow/metamod gains, frozen requests/economy and native laws remain authority.

## Initial focused batch

Hosted ingest printed2 failed,232 passed,5 skipped in333.87s at21:46:24UTC,
then stayed alive until03:35UTC cancellation. Both failed scheduler tests bypassed
stop on assertion failure; non-daemon dispatchers can explain failure to exit.
The exact original scheduling/preflight state was not captured and remains open.
The two original cases pass in3.26s locally with0.139–0.179s preflight checks;
this is neither hosted reproduction nor proof of the original failure cause.

The scheduler-only fixture now uses compact synthetic executable/artifact bytes
while retaining request hashing and both real dispatch preflights. Worker entry
and release events directly establish overlap and exclusive drain, rather than
sleep-dependent overlap. The5-second condition bound remains. Assertion failure
records supervisor/job/attempt state; an autouse finalizer stops dispatch,
cancels queued/running synthetic work and joins under a5-second bound.

Focused supervisor suite15/15 passes in7.25s. A deliberately failed assertion
after both workers enter exits1 in1.12s (outer1.73s), with no timeout or survivor.
One missing nested fixture directory and a recursive failure-probe hook were
corrected and their failed receipts retained. The initial sandboxed reproduction
failed fixture setup permissions before scheduling and was terminated at90s;
elevated execution resolved that environmental boundary, without wider limits.

Bulk receipts: out/sol61-ci-supervision/initial/. LOCAL released04:12:57UTC,
no owned heavy process. No timed solve, native build, simulation or WASM change.

## Source work awaiting qualification

Staged frozen preparation/Python/native/web checks, streamed logs, atomic start/end
records and bounded cleanup are in progress. Initial operational backstops are
600/1200/900/600s respectively,300s per CTest process,20m build and90m job.
These are containment limits justified by existing local207s native/99s web and
hosted333.87s ingest receipts; they are not hosted performance claims. Existing
tighter case/checker limits remain. Build concurrency is at most2.

Deterministic missing-candidate/report gates, complete stage tests, matching native
and real-WASM/web acceptance, source-compatible release qualification, and final
integration/publication remain pending. No real capability ceiling or percentage
allowance has been selected. Armour initially supplies coverage/classification.
The measured dirty Bow4 remains distinct from Oliver's unavailable improved Bow4.
The query branch2ce0a75b and Recomb's old failing source CI retain distinct identities.

The supplied Pro gate recommendations are incorporated only within these owners;
the old aggregate/zero-run constraints are superseded by Oliver's explicit approval.
A valid policy, bounded capability, and optimality remain independent outcomes.

## Focused Python contract batch

First staged/worker/reporter/corpus/lifecycle batch:122 passed,29 failed in20.60s
(outer21.16s), no timeout and no survivor. All24 unattended failures stopped at
the absent benchmark prerequisite in the fresh worktree;3 corpus stubs provided
empty reports rejected by the new finalization identity check;2 exposed process
exit observation differences in the new Windows job owner. Raw failures remain
in out/sol61-ci-supervision/python-contract-r1/. This does not explain the
original hosted scheduling failure.

Existing main29d9 benchmark97fb69b6b61070d601f8b8127634bd3d522ac0368e5d23402d8cef8243807900
and runtime82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d
were copied byte-for-byte after checking the production source/include/benchmark
trees and the frozen runtime manifest and declared file hashes. The lifecycle
batch uses the executable for identity hashing only; this is no solver performance
or candidate-native qualification. Materialization receipt:
out/sol61-ci-supervision/python-contract-r2-prerequisites.json.

Case stubs now carry the requested ID, and a wrong-ID negative remains rejected.
The Windows owner enumerates its job's live PID handles, checks membership and
actual exit signals, and waits on retained handles under the existing cleanup
bound. This is intended to distinguish delayed job accounting from a live child;
qualification remains pending. Native capability and sentinel negatives remain
unbuilt. The matching-source release verifier is a subsequent source slice.

Corrected batch at5f0cec03:151 passed,1 failed in38.37s (outer38.91s),
without timeout or survivor. Prerequisite, finalization and live-grandchild
negatives pass. The plain os._exit(7) case still records a live job member after
parent exit, overriding the native-crash classification. The retained receipt
contains no member PID/image; it cannot distinguish an OS observation issue
from real owned helper lifetime. This remains a blocker.

A narrow pending diagnostic slice records live owned PID/image/exit-state at the
actual parent-exit observation and preserves all membership, cleanup and survivor
checks. Paired0/7 exit codes and streamed/piped transports expose the distinction.
No failure-kind precedence is changed to obtain a pass. A focused recheck is
required; the original hosted scheduler cause remains independently open.

Identity probe at61c38e75:7 passed,5 failed in2.08s (outer2.62s), no timeout
or survivor. Four plain0/7 piped/streamed controls and the existing crash case
failed; real grandchild and cancellation negatives passed. The outer observation
identifies a live owned C:\Windows\System32\conhost.exe after client exit.
The hidden console created by CREATE_NO_WINDOW remains an owned platform helper.

Pending correction recognizes that helper only through verified job membership
and exact image identity against GetSystemDirectoryW, then terminates the job
and proves exit on all retained handles under the existing5s cleanup bound.
It retains actual parent exit status and records console cleanup separately.
Other or unknown live images retain descendant-lifetime failure; the cleanup-time
member snapshot catches newly observed application descendants. No live process
is omitted from ownership, termination or exit proof. Seven identity negatives
include a temporary conhost basename, wrong image, missing image and unowned host.

Next causal batch:19 narrow lifecycle checks, then the existing seven-file163-test
focused regression only if the narrow check passes with no survivor or unexpected
descendant. Bounds120s and300s, serial; no build or solve. All earlier failures
remain retained. This console evidence concerns the new worker owner, not the
original hosted5s scheduler-condition failure.
