# Execution-aware proposals and bottleneck-entry recovery v1

Implemented against reviewed main `d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d`.
The actual B/E/T matrix yields a small Ring-four original-cost improvement,
two substantially shorter but costlier Bow controllers, and the preserved
Amulet result. **Neither primary reaches the 25% cost target or the 50% count
target without a cost increase. No original lower or exact closure improves.**
The supplied argument is preserved once in [research-inputs](research-inputs/README.md).
The [qualification receipt](qualification.json) binds builds, controls and limitations.

## Matched original-price results

C is full expected original-price cost; N is expected primitive executions.
Every returned policy has complete native outcomes and a matched independent
evaluation. These are bounded results, not optimality certificates.

| Case | B C / N | E C / N | T C / N |
|---|---:|---:|---:|
| Bow-four | 223,349.000039 / 1,404,492.006968 | unchanged | unchanged |
| Ring-four | 227,989.251597 / 585,368.325510 | unchanged | **227,377.065450 / 583,110.349960** |
| Amulet | 12,541.579649 / 25,518.491313 | unchanged | unchanged |

B is the frozen reviewed executable. E adds entry coverage with cost-only
guidance. T uses the same complete row domains, with cost-only, quarter-scale
count, full-scale count and cost-only follow-through views. The two positive
weights were frozen as 0.25 C/N and C/N from the reference policies before
treatment results. They are proposal reward units, not currency-price edits.
[Frozen references](frozen-references.json) preserve those weights and targets.
The existing comparison owner [pairs all six B/E and E/T comparisons](comparison.json)
with zero exclusions. Bow and Amulet winners retain identical strategy hashes.

The new [Ring strategy](strategies/ring-four-count.strategy.json) is
0.268515% cheaper and uses 0.385736% fewer actions. It comes from the first
complete native option patch at lambda 0.09736999836758721. Its original-price
cost-only follow-through completes but returns the more expensive E policy;
the portfolio retains the cheaper count-discovered graph. Full lambda produces
a costlier intermediate controller, not a replacement winner. See
[the improvement receipt](ring-improvement.json).

Ring still spends 196,464.685914 on 573,117.520168 EssenceSuffering4 executions,
followed by Exalt cost 15,285.936637 and Annul cost 13,048.353975. Repeated
acquisition remains its dominant bottleneck. The repaired Amulet winner and
its prior execution evidence remain intact.

## Bow's actual legal boundary and delivered trade-offs

The historical `s12` is a **primitive Alteration followed by the global router**.
That label is only a locator. The old selector requested Exalt/Annul/Scour and
required Rare entries with mutable progress. The new selector requests
compiler-bound primitive Alteration/Augment decisions and checks independently
evaluated physical entries: global routability, no offer or checkpoint, exactly
one satisfying fracture, no fractured junk or other flags, and no mutable goal
progress. Mandatory option interiors do not qualify. No guard was removed to
manufacture an opportunity.

The actual audit admits 16 Alteration entries with summed immediate spend
138,607.176346, and one Augment entry with spend 30,397.270612. Selection uses
the largest **single physical entry** occupancy times original operation price,
so it chooses Augment `s2`, one prefix/no suffix, fracture mask 4, with
460,564.706243 expected visits. These node/mask values are evidence, not
hardcoded selectors. Alteration remains eligible; its occupancy is distributed
over 16 entries. Continuation tails are not added to immediate spend.

No admitted Essence guarantees a satisfying mutable goal for this Bow entry.
This does not establish that every Essence is illegal or ineffective. The
implemented alternative is a closed, paid Regal/Chaos controller with native
Exalt/Annul/redraw recovery. It retains the fracture and recovers lost mutable
goals itself. The full original root graph retains paid setup; there is no
free rarity cast, free reset or borrowed root-upper tail.

| Complete diagnostic controller | C | N | Change against retained Bow |
|---|---:|---:|---|
| [Cost-only entry recovery](strategies/T-bow-export-final-cb04-tradeoff-ad3322613db4.strategy.json) | 405,589.692463 | 331,010.580055 | about 81.59% costlier, 76.43% fewer actions |
| [Count-aware entry recovery](strategies/T-bow-export-final-cb04-tradeoff-4f729a5d8852.strategy.json) | 405,922.951785 | 327,477.870157 | about 81.74% costlier, 76.68% fewer actions |

Quarter-scale count guidance leaves the cost-only decisions unchanged.
Full-scale lambda 0.15902475694498588 changes two selected rows. The bounded
cost-only improvement pass returns the first diagnostic. Neither replaces
Bow's verified C 223,349.000039 policy. The diagnostic bottleneck moves to
Chaos: C/N 310,908.696428 in the cost-only graph and 306,763.182945 in the
count-aware graph. The latter pays more Annul/Exalt cost. The
[fresh economic projection](delivered-economics.json) retains native action
and region identity; [baseline economics](baseline-economics.json) preserve
the previous controllers.

This Magic family constructs 225 complete rows and uses 88,840 child work.
Its private C/N estimates differ from the full physical root result; only the
latter authorizes retention. It does not yet grow new protection/blocker options
inside this fractured-Magic family. Existing broad-root option growth and the
completed blocker repair are preserved. Nonmatching Essences, other entry
choices and further recovery families are not exhausted by this bounded v1.

## Native implementation and authority

The existing native row owner now carries original cost and primitive execution
reward separately. Primitive rows charge one execution; option rows use
`OptionKernel::expected_primitive_actions` with the same first-exit law as
their resource and successor outcomes. Macro invocations, graph steps and
currency quantities are not substituted for primitive executions.

Reward views share complete immutable transitions. Numerical preparation is
reset after reward or policy changes. Cost and count use the existing fixed
policy equations separately; count values have no original-cost lower,
permanent-prune, probability or executable-tail authority. Count weighting is
confined to closed domains with no unknown boundary count. Each changed graph
still passes the independent original-price native evaluator, including paid
setup/cleanup, all exits, observed choices and lost-goal recovery. Identical
selected-row reuse retains only compatible receipts; an absent count is null,
never a previous controller's count.

A bounded publication hook services the first newly verified compiler-bound
fractured controller, because the initial optional lane precedes Bow's expensive
controller. Existing verified bindings are preserved within the memory budget;
root-only artifacts receive no fabricated entry binding. A bounded-finish latch
abandons incomplete proposals but still publishes the cheapest completed policy.

The first real v6 Bow composition correctly refused a policy-scope mismatch:
the private builder had dropped native progress gating. v7 preserves the caller's
native gated law for the retained fracture and redraw rows. Its zero-progress
mass is zero because a satisfying fracture survives; native terminal aggregation
and scope still apply. A physical/gated mass-equivalence fixture checks that
argument. The scope comparison remains intact. Raw refused v6 evidence is kept.

## Capacity, identity and qualification

The [matched corpus](native-600-final840-400m/manifest.json) uses
600/840/870-second solve/final/watchdog limits, 8-GiB aggregate memory,
4-GiB checkers and 400M shared work. Only Bow's work allowance differs from its
preceding 200M corpus; that is an explicit capacity change. Fresh admission
reserves 14 GiB with existing physical/commit headroom. Cases run serially under
the existing runner and native cancellation/watchdog owners. Browser defaults
and historical high-water evidence remain unchanged.

[B's build receipt](build-B.json) reconciles its native sources with d2.
The initial B batch overlapped compilation: Ring stopped before its productive
Essence checker and returned C 223,450,050.408824. That censored result remains
on disk. An isolated B Ring repeat reproduces the historical 227,989.251597
reference used above. No latency speedup is inferred from this programme.

The main matrix uses [v7](build-ET-v7.json). Final [v8](build-ET-v8.json) changes
only bounded diagnostic artifact retention and related diagnostic fields.
The two Bow graphs are about 4.55 MB each, exceeding the initial unrelated
4-MiB per-artifact cutoff. v8 uses the existing shared quarter-budget allowance.
A separate [export profile](native-600-final840-400m-tradeoff-export-final/manifest.json)
raises only telemetry capacity to 640 MiB, without preallocation; actual retained
graphs total about 9.1 MB. Solver, checker, work and time limits are unchanged.
The earlier 64-MiB export and main evidence remain immutable. The final v8
export reproduces the same Bow C/N and retained winner. It is not relabeled
as a new main matrix. Fresh final-DLL checks re-evaluate all three delivered
graphs; the six original-profile controls qualify final v8 separately.

Each delivered graph completed 1,000 native trials at seed 20260826, with the
original 100,000-action and 4,000,000-graph-step limits:

| Graph | Successes | Action-limit stops | Other routing/application/price failures |
|---|---:|---:|---:|
| New Ring | 141 | 859 | 0 |
| Bow cost-only diagnostic | 252 | 748 | 0 |
| Bow count-aware diagnostic | 253 | 747 | 0 |

These heavily censored samples are execution evidence, not uncensored means,
quantiles or cost/properness authority. Independent full evaluations establish
complete original-price cost, goal mass and zero other mass. Unchanged Bow and
Amulet strategies were not resimulated. Their earlier evidence remains valid.

Focused checks pass: final v8 protected/setup/selection/abandon 497; v7 return
composition 523; native continuation API 35; blocker entry 36; bounded finish
14; earlier compile selector 1,214. The last includes pre-existing fixture
sampling and is distinct from the delivered 1,000-trial checks. Python corpus
and identity tests, docs metadata/lint and the generated view are recorded in
the final receipt. The rebuilt WASM is checked through retention/finish/abandon,
Conquest-five and exact Regalia controls. No web source or public ABI changed.

[Six original controls](original-preservation.json) pair without exclusions.
They reproduce current costs; CB05 retains its named-stop contract failure,
CB09 retains no policy, and CB03's better historical C 536,407.1454 remains
preserved despite current C 794,067.453040. The full native suite, fourteen-case
cohort and rendered UI review were not repeated. Late weighted Chaos checks
on Ring/Amulet hit remaining shared work caps, not infeasibility certificates;
their very large private estimates do not justify another cap campaign here.

## Documentation disposition

The requested audit is integrated into the existing mathematical owners:
[policies](../../solver/mathematics/policies.md#primitive-execution-reward),
[search](../../solver/mathematics/search-and-resumption.md#count-aware-proposals),
[lower bounds](../../solver/mathematics/lower-bounds.md), and
[numerical closure](../../solver/mathematics/numerical-closure.md).
They cover separate reward equations, first-exit normalization, occupancy,
mean-versus-tail limits, scalarization limits, residual/count amplification and
the conditions needed to transfer a count bound to a cost bound. The series
input appends the completed blocker repair, checked-potential negative and this
bounded execution result; the generated observations now include them.
Earlier static candidate coverage receives the economic credit. Adaptive
rescaling and checked potentials retain their negative/limited findings.
No unchanged lower-model or three-family adaptive campaign was repeated.
