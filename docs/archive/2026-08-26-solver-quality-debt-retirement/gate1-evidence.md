# Gate 1 — Broad-to-exact refinement attribution

Gate 1 uses
`conquest-lamellar-allflame-fractured-4-to-5-product8`, the shortest Gate 0
witness with a proper direct exact certificate followed by strict-lift failure.

## Neutrality

Across the pre-attribution and named-path reruns:

- lower: `36.4286171890972`;
- exact published upper: `2698.87479601436`;
- direct certification: `complete`, proper, cost-complete, zero off-policy,
  and cost-reconciled;
- compiled graph: 215 nodes / 563 edges; and
- core policy hash: `9c020a941326a237`.

The diagnostic changed only the strict-lift failure detail. Safe bounded
publication retained the same independently evaluated strategy.

## Masked failure

Selected-first refinement previously called
`account_selected_closure()` before checking the result returned by
`QuotientBellmanGraph::solve()`. An incomplete result has no authoritative
`selected_rows_by_state`, so the later indexing check replaced the first
failure with:

```text
proof envelope lost its selected quotient state
```

Checking the result at its authority boundary exposes the actual status:

```text
improper_policy: quotient Bellman entry has no certified path to a terminal
```

Bounded path attribution then names the first dead certified chain:

```text
9[option:eldritch_side_intent:suffix:eldritch_exalt:eldritch_ichor:1]
  > 10[scour]
  > 0[regal]
  > 1[scour]
  > cycle:0
```

The entry has one admitted row and one current certified row, but no row whose
entire support remains inside the terminal attractor. The first row reaches
cell 10, whose inherited selected continuation enters the deterministic
Scour/Regal renewal cycle.

## Root cause

This is not a missing vector element and not a literal quotient-state loss.
The selected-first implementation publishes only inherited selected rows,
demands that those rows already form a proper terminal-reaching policy, and
only after that success accounts and schedules alternative actions. Therefore
an inherited strict policy that is improper cannot reach the local alternative
certification machinery designed to repair it.

The contradiction is observable on the same candidate: direct compiled exact
evaluation proves the public policy proper and charges only the root Eldritch
setup/exalt path, while the selected-only strict quotient follows the dead
Scour/Regal chain. The first divergence is the continuation of the root
Eldritch side-intent outcome, before publication or lower-bound authority.

Unavailable `ModifierExclusionSignature` remains a widespread compatibility
counterexample (70 on this witness), but it is not promoted to the cause of
this failure. The deterministic cause proven here is bootstrap ordering:
alternative repair authority is created too late.

## Gate 2 requirement

Gate 2 must let selected-first refinement bootstrap an improper inherited
policy through bounded, on-demand certification of alternatives on the dead
reachable envelope. It must not globally materialize exclusion identity, must
retain the already verified direct incumbent as rollback authority, and must
stop as soon as a proper strict upper is recovered or the existing limits
produce a truthful resource/refinement refusal.
