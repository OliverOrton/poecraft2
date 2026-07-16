# Session Handoff - S7.5 Complete, S7.6 Next

Updated 2026-07-16 after the S7.5 cycle-optimization, cache-reuse, focused-
expansion, and policy-compression checkpoint. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

S7.0-S7.5 and S7.2R are complete. S7.3 remains the earlier out-of-sequence
fixed-option commit. S7.6 has not begun.

## Exact next boundary

Implement **S7.6 only: the end-to-end product gate**.

1. Run the appropriate complete native, binding, WASM, web, and repository
   acceptance suites once, at the end of S7.
2. Solve and compile each approved real craft, then verify every compiled
   strategy with exactly 10,000 native simulator runs using its pinned seed and
   tolerance.
3. Present the final native/WASM performance, memory, responsiveness, action,
   cap, optimality, graph-size, and simulator comparison to Oliver.
4. Deliver compiled strategies for Oliver's visual review; do not perform
   rendered or browser visual checks unless he explicitly asks.

The endgame fractured-ES case remains the known risk: full closure was stopped
after roughly seven minutes and the new focused path was stopped after roughly
five minutes without a completed report. Do not call it converged. If it still
prevents the S7.6 gate, report that evidence to Oliver before expanding scope or
changing mechanic behavior.

## S7.5 result

- Compact sparse rows no longer store direct entry-state transitions. Ordinary
  self probability and observed-choice self offers are solved algebraically by
  a row-local piecewise fixed point before Bellman comparison. The four final
  measured cases report 6, 15, 237, and 607 eliminated self-loop terms.
- Residual-prioritized backups remain as a bounded fallback. The measured path
  uses Howard-style policy improvement with fixed-policy SCC evaluation:
  algebraic direct loops, dense pivoted solves through 1,024-state components,
  BiCGSTAB above that, improper closed-component detection/repair, and a few
  alternating algebraic Gauss-Seidel seed passes. It converged the final
  one-mod, two-mod, ordinary, and advanced cases in 1/1/5/6 policy rounds.
  The retained `s7.5-dev` prioritized-only report still needed 1,666 rounds on
  the one-mod case, which is why policy iteration is the primary path.
- Sparse rows retain every price-independent equivalent-kernel variant as an
  operator plus expected resource quantities. Repricing selects the cheapest
  representative without rebuilding transitions. A compatible closure is
  retained on the solver calculation context; the Calculator keeps the solver
  handle alive across price-only solves for the same canonical goal and closes
  it when the goal/session changes. Telemetry reports transition-graph reuse.
- Large pending closures can switch to bounded optimistic focused expansion.
  It solves the current partial graph as a lower bound, expands only the
  optimistic policy fringe, and claims exactness only after that policy closes
  and its upper/lower gap is within tolerance. Partial caches are always
  repriced and re-evaluated. This path is implemented and instrumented, but it
  did not make the endgame case complete in the diagnostic window above.
- Compilation groups only state-independent equal-operator/equal-serialized-
  cost programs, caps a region at eight exact states, and keeps primitive
  Unveil plus renewal/protected-repeat/fracture routing state-local. Shared
  continuations use one node/subgraph while the master router retains exact
  per-state predicates. Compile order and node ids are canonicalized from those
  predicates, so discovery-order differences do not perturb native/WASM output.
- Expected-cost annotations use fixed six-decimal presentation precision. The
  annotations are metadata, and this removes harmless last-bit native/WASM
  differences without changing policy evaluation.
- Solver-only benchmark mode now supports `--skip-verification`; it is used for
  intermediate performance work and does not weaken the S7.6 10,000-run gate.
- The advanced compiled policy exceeded Emscripten's 4 MiB stack. Release WASM
  now uses a 16 MiB stack and 32 MiB initial memory; `scripts/build-wasm.ps1`
  also has an opt-in `-Diagnostics` mode for assertions, safe heap, and stack
  checks.

## Final S7.5 performance evidence

Final ignored reports are named
`build/performance/{native-solver,wasm-worker-solver,solver}-s7.5-final-case-<id>-v1.json`.
Every listed native/WASM comparison passed all 83 checks; simulator verification
was intentionally skipped.

| Case | S7.2 native solve | S7.5 native solve | S7.5 WASM solve | Working states -> regions | Nodes / edges | Max worker slice |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `oracle-real-one-mod` | 0.365 ms | 0.149 ms | 20.067 ms | 6 -> 2 | 6 / 11 | 5.487 ms |
| `oracle-real-two-mod` | 33.051 ms | 0.183 ms | 18.902 ms | 15 -> 5 | 9 / 23 | 4.993 ms |
| `ordinary-es-bench` | 688.372 ms | 12.352 ms | 88.978 ms | 135 -> 41 | 45 / 179 | 12.865 ms |
| `advanced-es-resist-bench` | 12,671.034 ms | 55.580 ms | 169.220 ms | 402 -> 133 | 137 / 538 | 18.688 ms |

The applicable large native cases improved by about 56x and 227x versus S7.2.
Solver-owned estimates are 0.30/0.31/1.45/6.72 MB native and
0.21/0.23/1.33/6.57 MB WASM. Persistent transition storage increases the
ordinary and advanced estimates versus S7.2's disposable rows, while the
original S7.0 one-mod baseline fell from 21.2 MB to 0.30 MB. All measured WASM
slices remain below the approved 50 ms budget.

## Implementation notes for S7.6

- Price-only reuse requires the same solver handle, start state, priced
  operator set, and expansion caps. Any structural goal/session/envelope change
  intentionally creates a new solver/cache.
- Equivalent sparse kernels keep all variants; never reduce the retained cache
  to the currently cheapest action or prices can make it stale.
- `focused_expansion`, lower/upper bounds, optimality gap, rounds, and cache
  reuse are in `solver_telemetry_v1`. A focused partial closure is not exact
  merely because policy iteration converged on its current rows.
- Residual-prioritized backup code is intentionally retained for policy-
  evaluation failure. Do not remove its work-unit limits or responsiveness
  telemetry during acceptance cleanup.
- Policy regions are exact matcher sets, not approximate policy trimming.
  Region width eight bounds router fan-in and compile-time condition size.
  Observation-owned options must remain singleton programs.
- Native/WASM abstract-state ids may differ. Compilation must continue using
  canonical predicate order and canonical emitted ids; the final comparisons
  now match strategy byte counts exactly.
- The advanced release WASM stack settings are necessary for recursive parsing
  of the roughly 4.84 MB compiled policy. Do not revert them without replacing
  that recursive stack use.
- The earlier advanced 10,000-run mean was 8.74% below forecast, outside its
  stored 8% tolerance. That correctness investigation is still S7.6 work.

## Verification performed

- No routine native, binding, WASM, web, simulator, or full-repository test
  suite was run. No rendered or visual check was performed.
- The native solver benchmark executable was rebuilt directly with C++20/O2;
  the final four solver-only native reports completed with verification skipped.
- `powershell -File scripts/build-wasm.ps1` completed successfully after the
  engine changes and rebuilt the checked-in worker module. A diagnostic
  safe-heap build identified the original advanced-policy failure as stack
  overflow; the final checked-in build is the normal release configuration.
- Final solver-only worker reports completed for the same four cases, and each
  native/WASM comparison passed 83 checks, including status, state/work counts,
  values, compressed graph counts, and exact strategy byte size.
- Focused unit coverage was added for price-only transition-cache reuse, but
  per the intermediate checkpoint cadence the test binary was not run.
- The endgame full-closure and focused-expansion diagnostic attempts were
  manually stopped without claiming a result; no simulator verification ran.

## Scope that remains parked

S6 Phase 3 ambient Emulator odds was skipped entirely and must not reappear.
Economy E0-E7 is complete except external production activation. Phase 12
accounts, publishing/community, mechanic track M1-M5, Phase 18 recombinators,
and ML remain deferred, blocked, parked, or later as documented.
