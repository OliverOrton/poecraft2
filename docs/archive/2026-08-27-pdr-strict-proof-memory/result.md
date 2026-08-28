# PDR Strict-Proof Memory Attribution And Repair Result

**Status: stopped at Gate 1 on 2026-08-27. Gates 2 through 5 were not entered.**

Parent: [Milestone summary](README.md)

## Result

The existing native coarse-graph checkpoint is not a truthful replay boundary
for the fixed four-mod PDR witness. The witness has an open incremental action
envelope: delayed automatic action generation continues after the last
prepared coarse Bellman graph. The checkpoint preserves the graph, action
ledger, and calculator identity represented at that boundary, but it does not
preserve the live scheduler state that determines subsequent carrier and row
generation.

The plan's first stop condition therefore fired. Memory attribution and repair
would have measured a different solve, so they were not attempted. All
experimental engine changes were removed; product behavior, native ABI, and
release WASM are unchanged.

## Gate 1 Evidence

The authoritative ordinary 1 GiB witness remains:

| Field | Ordinary PDR witness |
| --- | ---: |
| Independently evaluated bounded upper | `7866.432124027084` |
| Certified lower | `21.772459401271156` |
| Strict reforge work | `3507568` |
| Proof store plus quotient | `846846750` bytes |
| Native peak | `1179431999` bytes |
| Stop | `max_solver_owned_bytes` |

A temporary 300 MiB diagnostic control was used only to iterate on checkpoint
lifecycle in about one minute per ordinary run. It reproduced the same upper
and lower and stopped on the same resource class. It exposed three distinct
live representations at the stop:

- `1207` states in the last prepared Bellman transition-cache graph;
- `7242` calculator carrier states after delayed generation; and
- `61476` current rows, including rows appended after the prepared graph.

The first row outside the prepared graph belonged to carrier state `1207`,
immediately beyond the graph's state range. This is not incidental capacity:
the ordinary solve is deliberately growing its action/carrier envelope after
the reusable coarse graph was prepared.

Two replay attempts bracketed the mismatch:

1. A broader `7242`-carrier closure produced a roughly 73.5 MiB checkpoint,
   but replay reduced a different graph and terminated with no executable
   policy. That closure is not the original solve boundary.
2. A stable `1207`-state prefix produced a roughly 6.36 MiB checkpoint. After
   reconstructing the full dynamic vocabulary and incremental carriers, replay
   priced all `310` scanned actions but still lacked the scheduler's remaining
   work. It ended in about 4.8 seconds with bounded upper
   `9844.962286897467`, raw start value `9928.253796119903`, the unchanged
   lower `21.772459401271156`, `18451` unresolved action obligations, an open
   envelope, and `numerical_stability` termination.

The matched 300 MiB ordinary control instead ran about 62 seconds, published
upper `7866.432124027084`, and stopped on memory. The different incumbent,
termination, outstanding work, and wall time are conclusive non-neutrality.
Graph reuse alone is not enough to characterize strict-proof memory here.

## Missing Replay Authority

A faithful successor checkpoint must jointly serialize or deterministically
reconstruct the behavior-bearing state that remains live across the coarse
boundary:

- incremental carrier identities, order, generation, and scheduler cursors;
- delayed alternative rows and their evaluation/completion status;
- unexpanded, support, and focused frontiers;
- restricted values, incumbent lifecycle, and properness evidence;
- the complete action ledger plus per-family scheduling counters; and
- source/target graph generations used by strict partition and dependency
  ownership.

A first-strict-partition checkpoint may additionally need the persistent
oracle, partitions, obligations, kernels, dependencies, cursors, and incumbent
as one atomic proof boundary. Saving only a subset must refuse rather than
silently resume as a different solve.

## Successor Acceptance

Before returning to PDR memory attribution, one ordinary/save/replay triplet
must reproduce the fixed 1 GiB witness's request/action identities, incumbent
and independently evaluated upper, certified lower, strict frontier/work,
resource stop, and open-envelope obligations. Only then may replay telemetry
be treated as evidence about retained proof/quotient ownership.

No full acceptance pipeline or release-WASM rebuild was warranted because no
release source change was retained.
