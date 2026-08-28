# PDR Strict-Proof Memory Attribution And Repair

**Status: stopped at Gate 1 on 2026-08-27.**

Parent: [Documentation archive](../README.md)

- [Plan](plan.md)
- [Result](result.md)

The current coarse-graph checkpoint could not replay the four-mod PDR witness
faithfully. The ordinary solve's open incremental action scheduler continues
to create carriers and rows beyond the last prepared Bellman graph, and that
live scheduler state is not part of the coarse checkpoint contract. Both a
stable-prefix replay and a broader calculator-closure replay changed the
problem being solved.

The stop condition therefore fired before memory attribution. No experimental
solver change was retained. A successor must first add a scheduler-aware or
first-strict-partition checkpoint and prove ordinary/replay parity on this
witness before using replay evidence to repair strict-proof memory.
