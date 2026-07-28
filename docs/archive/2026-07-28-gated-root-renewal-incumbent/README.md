# Gated Root Renewal Incumbent

**Status: complete (2026-07-28), implementation retained.**

Parent: [Archive](../README.md)

- [Final report](report.md)
- [Completed plan](plan.md)
- [Tracked evidence summary](../../../fixtures/solver-natural-t1/v1/evidence/gated-root-renewal-incumbent-summary.json)

The opt-in gated solver now turns a completed root primitive
destructive-reforge row into the executable bounded policy “repeat this
reforge until goal.” It proves action legality and the same exact
engine-owned kernel signature for every non-goal exit, publishes value
`cost / success_probability`, and preserves the complete competing action
envelope.

Both frozen four-mod cases now return finite policies after the first Chaos
row. Each compiles to a four-node loop; neither is claimed globally optimal.
Exact discovery remains stopped by the next competing root broad reforge at
the unchanged work cap. The unrestricted exact solver remains the default and
is behaviorally unchanged.
