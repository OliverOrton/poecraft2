# Competitive Lazy Alternative Certification Report

**Status: medium bounded publication qualified; binding two-goal product gate
failed structurally before its first partition or upper.**

Measured on `codex/competitive-lazy-alternative-certification` from
`bd288c9041a5b54fa4ec134c7e1dec90486ac385`, 2026-08-02. The final archive
commit is reported in the final handoff.

## Outcome

Selected-policy-first construction fixes the eager-alternative ordering defect
on the qualified medium case. It does not make the frozen two-goal selected
closure affordable. Gate 5 consumed the complete 20,000,000-work budget before
partition initialization and returned `refused_resource_cap` with no policy.
Under the frozen decision table this is a selected-closure structural failure,
not an alternative-pruning or scheduler failure.

The milestone therefore stops at Gate 5. Gates 6 and 7 were conditional on a
valid upper and were not run. The branch retains the reusable proof,
selected-first, scheduling, bounded-publication, invalidation, and telemetry
work qualified through Gate 4, but it does not qualify the binding two-goal,
broader native, five-goal, or WASM product claims.

## Milestone checkpoints

| Gate | Commit | Result |
| --- | --- | --- |
| 0 | `222ef42242bb1c51cddb38444750bbd30cda6875` | identities, behavior-neutral work telemetry, and non-numeric hard prediction frozen |
| 1 | `53c538143b3599b99b5b8cd8317557ada14e39c7` | explicit collision-safe lower-only alternative obligations |
| 2 | `c10b2e3cdc85aaab0ec7c0fc6293a9392e66bc16` | production selected-policy-first closure and bounded publication |
| 3 | `4c19d2cc8d3b0569800457b90f91d9c62ad4e9e4` | deterministic competitive scheduler, revocation, and post-upper cap retention |
| 4 | `68591b7d5bc20d7525fb8b7137e0061a05cdf4f7` | repeated medium native qualification and scaled-breadth witness |
| 5 | final archive commit | one-shot binding structural stop and final handoff |

## Qualified medium boundary

The two frozen `reliability-class-belt` runs are semantically identical. Both
return `bounded_feasible` at cost `9.143792577895411`, retain transition/policy
hashes `ce5a144282753b26` / `6bee45662f66d2e4`, compile to strategy SHA-256
`87a5c6a5c0980b1696156aadbd8a4e94e804d66f6a0ef062dfbee105197855bb`,
exact-reconcile, and complete 10,000/10,000 simulations with zero off-policy
failures and zero reference-adapter calls.

Fifteen selected rows reach the first partition and executable upper at 882
work with zero alternatives materialized. First-partition wall is 21.16–27.24
ms and first-upper wall is 65.46–71.80 ms. The scheduler then attempts 180
alternative rows, spends 6,355,232 work, certifies 31 obligations, and retains
149 partial competitive blockers. The safe zero lower prunes none, so the
publication is bounded rather than exact. Peak solver-owned memory is
126,975,163 bytes, including 106,092,192 bytes of obligation storage.

## Frozen Gate 5 invocation

The case `natural-t1-breadth-two-4e65dda9c53b` was invoked exactly once with
its complete 27-action product vocabulary, the unchanged 20M/1 GiB caps, a
900-second external watchdog, exact compiled evaluation requested, and 10,000
simulations requested. The watchdog completed in 6,233.857 ms, did not fire,
and left no survivor. The native report completed in 5,854.3089 ms, including
3,706.5743 ms in solve.

| Field | Result |
| --- | ---: |
| Coarse work | 14,077,632 |
| Remaining exact allowance | 5,922,368 |
| Selected rows begun / completed | 2 / 2 |
| Selected work / transitions | 5,922,368 / 345,192 |
| Alternative rows / work | 0 / 0 |
| Exact carriers materialized / retained | 4 / 0 |
| Exact kernels / quotient classes | 2 / 0 |
| First partition work / wall | not reached / not reached |
| First upper work / wall | not reached / not reached |
| Native live / peak owned bytes | 305,294,516 / 375,483,695 |
| Frozen solver-owned limit | 1,073,741,824 |
| Production reference-adapter calls | 0 |

The selected and coarse charges sum to exactly 20,000,000. The solver stopped
on `max_reforge_work` before partitioning, so the milestone publication
invariants could not run: action accounting is incomplete, no alternative
obligation exists, and the 194 reported avoided alternative rows are a work
counter rather than proof representation. Scheduling rounds, certifications,
partial obligations, noncompetitive verdicts, revocations, policy
improvements, and retained bounded publications are all zero.

The reported lower `752.9009075663787` is the pre-quotient coarse lower. No
carrier-wide obligation lower could participate because selected closure
stopped before obligation construction. There is no upper and no exactness
claim. Properness, lumpability, compilation, exact reconciliation, and
simulation are correctly not applicable rather than failed.

## Prediction score

Gate 0 froze both first-partition and first-upper work as “not presently
estimable.” It recorded the only binding condition: the root selected row and
recursive selected successor closure had to fit in 5,922,368 post-coarse work.
The single measurement now shows two selected rows consuming that allowance
exactly without a partition or upper. There is no numeric prediction to score;
the known selected-closure risk is realized.

Gate 0 was still correct to continue: no pre-change measurement proved this
failure, and the directive prohibited stopping on a warning or conditional
projection. Gate 4 also demonstrates why the architecture remains useful—the
medium first upper uses only 0.0139% of eventual exact work—but that result did
not establish hard-case selected-closure affordability.

## Hash and evidence boundary

The Gate 5 benchmark binary SHA-256 is
`18b55632d0840d78db639583105a815eb0089659bbd9d6cd1f7c8b63a0c0cca3`.
The frozen fixture is `1dc33b...cb3be`, the artifact manifest is
`22bbb2...de5`, and the economy content is `9175d3...d52f`. The incomplete
coarse transition and policy hashes are `4f26d305a908c59f` and
`3e384d3eb52f9ab7`; they are order-invariant evidence for this single stopped
prefix, not successful publication hashes. The raw report SHA-256 is
`17f2dee8fa8d91c3a383037fc4371945758932cdf61ec7d61ec803ba7292be0f`.

Tracked structured evidence is
[`competitive-lazy-alternative-certification-gate5-structural-stop.json`](../../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate5-structural-stop.json).
Ignored raw report, partial, stdout, and stderr artifacts remain under
`build/acceptance/competitive-lazy-alternative-certification/gate5-hard-once/`.

The one-shot wrapper did not retain the exited native process code. That fact
is recorded rather than inferred. It does not make the outcome ambiguous: the
process produced a complete schema-valid final report, a completion line,
empty stderr, no timeout, and no survivor.

## Acceptance stop and unchanged scope

No Gate 6 native portfolio, Gate 7 release WASM, web test, TypeScript check, or
final `scripts/test.ps1` run was performed after the binding red. The last
narrow source acceptance was Gate 4's 98,160-check solve suite with zero
failures; the repeated medium workflow supplied compilation, exact evaluation,
and 10,000-run simulation evidence.

No cap, default, mechanic, price, admitted action vocabulary, public C ABI,
strategy JSON, WASM artifact, or frontend authority changed. Nothing was
pushed or merged. A new implementation boundary requires Oliver's choice.
