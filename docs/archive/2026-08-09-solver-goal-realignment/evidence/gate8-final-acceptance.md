# Gate 8 Performance And Final Acceptance

**Status: complete on 2026-08-13.**

Parent: [Solver Goal Realignment plan](../plan.md)

The tracked machine-readable record is
[gate8-final-acceptance.json](gate8-final-acceptance.json). It pins the final
measurements and SHA-256 identities of the ignored raw reports under
`build/solver-goal-realignment/gate8/final/`.

## Final sequence

The final release sequence ran in the plan's required order after focused
qualification was green:

1. the native tree built;
2. release WASM rebuilt with `-O3`, LTO, SIMD, WASM exceptions, and the default
   allocator/growth behavior;
3. the complete repository pipeline ran exactly once and passed;
4. the primary case passed natively and in release WASM;
5. the five-mod case returned the required bounded executable result;
6. the every-base matrix passed all 979 ordinary bases;
7. selected 10,000-run Veiled, Eldritch, Imprint, Warlord, and primary
   verification evidence passed; and
8. focused and portfolio native/WASM semantics matched.

No rendered or visual review was performed.

## Binding outcomes

| Case | Native | Release WASM | Exact/evaluated result | Sampled verification |
| --- | ---: | ---: | --- | --- |
| Four-natural-T1 primary | 218.703 s total | 277.906 s total | exact `3745.7309340083884` | 10,000/10,000, zero off-policy |
| Warlord control | qualified | 7.697 s total | exact `224.1238588972487` | 10,000/10,000, zero off-policy |
| Eldritch | 100.180 s total | 156.219 s total | exact `0.018630169563331064` | 10,000/10,000, zero off-policy |
| Imprint | 0.588 s total | 4.706 s total | exact `252.6535202127448`; exact 3:1 rare/Croaker use | 10,000/10,000, zero off-policy |

The release primary used 68 cooperative solve steps. Solve, compilation, and
verification consumed 151.270, 10.861, and 111.900 seconds respectively, for
277.906 seconds total, under the required five-minute boundary. The observed
WASM heap grew from 278,396,928 to 577,437,696 bytes. Native and release WASM
returned the same cost and public semantics.

The representative five-mod Runic Gauntlets case stopped on the named resource
cap after 22.104 seconds. It retained an independently evaluated policy upper
of `0.01165`, lower `0`, a strict nonzero gap, and 28 open incremental
obligations. The cheapest independently evaluated candidate was authoritative,
and its 10,000 sampled runs all succeeded with zero off-policy failures. This
is the exact bounded result required by the plan, not an exact-optimality claim.

## Repository and breadth acceptance

The single complete `scripts/test.ps1` invocation passed 18 ingest tests, 12
economy tests, 17 fixture tests, canonical artifact validation, 3,464,752
engine checks with zero failures, 12 benchmark specifications, 28/28 WASM smoke
checks, and the complete non-visual web suite.

The lightweight every-base matrix built all 979 ordinary sessions and found
modifier pools, feasible goals, complete prices, and passing compiled
evaluation for every one. Focused native/WASM comparisons for primary,
Warlord, Eldritch, and Imprint each passed 96 checks, 384 total.

## Single portfolio qualification

The native and release-WASM 49-case portfolios each ran once. Both published
49 policies with zero watchdogs, report errors, or off-policy failures. Forty-
two sampled expectations passed directly. Seven long-renewal policies exceeded
the 100,000-action sampled run ceiling; their exact compiled evaluation still
proved success probability one and zero off-policy mass. Their 100-run samples
failed only on `action_limit`, not on a semantic or routing error.

The first comparison exposed a benchmark-runner contract defect: release WASM
was not receiving the case's absolute and relative optimality-gap options, so
it continued to exact closure where native intentionally stopped bounded. The
runner now forwards both fields. The complete portfolio was not rerun; only the
30 affected release-WASM cases were corrected and overlaid on the original
aggregate. The final comparison passes 4,072 checks over all 49 cases.

One case reached different internal resource boundaries: native reached the
1-GiB selected-owned-byte cap before release WASM reached the 20-million
reforge-work cap. Public policy, value, compilation, exact evaluation, and
simulation semantics matched. Structural telemetry is therefore compared only
when both runtimes report the same internal resource-stop set; the comparator
continues to require all public semantic checks.

## Acceptance conclusion

The milestone completion contract passes. The primary is exact with an
independently evaluated compiled policy inside five minutes in both native and
release WASM. Relevant families have engine-owned candidate, dependency, or
exclusion reasons; automatic Veiled, Eldritch Exalt, and priced Croaker Imprint
routes work end to end; every supported ordinary base passes; Ring/Amulet and
the five-mod bounded publication contracts remain intact; and compiler,
evaluator, Simulator, worker, and non-visual product semantics agree.
