# Gate 6 Ring/Amulet evaluator recovery

**Measured:** 2026-08-09 through 2026-08-12 on
`codex/solver-goal-realignment`. This evidence covers the Ring/Amulet
publication recovery, the complete every-base matrix, and the two focused
current-product arbitrary-start controls. It does not claim final Gate 8
acceptance.

Parent: [active plan](../plan.md)

## Result

Both focused reliability cases now publish independently evaluated bounded
policies without changing the corpus caps:

| Case | Frozen run | Result | Published graph | Independently evaluated cost |
| --- | --- | --- | ---: | ---: |
| `reliability-class-ring` | `focused-v2` | state-cap stop with `bounded_feasible` policy | 3 nodes / 3 edges / 1,065 bytes | `490.41233174994977` |
| `reliability-class-amulet` | `focused-v3` | state-cap stop with `bounded_feasible` policy | 3 nodes / 3 edges / 1,068 bytes | `144.88045459605041` |

Both publications are `bounded_core_policy` with candidate kind
`direct_compiled_core_policy`. They are honest incumbents: the independent
global lower bound remains zero and neither result claims exact optimality.
Their emitted policies are nevertheless executable and exact-evaluate with
success probability one, complete cost, and zero failure, stop,
action-not-applied, no-matching-edge, unresolved, and off-policy mass.

The Amulet isolated runner completed in `1,634.185 ms` with a 60-second
watchdog, `timed_out=false`, `survivor=false`, and no process left behind.
Native phase time was `1,169.043 ms`: registry/layout `16.675 ms`, solve
`688.306 ms`, compile `0.459 ms`, and independent exact evaluation
`170.465 ms`.

All harness cap checks passed. The only solve stop bit was the authored state
cap; `max_reforge_work` was no longer a stop. Native retained ownership was
47,749,655 bytes live and 103,846,608 bytes measured peak. The standalone
exact evaluator retained and peaked at 52,235,719 bytes inside its unchanged
1 GiB evaluator envelope. Process working-set snapshots are recorded in the
raw report but are not mislabeled as a measured process peak.

## Failure chain and repairs

Gate 0 froze two independent failures: direct evaluation exhausted the
20,000,000 logical-reforge-work budget, then strict refinement rejected a
strict junk class assigned to multiple coarse parents. Three existing root
repairs were retained and inspected before this run:

1. strict layouts refine their coarse parent layout rather than reconstructing
   an unrelated partition;
2. direct evaluation and strict-lift publication stages each receive an
   independent `max_reforge_work` budget; and
3. exact attribution selects the memory-projected shared-row solver when the
   expanded pair transpose cannot fit the remaining owned-byte envelope.

Those fixes exposed a valid three-state destructive-renewal core, but the
compiler initially serialized it through a semantically empty router. Exact
evaluation recognizes goal-progress gating only when the retry is local to the
operation, so that four-node graph expanded the complete physical reforge
distribution. `solver_compile.cpp` now emits the proved renewal directly as
`start -> renewal`, `renewal -> goal`, and `renewal -> renewal`. The latter is
the default retry edge. Exact topology assertions reject a reintroduced router,
and the compiler witness's raw gated-kernel bits must still match the runtime
calculator kernel.

That repair was sufficient for Ring. Amulet then isolated the remaining work
explosion: one factored final-depth row spent the complete budget enumerating
44 physical roll buckets and canonical completed sets before committing a
single recurrence term.

The final repair enables the calculator's existing proved exchangeable-family
compression inside the **independent evaluator's own CalcContext**. It does not
reuse a solver row. A family is mergeable only when it is junk-only,
externally group-disjoint, and identical in side, coarse junk observation,
goal-block mask, and per-family weights. Goal, below-tier, multi-observation,
and externally conflicting families remain physical. The evaluator enables
this exact implementation only for a clean coarse semantic carrier;
identity-observing or survivor-preserving strict carriers retain the raw V3
path.

No probability was dropped, no mapping rule was weakened, no independent
evaluation was bypassed, and no state, transition, memory, time, or reforge cap
was increased.

## Independent evaluator A/B

The focused native evaluator regression evaluates one tractable compiled Chaos
renewal twice: once with exact exchangeable-family compression and once with
the internal raw-family control. Both are independently reconstructed exact
evaluations. The test requires equality within `1e-12` for every public
terminal/off-policy/unresolved mass, known/total/occupancy cost, expected
material quantity, every edge traversal, and the derived canonical goal/retry
probabilities. It also requires the compressed run to retain the same physical
family count while reducing roll buckets, frontier nodes, and V1 logical work.

Raw probability-bit hashes are intentionally not conflated with semantic
equality: the two exact summation paths have distinct observed hashes,
`1db4f52dbae5d008` (compressed) and `1a4ecd2db3ada590` (raw). Each is nonzero.
The separate compiler contract remains bitwise: its certified witness hash
must equal the runtime hash for the selected path.

The final shared native build completed all 49 build steps. The focused
evaluator command then reported:

```text
poecraft_engine_tests.exe --solver-eval-only data/compiled/current
  solver eval exchangeable-family A/B:
    compressed_hash=1db4f52dbae5d008
    raw_hash=1a4ecd2db3ada590
  solver evaluator tests: 16,786 checks, 0 failures
```

That suite also completed its existing 10,000-run S8.4 accounting loop and
10,000-run exact-versus-Simulator control. Before the evaluator compression,
the focused solver/compiler topology checkpoint reported 98,566 checks and
zero failures; its exact and bounded compact-renewal controls each completed
10,000 Simulator runs. The benchmark runner itself deliberately skipped Monte
Carlo verification, so these native controls are the Simulator evidence and
the benchmark reports remain exact-evaluation evidence.

## Amulet exact work delta

The V2 and V3 attempts use the same fixture and each publication stage keeps
the same independent 20,000,000 logical-work cap. Direct compiled evaluation
hit that cap in V2; the detailed retained row below is the subsequent
`strict_selected` publication attempt, which exposed the same clean
40-family/44-bucket mechanics and interrupted before a recurrence commit. V3
is the independently owned `exact_evaluation` row and completed at 1,043,760
logical units, a 94.781% reduction. The native A/B above separately proves the
compressed and raw independent-evaluator semantics.

| Row metric | V2 retained raw-family attempt | V3 compressed exact evaluation | Reduction |
| --- | ---: | ---: | ---: |
| Physical families built | 40 | 40 | 0% |
| Roll buckets built | 44 | 23 | 47.727% |
| Availability classes | 39,894 | 560 | 98.596% |
| Frontier nodes | 444,445 | 43,490 | 90.215% |
| Eligible nonterminal edges | 2,605,242 | 122,376 | 95.303% |
| Terminal contributions | 774,648 | 82,468 | 89.354% |
| V3 predecessor entries | 556,500 | 26,072 | 95.315% |
| V3 denominator edges | 4,724,200 | 122,640 | 97.404% |
| V3 subset checks | 1,794,000 | 64,560 | 96.401% |
| V3 candidate sets | 299,000 | 11,520 | 96.147% |
| V3 recurrence terms | 0 | 11,520 | completed |
| V3 commits | 0 | 11,520 | completed |
| V1-equivalent logical work | 20,000,025 attempted | 1,043,760 | 94.781% |
| V2 evaluator work | 9,321,131 | 516,308 | 94.461% |
| V3 evaluator work | 11,875,135 | 505,260 | 95.745% |

The completed V3 cache-miss row is explicitly owned by `exact_evaluation`,
uses evaluator `v3_factored`, and has disposition `completed`. Its second
request is an evaluator-local cache hit. The first row creates two canonical
terminal successors, merges 82,466 duplicate terminal contributions, interns
two states, and reconciles the compiled value with zero reported solver-cost
delta. This is independently recomputed evaluator evidence, not publication of
the solver's coarse cached distribution.

## Ring topology checkpoint

Ring did not need the family-compression repair and was not rerun after V2.
The preserved V2 result already proved the shared topology boundary:

- state-cap stop with lower bound `0` and evaluated upper bound
  `490.41233174994977`;
- direct certification `complete`, `proper=true`, complete cost, zero
  off-policy probability, and reconciled solver/evaluator cost;
- one exact-evaluation V3 row completed at 8,294,364 logical units;
- graph 3 nodes, 3 edges, 1,065 bytes; and
- exact strategy evaluation `matched`, success probability one, no timeout,
  and no surviving process.

Restricting V3 to Amulet preserved this already-green evidence and followed
the focused testing boundary; no broad portfolio was run.

## Commands and frozen artifacts

The final build and focused commands were:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset all-native --parallel 4
build\engine\poecraft_engine_tests.exe --solver-eval-only data\compiled\current
py -3 tools/ingest/benchmark_solver_corpus.py --root . --executable build/solver-goal-realignment/gate6/frozen-v3/poecraft_solver_benchmark.exe --artifact data/compiled/current --corpus fixtures/solver-reliability/v1/manifest.json --output build/solver-goal-realignment/gate6/focused-v3 --case reliability-class-amulet --max-workers 1 --watchdog-ceiling-seconds 60 --goal-progress-gated-reforges
```

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| V1 diagnostic benchmark executable | 9,687,350 | `0cd5811f5d2b4e33eba83ca7cfd4ffb430c8d28e29116bbd688d3ba7aa481498` |
| V1 Ring report | 220,464 | `2602bb0638b12633ce0fe3e13d9d8bba114896a68e3fb94d657b2448661adf5e` |
| V1 Amulet report | 214,470 | `c9aed94d9a9e331e235a5bb56c3b86301472e320676ceda3c5efbdec7369fc92` |
| V2 topology benchmark executable | 9,687,862 | `981fe5e089c03b330e511100e613700445f5e64d17c5016a95db7f7f35fd1b14` |
| V2 Ring report | 221,069 | `737414ca470bff03fba9d0ef51cdecf577eacacdee40098a07cf5725e3cd28db` |
| V2 Amulet report | 214,500 | `225f23868701fe89cfced5c15a63c1e7457f1ae6ee505c6ba15963c8fe184ebe` |
| V3 compressed benchmark executable | 9,739,759 | `8900d27a26ec4b999e3bcf2b0a998afe2a89d8c51435accfa636ea3bc45a758b` |
| V3 ledger | 8,556 | `f3e948a24fc391df6eda9dc3ef4374a7f3011ac1464c990073136d4d9db2b9db` |
| V3 Amulet report | 221,023 | `5783af62390e3a7c62feaeea068fc9dd1a3465285771bcb4bb5cab68444f6c6d` |
| V3 Amulet strategy | 1,068 | `0d4c64f9006a0aba7c490e31cb1dbe204b417c2bef30b3cf1d16e468713769e6` |

Raw workspace-local evidence is retained under:

- `build/solver-goal-realignment/gate6/focused-v1`;
- `build/solver-goal-realignment/gate6/focused-v2`;
- `build/solver-goal-realignment/gate6/focused-v3`;
- `build/solver-goal-realignment/gate6/frozen`;
- `build/solver-goal-realignment/gate6/frozen-v2`; and
- `build/solver-goal-realignment/gate6/frozen-v3`.

The V2 executable and reports remained untouched during V3.

## Every-base matrix and arbitrary starts

The final direct session-builder audit constructed every 979 compiled ordinary
base at item level 86. All 979 had a reachable natural-family feasibility
witness, fully accounted price vocabulary, a legal one-action transition, and
a compiling and exact-evaluating one-operation strategy. The audit completed
2,811,093 checks with zero failures and covered every represented item class,
low/high item-level workspace switching, influence masks, Eldritch eligibility,
implicit initialization, and normal/magic/rare action vocabulary.

The two current-product fixtures initially exposed two independent boundaries.
The full-junk-prefix suffix-salvage control found a 69.871-chaos core policy but
the strict compiler/evaluator refinement exhausted the joint 1 GiB owned-memory
budget. The metamod start completed the restricted graph but had no executable
upper while delayed action rows remained open. Enabling the already-audited
`high_impact_executable_uppers` work scheduler closed the salvage case without
raising a cap. The metamod case then proved that operator-major incremental
scheduling had skipped carriers discovered after an earlier operator sweep.

High-impact scheduling now maintains an exact completed state/operator-pair
ledger and closes its full delayed-action Cartesian product before declaring
the envelope exhausted. The ledger is included in solver-owned memory
accounting. The constructive renewal proof also accepts a stationary exact
renewal kernel on any already-expanded strict carrier, composes it through the
real fresh-base setup row, and prefixes an arbitrary start with priced Restart.
It never treats the original protected carrier as the fresh carrier. Renewal
row retention validates the original row owner and exact retry signature.

The final native qualification is retained under
`build/solver-goal-realignment/gate6/final-native-v3-20260812`. Both cases use
the same explicit 1 GiB cap, close the complete action envelope with no open
obligations, compile, independently exact-evaluate to their solve value with
success probability one and zero off-policy mass, and complete 10,000 of
10,000 Simulator runs:

| Case | Exact cost | Solve | Graph | Sample mean |
| --- | ---: | ---: | ---: | ---: |
| `vaal-regalia-allflame-metamod-start` | `61.3644166136243` | 26.752 s | 169 nodes / 333 edges | `60.89687999999893` |
| `vaal-regalia-allflame-full-prefix-suffix-salvage` | `69.871075553584731` | 8.259 s | 3 nodes / 3 edges | `69.911720000000187` |

The metamod policy's proper bootstrap is an exact
`essence:Metadata/Items/Currency/CurrencyEssenceAnger6` renewal at
`69.04447612985625`, composed through a `68.9992115216657` fresh-base anchor
and the arbitrary protected start. Its complete policy improves that bootstrap
to the exact 61.364-chaos optimum. The root Chaos candidate is explicitly and
correctly rejected because its nonterminal retry signature changes after the
protected start is consumed.

Final artifact hashes from the qualifying reports are:

| Artifact | SHA-256 |
| --- | --- |
| Native benchmark executable | `07fa9eaf4e01bd271ebc9555dc0c01dc53d96bb6ca5c156a00d443465d9e4faf` |
| Gate 6 ledger | `01b418b3ad9f8800350ba11150a50c2564cb77c2d80fd552884315fe5049a897` |
| Metamod report | `af25ae5a7bfd76eb41a90f2341045727f77705000d2895704700355bfae3e292` |
| Metamod strategy | `bd83cca8c5eecbaed334cf50468e0efc8efe69db76ff1f2a38e8f352b63912a2` |
| Salvage report | `94c31b11f3916b10e2a0662f637c56635d7ab614a9d2f821e3e26445a9a96cea` |
| Salvage strategy | `c150aa57a763c96c00257e036f150d4ed44692f5e28813e9d81fc904d3860c89` |

The final focused native suites were Solve 6,672 checks, compiler 804, and
evaluator 1,174, all with zero failures. Gate 6 is closed. Release-WASM,
frontend, primary, and portfolio acceptance remain owned by Gate 8.
