# Four-T1 Automatic-Envelope Replacement Qualification Result

**Status: stopped precisely on 2026-08-21.**

Owner: Oliver

Starting commit: `8a4f3533a24a287e0ce62775de8d2bb3ec1f5beb`

Plan: [Four-T1 Automatic-Envelope Replacement Qualification](plan.md)

## Result

The repaired four-natural-T1 Calculator case still does not close within its
checked 300-second native boundary. It therefore does not replace the retired
`3745.7309340083884` optimality authority, and no new exact value is claimed.

This qualification did make the finalizer cooperative, repaired a real paired-
graph proof defect, and avoids independently evaluating a byte-identical exact
certificate on every competitive-frontier pass. Those changes are retained.
They turn the former uninterruptible finalizer into a truthful watchdog stop
and remove repeated certification work, but the remaining exact alternative
closure still exceeds the case boundary.

## Reproduced Owners And Repairs

The starting executable finished its ordinary solve in about eleven seconds,
then blocked inside the selected-policy strict fallback for more than seven
minutes. A stack sample localized the block to synchronous
`lift_policy_quotient()` inside the cooperative finalizer. The fallback now
uses `PolicyExactLiftWork`, advances one work item at a time, publishes its
actual refine/compile/certify phase, and yields through the existing solve
checkpoint.

The first strict pass then exposed two related assertion issues:

- the compiler reported 156 designated bounded default edges, but the pairing
  normalizer recognized only the 155 `policy_route_*` defaults and omitted the
  one `refined_parent_*` default; and
- strict-lift completion checked executable/proper/cost flags without also
  requiring a complete paired assertion, so a post-evaluation pairing failure
  could retain enough semantic flags to be accepted.

The normalizer now recognizes both compiler-designated router forms. Every
strict-lift completion path also requires `Complete` plus
`paired_default_only`. A focused regression asserts this invariant on the
product Fracture lift.

Across competitive-frontier passes, the emitted fail-closed graph remained
byte-identical: 174 nodes, 515 edges, and 546,071 JSON bytes. A completed
proper, cost-complete, zero-off-policy, paired assertion may now supply its
exact evaluation to the next byte-identical certificate. The current solver
cost is always restored and reconciliation is recomputed; a different graph,
an unpaired result, or any failed semantic prerequisite is not reusable. The
retained assertion is debited from the owned-memory envelope.

## Checked Native Stop

The final native run is
[native-qualified-unverified.json](evidence/native-qualified-unverified.json).
It stopped as follows:

| Measurement | Result |
| --- | ---: |
| Solve wall time | 300,510.14 ms |
| Total case wall time | 300,546.48 ms |
| Stop | `watchdog_expired` |
| Largest cooperative step | 54,203.23 ms, local reoptimization |
| Expanded / discovered states | 1,207 / 2,907 |
| Sparse rows / transitions | 11,739 / 23,629 |
| Finite executable incumbent bound | 3,759.5969190423507 Chaos |
| Global lower bound | unavailable |
| Solver-owned estimate at stop | 95,896,907 bytes |

No checked state, row, transition, reforge-work, or owned-memory cap fired.
The automatic preparation phase completed without a deferred candidate: 482
carriers produced 890 automatic operators; 10,162 candidate rows were
considered, 4,901 were eligible, 5,261 were rejected, six collapsed, and 49
were selected by the coarse pass. The largest eligible families were 2,573
temporary-bench candidates, 1,921 Eldritch-side candidates, 291 primitive
Fracture candidates, and 116 protected-side candidates. This is not the old
automatic-admission omission.

The high-impact incremental envelope remained explicitly open at the stop:
18,414 actions remained, including 13,842 unevaluated and 4,572 unresolved.
The result is therefore fail-closed even independently of elapsed time.

The exact lift's first pass materialized 5,820 strict states, 5,796 kernels,
30,566 transitions, and 5,820 partition classes over nine rounds. Its compiled
graph evaluation discovered 5,653 pairs. Subsequent frontier passes grew to
5,924 states / 5,890 kernels / 30,660 transitions and then 5,961 / 5,927 /
30,697. Their byte-identical graph evaluations reused the completed proof and
reported zero certification pairs. The remaining time is repeated strict
carrier discovery and local alternative reoptimization, not strategy
compilation, graph size, exact graph evaluation, automatic preparation, or a
resource cap.

## Owner-Authorized Ten-Minute Diagnostic

Oliver authorized one unchanged native rerun with a longer diagnostic window
to determine whether the checked stop merely needed a little more time. The
diagnostic copied the checked manifest and case into ignored build scratch and
changed only the enclosing maximum wall time and case watchdog from 300 to 600
seconds. The checked corpus was not edited. Its persisted result is
[native-extended-600s.json](evidence/native-extended-600s.json).

The extension also stopped at its watchdog, after 599,992.93 ms of solving and
600,029.78 ms total. It completed 78,737,871 cooperative solve steps; the
largest individual step was 51,936.29 ms in local reoptimization at
finalization work item 90,898. Live progress continued advancing through the
deadline rather than freezing. The later strict dimensions remained fixed at
5,961 states, 5,927 kernels, and 30,697 transitions, and the reused graph
certificate continued to require zero new certification pairs. The product
action envelope was still explicitly open with 18,414 actions remaining:
13,842 unevaluated and 4,572 unresolved. The finite executable upper remained
3,759.5969190423507 Chaos, the global lower remained zero, solver-owned memory
was 102,115,226 bytes, and every checked resource-cap assertion passed.

This rules out only the modest extension: ten minutes is still insufficient.
The observed sweep is advancing, but its remaining denominator is not exposed,
so this result does not establish its natural completion time. A substantially
longer exploratory ceiling could measure that time, but it would not repair the
checked five-minute product boundary. Semantic proof or row reuse remains the
next solver-quality owner.

## Follow-Up Regression Audit And Repair

Oliver asked whether the slowdown could have been caused by the immediately
preceding work because the four-mod case had completed before. History confirms
that the old fast result and the current run did not solve the same decision
problem: the parent at `c955d02` prepared zero automatic carriers. Commit
`8a4f353` deliberately repaired that omission, so the newly expensive work is
the proof of the previously skipped carrier-local automatic action envelope.
The old `3745.7309340083884` result remains an executable upper, not a valid
performance or optimality baseline for the repaired envelope.

The audit nevertheless found and repaired three avoidable amplifiers without
changing mechanics, prices, kernels, or Bellman ordering:

- certifying one already-created strict alternative obligation rebuilt the
  complete descriptor/kernel vocabulary for that carrier and then validated
  the selected operator again; certification now collision-safely validates
  only the named descriptor and carrier-local admission before the existing
  exact candidate check;
- variants in one temporary-bench effect group share the same complete exact
  `OptionKernel`; under a concrete economy, every variant above the minimum
  immediate price is therefore globally dominated for every continuation
  value. The first minimum is retained. An unpriced caller retains all
  variants because it has no ordering authority; and
- local alternative scheduling inherited the last graph-assertion phase and
  performed long non-yielding sweeps. It now reports
  `LocalReoptimization`, clears completed graph-evaluation progress, and
  checkpoints every 32 pending obligations.

The checked repaired native result is
[native-repaired-300s.json](evidence/native-repaired-300s.json):

| Measurement | Before | Repaired |
| --- | ---: | ---: |
| Solve wall time | 300,510.14 ms | 300,149.74 ms |
| Largest public solve step | 54,203.23 ms | 1,445.44 ms |
| Automatic operators | 890 | 135 |
| Automatic rows collapsed | 6 | 2,149 |
| Temporary-bench rows | 2,552 | 426 |
| Maximum temporary rows per carrier | 311 | 45 |
| Coarse sparse rows | 11,739 | 9,613 |
| Open automatic actions | 18,414 | 16,277 |
| Unresolved automatic actions | 4,572 | 2,452 |
| Finite upper bound | 3,759.5969190423507 | 3,759.5969190423507 |

The strict frontier is unchanged at 5,961 states / 5,927 kernels / 30,697
transitions, which is positive behavior-preservation evidence. The checked run
still stops before exact closure, so the reductions do not satisfy the primary.

The owner-authorized ten-minute diagnostic was repeated once after the repair;
its result is
[native-repaired-600s.json](evidence/native-repaired-600s.json). It also stops:
600,120.43 ms solving / 600,153.07 ms total, 48,847,540 cooperative solve
steps, a 1,449.75 ms largest public step, the same finite upper, and no lower
certificate. The strict dimensions stay fixed and graph-certification pairs
stay zero while local reoptimization advances. The coarse envelope remains
open at 16,277 actions (13,825 unevaluated, 2,452 unresolved). No checked cap
fires. The action-vocabulary reduction and responsiveness repair are real, but
the dominant exact alternative program still requires semantic row/proof reuse
across equivalent carrier/operator work rather than another modest timeout.

The follow-up native build passes. The policy-refinement control passes 962
checks and the S8.3 automatic-admission control passes 534 checks, including
the pinned 10,000-run Veiled simulation. `git diff --check` passes. Release
WASM, the four-T1 10,000-run verification, web acceptance, and the full
repository pipeline were not run because the repaired native primary again
hit the explicit stop condition before publication.

## Numerical Upper-Pass Lifecycle Follow-Up

A subsequent audit rejected publication batching as the owner: full batching,
64-row batching, Q-triggered batching, an internal 16-row Bellman batch, and
publication-only deferral all made the checked path worse. Their source
changes were removed.

The audit found one real defect caused by the now-complete action envelope.
When a temporary focused upper-policy experiment latched numerical stability,
`run_focused_lower_unit()` terminated the complete solve rather than rejecting
that experiment. The retained repair calls the existing failed-pass finalizer,
which restores the lower snapshot and temporary rows, records the rejection,
and resumes the incremental scheduler. A focused regression covers the
rollback and pass-ledger transition.

The final-source run stopped at 300,868.57 ms of solve time / 300,905.34 ms
total. Its largest public step was 1,407.25 ms. All 717 requested upper passes
started; 439 were proper and 278 were rejected with the final numerical-
ordering reason. It completed strict frontiers through 5,961 states / 5,927
kernels / 30,694 transitions. The raw finite upper remained
`3759.5969190423507`; no lower certificate was published. The envelope stayed
open with 13,379 unevaluated and 4,205 unresolved actions.

One magic, one-prefix/one-suffix Imprint carrier now defers at the generic
256-program work allowance after 182,778 action-state evaluations and
2,570,418 merged outcomes. Flat 512/1,024 cap probes and scalar/price-order
word-enumeration changes failed to close it and were removed. The sound next
architecture is unique exact-kernel graph search rather than a larger flat
word budget. The five-minute stop also shows this is not by itself sufficient:
strict semantic row/proof reuse remains independently necessary.

The final native build, S8.3 automatic-admission control (534 checks, including
its pinned 10,000-run Veiled simulation), and focused solver controls (3,188
checks) pass. The aggregate automatic-Eldritch executable still reports 13
pre-existing resource-stop test failures; an exact source A/B against accepted
HEAD reproduces the same 13 failures with this lifecycle patch absent. The four
new lifecycle assertions add no failure. Release WASM, primary verification,
and the full pipeline were not run because the primary stopped.

## Earlier Acceptance

The following passed:

- native build;
- focused solver suite: 96,140 checks;
- focused refinement suite: 362 checks;
- focused compiler suite: 834 checks;
- focused evaluator suite: 18,065 checks;
- release-WASM rebuild;
- release-WASM engine smoke: 28/28 checks;
- complete nonvisual web tests;
- `npx tsc --noEmit`; and
- `git diff --check`.

The release-WASM four-T1 primary, pinned 10,000-run verification, and full
repository pipeline were not run because the native primary hit the plan's
explicit stop condition before publication. No browser visual review was
performed. Nothing was pushed.

## Precise Next Owner

The next solver-quality scope is semantic reuse in strict automatic-
alternative closure. It must measure obligations by automatic kind, effect or
template identity, source coverage, frontier discoveries, and elapsed work;
then prove and reuse one exact alternative row across equivalent
carrier/operator classes through existing collision-safe identities. The
target is the repeated roughly 5,900-carrier frontier pass, especially
temporary-bench and Eldritch-side alternatives—not compiler compaction, action-
family broadening, weakened exactness, a larger cap, or another repeated graph
evaluation.

Five-T1 recovery remains separate. Oliver must select the next implementation
chunk before work resumes.
