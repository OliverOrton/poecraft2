# Verified Interim Upper Publication

**Status: complete on 2026-08-21.** See the [result](result.md).

Parent: [Automatic Action Relevance And Proof Reuse](../2026-08-21-automatic-action-relevance-and-proof-reuse/result.md)

## Objective

Make an independently verified strict executable strategy visible through the
existing cooperative solve progress upper bound while exact alternative-action
refinement continues. Do not change the lower bound, exactness classification,
action admission, Bellman ordering, or final result contract.

The checked target is the Allflame four-natural-T1 Conquest Lamellar case. The
observed defect is that strict refinement can compile and exactly evaluate a
cheaper proper strategy while the browser continues to display the older
coarse `3759.5969190423507` upper for the remainder of the solve.

## Gate 0 — Authority Audit

- Trace strict selected-policy publication through compilation, exact graph
  evaluation, properness, executability, paired defaults, and zero off-policy
  checks.
- Trace `PolicyExactLiftWork` frontier restarts and the outer `SolveWork`
  progress path.
- Reuse the existing C/WASM/TypeScript `upper_bound` contract; do not add a
  parallel frontend authority.

## Gate 1 — Monotone Interim Incumbent

- Publish an interim upper only after the complete compiled-policy assertion
  succeeds with finite nonnegative exact cost.
- Retain the best observed strict upper across competitive-frontier pass
  restarts.
- Tighten only the live progress upper. Leave the live lower, result values,
  policy, refined artifact, convergence, and termination untouched until the
  existing final publication boundary completes.

## Gate 2 — Focused Qualification

- Build the native engine and run the focused native solver suite.
- Run the real four-T1 primary once with verification disabled and confirm the
  strict upper appears before the five-minute stop, remains monotone across
  frontier restarts, and does not create a lower certificate.
- Rebuild release WASM, type-check the web app, and run the focused WASM engine
  smoke suite. Do not run the full repository pipeline.

## Stop Conditions

- Any upper is exposed before independent compiled-policy evaluation proves a
  proper executable success-one policy with zero off-policy mass.
- The lower bound, exactness, final policy, or action envelope changes merely
  because an interim upper is observed.
- The real primary raises its upper after a strict frontier restart.
