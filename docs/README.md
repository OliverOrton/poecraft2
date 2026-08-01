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
| Completed plans and point-in-time reports | [Archive](archive/README.md) |

The primary area indexes are compositional: each links the narrower references
it owns. Area `NOTES.md` files hold observations that have not yet earned a
stable contract. [The inbox](notes/inbox.md) is for uncategorized raw material.

## Execution State

The selected enabling implementation boundary is
[Solver Iteration Infrastructure And Decomposition](active/solver-iteration-infrastructure.md).
Gate 0 is frozen. It establishes the canonical fast native workflow and
decomposes oversized solver sources/private contracts without changing solver
behavior.

The queued algorithmic boundary remains
[Proof-Carrying Quotient Refinement During Solving](active/proof-carrying-quotient-refinement.md).
Its plan is unchanged and resumes immediately after the enabling milestone.
It will retain certified quotient transitions during solve and feed
counterexamples from the existing shared partition back into policy
improvement, without changing mechanics, action filtering, strategy format,
or the 1 GiB cap.

The archived
[Policy-Guided Exact Refinement qualification](archive/2026-07-31-policy-guided-exact-refinement/README.md)
retains the shared exact contract/evaluation/compiler foundation. Its final
two-goal run stopped before partition initialization at `1,089,111,449` bytes
under the unchanged `1,073,741,824`-byte cap, so reconstruct-then-merge is
classified as a correctness bridge rather than the scalable endpoint.

The completed
[Cross-Base And Compiled-Strategy Reliability Pass](archive/2026-07-30-cross-base-strategy-reliability/README.md)
qualifies all 979 supported ordinary bases and a 49-case native/release-WASM
portfolio through solve, compilation, exact Calculator evaluation, and
Simulator execution. It preserves the qualified Fracture hashes and changes
no mechanics, solver objective, or action-filtering scope.

The completed
[Fracture-Local Coarse-Parent Prototype](archive/2026-07-29-fracture-local-coarse-parent/README.md)
replaces the product solver's globally strict Fracture observer with a
six-class ordinary/reforge parent plus an exact solver-local goal-hit and
priced-Restart composition. The frozen full-four carrier graph closes at 927
states with exactly 217 root Chaos successors, zero Fracture miss-state IDs,
and a proper compiled bounded policy. Exact primitive Fracture behavior and
product surfaces remain unchanged.

The completed
[Practical Exact Four-Goal Solving Research](archive/2026-07-29-practical-four-goal-solving-research/README.md)
finds that the 1,030 ordinary zero-goal carriers do not generate the broad-row
work. Fracture independently changes the action-driven parent from 6 junk
classes and 217 projected root Chaos carriers to 105 classes and 134,477
carriers; the public constructor also forces complete group identity.
Dependency-only cleanup would have the same counterfactual effect but is not
in this case's parent `layout_actions`. The report recommends a
Fracture-local coarse-parent prototype, which Oliver has now selected.

The preceding
[Upper-Cap Sensitivity And Zero-Progress Renewal Audit](archive/2026-07-29-upper-cap-zero-progress-renewal/README.md)
reproduced the rejected 200,000-state scheduler exactly. A corrected long run
found a real but microscopic `4.07634` upper reduction at 387,556 states, and
the exact audit rejected every additional ordinary carrier merge because
retained non-renewal actions observed them. Only the existing retry basin
passes the full renewal contract.

The preceding rejected
[High-Impact Partial-State Executable Upper Policies](archive/2026-07-29-high-impact-executable-uppers/README.md)
milestone restored its experimental behavior after no completed frozen row
met the strict-admission or 10% upper-Q gate. It retains bounded observational
upper-policy provenance telemetry.

The completed
[Q-Directed Deep Solving And Automatic Eldritch Side Actions](archive/2026-07-28-q-directed-eldritch-side-actions/README.md)
milestone retains stored-row Q refinement, exceptional-support expansion, and
four automatic real-resource Eldritch side options for eligible armour. The
frozen lower bounds improve materially, but the action intervals remain open.

The completed
[Chaos-Anchored Incremental Action Generation](archive/2026-07-28-chaos-anchored-incremental-actions/README.md)
milestone retains an opt-in gated scheduler that releases and expands Chaos
successors before delayed Fossil, corrected-Harvest, and goal-relevant Essence
rows. Open alternatives remain explicitly unresolved and block exactness.

The completed
[Harvest Natural Pools And Shared Exact Reforge Frontier](archive/2026-07-28-harvest-shared-reforge-frontier/README.md)
milestone retains the owner-approved targeted-natural Harvest correction.
Its exact shared structural-DAG prototype matched sequential outcomes and
hashes, but it retained action-additive work, stopped at the same cap, added
about 47 MiB, and increased total wall time, so the frontier was restored.

The completed
[Root Broad-Row Falsification](archive/2026-07-28-root-broad-row-falsification/README.md)
milestone retains exception-safe interrupted-action telemetry and rejects its
success-only Fossil upper after both real remaining-work runs prove zero
terminal mass. Its structural follow-up was tested and rejected by the
completed milestone above.

The completed
[Gated Root Renewal Incumbent](archive/2026-07-28-gated-root-renewal-incumbent/README.md)
turns a completed gated root Chaos row into an exact fixed
repeat-until-goal policy and finite bounded incumbent before the next root
broad action reaches the unchanged reforge-work cap. Both frozen four-mod
cases now return executable four-node policies; exact discovery still stops
on the competing root reforge.

The preceding completed
[Goal-Progress-Gated Reforge Mode](archive/2026-07-27-goal-progress-gated-reforge/README.md)
adds an opt-in restricted exact solver mode without changing the default
unrestricted solver. Both frozen four-mod first Chaos rows now fit under
200,000 states. Corrected raw telemetry shows that another root broad reforge,
not partial-state expansion, is the immediate unchanged-cap bottleneck.

The completed
[Pre-Expansion Probability-Lower Audit](archive/2026-07-27-pre-expansion-probability-lower-audit/README.md)
isolated the graph-free part of the optimistic probability cover and compared
complete root-action-class lowers against the archived renewal uppers. The
probe kept graph work flat but separated zero of four hard cases; cheap
junk-bench-first classes remained at `0.005872` or `0.01477` chaos.
Measurement source was restored.

The completed
[Certified Root-Action Feasibility](archive/2026-07-27-certified-root-action-feasibility/README.md)
pass rejected public integration of the existing proof machinery. All four
hard cases completed exact first-action projection over 91 to 107 classes
before broad-row work, but none had a finite executable incumbent; each then
hit the first Chaos state cap. The positive constructive oracle passed and all
measurement source was restored.

The completed
[True First-Frontier Successor Census](archive/2026-07-27-true-successor-frontier-census/README.md)
measured complete exact Chaos supports from 222,580 to 3,204,323 states. Their
one-sided projection products are 98.71% to 99.65% occupied, so completion
reveals dense joint combinations rather than a hidden small duplicate class.
No production architecture or behavior change was selected.

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
