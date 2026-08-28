# Solver Development Checkpoint/Replay

**Status: completed 2026-08-27.** Oliver selected the remaining solver-debt
item on 2026-08-27. This boundary implements a native-development checkpoint at the
completed coarse transition-graph seam. It does not change solver mechanics,
publication authority, product defaults, or release-WASM behavior.

Parent: [Active work](../README.md)

## Objective

Persist the expensive, price-independent coarse solve closure together with
the exact Calculator state, planner-operator, and state-local admission
authority needed to replay it in another native process. A replayed solve must
enter the existing transition-cache compatibility path and rerun Bellman,
refinement, compilation, and evaluation normally.

This is not a request/result cache. A checkpoint contains the actual sparse
graph and its state/operator namespace. It is accepted only for the exact
caller-supplied identity and exact binary format version.

## Gates

1. Define a versioned, native-development-only binary format with magic,
   format version, exact identity, payload length, and checksum. Refuse
   incomplete/focused graphs, proof-carrying quotient graphs, in-flight
   automatic admission, and in-flight reforge rows.
2. Serialize and restore the interned abstract states, dynamic planner
   operators, candidate ordering, state-local automatic admission decisions,
   and every behavior-bearing coarse sparse-graph arena.
3. Expose save/load through the native C ABI and benchmark CLI only. The
   release WASM export list remains unchanged.
4. Prove a clean cross-process save/load rerun reports transition-cache reuse
   and matches the ordinary result on values, bounds, policy/graph identities,
   and compiled exact evaluation. Add mismatch, truncation/corruption, and
   unsupported-boundary refusal controls.
5. Run focused native acceptance, benchmark a representative replay, update
   stable solver documentation and `HANDOFF.md`, then archive this boundary
   with coherent local checkpoint commits.

## Boundary Choice

The completed coarse graph is the smallest faithful high-value ownership cut:
it removes reachable-state expansion and coarse transition construction from
later refinement/compiler experiments while preserving normal downstream
execution. A first-closed-strict-partition snapshot would additionally require
serializing coroutine continuations, the persistent exact oracle, proof-store
dependency generations, quotient partition state, and in-flight exact kernels.
That is a distinct format extension, not a prerequisite for completing the
original graph-replay debt item, and is outside this milestone.

## Non-goals

- no browser or WASM checkpoint API;
- no portable checkpoint across engine builds, artifacts, goals, or option
  scopes;
- no checkpoint as correctness, exactness, or publication evidence;
- no generic object-memory dump and no JSON result cache;
- no solver-quality heuristic or resource-cap change.
