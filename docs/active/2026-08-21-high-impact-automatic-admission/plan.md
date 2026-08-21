# High-Impact Automatic Admission Closure

**Status: completed and accepted on 2026-08-21.**

Owner: Oliver

Starting commit: `c955d028e4e85a220cf33d6d01517b64dd06906d`

Parent: [Active work](../README.md)

Result: [accepted result](result.md)

## Objective

Make the Calculator high-impact incremental scheduler evaluate the same
carrier-local automatic action envelope as the existing incremental scheduler.
Exact closure must be impossible until every discovered non-goal carrier has
completed automatic preparation or the solve reports a named open resource
obligation.

## Confirmed defect

The normal Calculator request enables both goal-progress-gated reforges and
high-impact executable uppers. That scheduling branch processes delayed
Fossil, Harvest, and Essence operators but bypasses the carrier-local automatic
preparation added by the completed product-dependency reachability work.

The stored real four-T1 result retains 158 automatic product dependencies but
reports zero automatic carriers and zero synthesis time before publishing
`exact_closed`. The five-T1 priced result has the same zero-carrier omission.

## Preserved boundaries

- Preserve every current Essence, Harvest, bounded Fossil, Influence, Veiled,
  bench, Eldritch, and related product/action filter exactly.
- Keep dependency primitives non-selectable; only materialized state-local
  options may contribute solver rows.
- Do not change mechanics, probabilities, Bellman comparisons, prices, proof
  caps, result vocabulary, or strategy compilation.
- Keep cancellation and resource exhaustion cooperative and fail closed.
- Do not run the full repository acceptance pipeline for this focused repair.
- Rebuild the tracked release WASM artifact after the native repair.

## Gates

1. Add a focused combined regression containing both a delayed reforge and a
   forced-winning automatic action; capture its pre-fix zero-carrier failure.
2. Give high-impact scheduling a per-carrier automatic-preparation obligation
   and route materialized operators through its existing alternative-row
   lifecycle.
3. Require the automatic carrier cursor to be caught up before incremental
   exact closure; resource or cancellation stops retain an open obligation.
4. Run the focused native automatic-admission and incremental-scheduler tests,
   then characterize the real four- and five-goal cases far enough to observe
   nonzero carrier/family dispositions. Five-goal exact recovery is not part of
   this repair.
5. Rebuild release WASM, run targeted WASM/web and TypeScript checks, update
   the handoff with measured results, and create a coherent local checkpoint
   commit. Do not push.

## Acceptance

- The combined regression fails before the repair and passes afterward.
- High-impact runs prepare every retained carrier once and expose admitted
  automatic rows to the same priced policy selection as other alternatives.
- `exact_closed` cannot be earned while automatic carrier preparation is
  pending, unresolved, cancelled, or resource-capped.
- Existing narrow product-family scope remains unchanged.
- Native and release WASM targeted checks pass, documentation reflects the
  actual boundary, and the worktree is captured in a local co-authored commit.
