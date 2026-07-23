# Active Work

**Status: no implementation boundary is active.** The mechanical solver split
completed on 2026-07-22. Oliver must select the next chunk before
implementation resumes.

Parent: [Documentation map](../README.md)

The completed
[mechanical solver split](../archive/2026-07-22-mechanical-solver-split/README.md)
restructured `engine/src/solver_solve.cpp` into phase-scoped translation units
under strict body-token, bit-hash, existing-test, and performance parity
gates. It added no behavior or tests and did not run the exact oracle.

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
