# Historical Five-Goal Quality Regression v1

**Status: completed.** Oliver selected this boundary on 2026-08-30 from clean
`main` checkpoint `a9b8e5cc1343f635e7214c0eaadcb802676f3357` with only
protected untracked `0` present.

Parent: [Documentation archive](../README.md)

## Objective

Make the current solver independently generate an exact-evaluated clean
Conquest Lamellar five-goal strategy cheaper than the historical
`87361.16904205013`-Chaos witness. Reproduce the historical generation
conditions before attributing a code regression, then retain only fixes proved
by matched evidence.

The historical strategy remains an evaluation oracle only. It may not be
installed, seeded, adapted, or supplied to current search.

## Starting Evidence

The historical strategy SHA-256
`6082eef2462295c167cc6b0ec9855b397f2f3f673c15245fdef93b2300358104`
still exact-evaluates under current native authority with success probability
one, zero off-policy mass, exact cost `87361.16904205013`, and expected
actions `8367.301680564138`.

The recent current comparison was not generation-matched:

- historical bounded finish: 60 seconds; recent current: 10 seconds;
- historical solve step: 1,024 work items; recent current: eight;
- historical Imprint programs: enabled; recent current: disabled; and
- historical base override: five Chaos; recent current: one Chaos.

Item base, item level, clean rare start, five exact affix families, league
economy snapshot, restart-disabled policy, state/row/transition caps, and
independent exact-evaluation limits otherwise match. Goal slot order is not
semantic authority and will be normalized by identity rather than treated as
a difference.

Historical evidence:

- native report SHA-256
  `11ac9077ad6a3f0b339f2b1869816caf430777e33346abb79d22a763878a09cd`;
- diagnostic case SHA-256
  `f528c35843096e96bb30e7fc8f4bf5606d418baef658ec88b501ec5933d8cb74`;
  and
- benchmark manifest SHA-256
  `0e78287c6adc5ca11b41b53e8dd5eec326b1e50f672c6470dc4d41a0652850b1`.

## Execution

### 1. Reproduce before bisecting

Run current native source against an immutable case matching the historical
60-second request, work-step size, Imprint scope, economy, caps, action
envelope, and exact evaluator. Record all request, executable, artifact,
strategy, row/state/work, occupancy, and result identities.

If current source reaches or beats 87k, classify the recent quality gap as a
request/budget comparison error and continue from the best current-generated
strategy rather than changing solver code.

### 2. Factor the generation differences

If required, vary only one of bounded finish, work-step size, Imprint scope,
and base override at a time. Exact cost and strategy occupancy are authority;
candidate estimates are not. Stop varying a factor once its causal effect is
clear.

### 3. Locate a real source regression

Only if current source loses under the matched historical request, build the
historical accepted checkpoint in an isolated detached worktree and run the
same current artifact/case. Bisect source checkpoints between the last good
and first bad behavior using bounded strategy identity, exact cost, carrier
epochs/subsets, row work, generated-action lifecycle, and first selected-
policy divergence.

### 4. Repair and improve

Implement the smallest source-backed correction to lost work ordering,
carrier/action admission, row availability, candidate construction,
properness, compilation/evaluation, or incumbent publication. Do not add a
new planner, permanent fragment library, historical policy injection, action
filter, cap increase, mechanic/probability change, or unproved lower/pruning
authority.

Continue exact-evaluated iterations until a current-generated strategy is
strictly cheaper than `87361.16904205013` Chaos or an in-scope correctness
blocker is proved. A strategy is not a win until it compiles, has success
probability one, zero off-policy mass, and reconciled complete cost.

### 5. Qualification

Use focused builds/runs while attributing. Run exactly 10,000 Simulator trials
only for the final retained materially changed compiled strategy. Run broader
native/WASM/full acceptance once only if the final production change requires
it. Archive exact failures honestly and do not push.

## Living Log

### 2026-08-30 — Activation

- Preflight matched `main` at
  `a9b8e5cc1343f635e7214c0eaadcb802676f3357`, upstream
  `91b0c3db79613bcf5f1e8b222e3d91d2aefaad5e`, with sole protected untracked
  `0` untouched.
- Historical and recent cases match item, clean start, exact five goal
  families, economy snapshot, restart policy, and major resource caps, but do
  not match time, step size, Imprint scope, or base override.
- Next: create one immutable current-native historical-request reproduction
  and exact-evaluate it before inspecting source history further.

### 2026-08-30 — Matched reproduction

- The Solver Lab's current product profile could not preserve the historical
  request: import normalized `solve_step_work_items` from 1,024 to eight and
  disabled Imprint. The imported draft
  `draft-5d44b1de-4703-4ac3-9cc2-64aa5128d687` was never frozen or run and was
  discarded through the JSON CLI after dry-run preflight. No catalog or SQL
  edit was used.
- The archived manifest was therefore run directly through the repository's
  native benchmark CLI. Current `a9b8e5c` under the exact 60-second request
  returned an independently exact-matched 14,454,067.42607058-Chaos policy:
  12,847 expanded states, three carrier epochs, 12,844 carrier candidates, 63
  goal-subset visits, and two joint attempts. The caller action vocabulary was
  the same 28-action set as the historical run.
- Detached checkpoint `2b8d5ac` reproduced the accepted search shape against
  the same current compiled artifact. Its 60-second run reached 35 epochs and
  14 joint attempts, one attempt short on this machine. A bounded 90-second
  reproduction completed attempt 15 and produced exact cost
  `87361.1690420501`, success probability one, zero off-policy mass, and the
  byte-identical historical strategy SHA-256
  `6082eef2462295c167cc6b0ec9855b397f2f3f673c15245fdef93b2300358104`.

### 2026-08-30 — First-bad source and repair

- The last-good/first-bad work boundary is `1f68497` / `b6fb861`. Under the
  same 60-second case, `1f68497` retained the historical cadence (33 epochs,
  14 attempts, 42,957 completed joint rows), while `b6fb861` immediately
  produced the modern three-epoch/two-attempt/14.45M signature.
- The regression was not the action catalogue, item, goal, economy, fragment
  core, or policy compiler. `b6fb861` made a frozen automatic frontier one
  indivisible epoch and kept a missing-frontier request urgent until all of
  its carrier-local automatic work completed. Forced pending-value service
  further reinforced that drain. New carrier work repeatedly displaced value
  updates and joint executable-policy assembly.
- The retained repair restores the ordinary interleave: only explicit dynamic
  preparation resumes an epoch; missing-frontier states use ordinary exact
  refinement order and retire when selected; and forced refinement no longer
  reopens `PendingValues` rows as unresolved. Missing-frontier offer and
  completion telemetry now records that exact-refinement selection boundary.
  New focused coverage locks this lifecycle.
- A repaired 60-second current run restored the ladder shape (31 epochs and
  12 attempts before the local deadline) instead of the bad three/two shape.
  It honestly remained one bounded window short of the winning assembly.

### 2026-08-30 — New best and qualification

- A 120-second continuation under the same item, clean start, five exact goal
  families, economy, base override, caller action vocabulary, Imprint scope,
  restart policy, solve-step size, resource caps, and exact evaluator produced
  a new independently generated strategy at exact cost
  `85408.64362148782` Chaos. This is 1,952.525420562306 Chaos (2.2350%) cheaper
  than the historical witness.
- Exact authority: success probability one, off-policy mass zero, converged
  and cost-complete evaluation, reconciled cost, 8,259.46821052856 expected
  actions, 775 compiled nodes, and 1,634 edges. Search evidence: 6,329 expanded
  states, 53 carrier epochs, 12,964 candidates, 909 goal-subset visits, 24
  joint attempts, one success, and 47,709 rows in the last completed attempt.
- Winning identities: input SHA-256
  `deeffca4cebb0af4339cbfae7ee1037c7ad0431ebabb6da2f7360652f75fdee0`,
  product-action SHA-256
  `1cc42c96627601b5ddd4888bf6e571c9344f985a5028b36cfd2a898aefcdbb77`,
  native report SHA-256
  `d7fef627222e3a799ed0ab6177b8776d518cba5c45b8c402260bc1ee3ffc3091`,
  and 425,470-byte strategy SHA-256
  `9e8687ac1f1de705cd1bef59e5269395190e6b8d2134d26d0cf1aac2468717b1`.
  The current artifact manifest SHA-256 is
  `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`.
- Required native Simulator qualification completed 10,000/10,000 runs with
  seed `20260823`: 10,000 successes, zero failures/stops/action limits/step
  limits/no-edge/action-not-applied/missing-price outcomes, 82,050,790 total
  actions, and complete sampled cost. Sample means were 8,205.079 actions and
  84,658.65664399428 Chaos.
- The focused native solve suite passed 86,241 checks with zero failures.
  The engine build passed. No C ABI, strategy vocabulary, compiled data, or
  WASM-facing contract changed, so WASM rebuild and broader pipeline work were
  not required for this internal scheduling repair. The protected untracked
  file `0` remained untouched and no push was performed.
