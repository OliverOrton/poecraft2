# Exact Solver Action And State Pruning

**Status: completed and archived on 2026-07-21.**

Parent: [Exact Solver Action And State Pruning archive](README.md)

## Objective And Fixed Boundaries

Reduce complete product-solver work in state-local automatic action
construction and discovered-state growth while preserving the admitted
product action envelope, exact transition/policy behavior, repricing safety,
compiled strategy behavior, and every Q5 resource cap. Approximate compaction
was forbidden.

Early rejection required the same exact legality, producibility, equivalence,
or lower/upper-bound authority as the full solver. Price-dependent pruning
could not make a partial transition graph appear price-independent.

## Completion By Phase

- **A0 — baseline:** pinned the accepted Q5 two-T1 and three-slot reports and
  added focused native oracle gates.
- **A1 — impossible automatic actions:** exposed the engine-owned explicit
  affix producibility mask and rejected protected repeats whose follow-up
  could not satisfy the target or whose exact lock setup was illegal.
- **A2 — exact reuse:** measured protected time falling from 17.709 seconds to
  0.268 seconds. The surviving 9,191 rows already reduced to 101 exact
  templates with 9,090 hits, so no additional speculative reuse layer was
  added. Price variants remain available for exact completed-graph repricing.
- **S1 — constructive upper:** deterministic goal finishes and Restart are
  scheduled before broad stochastic kernels. The accepted three-slot upper
  becomes finite at expanded state 1.
- **S2 — exact state certificate:** a relaxed engine-owned goal-production
  set cover supplies an admissible continuation lower. A carrier stops only
  when one executable row upper is strictly below every other admitted
  operator's optimistic lower. Witnesses retain both bounds and the omitted
  operator count. Price-bound partial graphs are not cached.
- **S3 — terminal/discovery accounting:** telemetry distinguishes accepted
  certificates, operators pruned, first constructive upper, and the exact
  discovered/expanded counts. No unsafe terminal-state canonicalization was
  introduced.
- **A3 — acceptance:** native oracle suites, both product controls, compiled
  behavior, 10,000-run verification, WASM rebuild, non-visual web tests,
  documentation, and handoff were completed.

## Result

The three-slot product retained exact value `3`, its 32-action product
envelope, 5-node compiled strategy, and 10,000/10,000 mean-cost-3 verification.
Its strict graph fell from 169,892 discovered / 56,838 expanded states and
1,214,860 rows to 2 / 2 states and 1 row. The selected bench upper was `3`;
the cheapest competing optimistic lower was `3.0058720000000001`.

The two-T1 product accepted no certificate and retained exact value
`230.26738656962243`, 189,946 strict states, 903,935 rows, its 6,391-node
compiled strategy, and passing 10,000-run verification. Every cap remained at
the Q5 value.
