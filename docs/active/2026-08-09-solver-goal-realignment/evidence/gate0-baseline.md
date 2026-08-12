# Gate 0 Baseline

**Measured:** 2026-08-09 on `codex/solver-goal-realignment` after the
boundary-only commit `8b67d1ace9aea89907e0c6834e7b9e235b1c3413`.

Parent: [active plan](../plan.md)

This is the frozen before-state for the solver goal-realignment milestone. The
benchmark outputs below are workspace-local raw artifacts under
`build/solver-goal-realignment/gate0`; this document retains their identities
and the material scalars without checking large generated reports into Git.

## Frozen identities

The production source baseline is
`c00c18133f88151dc971955c161d01e2178aef4b`. The next commit contains only the
active documentation boundary.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/engine/poecraft_engine_tests.exe` | 11,609,156 | `3d064bb3072a871236b84b44bd61bf7a88de2084088c1f691de85c75902ef493` |
| `build/engine/poecraft_solver_benchmark.exe` | 9,624,380 | `bc8ef6375c8573c151a98ddda29039e71671297598349342a70e45d0b017e69f` |
| `build/engine/poecraft_engine.dll` | 9,347,811 | `18a98c079e3bdbe73d1a90830170a011b89d1ac7762b2af7fbeb118ec0a067b4` |
| `build/engine/libpoecraft_engine.dll` | 5,214,854 | `5c1992a6e25dc6f7d520b6c02395a58b40a1c6061096b51c64c1976410ce5c11` |
| `bindings/wasm/dist/poecraft_engine.mjs` | 41,798 | `2eb81eb0005fa50fa70291b421dacd83f7e1a28d1333fff93733eaba565650cd` |
| `bindings/wasm/dist/poecraft_engine.wasm` | 4,981,408 | `1038ed0e16806c5b3a6d557b8a07de69cef6ed41f90af455c4a545318716c9c9` |
| `data/compiled/current/manifest.json` | 5,810 | `34c588b7615b3d5148f35fe04f858cd75f6f1e35fab0f500aba5867b5d9bf04b` |
| `data/compiled/current/game-data.json` | 16,439,442 | `af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5` |
| `data/compiled/current/strings.json` | 6,534,436 | `654a55489b47740672aea5a27ef52f01e23efc3bf5ce337be5a58756b72f955c` |

The compiled source identities are data hash
`93c97d879e11b2022fc272b4d51c6336c3656151cd758ec1c2427d8f74bfc615`
and source `repoe-e4eaf06c20e1ddb4` /
`e4eaf06c20e1ddb48f5173f7767c40eea637391925fa58d7f2aea1bed8724cac`.

The economy index SHA-256 is
`523f1f519acdd074def76936e5522e285c9a7c7e1e8f74961021abff566622f7`.
The pinned Allflame snapshot has content identity
`a122cad9494aa3361016b6f9c542e029e7aa1465de6d04bd6b5b150b5d26c485`
and file SHA-256
`05cddc3b8440bed43049ae0e36bb5feba9d9ca28eb4277c101aa9764ed55c88d`.
The historical Mirage snapshot remains byte-identical: content identity
`9175d37d83d90ab936e572f04c7599afbf18ff6cefc90786a5276da1759cd52f`
and file SHA-256
`9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.
Neither complete economy portfolio was rerun.

The new primary fixture identities are:

```text
manifest      8ae9d442d5015923074255686b86cbdfac3e9c2e1412f5ebf959aa1b8e9a9a70
primary case  09c5d4b89423de34761f0b5fc993770a31d4c28eed91b5d2d6190dd3f8155895
```

Its harness-recognized strict expectation tuple is `converged`, `exact`,
`compiled`, `run`. Exact graph evaluation is required separately. The native
benchmark does not consume the descriptive `benchmark_mode` or the two
watchdog fields; `anytime-solver-corpus-runner-v2` enforced the 300-second
process watchdog (`timed_out=false`, no surviving process).

## Primary Allflame control

The primary fixture is an empty rare item-level-86 Conquest Lamellar with a
five-chaos fixture-local base price and four natural T1 family goals:

- `LocalIncreasedArmourAndEvasion8`;
- `LocalIncreasedArmourAndEvasionAndStunRecovery6`;
- `LocalBaseArmourAndEvasionRating8`; and
- `AdditionalPhysicalDamageReduction5_`.

The frozen native result is bounded, not exact:

| Field | Before |
| --- | --- |
| Wall / solve | 57.938 s / 48.536 s |
| Result | `refused_resource_cap`; `transition_cap` |
| Cap hits | `max_transitions`, `max_solver_owned_bytes` |
| Declared scope | exact only within `goal_progress_gated_reforges` |
| Lower / upper | `0` / `588884.99679812137` |
| Published policy | verified Chaos renewal fallback, cost `588884.99679812137` |
| States | 3,621 discovered and expanded; 60 goal; 338 policy-reachable |
| Sparse work | 29,042 state/action rows; 45,905 retained transitions |
| Distribution work | 11,727,369 raw outcomes; 11,122,411 logical reforge work |
| Native owned bytes | 111,339,003 live; 141,295,273 measured peak |
| Product roles | 24 candidates; 157 automatic dependencies; 105 filtered |
| Fossils | 9 generated of 15,275; 15,266 deferred |
| Automatic work | 888 operators; 92 dependency primitives |
| Automatic rows | 12,415 considered; 5,317 eligible; 7,098 rejected |
| Incremental envelope | open: 15 admitted; 9,767 non-improving; 33,990 unevaluated; 1 unresolved; 2 inapplicable |
| Outside Chaos support | 581 states |
| Work hashes | transition `53c5d280ea4b6207`; extracted policy `b5c14af50d51e35d` |
| Core-policy hash | `a80e62b746399972` |
| Compiled fallback witness | `f9fb80ca56e18a2f` |
| Compile / exact evaluation | 6 nodes, 7 edges, 2,965 bytes; exact cost matched; success 1; failure/off-policy/unresolved 0 |
| Simulation | skipped in the baseline runner invocation; the final acceptance must pass `--run-verification` for 10,000 runs |

Candidate families were Currency 10, Fossil 9, Harvest 4, and Fracture 1.
Dependencies were 147 bench and 10 Eldritch/other descriptors. Filtering
removed 65 Essences, 29 Harvest rows, one bench row, and ten other rows. The
explicit reasons include four corruption-only and 61 non-goal Essences, 29
non-goal Harvest rows, three deferred Veiled primitives, one excluded Eldritch
Exalt, and six unrelated Influence Exalts.

Automatic eligibility was:

```text
kind                 considered  eligible  rejected
Imprint                     485         0       485
protected side            2,410       116     2,294
temporary bench           4,009     2,573     1,436
primitive Fracture        3,570       873     2,697
Eldritch side             1,794     1,755        39
Cannot Roll                 147         0       147
```

The preferred 297-state, five-action core policy was retained as a bounded
candidate, but direct certification tried to emit more than the 64 MiB strategy
JSON cap. Strict lift then requested 948,156,116 bytes against a 947,607,740
byte proof budget. Certification recorded 1,250 alternative obligations and
zero unresolved obligations, but scheduled none, avoided 6,737 exact
alternative rows, and left both action accounting and the exact alternative
envelope open. The fallback compiler consequently emitted the smaller verified
Chaos policy. This is valid incumbent evidence, not a proof of Chaos
optimality.

Raw partial report:
`build/solver-goal-realignment/gate0/primary-allflame-canonical/partials/conquest-lamellar-allflame-four-natural-t1.1786298794020189600-812.json`,
873,004 bytes, SHA-256
`4463d6544ef5fe7e418f53305849d256e7fc8db546e348d0c7eefb93589878d9`.

## Focused benchmark controls

All focused corpus solves used `goal_progress_gated_reforges`. `A/D/F` below
means product candidates / automatic dependencies / filtered descriptors;
`admit/non-improve/unevaluated/unresolved/inapplicable` is the incremental
action envelope.

| Case | Result and bounds | A/D/F | States (discovered/expanded) | Rows/transitions/reforge | Envelope | Compile/evaluate |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `oracle-real-one-mod` | exact, `L=U=8.0201442841288433`, 0.311 s | 273/0/0 | 3/3 | 4/3/2,592 | closed, 0/0/0/0/0 | 7/9/2,159 B; matched |
| `reliability-start-fractured` | exact, `L=U=7.9350414538068117`, 0.549 s | 17/146/118 | 83/83 | 1,830/836/471,643 | closed, 114/1,104/0/0/26 | 10/14/29,180 B; matched |
| `reliability-class-ring` | no policy, state cap, `L=490.4123317499616`, 1.086 s | 23/126/115 | 2,163/1,000 | 6,340/27,062/19,999,980 | open, 0/0/6,256/3/0 | not applicable |
| `reliability-class-amulet` | no policy, state cap, `L=144.88045459604126`, 1.297 s | 23/135/115 | 3,038/1,000 | 6,587/29,776/19,999,994 | open, 0/0/7,409/3/0 | not applicable |
| `reliability-class-belt` (supplemental) | exact, `L=U=9.143792577895411`, 0.397 s | 17/81/110 | 51/51 | 509/246/289,917 | closed, 0/326/0/0/10 | 10/14/23,264 B; matched |

The Ring and Amulet each retained a three-state bounded core candidate, but
direct exact certification exhausted `max_reforge_work`; strict refinement
then failed structurally because one strict junk class spanned multiple coarse
parents. No policy was published, so equal-looking internal core values were
not exposed as executable bounds. These are the frozen Gate 6 evaluator and
partition failures.

Raw report identities:

| Case | Bytes | SHA-256 |
| --- | ---: | --- |
| one-mod | 68,354 | `58f31a083f0c4985b37c217a6d57cde891b28731aef002950b5e5acfe802d728` |
| partial fractured | 393,657 | `74144a56dc4b34921638b9acc011e656a4d8c2e4cd81818a87d6ef7eda2600b7` |
| Ring | 212,462 | `21cece699d8c6b8df2eca8a28b9381026bafd1c2177aab9cac982f3c1a12acac` |
| Amulet | 212,422 | `323622fea14b3427ecc57a8537690d69e62ed292c2a74deefcb3e6c69a83e3c8` |
| Belt | 361,330 | `9064101f2b4d061419d7fc6f560cdc47514aaf00fe98b87b3b9a1d513f4c9e92` |

The tracked corpus does not yet contain benchmark cases for a forced automatic
Eldritch winner, an explicit observed-Unveil strategy, or the focused Imprint
checkpoint. Gate 0 therefore preserves those as native custom controls rather
than mislabeling unrelated benchmark cases:

```text
poecraft_engine_tests.exe --solver-api-only data/compiled/current
  solver API exact-router: V=5.4351; empirical=5.4229; 24 states
  public product Eldritch: V=1.1; 9 states; 10,000/10,000 successes
  2,293 checks, 0 failures

poecraft_engine_tests.exe --solver-automatic-eldritch-only
  exact=10.473684; empirical=10.571800; 337 states; 10,000 runs
  85 checks, 0 failures

poecraft_engine_tests.exe --solver-imprint-only data/compiled/current
  V=105.432000; 2,230 checkpoint creates; 2,166 restores; 64 runs
  59 checks, 0 failures

poecraft_engine_tests.exe --solver-compile-only data/compiled/current
  explicit observed Unveil: solver/exact cost=1.507122403857;
  success=1; failure/off-policy/unresolved=0
  compiled exact-router control: V=5.4351; empirical=5.4585; 10,000 runs
  791 checks, 0 failures
```

These controls establish primitive, compiler, evaluator, and Simulator support;
they do not provide the full product benchmark telemetry contract. Dedicated
public-path cases are required by the implementation gates.

## Frozen economy gaps

The Allflame snapshot contains 861 priced keys, 585 explicit missing keys, and
28 low-confidence keys. It prices no Bestiary input: `beast:craicic-chimeral`
and `beast:rare` are explicit missing keys, while the required
`beast:craicic-croaker` is absent even from the catalog. `base` is also missing
and is supplied only by the primary fixture's explicit five-chaos override.

This baseline preserves all prior Allflame and Mirage immutable artifacts. The
Gate 2 repair must add Croaker/current-economy fixtures and snapshots rather
than rewriting historical Chimeral evidence.
