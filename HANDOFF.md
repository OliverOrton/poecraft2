# Session Handoff — Strategy Calculator Mode Phase B next

Written 2026-07-14 after completing Phase A of
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md).
Read [AGENTS.md](AGENTS.md), then [docs/direction.md](docs/direction.md).

## Current state

Strategy Calculator Mode Phase A is complete. The native engine can evaluate
an already-compiled strategy graph as an absorbing Markov chain over graph
nodes and solver states; no strategy JSON is reparsed.

Implemented in `engine/src/solver_eval.cpp`:

- operation nodes resolve to registry descriptors, including fossil loadouts
  and synthetic restart;
- graph conditions derive family/group targets and compile to abstract-state
  predicates with simulator parity;
- unsupported operations and structural condition gaps are refused together
  with element-level node/edge messages;
- forward propagation reports terminal/failure/unresolved mass, expected
  actions, per-price-key consumption, node visits/classes, and edge flows;
- router cycles stop at the sweep cap and retain per-node unresolved mass;
- count/rarity-only graphs use an explicit empty-goal construction path while
  ordinary `pc_solver_create` goals still require at least one slot;
- deterministic JSON serialization backs the query-required-count C ABI
  `pc_strategy_evaluate` and `pc_strategy_eval_options` in
  `engine/include/poecraft/solver.h`.

Whole-graph evaluation uses a strict junk partition that distinguishes the
complete exclusion-group effect mask. This fixed the abstraction drift found
by the post-Regal regression: exact expected actions are `51.637347`, versus
`51.539133` over 30,000 simulator runs. The ordinary DP solver deliberately
keeps its existing compact, approximately-sound partition; only strategy
evaluation enables the strict refinement.

## Verification

- `powershell -File scripts/build.ps1` — pass.
- `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`
  — 118,285 checks, 0 failures.
- The new `test_solver_eval.cpp` is registered in all required locations:
  `engine/CMakeLists.txt`, `scripts/build.ps1` `TestSources`, and
  `tests.hpp`/`test_main.cpp`.
- Tests cover closed forms, the strict abstraction regression, a 30k-run rich
  exact-vs-MC graph, Vaal Regalia exact-vs-MC, condition parity, illegal and
  no-edge failures, priority/default routing, price-key/action resolution,
  refusals, unresolved cycles, deterministic JSON, C ABI queries, and
  per-sweep mass conservation.

## Next task

Implement **Phase B only** from
[strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md):

1. add `pcw_strategy_evaluate` in `bindings/wasm/wasm_api.cpp`;
2. rebuild WASM with `scripts/build-wasm.ps1`;
3. add worker/protocol/client strategy-evaluation plumbing;
4. add the happy-path and refusal engine-smoke tests;
5. run the Phase B web gates and the full `scripts/test.ps1` gate specified by
   the plan.

Do not begin Phase C UI/design work as part of Phase B. After Phase C, resume
[s6-plan.md](docs/s6-plan.md) Phase 1.

## Important files

- `engine/src/solver_eval.cpp`
- `engine/tests/test_solver_eval.cpp`
- `engine/src/solver_internal.hpp`, `solver_abstract.cpp`, `solver_calc.cpp`
- `engine/src/solver_api.cpp`, `engine/include/poecraft/solver.h`
- `engine/src/engine_internal.hpp`, `simulator.cpp`
- `docs/strategy-calculator-mode-plan.md`

## Rulings and gotchas

- The strict exclusion-effect partition is evaluator-only. Do not silently
  switch the DP solver to it; that changes its state-space/performance model.
- Evaluator support comes from `calc_supports(descriptor)`, not a second
  hardcoded refusal list in bindings or UI.
- Illegal operation attempts count toward expected actions, matching the
  simulator, but do not consume price keys.
- Result costs remain price-independent consumption vectors; Phase B and the
  UI must not introduce crafting-rule or routing authority.
- `pcw_*` JSON uses six-decimal doubles, so web probability-sum assertions need
  the plan's serialization tolerance.
- Phase A added a C ABI but intentionally stopped before WASM/bindings; Phase B
  must rebuild the WASM artifact before running web tests.
- Commits remain local unless Oliver explicitly asks to push.
