# Product Action Dependency Reachability

**Status: completed.**

Parent: [Documentation index](../../README.md)

- [Plan](plan.md)
- [Gate 0 baseline](evidence/gate0-baseline.md)
- [Implementation report](report.md)
- [Final evidence](evidence/final-evidence.md)

## Objective

Keep the Calculator's existing narrow goal-relevant action discovery while
making every dependency of a materialized automatic option reachable through
the native registry, layout, solver, compiler, exact evaluator, and release
WASM path.

The governing contract is:

> Every action that passes the existing product relevance rules is either
> admitted as a candidate, retained as an automatic-option dependency, or
> rejected with a correct deterministic reason.

The implementation preserves the narrow Harvest, Essence, and bounded Fossil
filters; separates candidates from engine-owned dependencies; makes automatic
Eldritch reachable through native and release-WASM product paths; adds bounded
Cannot Roll and cleanup compositions; rejects corruption-only Essences
natively; and discloses the scoped solve compactly in Calculator results.
