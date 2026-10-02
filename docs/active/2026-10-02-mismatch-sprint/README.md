# Bow cost mismatch sprint — WIP tier handoff

Stopped at Oliver's explicit request to replace this fast turn with standard tier.
Do not treat this checkpoint as reviewed or qualified production authority.

- Worktree: `C:/Users/Oliver/Documents/poecraft2-mismatch-sprint`
- Branch: `dot/mismatch-20261002`
- WIP source commit: `3162ef791873b231c722265c41a650eb0d39e6b3`
- Verified baseline: `f08facbb93204bf721048c21b94080ab582ee9f8`; engine tree equals
  `792b37e8e1d73a90c754087eda2332d7586db570` (direct engine diff empty).
- Main at startup: `ebcd98cdda2c1828cd855f549831951e6ba7470a`.
- Only this sibling was edited. Main/integration checkout, protected root `0`,
  canonical data/runtime/prices, dependencies, hosting and remotes were untouched.
  No extra agent, timed solve, worker or Simulator was launched. Old allowances
  remain historical; any new heavy run requires the parent's serial slot.

## Decisive finite finding

The final worker and earlier native treatment agree on coarse graph hash
`2036cb535bfbfb3b`, transition hash `6513a7a84b7b4f27`, policy hash
`50869f443cc7a277`, 218 selected states, five actions, root Exalt, and coarse cost
32066887.22089472c. Their independently checked graph costs are
28280720.16724688c / 28280720.167247154c. The larger candidate is proper,
completely priced, zero off-policy, unreconciled; its valid root upper survives.
The separately checked 51222.5208199995c policy wins. Lower remains
184.68222144324236c, closure false.

A finite native test now reconstructs the exact frozen 36-action scoped goal
through `pc_solver_create` and the existing `solver_lower_diagnostic_calculator`
seam, without starting Solve. Original root: three held natural T1 prefixes,
Dexterity7 and LocalIncreasedAttackSpeed3, rare ilvl86 Spine Bow.

First selected Exalt row:

| Quantity | Broad parent | Native strict / execution pool |
| --- | ---: | ---: |
| Missing Mana suffix hit | 0.0084745762711864406 | 0.010845986984815618 |
| Total probability | 1 | 1 |
| Projected support classes | 3 | 3 |
| Primitive price | 1.77c | 1.77c |

Projected distribution L1 difference is 0.028456928563549002. The independent
execution weighted-pool oracle has total weight 46100 and matches every projected
strict-row probability. This is an actual first-row transition divergence, not
an immediate price difference. Broad parent exclusion signatures are deliberately
merged (`solver_calc.cpp`, `contract_requires_semantic_carrier_partition`); strict
checking restores them. No native law was changed or approximation added.

This does NOT yet apportion the entire 3786167.0536478423c discrepancy. The saved
1.1175870895385742e-08 residual belongs to the cheaper pre-restore working snapshot
(start 51222.52081999526), not a separately retained residual for the larger
218-state coarse candidate. Do not claim it rules out stale coarse values. The
large emitted graph and full row/value snapshot are absent from the saved worker
JSON; only its checked metadata is retained there. No baseless recomputation was
started. Last unrun test addition prints the broad representative's execution pool
and suffix identities to distinguish its exact exclusion law from the original.

## Proposed safety change, not yet qualified

`BoundedPolicyIncumbent` now carries sticky negative provenance
`statewise_values_rejected`. Every successful compiled assertion path records
failed cost reconciliation; guided strict snapshots also check coarse-value
reconciliation. A later coincident root scalar cannot clear this veto.

Consumers refuse these copied values for automatic-admission upper pruning,
incremental alternative retirement, incremental upper improvement, focused upper
proof initialization, carrier-boundary scalar certificates, and captured joint
continuation boundaries. Independent graph ownership, properness, root cost,
portfolio selection, and independently checked entry certificates stay intact.
Numerical vectors remain available as diagnostic/proposal payloads. This patch
adds no positive authority and does not suppress the valid mismatching policy.

Files: solver_solve_types.hpp, solver_solve_finish.cpp,
solver_solve_return_bridge.cpp, solver_solve_expand.cpp,
solver_solve_incremental.cpp, solver_solve_focused.cpp,
solver_solve_constructive.cpp; finite tests in test_solver_solve.cpp and
first-row witness in test_solver_metamod_recovery.cpp.

Replacement must review the gate for all relevant consumers and resumable/cached
boundary paths before recommending promotion. In particular, this is not a proof
that every pre-existing broad value-table use is natively certified. It is a veto
on reuse after an observed mismatch. No full mathematical repair is claimed.

## Evidence and pending work

- Baseline native test build passed (`build-baseline.log`).
- First finite diagnostic + metamod controls passed: 534 checks, zero failures
  (`row-diagnostic.log`).
- Exact frozen 36-action scope + independent native pool + metamod controls:
  604 checks, zero failures (`full-scope-row.log`). This predates production
  safety edits and the last representative-pool printing addition.
- `git diff --check` passed at checkpoint. An initial Python write introduced
  CRLF whitespace differences; normalized back to original LF before all builds
  and commits. No remaining line-ending churn.
- Safety build was deliberately interrupted for the requested tier handoff,
  before its queued assertion test launched. `build-authority.log` is partial;
  `authority.log` was not produced. No safety test pass is claimed.
- Finite assertion/classification regressions, bounded Finish, selected fallback,
  updated metamod witness, and an incremental integrity check remain pending.
- WASM build/worker replay, full suite, independent large-graph reevaluation and
  fresh economic qualification remain unrun.
- Bulk logs/stop receipt: `out/mismatch-sprint/`. Compact immutable hashes are in
  `checkpoint-evidence.json` beside this record.

Next commands, after replacement reviews the WIP diff:

```powershell
powershell -File scripts/dev-engine.ps1 -Task Tests -Jobs 2
build/engine/poecraft_engine_tests.exe --solver-assertion-service-only
build/engine/poecraft_engine_tests.exe --solver-bounded-finish-only
build/engine/poecraft_engine_tests.exe --solver-selected-fallback-only
build/engine/poecraft_engine_tests.exe --solver-metamod-recovery-only C:/Users/Oliver/Documents/poecraft2/data/compiled/current
```

No owned process remains. The two-job build wrapper PID 50476 and its
identity-verified descendants were stopped to prevent queued test launch;
`stop-receipt.json` records an empty survivor set. Unified exec session 92807
returned exit -1 (deliberate termination). Earlier build/test sessions 85126,
53664 and 36522 returned exit 0. No solver session exists.

Hard times are unchanged: checkpoint by 16:25 UTC, wrap at 16:45 UTC, all owned
processes stopped and handoff ready by 16:55 UTC on 2026-10-02. Never use 17:00
reset or switch/retry on quota exhaustion. Parent owns the tier replacement.
