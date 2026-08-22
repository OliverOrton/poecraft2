# Temporary Blocker Tier Canonicalization

**Status: selected for implementation on 2026-08-22.**

Owner: Oliver

Starting commit: `e030997e32ea46c0b1f4e26934b08ea6ae70c1cd`

Parent: [Active work](../README.md)

## Objective

For one carrier-local temporary-blocker effect, retain only the cheapest
legal, priced blocker craft under the active solve economy before building a
child fixed option, exact kernel, planner operator, Bellman row, or proof
obligation. Equivalent higher-priced bench tiers must not propagate into the
solver merely because they use different resource keys.

## Confirmed baseline

Temporary blockers are already grouped by exact follow-up, goal-slot, blocker
side, pool-tag behavior, and the carrier-local blocked eligible pool. The
current implementation evaluates one representative fixed-option kernel per
effect, then expands every separately priced blocker into a planner variant
and removes the non-cheapest variants immediately before operator admission.
That late result is sound and already prevents extra Bellman rows, but it
retains avoidable planner-variant construction and diagnostic work.

The checked four-T1 primary considered 139,271 raw temporary blocker variants,
formed 2,544 carrier-local effect classes, then emitted 4,779 candidate
decisions before late price collapse left 521 rows. The selected change moves
the already-authorized fixed-economy price ordering to the effect boundary. It
is expected to reduce planner/decision overhead, not the already-collapsed row
count.

## Preserved boundaries

- Cheapest means minimum immediate blocker cost under the solve's active
  economy. No price-independent ordering is invented across different
  currencies.
- An unpriced Calculator caller retains all variants because no economy owns
  their ordering.
- Only variants already proved to share the complete carrier-local blocker
  effect may compete. Distinct conflict masks, blocker sides, pool-tag
  behavior, follow-ups, or goal-slot semantics remain separate.
- Deterministic ties retain the first canonical variant.
- Mechanics, probabilities, action vocabulary, Bellman comparisons, proof
  caps, goal-slot behavior, and strategy JSON remain unchanged.
- The broader goal-slot-equivalence finding remains a separate successor.
- Do not run the full repository acceptance pipeline for this focused change.

## Gates

1. Move active-economy blocker price selection into carrier-local effect
   synthesis so each priced effect carries exactly one blocker action forward.
2. Extend the focused automatic-option regression to prove both price
   directions, deterministic removal before operator construction, and the
   preserved unpriced behavior.
3. Build native and run the focused automatic solver test. Characterize the
   real four-T1 synthesis reduction without running the five-minute primary or
   full acceptance pipeline.
4. Rebuild release WASM because browser-visible native solver behavior changed,
   update the handoff/result, and create one local co-authored checkpoint.

## Acceptance

- A priced exact-equivalent blocker effect exposes only its cheapest tier to
  fixed-option construction and admission.
- Reversing the active economy reverses the retained tier.
- An unpriced caller still retains every differently resourced variant.
- Existing exact kernels, values, legality, cleanup, and deterministic tie
  authority remain unchanged in focused tests.
- Native build, focused native automatic-option tests, and release WASM build
  pass; no full primary or full repository pipeline is run.
