# 3. One retained-artifact boundary, not another solver framework

**Status:** proposed M2 design. Names in the conceptual interface table below describe operations to implement or reuse; they are not claims that these APIs already exist.

## 3.1 Current implementation and the actual seam

The reviewed `IncumbentPortfolio` already contains output, pending and retained storage, but its retained vector is reachable through `SolveWork::Impl::certified_fallback_portfolio`. Current construction code admits, replaces, sorts and prunes it, and publication directly takes and erases entries. `best_current_certified_fallback()` is a mutating operation despite looking like a lookup. The source intentionally distinguishes retained provenance validity from full independent verification. [R12](SOURCES.md#r12), [R13](SOURCES.md#r13), [R14](SOURCES.md#r14).

The target is the **retained pool's lifecycle**. Do not create a second pool, a universal policy registry, or a class that merely forwards unrestricted `Impl&`. Existing proposal builders may still own staged candidates; the evaluator still supplies verification; the publication pipeline still seals the result. The retained-pool owner stores and transitions their outputs through a narrow contract.

Before editing, enumerate every current writer. The source excerpts establish the issue, but do not substitute for a complete local caller search at implementation time. Include tests, telemetry readers, private assertion handoffs, memory estimation and finalization paths, not only the named methods below.

## 3.2 Preserve the distinctions that are easy to accidentally erase

### Retained does not mean independently verified

`retain_certified_incumbent()` currently checks `retained_incumbent_invalid_reason`, so its accepted population can include a compiled materialized candidate awaiting final graph evaluation. Full publication eligibility also requires independent certification/evaluation, properness, executability, finite nonnegative cost and the existing reconciliation contract. A refactor must represent both populations; it must not drop the first because of the method's name. [R13](SOURCES.md#r13).

Use the existing role information to distinguish a retained proposal from a verified deliverable. A private helper or a small tagged result may clarify this. Do not add one giant enum that discards simultaneously relevant facts, or let an arbitrary Boolean assignment mint a certificate. The full check remains attached to the existing producer/checker and context.

### Retention ordering and final output selection are different questions

The current storage order uses `certified_upper_bound`, root operator, kind, witness identity and portfolio identity. The bounded pool holds at most four entries, deduplicates by its current identity, and can return `true` for a noncompetitive fifth candidate without storing it. That return means the request was handled, not necessarily that new storage exists. [R13](SOURCES.md#r13).

Preserve that behaviour. A proposed internal result such as `stored`, `already_present`, `handled_not_stored`, `invalid`, or `resource_refused` can make the distinction testable, but the caller migration must map each outcome deliberately. Do not accidentally turn `handled_not_stored` into a resource stop or retry trigger.

Do not replace storage ordering with evaluated-cost ordering while claiming neutrality. The final publication owner's selection among valid ready artifacts has its own existing semantics. A later improvement to retention competitiveness would be a separately compared algorithm treatment.

### A complete artifact is more than a price and a graph name

Maintain the graph bytes, certification bytes or their existing identity relation, full root certificate, native goal/action/price/mechanics context, applicable generation/prefix provenance, verification status and evaluated original cost as one supported combination. A private root-only result does not supply arbitrary parent-state policy rows or statewise values. Its existing validation explicitly rejects that contamination. [R13](SOURCES.md#r13), [R16](SOURCES.md#r16).

A historical best verified scalar is an observation, not an owned deliverable. Progress can report it as historical only with that label. Availability must refer to a currently owned, compatible artifact or complete handoff permitted by the current control path.

## 3.3 Conceptual interface and mutation authority

Integrate these responsibilities into the existing owner. The exact C++ spelling should follow current code and avoid additional hot allocations.

| Operation | Inputs / output | Mutation and validation contract |
|---|---|---|
| Retain a materialized candidate | Candidate plus explicit current context and available memory projection → disposition | Existing provenance validation, identity deduplication and four-entry replacement rules; failed admission leaves the old pool intact |
| Attach completed evaluation | Identified candidate/generation plus the existing complete assertion/evaluation output → updated role or refusal | Reject stale candidate identity or mismatched graph/context; partial output never promotes availability |
| Prune invalid retained entries | Explicit current authority context → removals/refusal reasons | Called at an intentional existing service boundary, not by presentation; preserve the current reason distinctions and counters |
| Read retained/available summary | Const validated view or explicitly versioned summary → IDs, role, value, availability | No pruning, compilation, evaluation, scheduling, lazy model construction or graph-sized allocation |
| Select/take for publication | Existing comparison policy and current eligibility → owned candidate bundle or none | Selection and removal are one owner operation; no pointer surviving erase, reallocation or a suspension |
| Account retained storage | Const owner state → compatible dynamic/static ownership contribution | Uses real capacities and existing accounting semantics; shared transitive objects cannot be silently omitted or counted twice |

This is a design boundary, not a demand to introduce six public methods or six new types. Reuse the current validation helpers. A generic callback that hands the entire mutable pool back to the caller would defeat the goal.

The owner may receive a small immutable context assembled by `Impl`. That context must include exactly the provenance needed by the existing checks, not a weak substitute such as a source hash alone. Avoid rebuilding graph-prefix or price identities on each progress poll. If a validated eligibility view is retained, specify exactly which changes invalidate it and mark stale state unavailable/unknown until refreshed at the correct owner boundary. Do not create a general cache project under this heading.

## 3.4 Migration shape

First isolate the tests for admission, provenance eligibility, comparator ties, bounded replacement, explicit pruning and take-out. Characterize the existing “handled without retaining” outcome and both retained roles before changing names.

Then route existing retained-vector mutations through the owning operations. Preserve when the operations are invoked, what they charge, which diagnostics they emit and what their return values mean. Remove the mutable retained-vector alias and direct external insert/erase/sort/move access once all selected callers have migrated. A `mutable_entries()` accessor or broad friend access to the same vector is not completion.

Keep initial proposal construction and completed private assertion production separate. Completed producer output may be moved into the pool only through its real eligibility/ownership boundary. An incomplete coroutine does not become complete because Finish was requested. The current Finish path already takes complete evidence and interrupts optional unfinished work; do not reintroduce speculative capture or compilation after acknowledgement when a fallback exists. [R14](SOURCES.md#r14), [R16](SOURCES.md#r16).

Finally switch passive readers to the owner view and replace publication's direct minimum/move/erase sequence with the controlled transfer. Final normalization, invariant checking, cap classification and sealing stay in `PublicationPipeline`. Do not create a second “sealed” result authority inside the pool.

## 3.5 Lifetime, failure and identity obligations

**No borrowed-vector handles across suspension.** A const pointer into a vector is still invalidated by erase or replacement. Retain an owned bundle, or use an existing stable identity with a checked generation and reacquire it at a safe point. A new ID counter is not itself proof of semantic compatibility.

**Transitive immutability matters.** A `const` outer struct is not sufficient when nested shared pointers or aliases still permit mutation of graph/certificate storage. Inventory that aliasing before promising an immutable verified view. Do not convert everything to shared ownership to solve this superficially; doing so changes memory/lifetime assumptions.

**Failed replacement preserves a useful owned witness.** The existing output replacement path attempts to retain displaced compiled evidence and can refuse an unverified preferred candidate under memory pressure. Preserve that protection. Preparation, candidate copies and old/new overlap must be admitted before the old witness is destroyed. Do not silently defer the cap check until after moving the only valid artifact away. [R13](SOURCES.md#r13).

**Context change invalidates eligibility, not historical evidence.** A changed goal, price, permitted scope, graph prefix, artifact generation, root item or certification relation must not reuse old eligibility. An old artifact can remain historical data without being usable in the new query. Report the distinction rather than rewrite its saved identity.

**Logical passivity and physical overhead are separate.** A read can leave the pool unchanged but still cost enough to change a wall-time-limited solve. Check both semantic read purity and bounded observation overhead. Adding a deep validation scan to every progress event is not an acceptable replacement for mutating reads.

## 3.6 The four-pointer accounting trap

Current telemetry/accounting defines:

```cpp
constexpr std::uint64_t kIncumbentPortfolioAliasAccountingOffset =
    4 * sizeof(void*);
```

The comment ties this subtraction to compatibility references, not to independent candidate payload. [R15](SOURCES.md#r15).

Map each of the four compensated shells to its actual field before changing the layout. Removing one alias does not justify removing all four compensations. Conversely, keeping the old deduction after the corresponding field disappears can make a ledger too small. Reconcile the changed `sizeof` contribution and the explicit adjustment together, in every fast/full accounting route that consumes them.

Retain accounting for all real candidate vectors, compiled strings, certificates, in-flight assertion output, coroutine frames and old/new overlap. The purpose is to preserve the resource contract, not to force the byte counter to the same numerical value after real storage has changed. Explain actual structural-byte deltas separately from dynamic payload and semantic work. Cumulative logical work never decreases when scratch is released.

No success claim may come from a cap increase, omitted frame/payload, unexplained alias deduction, or a changed accounting population.

## 3.7 Acceptance matrix

| Fixture | Required result |
|---|---|
| Retain a compiled candidate that has not completed independent evaluation | It remains retained with a nonverified role; availability stays false |
| Attach the matching complete assertion | Role may become available only after all existing checks |
| Attach an assertion for another graph/context/generation | Refused; no mixed bundle is observable |
| Retain duplicate identity | Existing deduplication result, no extra payload charge |
| Offer a noncompetitive fifth candidate | Existing handled/not-stored semantics; no new stop or retry |
| Equal ranking keys / stable ties | Same existing deterministic order and winner |
| Repeated progress reads | Same pool, cursor, counters and authority; bounded memory/time |
| Explicit prune on an invalidated context | Removal/availability/counters follow the mutating owner only |
| Take candidate and mutate the pool afterward | Transferred object remains valid; no dangling borrowed pointer |
| Allocation/cap refusal during replacement | Old compatible fallback remains owned and deliverable |
| Complete private evidence just before sealing | Eligible evidence is considered through the existing handoff |
| Partial private evidence at Finish | Interrupted without publication; spent work retained |
| Cancel competes with Finish / stale invocation messages | Existing precedence and one terminal result are preserved |
| Root-only candidate | No parent-state authority, fabricated rows or foreign layout IDs |
| Alias-shell removal | Fast/full ledgers agree with the documented population; near-cap behaviour is not improved by undercounting |

These tests establish the selected boundary. They do not prove all implicit SSP mechanics, every numerical certificate or unlimited scheduler progress. [Mathematical limits](05_RESEARCH_AND_MATHEMATICS.md).
