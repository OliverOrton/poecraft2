# True First-Frontier Successor Census

**Status: completed negative architecture result on 2026-07-27.**

Parent: [Archive](../README.md)

Plan: [Completed plan](plan.md)

Report: [Final report](report.md)

Evidence:
[true-successor-frontier-census-summary.json](../../../fixtures/solver-natural-t1/v1/evidence/true-successor-frontier-census-summary.json)

The live exact Chaos evaluator completed the previously cap-censored first
rows for four frozen natural-T1 hard cases. Exact support ranges from 222,580
to 3,204,323 states. Every deterministic census field repeated identically.

The support is 98.71% to 99.65% dense in the Cartesian product of the
collision-checked prefix and suffix payload projections. The first 200,000
states had already exposed every one-sided and goal-status projection class;
completion added joint combinations inside those classes. Combined with the
preceding falsification of rank-one probability factorization, the census
selects no compact exact Bellman representation.

The closest cleanup ceiling also remains insufficient. Full-four needs 22,581
states removed merely to fit one carrier and its Chaos support under the
existing cap, while impossible free deletion of every nonterminal successor
with two goals on one side removes only 21,887.

All measurement-only source was restored. No mechanics, solver behavior, cap,
ABI, artifact, binding, WASM, web, or product behavior changed.
