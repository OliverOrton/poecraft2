# Product Action Dependency Reachability

**Status: active implementation boundary selected by Oliver on 2026-08-08.**

Parent: [Documentation index](../../README.md)

- [Plan](plan.md)
- [Gate 0 baseline](evidence/gate0-baseline.md)

## Objective

Keep the Calculator's existing narrow goal-relevant action discovery while
making every dependency of a materialized automatic option reachable through
the native registry, layout, solver, compiler, exact evaluator, and release
WASM path.

The governing contract is:

> Every action that passes the existing product relevance rules is either
> admitted as a candidate, retained as an automatic-option dependency, or
> rejected with a correct deterministic reason.

Implementation, final evidence, stable documentation, and the completed
handoff will be added in the second and final milestone commit.
