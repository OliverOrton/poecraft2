# Selected Policy Publication And Cooperative Exact Refinement

**Status: active. Selected by Oliver on 2026-08-16; Gate 0 candidate and
finalization evidence is in progress.**

Parent: [Calculator WASM Scheduling And Progress](README.md)

## Objective

Recover useful strategy publication for Oliver's from-empty five-natural-T1
Conquest Lamellar without weakening proof standards, hiding incomplete action
scope, raising caps, or adding another fallback-only success condition.

The immediate product objective is narrower than global optimality:

1. preserve the stable selected coarse policy that currently estimates near
   `690872.22056` chaos;
2. exact-refine, compile, and independently evaluate that candidate before
   replacing it with the existing Chaos renewal fallback;
3. publish the cheapest independently evaluated executable candidate as an
   honestly bounded result; and
4. make exact refinement cooperative so the normal WASM worker can report
   real progress and observe cancellation during the currently synchronous
   strict lift.

The approximately `690872` value is diagnostic evidence, not an acceptance
oracle. Exact refinement may change it or reject the policy. A candidate earns
publication only from the exact compiled-strategy evaluator.

## Frozen Evidence

The selected five-natural-T1 witness is:

- empty rare item-level-86 Conquest Lamellar;
- published Allflame economy
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`;
- no base or action-price override;
- natural T1
  `LocalIncreasedArmourAndEvasionAndStunRecovery6`;
- natural T1 `LocalIncreasedArmourAndEvasion8`;
- natural T1 `LocalBaseArmourAndEvasionRating8`;
- natural T1 `AdditionalPhysicalDamageReduction5_`; and
- natural T1 `ChanceToSuppressSpellsHigh5___`.

Current release-WASM behavior after the retained numerical-stop repair:

| Evidence | Current result |
| --- | ---: |
| stable coarse selected-policy estimate | `690872.22056` |
| published evaluated Chaos upper | `37279857.73995944` |
| certified lower | `0` |
| policy / termination | `bounded_feasible` / `numerical_stability` |
| solve / total wall | `1359.401` / `5304.073` ms |
| maximum bounded worker step | `93.106` ms |
| compiled strategy | 3-node/3-edge Chaos renewal |
| remaining action-envelope work | `20175` |
| cap hits | none |

The five-T1 result is now responsive and sound, but it does not recover useful
strategy quality. The original 175-minute run already carried the same
approximately 37.28-million-chaos fallback; the repair made that fallback
return promptly rather than inventing it.

The exact three-prefix witness separately proves that strict finalization is a
product bottleneck. Its selected solve reaches native `Done` quickly, then
`finishSolverSolve()` spends about `142.123` seconds synchronously rebuilding
and certifying the exact policy. Oliver's current three-prefix-plus-spell-
suppression witness shows the same `Finalizing and verifying policy` phase.

Raw evidence and hashes remain in
[Gate 1/2 evidence](evidence/gate1-gate2-selection-progress.md).

## Source-Confirmed Diagnosis

These are implementation facts, not hypotheses:

1. `advance_policy_selection()` in
   `engine/src/solver_solve_bellman.cpp` retains `policy_rows` after two
   complete unchanged policy rounds and raises the named numerical-stability
   stop. The selected policy exists at that boundary.
2. `SolveWork::Impl::finish()` in `engine/src/solver_solve_finish.cpp` sets
   `restore_output_incumbent` whenever the current policy can no longer become
   globally exact. Numerical stability satisfies that condition.
3. The restore block replaces `result.values`, `result.policy`, preferences,
   reachability, and `policy_rows` with `output_incumbent` before the
   authoritative extraction and direct compiled-policy assertion. On the
   five-T1 witness that incumbent is the Chaos renewal policy.
4. Direct compiled-policy assertion therefore certifies the restored Chaos
   policy. The cheaper stable selected policy never becomes an independently
   evaluated portfolio candidate.
5. Strict lift runs only after a materialized `result.policy_available`
   candidate reports a compatibility trigger or failed direct assertion. It
   calls monolithic `refinement::lift_policy_quotient()` from
   `engine/src/solver_policy_refinement.cpp`.
6. `pc_solver_solve_step()` is cooperative, but
   `pc_solver_solve_finish()` calls the entire `SolveWork::finish()` pass
   synchronously. `engine-worker.ts` can only emit a synthetic `finalizing`
   update before calling the blocking WASM facade.
7. Compilation and independent exact evaluation are distinct from optional
   10,000-run Simulator sampling. The five-T1 Chaos graph compiles and exact-
   evaluates in under one second; strict carrier construction/refinement, not
   the final three-node graph evaluator, owns the long exact-case boundary.

## Product And Proof Contract

The implementation must keep these authorities separate:

```text
coarse selected policy
  -> unverified publication candidate
  -> exact carrier/refinement proof
  -> compiled executable graph
  -> independent exact graph evaluation
  -> certified candidate portfolio
  -> cheapest evaluated publication
```

- A coarse value is never a public upper merely because the policy stopped
  changing.
- An unverified selected candidate is never mixed into the certified fallback
  portfolio.
- Candidate ordering before exact evaluation may schedule work, but cannot
  choose the published cost.
- Only proper, executable, zero-off-policy, cost-complete independently
  evaluated artifacts compete for publication.
- `numerical_stability` remains the stopping cause unless an independent
  global closure proof earns `exact_closed`.
- The lower remains zero while the incremental action envelope is open.
- A direct candidate may be published bounded even when strict objective order
  or global action optimality remains unresolved.
- Exactness, executable strategy quality, and worker responsiveness are three
  different gates.

## Acceptance Definition

The five-T1 quality gate passes only when the ordinary release-WASM product
request:

1. returns within five minutes with no cap increase;
2. publishes a proper executable strategy that is not byte-equivalent to the
   current Chaos-only renewal graph;
3. independently exact-evaluates with zero unresolved/off-policy mass and
   complete cost accounting;
4. has an evaluated cost strictly below `37279857.73995944` by the existing
   value-comparison tolerance;
5. selects the cheapest independently evaluated candidate, including the
   retained Chaos fallback;
6. reports lower zero and all open obligations honestly unless a stronger
   proof independently closes them; and
7. matches the fixed 1,024-work control on termination class, bounds,
   evaluated policy cost, selected actions, and strategy hash.

This gate deliberately does not pin the coarse `690872.22056` estimate as the
expected compiled cost and does not require exact optimality. If the selected
candidate cannot be certified or is not cheaper than Chaos, useful five-T1
strategy recovery has failed even if the solver returns quickly.

For cooperative finalization, every selected witness must keep each native
WASM step at or below 250 ms, surface changing native-owned refinement work,
and acknowledge cancellation at the next cooperative boundary. The terminal
summary and strategy must remain identical to an uninterrupted run.

## Non-Goals

- Do not add another fallback family or weaken the current fallback proof.
- Do not raise solver, refinement, compiler, evaluator, transition, reforge,
  memory, or Simulator caps.
- Do not treat the coarse ~690k value as certified.
- Do not alter crafting mechanics, action legality, prices, goal semantics,
  state identity, transition probabilities, or the objective.
- Do not close missing-price or unsupported-action obligations by assumption.
- Do not require global exactness from the five-T1 witness while its action
  envelope is explicitly open.
- Do not use 10,000 capped Simulator runs as cost evidence for a strategy whose
  expected action count exceeds the unchanged 100,000-action run limit.
- Do not move crafting-rule authority into TypeScript.
- Do not run the full acceptance pipeline before the final gate.

## Gate 0 - Freeze Candidate And Finalization Evidence

Before publication behavior changes, add a narrowly scoped diagnostic snapshot
at the numerical-stability boundary and freeze two release-WASM witnesses:

1. the exact five-natural-T1 request above; and
2. the empty Conquest Lamellar with the same three prefixes plus only natural
   T1 spell suppression, matching Oliver's current screenshot.

Record before incumbent restoration:

- selected policy-row hash and selected operator counts;
- coarse start value and residual;
- reachable selected states and choice/preference payload counts;
- current graph/action/economy/artifact identities;
- strict-order-suppressed state/row count and bounded samples; and
- whether the candidate is structurally materializable without claiming it is
  executable.

Add stage timers and logical counters for:

- final row extraction/materialization;
- direct compilation/assertion;
- strict carrier discovery;
- selected kernel construction and cache hits;
- observation partition and refinement rounds;
- local reoptimization/rebuilds;
- strategy serialization; and
- independent exact graph evaluation.

Instrumentation must be behavior-neutral on selected rows, values, bounds,
termination, action IDs, policy/transition/strategy hashes, and cap accounting.
For wall time, require no more than 5% or 100 ms overhead, whichever is larger,
on the selected short control. If instrumentation alone crosses the 250 ms
worker boundary, reduce its retained detail before proceeding.

Gate 0 exit:

- the five-T1 pre-restore candidate is reproducible in eight- and 1,024-work
  modes;
- the exact phase owning the long strict lift is measured rather than inferred;
  and
- no result semantics change.

## Gate 1 - Preserve The Selected Policy As An Unverified Candidate

Capture the current stable selected policy before `restore_output_incumbent`
can replace it. Prefer a dedicated type or explicit state such as
`UnverifiedSelectedPolicyCandidate`; if `BoundedPolicyIncumbent` is reused, its
certification booleans must remain false and APIs must make it impossible for
uncertified candidates to enter `best_current_certified_fallback()`.

The snapshot must retain only what exact publication needs:

- values and selected `policy_rows`;
- per-row prices and observed-choice sources;
- policy reachability and quotient representatives;
- exact start item plus goal/economy/action/artifact/graph identities; and
- numerical-stop provenance and selected-policy hash.

Use the existing bounded-policy row capture/materialization authority rather
than reimplementing action or preference reconstruction. Account every retained
allocation against `max_solver_owned_bytes`; do not hold duplicate full graphs
or strategy JSON payloads merely for diagnostics.

The certified Chaos incumbent remains intact and independently publishable.
The new selected candidate is an additional certification input, not a
replacement and not yet a result.

Focused native tests must prove:

- candidate capture happens before restoration;
- restoration cannot mutate the captured row policy;
- unverified candidates cannot publish or affect a public upper;
- identity/generation changes invalidate the candidate; and
- memory refusal leaves the existing certified fallback behavior unchanged.

Gate 1 exit: telemetry shows both the stable selected candidate and the Chaos
incumbent with distinct identities and proof states.

## Gate 2 - Certify And Compare The Selected Candidate

Refactor final publication into an explicit candidate pipeline:

1. materialize the selected candidate's complete coarse policy;
2. run the existing direct compiled-policy assertion against that candidate;
3. if exact identity/observation compatibility requires it, run strict
   policy-guided lift for that candidate rather than the restored fallback;
4. retain only a proper, executable, cost-complete, zero-off-policy exact
   artifact;
5. independently verify every surviving fallback whose stored proof is stale;
6. compare independently evaluated costs; and
7. publish the cheapest verified candidate.

Do not compare the ~690k coarse estimate with a certified fallback cost as if
they had equal authority. The estimate may prioritize certification order only.

Publication semantics:

- a certified selected candidate that beats Chaos becomes the bounded result;
- a successful strict lift may change local policy decisions and cost, but its
  compiled graph owns the published value;
- numerical stability remains the termination when strict/global optimality is
  unresolved;
- independent global closure may still promote to exact;
- if selected certification fails, publish the existing certified fallback
  with the precise selected-candidate failure, but mark this gate failed; and
- if the selected artifact is Chaos-only or does not beat the current Chaos
  upper, do not claim strategy recovery.

First qualify this path in an isolated native process with a five-minute outer
watchdog. The current monolithic lift may be used only to establish candidate
viability. If it blocks beyond the boundary, record the last stage/counters and
proceed to cooperative work before retrying; do not increase caps or weaken
the assertion.

Gate 2 exit: the exact five-T1 candidate either becomes a cheaper independently
evaluated non-Chaos strategy, or the plan stops with a concrete structural,
resource, or policy-compatibility failure. Fast Chaos fallback alone is not a
positive Gate 2 result.

## Gate 3 - Make Exact Lift Retained And Cooperative

Enter only after Gate 2 proves a useful selected candidate exists, or after the
monolithic viability attempt reaches a measured atomic-work boundary that must
be split before certification can finish.

Introduce a retained `PolicyExactLiftWork` (name not contractual) that borrows
immutable solve/mechanics inputs and owns all resumable refinement state. It
must advance through explicit stages such as:

1. selected-policy carrier discovery;
2. exact kernel construction;
3. backward observation propagation;
4. closed partition refinement;
5. proper fixed-policy evaluation;
6. witness-driven local reoptimization or vocabulary widening;
7. strategy compilation; and
8. independent exact graph evaluation.

Each `step(max_work_items)` consumes deterministic logical work. Wall-clock
adaptation remains a worker scheduling policy, not native proof authority. A
single broad kernel must itself expose resumable frontier/transition work if it
can exceed 250 ms; wrapping a multi-second kernel in an outer step does not
qualify.

The retained object must preserve cumulatively across resumes:

- refinement class/round limits;
- exact state, kernel, transition, reforge-work, and memory limits;
- frontier-expansion and local-reoptimization work already spent;
- deterministic ordering and hashes;
- exact start, action vocabulary, economy, and graph identities; and
- the already-certified fallback portfolio.

Do not restart a refinement pass merely to create a cooperative boundary. A
cancelled or capped run must release retained work without publishing a partial
candidate. Resume is internal to one solve invocation; cross-session resume or
persistence is not in scope.

Native equivalence tests must compare monolithic reference and stepped work on
small exact fixtures at work sizes 1, 8, and 1,024. Values, status, classes,
selected actions, compiled JSON bytes, exact evaluation, work accounting, and
failure classification must match.

## Gate 4 - Integrate Native Finalization With WASM Progress And Cancellation

Move candidate extraction/refinement/certification before native `Done` so
`pc_solver_solve_step()` remains the cooperative owner of unfinished work.
Append public progress phases only if required; do not renumber existing C ABI
values. At minimum distinguish native-owned `refining`, `compiling`, and
`certifying` work from ordinary Bellman iteration.

After this change:

- `pc_solver_solve_finish()` may package and transfer an already finalized
  result, but must not start another unbounded proof pass;
- WASM bindings must expose the appended progress vocabulary and counters;
- `engine-worker.ts` must render native phases rather than manufacture a
  synthetic finalization phase around a blocking call;
- cancellation must be observed between retained native work units;
- `pc_solver_solve_abandon()` must release refinement state and retain bounded
  abandoned telemetry; and
- no successful summary or strategy may publish after cancellation wins.

Update every downstream surface named by the change-impact map: public solver
header if the phase contract changes, native API mapping, WASM facade,
TypeScript protocol, worker/client, Calculator presentation, benchmark runner,
fixtures, and stable solver/WASM/product documentation.

Gate 4 exit on both exact refinement witnesses:

- every native step is at most 250 ms;
- changing carrier/kernel/class/partition/evaluation work is visible at least
  every 250 ms while work continues;
- cancellation acknowledges at the next cooperative boundary;
- uninterrupted results remain semantically identical; and
- final `finish` packaging is no longer the multi-second computation owner.

## Gate 5 - Focused Qualification

Run one focused acceptance set after the implementation is complete:

| Case | Required result |
| --- | --- |
| exact five-natural-T1, eight-item product | cheaper independently evaluated non-Chaos bounded strategy; honest zero lower/open obligations; under five minutes; max step 250 ms |
| exact five-natural-T1, 1,024 control | same termination, bounds, evaluated cost, selected actions, and strategy hash as product mode |
| three-prefix exact control | retain exact value/hash semantics and complete cooperatively |
| three-prefix plus spell-suppression exact control | retain exact policy semantics and complete cooperatively |
| existing four-goal qualification | retain native/release-WASM semantic equality and under-five-minute boundary |
| Eldritch, Warlord, and Imprint publication controls | retain exact/bounded status, operation vocabulary, evaluated cost, and strategy hashes unless a documented independently evaluated improvement is selected |

Required checks:

- fresh native build;
- focused native solve, API, refinement, compilation, and exact-evaluation
  tests;
- focused worker/WASM progress, cancellation, result-presentation, corpus, and
  benchmark tests;
- `npx tsc --noEmit`;
- release-WASM rebuild;
- eight/1,024 scheduler parity; and
- `git diff --check`.

Independent exact evaluation is mandatory for every published strategy. Run
exactly 10,000 Simulator trials only when an existing acceptance contract
requires sampling or the changed strategy's expected action count fits the
Simulator run boundary well enough for the sample to be meaningful. Record an
over-limit strategy as sampled verification not applicable; never count capped
runs as cost corroboration.

Do not run routine full suites between gates.

## Gate 6 - Final Acceptance And Archive

After the focused set passes:

1. audit candidate proof-state separation and retained-work ownership;
2. audit public ABI additions for append-only numbering and complete binding
   coverage;
3. run `powershell -File scripts/test.ps1` once;
4. record final request identities, source/WASM/report hashes, stage timings,
   maximum step and cancellation latency, bounds, policy/strategy hashes,
   exact-evaluation results, and any applicable 10,000-run evidence;
5. update stable solver, WASM, Calculator, decision, and evidence documents;
6. archive the active milestone; and
7. return `HANDOFF.md` to no active boundary.

Oliver retains rendered/visual review. This plan does not authorize automated
browser screenshots or visual smoke.

## Checkpoint Commits

Create local-only coherent checkpoints with the required co-author line for:

1. accepted plan and frozen candidate/finalization evidence;
2. selected-policy candidate preservation;
3. selected-candidate certification and portfolio publication;
4. retained cooperative exact lift;
5. WASM progress/cancellation integration; and
6. final qualification, stable documentation, and archive.

Do not push unless Oliver explicitly requests it.

## Stop Conditions

Stop and hand off precisely if:

- the pre-restore ~690k selected policy cannot be reproduced with the frozen
  request;
- candidate capture changes selection, bounds, hashes, caps, or timings beyond
  the Gate 0 neutrality allowance;
- the selected policy is improper, non-executable, off-policy, cost-incomplete,
  Chaos-only after exact refinement, or no cheaper than the existing fallback;
- publication requires using a coarse estimate as a certified upper;
- exact refinement requires a cap increase or mechanics/action/price change;
- one supposedly cooperative native work unit remains above 250 ms and cannot
  be decomposed without a new architecture decision;
- cancellation can publish a successful result or lose bounded abandoned
  telemetry;
- native and release WASM differ semantically; or
- exact/global closure requires a missing-price or mechanic ruling from Oliver.

No stop condition authorizes silently falling back and declaring strategy
quality recovered.

## Review Questions

External review should challenge at least these points:

1. Is pre-`restore_output_incumbent` the correct and complete capture boundary,
   or is an earlier immutable policy snapshot required?
2. Should unverified selected candidates use a new type rather than reuse
   `BoundedPolicyIncumbent` with false certification fields?
3. Can existing direct compiled-policy assertion consume a candidate snapshot
   without mutating the public `SolveResult`, or should it accept an explicit
   policy view?
4. Does any code path still compare coarse estimated cost with independently
   evaluated cost as equal publication authority?
5. Which exact-lift inner loop is the smallest sound resumable boundary for the
   measured 4,215-kernel strict pass?
6. Can strategy compilation and exact graph evaluation reuse their existing
   stateful machinery without duplicating retained graphs under the solver
   memory cap?
7. Are the non-Chaos and strictly-cheaper five-T1 acceptance checks strong
   enough to prevent another fallback-only false positive without pinning an
   unproved cost?
8. Are exactness, utility, and responsiveness kept independent at every gate?
