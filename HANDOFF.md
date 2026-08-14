# Session Handoff

**Status: active implementation boundary at Gate 1.**

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

Gate 0 is complete. The normal 8/off request and 1,024/off control both reached
the same 705-state, 5,781-row, 109-round interval with lower zero and upper
`13882.856439060486`, then failed to return a public result by the five-minute
outer boundary. The 8/on and 1,024/on controls both closed exactly at
`2186.6911143146394` with identical transition/policy hashes in 133.594 and
136.817 seconds respectively. Eight-item stepping was faster and kept measured
native steps under 77 ms, so larger chunks are not the repair.

Gate 0 also reproduced a separate progress/cancellation defect. In the 8/on
run, native progress reported `done` after 0.286 seconds at value
`2244.8394347831436`; synchronous final extraction then consumed 133.301
seconds and changed the exact value to `2186.6911143146394`. No progress or
cancellation boundary exists inside `finishSolverSolve`. The off-mode
watchdog likewise could not return while final extraction was running.

Do not raise caps, weaken proof, enable a benchmark-only scheduler blindly,
change mechanics/action scope/prices, or trade throughput for multi-second
uncancellable steps. The selected release-WASM product witness must eventually
finish within five minutes with the same terminal semantics as its fixed
1,024-work control, while individual worker steps remain at most 250 ms.

Next action: checkpoint the frozen witness and Gate 0 evidence, finish the
Gate 1 soundness/breadth audit of high-impact scheduling, and isolate a
cooperative design for final extraction. A synthetic `finalizing` label alone
is insufficient because cancellation must remain real. The full acceptance
pipeline is reserved for the end of the milestone.
