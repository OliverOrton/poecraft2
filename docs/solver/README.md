# Solver

The solver takes a native item, a goal, an action/program scope, and prices, then
attempts to produce a certified result and an executable strategy. Native
mechanics remain authoritative.

Use the mathematical route for **why a transformation or result is valid** and
the mechanism route for **how the current source implements it**. Neither is a
mandatory whole-library read.

## Mathematical route

| Question | Reference |
|---|---|
| What problem and policy class are being optimized? | [Mathematical model](mathematical-model.md) |
| How do the arguments compose across the solver? | [Mathematical reading guide](mathematics/README.md) |
| What are the exact preconditions and known counterexamples? | [Claim ledger](claims.md) |
| What has research established, and what remains open? | [Research questions](research.md) |

## Current mechanism route

| Responsibility | Reference |
|---|---|
| Requests, restrictions, and action admission | [Request and action scope](request-action-scope.md) |
| State fields and carrier representation | [States and carriers](states-carriers.md) |
| Kernels, probability, and reforge work | [Transitions and reforge work](transitions-reforge.md) |
| Expansion and numerical work | [Scheduling and Bellman search](scheduling-bellman.md) |
| Executable candidate authority | [Upper authority](upper-authority.md) |
| Admissible patterns and retirement | [Lower and pruning authority](lower-pruning.md) |
| Exact alternative closure | [Strict closure](strict-closure.md) |
| Returned strategy and evaluation | [Publication](publication.md) |
| Limits, lifetime, and replay | [Resources, resume, and replay](resources-resume-replay.md) |
| Diagnostic fields | [Telemetry](telemetry.md) |
| Complete flow and source ownership | [Flow](flow.md) and [solver internals](../foundation/solver-internals.md) |
| Measurements and comparison semantics | [Benchmarking](benchmarking.md) |

Lower, upper, ordering, and exactness evidence are separate. A finite partial
row or estimate does not acquire authority through its type name. A useful
policy may be returned while proof remains open, and a completed auxiliary
model need not solve the native request.

Current work is owned by [HANDOFF](../../HANDOFF.md). [Architecture history](architecture-history.md),
[notes](NOTES.md), and the [archive](../archive/README.md) answer particular
historical questions; they are not the routine startup path.
