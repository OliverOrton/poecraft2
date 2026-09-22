# First-policy service and ownership design

This is a **proposed design envelope**, not a mandate to implement every component. T1 chooses the smallest causal treatment. Preserve current mechanics, authority and algorithmic decisions. A wall-time improvement produced by skipping work, changing the graph, raising a cap or hiding a blocking call is not this design.

## 1. Keep the existing ownership hierarchy

```text
SolveWork / existing scheduler
    → PublicationPipeline's initial-candidate or final-publication task
        → CompiledPolicyAssertionWork
            → compiler-local work, only if measured work requires it
            → existing strategy parser / StrategyEvalWork
        → complete asserted artifact
    → IncumbentPortfolio / final publication sealing
```

No new publisher, long-lived controller cache, all-purpose task executor or alternative strategy language is selected. `solver_compile_contracts.hpp` is the natural narrow boundary for a compiler-local work API. Put compiler implementation state in a private implementation object, not in the already broad `SolveWork::Impl` header. The existing native source inventory/build owners must include any genuinely new translation unit. The documented single-TU policy oracle and evaluator lifecycle boundaries remain intact. [R12, R17]

A source dependency should be an explicit frozen input or a bounded callback with a stated role. Giving a new class unrestricted `Impl&` access does not close the boundary. Avoid a universal context object that simply relocates the god object. Existing compiler helpers, `ConditionExpr`, operation serialization and ordinary strategy parser stay authoritative; do not duplicate their predicates or mechanics.

## 2. Freeze the compilation input at the existing candidate boundary

The early candidate already pauses discovery while its selected proof is checked. Preserve that discipline. A compile input binds the exact start, native session/layout vocabulary, selected operations and observation choices, value annotations, routing declarations, target semantics, original prices/scope, numerical options and resource limits. The current proof copies and their owner lifetimes are part of that bundle. [R5, R16]

When extending its lifetime, do not retain a reference into a resizable vector, a temporary options object or an output candidate that another active owner may replace. Prefer existing frozen proof storage; copying the entire calculator solely to avoid thinking about lifetime is not acceptable. If mutation is intentionally allowed, supply a checked generation/dependency contract and refuse/restart stale work rather than silently mixing generations.

The staged compiler may mutate its own condition/route/serialization workspace. Some compiler paths also request calculator kernels or materialization. Those side effects must use existing calculator ownership/cap contracts; incomplete transition transactions cannot be disguised as immutable inputs. Enumerate these calls before introducing suspension. No new raw calculator interning or asynchronous mutation is selected merely because the work object has a cursor.

## 3. Choose a shape according to the observed cause

### Branch A — Generated-code entry dominates

Partition the measured function into genuinely separate, semantically named bodies at existing construction boundaries. Merely inserting checkpoints within one huge coroutine can leave its generated resume body large; the runtime may need to compile that body before the first checkpoint. Conversely, a small orchestration function can call a large helper, shifting the same cold block into that helper. Measure the actual code bodies and their first entries.

Use compiler-local methods or same-TU leaf helpers where possible. Select a specific no-inline boundary only when optimization otherwise recreates the problematic body. Do not assume source `noinline` is a guarantee about every downstream optimization pass; inspect final code. Do not add `optnone` to the whole compiler or finalizer by analogy with telemetry. A narrowly scoped fallback needs measured cold and warm costs, exact semantic controls, a source comment pointing to the receipt, and a re-open condition. [P2, P3]

Keep hot reforge, sparse Bellman and checker kernels at the existing optimization levels. Do not simultaneously attempt to remove the finish-TU O1/non-LTO exception. No prewarming outside the measured request is allowed as a claimed latency fix. If prewarming is studied privately, include its full cost and cancellation/ownership semantics.

### Branch B — Useful native work dominates

Where required, evolve the existing assertion's coarse `Compiling` stage into private substages, without changing the public enum/ABI:

```text
input admission / frozen proof
    → source-policy domain and vocabulary
    → operation regions and route conditions
    → certification graph emission
    → parsing / evaluator admission
    → retained exact evaluation
    → permitted product re-emission or recovery reevaluation
    → paired-default validation
    → complete assertion
```

These are candidate seams, not an assertion that each currently fits in 250 ms. T1 identifies which need a resumable loop and which need only an external return boundary. The success paired-emission path inside `advance_evaluation` must be covered as well as the first `compile_and_prepare` call. Keep supplied-precompiled input and cached paired evaluation as distinct existing fast paths. [R6]

The compiler-local `step` must do bounded work and return to the actual caller. An outer loop that drains it before returning is not cooperative service. The blocking compiler API, used by genuinely synchronous callers, may drain the same producer, but it must not be used accidentally by the stepped assertion. All child calls that can independently dominate the interval need attribution; a cursor around an unbounded parser or sort does not bound that call.

Prefer loops and a small work object over child coroutines per edge/condition. Coroutine use is justified by lifetime and existing infrastructure, not mandatory as a style rule. Capture persistent locals explicitly. Preserve comparator order, JSON field/node/edge order, deterministic IDs and floating accumulation/formatting order. Do not add a new condition simplifier or change region sharing while restructuring.

If a monolithic parser or evaluator constructor itself is the measured residual bottleneck, extend that *existing* owner at its earliest valid suspension boundary; do not build a second parser or change evaluator semantics. If the necessary change is substantially wider than the first-policy boundary, stop with the measured residual and proposed separate scope.

### Branch C — Full ownership auditing dominates

At a closed phase, compute the existing conservative charge of demonstrably fixed compiler structures once. Maintain a separate growing contribution for JSON capacity, literal-count map storage, decision/provenance vectors, generated kernels, paired artifact copies and every other remaining mutable allocation. The set must come from a complete writer inventory, not an optimistic list.

The current `ConditionExpr` stores immutable JSON; `json()` is not a lazy allocation. Its recursive `owned_bytes()` can repeat work. Preserve the same charging semantics, including any conservative duplicate accounting of shared expression nodes, unless a separately selected accounting correction is justified. A “physical unique bytes” rewrite that changes admission is not a transparent performance refactor. [R8, R9]

Cache invalidation follows **storage mutation and capacity**, not merely a “policy unchanged” flag. Count increments in an existing map may allocate nothing; key insertion, rehash, vector growth, string capacity changes and final provenance retention do. Check growth overlap before allocation where the existing contract requires it; release only after the old storage is actually gone. Overflow saturates or refuses consistently, never wraps into free capacity.

Use the original deep audit as a differential oracle at every mutation fence in focused tests, including failure injection. Demonstrate equality or the same documented conservative overestimate and show cap decisions match. Do not lower the existing gate by skipping memory observations. Keep telemetry requests distinct from actual full traversals to avoid misleading speedup counters.

## 4. Partial compiler output is not a deliverable

`take_result` or its equivalent must refuse before the compile transaction completes. The compiler result consists of complete JSON plus matching compiler telemetry, route/decision provenance and default-mode information. These fields cannot be separately swapped from different generations. Serialization prefixes may be private scratch but never user-visible strategies.

The compiler result is still an **unverified proposal**. The ordinary parser/evaluator and assertion finalization retain responsibility for success/off-policy mass, properness, complete original-cost accounting, entry scope and reconciliation. Reaching compiler Done does not authorize an upper, global lower or exactness label. Expected-cost annotations retain their existing presentation meaning, not independently checked entry values. [R16]

The certification/product pair must preserve every non-designated field: operations, conditions, priorities, goal, offer context, fixed programme commitments, costs and graph structure. Only the already authorized default-target relation may differ, and the existing acceptance/recovery rules decide when native reevaluation is necessary. Do not replace the existing check with a looser “same number of nodes” comparison or normalize arbitrary JSON differences away. [R6, R16]

## 5. Control and memory are part of completion

Cancel remains invocation-scoped and wins before terminal commitment. No message handler reenters executing WASM. If Finish arrives before any verified policy, preserve the existing behavior; do not manufacture an upper or imply that stopping mid-compile can provide one. Once another compatible verified artifact is available, unfinished optional work cannot indefinitely prevent its delivery. Preserve the current bounded-finish owner rather than inventing a second control path.

On cancellation or exception, destroy staged children before their input/storage owners. Move operations that leave borrowed references valid must be explicit. Include the new work object's frame/shell, containers, parser, evaluation outputs, certification/product overlap and suspended verification data in existing aggregate accounting. Proof-local reservations and invocation allocation domains must not double-charge the same retained allocation or leave a gap. The new setup allocator is not automatically suitable for a different lifetime; reuse it only after its domain and deallocation lifetime are proved appropriate. [R10, R15]

Keep the old valid witness when new compilation, allocation or verification fails. Preserve consumed logical work and relevant failure disposition; releasing storage does not refund work. Synchronous destruction remains real work even when acquisition was cooperative. Qualify cold and warm cancellation of the selected new boundary, not only successful completion.

## 6. A structural improvement has a measurable endpoint

Record the actual changed owners, public/internal dependency surface, mutations removed and build impact. For source extraction, count the affected function/body and include fan-out only with a reproducible method; do not import the historical brace-script totals as fresh metrics. For code-shape work, generated Wasm bodies and first-entry latency are the pertinent measurements. For a work object, stable ownership and completion-only result access matter more than its file length.

If a narrow code-shape fix passes all relevant gates, stop instead of completing the optional staged architecture anyway. If work must be staged, leave a compiler-local boundary that future emission changes can be reasoned about without loading every solver owner. Avoid a larger generic framework whose migration cost exceeds the measured benefit.
