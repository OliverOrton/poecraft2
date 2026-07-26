# Gap-Directed Natural-T1 Research Report

**Status: Gates 0–5 completed on 2026-07-25. The conclusion awaits Oliver's
acceptance; this report does not authorize production implementation.**

Parent: [Active work](README.md)

Plan: [Gap-directed natural-T1 solver research](gap-directed-natural-t1-research.md)

Boundary: [HANDOFF](../../HANDOFF.md)

## Decision

Recommend one production milestone: **exact state-local automatic-action
constraint generation**.

The milestone should separate complete, lightweight action-descriptor
enumeration from exact local kernel construction; attach a cheap admissible
lower envelope to every unseen descriptor; and materialize an action only when
an exact separation test says it can improve the incumbent or violate the
Bellman certificate. Restricted-action values must remain search guidance.
They must never replace the proof-bearing lower bound.

This is the smallest architecture change aimed at the measured product
failure. Four of five cases create a 200,000-state local frontier during their
first carrier, before producing any bound. Raising reforge work did not change
that outcome. Reusing constructive-policy properness proofs is compatible and
worth retaining as a second milestone, but it can only help after a policy
exists.

## Pinned Portfolio

The cases were selected from committed metadata before fresh solves. The
run-local portfolio manifest SHA-256 is
`a4f080e680499b95d2b6b5e635c4bc9fd6678a82f4ce698ea8c6882b37fb3efd`.
All cases use item level 86, a rare empty explicit start, artifact-manifest
SHA-256
`6b8bbfe77abf87373ec1782fb364500e064eb7539f1d3f0fdb210a807c927875`,
and Mirage economy
`economy:mirage:9175d37d83d90ab936e572f04c7599afbf18ff6cefc90786a5276da1759cd52f`
with content SHA-256
`9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.

| Case and source SHA-256 | Base / class | Goals / sides | Stratum / minimum draw probability |
| --- | --- | --- | --- |
| `natural-t1-smoke-dire-pelt-three`<br>`30e75291a63155779b1ee8907e3a3009e7d05952cf5eac4d6c0ac576612446fb` | Dire Pelt / Helmet | `ItemFoundRarityIncreasePrefix3`, `IncreasedLife9`, `ChaosResist6` / PPS | smoke three / 0.0021244954 |
| `natural-t1-full-three-24920b3b28de`<br>`d5d924762d7fa945cd43c0a70c4787f5a37ccc1160d2d262e99c7a8c789b9294` | Onyx Amulet / Amulet | `LightningDamagePercent5`, `ChaosResist6`, `AllResistances6` / SSS | full three / 0.0011646867 |
| `natural-t1-deep-three-low-probability-af4719c816f3`<br>`317d29285c1932ca20c90449db3f7664272cb986231a55550ea820fc65fcc95d` | Amethyst Ring / Ring | `FireResist8`, `AddedColdDamage9`, `FireDamagePercent4` / PSS | deep three low probability / 0.0003051572 |
| `natural-t1-full-four-47d8b909aa88`<br>`1f466afdcf9fafe9a9e67acdb0207eae7c54114b6e2f1f8fceb904a5541c4282` | Spine Bow / Bow | `ManaGainedFromEnemyDeath6`, `LocalIncreaseSocketedGemLevel1`, `LocalAddedPhysicalDamageTwoHand9`, `LocalAddedColdDamageTwoHand10` / PPPS | full four / 0.0010979627 |
| `natural-t1-deep-four-low-probability-1a1102b0e06b`<br>`5506dd22bfd1fdc69ae3c2dd8a1473546d9db752d8a2f713caba8b9ff62f4191` | Jewelled Foil / Thrusting One Hand Sword | `LocalIncreasedPhysicalDamagePercent8`, `LocalIncreasedAccuracyNew5`, `LocalAddedFireDamage10_`, `ManaLeechPermyriadLocalSuffix1` / PPSS | deep four low probability / 0.0001891933 |

The principal caps were 200,000 discovered/stored states, 25,000 expanded
states, 300,000 rows, 3,000,000 transitions, 256 MiB selected owned memory,
absolute gap 5, and relative gap 0.1. Gate 2 changed only the run-local
`max_reforge_work` control to 10M, 30M, and 100M.

## Baseline And Candidate Result

Gate 1 ran one warmup and three measured native repetitions with one worker.
Every case exhausted the original 3M reforge allowance during its first
expansion. Gate 2's 100M control is shown below; its status, bounds, counts,
cap hits, and deterministic hashes were identical to the 30M rung.

| Case | Fresh 3M median/result | 100M `L / U` | 100M expanded / discovered | Work and peak owned memory | Gate 4 candidate |
| --- | --- | ---: | ---: | --- | --- |
| Dire Pelt three | harness error surfacing reforge cap | 93.7459 / 10,209.5622 | 2,301 / 95,118 | 18.350M reforge; 61,661 rows; 873,813 transitions; 270.33 MB | rejected before solver trial |
| Onyx Amulet three | 425.754 ms / resource cap | 0 / none | 1 / 200,000 | 10.143M reforge; 82,511 outcome entries; 87.36 MB | rejected before solver trial |
| Amethyst Ring three | 310.679 ms / resource cap | 0 / none | 1 / 200,000 | 8.152M reforge; 49,806 outcome entries; 82.57 MB | rejected before solver trial |
| Spine Bow four | 225.727 ms / resource cap | 0 / none | 1 / 200,000 | 6.119M reforge; 17,850 outcome entries; 74.22 MB | rejected before solver trial |
| Jewelled Foil four | 226.359 ms / resource cap | 0 / none | 1 / 200,000 | 6.119M reforge; 19,949 outcome entries; 74.53 MB | rejected before solver trial |

The Gate 4 candidate has no real `L(t), U(t)` curve because it failed the
exactness/utility qualification before integration. Current telemetry exports
no useful cheap per-candidate admissible envelope. With the only safe generic
zero envelope, exact separation materialized 4/4 synthetic kernels and avoids
zero exact materializations on every portfolio trace. Selected-action-only
replay would appear faster but is not an exact algorithm, so it was not run or
reported as a solver result.

## Bound, Work, And Memory Trends

The Dire Pelt control produced the only bound curve. Its first recorded
`L=93.40585` arrived at 1.456 seconds without an upper policy. The first finite
upper appeared at 20.048 seconds with `L=93.51210` and `U=10,243.45595`.
At the 36.762-second memory stop, `L=93.74587` and `U=10,209.56218`; the
relative gap remained 107.9068. No standard gap threshold was reached.

From first sample to stop, `L` rose only 0.34002, or 0.00963 per second.
After the first finite policy, `U` fell 33.89377, or 2.02785 per second.
Constructive-policy work consumed 26.459 seconds, including 9.136 seconds in
17 fallback start-properness checks. The run stopped at roughly 270.33 MB
selected peak owned memory after 1.343 billion outcome entries had been
considered.

Automatic admission is earlier and more general than that upper-policy cost.
The four full/deep first carriers considered 866 eligible automatic candidates
in total and stopped before selecting one. The smoke considered 74,525
eligible automatic candidates and selected 101. These selection counts bound
an observational savings ceiling only; action non-use is not a pruning
certificate.

## Causal Ranking

1. **Eager exact state-local automatic-action materialization.** It prevents
   all bounds in four cases and creates most of the initial state frontier.
2. **Constructive upper acquisition and repeated properness validation.** It
   owns most smoke wall time after graph generation but cannot help a case that
   has no policy.
3. **Weak lower lift after a policy exists.** The smoke lower bound barely
   moves, but this is downstream of the pre-bound blocker.

The reforge-cap ladder rejected insufficient reforge allowance as the primary
cause. The 30M and 100M controls were identical. A heuristic or
selected-action-only subset was rejected by a hidden-optimum counterexample.
An inadmissible envelope was rejected, and repricing demonstrated that stale
envelopes can become invalid. A generic zero envelope is exact but gives no
materialization savings.

The properness-reuse study found a plausible secondary optimization. If all 17
measured checks cost equally, caching every proof after the first has an
8.598-second aggregate ceiling. That is a projection from aggregate timers,
not a candidate wall-time curve, and it does not improve incumbent quality.

## Prototype And Literature Controls

The isolated C++20 prototype ran one warmup and three measured native
repetitions. All four outputs were byte-identical with SHA-256
`282ea1f8ecf8e6e0b65b33984ea2580b133e39f7c241832caa5dd51992ee3b1a`.
With a useful admissible envelope it materialized 2/4 kernels and matched
exhaustive `L=U=20`. The zero-envelope control materialized all four. Its
versioned properness key reduced 17 checks to one check and 16 reuses, while
invalidating on economy, vocabulary, policy, or transition-dependency change.

Primary SSP/action-generation sources support partial solution graphs, bound-
based action elimination, and exact separation. The critical constraint is
that a value computed from an incomplete action set can exceed the true
optimal value; it is not automatically an admissible lower bound. Verified
source links and claim mappings are retained in
`build/gap-directed-natural-t1-research/gate3/report.md`.

## Ranked Production Candidates

1. **Exact state-local automatic-action constraint generation — recommend.**
   It directly addresses the pre-bound failure and preserves a path to exact
   closure if the descriptor envelope and separator are valid.
2. **Versioned constructive-policy properness-proof reuse — retain for a
   follow-up.** It has a bounded measured ceiling and compatible invalidation
   rules, but only helps after an upper candidate exists.
3. **A stronger goal/side/capacity lower abstraction — defer.** It may improve
   the weak smoke `L`, but current evidence says action materialization blocks
   it from running on four cases.

Do not ship selected-action-only admission, restricted-action values labeled
as certified bounds, or zero-envelope lazy machinery that avoids no exact
work.

## Recommended Milestone Contract

Implement the recommendation as one independently rejectable native milestone:

1. Enumerate every state-local automatic action as a lightweight descriptor,
   without creating its local exact state context.
2. Define a descriptor-level envelope `B(s,a)` and prove it is no greater than
   the exact action value under the current certified continuation bound.
3. Keep a separate certificate
   `L(s) = min(materialized exact Bellman terms, unseen B(s,a))`.
4. Before a finite upper exists, materialize promising descriptors in
   deterministic envelope order while preserving the unseen contribution to
   `L`. After a proper executable `U` exists, materialize every unseen action
   that can violate the incumbent or Bellman certificate.
5. Invalidate descriptor envelopes on goal, price, action-vocabulary,
   abstraction, or dependency-version changes. Reuse a complete exact
   price-independent kernel only when its dependency closure is unchanged.
6. Preserve mechanics, action scope, prices, Bellman meanings, tie-breaking,
   public caps, ABI, and eventual exact answers.

Stop the milestone if no useful envelope can be proved cheaply. Do not merge
constraint-generation machinery whose only safe envelope is the zero bound.

### Acceptance cases

- The strong-envelope, zero-envelope, hidden-optimum, inadmissible-envelope,
  and repricing synthetic controls must pass.
- Existing small exact native cases, including natural one-T1 controls, must
  preserve exact value, policy, transition, and deterministic hashes. Do not
  run the prohibited exact natural two-T1 oracle.
- On the pinned five-case portfolio, the descriptor enumeration must be
  complete. The four full/deep cases must no longer hit 200,000 discoveries
  during their first carrier, and must produce a finite executable `U` under
  the same 256 MiB/200,000-state boundary.
- Against the Dire Pelt control, the median first finite `U` must beat
  20.048 seconds, and the equal-36.762-second curve must improve the certified
  gap without exceeding baseline owned memory.
- Final comparison uses one warmup plus three measured native repetitions,
  one worker, identical inputs/caps/economy/artifact, complete work
  attribution, and no watchdog survivor.
- If a strategy qualifies for final verification, run exact compiled-policy
  evaluation and the required 10,000 simulator trials. Rebuild release WASM
  and run headless WASM confirmation only after native qualification.

### Commands

All commands remain wrapped by the detached 900-second watchdog described in
the plan:

```powershell
powershell -File scripts/build.ps1

$env:PYTHONPATH = "tools/ingest;bindings/python"
py -3 tools/ingest/benchmark_solver_corpus.py `
  --root . `
  --executable build/engine/poecraft_solver_benchmark.exe `
  --artifact data/compiled/current `
  --corpus build/gap-directed-natural-t1-research/gate2/inputs/reforge-100m/manifest.json `
  --output build/gap-directed-natural-t1-research/production-candidate `
  --max-workers 1 `
  --no-exact-evaluation

powershell -File scripts/build-wasm.ps1
powershell -File scripts/test.ps1
```

Use the tracked corpus runner's exact-evaluation and simulation stages only
for a qualified final strategy, at the standing 10,000-run cadence.

## Evidence And Boundary

The Gate 1 analysis SHA-256 is
`02597f7b22c86bd22a343c93dbf5f9050ba6f805bfbb520c23c636cb6e416229`;
Gate 2 is
`ed7dcf94426b5b05d5b882200ea5c9c3f973ae639102faba7945b3b401a3d0b0`;
Gate 3 is
`947ec61aa7ad252a6c511f2a37a1f970934b1bae1d7dc0eed2eb16c806113ea4`;
and Gate 4 is
`2fa562a96f31c301e8e1efd8e8ad584efd7560491ec89bdb122370b3633b5f3f`.
Every watched process recorded exit status, timeout, survivor state, wall
time, log hash, and output hashes under
`build/gap-directed-natural-t1-research/`.

No diagnostic or production solver source, default, cap, ABI, tracked fixture,
compiled data, WASM artifact, mechanic rule, or economy data changed. No
simulation ran. The exact natural two-T1 oracle remained prohibited and did
not run. The active plan and this report remain unarchived until Oliver accepts
or revises the conclusion.
