# Private Windows launcher qualification — October 6, 2026

The qualified private supervisor launches batch roots with
`CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | CREATE_SUSPENDED` (134218244).
It retains the existing suspended admission into a Windows job, exact owned
process handles and creation tokens, cancellation/watchdog, redirected output,
finite pipe drain and conservative handling of unknown images or membership.
No new runner architecture, Terminal settings change or broad host cleanup was
introduced.

The isolated branch `dot/terminal-launcher-repair-20261006` starts from verified
main `1b038ef8bec8b11092e9fef1f51f726ab8e93bc8`. It carries the already qualified
private owner from `d485ff111fc6b3aafa1679bca97b9ff9bd70d2a8` and its existing
tests. The new production change relative to that owner replaces
`DETACHED_PROCESS` with `CREATE_NO_WINDOW` and explains console inheritance.
Main's older supervisor already used `CREATE_NO_WINDOW`; this repair qualifies
the stricter private owner, rather than claiming that main had the detached flag.

## Solver-owner invocation

Use this exact module in place of the old detached convenience copy:

`C:\Users\Oliver\Documents\Codex\2026-10-04\task\poecraft2-terminal-launcher-repair\tools\ingest\poecraft_ingest\solver_worker.py`

Module SHA256:
`670a258e01ff30bcf12f66b1614cdf082c8f423dc328a0b4a4db9b17609dc74b`.
The existing `run_isolated_process(command, cwd=..., watchdog_seconds=...,
cleanup_drain_seconds=..., log_path=..., on_started=..., cancel_requested=...)`
API is unchanged. Keep each owner's resolved compiler/solver command, working
directory, frozen-input identities, deadlines and memory/work caps. The
`solver_lab_contracts.py` dependency is unchanged between the carried owner and
verified main; retain its existing caller qualification. Do not overwrite the
old `qualified-ci-d485/solver_worker.py` copy or reuse its detached source hash
as evidence for this repair. Parent LOCAL coordination still controls feature
execution.

## Accepted evidence

The approved historical Terminal PID 10700, creation token
`10700:134357325419465478`, was closed after an exact held-handle recheck. Its
366 visible windows became zero. A redundant post-exit image query returned
WinError 31 after the original handle had signaled and windows were absent;
the original receipt and reconciliation both remain retained.

A bounded unchanged-owner reproduction showed a default child under a detached
root creating one uniquely titled Terminal window, which remained after native
client exit and successful job cleanup. The matched child with
`CREATE_NO_WINDOW` created zero windows. The identified diagnostic window was
closed and verified absent. This establishes a launch/lifetime mechanism, but
does not map every historical window to its original command.

The repaired root then passed these finite KIDS checks:

| Gate | Result | Process identities proved absent | Global Terminal windows |
|---|---|---:|---|
| Default child and grandchild inheritance, default/new process groups, captured/streamed output, exit 0/7 | 8 passed | 34 | 0 before, 0 after |
| Installed VS Ninja, ordinary and console pools, exit 0/7, cancellation and watchdog | 8 passed | 26 | 0 before, 0 after |
| Existing cleanup and same-owned-handle authority, forced denied-image negatives, staged/nested-memory/owned-hidden-console acquisition | 37 passed | 55 | 0 before, 0 after |

All phases also proved unchanged production source during execution, no parent
survivor, no cleanup error and no unexpected outer descendant lifetime. Each
Windows test observes desktop Terminal windows independently of the job and
fails on any new window. Test-only cleanup is limited to a newly observed,
identity-rechecked HWND with the explicit regression title; uncertain windows
are left untouched and stop qualification. No such cleanup was needed in the
repair qualification.

Ninja targets were harmless Python scripts. No native engine/compiler build,
solver root, browser, deployment, dev-server restart, price refresh or main push
was performed. The explicit-console identity fixture now creates an owned
hidden child instead of calling `AllocConsole`, preserving its early owned
console/image assertions without intentionally creating a delegated desktop
window. Candidate/legacy labels now identify the nonvisible console as the
candidate; the detached controls remain retained.

## Retained negatives and scope

`ninja-1` passed four exit cases and process/window cleanup, then failed a fixture
assertion that expected ordinary Ninja's buffered child output after
cancellation. The corrected fixture commits the child's console and creation
identity before publishing readiness, independently of Ninja's output buffer;
console-pool captured-output assertions remain required. `ninja-2` failed
because that fixture's child identity import was initially placed outside the
generated child script. Its exact `NameError` is retained. `ninja-3` passed all
eight cases. Neither failure required changing production code or relaxing
process ownership, cleanup or global-window conditions.

Full local receipts are in:

`C:\Users\Oliver\Documents\Codex\2026-10-04\task\ci-evidence\terminal-launcher-repair-20261006`

The successful phases are `lightweight-1`, `ninja-3` and `lifecycle-1`; failed
Ninja attempts are separate immutable directories. The earlier incident,
approved cleanup, source review and paired mechanism probe remain in adjacent
`ci-evidence` directories. This is source and local process qualification, not
hosted CI qualification, product activation or measured solver economics.
