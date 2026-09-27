# Pinned evidence and primary research

Repository links are pinned to `184b934396e9aba7d009f8dcbad3f488ee71dbfd` unless explicitly historical. Named source functions/ranges were inspected for this review; this is not a claim of exhaustive codebase review. Recorded results were read from the living records, not rerun.

## S1 — main, work rules and handoff

[Commit](https://github.com/OliverOrton/poecraft2/commit/184b934396e9aba7d009f8dcbad3f488ee71dbfd) · [AGENTS](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/AGENTS.md) · [HANDOFF](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/HANDOFF.md).

## S2 — completed U record

[Dual-lane target support](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/docs/active/2026-09-26-dual-lane-target-support/README.md). Basis for implemented shared checking, private Finder R, unimplemented Current neutral, run identities/statuses and qualification. Full record read.

## S3 — preceding selected plan

User-visible attachment `poecraft2_dual_lane_goal_plan_adb5210.zip`, `CODEX_PROMPT.md` (mounted prompt also read through Files). U3 requested the neutral profile but explicitly allowed a precise blocked disposition; this new packet expands the implementation authorization. Attachment checksums are in `evidence/baseline.json`. It is prior planning input, not evidence that its intended features landed.

## S4 — compiler target/control owner

[`engine/src/solver_compile.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_compile.cpp), inspected start through line 275: exact-root implicit emission, shared target-ingress guard, Finder ordinary graph/control emission.

## S5 — Current assertion reuse and acceptance

[`engine/src/solver_policy_assertion_work.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_policy_assertion_work.cpp), inspected lines 210–390: request identity in reuse, proper/executable checks and failure invalidation. U record describes the other target/root callsites.

## S6 — Current constructor

[`engine/src/solver_solve.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_solve.cpp), inspected lines 1–220 and 325–500: direct coverage refusal, profile/scope/caps, direct deterministic anchor and delayed-action scheduling.

## S7 — certified lower snapshots and selected uppers

[`engine/src/solver_solve_incremental.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_solve_incremental.cpp), lines 1070–1315: `certified_incremental_lower_values`, root-incumbent refresh and proper selected-policy improvement. Lines 1450–1745 show early publication, open-envelope continuation and post-upper snapshot service.

## S8 — delayed-row classifier

[Same incremental source](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_solve_incremental.cpp), lines 1800–2265: refinement priorities, classification setup, exact terminal/self candidate upper, unknown support and admission/NonImproving boundary. These are actual consumers to test, not an assertion that the neutral profile already exists.

## S9 — final publication/proof promotion

[`engine/src/solver_solve_finish.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_solve_finish.cpp): `run_publication_pipeline`, forced goal-cover setup, refined publication termination, exact-value assignments, result normalization and final invariant checks. Full response searched for these named sites; this is not a line-by-line audit of the whole coroutine.

## S10 — Finder construction and checking

[`engine/src/solver_finder.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_finder.cpp), lines 420–850 and 820–1100: native reached-entry validation, retained-side generator, existing progress holes, eight-attempt cap and checker admission/limits.

## S11 — generated retained-side branches

[Same Finder source](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_finder.cpp), lines 730–823: full held-side tests, target-count threshold, source/ready tier branches and Chaos fallback. This supports the *restriction*, not a measured opportunity cost.

## S12 — native side-intent vocabulary

[`engine/src/solver_options_helpers.hpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_options_helpers.hpp), lines 360–717: paid setup selection, Annul/Chaos and conditional Exalt side-intent descriptions. Lines 960–1215 show narrower protected/metamod and renewal grammar. Native mechanics remain owned by the engine; no external game-rule claims are imported.

## S13 — existing compiler control and composition interfaces

[`engine/src/solver_compile_contracts.hpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_compile_contracts.hpp), lines 1–310: `FinderControlGraph`, bindings, shared success guard, fixed controller, first-return and local-continuation compiler APIs.

## S14 — existing native candidate/evaluation/retention seam

[`engine/src/solver_solve_return_bridge.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/src/solver_solve_return_bridge.cpp), inspected local/private construction around 1310–1575 and searched complete response for `retain_certified_incumbent`, graph-local entry and compiled-artifact sites. These establish reusable ownership patterns, not universal availability or low cost for the new family.

## S15 — existing execution owners

[`docs/foundation/tooling.md`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/docs/foundation/tooling.md). The immediately preceding pinned version was read in the prior review; current AGENTS and U record reaffirm the established runner/worker paths. Re-read only relevant changed commands at implementation; do not assume new flags exist.

## S16 — existing native test harness

[`engine/tests/test_main.cpp`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/engine/tests/test_main.cpp). The preceding revision's selectors were inspected; confirm exact selectors/arguments on the working tree before calling them. No test listed in VALIDATION is claimed run natively here.

## S17 — retained-side programme history

[`docs/active/2026-09-25-seed-retention/README.md`](https://github.com/OliverOrton/poecraft2/blob/184b934396e9aba7d009f8dcbad3f488ee71dbfd/docs/active/2026-09-25-seed-retention/README.md). Full record read. It reports A3/A5 K costs, reached programme-entry validation, paid Chaos recovery counts, runtime/memory and negative comparison against Current. Those are historical K measurements, distinct from U's newer timings.

## Primary research and exact use

### P1 — fixed-point certificate separation

Chatterjee, Quatmann, Schäffeler, Weininger, Winkler and Zilken, **Fixed Point Certificates for Reachability and Expected Rewards in MDPs** (TACAS 2025; extended arXiv version). [Author paper](https://arxiv.org/abs/2501.11467). The work separates numerical computation from lightweight checked certificates and formalizes finite-MDP soundness. Used for the design principle of independent evidence; it does not establish this engine's implicit model, member correspondence, native numerical enclosure or new-target proof portability. No external checker is added by this programme.

### P2 — complete temporally extended actions

Sutton, Precup and Singh, **Between MDPs and semi-MDPs: A framework for temporal abstraction in reinforcement learning** (1999). [Publisher abstract](https://www.sciencedirect.com/science/article/pii/S0004370299000521). Options formalize policies over time. Used to keep initiation, internal execution and exit laws together; not proof of current native programme legality or of an economic gain.

### P3 — controller-sketch synthesis

Andriushchenko et al., **PAYNT: A Tool for Inductive Synthesis of Probabilistic Programs** (CAV 2021). [Open publisher chapter](https://link.springer.com/chapter/10.1007/978-3-030-81685-8_40). Read its overview and sketch/oracle architecture: a finite family of controller realizations is analyzed against a specification, with feedback supporting search over the family. Used as a foundation for explicit candidate family versus verified candidate distinctions. No claimed PAYNT performance, family-exhaustiveness guarantee or external implementation is transferred to this bounded Finder.

### P4 — policy improvement and lookahead

Bertsekas, **Biased Aggregation, Rollout, and Enhanced Policy Improvement for Reinforcement Learning** (2019). [Author paper](https://arxiv.org/abs/1910.02426). Used for base-policy/multistep improvement context. This packet derives its small complete-policy examples directly and does not import approximate aggregation or a universal SSP guarantee from the abstract.

The relevant literature supports the method distinctions. It is not evidence that this programme is novel or will improve a specific native benchmark. The source-driven candidate limitation and the unfinished Current consumer boundary determine the next work.
