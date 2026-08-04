# Engineering Report

## Resource contract

Legacy `reforge_work` was an active-evaluator ledger, not a universal unit. On
raw V1 it charged common row setup plus a frontier-node charge and every roll
bucket for every frontier node, even when control flow short-circuited. Harvest
guaranteed support could be traversed more than once while the ledger charged
one scan. V2 instead counted sparse availability and eligible edges; V3 counted
predecessor indexing, denominator edges, subset checks, recurrences, and
commits. Comparing those totals as if their units were interchangeable was the
historical error.

`max_reforge_work` now consumes `logical_work_v1`: the stable legacy-equivalent
V1 envelope calculated for every evaluator. `reforge_frontier_work` remains the
legacy active ledger, and telemetry separately reports V1, V2, V3, and physical
component counts. No weighted cross-version score exists.

V3 effort is structurally bounded by the same row dimensions: for `N` frontier
states and `B` buckets, predecessor and availability storage are bounded by
`N` and `N * ceil(B/64)`, denominator/candidate work by `N * B`, and subset and
recurrence work by at most six affix picks per eligible edge. Existing
`max_solver_owned_bytes`, transactional row publication, and cooperative
cancellation therefore remain the evaluator-safety boundaries; no second
evaluator cap was needed.

Nested automatic and comparison contexts now receive the parent's remaining
logical allowance before executing. Their active and logical deltas merge even
when the parent later refuses. Interrupted work is observable but cannot
publish or certify a row.

## V3 qualification and integration

The focused synthetic and real-artifact matrix passed 252,997 checks with raw
V1 equivalence across ordinary, forced Essence, targeted/exceptional Harvest,
positive/zero/additive/forced Fossil behavior, empty/fractured carriers,
prefix/suffix/mixed and below-tier goals, identity exclusions, and reverse
enumeration. Exact target maps, probability mass, Bellman values, and selected
semantic actions match.

The same-binary binding median improved from 697.594 ms on V1 to 507.483 ms on
V3, or 27.25%. Across 31 eligible reliability cases, solve wall improved 0.30%
in aggregate; the worst measured case was +6.66%. Peak selected-owned memory
moved +0.007% and maximum cooperative latency improved 0.54%. Release WASM
improved binding solve wall 0.71%, held native-owned memory effectively flat,
kept maximum slice change at +7.14%, and acknowledged cancellation in 15.522 ms
against the 250 ms limit.

Coarse rows remain V1. Strict selected and competitively scheduled alternative
rows use V3 generically. No named action, modifier, fixture, or conditional
family dispatch was added. The native benchmark retains the raw-V1 rollback
control and sparse-V2 diagnostic.

## Scaling and fallback

The single selected-only 100M run reproduced the preserved logical envelope:
14,077,632 coarse plus 85,922,368 strict logical work, 40 completed selected
rows, 6,903,840 transitions, and no partition. V3's active evaluator effort was
102,322,134, which is reported separately and is not the cap basis. The result
confirms that V3 removes evaluator wall but not the number of selected carriers
or the quotient/alternative-closure problem.

The single canonical hard 20M run preserved all 27 actions, the 1 GiB memory
cap, and the 900-second watchdog. Coarse work remained 14,077,632 and the
strict allowance 5,922,368, completing two rows and 345,192 transitions. The
lower remained `752.9009075663787`; selected alternatives remained unresolved;
and the independent fallback supplied upper `3984.9346987650665`. Its four
nodes, four edges, 1,208 bytes, transition/policy hashes, and compiled SHA-256
`2be1d46e20de86f7f86d1d74e3ed897ed9e49f8753bfd69dade828d9a353bcf7`
are unchanged. Exact evaluation matched and all 10,000 simulations succeeded
with zero off-policy failures.

The 48-case native portfolio produced 46 expected passes and the same two
rare-renewal reconciliation misses, with 380,000 simulations and zero
off-policy failures. Honest nested pre-execution budgeting changes two other
archived coarse-only classifications; a same-binary raw-V1 control has
identical hashes, bounds, status, and zero strict rows, so those changes do not
come from V3.

## Final answers

1. Legacy `reforge_work` measured the selected evaluator's historical ledger,
   which was V1 node-plus-all-bucket work in production, not universal effort.
2. It omitted repeated Harvest traversal and many physical operations, while
   charging buckets that V1 short-circuited; V2/V3 counted different work.
3. `max_reforge_work` now limits the evaluator-neutral `logical_work_v1`
   search envelope.
4. Structural bounds, `max_solver_owned_bytes`, transactional publication, and
   cooperative cancellation separately limit evaluator safety.
5. Yes. Children receive remaining budget before work, all executed deltas are
   merged, and interrupted rows cannot publish.
6. Yes. V3 is exact for every supported ordinary/Essence/Harvest/Fossil family
   and all required carriers, goal states, exclusions, and enumeration orders.
7. Yes. It clears the 25% native binding gate, does not regress the aggregate,
   and improves the exercised release-WASM solve while keeping headroom.
8. V3 is integrated generically for strict rows; coarse remains V1. No
   conditional family dispatch was required.
9. Yes. The hard case returns the same independently certified executable
   Chaos-renewal fallback and zero-off-policy compiled behavior.
10. V3's rejection is superseded; V2's wall-time rejection remains; V1 slopes
    are V1-only. Frontier/density/fallback findings remain valid, and capped
    alternatives remain unresolved.
11. The number of selected carriers and lack of partition/alternative closure
    still drive selected-closure scaling. Dense joint state plus finalization
    architecture—not the terminal accumulator—still drives five-goal scale.

## Remaining boundary

Selected and alternative closure, rare-renewal numerical reconciliation and
finite-horizon practicality, and five-goal finalization remain unresolved.
Checkpoint/replay remains deferred. No default cap, mechanic, action filter,
goal/state abstraction, quotient proof, Bellman rule, strategy vocabulary, or
frontend crafting authority changed.
