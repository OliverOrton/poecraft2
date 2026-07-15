# Session Handoff - S7.1 Complete, S7.2 Next

Updated 2026-07-15 after the S7.1 one-item correctness and state-substrate
checkpoint. Read [AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md),
this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

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

Stop before S7.3. Do not add planner options/macros, cycle acceleration, policy
iteration, cache reuse, or policy compression yet. Do not run routine native,
binding, WASM, web, simulator, full-repository, or visual-browser gates at the
intermediate checkpoint. Oliver owns visual review.

## S7.1 result

- Added the permanent owner-rule fixture
  `fixtures/spec/action-results/vaal-regalia-ilvl-86-tied-eldritch-currency.json`.
  Absent and tied Eldritch dominance is pinned to ordinary Exalt/Chaos/Annul;
  side-specific intent still requires the later explicit setup option.
- Added `remove_crafted_modifiers` as native action 25 across the C ABI,
  Python/WASM bindings, strategy compiler/simulator vocabulary, and web action
  surfaces. Its solver and simulator resource vector is exactly one `scour`,
  and the action removes every non-fractured crafted explicit modifier without
  changing item rarity or ordinary modifiers.
- Added exact solver calculation for Harvest fire/cold/lightning resistance
  conversion using the engine-owned targeted pool, same-required-level rule,
  side locks, fractured-carrier exclusion, viable-source selection, and exact
  target weights.
- Added exact Fracture calculation: every eligible explicit carrier receives
  its uniform outcome and remains distinguishable after projection.
- Replaced the old item-wide crafted/fractured materialization approximation
  with goal-carrier masks and per-junk-class crafted, fractured, and intersection
  counts. Exact group-effect partitioning is enabled only for Unveil, Harvest
  conversion, Fracture, and crafted-modifier removal candidate sets.
- Extended ordinary strategy conditions so family/group and mod-count guards
  can require crafted/fractured slot flags. Compiled policies now serialize
  their materialized explicit start modifiers, including the approved fractured
  endgame start.
- Preserved the historical `refusal-unsupported-fracture` benchmark id for
  S7.0 comparison, but changed its current expected refusal to unreachable goal
  now that Fracture is evaluator-supported.
- Rebuilt `bindings/wasm/dist/poecraft_engine.wasm` because the engine action
  ABI and strategy vocabulary changed.

## Verification performed

- `powershell -File scripts/build.ps1` completed successfully. This configured
  and compiled the native engine, test executable, and benchmark target; it did
  not execute the test binary.
- `powershell -File scripts/build-wasm.ps1` completed successfully.
- `git diff --check` is clean.

Per the intermediate-phase cadence, the newly authored focused checks and all
routine native, Python, WASM, web, simulator, benchmark, and repository suites
were not run. No rendered or visual checks were performed.

## S7 baseline and acceptance reminders

- `powershell -File scripts/benchmark-solver.ps1` remains the single native and
  worker/WASM benchmark entry point. Do not overwrite the committed S7.0
  reports under `build/performance`.
- S7.0's comparison recorded 421 checks with zero mismatches across the eight
  then-enabled cases. Its structural counts predate the new primitive and newly
  supported evaluators and remain historical comparison evidence.
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
