# Constructor activation and outer/inner guard audit

Source-only audit of frozen c66c37213a0bb3efc707967cee098487a66fdd76, engine
3a53576b2ba8e5e80928cedb8c7fe742a256ad40. The CI-qualified Tests binary is
771ea27508440f148c3d509e15a576edcf3e5cbb3812e4f211f9774cb021d81f,
20,596,061 bytes. Its sole selector stops in 406.246 ms, 0/3 cases complete,
at "focused root-only seed still refuses". Independent baseline certification
and the cost-13 assertion precede that failure and pass by execution order;
the exact scalar is not printed. No treatment or negative case is reached.
LOCAL releases at 18:37:36 UTC, empty census and clean owned-job drain.

## Actual constructor selection, derived from source

The request supplies Annul, Exalt, Scour and Alchemy, with no fixed options and
automatic_candidates false. The constructor initializes generation from
goal_progress_gated_reforges, then partitions its supported priced static
operators (solver_solve.cpp:468-571). incremental_alternative_type admits only
primitive Essence, Fossil and HarvestReforge (solver_solve_expand.cpp:525-547).
All four requested primitives return false, including Alchemy: being a Reforge
transition is not sufficient for this delayed-family classification. The delayed
set is empty; the constructor resets generation false and closes the envelope.
The manually appended native rows and baseline-check child do not reopen it.

At the failing outer call, the eight predicates at
solver_solve_incremental.cpp:792-799 therefore have these source-derived values:

| Rejecting predicate | Value |
| --- | --- |
| consumed | false |
| finalized_result.has_value() | false |
| !high_impact_executable_uppers | false |
| !incremental_action_generation | **true** |
| incremental_envelope_closed | **true** |
| !incremental_upper_policy_dirty | false; fixture sets dirty true |
| incremental_upper_policy_pass | false |
| !output_incumbent.has_value() | false; checked root-only baseline exists |

The fourth predicate short-circuits the call before it clears the last-failure
string, increments requests or calls begin_focused_upper_solve. The unchanged
counters are 0 requested/0 started/0 rejected, and the last failure remains
empty. These are source-derived values, not captured runtime fields. The native
log records only the conjunction's failure, so it must not be described as a
runtime observation of the named inner seed refusal.

The inner inputs at solver_solve_focused.cpp:806-845 would also refuse a root-only
seed if that owner were reached: no statewise upper authority, no focused fallback,
no strict cache, economic Restart false and restart_cost infinity. Its ordered
four prechecks are missing seed/fallback, active strict cache, Restart disabled
without a statewise seed, and invalid Restart price without a statewise seed.
The first would select seed_root_only_incumbent_without_focused_fallback; the
later fallback-only branch additionally requires a finite frontier upper below
kValueCeiling. None of those inner branches is reached in this fixture call.

The same inactive/closed flags also stop maybe_install_incremental_anytime_incumbent
at solver_solve_incremental.cpp:1121-1127. Simply deleting the focused assertion
would not exercise joint service, and would discard its expected guard coverage.

## Unapplied diagnostics and setup boundary

proposed-outer-inner-diagnostics.patch adds a test-only snapshot of all eight
outer predicates, all four inner precheck inputs, the fallback frontier guard,
selected anchor/delayed IDs, exact counters/reason and the existing progress ring.
It snapshots constructor and both sides of the single outer call, and prints the
checked baseline scalar. It preserves every original expected-result gate and
calls the owner once. It changes neither activation flags, delayed selection,
allowed operations, production source, numerical laws nor checker authority.

Diagnostics alone leave this assertion failing. To test active joint service with
hand-built rows, a future explicit internal unit-test setup would have to represent
an open incremental envelope and be labeled as such. It would prove the service
under those supplied internal preconditions, not activation by this four-action
constructor or normal product scheduling. No such setup is applied or included
in the diagnostic proposal. A naturally activated product case remains a separate,
evidence-gated observation; the original Conquest root slot 2 stays unused.

Prior scope/placeholder failures and the losing P0/Current-tail result remain
intact. This audit performs no build, native rerun, source correction or activation.
