# Cross-Base And Compiled-Strategy Reliability Pass

**Status: completed and qualified on 2026-07-30.**

Parent: [Documentation archive](../README.md)

The reliability pass completed without changing crafting mechanics, solver
objective, action-filtering scope, or the qualified Fracture-local transition
model. It makes policy quality independent from stopping cause, validates all
979 engine-certified ordinary-session bases, and qualifies the returned-policy
chain through native and release-WASM compilation, exact evaluation, and
simulation.

## Qualified Result

- The structural harness builds all 979 ordinary sessions and validates base
  identity, modifier/family/tier consistency, influence and implicit masks,
  representative goals, action registry/candidates, exact Restart and
  Eldritch legality, item serialization, and base/item-level isolation with
  base/class-attributed failures.
- The deterministic reliability corpus contains 49 cases: 27 class smokes,
  six breadth cases, seven representative shapes, two dense deep cases, five
  authored special starts, and two selected verification cases.
- All 49 final native cases completed with zero expectation failures,
  incomplete cases, watchdog survivors, compile failures, exact-evaluation
  mismatches, or simulator off-policy failures.
- The selected Gloves and Ring policies both compile and exact-reconcile.
  Each completed 10,000/10,000 successful native runs with zero failures and
  zero off-policy actions. Solver/exact costs are
  `37.126855862088689 / 37.126855862092299` and
  `490.41233174996785 / 490.41233174992311`.
- The final release-WASM report completes all 49 cases with every expectation
  met. Both selected policies again complete 10,000/10,000 successful runs;
  the maximum cooperative solve slice is 5.76 seconds under the corpus's
  established 20-second stress ceiling, and peak reported WASM heap is about
  990 MB.
- Every published policy is strategy-executable. Product-coarse policies that
  need unrepresented exclusion identity, capped renewal policies without an
  exact witness, and primitive renewals whose expected actions exceed the
  100,000-action simulator contract are refused with precise compatibility
  reasons rather than emitted as mismatching strategies.
- Compiled base state preserves rarity, flags, influence bits, Eldritch tiers,
  and authored crafted/fractured/Veiled modifiers. Unveil offer identity is
  exact across compilation and evaluation.
- The qualified Fracture transition hash `04a66ba6c6dfcabf`, policy hash
  `3e5d7530e7aed5fb`, and compiled-strategy SHA-256
  `e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`
  remain acceptance invariants.
- Release WASM was rebuilt. Oliver retains ownership of the manual visual
  checklist; Codex performed no rendered visual review.
- The final `powershell -File scripts/test.ps1` pipeline passes, including
  ingest/artifact validation, Python bindings, 2,991,320 engine checks,
  benchmark specification validation, 27/27 WASM smoke checks, reliability
  corpus contracts, and all remaining non-visual web suites.

Final local evidence:

- native staged reports:
  `build/acceptance/cross-base-reliability/native-acceptance-final-v4/`;
- release-WASM report:
  `build/acceptance/cross-base-reliability/wasm-report-final-v2.json`; and
- tracked corpus and generation provenance:
  `fixtures/solver-reliability/v1/`.

## Objective

Stabilize the existing product across every supported ordinary-session base
and make every returned solver policy work through the complete product chain:

```text
Solve -> compile -> Strategy Board -> exact Calculator -> Simulator
```

This is a reliability and bug-fixing milestone. It does not add crafting
mechanics, automatic Veiled planning, the one- through three-goal anchor
library, action-filtering scope, solver objectives, or new solver
abstractions.

## Qualified Boundary

- source branch: `codex/fracture-local-coarse-parent`
- source commit: `25d5bbe6791beb61eae803219563575346def2dc`
- implementation branch: `codex/cross-base-strategy-reliability`
- unspecified reforge-work default: `20,000,000`

Local `main` was fast-forwarded to the qualified source commit before the
implementation branch was created. All milestone commits remain local.

## Frozen Work

1. Separate stopping cause from policy availability and make every state,
   transition, memory, sweep, and reforge-work cap visible. Distinguish exact,
   bounded, capped-with-policy, capped-without-policy, unsupported,
   unreachable, and compilation/evaluation failures. Make skipped,
   unsupported, and unpriced action counts understandable and retain a
   mixed-side rarity-cap regression without rarity special-casing.
2. Add cheap deterministic structural validation for all 979
   engine-certified ordinary-session bases: lifetime, identity, pools,
   families, tiers, goals, actions, eligibility, serialization, and
   base/item-level switching.
3. Add a versioned cross-base reliability portfolio without changing the
   historical natural-T1 v1 corpus. Reuse the existing generator, runner,
   watchdog, resumable ledger, and reports, with staged smoke, breadth,
   representative hard, and targeted deep execution.
4. Require every `policy_available` benchmark result to compile, serialize
   natively and through WASM, open as an unsaved Strategy Board document,
   evaluate exactly, simulate, preserve save/reload behavior, and reconcile
   cost within the existing tolerance. Run 10,000 simulations on the selected
   verification subset.
5. Inventory and close compiler/evaluator vocabulary gaps for primitive
   renewal, repeat-reforge, Fracture-local, Restart fallback, temporary bench
   blockers, protected routes, multimod, Eldritch actions, Imprint programs,
   and explicit Veiled/Unveil compiler fixtures. Refuse genuine unsupported
   boundaries early and precisely.
6. Make only measured, semantics-preserving graph improvements: remove
   unreachable structure, coalesce equivalent regions and routing, canonicalize
   Restart/failure paths, preserve the complete starting state, keep identity
   deterministic, and improve diagnostics and provenance.
7. Add non-visual product coverage for editing, prices, solve lifecycle,
   exact/bounded/capped presentation, compiled strategies, both strategy
   runners, workspace persistence/draft recovery, and large-board
   degradation. Prepare a short manual visual checklist for Oliver; Codex
   performs no rendered visual review.

## Acceptance Contract

- All 979 supported ordinary bases pass structural validation with exact
  failure attribution.
- The cross-base portfolio completes without crashes, stale handles,
  malformed strategies, or unclassified failures.
- Every returned policy compiles and works in exact Calculator and Simulator
  paths, including native/WASM serialization and workspace save/reload.
- Exact compiled-policy cost agrees with the solver-evaluated policy cost.
- The selected verification subset passes the existing 10,000-run success and
  off-policy contract.
- Every cap hit is visible and correctly named.
- Existing qualified Fracture behavior and solver transition hashes remain
  unchanged.
- Release WASM is rebuilt if engine-visible strategy behavior changes.
- The appropriate complete native, WASM, and web acceptance suites run once
  after implementation.
- Stable documentation and `HANDOFF.md` are updated, this plan is archived,
  and one final local milestone commit is created with the agent co-author
  line.
