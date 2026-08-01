# Solver Iteration Infrastructure Final Report

**Status: completed enabling milestone.**

Verified 2026-08-01 on `codex/solver-iteration-infrastructure`, starting from
`f1ad2a2ab32d948b11758972aafb198cee9ea483`. The gate commits before final
archival are `7d64cc6`, `25ca6f2`, `c236e50`, `5161524`, and `c042d8a`.

## Outcome

Native iteration now uses a checked-in Release CMake/Ninja preset, parallel
builds, compile commands, focused build/test tasks, and ten non-overlapping
CTest entries. CMake, the explicit g++ fallback, and both WASM paths consume
`engine/engine-sources.txt`. The raw-g++ path remains an explicit warned
fallback.

The largest solver sources and catch-all private header were mechanically
decomposed by responsibility. A stable
[solver internals map](../../foundation/solver-internals.md) explains phase
flow, representations, ownership, edit routing, and determinism/exactness
invariants. The anonymous production policy oracle remains one translation
unit through named `.inc` responsibility fragments so linkage and lifetime did
not change.

## Native Build And Test Measurements

| Measurement | Before | After |
| --- | ---: | ---: |
| Uncached clean parallel all-native build | 22.813 s | 20.383 s |
| Canonical warm-ccache clean all-native build | n/a | 5.988 s |
| Development-wrapper no-op | n/a | 0.398 s |
| Representative implementation edit plus relink | 9.857 s (`solver_refinement.cpp`) | 3.165 s (`solver_eval_report.cpp`) |
| Focused shared refinement | 301/0, 0.316 s | 301/0 |
| Focused policy refinement | 4,829/0, 0.353 s | 4,829/0 |
| Full native executable | 2,997,866/0, 36.480 s | 2,997,866/0, 35.963 s |
| Parallel non-overlapping CTest | n/a | 10/10, 12.47 s real |
| Benchmark validation | 12/0, 0.694 s | 12/0 |

The uncached clean result is only 10.7% lower and may include run variance; it
is not claimed as a major clean-build speedup. The material wins are focused
navigation, one-leaf edit locality, parallel test processes, and branch-return
cache hits.

CTest serial execution measured 35.872 s and parallel execution 12.434 s in
Gate 1, a 65.3% reduction. The final parallel run was 12.47 s. ccache 4.10.2's
deterministic A -> B -> A probe took 15.660 s, 9.697 s, and 0.299 s; the return
to A was a direct hit. The integration is optional and absent-safe.

No PCH, unity build, alternate linker, duplicate CMake/Ninja installation, or
fallback timing rerun was retained because no evidence justified the added
complexity or cost.

## Solver File Map

| Former owner | Result |
| --- | --- |
| `solver_refinement.cpp`, 7,228 lines | 321-line orchestrator; separate observation, features, partition, evaluation, graph core/discovery/routing owners |
| `solver_policy_refinement.cpp`, 7,156 lines | 400-line oracle/lift unit; named oracle responsibility fragments; independent 327-line assertion unit |
| `solver_options.cpp`, 6,152 lines | 1,079-line kernel owner plus automatic, build, import, semantics, and temporary-bench units |
| `solver_eval.cpp`, 6,283 lines | 3,843-line coupled work lifecycle plus independent resolution/report units and 1,506-line helpers |
| `solver_compile.cpp`, 3,521 lines | 2,306-line ordered emission owner plus condition/routing and serialization helpers |
| `solver_internal.hpp`, 2,885 lines | model (1,197), calc (711), eval (300), solve (678), compiler (39), and a five-line compatibility umbrella |

The evaluator work lifecycle and compiler emission function remain above the
general 500-1,500-line guideline because their state/evaluation order is
tightly coupled. They are explicit remaining debt, not arbitrary missed file
splits.

## Header Fan-Out

| Header | Before | After |
| --- | ---: | ---: |
| `solver_internal.hpp` umbrella | 30 | 6 |
| `solver_model.hpp` | n/a | 42 |
| `solver_calc_types.hpp` | n/a | 40 |
| `solver_eval_types.hpp` | n/a | 31 |
| `solver_solve_contracts.hpp` | n/a | 30 |
| `solver_compile_contracts.hpp` | n/a | 9 |
| `solver_solve_types.hpp` | 16 | 23 |
| `solver_refinement.hpp` | 10 | 17 |
| `solver_sparse_policy.hpp` | 10 | 17 |
| `solver_policy_refinement.hpp` | 5 | 6 |

Measured uncached rebuilds were 3.165 s/one TU for a leaf, 12.241 s/17 TUs
for `solver_refinement.hpp`, and 18.510 s/42 TUs for `solver_model.hpp`.
The umbrella reduction and nine-unit compiler boundary improve late-phase
locality, but broad core/refinement fan-out grew with the added translation
units. The next possible seam is goal/registry value types versus abstract
layout/state declarations plus genuine forward declarations; it is deferred
until quotient representation work stabilizes.

## Incremental WASM

Retained. `scripts/dev-wasm.ps1` uses an Emscripten-only CMake/Ninja target,
the canonical engine inventory, and the canonical 61-symbol
`bindings/wasm/wasm-exports.txt`. `scripts/build-wasm.ps1` remains the working
direct-emcc release/diagnostics fallback.

| Operation | Wall time |
| --- | ---: |
| Direct release fallback | 124.885 s |
| Incremental clean | 29.607 s |
| Incremental leaf edit plus relink | 17.367 s |
| Incremental no-op | 0.711 s |

The clean and leaf paths are 76.3% and 86.1% lower than direct. The direct
fallback produced the committed release module. All 61 exports are callable,
ABI remains 2, and the release-WASM worker tests pass. Emscripten 6 required
the standard-equivalent `shared_ptr::use_count() != 1` form because deprecated
`unique()` is absent. MinGW CMake now emits `poecraft_engine.dll` and copies its
thread runtime beside it so Python binding tests do not depend on PATH.

## Behavior And Acceptance

- Public `engine/include/poecraft` headers are unchanged from `f1ad2a2`; C ABI
  version and WASM facade exports are unchanged.
- The required artifact source/data identity and payload hashes remained
  unchanged. `scripts/test.ps1` regenerated the ignored derived manifest, so
  its wall-clock `generated_at_utc` and whole-file SHA changed as designed;
  SQLite was not edited by hand.
- The one-goal belt replay preserved transition/policy hashes
  `ce5a144282753b26` / `6bee45662f66d2e4` exactly.
- The prohibited 467-second two-goal case was not rerun. Its tracked
  `4f26d305a908c59f` / `3e384d3eb52f9ab7` evidence and work counters are
  unchanged.
- The tracked qualified Fracture evidence file remains SHA-256
  `33d75eca0d6e231d27f04de7de0ae06a83b8f0e96525328598478d38be2435ef`,
  retaining transition/policy hashes `04a66ba6c6dfcabf` /
  `3e5d7530e7aed5fb`, work counters, and compiled strategy SHA-256
  `e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`.
  A diagnostic attempt to run that old qualification fixture through today's
  later action envelope reached the known sweep-cap path and was rejected as a
  parity oracle; it did not replace the frozen evidence.
- Native focused suites, the 10-entry CTest partition, the monolithic native
  suite, benchmark validation, `npm test`, and `npx tsc --noEmit` passed.
- One complete `scripts/test.ps1` run passed in 66.524 s. An earlier attempt
  stopped after 15 Python tests because the CMake DLL was not loadable; the
  packaging repair above was applied before the complete acceptance run.
- Compiled-strategy simulator checks used 10,000 runs where verification was
  requested. The replay timing probe also used its fixture's 10,000-run gate.
- No rendered browser, screenshot, or visual review was performed.

## Replay/Checkpoint Decision

Replay is intentionally not implemented. The proposed future boundary is
after deterministic graph closure and before policy optimization/refinement.

| Probe | Pre-boundary graph work | Post-boundary measured work | 20% load/validation ceiling |
| --- | ---: | ---: | ---: |
| Completed two-mod oracle | 1.397 ms | about 310 ms for extraction, compilation, and exact evaluation (excluding 10,000-run simulation) | 0.279 ms |
| Current 927-state full-four envelope | 1,233.107 ms | about 246,075 ms before its known sweep-cap stop | 246.621 ms |

The easy case cannot amortize a practical replay loader. The hard case shows
large potential benefit for later Bellman, repair, compiler, evaluator, and
reliability iteration, but the next quotient milestone changes carrier
identity, graph discovery/closure, transition projection, and proof inputs
before the checkpoint. Action/option authority, abstraction, and graph
construction changes must invalidate captures; Bellman/policy improvement,
repair/lifting, compilation, evaluation, reporting, and reliability work occur
afterward. The correct sequence remains: stabilize proof-carrying quotient
representation, then design a versioned development-only format with strong
artifact/config/compiler/source identity, corruption/collision checks,
fresh-versus-replay parity, conservative invalidation, and no product proof
authority or committed large captures.

## Handoff

Proof-carrying quotient refinement is restored unchanged as the next
implementation milestone. The enabling work does not implement, supersede, or
archive that algorithmic plan. Nothing was pushed or merged.
