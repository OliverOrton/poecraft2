# Solver

**Status: current architecture index.** Verified against current source on
2026-08-27. Historical measurements and superseded implementation narratives
live in [Architecture History](architecture-history.md) and the dated archive.

Parent: [Documentation](../README.md) | Private source map:
[Solver Internals](../foundation/solver-internals.md)

The solver turns a native item, an exact item goal, an action envelope, and an
economy into an executable strategy. Mechanics remain native engine authority;
the web app only describes a request and presents native results.

## Current Contract Pages

| Question | Authority |
| --- | --- |
| Which actions and restrictions are in this solve? | [Request And Action Scope](request-action-scope.md) |
| What is a solver state and what carrier facts remain exact? | [States And Carriers](states-carriers.md) |
| How are stochastic action rows built and charged? | [Transitions And Reforge Work](transitions-reforge.md) |
| How are states expanded and policies solved? | [Scheduling And Bellman Search](scheduling-bellman.md) |
| Where can an executable upper come from? | [Executable Upper Authority](upper-authority.md) |
| What may prune work or become a public lower? | [Lower And Pruning Authority](lower-pruning.md) |
| What turns a coarse policy into an exact proof? | [Strict Closure](strict-closure.md) |
| What is required before a strategy is returned? | [Publication, Compilation, And Evaluation](publication.md) |
| Which caps, resumable tasks, and replay boundaries exist? | [Resources, Resume, And Replay](resources-resume-replay.md) |
| Which progress and diagnostic fields should I inspect? | [Telemetry](telemetry.md) |

Supporting references:

- [End-to-end solver flow](flow.md) retains the detailed sequence diagrams.
- [Benchmarking](benchmarking.md) defines corpus and harness practice.
- [Solver notes](NOTES.md) contains observations that have not become stable
  contract.
- [Architecture history](architecture-history.md) preserves the former
  monolithic reference and its dated addenda; it is evidence, not current
  sequencing authority.

## Authority Ladder

```text
mechanics + request scope
  -> native transition rows
  -> reachable sparse graph and Bellman policy
  -> independently executable upper candidate
  -> strict carrier/action accounting
  -> compiled graph and exact graph evaluation
  -> bounded or exact public result
```

An upper, a lower, and exactness are separate authorities. A useful compiled
policy may be returned while the lower remains weak. A restricted action
envelope may have an exact value within that envelope without proving anything
about disabled actions. A partial row, frontier, checkpoint, heuristic score,
or sampled simulation never gains proof authority by being finite.

## Failure Vocabulary

- `no_executable_policy`: no proper fully priced policy was retained.
- `refused_resource_cap`: useful bounded evidence may exist, but a named work,
  state, transition, memory, compilation, or evaluation limit stopped proof.
- `requested_bounded_finish`: the caller requested publication at a cooperative
  boundary; this is not an exactness claim.
- `target_gap`: a certified lower and executable upper met the requested gap.
- `exact_closed`: the requested action envelope and strict proof obligations
  closed and the selected compiled strategy reconciled exactly.
- `numerical_stability`: a policy equation or reconciliation check could not
  establish its required numerical contract.
