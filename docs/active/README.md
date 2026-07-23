# Active Work

**Status: the mechanical solver split is selected. Baseline capture is the
exact next step; no source motion has started.**

Parent: [Documentation map](../README.md)

Oliver selected the
[mechanical solver split](mechanical-split.md) on 2026-07-22. It restructures
`engine/src/solver_solve.cpp` into phase-scoped translation units under strict
body-token, bit-hash, existing-test, and performance parity gates. It adds no
behavior or tests and does not run the exact oracle.

The Calculator, `solver_internal.hpp`, and web-test splits remain deferred.
The oracle performance profiling order recorded in
[HANDOFF](../../HANDOFF.md) remains unselected and must not be interleaved.

The bounded policy results and benchmarking plan completed B1 through B6 on
2026-07-22 and is preserved in its
[dated archive](../archive/2026-07-22-bounded-policy-and-benchmarking/README.md).
Its stable contracts now live in the solver, product, engine, decisions, and
evidence documents linked from the [documentation map](../README.md).

The preceding exact constructive-policy search established a finite natural
three-T1 bracket but did not close exact optimality. It is preserved as a
[historical incomplete milestone](../archive/2026-07-22-exact-constructive-policy-search/README.md)
and no longer owns sequencing.
