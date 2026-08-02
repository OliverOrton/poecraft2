# Proof-Carrying Quotient Refinement Report

**Status: retained implementation with a measured Gate 5 structural stop.**

Implemented on `codex/proof-carrying-quotient-refinement` from
`882e70968cd86090e9fc4e882fc6e01886aa62a4`. Gate 4 is
`dac7c6f9670a17e788381fd1ce4c33fc8c4925e2`; the final documentation commit is
recorded in the root handoff after creation.

## Outcome

The retained implementation proves that exact carrier coverage can be
streamed through the existing shared split-only partition and Bellman
authorities without a global strict-carrier vector, complete strict adjacency
graph, second observation vocabulary, second evaluator, or compiler change.
Focused proof gates and the medium end-to-end case pass through properness,
unchanged strategy compilation, independent exact reconciliation, and 10,000
successful simulations with zero off-policy routing.

The architecture is not core-qualified. The frozen binding two-goal run
returned `refused_resource_cap` on `max_reforge_work` before partition
initialization and without publishing a policy. The run completed only four
exact carrier materializations and two exact kernels, but those kernels
contained 345,192 transitions. Compilation, exact compiled evaluation,
reconciliation, and simulation were consequently not applicable.

Per the amended directive, that binding red result ended Gate 5. The
remaining native hard cases and portfolios were not spent, Gate 6 was not
entered, and the final full suite was not run.

## Qualification matrix

| Level | Result | Binding reason |
| --- | --- | --- |
| Core quotient | unqualified | The frozen two-goal case produced no executable policy before `max_reforge_work`. |
| Five-goal scale | unqualified, not run | Native scale acceptance stopped after the binding core failure. |
| WASM product | unqualified, not run | Gate 6 was not reached after native core failure. |

Passing focused and medium evidence remains valid, but it is not promoted to
any of these product qualification claims.

## Retained production architecture

The final production path has these properties:

1. Strict states are retained as replay locators and per-carrier immutable
   compact-kernel payload IDs, not a global semantic-key population.
2. One exact carrier slice is materialized at a time. Its ledger charge is
   released after deterministic partition replay.
3. `refine_closed_probabilistic_partition_replay` is the replay-backed entry
   to the existing shared probabilistic partition. External frontier labels
   remain collision-free and distinct until closed.
4. `SolveTransitionCache` owns immutable proof payloads and independent
   generation-stamped row use sites, reverse dependencies, invalidation
   worklists, and memory accounting.
5. Bellman rows distinguish optimistic lower evidence from certified upper
   evidence. Uncertified aggregation cannot publish an executable policy.
6. `QuotientPublicationAudit` requires current reachable certificates,
   closed target generations, terminal-reachable bottom components, and a
   finite proper exact value for every entry.
7. Compilation uses canonical strict locator coverage and the existing
   `RefinedPolicyCompileRouting`. Quotient IDs and new action vocabulary never
   enter strategy JSON.
8. The reconstruct-then-merge adapter remains a bounded focused oracle only.
   Production medium and hard paths report zero invocations.

No mechanic, price, product action filter, solver objective, public C ABI,
strategy schema, action vocabulary, or frontend authority changed.

## Gate boundaries and evidence

| Gate | Commit | Result | Tracked evidence |
| --- | --- | --- | --- |
| 0 | `4193f086bc7deffb5ce0e3b81f4045a42a4fe3c9` | provenance, fixtures, predictions, and qualification classes frozen | `proof-carrying-quotient-gate0.json` |
| 1 | `5c531d0c9eff204954a5d3d6883a0a2e6d99726a` | proof identity, collision validation, replay, invalidation, and ledger conservation | `proof-carrying-quotient-gate1.json` |
| 2 | `9e0ae6f3135515a9b358ee178a16b3658bea9939` | shared-partition CEGAR integration and cyclic witnesses | `proof-carrying-quotient-gate2.json` |
| 3 | `62ca542e76829d39a27323fa2d5c1cc6266ba567` | streamed Bellman integration, medium projection, and hard-case prediction | `proof-carrying-quotient-gate3.json` |
| 4 | `dac7c6f9670a17e788381fd1ce4c33fc8c4925e2` | publication audit, properness, compilation, reconciliation, and medium 10,000-run verification | `proof-carrying-quotient-gate4.json` |
| 5 | final archive commit | binding two-goal resource refusal and explicit stop | `proof-carrying-quotient-gate5-structural-stop.json` |

All evidence files are under
`fixtures/solver-reliability/v1/evidence/`. The raw Gate 5 report is an
ignored reproducible artifact at
`build/acceptance/proof-carrying-quotient/gate5-native/two-goal/natural-t1-breadth-two-4e65dda9c53b.json`.

## Focused and medium qualification retained

The final pre-hard checks were:

- quotient proof: 259 checks, zero failures;
- policy refinement: 4,829 checks, zero failures;
- compiler routing: 750 checks, zero failures; and
- medium `reliability-class-belt`: `bounded_feasible`, exact reconciled,
  compiled proper, and 10,000/10,000 simulations successful with zero
  off-policy failures.

The medium report completed in `1,331.4421 ms`, including `30.1819 ms` solve,
`10.1179 ms` compile, and `581.3679 ms` verification. It retained 266 exact
carriers in 19 quotient classes. Its strategy had 14 nodes, 33 edges, and
SHA-256
`adf9ae9312ae1c184a3f467effde14e4c52ef789678a78ad1216bf53a4e04003`.
The solver root `9.143792577895411` reconciled to independent exact cost
`9.14379257789546`, with zero exact off-policy mass.

The medium native peak was `79,085,896` bytes. Quotient-local accounting was
`3,994,390` bytes, including `181,604` row/kernel bytes and a one-slice peak
of `28,472` bytes. The path had zero reference-adapter calls.

## Binding Gate 5 measurement

The case was invoked directly with exact compiled evaluation requested,
10,000 verification runs requested, and goal-progress-gated reforges. A hidden
PowerShell child process supplied the external 900-second watchdog. The
watchdog did not fire.

| Field | Result |
| --- | ---: |
| Case | `natural-t1-breadth-two-4e65dda9c53b` |
| Actual status | `refused_resource_cap` |
| Stop cause | `reforge_work_cap` |
| Named resource | `max_reforge_work` |
| Frozen cap | `20,000,000` |
| Report total / solve wall | `5,787.0836 / 3,679.1468 ms` |
| Coarse start lower | `752.9009075663787` |
| Exact carriers materialized / retained | `4 / 0` |
| Exact transitions / kernels | `345,192 / 2` |
| Partition rounds / classes | `0 / 0` |
| Certified rows | `0` |
| Peak live slice | `1 / 4,198,696 bytes` |
| Native live / peak owned | `305,293,988 / 375,483,167 bytes` |
| Working set after run | `386,867,200 bytes` |
| Reference adapter calls | `0` |
| Complete strict graph reconstructed | no |
| Policy / compile / exact evaluation / simulation | unavailable / not applicable / not applicable / not applicable |

The raw report SHA-256 is
`fcef98a4ddadeec6d6c3cda51ab53d4710bba23097a86e1516f5dcbabfe32837`.
The benchmark executable SHA-256 is
`9cf90458cdb27ead4732d3529d72f66958a041233ee273e7a849060e6843a718`.
The runtime artifact manifest SHA-256 is
`22bbb2ee77064bd21c205b99e231767416dfb6a8e7ba3ae0d7374423522e7de5`.

The portfolio harness says the classified refusal meets its broad corpus
expectation, but that is not the binding milestone contract. Gate 5 requires
an executable policy, exact reconciliation, and 10,000-run zero-off-policy
verification for this case; all are absent. The core result is therefore red.

## Prediction versus actual

Gate 3 predicted 13,076 final quotient cells, 10,518 certified row use sites,
a `561,416,670`-byte peak, and 560 seconds, with ranges of 9,600-18,000 cells,
430-760 MB, and 470-700 seconds.

The Gate 5 prefix cannot validate or falsify that estimate. It stopped before
partition initialization with zero cells and zero certificates, so the lower
observed memory and wall time describe an incomplete pre-partition prefix,
not a completed quotient solution. The prediction remains explicitly
non-comparable rather than being scored as an overestimate.

## Smallest preserved failure witness and cause

All focused proof witnesses passed. The smallest reproducible production
failure is therefore the frozen two-goal fixture itself, whose failing prefix
is only four exact carriers and two kernels. Its first retained compatibility
counterexample is coarse state 18, primitive `exalt`, required feature mask
`2297351`, unavailable mask `2097152`, with a preserved exclusion witness.

Source inspection localizes the resource shape:

- `quotient_compact_action_rows` first certifies the current selection, then
  asks `candidate_selection` for every already-admitted operator when the
  carrier has no complete inherited selection or is affected by local
  reoptimization;
- `candidate_selection` materializes exact primitive or option outcomes under
  the existing ledger; and
- streamed discovery retains only one carrier slice, but completes those
  candidate rows before installing the carrier in the shared partition.

Thus carrier storage is bounded, but candidate breadth can consume the work
budget before selected-policy closure. Raising `max_reforge_work`, narrowing
the product vocabulary, or adding named fixture behavior would evade rather
than satisfy the architecture contract.

## Next architecture recommendation

The next boundary should schedule proof by competitiveness:

1. Discover and certify selected-policy closure first.
2. Retain every admitted but uncertified alternative as an explicit unresolved
   lower-only proof obligation; do not discard it or treat it as an upper.
3. Certify an alternative lazily and transactionally only when Bellman bounds
   or a counterexample show that it can be competitive.
4. Continue to publish an upper only when every selected reachable row is
   current, closed, lumpable, proper, compiled, and independently reconciled.
5. Preserve action independence, the existing vocabulary, and all frozen
   mechanics, price, cap, ABI, strategy, and frontend boundaries.

Deterministic checkpoint/replay remains deferred. Oliver must select a new
active implementation plan before work resumes.

## Acceptance not run

After the binding red result, the qualified Fracture regression,
representative four-goal case, five-goal scale case, 27-case smoke corpus,
49-case native reliability portfolio, selected Ring/Gloves verification,
release WASM rebuild and corpus, web tests, TypeScript check, and final
`scripts/test.ps1` were not run. No browser or rendered visual review was
performed. Nothing was pushed or merged.
