# Documentation Index

**Status: current documentation map and lifecycle policy.** This file organizes
the library; [Project Direction](direction.md) summarizes product direction and
[HANDOFF](../HANDOFF.md) owns the exact next implementation boundary.

## Start Here

Read current work in this order:

1. [Project Direction](direction.md)
2. [HANDOFF](../HANDOFF.md)
3. [B1 Bestiary And S8 Solver Capability Plan](active/bestiary-and-solver-capability-plan.md)

[Implementation Plan](implementation-plan.md) is the portfolio-level roadmap.
It is broader and more historical than the exact boundary in `HANDOFF.md`.

## Folder Lifecycle

| Folder | Purpose | Sequencing authority |
| --- | --- | --- |
| `active/` | The one current phased execution plan | Yes, within the boundary named by `HANDOFF.md` |
| `foundation/` | Stable architecture and repository ownership | No |
| `engine/` | Implemented data, item-state, pool, weight, and bitset references | No |
| `solver/` | Stable solver architecture | No |
| `product/` | Implemented workspace and strategy-model references | No |
| `economy/` | Implemented economy architecture and deployment operations | No |
| `future/` | Deferred research, mechanic, account, and product designs | No |
| `archive/` | Completed execution plans, final handoffs, and historical reports | Never |

Root-level `direction.md` and `implementation-plan.md` are the only Markdown
documents kept directly under `docs/` besides this index. New phased work goes
under `active/`; when complete, it moves to a dated archive folder.

## Active Work

- [B1 Bestiary And S8 Solver Capability Plan](active/bestiary-and-solver-capability-plan.md)

## Stable References

Foundation:

- [Architecture Plan](foundation/architecture-plan.md)
- [Codebase Structure](foundation/codebase-structure.md)

Engine and data:

- [Data Shapes And Ingest](engine/data-shapes-and-ingest.md)
- [Engine Bitsets](engine/engine-bitsets.md)
- [Item State Flow](engine/item-state-flow.md)
- [Mod Data And Pool Semantics](engine/mod-data-and-pool-semantics.md)
- [Weight Calculation Flow](engine/weight-calculation-flow.md)

Solver:

- [Crafting Solver And Calculation Engine Plan](solver/crafting-solver-plan.md)

Product:

- [Desktop Workspace UI](product/desktop-workspace-ui.md)
- [Strategy Editor UI](product/strategy-editor-ui.md)

Economy:

- [Economy Ingest And League Switching](economy/economy-ingest-plan.md)
- [Economy Refresh Deployment](economy/economy-deployment.md)

## Future And Deferred Work

- [Accounts, Publishing, And Discovery](future/accounts-publishing-and-discovery.md)
- [ML Strategy Planning Architecture](future/ml-strategy-planning.md)
- [Parked Mechanic And Recombinator Extensions](future/solver-mechanic-extensions.md)

## Historical Evidence

[Archive Index](archive/README.md) lists completed plans and their final
handoffs/reports. Archived documents preserve the wording and decisions of their
time; their internal sequencing language is not current authority.

## Maintenance Rules

- Every document starts with a lifecycle/status statement.
- `HANDOFF.md` names one exact next boundary and links one active plan.
- `direction.md` stays short and links subjects instead of duplicating specs.
- Stable references describe implemented contracts; phased checklists belong in
  `active/` or `archive/`.
- Future documents may contain architecture sketches but must state that they
  are not scheduled.
- Completed plans and point-in-time reports move to a dated archive folder; do
  not delete their failed gates or superseded decisions.
- Keep links relative and run the local Markdown-link audit after moves.
