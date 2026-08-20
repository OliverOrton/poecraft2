# Session Handoff

**Status: a condition-efficient strategy compilation and consolidation plan
is written for review; implementation has not started.**

## Current boundary

The proposed
[Condition-Efficient Strategy Compilation And Consolidation](docs/active/2026-08-16-five-t1-restart-monotone-recovery/condition-efficient-strategy-compilation-plan.md)
is the next reviewable scope. Oliver has not yet authorized implementation. It
supersedes the cooperative exact-reforge proposal before that proposal began.

The binding hypothesis is current compiler underuse of the existing condition
system. Both policy-tree builders split on the widest/most balanced feature,
emit one edge per feature value, and hash-cons only completed routers with the
same ordered targets and serialized conditions. They do not optimize for
distinct continuations, coalesce same-target siblings with `any`, minimize
edges/condition bytes, or use downstream subtree sharing in feature choice.
The native simulator already supports nested `all`, `any`, `not`, and
`at_least`, structurally memoizes equal compiled conditions, and preserves
priority/source order.

Gate 0 therefore persists actual graph JSON and measures same-target groups,
mutual-exclusion provenance, priorities, condition expression structure,
repeated bytes, route depth, and the proof-safe reduction ceiling. The
implementation then introduces a typed canonical condition authority,
priority-safe same-target coalescing, and—only if still material—a
continuation-aware reduced multi-valued decision DAG and existing-vocabulary
factoring. The touched compiler is subsequently split into condition,
feature/domain, DAG, and emission/accounting modules. No public strategy schema
change is authorized.

Current native Witness B is 92 nodes / 338 edges / 150,813 JSON bytes: 84
policy routers, one local gated router, three primitive operations, four
infrastructure nodes, 248 condition-bearing edges, and 116,972 condition bytes.
The retained four-goal graph has 4,594,437 condition bytes in 4,737,473 JSON
bytes. Existing complete-behavior Fracture and gated-operation sharing remain
authoritative and are not reimplemented.

The predecessor
[bounded result](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/gated-route-compaction-result.md)
remains authoritative. Witness A is exact at `624800.9519118543`. Witness B is
a proper bounded executable policy at `16226566.773294946`, success one, zero
off-policy mass, and lower bound zero. Its actions are Chaos, Exalt, and Dense
Fossil plus a one-socket resonator. Direct certification remains fail-closed
as `cost_mismatch` against `37279651.842345364`, and strict carrier 5983 maps
outside the solved coarse graph. Do not call the result optimal.

The plan preserves but defers exact-reforge responsiveness, strict carrier
5983 mapping repair, stored/exact cost reconciliation, lower-bound and
alternative-envelope closure, Fracture-Q completion, broader five-mod action
quality, versioned condition references/BDD/bytecode, unrelated solver-file
decomposition, and browser presentation work. These are explicitly listed in
the plan so they are not lost.

The native build and focused evaluator (18,065), compiler (815), refinement
(362), and solver (96,120) suites passed at the predecessor checkpoint. A
manual release-WASM rebuild was subsequently requested and succeeded; its
generated `.wasm` is currently modified and uncommitted, with no downstream
test run. The new plan performs one final native/WASM/full acceptance round
after implementation.

## Prior history

The original
[Five-T1 Restart-Monotone Strategy Recovery](docs/active/2026-08-16-five-t1-restart-monotone-recovery/plan.md)
hit its explicit Gate 4 stop condition. Oliver approved the
[selected successor](docs/active/2026-08-16-five-t1-restart-monotone-recovery/successor-plan.md)
after reviewing the follow-up evidence. Do not resume unstarted gates from the
old plan, raise the solver/WASM cap, or describe the five-T1 priced-base
strategy as recovered.

The original Gate 4 native source checkpoint was `705c25c` on
`codex/solver-goal-realignment`. It adds the checked 27-action Restart-free and
28-action priced-base cases, explicit product/certification compiler modes,
fail-closed bounded certification, structurally verified default-only graph
pairs, retained certificate/product telemetry and memory accounting, stable
failure classes, and observation propagation evidence. Nothing was pushed.

Witness A is sound at this boundary. Its fail-closed certificate has 170
policy-router defaults to `offpolicy`, is proper/cost-complete with zero
off-policy mass, and pairs structurally with the 184-node/666-edge safe product
graph. Independent product evaluation matches at `624800.9519118543`; the
product SHA-256 is
`f12a2cb13137e69d7b107015da9d417026a4b01accf5cb7206da18d315b2ee62`.
The final focused native run took 5.50 seconds with a 224.30 ms largest step.

Witness B now clears the former observation refusal. Its 2,015-node/4,123-edge
fail-closed certificate completes observation propagation in 10 rounds with
six canonical requirements. Conservative projected observation peak is
4,313,004 bytes versus a 2,905,660-byte post-fixed-point owned estimate.

The new boundary is exact pre-component pair discovery: 8,395,474
state/action pairs, 35,837 states, 544 rows, 8,396,650 transitions, and
1,178,801,916 owned bytes against a 1,050,982,663-byte evaluator budget. Row
payload is 268,692,800 bytes; retained observation requirements are only
594,480 bytes. The stable classification is
`exact_eval_pair_discovery_memory_cap`. The same path has a 2,636.42 ms public
step. Publication therefore remains the six-node Chaos renewal at
`37279857.73995944`, above the plan's one-million-chaos materiality ceiling.

Detailed evidence and exact commands are in the
[Gate 1/Gate 4 record](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/gate1-gate4-stop.md).
The focused native build, compiler suite (815 checks), evaluator suite (16,801
checks), solver suite (96,082 checks), both frozen cases, and `git diff
--check` passed. No release-WASM rebuild, web suite, Warlord/automatic Gate 3
matrix, or full `scripts/test.ps1` acceptance run was performed after this
checkpoint, as required by the stop condition.

Successor Gate 0 is complete. Solver diagnostic telemetry now splits model,
observation, pair discovery/interning, exact kernel, pair refinement, component
construction/solve, and finalization active time, and a resource-stopped result
retains its subphase plus the actual internal refined-pair limit. The focused
evaluator suite passed 16,819 checks and the focused solver suite passed 96,083.

Successor Gate 1 is complete. Product-local Fracture policy regions share only
when their operation, accounting roles, acceptable-hit route, and common retry
default match. The focused oracle collapsed three selected states to two
distinct executable behaviours while matching the strict lift's exact cost and
operation accounting. The frozen four-goal graph collapsed its 767 Fracture
routes to seven, from 1,813 nodes / 3,832 edges / 5,205,249 bytes to 292 nodes /
1,549 edges / 4,737,473 bytes. Independent exact evaluation still matches
3,745.73093400839 with success 1 and zero off-policy mass. The focused compiler
suite passed 815 checks and the solver suite passed 96,111. Detailed evidence
is in
[successor Gate 1](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/successor-gate1.md).

Successor Gate 2 is complete. Witness A remains independently evaluated at
624,800.9519118543, paired-default-only, success 1 / off-policy 0, and its
largest native step is 234.23 ms. Witness B remains resource-deferred and
publishes the 37,279,857.73995944 Chaos renewal. Its unchanged 2,015-node
certificate stops before refinement at 8,395,474 raw pairs and 1,178,823,076
evaluator-owned bytes versus 1,050,981,903. Pair discovery owns 3.30 seconds,
pair interning 1.39 seconds, and refinement is not reached. Detailed evidence
is in
[successor Gate 2](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/successor-gate2.md).

Successor Gate 3 implemented a compact collision-safe chained pair index and
retires discovery-only indexes after recording the true closed-discovery peak.
The focused raw-reference and resource tests pass. Witness A remains sound at
624,800.9519118543 with a 232.95 ms largest native step. Witness B clears the
old byte boundary: its index peaks at 75,497,472 bytes and total evaluator peak
is 938,125,764 versus the 1,050,981,791-byte cap.

The newly reached boundary is count-owned. Discovery stops at the unchanged
`max_transitions = 10,000,000` with 9,998,209 raw pairs, before refinement.
The stable classification is now
`exact_eval_pair_discovery_transition_cap`; no refined-class conclusion is
available. The published strategy is still the six-node Chaos renewal at
37,279,857.73995944, and the largest native step remains 2,799.90 ms. Detailed
evidence is in the
[Gate 3 stop](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/successor-gate3-stop.md).

The successor plan's explicit stop condition fired. Continuing requires either
raising the transition cap or a materially broader pre-closure quotient or
streaming transition architecture. Gates 4-8 were not started; no release WASM
or full acceptance pipeline was run. Do not call the priced five-T1 strategy
recovered or resume action-semantics work without Oliver selecting the next
chunk. Preserve the current tree and checkpoints.

Oliver subsequently authorized a transition-cap increase if it would be
useful. Scoped 12-million and 13-million probes were not useful: both still
stopped during raw discovery, and the 13-million run peaked at 1,042,815,196
bytes, leaving only 8,166,595 bytes below the evaluator budget. Raw closure
therefore needs more transitions than can fit in the current representation.
The checked fixture and engine default remain at 10,000,000. See the
[transition-cap probe](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/transition-cap-probe.md).

The remaining viable boundary is specifically a pre-closure quotient or
compact/streamed transition representation under the existing byte cap. No
implementation of that broader architecture was active at the stopped
checkpoint.

Oliver selected the
[transition-carrier recovery plan](docs/active/2026-08-16-five-t1-restart-monotone-recovery/transition-carrier-plan.md).
Gate 0 is complete. The post-contraction-only `via` authority now lives in a
checked row sidecar, reducing every raw `EvalTransition` from 32 to 24 bytes
while preserving exact double probabilities and all identities. Full/fast,
conversion, attribution, and post-contraction memory ledgers include the
sidecar. The destructive-cycle raw/reference oracle exercises nonzero sidecar
storage and remains identical. The evaluator suite passed 16,823 checks, the
solver suite passed 96,113, and the native build passed. See
[Gate 0 evidence](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/transition-carrier-gate0.md).

Gate 1 is complete. Witness A remains independently evaluated at
624,800.951911854 with success one and zero off-policy mass. At the checked
10-million transition cap, Witness B stops at 9,998,209 raw pairs with an
858,541,852-byte evaluator peak. One temporary 16-million probe also remained
open at 15,998,209 pairs, with a 1,011,645,812-byte peak against the
1,050,981,759-byte evaluator budget. The checked fixture is restored to 10
million. See the
[Gate 1 decision](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/transition-carrier-gate1.md).

Gate 2 is complete and stopped. Fixed-size segmented storage now carries raw
pairs and discovery links, and the 24-byte pair record omits only operation and
action metadata derived exactly from its retained compiled node. The native
build, evaluator suite (16,852 checks), solver suite (96,113 checks), segment
boundary, and raw/reference oracles pass.

Witness A remains independently evaluated at 624,800.951911854, success one,
zero off-policy mass, with a 227.24 ms largest native step. Witness B's checked
10-million run now peaks at 600,881,764 evaluator bytes, down from 858,541,852.
One temporary 18-million probe crossed the former 16.7-million allocation
cliff but still stopped in raw discovery at 17,998,209 pairs and
1,026,151,572 peak bytes against 1,050,981,759. The fixture is restored to 10
million. Detailed evidence is in the
[Gate 2 stop](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/transition-carrier-gate2-stop.md).

The exact next owner is broader than another pair/cap tweak: raw transition
targets must be reduced or streamed before full retention, and the subsequent
partition replay must fit without materializing per-raw-pair caches. No
closed-partition class count exists yet. The priced five-T1 publication remains
the six-node Chaos renewal at 37,279,857.73995944 and is not recovered. Parent
successor Gates 4-8 remain unstarted. No WASM build, web suite, or full
acceptance pipeline was run. Do not resume until Oliver selects a new scoped
plan.

The stopped implementation checkpoint is `3acd2a4`; its Gate 2 evidence is
`8b51928`. Oliver then instructed the work to continue while explicitly
checking prior attempts before selecting an implementation. The new
[streamed evaluator closure plan](docs/active/2026-08-16-five-t1-restart-monotone-recovery/streamed-evaluator-closure-plan.md)
reuses the retained replay partition, rejects open-graph merging and whole-run
checkpoint work, and begins with a behavior-neutral census.

That census is complete. At the checked 10-million cap, Witness B contains
9,998,209 raw pairs: 9,987,873 routers and 10,335 operations. Only 3,965
operation pairs had expanded, no router pair had expanded, and 9,994,243 pairs
were pending. All 9,974,257 retained transition policy states equal their
target-pair states, but only 12,126 transitions use the existing single-root
policy-route compression. Evaluator peak was 600,881,884 bytes against
1,050,981,759. Witness A remained independently exact at 624,800.951911854,
success one, and zero off-policy mass. See the
[Gate 0 census](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/streamed-closure-gate0.md).

Gate 1 therefore selects exact online deterministic routing and not derived
transition routing. It may skip only non-operation, non-modifier-offer routers
with collision-safe exact node/edge traces; deterministic cycles must remain
raw. Do not start parent successor Gates 4-8 or run the full acceptance
pipeline during this boundary.

Gate 1 is now complete at `88cc69e` plus the compact collision-safe trace
index at `7432410`. Focused controls cover long chains, multiple roots, a
385-trace rehash boundary, direct/raw-reference and single-step parity,
modifier offers, and deterministic cycles. The native build, evaluator suite
(18,004 checks), and solver suite (96,113 checks) pass.

The checked Witness B now has 35,828 raw pairs, all but the start pair
operations, and zero router pairs. At ten million it stops with 31,862 pairs
pending, a 239,404,440-byte row payload, a 40,097,520-byte exact trace payload,
and a 362,706,844-byte evaluator peak. Witness A remains independently exact
at 624,800.9519118543, success one, and zero off-policy mass.

The plan's only higher probe used 20 million transitions and was restored to
the checked ten-million cap afterward. It still stops in discovery with
27,588 operation pairs pending, 1,007 rows, 19,972,223 transitions, a
479,377,920-byte row payload, an 80,202,800-byte trace payload, and a
650,793,188-byte peak. The measured slope reaches one GiB near 33.9 million
transitions, while comparable row sharing projects closure much later. No
partition was reached, so Gate 3 did not start. See the
[Gate 1/Gate 2 stop](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/streamed-closure-gate1-gate2-stop.md).

The priced five-T1 publication remains the six-node Chaos renewal at
37,279,857.73995944. The independent exact evaluation in the stopped reports
is of that fallback, not the unmaterialized candidate.

Oliver has now selected the
[replayable operation-row recovery plan](docs/active/2026-08-16-five-t1-restart-monotone-recovery/replayable-operation-row-plan.md).
Gate 0 is complete. Broad stable Chaos rows own 9,962,130 of 9,974,257
measured outcomes; the state-local Exalt fringe owns only 2,632. A 32-bit
route token plus current trace payload is about 80.0 MB at the checked prefix
versus 239.4 MB of routed rows. Raw `(route root, state)` and propagated-
observation cache keys both measured zero reuse; the latter cost 225.7 MB for
only a 1/256 shadow sample and was removed. Deterministic routing projects to
65.8 seconds and is the runtime owner. See the
[Gate 0 census](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/replayable-row-gate0.md).

Gate 1 is complete. It retains immutable stable kernels once under existing
exact authority, stores one compact route-result token per broad outcome,
keeps the tiny state-local fringe direct, and interns collision-safe route
results. At the same ten-million logical boundary, row payload falls from
239,404,440 to 39,949,692 bytes and evaluator peak from 364,521,388 to
167,478,312 bytes. Two stable kernels own 1,310,720 bytes; 279 replayable rows
own 39,886,500 token bytes and reference 240,257 exact route results. See the
[Gate 1 result](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/replayable-row-gate1.md).

Gates 2 and 3 are complete in bounded controls at `5e60994`, with the frozen
attribution-order correction at `942735d` and truthful refinement diagnostics
at `25c60bd`. Tokens feed the existing split-only replay partition directly;
compact projection hashes are bucket selectors only and exact replay equality
remains authoritative. Quotient construction, component input, raw
disaggregation, terminal/route flow, and row-level attribution share the same
row vocabulary. Affordable attribution preserves legacy edge order; only a
carrier that exceeds the actual byte envelope uses target-row aggregation.

The final native build, evaluator suite (18,031 checks), quotient-proof suite
(381 checks), solver suite (96,108 checks), and `git diff --check` pass.
Witness A remains independently exact at `624800.9519118543`, success one,
zero off-policy mass, with a 182.49 ms largest step.

Witness B's checked ten-million run remains
`exact_eval_pair_discovery_transition_cap`: 35,828 pairs, 31,862 pending,
39,949,692 row bytes, and a 167,478,344-byte evaluator peak. The one authorized
temporary 30-million probe was restored immediately. It closes discovery and
partitions 35,828 raw pairs to 1,843 classes, then stops during quotient
conversion before component construction. The exact owner is 596,861,448
bytes of effectively direct row payload plus conversion overlap: evaluator
owned is 939,244,724 bytes, transient is 112,582,028, and measured peak is
1,051,826,752 against the 1,050,980,927 budget. Discovery takes 177.81 s,
pair refinement/conversion takes 99.02 s, total case time is 288.41 s, and the
largest cooperative step is 89.95 s.

The 30-million artifact predates `25c60bd`, so it contains the legacy
`exact_eval_pair_discovery_memory_cap` label and displays the 1,843 quotient
classes as both raw and refined counts. Its internal subphase is
`pair_refinement`; future runs preserve 35,828 raw / 1,843 refined and classify
this as `exact_eval_pair_refinement_memory_cap`. Do not repeat the probe merely
to rewrite telemetry.

The preferred candidate still has no exact cost, success, off-policy, or
component result. Publication remains the independently evaluated six-node
Chaos renewal at `37279857.73995944`; do not describe five-T1 as recovered.
The next plan must audit all prior carrier attempts and address both the later
state-local/direct-row tail/lifetime overlap and replay-partition/quotient-
conversion cooperativity. A cap-only increase is not sufficient. Parent
successor Gates 4-8, release WASM, web acceptance, Warlord/automatic matrix,
the primary publication run, and the full acceptance pipeline remain closed.
Detailed evidence is in the
[Gates 2-4 stop record](docs/active/2026-08-16-five-t1-restart-monotone-recovery/evidence/replayable-row-gates2-4-stop.md).

Oliver selected the next exact scope in the
[proof-gated route and operation-graph compaction plan](docs/active/2026-08-16-five-t1-restart-monotone-recovery/gated-route-evaluator-compaction-plan.md).
The source/artifact audit attributes the 2,015 candidate nodes to 757
operation regions, 755 local gated retry routers, 499 policy-route routers
including the root, and four fixed nodes. Generic policy-router subtree
hash-consing and exact online router skipping already exist. The newly selected
owner is that non-shared local gated routes prevent the evaluator's direct-
self-loop test from selecting the compact goal-progress-gated kernel, leaving
full physical outcome rows in the measured 596,861,448-byte direct tail.

Gate 0 must first attribute gated/full rows, bytes, time, route proof results,
behavior-signature multiplicities, node-identity-blocked row reuse, and
canonical Fracture Q/revisit evidence without changing graph behavior, values,
hashes, selected actions, or cap classifications. Only then may a structurally
proved local-gated-kernel path land. Fracture policy selection is a separate
diagnostic lane and requires an exact cheaper-Q witness before any repair.
Release WASM, the parent successor Gates 4-8, Warlord/automatic acceptance,
the priced primary, and the full pipeline remain closed until Witness B is
independently materializable.
