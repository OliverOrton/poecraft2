# Recovery-Scoped Restart And Successor-Aware Bounds Result

**Status: complete (2026-08-22).**

Parent: [plan](plan.md)

Source checkpoint: `1e21260`

Release-WASM checkpoint: `cfd8904`

## Accepted behavior

Calculator solves now exclude ordinary economic Restart by default. An
unchecked option labelled “Allow abandoning this item and buying a fresh
base” restores the historical action scope. Native and explicit engine callers
remain backward compatible unless they set the new disable flag.

Mechanic-owned replacement is separate. Product-local Fracture miss still
charges the `base` price, reaches a fresh Normal carrier, and compiles one
dedicated retry operation. Bounded policies without economic Restart route an
unmatched state to failure instead of inventing a fresh-base action.

Operator lower bounds now use
`may_survive(source, operator) | may_reach(operator)`. Runtime execution paths
apply the existing refinement contracts sequentially. Exact reset removes
source progress; incomplete or uncertain semantics retain it; may-destroy
actions such as Annul retain every slot that any successor can preserve. The
state heuristic takes the maximum of its universal and proved shape-aware
components, and exact Restart prices the fresh Normal successor. The existing
strict rare-carrier cover remains available outside Eldritch-eligible sessions.
The reported concrete-rare sort defect was stale: the current source already
counts prefix plus suffix on both sides and required no change.

Disabling Restart exposed a separate initializer assumption. A legal Warlord
policy can contain a stochastic retry SCC rather than a strict rank DAG. In
restricted mode, and only when no executable incumbent exists, the solver now
seeds unresolved states deterministically and lets the existing exact SCC
evaluator prove properness or reject the component. The seed is never
publication authority. Historical Restart-enabled initialization is unchanged,
and an already-certified restricted incumbent does not pay for this fallback.

## Soundness controls

Focused native tests prove:

- unrestricted mode still selects Restart when it is genuinely cheapest;
- restricted mode has no ordinary Restart policy row;
- Product Fracture retains exact paid replacement and completes 10,000/10,000
  sampled runs;
- bounded unmatched routing fails closed without economic Restart;
- Chaos and Scour remove ordinary source progress but preserve fractured goal
  progress;
- Annul retains all source goal slots because each may survive; and
- every checked strengthened lower is no greater than immediate cost plus the
  exact expected successor heuristic, with deterministic repeated results.

The representative native values were:

| Operator | May-survive mask | Operator lower | Exact relaxed backup |
| --- | ---: | ---: | ---: |
| Chaos | `0` | `100` | `102.778139` |
| Scour | `0` | `11` | `11` |
| Annul | `7` | `5` | `7` |

## Product measurements

The checked Allflame four-natural-T1 Conquest Lamellar case completed solve
and finalization in `47.789` seconds, then completed required verification in
`65.049` seconds (`113.838` seconds total). It stopped truthfully at the
pre-existing `max_imprint_program_work` cap, not the watchdog:

- 3,324 discovered / 1,207 expanded / 2,117 frontier states;
- 11,405 state-action rows and 31,126 transitions;
- certified lower `21.772459401332767`;
- independently evaluated executable upper `3759.9763122101763`;
- 347,209,083 native peak-owned bytes;
- 87 compiled nodes / 241 edges; and
- 10,000/10,000 successful simulations, empirical mean
  `3737.4451776349074`.

The ordinary Restart action has zero search rows. The graph's only Restart
operation is `product_fracture_restart`, reached exclusively after a selected
Fracture miss. Exact graph evaluation matched the published cost with success
one and zero off-policy mass. The result remains bounded because finite
Imprint-program closure hit its existing work cap; this milestone did not
weaken that classification or claim global exactness.

The Allflame Warlord control closes exactly at `224.1238588972487` in `1.930`
seconds. Its 8-node / 13-edge graph uses Warlord Influence Exalt followed by a
Harvest Reforge Fire retry SCC, contains no Restart, exact-evaluates at the
solver value, and completes 10,000/10,000 simulations with empirical mean
`223.95892804999892`.

Final reports and compiled artifacts:

- [primary native report](primary-native.json)
- [primary strategy](strategies/conquest-lamellar-allflame-four-natural-t1.strategy.json)
- [Warlord native report](warlord-native.json)
- [Warlord strategy](strategies/vaal-regalia-allflame-warlord-exalt-goal.strategy.json)

The retained [pre-gating primary diagnostic](diagnostic-primary-before-scc-seed-gating.json)
and [pre-fix Warlord diagnostic](diagnostic-warlord-before-scc-initializer.json)
record the initializer failure that led to the bounded SCC seed. Matching
`.partial.json` files are the benchmark runner's last cooperative snapshots,
preserved rather than discarded.

## Acceptance

- `powershell -File scripts/build.ps1`: pass.
- Native solver: 96,543 checks, zero failures.
- Native compiler: 840 checks, zero failures.
- Native API: 2,978 checks, zero failures.
- Native Calculator: 436,308 checks, zero failures.
- Native refinement: 362 checks, zero failures.
- `powershell -File scripts/build-wasm.ps1`: pass; only the existing wasm32
  tautological 64-bit capacity-guard warnings were emitted.
- `npx tsc --noEmit`: pass.
- Complete `npm test`: pass, including 28/28 release-WASM smoke checks.
- Required compiled-policy verification: 10,000 runs per qualified strategy.
- Full repository pipeline: deliberately not run.
- Rendered/visual browser review: not run; it remains Oliver's.

## Remaining boundary

This work removes an overly broad action and strengthens safe lower-bound
inputs, but it does not close the primary exactly. The next solver owner, if
selected, is the capped finite Imprint-program closure and its downstream
strict alternative/refinement work. No successor is selected by this result.
