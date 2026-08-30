# Carrier-Ladder Resumable Joint-Policy Continuation v1

**Status: stopped at Outcome B — benchmark-private resumability proved;
production activation rejected and removed.**

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
  initially passed 51 checks with zero failures. No long solve or Simulator
  run was launched for that checkpoint.
- 2026-08-30: connected the object only to benchmark-private
  `resumable_continuation` mode and emitted bounded lineage under the existing
  carrier-ladder diagnostic report. The first causal PDR witness captured
  candidate `1cbf53094bd04aa9` and named missing state
  `cbcf6552cf31d0da`, but correctly refused `stale_identity` after the action
  vocabulary grew append-only from 288 to 300 entries. The fixed selected rows
  remained semantically valid, proving the identity contract had bound the
  mutable suffix too broadly.
- 2026-08-30: changed compatibility to bind the exact capture-time action
  prefix plus monotone vocabulary size; every retained decision still validates
  the complete planner action and row semantic keys. The second causal PDR
  witness deterministically captured candidate `51d67b3219b70c43`, resumed it
  once, appended the serviced row, and then refused `stale_generation` because
  the candidate's numeric state-index vectors had capture-time capacity 3431
  while the exact graph had grown append-only to 5742 states. This named the
  remaining representational defect rather than weakening graph compatibility.
- 2026-08-30: made bounded state-index capacity growth explicit and extended
  the mechanics-independent fixture across two graph-growth/resume cycles.
  The focused command now passes 54 checks with zero failures. The final causal
  PDR witness at
  `build/qualification/resumable-joint-policy-continuation-v1/pdr-diagnostic-r3/report.json`
  retained candidate `51d67b3219b70c43` through two resumptions and three
  distinct missing states (`cbcf6552cf31d0da`, `c5a8e1d23d3e1891`, and
  `4049f0ae084648d7`), with one capture, three yields, 1,189 fixed decisions,
  48,183 retained work, zero global reconstruction, zero stale discard, and
  43 ordinary interleave events. Source/target generations grew from
  13,564/3,431 to 150,188/7,242 while the exact action prefix remained equal
  and the action vocabulary grew to 306 entries. Retained candidate ownership
  was 169,227,180 bytes with 350,576 transient peak bytes.
- The same final diagnostic let the ordinary solver close independently exact,
  compile, and reconcile PDR at `3758.12442725521` Chaos,
  `8608.87713157432` expected actions, success `1`, off-policy mass `0`, and
  strategy SHA-256
  `c5ddf81a73eeec532a3efdbcbe661216942c32464ac51127401bd657b3aa1597`.
  The retained candidate did not hand off or publish: it remained safely
  waiting on its third obligation. This is causal proof that candidate-local
  resumption can improve ordinary exact service without acquiring planner,
  compiler, evaluator, or publication authority.
- 2026-08-30: promoted the same mechanism locally through the existing
  high-impact ordinary profile and charged candidate ownership to the normal
  live/peak solver byte ledgers. The first production PDR attempt exposed an
  in-scope O(candidate-size) fast-ledger defect and reached its 300-second
  watchdog at 7,133 states. Caching current retained bytes while preserving the
  full audited traversal repaired that implementation defect; the focused
  fixture passed 60 checks and the native benchmark rebuilt. This production
  code was never committed.
- The corrected production PDR run at
  `build/qualification/resumable-joint-policy-continuation-v1/pdr-production-r2/report.json`
  closed exact in 227,219.0545 ms at `3758.12442725521` Chaos,
  `8608.87713157432` expected actions, success `1`, off-policy mass `0`,
  complete reconciled pricing, and strategy SHA-256
  `c5ddf81a73eeec532a3efdbcbe661216942c32464ac51127401bd657b3aa1597`.
  Compilation produced 312 nodes and 881 edges; all caps passed, including
  normal peak ownership of 804,899,393 bytes. This was a different, strictly
  better strategy than the exact PDR reference.
- An older handoff pointed to the later 180-second no-Imprint clean-five
  revision. Its one run produced valid but inapplicable evidence at
  `91889.5602708236` Chaos and `8333.24955468757` actions. The selected boundary
  instead requires the matched 120-second request that produced the qualified
  current-main baseline. The repository JSON CLI imported its authoritative
  source, changed only `/requested_bounded_finish_seconds` from 60 to 120,
  validated it natively, and saved immutable revision
  `case-rev-54343c2296afe4f624f622c16779498c`, revision SHA-256
  `54343c2296afe4f624f622c16779498cfc0f0d03f6170e323837a1a06c74edb0`.
- The matched production clean-five run at
  `build/qualification/resumable-joint-policy-continuation-v1/clean5-production-120/report.json`
  reached its natural requested bounded finish but retained only the primitive
  fallback: exact cost and expected actions were both
  `470485191.442781`, versus the required `98220`/`10000` ceilings and the
  matched `85408.64362148782` baseline. The graph was valid and independently
  exact-evaluated with success `1`, off-policy mass `0`, complete reconciled
  pricing, all caps passed, and strategy SHA-256
  `a9e33a62edd46e870327888d891738aa71c5ea436c27994047734ca94f11b61b`.
  This is a quality/eligibility failure, not invalid publication or a resource
  failure.
- Outcome B: the resumable representation and multi-state lifecycle work, but
  the available exact capture predicate admits both useful PDR ownership and
  harmful clean-five ownership. A concrete first-missing state, exact selected
  prefix, compatible boundary, stable identity, and improving candidate
  estimate do not prove that preferentially servicing that continuation is a
  better use of the remaining bounded horizon than ordinary ladder work. The
  smallest missing predicate is an action/state-specific exact
  retention-capacity certificate that authorizes continued ownership only when
  the competitive candidate's named continuation can be retained and serviced
  within proved capacity without displacing the ordinary ladder's stronger
  executable-upper path. The future Retention-Capacity proof pilot's precise
  consumer is this candidate's capture/continue/release decision; it must not
  become a new planner or heuristic score.
- The uncommitted production activation, normal-ledger integration, and cached
  fast-byte optimization were removed after the matched primary failure.
  Retained HEAD `a26ad04` is benchmark-private diagnostic code only. The
  partial four-to-five and non-armour controls were not run because both
  primaries did not pass. Simulator, WASM rebuild, broad native tests, and the
  complete repository pipeline were not run; no failed production strategy is
  being claimed or retained.

## Outcome

Outcome B. Snapshot-pure candidate-local resumption is proved and retained only
behind the disabled benchmark-private diagnostic. Production activation is not
qualified because the same eligibility contract that closes PDR destroys the
matched 120-second clean-five upper. No implementation boundary remains active;
the next chunk requires an explicit direction choice.
