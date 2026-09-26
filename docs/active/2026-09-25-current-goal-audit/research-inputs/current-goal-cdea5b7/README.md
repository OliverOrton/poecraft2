# Current solver: terminal semantics, historical comparability, and clean-target search

**Reviewed main:** `cdea5b7f9556ae30c43f9a74249d64b3dcf0f029` — Add native retained-side finder and seed observation.  
**Historical semantic boundary:** `2b8d5acd8b7cd3598dca1756f7b5d53ab914c692`, 23 August 2026.  
**Task:** research and implementation plan; no repository changes or native benchmark runs were made in this review.

## Decision

Pause further Finder-grammar expansion. Select E0–E5 in [PLAN.md](PLAN.md): make the Current solver's final-goal contract explicit, audit the historical model boundary, compare clean and coverage-only targets safely, and characterize one Current clean-target continuation bottleneck. Preserve the successful K Finder work, but do not call it a Current fix.

Three experiments answer different questions:

| Arm | Target | Purpose |
|---|---|---|
| L: legacy clean | Current goal: rarity, requested threshold, and no unmatched explicit affixes | Baseline and compatibility |
| E: explicit clean | Coverage plus explicit occupancy constraints proven equivalent to L for A4/A5 | Same-model representation/control check |
| R: coverage only | Rarity and requested threshold; extras permitted | Deliberately different-model diagnostic |

A cheaper or easier R result is **not** a strategy improvement for L. Merely rewriting L as E is **not** an algorithm improvement. A subsequent Current treatment must still solve L/E with complete native policy checking.

## Read and execute

[CODEX_PROMPT.md](CODEX_PROMPT.md) is the self-contained executor instruction. [REVIEW.md](REVIEW.md) and [HISTORY.md](HISTORY.md) establish the evidence. [GOAL_CONTRACT.md](GOAL_CONTRACT.md), [CONSUMER_MAP.md](CONSUMER_MAP.md), and [EXPERIMENTS.md](EXPERIMENTS.md) specify the implementation and measurement contracts. [MATHEMATICS.md](MATHEMATICS.md) contains derivations and counterexamples; [VALIDATION.md](VALIDATION.md) and [DOC_MAINTENANCE.md](DOC_MAINTENANCE.md) define acceptance.

[PRODUCT_DECISION.md](PRODUCT_DECISION.md) recommends a future explicit extras/occupancy interface, without authorizing a default migration. [DATA_REQUEST.md](DATA_REQUEST.md) identifies missing local evidence. Sources are in [SOURCES.md](SOURCES.md).

## Non-goals

No global rollback of the August commit; no one-line terminal patch; no silent v1/default change; no broader action budget, neural model, new solver lane, all-action quotient, generic cache, or renewed O/S tuning. Do not import Finder's roughly 524k A5 policy as though it improves Current's roughly 85.6k controller. Do not rewrite historical evidence or reinterpret old `exact` labels without their declared scope.

## Evidence grades

**Source-confirmed** means an inspected pinned implementation or commit. **Recorded** means a repository result, not a new measurement. **Derived** means a stated mathematical/arithmetic implication. **Proposed** means not implemented or qualified by this packet. Command flags and type names explicitly labelled proposed do not exist yet.

The abstract tests validate only the stated examples and finite signature algebra. They do not model PoE mechanics, establish native runtime performance, or discharge production proof obligations.
