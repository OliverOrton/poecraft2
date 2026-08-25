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

The
[Solver Anytime Planning, Proof Patterns, And Debt Retirement](active/2026-08-25-solver-anytime-proof-realignment/plan.md)
plan is active at Gate 3 by Oliver's explicit 2026-08-25 instruction. Its completed
[documentation preflight](active/2026-08-25-solver-anytime-proof-realignment/preflight-audit.md)
confirms the current contract and records the stable-reference cleanup; its
[Gate 0 evidence](active/2026-08-25-solver-anytime-proof-realignment/gate0-evidence.md)
pins the current-semantics benchmark and attribution authority, and its
[Gate 1 evidence](active/2026-08-25-solver-anytime-proof-realignment/gate1-evidence.md)
establishes the typed action-envelope lifecycle and mechanic-family controls.
The [Gate 2 evidence](active/2026-08-25-solver-anytime-proof-realignment/gate2-evidence.md)
establishes monotone verified-incumbent and progress authority.

Current source behavior starts from `a1449fa` (`Record Imprint scope
acceptance`). Calculator and general solver benchmarks default generated
automatic Imprint programs off, while dedicated controls opt in; the low-level
engine retains its compatibility default. Exact terminal success permits only
requested explicit affixes, ordinary Calculator solves disable voluntary
economic Restart, and an open incremental action envelope publishes only an
independently global proof lower—not its restricted-search value.

The current five-T1 product result is an independently evaluated proper
`14454067.4260706`-Chaos policy with a certified `36.4286171890906` lower.
The historical `87361.1690420501` policy remains valid measured evidence but
is not a deterministic current-source baseline. The proposed plan uses the
reproducible current-source `1562083.15196689` recovered-ordering result as
its first quality qualification and treats 87k as a research anchor.

The current retained acceptance and the unresolved release-WASM 50 ms worker-
slice qualification are summarized in [HANDOFF](../HANDOFF.md). Detailed
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
