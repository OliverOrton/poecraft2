# Shared target support for Current and Strategy Finder

**Reviewed repository:** OliverOrton/poecraft2  
**Pinned main:** `adb521064cd2d7b8d1dc67b6aebb258bdb2283f6`  
**Selected programme:** U0–U5. Native, diagnostic-first implementation; no public goal-default migration.

## Decision

Finish the target-support boundary left open by the Current-goal audit, through one shared request-bound policy-checking path and two explicit consumers. The experimental Finder must not require Current's all-action proof capability merely to check a policy. Current must not acquire permission to reuse clean-target proof merely because the Finder can check a coverage-target policy.

This is a capability/semantic experiment, not a claim of cheaper strategies for the unchanged clean target. It gives both search lanes actual work now while avoiding a second mechanics engine, interpreter, portfolio, or generic proof framework.

The source review found a potentially much narrower evaluator task than the preceding closeout suggests: `derive_model` collects an observation vocabulary from **all graph conditions**, while final success mass is accumulated from graph terminal routes. That observation `GoalSpec` is not automatically the original requested target. First test faithful evaluation of a small coverage graph; do not replace the entire evaluator or overwrite its observer universe on the basis of the word “goal.” Exact shortcuts and original-request acceptance still require qualification.

## Deliverables

1. A shared, original-request-bound goal-check contract actually consumed by Current's publication checks and Finder's acceptance; preserve every nonterminal graph observation.
2. Native-private coverage-only Finder operation through its own complete-policy acceptance, without the Current proof precondition.
3. A separately named, **bounded-only target-neutral Current diagnostic profile**, using the existing Current search owner, zero certified lower and no unqualified clean-dependent proof/pruning. Default clean Current remains unchanged.
4. A controlled clean/coverage experiment in each lane, plus clean preservation controls. Differences between terminal sets are model effects, not same-goal improvements.
5. Per-lane capability, refusal, proof and validation records; shared changes do not get credited to an untested consumer.

## Reading order

Start with [CODEX_PROMPT.md](CODEX_PROMPT.md), then [PLAN.md](PLAN.md). The detailed implementation contracts are [SHARED_CHECKING.md](SHARED_CHECKING.md), [CURRENT_PROFILE.md](CURRENT_PROFILE.md) and [FINDER_INTEGRATION.md](FINDER_INTEGRATION.md). Consult [MATHEMATICS.md](MATHEMATICS.md) for the arguments and counterexamples, and [VALIDATION.md](VALIDATION.md) before measurements.

[REVIEW.md](REVIEW.md) distinguishes implemented work, recorded results, source findings and untested hypotheses. [DATA_REQUEST.md](DATA_REQUEST.md) names the existing evidence locations. [DOCS_AND_ROADMAP.md](DOCS_AND_ROADMAP.md) specifies canonical integration and what remains outside this programme. [SOURCES.md](SOURCES.md) pins the evidence and primary research.

## Evidence boundary

This packet was produced by read-only GitHub inspection, review of the previous conversation/plan, and primary-source research. It contains new abstract standard-library checks, not new native or WASM measurements. No repository source, issues, commits or defaults were modified. Native feasibility, resource use and economics of the proposed consumers remain implementation tasks.

Names introduced for proposed types/flags are design names, not claims that current main accepts them. Main's existing diagnostic vocabulary and runner are reused. If main advances, inspect the delta before applying this plan. Preserve protected root `0` without inspecting it.
