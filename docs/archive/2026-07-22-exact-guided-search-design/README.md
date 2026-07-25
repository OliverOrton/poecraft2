# Exact Guided Search Design Review

**Status: historical research and prototype evidence.** This archive does not
own current sequencing or authorize source changes.

Parent: [Documentation archive](../README.md)

The [preserved report](report.md) comes from
`codex/exact-search-design` commit `273831f`. It compared poecraft2's focused
stochastic shortest-path search with LAO*, RTDP/LRTDP/HDP, BRTDP, stochastic
bisimulation, pattern databases, and constraint-generated action search. It
also measured three narrow prototypes against the contemporary natural-T1
cases.

## Disposition

| Candidate | Historical result | Current disposition |
| --- | --- | --- |
| Lock-aware scheduling signature | Exact but inert on the selected case | Prototype source was not merged |
| Reserved post-lock expansion quota | Reached one post-lock state but exhausted reforge work earlier | Rejected |
| Reuse the first constructive incumbent | Large matched-case reduction in repeated constructive work | Original patch was not merged; the later bounded-policy implementation adopted retained constructive-witness reuse with explicit validity boundaries |
| Action-incremental Bellman model with a lock-aware lower envelope | Proof-first future direction | Unselected research input only |

Only the report was transferred to `main`. The experimental changes to the old
monolithic `solver_solve.cpp` and its telemetry were deliberately left on the
historical branch. Current solver contracts live in
[Solver](../../solver/README.md); the contemporary implementation milestone is
[Exact Constructive Policy Search](../2026-07-22-exact-constructive-policy-search/README.md).

The report records an exact two-T1 oracle run performed before the current
prohibition. Do not rerun that oracle from this archive.
