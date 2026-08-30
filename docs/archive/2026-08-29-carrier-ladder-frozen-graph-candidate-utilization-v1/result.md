# Carrier-Ladder Frozen-Graph Candidate Utilization v1 Result

**Outcome: completed diagnosis-only on 2026-08-29.**

Parent: [Archived boundary](README.md)

## Verdict

The current optimizer and terminal publication do not lose a substantially
cheaper policy already present in the frozen completed-row set. A new bounded
joint-attempt lineage proves that terminal assembly sees every completed
alternative and still cannot form a proper policy because concrete
continuation rows are absent.

The historical 87k strategy remains executable under the current engine,
artifact, and economy: independent exact evaluation returns success
probability one, zero off-policy mass, and cost `87361.16904205013`. Its
Eldritch-heavy shape is therefore valid diagnostic evidence, not a stale
semantic artifact. It was never injected into the current solve.

The first actionable divergence is online completion. The current action
envelope contains the Eldritch dependencies and the solver generated and
priced them, but checkpoint assembly first stopped at state 1780 / goal mask
8, while terminal assembly stopped at unexpanded non-carrier state 4489 / goal
mask 0. The diagnostic run retained two open missing-frontier obligations with
zero priority offers or service completions.

## Retained Change

The disabled benchmark-private exact-boundary telemetry now records all joint
attempt invocations and their completed-row, selection, properness,
compile/evaluation, portfolio, and terminal-visibility lineage. Ordinary
solver and product behavior do not read it.

The decisive run is job `job-cbf1b9bc-e10b-4782-8b1b-34feb639c29d`, attempt
`attempt-4113e966-7616-4feb-9c61-63c75b17ceed`, revision
`case-rev-33ceac3cbb5a476ad183a28f47966566`. It naturally completed with
strategy SHA-256
`bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`
and independently matched exact cost `1550334.436668944`.

Native build and `git diff --check` passed. Simulator and the full repository
pipeline were not run because no behavior-changing compiled strategy or
cross-layer production behavior was retained.

## Next Owner

[Carrier-Ladder Online Continuation Completion v2](../../active/2026-08-29-carrier-ladder-online-continuation-completion-v2.md)
owns the narrow follow-up. It must turn a live, exact, solve-local missing
continuation into existing ordinary row service early enough to retry joint
assembly. It does not authorize a fragment library, a second planner, cap
increases, or historical-strategy injection.
