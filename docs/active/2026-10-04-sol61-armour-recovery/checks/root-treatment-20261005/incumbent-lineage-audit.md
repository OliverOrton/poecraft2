# Matched negative: active incumbent role and retained graph lineage

This source-only audit uses frozen source/checkout
`5a74619339b5283a22074b79026ea3c0177cc0ca`, engine
`056f257007efd976d9fd48420d6d85d9dac797f8`, and the existing qualified
Benchmark `57ae44df` slot-2 result. No code, build or new native run occurs.
Both root slots are spent; LOCAL remains released at 19:25:45 UTC.
Delivery recheck observes remote main `7252027c80856628ed16734583bfc9d6e166458b`;
the isolated tested checkout is unchanged. No fetch, merge or rebase occurs.
[Compact economic result](summary.json) and
[bounded lineage projection](incumbent-lineage-projection.json) retain the
measured fields without copying the bulk result or historical trace.
The final original treatment receipt is preserved losslessly once as
`result.json.gz`; [retention hashes](receipt-retention.json) bind compressed and
original bytes. Raw final/partial outputs remain local; no historical trace is read.

The selected predicate is too narrow for a distinct checked-graph representation:
an ordinary independently checked incumbent can retain its executable graph while
its copied statewise table is rejected. The selected scheduling path accepts only
the `compiled_root_entry_only` construction format. The actual final seed refusal
identifies the ordinary/rejected-table format instead. This supports a minimal
graph-only scheduling alternative, conditional on full current compatibility;
it does not certify any unlogged per-refusal object or establish economic recovery.

## Measured owners and their different authority

| Owner in the existing receipt | Identity | Checked cost | Role |
| --- | --- | --- | --- |
| Preferred `gated_primitive_destructive_renewal` | `12026856962075642739` | 627313592.5067186 | Independently evaluated graph; copied root/statewise estimate does not reconcile |
| Retained `selective_completion_root` | `9082061597894067139` | 831786.9880757828 | Separate root-only controller; not selected at publication |
| Retained/exported `selective_completion_root` | `9233180967849648666` / `8022d92bb793621a` | 101311.35474896732 | Strict cheapest independently evaluated winner; byte-identical control |

The primitive publication sample records absolute reconciliation delta
14.414810538291931 and relative delta 2.297863521410127e-8. Its copied estimate
627313606.9215292 is visible separately in bounded operator-provenance samples;
it must never become an executable statewise continuation or pruning authority.
The candidate-estimate telemetry marks the primitive source verified at exact cost
627313592.5067186, while the independent verified-upper portfolio names the
different 101311 winner. Sparse bound samples first show checked primitive upper
at 132.2647 ms, 831786 root upper at 26034.5542 ms, and 101311 root upper at
39902.3938 ms. These are recorded samples, not precise installation timestamps.

Upper service has 33 requested/0 started/33 rejected; only its **last** refusal
reason is retained. Joint attempts and new joint admissions are zero. The receipt
does not supply all 33 incumbent identities, flags or compatibility results at
those individual call sites. It therefore cannot prove that each refusal saw the
same object or satisfied every additional safe-checkpoint predicate.

## Source reconstruction

1. `solver_solve_constructive.cpp:6271` constructs the primitive through
   `install_output_incumbent`. Its fresh ordinary `BoundedPolicyIncumbent`
   (`:3673`) retains default `compiled_root_entry_only=false` and captures the
   paid renewal policy, value table and native witness. `commit_output_incumbent`
   (`:3483-3557`) assigns the preferred `output_incumbent`; observing a portfolio
   upper does not replace that object with the best retained graph.
2. `solver_solve_finish.cpp:6105-6264` independently checks the initial captured
   graph before ordinary discovery resumes. A valid proper, executable,
   cost-complete, zero-off-policy graph sets the checked cost, compiled artifact
   and independent flags. Root-cost reconciliation is recorded separately at
   `:6251`; failure is a sticky veto on the copied table. Graph validity does
   not require the copied coarse estimate to match.
3. `solver_solve_types.hpp:1273-1287` defines
   `has_statewise_upper_values()` as neither sticky-rejected nor root-only.
   `record_root_cost_reconciliation(true)` cannot clear an earlier rejection.
   Thus the final branch in `solver_solve_focused.cpp:820-826` logically proves
   an existing ordinary incumbent with rejected statewise values and no focused
   fallback. It does **not** itself inspect independent-check flags or graph
   compatibility. `solver_solve_incremental.cpp:825-836` rejects the upper pass
   and restores temporary admission; no statewise values are promoted.
4. The new checkpoint in `solver_solve_incremental.cpp:1095-1124` first requires
   `compiled_root_entry_only=true`, then checks the existing certified-incumbent
   validator. The ordinary rejected-table object is excluded before that latter
   check. `continue_initial_candidate` at `:1672` repeats the format restriction;
   its other branch at `:1691` also refuses any already-present output incumbent.
5. `solver_solve_selective_completion.cpp:342-421` separately creates root-only
   graphs with no parent bindings, exact root certificate, current identity and
   checked flags, then calls `retain_certified_incumbent`, **not**
   `commit_output_incumbent`. A better retained root graph therefore need not
   replace the preferred primitive `output_incumbent` or make its root-only
   predicate true. The runtime preferred primitive and separate publication
   candidates corroborate this reconstructed lineage.
6. `certified_incumbent_invalid_reason` (`solver_solve_constructive.cpp:3205`)
   checks retained payload/context, independent flags, properness, executability
   and exact-cost bounds. Sticky statewise rejection is intentionally not a
   graph-invalid reason. The retained contract (`:3119-3202`) checks goal,
   economy, action prefix, caller scope, artifact, append-only graph prefix and
   generation; root-only objects additionally require explicit placeholder shape
   and exact-entry continuation certificate. Final publication
   (`solver_solve_finish.cpp:2587-2637`) validates/selects the cheapest checked
   retained graph. The primitive publication record is independently evaluated
   but loses; the selective root graph wins and its retained artifact exports.

Accordingly the evidence supports a checked ordinary graph with a rejected
table, rather than a blanket absence of independently checked graphs. Its
compatibility at every historical refusal remains unlogged. The measured
whole-root negative establishes failure to activate this selected treatment,
not the first unique cause of the historical search gap or an introducing commit.

## Smallest supported alternative, not applied

At the two existing checkpoint/queue sites, select a **compatible independently
checked graph whose statewise values are unavailable**. This capability includes
both a root-only controller and an ordinary graph with a sticky-rejected table.
Require the existing certified-incumbent validator before attempting service;
preserve every existing safe-point, geometric cadence, unused one-shot checker,
retention and shared-resource condition. Do not relabel the ordinary object
`compiled_root_entry_only=true`: its parent bindings/vectors violate that format's
own certificate contract.

The existing joint builder already copies certified frontier values/actions only
under `has_statewise_upper_values()` (`solver_solve_constructive.cpp:4836-4848`).
For the rejected-table case that branch stays closed. A proposal must therefore
construct complete paid native selected support; missing positive successors,
scope violations, improper policies and resource excess remain refusals. The old
checked graph is retained only as an executable fallback, never a terminal cost
for arbitrary non-root states. Reusing any graph at a new entry still needs the
separate existing exact-entry certificate; a root check supplies no such blanket
authority. The compiler and independent checker remain unchanged. Candidate
recapture also preserves inherited rejection (`:946-948`).

Any later finite qualification must actually independently check an ordinary
graph with a deliberately non-reconciling copied table, then show unchanged seed
refusal, complete-support service/checking, fallback continuity, and the existing
missing-support/ownership negatives. Unchecked/incompatible graph controls must
remain refused. An improvement relative to 627313592 alone is not the economic
target: only a checked result cheaper than the retained 101311.35474896732 winner
would improve this Conquest outcome. No new root is authorized or available in
the spent allowance; any execution needs separate parent coordination.
