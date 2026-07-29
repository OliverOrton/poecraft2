# Practical Exact Four-Goal Solving Research

**Status: completed research; no optimization implemented.**

Parent: [Documentation archive](../README.md)

This investigation attributes the first 200,000 states of
`natural-t1-full-four-47d8b909aa88`, measures the global abstraction cost of
Fracture and cleanup observers, projects the completed root distributions onto
coarser layouts, and reprices the retained root rows against hypothetical
one-to-three-goal anchor values.

The principal result is that zero-goal carriers are not the work source. The
current global exact-group layout is: Fracture alone expands the root Chaos
support from `217` coarse carriers to `134,477` strict carriers. The public
solver constructor also forces that exact layout even without an observing
action. Dependency-only crafted-mod cleanup would have the same effect if it
were in the parent vocabulary, but it is not in this case.

No solver behavior, mechanic, public ABI, binding, strategy vocabulary, or
product default changed. The retained source is a finalization-only audit
behind the existing benchmark-only, default-disabled
`high_impact_executable_uppers` option.

- [Research report](report.md)
- [Paste-ready implementation handoff](handoff.md)
- [Tracked evidence summary](../../../fixtures/solver-natural-t1/v1/evidence/practical-four-goal-solving-research-summary.json)
