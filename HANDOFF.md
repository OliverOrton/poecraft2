# Session Handoff

**Status: B1 through B6 are complete and accepted. Oliver selected the new
[mechanical solver split](docs/active/mechanical-split.md); baseline capture is
the exact next step and no source motion has started.**

Oliver selected the now-completed
[bounded policy results and benchmarking plan](docs/archive/2026-07-22-bounded-policy-and-benchmarking/plan.md)
on 2026-07-22. The branch is `codex/bounded-policy-contract`, based on clean
`main` commit `60500ef`. Boundary documents were committed as `0c18493`; the
B1 implementation, including retained constructive-witness reuse, is committed
as `58aa5ea`; B2 product presentation is committed as `86e337f`; the B3 native
feasibility/generator implementation is `4c99cd0`, and its pinned corpus is
`d06efff`. B4 orchestration, bound trace, action utility, and search-cost
telemetry are committed as `1d891a0`; B5 stratified reporting and the staged
iteration workflow are committed as `e40b73d`, with accepted B5 handoff
`099d6aa`. B6 final acceptance, the materialized-carrier permanent-bench fix,
stable documentation, evidence summary, and plan archive are committed as
`3d76198`.

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
regression source. It was not investigated during B2-B6. Do not investigate or
fix it unless Oliver selects the profiling chunk separately.

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

## B3 accepted boundary

B3 adds the dedicated native `pc_solver_goal_feasibility` three-way query and
Python binding. A `feasible` result requires an engine-resolved, item-level and
positive-base-weight natural T1 assignment that satisfies rare-item side
capacity and every exclusion group, plus an admitted legal ordinary reforge
witness from the supplied empty normal/rare start. Structural lack of natural
T1 weight, side capacity, or compatible groups returns `infeasible` with a
reason. Unsupported starts or missing admitted witnesses return `unknown`.
The query never uses capped solving as evidence.

The seeded generator supports explicit base paths, base-list files, item
classes, and named base pools; item level and start item; one-to-four natural
T1 goals with exact prefix/suffix composition; family/tag filters; cases per
stratum; seed; solver resource caps; and generator/case watchdogs. Python only
proposes base-reach T1 families for the initial uninfluenced corpus. Native
feasibility remains the acceptance authority. Emitted goals use acquisition-
aware family keys, so bench/crafted members cannot satisfy them; priced bench
actions remain in the goal-relevant envelope.

Pinned corpus evidence in `fixtures/solver-natural-t1/v1`:

- 14 smoke, 120 full short-budget, and 12 deep cases (146 total) across six
  bases/classes: Body Armour, Helmet, Bow, Thrusting One Hand Sword, Amulet,
  and Ring;
- natural goal counts 1-4, varied prefix/suffix mixes, and deep cases with at
  least 100 natural pool modifiers and minimum single-draw goal probability
  no greater than 0.01;
- the smoke corpus includes the bounded three-natural-T1 Dire Pelt with
  `ItemFoundRarityIncreasePrefix3`, `IncreasedLife9`, and `ChaosResist6`;
- all 146 cases are native `feasible/natural_reforge_witness`, all resolved
  goal mods are base-reach, family tier 1, positive weight, and none is a
  bench/crafted goal;
- the generation funnel records 153 attempts, 146 accepts, 2 duplicates, 1
  deliberate ineligible-base probe, 3 deliberate infeasible probes, 1
  deliberate unknown probe, 1 zero-weight family, 2 group/capacity conflicts,
  and 0 exhausted strata; and
- full regeneration was byte-for-byte deterministic.

Generation provenance pins clean engine commit
`4c99cd093ed85b9aa2ecb86c3c9117b0f356068b`, GCC 14.2.0, ABI 2, native binary
SHA-256 `ee8d3d97955e864b1018dd41b1f87891b1b4952aa8fa076c331dbf589539c89d`,
artifact manifest SHA-256
`6b8bbfe77abf87373ec1782fb364500e064eb7539f1d3f0fdb210a807c927875`,
config SHA-256
`6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`,
and raw economy snapshot SHA-256
`9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.
B4 run records must separately pin the solver build so A/B runs consume these
identical explicit case files.

B3 targeted evidence:

- `scripts/build.ps1` completed after the native C ABI and test changes;
- `poecraft_engine_tests.exe --solver-feasibility-only
  data/compiled/current` passed 37 checks with zero failures;
- `py -3 -m unittest tools.ingest.tests.test_natural_t1_corpus -v` passed all
  three tests, including byte determinism and native natural/bench/group
  semantics;
- the configured 146-case dry run and committed regeneration both completed
  under the generator watchdog; the final run took about 11 seconds;
- `scripts/build-wasm.ps1` rebuilt the tracked WASM after the C ABI addition;
- a whole-corpus validation checked unique IDs, schemas, strata counts,
  goal-count consistency, T1/base-reach/positive-weight provenance, native
  feasibility, and all six intended classes; and
- `git diff --check` passed before the B3 commits.

No solver benchmark, exact evaluation, or 10,000-run simulation was executed
in B3; those remain staged for B4-B6 under the selected cadence.

## B4 accepted boundary

B4 adds a native natural-corpus benchmark contract and
`tools/ingest/benchmark_solver_corpus.py`. The runner accepts arbitrary corpus,
artifact, executable, and output paths; orders cases deterministically; runs
each case in an isolated process group; applies the case watchdog under the
default 900-second ceiling; kills the process tree on expiry; checks for a
surviving parent; writes an atomic resumable ledger; skips completed reports;
and supports opt-in worker concurrency constrained by each case's declared
solver-owned-byte cap. Default hard-case concurrency is one. Run provenance
separately records source commit/dirty paths and native executable SHA-256, so
it is independent of the B3 generation-engine pin.

The stepped C/WASM progress surface now reports focused round and incumbent
kind, discovered/expanded/frontier states, sparse rows/transitions/reforge
work, and live/peak selected owned bytes in addition to `L`, `U`, and both
gaps. The native report samples this at every round/incumbent change and
bounded wall intervals, calculates per-cap proximity, flags whether each
sample raised `L` or lowered `U`, and derives time to first incumbent plus
standard absolute/relative gap thresholds.

Exact evaluator action utility is sourced from retained exact state/action
occupancy. Per action it reports exact visits/applies, priced expected spend
and known-cost share, distinct reachable states, and regions stratified by
goal progress, rarity, blocker count, crafted count, fractured goal subset,
and fractured count. Probability of any use is emitted only where the retained
evidence proves it (`exact_zero`); cyclic occupancy is explicitly reported as
insufficient rather than mislabeled as a hitting probability. Simulator action
distributions remain separate empirical corroboration.

Solver telemetry attributes row attempts, raw outcomes, retained transitions,
reforge work, cache requests/hits, row-construction wall time, and retained
selected-allocation growth by action ID. A lower-versus-executable-upper policy
section compares selected abstract-state counts by action. The telemetry marks
all search-cost data observational and records
`non_use_is_pruning_certificate:false`; benchmark action non-use never changes
admission or pruning.

B4 targeted evidence:

- `scripts/build.ps1` completed after native/header/test changes;
- `py -3 -m pytest tools/ingest/tests/test_solver_corpus_runner.py -q`
  passed four watchdog, resume, deterministic-order, and memory-budget tests;
- the historical 12-case benchmark manifest and all 146 generated natural-T1
  cases passed native `--validate-only`;
- the clean committed smoke run at
  `build/reports/b4-boundary-smoke/ledger.json` pins commit
  `1d891a01ae3cfce84fbefb08ba17e3a0e6df9f1f`, a clean tree, and executable
  SHA-256 `50add1869715e45276bcad914fbff60df76f5e9e144a6513fb15134629f56141`;
- that isolated one-natural-T1 case completed in 4,471.660 ms as
  `bounded_near_optimal` / `target_gap`, compiled, completed exact priced
  evaluation, produced 16 bound samples with first incumbent at 820.862 ms,
  reported utility for six policy actions and search cost for 176 considered
  action IDs, and left zero survivors;
- repeating the same run directory skipped the completed case through the
  ledger resume path;
- `npx tsc --noEmit` passed after the additive progress contract; and
- `scripts/build-wasm.ps1` rebuilt the tracked release module, followed by a
  clean `git diff --check` before commit `1d891a0`.

No full native/web suite and no 10,000-run simulation was executed in B4; both
remain reserved for B6.

## B5 accepted boundary

B5 adds pure-Python stratified aggregation and paired comparison over the raw
per-case B4 reports. Reports group by base, class, base/class, natural-T1
count, prefix/suffix side mix, natural-pool density, minimum goal probability,
incumbent kind, policy status, and termination. They report exclusive
exact/near-optimal/feasible/no-policy rates, refusal and target-reach rates,
median/p90/p99 time and selected-owned memory, time to first incumbent and
target, lower/upper progress, work and compiled-graph distributions, exact
policy action utility, observational action search cost, outliers, and paired
deltas/regressions.

Paired comparison accepts a case only when its ID and complete
session/start/goal/caps inputs match. Search-cost output repeats that action
non-use is not a pruning certificate. Analytics remain separate from native
and WASM correctness; they do not change solver admission, pruning, Bellman
comparisons, tie-breaking, epsilon, or gap stopping.

`fixtures/solver-natural-t1/v1/benchmark-stages.json` and
`tools/ingest/benchmark_bounded_policy_stage.py` encode the ordered funnel:
smoke, full short-budget candidate, deep representative/hard cases, explicit
compiled-policy exact evaluation, and the B6-only acceptance-verification
subset. Each non-smoke stage requires a green predecessor ledger under the
same label. The acceptance stage is refused by the B5 command so the required
10,000-run cadence cannot be spent early.

Native benchmark exit code 2 is treated as a completed measurement when the
process ended under its watchdog, left no survivor, and wrote its report. Its
case-level expectation miss remains explicit in the ledger and report so
no-policy and refusal outcomes are not censored. Exit code 1, timeout,
survivor, missing report, runner error, or memory-budget launch refusal remains
an incomplete run.

B5 targeted evidence:

- the native benchmark rebuilt successfully after adding generated corpus,
  feasibility, and generation dimensions to each raw case input;
- `py -3 -m pytest tools/ingest/tests/test_solver_reports.py
  tools/ingest/tests/test_solver_corpus_runner.py -q` passed all 10 focused
  analytics, paired-input, stage-gate, watchdog, resume, native-exit, ordering,
  and memory-budget tests;
- the clean committed two-case smoke at
  `build/reports/bounded-policy/b5-boundary/smoke/ledger.json` pins source
  commit `e40b73db00716edf7ad3ed53ec36eec51678cc49`, a clean tree, and executable
  SHA-256 `e09c2f0d5bef91430773bba949a0645963b3d8f81124a72511dee5f2937f46e3`;
- its one-natural-T1 case completed as `bounded_near_optimal` / `target_gap`
  in 4,721.542 ms, while the three-natural-T1 Dire Pelt completed as an honest
  `refused_state_cap` / no-policy measurement in 506.763 ms; the aggregate
  therefore retained 50% near-optimal, no-policy, refusal, and target-reach
  rates and recorded zero survivors; and
- a paired self-comparison covered both identical-input cases with zero
  exclusions and zero regressions.

No full-short or deep corpus, exact-evaluation stage, complete native/web
suite, or 10,000-run simulation was executed in B5. Those runs remain staged
for owner-selected iteration and the single B6 acceptance cadence.

## B6 accepted boundary

B6 completed the final downstream acceptance and documentation gate. During
the pass, the historical WASM smoke fixture was corrected to request its
crafted bench family instead of letting a broad natural group stand in for a
bench goal, and both stale ABI assertions were updated to ABI v2. That exposed
a real state-local admission defect: permanent-bench discovery could reject a
legal finish when a temporary abstract context could not rematerialize the
already-existing dense exact carrier. Permanent deterministic benches now
evaluate directly on that materialized carrier, intern their exact successor
in the owning context, and cache the exact distribution. This restores legal
output behavior without changing mechanic authority, admission criteria,
Bellman comparisons, search guidance, or pruning.

The single final B1 two-natural-T1 oracle was not rerun. Oliver's exactly-once
directive and the standing 15-minute ceiling make the retained report the B6
oracle evidence. B6 revalidated report SHA-256
`9501b055aa1808bc17aa3213f001daf134c2bfc2f621ab2867c90d81a0c061da`
and all acceptance facts: exact value `230.26738656962243`, 57,182 expanded
states, 738,139 rows, 1,165,840 transitions, `exact` / `exact_closed`, graph
6,391 nodes / 9,607 edges / 2,668,693 bytes, and constructive-witness counters
3 syntheses / 323 reuses / 0 refreshes. Its measured wall time remains
1,092,227.105 ms; the open comparison and owner-selected profiling order are
recorded above.

The explicit bounded acceptance case was
`natural-t1-smoke-one-armour-09767fd6f824`. It completed under the detached
900-second watchdog as `bounded_near_optimal` / `target_gap`, with target met,
`L = 3.0923164022485841`, `U = 8.0902955687919231`, absolute gap
`4.9979791665433391`, and relative gap `1.6162573671015839`. Its compiled graph
had 947 nodes / 2,831 edges / 1,154,938 bytes. Exact compiled-policy evaluation
converged to `8.0902955687899443`, differing from certified `U` by
`-1.9788e-12`. The required simulator run then completed exactly 10,000 trials:
10,000 successes, zero failures, zero off-policy failures, mean cost
`8.0376181789999475`, and passed corroboration. This is a bounded certificate,
not an exact-optimality claim.

B6 gate evidence:

- the post-fix native build completed with exit 0 in 218,771.894 ms under the
  detached watchdog;
- the complete native suite covered capped `bounded_feasible` / resource-cap
  termination, early `bounded_near_optimal` / target-gap termination, and
  native-versus-compiled exact-value parity as part of 513,211 checks with zero
  failures;
- the final `scripts/test.ps1` cross-layer pass completed with exit 0 and no
  watchdog survivor in 46,196.463 ms, validated all 12 historical solver
  specifications, and passed ingest, binding, native, and complete web/WASM
  tests;
- `test_natural_t1_corpus.py`, `test_solver_corpus_runner.py`, and
  `test_solver_reports.py` passed all 13 determinism, feasibility,
  watchdog/resume/survivor, aggregation, and paired-comparison tests;
- `npx tsc --noEmit` passed;
- `scripts/build-wasm.ps1` completed with exit 0 in 74,349.823 ms after the
  engine fix, producing tracked release `.wasm` SHA-256
  `b70d3cf05c0ae9a2a2429bffbde9c279b24b74cad3d670a48090ec6a5b5b77c6`;
  and
- the checked-in
  [B6 acceptance summary](fixtures/solver-natural-t1/v1/evidence/b6-acceptance-summary.json)
  validates as JSON, `git diff --check` passed, and the documentation
  relative-link/reachability audit passed after the plan archive move.

Initial acceptance attempts exposed the two stale web assumptions and the
materialized-carrier defect above; those were debugged before the final green
pipeline. No performance profiling or mechanic change was attempted. No
rendered or screenshot review was performed, per Oliver's ownership boundary.

## Exact stopping point

Chunks completed with evidence: B1, B2, B3, B4, B5, and B6. Oliver accepted
the completed B6 result and selected a new post-B6 internal restructuring
boundary. Its source parent is clean commit
`edb7d8fc10eee2a8c34d86b08c5a471243bc6c8a`.

Exact stopping point: before the mechanical-split baseline build, native fast
gates, body-token inventory, or benchmark repetitions. Start with Gate 0 of
the active plan. Do not move source until every baseline artifact is complete
and internally deterministic.

The selected chunk splits `engine/src/solver_solve.cpp` only. It adds no tests
or behavior. Calculator, `solver_internal.hpp`, and web-test splits are
deferred. The exact oracle must not run, and the future profiling order above
must not be investigated or tuned. Every process retains the 15-minute ceiling.

At selection time, CMake was not available; GCC, Node, npm, and the Python
launcher were available. Re-check rather than assume. If CMake remains absent,
update its explicit source list by inspection and record that the CMake path
was not build-tested locally.

There are no blockers at selection. The next session must capture fresh
current-tree counts, hashes, and five-run medians exactly as specified in the
active plan; historical B1 timings are context only and cannot substitute.

## Economy pipeline state (2026-07-22, outside chunk scope)

Known product gap: the Calculator cannot price `harvest_reforge:defences`.
The engine's action id is plural (from `fixtures/economy/harvest-recipes-v1.json`
and the game's `defences` tag), but every published snapshot carries the stale
singular key `harvest_reforge:defence`, seeded before the S7.2R fixture
correction and never re-seeded. `defences` is the only harvest tag whose
spelling differs between the two vocabularies, and no pinned case admits the
defence reforge, so no test observes this gap in either direction. A
from-scratch recipe seed was verified to produce the correct plural
`derived_recipe` keys with zero code changes.

Incident: on 2026-07-22, during diagnosis, `poecraft-economy ingest-fixture
--force` rebuilt the untracked local `data/economy/poecraft-economy.db` from
frozen test fixtures. The 2026-07-15 live ingest history and quote rows were
lost from the database. The content-addressed live raw payloads in
`data/economy/raw/poe-ninja/` survive, and every committed snapshot artifact
and `apps/web/public/economy/league-index.json` is unaffected.

Do not run `poecraft-economy publish` in this state: the database's compiled
snapshots (including mirage `c33c84de…`) are derived from test-fixture prices,
not market data, and publishing them would put fake prices in the product
league index. Publish only after the database is rebuilt from live data — a
fresh `refresh`, or a reconstruction of the July 15 fixture from the surviving
raws — under a separate owner-selected step. That step changes tracked files
(a new snapshot JSON plus the league index) and must run between chunks, not
during one.
