# Session Handoff - S7.3 Complete Out Of Sequence, S7.2 Next

Updated 2026-07-15 after the S7.3 fixed solver-option checkpoint. Read
[AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

Oliver explicitly selected S7.3 while the branch was still at S7.1. S7.3 is
therefore complete ahead of S7.2. S7.2 has not been implemented, and S7.4 has
not begun.

## Exact next boundary

Implement **S7.2 only: action control, storage, and the first performance
pass**.

1. Before changing solver performance, capture fresh unoptimized native and
   worker/WASM measurements for the approved T1/T1 oracle and the newly enabled
   ordinary, advanced, and endgame cases. Preserve every S7.0 report and its
   historical any-tier oracle row.
2. Build the goal-relevance/dependency analyzer and complete action-inclusion
   diagnostics. Keep uncertain actions deferred unless a proof permits pruning.
3. Generate relevant fossil loadouts lazily and collapse only certified
   identical abstract transition kernels, preserving price ties and reasons.
4. Add independent discovered/expanded-state, row, transition, reforge-work,
   solver-byte, and compiled-graph caps using the approved per-case values.
5. Store evaluated sparse transition rows once, compact the state payloads, and
   split Bellman work into bounded units that preserve the 50 ms worker-step and
   250 ms cancellation budgets.

Do not reimplement S7.3 while doing S7.2. Stop before S7.4; renewal/repeat
options, protected retry loops, observation-aware Unveil options, and fracture
retry policies remain S7.4 work. Do not run routine native, binding, WASM, web,
simulator, full-repository, or visual-browser gates at the intermediate
checkpoint. Oliver owns visual review.

## S7.3 result

- Added an explicit solver-goal `options` contract with four fixed definitions:
  `scour_alchemy`, `eldritch_side_intent`, `protected_side`, and
  `multimod_finish`. An explicit `actions: []` now means options-only; omitting
  `actions` still selects the full primitive registry.
- Added a solver-only planner registry whose primitive wrappers keep their
  action-registry indices and whose fixed options are appended. Selected policy
  entries retain an explicit primitive-or-option tag.
- Added price-independent fixed option kernels cached by option definition and
  entry abstract state. Every kernel reports legality, evaluator support,
  almost-sure termination, exact primitive action count, exact resource
  quantities, and every outer exit state with probability.
- Scour/Alchemy is legal only when the exact post-Scour state permits Alchemy;
  preserved fractures or locks therefore cannot be hidden behind a two-action
  alias.
- Eldritch side intent requires an explicit ordered Ember/Ichor setup program
  and a selected Eldritch Exalt, Chaos, or Annul. The kernel verifies the
  requested dominance before the final craft, charges every setup currency,
  and leaves the resulting implicit tiers on every exit.
- Protected-side options resolve the session's approved prefix/suffix lock
  bench craft, charge it once, then run a selected Scour or exact supported
  reforge. A charged lock no-op is rejected, and all operation exits return to
  the outer solver.
- Deterministic Multimod finishes accept one or two explicitly selected
  ordinary bench crafts. Sequential engine application checks crafted limits,
  group conflicts, and side capacity exactly; any charged no-op makes the
  option illegal.
- Policy compilation expands a selected option into an ordinary chain of
  primitive Strategy Board operation nodes. Only the first node carries the
  state's expected-cost annotation; the simulator charges the ordinary
  primitive price keys and the final operation routes back through the outer
  state router. No option-only strategy vocabulary was added.
- Added native coverage for all four kernels, options-only public goal parsing,
  exact Multimod cost/policy selection, and primitive-only compiled graph shape.
  The tests were authored but not executed at this intermediate checkpoint.
- Rebuilt `bindings/wasm/dist/poecraft_engine.wasm` so the worker uses the same
  fixed-option solver implementation.

## Verification performed

- `powershell -File scripts/build.ps1` completed successfully during engine
  integration. This configured and compiled the native engine, test
  executable, and benchmark target; it did not execute any test binary.
- The final touched native engine and test translation units also compiled
  successfully with `g++ -c`; no binaries were executed.
- `powershell -File scripts/build-wasm.ps1` completed successfully against the
  final source. Two immediately preceding invocations transiently failed when
  the in-place `wasm-opt` step could not reopen its Windows output path; the
  unchanged final retry succeeded, so the committed WASM is the normal
  optimized script output.
- `git diff --check` is clean.

Per the intermediate-phase cadence, the newly authored focused checks and all
routine native, Python, WASM, web, simulator, benchmark, and full-repository
suites were not run. No rendered or visual checks were performed.

## S7 baseline and acceptance reminders

- `powershell -File scripts/benchmark-solver.ps1` remains the single native and
  worker/WASM benchmark entry point. Do not overwrite the committed S7.0
  reports under `build/performance`.
- S7.0's comparison recorded 421 checks with zero mismatches across the eight
  then-enabled cases. Its structural counts predate the new primitive,
  evaluators, and planner options and remain historical comparison evidence.
- The approved directional minimums remain 5x geometric-mean solve speed and
  2x lower peak memory where the baseline leaves headroom. Continue while a
  material safe S7 bottleneck remains; Oliver evaluates the result.
- Final correctness remains S7.6 only: compile every approved real policy and
  run it exactly 10,000 times in the native simulator with the stored tolerance.
- The approved responsiveness budgets remain 50 ms per worker step and 250 ms
  cancellation acknowledgement. Per-case corpus caps remain authoritative.

## Scope that remains parked

S6 Phase 3 ambient Emulator odds was skipped entirely and must not reappear.
Economy E0-E7 is complete except external production activation. Phase 12
accounts, publishing/community, mechanic track M1-M5, Phase 18 recombinators,
and ML remain deferred, blocked, parked, or later as documented.
