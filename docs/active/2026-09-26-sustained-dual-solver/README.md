# Sustained dual-solver programme

Oliver selected execution of the attached P0–P9 plan. The ZIP is retained
verbatim in `research-inputs/sustained-184b934/` (SHA-256
`55289fdb04928e3edd3af981c044f9c6b30ff1316554464ae1d4189149e316c7`);
its 22 declared payloads, sizes and hashes were verified before import. The
packet is research input. Native source, applicable repository rules and
Oliver's request retain authority.

## P0 baseline and expectation identity

Started at clean `main` `184b934396e9aba7d009f8dcbad3f488ee71dbfd`,
matching the packet's reviewed ref. Protected root `0` was not inspected.
The previous [dual-lane record](../2026-09-26-dual-lane-target-support/README.md)
owns original U evidence. The saved U4 ledgers under
`out/dual-lane-target-support/U4/` use compiled manifest SHA-256
`852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`,
role corpus SHA-256
`404c0ee632b199b85a5fce76a62f9d75ca59839849b2b9efd1bcf4c174125b20`,
and benchmark executable SHA-256
`99e03218c6274413374bdd9c21f79237cf88e27f3a2c17bbb26147218956a121`.
The A4/A5 roots are empty Rare Conquest Lamellar level 86, with pinned Allflame
prices, goal-relevant product scope, 240-second Finish, 300-second native
watchdog, 315-second host cleanup and 1-GiB solver cap. The runs were serial.

| Saved lane and target | A5 checked upper | Actual status | Expectation and process |
|---|---:|---|---|
| Current ordinary clean L | C85,558.7061856044 | `refused_unsupported_action` reliability class with bounded policy and matched exact graph evaluation | met, exit 0 |
| Finder conditional-retention L | C524,079.6986482172 | `bounded_feasible`, matched exact graph evaluation | unmet, exit 2 |
| Finder conditional-retention R | C133,226.02816593435 | `bounded_feasible`, matched exact graph evaluation | unmet, exit 2 |

Finder's saved failure is an expectation-schema mismatch: the case's Current
contract requires Current telemetry sections, while Finder returns version-1
Finder telemetry. No report errors or cap-check failures were present. The
new corpus copies the old cases, leaves their Current expectations intact, and
adds two A5 Finder cases with explicit solver-mode and goal-terminal
expectations. Its manifest SHA-256 before further edits is
`eb450af001c2cd732293eafa5aa68e2ff92056a21fcdd8dcb500647fd81fa0674`.
The benchmark's new Finder branch still requires a finished solve, bounded
policy status, a compiled graph and matched exact evaluation when a policy is
returned; an absent policy is separately classified. Native `--validate-only`
accepted all eight new corpus specifications. A source-matched Finder A5 L run
returned exit 0 with expectation met, bounded policy C524,079.6986482172,
matched exact evaluation and no report errors. It used benchmark executable
SHA-256
`3df3ce72e001c563c834c2bd909b701648705d737fe5d90f7f159d83150fabf4`
and the new corpus manifest hash above; its
[ledger](../../../out/sustained-dual-solver/P0/finder-L-expectation/ledger.json)
retains the exact command and graph. Host wall was 40.53 seconds; this single
case is a contract check, not a timing comparison.

The separate A5 Current reliability class comes from telemetry
`actions.unsupported_observed=1` with sample `chaos`; policy compatibility
reports `primitive_renewal_expected_actions_exceed_simulator_cap` at state 0.
The independent checked graph still exists, while the full requested action
envelope has that unsupported observation. The source classifier checks this
before policy status and emits `refused_unsupported_action`; it must not be
silenced merely because an upper is available.

The older A3 Current F4 report at
`out/strategy-finder/F4/current/cases/rp-a3-product8-long240.json` has a
bounded checked policy and `termination=refused_resource_cap` but an empty
diagnostic `cap_hits` array and `cap_hit_mask=0`. The C API derives the mask
from `diagnostics.cap_hits`, then maps any remaining `RefusedResourceCap`
termination to `other_resource_cap`; this explains the apparent mask/cause
disagreement. Final publication retains a coarse stop through
`successful_refined_publication_termination`. The saved report does not expose
which earlier coarse branch chose that stop, so a root-cause repair is not
claimed. Its separate bounded-best-policy contract failed in that saved run.

At the first read, the hosted Windows run for `184b934` was still
`in_progress`; the saved review's older build success is not a final test
result. The GitHub run is
[36282378378](https://github.com/OliverOrton/poecraft2/actions/runs/36282378378).

## P1/P2 target-neutral Current

The internal immutable `GoalProofProfile` has default `ordinary_clean` and
private `target_neutral_zero` capabilities. The latter permits the global
zero lower only under checked finite nonnegative prices; positive clean
patterns, retirement and global exact closure are unavailable. The admission
rule is enforced by synchronous and stepped C API paths and direct `SolveWork`.
The benchmark, isolated worker and corpus runner carry a typed diagnostic flag
with resume identity. Publication requires an independently checked artifact;
selected-policy strict lift remains available for an upper. Neutral results
report `closure_unavailable_by_profile`, lower zero, bounded status and no
exactness. Default ordinary clean remains the original path.

Focused native solve tests poison old positive evidence at the incremental,
focused and final-publication consumers and verify that neutral output remains
zero and non-exact. A simultaneous-handle API test verifies ordinary R refusal
and neutral R synchronous/stepped admission. Native solve suite passed 87,672
checks after the selected-lift change; API suite passed 3,015 checks before that
last internal change. Python proof-flag/resume test passed. The final selected
graph can be improper under exact mechanics: an A4 neutral L coarse direct
check had success probability 0.028958 and off-policy mass 0.971042. Its strict
selected lift failed `invalid_policy_transition`, so a checked fallback was
retained. This is an upper-quality boundary, not a proof promotion.

## P3 semantic contrast

The original A4/A5 roots, prices, action scope, 240-second Finish and 1-GiB
cap were preserved. Exact graph evaluation matched every listed checked upper.
Original reports are under `out/sustained-dual-solver/P3/`. This is a semantic
contrast across L/R, not a same-problem treatment comparison for those targets.

| Case/profile | Checked upper (Chaos) | Lower | Result |
|---|---:|---:|---|
| A5 ordinary L | 85,558.7062 | 36.4885 | bounded, requested Finish |
| A5 neutral L | 85,558.7062 | 0 | bounded, requested Finish |
| A5 neutral R | 33,109.8416 | 0 | bounded, requested Finish |
| A4 ordinary L | 3,746.1319 | 21.7725 | bounded, requested Finish |
| A4 neutral L | 15,772,148.6906 | 0 | bounded, discovery complete after checked fallback |
| A4 neutral R | 588,884.9957 | 0 | bounded, requested Finish after checked fallback |

The A5 three-arm runs used one executable freeze. A4 neutral L/R were repaired
after the initial A4 arm; their source identity differs from A4 ordinary L and
the A5 freeze. They are capability evidence, not clean source-matched timing
effects. The original A4 neutral attempts without selected lift returned no
policy; those reports remain under P3. The repaired A4 reports returned checked
graphs but their outer expectation exited 1 because the benchmark had skipped
initializing the coverage bounded-result contract and had not treated declared
closure unavailability as an open obligation. That reporting path was corrected
after the reports and built; the preserved reports retain their original exit.
No replay is silently relabelled as a pass.

## P4/P5 shared construction and Finder

`SelectiveCompletionProducer` is the native owner for a root Chaos acquisition,
held-side selection, state-local automatic programme admission and a complete
paid control graph. Both the original retained-side control and a reroll/repair
follow-through are retained. The producer proposes controllers; the existing
compiler, exact graph evaluator and `SelectiveProgrammeEntryValidator` admit
them. The latter checks the positive-mass reached item/control entries against
the exact native semantic option and complete proper kernel. Neither producer
nor Finder grants a lower or closure proof.

An Exalt fill and a fill-plus-repair proposal were tested on A5 L. Their root
graphs could evaluate, but reached entry `c10` with prefix/suffix occupancy
3/1 and goal mask 15 did not match the selected native programme. Two bounded
guard attempts did not repair that obligation. The rejected fill branches were
removed from the final grammar. Negative receipts remain under `out/sustained-
dual-solver/P5/`; they are not accepted policy evidence. The surviving
reroll/repair controller passed 691/691 reached-entry checks and exact original-
root graph evaluation. Finder creates it as a checked follow-through of the
accepted retained-side controller, with semantic candidate identity rather
than calculator-local numeric IDs.

Source-matched final eight-attempt A5 L reports are under `out/sustained-dual-
solver/P7/finder-control-final-8-L/` and `finder-selective-final-8-L/`.
Both returned expectation-met exit 0 and matched exact graph evaluation. The
control checked upper is C524,079.6986482172 in 23.66 s host wall; the
selective grammar checked C379,815.678156585 in 37.90 s. The latter is 27.53%
cheaper than this Finder control, but 4.44 times the retained Current ordinary
A5 L reference C85,558.7061856044. Each is one timed run, so no robust speed
claim follows. An earlier eight-attempt trial queued two fill candidates and
failed to reach the follow-through; its negative receipts are preserved at
`out/sustained-dual-solver/P7/finder-selective-8-L/`.
Both final Finder arms used executable SHA-256
`aff3ac470fa701637dbe537627ef343c2a475713ae384ff7aa88cc161bd8df44`,
corpus SHA-256 `eb450af001c2cd732293eafa5aa68e2ff92056a21fcd8dcb500647fd81fa0674`,
and compiled manifest SHA-256
`852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`.
The selected strategy files in those ledgers have SHA-256
`88b3d669f1ff52ee3baba998edfb92b779ef0a00360db4b38774a673cf8912ca`
(selective) and
`99df169ae2a5032f692a89ba7e3f7df5d4748e31b64f5b3771ffde7d811ec224`
(control).

## P6 Current service boundary

The private, default-off Current service is wired to the same producer and
entry validator. It uses a separate native calculator, checks the original-root
graph within remaining aggregate memory and cumulative reforge work, and can
offer only a better independently checked root artifact to the existing
incumbent portfolio. It starts only from a pre-Finish checked incumbent. The
public/default Current path and proof capabilities are unchanged.

The A5 and A4 240-second real treatments did **not** reach that entry boundary.
For A5 the first finite incumbent trace sample appeared at 263.70 seconds,
after Finish;
its service telemetry was still `not_requested` because that report predates
the explicit queued/censored status repair. Its ordinary checked C85,558.71
policy and exact matched graph survived. For A4, the first finite incumbent
trace sample appeared at 242.16 seconds; the repaired telemetry reports
`censored_no_pre_finish_incumbent`, zero checks, and a bounded C9,832.96
policy. The source-matched service-off A4 run returned C3,746.13 and first
finite incumbent sample at 181.07 seconds. A per-step portfolio scan in the
service gate was identified as unnecessary pre-incumbent overhead and replaced
with a constant-time availability guard **after** these runs. No post-fix same-case timing or
economic claim is made. No candidate reached Current's checker in the measured
real runs, so the Current service is structurally implemented but not qualified
for activation. P6B's conditional local placement was ineligible: its required
inferior checked root proposal did not exist in Current's run.

## P7 budget and remaining qualification

All 24 authorized timed native case invocations have been used: P0–P3 13,
P5 five, P6/P7 six. Historical and invalidated attempts remain counted. The
24-attempt Finder comparison, source-matched post-gate-fix Current control, and
real Current service economics are unmeasured. No further timed native run is
implied. Native source build, solve suite (87,672 checks), API suite (3,015)
and focused Python treatment tests (3) pass. The proposed mathematics remains
conditional: zero is a global lower only under nonnegative costs; a positive
selected value can support a checked policy upper without being a global MDP
lower. Packet examples do not establish native mechanics.

## P8 release runtime

The release WASM bundle was rebuilt from the final native source with
`scripts/build-wasm.ps1`; `npm test` and `npx tsc --noEmit` passed for the web
layer. The actual Calculator probe uses the existing client, worker and
release WASM through compact transport. Its additional test-only solver-mode
argument selected the existing experimental Finder without changing product
defaults. Supervised receipts and reports are under
`out/sustained-dual-solver/P8/`:
The rebuilt WASM SHA-256 is
`2064beb626322350e07fcca72ad5bca2b231463c5edab530a107144c2e255319`.

| Probe | Outcome | UI-to-usable | Largest native call |
|---|---|---:|---:|
| Current unattended 240-second Finish | checked C85,558.7062, bounded | 240.54 s | 306.74 ms |
| Current early Finish | same checked cost, bounded | 37.70 s | 305.04 ms |
| Finder unattended | checked C320,800,932.13, `finder_complete` | 6.73 s | 461.68 ms |
| Finder early Finish | checked C470,485,195.56, bounded | 0.23 s | 44.21 ms |
| Current setup cancellation | cancelled, no strategy | 0.24 s | 29.32 ms |

The cancellation report records 18.03 ms native release and no surviving host
process. An initial cancellation probe failed with a test-only variable-name
error before the solve; its supervisor receipt is preserved separately, and
the corrected probe passed. These product calls have adaptive work policy and
the existing product Finder grammar; they are lifecycle checks, not native
fixed-eight selective-grammar comparisons. The new private grammar and proof
profile are not exposed in this Calculator transport, so no product
qualification or promotion is claimed for them. Rendered visual review was
not performed.

## P9 disposition

Target-neutral Current L/R support is implemented privately with zero global
lower and no exact closure. Shared selective completion is admitted through
Finder and yields a cheaper A5 Finder controller, still above Current's normal
checked cost. Current's separate default-off service needs a case with a
pre-Finish checked incumbent and a completed root proposal before its adoption
path can be qualified. The next decision is whether to authorize a new matched
run allowance and a reduced fixture for that boundary; this programme does
not activate the service or promote the selective Finder grammar by default.
