# Pre-Expansion Probability-Lower Audit Report

**Status: final negative result.**

Parent: [Milestone archive](README.md)

## Decision

Gate 3 rejected the candidate. Zero of four hard natural-T1 cases obtained
strict action-class separation against its archived exact fixed-renewal
upper. Production pruning, root-certificate integration, ABI work, and
product work were not entered.

The result is stronger than “the lower remained small.” The probe completed
the legal root-action envelope, projected every class, supplied a lower for
every class, and changed no state, row, transition, or reforge-work count.
The complete comparison still failed by five to ten orders of magnitude.

## What Was Audited

The existing `CalcContext::optimistic_goal_draw_probability` helper is
graph-free with respect to solver transitions: it materializes the unchanged
start carrier and reads weighted pools and exclusion groups, but it does not
intern successors or call `CalcContext::outcomes`.

The existing `prepare_goal_cover_cost` is not graph-free as a whole. Its
`exact_destructive_envelopes` path calls `CalcContext::outcomes` for
destructive actions. The measurement therefore:

- skipped that exact-envelope path;
- retained the finite rarity, satisfied-goal-mask, and affix-count MDP;
- separated guaranteed Essence and forced Fossil modifiers from their
  weighted additions;
- used the more favorable normal or guaranteed-tag Harvest probability;
- forced one exact legal first registry action for each root class; and
- saved and restored every production cover cache.

For a junk bench that changed no represented goal-progress state, the MDP had
no conditioned transition. The sound complete-scope fallback charged the
exact first-action price and granted a free terminal continuation. This is
deliberately weak, but it is the correct lower-only treatment when the
relaxation does not retain the junk/blocker effect.

The per-slot control minimized `cost / optimistic_probability` over every
placer, treating guaranteed placement as probability one. It was recorded
separately and never added to an action price without a conditioned proof.

## Oracle

A clean empty-rare two-slot fixture had two deterministic goal benches costing
3 and 5. The exact control and full-evidence audited solve both returned value
8 and the same root policy. The graph-free relaxation also returned 8, and
its before/after state, row, transition, and reforge-work counters were
identical.

The measurement build's focused solver suite completed 545 checks with zero
failures. After every measurement-only source edit was restored, a clean
engine rebuild and the same solver-only suite completed the production
baseline's 532 checks with zero failures.

## Frozen Results

All observations occurred after automatic admission and before the first
ordinary root row.

| Case | Classes | Relaxed start lower | Archived exact upper | Lowest competing class | Strict margin |
| --- | ---: | ---: | ---: | ---: | ---: |
| full-three | 100 | 431.4000 | 1,918,267.5088 | 0.005872 | -1,918,267.5029 |
| deep-three | 96 | 15.9802 | 575,497.5226 | 0.005872 | -575,497.5168 |
| full-four | 84 | 431.4000 | 193,266,777.2758 | 0.014770 | -193,266,777.2611 |
| deep-four | 90 | 431.4000 | 175,126,199.4864 | 0.014770 | -175,126,199.4716 |

The selected renewal classes themselves received probability-MDP lowers of
19.44 to 455.73 chaos. Even ignoring the cheap bench blockers, these are far
below their archived uppers.

The smoke control had no archived selected upper. It reported a relaxed start
lower of 144.31 over 104 classes and reproduced its prior 11,000,000
reforge-work stop.

Every hard case reproduced the completed root-action baseline's final
deterministic behavior:

| Case | Final discovered | Expanded | Reforge work | Stop |
| --- | ---: | ---: | ---: | --- |
| full-three | 200,000 | 1 | 10,143,446 | `max_discovered_states` |
| deep-three | 200,000 | 1 | 8,152,092 | `max_discovered_states` |
| full-four | 200,000 | 1 | 6,119,280 | `max_discovered_states` |
| deep-four | 200,000 | 1 | 6,119,280 | `max_discovered_states` |

The shadow itself consumed 124,910 to 7,483,074 deterministic evaluator
operations. Its 0.13-to-6.45-second wall times are machine/compiler-bound.

## Interpretation

This closes the proposed assembly of the archived renewal upper and the
current optimistic pool-probability machinery. A finite bracket is easy; a
certificate is not.

The goal-mask/count relaxation discards exactly the information needed for
the lowest competing classes. A cheap non-goal bench may alter blockers and
the later pool without satisfying a goal slot. Once that effect is erased,
the only universal conditioned lower is its immediate price. The result is
therefore not repaired by publishing the MDP lower, using it in
`optimistic_operator_lower`, or attaching the archived renewal upper.

This does not prove verified-next-action impossible. A later attempt needs
both:

1. an executable upper that fits the product computation boundary; and
2. an admissible competing-class lower that retains enough non-goal
   first-action, blocker, and preservation state to charge the downstream
   work.

The current fixed-renewal upper exceeds the product work cap on both
three-mod cases, and the current graph-free goal-progress lower cannot
represent the cheap bench-first competitors. Neither half is ready for
production.

## Identity And Retention

- Starting source:
  `8976750ce86de97184f17005bca0029fdcaa78a0`.
- Frozen-plan source:
  `323b05b63c524c43d0039e2db226c91e0f5b842a`.
- Measurement executable SHA-256:
  `9c07b9f2a0386a18bb35c3a4de6927067a9cd04e79bb63862aebaa11789a7b61`.
- Portfolio manifest SHA-256:
  `a4f080e680499b95d2b6b5e635c4bc9fd6678a82f4ce698ea8c6882b37fb3efd`.
- Artifact manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Generator-config SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Economy content SHA-256:
  `9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.
- Compiler/machine: GCC 14.2.0, Windows 11, Intel i7-13700K, 24 logical
  processors, 68,396,957,696 physical bytes.

Raw hashes and per-case details are pinned in the tracked summary. Every
measurement-only engine/test edit was restored. No mechanics, action scope,
solver behavior, caps, artifact, bindings, WASM, web, or product behavior
changed.
