# Gate 0 Selected Policy And Finalization Evidence

**Status: complete.** Captured on 2026-08-16 from approved-plan commit
`982b2f2` plus the Gate 0 instrumentation diff. Public behavior is unchanged
on the retained five-T1 and three-prefix controls. The newly frozen four-goal
control exposes a precise pre-existing final-evaluator/resource-stop boundary.

Parent: [Selected Policy Publication And Cooperative Exact Refinement](../selected-policy-refinement-plan.md)

## Build And Focused Checks

- fresh native `scripts/build.ps1`: pass;
- focused native solver solve tests: 94,757 checks, zero failures;
- focused native policy-refinement tests: 826 checks, zero failures;
- focused benchmark-corpus tests: seven tests pass;
- release WASM rebuild: pass on retry;
- first WASM rebuild attempt reached final metadata DCE, then failed opening
  the generated `.wasm` output with a transient Windows `Invalid argument`;
  no source or build-script change was made for the successful retry;
- release module SHA-256:
  `da21809d726bcc18046a7c19d5cd004e638b0ef193302bd45a669705452a29cf`;
- release WASM SHA-256:
  `134865755a74b06ed60e1d462835847042699ccb8e20face974add0f2a57705c`.

No full repository pipeline or rendered browser review was run.

## Five-Natural-T1 Selected Policy

The ordinary eight-work product profile and fixed 1,024-work control are
semantically identical:

| Field | Work 8 | Work 1,024 |
| --- | ---: | ---: |
| total wall ms | `5662.466` | `5490.133` |
| solve wall ms | `1404.317` | `1235.121` |
| synchronous finalization ms | `877.113` | `864.865` |
| maximum worker slice ms | `97.263` | `118.760` |
| public policy / termination | `bounded_feasible` / `numerical_stability` | same |
| lower / evaluated upper | `0` / `37279857.73995944` | same |
| compiled graph | 3 nodes / 3 edges | same |
| compiled strategy SHA-256 | `27fd249914ea5026e2d65de4ab1d409762449242ca02a57a06a0248b5f8f2694` | same |
| selected coarse start estimate | `690872.2205617285` | same |
| selected sparse-policy hash | `02bc7bb2b946bcf7` | same |
| selected rows / reachable states | `1345` / `691` | same |
| reachable selected rows / actions | `689` / `9` | same |
| structural materialization | yes | yes |
| snapshot wall ms | `5.3751` | `5.2577` |
| snapshot transient bytes | `6921` | `6921` |

The final unchanged policy round contains one tolerance-suppressed strict
comparison at state 878: retained row 4438 evaluates to
`690872.2205617285`; strictly preferred row 4436 evaluates to
`690872.2182025847`. The delta is `0.002359143807552755`. This is evidence that
the stable selected policy exists before restoration, not an executable-cost
certificate.

The public result remains the independently evaluated Chaos renewal. Gate 0
does not admit the selected snapshot to the certified fallback portfolio.

Raw reports:

- [work 8](gate0-selected-policy-five-t1-work8.json), SHA-256
  `3a6b4fabb58ec53b837e6de37367a0eb7e024f824b42e7923e2b32071ea187fb`;
- [work 1,024](gate0-selected-policy-five-t1-work1024.json), SHA-256
  `b49bf0d549860058d10ac800463b16c28d6dfe87cd11d96018ca9d2de64a374f`.

## Exact Three-Prefix Neutrality Control

The retained product witness remains exact at
`2186.6911143146394`, with lower and upper equal, independent exact graph
evaluation matched, a 42-node/121-edge compiled strategy, and a maximum worker
slice of `52.253` ms. Its compiled strategy SHA-256 is
`32faed8fc7dee80383d892091d55c0eb28315a24ccfaba7f2a153e36eacc5459`.

Total wall was `108832.310` ms and synchronous finalization was
`89281.304` ms. The measured finalization stages were:

| Stage | Wall ms |
| --- | ---: |
| extraction/materialization | `3.528` |
| direct certification | `15211.687` |
| strict lift total | `73777.968` |
| strict carrier discovery | `12474.332` |
| strict partition refinement | `39462.120` |
| strict fixed-policy evaluation | `15.226` |
| strict local reoptimization | `33.131` |
| compilation/serialization, all attempts | `65.283` |
| exact graph evaluation, all attempts | `30365.432` |

The report is marked expectation-failed only because Gate 0 deliberately used
`--skip-verification` while the archived corpus still requires 10,000 sampled
runs for final acceptance. Solve, compile, and independent exact evaluation
all passed.

Raw report: [three-prefix work 8](gate0-finalization-three-prefix-work8.json),
SHA-256
`63a1ffd63a8e5520c24772cc896357bcf3ce47ad4920b8a49a2e79524dd0eab2`.

## Newly Frozen Four-Goal Boundary

Oliver's empty Conquest Lamellar with the same three natural T1 prefixes plus
natural T1 spell suppression completed after `173703.979` ms. It did not close
exactly:

- direct certification: `52141.243` ms;
- strict lift: `116402.840` ms;
- strict carrier discovery: `13478.537` ms;
- strict partition refinement: `42879.392` ms;
- combined exact graph evaluation attempts: `105085.191` ms;
- strict carriers / kernels / cache hits: `113487` / `3844` / `3`;
- the strict compiled graph reached 95 nodes, 319 edges, and 3,906 exact
  evaluator pairs before exact attribution exhausted `max_sweeps=100000`;
- the retained independently evaluated fallback costs
  `588884.995654997` and has 6 nodes / 7 edges.

The current result reports `bounded_feasible` with lower zero but termination
`exact_closed`. That combination is a pre-existing result-truth defect in
`successful_refined_publication_termination()`: the restored fallback inherits
the coarse exact stop even though strict global closure failed. Gate 2 must
repair it while restructuring candidate publication. It is not accepted as an
exact control result.

Raw report: [four-goal work 8](gate0-finalization-four-goal-work8.json),
SHA-256
`504eeead462b1c82a43c9e015e6e3127f0cd794d7bdc7ef93f596ec2e006cb51`.

## Gate Decision

Gate 0 passes:

- the five-T1 pre-restore policy reproduces exactly across work sizes;
- the snapshot is structurally materializable but remains explicitly
  unverified and non-publishable;
- measured snapshot overhead is under 6 ms, well inside the 100 ms allowance;
- the retained five-T1 public result and known three-prefix exact value remain
  unchanged; and
- the long boundary is measured: partitioning matters, but exact graph
  evaluation is the largest aggregate owner on both exact witnesses.

Proceed to Gate 1 selected-policy preservation. Cooperative work must cover
the evaluator as well as carrier/refinement loops; moving only kernel discovery
will not close the measured finalization boundary.
