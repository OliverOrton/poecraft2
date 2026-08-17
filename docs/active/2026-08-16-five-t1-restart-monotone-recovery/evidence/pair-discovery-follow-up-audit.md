# Pair-Discovery Follow-Up Audit

**Status: source/artifact audit at `5deaa3f`; no source behavior changed.**

Parent: [selected successor plan](../successor-plan.md)

## Reviewed Claims

| Claim | Result | Evidence and consequence |
| --- | --- | --- |
| The next hard limit is external `exact_max_pairs = 5,000,000` because the candidate needs 8,395,474 pairs. | **Rejected.** | `CompiledPolicyAssertionWork` sets internal `StrategyEvalOptions.max_pairs` from Solve `max_state_action_rows`, 1,215,000 in the frozen case. The evaluator deliberately allows raw operation/state discovery above that value so a later exact quotient may merge it. The 8,395,474 count is raw pairs, not a measured final quotient class requirement. A future quotient-cap stop is possible but unproved. |
| The failed work has no useful timing because `phase_wall_ms.verification` is null. | **Partly valid.** | External verification is intentionally disabled, so null is correct. Solver telemetry reports about 3.413 s exact graph evaluation, 3.745 s direct certification, 4.084 s strict lift, and 12.358 s total Solve for the final priced witness. It still lacks evaluator-internal discovery/partition/component timing, so subphase attribution is required before lower-level representation work. |
| The 49-case portfolio has no five-goal case and conflates refuted and resource-deferred outcomes. | **Valid with wording correction.** | Manifest goal-slot counts are 34 one-goal, 6 two-goal, 4 three-goal, and 5 four-goal cases. In the named three-goal cases, the cheaper preferred/core candidates exact-evaluate as improper (success 0 and 0.089204); separately evaluated primitive-renewal fallbacks publish successfully. Witness B never receives an exact verdict because evaluation reaches a memory cap. Top-level publication says `bounded_fallback_policy` for both paths even though detailed telemetry differs. |
| 767 Fracture route nodes are duplicates and 767 Fracture operation nodes can follow. | **Route claim confirmed; operation claim needs full-behavior grouping.** | The accepted 1,813-node graph contains exactly 767 `_fracture_route` routers and seven distinct outgoing route signatures, with group sizes 440, 105, 105, 105, 4, 4, and 4. There are 767 incoming Fracture operation nodes. Their `operation` descriptors are identical, but full node payloads form 49 groups because expected-cost annotations differ. Existing compiler policy says shared regions omit non-uniform expected-cost annotations, so sharing is valid only when the complete operation/condition/retry/accounting behavior matches. |
| Compilation is 0.3906 ms for the 2,015-node graph and therefore irrelevant. | **Rejected measurement association; broad conclusion only partly valid.** | 0.3906 ms is external compilation of the six-node published fallback. The internal 2,015-node candidate reports about 61.2 ms aggregate strategy compilation and about 3.413 s exact graph evaluation. Compilation is not the immediate measured bottleneck, but compiler graph sharing can reduce evaluator carriers, pairs, memory, and WASM graph size, so it is a scaling candidate rather than cosmetic cleanup. |

## Frozen Artifact Values

Source artifact:
`build/qualification/gate4-pair-discovery-boundary-final.json`.

- Solve: 12,358.394 ms.
- External published-fallback compile: 0.3906 ms.
- Direct certification: 3,745.1308 ms.
- Strict lift: 4,083.7786 ms.
- Exact graph evaluation: 3,413.1537 ms.
- Candidate graph: 2,015 nodes, 4,123 edges, 757 policy regions.
- Discovery: 35,837 states, 8,395,474 raw pairs, 544 rows, 8,396,650
  transitions.
- Owned bytes: 1,178,801,916 versus evaluator budget 1,050,982,663.
- Row payload: 268,692,800 bytes; observation requirements: 594,480 bytes.
- Stable failure classification: `exact_eval_pair_discovery_memory_cap`.

The Fracture census uses the embedded prepared strategy in
`build/qualification/final-wasm-goal-realignment-verified.json` and is a
representation diagnosis, not acceptance evidence for current source.
