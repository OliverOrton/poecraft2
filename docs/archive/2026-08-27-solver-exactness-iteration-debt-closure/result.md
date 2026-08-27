# Solver Exactness, Iteration, And Debt Closure — Result

**Status: complete with cross-process replay explicitly deferred.**

Parent: [Plan](plan.md)

## Delivered

- Exact V3 destructive rows are deterministic cooperative continuations.
  Suspended or cancelled work cannot enter row caches or proof authority.
- Product Solve no longer carries Mirrored/Synthesised dimensions or admits
  the irrelevant mirror-producing Fossil. Native simulation semantics remain.
- Strict alternative discovery returns a newly exposed carrier frontier to
  the persistent grow-in-place owner immediately.
- Telemetry collection/accounting and JSON serialization have separate source
  owners. Strict frontier yields are visible, and documents identify compact
  versus full-evidence output intent.
- The current solver reference is a concise index plus narrow pages for scope,
  state, transitions, scheduling, upper/lower authority, strict closure,
  publication, resources/replay, and telemetry. The old monolith is preserved
  as historical architecture evidence.
- Influenced modifier UI order is shared across goal options and pool display.
  Very small nonzero probabilities use scientific notation instead of looking
  like zero, and Simulator sampling shows a Wilson 95% interval.
- Synthetic Restart is excluded from ordinary renewal/reforge dispatch, so a
  base purchase can no longer be evaluated as a Transmute roll in exact or
  factored contexts.

## Measured PDR Repair

The matched four-mod PDR control used the same 50M logical-work / 1 GiB product
boundary and skipped sampled verification.

| Measure | Before | After frontier-yield repair |
| --- | ---: | ---: |
| Wall stop | 300 s watchdog | 168.418 s named resource stop |
| Alternative rows begun/completed | about 14,006 | 2 / 2 |
| Alternative obligations processed | 299,394 | returns after first growing row |
| Strict frontier insertions/states | 0 / 0 | 1 / 1 |
| Logical / V3 strict reforge work | about 32.08M / 27.21M | 3,507,568 / 1,380,787 |
| Exact states / transitions / kernels | 13,991 / not closed / about 14,054 rows | 13,991 / 161,045 / 13,625 |
| Retained exact evaluated upper | `7866.432124027084` | `7866.432124027084` |
| Certified lower | `21.772459401271156` | `21.772459401271156` |

The repair is a real proof consumer: it removes stale exact-row work and
reaches the existing grow-in-place path. It does not close the case. The next
owner is retained strict/proof memory during the second carrier-discovery
generation: proof store plus quotient retained 846,846,750 bytes and the total
native peak reached 1,179,431,999 bytes before the 1 GiB solver-owned boundary
was accepted as `max_solver_owned_bytes`.

The direct coarse solver value (`8084.68`) still differs from the exact
compiled value (`7866.43`), so strict refinement remains necessary; this is
not a justification for publishing the coarse value.

## Deliberately Not Claimed

Cross-process development checkpoint/replay was not implemented. The existing
same-`CalcContext` price-independent transition-cache reuse is real and tested,
but it cannot be written to disk faithfully without serializing calculator
state/operator/admission authority. The first strict partition is larger
still: its persistent oracle, partition generations, proof obligations and
dependencies, Bellman graph, exact kernels/cursors, and verified incumbent are
joint state. Saving only the request, final result, or sparse graph would still
rebuild the expensive owners and was rejected as a misleading checkpoint.

This is the sole unfinished item in the original nine-item cleanup list. It
should be a dedicated versioned-format boundary rather than being hidden
inside another solver behavior change.

## Acceptance

Passed:

- fresh release native build;
- focused solve, API, and policy-refinement suites at 7,902, 2,644, and 2,083
  checks with zero failures;
- rebuilt release WASM;
- `npm test` and `npx tsc --noEmit`;
- the complete `powershell -File scripts/test.ps1` pipeline, including
  3,417,248 native checks with zero failures, benchmark specifications,
  binding tests, release-WASM tests, and the repository's 10,000-run compiled
  strategy controls;
- documentation link audit; and
- `git diff --check`.

The fresh PDR control intentionally skipped sampled verification because its
compiled policy was independently exact-evaluated and retained only as a
bounded result. No rendered browser review or broad benchmark matrix was run
or claimed.
