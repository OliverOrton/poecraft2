# Strategy discovery first, native checking always

**Repository:** OliverOrton/poecraft2  
**Reviewed main:** `28a955056a41699d1fa9996ee345b70678937f21`  
**Date:** 24 September 2026  
**Status:** proposed next programme; no repository implementation was made by this review.

## Decision

Proceed with a separate, explicitly experimental strategy-finder lane. Reuse native mechanics, macro semantics, ordinary strategy execution and independent policy checking. Keep the current certified-search lane and its default behavior intact. Initially end the new lane at a checked executable original-root strategy, not a proof handoff.

The first finder is **bounded heuristic search over native controller structures**, not a neural policy, a CUDA rewrite, a second mechanics engine, or beam search over fortunate sampled trajectories. Relative goal roles parameterize candidate construction; unequal weights and contexts remain unequal. A learned ranker is a later interchangeable search component, not the initial acceptance authority.

The opportunity is plausible, not established. The latest native boundary trial saved only 0.04439% for about 45 seconds of speculative service; it was not deployed. That is useful evidence for changing the breadth and representation of discovery, not proof that any particular new search algorithm will win.

## What implementation is authorized to attempt

Implement **F0–F4** in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md), as separate reviewable checkpoints. This is one coherent vertical slice: request-bound acceptance; a peer finder with from-root bootstrap; a real heuristic controller search; experimental Calculator selection; and matched qualification/data receipts.

Learning, concurrency, GPU work and proof integration are mapped but **not authorized as automatic follow-through**. No requirement to obtain a large economic gain before retaining a small functional experimental lane; no permission to call that lane economically superior before the comparisons exist.

## Read order

1. [CODEX_PROMPT.md](CODEX_PROMPT.md): executable handoff, including stop/decision rules.
2. [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md): dependencies, files, acceptance and deliverables.
3. [ARCHITECTURE.md](ARCHITECTURE.md): trust boundaries, interfaces and ownership.
4. [SEARCH_DESIGN.md](SEARCH_DESIGN.md): selected search, alternatives, bootstrap and pseudocode.
5. [VALIDATION.md](VALIDATION.md): authoritative negative tests and fair experiments.

The concrete candidate/control adapter is in [CANDIDATE_INTERFACE.md](CANDIDATE_INTERFACE.md).

For a specific question, use [MAIN_REVIEW.md](MAIN_REVIEW.md), [DATA_AND_LEARNING.md](DATA_AND_LEARNING.md), [MATHEMATICS.md](MATHEMATICS.md), [PRODUCT_AND_ROADMAP.md](PRODUCT_AND_ROADMAP.md), [DATA_REQUEST.md](DATA_REQUEST.md), [DOC_MAINTENANCE.md](DOC_MAINTENANCE.md) or [SOURCES.md](SOURCES.md). The original supplied direction is in [inputs/user_direction.original.md](inputs/user_direction.original.md); it is advisory, not an instruction to implement every listed technology.

The thirteen source-handoff questions and the main falsifiers are mapped in [DECISIONS_AND_RISKS.md](DECISIONS_AND_RISKS.md).

## Non-negotiable distinction

A finder may fail to consider a good strategy. It may NOT return a policy whose legality, original goal, complete stochastic execution, paid resources, properness or numerical acceptance was replaced by a prediction. The current evaluator's documented tolerance-based numerical contract is retained; this packet does not upgrade it to outward-rounded rational certification.

All proposed type/file names in this packet are design recommendations, not claims that these interfaces already exist. Existing owners are named separately. The implementing agent may simplify proposed interfaces after inspecting actual callers, but must preserve their semantic obligations and record the decision.
