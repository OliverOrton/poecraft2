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

[Active](active/README.md) records the selected
[gap-directed natural-T1 solver research](active/gap-directed-natural-t1-research.md).
It is a measurement/prototype boundary for improving certified bounded and
near-optimal behavior on three-/four-T1 goals; it does not authorize a
production solver change. The accepted WASM progress-accounting follow-up is
pinned by its
[tracked evidence](../fixtures/solver-scaling/v1/evidence/wasm-progress-accounting-fix-summary.json)
at source commit `c58b71a`. The completed
[focused-round performance attribution](archive/2026-07-23-focused-round-performance/README.md)
is archived with its measured no-default-change result. [HANDOFF](../HANDOFF.md)
records the exact completed boundary. The completed
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
