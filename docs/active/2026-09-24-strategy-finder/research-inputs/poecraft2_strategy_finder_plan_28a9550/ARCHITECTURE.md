# Architecture and authority contracts

## 1. Shape of the system

```text
Calculator / corpus / Lab
           │ same native-resolved problem + priced action grammar
           ▼
   immutable problem identity  +  separate algorithm/run configuration
           │
           ├── Current certified-search work ──────────────┐
           │    existing behavior and proof owners       │
           │                                             ▼
           └── Experimental PolicyFinderWork       request-bound policy
                  │                                compilation/checking
                  ├─ descriptors / role features          │
                  ├─ heuristic controller search          ▼
                  └─ complete untrusted candidates  owned checked artifact
                                                         │
                                                         ▼
                                              common result/export lifecycle

Later, separately: checked artifact → compatible incumbent import → existing proof
```

The new lane ends at checked policies. `PolicyFinderWork` must not instantiate a legacy `SolveWork::Impl`, run full strict competitor closure, write ProofStore, or masquerade as an existing dirty/count mode. Selected-policy refinement and SCC work inside the evaluator remain required where the evaluator needs them. “No optimality proof” does NOT mean “disable native policy validation.”

## 2. Proposed interfaces (names may change, meanings may not)

| Object | Required content | Must not contain / imply |
|---|---|---|
| `ResolvedCraftProblem` | artifact/session identity; exact root and persistent context; native GoalSpec/terminal semantics; allowed primitive+macro grammar and dependencies; original price vector | algorithm-selected pretend root; lossy feature identity as semantic key |
| `FinderConfig` | version, search seed, limits, heuristic/model feature version, beam/check controls, supported capabilities | altered prices or action scope hidden as an algorithm option |
| `ProposalFeatures` | possibly lossy role/context representation; feature-availability mask; exact-source join ID | certified kernel, legal/proper flags or public lower/upper |
| `NativeCandidateSpec` | structural control, native operation/spec references, guarded branches, explicit loops/stages and recovery, immutable candidate lineage | supplied probabilities/costs to trust; declared optimality; implicit control memory |
| `CompiledCandidate` | ordinary graph bytes, compiler-authored declarations, full problem/grammar binding, construction status | `policy_available=true` before acceptance |
| `CheckedPolicyArtifact` | immutable graph+request/context+evaluation receipt; goal/mass/properness/cost statuses; primitive/resource expectations; numerical-contract version; artifact identity | source-value vectors transplanted from a heuristic context |
| `FinderProgress` | lifecycle, work/resources, candidate counts by actual stage, best CHECKED cost, declared limit/stop cause | best heuristic score displayed as cost; closed action envelope; fabricated convergence |
| `VerificationOutcome` | accepted artifact OR typed semantic/resource/numerical failure | all failures collapsed to infinite crafting cost |

These are proposed concepts, not interfaces already in main. Prefer small views/adapters of native types to duplicated stores. Do not allocate a universal schema registry just to name them.

Separate three identities:
1. **Problem:** physical target, original prices, permitted behavior.
2. **Run/treatment:** lane, algorithm/model, seed, budgets, source/runtime.
3. **Artifact:** immutable executable graph and its exact checking context.
A changed lane is not a changed crafting objective. A changed price/goal/action scope is.

## 3. Request-bound acceptance: the first essential extraction

Current generic graph evaluation evaluates a supplied program. The graph supplies terminal labels and condition vocabulary; `collect_condition_targets` builds observation targets from conditions. Legacy solved-policy compilation supplies the missing trusted relationship to the request. A more aggressive proposer needs this relationship explicitly enforced. This is an interface audit requirement, not a demonstrated vulnerability in currently published policies. [R11, R14–R15]

Acceptance must establish:
- exact original start/session/artifact, not an easier partial starting item;
- original target and required rarity/clean-terminal semantics;
- all reached success-terminal items satisfy THAT native target, even if candidate routing omits a goal condition;
- all executed primitive dependencies belong to an allowed native program/scope; dependency-only primitives cannot be promoted to free independent actions;
- correct paid setup, reset/replacement, internal choices and cleanup;
- complete positive-probability behavior, with zero accepted off-policy/illegal/missing-price/failure/unknown/STOP mass;
- proper original-root execution and finite original monetary/resource/count results;
- existing numeric and coefficient-provenance contract, not a stronger unimplemented rational enclosure.

Make the checker consume the external resolved goal as trusted context or require a compiler-issued request-bound terminal contract that cannot be supplied by the proposer. If an evaluator observation partition omits needed goal distinctions, add the external target's observation requirements or use its existing exact path; inspecting only truncated terminal examples is insufficient. Preserve the general Strategy Builder's authored-success meaning unless a separately selected change requires more.

Do not accept a graph solely because its self-declared success mass is one. Test a zero-action fake-success graph, deleted goal predicates, a weakened tier/rarity target, an easier root, a scope-expanded action, and substituted price/context identities.

Only the acceptance owner can construct a `CheckedPolicyArtifact`. A score, model output, emitted JSON annotation or candidate status cannot set its validity. Keep current graph/certificate pairing and failed-pair clearing. Validation of a candidate cannot overwrite the previous checked winner on failure.

## 4. Native mechanics and candidate compilation

Use existing native descriptor and macro semantics. Split **enumerating native candidate descriptions** from **materializing every expensive kernel** where a real existing coupling prevents early ranking. `AutomaticOptionSynthesis` demonstrates an existing descriptor stage; its wrapper/API is not already general or automatically cheap. Extract one narrow consumer, preserve legacy caller behavior, and count its construction cost. [R10]

The main compiler takes a `SolveResult`. Do not forge `converged`, closure flags or proof vectors to call it. Prefer a narrow candidate compile view/adapter that provides required selected control without any solved-status claim. Reuse primitive/program emission, exact conditions, source identity, and graph-local provenance. A distinct small entrypoint inside the existing compiler is acceptable; a second rules engine is not.

A model is a SEARCH advisor, not an interpreter required to execute the returned graph. Freeze its choices into supported native conditions and concrete action parameters. A controller depending on a runtime neural query is outside this initial programme.

Mandatory option interiors remain indivisible unless native semantics already expose a genuine observation/choice boundary. Finite repair depth must be represented by controller state when it matters. A recurring router patch must be checked as recurrent, not priced using a finite-prefix calculation.

The benchmark-private fragment IR is a design/reference asset and test oracle. Do not automatically move it into the release inventory or assume its single-entry FinalSuccess-only flattener supports arbitrary return boundaries. Use it only if its actual contract matches the selected complete controller; record that choice. [R13]

The minimal control-node view and first seed/coordinated-family examples are in [CANDIDATE_INTERFACE.md](CANDIDATE_INTERFACE.md). They are proposed native adapters, not a new public strategy language.

## 5. Search state versus model state

There are three intentionally different representations:
- **Exact native source:** physical item plus required control context, stable original identities.
- **Backend calculation state:** a `CalcContext`/evaluator abstraction that is valid for its declared operators/observations.
- **Proposal state/features:** possibly aliased role/status/context information used to rank or construct rules.

A proposal feature collision can cause poor ranking. It must never share a native transition cache entry, exact state ID, continuation certificate, inferred legality or checked artifact. Fresh candidate-local layouts get fresh namespaces. Their smaller action sets can be useful for discovery, but do not change the full request or retire omitted competitors in the legacy proof lane.

## 6. Lifecycle and resource ownership

Recommended peer lifecycle:

```text
Resolve → SeedCandidates → Search
  Search → BuildCandidate → RequestBoundCheck → Accept/Reject → Search
  any active stage → FinishRequested → Seal best checked result → Done
  any active stage → CancelRequested → Release actual owning work → Cancelled
```

Partial proposals may survive within the bounded search queue; only complete checked artifacts survive as deliverable results. Finish stops creating speculation; it cannot turn a half-checked candidate into the winner. If no artifact is checked, return `no_verified_policy`/existing equivalent truthfully. Never return an optimistic estimate instead.

The existing `pc_solver` owns `SolveWork` directly. Use a small tagged peer dispatcher (or equivalently small interface) for begin/step/finish/cancel/progress/telemetry/export. Do not only dispatch `step` and leave getters or abandonment assuming legacy work. An old/default options layout selects legacy. Unknown modes reject. New finder state-value/proof-handoff APIs refuse where no such authority exists. [R07–R09]

All concurrent ownership counts: immutable problem views where physically owned, candidate contexts, beam, emitted graphs, checker scratch, model features and retained best artifact. Shared allocations are charged once by their real owner, including reallocation overlap. One expensive checker is live initially. No proof parent should be retained just to give the finder a place to live.

Wall time, cumulative native logical work and memory have distinct meanings. Rollback frees live storage; it does not refund spent native work. A check limit is in addition to, not instead of, the existing time/work/memory limits. Budget and current winner may change across suspension; adoption compares with the current compatible winner and checks invocation/generation identity.

## 7. Structural scope

New peer sources can be `solver_finder.*` and a narrow shared candidate-acceptance/compile view. Those names are recommendations. Avoid adding the new beam/model fields to `solver_solve_types.hpp`/legacy Impl. Do not mechanically split every giant coroutine first. Preserve `ProductionPolicyOracle` TU and the evaluator lifecycle.

Extract only the mechanisms needed by two consumers; no requirement to migrate every legacy callsite before the new lane can be tested. No public default switch until the evidence gate in [VALIDATION.md](VALIDATION.md).
