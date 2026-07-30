# Fracture-Local Coarse-Parent Prototype

**Status: completed and qualified.**

Parent: [Documentation archive](../README.md)

The product solver now keeps Fracture out of its ordinary/reforge parent
observer set. Its parent uses six junk classes, while a solver-local exact
Fracture row emits one branch per acceptable physical goal hit and one
aggregate dead-miss branch through priced Restart. Calculator, primitive
Fracture, simulator, authored strategies, the C ABI, and strategy vocabulary
remain exact and unchanged.

On the frozen gated full-four case, root Chaos support is exactly 217 and the
carrier graph closes at 927 discovered / expanded states rather than stopping
at 200,000 / 160. All 29 selected Fracture rows pass properness, their compiled
misses share one canonical Restart node, and repeated qualification produces
identical transition, policy, and strategy hashes.

The result is a proper bounded policy within the existing zero-progress reroll
restriction. The incremental action envelope remains open, so the milestone
does not claim unrestricted exact optimality.

- [Frozen plan](plan.md)
- [Final report](report.md)
- [Tracked evidence summary](../../../fixtures/solver-natural-t1/v1/evidence/fracture-local-coarse-parent-summary.json)
