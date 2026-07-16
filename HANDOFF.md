# Session Handoff - S7.4 Complete, S7.5 Next

Updated 2026-07-16 after the S7.4 renewal and observation-aware option
checkpoint. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

S7.0-S7.4 and S7.2R are complete. S7.3 remains the earlier out-of-sequence
fixed-option commit. S7.5 has not begun.

## Exact next boundary

Implement **S7.5 only: deep optimization and cache reuse**.

1. Eliminate direct self-loop contributions algebraically, including the new
   exact-equivalent renewal loops.
2. Benchmark residual-prioritized backups and the existing SCC/fixed-policy
   machinery; implement policy iteration if it wins the pinned matrix.
3. Retain compatible transition caches for price-only re-solves.
4. Add bounded focused expansion only if the preceding work still misses the
   hard corpus.
5. Compress policy regions before compilation.

Stop before S7.6 acceptance work. Do not run routine native, binding, WASM,
web, simulator, full-repository, or visual-browser gates at this intermediate
checkpoint. Oliver owns visual review.

## S7.4 result

- Goal JSON now accepts three lazy, price-independent option definitions:
  `renewal`, `protected_repeat`, and `fracture_prepare`. Renewal uses an
  explicit `until` predicate over selected zero-based goal-slot indices and a
  minimum satisfied count. Supported attempt programs are Alteration, Chaos,
  Essence, Fossil, Harvest reforge, Veiled Chaos, explicit Scour/Alchemy, and
  Veiled Chaos followed by observed Unveil.
- Renewal hides a failure only when evaluating the next attempt produces the
  exact same price-independent kernel as the entry attempt. That outcome is
  normalized to an entry self-loop and repays the attempt cost. Changed
  carriers, illegal continuations, salvage states, and brick states stay
  visible as ordinary outer-policy exits.
- `protected_repeat` executes the selected prefix/suffix lock before every
  attempt. A result that still carries the setup lock is not normalized to a
  retry, so no setup application is silently reused or omitted.
- Option rows now use the kernel's state-dependent expected resource vector.
  This preserves primitive quantities when a conditional step runs on only
  some paths, most importantly Fracture after successful carrier preparation.
  Sparse Bellman rows can contain ordinary chance exits and observation choice
  groups together.
- Veiled Chaos plus Unveil retains each sampled offer as an observation-owned
  choice group. Extraction records preferences per concrete pre-Unveil
  abstract state. Compilation emits ordinary Veiled Chaos, exact-state offer
  dispatch, `has_unveil_option` routers, concrete primitive Unveil operations,
  and retry/outer-exit edges; it never preselects an unseen result.
- `fracture_prepare` names one satisfying goal slot as the exact carrier. It
  repeats preparation only across certified equal kernels, conditionally runs
  primitive Fracture when that carrier is ready, and exposes every correct and
  wrong fractured carrier to the outer policy. Wrong fractures can therefore
  be salvaged or sent through the ordinary Restart action instead of being
  hidden inside the option.
- Fractured modifiers continue to satisfy normal goal-family/group conditions.
  Ordinary influence and fractured modifiers are mutually exclusive in direct
  engine application, solver legality, product option dependencies, and the
  manual WASM fracture/import paths. Fracture does not reject Eldritch implicits;
  their exact tiers remain in abstract state and compiled routing conditions.
- Goal-relevant registry generation retains option-named primitives, requested
  lock/Multimod dependencies, and conditional Fracture without admitting those
  structural primitives as unrestricted product actions.
- Every selected S7.4 option expands into existing Strategy Board operations,
  routers, and conditions. No simulator operation or condition vocabulary was
  added.

## Implementation notes for S7.5

- A normalized renewal attempt is intentionally still a Bellman self-loop.
  S7.5 should remove its algebraic contribution before comparing prioritized
  value iteration with policy iteration.
- `OptionKernel::retry_states` stores concrete states needed only to expand the
  hidden self-loop back into an editable graph. They are not outer Bellman
  successors. `continuation_states` similarly owns the conditional Fracture
  routing recipe.
- `OutcomeChoiceOption` distinguishes the observed pre-Unveil state, actual
  selected successor, and Bellman-normalized successor. Do not collapse these
  fields during policy compression.
- Per-solve distributions and option kernels are still released after compact
  sparse rows are copied. Compatible cross-solve retention remains S7.5 work.
- The sparse row Q path now deliberately sums both ordinary transitions and
  observed-choice groups. Primitive Unveil still uses choice groups only.
- The compiler recomputes selected S7.4 kernels to recover their editable
  expansion recipes. Price-only cache reuse and region compression must retain
  this price-independent behavior.

## Existing performance evidence and open diagnostics

- The clean pre-S7.2 full-corpus command was stopped after 3,036 seconds without
  completing the native report. The ignored record remains
  `build/performance/solver-pre-s7.2-unoptimized-timeout-v1.json`.
- The pinned `s7.2-final` one-mod report measured 0.365 ms native and 83.570 ms
  WASM; ordinary ES measured 688.372 ms native and 1,193.949 ms WASM. Native
  and WASM agreed on values, structures, and the requested 10,000-run outcomes.
- The advanced case's 10,000-run mean was 8.74% below forecast, just outside
  its stored 8% tolerance. That correctness investigation remains S7.6 work.
- The endgame case remained CPU-bound after sparse storage and held roughly
  61 MB working set when stopped after 335 seconds. Direct self-loop removal,
  SCC/policy iteration, and prioritized work are the intended S7.5 response.
- Worker cancellation acknowledgement was 18.115 ms against the approved
  250 ms budget; the exhaustive-registry stress refusal stayed under the
  50 ms slice budget at 34.876 ms.

## Verification performed

- The normal compile-only `scripts/build.ps1` wrapper entered its fallback
  optimized build but exceeded the command's 124-second timeout without
  returning a compiler diagnostic. No produced test binary was executed.
- Every changed native translation unit and affected solver test source passed
  direct C++20 `-fsyntax-only` compilation.
- A complete compile-only `-O0` shared-engine link succeeded across all native
  engine sources.
- `powershell -File scripts/build-wasm.ps1` completed successfully and rebuilt
  the checked-in worker WASM engine with the S7.4 option logic. It ran no test.
- Focused S7.4 unit coverage was added for exact renewal routing, protected
  setup repayment, observed Unveil compilation, exact-carrier Fracture,
  fractured goal satisfaction, influence exclusion, and Eldritch compatibility.
  Per the checkpoint cadence, those test binaries were not run.
- No routine native, binding, WASM, web, simulator, or full-repository test
  suite was run. No rendered or visual check was performed.

## Scope that remains parked

S6 Phase 3 ambient Emulator odds was skipped entirely and must not reappear.
Economy E0-E7 is complete except external production activation. Phase 12
accounts, publishing/community, mechanic track M1-M5, Phase 18 recombinators,
and ML remain deferred, blocked, parked, or later as documented.
