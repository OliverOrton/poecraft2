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
| Native solver phase flow and private source ownership | [Solver internals](foundation/solver-internals.md) |
| Implemented crafting behavior | [Mechanics](mechanics/README.md) |
| Native data, state, pools, weights, bitsets, and WASM | [Engine](engine/README.md) |
| Exact planning and strategy compilation | [Solver](solver/README.md) |
| Workspace, Calculator, and strategy surfaces | [Product](product/README.md) |
| Economy data and deployment | [Economy](economy/README.md) |
| Durable choices and terms | [Decisions](decisions.md) and [glossary](glossary.md) |
| Measurements, fixtures, and acceptance evidence | [Evidence](evidence.md) |
| Deferred possibilities, not scheduled work | [Future](future/README.md) |
| Open mechanic rulings and raw observations | [Notes](notes/ruling-needed.md) |
| Proposed or selected execution boundary | [Active work](active/README.md) |
| Completed plans and point-in-time reports | [Archive](archive/README.md) |

The primary area indexes are compositional: each links the narrower references
it owns. Area `NOTES.md` files hold observations that have not yet earned a
stable contract. [The inbox](notes/inbox.md) is for uncategorized raw material.

## Execution State

The active
[Exact Same-Side Closure](active/2026-08-26-exact-same-side-closure/README.md)
boundary begins from clean checkpoint `9ae5a1d`. Its first stabilization target
is the reproduced stale transition-row crash during reachable-incumbent
construction. Its primary product target is genuine `exact_closed` publication
for one clean exact three-prefix and one clean exact three-suffix goal under the
current junk-free terminal contract. Focused native witnesses precede any
broad benchmark work.

The completed
[Solver Quality Measurement, Refinement, And Debt Retirement](archive/2026-08-26-solver-quality-debt-retirement/README.md)
boundary began from clean checkpoint `ec3fbd3` and completed Gates -1 through
8 on 2026-08-26. The
Calculator, benchmark runners, and diagnostic ladder now select the versioned
native `calculator_product_v1` behavior bundle instead of copying low-level
defaults across layers. Its completed
[Solver Anytime Planning, Proof Patterns, And Debt Retirement](archive/2026-08-25-solver-anytime-proof-realignment/README.md)
milestone remains its predecessor and retains current-semantics benchmark authority, typed action-envelope
and incumbent ownership, the qualified scheduler fallback, admissible proof
patterns and measured consumers, cooperative publication/evaluation, release-
WASM responsiveness, and the passing final acceptance record.

The completed boundary began at `a1449fa` (`Record Imprint scope acceptance`)
and its final source checkpoint is `cb26c29`. Calculator and general solver
benchmarks default generated
automatic Imprint programs off, while dedicated controls opt in; the low-level
engine retains its compatibility default. Exact terminal success permits only
requested explicit affixes, ordinary Calculator solves disable voluntary
economic Restart, and an open incremental action envelope publishes only an
independently global proof lower—not its restricted-search value.

The current five-T1 product result is an independently evaluated proper
`14454067.4260706`-Chaos policy with a certified `36.4885317287664` lower.
The historical `87361.1690420501` policy remains valid measured evidence but
is not a deterministic current-source baseline. The completed milestone's
behavior-changing scheduler did not qualify its retained controls, so the
explicit legacy-order fallback remains. Final exact replay confirms the 87k
artifact is still valid observational evidence, not current publication
authority.

The current retained acceptance and the closed release-WASM 50 ms worker-slice
qualification are summarized in [HANDOFF](../HANDOFF.md). Detailed
milestone results, stopped experiments, strategies, and raw evidence live in
the [archive](archive/README.md) and [evidence index](evidence.md); they have no
current sequencing authority.

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
