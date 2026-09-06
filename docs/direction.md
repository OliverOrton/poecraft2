# Project direction

poecraft2 is a Path of Exile crafting simulator and expected-cost planner. One
native engine owns crafting behavior. Ingested canonical data becomes a compiled
runtime artifact; Python and WASM bindings expose the engine to native workflows
and the web product.

## Solver objective

The research objective is to extend **certified exact closure as far as
practically achievable**. Strong executable policies, admissible lowers, faster
preparation, and lower memory use are useful intermediate results. They are not
automatically evidence of wider exact closure.

Keep fixed semantic cohorts and resource envelopes for honest comparisons, while
expanding a separately labelled development frontier. The target model,
properties, and reference evidence remain useful when an algorithm is replaced.

The [mathematical model](solver/mathematical-model.md) defines the optimization
question. [Mathematics](solver/mathematics/README.md) explains the arguments.
[Solver mechanisms](solver/README.md) and the [source map](foundation/solver-internals.md)
explain the implementation. [Research](solver/research.md) connects findings to
questions rather than technique-specific chronology.

## Product and authority

Native and browser surfaces are interfaces to the same mechanics authority.
Canonical SQLite and derived runtime data retain their existing ownership.
Oliver decides ambiguous crafting mechanics. A useful bounded policy is not
labelled optimal merely because it compiles or its value was evaluated.

Native experimentation can use a different execution resource envelope from the
browser when that difference is explicit. Neither a larger budget nor a product
restriction silently changes the meaning of a comparison.

The web product, economy pipeline, and other deferred features keep their
existing contracts. This orientation selects no new implementation boundary.
Use [HANDOFF](../HANDOFF.md) for actual active work, [AGENTS](../AGENTS.md) for
shared operating policy, and the [documentation map](README.md) only when an
owner needs locating. Detailed milestones and historical measurements remain in
the [archive](archive/README.md), not in this direction page.
