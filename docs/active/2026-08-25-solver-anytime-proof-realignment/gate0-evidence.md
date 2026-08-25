# Gate 0 — Current-semantics baseline

Status: passed on 2026-08-25. This evidence is current-semantic authority for
the active plan, not final acceptance.

## Reproduction identity

- Manifest: `fixtures/solver-anytime-proof-realignment/v1/manifest.json`.
- Engine/product checkpoint: `a1449fa`; documentation/preflight checkpoint:
  `fe2c542`.
- Artifact: the manifest-pinned `data/compiled/current` dataset, source version
  `repoe-e4eaf06c20e1ddb4`.
- Economy and action scope are case-pinned. General product cases use exact
  terminal goals, gated destructive reforges, no voluntary Restart, and
  Imprint off. The tri-elemental Restart control, explicit-action oracle, and
  explicit Imprint-on control disclose their separate scopes.
- Sampled verification was skipped at this implementation gate. Every
  published strategy completed independent exact evaluation; the final gate
  owns required 10,000-run verification.

The native harness now rejects an active-plan case when its role, economy,
action scope, terminal threshold, work/wall controls, caller-selected semantic
flags, full-evidence setting, or exact-evaluation contract is implicit.

Reproduction command:

```powershell
powershell -File scripts/benchmark-solver.ps1 -Runner native -SkipBuild `
  -SkipVerification -Progress `
  -Corpus fixtures/solver-anytime-proof-realignment/v1/manifest.json `
  -Label solver-anytime-gate0
```

## Baseline outcomes

| Role | Status | Certified lower | Evaluated upper | Graph | Wall ms | Max slice ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| clean zero→five | bounded | 36.4286171890906 | 14454067.42607058 | 514/1788 | 73577.454 | 4284.545 |
| four-of-five fixed work | named 1,000-state refusal | 3.47245 | 59810.953776974464 | 51/149 | 41083.887 | 4860.634 |
| owner fractured four→five | bounded | 3.47245 | 2698.8747960143623 | 215/563 | 21848.866 | 4875.952 |
| dirty preserved three→five | exact | 0.0380128125 | 0.0380128125 | 6/7 | 908.124 | 550.078 |
| partial-five Bow, 10-second search | bounded | 101.067 | 6026985788.494062 | 154/387 | 58963.592 | 15601.674 |
| tri-elemental Bow | bounded | 0 | 79273.32503083374 | 83/239 | 42020.962 | 2771.486 |
| Warlord | exact | 307.5563120367928 | 307.5563120367928 | 21/45 | 15005.460 | 573.205 |
| ordinary one-mod oracle | exact | 23.78999999999971 | 23.78999999999971 | 6/7 | 316.507 | 159.508 |
| explicit Imprint-on | exact | 705.3942599999995 | 705.3942599999995 | 8/10 | 828.617 | 297.665 |

All nine declared expectations passed. Exact evaluation reported a proper,
completely priced, finite policy with success probability approximately one
and zero off-policy mass in every case. Open-envelope restricted values did
not become public lower authority; the bounded product cases retained their
independently global relaxation lower.

Selected executable families included the expected protected-prefix plus
Eldritch retry for the owner carrier; Eldritch Annul/Exalt for the dirty
carrier; Warlord Exalt for the influence control; and Imprint/restore/Regal
only in the explicit Imprint-on control. The exact dirty control keeps
`eldritch_exalt` dependency-only and admits no primitive actions.

## Trajectory and attribution

The full-evidence native report retained 135 trajectory samples for clean
five-T1, 46 for the owner carrier, 21 for the 10-second Bow, 28 for the
tri-elemental Bow, and complete shorter traces for the other controls. It
records each incumbent change, lower component, action-envelope state,
goal-mask/carrier distribution, rows, transitions, reforge work, memory,
phase wall, maximum slice, exact-evaluation result, action/material use, and
compiled graph census.

First finite/first independently verified upper milestones were:

| Case | First finite ms | First verified ms |
| --- | ---: | ---: |
| clean zero→five | 39.732 | 68932.692 |
| four-of-five fixed work | 13.210 | 19196.806 |
| owner fractured four→five | 13.108 | 21083.952 |
| dirty preserved three→five | 194.147 | 194.147 |
| partial-five Bow | 57.917 | 35438.261 |
| tri-elemental Bow | 30573.155 | 30573.155 |
| Warlord | 14483.044 | 14483.044 |
| explicit Imprint-on | 645.295 | 645.295 |

At stop, the open cases truthfully disclosed their outstanding envelopes:
clean `333 admitted / 203898 unevaluated / 20878 unresolved`; fixed-work
four-of-five `308 / 0 / 22404`; owner four-of-five `1903 / 102353 / 6772`;
partial Bow `0 / 7402 / 269`; and tri-elemental Bow `0 / 7506 / 1146`.
The dirty, Warlord, oracle, and Imprint controls closed their envelopes.

Candidate publication samples retain the candidate identity, source/stage,
estimated and independently evaluated costs, reconciliation delta,
disposition, and reason for every retained or rejected publication candidate.
Scheduling attribution is recorded by named admission stage and carrier shape;
automatic admission/transition work is recorded per action, allowing exact
family aggregation. Gate 0 also added typed `no_finite_incumbent` counts for
each operator family whose admissible lower evaluation was skipped.

## Repetition authority

Three fresh runs of the owner four→five control reproduced:

- upper `2698.8747960143623`, lower `3.4724500000000003`;
- 215 nodes and 563 edges;
- strategy SHA-256
  `2062ec8b3400fd5cb54538ce359b22821e681dfa11049c4cab5a8f696eac5db5`;
- identical selected action/material authority; and
- wall times `21848.866`, `21637.822`, and `21600.783` ms.

The fixed-work refusal, values, properness, selected policy, and graph identity
are therefore stable. Wall-time variation is informational only.

## Gate decision

Gate 0 passes. The required corpus is pinned and validated, current semantic
outcomes are reproduced, policy/value identity is stable under repetition,
and the missing skip reason is now observable without changing solver
behavior. Gate 1 may establish the typed action-envelope lifecycle.
