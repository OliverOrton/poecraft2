# Documentation

**Status: current documentation map and lifecycle policy.** Start here for
repository knowledge. [Project direction](direction.md) is the short product
orientation; [HANDOFF](../HANDOFF.md) names the implementation boundary only
when Oliver has selected one.

## Find The Authority

| Need | Go to |
| --- | --- |
| Product orientation and current posture | [Project direction](direction.md) |
| Architecture, layer ownership, and change impact | [Foundation](foundation/README.md) |
| Implemented crafting behavior | [Mechanics](mechanics/README.md) |
| Native data, state, pools, weights, bitsets, and WASM | [Engine](engine/README.md) |
| Exact planning and strategy compilation | [Solver](solver/README.md) |
| Workspace, Calculator, and strategy surfaces | [Product](product/README.md) |
| Economy data and deployment | [Economy](economy/README.md) |
| Durable choices and terms | [Decisions](decisions.md) and [glossary](glossary.md) |
| Measurements, fixtures, and acceptance evidence | [Evidence](evidence.md) |
| Deferred possibilities, not scheduled work | [Future](future/README.md) |
| Open mechanic rulings and raw observations | [Notes](notes/ruling-needed.md) |
| Completed plans and point-in-time reports | [Archive](archive/README.md) |

The primary area indexes are compositional: each links the narrower references
it owns. Area `NOTES.md` files hold observations that have not yet earned a
stable contract. [The inbox](notes/inbox.md) is for uncategorized raw material.

## Execution State

The active implementation boundary is the measurement-only
[True First-Frontier Successor Census](active/plan.md). It measures the
complete exact Chaos support of four frozen natural-T1 hard cases without
authorizing a cap change or production compaction.

The completed
[Exact Quotient Audit](archive/2026-07-27-exact-quotient-audit/README.md)
proved the live completed quotient path with a fast 10-to-3 merge regression,
retained raw Unveil modifier identity because compiled execution observes it,
and corrected historical cap-stopped claims: incomplete shadow grouping is
not a completed exact quotient. No production behavior changed.

The completed
[Action-Local Side Factorization Falsification](archive/2026-07-27-action-local-side-factorization/README.md)
rejected simple count-conditioned prefix/suffix convolution after an exact
Chaos outcome table produced a non-zero probability minor. Conditioning on
final remaining side weights restored rank one but required more marginal
identities than the joint table. Measurement source was restored; only a
fracture/goal regression, documentation, and evidence remain.

The completed
[Protected-Core Compression Ceiling Census](archive/2026-07-27-protected-core-ceiling-census/README.md)
rejected cleanup as the next pre-bound architecture. Collision-checked
impossible side erasure produced very large all-state ceilings, but states
with at least two goals on one protected side were at most 5.68% of the
cap-censored observed stream. Its broader signal led to the action-local test
above; neither projection has equivalence or pruning authority. All
measurement-only engine source was restored.

The completed
[Streaming Broad-Lower Fold Falsification milestone](archive/2026-07-27-streaming-broad-lower-fold/README.md)
restored its failed shadow candidate and retained versioned successful
fallback properness-proof reuse. The owner case replaced 16 repeated
start-properness scans with validated reuse and cut solve wall by 22.3% with
identical deterministic results. It does not improve the hard pre-bound
failures.

The completed
[Anytime Benchmark Completion milestone](archive/2026-07-26-anytime-benchmark-completion/README.md)
durably preserves native step-boundary trajectories through watchdog
termination, separates censoring from failures and completed resource-cap
measurements, pins stronger experiment identity and corpus roles, and records
a fresh smoke baseline. Pure analytics remain deferred until a second
candidate; accumulated-gap racing is rejected.

The completed
[R4 browser transfer and solver lifetime milestone](archive/2026-07-26-browser-transfer-lifetime-r4/README.md)
uses transferable strategy bytes, removes redundant graph clones, and releases
the scoped product Solve handle after handoff. Its rebuilt release-WASM
non-visual acceptance is indexed in [Evidence](evidence.md).

The preceding
[broad-action separation and renewal research](archive/2026-07-25-broad-action-separation-research/README.md)
is archived with a negative architecture result: fixed-policy renewal
produced exact finite candidates, but cheap separation excluded zero broad
actions and the compact evaluator remained over the current reforge-work cap
on both three-mod hard cases. No production source was retained. The
[exact automatic-action constraint-generation milestone](archive/2026-07-25-exact-automatic-action-constraint-generation/README.md)
is archived with a negative Gate 2 qualification result: temporary-bench
kernel deferral moved the first-carrier 200,000-state failure to the first
ordinary broad reforge, so no production source was retained. The preceding
[gap-directed natural-T1 research](archive/2026-07-25-gap-directed-natural-t1-research/README.md)
is historical input. Those three research lineages retained documentation and
evidence only; their prototypes were restored. The
accepted WASM progress-accounting follow-up is pinned by its
[tracked evidence](../fixtures/solver-scaling/v1/evidence/wasm-progress-accounting-fix-summary.json)
at source commit `c58b71a`. The completed
[focused-round performance attribution](archive/2026-07-23-focused-round-performance/README.md)
is archived with its measured no-default-change result. [HANDOFF](../HANDOFF.md)
records the clean no-active boundary and recent result. The completed
[mechanical solver split](archive/2026-07-22-mechanical-solver-split/README.md)
is archived, and the read-only
[post-B6 reconnaissance](archive/2026-07-23-post-b6-reconnaissance/README.md)
is preserved as historical input.
The bounded executable-policy, natural-T1 corpus, benchmark orchestration, and
reporting milestone is complete and indexed by its
[dated archive](archive/2026-07-22-bounded-policy-and-benchmarking/README.md).
The completed action/state pruning milestone is indexed by its
[dated archive](archive/2026-07-21-solver-action-state-pruning/README.md), and
the earlier Q0-Q5 solver work is indexed by the
[Exact Solver State Scaling archive](archive/2026-07-20-solver-state-scaling/README.md).

Lifecycle has one direction:

```text
notes -> stable area reference or future design
active plan -> dated archive with its final handoff and evidence
```

`future/` never establishes sequence. Archived checklists, targets, and
handoffs preserve history but have no current execution authority.

## Repository Entry Points

- [Developer quickstart](../README.md)
- [Agent instructions](../AGENTS.md)
- [Claude instructions](../CLAUDE.md)
- [Current handoff](../HANDOFF.md)
- [Documentation templates](_templates/area-readme.md)

## Maintenance Policy

- Extract lasting facts from completed plans into the owning stable area,
  [decisions](decisions.md), [evidence](evidence.md), or the
  [glossary](glossary.md) before archiving the plan.
- Keep documents fractal: an area README explains ownership and links narrower
  pages; a narrower page links its parent. Prefer links over duplicated prose.
- Stable behavior pages carry a verification stamp with date, commit, and
  checked implementation paths. Mark code-dependent claims unverified when
  that evidence is unavailable.
- Treat roughly 500 lines as a review trigger. Split by responsibility when a
  page mixes several authorities; do not split a cohesive reference merely to
  meet a number.
- Promote notes when they become durable, delete them when disproved, and leave
  unresolved mechanic behavior in [ruling-needed](notes/ruling-needed.md) for
  Oliver. Agents do not research or infer Path of Exile rules.
- Use short descriptive filenames. Move completed execution material into a
  dated archive folder and give that folder an index.
- Keep relative links valid. After structural moves, run a one-off link and
  reachability audit; no documentation lint automation is currently adopted.
