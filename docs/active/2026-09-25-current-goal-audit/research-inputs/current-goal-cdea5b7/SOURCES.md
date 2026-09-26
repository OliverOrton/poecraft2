# Pinned source index and primary research

Repository sources were read via the GitHub connector. A source inspection is not a new native performance measurement. Source line ranges below are navigation aids at the pinned revisions, not a claim that surrounding uninspected files were exhaustively audited.

## Current main and completed work

- **R1:** [Current head](https://github.com/OliverOrton/poecraft2/commit/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029); [AGENTS](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/AGENTS.md).
- **R2:** [Current HANDOFF](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/HANDOFF.md).
- **R3:** [K seed/retention living record](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/active/2026-09-25-seed-retention/README.md); includes actual native results and out-paths.
- Current-head [Windows run 36197568022](https://github.com/OliverOrton/poecraft2/actions/runs/36197568022) and [knowledge run 36197568120](https://github.com/OliverOrton/poecraft2/actions/runs/36197568120) both completed successfully during this review.

## Historical boundary and comparison

- **R4:** [Semantic-boundary commit](https://github.com/OliverOrton/poecraft2/commit/2b8d5acd8b7cd3598dca1756f7b5d53ab914c692); parent `82224d0b5075d32cb920fa889c6536a7121ffc70`, 2026-08-23 09:13:32 UTC.
- **R5:** [Parent CalcContext](https://github.com/OliverOrton/poecraft2/blob/82224d0b5075d32cb920fa889c6536a7121ffc70/engine/src/solver_calc.cpp), `CalcContext::is_goal_state`.
- **R6:** [Parent compiler](https://github.com/OliverOrton/poecraft2/blob/82224d0b5075d32cb920fa889c6536a7121ffc70/engine/src/solver_compile.cpp#L435-L458), rarity plus all/at-least requested predicates.
- **R7:** [Boundary clean-goal result](https://github.com/OliverOrton/poecraft2/blob/2b8d5acd8b7cd3598dca1756f7b5d53ab914c692/docs/active/2026-08-22-exact-goal-carrier-ladder/result.md), clean 87,361.169-Chaos C5 upper, no optimality closure; archived experiment scope.
- **R9:** [Parent HANDOFF](https://github.com/OliverOrton/poecraft2/blob/82224d0b5075d32cb920fa889c6536a7121ffc70/HANDOFF.md); [actual old Imprint report](https://github.com/OliverOrton/poecraft2/blob/82224d0b5075d32cb920fa889c6536a7121ffc70/docs/active/2026-08-22-five-t1-goal-band-recovery/imprint-control-final.json). [actual old Warlord input/status/evaluation excerpt](https://github.com/OliverOrton/poecraft2/blob/82224d0b5075d32cb920fa889c6536a7121ffc70/docs/active/2026-08-22-five-t1-goal-band-recovery/warlord-control-final.json). Both reports were inspected, not independently re-evaluated during this review; full reached-terminal equivalence is still unestablished.

## Actual goal consumers

- **R8:** [Current CalcContext terminal](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_calc.cpp#L1316-L1338).
- **R10:** [Compiled terminal](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_compile_conditions.hpp#L345-L393), `exact_goal_condition`.
- **R11:** [One-action summary](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_api.cpp#L1784-L1880), `pc_calc_action_outcomes`.
- **R12:** [Calculator product reference](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/product/calculator.md), Exact One-Action Result section.
- **R13:** [GoalSpec](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_model.hpp#L333-L371); [v1 parser](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_api.cpp#L280-L400).
- **R14:** [Lower setup/projection](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_solve_bounds.cpp#L1-L250); [typed proof patterns](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_proof_pattern_manager.hpp#L1-L205).
- **R15:** [Phase lower probability](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_phase_probability.cpp#L325-L450), scope/frame/goal-domain validation; also its crafted-observation and potential identity definitions near the start.
- **R16:** Existing target-dependent consumers to complete in the implementation map: [reforge](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_reforge.cpp), [options](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_options.cpp), [option synthesis](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/engine/src/solver_options_helpers.hpp). These are navigation targets; this review does not assert every current call site was audited.
- **R17:** [Scope contract](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/solver/request-action-scope.md), [lower mathematics](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/solver/mathematics/lower-bounds.md), [policy mathematics](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/solver/mathematics/policies.md), [numerical contract](https://github.com/OliverOrton/poecraft2/blob/cdea5b7f9556ae30c43f9a74249d64b3dcf0f029/docs/solver/mathematics/numerical-closure.md). Use the existing canonical owners rather than adding competing semantics.

## Primary outside research

- **P1:** PRISM manual, [Reward-based properties](https://www.prismmodelchecker.org/manual/PropertySpecification/Reward-basedProperties), reachability-reward target and stopping semantics. Used to distinguish model/property selection from numerical exactness. Its convention for nontermination is not assumed to be identical to every native query.
- **P2:** Chatterjee, Quatmann, Schäffeler, Weininger, Winkler and Zilken, *Fixed Point Certificates for Reachability and Expected Rewards in MDPs*, TACAS 2025 / arXiv:2501.11467. [Paper](https://arxiv.org/abs/2501.11467), [HTML v1](https://arxiv.org/html/2501.11467v1). Target-specific fixed-point certification and explicit validity premises; not a new native checker implementation or a substitute for native model correspondence.
- **P3:** Bertsekas, *Biased Aggregation, Rollout, and Enhanced Policy Improvement for Reinforcement Learning*, 2019. [Abstract/source](https://arxiv.org/abs/1910.02426). Context for using a genuine continuation value instead of zero in a subproblem. No approximate aggregation or rollout algorithm is selected by this packet.

No online game-mechanics facts were used. Native code/data remains the mechanics authority. The archive/direct-source contents establish the history; external papers do not establish which historical poecraft2 result was comparable.
