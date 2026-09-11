# Small authored updates to reconcile during implementation

These are proposed text changes, not already-applied repository edits. Preserve historical receipts and generated artifacts. Reconcile the real local stage before using current-status text.

## Current HANDOFF: push-visibility correction

At reviewed remote `edd8ec25f40b11c8ba4f4aaf157db9405b593ed2`, replace only the stale last paragraph that calls the completion local-only with:

> The completed goal-reaching programme is present on remote `main` at
> `edd8ec25f40b11c8ba4f4aaf157db9405b593ed2`, reviewed from base
> `a5ef8a5b7d6263fdc60f94957b0b76f7978f8f72`. Its original receipt records the
> implementation session's actions; this current handoff records observed remote
> visibility. Work remains sequential without subagents or automatic restart;
> future commits stay local unless Oliver separately authorizes a push. Preserve
> protected path `0` and unrelated work. The deferred Veiled split is unrelated.

When Oliver sends this next plan, replace the old “No next implementation has been selected” sequencing sentence with the actual selected programme and its real current step. Do not leave both statements active or describe future acceptance as completed.

## Bounded strategy description: do not imply optimality

Trace the metadata producer. For a **successfully evaluated bounded result**, suitable scope wording is:

> Independently evaluated executable policy under the declared zero-progress-reroll
> and no-economic-restart restrictions, with automatic Imprint programs excluded.
> Optimality has not been established; consult the solver's certified bounds and
> result status.

Where the description is created **before evaluation** or shared by several statuses, use neutral wording instead:

> Strategy for the declared zero-progress-reroll and no-economic-restart scope,
> with automatic Imprint programs excluded. This scope description does not assert
> successful evaluation or optimality; consult the solver result and its bounds.

Generate the restrictions from the actual existing scope owner; do not hardcode those restrictions for a request where they differ. Exact results may retain an appropriately justified exact label. A description change must not set a native status, alter a certificate, or manufacture a bound.

Do not edit the old retained strategy JSON to apply this wording retroactively. Its bytes are evidence. Add a focused bounded-versus-exact metadata test at the actual producer when that owner is changed.

## Expected-cost annotations

No blanket replacement is supplied for node `expected_cost`: this review did not establish the producer's complete field semantics. Determine whether it means a provisional continuation estimate, a verified entry cost, or something else. Describe that exact meaning in the existing schema/source documentation. Never replace every node value by the independently evaluated root value, and never use a provisional annotation as a verified continuation.

## Relevant mathematical insertion

Use review_and_knowledge.md section 5 as the authored argument to integrate, or link the existing equivalent owner instead of duplicating it. Preserve the distinction between evaluated cost attribution, one-time deviation proposals, proper policy replacement, and full-scope optimality proof. The 16 Python checks are exact synthetic examples, not proof of native correspondence.
