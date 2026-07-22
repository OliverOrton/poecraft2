# Session Handoff

**Status: B1 and B2 are complete and accepted. B3 seeded natural-T1 corpus
generation is the exact next chunk; no B3 code has been started.**

Oliver selected the
[bounded policy results and benchmarking plan](docs/active/bounded-policy-and-benchmarking.md)
on 2026-07-22. The branch is `codex/bounded-policy-contract`, based on clean
`main` commit `60500ef`. Boundary documents were committed as `0c18493`; the
B1 implementation, including retained constructive-witness reuse, is committed
as `58aa5ea`; B2 product presentation is committed as `86e337f`.

The preceding exact constructive-policy milestone remains review evidence only
on `codex/exact-search-design` at `273831f`. It was not merged, rebased,
cherry-picked, or used as a source donor. Candidate A/C search guidance remains
forbidden. The incumbent bundle and retained witness are for executable
upper-bound/output production only.

## B1 accepted boundary

B1 implements ABI v2 bounded-result vocabulary, exact/bounded policy status and
termination separation, numerical-versus-product gap separation, atomic
same-round incumbents, complete bounded-policy stitching, target-gap stopping,
exact native/compiled policy evaluation and occupancy foundations, downstream
Python/WASM/TypeScript propagation, benchmark reporting, stable documentation,
and rebuilt WASM artifacts.

Targeted evidence on the committed implementation:

- `scripts/build.ps1` passed;
- `poecraft_engine_tests.exe --solver-solve-only` passed 391 checks with zero
  failures;
- the earlier ABI/API narrow run passed 45,717 checks with zero failures;
- all six changed benchmark manifests passed `--validate-only`;
- `npx tsc --noEmit` passed;
- `scripts/build-wasm.ps1` completed after the ABI and final native retention
  changes; and
- `git diff --check` passed before the implementation commit.

Oliver authorized exactly one superseding 1,800-second watchdog for the final
B1 oracle after the earlier 900-second deadline was shown to be miscalibrated.
The detached run completed with no survivor in 1,092,227.105 ms and wrote
`build/diagnostics/b1-two-t1-oracle-1800.json`. It passed every required fact:

- exact value `230.26738656962243`;
- 57,182 expanded states, 738,139 state-action rows, and 1,165,840 transitions;
- transition hash `ad4fc4865f2872e9` and policy hash `f797e61b00a127a7`;
- compiled graph 6,391 nodes / 9,607 edges and 2,668,693 strategy bytes; and
- constructive witness 3 syntheses / 323 reuses / 0 refreshes.

This is the re-pinned `60500ef`-lineage structural baseline. The tracked
`fixtures/solver-scaling/v1/evidence/q5-two-t1-product.json` remains the
historical `7b11b34`-era timing/evaluation report and must not be overwritten;
it is the comparison source below. Full exact-evaluation and 10,000-run evidence
is deferred to the single B6 acceptance cadence.

The retained witness refresh rule is: synthesize when absent; reuse while goal,
economy, the complete action prefix present at synthesis, referenced
rows/operators, executable dependencies, and properness remain valid.
Monotonic graph and lazy vocabulary extension do not invalidate the witness.
Refresh on an existing dependency change or validation failure, not on a mere
focused round, graph/vocabulary extension, or lower-bound update. Direct or
partial executable upper policies may still replace it atomically when cheaper.
Concrete policy references and Unveil preferences are materialized once from
captured same-round source data only when a bounded incumbent is returned.

## Open performance note and future profiling chunk

The new oracle is still roughly twenty times slower than the approximately
51-second `7b11b34` solve: `phase_wall_ms.solve` is 1,083.087 s versus 50.712 s,
a measured 21.36x ratio. This confirms a second, non-constructive per-round
regression source. Do not investigate or fix it in B2-B6 unless Oliver selects
the profiling chunk separately.

The comparison below is field-for-field in milliseconds. `—` means the timer
was absent or deliberately skipped, not zero. The old report's 50.712-second
solver phase is the like-for-like baseline; its 96.166-second report total also
contains 31.354 seconds of exact strategy evaluation, 6.882 seconds of
verification, and other report overhead. The new B1 run deliberately skipped
those post-solve gates under the intermediate testing cadence, so report totals
are not directly comparable even though their arithmetic delta is shown.

| Phase/time field | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `phase_wall_ms.registry_layout` | 17.976 | 17.922 | +0.055 |
| `phase_wall_ms.solve` | 1,083,087.349 | 50,712.495 | +1,032,374.855 |
| `phase_wall_ms.compile` | 788.223 | 1,163.060 | -374.837 |
| `phase_wall_ms.verification` | — | 6,881.602 | skipped |
| `phase_wall_ms.total` | 1,092,227.105 | 96,165.783 | +996,061.323 |
| `exact_strategy_evaluation.wall_ms` | — | 31,353.698 | skipped |
| `focused_expansion.duration_ns` | 841,222.378 | 28,777.684 | +812,444.694 |

`focused_expansion.duration_ns` is the implementation's cumulative focused
lower/upper optimization timer across rounds, despite the generic field name;
it is not a second independent wall clock.

| `solver_telemetry.timings_ns` field | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `registry_generation` | 0.136 | 0.140 | -0.004 |
| `abstract_layout` | 0.079 | 0.082 | -0.004 |
| `solve_setup` | 0.119 | 0.117 | +0.001 |
| `expansion` | 19,837.635 | 21,378.521 | -1,540.886 |
| `transition_calculation` | 7,136.128 | 8,517.387 | -1,381.259 |
| `optimization` | 4,842.530 | 148.657 | +4,693.873 |
| `constructive_policy` | 121,995.350 | — | new phase |
| `strict_clean_goal_cover` | 13,221.741 | — | new phase |
| `extraction` | 7,620.506 | 5,648.039 | +1,972.467 |
| `compilation` | 755.155 | 1,124.907 | -369.752 |
| `verification` | — | — | both null |

| Expansion phase/detail field | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `prepare` | 9,288.257 | 13,241.009 | -3,952.751 |
| `prepare.byte_audit` | 172.693 | 6.589 | +166.104 |
| `prepare.automatic_admission` | 8,733.396 | 13,003.213 | -4,269.817 |
| `prepare.diagnostics` | 249.690 | 205.003 | +44.687 |
| `prepare.operator_pricing` | 118.398 | 18.467 | +99.931 |
| `kernel` | 2,928.156 | 3,711.301 | -783.146 |
| `sparse_row` | 1,802.919 | 2,126.367 | -323.448 |
| `row_byte_audit` | 49.419 | 51.976 | -2.558 |
| `diagnostics` | 31.957 | 57.020 | -25.063 |
| `cache_release` | 116.388 | 84.383 | +32.005 |
| `periodic_cap_byte_audit` | 4,835.254 | 1,617.043 | +3,218.211 |
| `finalize` | 479.168 | 178.835 | +300.333 |
| `finalize_byte_audit` | 240.038 | 85.687 | +154.351 |
| `attributed` | 19,531.517 | 21,067.934 | -1,536.417 |
| `unattributed` | 306.118 | 310.587 | -4.468 |

Remaining non-zero telemetry timer leaves are also compared below. Timer leaves
not listed were exactly zero in both reports: all detail timers for
`fracture_prepare`, `multimod_finish`, `permanent_bench`, `protected_side`, and
`renewal`; the unused detail timers under `imprint`, `primitive_fracture`, and
`temporary_bench`; primitive-family `essence`, `bestiary`, and `other`; and
`bench.row_time_ns`.

| Other telemetry timer | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `automatic_candidates.admission_phases.local_context_ns` | 129.181 | 3,954.730 | -3,825.549 |
| `automatic_candidates.admission_phases.local_context_other_ns` | 88.737 | 1,221.376 | -1,132.639 |
| `automatic_candidates.admission_phases.local_layout_build_ns` | 14.046 | 930.162 | -916.116 |
| `automatic_candidates.admission_phases.local_ledger_init_ns` | 1.207 | 82.687 | -81.480 |
| `automatic_candidates.admission_phases.local_planner_build_ns` | 25.190 | 1,720.506 | -1,695.316 |
| `automatic_candidates.admission_phases.synthesis_ns` | 680.485 | 508.329 | +172.156 |
| `automatic_candidates.by_kind.imprint.time_ns` | 68.440 | 59.173 | +9.267 |
| `automatic_candidates.by_kind.primitive_fracture.row_time_ns` | 0.001 | 269.462 | -269.461 |
| `automatic_candidates.by_kind.temporary_bench.precompile_time_ns` | 2.422 | 2.475 | -0.053 |
| `automatic_candidates.by_kind.temporary_bench.enumeration_time_ns` | 457.271 | 146.425 | +310.846 |
| `automatic_candidates.by_kind.temporary_bench.row_time_ns` | 496.259 | 458.347 | +37.912 |
| `automatic_candidates.by_kind.temporary_bench.time_ns` | 6,763.699 | 6,825.972 | -62.273 |
| `cache.distribution.build_ns` | 7,136.128 | 8,517.387 | -1,381.259 |
| `cache.reforge.build_ns` | 354.748 | 316.299 | +38.449 |
| `cache.primitive_families.currency.time_ns` | 1,707.937 | 1,538.761 | +169.176 |
| `cache.primitive_families.currency.row_time_ns` | 2,532.422 | 2,355.020 | +177.402 |
| `cache.primitive_families.fossil.time_ns` | 649.577 | 761.499 | -111.923 |
| `cache.primitive_families.fossil.row_time_ns` | 1,430.655 | 1,589.652 | -158.997 |
| `cache.primitive_families.harvest.time_ns` | 209.453 | 940.185 | -730.733 |
| `cache.primitive_families.harvest.row_time_ns` | 445.470 | 1,352.565 | -907.095 |
| `cache.primitive_families.bench.time_ns` | 4.410 | 6.018 | -1.608 |
| `cache.primitive_families.fracture.time_ns` | 37.398 | 133.733 | -96.335 |
| `cache.primitive_families.fracture.row_time_ns` | 0.001 | 269.462 | -269.461 |
| `diagnostic_cost.calc_owned_byte_audit_ns` | 4,934.798 | 1,485.834 | +3,448.964 |
| `diagnostic_cost.calc_owned_byte_ledger_ns` | 8,503.782 | 9.087 | +8,494.695 |

The top three phase deltas by direct reported wall contribution, and therefore
the exact future profiling order, are:

1. cumulative focused lower/upper optimization:
   `focused_expansion.duration_ns`, +812,444.694 ms;
2. retained constructive-policy acquisition/validation:
   `timings_ns.constructive_policy`, 121,995.350 ms (the phase did not exist in
   the `7b11b34` code, so this is a new wall contribution rather than a
   same-field regression); and
3. strict clean-goal-cover preparation:
   `timings_ns.strict_clean_goal_cover`, 13,221.741 ms (also new since the old
   report).

The first item is the confirmed non-constructive per-round regression source.
The second and third overlap the same overall solve but are separately timed;
do not add phase totals as though they were independent report wall clocks.
Byte-ledger and audit growth are supporting profiling leads, not additions to
the owner-selected top-three scope. No performance fix was attempted after the
B1 gate passed.

## B2 accepted boundary

B2 compiles exact or bounded native policies solely when `policy_available`.
Calculator now offers optional absolute-chaos and relative-percent targets and
passes only positive values to the engine. Live progress shows lower/upper
bounds, absolute gap, and factor. Final result markup separately identifies:

- exact evaluated returned-policy cost;
- optimal-cost lower bound and certified policy upper bound;
- absolute gap and conservatively rounded multiplicative factor;
- exact, bounded-near-optimal, bounded-feasible, or no-policy quality;
- exact close, target gap, cap (including named cap hits), or no-policy
  termination;
- requested and fired target criteria;
- pinned economy; and
- the complete admitted priced action ID list plus exclusions/skips.

The DOM contract uses the approved “Certified within 1.10x of optimal” and “At
most 10% more expensive than optimal” forms, states that a weak lower bound can
make the certificate pessimistic, and never calls a bounded policy exact or
calls its upper bound the optimum. Bounded policies can open in Strategy Board
and use the existing evaluator/simulator surface when compilation succeeds.
The product verification button now requests 10,000 runs and labels simulation
as corroboration rather than the authoritative evaluation.

B2 evidence:

- `npx tsx test/solver-result-presentation.test.ts` passed all five focused
  DOM/predicate/target cases;
- `npx tsc --noEmit` passed; and
- `git diff --check` passed before commit `86e337f`.

No rendered or visual review was performed, per Oliver's ownership boundary.

## Exact resume point

Start B3 only. Per the active plan:

1. implement seeded generator inputs for base/list/class/pool, item level,
   start item, natural T1 goal count and side composition, natural family/tag
   filters, seed, cases per stratum, caps, and watchdog;
2. add the dedicated engine-owned three-way feasibility query: `feasible`,
   `infeasible` with reason, or `unknown`;
3. use SQLite only to enumerate candidates. Goals must be naturally rollable
   generation-type T1 modifiers on the selected base/item level; crafted,
   bench, essence-only, and other guaranteed-only modifiers cannot be goal
   slots or satisfy natural-only slots;
4. keep legal priced bench operations in the action envelope, while recording
   generation attempts/rejections and full corpus/build provenance; and
5. produce the smoke, full short-budget, and deep pinned corpus strata exactly
   as B3.4 requires.

If any Path of Exile mechanic or feasibility semantic is ambiguous, stop at B3
and ask Oliver; do not research or guess. Do not begin B4 until every B3
acceptance requirement passes.

The standard hard watchdog is 15 minutes for every later run. Use narrow
changed-layer tests during B3-B5, then the complete affected suites and required
10,000-run verification once at B6. At the B3 boundary, commit B3 first, then
update this status/evidence/next-chunk block and commit the HANDOFF separately.

Chunks completed: B1, B2. Exact stopping point: before B3 implementation. Still
awaiting Oliver later are rendered UI review, corpus-strata sanity review, and
acceptance of the B6 results.
