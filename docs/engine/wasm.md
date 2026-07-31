# WebAssembly runtime

**Status: implemented runtime and integration reference.** This page records
the current WASM surface, product usage, long-work protocol, memory model, and
evidence boundaries.

Parent: [Engine](README.md)

Verified against code and rebuilt release module: 2026-07-31 on
`codex/policy-guided-exact-refinement`. Complete non-visual web acceptance is
pending under the active handoff.

Release-wrapper export map verified in the tracked
`bindings/wasm/dist/poecraft_engine.mjs` generated at this boundary. The
tracked `.wasm` SHA-256 is
`52b41eca276d679135ac22c99f5694c64daa7091337c0730c55109ff17db7e08`.

## Architecture

The WASM build compiles the same C++20 engine sources as the native library
plus `bindings/wasm/wasm_api.cpp`. That facade exports a handle-and-JSON API
named `pcw_*`; TypeScript does not call the lower-level `pc_*` C ABI directly.

The web app creates one module inside `engine-worker.ts`. `EngineClient`
communicates with the worker through request IDs, ordinary structured-clone
messages for small values, and transferable `ArrayBuffer` payloads for
strategy documents. The process-level engine service opens one compiled data
bundle and shares that worker/client within the browser tab. The C++ engine
itself is single-threaded; isolation from the UI thread comes from the Web
Worker.

The worker retains native handles for data, sessions, contexts, items,
compiled strategies, economies, simulators, and solvers. TypeScript wrappers
must close scoped handles; the long-lived data/module lifetime currently ends
with the worker/tab rather than an ordinary product `data_close` path.

## Build and release artifact

`scripts/build-wasm.ps1` activates Emscripten from `$env:EMSDK` or `C:\emsdk`,
regenerates the Harvest allowlist header, and compiles every `engine/src/*.cpp`
file plus the facade. The release flags include:

- C++20, `-O3`, disabled floating-point contraction, and C++ exceptions;
- modularized ES module output named `createPoecraftEngine` for
  web/worker/node;
- growing linear memory, 128 MiB initial memory, a 4 GiB configured maximum,
  and a 64 MiB stack;
- a non-exiting runtime and exported `ccall`, `cwrap`, `UTF8ToString`, and
  `HEAPU8` helpers.

`-sPTHREADS` is not present. The tracked release outputs are
`bindings/wasm/dist/poecraft_engine.mjs` and `.wasm`. A diagnostics build adds
Emscripten assertions, safe-heap checks, and stack-overflow checks, but those
are not release behavior.

Vite treats the worker as ES2022 and suppresses Node `process` detection for
the production worker loader. The generated module still supports Node so the
same client/worker integration can run under `worker_threads` tests.

## Implemented exported capability

The tracked release module exports these facade groups:

| Group | Exported capability |
| --- | --- |
| Data | ABI version, open/summary/base catalog, Bestiary presentation, close |
| Session/context | Open/close, mod count/info, seeded action context |
| Item | Create/clone/close/info, add/remove/fracture, stable-key export/import |
| Craft actions | Apply one, run a batch, pool debug, Bestiary apply/calculate |
| Strategy evaluation | Compile/close, synchronous evaluate, and stepped begin/step/finish/close |
| Economy/simulation | Economy open/close; simulator open/close/chunk/result |
| Solver | Open/close, enumerate actions, calculator odds, synchronous solve, stepped solve begin/step/finish/abandon, state value, projection, compile, raw compiled-strategy transfer, log, telemetry |
| Response transfer | Result status, raw response-data pointer/size, and explicit response clear |
| Diagnostics | Live-handle count and memory statistics |
| Emscripten plumbing | `malloc`, `free`, and the runtime helpers listed above |

This is an implemented capability inventory, not a claim that every export is
used by the current UI.

## Product paths and exported-but-unused paths

The current product uses the JSON facade for catalog/session/item operations,
craft actions, pool inspection, Bestiary, compiled-strategy simulation,
calculator odds, economy handles, stepped exact strategy evaluation, and the
stepped strategy solver. Long-running product operations use the stepped
forms so the worker can report progress and process cancellation.

Strategy compilation and exact evaluation inputs are UTF-8 encoded on the
main thread and their backing buffers are transferred to the worker. Solver
policy compilation uses `pcw_solver_compile_transfer`: native writes the
ordinary v1 strategy JSON into the facade response string without nesting it
inside another escaped JSON document. The worker slices the raw response bytes
from `HEAPU8`, clears the response string, and transfers the byte buffer to the
main thread, where `EngineClient` decodes and parses once. Native errors keep
their result code and JSON detail.

These exports are implemented and bound in `engine-wasm.ts` but have no normal
current product call site, or exist chiefly for tests/diagnostics:

- `pcw_item_clone` (a plain item-state clone; it does not copy the facade's
  Bestiary compound checkpoint);
- `pcw_run_batch`;
- synchronous `pcw_strategy_evaluate` and synchronous `pcw_solver_solve`;
- compatibility `pcw_solver_compile` (the product uses the raw transfer form);
- `pcw_solver_state_value`, `pcw_solver_project`, and `pcw_solver_log`;
- direct `pcw_live_handle_count` and `pcw_memory_stats` calls;
- ordinary `pcw_data_close` during a tab's service lifetime.

An unused export is not an unimplemented API. It remains part of the compiled
surface until deliberately removed.

## Native capability not surfaced directly

The WASM facade intentionally does not mirror every low-level native entry
point. Native-only or facade-aggregated capability includes:

- loading an artifact from filesystem paths and the explicit native capacity
  report;
- raw dumps of effective tags, group memberships, masks, influence/reach
  details, and per-row weight internals beyond the JSON summaries;
- direct RNG reseed/next calls, performance timing counters, and the last
  action trace;
- granular simulator and per-solver/evaluator memory queries (the facade
  returns aggregate JSON instead).

The high-level Python binding also does not currently wrap the solver C ABI.
That is a binding boundary, not absence of the native implementation.

## Stepped work, progress, and cancellation

Cancellation is cooperative. An `AbortSignal` posts a cancel request to the
worker, but the worker can inspect it only after the current native step
returns and the event loop runs. There is no preemptive cancellation inside a
C++ step.

### Strategy solver

The worker calls `pcw_solver_solve_begin`, repeatedly calls `step`, then calls
`finish` only after native progress reports done. It starts with the requested
chunk size or 8 work items, adapts toward roughly 12 ms steps, and clamps later
chunks to 1–4 work items; a phase change resets the chunk to 1. It yields after
about 8 ms of accumulated unyielded work or when `yieldEveryStep` is requested.
Progress covers expanding, iterating, and done phases and is throttled to
roughly 100 ms apart except for first, phase-change, and final updates. ABI v2
progress also carries live lower/upper and absolute/relative gap values,
focused round, incumbent kind, discovered/expanded/frontier state counts,
rows/transitions/reforge work, and live/peak selected owned bytes. The latter
fields are observational reporting only. Non-finite unavailable values cross
the JSON facade as `null`.

Every bounded solve step obtains those progress bytes from the conservative
incremental selected-owned-byte ledger. Full selected-allocation walks remain
at audit/cap checkpoints, telemetry snapshots, finalization, and explicit
memory-statistics requests. Progress can therefore conservatively overestimate
the audited live selection, but it must never undercount it. This avoids
repeating a whole-graph accounting walk after each 1–4-item worker step without
changing cap enforcement or final accounting.

On cancellation the worker returns a cancelled result with its latest
progress/worker telemetry and calls `pcw_solver_solve_abandon` in cleanup.
Abandon resets the native in-progress solve while retaining bounded abandoned
telemetry for diagnosis.

The completed ABI v2 summary keeps `converged` as the exact-closure flag and
separately reports policy availability/status, termination, `L`, `U`, exact
returned-policy cost, gaps, requested targets, and the firing target. The
facade serializes unavailable numeric claims as `null`. Gap options are
forwarded to native solve and are product stopping targets only.

### Exact strategy evaluation

The evaluator calls the stateful begin/step/finish API. It begins at the
requested chunk size or 16, adapts toward roughly 16 ms, and may grow to
16,384 work items. Discovery is chunked; solving advances one strongly
connected component at a time; fallback and finalization are also surfaced as
phases. The worker yields after each native step. Cancellation closes the
evaluation and its scoped economy/strategy handles in `finally` cleanup.

### Monte Carlo simulation

The simulator runs chunks of 1,000 by default and adapts between 1 and 10,000
runs toward roughly 16 ms. It yields between chunks and can return a partial
cancelled result.

These pacing values are current scheduling policy, not performance guarantees.

## Resource defaults and memory reporting

When product code omits native overrides, exact evaluation defaults include
100,000 states and sweeps, 1,000,000 state/node pairs, 10,000,000 transitions,
16 top classes per node, a 512 MiB selected-owned-allocation cap, and a 64 MiB
output-JSON cap. Strategy solve defaults include 200,000-state/search limits,
1,215,000 state/action rows, 10,000,000 transitions, 50,000,000 reforge work,
a 1 GiB selected-solver-owned cap, 100,000 compiled nodes, 400,000 compiled
edges, a 64 MiB strategy-JSON cap, and a 1 MiB telemetry-JSON cap. Only the
state/search and row limits retain the Q4 scaling values. Oliver raised the
default reforge-work allowance from 11,000,000 to 20,000,000 on 2026-07-30,
then to 50,000,000 on 2026-07-31. The transition, memory, compiler, and JSON
caps did not change.

Those native caps cover selected allocations accounted by the evaluator or
solver. They are not limits on the entire WASM heap, Emscripten stack, facade
registries, response-string capacity, parsed data, TypeScript objects,
structured-clone messages, or JSON copies.

`max_solver_owned_bytes` is already an optional per-solve native and WASM
override. Its 1 GiB default can be raised independently, but the value accounts
only selected native solver allocations; it does not reserve or cap the whole
WASM heap. Browser headroom must also cover compiled data, facade state,
response strings, JavaScript objects, and transient copies.

`pcw_memory_stats` estimates facade handle registries, the reusable response
string, and selected solver/evaluator-owned allocations. TypeScript augments
it with `HEAPU8.byteLength` as `wasm_memory_bytes`. That byte length is the
current linear-memory high-water size: Emscripten memory can grow and is not
shrunk when handles close. It is not live allocation or browser-process RSS.

The raw compiled-strategy path clears the facade response string after the
worker has copied its bytes, releasing that retained string capacity for C++
reuse. Closing the scoped product solver also releases its selected-owned
transition closure. Neither operation promises to shrink the already-grown
WASM linear memory.

The facade copies large economy JSON through an explicit heap allocation;
ordinary JSON calls use Emscripten string marshalling and the configured
stack. The worker drops the raw artifact bytes after building the web catalog,
but its JavaScript catalog and transient copies are outside native memory
statistics.

## Evidence in the repository

Evidence must be read at the layer it actually exercises:

- Native CTest sources and benchmarks exercise the C ABI, engine algorithms,
  fixture parity, and native performance. Native timing cannot be assumed to
  equal browser WASM timing.
- `apps/web/test/engine-smoke.test.ts` loads the compiled release WASM in a
  Node `worker_threads` worker through the same `EngineClient`. Its cases cover
  the memory-safe large-payload path, pool fixture parity, stateful exact
  progress, solve progress, cancellation cleanup/fallback, compilation, and
  simulation.
- The 2026-07-26
  [R4 transfer/lifetime evidence](../evidence.md#browser-transfer-and-solver-lifetime-r4)
  records a 36,224-byte raw compiled strategy, handle count `5 -> 4`,
  selected native live bytes `15,434,223 -> 3,752`, unchanged
  `278,396,928`-byte WASM high-water, and a 56.07 ms maximum solve step in the
  release-WASM Node worker. It is ownership evidence, not browser-process RSS.
- The [solver-scaling v1 evidence](../../fixtures/solver-scaling/v1/README.md)
  preserves the Q5 non-visual Node/Worker baseline and the rebuilt-module
  action/state-pruning follow-up for the accepted three-slot product. The
  follow-up closes solve, compile, and 10,000-run verification in 270 ms with
  two states, one row, a 19.55 ms maximum Worker slice, and no growth from the
  278,396,928-byte starting WASM heap.
- The
  [progress-accounting acceptance](../../fixtures/solver-scaling/v1/evidence/wasm-progress-accounting-fix-summary.json)
  replays the previously pathological one-goal/4,000-state solve with the real
  four-item cadence. Fresh accepted source reduced solve wall from 257.212 to
  7.994 seconds (32.17x; 96.89% removed) while preserving bounds, status,
  states, live/peak accounting, and transition/policy hashes. The rebuilt
  binary exactly matches the earlier 4.468-second measurement overlay; the
  timing difference is run variance. This is headless Node-WASM evidence, not
  browser/device timing proof.
- The tracked release wrapper itself is evidence of the module's current
  assignment/export map.

The Q0-Q5 and action/state-pruning acceptances rebuilt the module, ran the
complete Node worker suite, and emitted product-scale reports. This is real
WASM integration evidence, but it is not a real-browser/device benchmark.

## Export inventory

The source facade marks all public functions `EMSCRIPTEN_KEEPALIVE`. The
explicit `$Exported` array in `scripts/build-wasm.ps1` and the tracked release
module include the four stepped solver functions:

```text
pcw_solver_solve_begin
pcw_solver_solve_step
pcw_solver_solve_finish
pcw_solver_solve_abandon
```

They also include `pcw_response_data`, `pcw_response_size`,
`pcw_response_clear`, and `pcw_solver_compile_transfer`. The build inventory
and product-required public surface were synchronized in R4; the tracked
wrapper remains the release check.

## Open evidence and product unknowns

The repository does not yet establish:

- browser/device throughput, peak memory, or worst-step cancellation latency
  for difficult exact evaluations and solves;
- practical 4 GiB memory availability across supported browsers/devices, or
  full memory including JavaScript catalogs and other transient copies;
- whether released C++ allocations return reusable heap space in each hard
  workload, beyond the fact that the linear-memory high-water does not shrink;
- an enforced product-wide live-memory budget or budgeted retained-cache mode;
- performance equivalence between native benchmarks, Node WASM workers, and
  browsers.

These are unknowns to measure or decide, not implemented guarantees.

Implementation entry points: `scripts/build-wasm.ps1`,
`bindings/wasm/wasm_api.cpp`, `apps/web/src/app/engine-wasm.ts`,
`apps/web/src/app/engine-worker.ts`, `apps/web/src/app/engine-client.ts`, and
`apps/web/test/engine-smoke.test.ts`.
