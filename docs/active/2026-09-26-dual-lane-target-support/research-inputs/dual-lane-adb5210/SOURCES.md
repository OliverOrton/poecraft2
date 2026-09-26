# Pinned source and primary research index

Repository source below is pinned to **adb521064cd2d7b8d1dc67b6aebb258bdb2283f6**, except the explicitly labelled K measurement source. File positions identify reviewed ranges or named functions, not a claim that every transitive caller was audited. Source-confirmed structure and recorded benchmarks are distinct.

## R01
**Latest E living record — recorded results, scope, missing comparison and validation.**

[docs/active/2026-09-25-current-goal-audit/README.md](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/docs/active/2026-09-25-current-goal-audit/README.md)

Whole document reviewed. Contains L/E/R decisions, actual A4/A5 clean costs/lowers/work, before-work R refusal, sparse same-clean entry evidence and dirty-build qualifications. Its evaluator explanation is an interpretation to test against R06/R07, not proof that graph-defined loose success is impossible.

## R02
**K living record — retained Finder and S measurements, not a new Current gain.**

[docs/active/2026-09-25-seed-retention/README.md at cdea5b7](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/active/2026-09-25-seed-retention/README.md)

Whole record reviewed in the preceding/current conversation. K Finder A3/A5 results, costs/limits, all reached programme validation, candidate counting, original-root acquisition, timing/ownership and non-public status. The new E record independently retains this disposition. Do not use these historical timings as a matched U treatment comparison.

## R03
**Mode-independent refusal before dispatch.**

[engine/src/solver_api.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_api.cpp#L1931-L2068)

Reviewed lines 1900–2070: `pc_solver_solve` and `pc_solver_solve_begin` reject `ExtraExplicitPolicy::Allow` before selecting Current/Finder. This is a conservative capability guard, not evidence that Finder consumes Current's lowers.

## R04
**Existing native terminal/assessment types.**

[engine/src/solver_model.hpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_model.hpp#L333-L400)

Reviewed lines 325–445: `ExtraExplicitPolicy`, `GoalCountRange`, `GoalTerminalConstraints`, `GoalAssessment` and `GoalSpec`. Do not introduce a duplicate goal representation or turn current assessment into an irreversible phase.

## R05
**Fresh native diagnostic resolution and E normalization.**

[engine/src/solver_api.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_api.cpp#L613-L722)

Reviewed lines 585–725: explicit-clean validation, coverage policy set before calculator construction, separate Current/Finder handle-owned work objects.

## R06
**Condition observer collection and inferred evaluation model.**

[engine/src/solver_eval_helpers.hpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_eval_helpers.hpp)

Reviewed `collect_condition_targets` (in lines 710–1030) and `derive_model`/`derive_checked_model` (in lines 1100–1450). All nondefault conditions contribute observers; union goal construction uses all collected entries; exact class/overlap/threshold limitations and count observations are explicit. Generic count/rarity-only graphs are allowed. This inferred goal is not automatically the original request target.

## R07
**Graph-defined absorption and qualified gated routes.**

[engine/src/solver_eval.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_eval.cpp)

Reviewed finalization's `output.success_probability += terminal_mass[node]` under `PC_TERMINAL_SUCCESS`, the `EvalAbsorption`/routing lifecycle, and `condition_is_exact_zero_goal_progress` / `classify_local_gated_route` in lines 395–650. These findings distinguish graph interpretation from original-request checking; they do not establish end-to-end R qualification.

## R08
**Finder acceptance and proposal ownership.**

[engine/src/solver_finder.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_finder.cpp#L1-L350)

Reviewed `prepare_finder_candidate`, `finder_evaluation_accepted`, constructor, graph/control matching, default success rejection, original-root check, standalone/dependency scope and initial frontier. Reuse these checks; a shared target check does not supersede reached native programme validation.

## R09
**Current compiled assertion and reuse.**

[engine/src/solver_policy_assertion_work.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_policy_assertion_work.cpp#L1-L255)

Reviewed pair-default equality, `reuse_compiled_policy_assertion_evaluation`, retained bytes and work-owner fields. Preserve Current's graph pairing/properness contract while binding target evidence. Do not replace it wholesale with Finder grammar validation.

## R10
**Native evaluator identities, exact domains, result/timing vocabulary.**

[engine/src/solver_eval_types.hpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_eval_types.hpp)

Reviewed lines 1–180 and 460–650: complete continuation-member coverage, graph-local provenance tied to bytes, active stage timers, raw/replay row census, truncated final-state classes. Numeric handles and representative samples are not complete semantic authority.

## R11
**Current proof consumers and goal-specific preparation.**

[engine/src/solver_solve_carrier_pattern.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_solve_carrier_pattern.cpp#L1-L270)

[engine/src/solver_solve_bounds.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_solve_bounds.cpp)

Reviewed `completion_proof_lower_value`, carrier/identity/terminal debt readers, clean-projection work in lines 2640–3020 and `optimistic_completion_cost` at the end. This inventory is a starting map, not exhaustive proof-consumer certification. U3 must inspect direct readers and all affected pruning/closure callers.

## R12
**Shared native assessment and goal-dependent reforge compression.**

[engine/src/solver_calc.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_calc.cpp)

[engine/src/solver_reforge.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/src/solver_reforge.cpp)

Reviewed `assess_goal_state` and `goal_progress_gated && is_goal_state(successor)` terminal accumulation. Target changes can affect stopped/compacted programme representations even where raw primitive sampling rules are unchanged. Preserve all routing observers and qualify shortcuts.

## R13
**Tooling owner.**

[docs/foundation/tooling.md](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/docs/foundation/tooling.md)

Reviewed lines 1–210: corpus/worker/Lab identity and supervision, project interpreter, named case resolution, actual Calculator default-finish versus early-Finish, compact transport and no unbounded polling/supervisor duplication.

## R14
**Existing test selectors and fixtures.**

[engine/tests/test_main.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/tests/test_main.cpp#L1-L175)

[engine/tests/test_solver_eval.cpp](https://github.com/OliverOrton/poecraft2/blob/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6/engine/tests/test_solver_eval.cpp)

Reviewed selectors and existing native synthetic session/graph helpers. Commands in the packet use these owners rather than inventing an independent qualification harness.

## R15
**Pinned repository state and hosted checks.**

[Commit](https://github.com/OliverOrton/poecraft2/commit/adb521064cd2d7b8d1dc67b6aebb258bdb2283f6)

[Windows run 36216220418](https://github.com/OliverOrton/poecraft2/actions/runs/36216220418)

[Solver knowledge run 36216220550](https://github.com/OliverOrton/poecraft2/actions/runs/36216220550)

Windows was initially in progress and later completed successfully; build/test job 108332587320 succeeded. Knowledge completed successfully. These are inspected hosted results, not workflows rerun by the reviewer.

## Primary research

### P1 — Target-defined reward queries

[PRISM manual: Reward-based properties](https://www.prismmodelchecker.org/manual/PropertySpecification/Reward-basedProperties)

Reviewed the official reachability-reward section. It separates model/reward from the stopping property. PRISM's precise convention for non-almost-sure reachability is not silently substituted for poecraft2's native proper-policy contract. This supports the framing, not the implementation.

### P2 — Candidate calculation versus checked proof

[Chatterjee et al., Fixed Point Certificates for Reachability and Expected Rewards in MDPs, TACAS 2025 / arXiv:2501.11467](https://arxiv.org/abs/2501.11467)

Reviewed the primary abstract and scope: finite-MDP target-specific certificates, elementary fixed-point reasoning, formal soundness/checker. This is precedent for explicit proof assumptions and separate checking. It does not supply the native implicit-model correspondence, floating-point enclosure or all-action coverage required here.

### P3 — Selective search with executable bounds

[McMahan, Likhachev and Gordon, Bounded Real-Time Dynamic Programming, ICML 2005](https://publications.ri.cmu.edu/bounded-real-time-dynamic-programming-rtdp-with-monotone-upper-bounds-and-performance-guarantees)

Reviewed the primary author/institution record. Selective search with upper/lower bounds is established. We are not installing a new BRTDP engine, importing benchmark speedups, or treating Finder heuristic pruning as exact action retirement.

## Novelty and attribution

The proposed U work is an engineering/native-correspondence capability, not a new target-inclusion theorem or new SSP search algorithm. Its value is resolving a concrete shared-model/request/checking boundary with two actual search clients and honest proof support. Broad research novelty remains unassessed in this packet.
