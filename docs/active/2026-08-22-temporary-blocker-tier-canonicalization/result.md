# Temporary Blocker Tier Canonicalization Result

**Status: completed and accepted on 2026-08-22.**

Plan: [Temporary Blocker Tier Canonicalization](plan.md)

Implementation checkpoint: `a8deff0`

## Result

Priced temporary-blocker synthesis now selects the cheapest legal blocker
while forming one exact carrier-local effect group. Only that blocker becomes
the representative fixed option and later planner variant. Strict price ties
keep the first admitted blocker. Unpriced callers continue to retain every
differently resourced variant.

The ordering remains exact because a group already requires the same
carrier-local blocked eligible pool, blocker side, follow-up, pool-tag
behavior, and goal-slot semantics. Its variants differ only in the immediate
blocker resource vector. No mechanic, transition probability, Bellman
comparison, proof cap, action vocabulary, or strategy JSON changed.

## Focused Regression

The automatic-option regression now proves:

- a `5`-Chaos blocker loses to its exact-equivalent `2`-Chaos blocker before
  the discarded action receives an operator or decision record;
- reversing the active prices reverses the sole retained blocker;
- an exact price tie deterministically retains the first blocker; and
- an unpriced caller retains both resource variants.

The existing exact solve, selected action, value, cleanup, owned-byte ledger,
compiled strategy, and simulator checks remain in the same focused suite.

## Four-T1 Characterization

No five-minute primary was run for this focused cleanup. The stored accepted
primary baseline reported 4,779 temporary-bench candidate decisions, including
2,442 late price-collapsed variants, and 521 admitted rows. Under the unchanged
carrier/effect envelope, this change removes those late variant decisions but
does not reduce the 521 rows: the previous implementation already collapsed
the tiers immediately before row admission.

The separate goal-slot audit remains the material row/proof opportunity. That
baseline has 45 temporary-bench action identities for 12 blocker/follow-up
behaviors, 521 rows, 248 unique transition templates, and 273 exact transition
template hits. The retained root witness sample contains 33 duplicate rows
beyond one representative across 12 identical-Q groups. Protected-repeat
options also use per-slot exits; their possible forced continuation after a
different useful goal hit remains source-confirmed but not reproduced in a
selected executable upper.

## Acceptance

- `powershell -File scripts/build.ps1`: pass.
- Focused `poecraft_solver_s8_3`: pass, 0.42 seconds.
- `powershell -File scripts/build-wasm.ps1`: pass; tracked release WASM
  rebuilt from the accepted native source.
- `git diff --check`: pass before the implementation checkpoint.
- Full primary, full repository acceptance, TypeScript/web tests, and a new
  10,000-run verification were intentionally not run. The change neither
  produced nor requalified a compiled strategy.

## Successor Boundary

No implementation boundary is active. The next precise candidate is
carrier-local exact row equivalence for goal-slot variants: compare mapped
transition kernels plus active-economy immediate cost, retain one deterministic
representative when they are identical, and preserve genuinely different
stopping behavior. A stronger any-relevant-progress option for temporary and
protected repeats should be evaluated only after that behavior-neutral row
collapse establishes the actual reduction and protects counterexamples where
exit semantics matter.
