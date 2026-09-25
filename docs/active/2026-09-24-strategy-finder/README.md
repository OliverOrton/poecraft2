# Experimental strategy finder — living record

Oliver selected execution of the received [F0–F4 packet](research-inputs/poecraft2_strategy_finder_plan_28a9550/IMPLEMENTATION_PLAN.md) at reviewed main `28a955056a41699d1fa9996ee345b70678937f21`. ZIP SHA-256: `ddd3d0b81babd68bb1e6ef0cc9f0d9cb794b7b627fb16d2a3345fe80b0783f71`. All 20 manifest payloads passed path, length and SHA-256 checks before verbatim import. The packet is a proposed design selected by Oliver, not an existing engine contract or mechanics ruling.

## F0 — native acceptance boundary

The current C API creates a goal-bound `CalcContext` at `pc_solver_create`, then `pc_solver_solve_begin` directly owns a legacy `SolveWork`. The generic strategy evaluator otherwise reads a graph's self-declared success terminal. The legacy policy compiler supplies the trusted goal relationship; a separate proposer therefore needs explicit original-request binding before evaluation. The first finder adapter adds native candidate graph emission for one or two paid primitive stages and a separate preparation gate. It does not forge `SolveResult` or publish a policy.

The preparation gate parses the ordinary graph through the existing strategy compiler; requires the exact original physical start item; requires every incoming success edge to carry the native compiled clean-terminal goal predicate; and requires every operation to resolve to a requested primitive action. A candidate with no such guarded success ingress is refused. The price table is supplied to the subsequent independent evaluator, whose complete-cost, success, failure, off-policy and properness results decide acceptance. This is a deliberately narrow supported grammar. Unknown native macro interiors are not flattened into primitive lists.

Focused `compile` tests passed: valid original-target zero-action terminal, changed Eldritch start context, fake success predicate, out-of-scope Exalt, and a priced versus unpriced finite one-attempt graph. A constructed one-action Chaos loop against an impossible clean target did not produce a valid root policy; no economic label was assigned to it.

## F1 — peer finder and C lifecycle

`PolicyFinderWork` now proposes native primitive controllers from the request root and sends complete ordinary graphs through the F0 gate and independent priced strategy evaluator. It retains the cheapest checked graph only when cost is complete, execution is proper and eventual success is one. One checker is live at a time. `solver_mode` is appended to the C options struct; omitted/old options preserve Current. The handle dispatches begin, step, bounded Finish, abandon, summary, graph output, telemetry and memory separately from `SolveWork`. Finder summaries have no lower bound, gap or optimality convergence. Per-state proof queries and proof handoff are unavailable. An unknown mode is rejected.

Two small native original-root fixtures produce checked ordinary one-action graphs: a Magic prefix goal and a Magic suffix goal with priced Alteration renewal. A Normal-root staged Transmutation→Alteration graph is also checked. The initial supported domain is these simple renewal and paid rarity-setup patterns; this is not broad PoE coverage. No-priced-seed, early Finish and tiny-memory cases are covered. Focused compile tests passed (1,403 checks) and API tests passed (2,957 checks). The API tests cover mode validation, no-policy bounded Finish, output and abandon; they do not yet show a completed full-data Calculator finder run.

The two-stage emitter routes each miss from setup to renewal and each miss from renewal back to renewal. It has no invented reset. F2 must add bounded controller search and a heterogeneous checked case. F3 must propagate mode through WASM and the web UI. F4 comparisons remain unrun.

## Authority and implementation notes

The three identities remain separate: request/session/goal/scope/prices; finder run configuration and budget; and checked executable graph plus its evaluation receipt. An approximate feature or heuristic score may order proposals but cannot set policy availability or enter lower/exact authority. The emitted one/two-stage grammar is recurrent. A two-stage candidate preserves stage control in distinct operation nodes; every native outcome still routes through the ordinary evaluator. The initial root serializer is checked by exact parsed item identity and refuses any root fields the ordinary strategy format cannot represent.

The first checkpoint was committed at `cdcfcd8`. The current uncommitted checkpoint changes the native C ABI, finder peer, two-stage emitter, and focused compile/API tests. Web and WASM behavior is still unchanged.
