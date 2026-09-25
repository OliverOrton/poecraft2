# S: measure the selector that actually runs

## 1. Exact question

Does adding native acquired-goal credit to Current's **gated first-policy** progress test change the winning complete priced row at an actual invocation? If yes, is its different downstream candidate useful, unavailable or expensive?

This is not “do dirty states exist?” or “can a debt count tie?” The function, its current gate and key are in [R7]. No behaviour change to Current is authorized by this packet.

## 2. Hook and passivity

Add the diagnostic at `SolveWork::Impl::select_joint_policy_seed_row` (or a tightly scoped pure key helper extracted from it). Preserve the existing callback signature and return value when disabled. For enabled comparison, consume the same frozen available rows and source context; do not recursively call the selector, resolve new rows, query a new continuation or change a cache from a status read.

Prefer one traversal of each already-read row that computes the old and alternative event masses alongside each other. Keep production mass accumulation/order and existing choice handling unchanged. If extracting a helper, retain old-path bit/decision parity first. A diagnostic overhead comparison is not an economic treatment.

Record the caller intent if available cheaply: initial candidate construction, selected-prefix resumption, readiness query or other named owner. Do not assume every call is a policy-installation attempt. Some read-like uses can invoke the same selector. The aggregate should cover the entire declared run, with late re-entry counted rather than assuming the first output event permanently closes the gate.

## 3. Fields that establish coverage

Always expose `instrumentation_enabled`, schema/source identity, begin/end logical cursor and `observation_status`. Aggregate:

```
selector_calls_total
calls_outside_state_rows
calls_gate_true
calls_gate_false_with_output_object
calls_gate_false_high_impact_disabled
calls_gate_false_incremental_disabled
calls_with_no_eligible_complete_priced_row
complete_priced_row_comparisons
calls_with_different_progress_mass
calls_with_different_full_key_minimum
witnesses_retained / witnesses_omitted
```

Gate-off reasons can overlap; do not sum them into a partition without declaring first-match logic. Per call record output-object existence separately from independent-verification availability. A candidate object is not a checked graph.

Useful mutually explicit final dispositions:

- `not_instrumented`: no measurement; not a negative.
- `measured_no_selector_calls`: instrument enabled and complete declared run, zero invocations.
- `measured_gate_inactive`: calls occurred but no gate-on call.
- `measured_no_eligible_rows`: gate-on occurred but no eligible complete rows.
- `measured_no_selection_reversal`: eligible comparisons performed, same minimum each time.
- `measured_reversal_tail_unknown`: changed winner, complete continuation unavailable or untested.
- `measured_reversal_checked_not_better`: completed comparative candidate does not improve the old complete candidate/root result.
- `measured_reversal_checked_better`: compatible complete candidate comparison improves cost on its stated entry/root.
- `censored`: run or diagnostic coverage interrupted; retain covered prefix and omissions.

Use counters and an evidence/result field rather than pretending every call has the strongest final status. No extra count-based success gates.

## 4. Counterfactual key

Let D be the current debt, G the current native satisfied-slot count. During the exact current gate only:

```
old_advance(y) = D(y) < D(s)
new_advance(y) = old_advance(y) OR G(y) > G(s)
```

Retain the actual goal-array membership/range preconditions and all true-terminal handling in the existing code. Outside the gate the alternative returns the exact old selection.

For each eligible row retain:

```
source exact semantic key + native namespace/generation
row locator + collision-checked semantic identity
planner/runtime programme identity
immediate original cost
old progress probability
new progress probability (UNION, not sum)
true goal probability
restart classification
literal pending-route count
old complete key / new complete key
old winner / alternate winner
first differing tuple coordinate
```

For stochastic observed-choice groups, use the current existential-within-group rule only to compute this heuristic; add the group probability at most once. It grants no right to choose before the native observation. Keep production policy-choice resolution unchanged.

The existing pending-route count is not probability mass and does not establish executable completeness; it counts particular positive-mass unknown routes before cost in the startup key. Do not reinterpret or reorder it in S. Equally, do not claim an old scalar `selection_values` entry makes an unknown route executable.

## 5. Bound the evidence without making missingness ambiguous

Retain at most 32 full comparison witnesses, with independent aggregate totals over all eligible calls. Include changed and unchanged examples; preferentially retain the first distinct semantic reversal rather than 32 repeats of one source. Hashes locate; exact semantic keys or a checked canonical encoding establish equality. Run-local numeric IDs are not portable identity.

For changed rows, record native source and outcome properties as overlapping masks: requested coverage, any extra affix, capacity obstruction by side, missing-goal blocker, held prefix/suffix subsets, below-tier presence, fracture/craft, persistent state and hidden-control flags. A side-obstructed high-progress state sets both flags. These describe the actual input; they do not certify cleanup affordability.

Diagnostic vectors and retained keys count against the original budget. No full report rewrite per call or copying all rows repeatedly. Do not claim zero overhead; measure it on fixed logical fixture inputs separately from timed root outcomes.

## 6. Economic follow-through

At most two distinct reversals may be followed through an existing complete native candidate/continuation path. Mark:

```
complete supported continuation already available
complete construction possible within the remaining admitted witness budget
missing positive-probability route
no compatible boundary certificate
unsupported programme/scope
resource censored
proper but more expensive / useful / unchanged
```

Never assign the old root value to an arbitrary successor. Never turn a partial row into a normalized selected action. A comparison that only proves a cheaper local tail cannot be called a cheaper original-root policy. A later unrelated candidate winning is not attributable to the changed seed without identity/lineage.

A live S-only intervention is deferred until an explicit later choice. This programme supplies the coverage and witness necessary to select it; it does not mix S with the finder G treatment.

## 7. Mandatory small native tests

Test the exact real helper/caller path for: gate-on winner reversal; progress mass changes but pending routes prevent reversal; old cleanup progress retained; overlapping debt/acquisition event counted once; one choice event with several advancing alternatives counted once; invalid/incomplete/unpriced rows remain ineligible; output object present but unverified disables the gate; gate-off exact old behaviour; no-call versus no-row versus no-reversal serialization; capped witness storage with aggregate counts intact. Use fixed logical data to test diagnostic passivity. Synthetic positive fixtures validate the observer, not the prevalence of the effect in A5.
