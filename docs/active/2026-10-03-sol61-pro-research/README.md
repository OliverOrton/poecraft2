# Conquest and armour source snapshot for Pro review

**FINITE_NATIVE_QUERY_AND_BLOCKER_CONTROLS_PASSED.** Whole-product, performance
and WASM qualification remain unrun. Review branch
`dot/pro-sol61-conquest-armour-20261003` descends from released main
`29d9e666b9d11dbe9ba58959f2598df9495dc49d` through the initial source-only snapshot
`3dbee30bcb3ac6c12f120a7aa9f931227231c45b` and its test-only budget-fixture correction.
It applies only the eleven explicit native source/test paths from the approved
fixed-attempt and finite-query patches. Main is unchanged. This branch has no
new compiled artifact, production proposal activation, data/price refresh or
normal published policy. Its inherited main WASM does not match the new source.

The two original patch SHA-256 identities and initial source pins are in
[publication-source-pins.json](publication-source-pins.json); the test-only recheck
is pinned in [query-r2-publication.json](query-r2-publication.json). Initial apply order is
`fixed-attempt-r2-source-only.patch`, then `eldritch-query-r1-source-only.patch`.
Both applied cleanly to the released baseline. Nine source pins match the owner
snapshot exactly; `test_main.cpp` and `tests.hpp` additionally retain main's CI
API-test selector fixes. These contexts define a new combined snapshot.
The excluded commit `66cca8f587f5eb85c91122b7c1b01e278bf312c5` is not a parent or
ancestor. The complete baseline tree is inherited with only named allowed file
replacements; the protected root entry is neither inspected nor changed, and
the original diagnostic checkout and index are untouched.

## Qualification and historical evidence

The owner completed the bounded native recheck before the quiet window:
**2,584 finite query checks, 171 blocker-growth checks and 81 native-entry checks,
zero failures**. A two-job native test build passed in 18.71 seconds. The
[recheck receipt](query-r2-summary.json) pins source and binary identities, phases,
budget units, zero survivors and the limited qualification scope. No checks were
rerun during publication.

The original finite query run failed three of 2,579 assertions because its
fixture capped the ordinary child ledger while automatic admission used a
separate ledger. The [preserved initial failure](query-r1-failure-evidence.json)
retains results, failure expressions and artifact hashes. The test-only correction
proves the separate unowned ledgers, attaches an independent shared owner already
exhausted at one unit, verifies refusal with unchanged committed work and operator
rollback, then verifies an uncached retry charging that owner. Tolerances and
production source are unchanged. No production cap-enforcement bug was established.

New real-policy evaluations/performance, WASM rebuild, whole-product/web checks
and hosted qualification of this corrected source remain **UNRUN**. The inherited
main WASM still does not match the query source. Proposal three remains disabled.
Source-only publication checks cover patch identities/scope, application, final
source pins, bounded receipt JSON, whitespace and credential patterns. Historical
source-review and publication receipts retain their original unqualified status;
the recheck receipt is the current authority for the focused native gate only.

The byte-preserved initial [owner source review](source-review.json) describes finite
query intent identity, query-first/full-first behavior, scope refusal, checkpoint
restoration, cancellation, retry and ledger fixtures. Completion authorizes only
the requested finite query, never the broader action envelope. Native laws,
original goal/root/scope, exact positive-entry validation, prices and resource
obligations remain the required authorities.

The two-file fixed-attempt patch had earlier reported 171 growth and 81 native
checks plus a successful native test build. The preserved
[pre-query results](fixed-attempt-pre-query-results.json) record that older run.
These checks are not results for this branch or the later query patch.

The [pre-query Conquest receipt](conquest-pre-query-summary.json) reports a checked
feasible upper of 99095.4091604038, 190 positive entries and 15570088 validation
work. Peak native bytes are unmeasured, not zero. The graph was a diagnostic
historical import; this is no normal-search publication, exact-closure extension
or new optimality/financial claim. Its numeric delta remains diagnostic.

The [four armour results](armour-first4-scalar-summary.json) contain one checked
body alternative at 31591.632275626907 and three failures to deliver an
alternative: boots reached the reforge-work cap, gloves reached memory/work
limits, and helmet validation reached aggregate capacity. Their retained Chaos
fallbacks remain explicitly classified. No case has a matched historical
baseline at the same full case/source/cap identity; success probability one
establishes execution/pricing rather than a quality pass. These are older
released-source diagnostic runs, not query-patch measurements. Full goals, starts,
caps, runtime and historical source/binary pins are preserved in
[historical-input-identities.json](historical-input-identities.json).

## Ownership question for the next review

The existing entry validator borrows `const StrategyPolicyEntryCertificate&`
from the checker's live result census. Moving that result after validator
construction would leave the validator attached to a moved-from object;
resetting the checker would dangle the reference. Any future cleanup must first
own the complete immutable root result, then construct the validator against
stable owned census storage and retain it through validation/publication, with
complete provenance, resource, output, cost and aggregate memory accounting.
This query patch changes none of those ownership paths.

No heavy process is held or queued by this publication. The October 3
America/Vancouver wind-down is 16:45 and the quiet window is 17:00-22:00.
Further qualification requires the parent's serialized LOCAL grant.

## Hosted CI visibility and quiet-window checkpoint

The latest read-only check at publication still reports main commit `29d9e666`'s
[Windows run 37155615109](https://github.com/OliverOrton/poecraft2/actions/runs/37155615109)
as Test-active after Build passed. Python/native/web progress inside that step
is unknown; the previous authenticated log fetch returned `404 BlobNotFound`.
No green or stall conclusion is supported. One monolithic Test step and no
explicit job/step timeout leave an observability gap. Future review should choose
finite limits from measured evidence and expose stage completion, elapsed time
and bounded partial logs while preserving frozen identities, serial order,
native cancellation, cleanup and failure receipts. No workflow change,
cancellation or rerun is included here.

Publication during October 4 00:00-05:00 UTC (October 3 17:00-22:00 PDT) performs
lightweight Git/docs work only. No local build, test, timed solve or browser
process is started. Original failing evidence, main and protected metadata remain
unchanged. Real Conquest/armour performance is unmeasured for the corrected query
source, and its finite passing gate must not be presented as a performance win.
