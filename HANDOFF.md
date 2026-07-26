# Session Handoff

**Status: no active implementation boundary.** Oliver must select the next
chunk before implementation resumes.

## Main State

Local `main` contains the completed gap-directed, exact automatic-action, and
broad-action research lineage plus
[R4 browser transfer and solver lifetime](docs/archive/2026-07-26-browser-transfer-lifetime-r4/README.md).
The three research milestones contributed documentation and evidence only; no
diagnostic prototype, engine, binding, web, data, fixture, or script change
from those experiments was retained. R4 is the subsequent product/runtime
implementation.

Commits remain local-only. Nothing was pushed.

## Latest Completed Result

R4 replaced the Calculator's solve-to-strategy nested JSON/copy chain with raw
transferable UTF-8 bytes:

1. native compilation writes the ordinary v1 strategy JSON to the facade
   response without escaping it inside a second JSON document;
2. the worker copies once from WASM memory, clears the native response, and
   transfers the backing buffer;
3. `EngineClient` decodes and parses once on the main thread;
4. strategy compile/evaluation inputs use the same transferable-byte
   ownership model; and
5. uniquely transferred graphs are adopted, with clones retained only for
   separate document or persistence owners.

Calculator now opens a fresh scoped solver for each Solve. It obtains summary,
telemetry, and compiled strategy, then closes that solver, the envelope solver,
and economy in terminal cleanup. Later solves and repricing rebuild. The
ordinary odds/picker solver remains independent and long-lived.

The release export inventory now includes raw response transfer and all stepped
solve functions. Native compiler limits remain 100,000 nodes, 400,000 edges,
and 64 MiB strategy JSON. Solver behavior, mechanics, action scope, C ABI,
Python bindings, public caps, and strategy vocabulary did not change.

## Acceptance

The rebuilt release artifact is 2,337,043 bytes with SHA-256
`db1789d432ce2c8fe9b5073835b8b941c2bf7602b1e1ceb8e262b9040e87795e`.

- `powershell -File scripts/build-wasm.ps1` passed.
- `npm test` in `apps/web` passed, including 27/27 release-WASM worker smoke
  checks.
- `npx tsc --noEmit` in `apps/web` passed.
- Focused real-worker evidence transferred 36,224 strategy bytes in 38.35 ms.
  Closing the scoped solver changed handles `5 -> 4` and selected native live
  bytes `15,434,223 -> 3,752`; WASM high-water correctly stayed
  `278,396,928 -> 278,396,928`. Maximum observed solve step was 56.07 ms.
- No process survived any watchdog. The first build exposed and then fixed an
  anonymous-namespace export-linkage error.

This is non-visual release-WASM Node-worker evidence. It does not establish
browser/device throughput, process RSS, or total JavaScript memory. Strategy
behavior was unchanged, so the plan did not require a new 10,000-run quality
campaign. Oliver did not authorize rendered review.

Full commands, hashes, and limitations are in the
[R4 report](docs/archive/2026-07-26-browser-transfer-lifetime-r4/report.md).

## What Can Be Selected Next

The clearest bounded product continuation is R5 verification presentation:
truthful terminal/off-policy gates, sampled uncertainty, and remaining
authored-Unveil evaluator boundaries. It is deferred and unselected in the
[solver roadmap](docs/future/solver-roadmap.md).

The hard natural-T1 solver research has no accepted production architecture.
Its latest broad-action result says not to resume by landing either rejected
prototype, raising caps, or treating the fixed-policy renewal evaluator as a
general Bellman kernel. A continuation needs either a new exact broad-kernel
representation preserving continuation observations or a materially narrower
product objective.

Commits must remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
