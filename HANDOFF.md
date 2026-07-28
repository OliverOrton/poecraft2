# Session Handoff

**Status: no active implementation boundary.** Oliver must choose the next
chunk before implementation resumes.

## Latest completed result

The 2026-07-27
[Pre-Expansion Probability-Lower Audit](docs/archive/2026-07-27-pre-expansion-probability-lower-audit/README.md)
tested whether an isolated graph-free probability relaxation could pair with
the archived exact renewal uppers to certify a hard-case next action before
broad-row work.

The candidate passed its exact small oracle and changed no solver state, row,
transition, or reforge-work counter at the pre-row boundary. All four hard
cases had complete legal action scope and finite class lowers, but none
separated:

- relaxed start lowers were 15.98 to 431.4 chaos;
- archived exact renewal uppers were 575,497 to 193,266,777 chaos; and
- cheap junk-bench-first classes remained at `0.005872` or `0.01477` chaos.

The goal-mask/count relaxation erases the blocker effect of a non-goal first
craft. Its universal complete-scope fallback can therefore charge only the
exact first price and grant a free finish. Direct integration of this lower is
closed.

Every measurement-only engine and test edit was restored. The retained
changes are documentation and
[tracked evidence](fixtures/solver-natural-t1/v1/evidence/pre-expansion-probability-lower-audit-summary.json)
only. No mechanic, condition, action, transition, solver algorithm, product
cap, ABI, artifact, binding, WASM, web, or product behavior changed.

## Plausible future boundary

Verified next action remains possible in principle, but a new selected plan
must address both sides at once:

1. produce a certified executable upper inside the product computation
   boundary; and
2. retain enough non-goal first-action, blocker, and preservation state in the
   competing lower to charge downstream work.

Making the root bracket merely finite, publishing the current probability
cover, raising caps, or re-running goal-mask-only variants is not supported by
the completed evidence.

## Repository state

- Local `main` contains the completed audit after the final fast-forward.
- `codex/pre-expansion-probability-lower-audit` remains at the same commit.
- Nothing is pushed.

Commits remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
