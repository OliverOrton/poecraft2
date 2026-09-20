# 7. Queued next programme: cooperative setup with honest runtime parity

**Status: queued design brief. Not automatically authorized by CODEX_PROMPT.md.**

After M0–M3, this is the recommended next performance target unless new evidence exposes a more urgent correctness blocker. It is selected conceptually now so the structural interval has an endpoint, not so it grows into another open-ended implementation.

## 7.1 Why this outranks a new cache or solver heuristic

Current A already fails initialization ≤250 ms, setup cancellation-to-release ≤1 s, ordinary step ≤250 ms and usable delivery within 65 s of worker start. A7 fixes Finish-specific work, but the largest uninterrupted setup calls remain. The setup disposition identifies both goal-cover preparation and retention preparation; converting only one cannot plausibly satisfy the stated gates. B's two completed Ring probes select neither representation adaptation nor response caching. [R05–R07](SOURCES.md#r05).

This is not a newly imposed latency policy. These thresholds are in the completed programme's current disposition. No quantum adjustment or JS `await` can preempt a synchronous native call. The existing staged-work argument is the mathematical basis for the conversion, not evidence that it has happened. [R06](SOURCES.md#r06), [R17](SOURCES.md#r17).

## 7.2 P0 — Classify Conquest-four's WASM gap before misdiagnosing it

Freeze one exact request, original prices, data, permitted scope, goal order, start item, binary identities and resource profiles. Use the existing provenance and report owners. The recorded native matched B/A controls both return the cheaper reference; the WASM observation returns C5218.04094969 and has no ready fallback before automatic finish. This does not establish whether the gap is scheduling/cap exposure, activation, compilation, arithmetic or evaluation. [R05](SOURCES.md#r05).

First compare independent evaluation of the same saved complete graph under genuinely matching native and WASM inputs, without launching another search. Keep evaluator limits explicit. If one runtime refuses or values that identical graph differently beyond the existing contract, investigate that semantic/numerical/binding boundary before performance refactoring. A refusal at a smaller allowed capacity is not automatically semantic disagreement.

If the same graph evaluates compatibly but discovery differs, compare actual first-policy service, selected rows, stop owner and logical work. Wall-time budgets alone do not isolate algorithmic parity across different runtimes. Record a discovery/capacity gap instead of pretending the saved graph was discovered. Do not require an unexplained search-quality gap to halt all future responsiveness work indefinitely once semantic compatibility is established.

**Gate:** a correctly scoped classification or an explicit unresolved counterexample. No raw source-value ratio is accepted as a diagnosis, and no higher WASM cap or earlier special-case candidate is slipped into a parity comparison.

## 7.3 P1 — Define a two-owner staged setup contract

The actual inspected/disposition owners are `prepare_goal_cover_cost` in `solver_solve_bounds.cpp` and retention construction/checking in `solver_phase_probability.cpp`, reached during begin. Identify every getter/snapshot/bound consumer capable of triggering preparation and every ready flag/pointer that exposes results. The next executor must read the full relevant functions and their nested probability calls before writing the state machine. [R06](SOURCES.md#r06).

Proposed lifecycle:

```text
cheap request validation
  → pending setup with reserved ownership
  → staged goal-cover work
  → complete goal-cover commit
  → staged retention/probability work
  → complete retention commit
  → ordinary search admission
```

This is a conceptual dependency order, not permission to change the current preparation order. Preserve actual required ordering and any independently publishable existing lower components. Do not publish unfinished aggregate tables, normalize partial probability mass, or let a snapshot force synchronous completion. Readiness must describe a completed object, not merely that construction started.

Define where invalid input is rejected immediately, which expensive validation is deferred and how its error is reported, and how cancellation stops the pending invocation. A generic exception catch must not turn cancellation into continued solving or a mathematical refusal into an empty successful table.

**Gate:** actual producer/consumer/readiness and error-timing map for both owners, with concrete staging/commit points. A wrapper around a still-blocking nested probability routine does not pass.

## 7.4 P2 — Convert setup and release in bounded stages

Implement cooperative continuations in the existing owning system, with stable cursors and complete-result commit. Include nested work that otherwise exceeds the call budget. Move or release scratch through its actual owner. Reserve before allocations and transfer pre-ledger proof ownership into normal accounting exactly once, without an uncharged gap or double charge.

Changing the suspension granularity may change timing and memory; it must not change the underlying probability law, action coverage or published lower validity. Cumulative logical work is not refunded when cancellation releases staged storage. Record source work versus host elapsed time separately.

The 23.081-second setup cancellation observation includes both begin and 4.813 seconds of abandonment/cleanup. Fixing begin alone may still fail cancellation-to-release. Reclamation and destructor work therefore belong to this deliberately wider programme. A UI acknowledgement without released worker/native resources is not the endpoint. [R05](SOURCES.md#r05).

Do not require a child coroutine per primitive outcome or move hot kernels across translation units without a measured reason. Preserve the release toolchain first; optimizer/lifetime changes are separate treatments.

## 7.5 P3 — Qualify each existing gate independently

Measure initialization, max individual native call, normal steps, requested Finish, cancellation-to-release, graph packaging and total usable delivery with their true clock origins. Check cancellation in each setup stage and cleanup state, plus completion without cancellation.

Use cheap fixtures to reject partial-table publication, read-triggered preparation/reentry, duplicate commits, work refunds, memory transfer gaps, stale invocation messages and error-order changes. Use a small matched native/WASM cohort only when the staged contract works. Keep original graph/cost/goal/action/price semantics and stop classifications visible.

The existing 250-ms begin and step limits, 1-second setup-cancellation limit, 10-second Finish limit and 65-second worker-start delivery target are distinct acceptance fields. Improving one does not excuse failing another. A smaller first call achieved by doing the full old workload in the next call is not success. Rendered UI review remains separate.

**Stop:** bounded release cannot be achieved within the selected concrete owner design, newly exposed proof tables violate complete-evidence invariants, error contracts are unresolved, or an independent runtime semantic discrepancy remains. Report the actual dependency rather than adding another speculative subsystem.

## 7.6 What follows after responsiveness

Select later work from changed evidence, not elapsed time. Reopen a candidate-layout adapter only after a real private-constructor discriminator reduction survives native legality and routing. Reopen response reuse only after native item/control correspondence, changed boundary, sparse fill and all-in cost show a useful opportunity. The current Ring probes are preserved negatives, not permanent impossibility results. [R07](SOURCES.md#r07).

A user action-limit feature needs an explicit expected-count, pathwise-budget or completion-probability contract and failure terminal semantics before optimization. A lower-bound programme needs a specific native-valid model refinement that can affect the root or competitive obligations. Whole-solver multithreading, a permanent recipe library, generic caching, broad utilities or a rewritten scheduler are not automatic successors.
