# Carrier-Ladder Resumable Joint-Policy Continuation v1

**Status: active — Phase 2 contract/fixture passed; Phase 3 integration next.**

## Objective

Retain and incrementally resume one exact ordinary joint-policy candidate when
its selected-policy traversal encounters a missing continuation. Exact service
for that one continuation must flow through the existing refinement and row
lifecycle while ordinary carrier, value, and joint-policy interleaving
continues. The candidate must resume from its saved traversal state instead of
reconstructing the complete global policy after every serviced state.

## Scope

The source baseline is the ordinary-interleaving engine at
`b690cad1e376baa5b267603f5cb619c9596a4f94`; the working checkpoint is its
evidence/WASM child `a270bd35902546aac09ba41d6187f4f15e1bafa7` with no
engine diff. V1 permits at most one snapshot-pure candidate and one exact
continuation obligation. It adds no planner, global scheduler profile, fixed
window, case-specific rule, action filtering, cap increase, mechanic change,
fragment authority, RCASSP proof, or publication authority.

## Starting evidence

- Current ordinary interleaving independently exact-evaluates clean-five at
  `85408.64362148782` Chaos, `8259.46821052856` expected actions, success `1`,
  off-policy mass `0`, strategy SHA-256
  `9e8687ac1f1de705cd1bef59e5269395190e6b8d2134d26d0cf1aac2468717b1`.
- Rejected one-carrier and 128-carrier ownership produced approximately
  `131111.404113426` and `922300.664991073` PDR cost. Fixed service counts are
  not the required unit.
- Sticky missing-witness service reproduced exact PDR cost
  `7852.71432971444`, `3375.77709434794` expected actions, success `1`,
  off-policy mass `0`, complete reconciled pricing, strategy SHA-256
  `f1b6c20001bd29b75346176852b4fc94b00d68167e5ffb005ed83bfaf01aa61e`,
  report SHA-256
  `9721a1cf3d09946089921a10e1e17ed80c10620538cc85146dfa4c2e877268d0`.
  Its caller scope is goal-relevant, step-8, gated reforges, restart disabled,
  Imprint disabled; economy identity is
  `economy:allflame:de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`.
- The same sticky behavior regressed matched clean-five to
  `1650831.39165144`. Whole-frontier ownership is rejected.
- Complete global joint-policy retries service continuations but spend too
  much bounded work. The selected unit is the unfinished continuation state of
  one exact joint-policy candidate.

Raw reports and strategies remain under
`build/qualification/pdr-retention-capacity-operator-proof-pilot-v1/`; only
bounded projections belong in this document.

## Design invariants

- Capture may begin only from an actual ordinary joint-policy attempt with an
  exact selected prefix, concrete missing state, compatible certified
  boundary, stable semantic identity, and existing exact root estimate.
- The fixed prefix binds caller/action/goal/economy/mechanics/artifact/terminal
  identities; source, target, admission, graph, incumbent, boundary, and proof
  generations; selected row and observed-choice identities; traversal cursor;
  missing state; and retained work/byte ownership.
- A hash accelerates lookup but does not replace canonical equality. Numeric
  context-local IDs are never the complete reuse proof.
- Global value changes may not silently rewrite the prefix. Structural row,
  action, scope, mechanics, terminal, price, or generation incompatibility
  discards or explicitly rebases the candidate.
- One service event requests one existing exact continuation obligation and
  yields. It never reserves a carrier frontier or fabricates a row.
- Completion still uses existing properness, compiler, independent evaluator,
  verified-incumbent comparison, and publication paths.

## Plan

1. Trace current selection, first-missing handling, exact refinement, retry,
   identity, properness, compilation, evaluation, portfolio, and finalization;
   locate exactly where candidate work is discarded.
2. Add the smallest internal resumable-candidate contract and a mechanics-
   independent exact fixture covering two yield/resume cycles, snapshot purity,
   rebase/staleness/release, competition, properness, resource interruption,
   deterministic identity, and byte accounting.
3. Add a disabled-by-default benchmark-private diagnostic and prove on one PDR
   run that the same candidate services at least two missing states without
   global prefix reconstruction.
4. Promote only that mechanism into ordinary interleaving, then qualify PDR
   and clean-five. Run partial four-to-five and the established non-armour
   control exactly once only after both primaries pass.
5. Retain and close only a buildable implementation that meets every numerical
   and authority gate below. Otherwise stop at the deepest coherent diagnostic
   checkpoint and name the exact missing contract.

## Decision criteria

- PDR must reproduce strategy
  `f1b6c20001bd29b75346176852b4fc94b00d68167e5ffb005ed83bfaf01aa61e`
  or independently exact-evaluate a different strategy at no more than
  `7852.71432971444`, with success `1`, zero off-policy mass, complete prices,
  and reconciled cost.
- Clean-five must independently exact-evaluate at no more than `98220` Chaos
  and no more than `10000` expected actions; preferred cost is at most
  `87361.16904205013`.
- Partial four-to-five and non-armour must retain an exact executable strategy
  within 15% of matched current-main baselines, with unchanged scope/caps and
  no invalid publication or unexplained watchdog.
- PDR and clean-five are the only architecture-iteration solves. No Simulator
  runs until a materially changed final retained strategy requires the single
  10,000-run qualification.

## Work log

- 2026-08-30: verified `a270bd3` is a clean evidence/WASM child of the
  authoritative `b690cad` engine source. Protected untracked `0` is the sole
  dirty path and remains untouched. Locked the exact PDR reference and rejected
  fixed-window/sticky evidence above. Began Phase 1 audit; no source mutation or
  new long solve yet.
- 2026-08-30: source audit found the discarded state in
  `try_install_reachable_incumbent`: `rebuild_reachable` names and schedules an
  exact missing continuation, then stack-local selected rows, reachability,
  observation routing, and traversal cursor are destroyed by `restore`.
  `maybe_install_incremental_anytime_incumbent` later starts the entire attempt
  again. Ordinary refinement already owns exact service; no scheduler or row
  lifecycle replacement is needed.
- 2026-08-30: added the internal snapshot-pure continuation contract and a
  mechanics-independent two-resume fixture. It retains exact semantic keys as
  collision guards, treats numeric state/row IDs only as locators, permits
  unrelated value/proof generation advance without rewriting a fixed prefix,
  and refuses structural, scope, boundary, generation, properness, resource,
  compiler, and evaluator failures explicitly. A heuristic root estimate has
  no pruning/publication authority; exact noncompetition requires a separately
  supplied admissible lower.
- Focused evidence: native target built successfully; command
  `build/engine/poecraft_engine_tests.exe --solver-joint-policy-continuation-only`
  passed 51 checks with zero failures. No long solve or Simulator run was
  launched. Next, connect this object only to the benchmark-private diagnostic
  mode, expose bounded lineage in the existing report, rebuild the native
  benchmark, and run one PDR causal witness only after the diagnostic shows
  the same identity can reach its second wait state.

## Outcome

Open.
