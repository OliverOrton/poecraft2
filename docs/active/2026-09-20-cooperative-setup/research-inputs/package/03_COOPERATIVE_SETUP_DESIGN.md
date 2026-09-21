# 3. Two-owner cooperative setup design

**Design, not implementation.** Names below describe roles; reuse existing types/owners and choose local names after source inspection. The current `CooperativeTask` and `ProofPatternManager` already exist. [R14, R18](SOURCES.md)

## 3.1 Actual source dependencies

| Existing boundary | Source-confirmed behavior | Required change |
|---|---|---|
| `SolveWork::Impl` construction | High-impact cover, then requested retention, then ledger initialization | Return a valid pending work object before expensive preparation; preserve dependency order |
| Ordinary non-high-impact cover | Lazy by design, including early root-cap attribution | Do not unconditionally move it ahead of ordinary work |
| `prepare_goal_cover_cost` | Sets ready before constructing several tables/contracts | Separate active construction from committed availability |
| Cover consumers | Phase proposal and optimistic completion can call prepare | Scheduler/service advances; getters only observe committed evidence |
| Retention owner | Reserves 32/64 MiB; builds support/probability; publishes after checking; broad optional-refusal catch | Staged work with typed terminal reasons and atomic checked-potential adoption |
| Probability producer | Frozen-vector event minima, complete coverage, check/repair rounds, final buffer transfer | Preserve round identity and complete evidence across suspension |
| Abandon API | Peak audit, telemetry snapshot, serialization, then work destruction | No new computation through observation; account for all release work |
| Worker | Clock starts before begin; controls between native calls; finally abandons | Keep wall window/one-response semantics and actual event-loop opportunities |

Sources: [R11–R18](SOURCES.md). This is an inspected map, not an exhaustive caller census. S1 must enumerate remaining local call sites before editing.

## 3.2 Separate preparation state from solve-result state

A conceptual state is `(stage, cursor, committed, staged, consumed_work, owned_bytes)`:

```text
cheap request validation / resolved scope
    → cover dependency pending (when required)
    → cover preparing
    → committed cover evidence or named refusal
    → retention preparing (when selected and prerequisites available)
    → committed retention evidence or named refusal
    → dependent search

any preparing stage → cancel observed → bounded cleanup → released invocation
```

An internal setup substage can live under the existing public setup/expanding presentation. Do not change the 200-byte public progress structure merely to expose this state. Prefer existing versioned trace/telemetry mechanisms for additional detail. Prove progress access is safe before the first search-sized value array exists.

Do not overload one `ready` flag to mean started, recursion suppressed, successful, or optional refusal. A refused optional component records its reason and falls back only according to the existing lower contract. Cancellation is terminal for that invocation; a cap stop remains a resource disposition. A phase can be incomplete without being invalid, and a numerical proposal can exist without any published lower.

Cheap validation retains its immediate contract. Expensive validation may now complete during stepping: enumerate each changed error timing and preserve its category/one-terminal-result behavior. Do not claim byte-for-byte control timing equivalence after changing when expensive work runs.

## 3.3 Goal-cover owner

Stage construction-only maps, predecessor/policy arrays and temporary tables. The currently combined function updates several pattern families; identify which complete component can legitimately commit independently. Do not call the whole staging object public evidence because one table has finished. Conversely, preserve an existing valid component if a later independent component refuses.

Keep current nested order through actions, subsets, draw permutations, masks, rarity/occupancy states, sweep updates and tie resolution. A quantum controls **how far that same sequence advances**, not which semantic work is skipped. Native draw/probability helpers may themselves need resumable cursors if one invocation dominates the slice; profiling selects those seams. A fixed number of actions is not a wall-time bound.

The base read contract is: return a completed compatible contribution or its previously authorized fallback, without preparation, graph interning, expensive validation or mutation. Dependent search must wait at an explicit service boundary instead of silently proceeding on an accidentally missing table. This preserves the original preparation dependency rather than proposing a new lower-first/upper-first scheduler.

A blocking diagnostic/test facade may drain the staged task to completion **only where blocking is an explicit caller contract**, using the same producer and acceptance logic. It must not be reachable from passive progress or abandonment.

## 3.4 Retention owner and frozen-vector rounds

The support view, selected fracture frame, caller/action scope, prices, prepared cover proposals and reuse dependencies form the task's input identity. Borrow only objects whose lifetime and immutability the owner guarantees. Any native calculator cache mutation remains owned by that calculator and is separately charged; a broad `Impl&` dependency is not a substitute for a defined lifetime contract.

Within a probability round, freeze candidate `h_g` and its generation `g`. Build complete relation rows from that vector, including event capacities, cheapest event/member assignments, self/no-op alternatives, price reactivation and native action coverage. All rows used by a check must belong to the declared generation and model. Resuming midway changes no vector. A repaired/proposed vector starts the appropriate rebuild/check round; it cannot inherit the old vector's minimizing assignments as native all-vector kernels. [R13]

Useful stage seams are domain/support preparation, native draw evidence, source/action/phase relation construction, complete coverage validation, quotient row construction, checking/repair, member-safety projection and final immutable transfer. These are candidates for implementation boundaries, not a guarantee that one whole seam is short enough. Inspect/check expensive inner loops and quotient calls.

Retain the existing optional early checked-subsolution endpoint exactly when its original premises pass. “Preparation was interrupted” alone does not establish that endpoint. Every public `PreparedPhasePotential` must have completed the required native checking and complete-member conditions; its pointer is committed only after the matching metadata/tables are ready.

## 3.5 Memory and exception safety

Use one accounting equation over **distinct owned allocations**:

`live = context_owned + committed_owned + staged_dynamic + active_frames + transient_overlap`.

Track reservations separately from actual live bytes so the same allocation is not counted twice. Include pre-ledger setup, borrowed/shared owners and container capacities. Move ownership atomically: verify fit before creating/copying, transfer the reservation/charge once, then relinquish the old owner. Releasing scratch reduces live bytes, not cumulative logical work or past peak usage.

The existing coroutine factory allocates a frame before first resume. Include parent and child frame charges before the child performs its first admission. Checkpoint-reported nested bytes are not automatically correct after hidden cache growth or a capacity change. Explicitly charge active work even between checkpoints. Retain existing immutable charges only while their transitive storage really is frozen. [R14]

A source-preserving refactor can change compiler frame size. Record actual native/WASM layouts and reconcile the ledgers; do not subtract new storage merely to keep the old peak. Preserve the previous pool's +16-byte accounting distinction. No saved root value can outlive its full graph/certificate/context tuple. [R05, R10]

## 3.6 Cancellation and reclamation

Cancellation reaches the native owner only at safe non-reentrant control points. The worker's real event-loop yields are necessary for queued messages; coroutine checkpoints that are all drained within one synchronous native call do not expose those opportunities.

On cancellation: latch invocation intent; prohibit further preparation/search/proof publication; retain a bounded passive diagnostic snapshot; release active child before its referenced parent; roll back incomplete calculator work through the existing owner; release staged memory; then complete the terminal cancellation response. Complete context-owned caches may remain only if their existing ownership contract allows it and the UI cleanup endpoint does not falsely claim those handles were closed.

Attribute four intervals separately: snapshot acquisition, serialization, active-task/calculator rollback, and actual storage destruction/handle closure. The current abandonment code performs the first two before `solve_work.reset()`. Therefore “cleanup is slow” does not identify a destructor problem. [R15, R17]

Do not try to suspend inside `CooperativeTask::reset`/a destructor. Prefer a bounded synchronous abandon by making the staged payload and snapshot genuinely bounded. If measured release cannot meet the gate, first specify a resumable release owner plus an explicit caller protocol, including old synchronous API behavior. That is an additive-interface decision, not a silent change to what `abandon` means. A hard worker termination or an early UI acknowledgement is not the normal release proof.

The existing generic retention catch is an important hazard: cancellation currently appears as a runtime exception in the probability helper. Introduce or reuse a typed interruption/disposition path so cancellation cannot be swallowed as optional proof refusal followed by continued search. Do not identify control outcomes by matching English exception strings.

## 3.7 Throughput and ordinary publication remain separate

Cooperativity need not reduce total CPU work. Yield overhead may slightly increase it; the main required gain is a bound on uninterrupted work and control latency. Keep cheap kernels batched and measure cold total work. Do not create a coroutine per trivial scalar operation.

The recorded 369 ms call is ordinary ladder-to-compilation work, not constructor preparation. Instrument its exact internal span during S4; preserve its failure if not fixed. A future narrow compiler/publication slice can be selected from that evidence. Removing the whole-TU O1/non-LTO workaround, enabling Asyncify/JSPI, introducing threads or rewriting the scheduler would add independent treatments and is not selected here.
