# Competitive Lazy Alternative Certification

**Status: selected next structural boundary; implementation has not begun.**

Owner: Oliver

Parent: [Active work](README.md)

Source evidence:
[Exact Reforge-Work Growth Diagnostic](../archive/2026-08-02-reforge-work-growth-diagnostic/README.md).

## Objective

Replace eager pre-partition certification of every admitted alternative with
proof scheduling driven by the current selected policy, Bellman
competitiveness, and refinement counterexamples. Preserve the complete
admitted vocabulary and all existing proof/publication requirements while
avoiding linear expansion of alternatives that cannot yet affect the selected
executable policy.

## Required shape

1. Discover and certify selected-policy closure first.
2. Represent every admitted but uncertified alternative as an explicit
   unresolved lower-only proof obligation. It may influence optimistic lower
   reasoning but cannot support an executable upper.
3. Certify an alternative transactionally only when a bound proves it can be
   competitive or an existing counterexample invalidates the current
   selection.
4. Preserve all action-independent observation, replay, partition,
   invalidation, properness, compilation, and exact reconciliation authorities
   from the proof-carrying quotient milestone.
5. Publish an upper only when every selected reachable row is current, target
   dependencies are closed, the shared partition is lumpable, every entry is
   proper, compilation succeeds, and independent exact cost reconciles.

## Non-goals and frozen boundaries

Do not change mechanics, prices, product action filtering, the admitted action
vocabulary, the 1 GiB memory cap, the 20M frozen product-case cap, watchdogs,
solver objective, public C ABI, strategy JSON, WASM contract, or frontend
authority merely to make the case pass. Do not hard-code named actions or
fixtures, and do not treat unresolved alternatives as absent.

Deterministic checkpoint/replay remains deferred. A detailed implementation
and qualification plan must be established before source edits begin.
