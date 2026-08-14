# Gate 5 Proportional Final Acceptance

**Status: complete on 2026-08-14.**

Parent: [Solver Decision Provenance plan](../plan.md)

The selected acceptance row is **Q, ordering, or policy-selection change**,
together with crossed Calculator and result contracts. Raw reports are ignored
under `build/solver-decision-provenance/gate5/`; this record pins their names,
sizes, SHA-256 identities, outcomes, and qualifications.

## Focused and breadth checks

| Check | Result |
| --- | --- |
| Fresh native build | pass |
| Policy-refinement | 4,893 checks, 0 failures |
| Solver solve | 98,824 checks, 0 failures |
| Solver refinement | 302 checks, 0 failures |
| Quotient proof | 357 checks, 0 failures |
| Exact evaluator | 16,786 checks, 0 failures |
| Solver API | 2,539 checks, 0 failures |
| Calculator/result/strategy-mode web tests | pass |
| `npx tsc --noEmit` | pass |
| Release WASM rebuild and engine smoke | 28/28 pass |
| Every-base matrix | 2,811,093 checks across 979 ordinary bases, 0 failures |
| Full repository pipeline | pass: 18 ingest, 12 economy, 17 fixture, 3,464,781 native checks, 12 benchmark specs, 28/28 WASM smoke, complete non-visual web suite |

The focused API suite includes current Imprint and product Eldritch 10,000-run
controls. No routine suite was repeated between intermediate checkpoints.

## Frozen goal-realignment controls

`final-native-goal-realignment.json` and
`final-wasm-goal-realignment.json` retain exact primary, Warlord, Eldritch, and
Imprint behavior. Their semantic comparison passed 320 checks with no
mismatches.

| Raw report | Bytes | SHA-256 |
| --- | ---: | --- |
| `final-native-goal-realignment.json` | 3,916,734 | `63655b176d72a741c180b732c38528b801815e4f78bfb85b02bd193bccb758da` |
| `final-wasm-goal-realignment.json` | 22,045,558 | `0995d24bf2eac37099743870ff7877dd04d3c2ad627173a34449dccd3b7900cf` |
| `final-goal-realignment-semantic.json` | 71,108 | `2c3acc9803d7dbb8a62a0793a44d20f228e01fcc6d90bc3640b966edc0e5dc4a` |

The primary remains exact at `3745.7309340083884`. Native total wall time was
92.854 seconds and release-WASM total wall time was 147.292 seconds, below the
five-minute boundary and respectively about 58% and 47% below the frozen
Goal Realignment measurements. The compiled strategy remains byte-identical to
the frozen primary.

## Portfolio qualification and final witness

The one complete current native and release-WASM 49-case portfolio each
published every policy: 2 exact, 29 bounded feasible, and 18 resource-capped.
The comparison passed 3,729 checks with zero semantic mismatches.

| Raw report | Bytes | SHA-256 |
| --- | ---: | --- |
| `final-native-portfolio.json` | 21,476,991 | `76cc08614a6d822e985eb4c8fbef0c20f00d909e28cc16d2018a6f1f5d5ec15e` |
| `final-wasm-portfolio.json` | 43,668,560 | `d223265e72c40f680d44a449aae860763e1a06caad8572b20fa460cefbf6ca9a` |
| `final-portfolio-semantic.json` | 766,991 | `e8957f9aa1eb21a80dd208b1f09f23efd3436208a5a8bb982d610cd75110265e` |

That run produced a new **reproduced-current** soundness witness in
`natural-t1-breadth-two-f2efa568ca5e`: public lower
`278.9524669786507` exceeded a frozen executable exact strategy cost of
`274.52189255967016`. Strict projection encountered an exact successor whose
coarse parent was not selected. The optional identity-reuse probe then threw
`AdapterFailure` before the admitted-row builder could use its existing
certified fallback. Allowing that optional probe to decline reuse restored
strict exploration, which then honestly reached `max_transitions` rather than
closing. The remaining defect was result authority: the coarse lower did not
remain globally valid after unclosed strict refinement.

Checkpoint `970e133` fixes both parts. Non-resource reuse failure declines
reuse and permits the certified fallback. A non-exact result whose strict lift
ran but did not close publishes lower zero with provenance
`unclosed_strict_refinement_universal_zero`. This is deliberately narrower
than zeroing every bounded lower: existing globally valid closed-envelope
positive lowers remain supported.

The final native witness is `f2-truth-native.json` (705,101 bytes,
SHA-256
`656bb48eede071fc42aae0c70a259873366c23eb56998378ffde92cd3423832d`).
It publishes `refused_resource_cap`, lower `0`, evaluated upper
`453.49800766071047`, global-lower certification false, and the named strict
`max_transitions` reason. The final release-WASM 10,000-run report is
`wasm-verified-natural-t1-breadth-two-f2efa568ca5e.json` (1,295,177 bytes,
SHA-256
`386717fcbac28e1346749bd6b9c46320de2a5d03d542f4df7cb63de309ad1bfa`).
The native/WASM overlay passed, with comparison report SHA-256
`ca1bc9f89a73ed19d050df366eabff5b1887495deb849f844c17a56cf39a7507`.

The complete portfolio was not repeated after this final evidence-selected
repair. The correction changes lower-bound authority for unclosed strict
results and the witnessed optional reuse path; it does not change the exact
primary path. Final affected-case reports and focused semantic overlays are
used in the same manner as the accepted Goal Realignment Gate 8 correction.

## Changed-strategy verification

Seven release-WASM prepared strategy hashes differed from the frozen Goal
Realignment portfolio and were run with 10,000 requested simulations each.
All seven exact strategy evaluations matched their reported policy values and
reported zero off-policy mass.

| Case | Prepared strategy SHA-256 | Sample outcome | Raw report SHA-256 |
| --- | --- | --- | --- |
| `natural-t1-breadth-two-4e65dda9c53b` | `5c6047fafc0f85319c77841a2330cb05a35216e171b0ae3d6db6170d23644fdf` | 10,000/10,000 success | `b3477d5d12dfdd15afd37502c220a325b70f96ec6160de1597e9ade4684c4e32` |
| `natural-t1-breadth-two-b0082f83098f` | `4e69f64666c13646fac929c17b51c3921cd2b9b43637a467e682d43ccdfceeba` | 10,000/10,000 success | `3d18a33126c5dca567cd9123ff3ad6dbadc9cf578444ab03b68b4055f54e3858` |
| `natural-t1-breadth-two-f2efa568ca5e` | `37e15497dc5b993fc0435f86bb2cf60ea584123af654817d831e17ac5b2918c1` | 10,000/10,000 success | `386717fcbac28e1346749bd6b9c46320de2a5d03d542f4df7cb63de309ad1bfa` |
| `natural-t1-deep-four-dense-a64d3d932590` | `295fc17aad5bd4fe71d07c6e21143d01b9b8a886354e4a0974e86bc84b1da901` | 16 success; 9,984 action-limit stops | `079818059e53369bb841092355254e86dbc663343a75d765583b0d266dd65350` |
| `reliability-class-sceptre` | `50604a5f5ca977e4b308a19ebf65e599e121d81637cae5af6b10a21e75e96461` | 10,000/10,000 success | `a09e4d0e329b6c1a19a05766e16d5aa851ae5c98d37327026dbcfc1526fb9f6a` |
| `reliability-class-staff` | `6dc07e21a33ca76bf50ea3cb639ab47e20dec1c577acb421521fef161b721bb8` | 10,000/10,000 success | `7af0c8263b8dea0f87ecf2a710782ca14c9287865e577ad2bf48a5b5dc245263` |
| `reliability-start-influenced` | `1dbae077af9b3a137dd784f2a3decb0a832040a99f858b6ee1d25e65f65f5d1f` | 10,000/10,000 success | `f834aba02bb742e02559c3d053d1daf03ffbfd7f8dc844e9542695c098ddb5e9` |

Aggregate sampled outcome: 70,000 requested, 60,016 successes, 9,984
failures, all failures from the unchanged 100,000-action Simulator ceiling,
and zero off-policy failures. The deep strategy executed 999,238,392 sampled
actions before those horizon stops; its exact graph evaluation remains
successful with probability one.

The benchmark process currently returns nonzero for these reports because an
explicit `--verification-runs 10000` override is still compared with the
ordinary 100-run fixture expectation. The JSON counters prove the requested
10,000 runs occurred. The mismatch is a harness reporting issue, not a solver,
strategy, or Simulator semantic failure, and was not expanded into this
milestone.

## Evidence classification

- The Calculator envelope leak and the `f2` lower-bound witness are
  **reproduced-current**.
- Shared Q, finite-order, closure-precondition, and public lower-provenance
  findings are **source-confirmed** and covered by focused tests.
- Frozen Goal Realignment controls are **archived-qualified** and replayed
  where selected by changed reach.
- The historical Fossil-to-Chaos request is unavailable. Its cause remains a
  **hypothesis**, not a claimed localization.

No cap was raised, exactness was not weakened, no mechanic changed, and the
mixed-action seven-step follow-up was not started.

## Final pipeline disclosure

The first full-pipeline attempt passed the Python/data/artifact layers and
reached 3,464,781 native checks, where it found one stale S8.3 assertion. The
test required a lower-variance blocker to win an exact cost tie, which was the
superseded pre-Gate-2 behavior. The accepted contract uses stable row identity
for exact equality and does not introduce variance as a second optimization
objective. After updating that assertion, the focused S8.3 suite passed 525
checks with zero failures.

The post-fix final pipeline then passed end to end: 18 ingest tests, 12 economy
tests, canonical database validation, six fixture selections, compiled
artifact validation, 17 fixture tests, 3,464,781 engine checks, 12 solver
benchmark specifications, 28/28 WASM smoke checks, and the complete
non-visual web suite. Both invocations are disclosed; only the post-fix run is
the accepted final state.
