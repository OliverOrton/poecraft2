# Arguments, counterexamples, and research disposition

The relevant canonical arguments already live in `mathematics/search-and-resumption.md`, `mathematics/policies.md` and `publication.md`. This document supplies a scoped extension for compiler service, not a new SSP algorithm or a claim that existing native policies are wrong. The offline checks validate the abstract examples below, not the native implementation. [R15, R16]

## 1. A count-limited step is not time-limited

Let one externally blocking call perform work units 1,…,q, with duration d_i. A work ceiling q≤q_max gives no useful latency bound unless each d_i is bounded. One unit can contain a complete compiler, parse and evaluator construction. Thus reducing q from eight to one may leave the same long unit untouched.

More generally, for a particular call,

    T_external = T_runtime_entry + T_native_body + T_marshalling_and_observation.

The equality is a partition of non-overlapping intervals, not a licence to sum inclusive timers. Native body work may itself be a sum of several nested stages or amortized batches. First-use runtime compilation can occur before a callee's first in-body timestamp. A residual between caller and body measurements is not automatically compilation: locate wrapper, allocator and clock overhead first.

A hypothetical bound of the form

    T_external ≤ B_entry + q_max·B_unit + B_exit

requires all three bounds and the specified runtime/host assumptions. C++ checkpoints alone prove none of them. Likewise, bounded control response needs a bound on time to observe intent, time to the next interruptible boundary, required publication/rollback/release, and transport. No hard real-time theorem is claimed for an arbitrary browser, operating-system scheduler or input.

**Counterexample:** one unit requires 300 ms before function entry and 5 ms of body work; a one-unit quota still takes 305 ms. Splitting body work into five 1-ms resumptions without reducing the same entry cost still leaves the first call over 250 ms. This is a synthetic explanation of why code shape and continuation shape are distinct, not a native measurement.

## 2. Prefix preservation for deterministic compilation

Fix a complete input I: exact source item, selected decisions, observation choices, layout/action vocabulary, original target/scope/prices, stable routing and resource limits. Let a blocking compiler apply deterministic transformations f_1,…,f_n, in the existing order, to a workspace W. It emits graph G and compiler provenance P only on completion.

A staged implementation maintains (I, k, W_k, K), where k is the next transformation and K is previously committed evidence outside this compiler. A suspension is a stuttering step: it changes no semantic component. A productive step must implement the same next transformation or an explicitly proved decomposition preserving its observable result, ordering and side effects. Completion commits (G,P) as one bundle.

Induction on productive steps shows that, with unchanged inputs and successful resource admission, W_k agrees with the corresponding blocking prefix. Hence the final graph and provenance agree under the declared serialization contract. This proof does not follow merely from a cursor: references must remain live, input mutation must be excluded or detected, operation order preserved, and interrupted calculator transactions must obey their own rollback contract.

At any earlier stop, K remains valid and W_k is discarded or retained privately. No graph prefix is a complete strategy. Graph compilation alone never becomes properness or value evidence. Reusing a certificate requires the same complete graph/context/entry identity or the existing explicitly proved paired-default relation. [R15, R16]

Two policies with the same root scalar need not have the same graph. A certificate for graph A does not certify graph B because B was emitted by the same compiler or has the same node count. The abstract tests reject a changed non-default edge and a changed operation even when default normalization would otherwise match.

## 3. Paired certification and product graphs

Let G_c be the fail-closed certification graph and G_p the product graph. The existing compiler contract designates a set D of bounded default edges and a specific allowed target replacement relation. Agreement outside D is a semantic precondition, not merely a formatting preference.

Where the existing complete native evaluation establishes zero probability of taking those changed defaults from the authorized entry, changing only their destinations preserves the reachable execution law at that entry. This implication depends on the native evaluation, complete routing and exact authorized pair relation. It does not establish equality at arbitrary entries, nor does a numerical threshold alone provide a new exact-zero theorem.

If the default branch is reached and product recovery is proposed, the existing recovery path must evaluate that complete product controller. A stage split must not skip this second evaluation or return a successful certification result while product emission/pairing is unfinished. The native checker’s current numeric and mass contracts are preserved rather than strengthened by terminology. [R6, R16]

## 4. Frozen accounting can be amortized only at a real mutation boundary

Let the existing conservative charge be

    M(W_i) = B(F) + D(A_i) + C(J_i) + O_i,

where F is an unchanged construction domain, A_i remaining mutable structures, J_i the growing JSON buffer and O_i the accounted overlaps/frames not already included. The decomposition must cover the *actual* owner inventory and existing charging convention.

If F and every transitive capacity it owns are unchanged, B(F) may be computed once. If mutations to A_i and J_i have exact known conservative deltas, maintain D and C incrementally and compare against the original full audit. Induction on mutations gives the same M and cap decision at each check/admission boundary. Computing the “fixed” part before a lazy cache or output-binding vector stops growing breaks the premise.

For old buffer b_o, proposed new buffer b_n and limit H, simultaneous growth must be admitted against the overlap before freeing b_o. Checking only the post-move total can falsely admit a growth that exceeds H while both are live. Failure must leave the old usable object and its charge intact. Releasing either buffer decreases live memory but never rewinds cumulative work debit.

If a full traversal of fixed storage costs O(S) and is repeated for E emitted edges, it contributes O(E·S) traversal work. A valid frozen/debit decomposition can remove that repeated fixed traversal, but the actual runtime benefit depends on its measured share; node traversal, hashing, string capacity and parser costs remain. This is an amortization argument, not an observed complexity or speedup result for the entire compiler. [R8, R9]

## 5. Progress observations and algorithm progress are different

A phase sample is a statement about the observed owner at a boundary, not a complete stack trace. Similarly, a “first verified” sample can lag the actual event if sampling is throttled or events are omitted. Keep a monotonic native event sequence and exact owner-side event when available; otherwise call it the first *observed* verified time.

The current paired receipts have later first-verification observations but earlier usable delivery. These are separate performance dimensions. Do not compute a stage duration from a worker-origin timestamp and a UI-origin timestamp without an explicit clock correspondence. A diagnostic read can be logically passive and still impose wall-time/compilation overhead that changes the algorithm's useful work before a real deadline.

Structural equivalence with an unchanged operation sequence does not guarantee identical wall-time stopping points or identical chosen policies under a deadline. Validate both deterministic no-interruption prefixes and the actual deadline-bound product result. Fairness and eventual exact closure remain separate open properties. [R15]

## 6. Primary research and how it changes this plan

**V8:** the current upstream architecture documents baseline Liftoff, optimizing Turboshaft, first-use compilation behavior and profiling/tiering interactions. These make generated-function shape and normal-runtime cold controls relevant. The installed Node/browser version must be recorded; current upstream names do not identify the user's runtime automatically. This is not evidence that V8 caused the remaining span. [P1]

**Clang:** `optnone` suppresses most function optimization, and a function attribute does not automatically apply to nested lambda call operators. No-inline and coroutine attributes have specific semantics, not generic performance guarantees. In particular, an attribute promising destruction only after completion is incompatible with a task intentionally destroyed at intermediate cancellation points. No such lifetime promise is selected. [P2]

**Emscripten:** source compilation and final link optimization, symbol/debug options and profiling builds are distinct. Use installed tool support, identify code-changing diagnostic flags, preserve the current release exception-handling and floating-point flags, and qualify normal release output. These docs motivate controlled experiments, not an SDK upgrade or a broad new optimization configuration. [P3, P4]

## 7. Novelty and canonical claims

Checkpoint insertion, function extraction, amortized byte accounting and deterministic compiler preservation are established techniques. Do not claim publishable novelty from them. Their contribution here is a qualified, maintained implementation boundary enabling later native SSP work without losing action, cost, observation or artifact authority.

Keep CLM-0002 and CLM-0021 scoped to their existing conditional statements; affected compilation/operation preservation also relates to CLM-0004. Add implementation correspondence only for actual checked paths. Do not close broad compiler/evaluator equivalence, complete native lower validity, numerical enclosure, scheduler fairness or exact-closure claims because synthetic tests or traceability lint pass. [R15–R17]

The future research questions remain explicit: generic entry service for the C4 discovery gap; native-valid persistent distinctions that eliminate a cheap optimistic winner; and action-count distribution/constraint semantics. None is selected as a hidden extension of this compilation work.

## 8. Safe pending reads do not prove service equivalence

Under the existing nonnegative-cost assumptions, returning zero while a lower producer is pending can preserve lower-bound safety. It does not preserve a synchronous consumer's one-time decision. Suppose a decision needs a completed lower of 3 to certify that a cost-3 action dominates alternatives. A first pending read returns zero, so the certificate is not established. Marking that encounter as permanently attempted can lose the certificate when the real lower later commits, without ever publishing an invalid numerical lower.

Thus staged refinement needs two separate premises: (i) every exposed value/evidence bundle has valid scope and readiness; (ii) pending consumers whose work remains required receive their compatible committed generation before a dependent final decision, subject to the existing cancellation/resource/terminal boundaries. This is a local continuation obligation, not a new global scheduler-fairness theorem.

Current constructive-certificate failures make this a native hypothesis worth testing; they do not prove it is the cause. The S8.3 bracket must be diagnosed independently. A provisional tuple with mismatched scope is not repaired by clamping it to a policy value. Keep authority/graph/entry generation correspondence and a final fail-closed check. [R24–R28; 08_INTEGRITY_GATE.md]
