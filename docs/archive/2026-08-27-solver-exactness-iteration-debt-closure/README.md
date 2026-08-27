# Solver Exactness, Iteration, And Debt Closure

**Status: completed 2026-08-27 with cross-process replay explicitly
deferred.**

Parent: [Documentation archive](../README.md)

- [Plan](plan.md)
- [Result](result.md)

This milestone delivered eight of nine selected stabilization items: genuinely
cooperative exact reforge rows, product-solver state reduction, a measured
strict frontier-yield repair, telemetry/source separation, rebuilt current
solver documentation, and the influenced-order and low-probability product
audits. Final acceptance also repaired synthetic Restart incorrectly entering
Transmute renewal dispatch.

The PDR control reached one strict frontier, reduced alternative-row work from
about 14,000 completed rows to two, and moved the named stop to retained
proof/quotient memory. It did not close the four-mod case. Honest
cross-process checkpoint/replay remains the dedicated next iteration-tooling
boundary; request/result JSON caching was explicitly rejected as insufficient.
