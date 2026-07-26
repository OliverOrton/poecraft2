# R4 Browser Transfer And Solver Lifetime Report

**Status: completed on 2026-07-26.**

Parent: [R4 archive](README.md)

## Result

All Gates 0–5 passed.

- The WASM facade now exposes native compiled-strategy JSON as raw response
  bytes with explicit result status, size, and clear operations.
- The worker copies those bytes once from `HEAPU8` and transfers the backing
  `ArrayBuffer`; `EngineClient` decodes and parses once on the main thread.
- Strategy compile and exact-evaluation inputs are UTF-8 encoded once on the
  main thread and transferred to the worker.
- `prepareSolverStrategy` adopts the uniquely transferred graph. Full graph
  clones remain only when a separate Strategy Builder or persistence owner is
  created.
- Calculator no longer stores a keyed Solve solver. Each invocation owns a
  fresh scoped solver and closes it with the envelope solver and economy after
  summary, telemetry, and strategy handoff.
- The ordinary odds/picker solver remains independent and long-lived.
- Native compiler defaults remain 100,000 nodes, 400,000 edges, and 64 MiB of
  strategy JSON. Native refusal details remain authoritative.
- The explicit release export inventory now includes response transfer and all
  stepped-solve functions.

The compatibility `pcw_solver_compile` facade remains exported, but the
product path uses `pcw_solver_compile_transfer`.

## Acceptance

The rebuilt release artifact is 2,337,043 bytes with SHA-256
`db1789d432ce2c8fe9b5073835b8b941c2bf7602b1e1ceb8e262b9040e87795e`.

The complete acceptance passed:

- `powershell -File scripts/build-wasm.ps1`;
- `npm test` in `apps/web`, including 27/27 release-WASM worker smoke checks;
- `npx tsc --noEmit` in `apps/web`; and
- focused `engine-smoke.test.ts` lifecycle evidence after the final
  measurement output was added.

The focused release-WASM Node-worker measurement recorded:

| Measurement | Result |
| --- | --- |
| Compiled strategy transfer | 36,224 bytes in 38.35 ms |
| Live handles after scoped close | `5 -> 4` |
| Selected native live bytes after scoped close | `15,434,223 -> 3,752` |
| Selected native peak field after close | `3,752` |
| WASM linear-memory high-water | `278,396,928 -> 278,396,928` |
| Maximum observed solve step | 56.07 ms |

The post-close peak field is reported verbatim; it is an aggregate of the
remaining live facade owners, not a historical process peak. The unchanged
linear-memory high-water is expected and is not live allocation or
browser-process RSS.

Raw watchdog evidence remains locally under
`build/browser-transfer-lifetime-r4/gate4/`. The successful log SHA-256 values
are:

| Check | Log SHA-256 | Watchdog SHA-256 |
| --- | --- | --- |
| Release WASM rebuild | `d959a6612cdf7e081bf8065d8561dbf6f5d7457d19c2d72ea840a1f4d1165096` | `3bfa484bf29323f9ea37f146db9a4a6f0d2a6b685d1bcb9578dec41bc0b88749` |
| Complete web test | `059d937b4486643ad6225159e69e78b8a40c8091853d794356984844ad251f73` | `3ac0c3246a489c576edd53e37910099d3bcb7503cb6ae6911a6e695814b30d40` |
| TypeScript check | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `210cc64113fc5444cefc601802df9d140ee4becaa53d1f8c1efc26b0fdd0974a` |
| Focused worker evidence | `5538b984b6ea19f08910e945a085fa8e3216df4052ef9ef38557b1a9a0189538` | `1c7787d5efcc02ebc438fd2e3c0be3a5d30146a0586d98108968193b536de7a7` |
| Markdown link audit | `50b908207742aaec16ee537537afaab24f184ce18df7a02ea4b003f7f5b8cb49` | `3970c4c235e9a8215398ba96e776ca2635f952efb9a88ca6637349836494fa33` |

The link audit checked 892 local Markdown targets with zero missing.

The first release build failed because the three response accessors were
inside an anonymous namespace and therefore unavailable to the linker export
list. Moving them to public linkage resolved the defect. That failed attempt
timed out nowhere, left no survivor, and is preserved by log SHA-256
`62ff33277063e2f24cd2e92b39644954310c5a449576bb9c3745187af2807f69`
and watchdog SHA-256
`f2450c7e9880b151af3426eab455bf87a76141c09ea04a5179d35c7ac8e1274b`.

Strategy behavior did not change, so the plan did not require a new
10,000-run quality campaign. Existing automated strategy execution stayed in
the complete web suite. Oliver did not authorize rendered review.

## Remaining Boundaries

R4 does not establish real-browser/device throughput, total JavaScript or
process memory, or practical 4 GiB availability. A retained transition-cache
mode remains deferred and would require an enforced product memory budget.
Verification presentation and uncertainty remain R5 work.
