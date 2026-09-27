# Current: finish target-neutral discovery without losing selected-policy capability

## 1. Supported combinations

The proposed internal enum has two initial values: `OrdinaryClean` and `TargetNeutralZero`. Names are NEW. Derive immutable capabilities from the enum; do not let callers independently combine arbitrary booleans into an incoherent profile.

| Goal/profile | Native search | Global positive lower | Proof retirement | Global closure |
|---|---|---|---|---|
| L / OrdinaryClean | existing path | existing qualified producers | existing qualified rules | existing exact-capable path |
| E normal form / OrdinaryClean | identical L | identical L | identical L | identical L |
| R / OrdinaryClean | explicit refusal | not qualified | not qualified | not qualified |
| L or R / TargetNeutralZero | new Current path | zero only | disabled | unavailable by profile |
| Other terminal shapes | refuse unless separately qualified | unspecified | unspecified | unspecified |

Finder is a separate owner. Its native R acceptance is already available and must not depend on this enum. No public default changes.

Required inputs are the resolved native goal, original exact root, action/program scope, original prices, data identity, capacity limits and proof profile. All private children inherit the profile explicitly. Reject incompatible cached proof/resume artifacts by full identity. An equivalent E normal form may keep L's goal identity but not erase a difference in proof profile.

## 2. Three value channels

Separate these meanings at the interfaces that matter:

1. **Global lower:** an admissible lower on the original full allowed decision problem. In neutral mode this is zero only.
2. **Selected-policy value/upper:** the cost or valid supersolution of one proper executable controller on its declared domain. It may be positive and remains useful in neutral mode.
3. **Search estimate:** a restricted/coarse/heuristic value used to choose work or propose a controller. It cannot certify retirement or exactness.

Do not force a whole-codebase strong-type rewrite. Reuse the existing proof-lower types, add a small profile guard and distinctly named conversions at current handoff points, and replace ambiguous conversions actually touched. The acceptance tests must target consumers, not just inspect type names.

Example: a fixed row costs 2, returns to itself with probability 1/2 and reaches the target otherwise. Its exact repeated-policy cost is 4 even when the certified global state lower is zero. Another action may cost 1, so 4 is not a full-problem lower. The current classifier's terminal-or-self shortcut uses this exact selected-row property; retain it in a selected-row value channel.

## 3. Required source map and changes

Paths are current inspected owners. Exact line numbers drift; locate the named symbols on the pinned tree.

| Owner | Existing dependency | Neutral requirement |
|---|---|---|
| `solver_solve.cpp::SolveWork::Impl::Impl` | R refusal, option/profile setup, solution scope, initial anchors | validate goal/profile pair; preserve priced direct-goal anchors; stamp bounded-only capability |
| `solver_api.cpp` sync/begin dispatch | target/mode refusal and lifecycle | resolve mode/profile once, no bypass via direct/native API; no Finder proof prerequisite |
| `solver_solve_bounds.cpp`, `solver_solve_carrier_pattern.cpp` | goal-cover tables, `optimistic_completion_cost`, `completion_proof_lower_value` | do not prepare/consume unqualified positive proof; proof reads return declared zero, not unknown/infinity |
| `solver_phase_probability.cpp`, native-retention preparation | target-specific finite proof models | inactive under neutral; raw mechanics/selected-option evaluation stays enabled |
| `solver_solve_incremental.cpp::certified_incremental_lower_values` | full-envelope result-values promotion, retained focused snapshots | neutral vector is zero, even if poisoned snapshots/closed-envelope flags exist |
| same, `begin_incremental_classification` | selected proper upper snapshots and retirement calls | retain legitimate upper snapshot; suppress unqualified retirement side effects |
| same, `advance_incremental_classification` | lower Q, terminal/self upper, NonImproving | compute selected terminal/self value faithfully; do not mark lower-based NonImproving under neutral |
| `solver_solve_expand.cpp`, `solver_options_automatic.cpp` | operator-proof early skips, automatic admission floors and caps | preserve legality/scope/native complete law; disable proof-based skips, do not silently disable whole automatic grammar |
| `solver_solve_operator_proof.cpp`, `solver_solve_envelope_proof.cpp`, strict-pattern owners | direct lower consumers and proof-store records | neither issue nor consume incompatible certificates; disabled status not complete proof |
| `solver_solve_focused.cpp`, `solver_solve_bellman.cpp` | focused lower snapshots, convergence, selected-policy numerical iteration | keep selected policy math; mark heuristic/stability distinctly; no unsupported global promotion |
| `solver_policy_refinement.cpp` and adapters | selected lift versus alternative proof | retain selected-policy observation/refinement required to execute/check; skip all-action proof scheduling for neutral |
| `solver_solve_finish.cpp::run_publication_pipeline` | unconditional/conditional proof setup, exact branches, normalization | no forced proof preparation; check and publish owned complete graph; bounded-only result before and after refinement |
| `successful_refined_publication_termination`, `normalize_publication_result`, invariant checks | exact termination synthesis | require profile capability in every exact promotion; final assertion rejects accidental exactness |
| telemetry, result/report, cache/resume identity | labels and reused state | include actual capability, refusal and profile; unavailable closure differs from not reached |

This is a required initial map, not a claim of exhaustiveness. Use a source/callsite sweep for positive-bound writers, NonImproving/retired transitions and exact-status assignments. Attach each additional consumer to this map and a focused test. Do not attach a generic proof checker to every hot loop.

## 4. Setup must finish honestly

An inactive proof task cannot leave `while (!advance_setup())` waiting forever. Add a real NotRequested/Disabled completion path for proof-only stages, while retaining mandatory initialization and memory-ledger stages. `Ready` continues to mean that the corresponding usable evidence exists. Never set ready flags and leave old positive arrays reachable.

Proof-free mode is not computation-free mode: original root resolution, native registry, actual target predicate, selected policy rows and exact evaluator can still require work. Keep cancellation/checkpoint accounting and existing setup readiness tests.

## 5. Keep discovery moving

Blindly returning false from every classification function can terminate or starve the incremental scheduler. Preserve its control contract: a work item is still admitted, deferred, queued, completed or resource-refused according to native evidence. If a proof-dependent decision is unavailable, retain the unresolved obligation and permit the ordinary finite scheduler/refinement fallback to service it.

A local fixed-policy comparison can propose a new selected row without globally retiring the old alternative. Delayed actions and native automatic synthesis remain in the same declared scope. If neutral removes a valid heuristic that was incidentally stored in a proof vector, prefer a separately named nonauthoritative accessor only when it is target-correct and already needed; initially zero guidance is safer than copying a clean-specific numerical answer.

Do not introduce an arbitrary new beam or narrow the permitted action family merely to make R finish. A cap with a checked policy remains a legitimate bounded result; a cap without a policy remains no verified policy.

## 6. Publication has two independent obligations

**Execution:** a complete graph under the original request is checked, proper, priced and retained with its exact context. This remains mandatory.

**Optimality:** all relevant alternatives support the claimed original-scope bound/closure. This is unavailable in the neutral profile.

Set bounded-only policy/termination state before entering paths that formerly assume convergence implies exactness. Keep the final invariant check as defense in depth, not the only guard. Do not publish an unchecked working `result.values[start]` as U. Numerical exactness for a fixed controller is not MDP optimality.

Expose `closure_unavailable_by_profile` in a typed capability field; preserve an orthogonal actual stop cause (Finish, work cap, exhaustion, cancellation). Relative gap against zero may be infinite/unavailable; do not render a misleading exact multiplicative ratio. A target-gap request should reject unsupported capability or state that it cannot fire; it must not wait indefinitely for an impossible proof event.

## 7. Native acceptance matrix

The tests exercise current entry points or existing focused harnesses, not a Python reimplementation:

- Sync and stepped R refuse under OrdinaryClean and run under TargetNeutralZero.
- Two simultaneously live handles with L/R and different profiles cannot cross-contaminate caches or readiness.
- Inject a positive clean completion table, focused snapshot, `result.values`, envelope bound and stale proof-store result: global lower remains zero; no row retired by those values; no exact status.
- A complete positive-cost terminal/self delayed row obtains the correct candidate upper without a positive global lower.
- An alternative with an unknown losing exit is not executable, even when its good exit reaches R.
- Full native choice timing is retained; no pre-offer optimization from knowledge available only after observation.
- A productive exact-target clean fixture and the existing exact Regalia path preserve ordinary proof behavior.
- Zero-cost cycles do not become proper by virtue of a zero lower.
- Cancel/Finish during disabled-proof transitions, selected-policy lift and candidate checking preserve valid incumbents and release owned scratch.
- Direct internal construction, private child options and compatible resumption enforce the same profile.
- Exhausted/closed coarse graphs in neutral mode cannot manufacture ExactClosed.

## 8. If a dependency is hard

The already-known existence of positive lower consumers is not a hard blocker; isolating them is the selected work. A genuine blocker is a reduced native example in which the selected-policy constructor requires an unsupported semantic law, the request cannot be faithfully compiled, or a proof cannot be separated without changing the admitted action problem. Reduce it, describe the exact invariant and smallest unresolved interface, preserve refusal, and continue independent P4–P7 clean work.

Do not select a new all-action proof algorithm in response. A later positive coverage-proof profile must retarget each valid producer and its all-action native correspondence separately, with zero target boundaries and valid predecessors. This programme prepares that seam but does not claim general R optimality closure.
