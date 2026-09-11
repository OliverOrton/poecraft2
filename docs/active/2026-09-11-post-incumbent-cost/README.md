# Post-Incumbent Cost Improvement and Native Headroom v1

Selected by Oliver after reviewed `edd8ec25f40b11c8ba4f4aaf157db9405b593ed2`.
The original [implementation plan](imported/implementation_plan.md) and
[review and argument](imported/review_and_knowledge.md) are retained once without
rewriting their historical observations. Work is sequential, without subagents,
inherited deadline, automatic restart or push. Protected path `0` is untouched.

## Outcome and controls

The two-primary material cost objective is **unmet**. The native Ring capacity
arm is useful, but neither solver treatment lowered either matched primary
cost; both were removed. The retained code corrects compiler descriptions
without assigning optimality from a preliminary status. All current core and
exposed controller costs are preserved after restoring solver logic.

M0 and first-policy recovery are complete. The reviewed executable is preserved
as `out/post-incumbent-cost/baseline-edd8ec2.exe`, SHA-256
`d25d8a859ac7298af074ddd4aa2be20090f5091d18deb9a572a1448892b9a912`.
Its source checkpoint was clean and local `origin/main` equals the reviewed
commit. Fresh primary controls use the current typed corpus runner, original
cases, retention `reuse`, exact independent evaluation and 240/300/315-second
finish/native/host limits. There is no new baseline census.

Existing exact evaluator accounting attributes 98.90474248027828% of Ring cost
to Harvest Augment Defences (575,816.2966875376 of 582,192.807187538). Amulet
Annul costs 32,535,336.708039645 of 40,215,428.995558396 (80.90262250250524%),
mostly in the zero-goal region. Existing post-incumbent upper passes already
run; their selected entry domain, completed alternatives and missing tails are
the investigation. Node `expected_cost` comes from a uniform finite source
`result.values` annotation over a compiler region, not independent entry
evaluation. No attribution uses that annotation.

The separately identified [Ring capacity case](memory4g-manifest.json) changes
only case identity and solver-owned memory from 1 GiB to 4 GiB. All other
native/evaluator limits and scope remain identical. Initial host observation:
66,806,096 KiB visible physical memory, 43,122,548 KiB free. The current corpus
reservation equals the solver cap; safe serial admission additionally leaves
room for the unchanged 1 GiB evaluator, serialization, native data and OS.
The unchanged executable returns verified U=204,763.14825000268 at 4 GiB,
64.83% below the fresh original-profile U=582,192.807187538. Actual solver
ownership peaks at 1,118,120,640 bytes; the next limiting owner is the unchanged
50-million reforge-work cap. This is a capacity gain. The current observed
one-GiB refusal is 456,471,865 owned + 790,419,257 requested scratch bytes in
`solver_reforge.cpp`'s family-override preparation, not the older N2 allocation.
The [capacity-baseline controller](experiments/strategies/cb06-capacity-baseline.strategy.json)
is preserved byte-for-byte with its evaluation identity. Its historical
description is unchanged; it is a bounded controller, not an optimality claim.

Amulet exhausted 50-million reforge work while selected missing continuations
remained and time was available. Its conditional 100-million arm recorded
12,536 refinement-service selections and 27 proper upper passes, but returns the same
verified U=40,215,428.995558396. Neither arm changed browser limits. Native
runs were serial; Ring used an explicit 8-GiB host reservation budget and
admission required at least 12 GiB available physical memory.

The [implementation profile](implementation-profile.json) was frozen before
treatment: Ring 4 GiB/50-million work, target U<=163,810.51860000216; Amulet
1 GiB/100-million work, target U<=32,172,343.196446717. Other controls remain
identical. Original 1-GiB/50-million preservation checks remain required.

### Treatment disposition

Temporary native observations reproduced both baseline costs and were removed
after preserving their [source patch](experiments/m1-native-observation.patch)
and executable identity. At the first incumbent, completed root Chaos is
selected by joint construction but exits the incumbent's verified entry domain;
its first missing positive-mass state is 32 on both primaries. The ordinary
upper pass already admits completed alternatives and correctly leaves these
Q values unknown. Both retained candidates later report `resource_interrupted`.
This does not establish a cheaper policy or blame the first-policy preference.

The [compact cost and continuation receipt](m1-cost-and-continuation.json)
binds evaluator spend, graphs, semantic entry keys and observed row snapshots.
These rows are requested, legal, scheduled and kernel-complete; their remaining
distinctions are continuation coverage and actual candidate delivery:

| Snapshot / entry | Alternative | Incumbent-domain Q | Missing direct rows | Disposition |
|---|---|---:|---:|---|
| Ring first upper / root | Chaos | unknown | 2,478 | Joint seed; first missing entry 32 |
| Ring first upper / root | Exalt | unknown | 1 | 0.006103143118706134 mass outside the domain |
| Ring pass 8 / one unwanted prefix (2510) | Annul incumbent vs Chaos seed | selected incumbent finite | — | Construction replaces a known cleanup decision |
| Amulet pass 8 / root | Exalt | 40,215,429.49569152 | 0 | Loses to source V=40,215,429.00866072 on this old policy |
| Amulet pass 8 / root | Chaos | unknown | 3,361 | Kernel complete; tails remain uncovered |

Source Q and V in this table are masked policy-pass calculations, not new
independently verified entry certificates. Cost shares above belong to the
separately evaluated returned graphs. Snapshot row samples are bounded; they
are not an all-family census.

Rejected treatment T1 deduplicated pending states in the existing continuation and joint
walk. Shared reforge successors were enqueued once per parent until their first
processing, retaining many copies of each full semantic node. The unchanged
Ring walk reached 996,328 entries while the final graph had 1,084 expanded
states. Marking discovery on enqueue preserves the same unique states and
observations; candidate properness, fixed-policy evaluation and independent
publication still decide whether it is usable. Fresh joint-walk scratch includes
the additional queued bitmap in its byte accounting.

The discriminating dense-shared-tail native fixture fails three checks under
the old owner and passes all 190 continuation checks after T1. Matched selected
profile runs preserve both costs exactly, so T1 misses the cost objective.
Ring's native-owned peak falls by 62,902,790 bytes (1,118,120,640 to
1,055,217,850); Amulet completes more upper passes but its peak rises to
297,284,841 bytes. Its policy still costs 40,215,428.995558396. These are memory
and work observations, not a cost-improvement claim.

T2 tested keeping compatible incumbent row decisions at non-root entries of the existing
joint proposal. At uncovered entries it prefers a completed native row only
when every positive direct exit and every offered choice reaches the actual
goal or strictly reduces affix count at the same rarity. Missing tails still
go to ordinary refinement. The root proposal, no-incumbent behavior, prices,
probabilities and full properness/evaluation obligations are unchanged. This
removal preference is construction guidance, never a terminal or pruning proof.
The native fixture fails two checks before T2 and passes 194 checks afterward.
Returning to the root repeats the proposed root action: this is a permanent
controller candidate, not a one-time deviation calculation.

Both matched T2 runs preserve the same evaluated costs. Ring reaches its work
cap with the candidate still waiting for one continuation; Amulet reaches
requested finish with 7,421 expanded states, 64 proper upper passes and an
uncovered native entry 4233 (goal mask 1, zero owned rows). More completed work
does not meet the cost objective. T2 and its fixture mutation were removed;
its registered patch and results remain evidence. No third ordering variation
is selected. Both 20% targets
are unmet; no new exact closure was obtained on either primary.

The service counter records selection for ordinary refinement, not proof that
every selected row completed before a later cap. Pending entries remain unknown
and neither treatment publishes them as tails. The last verified controller
survives both failed improvement mechanisms.

Future compiled descriptions retain scope and state that compilation does not
establish policy optimality. The final result owns optimality status; old stored
graphs are untouched. The first description fix still trusted the source
`Exact` status, but CB02's graph was compiled before its final bounded
classification. That native counterexample motivated status-neutral wording.
The [classification receipt](experiments/metadata-provisional-classification.json)
preserves the conflicting source description and final status.

## Qualification

Runs continued through the actual returned, independently evaluated controller
and ordinary improvement. Uncovered positive-mass tails remained unknown;
failed improvement attempts did not replace the verified artifact. Restoring
the reviewed solver logic preserves the existing no-incumbent scheduler guard,
first-policy certification and finish. CB09 no-policy and CB05 named-stop
failure remain visible. Wand/Shield are exposed regression cases.

Matched primary results are complete. T1 passed 662 selected native checks
(continuation, compiler metadata, selected fallback and bounded finish), but
failed [original-profile cost preservation](experiments/t1-core-regression.json):
CB01 Conquest-five returned
470,485,195.5626143 instead of 85,558.70618560436. Both graphs independently
evaluate with success probability one, zero off-policy mass and zero cost
mismatch. L remains 405.3694021063399. T1 made zero successful joint candidates
versus one in the control, and reached requested finish waiting for a
continuation; more rows and proper upper passes did not preserve root quality.
The mutation also affected first-policy construction, so its memory benefit
does not justify retaining it. T1 and its fixture were removed too.

The T1 core run stopped after this result. The running CB02 process and its
runner were terminated through verified process identities; both are confirmed
absent. Partial CB02 evidence is not a qualification result. Original executable,
patch and report identities remain intact under their experiment paths.

The final source retains only compiler descriptions and their focused tests,
with solver construction and continuation restored to the reviewed baseline.
The restored solver build passed 88 initial compiler-description checks,
TypeScript and the focused transfer/presentation checks. The final neutral
wording passes 104 native checks, fresh native Ring/exact Regalia publication,
and all three actual WASM modes. Capacity findings and both rejected treatment
results remain part of this record.

## Original-profile preservation results

The restored-solver core/exposed cohorts match all 14 baseline cases under the
existing strict comparison owner: no exclusions, changed finite costs, changed
policy classes or performance flags. Thirteen returned graphs independently
evaluate; their complete JSON content is identical to the controls after
excluding only `description`. CB09 has no graph in either run. All run-owned
processes exited with zero survivors.

These timed cohorts use native build `9c32922f04b043503459db78868829e45198618a30999cc68c74233f3656a9d9`,
with restored solver logic and the initial description correction. The later
status-neutral wording changes only compiler metadata; focused final native
and WASM publication checks qualify that final text separately.

All values below are full precision from the reports. L and finite U match
the current control exactly; costs use the frozen Allflame/base-one economy.

| Case | L, both builds | Verified U, both builds | Final policy class | Stop |
|---|---:|---:|---|---|
| CB01 Conquest five, empty | 405.3694021063399 | 85558.70618560436 | bounded_feasible | requested_bounded_finish |
| CB02 Conquest four, empty | 198.8334996747695 | 3746.1319409485764 | bounded_feasible | requested_bounded_finish |
| CB03 Conquest three-to-five, partial | 405.11164995948883 | 794067.4530398862 | bounded_feasible | requested_bounded_finish |
| CB04 Spine Bow four | 341.0345603014526 | 223349.0000393144 | bounded_feasible | requested_bounded_finish |
| CB05 Spine Bow five | 297.7999999702199 | 87200457.42804676 | bounded_feasible | other_resource_cap |
| CB06 Amethyst Ring two | 10.169847833384168 | 582192.807187538 | bounded_feasible | memory_cap |
| CB07 Amethyst Ring four | 201.85524142944053 | 3725358919.4984236 | bounded_feasible | memory_cap |
| CB08 Onyx Amulet three | 174.79327321907147 | 40215428.995558396 | bounded_feasible | reforge_work_cap |
| CB09 Amethyst Ring three | 172.4952745977795 | none | none | memory_cap |
| CB10 Jewelled Foil | 329.53999996704596 | 45635.96043201217 | bounded_feasible | memory_cap |
| CB11 Dire Pelt | 40.20377012689141 | 1934.63683986346 | bounded_feasible | memory_cap |
| CB12 Vaal Regalia | 65.60036144971359 | 65.60036144971359 | exact | exact_closed |
| HO01 Exposed Shield | 93.66114293680295 | 2071.736538384866 | bounded_feasible | requested_bounded_finish |
| HO02 Exposed Wand | 110.28721258971052 | 3963.671015833939 | bounded_feasible | state_cap |

CB05 still fails its named-stop contract despite a valid evaluated controller.
CB09 still fails policy availability. This is 11/12 core graphs and 10/12 core
expectations, plus 2/2 exposed graphs and expectations. The partial Conquest
result remains above the preserved historical 80,720.78955245294 reference;
that historical controller was not used as a seed. No new exact closure is
claimed; the Regalia anchor is preserved.

Wand's named stop changed from `memory_cap` (mask 4) to `state_cap`
(mask 5, state and memory). Its graph, bounds, policy class and expectation
result are unchanged. The observed stop difference is retained; it is not
attributed to an algorithmic speedup or treated as identical stopping evidence.

## Final publication and receipts

The final native executable is `d251c303e6f3e3fba746a387f35d274b6b7be15ca1bd7aef4b18b7a740374401`;
the final WASM is `fb48b3ae34ec9ceb194e726796ed22ac6f356bd61b4a89caa2ebcaf6b6d78e28`.
Fresh Ring and Regalia reports preserve their evaluated costs and native
bounded/exact classifications, and emit the neutral description. The final
metadata fixture passes 104 checks across preliminary statuses and all eight
scope combinations. TypeScript and two focused web test files pass.

| WASM mode | Returned verified U | Independent evaluation | Live handles after cleanup |
|---|---:|---|---:|
| normal | 582192.807187538 | matched | 0 |
| finish | 938064.6067502735 | matched | 0 |
| abandon | not returned | abandon after verified incumbent | 0 |

Finish is requested only after native telemetry shows the first verified
incumbent, and retains its full cost of 938,064.6067502735. Normal delivery
continues to the original Ring cost of 582,192.807187538. Both emitted graphs
have complete independent cost accounting and zero off-policy mass. Abandon
is observed after verification too. All three isolated processes exit with
no survivors. Process wall, observation times, memory and cleanup are retained
in the qualification receipt; zero handles is not an empty WASM heap.

The first normal adapter attempt passed JSON bytes directly to `JSON.parse`
and failed before its extra graph evaluation. The facade return type identified
that conversion error. Decoding UTF-8 fixes the adapter; all three modes then
pass with unchanged native/WASM binaries. The failed receipt remains visible.

The [full result projection](results.json) is generated through the existing
run/report owners and retains per-case identities, full-precision bounds,
cost attribution, observation times, wall time, owned memory and strict pairs.
[Qualification](qualification.json) binds final checks and cleanup;
[build identity](build-identity.json) binds the final source patch and binaries;
[import identity](import-identity.json) verifies all nine supplied files
byte-for-byte against the original ZIP. The full statistical report remains
at `out/post-incumbent-cost/matched-report.json`. Rejected T1 Amulet peak-memory
and T2 Amulet wall-time flags remain visible in those comparisons.

No Simulator, full acceptance suite, full web suite, soak or rendered UI review
was run. The native capacity graph is not claimed as separately qualified
browser delivery. Browser solver limits remain unchanged. The temporary
adapter was removed after its final patch was preserved.

The remaining capability bottleneck is completing compatible cleanup/tails
for competitive reforge controllers within the selected work/time
limits. Extra rows, storage savings and old-policy visitation alone do not
supply that root cost improvement. Both speculative mechanisms are stopped.
