# Sources and evidence provenance

All repository links are pinned to the reviewed head unless explicitly noted. Source establishes implementation; Claude's appendix supplies attributed measurements; the included equations/specification checks establish only their stated finite examples. No web research was used to decide PoE mechanics.

## S1 — Pinned main and comparison

[Pinned main and comparison](https://api.github.com/repos/OliverOrton/poecraft2/compare/02fa1fe8759686110c9b5bbc475d866f6c7f3926...96111477c27b09c9855147b3d3129f7570220b7d). 24 commits; source change does not establish performance equivalence.

## S2 — AGENTS

[AGENTS](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/AGENTS.md). Operating rules; same blob as earlier reviewed head.

## S3 — Research standards

[Research standards](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/solver/research-standards.md). Proof, scope, evidence and budget separation.

## S4 — Current status

[Current status](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/solver/current-status.md). Historical snapshot plus ABI3/Foulborn/Calculator deltas; not fresh performance qualification.

## S5 — HANDOFF

[HANDOFF](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/HANDOFF.md). Latest release status, preserved IC gaps and budgets; not this task's deployment permission.

## S6 — Foulborn and solution scope

[Foulborn and solution scope](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_solve.cpp#L25-L170). Current candidate/dependency-triggered proof demotion and existing scope strings.

## S7 — Native Scour

[Native Scour](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/actions_basic.cpp#L637-L735). Exactly-one-lock behavior drops opposite fractures in reviewed code; native Annul control.

## S8 — Scour mirrors

[Scour mirrors](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_registry.cpp#L1414-L1440). Refinement preserved/destroyed affix selectors; also solver_abstract.cpp 1102–1138.

## S9 — Metadata derivation

[Metadata derivation](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_registry.cpp#L1607-L1740). Copies renewal facts into nonrenewal metadata without correcting lock/NoRoll booleans; engine_internal.hpp 784–821 defines facts.

## S10 — Admission counter semantics

[Admission counter semantics](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_model.hpp#L240-L276). Transient cumulative counters and explicit parent-reforge-cap exemption.

## S11 — Existing early finish ordering

[Existing early finish ordering](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_solve_expand.cpp#L542-L660). Primitive-first phase; permanent Bench finish before Chaos; automatic work delayed.

## S12 — Native retry equivalence

[Native retry equivalence](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_options.cpp#L423-L527). Surviving lock exits; kernel-pointer/complete-entry equality; not every target miss retries.

## S13 — Protected and Multimod generators

[Protected and Multimod generators](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_options_helpers.hpp#L712-L1130). Multimod-in-goal condition; protected Scour nested in missing-opposite-goal loop.

## S14 — Finder programme compiler

[Finder programme compiler](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_compile.cpp#L256-L300). Only EldritchSideIntent accepted; held side is opposite affected side.

## S15 — Policy mathematics

[Policy mathematics](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/solver/mathematics/policies.md#fixed-policy). Canonical finite-policy, properness, original-root and recurrence arguments.

## S16 — Scour abstract applicability

[Scour abstract applicability](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/engine/src/solver_abstract.cpp#L1102-L1140). Exactly-one-lock remaining count only counts locked side in reviewed implementation.

## C1 — Claude research input

[Claude research input](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/active/2026-09-29-exact-solver-audit/README.md). Full input read; recommendations are not owner-selected implementation. Contains recorded owner rulings and F1–F14.

## C2 — Claude evidence appendix

[Claude evidence appendix](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/active/2026-09-29-exact-solver-audit/evidence.md). Full appendix read. E1–E8; mixed-build provenance explicitly incomplete, timings single runs.

## C3 — W1 actual case

[W1 actual case](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/active/2026-09-29-exact-solver-audit/witness/ring/cases/w1-prefix-only-clean.json). 90-second Finish, 150 watchdog, exact root holds two goals despite copied metadata.

## C4 — W1 authored native oracle

[W1 authored native oracle](https://github.com/OliverOrton/poecraft2/blob/96111477c27b09c9855147b3d3129f7570220b7d/docs/active/2026-09-29-exact-solver-audit/witness/authored/w1-prefix-lock-then-scour.strategy.json). Hand-authored paid graph with clean goal guard, not solver-discovered policy.

## P1 — Sutton, Precup & Singh (1999)

[Sutton, Precup & Singh (1999)](https://www.sciencedirect.com/science/article/pii/S0004370299000521). Between MDPs and semi-MDPs. Publisher search abstract accessed; direct open returned 403. Used only as conceptual options/SMDP precedent, not native mechanics or an imported theorem.

## P2 — Chatterjee et al. (2025)

[Chatterjee et al. (2025)](https://arxiv.org/abs/2501.11467). Fixed Point Certificates for Reachability and Expected Rewards in MDPs. Primary abstract accessed; certificate/checker separation does not prove native poecraft2 correspondence.

## Prior assigned plan

The attached `poecraft2_metamod_audit_02fa1fe.zip`, especially CODEX_PROMPT and PLAN, is the preserved M0–M5 scope. It was not implemented. The execution prompt was read through Files; the mounted archive was inspected directly. Its SHA-256 is recorded in `evidence/review.json`.

## Review limits

No complete local repository checkout, hidden user worktree, ignored raw output corpus, native executable or benchmark was accessed. No solver test, build, simulation or deployment ran in the planning session. Only selected present source owners were independently checked; claims about uninspected call chains remain required implementation verification.

Latest head reports `[skip ci]`; same-head workflow query returned zero. Relevant earlier/native/hosted validation must retain its own source and layer, and known inherited failures remain visible. Recheck main and local diff at implementation start.
