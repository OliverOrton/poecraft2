# 6. Research, mathematical obligations and counterexamples

## 6.1 Research selection

This review revisited primary documentation for PowerShell JSON date handling, C++ coroutine resumption/destruction, and Emscripten asynchronous execution, plus the 2025 fixed-point certificate paper. The practical consequence is to repair the serialization boundary and reuse the native staged/checked architecture—not to add a new optimization algorithm. [P01–P04](SOURCES.md)

**PowerShell:** default timestamp parsing can change the value's runtime type. A string-preserving mode exists, but its parameter requires 7.5+. This supports testing the actual wrapper argument; it does not identify the unseen failing manifest's complete diff. Prefer preserving the literal field with the already selected JSON/Python owner where broad shell compatibility matters. [P01]

**Coroutines:** suspension and resumption require explicit valid ownership/state. The existing custom task already accounts for its frame and accepts checkpoints. C++ coroutine syntax does not bound the amount of work before a suspension or make destruction asynchronous. [R14, P02]

**WebAssembly:** Asyncify and JSPI are real alternative mechanisms for suspending synchronous-looking code. They require their own build/runtime integration. They are not selected here because this project needs the same native owner semantics, explicit staged memory and cancellation behavior, and has an existing continuation type. This is an engineering choice, not a claim those technologies are unavailable. [P03]

**Certificate separation:** the fixed-point certificate paper develops witnesses/checking for reachability and expected rewards. It supports separating numerical proposal from acceptance, but does not establish poecraft2's native implicit-model correspondence or a latency guarantee. Preserve the existing accepted native relation/checker boundary. No new exactness claim follows from making it interruptible. [P04]

## 6.2 Prefix safety as a stuttering refinement

Let `K` be the committed authoritative evidence and `S` private staged work. Let the synchronous producer perform a deterministic sequence of primitive updates followed by its original acceptance test. The staged implementation refines this sequence by adding checkpoints that do not alter `K`, input identity, primitive-update order or acceptance premises.

A sufficient invariant is:

1. Every object in `K` has completed its original native validity checks and has compatible scope.
2. Every unfinished object is in `S` and inaccessible as authoritative evidence.
3. A checkpoint preserves both facts; a commit moves only an accepted complete object.
4. Cancellation releases `S` through its owner and cannot start further production.

Induction over updates, checkpoints and commits preserves validity at every observable prefix. If the uninterrupted and staged computations execute the same arithmetic/update order and final inputs, their committed result can be compared exactly. This is a **conditional design argument**, not a proof that current code has all these properties. Container order, floating-point accumulation, lazy cache mutation and error timing are explicit native obligations.

A strong all-or-nothing transaction over every pattern is unnecessary if an existing component can independently issue valid evidence. Commit that component at its own original boundary and leave other components pending. Do not convert a pending status into a global zero overwrite or erase an already stronger independent lower.

## 6.3 Missing probability mass cannot disappear at a checkpoint

Consider a fixed controller with one action: success with probability 9/10 and a cost-bearing absorbing trap with probability 1/10. A generator that has emitted only the success branch has not established a proper success policy. Normalizing its 9/10 partial mass to one removes the trap and changes the model.

Accordingly a yielded row cannot be admitted to the ordinary completed-row store. A yielded lower table cannot be called converged because its ready flag was set to suppress reentry. Independent lower contributions that were already valid are a separate matter; their reuse needs no fabricated completion of the new row.

## 6.4 Frozen-vector minima are generation-specific

For events `e` with valid capacities and complete native coverage, a lower producer may construct an optimistic relation by minimizing `sum_e p_e h(s_e)` under those capacities at a frozen vector `h`.

The minimizing assignment is value-dependent. With two possible successor values, vector `(1,10)` chooses the first; vector `(10,1)` chooses the second. Reusing the first assignment after the vector changes reports 10 when the true minimum is 1. With source potential 5 and zero immediate cost, the stale relation appears to accept `5 ≤ 10` even though the correct relation violates `5 ≤ 1`.

Thus a completed numerical check on stale frozen rows is not a check of the new native nonlinear relation. The existing retention producer already rebuilds minima at each candidate and checks simultaneous dependencies. Cooperativity must preserve its vector/model generation across every suspension and rebuild required minima after repair. [R13]

This example is synthetic, not evidence of a current native wrong answer. It is an important rejection test for the proposed staged conversion.

## 6.5 Ownership, feasibility and interruption

For a fixed request `theta`, each compatible complete proper policy `pi` supplies `V*(root) ≤ J_pi(root)`. Stopping computation and delivering an owned verified policy preserves that inequality. It does not establish convergence or a bound on later crafting duration.

The issuer must own the full graph, root certificate, request/price/context identity and evaluated value. A historical cost without its graph is insufficient; a root-only certificate supplies no arbitrary-entry tail. The completed retained-pool change is a useful implementation of this obligation across verification suspension. The new setup task must not undo it. [R10]

If setup is cancelled before an artifact exists, cancellation returns no invented strategy. If an optional proof component refuses, previously compatible verified artifacts and independent lower evidence follow their existing owners. Finish and cancellation remain different control outcomes.

## 6.6 Memory and work are different measures

Let `A_n` be the set of distinct live allocations after a native update. A sound live accounting model includes their charged capacities, task frames, committed objects and scratch overlap, with shared ownership counted once according to the existing owner contract.

A move changes ownership, not the number of live copies. A staged copy temporarily adds storage and requires admission before allocation. Detached in-flight work remains in `A_n`. Releasing scratch can decrease the live total, while cumulative logical work `W_n` must stay nondecreasing. Pre-ledger reservations must be transferred exactly once to the initialized ledger, not vanish during a constructor/task transition.

The supplied toy checks exercise one-time transfer and no work refund. They cannot prove the native ledger complete; source/layout auditing and actual cap fixtures remain required.

## 6.7 A response-time decomposition is not a theorem about the browser

Write an intent-to-release interval as

`T_cancel = T_dispatch + T_remaining_atomic + T_snapshot + T_release + T_transport`.

Making preparation yield improves only the opportunities represented by dispatch and remaining atomic work. If snapshot or release takes several seconds, acknowledging intent earlier still fails the one-second endpoint. Scheduling delays and allocator/runtime behavior are host-dependent. The targets are measured contracts on declared environments, not platform-independent worst-case bounds proved by this equation.

Preserve the wall-clock start before begin. Moving it after pending setup would grant additional search time and confound policy comparisons. Logical-fence equality and same-wall-budget usefulness are complementary measurements, not interchangeable proofs.

## 6.8 What is intentionally not reselected

The two saved-Ring observation/response probes remain bounded negative results. A future representation adapter needs a smaller **actual private-constructor** global partition after bench/routing/member requirements. A response cache needs native interior correspondence, boundary/fill and all-in savings, not similar node labels or a seven-pair cardinality delta. [R20]

A new lower model needs a specific native-valid refinement that removes the current optimistic bottleneck. More accurately solving the same optimistic model cannot exceed its own optimum. An expected-action penalty still does not imply a hard pathwise cap or a completion probability. These remain useful research directions with explicit gates, but none is an implementation prerequisite for responsive setup. [A02]

## 6.9 Novelty posture

No novelty is claimed for coroutine staging, transaction-like publication, fixed-point checking or the elementary inequalities above. The plausible project contribution remains a verified resource-bounded interface between native implicit-model construction, executable policies and proof checking. This programme supplies lifecycle/response qualification, not a new SSP theorem or an exhaustive literature search.

See [synthetic checks](checks/test_handoff_checks.py) and [their actual results](evidence/check_results.json). All are abstract/unit examples or diagnostic-tool tests, not native game-mechanics emulation.
