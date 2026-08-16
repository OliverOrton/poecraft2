# Final Selected-Policy Refinement Acceptance

**Status: passed on 2026-08-16.** This record closes Gates 3-6 of the
[selected-policy refinement plan](../selected-policy-refinement-plan.md).

## Frozen identities

- behavioral source checkpoint:
  `f3b908011490b39347e8fad6945b471d888b9845`;
- release WASM: 5,292,139 bytes, SHA-256
  `c1b873930209fe86fae066ea85cf24130f0638eba0783c214bfa22ffcc5fd528`;
- Allflame economy identity:
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`;
- five-natural-T1 request SHA-256:
  `078cbb75e5ec1c7b4b74fa37f1c9a7467c7dc6bf6bc219239d3ea3274f1693a3`;
- three-prefix request SHA-256:
  `7ecfc7c6cbeef94c0f6985724f6f7afc1416505eca9a50806fbbaaf6a469862c`;
- three-prefix-plus-spell-suppression request SHA-256:
  `f12c5117c5dffe346d94a242e822657a6e3bb67eabf037ae68dd05968238fee2`.

The large raw reports remain local under `build/qualification`; their hashes
make this summary auditable without committing approximately 60 MiB of
duplicated telemetry:

| Report | SHA-256 |
| --- | --- |
| `final-wasm-five-t1-step8.json` | `cd4716cd3b027ebe2e84a5f30ed48d11695bc1ab9242992041dd9469845bc584` |
| `final-wasm-five-t1-step1024.json` | `c18c67ffaec3314bf5c29bd229baba75769f2882c7b059753f3f106856e5c333` |
| `wasm-four-goal-final2.json` | `627e366a8fd54dd058cfa88fe68b3eb26e0ec67dc0cf87f0eff06949f8ae3d93` |
| `final-wasm-goal-realignment-controls.json` | `d53c5a095280c6e08a59b71e17f6a39c45cff1b004ab80bab042c8d102b28e68` |
| `final-wasm-goal-realignment-verified.json` | `2b05308585a93ccaa8d01d9e0ab6d012ccebcccce56bae77d9878f6fb5900164` |
| `final-native-four-natural-t1.json` | `db0a674c4b11d64319079ae2cfadee4aa340341d26db6f71980661f23bbd4c51` |

## Five-natural-T1 result

The normal eight-item release-WASM request and fixed 1,024-work control are
semantically identical:

| Field | Eight-item product | 1,024-work control |
| --- | ---: | ---: |
| policy / termination | `bounded_feasible` / `numerical_stability` | same |
| certified lower | `0` | `0` |
| exact evaluated upper | `624800.9519118543` | same |
| solve / total wall | 6,334.119 / 14,249.939 ms | 6,250.739 / 13,856.970 ms |
| maximum native solve step | 185.229 ms | 187.872 ms |
| graph | 184 nodes / 666 edges | same |
| strategy SHA-256 | `30c802cbf02f6276cb76b53912aea41b6da6e5f9abeeefb1be132cffb9691487` | same |

The published graph uses `alteration`, `annul`, `augment`, `exalt`,
`harvest_augment:defences`, `harvest_augment:physical`, `regal`, `restart`,
`scour`, and `transmute`. It is not the former three-node Chaos renewal. Its
exact cost is about 59.7 times cheaper than the independently reevaluated old
Chaos fallback at `37279857.73995944`.

The selected coarse estimate was `690872.2205617285`; it was not used as proof.
Independent selected-candidate certification consumed 4.883 seconds. Across
the final report, direct certification consumed 0.804 seconds, selected
strategy compilation 0.036 seconds, and exact graph evaluation 5.629 seconds.
The exact graph has success probability one, complete pricing, and zero
off-policy mass.

The result remains honestly bounded: one missing-price and one unsupported
action leave the full request envelope open, so the public lower stays zero.
The benchmark workflow therefore labels the overall request
`refused_unsupported_action` even though it returns the cheaper executable
bounded policy. No cap fired. The policy's roughly 2.19 million expected
actions per success exceeds the unchanged 100,000-action Simulator horizon,
so 10,000-run sampled verification is not applicable; the independent exact
graph evaluation is authoritative.

## Cooperative exact controls

| Case | Result | Exact cost | Graph / strategy SHA-256 | Solve / total wall | Max step |
| --- | --- | ---: | --- | ---: | ---: |
| three prefixes | `exact` / `exact_closed` | `2186.6911143146394` | 42/121, `32faed8fc7dee80383d892091d55c0eb28315a24ccfaba7f2a153e36eacc5459` | 31,893.566 / 68,944.305 ms | 164.756 ms |
| three prefixes + spell suppression | `exact` / `exact_closed` | `38756.439580588245` | 95/319, `663b2b106671492d217d95a83de86fb58cae2327183f7053dcbf2259add7363d` | 33,059.629 / 37,910.963 ms | 238.509 ms |

Both remain below the new 250 ms cooperative-finalization boundary and have
success probability one, complete exact-cost accounting, and zero off-policy
mass. Native API tests abandon an in-progress solve after reaching a retained
`refining`, `compiling`, or `certifying` phase. The release-WASM worker test
also cancels in those native-owned phases before `Done`; `Done` now means the
following finish call is packaging-only. Successful runs do not measure a
separate cancellation latency, so the evidence is the next-step boundary plus
the measured maximum step, not an invented timing.

The standalone qualification runner still marks these supported-priced-subset
exact cases false when its old expectation helper demands the telemetry literal
`exact_abstract` (and the new four-goal fixture also says `solved` where the
runner emits `converged`). The actual public status, exact bounds/cost, graph,
hash, cap checks, and verification all pass. This is a pre-existing runner
expectation-vocabulary mismatch, not a strategy or proof failure; changing
that broader reporting contract was outside this milestone.

## Retained controls and sampling

The final release-WASM control batch preserved every frozen strategy hash and
zero off-policy behavior:

| Case | Exact cost | Strategy SHA-256 | 10,000-run mean | Sample result |
| --- | ---: | --- | ---: | --- |
| four natural T1 | `3745.7309340083884` | `af42f6ee5580fcb4661739c92f82923f944aca5c63f71ea80038c17bb9ecbe37` | `3738.9538811199136` | 10,000 successes, zero failures/caps/off-policy |
| automatic Eldritch | `0.018630169563331064` | `d4ba75a3bedc40be107924e5fa4e1af0d97f624d0713762e29d5718436344b66` | `0.018653899999999335` | 10,000 successes, zero failures/caps/off-policy |
| Warlord | `224.1238588972487` | `72058b147fd93b32fb5daa3a41f4bb9620863e52debf00bc962f0cf9d3fc6f8a` | `223.95892804999892` | 10,000 successes, zero failures/caps/off-policy |
| Imprint | `252.6535202127448` | `dfca593541f3f1267a4eb3768ab0f741af979b74c270478a7367b1d1710210ff` | `250.27322565002683` | 10,000 successes, zero failures/caps/off-policy |
| three prefixes | `2186.6911143146394` | `32faed8fc7dee80383d892091d55c0eb28315a24ccfaba7f2a153e36eacc5459` | `2199.461278130007` | 10,000 successes, zero failures/caps/off-policy |

The four-natural-T1 native and release-WASM reports agree on exact
termination, bounds, cost, and the 1,813-node/3,832-edge graph. Its retained
legacy worker budget is 20 seconds; it is not one of the two new 250 ms
cooperative-finalization witnesses.

## Proof, ABI, and build audit

- `UnverifiedSelectedPolicyCandidate` is a distinct type and begins with no
  certification authority. It enters the bounded portfolio only after an
  executable, proper, cost-complete, zero-off-policy, finite exact assertion or
  equivalent strict certificate.
- Portfolio selection ignores any candidate without independent certification
  and evaluation. The former Chaos incumbent remains independently available
  until the cheaper candidate wins strict evaluated-cost order.
- The finalization continuation, exact work, and completed result are included
  in fast and audited selected-owned-byte accounting.
- C ABI values `expanding = 1`, `iterating = 2`, and `done = 3` are unchanged;
  `refining = 4`, `compiling = 5`, and `certifying = 6` are append-only and
  mapped through native, WASM, TypeScript, worker, and product surfaces.
- Emscripten 6.0.0 full-LTO coroutine lowering produced invalid SSA for
  `solver_solve_finish.cpp`. The release builder compiles that orchestration-
  only translation unit as an optimized non-LTO WebAssembly object, then links
  it with full-LTO hot engine kernels. The qualified release module above is
  built by that path.

## Acceptance commands

- fresh `scripts/build.ps1`: passed;
- focused `poecraft_solver_solve`, `poecraft_solver_eval`, and
  `poecraft_solver_api`: passed;
- release `scripts/build-wasm.ps1`: passed;
- focused worker/WASM and result-presentation tests: passed;
- `npx tsc --noEmit`: passed;
- `git diff --check`: passed;
- one final `powershell -File scripts/test.ps1`: passed, including 3,462,490
  native checks with zero failures, benchmark-spec validation, 28/28 release-
  WASM smoke checks, and the complete non-visual web suite.

Oliver retains rendered and real-device/browser review; none was performed.
