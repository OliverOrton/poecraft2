# Source and evidence index

All repository links are pinned to the reviewed main. A source-confirmed interface is different from a documented contract, a recorded experiment, an inference, and a proposed type. This was a targeted source/architecture review, not a build, native benchmark, or exhaustive audit.

## Supplied direction

**U01:** [Verbatim supplied handoff](inputs/user_direction.original.md). Its discovery/check/proof separation, optional bounds/ML/GPU ideas and shared UI request requirements are inputs to this review. Algorithm and implementation selections in the packet are recommendations, not claims that the handoff already decided them.

## Repository sources

**R01 — Branch and commit.** https://github.com/OliverOrton/poecraft2/commit/28a955056a41699d1fa9996ee345b70678937f21
Fresh main/parent/message; final branch recheck matched.

**R02 — Current working rules and handoff.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/AGENTS.md
Read alongside https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/HANDOFF.md. Actual selected task, source restoration, ownership and validation rules.

**R03 — Native boundary programme closeout.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/active/2026-09-24-native-boundary-repair/README.md
Recorded counts/context/positive and negative candidate outcomes; diagnostic profile only.

**R04 — Compact native boundary evidence.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/active/2026-09-24-native-boundary-repair/evidence/n0-n2-native.json
Exact cost, graph/build/source/report identities. Raw out files were NOT available to this review.

**R05 — Source-ownership map.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/foundation/solver-internals.md
Existing subsystem and fragment/proof ownership; not proof that every caller was audited.

**R06 — Existing end-to-end flow.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/solver/flow.md
Native inputs, Calculator priced vs odds contexts, policy evaluation and export ownership.

**R07 — Native API lifecycle.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_api.cpp
Read source lines 1–265, 470–690, 1370–1665, 1800–2070. Registry/goal/options, direct SolveWork ownership, lifecycle; not whole-file correctness audit.

**R08 — Shared Calculator request construction.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/apps/web/src/app/solve-workspace.ts
Read 1–275: frozen CalculatorSolveRequest, delivery trace, goal modes, price filtering and strategy adoption.

**R09 — Current product protocol.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/apps/web/src/app/engine-protocol.ts
Read 430–715: goals, fixed options, action contracts, result/status/option fields. Remaining callers must be reconciled by implementation.

**R10 — Native descriptor synthesis.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_options_helpers.hpp
Read 290–420. Also https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_options_automatic.cpp, lines 400–700 staging/accounting. No claim that a polished lazy descriptor API already exists.

**R11 — Current policy compiler contract.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_compile_contracts.hpp
Read 1–290: SolveResult-based compile input, program emission and composition contracts. Proposed candidate-only view is new work.

**R12 — Numerical authority.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/solver/mathematics/numerical-closure.md
Read 1–230: native/model/numerical/optimality distinctions and current open correspondence obligations.

**R13 — Existing private fragment result.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/archive/2026-08-28-verified-executable-graph-fragment-core-v1/result.md
Historical retained single-entry probability-free IR; benchmark-only, FinalSuccess flattening. Not product/whole finder qualification.

**R14 — Evaluator target/observation collection.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_eval_helpers.hpp
Read 730–1030: collect_condition_targets and native operation matching. Supports need to audit external requested-goal binding; not a demonstrated published-policy bug.

**R15 — Evaluator request interface.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_eval_types.hpp
Read 385–440: evaluator options, native limits, continuations/decision entries. New external-target acceptance adapter must be qualified.

**R16 — Native-feasibility corpus and splits.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/fixtures/solver-natural-t1/v1/README.md
Current corpus: 14 smoke/120 short/12 deep, native feasibility owner, generator strata and evidence distinctions; not a training-throughput measurement.

**R17 — Existing heuristic calibration.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_dirty_guidance.hpp
Read 1–215: distinct cost/count rewards and same-controller run-local heuristic calibration.

**R18 — Existing complete artifact checks.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/engine/src/solver_solve_constructive.cpp
Read 1–225 and 4700–4900: context/economy/graph/properness checks and complete sparse-row candidate staging. No claim of independent ready-to-call root seed API.

**R19 — Tooling map.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/foundation/tooling.md
Owner map referenced by current AGENTS/flow; command syntax must be rechecked with installed --help. Prior identical-path excerpt read in preceding review; do not infer runtime paths exist on this host.

**R20 — Calculator product/entrypoint map.** https://github.com/OliverOrton/poecraft2/blob/28a955056a41699d1fa9996ee345b70678937f21/docs/product/calculator.md
Read lines 1–70: native goal/odds ownership and actual component path `apps/web/src/app/components/pc-calculator.ts`. The historical audit dates on that page remain historical; new-mode behavior is proposed.

## Hosted checks

https://github.com/OliverOrton/poecraft2/actions/runs/36066680514 — Windows: completed success at final review.
https://github.com/OliverOrton/poecraft2/actions/runs/36066680501 — Solver knowledge: completed success on individual final run recheck.
These hosted results qualify the committed source under their own configured tests, not the proposed finder.

## Primary research and transfer limits

**P01 — Andriushchenko et al., PAYNT: A Tool for Inductive Synthesis of Probabilistic Programs (CAV2021).** https://link.springer.com/chapter/10.1007/978-3-030-81685-8_40
Read primary publisher text on sketch families, oracle-guided synthesis and fixed-program checking. Supports controller-sketch formulation; no direct import of its quotient/synthesis algorithms or runtime claims.

**P02 — Hansen and Zilberstein, LAO*: A heuristic search algorithm that finds solutions with loops (2001).** https://www.sciencedirect.com/science/article/pii/S0004370201001060
Primary publisher search record reviewed for cyclic policy-graph formulation. No claim that the new bounded beam inherits LAO* convergence or optimality.

**P03 — McMahan, Likhachev and Gordon, Bounded Real-Time Dynamic Programming (ICML2005).** https://www.cs.cmu.edu/~ggordon/mcmahan-likhachev-gordon.brtdp.pdf
Read author-hosted abstract and bound/SSP premises. Useful focused upper/lower precedent; no transfer of guarantees to heuristic pruning without those premises.

**P04 — Kocsis and Szepesvari, Bandit Based Monte-Carlo Planning (ECML2006).** https://link.springer.com/chapter/10.1007/11871842_29
Publisher abstract explicitly gives finite-horizon/discounted setting. Informs later macro-MCTS alternative; does not certify undiscounted native SSP policies.

**P05 — Anthony, Tian and Barber, Thinking Fast and Slow with Deep Learning and Tree Search (2017).** https://arxiv.org/abs/1705.08439
Primary abstract on Expert Iteration and its tested domain. Planning/generalization feedback is relevant; no presumed poecraft2 training benefit or optimal teacher labels.

**P06 — Zaheer et al., Deep Sets (2017).** https://arxiv.org/abs/1703.06114
Primary abstract on set/permutation-aware representations. Justifies a later modeling option, not equality of unequal physical goal bindings.

**P07 — Kumar et al., Conservative Q-Learning for Offline Reinforcement Learning (2020).** https://arxiv.org/abs/2006.04779
Primary abstract on dataset/policy distribution shift. Its learned reward bounds are not native admissible cost lowers here; algorithm not selected.

**P08 — Chatterjee et al., Fixed Point Certificates for Reachability and Expected Rewards in MDPs (TACAS2025).** https://arxiv.org/abs/2501.11467
Primary abstract on certification/checker separation. Does not provide current native model/goal/numerical correspondence automatically.

**P09 — Ravindran and Barto, Relativized Options: Choosing the Right Transformation (ICML2003).** https://www.cse.iitm.ac.in/~ravi/papers/ICML03.pdf
Author-hosted paper retrieved. Relative control representations are precedent; approximate transfer is not native law equivalence.

## Execution evidence and limits

Only packet construction/validation and the included exact-rational toy contract tests were executed in this review. No native build, new solver run, ML training, benchmark, simulation, visual browser review, repository edit or push was performed. A direct raw-source download attempt from the container failed DNS; source review used the GitHub connector. No claim depends on that failed download.
Raw existing source/research context and the attached direction were sufficient to select the architecture. Missing real data-generation throughput, best initial controller grammar and eventual comparative quality are explicitly measurements for F0–F4, not guessed results.
