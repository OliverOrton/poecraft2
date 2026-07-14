# Session Handoff — Calculator OOM fixes complete; s6 Phase 1 next

Written 2026-07-14 after implementing and gating the Calculator-mode
rare-reforge exact-evaluation OOM fixes. Read [AGENTS.md](AGENTS.md), then
[docs/direction.md](docs/direction.md), then this file. The next task is
[docs/s6-plan.md](docs/s6-plan.md) Phase 1. Phase D in
[docs/strategy-calculator-mode-plan.md](docs/strategy-calculator-mode-plan.md)
remains unscheduled.

## Current state

The `std::bad_alloc` failure for wide rare-reforge loops is fixed without
changing exactness or the v1 result contract:

1. `CalcContext` caches `shared_ptr<const OutcomeDistribution>` objects.
   Reforge states with the same preserved base now return the same distribution
   object instead of copying thousands of entries per `(state, action)`.
   State-dependent illegal/unapplied self-loops remain per-state.
2. Strategy evaluation assigns deterministic integer edge indices in
   node/edge order. Transition rows and edge-flow accumulation no longer carry
   heap string copies; serialization still emits authored edge order.
3. Legal operation pairs share one routed row keyed by
   `(node, distribution object)`. Router and illegal-state rows remain unique.
   Every solver/reference consumer reads through the row table.
4. SCC discovery now runs iterative Tarjan directly over shared rows. It no
   longer duplicates the logical edge relation into adjacency and reverse
   vector-of-vectors.
5. `CalcContext::intern_state` enforces the evaluation's `max_states` before
   inserting a new state. `max_transitions` is a new public evaluation option
   (default 10,000,000 stored row entries). Both limits surface as
   `PC_RESULT_CAPACITY_EXCEEDED` with explicit messages, not `bad_alloc`.
6. The committed WASM module was rebuilt with `-sMAXIMUM_MEMORY=4GB`.
7. Worker errors transport raw `EngineError.detail`, so the client adds the
   `poecraft engine error <code>:` prefix exactly once. Calculator UI copy uses
   the retained error code: code 4 is unsupported vocabulary; allocation and
   capacity failures use honest “too large to evaluate exactly” copy.

## Pinned regression and performance

`apps/web/test/strategy-eval-benchmark.ts` now includes the original wide
Vaal Regalia shape: Normal start → Alchemy, then a Chaos self-loop until
`LocalIncreasedEnergyShield11` tier 1. Through the real Node/WASM worker under
default options it completes in about 0.9 s, converges with zero fallback
sweeps, and pins:

- success: within `1e-10` of 1;
- expected actions: `24.91546431794879`;
- Alchemy consumption: `1`;
- Chaos consumption: `23.915464317948782`;
- all hit/miss/repeat edge traversals.

The two existing magic-loop benchmark pins remain unchanged. A final standalone
run recorded 31.177 ms and 31.004 ms warmed medians (45.99× and 113.21× versus
their recorded baselines); progress callback overhead remained below 0.8 ms.

## Acceptance gates — all green

- `powershell -File scripts/build.ps1`
- `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`
  — 118,426 checks, 0 failures
- `powershell -File scripts/build-wasm.ps1`
- `npx tsc --noEmit` in `apps/web`
- `npm test` in `apps/web` — 22/22 worker smoke tests plus all presentation and
  model tests
- `npm run benchmark:strategy-eval` in `apps/web`
- `npm run build` in `apps/web`
- `powershell -File scripts/test.ps1`

New gates cover shared reforge identity, native and C-ABI state/transition
caps, capacity code 8 through the real worker, absence of `bad_alloc`, raw
detail transport, one prefix only, and code-aware Calculator markup for codes
4, 5, and 8.

## Next task — s6 plan Phase 1 only

Implement [docs/s6-plan.md](docs/s6-plan.md) **Phase 1 — Solve in the
workspace (solve → Strategy Board)**. It starts with the required image-model
design loop for the solve panel and placement decision. Reuse the existing
Strategy Builder board-annotation mechanism for compiled-policy expected-cost
badges; do not introduce a second badge system.

Stop after the Phase 1 gate, rewrite this handoff, and do not begin Phase 2.

## Gotchas

- Exactness rulings from the previous handoff still stand: no probability
  cutoffs in discovery, junk coarsening, frontier/epsilon changes, or result
  contract changes.
- Shared rows are valid only for legal operation pairs with the same
  distribution object. Router nodes and state-dependent results must stay
  unique.
- `max_transitions` counts stored transition plus absorption entries across
  unique rows, not the dense logical references created when many pairs share
  a row.
- `PC_RESULT_CAPACITY_EXCEEDED` is currently code **8**. Presentation also
  treats legacy/symptom codes 5 and 7 as size failures, but new cap failures
  should be code 8.
- The engine WASM module is committed. Rebuild it after engine/C-ABI changes
  with `scripts/build-wasm.ps1`; the script self-activates `C:\emsdk`.
- Do not use Codex's built-in browser for this repository. Use a separate
  headless browser process when a browser smoke is required.
- PoE1 mechanic ambiguity goes directly to Oliver; never research or guess.
- Commits are local-only unless Oliver explicitly says to push.
