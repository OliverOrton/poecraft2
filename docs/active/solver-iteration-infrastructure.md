# Solver Iteration Infrastructure And Decomposition

**Status: Gate 1 complete; Gate 2 mechanical decomposition is next.**

Owner: Oliver

Branch: `codex/solver-iteration-infrastructure`

Starting commit: `f1ad2a2ab32d948b11758972aafb198cee9ea483`

Parent: [Active work](README.md)

Queued algorithmic milestone:
[Proof-Carrying Quotient Refinement During Solving](proof-carrying-quotient-refinement.md).
That plan remains unchanged and resumes immediately after this enabling work.

## Objective

Make native solver iteration fast, focused, and understandable without changing
solver behavior. Establish the already-demonstrated CMake/Ninja path, give each
build and test workflow one obvious command, consolidate engine translation-unit
ownership, split the largest solver sources/private contracts by responsibility,
document the execution flow, and measure whether ccache or incremental WASM
compilation earns its maintenance cost.

The primary decomposition goal is edit comprehension and locality. Future work
should be able to open the graph, abstraction, policy, compiler, evaluator, or
option-building owner without loading several 6,000-7,000-line files and a
2,885-line catch-all private header.

## Frozen non-goals

This milestone does not change crafting mechanics, solver mathematics, action
filtering, state abstraction, resource caps, policy semantics, public C ABI,
strategy vocabulary, frontend behavior, memory limits, floating-point flags, or
deterministic ordering. Function/type movement is mechanical; algorithms are not
redesigned while files move.

Checkpoint/replay is intentionally deferred. The proof-carrying quotient work
changes graph construction and internal representation, so a format designed
before that representation stabilizes would be immediately invalidated or would
constrain the new design around a temporary structure.

## Gate 0 frozen boundary

Verified on 2026-08-01 before tracked implementation edits:

| Field | Frozen value |
| --- | --- |
| Requested starting commit | `f1ad2a2ab32d948b11758972aafb198cee9ea483` |
| Starting worktree | clean |
| Implementation branch | `codex/solver-iteration-infrastructure` |
| CMake | `3.29.5-msvc4` at the installed Visual Studio path |
| Ninja | `1.12.1` at the installed Visual Studio path |
| Compiler | MSYS2 UCRT64 GCC `14.2.0` |
| ccache | absent before the authorized Gate 1 experiment |
| Compiled artifact manifest SHA-256 | `ba1feb596d6f07903dfa992d4ba42184bee30c18660ab56c84dad1c5cadc8a27` |
| Existing focused test executable SHA-256 | `4969f073b37331f94005b0d0b29052863cd3ec441ce55ae80d7ae55defb4e996` |
| Existing benchmark executable SHA-256 | `808d3521ce5ed2b55ef03e8e55afc9aab862e496ead11940ad614894b10e677d` |

The current focused refinement suites were rerun from those frozen binaries:

| Suite | Checks | Failures | Wall time |
| --- | ---: | ---: | ---: |
| shared refinement | 301 | 0 | 0.316 s |
| policy refinement/lifting | 4,829 | 0 | 0.353 s |
| full native engine executable | 2,997,866 | 0 | 36.480 s |
| benchmark manifest validation | 12 specs | 0 | 0.694 s |

Current behavior evidence is frozen from the existing reports; the prohibited
long two-goal case was not rerun:

- `natural-t1-breadth-two-4e65dda9c53b`: transition hash
  `4f26d305a908c59f`, policy hash `3e384d3eb52f9ab7`, 171 discovered/expanded
  coarse states, 7,107 rows, 4,292 sparse transitions, 1,061,856 outcomes,
  106,722 Bellman backups, 183,062 retained exact carriers, 423,756 exact
  transitions, and a `refused_resource_cap` result before partition
  initialization at 1,089,111,449 bytes. The report SHA-256 is
  `cf3feb1a36aeb559bb5f4bdc8b0a160639d3310e3a7f4377218365eb85734ba6`.
- Current one-goal belt witness: transition hash `ce5a144282753b26`, policy
  hash `6bee45662f66d2e4`, 51 discovered/expanded states, 505 rows, 243 sparse
  transitions, 7,458 outcomes, and 624 Bellman backups.
- Qualified Fracture control: six parent junk classes, 217 root Chaos
  successors, 927 discovered/expanded states, 319 policy-reachable states,
  8,015 rows, 16,899 transitions, 344,040 outcomes, 3,407,700 reforge work,
  zero completed-row recomputation, transition hash `04a66ba6c6dfcabf`, policy
  hash `3e5d7530e7aed5fb`, and compiled strategy SHA-256
  `e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`.

The historical `build/incremental-engine/.ninja_log` records approximately
16.15-18.68 second parallel builds. A fresh separate Ninja baseline, configured
with the required toolchain and `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, measured:

| Operation | Baseline |
| --- | ---: |
| Clean parallel native build, all targets | 22.813 s |
| No-op engine-library build | 0.071 s |
| Touch `solver_refinement.cpp`, rebuild TU and relink static engine | 9.857 s |

The raw-g++ fallback was not timed. `build/engine/candidate-build.out.log`
contains the historical successful fallback completion, and the selected work
accepts the established roughly seven-minute history without spending the one
allowed confirmation run.

### Starting source and header map

| File | Lines | Current mixed responsibilities |
| --- | ---: | --- |
| `solver_refinement.cpp` | 7,228 | observation signatures, graph closure, closed partitioning, validation/repair, exact evaluation |
| `solver_policy_refinement.cpp` | 7,156 | policy adapter, strict graph construction, candidate selection, lifting, exact policy publication |
| `solver_options.cpp` | 6,152 | primitive/automatic option construction, filtering, recipes, candidate synthesis |
| `solver_eval.cpp` | 6,283 | strategy parsing, graph construction, occupancy/influence, exact evaluation, reporting/work lifecycle |
| `solver_compile.cpp` | 3,521 | compiler vocabulary, quotient routing, graph emission and validation |
| `solver_internal.hpp` | 2,885 | registry, refinement, abstract state, calculation, evaluator, solve, diagnostics, compiler contracts |

Ninja dependency information gives this starting header fan-out:

| Header | Dependent translation units |
| --- | ---: |
| `solver_internal.hpp` | 30 |
| `solver_solve_types.hpp` | 16 |
| `solver_refinement.hpp` | 10 |
| `solver_sparse_policy.hpp` | 10 |
| `solver_policy_refinement.hpp` | 5 |

The full target lists are reproducible from
`build/solver-iteration-infrastructure/baseline/.ninja_deps`; final evidence
will record the after counts and representative measured rebuilds.

## Gate 1 - canonical fast native workflow

1. Check in a reproducible Ninja/GCC Release preset with compile commands.
2. Establish one checked-in engine source inventory consumed by CMake, the
   PowerShell fallback, and WASM.
3. Add `scripts/dev-engine.ps1` with configure, engine-only, tests-only,
   benchmark-only, selected-suite, all-native, benchmark-validation,
   rerun-failed, and explicit clean-rebuild workflows. Parallel Ninja is the
   default.
4. Make `scripts/build.ps1` discover the installed Visual Studio CMake/Ninja
   paths when absent from `PATH`, and emit a prominent warning before the
   direct-g++ all-source fallback.
5. Register non-overlapping CTest suites with focused solver labels. Keep a
   convenient explicit full invocation without making default parallel CTest
   redundantly execute the entire collection.
6. Measure no-op, representative one-TU plus relink, clean parallel build,
   focused suite, full native suite, benchmark validation, and CTest parallel
   benefit. Qualification limits are 2 s, 30 s, and 60 s respectively for the
   first three build measurements.
7. If absent, install only MSYS2 UCRT64 ccache. Measure deterministic A -> B ->
   A rebuild wall time and hit rate with the exact Release flags including
   `-ffp-contract=off`. Retain integration only if reliable and materially
   useful; otherwise record the negative result.

Commit this gate independently.

### Gate 1 result

Completed on 2026-08-01:

- `engine/engine-sources.txt` is the single translation-unit inventory for
  CMake, the native fallback, and WASM. Both CMake and PowerShell reject drift.
- `engine/CMakePresets.json` pins Ninja, UCRT64 GCC, Release optimization,
  compile commands, and the established `build/engine` output directory.
- `scripts/dev-engine.ps1` implements every requested focused workflow;
  `scripts/build.ps1` discovers the installed tools outside `PATH` and warns
  prominently before fallback.
- CTest now partitions the prior monolithic invocation into ten non-overlapping
  native entries (header smoke, core, and eight solver owners). Serial execution
  took 35.872 s; parallel execution took 12.434 s, a 65.3% wall reduction. The
  focused policy/refinement subsets remain direct modes and are not duplicated
  in the all-native set.
- The canonical wrapper no-op engine build took 0.393 s including PowerShell
  startup and Ninja's inventory check. The fresh pre-change all-target build
  took 22.813 s and the engine-only build after infrastructure configuration
  took 14.223 s, both below the 60 s qualification limit.
- MSYS2 package `mingw-w64-ucrt-x86_64-ccache` 4.10.2 was installed under the
  prompt's explicit authorization. With the exact Release command containing
  `-O3 -DNDEBUG -ffp-contract=off`, the deterministic A -> B -> A probe measured
  15.660 s initial cache population, 9.697 s for the B leaf edit/relink, and
  0.299 s returning to A. The probe was 100% cacheable; its final A lookup was a
  direct hit. The A/B/A probe hit rate was 1/3, while the full initial population
  explains the displayed cumulative 1/34 rate. Optional integration is retained
  because it materially improves branch-return builds and is absent-safe.

No PCH, unity build, alternate linker, fallback timing run, solver behavior, or
public contract change was introduced.

## Gate 2 - mechanical solver subsystem decomposition

Inventory types, functions, shared state, and includes, then move existing
definitions into descriptive owners derived from their real responsibilities.
Target coherent files are generally 500-1,500 lines, without one-function
files or separation of tightly coupled logic merely to meet a line target.

Expected ownership seams include exact graph construction/closure, carrier
canonicalization, observations/projections, closed partition construction,
validation/repair, policy graph/adapters, candidate scheduling, exact policy
evaluation/properness, compilation, evaluator graph construction and
occupancy/influence, telemetry/reporting, and option/action-family synthesis.

Split the private catch-all into stable core, registry/option, graph/refinement,
evaluation, solve/telemetry, and compiler contracts where dependencies permit.
A compatibility umbrella may be used during motion, but final leaf translation
units must not all acquire the whole original header under another name.

Preserve ordering, visibility, linkage, ownership/lifetimes, evaluation order,
and floating-point flags exactly. Narrow compile checks are allowed while
moving; routine suites wait for final acceptance.

Create a stable foundation architecture map covering phases, representations,
file ownership, edit routing, and exactness/determinism invariants, and index it
from `docs/README.md`. Commit the mechanical decomposition separately from
build tooling.

## Gate 3 - build impact audit

Using compile commands, Ninja dependency data, and measured rebuilds, compare
before/after fan-out for a leaf implementation edit, a refinement-specific
header, and the broad shared internal header. Report navigability, leaf edit
locality, clean-build parallelism, and header fan-out honestly. If the broad
header remains broad, name the next narrow seam without beginning another
restructure.

## Gate 4 - incremental WASM investigation

Investigate an object-based or CMake/Ninja incremental Emscripten build using
the canonical source inventory. Retain it only if it produces the existing
module pair, preserves exports/runtime behavior, keeps direct emcc as a working
fallback, and materially improves measured incremental builds without changing
memory/ABI/behavior. Otherwise record a precise negative result. Any retained
WASM change receives its own commit.

## Replay/checkpoint evidence only

Do not implement replay. At the final boundary, record graph-construction time,
post-boundary time, expected future pre/post-boundary change classes, an
estimated load/validation budget, and which downstream compiler/repair/eval
work would benefit. Future replay remains development-only, deterministic,
versioned, strongly identified, conservatively invalidated, corruption checked,
parity tested, at most 20% of fresh construction cost, and never product proof
authority. No large captures are committed.

## Final acceptance and closure

Run once, after the complete implementation:

1. clean CMake/Ninja native build and measured no-op/incremental rebuilds;
2. focused solver suites, all non-overlapping CTest suites, full native engine
   invocation, and benchmark validation;
3. release WASM plus relevant web tests because source inventory changes;
4. `npm test` and `npx tsc --noEmit` in `apps/web`;
5. `powershell -File scripts/test.ps1` once; and
6. exact comparison of frozen hashes, outcomes, bounds, work counters,
   compiled behavior, C ABI/WASM exports, and benchmark results.

Any compiled-strategy verification uses 10,000 simulator runs. No rendered or
visual review is performed.

On success, extract durable architecture/build facts, archive this plan in a
dated folder with the final report, restore proof-carrying quotient refinement
as the current implementation milestone, update `HANDOFF.md`, and leave a clean
local branch. Do not push or merge.
