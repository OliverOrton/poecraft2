# Session Handoff

**Status: active implementation boundary at Gate 0.**

Oliver selected
[Calculator WASM Scheduling And Progress](docs/active/2026-08-14-calculator-wasm-scheduling-progress/plan.md)
on 2026-08-14 from a live normal-Calculator report. The frozen source baseline
is `191bc010aad7ece2a227766d6000117074ad9e52` on
`codex/solver-goal-realignment`.

The exact witness is an empty rare item-level-86 Conquest Lamellar
(`Metadata/Items/Armours/BodyArmours/BodyStrDex20`) in the published Allflame
snapshot `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`.
Success requires all three T1-or-better natural prefixes:
`LocalIncreasedArmourAndEvasionAndStunRecovery6`,
`LocalIncreasedArmourAndEvasion8`, and
`LocalBaseArmourAndEvasionRating8`.

The primary product reproduction uses no manual price override. The snapshot
has no `base` price. A separately labelled `base = 5` comparison may be used
only to relate this witness to the earlier four-goal qualification fixture.

Gate 0 must create the deterministic witness and reproduce the actual release-
WASM Calculator profile: product action scope and pricing, normal Calculator
options, high-impact mode off, and the worker's effective eight-work-item
maximum. Capture the full progress sequence under a bounded watchdog. Then run
the three isolated controls: 1,024/high-impact-off, eight/high-impact-on, and
1,024/high-impact-on. Do not infer a scheduler repair before these modes are
compared.

The currently known source facts are that the archived 147.292-second primary
used both 1,024 work items and diagnostic high-impact scheduling, while the
normal Calculator uses at most eight and does not enable that mode. The UI's
1,263 states is only expanded-state progress; existing rows, transitions,
reforge work, memory, and other counters may continue while it and the bounds
stay flat. A zero lower while the delayed action envelope is open is sound and
must remain zero.

Do not raise caps, weaken proof, enable a benchmark-only scheduler blindly,
change mechanics/action scope/prices, or trade throughput for multi-second
uncancellable steps. The selected release-WASM product witness must eventually
finish within five minutes with the same terminal semantics as its fixed
1,024-work control, while individual worker steps remain at most 250 ms.

Next action: commit this activation boundary, add the exact fixture/diagnostic
runner support if needed, and execute Gate 0 only. The full acceptance pipeline
is reserved for the end of the milestone.
