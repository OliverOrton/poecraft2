# Review of a5ef8a5: what the next session should change

**Repository:** OliverOrton/poecraft2  
**Reviewed remote main:** `a5ef8a5b7d6263fdc60f94957b0b76f7978f8f72`  
**Date:** September 10, 2026  
**Posture:** read-only repository/source review. No native build, solve, test, simulation, queue mutation, repository edit, or public action was performed here.

## Decision

Do not resume the old selector implementation. It reached its specified falsification stop, and the new evidence defeats its application to these captured cases. Select **Goal-Reaching Row Delivery and Reforge Coverage Recovery v1** instead.

The distinction is:

- **Old hypothesis:** completed rows already contain a proper controller; choose them differently.
- **Current task:** construct and service a native goal-reaching continuation, preserve all its failure routes, and then use the existing controller machinery.

M0 is implemented and pushed. Use it. The older reports/calculations remain scientific history; their existence does not override the new native results. The accompanying plan is the new execution instruction; current repository rules and newer local work must still be reconciled.

## 1. What landed

The two new commits after the previously reviewed `44fdca8` include M0 (`b18e757`) and the proper-policy investigation (`a5ef8a5`). M0 supplied the compact tooling map and typed corpus/worker controls for retention activation and host watchdog. It did not create another execution owner. The latest implementation record says all experimental solver-runtime changes were removed and rebuilt ordinary results reproduce B5. A retained test exercises the actual native helpers and complete caller. [R01–R03]

| Captured completed-row view | Discovered | Expanded | Complete rows | True goals | Proper root controller in that view |
|---|---:|---:|---:|---:|---|
| CB06 initial | 2,686 | 32 | 261 | 0 | No |
| CB08 initial | 4,342 | 32 | 260 | 0 | No |
| CB06 after one selected-dependency batch | 3,295 | 160 | 1,331 | 0 | No |
| CB08 after one selected-dependency batch | 4,896 | 160 | 1,639 | 0 | No |

These are committed native measurements, not repetitions performed here. There was no proper independently executable frontier either. A finite view with no true goal or proper committed terminal continuation cannot yield goal absorption by rearranging its action selection. This says nothing about infeasibility of the full native problem. [R02, R04]

The current fixture confirms the narrow rank/infinite-SCC repair limitation, but the complete existing numerical seed finds the toy controller with values 5 and 6. The existing joint progress selector also selects its escape and return. This explicitly narrows the previous research finding: the toy is not a reproduced whole-solver bug. Keep the 77-check native regression and the conditional argument; do not promote the removed generic selector. [R02]

## 2. The reforge concern is now supported, with a precise limit

The bounded per-source table records legal, requested, queued root Harvest actions with no completed rows:

- CB06: `harvest_reforge:defences`, `harvest_reforge:elemental`, `harvest_reforge:fire`.
- CB08: `harvest_reforge:chaos`, `harvest_reforge:elemental`, `harvest_reforge:lightning`.
- Admitted Fossil combinations also remain queued; the Amulet's admitted Wrath Essence remains queued. Exact IDs are in the saved table.

This establishes incomplete exploration at those measured sources, not that any one of those actions would improve the policy or that all such actions are globally starved forever. The coverage table is bounded and is not an exhaustive whole-run action census. [R02, R04]

For Ring root 0, the saved Chaos row is complete with 2,478 direct successor routes lacking rows; the complete Exalt row has 15 such missing routes. Scour has one missing Normal continuation. At the Normal entry, legal Transmute/Alchemy lack rows. These counts motivate comparing the actual continuation burden, not selecting a family merely by its name or immediate price. They are not measurements of comparative policy cost. [R04]

## 3. Zero goals does not mean zero acquired goal modifiers

The terminal requires the requested rarity and satisfactory goal slots **and** no extra occupied explicit affixes. A full goal mask can therefore remain nonterminal. The current native basic reforge code targets four to six rare affixes; early pool exhaustion is a separate condition, so a target count alone is not a proof of minimum final occupancy. [R05, R06]

CB06 is an empty Rare two-goal request; CB08 is an empty Rare three-suffix request. More refilling can generate desirable partial items without creating a junk-free goal. The current record does not report whether a full-goal-but-dirty item is among the discovered population. That must be measured before declaring cleanup to be the actual cause. [R07, R08]

**New implementation hypothesis:** a true-goal-producing cleanup/addition/rarity transition is missing or not serviced, and broad selected-prefix completion is opening far more work than necessary to find one first controller.

Start by separating:

1. no sufficient goal modifiers have been acquired;
2. all required modifiers are present, but junk/rarity/control prevents terminal success;
3. a near-terminal source exists but its finishing action is queued, absent, refused, or interrupted;
4. a terminal row exists but its failure/recovery branches are not executable.

These are proposed diagnostic distinctions, not facts already measured on CB06/08. Do not turn a full mask into success, grant cleanup for free, or infer inapplicability from zero observed terminals.

## 4. Source seams worth investigating—not diagnosed defects

The legacy scheduler in `schedule_next_incremental_alternative` has an early return when alternative rows exist but the retained carrier count is below the warm-start checkpoint (128). The high-progress mode is explicitly disabled; source describing its more elaborate lanes is not evidence that they run. The current publication path's open-incremental joint attempt also requires an incumbent. These are relevant control seams, not proof of which branch caused the captured stop. [R09–R12]

Have Codex trace the actual branch conditions at the known stop. Distinguish a temporary work-order yield, a blocked prerequisite, a completed scope, and an actual resource/finish refusal. Do not simply remove 128, enable a previously rejected scheduler, or force an unbounded drain. Prior B1/B2 and the single joint-dependency batch already showed that indiscriminate additional service is insufficient. [R02]

## 5. What a useful new witness establishes

There are three stages:

1. A complete native action at a genuinely reachable item has positive mass into the **actual terminal predicate**.
2. All its positive-mass nonterminal outcomes can be routed by a finite proper candidate, including destructive failures and retries.
3. The emitted strategy is independently evaluated and returned in the original full-scope solve.

A favorable path or a single terminal leaf establishes only stage 1. A policy built from a selected subset of allowed actions may provide an executable upper after qualification, but it does not close the remaining action ledger or establish an optimum. Preserve existing certified lowers unchanged unless a separate valid proof changes them.

For these empty Rare starts, compare a reached cleanup route with the existing additive route and a relevant queued reforge. Annul/Regal/Exalt or other actions are **candidate probes only when native legality and exact rows justify them**. This report does not prescribe a new universal recipe, an action preference baked into production, or a reset that secretly enables economic Restart.

## 6. Resources and guidance

Ring's latest dependency experiment refuses admission at 473,470,127 recorded peak owned bytes, under a 1 GiB cap; the requested next scratch allocation is not reported in that peak. Capture retained ownership, requested increment, actual cap, and action/source before attributing this to overconservative accounting. [R02]

The plan keeps the 1 GiB baseline and permits one separately labelled 4 GiB native diagnostic only when a specifically necessary row is memory-blocked and existing host admission can safely reserve it. No 8 GiB campaign, silent cap increase, or relabelling of a capacity win as a same-budget algorithmic win is selected.

Adaptive cost estimates, zero-goal policy simplification, parallelism, and stronger lower models remain possible later directions. None is a prerequisite here. In particular, an estimate cannot replace a terminal route, and the already implemented zero-progress retry restriction must not silently change.

## 7. Required knowledge update

Append the new native disposition to the existing policy/claim research owners. Keep the distinction between a real narrow helper limitation and a falsified native applicability hypothesis. Preserve the source/operator coverage evidence, and add the actual terminal-reaching chain and its blocker once known.

Do not create another mathematical ledger, tool inventory, or raw-report framework. M0's compact route is the route for this work. Historical local-only wording in old receipts stays historical; the live HANDOFF should state the actual next task and current local/push status.

## Limits

This review inspected current records, a bounded portion of `results.json`, and selected scheduler/priority/publication/action source. It did not inspect every case row or prove a new native controller exists. The cleanup/goal-debt hypothesis and control seams are leads for a bounded native experiment. No speedup, recovered policy, or new exact closure is claimed.

References resolve in `source_register.json`. All repository references there are pinned to the reviewed main, except where historical revisions are explicitly named.
