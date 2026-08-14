# Session Handoff

**Status: active implementation boundary at the Gate 2 architecture decision.**

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

Gate 1 is complete. The current operator-major scheduler closes the exact
delayed state/operator-pair ledger and retains independent strict/publication
proof authority. Under eight-item stepping, the primary, Oliver witness,
Eldritch, Warlord, and Imprint controls all close exact without watchdogs. The
normal Calculator now opts into that scheduler.

The worker and Calculator progress repair is retained. Native `Done` becomes a
worker-owned `finalizing` phase, finalization has its own wall-time metric, and
the real final `done` snapshot is rebuilt from the published summary. The UI
shows elapsed time plus discovered/frontier/row/transition/reforge/memory work.
The exact witness now returns in about 142 seconds instead of failing the
five-minute boundary, with max bounded step 76.028 ms and the final exact value
`2186.6911143146394`.

One architecture boundary remains: the 142.123-second strict lift is still a
single synchronous WASM finalization call. A cancel at the finalizing boundary
prevents it; a cancel clicked after it begins is preserved as cancelled but
cannot shorten the wait. Genuine interruption requires an incremental retained
strict-lift work object across C ABI/WASM steps (principally 4,215 selected
kernel builds here), or an equivalent isolated reconstructible worker. A UI
label cannot satisfy that part of Gate 2.

Next action: checkpoint this selected implementation and decide whether the
incremental strict-lift architecture remains inside this milestone. Do not
start final acceptance or archive while the Gate 2 responsiveness contract is
unresolved. The full pipeline remains reserved for the end.
