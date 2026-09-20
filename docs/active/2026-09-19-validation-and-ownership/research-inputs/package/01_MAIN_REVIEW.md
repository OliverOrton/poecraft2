# 1. Current-main review and decision

**Pinned source:** `d13c9b835186ce35cb51aef0de6d0027f8b8a1a7`, tree `cc6d4bd642bcefb422158d7cefc51ade34d5a63f`. The commit is “Deliver request-scoped Finish and frozen solver progress exports,” dated 19 September 2026, with parent `4594e44b6b851511ae5471e52f232bce57d16856`. The old structural audit was against that parent. [R01](SOURCES.md#r01).

## 1.1 What the long implementation actually delivered

The A programme repaired actual Calculator export binding and froze the submitted request. It added invocation-scoped Finish through the existing client/worker/native path, transferred complete private assertions with their root certificates, preserved cancellation precedence, and sealed a complete compatible verified artifact before delivery. That is meaningful lifecycle and product work, not a new solver optimum. [R04](SOURCES.md#r04), [R05](SOURCES.md#r05), [R16](SOURCES.md#r16).

The recorded actual Calculator Finish-to-usable interval is **5.483 seconds**, passing the declared 10-second gate. Worker RPC intervals of 0.879/0.974 seconds are different measurements: they do not separately timestamp graph usability. The actual component probe uses linkedom, the real component/client, a Node worker and WASM; it is not a rendered-browser review. [R05](SOURCES.md#r05), [R36](SOURCES.md#r36).

The positive policy evidence remains valuable. Native Bow, Ring, Amulet and Conquest-four retain their reference bytes, original-cost results and primitive counts. Conquest-five remains at **85558.70618560436 Chaos** and **8407.202314771383 expected primitive actions** across the four recorded worker arms. Regalia remains exact at **65.60036144971359**. The public progress structure remains **200 bytes**. These statements preserve their recorded profiles; they are not a claim that the same policies are discovered under every browser default. [R05](SOURCES.md#r05), [R08](SOURCES.md#r08).

The Calculator graph comparison is a specifically declared semantic comparison: the report removes only its attached economy identity, then checks full nodes, edges, base, scope and annotations, together with ordered goal/action scope and frozen prices. Do not retell this as raw file-byte equality. [R05](SOURCES.md#r05).

## 1.2 The current failures, with their original thresholds

| Contract or observation | Recorded result | Current disposition |
|---|---|---|
| Finish intent to usable graph and cleanup, at most 10 s | 5.483 s in the actual Calculator probe | Qualified in that probe |
| Usable graph within 65 s of worker start | Worker native export alone takes 70.160/69.462 s; Calculator request-to-ready is 75.925 s, including 60 ms before worker request | Failed; do not mix the clocks |
| Initialization at most 250 ms | Conquest begin 18.17–18.73 s | Failed |
| Setup cancellation to released resources at most 1 s | Actual Calculator 23.081 s | Failed |
| Stepped call at most 250 ms | Conquest maxima 568–660 ms; Conquest-four reaches 1111 ms before a ready fallback | Failed |
| Conquest-four WASM against cheaper reference | WASM C5218.04094969; native reference C3746.13194095 | Unqualified gap; cause not established |
| Broad certified exact closure | No new closure established by this delivery programme | Open |

**All five time thresholds above are already present in the current A disposition.** The older audit warned that step timing alone did not cover startup. The new programme has since measured and explicitly failed initialization and cancellation gates too. They must not be described as newly proposed by this package. [R05](SOURCES.md#r05).

The setup cancellation arm ran zero search steps: begin consumed 18.346 seconds and abandonment/cleanup added 4.813 seconds. The issue is not merely a late search sweep. The cost gap on Conquest-four is also separate from startup latency; a matched native B/A comparison preserves the cheaper graph but does not explain the WASM difference. [R05](SOURCES.md#r05).

## 1.3 Setup is a deliberately bounded stop, not a failed cooperative implementation

The setup disposition records **zero cooperative-conversion variants**. Its A5 begin of 12.33619 seconds consists of about 8.08896 seconds of goal cover and 4.24431 seconds of retention preparation. A later manual arm records 11.747/6.673 seconds. Cross-build timing differences are not isolated effects of an algorithm change. [R06](SOURCES.md#r06), [R05](SOURCES.md#r05).

The reported goal-cover owner sets its ready flag before synchronous table/contract construction, and existing consumers/snapshot paths may invoke preparation. Inserting suspension points without staging and removing read-triggered reentry risks exposing incomplete lower evidence. Retention is a second synchronous probability/checking owner. Even eliminating one owner leaves seconds of uninterrupted work. The previous programme declined a two-owner conversion because it exceeded its chosen one-owner repair, not because the conversion is impossible. [R06](SOURCES.md#r06).

The follow-on should therefore be intentionally scoped across both owners, rather than another request to “move initialization to the first step.” This package queues that work after validation and one ownership consolidation.

## 1.4 The two Ring probes are finished; neither selects implementation

The B record compares two saved Ring controllers, C227377.06545019808 and C220743.46354691702. Their raw evaluator pair counts are 185337/185344, refined counts 10994/11001, and cold checks about 10.022/9.999 seconds. Those are existing fixed-policy diagnostics, not fresh discovery or a demonstrated incremental-checking speedup. [R07](SOURCES.md#r07).

The observer diagnostic finds different local requirements, but both inspected layouts still have 109 junk classes and 30 count observations. Whole-vector observation requirements do not prove a particular count discriminator can be removed from the private constructor. No qualified class reduction has been established by this probe.

The response diagnostic has syntactic/routing comparisons but not the native aligned overlap, boundary size, fill or retained-memory evidence needed to select a cache. A difference of seven states is not evidence that only seven states changed. Its second component-build plus solve phase is about 2.505 seconds; making that phase free would not remove approximately ten seconds of total evaluation. That narrow ceiling is not a proof against broader reuse, but neither is it evidence for implementing it now. [R07](SOURCES.md#r07).

**Disposition:** retain both results. Reopen the observer adapter only after an actual native discriminator reduction is demonstrated; reopen response caching only with native aligned overlap and a useful budgeted interface. Do not repeat these same probes merely because a new session lacks their history.

## 1.5 The new main still fails both hosted checks

The Windows push run **35465746330**, job **105957628596**, built successfully and then failed in the first Python test stage: **35 tests, four failures, twelve errors**. The logs show missing pytest imports, absent production compiled-data inputs, raw-hash mismatches, and execution by Python **3.14.7** despite a **3.12.10** setup. Native and web tests in that workflow were not reached. The independently recorded local qualifications are not invalidated by this CI infrastructure failure; nor do they make that CI run a pass. [R31](SOURCES.md#r31), [R20](SOURCES.md#r20), [R21](SOURCES.md#r21).

The knowledge push run **35465746343**, job **105957628688**, passed lint with zero errors and passed eight knowledge tests, then failed one of eleven focused pytest tests at `report["observations"][0]["sources"][0]`. The intended record has id `support`; the first record is now a valid conditional observation. Later generated-report checks were not reached. This is a positional assumption, not a rejected mathematical argument. [R32](SOURCES.md#r32), [R23](SOURCES.md#r23), [R24](SOURCES.md#r24).

These are new-main failures, not merely historical findings. The compact extraction is in [ci_observations.json](evidence/ci_observations.json). Logs were read but are not reproduced wholesale in this packet; no digest of an unretained raw log is invented.

## 1.6 Two fresh small checks narrow the repair

The exact current Git blob for `evaluator-straight.strategy.json` has SHA-256 `71bd8aea350928a3da87103b7d78c454b9e024cb5502fb3af66c9ad2987e333f`. Replacing its 35 LF endings with CRLF gives `fd46a2e64f3e75aeff0b6086ff7516d6214506ced3ba58d3974d244b03d015eb`, exactly the reported Windows failure. Parsed JSON is equal. This explains **one fixture**, not every hash failure. [R33](SOURCES.md#r33), [fresh results](evidence/reference_checks.json).

An isolated test file containing a passing TestCase method and a deliberately failing top-level test makes unittest report a pass with one collected test; pytest collects both and exposes the intended failure. Installing pytest does not fix a collector that ignores those functions. This is a small demonstration, not a count of missing repository tests. The official interoperability contract supports using pytest for ordinary TestCase tests but does not support `load_tests`; inspect actual repository hooks before migration. [P01](SOURCES.md#p01).

## 1.7 The remaining ownership debt is concrete

`IncumbentPortfolio` still exposes mutable storage through compatibility aliases. `best_current_certified_fallback()` prunes the retained vector while acting as a getter, and the publication coroutine still directly selects, moves and erases retained elements. The functional Finish fix does not complete encapsulation of this lifecycle. [R12](SOURCES.md#r12), [R13](SOURCES.md#r13), [R14](SOURCES.md#r14).

Three details must shape the design:

- The retained pool legitimately contains materialized candidates awaiting final checking as well as verified ones. Enforcing “every retained entry is verified” would change the search/publication pipeline.
- The existing storage comparator is certificate-upper, root operator, kind, witness identity, then portfolio identity. The pool has a four-entry bound. Changing those choices is not structural equivalence.
- The ledger subtracts `4 * sizeof(void*)` for compatibility-reference shells. Removing an alias without reconciling the matching static offset can undercount; removing the entire offset while aliases remain can overcount. Audit both fast and full accounting. [R13](SOURCES.md#r13), [R15](SOURCES.md#r15).

The selected change is therefore **one retained-pool admission/prune/read/transfer boundary**, integrated into existing owners, not a new portfolio alongside the old one. Final result sealing remains the publication owner's job.

## 1.8 Decision and uncertainty

Repair validation first, then finish that one ownership boundary, qualify it and stop. Do not spend this interval on a universal writer, every duplicated utility, the whole `Impl`, the action-ledger scheduler migration, LTO, all headers, or another solver technique.

The unresolved production-data provisioning route is a real planning dependency: the repository's current tests require ignored generated data. The executor must resolve a pinned, authorized, reproducible route, or report the blocker. A tiny fixture is not the real corpus and a remembered hash is not the missing data. This review does not assume a public dataset download or authorize publishing local data.

The review is targeted. Full old/current diff coverage, exhaustive current complexity counts, fresh native/WASM timings and a profile are not claimed. The selected milestones derive from the explicit current evidence above; other historical debt remains a revalidation backlog rather than asserted new-main facts.
