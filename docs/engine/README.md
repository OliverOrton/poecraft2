# Engine

**Status: stable engine-area entry point.** This area describes implemented
runtime contracts. It does not own mechanic rulings or future sequencing.

Parent: [Documentation](../README.md)

Verified against code and the mechanical solve split: 2026-07-22 @ `042a281`

## Scope

The engine is one C++20 implementation behind a C ABI. It loads the compiled
game-data artifact, builds an immutable session for one ordinary base and item
level, applies actions to compact item values, compiles and runs strategy
graphs, calculates exact transitions, and solves supported one-item goals.

```text
compiled data
  -> DataImpl
  -> immutable SessionImpl
  -> ActionContextImpl + pc_item_state
  -> sampled action / compiled simulator
  -> exact calculator / strategy evaluator / solver
```

The web product reaches the same implementation through a JSON-shaped WASM
facade running in a worker. Python reaches a subset through the shared library.

## References

- [Data](data.md) — canonical/compiled/runtime data boundaries and session
  construction inputs.
- [Items](items.md) — compact item and modifier-slot representation, lifetime,
  and persistence.
- [Pools](pools.md) — modifier vocabulary, session universes, candidate
  filtering, caches, and introspection.
- [Weights](weights.md) — first-match rows, integer weighting, prefix-sum
  sampling, and action weight contexts.
- [Bitsets](bitsets.md) — implemented dense-mask storage and invariants.
- [WASM](wasm.md) — build configuration, exported surface, worker stepping,
  cancellation, progress, memory, evidence, and unknowns.
- [Engine notes](NOTES.md) — non-authoritative open debt or ideas.
- [Mechanics](../mechanics/README.md) — authoritative implemented action
  behavior and Oliver rulings.

## Public Boundaries

| Header | Ownership |
|---|---|
| `result.h` | Result codes and caller-owned error information |
| `api.h` | ABI version, data loading, summaries, and base enumeration |
| `bitset.h` | C++ helpers over caller-owned dense `uint64_t` words |
| `item_state.h` | Value-copyable item and slot representation |
| `session.h` | Sessions, contexts, actions, pools, and diagnostics |
| `bestiary.h` | Compound Bestiary checkpoint/action/calculation contract |
| `simulator.h` | Strategy graph, economy, chunked simulation, traces, and accounting |
| `solver.h` | Exact calculation, graph evaluation, solve, compile, telemetry, and memory |

The headers are authoritative for exact symbols and struct layouts. Stable
documentation summarizes their contract instead of duplicating every
declaration.

## Implementation Map

| Concern | Primary implementation |
|---|---|
| Data parsing | `engine/src/data_loader.cpp` |
| Session universe, masks, weights, pools | `engine/src/session_builder.cpp` |
| Item helpers | `engine/src/item_state.cpp` |
| One-item actions | `engine/src/actions_basic.cpp` |
| Bestiary | `engine/src/actions_bestiary.cpp`, `bestiary_api.cpp` |
| C ABI facade | `engine/src/api.cpp` |
| Strategy compiler and simulator | `engine/src/simulator.cpp` |
| Solver registry and exact transitions | `engine/src/solver_registry.cpp`, `solver_calc.cpp`, `solver_reforge.cpp`, `solver_options.cpp` |
| Solve lifecycle and optimization | `engine/src/solver_solve_types.hpp`, `solver_solve.cpp`, `solver_solve_expand.cpp`, `solver_solve_bellman.cpp`, `solver_solve_focused.cpp`, `solver_solve_constructive.cpp`, `solver_solve_heuristics.cpp`, `solver_solve_quotient.cpp`, `solver_solve_finish.cpp`, `solver_solve_telemetry.cpp` |
| Policy compilation | `engine/src/solver_compile.cpp` |
| Exact whole-graph evaluation | `engine/src/solver_eval.cpp` |
| Solver C ABI | `engine/src/solver_api.cpp` |
| WASM JSON facade | `bindings/wasm/wasm_api.cpp` |

Shared private structures are declared in `engine/src/engine_internal.hpp` and
the private phase headers indexed by
[Solver internals](../foundation/solver-internals.md). They are implementation
details, not an ABI. `engine/src/solver_internal.hpp` remains a compatibility
umbrella for deliberate cross-phase callers.

## Runtime Model

### Loaded data

`pc_data_handle` owns one immutable compiled dataset. Native loading reads the
three artifact files; memory loading parses their bundled JSON form. The loader
rejects malformed or inconsistent parallel arrays.

### Sessions and action contexts

`pc_session_handle` pins a stable base metadata path and item level. It builds
the complete supported one-item mod universe, dense session IDs, parallel hot
arrays, direct mechanic lookup tables, masks, base-signature weights, and UI
families. Cluster and unsupported-domain bases fail session creation
explicitly.

`pc_action_context_handle` owns all mutable per-worker execution state: RNG,
scratch masks, uncommon influence-signature weight tables, weighted-pool
caches, the most recent selection trace, and optional performance counters.
Sessions are not mutated by action execution.

### Items and actions

`pc_item_state` is a fixed-capacity value. The C ABI applies each action to a
private copy and commits only when it succeeds, so a failed or inapplicable
action leaves the caller's item unchanged. Action selection and mutation remain
native; TypeScript receives result and debug data only.

### Strategy and exact work

Strategy JSON compiles to an engine-owned graph. A simulator owns its private
action context and runs in bounded chunks. Exact graph evaluation and solver
work both provide synchronous C ABI entry points and cooperative stateful
begin/step/finish forms; the browser uses the stateful forms.

The solver owns an action registry, abstract-state context, price-independent
transition data, active work, retained result, optional compiled strategy, and
bounded telemetry. Its exact architecture is documented by the solver area,
not duplicated here.

## Support Boundary

- Ordinary non-cluster bases supported by the artifact can create sessions.
- Cluster records are preserved in canonical and compiled data, but cluster
  session construction returns `PC_RESULT_UNSUPPORTED_FEATURE`.
- Current action state is one-item. Recombinator/two-item state and transfer
  masks are not implemented.
- Item fields can preserve quality, sockets, links, enchantments, and numeric
  slot rolls, but current crafting actions do not implement those mutation
  systems or populate numeric stat rolls.
- Mechanic-specific support by Emulator, Calculator, Strategy Builder,
  Simulator, and Solver is indexed in [Mechanics](../mechanics/README.md).

## Invariants

- C ABI structs that cross an extensible boundary carry `struct_size` and
  `abi_version` where defined by their header.
- The current public ABI is version 2. The bounded-policy summary/progress
  growth intentionally broke ABI v1 because output helpers replace complete
  compile-time structs; consumers must rebuild and must not claim v1 binary
  compatibility.
- Variable-length results use query-required-count or query-required-buffer
  conventions.
- Every created opaque handle has a matching null-safe destroy function.
- Data and sessions are immutable; worker-owned mutable objects are not shared
  concurrently.
- Session mod IDs are dense, session-local, and never persistent identities.
- Group IDs enforce exclusivity; family IDs are display/goal identities and do
  not replace group blocking.
- Ordered source weight rows remain ordered, and all sampling uses engine-owned
  RNG state.
- Crafting mechanics are never reimplemented in TypeScript.
