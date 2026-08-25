# Targeted Imprint-Scope Follow-Up

**Status: complete; focused acceptance passed with retained WASM performance
qualification (2026-08-25).**

Parent: [Carrier-Aware Proof Bounds And Consumers](../../plan.md)

## Ruling and controlled case

Oliver ruled that Calculator solves and general solver benchmarks exclude
automatic Imprint programs by default. Imprint remains an explicit opt-in with
dedicated correctness controls.

The retained current Imprint-on report is
[`../gate5-five-t1/report.json`](../gate5-five-t1/report.json). The new
[`five-t1-imprint-off-report.json`](five-t1-imprint-off-report.json) uses the
identical clean five-natural-T1 Conquest Lamellar request and changes only
`consider_imprint_programs` from `true` to `false`. Both runs use the same
compiled data and reach a host-requested bounded finish. The new report exact-
evaluates its compiled policy; this comparison does not request another
Simulator verification because the compiled graph and exact result are
unchanged from the retained control.

## Current on/off comparison

| Measurement | Imprint on | Imprint off |
| --- | ---: | ---: |
| Certified lower | `36.4286171890906` | `36.4286171890906` |
| Exact-evaluated upper | `14454067.4260706` | `14454067.4260706` |
| Compiled graph | 514 / 1,788 | 514 / 1,788 |
| Solve wall | 68,930.137 ms | 69,436.067 ms |
| States discovered / expanded | 14,145 / 12,847 | 14,215 / 12,847 |
| Rows / transitions | 76,275 / 334,539 | 77,373 / 343,180 |
| Incremental carrier admissions | 1,486 | 1,588 |
| High-progress carrier admissions | 207 | 227 |
| Carrier/action admissions | 19,734 | 20,833 |
| High-progress carrier/action admissions | 1,191 | 1,254 |
| Admitted / unevaluated / unresolved obligations | 333 / 204,019 / 19,402 | 333 / 203,925 / 20,499 |
| Remaining obligations | 223,421 | 224,424 |
| Imprint programs evaluated | 256 | 0 |
| Program action-state evaluations | 31,182 | 0 |
| Program outcomes merged | 633,968 | 0 |
| Automatic reforge logical work | 269,094,128 | 288,353,406 |
| Ordinary report reforge work | 709,734 | 709,734 |
| Native live / peak owned bytes | 160,008,683 / 313,306,064 | 163,170,392 / 317,010,144 |
| Maximum solve step | 4,302.720 ms | 4,313.917 ms |
| Direct certification | 7,387.048 ms | 7,482.573 ms |
| Exact graph evaluation | 6,563.163 ms | 6,656.947 ms |
| Strategy compilation | 61.741 ms | 52.872 ms |
| Publication / stop owner | bounded core / requested finish | bounded core / requested finish |

The upper trajectories are numerically identical. Each drops from the
470.485-million renewal through `10067712.858417207`,
`2044081.9418072316`, `1777917.7732045916`, and the same intermediate
incumbents to `1683720.400801585`; finalization then tests
`16914883.725546554` and independently publishes
`14454067.42607058`. Timings differ by ordinary run noise only.

Terminal discovery is also identical. Satisfied mask 31 has 58 discovered
carriers, 27 expanded carriers, 31 frontier carriers, 29 exact goal states,
and six policy-reachable carriers in both reports. The ordinary state counter
continues to report zero because it has different ownership and is not the
carrier goal census.

Disabling Imprint removes all program evaluation and merged program-outcome
work. It does not simply subtract total work: the cooperative scheduler uses
the released window to admit 102 more carriers, 1,099 more carrier/actions,
and 1,097 more unresolved obligations. That accounts for the slightly larger
row, transition, and memory counts. Both finalizations publish the same
proper direct core, record the same solver/exact cost mismatch on the coarse
candidate, and refuse strict lift for the same reason: `proof envelope lost
its selected quotient state`.

## Historical 87k versus current 14.45M audit

The historical
[`zero-to-five-native-60s-verified.json`](../../../2026-08-22-exact-goal-carrier-ladder/zero-to-five-native-60s-verified.json)
and the retained current Imprint-on report have the same request after
excluding report metadata. Both enable Imprint, and neither compiled policy
selects an Imprint operation. The historical solve evaluated zero Imprint
programs; the current run evaluates 256 but does not select one. Imprint scope
therefore cannot explain the policy-quality change.

The source boundary is commit `b6fb861` (`Recover multi-epoch exact carrier
strategies`). A detached build at that commit, run with this follow-up's
Imprint-off case, reproduced the current `14454067.4260706` upper, 12,847
expanded states, and 514 / 1,788 graph. The detached worktree was clean and
removed after the audit; no source from it entered this tree.

Before `b6fb861`, the historical solve allocates its bounded window across 35
smaller carrier epochs, expands 5,722 states, admits 5,408 exact actions, and
publishes the independently evaluated `87361.1690420501` policy in a 607 /
1,460 graph. At and after `b6fb861`, frozen automatic epochs run to completion,
new exceptional support and missing certified-frontier carriers receive
urgent work, pending-value rows force refinement, and publication retains a
proper fully materialized joint policy instead of preferring an unverified
cheaper coarse estimate. The current run consequently spends the window on
three larger epochs, expands all 12,847 states, admits 333 exact actions, and
publishes the different proper direct core.

This is a time-bounded scheduling/publication change, so incumbent quality is
not monotonic across the source boundary. The historical 87k graph remains
valid independently evaluated policy evidence; it is not a current-source
deterministic policy-quality baseline.

## Gate 3 and Gate 4 disposition

The no-Imprint result does not change any Gate 3 proof result or Gate 4's
failure mechanism. The global lower remains `36.4286171890906`; cheap
unresolved obligations still own the descriptor minimum; remaining
obligations increase rather than close. Gate 3 was not repeated and Gate 4
was not reconstructed.

## Scope implementation

- Calculator state and `calculatorSolveOptions` now default
  `consider_imprint_programs` to `false`; selecting the existing control opts
  in.
- Native and release-WASM general benchmark runners resolve an omitted corpus
  field to `false` and continue to honor explicit `true`.
- Dedicated native Solve/compiler Imprint controls request `true` explicitly.
- Telemetry and compiled-strategy provenance continue to disclose the
  selected scope. The low-level engine's omitted-option default remains
  historically enabled for compatibility.

## Acceptance

- Native build: pass.
- Solver Solve: 103,221 checks, zero failures.
- Solver compiler: 583 checks, zero failures, including its 10,000-run
  compiled controls.
- Dedicated Solver Imprint: 60 checks, zero failures. Its fixture opts in
  explicitly and retains balanced create/restore execution.
- Release WASM rebuild: pass. The rebuilt module is byte-for-byte unchanged,
  as expected for a caller-default change with no engine ABI/source change.
- Native/release-WASM `oracle-real-one-mod`: both disclose
  `imprint_programs_considered: false`, converge exactly at
  `23.7899999999997`, compile 6 nodes / 7 edges, and produce the identical
  seeded 10,000-run result: 10,000 successes, zero failures or off-policy
  runs, mean cost `24.0086099999999`. A separate native exact evaluation
  matched the same cost with success probability 1 and zero off-policy mass.
- The generic comparison remains nonzero for the pre-existing inactive exact-
  evaluation representation difference (native object versus WASM `null`)
  and the release-WASM maximum worker slice of `186.781` ms against the 50 ms
  corpus cap. Gate 5 measured `166.233` ms on the same case. The cap was not
  weakened; all functional policy, value, structural, telemetry, and seeded-
  simulation comparisons pass.
- Complete nonvisual web suite: pass, including 28 / 28 release-WASM smoke
  checks and the new general-benchmark default/explicit-opt-in control.
- `npx tsc --noEmit`: pass.
- `git diff --check`: pass.
- Full repository pipeline: deliberately not run.
- Rendered/visual review: deliberately not run; it remains Oliver's.

The follow-up is complete. It changes only caller defaults and explicit test
scope; Gate 3 proof work and the stopped Gate 4 descriptor experiment remain
untouched.
