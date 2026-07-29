# Q-Directed Deep Solving And Automatic Eldritch Side Actions — Final Report

Date: 2026-07-28

Plan: [Q-Directed Deep Solving And Automatic Eldritch Side Actions](plan.md)

Evidence:
[tracked summary](../../../fixtures/solver-natural-t1/v1/evidence/q-directed-eldritch-side-actions-summary.json)

## Decision

Retain Q-directed refinement and the four automatic Eldritch side actions.

The Q scheduler is exact about stored action distributions and conservative
about status. Completed rows remain retained for the solve lifetime. Bellman
changes reprice their Q values but do not rebuild their probability kernels.
Unexpanded fringe mass keeps admissible lower and executable upper values.
An alternative is admitted only when its complete upper Q beats the current
executable upper, and is rejected only when its complete lower Q cannot beat
that upper. Overlap remains unresolved.

The frozen cases are not solved by this milestone. Their lower information and
deep expansion improve materially, but the upper ends of the completed
alternative intervals remain dominated by the enormous Chaos continuation
value. Product caps are unchanged and no false admission, rejection, or
exactness claim is retained.

## High-Cap Control

The pre-change control used run-local limits of 100,000,000 reforge work,
10,000,000 transitions, 512 MiB solver-owned memory, 200,000 discovered
states, 25,000 expanded states, one cooperative work item per step, and a
bounded diagnostic watchdog.

| Case | Solve wall | Rows / transitions | Action envelope | Boundary |
| --- | ---: | ---: | --- | --- |
| full-four | 2,934.008 ms | 254 / 707,000 | 0 admitted, 7 unresolved, 697 unevaluated | 200,000 discovered states |
| deep-four | 142,923.291 ms | 1,606 / 1,373,645 | historical control admitted 1,358 overlapping rows | incomplete optimization |

The deep control exposed an unsound historical fallback: it admitted
overlapping rows merely because the restricted graph was partial. That path
was removed. Only strict Q separation may classify a row while the graph is
incomplete; joint overlap admission is permitted solely after the entire
restricted graph and action envelope are complete.

## Retained Q Architecture

For every unresolved completed row, ordinary successor uncertainty begins as

`P(s' | s, a) * (U(s') - L(s'))`.

Exact row-Q width supplies the self-loop/retry normalization. Choice groups
include every successor that can still win the observed choice. Contributions
are combined across unresolved rows, then a deterministic bounded batch is
expanded in descending decision impact. Carrier-relative self transitions,
terminal exits, retry mass, action cost, and exceptional support remain in the
retained sparse row.

After a batch the solver:

1. updates admissible state lowers;
2. improves an executable upper policy;
3. reruns Bellman optimization;
4. reprices every completed delayed row;
5. admits one proved improvement, rejects proved non-improvements, and leaves
   overlap unresolved; and
6. repeats until the requested proof closes or a real resource boundary is
   reached.

The arbitrary 32-state checkpoint is only the initial handoff. Q refinement
uses batches of at least 1,024 high-impact states. A completed row is never
re-enumerated after a value change; telemetry records
`completed_rows_recomputed: 0`.

The initial probability-directed pass moved the full case only slightly and
left the deep case with combined-policy movement but weak lowers. The retained
second refinement restores the existing probability-aware optimistic clean
goal cover as an admissible incremental lower. It does not reuse the rejected
goal-count-only lower, does not treat a restricted value as global, and falls
back to zero where the abstraction cannot prove a finite value.

## Exceptional Support

Chaos state IDs remain the ordinary-support authority. Fossil forced/added
mods, Essence guarantees, and Eldritch side actions may introduce exact delta
states. Those states are interned through the normal parent solver context,
queued, and forced ahead of ordinary Q refinement. A row cannot be admitted
or rejected while any nonterminal outside-Chaos successor remains unexpanded.

The Eldritch delta oracle deliberately removes a preserved prefix goal from
ordinary random support. It discovers 186 states outside Chaos support and
expands all 337 discovered states before closing the envelope.

## Automatic Eldritch Side Actions

Automatic generation is limited to engine-certified Eldritch-eligible rare
carriers. `session_builder.cpp` grants that eligibility only to `Helmet`,
`Body Armour`, `Gloves`, and `Boots`.

At most four high-level goal-relevant options are synthesized for a carrier:

- Eldritch Annul Prefix;
- Eldritch Annul Suffix;
- Eldritch Chaos Prefix; and
- Eldritch Chaos Suffix.

The generator inspects actual stored implicit tiers. Existing requested-side
dominance uses the final currency directly. Otherwise it chooses the cheapest
priced legal real Ember/Ichor setup that establishes the requested dominance,
then applies the final real currency. Setup is neither hidden nor repaid when
already present. The exact kernel retains implicit tiers and the complete
explicit carrier.

These options are evaluated directly in the parent solver context. A
temporary automatic-option context can use a finer option-specific junk
partition whose representative cannot rematerialize the exact parent carrier;
the artifact-backed armour gate exposed that mismatch. Parent-context
evaluation preserves exact state IDs and routes Eldritch Chaos support through
the same delta-state lifecycle as other alternatives.

No automatic standalone Ember, Ichor, Eldritch Exalt, arbitrary implicit
rolling, Veiled craft, or Influence Exalt is added. Raw Eldritch Chaos,
Eldritch Annul, target-side metamod behavior, manual Calculator/Emulator
actions, and the retry-basin exclusion remain unchanged.

## Exact Gates

Native controls prove:

1. the largest exact probability-weighted uncertainty contribution is chosen;
2. successor refinement tightens its parent interval;
3. a proved better row is admitted;
4. a proved worse row is rejected;
5. admission triggers Bellman reoptimization;
6. multiple admissions form a combined improved policy;
7. omitted fringe mass retains certified bounds;
8. exceptional support is expanded before classification;
9. completed probability rows are not recomputed; and
10. repeated transition and policy hashes are deterministic.

Eldritch controls prove absent behavior on an ineligible session, exact
prefix/suffix dominance, direct execution under existing dominance, paid
setup under missing dominance, opposite-side preservation, target-side Annul,
real compiled operations, delta expansion, and different side choices on
different carriers.

The synthetic compiled Eldritch policy has exact expected cost `10.473684`.
Ten thousand deterministic simulator runs measured `10.571800`, with 10,000
successes and no unapplied action or missing edge.

The artifact-backed Vaal Regalia control uses a completed prefix goal and a
missing suffix goal with suffix junk. Bellman selects an automatic suffix-side
action without a prescribed route. It returns a bounded executable policy over
126 states, with 125 nonterminal states expanded.

## Frozen Qualification

The final full-four diagnostic retained the high-cap run-local limits.

| Measurement | Full-four |
| --- | ---: |
| Root lower / executable upper | 432.406853 / 60,341,416.987842 |
| Discovered / expanded | 200,000 / 8,908 |
| Rows / transitions | 56,413 / 1,188,077 |
| Reforge work | 14,246,493 |
| Q refinement | 9 rounds / 9,216 selected states |
| Completed rows recomputed | 0 |
| Unique kernels / carrier reuse | 8 / 8,907 |
| Live / peak owned bytes | 83,599,787 / 150,805,909 |
| Solve wall | 18,471.297 / 16,856.539 ms |
| Stop | `max_discovered_states` |

Completed exact intervals were:

| Action | Before lower Q | Final lower Q | Final upper Q |
| --- | ---: | ---: | ---: |
| Lucent | 9.230433 | 439.802286 | 60,341,420.464133 |
| Harvest attack | 3.316419 | 433.790212 | 60,341,418.631810 |
| Harvest cold | 14.522785 | 444.999901 | 60,341,428.559379 |
| Harvest elemental | 47.701443 | 478.173969 | 60,341,463.119122 |
| Harvest mana | 89.249817 | 519.829764 | 60,341,500.471415 |
| Harvest physical | 14.524469 | 444.999935 | 60,341,429.278155 |

Every interval still overlaps the incumbent. The result therefore reports zero
admitted, zero proved non-improving, seven unresolved, 195,969 unevaluated,
and an open remaining envelope of 195,976. It is a bounded executable policy,
not exact.

The release-binary repetition preserves bounds, states, expansions, rows,
transitions, reforge work, Q refinement, completed intervals, transition hash
`d3c8789915cd57b4`, and policy hash `8b2a568f3c9cfd35`. Wall time varies and is
not part of the deterministic contract.

The bounded deep-four run was intentionally stopped rather than allowing one
diagnostic to dominate the milestone. Its last cooperative snapshot at
267,574.736 ms records:

- root interval `432.410495 .. 185,688,633.771637`;
- 200,000 discovered and 11,570 expanded states;
- 72,703 rows and 1,929,459 transitions;
- 32,227,320 exact reforge work;
- 250,178,922 solver-owned live bytes; and
- refinement round 134.

Compared with the original Chaos incumbent `185,688,651.382798`, the
executable upper improves by `17.611161` while the certified lower rises from
`1` to `432.410495`. Because this is a running partial snapshot, it carries no
final action classification or exactness claim.

Wall figures are specific to this GCC build and machine. Deterministic work,
states, Q witnesses, lifecycle counts, and hashes are the portable evidence.

## Acceptance And Scope

The artifact-backed native suite passes 515,252 checks with zero failures,
including the new 10,000-run compiled Eldritch control. The release native
build and release WASM rebuild pass. The non-visual web suite and TypeScript
no-emit pass. Benchmark validation accepts all 12 standard and 146 natural-T1
specifications. The final release build is warning-free.

The default unrestricted solver, product caps, action filtering outside the
four automatic options, economy accounting, manual Eldritch surfaces, and
compiled primitive semantics remain unchanged. No new frozen strategy
qualified for compilation or simulation; the returned frozen policy remains
the prior exact gated Chaos renewal witness.
