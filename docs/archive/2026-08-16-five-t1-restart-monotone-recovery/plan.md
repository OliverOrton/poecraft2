# Five-T1 Restart-Monotone Strategy Recovery

**Status: stopped on 2026-08-16 at Gate 4 stop condition 5, with stop
conditions 6 and 7 also present. Gates 2, 3, and 5-8 were not started.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](README.md)

## Objective

Make Oliver's real from-empty five-natural-T1 Conquest Lamellar request return
a useful, independently certified strategy in the normal Calculator when a
priced base admits Restart, without weakening proof standards, hiding action
scope, raising caps, or turning automatic crafting families into decorative
registry entries.

The immediate product contract is:

1. a valid extra action such as Restart must never erase a cheaper executable
   policy that was available in the smaller action set;
2. bounded-policy certification must expose every route miss instead of
   silently turning it into a legal Restart;
3. the Calculator's high-impact scheduler must actually synthesize and
   evaluate relevant state-local automatic candidates;
4. exact graph evaluation must fit the existing memory cap and remain
   cooperatively interruptible for the graph sizes already emitted by the
   solver; and
5. the five-T1 result must be materially useful, not merely a few chaos below
   the approximately 37.28-million-chaos renewal fallback.

This milestone does not promise global optimality. If alternatives remain
unclassified, the result remains bounded with lower zero. It does promise that
the published upper is the cheapest retained, executable, fail-closed-
certified candidate and that adding valid actions cannot make that upper worse.

## Frozen Product Witnesses

All primary witnesses use:

- empty rare item-level-86 Conquest Lamellar
  (`Metadata/Items/Armours/BodyArmours/BodyStrDex20`);
- published Allflame source snapshot
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`;
- all five natural T1-or-better goals:
  `LocalIncreasedArmourAndEvasionAndStunRecovery6`,
  `LocalIncreasedArmourAndEvasion8`,
  `LocalBaseArmourAndEvasionRating8`,
  `AdditionalPhysicalDamageReduction5_`, and
  `ChanceToSuppressSpellsHigh5___`; and
- normal Calculator options, including goal-progress-gated reforges and
  high-impact executable uppers.

### Witness A - Restart-free reference

The source snapshot has no base price and therefore exposes 27 candidate
actions with no admitted Restart. The currently qualified release WASM result
is:

| Evidence | Current result |
| --- | ---: |
| termination | `bounded_feasible / numerical_stability` |
| lower / evaluated upper | `0 / 624800.9519118543` |
| graph | 184 nodes / 666 edges / 482,233 JSON bytes |
| selected-candidate status | `verified_retained` |
| selected actions | 9 distinct action IDs |

This value is now provisional evidence, not an acceptance oracle. The emitted
bounded graph contains 170 default edges to `bounded_default_restart`, so its
previous zero-off-policy evaluation did not prove that all intended routes
were covered. Gate 1 must certify the same policy with fail-closed defaults
before this result can be reused as the monotonic incumbent.

### Witness B - Oliver's priced-base request

Oliver's attached Calculator diagnostic has the same goal and source snapshot.
Its effective economy identity is
`economy:allflame:de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0:effective:6543902fbf647920:pin:7789a049f514607d`.
The only reconstructed override is `base = 5`, which adds Restart and changes
the candidate count from 27 to 28.

| Evidence | Current result |
| --- | ---: |
| termination | `bounded_feasible / numerical_stability` |
| lower / evaluated upper | `0 / 37279857.73995944` |
| selected coarse estimate | `37279651.776847705` |
| selected-policy structure | 867 reachable states / 216 reachable rows |
| selected-candidate status | `not_materializable` |
| fallback | independently evaluated Chaos renewal |
| largest public worker step | `2498.6` ms |

The direct core policy did compile to 2,015 nodes and 4,123 edges. Exact
evaluation refused it before component discovery because the observation
fixed-point preflight projected 1,373,053,076 transient bytes against a
1,054,136,463-byte allowance. The probe was
`observation_fixed_point_dense_nodes`, with 2,015 units at 226,996 bytes per
unit. This is an evaluator-representation refusal, not a strategy-JSON refusal.

The terminal progress snapshot is also false: it reports zero expanded and
discovered states even though the solve records source generation 1,885 and
target generation 1,785.

### Witness C - Automatic-admission control

The same five-T1 request with high-impact scheduling disabled was cancelled
after 30 seconds. It proves that state-local automatic generation is real but
cannot simply be enabled exhaustively:

| Evidence | Result at cancellation |
| --- | ---: |
| automatic carriers | 82 |
| admission-discovered states | 54,995 |
| rows / transitions | 14,959 / 2,821,458 |
| reforge work | 2,941,611 |
| Eldritch candidates / eligible / rows | 392 / 390 / 390 |
| protected-side candidates / eligible / rows | 810 / 76 / 60 |
| temporary-bench candidates / eligible / rows | 1,320 / 892 / 892 |
| Cannot Roll candidates / eligible / rows | 49 / 10 / 10 |
| Imprint considered / admitted | 82 / 0 |
| largest public worker step | `4797.835` ms |

The product's high-impact mode instead reports zero automatic carriers because
its scheduler bypasses state-local dynamic preparation. The repair must be
prioritized and cooperative; disabling high-impact mode is not acceptable.

## Review Of The Additional Audit

The external review was useful, but its findings do not all describe the
current tree.

| Claim | Current-tree disposition |
| --- | --- |
| Bounded route defaults hide misses | Accepted and promoted to Gate 1. `solver_compile.cpp` routes bounded defaults to `bounded_default_restart`; the current five-T1 artifact has 170 such defaults. |
| Distinguish graph-scale failure from policy quality | Accepted. Compilation size, evaluator memory, evaluator time, policy materialization, and policy value become separate classifications. |
| Require a material strategy-quality result | Accepted. A tiny improvement over 37.28 million is not success. |
| Use a smaller viability ladder first | Accepted. Fail-closed routing and automatic admission qualify on focused controls before the priced-base five-T1 primary runs. |
| Four-goal condition hoisting blocks this milestone | Rejected as stale. The cited 60,148,774-byte artifact is from an older compiler path. The current corresponding corpus graph is 68,473 bytes, the current Conquest four-goal graph is 5,205,249 bytes, and the current five-goal graph is 482,233 bytes. The priced-base direct graph already compiles and fails later in evaluation memory. |
| Cross-edge condition sharing, operation-node merging, condition memoization | Deferred unless Gate 0 timing attributes the selected boundary to one of them. They are not current proof blockers. |
| Operation-node duplication caused evaluator memory | Rejected, consistent with the review's correction. State-to-node execution is functional; the observed refusal is dense observation-requirement propagation. |
| Refactor duplicated sparse-row Q math now | Deferred. The dangerous divergence is already fixed and this milestone has a reproduced product boundary. |

The following completed work is explicitly out of scope: strict sparse-policy
objective ordering, the separate numerical-stability latch, truthful
termination, certified global lowers, removal of the old denominator cutoff,
Calculator Fossil-odds isolation, and replacement of the old flat router by a
region DAG.

## Source-Confirmed Diagnosis

1. `apps/web/src/app/solver-result-presentation.ts` enables
   `high_impact_executable_uppers` for normal Calculator solves.
2. The high-impact branch of
   `SolveWork::Impl::schedule_next_incremental_alternative()` schedules only
   the static delayed operator set. It never enters the dynamic preparation
   path used below that branch.
3. `advance_incremental_dynamic_preparation()` calls the existing cooperative
   state-local automatic generator, then separates newly appended dynamic
   operators from static operators. That path is functional when high-impact
   mode is off.
4. The only focused high-impact native regression currently covers early Chaos
   renewal. It does not combine high-impact scheduling with state-local
   automatic synthesis.
5. `solver_compile.cpp` chooses `bounded_default_restart` for bounded policy
   route defaults and `offpolicy` for exact policy defaults. A legal Restart
   can therefore absorb a bounded route miss while reporting zero off-policy
   mass.
6. `build_policy_observation_requirements()` constructs a union requirement,
   projects three dense copies of that maximum payload for every graph node,
   and rejects the priced-base graph at the pre-component memory check.
7. `propagate_policy_observations()` already groups equal propagation
   authorities, but still stores and copies a full `ObservationRequirement`
   for every node on every round. The smallest exact representation change
   must be chosen from measured payload and group cardinalities, not assumed.
8. The Restart-free selected policy remains executable when a priced Restart
   is added. Therefore the true optimum and the best retained executable upper
   cannot worsen solely because the action set grew. The current approximately
   59.7x publication regression is a candidate-preservation failure.

## Proof And Publication Contract

The implementation must preserve this authority chain:

```text
restart-free action subset
  -> selected executable seed
  -> fail-closed compiled certification graph
  -> independently evaluated retained incumbent
  -> release Restart and prioritized automatic candidates
  -> solve/refine additional candidate policies
  -> fail-closed certification of every contender
  -> exact evaluation of the product graph
  -> cheapest certified publication
```

- A certification graph routes every unmatched policy condition to
  `offpolicy`, including bounded policies.
- A product graph may retain the existing safe Restart fallback for bounded
  execution, but it cannot certify itself merely because Restart eventually
  succeeds.
- Certification and product graphs may differ only in compiler-designated
  default targets. Zero reachable fail-closed default mass proves that this
  difference is behaviorally unreachable for the evaluated start.
- A failed, incomplete, or unmaterializable new candidate cannot evict an
  already certified incumbent.
- Adding a priced action may improve the upper or leave it unchanged. It may
  widen the unresolved envelope and keep the lower at zero; it may not worsen
  the retained upper.
- Resource-deferred automatic work remains explicit open-envelope work. It
  cannot be described as rejected, complete, exact, or irrelevant.

## Acceptance Definition

The milestone is complete only when all of these hold:

1. The Restart-free five-T1 policy passes fail-closed exact evaluation with
   success probability 1, properness, complete cost, zero off-policy mass,
   zero unresolved terminal mass, and zero reachable route-default hits.
2. The priced-base five-T1 run publishes an independently evaluated policy at
   no greater cost than the fail-closed-certified Restart-free incumbent plus
   the existing value-comparison tolerance.
3. The priced-base evaluated upper is at most `1,000,000` chaos. This is a
   product materiality target informed by the approximately 690,872 coarse
   estimate and 624,801 current candidate; passing monotonicity against a
   degraded renewal baseline is not sufficient.
4. The published product graph and its fail-closed certification graph agree
   on reachable behavior and evaluated cost. The product graph is executable,
   proper, and cost-complete.
5. High-impact mode records nonzero state-local automatic carriers and a
   truthful per-family admitted/rejected/deferred ledger. The focused
   Eldritch control must produce an eligible Eldritch row; no automatic family
   is required to win the primary policy.
6. Any unclassified static or automatic action keeps action-envelope closure
   false and the certified global lower at zero. `ExactClosed` remains
   unavailable unless the full priced supported envelope actually closes.
7. The priced-base release-WASM primary finishes within five minutes on the
   qualification machine, no public worker step exceeds 250 ms, cancellation
   remains observable in every long phase, and terminal progress retains real
   state/generation counters.
8. The Restart-free reference does not regress in termination truth, evaluated
   upper, action scope, or responsiveness. Hashes may change only when the
   fail-closed or automatic-action repair changes the executable graph, and
   every change must be explained by evidence.
9. Required fast compiled-strategy controls complete 10,000 Simulator runs
   with zero failures, cap hits, and off-policy executions. The five-T1 graph
   may continue to use independent exact evaluation instead of sampling when
   its expected action horizon makes 10,000 runs inapplicable.

## Non-Goals

- No Path of Exile mechanic changes or new owner rulings.
- No cap increase, larger WASM heap, looser tolerance, approximate state merge,
  or lower-bound promotion.
- No frontend crafting-rule authority or TypeScript reimplementation of action
  relevance.
- No switch back to exhaustive non-high-impact scheduling.
- No claim that Eldritch, protection, temporary bench, Cannot Roll, Imprint,
  or any other automatic family must appear in the winning five-T1 policy.
- No condition-hoisting, cross-edge sharing, operation-node merging, or
  evaluator condition memoization unless new timing evidence selects it.
- No sparse-Q cleanup, full solver rewrite, or revival of previously rejected
  broad-row/factorization experiments.
- No rendered browser review; Oliver retains that responsibility.

## Gate 0 - Freeze Truth And Cost Attribution

Add checked fixtures for Witnesses A and B and retain the current diagnostic C
as raw evidence. Freeze:

- item, goal, source/effective economy identities, and the sole `base = 5`
  override;
- candidate and admitted action IDs, including the one-action Restart delta;
- selected-policy identity, graph identity, source/target generations, and
  publication-candidate ledger;
- product and certification graph nodes, edges, JSON bytes, condition bytes,
  default-target counts, and hashes;
- per-family automatic carriers, candidates, eligible candidates, rows,
  transitions, reforge work, and final disposition;
- observation-propagation node count, group count, round count, direct and
  propagated payload percentiles, unique canonical requirement count, actual
  retained/peak bytes, projected bytes, and stage wall time; and
- per-step and per-phase wall time, including the longest cooperative step.

Add a stable failure classification that separates at least:

- `policy_not_materializable`;
- `strategy_compile_size_cap`;
- `strategy_compile_memory_cap`;
- `exact_eval_observation_memory_cap`;
- `exact_eval_component_memory_cap`;
- `exact_eval_time_or_round_cap`;
- `missing_price`;
- `unsupported_action`; and
- `route_coverage_failure`.

Instrumentation must be behavior-neutral. Compare values, bounds, termination,
action IDs, policy/strategy hashes, cap accounting, and the candidate ledger.
Also compare median wall time across at least three warmed focused controls; a
regression greater than 10% or 25 ms, whichever is larger, requires attribution
before Gate 0 closes.

Gate 0 runs only focused reproductions. Do not run the full repository
acceptance pipeline.

## Gate 1 - Make Bounded Certification Fail Closed

Introduce an internal compiler default-target mode with two explicit uses:

- product bounded graph: the existing safe Restart fallback; and
- certification graph: `offpolicy` for every unmatched policy route.

`CompiledPolicyAssertionWork` and every candidate-certification caller must use
the fail-closed form even when the candidate is bounded. If a precompiled
product graph is supplied, build or retain a paired certification artifact;
do not certify the product artifact's Restart defaults.

Prove structurally that the pair differs only at compiler-designated default
targets. Exact-evaluate both artifacts and require:

- zero off-policy and unresolved mass on the certification graph;
- zero reachable certification-default hits;
- identical start cost and reachable selected operations; and
- deterministic paired identities in native and WASM.

Add a minimal regression whose incomplete bounded router previously succeeded
through Restart. It must now fail certification with
`route_coverage_failure`, while its product graph remains safely executable.

Then re-certify Witness A. If the 624,800.9519118543 candidate takes any
fail-closed default, stop here, correct the accepted evidence and HANDOFF, and
produce a new precise routing boundary. Do not build later gates on an invalid
incumbent.

## Gate 2 - Preserve A Restart-Free Incumbent Before Expanding Scope

When Restart is priced, stage it as an unresolved incremental action instead
of allowing it to perturb the first restricted policy solve. The initial
subset must match Witness A's supported, priced, non-Restart actions under the
same Allflame quotes.

Before releasing Restart:

1. solve the restricted subset;
2. capture and fail-closed-certify its selected candidate;
3. exact-evaluate the product graph;
4. retain it in the publication portfolio; and
5. mark Restart as outstanding action-envelope work.

Release Restart only after a useful certified seed exists. Continue the normal
Bellman and candidate lifecycle with Restart included, but never replace the
seed with an unmaterializable, unevaluated, or more expensive candidate.

Add action-set monotonicity regressions for:

- the exact five-T1 request with and without `base = 5`;
- a small synthetic case where an added action is irrelevant;
- a case where an added action improves the policy; and
- a resource stop immediately after the added action is released.

In every case the full-scope published upper must be no worse than the
certified subset seed. A larger action set may keep exactness open; that is not
an upper-bound regression.

## Gate 3 - Admit State-Local Automatic Work In High-Impact Mode

Integrate the existing cooperative dynamic-preparation continuation into the
high-impact scheduler. Do not duplicate automatic mechanics or materialize all
carriers eagerly.

Use bounded waves:

1. prioritize carriers reachable under the retained incumbent;
2. order remaining carriers by the existing certified-upper/relevance and
   uncertainty evidence;
3. run cheap legality, preservation, and mandatory-setup-price checks before
   an exact kernel;
4. resume the existing state-local coroutine for at most one bounded
   checkpoint per public solve step;
5. evaluate newly admitted dynamic operators through the same exact
   state/operator row lifecycle as static delayed operators; and
6. re-optimize and re-rank after each wave that changes the certified
   incumbent.

Extend the completed-pair ledger so dynamically appended operators cannot be
silently skipped or evaluated twice. A carrier/family combination must end as
admitted, ineligible with reason, missing-price, unsupported, resource-
deferred, or not applicable. A fully evaluated admitted row, proven
ineligibility, or proven non-applicability may close its obligation. Missing
price, unsupported evaluation, and resource deferral keep the relevant
envelope open.

Qualify this architecture first on the three-prefix witness and focused forced
automatic controls. Do not run the priced-base five-T1 primary until:

- high-impact mode records nonzero automatic carriers;
- the forced Eldritch row is admitted and evaluated;
- protection, temporary-bench, Cannot Roll, Imprint, and Warlord controls
  retain their existing semantics; and
- no focused public step exceeds 250 ms.

If the first useful automatic row still requires the exhaustive 82-carrier,
54,995-state expansion shape, stop with the exact carrier/family/work ledger.
Do not disable high-impact mode or raise caps.

## Gate 4 - Scale Exact Observation Certification Under The Existing Cap

Use Gate 0's actual payload distribution to select the smallest exact change.
Preferred candidates, in order, are:

1. one propagated requirement per existing equal-authority propagation group;
2. interned immutable canonical requirement payloads with copy-on-write merge;
3. a sparse predecessor worklist that retains only changed requirements; or
4. a measured combination of the above.

Do not choose from the current worst-case union projection alone. Preserve the
same canonical requirement merge, action-preservation transfer, fixed point,
assignments, collapse/preservation telemetry, and round-cap semantics.

Memory accounting must cover actual owned payloads and bounded transient
scratch without multiplying one global maximum payload by every node when
that storage is not simultaneously owned. Conversely, do not replace a safe
overestimate with unaccounted allocation.

Make observation propagation a retained cooperative evaluator stage when one
round or group wave can exceed the worker budget. Cancellation must abandon it
without publishing a partial result.

Qualify in this order:

1. a tiny cyclic observation fixed-point oracle;
2. the exact three-prefix graph, with byte-for-byte value and route parity;
3. the current 184-node five-T1 graph;
4. the priced-base 2,015-node/4,123-edge direct graph; and
5. the four-natural-T1 and historical evaluator-memory controls.

Gate 4 passes only if the priced-base graph exact-evaluates under the unchanged
cap. If it reaches a new non-observation-memory boundary, record that boundary
accurately and stop the milestone there. A cap increase is not a pass.

## Gate 5 - Make Candidate Selection And Progress Monotone

> Execution note: the diagnostic Gate 4 qualification reached
> `exact_eval_pair_discovery_memory_cap` after observation propagation passed.
> Per Gate 4 and stop condition 5, execution stopped before this gate. See the
> [Gate 1/Gate 4 evidence](evidence/gate1-gate4-stop.md).

Keep the certified Restart-free seed alive across:

- Restart release;
- automatic-candidate waves;
- policy numerical stability;
- selected-policy materialization failure;
- direct certification refusal;
- strict-lift refusal; and
- resource or cancellation exits.

Every candidate must retain construction origin, selection authority,
certification artifact identity, product artifact identity, exact evaluated
cost, and disposition. Select strictly by independently evaluated finite cost;
do not compare estimates against evaluated candidates.

Repair terminal progress packaging so `done` preserves the last real expanded,
discovered, frontier, generation, row, transition, reforge, phase, and
finalization counters. A packaging-only finish cannot replace real counters
with zeros.

Expose the new failure class and fail-closed certification evidence through
the existing native -> WASM -> worker -> Calculator diagnostics contract. The
frontend presents native truth and acquires no solver authority.

## Gate 6 - Focused Native Qualification

Run one focused native qualification matrix after Gates 0-5 are complete:

- fail-closed router miss oracle;
- action-set monotonic synthetic controls;
- three-prefix Conquest Lamellar;
- three-prefix plus spell-suppression Conquest Lamellar;
- four-natural-T1 Conquest Lamellar;
- five-natural-T1 Witness A;
- priced-base five-natural-T1 Witness B;
- forced Eldritch;
- protected-side and temporary-bench controls;
- Cannot Roll;
- Warlord influence exalt; and
- Imprint.

Witness B must meet the full acceptance definition, including the monotonic
upper and `1,000,000`-chaos materiality ceiling. Record candidate origins,
paired graph hashes, default-hit counts, automatic-family verdicts, evaluator
memory, step distribution, total wall time, and final progress.

Do not proceed merely because Witness B beats Chaos by the ordinary floating-
point comparison tolerance. If it is above the materiality ceiling, record
`gate_passed_soundness_quality_unrecovered` and stop with the measured next
bottleneck.

Do not run the full repository acceptance pipeline in Gate 6.

## Gate 7 - Release-WASM Product Qualification

Rebuild the tracked release WASM from the accepted native source. Run the same
Calculator-owned request construction used by the product for Witnesses A and
B; do not substitute benchmark-only options.

Require native/release-WASM parity for:

- action IDs and the Restart delta;
- policy status, termination, stop cause, and lower/upper bounds;
- paired certification/product graph identities and exact evaluated cost;
- automatic-family obligation counts and dispositions;
- candidate selection origin;
- terminal progress counters; and
- resource-cap and failure classification.

Run small-step and 1,024-work controls for Witness B. Both must satisfy the
same semantics and cost, the normal small-step profile must finish within five
minutes, and neither may exceed the 250 ms public-step ceiling.

Run required 10,000-run Simulator verification on the fast Eldritch, Warlord,
Imprint, four-goal, and other selected compiled controls. Use exact graph
evaluation as the five-T1 authority when its action horizon remains beyond the
unchanged Simulator verification limit; record that decision explicitly.

This gate uses focused native/WASM and web protocol tests only. Oliver owns
rendered review, so no browser screenshot or visual smoke is part of the plan.

## Gate 8 - Final Acceptance, Documentation, And Archive

After every earlier gate passes:

1. run the native build;
2. rebuild release WASM if the accepted source is newer than the Gate 7
   artifact;
3. run `npx tsc --noEmit` and the complete non-visual web suite;
4. run `powershell -File scripts/test.ps1` exactly once as the final repository
   acceptance pipeline;
5. run `git diff --check`;
6. update stable solver, WASM, Calculator, evidence, and decision documents;
7. archive this plan with exact result values and hashes; and
8. leave `HANDOFF.md` at the next truthful boundary.

The final record must say separately whether the milestone recovered:

- certification truth;
- action-set-monotone publication;
- automatic-action reachability;
- evaluator scalability;
- WASM responsiveness; and
- material five-T1 strategy quality.

A sound bounded result above the materiality ceiling is valuable engineering
progress, but it is not completion of this plan.

## Checkpoint Commits

Use coherent local checkpoints, each ending with the required agent co-author
line:

1. freeze witnesses and add failure/scale telemetry;
2. make bounded certification fail closed;
3. retain the Restart-free monotonic incumbent;
4. integrate prioritized automatic admission into high-impact scheduling;
5. scale cooperative exact observation propagation;
6. make publication/progress monotone across failures;
7. qualify native and release WASM; and
8. record final acceptance and archive.

Do not push unless Oliver explicitly asks.

## Stop Conditions

Stop and update `HANDOFF.md` with a precise boundary if any of these occurs:

1. Witness A takes a fail-closed route default. The prior 624,800 result is not
   a qualified incumbent until routing is repaired.
2. The priced-base restricted seed cannot reproduce the fail-closed Witness A
   policy under identical non-base prices. Attribute the action/economy/state
   difference before proceeding.
3. Adding Restart causes the retained certified upper to increase. Do not
   relax the monotonicity requirement.
4. High-impact automatic admission remains at zero carriers, loses a dynamic
   pair, or requires exhaustive carrier expansion before its first useful row.
5. The exact evaluator cannot certify the 2,015-node graph under the unchanged
   cap without a materially broader representation change. Record actual
   payload/group evidence; do not raise the cap.
6. Any public worker step remains above 250 ms after the owning phase is made
   cooperative.
7. Witness B remains above `1,000,000` chaos or returns only the renewal
   fallback. Report soundness progress without claiming strategy recovery.
8. A mechanics ambiguity appears. Ask Oliver instead of inferring game rules.
9. Final acceptance fails. Do not archive or describe the milestone as
   complete.

## Review Questions

The pre-selection review challenged:

1. whether fail-closed certification plus product-graph exact evaluation is
   sufficient to prove the safe Restart defaults unreachable from the start;
2. whether delaying Restart is the smallest way to recover the action-subset
   incumbent without changing the solver objective;
3. whether the `1,000,000`-chaos threshold is the right material product bar;
4. whether Gate 0 exposes enough actual observation-payload structure to
   choose between grouping, interning, and a worklist; and
5. whether the automatic admission wave order can miss a cheaper incumbent
   without truthfully retaining the remaining obligations as open.
