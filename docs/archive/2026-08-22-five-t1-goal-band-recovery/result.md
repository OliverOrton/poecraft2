# Five-T1 Goal-Band Recovery Result

**Status: complete and accepted locally (2026-08-22).**

Parent: [plan](plan.md)

## Outcome

The exact owner-supplied four-of-five Conquest Lamellar now publishes a useful
independently evaluated strategy in about 22 seconds natively and in release
WASM. The strategy costs `2083.88214353439` Chaos, has 23 nodes and 36 edges,
is proper, completely priced, succeeds with probability one, and has zero
off-policy mass. Native and release WASM each complete 10,000 / 10,000 seeded
Simulator runs with zero failures and zero off-policy failures.

This boundary did not add a goal-mask-only continuation quotient or impose a
fixed crafting order. The measured defect was narrower: completed alternative
rows were tested one at a time, so a group of rows that formed a much cheaper
proper policy could remain unpublished. The solver now periodically constructs
and retains a joint start-reachable proper policy at geometrically growing
completed-row checkpoints. Those rows remain outside lower-bound and exact
envelope authority; only the immutable executable incumbent is retained.

Calculator also has a distinct bounded-finish lifecycle. After four minutes of
worker wall time it asks native Solve to stop open discovery at a cooperative
boundary and run its ordinary compilation, certification, and exact graph
evaluation. This is not cancellation, a fabricated cap, or exact closure. The
public termination and stop cause are both `requested_bounded_finish`.

## Primary evidence

[Native acceptance](requested-finish-native-20s-final.json) requested bounded
finish after 20 seconds and completed in `21.2893967` solve seconds and
`23.1825574` total seconds, including exact graph evaluation and simulation.
At the request boundary the solver had:

- 3,047 discovered states, 898 expanded states, and 2,149 frontier states;
- 22,856 rows, 79,612 retained transition entries, and 188,182 V1-equivalent
  reforge work;
- a `0.01165` globally certified lower and a `2139.976899001699` retained
  executable estimate;
- 17,636 open action obligations, so exact closure was not claimed;
- three joint-anytime attempts, one improving retained policy, and a best
  sparse-policy estimate of `2143.211884`;
- 53,307,653 live and 124,297,621 peak selected-owned bytes.

Final independent evaluation corrected the sparse estimate to
`2083.88214353439`; publication uses that evaluated value for both the upper
and returned-policy cost. Exact evaluation took `160.6597` ms and proved
success mass one, zero off-policy mass, complete pricing, and exact cost
reconciliation. The graph expects 61.274 actions per craft. Its selected
program uses Exalted Orbs, a temporary Strength/Dexterity blocker and crafted
modifier cleanup, Prefixes Cannot Be Changed with Harvest Reforge Physical,
and Eldritch Ichor plus Eldritch Chaos. It is not Chaos spam.

The 10,000 seeded native simulations completed in `1.6537654` seconds with
10,000 successes, zero failures, zero off-policy failures, and sampled mean
`2087.030535604002` Chaos. The sample is statistical evidence; the exact
`2083.88214353439` evaluation remains cost authority.

[Release-WASM acceptance](requested-finish-wasm-20s-final.json) reaches the
same result in `21.851909` solve seconds and `26.518839` total seconds. It
preserves the same lower, upper, evaluated cost, 23-node / 36-edge graph, and
10,000 / 10,000 simulation result. The largest worker slice is `6942.45` ms.
The reported WASM linear-memory high-water remains `278,396,928` bytes, but
the headless Node process RSS grows from 335,691,776 to 3,277,590,528 bytes.
That discrepancy is test-process evidence, not a browser heap claim, and keeps
real-browser memory/responsiveness measurement open.

## Action disposition

The exact primary has zero missing-price and zero unsupported-action skips.
The explicit priced candidate envelope contains 28 actions: 10 ordinary
currency, 13 Fossil, four Harvest, and one Fracture. Carrier-local automatic
dependencies add 147 Bench options, eight Eldritch setup actions, one each of
Eldritch Chaos, Annul, and Exalt, plus the existing protected-side and cleanup
programs. Imprint programs and voluntary economic Restart are excluded by the
owner-selected request; mechanic-owned recovery remains independent.

The 102 filtered product actions have explicit reasons: 61 Essences do not
force an exact goal modifier, four Essences are corruption-only, 29 Harvest
tags do not target a goal tag, four Influence Exalts have no goal mod, one
Bench action has no option effect, and the three veiled operations remain
deferred. This is action-scope evidence, not a claim that a filtered family is
globally dominated.

## Controls

- [Warlord](warlord-control-final.json) remains exact at
  `224.123858897249` Chaos in 1.874 solve seconds. Its 8-node / 13-edge graph
  exactly evaluates and completes 10,000 / 10,000 simulations with zero
  off-policy failures.
- [Magic Imprint retry](imprint-control-final.json) remains exact at
  `252.653520212745` Chaos in 0.322 solve seconds. Its 9-node / 11-edge graph
  exactly evaluates and completes 10,000 / 10,000 simulations with zero
  off-policy failures.
- [Four-T1](four-t1-control-final.json) retains the accepted bounded
  Imprint-work refusal: lower `21.7724594013328`, upper
  `3759.97631221018`, exact-evaluated 79-node / 223-edge graph, and 10,000 /
  10,000 successful simulations. The older corpus still expects exact closure,
  so its expectation flag remains false; the measured proof/result
  classification and value are unchanged.
- A new [non-armour partial-five control](non-armour-control-final.json) uses
  an exact rare Spine Bow with four natural T1 goals already present. It
  truthfully publishes a bounded, exactly evaluated proper graph after the
  requested finish with zero missing prices. Its attractive `74.9819737303`
  provisional row policy is not structurally materializable at that early
  boundary, so the verified publication remains the much weaker
  `2042605033.16647` renewal. This control is exact-evaluation-only and records
  that partial-frontier materialization remains a cross-base quality limit; it
  does not weaken the accepted armour primary.

## Correctness boundary

Joint anytime construction temporarily considers only fully materialized
completed rows, finds a start-reachable proper fixed policy, and restores every
row's prior admission state afterward. It does not close an open action family,
admit a row to the lower problem, or establish optimality. Final publication
still compiles the retained policy and independently evaluates the complete
runtime graph.

Requested bounded finish rolls back any staged state-local automatic
transaction and restores a live incremental upper pass before finalization.
The retained incumbent is immutable. Requested finish disables exact-closure
and target-gap promotion, preserves the certified global lower, reports zero
cap bits unless a real finalization cap occurs, and keeps all unresolved action
obligations visible. AbortSignal remains prompt abandon with no publication.

A regression test covers the request-to-finalization step boundary. During
qualification it caught and fixed a loop in which finalization repeatedly
prepared the same bounded-finish request instead of advancing.

## Acceptance

- `powershell -File scripts/build.ps1`: pass.
- Solver API gate: 2,990 checks, zero failures.
- Focused solver gate: 96,551 checks, zero failures.
- `powershell -File scripts/build-wasm.ps1`: pass; tracked release module
  rebuilt with the 62-symbol export manifest.
- `npx tsc --noEmit`: pass.
- Complete `npm test`, including 28 / 28 release-WASM smoke checks: pass.
- Native and release-WASM primary exact evaluation plus 10,000-run
  verification: pass.
- `git diff --check`: pass.
- Full repository pipeline: deliberately not run under the selected plan.
- Rendered/visual review: deliberately not run; it remains Oliver's.

## Remaining work

The primary is useful but not exact: 17,636 alternatives remain open and the
global gap is large. The nearest separate owners are improving the lower bound
and action-obligation closure, making provisional partial-frontier policies
materializable on non-armour cases, reducing the 6.94-second WASM work slice,
and measuring the Node-RSS/browser-memory discrepancy. None is required to use
the verified primary strategy now.
