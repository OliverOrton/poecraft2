# Current incumbent continuity (IC0–IC4)

**Disposition: stopped at IC2; trial repair not retained.** Oliver selected
execution of the [original IC plan](research-inputs/poecraft2_current_incumbent_continuity_plan.md).
A native witness located deferred checking; a trial checkpoint exposed a checked
incumbent before Finish. The real selective candidate then failed its independent
check. A separate dynamic memory-cap control also failed publication. The plan's
native-check stop condition applies. These results do not qualify service
activation, cheaper-candidate adoption, or a timed comparison.

## Source and scope

Local start was `3cb3ab7a6bcbf2a0a242523a95560ce76cdd286b`; available remote main
was `ae24e0bb8797fb953cb8b9eb5c8ef040fbba203c`. Their engine source is identical;
the intervening W commit is documentation only. Relevant worktree paths were
clean before the experiment. W was not repeated. Work was sequential, without
subagents, push, canonical-data edits, or access to protected root `0`.

The plan was preserved unchanged (SHA256
`13cb1d913b1e4439677c07e25d9a97a16d525c6ca35b2260d556b3471324178a`).
The experiment reused the native test executable, `SolveWorkTestAccess`, an
eight-mod synthetic session, actual native rows, compiler, independent evaluator,
and portfolio. No certificate flags, saved production graph, or invented values
were injected to establish initial availability. This synthetic request is not
A4 or A5 and does not establish their timing or policy quality.

## IC0: first missing transition

The four-goal fixture requests Rare families 100/102/103/104 from an empty Rare
root, Chaos/Annul/Exalt at 100/5/2, gated reforges and high-impact uppers, with
economic restart and state certificates disabled. Service-off and service-on
use separate calculator contexts and `step(1)` boundaries.

On the original engine, a native `gated_primitive_destructive_renewal` candidate
is captured at zero-based step 1005 / one completed row. Its early checker is
not scheduled. Final publication starts at step 1493 / 102 rows; a checked
direct core artifact appears later with cost 219.36322337787271. No pre-Finish
service admission occurs. Readiness/admission expectations fail: 19,717 checks,
two failures. See the [baseline trace](evidence/IC0-witness-failure.json).

That trace also records an actual solver-estimate/compiled-cost mismatch before
the direct compiled artifact is retained. A mismatched source estimate does not
erase a separately accepted bounded upper or establish exactness of the source
estimate. The one-goal control has no early owned candidate and completes
normally at 262 steps.

The source explains two obstacles: early renewal checking is private-dirty-search
gated, and its handoff is reached through an open incremental-envelope
continuation. Removing only the activation guard still failed. Enabling automatic
candidates did not reach the missing boundary in this small case either
([20,762 checks, two failures](evidence/IC1-auto.json)).

## IC1: unretained trial

The trial allowed the first complete renewal to enter the existing cooperative
checker between completed rows, saved its discovery phase, and resumed that
cursor afterward. It applied equally to service-off and service-on. Later
private improvement retained its activation. It added no grammar, lower producer,
public option, or certification shortcut.

The [trial lifecycle](evidence/IC1-boundary.json) passed 15,994 checks. For the
automatic four-goal fixture, capture was at step 1006 / one row; checking was
queued at step 1009 / two rows; the independent check and retained bundle were
observed at step 1011; service admission followed at step 1023. The early checked
renewal cost was **63,969.230769229565**, substantially worse than the later
219.36322337787271 core policy. This demonstrates earlier availability of that
specific controller, not earlier construction of the strong final policy or an
economic improvement. The existing return-bridge continuation runs before the
initial-candidate task yields back to service admission.

The trial is preserved as a [reviewable patch](evidence/ic1-unretained-trial.patch),
with exact source-file and executable hashes in the [manifest](evidence/trial-manifest.json).
It is **not applied** to the working engine or tests. The frozen local executable
is `out/current-incumbent-continuity/ic1-unretained-engine-tests.exe`; its SHA256 is
`564b5f3e95a8f18c4fe2863caa426eab1e4ecce416a551bc7530e0d141bc9cdd`.
Earlier development traces retain original-log hashes; this executable hash
identifies only the final trial, not those earlier builds.

## IC2: gate failures and independent controls

The native service fixture enables the existing synthetic Eldritch session and
automatic candidates. It supplies Eldritch Chaos/Annul and all four Ember/Ichor
tier prices. Separate variants use programme prices 0.01 and 10,000; ordinary
prices, limits, root and goal stay fixed. An initial fixture omitted the
`eldritch_` price-key prefix and correctly refused native admission; that fixture
error was corrected before the native-check counterexample below.

Both correctly priced variants naturally generate a graph and reach its
independent checker. Both are refused as **not converged**, with bounded trace
values success 0.008473, unresolved/residual 0.991527, failure 0.000000. These
rounded observations do not identify the cause of non-convergence or establish
that the controller is semantically invalid. Limits were not widened. Neither
variant reaches programme-entry validation or portfolio adoption. The original
expected-validation assertions failed twice in the
[qualification trace](evidence/IC2-refusal-witness.json).

The generated [low-price graph](evidence/native-proposal-fixture-2.json) and
[high-price graph](evidence/native-proposal-fixture-3.json) are preserved. Their
origin is the native producer, not seeded strategies. Do not treat the low-price
variant as a qualified cheaper candidate or the high-price variant as a valid
expensive rejection: both failed earlier at graph checking.

The subsequent [lifecycle controls](evidence/IC2-lifecycle-controls.json) explicitly
retain that refusal as a negative and test independent cleanup/publication
obligations. They finish with **30,763 checks, three failures**:

| Control | Observed result |
|---|---|
| Service-off versus waiting | Equal phase, values, policy rows, graph identity, successor/probability payloads, queue/cursor, certified lower, verified portfolio identity and logical reforge work in the tested prefix; no private service owners constructed |
| Repeated passive progress reads | No graph-identity, logical-work or event-sequence change in the tested projection |
| Owned incumbent during generation/checking | Same compatible portfolio identity, graph and accepted cost remain selectable |
| Native graph refusal | Scratch released; bounded Finish publishes the checked fallback; no entry-validation/adoption claim |
| Finish during generation/checking | `censored_finish_or_cap`; scratch released; checked fallback published |
| Cancellation during generation/checking | Public release plus work destruction; no finalized result; charged logical work unchanged |
| Tightened aggregate byte cap during checking | Service censored; checked fallback published |
| Tightened aggregate byte cap during generation | Service censored, but final result has no available finite-cost policy: three publication assertions fail |

The cap controls deliberately set the active allowance to one byte below measured
live ownership after admission. That is fault injection under a **changed limit**,
not evidence of a fixed-limit production regression. The remaining publication
headroom and whether the old bundle fits this reduced contract are unresolved.
No budget was increased to make the assertion pass.

The trace is a bounded native event stream with caller-step snapshots; snapshot
fields must not be backdated to individual sub-events. Reforge work is one
component, not complete whole-solver effort. These fixtures do not close the
full IC2 semantic projection, all staged/transfer/invalidation obligations,
validation-phase interruptions, duplicate or valid-expensive disposition, or
genuinely cheaper-native-candidate replacement. Ordinary exact and neutral-zero
regressions were not used to qualify the rejected trial. No Finder quality claim
or changed shared producer was retained.

## IC3 allowance and next decision

**Zero new timed native cases launched. Zero of the proposed four invocations
is authorized.** P remains 24/24 spent. IC2 did not pass, so no cohort freeze,
A4/A5 replay, reverse-order pair, Simulator, Calculator campaign, or product/WASM
activation was attempted. Existing W/P qualification gaps and the U/P retention
activation mismatch remain unchanged; no historical lower was transplanted.

The recommended next decision is a separately selected, bounded diagnosis of
the generated graph's non-convergence under the unchanged native evaluator
contract. The cap counterexample separately needs an explicit publication
headroom/ownership check before drawing a fixed-limit conclusion. Do not resume
timed qualification or integrate the scheduling patch until the relevant gate
is met. This closeout does not authorize either follow-on programme.

## Validation and local reproduction

Build owner: `powershell -File scripts/dev-engine.ps1 -Task Tests -Jobs 4`.
Trial selector: `--solver-integrity-only continuity`. Development outputs and
unabridged logs remain under `out/current-incumbent-continuity/`; committed
evidence files are compact projections with original-log hashes. The frozen
trial executable reproduces the final negative controls and exits nonzero.
No new runner, comparator, polling proxy, or supervisor was added.

All six trial source/test files were restored by reversing the saved patch after
a successful reverse-apply check. Their diff against the starting source is
empty. The normal native test binary was rebuilt from restored source. Focused
baseline checks passed: setup service **694 checks**, bounded Finish **202
checks**, zero failures. These establish restoration, not IC2 qualification.

Knowledge lint against `3cb3ab7` passed with zero errors, 18 existing warnings
and no claim changes. All six Markdown files passed local link checks. Patch,
frozen executable and original-log hashes match; the saved patch passes an
apply check against the restored source. Authored documentation passed whitespace
review. The unchanged input preserves four Markdown hard breaks, and the saved
diff preserves five blank context lines; these are intentional whitespace in
archived inputs, not engine-source defects. No native solver/test process was
running at the final local process check.

No mathematical claim/history or generated research-state file was changed.
The retained deliverable is this bounded negative, original input and
reproducible trial evidence; no production or release artifact change is claimed.
The closeout commit stays local; its identifier is reported in the completion reply.
