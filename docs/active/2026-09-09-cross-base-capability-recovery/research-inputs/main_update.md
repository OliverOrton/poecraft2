# Main update and audit revisions

**Reviewed main:** `be553608ecabde28a3dec07856255911459ec97d`  
**Previous audit:** `317438392c88c0151f41e48b4b0323b06b1d0037`  
**Change:** one direct child commit, “Enable candidate proof handoff and demanded continuations.”  
**Scope:** source/diff and retained experiment review; no repository edits, public actions, native runs, or new simulations.

## Decision

Keep the cross-base capability programme. Do not send a fresh session back to implement the handoff, repeat parent-4741 diagnosis, or merely connect the same heuristic consumer: those mechanisms have now landed. The next shared implementation target is conditional on the cross-base measurements, with **bounded frontier completion and partition-memory/repeated-work reduction** now the leading hypotheses. A new endgame library is still not selected. `[R27–R33]`

## What landed

The native benchmark has an optional `--proof-handoff-seconds` request. It reaches an existing stable-candidate checkpoint and suppresses both main-loop recovery and publication-preflight discovery reopening for that diagnostic attempt. It is not a public/browser default and does not extend or clear a real finish request. Paused discovery families remain open. `[R27, R29, R30]`

The strict quotient path can construct complete demanded rows beyond the immutable selected-policy table, defers nonprimitive applicability/kernel work until demanded certification, and can register a demanded physical parent without copying old selected decisions or inheriting unproved action admission. Fixed-policy and compressed-retry refusals remain. Physical parent registration has focused-fixture support; zero use in F6/F7 means it has no demonstrated causal real-case gain in those runs. `[R28, R29]`

A typed existing prepared lower is now consumed across every member of a strict cell. No new lower model is prepared by this consumer. The added alternative floor is the maximum of the immediate-price floor and the minimum statewise heuristic over the complete cell; it is not a newly computed successor-conditioned action value. `[R27, R32]`

## What the longer experiments established

| Observation | Result | Interpretation |
|---|---|---|
| F1, previous-source ordinary four-goal long control | L 198.8335; U 8,500.235; 245.282 s; zero strict rows | More discovery alone did not create proof time; worse than the known short-run 5,218.041 policy |
| F2, previous-source ordinary five-goal long control | L 405.3694; U 85,558.706; 249.986 s; zero strict rows | Extra elapsed time did not improve the endpoint |
| F3c, working diagnostic handoff | 5,252 strict kernels; 23.811 s strict work; old 5,218.041 policy returned | Establishes a real pre-finish window, not a new policy |
| F4/F4b, demanded new rows | 29 complete rows / 538 transitions; then state cap | Passes the old table boundary; identifies eager option admission as the next cap owner |
| F5, deferred option kernels | 1,832 new rows / 27,083 transitions; then missing physical parent | Removes the identified admission-cap obstruction, not the whole closure cost |
| F6, four-goal proof without added strict floor | 178.480 s strict work; one frontier-growth yield; root unchanged | Supported continuation work can consume the full window |
| F7, four-goal proof with added strict floor | 74,015 strengthened obligations; zero noncompetitive retirements; root unchanged | A real heuristic consumer, but no material root or elimination gain |
| F8, five-goal proof treatment | 117.089 s strict work; 16,010 exact states; replay-backed partition memory cap | More time is not the immediate remedy; the prior verified policy survives |

All entries are retained repository observations, not fresh runs in this audit. F1/F2 are controls on `3174383`, not newly measured ordinary 240-second runs of `be55360`. A same-source performance comparison must preserve that distinction. `[R28, R31]`

F7 returns at 240.690 seconds with 90,810 unresolved alternatives. It checks 4,616 carriers, finds positive values at 4,615, and spends 1.654 ms on these lookups. It builds 33,032 strict kernels and 9,694 beyond-table rows, but no new controller. Its 754,247,279-byte native peak remains under 1 GiB. F8 returns at 184.099 seconds after the partition-memory stop, with 191,505 strengthened local obligations and 205,396 unresolved alternatives. Neither meets the declared material-improvement or new-exact-closure gate. `[R28]`

The final ordinary short four/five-goal policies and the existing exact three-suffix anchor retain their previous bounds and graph bytes; rebuilt WASM four/five checks also preserve the results. The final three native populations are all Conquest. No new cross-base outcomes or fresh partial-three-to-five qualification are established by this commit. `[R28, R33]`

## Corrections to the earlier diagnosis

**Superseded:** “strict proof may not get a useful window” is no longer an unanswered question for this diagnostic. The ordinary long controls did not get one; the optional handoff does. Do not build another handoff or turn the experimental 60-second request into a universal rule.

**Superseded:** parent 4741 is not the current stopping explanation for this attempt. Complete new-candidate construction passes it. Later frontier completion and partition memory now dominate the measured proof attempt.

**More precise:** “the lower is not consumed” is false for F7. It is consumed, but mainly supplies a uniform source-state floor. For a complete cell C and legal action a, the added floor is

```text
B(C,a) = max(price_lower(a), min(h(s) for s in C)).
```

Under the existing domain proof this is sound: h(s) <= V*(s) <= Q*(s,a), and nonnegative continuation also justifies the price floor. It does not compute c(s,a) + E[h(s')], and it does not discriminate every expensive alternative. Do not replace max with price + h(s): that double-counts rather than establishing a new action bound. Zero retirements here do not prove that all stronger admissible heuristics would be ineffective. `[R27, R32]`

**New capability warning:** ordinary four-goal quality worsened with the longer run, but the report does not establish that an already verified 5,218 policy was discarded inside F1. That cheaper artifact came from a different short run; F1's first verified trajectory upper appeared during final evaluation at 241.725 s. Investigate candidate readiness/verification and high-water preservation without alleging an unsupported within-run monotonicity bug. `[R28]`

**New qualification caution:** F6 and F5 took different wall-triggered candidates (4,667 versus 4,539 expanded states). Passing a later point does not isolate physical-parent registration. F6 and F7 did capture the same reported candidate identity, making their consumer comparison more informative. Pair target/budget/clock identities; require candidate/row/generation matches only when the experiment claims to hold the candidate fixed. `[R28]`

## Changes to the programme

Use current ordinary execution, with the diagnostic handoff disabled, for the cross-base baseline. Keep optional handoff probes separately identified and only run them on evidence-selected families. Import compatible F0–F14 receipts rather than rerunning the whole campaign; do not overwrite archived F* evidence or execute a historical preparation script into its old output directory.

Retain 240/300/315 seconds and 1 GiB as the allowed development ceiling, not mandatory time spent per case. Stop on a real earlier cap. Keep a few actual short-finish controls, since a long-run 60-second sample cannot substitute for a separate short finish.

Select one of two measured implementation branches after breadth testing: **frontier-completion cost/reuse** or **partition-memory/rebuild cost**. Trace a specific obligation from demand through all successors to a completed row/proof use, including source/target generations and retained/scratch memory. Distinguish necessary new work from repeated work before building a cache or a new basin. Original open action-family coverage remains a separate closure obligation; never flip its flag to make an admitted-vocabulary proof look global.

The 12-case selection remains unchanged. Update CB01/CB02 with the new retained proof observations; preserve CB03 as an unresolved historical regression candidate and all non-Conquest outcomes as unmeasured here. The broad success criterion and two non-Conquest family requirement remain.

## Sources and limitations

References resolve in `source_register.json`. The update inspected the commit diff, full new living-record section, relevant current contracts, current HANDOFF, and comparison input fields. It did not independently execute the raw evidence archive or rerun native/WASM tests. Previous literature and historical source pins are preserved; no new external literature claim is needed for this delta.
