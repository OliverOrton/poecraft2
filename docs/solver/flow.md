# End-To-End Solver Flow

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page maps requests, work, and ownership across native and product surfaces. General proofs live in [mathematics](mathematics/README.md); file ownership lives in [Solver internals](../foundation/solver-internals.md). Neither this flow nor an old milestone chooses the next implementation.

## Purpose

Both native experimentation and the browser use the same native mechanic, solver, compiler, and evaluation authorities:

```text
native case / Lab request ──────────────┐
                                     v
browser request → worker → WASM/C ABI → native solve
                                     → evaluated ordinary strategy
                                     → bounded or exact result
```

The native engine owns legality, transition probabilities, state semantics, pricing calculations, and strategy execution. Orchestration owns input selection, lifetime, pinned evidence, and presentation—not a second set of crafting rules.

## Shared Inputs And Identities

A request binds compiled game data, session/base/item level, concrete item, resolved goal, declared action/program scope, prices, and computational limits. Goal slots use stable group/family and tier identities with the native minimum-satisfaction semantics.

Worker handles and session-local numeric IDs are not persisted semantic identities. Saved items, goals, economies, and strategies use their stable document identities. A new source executable is not evidence that old local handles retain meaning.

## Native CLI And Lab Path

The direct benchmark validates a native case and invokes the public native solver/evaluator. The Lab wraps that benchmark in an immutable job/attempt with process supervision, artifact integrity, and compact CLI reads. The GUI is optional; no MCP transport is required.

Use the existing Lab/corpus identity and result contracts. The Lab catalogue is not the canonical game-data database, and it does not certify a lower or policy. See [Solver Lab](../foundation/solver-lab.md) and [benchmarking](benchmarking.md).

## Calculator Setup And Handle Roles

Calculator creates or imports an item under a data/session/action context. Its persistent ordinary solver supplies the current registry and exact one-action odds. Each Solve creates a separate short-lived priced scope.

Goal edits clear the old result and replace the ordinary solver as required. Base/item-level changes replace the relevant session, context, item, and solver handles. Stable drafts do not persist these handles.

The persistent odds solver and the scoped Solve solver are different objects. Narrowing the priced Solve request must not silently remove one-action mechanics from the ordinary Calculator picker.

## Exact One-Action Odds

The client sends the ordinary solver, item, and canonical action ID through the worker/facade to `pc_calc_action_outcomes`. Native calculation returns the supported/legal result and complete successor/goal-probability information without mutating the source.

There is no sampled fallback for unsupported exact vocabulary. Imprint/restore uses its dedicated compound-state path because a checkpoint is part of the semantic state. Prices change displayed cost calculations, not the underlying native probabilities.

## Solve Preparation

Calculator pins its effective economy, requests native goal-relevant candidate descriptors, and selects candidates with complete price vectors. Fracture recovery requires its native base/recovery cost where applicable. Missing prices are not free actions.

The temporary envelope solver closes, and the product opens a fresh solver for the selected priced goal and loads the pinned economy. Candidate/dependency roles and state-local automatic synthesis remain native-owned. Gap targets are passed only when enabled.

Native profiles resolve product defaults. Goal-progress gating, automatic Imprint scope, and voluntary Restart restrictions must remain visible; they are not implementation details that can be omitted from an exactness comparison.

## Cooperative Native Solve

The stepped interface is:

```text
pcw_solver_solve_begin → pc_solver_solve_begin
repeated pcw_solver_solve_step → pc_solver_solve_step
pcw_solver_solve_finish → pc_solver_solve_finish
```

Native work constructs action contracts and layout, admits source-local operators, builds completed rows, prices them, and alternates search/Bellman/policy work. It retains candidates and independent lower evidence under their own authorities.

Publication work is also cooperative: direct assertion, optional strict repair, evaluation, and classification occur before the final result transfer. A numerical candidate remains unverified until it passes the relevant executable checks. A previously verified incumbent is not replaced by a promising estimate.

The current closed-domain assertion has narrow exact-coarse/closed/uncapped preconditions. It can make already-solved representatives executable while unmatched routes stay fail-closed. Independently evaluated bounded publication does not erase a source estimate mismatch or remove the need for strict exactness.

Goal-progress-gated rows can use a documented zero-progress retry basin only within that restricted scope. Partial-progress items retain their ordinary exact action-relevant state. A repeat-reforge incumbent additionally needs its complete action-local renewal witness; `cost / success_probability` alone is not enough.

An incremental delayed envelope can improve a restricted policy before all alternatives finish. An open envelope blocks unrestricted exactness, while independent global lower evidence and a verified artifact may still be returned. Proof retirement closes only its named carrier/operator obligation and creates no executable transition.

The worker adapts native work and yields to its event loop. Cancellation is cooperative: a queued cancel message is observed after native work returns. Unfinished work is abandoned through the existing handle lifecycle rather than promoted.

## Policy To Editable Strategy

When `policy_available` holds, the compile interface returns the already asserted ordinary strategy. Fixed-program choice routes preserve their exact observation carrier. The strategy retains its concrete start and the documented presentation/accounting annotations.

The WASM facade exposes bytes, status, and length. Bindings copy the bytes out of linear memory, clear the reusable response, and transfer the buffer from worker to main thread. The client parses once; `prepareSolverStrategy` validates/adopts the uniquely transferred document and supplies missing board positions. Opening a separate editor copy is the point at which a new document owner is created.

Output caps or unrepresentable vocabulary refuse compilation rather than invent another execution format. Browser display metadata is not numerical proof authority.

## Current Repricing And Lifetime

A price change updates displayed costs/readiness; it does not automatically rerun Solve. A later invocation pins a new economy and creates a fresh scoped solver. After summary, telemetry, and strategy transfer, the scoped solver, envelope solver, and economy handles close.

The browser therefore does not promise a live native transition cache for cheap in-place repricing. See the existing [repricing decision](../decisions.md#2026-07-18--browser-repricing-uses-rebuild-by-default). Native development replay has a separate, narrower contract.

## Exact Whole-Graph Evaluation

Strategy Builder evaluation is distinct from solving. A graph edit supersedes the previous request; product validation checks shape, the client transfers encoded graph bytes, and the worker steps native discovery, SCC/equation work, and finalization.

The result is accepted only for the current request version. Evaluation, economy, compiled-strategy, and session lifetimes are closed by their appropriate owners. Unsupported action or condition vocabulary is refused, not sampled.

The evaluator's state is the strategy-operation/item product with required observation/checkpoint information. It evaluates the supplied control graph; it does not search all alternative crafting policies. Exact graph evaluation therefore supplies policy evidence, not optimality on its own.

## Sampled Verification

The reviewed browser source displays a `Verify 10,000 runs` button. It samples the returned graph with the pinned economy and compares available aggregate cost evidence with the evaluated policy cost. This existing product behavior is separate from the engineering validation cadence in `AGENTS.md`; a documentation edit does not change the UI batch size.

The button is not the complete acceptance gate. Preserve its documented limitations concerning terminal/off-policy truth and statistical comparison in [Calculator](../product/calculator.md#current-verification-button). Keep sampled simulation, fixed-policy graph evaluation, and optimality evidence explicitly different.

## Failure And Ownership Checklist

| Boundary | Required interpretation |
|---|---|
| Unknown/unpriced action | Exclude or qualify its scope; never price it as free |
| Unsupported exact vocabulary | Named refusal, not silent Monte Carlo |
| Open graph or cap | Preserve only evidence actually certified; no fabricated success/frontier closure |
| Stale product request | Ignore its result through request/version checks |
| Cancellation | Observe cooperatively, abandon unpublished work, release owning handles |
| Document/session change | Release dependent handles before replacing their owner |
| Strategy handoff | Transfer and validate the already evaluated artifact |
| Mechanic ambiguity | Refer to Oliver, not a guessed product fallback |

## Code And Evidence Map

Browser orchestration lives in `pc-calculator.ts`, `solve-workspace.ts`, `engine-client.ts`, `engine-worker.ts`, and `engine-wasm.ts`. Strategy evaluation lives in `pc-strategy-editor.ts` and the shared client/worker path. `bindings/wasm/wasm_api.cpp` adapts the public C ABI in `engine/include/poecraft/solver.h`.

Native phase ownership is listed once in [Solver internals](../foundation/solver-internals.md). Use [change impact](../foundation/change-impact.md) when an edit crosses a handle, vocabulary, result, or binding boundary. Read the linked mechanism relevant to the task; the entire flow is not mandatory startup for a local fix.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/flow.md) and [solver-lab.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-lab.md), [solver-result-presentation.ts](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/apps/web/src/app/solver-result-presentation.ts), [calculator.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/product/calculator.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
