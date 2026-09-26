# Main review and next-direction decision

## 1. What landed at adb5210

The E programme implemented a useful shared substrate: `GoalTerminalConstraints`, `GoalAssessment`, native assessment and matching compiled predicates. Public v1 remains clean. The private explicit-clean case proves its preconditions and normalizes to the same L goal; it does not add a new search representation. Coverage-only R has a different native goal identity and calculator/predicate support, but full solve is refused before work. [R01–R05]

The decisive clean-versus-coverage **solver** comparison therefore did not happen. The previous programme is complete as a scoped safety audit, not as an answer about how much the terminal change affects exact-search difficulty. E's omitted wall run is justified by normalization. R's omission is a capability refusal, not equivalent work or a negative performance result.

Recorded Current results:

| Case | Checked clean cost | Certified lower | Expanded | Final rows | Retained transitions |
|---|---:|---:|---:|---:|---:|
| A4 | 3,746.1319409485764 | 198.8334996747695 | 7,213 | 135,519 | 214,017 |
| A5 | 85,558.70618560436 | 405.3694021063399 | 3,274 | 38,963 | 202,077 |

Both remain bounded. A5's emitted graph is byte-identical to J3. Its report separately retains a `refused_unsupported_action` reliability classification despite the valid checked policy; do not erase or reinterpret that label without the raw reason. A4 recovered the stronger reference, so do not reset its preservation target to the earlier 5,218 result. [R01]

The same-clean sample at state 396 is not an economic witness: it contains one satisfied goal, 3P/2S occupancy and `no_materialized_local_action`, but lacks complete physical identity and a checked alternative. The 30/34 sample split with 802 omitted observations is not a population census. Do not make this sparse sample the mandatory new repair target. [R01]

K Finder remains experimental: A3 14,034.203062534049 and A5 524,079.6986482172, much better than its former grammar but still worse than Current. Its retained-side family and native reached-entry checking are real; Current did not inherit that controller generator. S was measured gate-inactive; O was an economic negative. None needs another repetition here. [R02]

## 2. Both already benefit from some changes—but not automatically from everything

| Area | Shared now | Separate obligation |
|---|---|---|
| Goal assessment and terminal emission | Native assessment/compiler used by both | Each lane must propagate request identity and qualify accepted output |
| Mechanics, raw probabilities and programme semantics | Shared native owners | A generator must actually propose and service the programme |
| Strategy execution and numerical checking | Shared interpreter/evaluator | Policy accepted for the actual requested root/target/scope, not merely graph success |
| Current positive lower/proof closure | No Finder ownership | Target-specific, all-action validity required for Current |
| Finder retained-side grammar | Finder only | It is not automatically a Current proposal generator |
| Native/WASM transport | Common runtime substrate | A private native grammar or terminal mode is not automatically exposed in Calculator |

A shared source change can improve both correctness boundaries without changing either returned cost. A lane-specific grammar improvement is not a shared search improvement. The next records must state both facts explicitly.

## 3. New source finding: proof requirements are checked before solver-mode dispatch

Both `pc_solver_solve` and `pc_solver_solve_begin` reject `ExtraExplicitPolicy::Allow` before inspecting `requested_solver_mode`. The message combines two requirements: clean-target lower proofs and original-target graph evaluation are unqualified. [R03]

It was appropriate to refuse an unqualified target. But the two requirements belong to different capabilities. Finder has no need to prove full-MDP optimality. Once its fixed-policy checks are sound for R, keeping it behind Current's positive-lower support is unnecessary coupling.

Do not delete the guard globally. Make admission depend on **mode + resolved terminal + checking/proof profile**. Put matching preconditions at direct internal work construction so a benchmark cannot evade the safe boundary merely by bypassing the C API.

## 4. New source distinction: observer vocabulary is not the requested objective

`collect_condition_targets` collects family/group predicates from every nondefault graph edge, including nonterminal tests. `derive_model` builds a `GoalSpec` from that union and sets its internal threshold to all collected entries, then uses it to build the evaluator's calculation/observation layout. It deliberately supports count/rarity-only graphs. [R06]

The evaluator's final success mass, on the other hand, is accumulated according to `StrategyNode::terminal_kind` and reached graph routing. Its local gated-repeat proofs inspect the actual route condition/targets. [R07]

Consequences:

- A default-clean internal observation `GoalSpec` does **not by itself prove** that every evaluated graph requires a clean final item.
- Conversely, a graph reaching its self-declared success terminal does not prove that it satisfies the user's request.
- Replacing the observer goal with the requested target can drop nonterminal observers and make an otherwise valid calculation unsound.
- Copying only `extras=Allow` into an “all branch targets” goal can also be wrong for an any-k request, another rarity, or additional tested families.
- Existing exact/gated compression is a separate correspondence obligation; faithful generic routing alone is insufficient to qualify every accelerated path.

This is a source-confirmed reason to run small native differential tests first, not a claim that R has already passed native acceptance or that all historical outputs are wrong. U0 tests the precise boundary. U1 uses the smallest change supported by those tests.

## 5. Existing acceptance to reuse

Finder already checks exact start identity, original-goal success guards, operation scope and compiler-owned dependency occurrences. It correctly rejects default-edge goal decoration and revalidates every reached programme entry. Current already has compiled assertion, paired product/certification graphs, graph-bound provenance, properness and original-root value checks. [R08–R10]

Do not replace either with a generic “success_probability == 1” test. Share request-target assessment and target-match evidence; preserve the lane-specific compiler and programme guarantees. Current's cached assertion reuse must retain the same target/scope identity, and Finder must not accept a reused check under changed prices or target.

## 6. Direction selected

Complete the missing shared checking capability, unblock R in Finder when qualified, and implement a deliberately modest target-neutral Current profile to perform the unresolved model comparison. Keep public defaults and ordinary clean Current proof intact.

This is not a claim that R is an improvement over L. It is a necessary experiment about two different requested tasks. A future default migration, general occupancy UI, stronger target-specific proof models, same-clean retention search and Finder-to-Current incumbent import each require their own decisions.

The immediate success criterion is **two useful, honestly scoped native consumers of one qualified target boundary**, not an arbitrary economic percentage or a new architecture diagram. The next roadmap can then choose a same-clean algorithm based on actual evidence rather than another refused R run.

Source labels refer to the pinned [source index](SOURCES.md).
