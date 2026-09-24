# Source index

Repository source pin: `9032fee114aea3481482c57cdd30248cdc29a5bc`. Ranges are inspected source ranges, not automatic proof of all callers. Search results sometimes identified the prior default-head index; substantive findings above were fetched at the pinned main.

## R1 — Main and HANDOFF
[HANDOFF.md](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/HANDOFF.md)
Inspected: whole. Current D1 stop and unchanged runtime authority.

## R2 — D1 living record
[docs/active/2026-09-23-downstream-role/README.md](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/docs/active/2026-09-23-downstream-role/README.md)
Inspected: whole. Scope, timings, negative and documentation disposition.

## R3 — D1 compact census
[docs/active/2026-09-23-downstream-role/evidence/d1-native-census.json](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/docs/active/2026-09-23-downstream-role/evidence/d1-native-census.json)
Inspected: whole. Run A/B identities and distinct timer populations.

## R4 — Strict session and initialization call
[engine/src/solver_policy_refinement.cpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_policy_refinement.cpp)
Inspected: 2180–2560. Persistent session start, initial owner, locator iteration.

## R5 — Closure and partition input preparation
[engine/src/solver_policy_refinement.cpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_policy_refinement.cpp)
Inspected: 2570–2875. Existing payload interning, requirement assembly, replay node cache, timer end.

## R6 — Oracle initialization
[engine/src/solver_policy_oracle_setup.inc](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_policy_oracle_setup.inc)
Inspected: 1–305. Coarse discovery/indexing, vocabulary, strict child, observations.

## R7 — Oracle observation input/output construction
[engine/src/solver_policy_oracle_observations.inc](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_policy_oracle_observations.inc)
Inspected: 1–90. Copied input payloads and repeated whole-vector accounting.

## R8 — Observation canonicalization and grouping
[engine/src/solver_refinement_observation.cpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_refinement_observation.cpp)
Inspected: 1–315. Canonical required fields, actual features, authority-based groups.

## R9 — Observation propagation rounds
[engine/src/solver_refinement_observation.cpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_refinement_observation.cpp)
Inspected: 315–560. Frozen synchronous rounds, transfer/merge, result diagnostics.

## R10 — Existing graph-selection interning/sharing
[engine/src/solver_refinement_graph_discovery.hpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_refinement_graph_discovery.hpp)
Inspected: 1–230 and 305–465; search selected_action_source. Existing selected-action interning/kernel sharing and another producer of one-level authorities; inspect the exact source-construction block before reuse.

## R11 — Oracle memory and exact feature materialization
[engine/src/solver_policy_oracle_kernels.inc](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_policy_oracle_kernels.inc)
Inspected: 490–725. Existing 512-check audit and distinct physical-feature extraction.

## R12 — Nested-memory helper inventory
[engine/src/solver_policy_refinement_helpers.hpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_policy_refinement_helpers.hpp)
Inspected: 465–600. Full vector traversals and nested-byte-only ExactState helper.

## R13 — Canonical search/resumption mathematics
[docs/solver/mathematics/search-and-resumption.md](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/docs/solver/mathematics/search-and-resumption.md)
Inspected: 200–380. Witness, staged setup, binding scope, timer/performance distinctions.

## R14 — Operating rules
[AGENTS.md](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/AGENTS.md)
Inspected: whole. Protected root, no push, native authority, proportional testing.

## R15 — Experiment owners
[docs/foundation/tooling.md](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/docs/foundation/tooling.md)
Inspected: previous pin 9b1fb8e, 1–165; resolve current before execution. Existing runner/Lab/worker; not a new execution platform.

## R16 — Established six-case identity
[docs/active/2026-09-23-role-parametric/corpus/manifest.json](https://github.com/OliverOrton/poecraft2/blob/9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7/docs/active/2026-09-23-role-parametric/corpus/manifest.json)
Inspected: previous pin 9b1fb8e; no current case edit observed. A4/C5/bow IDs and ordinary profile.

## R17 — Native observation input contract
[engine/src/solver_refinement.hpp](https://github.com/OliverOrton/poecraft2/blob/9032fee114aea3481482c57cdd30248cdc29a5bc/engine/src/solver_refinement.hpp)
Inspected: 174–310. PolicyObservationNode one-level authority fields, fixed-point result and closed-partition contracts.

## Primary external research

[P1 — Clang/LLVM, Data flow analysis: an informal introduction](https://clang.llvm.org/docs/DataFlowAnalysisIntro.html). Official conceptual guide; read formalization/fixpoint and changed-input worklist discussion. No new native guarantee follows.

[P2 — Neil D. Jones, Carsten K. Gomard and Peter Sestoft, Partial Evaluation and Automatic Program Generation (1993)](https://studwww.itu.dk/people/sestoft/pebook/). Author page explains static/dynamic specialization. Search-result text was available; direct open failed. Not a full-book review.

[P3 — Paul Gainer, Ernst Moritz Hahn and Sven Schewe, Incremental Verification of Parametric and Reconfigurable Markov Chains (2018)](https://arxiv.org/abs/1804.01872). Primary abstract and university record identify stable-substructure reuse. No whole-paper technical claim or transferred speedup is relied on.

[Additional searched primary abstract — Accelerated Model Checking of Parametric Markov Chains (2018)](https://arxiv.org/abs/1805.05672). Engineering/data-structure contribution is relevant context. HTML full text was unavailable; no full-text conclusion is claimed.

## CI and branch identity

[Pinned commit](https://github.com/OliverOrton/poecraft2/commit/9032fee114aea3481482c57cdd30248cdc29a5bc).
[Windows run 36014849328](https://github.com/OliverOrton/poecraft2/actions/runs/36014849328).
[Solver knowledge run 36014849727](https://github.com/OliverOrton/poecraft2/actions/runs/36014849727).
Final observed states are in [evidence/ci.json](evidence/ci.json).

## Received prior plan

`poecraft2_downstream_role_plan_9b1fb8e/CODEX_PROMPT.md` and `DATA_REQUEST.md` were reread through Files. They selected D0–D4 and are now closed at D1. Their instructions and reported abstract checks are not runtime changes. The old audit reports at 4594e44b are historical background, not current source facts.
