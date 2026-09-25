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

## F2 boundary — Chaos/Annul recovery

The first heterogeneous controller attempt used the same synthetic ten-mod session as the compile tests. Its original root was an empty Rare item; the native goal required clean Rare prefixes with families from mods 2, 3 and 4; requested and priced actions were Chaos and Annul at one unit each. The proposed ordinary graph performed Chaos, then Annul until the explicit count fell to three or below, routing misses back to Chaos. This was generated from the original request, not imported from a saved policy.

Native strategy checking did **not** return a checked cost or policy. It threw `strategy evaluation shared-row exact attribution disaggregation residual exceeded epsilon: row=1, residual=1, tolerance=2e-12, reconstructed=2, solved=1, start_pair=0, start_row=0`. Temporary row-level diagnostics showed incoming mass from row 0 and a row-1 self transition; disabling exact exchangeable-family compression did not change the failure. The candidate was never accepted or published. The special recovery emitter and its failing test were removed; the runtime's unqualified Chaos→Annul proposal was also removed. The remaining one/two-stage setup-and-renewal grammar has passing focused tests.

This is an unresolved checker/grammar boundary, not evidence that the recovery graph is proper, that the evaluator's result is wrong, or that the finder improves A3/B3. Per the selected plan's stop rule, F2 search expansion and F3/F4 qualification stop here pending a native reconciliation of this reached-flow case. The next investigation should establish whether the graph is improper or shared-row attribution is wrong, with an exact row/pair flow argument and a bounded test before restoring any recovery template.

## Authority and implementation notes

The three identities remain separate: request/session/goal/scope/prices; finder run configuration and budget; and checked executable graph plus its evaluation receipt. An approximate feature or heuristic score may order proposals but cannot set policy availability or enter lower/exact authority. The emitted one/two-stage grammar is recurrent. A two-stage candidate preserves stage control in distinct operation nodes; every native outcome still routes through the ordinary evaluator. The initial root serializer is checked by exact parsed item identity and refuses any root fields the ordinary strategy format cannot represent.

F0 was committed at `cdcfcd8` and F1 at `b067018`. The C ABI accepts a finder selector; WASM and web do not yet expose it. No F2 recovery template remains in the runtime.
