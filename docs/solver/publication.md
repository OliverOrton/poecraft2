# Publication, Compilation, And Evaluation

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page owns the path from native candidate to returned strategy. It does not define optimality from a status string. [Policies](mathematics/policies.md) supplies the entry/properness argument, and [numerical closure](mathematics/numerical-closure.md) explains the endpoint and exactness conditions.

## Pipeline

Publication chooses a compatible verified incumbent, compiles the candidate into ordinary strategy JSON, parses that document through strategy authority, and independently evaluates the resulting operation graph. The returned strategy is the evaluated artifact, not a later recompilation of a different coarse policy.

The retained `PublicationPipeline` carries cooperative work through direct assertion, strict repair when selected by its contract, classification, and packaging. `IncumbentPortfolio` compares independently evaluated candidates; estimates and unverified numerical candidates remain separate.

Primary owners are `solver_solve_finish.cpp`, `solver_compile.cpp`, `solver_compile_conditions.hpp`, `solver_compile_serialization.hpp`, `solver_policy_assertion.cpp`, `solver_eval.cpp`, `solver_eval_resolve.cpp`, and `solver_eval_report.cpp`.

## Compiler Contract

Routers express observable distinctions between exact carriers whose continuations differ. Operation nodes invoke native actions; infrastructure nodes own start, success, off-policy failure, and any explicitly scoped recovery default.

Equivalent route signatures and identical operation regions may be shared. Compaction cannot merge semantically different conditions, change defaults, erase required checkpoint or observation state, or substitute an action label for its literal operation/resource payload.

Observed-choice routers remain tied to the pre-choice carrier that produced the offer. The compiler must not use information the executable strategy cannot observe. A fixed program may compress mandatory internal operations only while preserving the actual allowed decision timing.

Terminal routing expresses the native rarity, slot/tier threshold, and junk-free explicit-affix predicate. A relaxed goal mask is not sufficient. Unknown conditions or unsupported vocabulary fail rather than create a second execution language.

## Direct Assertion And Closed-Domain Routing

A narrow assertion mode can compile additional already-solved behavioral representatives instead of limiting the direct artifact to the root-policy-reachable subset. It requires an exact coarse result with closed required non-goal discovery and action coverage, no relevant state/resource cap, compatible behavioral representatives, and no structured refined routing already owning the policy.

Finite selected actions, representative identity, terminal consistency, and physical routing are checked. Unmatched entries remain fail-closed. This path expands executable routing for an already solved coarse domain; it does not perform new search or grant coarse estimates independent probability authority.

The direct graph can establish a bounded executable upper before strict lift. If its independently evaluated cost differs from the coarse estimate, that mismatch remains visible and blocks using the coarse result as reconciled exact closure. Do not generalize the narrow bounded-publication allowance to other refusal paths.

## Evaluation Contract

Exact evaluation constructs the reachable product of strategy operation, item state, and relevant choice/checkpoint state. It checks properness, solves success and expected resource/cost equations, and reports off-policy mass and price completeness.

“Exact evaluation” means graph-based native evaluation rather than Monte Carlo estimation. Its arithmetic and reconciliation contract still matters. The phrase alone does not assert rational coefficients or zero numerical error.

Final accounting can replace an incomplete quotient calculation with the retained
raw/shared flow only after its occupancy, disaggregation and quotient checks
complete, with no positive-input closed component. That completed flow clears
the preliminary unresolved mass and hard unresolved flag; a recurrent entry snapshot
does not. Direct transition and absorption edges are rebuilt from raw occupancy,
including edges retained before pass-through contraction. Compressed router
traces are then reconstructed through their existing owner. Raw-attribution caps
and failed acceptance checks still refuse; numerical thresholds are unchanged. The
[flow argument](mathematics/numerical-closure.md#flow-accounting) explains the
replacement; the [campaign evidence](../active/2026-09-07-continuous-bounds/README.md)
records its qualification.

Properness from one start does not prove properness at every operation entry present in the document. Arbitrary-entry continuation certificates request and validate their entries explicitly. A default failure, unsupported route, or incomplete member domain refuses the corresponding upper.

The success calculation and off-policy accounting answer different questions. Preserve both. A strategy with mass on an unmatched route is not rescued by renormalizing the mass that reached a known goal.

## Classification and returned evidence

Termination cause, policy availability/quality, lower provenance, and exactness are separate fields. A resource-stopped solve can return a bounded proper policy; a numerical or exhausted candidate can leave an earlier compatible upper intact.

The final classifier must bind the evaluated artifact to the same target and price/scope evidence used for the lower. An open graph value is not a substitute for a global lower. A rounded zero gap or finite upper does not by itself establish exact closure.

Product callers transfer and validate the already asserted graph. Presentation cost annotations and editable board positions do not become numerical proof inputs.

## Failure And Telemetry

Node, edge, JSON-size, pair, memory, or work caps can prevent a candidate from being certified. Invalid operations, missing prices, improperness, off-policy mass, incomplete accounting, stale identities, or cost mismatch are reported under the relevant refusal/classification contract.

Inspect candidate source and identity, direct versus strict stage, compiler graph sizes, evaluator pair counts, success/off-policy results, known-cost completeness, exact evaluated cost, and reconciliation. A failing optional candidate is not automatically loss of the incumbent already verified under the same request.

## Sampled validation is separate

Simulator executes the ordinary strategy through native sampled actions. It can expose behavioral defects but does not replace the graph evaluator or prove optimality. The owner-approved engineering validation cadence lives in `AGENTS.md`; this page does not duplicate a mandatory sample count.

The browser's existing verification button and its limitations are described in [Calculator](../product/calculator.md#current-verification-button). Changing engineering policy does not silently change that UI's implemented batch size.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md) and [flow.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/flow.md), [2026-08-30-carrier-ladder-early-executable-closure-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-early-executable-closure-v1/README.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
