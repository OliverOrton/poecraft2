# Chaos-Anchored Incremental Action Generation — Final Report

Date: 2026-07-28

Plan: [Chaos-Anchored Incremental Action Generation](plan.md)

Evidence:
[tracked summary](../../../fixtures/solver-natural-t1/v1/evidence/chaos-anchored-incremental-actions-summary.json)

## Decision

Retain the incremental action-envelope scheduler in the opt-in
goal-progress-gated solver.

The architecture passes its exact toy gates and changes the frozen execution
order materially: Chaos releases successors, 32 reached partial states are
expanded and receive inexpensive ordinary actions, and only then does the
solver evaluate delayed broad alternatives. Exact Lucent/Jagged and corrected
Harvest rows resolve entirely to Chaos-created state IDs in the frozen
controls, so no dense cross-action structural DAG is needed.

The milestone does not establish a better frozen policy. The large unexpanded
fringe leaves each completed alternative with a wide exact Q interval.
Admission and rejection are therefore both unproved. Those rows and the
remaining envelope are reported as unresolved, and the existing gated renewal
policy stays bounded rather than falsely becoming exact.

## Retained Architecture

Goal-progress-gated expansion separates each reached carrier's operators into
an anchor set and delayed alternatives. Chaos is the first destructive
reforge anchor. Completed anchor successors are interned and queued
immediately; automatic state-local candidates are then generated for reached
carriers and participate in the same delayed lifecycle rather than being
omitted.

Sparse state rows are linked by stable owner indices instead of assumed
contiguous spans. Bellman, focused expansion, constructive bounds, exact
quotienting, finish checks, policy extraction, and telemetry all traverse
those owner chains and ignore exact rows until admitted. This permits a
delayed row to be appended after optimization begins without invalidating
earlier row ownership.

Delayed rows have explicit `unevaluated`, `evaluating`,
`completed_admitted`, `completed_non_improving`, and
`unresolved_resource_limit` states. An improving exact row is admitted and
triggers another optimization. A row that introduces support outside Chaos
cannot be classified until those states have been interned, queued, expanded,
and valued. A resource-capped or value-incomplete row remains unresolved.
Exact closure is forbidden until the entire filtered envelope closes.

Chaos support is retained only as state-ID membership. Compatible Fossil and
Harvest distributions are still computed with their own exact probabilities
and work charges, then resolved through the ordinary interner. Exact
reforge-kernel signatures allow equivalent carriers to reuse a completed
kernel. Fossil added/forced modifiers and Essence guarantees can introduce
ordinary delta states. No structural DAG, probability renormalization, or
work-accounting discount is retained.

Unrestricted mode does not enable this scheduler and preserves its existing
complete-envelope behavior.

## Exact Toy Gates

The native oracle covers:

1. Chaos successors expanding before all root alternatives finish.
2. A better delayed Essence row being admitted.
3. A worse delayed row being proved non-improving.
4. Admission changing values and causing another Bellman cycle.
5. An Essence guarantee adding four states outside deliberately restricted
   Chaos support, with every non-goal delta state expanded before closure.
6. A one-row/one-expansion cap returning bounded/open rather than exact.
7. Carrier-specific policies selecting different actions.
8. Repeated solves preserving transition and policy hashes.

The completed oracle closes its action envelope, while the capped oracle
retains unresolved lifecycle state. This is the status distinction required
by the production schedule.

## Frozen Qualification

The product-cap diagnostic copies preserve the source cases' 3,000,000
reforge-work ceiling. The copies raise only the discovered-state ceiling to
200,000 because the already-known exact Chaos supports contain 134,477 and
123,697 states. Product fixtures and caps were not edited.

| Case | States discovered / expanded | Rows | Transitions | Reforge work | First alternative begins after |
| --- | ---: | ---: | ---: | ---: | ---: |
| full-four | 134,943 / 32 | 248 | 137,024 | 3,000,000 | 32 expanded states |
| deep-four | 124,161 / 32 | 248 | 125,854 | 3,000,000 | 32 expanded states |

Both runs then reach the unchanged cap during their selected first Fossil.
The previous gated production shape expanded only the root before reaching
that row. The implementation therefore passes the scheduling gate without a
product cap or accounting change.

For action-envelope qualification, diagnostic copies use 8,000,000
reforge-work, the same 200,000 discovered-state ceiling, 25,000 expanded
states, 256 MiB, one native work item per step, and a 32-state focused
checkpoint:

| Case | Completed delayed rows | Next interrupted row | Rows / transitions | Unique kernels / carrier reuse |
| --- | --- | --- | ---: | ---: |
| full-four | Lucent; Harvest attack | Harvest cold | 250 / 364,730 | 4 / 31 |
| deep-four | Jagged; Harvest attack | Harvest elemental | 250 / 346,692 | 4 / 31 |

Lucent uses 1,883,672 exact reforge work; Jagged uses 2,194,169. Corrected
Harvest attack uses 2,800,289 and 2,799,809 work. The next Harvest rows consume
the remaining 508,459 and 198,442 units before the cap. No completed delayed
row adds a state outside Chaos support.

The resulting exact Q intervals are:

| Case/action | Lower Q | Upper Q |
| --- | ---: | ---: |
| full / Lucent | 9.23043308034204 | 60,341,420.46413261 |
| full / Harvest attack | 3.316418998221694 | 60,341,418.63180959 |
| deep / Jagged | 11.592413790388758 | 185,688,655.34654388 |
| deep / Harvest attack | 3.2938370956250465 | 185,688,652.52784011 |

All intervals overlap the existing Chaos renewal incumbents
`60,341,416.98784247` and `185,688,651.38279814`. Each run therefore reports
zero admitted, zero proved non-improving, three unresolved lifecycle entries,
701 unevaluated alternatives, and a remaining envelope of 704. That is an
honest bounded result, not an architecture failure or an exactness claim.

## Determinism, Memory, And Wall Time

Two 8M runs per case preserve states, expansions, rows, transitions, reforge
work, lifecycle counts, Q witnesses, and hashes:

| Case | Transition hash | Policy hash | Live / peak owned bytes | Wall repetitions |
| --- | --- | --- | ---: | ---: |
| full-four | `2d624705bb704ffc` | `8b2a568f3c9cfd35` | 68,591,265 / 83,978,031 | 1,787.946 / 1,792.784 ms |
| deep-four | `08ee41675365b7fc` | `a7b433c931dabaf5` | 43,173,873 / 57,814,962 | 1,413.632 / 1,416.459 ms |

Wall time is machine/compiler-bound and does not survive a hardware or
compiler change. Work, states, lifecycle counts, exact Q values, and hashes
are the portable evidence.

## Consequence

The immediate root-action scheduling wall is removed in gated mode. The next
measured boundary is value precision over the large remaining restricted
fringe: compatible state IDs exist and exact alternative rows can be
evaluated, but their successor values are too loose to separate alternatives
from the incumbent.

Raising the action-work cap alone evaluates additional exact rows without
closing those intervals. A future milestone should target tighter usable
restricted values or a separately proved admission/bounding rule. It should
not revive the dense shared DAG, merge partial states by goal count, or
describe incomplete Q overlap as non-improvement.

Veiled and Eldritch actions remain outside this implementation boundary.

## Verification And Identity

- Starting source: `f37b4ef`.
- Plan commit: `97eac1c`.
- Artifact manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Natural-T1 corpus manifest SHA-256:
  `88570c7306ffaaab242c37896f4dbe2d529ad23e4f9e77e041012fb3d9f4822f`.
- Generator configuration SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Compiler/machine: GCC 14.2.0, Windows build 26200, Intel i7-13700K,
  16 cores / 24 logical processors, 68,396,957,696 physical bytes.

The release artifact-backed native suite passed 515,143 checks with zero
failures. Both benchmark manifests validated (12 standard and 146 natural-T1
cases). Release WASM was rebuilt; the non-visual web suite and TypeScript
no-emit check passed. The build reproduced only the existing GCC optimizer
warning in `prepare_goal_cover_cost`.

No new frozen policy was produced, so no additional frozen 10,000-run
simulation was applicable. The returned frozen strategy remains the
previously proved and compiled Chaos renewal policy; the new exact compiled
toy behavior is covered by the artifact-backed native suite.
