# Gate 7 compiler and presentation integrity

**Measured:** 2026-08-12 on `codex/solver-goal-realignment`.

Parent: [active plan](../plan.md)

## Result

Gate 7 is complete without a further implementation change. The headline
native policies from Gates 2, 4, 5, and 6 compile the actual selected policy,
preserve engine-owned action and observation provenance, exact-evaluate to the
published solve cost, have eventual success probability one, and expose zero
failure, unresolved, and off-policy mass. Their native Simulator controls use
the same strategy documents; Gate 5 and Gate 6 each completed the required
10,000-run selected-policy qualifications.

The focused compiler suite passed 804 checks. It covers strict regions,
Restart routing, modifier flags and tags, observed Unveil choices, quotient
choice collisions, the compiled artifact fixture, Imprint create/restore, and
10,000-run native simulations. The exact evaluator suite passed 1,174 checks,
including exchangeable-family A/B equivalence, loop accounting, and native
Monte Carlo reconciliation. The focused Solve suite passed 6,672 checks.

The frontend presentation contract explicitly distinguishes:

- exact policy, bounded feasible policy, bounded near-optimal policy, and no
  executable policy;
- lower bound, independently evaluated returned-policy cost, upper bound,
  absolute gap, and conservatively rounded multiplicative factor;
- named state/transition/memory/refinement caps and the stage that stopped
  publication;
- product goal-relevant scope and the zero-progress-gated reforge restriction;
- admitted primitive and automatic families, missing price keys, deferred
  automatic candidates, and open action obligations; and
- a Chaos-only exact result from a Chaos incumbent whose alternative envelope
  remains open.

Equal displayed numeric bounds without `policy_available` produce no compile
button, no verification result, and no exactness authority. Likewise, an
explicitly open action envelope overrides inconsistent exact metadata and is
presented only as an executable incumbent. HTML-sensitive action, economy, and
price identities are escaped.

The Strategy Board preserves solver provenance and engine-authored observation
conditions opaquely, round-trips nested router conditions, and evaluates the
same document. Calculator mode uses the existing annotation channel and
retains exact evaluator refusal text. No rendered or visual review was
performed; Oliver owns that review by repository policy.

## Focused commands

```powershell
build/engine/poecraft_engine_tests.exe --solver-solve-only
build/engine/poecraft_engine_tests.exe --solver-compile-only data/compiled/current
build/engine/poecraft_engine_tests.exe --solver-eval-only
cd apps/web
npx tsc --noEmit
npx tsx test/solver-result-presentation.test.ts
npx tsx test/solve-workspace.test.ts
npx tsx test/strategy-model.test.ts
npx tsx test/strategy-calculator-mode.test.ts
npx tsx test/calculator-goal-item.test.ts
npx tsx test/solver-benchmark-corpus.test.ts
```

All commands passed. The corpus contract suite reported seven passing tests;
the remaining focused TypeScript tests completed every assertion shown by
their executable scripts. Full native/release-WASM and repository-wide
acceptance remains owned by Gate 8.
