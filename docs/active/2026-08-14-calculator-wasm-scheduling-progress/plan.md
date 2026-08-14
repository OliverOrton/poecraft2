# Calculator WASM Scheduling And Progress

**Status: active.** Selected by Oliver on 2026-08-14 from a live release-WASM
Calculator report.

Parent: [Active work](../README.md)

Frozen source baseline:
`191bc010aad7ece2a227766d6000117074ad9e52` on
`codex/solver-goal-realignment`.

## Objective

Make the ordinary Calculator release-WASM path finish the selected Allflame
three-prefix solve within a useful product boundary, without weakening solver
semantics, raising resource caps, or trading away cooperative cancellation.
While work is legitimately continuing, the Calculator must expose changing
work that distinguishes a busy solver from a stalled one.

The live witness is an empty rare item at item level 86:

- base: `Metadata/Items/Armours/BodyArmours/BodyStrDex20` (Conquest Lamellar);
- league snapshot: published Allflame snapshot
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`;
- success means all three natural prefix goals at T1 or better:
  `LocalIncreasedArmourAndEvasionAndStunRecovery6`,
  `LocalIncreasedArmourAndEvasion8`, and
  `LocalBaseArmourAndEvasionRating8`; and
- normal Calculator options and action pricing, including
  `goal_progress_gated_reforges = true`.

The primary reproduction uses the published snapshot with no manual price
override because that is the product default implied by the supplied league
and item. The snapshot has no `base` price. A separate `base = 5` run is a
labelled benchmark control because the previously qualified four-goal fixture
used that override; it is not substituted for the live product request.

## Starting Diagnosis

The archived 147.292-second release-WASM result is not a normal Calculator
latency result. Its fixture explicitly requests 1,024 work items per step and
enables the diagnostic `high_impact_executable_uppers` scheduler. The
Calculator omits `chunkSize`, so the worker currently clamps the solve to at
most eight work items per step and does not enable that diagnostic option.
Small boundaries can repeat whole-graph Bellman work and select a materially
more expensive scheduling trajectory; this must be measured, not assumed.

The displayed 1,263 states is only the expanded-state counter. During dynamic
preparation, constructive policy work, focused lower work, alternative
scheduling, or refinement, that counter and the bounds may remain unchanged
while other existing native progress counters advance. A zero global lower
while the incremental action envelope is open is an intentional proof
fail-safe and may not be replaced with an uncertified positive lower.

## Invariants And Non-Goals

- Preserve action scope, prices, expected-cost objective, exactness,
  certificates, policy/strategy semantics, and public caps.
- Preserve the open-envelope zero-lower rule and all result-truth contracts.
- Do not enable the benchmark-only high-impact scheduler in product merely
  because it was fast in the archived qualification run.
- Do not solve throughput by creating long uninterruptible worker steps.
- Do not change Path of Exile mechanics or add action families.
- Do not redesign the solver, state abstraction, or quotient in this chunk.
- Do not perform rendered or visual UI review; Oliver owns it.
- Run the full acceptance pipeline once, only after the selected source and
  product changes are complete.

## Gate 0 - Freeze And Product-Equivalent Reproduction

Add a deterministic three-prefix fixture and a release-WASM diagnostic run
that uses the same goal, economy, product action envelope, prices, options,
and effective eight-item worker maximum as the Calculator. Freeze request,
artifact, WASM, economy, and binary identities.

Capture the progress time series, elapsed time, result or cooperative
watchdog stop, discovered/expanded states, frontier, policy sweeps, rows,
transitions, reforge work, memory, bounds, and incumbent/publication fields.
Use a bounded watchdog; do not leave an indefinite solve running.

Run separately, with the request otherwise fixed:

1. normal product scheduling (effective maximum eight), high-impact off;
2. 1,024 work items, high-impact off, isolating step granularity;
3. normal product scheduling, high-impact on, isolating scheduler mode; and
4. 1,024 work items, high-impact on, matching the old qualification profile.

The no-override product witness is primary. Run `base = 5` only where needed
to compare with the established benchmark profile. Gate 0 accepts a precise
attribution even if a watchdog stops one or more modes; a stopped run is not a
solver result.

## Gate 1 - Scheduling And Progress Attribution

Identify where small product steps repeat expensive work and whether the
solver is making hidden progress while the displayed fields are flat. Compare
terminal status, lower/upper/value, exact evaluation, deterministic policy and
compiled-strategy hashes, total work, number and distribution of worker-step
durations, repeated Bellman/optimization work, memory, and cancellation
latency.

The candidate must be selected from evidence. It may use adaptive cooperative
budgeting or resume more work across boundaries, but it may not alter the
mathematical request or silently activate a diagnostic solver mode. If a
solver-internal architecture change, cap increase, or proof weakening is
required, stop with a new precise handoff.

## Gate 2 - Product Scheduler And Truthful Progress

Implement the smallest evidence-supported correction. The normal Calculator
profile must retain cooperative progress callbacks and cancellation. On the
selected witness, no individual worker step may exceed 250 ms, and a cancel
request must be observed at the next cooperative boundary without another
multi-second solve step.

Use already available progress authority where possible. At minimum the
Calculator must show enough of discovered states, frontier, rows/transitions,
reforge work, memory, and elapsed time that changing work remains visible when
expanded states and bounds are temporarily flat. Do not manufacture a
positive lower or label an inferred subphase as native proof authority. A new
C ABI field is allowed only if existing telemetry cannot state the necessary
fact, in which case every downstream binding named by the change-impact map
must be updated.

Add focused native/WASM/worker/Calculator regressions for the changed contract.
If cancellation currently discards the only useful stopped-run telemetry,
retain a compact final progress snapshot without confusing cancellation with
a published solve result.

## Gate 3 - Focused Qualification

Rebuild native and release WASM after their respective source changes. Run the
three-prefix product-equivalent case and the four-goal qualification control.
The product case must:

- terminate within five minutes on the release-WASM product profile;
- match the terminal semantics of the fixed 1,024-work control (and remain
  exact if that control closes exactly);
- publish a proper executable policy whenever the control does;
- have truthful globally certified lower provenance;
- compile and exact-evaluate with the reported expected cost; and
- remain responsive under the Gate 2 worker-step and cancellation boundary.

The four-goal control must retain native/release-WASM semantic equality and
its existing under-five-minute exact result. Run exactly 10,000 simulator
trials for any changed compiled strategy that requires verification; do not
repeat simulation for byte-identical strategies.

## Gate 4 - Final Acceptance And Archive

After all selected implementation work is complete:

1. audit the diff and downstream contract surfaces;
2. run the focused native solve/API/evaluation tests;
3. run focused non-visual web/WASM tests and `npx tsc --noEmit`;
4. rebuild the tracked release WASM when engine-visible behavior changed;
5. run the selected product and four-goal semantic/performance controls; and
6. run `powershell -File scripts/test.ps1` once as final acceptance.

Record raw-report hashes, timings, request identities, responsiveness,
statuses, proof fields, exact-evaluation results, policy/strategy hashes, and
any 10,000-run verification. Update stable docs, archive this plan, and return
`HANDOFF.md` to no active boundary.

## Checkpoints

Create coherent local-only commits with the required co-author line for:

1. plan activation and frozen reproduction contract;
2. reproducible fixture and Gate 0/1 attribution;
3. accepted scheduling/progress implementation; and
4. final acceptance evidence, documentation, and archive.

## Stop Conditions

Stop and hand off precisely if the product and control requests differ in
action scope or prices unexpectedly; native and WASM differ semantically;
progress work changes mathematical results; passing requires a cap increase,
proof weakening, or multi-second cooperative steps; the selected case loses a
proper policy or exact closure available to its control; or a mechanic ruling
is required.

