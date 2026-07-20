# Documentation Restructure Inventory

**Status: historical audit and move map.** This records the Markdown tree at
the start of the 2026-07-19 documentation-only cleanup. It has no sequencing
authority.

Parent: [Documentation cleanup archive](README.md)

Starting point: commit `d5e38e3`. Sizes are bytes at that commit, except the
untracked root solver audit supplied with the starting worktree. `Inbound`
counts repository-relative Markdown links; source names are summarized where
the count is non-zero. External URLs and plain-text path mentions are excluded.

## Root And Current Orientation

| Starting path | Bytes | Classification | Purpose and authority | Inbound | Final destination |
| --- | ---: | --- | --- | --- | --- |
| `AGENTS.md` | 2,953 | current orientation | Agent workflow and repository constraints | 6, historical plans and handoffs | unchanged |
| `CLAUDE.md` | 2,654 | current orientation | Claude-facing workflow; subordinate to repository docs | 0 | unchanged |
| `README.md` | 6,085 | current orientation | Developer quick start; `docs/README.md` becomes the documentation authority | 0 | unchanged, shortened links |
| `HANDOFF.md` | 16,819 | current orientation | Exact boundary only while one is active | 12, root/docs/process/history | `docs/archive/2026-07-19-bestiary-solver-s8/handoff.md`; new minimal root file |
| `SOLVER-IMPROVEMENT-PLAN.md` | 29,821 | audit/report/evidence | Untracked reviewed solver audit/plan input; not current authority | 0 | `docs/archive/2026-07-19-bestiary-solver-s8/audit.md` |

## Product Design Tree

All files below are point-in-time product-design history. Their implemented
contracts are extracted to `docs/product/`; the complete tree, including PNG
references and mockups, moves together to `docs/archive/2026-07-product-design/`.

| Starting path | Bytes | Classification | Purpose | Inbound | Final destination |
| --- | ---: | --- | --- | --- | --- |
| `design/briefs/calculator.md` | 9,077 | historical plan | Calculator visual brief | 0 | same suffix under dated product-design archive |
| `design/briefs/calculator-goal-item.md` | 5,434 | historical plan | Goal-item design brief | 0 | same suffix under dated product-design archive |
| `design/briefs/calculator-item-context.md` | 4,485 | historical plan | Calculator context brief | 0 | same suffix under dated product-design archive |
| `design/briefs/economy-selector.md` | 715 | historical plan | League-selector brief | 0 | same suffix under dated product-design archive |
| `design/briefs/item-display.md` | 4,907 | historical plan | Shared item-display brief | 0 | same suffix under dated product-design archive |
| `design/briefs/s6-solve-panel.md` | 11,961 | historical plan | S6 solve-panel brief | 0 | same suffix under dated product-design archive |
| `design/briefs/strategy-calculator-mode.md` | 11,491 | historical plan | Strategy Calculator-mode brief | 0 | same suffix under dated product-design archive |
| `design/specs/calculator.md` | 6,886 | historical plan | Implemented Calculator spec | 0 | same suffix under dated product-design archive |
| `design/specs/calculator-goal-item.md` | 10,337 | historical plan | Approved goal-item spec | 0 | same suffix under dated product-design archive |
| `design/specs/economy-selector.md` | 1,169 | historical plan | League-selector spec | 0 | same suffix under dated product-design archive |
| `design/specs/item-display.md` | 3,429 | historical plan | Shared item-display spec | 1, Calculator spec | same suffix under dated product-design archive |
| `design/specs/s6-solve-panel.md` | 5,476 | historical plan | Implemented S6 solve-panel spec | 0 | same suffix under dated product-design archive |
| `design/specs/strategy-calculator-mode.md` | 5,573 | historical plan | Approved Calculator-mode spec | 0 | same suffix under dated product-design archive |
| `design/mockups/calculator/README.md` | 3,807 | audit/report/evidence | Mockup review | 0 | same suffix under dated product-design archive |
| `design/mockups/calculator/PROMPTS.md` | 8,623 | audit/report/evidence | Image-generation prompts | 0 | same suffix under dated product-design archive |
| `design/mockups/calculator-goal-item/README.md` | 1,199 | audit/report/evidence | Goal-item mockup review | 0 | same suffix under dated product-design archive |
| `design/mockups/calculator-goal-item/PROMPTS.md` | 4,479 | audit/report/evidence | Image-generation prompts | 0 | same suffix under dated product-design archive |
| `design/mockups/economy-selector/README.md` | 379 | audit/report/evidence | Selector mockup review | 0 | same suffix under dated product-design archive |
| `design/mockups/economy-selector/PROMPTS.md` | 405 | audit/report/evidence | Image-generation prompt | 0 | same suffix under dated product-design archive |
| `design/mockups/item-display/README.md` | 2,084 | audit/report/evidence | Item-display mockup review | 0 | same suffix under dated product-design archive |
| `design/mockups/item-display/PROMPTS.md` | 1,320 | audit/report/evidence | Image-generation prompts | 0 | same suffix under dated product-design archive |
| `design/mockups/s6-solve-panel/README.md` | 3,027 | audit/report/evidence | Solve-panel mockup review | 0 | same suffix under dated product-design archive |
| `design/mockups/s6-solve-panel/PROMPTS.md` | 12,485 | audit/report/evidence | Image-generation prompts | 0 | same suffix under dated product-design archive |
| `design/mockups/strategy-calculator-mode/README.md` | 3,670 | audit/report/evidence | Calculator-mode mockup review | 0 | same suffix under dated product-design archive |
| `design/mockups/strategy-calculator-mode/PROMPTS.md` | 7,532 | audit/report/evidence | Image-generation prompts | 0 | same suffix under dated product-design archive |

## Current And Stable Documentation

| Starting path | Bytes | Classification | Purpose and authority | Inbound | Final destination |
| --- | ---: | --- | --- | --- | --- |
| `docs/README.md` | 3,646 | current orientation | Primary documentation entry point | 2, direction and root README | unchanged, rewritten |
| `docs/direction.md` | 14,918 | current orientation | Short product orientation only | 12, root/process/docs/history | unchanged, reduced to about 100 lines |
| `docs/implementation-plan.md` | 50,217 | active plan | Portfolio plan containing completed history and deferred phases | 4, docs/foundation/root | `docs/archive/2026-07-19-project-roadmap/plan.md` |
| `docs/active/bestiary-and-solver-capability-plan.md` | 85,675 | active plan | Former B1/S8 execution authority | 10, current/history/future docs | `docs/archive/2026-07-19-bestiary-solver-s8/plan.md` |
| `docs/foundation/architecture-plan.md` | 14,945 | stable reference | Whole-system architecture mixed with future phasing | 2, direction and docs index | `docs/foundation/README.md`, verified rewrite |
| `docs/foundation/codebase-structure.md` | 24,657 | stable reference | Repository/layer ownership mixed with projected layout | 2, direction and docs index | merged into `docs/foundation/README.md` |
| `docs/engine/data-shapes-and-ingest.md` | 29,102 | stable reference | Canonical and compiled data contract | 2, direction and docs index | `docs/engine/data.md` |
| `docs/engine/engine-bitsets.md` | 32,706 | stable reference | Runtime bitset representation | 2, direction and docs index | `docs/engine/bitsets.md` |
| `docs/engine/item-state-flow.md` | 18,010 | stable reference | Item-state representation/lifecycle | 4, direction/index/engine/solver | `docs/engine/items.md` |
| `docs/engine/mod-data-and-pool-semantics.md` | 17,714 | mechanic authority | Old mixed mechanic vocabulary and pool contract | 10, docs across every area | `docs/engine/pools.md`; mechanic authority extracted to `docs/mechanics/` |
| `docs/engine/weight-calculation-flow.md` | 13,065 | stable reference | Implemented weighting math | 4, direction/index/engine/solver | `docs/engine/weights.md` |
| `docs/solver/crafting-solver-plan.md` | 31,600 | stable reference | Solver architecture mixed with S1-S8 phasing | 11, current/history/future/product docs | intact archive at `docs/archive/2026-07-19-bestiary-solver-s8/solver-plan.md`; verified `docs/solver/README.md` |
| `docs/product/desktop-workspace-ui.md` | 14,399 | stable reference | Workspace contract mixed with proposals | 7, current/foundation/product/history | `docs/product/workspace.md` |
| `docs/product/strategy-editor-ui.md` | 26,848 | stable reference | Strategy/editor contract mixed with future recombinators | 9, current/foundation/future/history | `docs/product/strategies.md` |
| `docs/economy/economy-ingest-plan.md` | 31,644 | stable reference | Implemented E0-E7 pipeline mixed with phasing | 3, direction/index/implementation | intact archive at `docs/archive/2026-07-15-economy/plan.md`; verified `docs/economy/data.md` |
| `docs/economy/economy-deployment.md` | 2,612 | stable reference | External activation runbook | 2, implementation and docs index | `docs/economy/deployment.md` |

## Deferred Work

| Starting path | Bytes | Classification | Purpose and authority | Inbound | Final destination |
| --- | ---: | --- | --- | --- | --- |
| `docs/future/accounts-publishing-and-discovery.md` | 14,534 | future/deferred work | Unscheduled account/community design | 5, direction/foundation/product/index | `docs/future/accounts.md` |
| `docs/future/ml-strategy-planning.md` | 26,610 | future/deferred work | Unscheduled ML architecture | 3, direction/foundation/index | `docs/future/ml.md` |
| `docs/future/solver-mechanic-extensions.md` | 9,836 | future/deferred work | Unscheduled mechanics/recombinator design | 6, direction/implementation/product/solver/history | `docs/future/mechanics-and-recombinators.md` |

## Existing Archives

Every file here remains historical. Folder READMEs become lifecycle parents;
short type names remove repeated topic wording.

| Starting path | Bytes | Classification | Inbound | Final destination |
| --- | ---: | --- | --- | --- |
| `docs/archive/README.md` | 2,015 | current orientation | 3, direction/docs/root | unchanged, expanded archive index |
| `docs/archive/2026-06-engine-performance/engine-hotpath-findings.md` | 21,814 | audit/report/evidence | 1, performance review | `.../audit.md` |
| `docs/archive/2026-06-engine-performance/engine-perf-review.md` | 22,045 | audit/report/evidence | 1, decision menu | `.../review.md` |
| `docs/archive/2026-06-engine-performance/engine-speedups-and-cheats.md` | 5,004 | historical plan | 0 | `.../decision-menu.md` |
| `docs/archive/2026-07-solver-s6/pre-s6-product-polish-plan.md` | 14,763 | historical plan | 1, implementation plan | `.../product-polish-plan.md` |
| `docs/archive/2026-07-solver-s6/s6-plan.md` | 26,867 | historical plan | 2, sibling plans | `.../plan.md` |
| `docs/archive/2026-07-solver-s6/strategy-calculator-mode-plan.md` | 35,362 | historical plan | 1, S6 plan | `.../calculator-plan.md` |
| `docs/archive/2026-07-solver-s7/HANDOFF-s7.6-final.md` | 9,792 | historical plan | 2, plan and report | `.../handoff.md` |
| `docs/archive/2026-07-solver-s7/solver-depth-and-performance-plan.md` | 40,875 | historical plan | 5, current/future/history | `.../plan.md` |
| `docs/archive/2026-07-solver-s7/solver-simulator-improvement-report.md` | 27,252 | audit/report/evidence | 0 | `.../report.md` |

## Evidence Outside `docs/`

| Starting path | Bytes | Classification | Purpose and authority | Inbound | Final destination |
| --- | ---: | --- | --- | --- | --- |
| `fixtures/solver-baselines/s8.0/README.md` | 10,271 | audit/report/evidence | Frozen S8.0 before-state manifest guide | 0 | unchanged; indexed by `docs/evidence.md` |
| `fixtures/solver-regressions/s8.4r/v1/evidence/README.md` | 7,070 | audit/report/evidence | Frozen S8.4R evidence guide | 0 | unchanged; indexed by `docs/evidence.md` |

## Permanent Authorities After The Move

- Implemented mechanic behavior and dated Oliver rulings: `docs/mechanics/`.
- Canonical data, pools, weights, representations, and ABI/runtime contracts:
  the owning stable area reference.
- Solver formalization and implemented architecture: `docs/solver/README.md`.
- Durable engineering choices: `docs/decisions.md`.
- Pinned cases and measured results: `docs/evidence.md`, linking raw fixtures.
- Deferred, non-executable designs: `docs/future/`.
- Point-in-time plans, handoffs, audits, reports, and design reviews:
  `docs/archive/`.
- Exact current work boundary: root `HANDOFF.md` only while a plan is active.
- Agent workflow: `AGENTS.md`; developer quick start: root `README.md`.

No starting document had unresolved destination or authority after the code
audits. Stale or conflicting stable claims are corrected from current code;
historical wording remains available only in dated archives.
