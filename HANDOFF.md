# Session Handoff

**Status: no active implementation boundary.**

The
[Fracture-Local Coarse-Parent Prototype](docs/archive/2026-07-29-fracture-local-coarse-parent/README.md)
is implemented and qualified on branch
`codex/fracture-local-coarse-parent`, starting from clean source
`490b9f77d7f143d9f14bba888ea229f47bd6919b`.

The frozen full-four carrier graph now uses six parent junk classes, has
exactly 217 online root Chaos successors, and closes at 927 discovered /
expanded states with no frontier. Product-local Fracture retains exact
physical goal-hit branches, aggregates every dead miss through priced
Restart, interns zero parent junk-miss states, and compiles all selected miss
routes to one canonical Restart node. Calculator, primitive Fracture,
simulator, and authored strategy semantics remain exact and unchanged.

The returned policy is proper and bounded within the existing
goal-progress-gated restriction. The incremental action envelope remains
open, so this milestone does not claim unrestricted exact optimality.

The next owner-selectable boundary is the deferred three-/two-/one-goal
executable anchor library. It is not active. Oliver must choose the next chunk
before implementation resumes.

Post-milestone maintenance on 2026-07-30 raises the unspecified solver
reforge-work default from 11,000,000 to 20,000,000. The selected solver-owned
memory default remains 1 GiB; native and WASM callers can already override it
per solve.
