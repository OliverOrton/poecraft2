# Five-T1 Exact-Evaluator Scaling And Recovery

**Status: stopped at successor Gate 3 on 2026-08-16. The compact pair index
cleared the byte cap and exposed the unchanged 10,000,000-transition boundary
before refinement. Gates 4-8 were not started.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](README.md)

## Objective

Make the priced-base five-natural-T1 candidate independently materializable
and useful under the existing one-GiB solver/WASM cap, without weakening
exactness, changing mechanics, raising resource limits, or disguising an
unevaluated candidate as a refuted one. Preserve Witness A's safe paired
certificate/product semantics and recover cooperative release-WASM behavior.

This plan supersedes the unstarted Gates 2, 3, and 5-8 of the original
[plan](plan.md). The original plan and its stopped Gate 4 evidence remain the
historical record; their unstarted gates have no execution authority.

## Frozen Evidence And Corrections

The detailed review is in the
[follow-up audit](evidence/pair-discovery-follow-up-audit.md). The binding facts
are:

- Witness B's fail-closed candidate has 2,015 nodes, 4,123 edges, and 757
  policy regions. Exact evaluation discovers 8,395,474 raw operation/state
  pairs and stops before component construction at 1,178,801,916 owned bytes
  against a 1,050,982,663-byte evaluator budget.
- The fixture's `exact_max_pairs = 5,000,000` belongs to the external benchmark
  evaluator. Internal Solve certification instead derives its refined
  quotient cap from `max_state_action_rows = 1,215,000`. Raw pair count is not
  the final quotient class count, so no future count-cap failure or required
  cap increase has yet been proved.
- External `phase_wall_ms.verification` is null because benchmark verification
  was deliberately disabled. Internal telemetry attributes about 3.41 seconds
  to failed exact graph evaluation, about 3.75 seconds to direct
  certification, and about 4.08 seconds to the subsequent strict-lift attempt,
  but does not split evaluator discovery, partition, and solve subphases.
- The accepted 1,813-node four-goal graph contains 767 product-local Fracture
  routers with only seven distinct executable route behaviours. Their
  operation descriptors are the same; 49 distinct full operation-node
  payloads remain because `expected_cost` is state/region annotation. Existing
  shared-region semantics already omit non-uniform expected-cost annotations.
- The 49-case reliability corpus has one- through four-goal cases only. Its two
  named three-goal examples publish independently evaluated renewal fallbacks;
  their cheaper core candidates are refuted as improper. Witness B is instead
  unevaluated because exact certification reached a resource cap. The public
  publication label currently collapses both outcomes to
  `bounded_fallback_policy`.
- The reported 0.3906 ms compilation is for Witness B's six-node published
  fallback, not its internal 2,015-node candidate. Internal candidate
  compilation is about 61 ms and is not the measured critical path. Fracture
  sharing is nevertheless potentially on the critical path because graph
  structure is the exact evaluator's carrier input.

## Invariants And Non-Goals

1. Do not change Path of Exile mechanics, goal success semantics, prices,
   Bellman objective order, policy stability tolerances, or candidate scope.
2. Do not raise `max_solver_owned_bytes`, `max_state_action_rows`, external
   exact-evaluator limits, Simulator action limits, or the five-minute product
   boundary.
3. Do not infer quotient size from raw pair count. Report raw pairs, initial
   classes, final classes, and the exact cap that owns each boundary.
4. Product-local Fracture sharing must key the complete executable behavior:
   operation, accounting roles, acceptable-hit condition, continuation, and
   default/retry destination. Annotation differences alone may not prevent
   sharing and may not acquire execution authority.
5. Certification remains fail closed. Product-safe Restart defaults may publish
   only through the existing structurally paired graph contract and independent
   product-graph evaluation.
6. Preserve deterministic values, bounds, action choices, route coverage,
   success/off-policy mass, and exact costs on controls. Structural hashes may
   change only where the accepted compiler representation changes, and the
   semantic comparison must explain the change.
7. Do not run the full repository acceptance pipeline before Gate 8. Rebuild
   release WASM before web acceptance because accepted native compiler/evaluator
   behavior is browser-visible. Oliver retains rendered UI review.

## Gate 0 - Exact-Evaluator Phase Attribution

Add behavior-neutral, cap-accounted timing and work telemetry for the internal
compiled-strategy evaluator. At minimum distinguish:

1. carrier/state discovery;
2. exact kernel/row construction;
3. observation and immediate-key preparation;
4. raw pair discovery;
5. closed-partition refinement; and
6. component construction and component solve.

The diagnostic result retained on a resource exception must expose completed
stage timings, raw counts, projected/owned bytes, the active refined-pair cap,
and the precise pre/post boundary. Timers must use monotonic wall time and add
negligible retained memory. Add focused native tests for complete and
resource-stopped results; do not rerun the real witness merely to test JSON.

Acceptance: focused evaluator and telemetry fixtures pass; existing results,
hashes, values, and cap classifications are unchanged apart from the new
fields.

## Gate 1 - Behaviour-Keyed Product-Local Fracture Sharing

Route product-local Fracture through exact policy-region compression using a
canonical full-behaviour signature. Reuse one operation/route continuation for
states whose emitted operation, acceptable-hit condition, retry/default
target, and accounting roles are identical. Follow the existing shared-region
rule: preserve a uniform expected-cost annotation, otherwise omit it.

Do not merely deduplicate JSON after compilation. Sharing must occur before
policy-route construction so route leaves, node/edge cap accounting, compiler
memory accounting, and exact evaluation all see the same canonical graph.

Add compiler tests proving:

- two states with identical Fracture behavior share a region;
- different acceptable-hit conditions do not share;
- different retry/default destinations or accounting roles do not share;
- the shared and unshared oracle graphs have identical exact cost, success,
  off-policy mass, and operation accounting; and
- certification/product graph pairing still differs only at designated
  bounded defaults.

Acceptance on the frozen accepted four-goal graph: the 767 Fracture routes
collapse to seven behavior groups (or a smaller count justified by a stronger
complete signature), exact evaluation remains proper with the accepted cost,
and the graph is materially smaller. Compilation speed is reported but is not
the qualification claim.

## Gate 2 - Priced Five-T1 Decision Point

Run Witness A once and the priced-base Witness B once with the Gate 0 telemetry
and Gate 1 compiler. Preserve all declared caps and prices.

Record for Witness B:

- certificate nodes, edges, policy regions, and Fracture behavior groups;
- raw states, raw pairs, rows, transitions, and row payload;
- initial and final quotient classes when reached;
- per-stage wall time and largest cooperative step;
- evaluator and total publication peak bytes; and
- exact cost, success/off-policy mass, publication kind, bounds, and strategy
  hash when materialized.

If Witness B certifies under cap, skip Gate 3. If it still stops, Gate 3 may
proceed only from the newly exposed owning payload/count. If it reaches the
1,215,000 refined-class cap, report the actual final/attempted class evidence;
do not substitute the unrelated 5,000,000 external limit and do not move a cap.

Witness A must remain proper, paired-default-only, independently product
evaluated at `624800.9519118543`, and within the 250 ms step boundary. A
representation-only hash change requires an explicit semantic equivalence
record.

## Gate 3 - Conditional Pair-Discovery Representation Work

Run this gate only if Gate 2 still stops. Choose exactly one smallest owning
change from evidence, such as compact pair keys, row/transition interning,
streamed replay-key construction, or an earlier exact quotient. State why the
chosen representation attacks both the measured owned-byte payload and any
proved refined-class/count boundary.

The implementation must preserve collision-safe identity and exact
probabilities. Hash-only equality, lossy observation keys, probability
truncation, optimistic pruning, or reconstruct-then-merge beyond the cap are
not acceptable. Add a raw oracle comparison on small graphs and a focused
resource test at the new representation boundary.

Repeat Gate 2 after the focused tests. Stop with a precise handoff if fitting
Witness B requires a materially broader solver/evaluator architecture or a cap
increase.

**Actual result:** the compact collision-safe discovery index reduced its peak
to 75,497,472 bytes and kept the evaluator below its byte cap, but discovery
then reached `max_transitions = 10,000,000` at 9,998,209 raw pairs before
refinement. Continuing requires a pre-closure quotient/streaming architecture
or a cap increase, so this stop condition fired. See the
[Gate 3 stop](evidence/successor-gate3-stop.md).

## Gate 4 - Cooperative Evaluation And Cancellation

Make every newly owning discovery, key-build, partition, and component loop
cooperatively step-able. No coroutine suspension may occur inside a catch
handler. Preserve exception classification across native and Emscripten.

Acceptance:

- native step-size A/B results are deterministic;
- the frozen Witness A and Witness B largest public worker step is at most
  250 ms in release WASM;
- cancellation acknowledgement is at most 250 ms while each affected phase is
  active; and
- progress counts are monotone and name the actual evaluator subphase rather
  than appearing frozen during pair work.

## Gate 5 - Publication Reason Truth And Five-Goal Coverage

Add a stable publication-reason distinction without changing candidate
selection:

- `evaluated_refuted`: the preferred candidate completed independent exact
  evaluation and failed properness, route coverage, or another semantic test;
- `evaluation_deferred_resource_cap`: independent evaluation did not reach a
  verdict because a named resource cap stopped it; and
- the existing successful exact/certified categories.

Keep detailed failure classification and cap ownership available. Native C
ABI/WASM JSON and TypeScript protocol names must agree if the field is public.
Do not label the independently verified fallback itself as improper when only
the rejected preferred candidate failed.

Add Oliver's frozen priced-base five-natural-T1 request as a permanent
five-goal reliability/qualification entry with proportional expectations. It
must distinguish an evaluated strategy from a resource-deferred candidate and
must not require sampled verification when the unchanged per-run action cap
makes that verification inapplicable.

## Gate 6 - Outstanding Strategy-Recovery Semantics

Only after Witness B is materializable, revisit the original milestone's
unstarted behavior work:

1. preserve the cheaper certified Restart-free incumbent when the priced base
   admits Restart;
2. prove adding a priced action cannot make the published certified upper
   worse;
3. restore prioritized state-local automatic admission, including the frozen
   Eldritch and Warlord controls, without exhaustive carrier expansion; and
4. retain all uncompleted alternatives as explicit open obligations.

Do not import the old gate implementation wording blindly. Re-freeze focused
red/green witnesses against the post-scaling source and implement only the
remaining reproduced defects.

Acceptance: the priced five-T1 published strategy is not the six-node Chaos
renewal and is below 1,000,000 chaos; Witness A and the four-goal control do not
regress; Eldritch and Warlord actions remain reachable and independently
executable.

## Gate 7 - Native And Release-WASM Qualification

Run the focused compiler, evaluator, solver, C ABI, and benchmark-manifest
tests once on the integrated source. Rebuild release WASM, then run the
non-visual worker/WASM protocol tests, `npx tsc --noEmit`, and the relevant web
suite.

Run native/release-WASM Witness A, priced Witness B, the four-goal control,
Eldritch, Warlord, and the new five-goal corpus entry. Compare values, bounds,
termination, publication reason, action IDs, exact costs, route defaults,
success/off-policy mass, graph semantics, memory, stage timings, progress, and
cancellation. Run 10,000 Simulator trials only where compiled-strategy
verification is required and applicable under the unchanged action limit.

## Gate 8 - Final Acceptance And Archive

After Gates 0-7 pass:

1. run `powershell -File scripts/test.ps1` exactly once;
2. run `git diff --check`;
3. update stable solver, engine/WASM, product, evidence, and decision docs;
4. archive both the stopped original plan and this successor with exact
   values, hashes, timings, caps, and skipped/inapplicable checks; and
5. leave `HANDOFF.md` at the next truthful boundary.

Completion requires all of: sound fail-closed certification, an independently
evaluated useful priced five-T1 strategy below one million chaos, unchanged
caps, at-most-250-ms release-WASM steps/cancellation, truthful publication
reason, and no native/release-WASM semantic mismatch. If any condition fails,
record the achieved soundness/scaling result without calling the milestone
complete.

## Checkpoint Commits

Use coherent local checkpoints, each ending with the required co-author line:

1. select the successor and record the audited boundary;
2. expose exact-evaluator subphase attribution;
3. share product-local Fracture regions by complete behavior;
4. record the priced five-T1 decision point;
5. scale the owning pair representation if Gate 3 is required;
6. make the affected evaluator phases cooperative;
7. distinguish publication reasons and add five-goal coverage;
8. recover remaining action-monotone/automatic semantics;
9. qualify native and release WASM; and
10. record final acceptance and archive.

Do not push unless Oliver explicitly asks.
