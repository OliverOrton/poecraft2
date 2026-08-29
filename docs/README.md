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
| Local native solver experiments, GUI, CLI, and supervision | [Native Solver Lab](foundation/solver-lab.md) |
| Deferred possibilities, not scheduled work | [Future](future/README.md) |
| Open mechanic rulings and raw observations | [Notes](notes/ruling-needed.md) |
| Proposed or selected execution boundary | [Active work](active/README.md) |
| Completed plans and point-in-time reports | [Archive](archive/README.md) |

The primary area indexes are compositional: each links the narrower references
it owns. Area `NOTES.md` files hold observations that have not yet earned a
stable contract. [The inbox](notes/inbox.md) is for uncategorized raw material.

## Execution State

No implementation boundary is active. Oliver must select the next chunk before
implementation resumes.

The completed
[Native Solver CLI Workflow v1](archive/2026-08-29-native-solver-cli-workflow-v1/README.md)
boundary adds versioned matrix files, bounded JSON-Pointer case derivation, and
one-shot submit/targeted-wait/summary through the existing Lab authority. Its
terminal acceptance also repaired local authoring that dropped native product-
envelope controls and broadened action-catalogue work; no solver, ladder,
fragment, proof, ABI, WASM, GUI, or product logic changed.

The completed
[Native Solver Lab CLI-First MCP Removal v1](archive/2026-08-29-native-solver-lab-cli-first-mcp-removal-v1/README.md)
boundary removed the repository and user-local Solver Lab MCP transport. The
existing structured JSON CLI and saved artifacts remain the automation
surface, with the typed service, catalog, supervisor, cancellation, identity,
evidence-integrity, and GUI contracts preserved and qualified.

The completed
[Generated Planner-Envelope Qualification And Ladder-Service Repair v1](archive/2026-08-28-generated-planner-envelope-qualification-ladder-service-repair-v1/README.md)
boundary qualified a full-minus-Fossil generated layer, attributed remaining
work to continuation/publication coverage, requalified the existing carrier
ladder, and closed diagnosis-only without a speculative behavior repair.

The completed
[Verified Executable Graph-Fragment Core v1](archive/2026-08-28-verified-executable-graph-fragment-core-v1/README.md)
adds a probability-free leaf-control IR, exact engine-built single-entry
verification, complete exit/SCC/resource evidence, FinalSuccess-only ordinary
flattening, independent evaluation, and an MCP-qualified isolated shadow lane.
It changes no incumbent, product default, C ABI, strategy vocabulary, or WASM
behavior and remains parked outside the active boundary.

The completed
[Native Solver Lab Unattended Execution and Identity Hardening](archive/2026-08-28-native-solver-lab-unattended-hardening/README.md)
boundary started from the completed GUI stabilization and hardened immutable
dispatch identity, watchdog enforcement, atomic hashed terminal publication,
orphan quarantine/recovery, evidence integrity, host headroom, and one
MCP-plus-supervisor unattended path. Gates 0–5 and every non-soak acceptance
check passed. Oliver explicitly waived its six-plus-hour soak; the result
retains that limitation and does not claim overnight qualification. The
completed fragment-core result preserves that wording.

The completed
[Native Solver Lab GUI Stabilization](archive/2026-08-28-native-solver-lab-gui-stabilization/README.md)
boundary makes explicit-null partial reports safe, moves cached refresh work
off Qt, preserves selection and durable action/error feedback, and qualifies
real service/CLI/MCP/GUI cancellation plus the complete GUI action matrix. It
changes no solver or mechanic behavior.

The completed
[Native Solver Lab Case Authoring](archive/2026-08-28-native-solver-lab-case-authoring/README.md)
boundary adds revision-safe local drafts, native validation, immutable
content-addressed case revisions, a fifth GUI Cases surface, a Calculator
clipboard handoff, CLI/MCP lifecycle parity, and a verified user-local Codex
MCP registration. It changes no solver, mechanic, ABI, or WASM behavior.

The completed
[Native Solver Lab v0](archive/2026-08-27-native-solver-lab-v0/README.md)
boundary provides persistent native experiments, a practical PySide6 GUI,
resource-aware supervision, JSON CLI, closed typed local MCP controls,
investigation bundles, canonical matrices, and direct-versus-Lab semantic
qualification while leaving mechanics and solver behavior native-owned.

The stopped
[PDR Strict-Proof Memory Attribution And Repair](archive/2026-08-27-pdr-strict-proof-memory/README.md)
probe established that the current coarse checkpoint cannot faithfully resume
the four-mod PDR witness's open incremental scheduler. A scheduler-aware or
first-strict-partition checkpoint must prove ordinary/replay parity before
replay can support strict-proof memory attribution; no solver change from the
probe was retained.

The archived
[Solver Research Architecture Audits](archive/2026-08-27-solver-research-audits/README.md)
preserve the read-only evidence and proposals behind this choice and the later
verified-option, PDR-memory, retention-proof, and learned-guidance tracks.

The completed
[Solver Development Checkpoint/Replay](archive/2026-08-27-solver-development-checkpoint-replay/README.md)
milestone implements native-development cross-process reuse of a completed
coarse transition graph. Replay reconstructs the exact calculator
state/operator/admission namespace, requires ordinary graph compatibility,
and reruns Bellman, refinement, compilation, and evaluation. It is not a
request/result cache, product proof authority, or release-WASM feature.

The completed
[Solver Exactness, Iteration, And Debt Closure](archive/2026-08-27-solver-exactness-iteration-debt-closure/README.md)
milestone delivered eight of its nine debt items, passed full acceptance, and
left honest cross-process checkpoint/replay as a dedicated future format
milestone. Its PDR repair reached a real strict frontier and moved the measured
boundary from stale alternative-row work to retained proof/quotient memory.

The completed
[Solver Stabilization And Action-Family Controls](archive/2026-08-27-solver-stabilization-action-family-controls/README.md)
milestone adds native-owned restricted family controls through Calculator and
release WASM and restores a green full acceptance record. Its temporary-Bench
PDR ablation moved the proof stop earlier into strict-carrier memory growth,
so cooperative resumable broad destructive rows remain the recommended next
boundary; the active successor now owns those row cursors and replay.

The completed
[Exact Same-Side Closure](archive/2026-08-26-exact-same-side-closure/README.md)
milestone makes current-contract clean three-prefix and three-suffix Calculator
goals genuinely exact, with deterministic compiled graphs and 10,000-run
Simulator controls. Its four-mod probe attributes the next boundary to strict
broad-alternative proof work rather than the repaired frontier or existing
goal-cover plumbing.

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

The current retained acceptance and recommended next boundary are summarized
in [HANDOFF](../HANDOFF.md). Detailed
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
