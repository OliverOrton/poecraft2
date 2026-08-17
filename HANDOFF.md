# Session Handoff

**Status: replayable exact operation-row recovery selected on 2026-08-17.
Gate 0 row-ownership and replay-cost census is active.**

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
Gate 0 is active. It must first attribute unique operation rows, exact outcomes,
routed entries, route reuse, bytes, and active time by action/family and
stable-shared versus state-local authority. The census selects exactly one
carrier only if it projects below both the 84.5-million-entry closure byte
boundary and the downstream replay-time boundary.

The existing split-only replay partition remains the quotient authority.
Exact attribution currently retains the raw graph and builds a full
transpose, so a discovery-only recipe is incomplete: partition, quotient
conversion, component solving, raw attribution, and final route-flow reporting
must share the replacement row vocabulary. Parent successor Gates 4-8,
release WASM, web acceptance, and the full acceptance pipeline remain closed
until Witness B is independently materializable or this plan records a new
precise stop.
