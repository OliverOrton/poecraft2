# Session Handoff

**Status: successor Gates 0-1 passed; the priced five-T1 Gate 2 decision point
is the active implementation boundary.**

The original
[Five-T1 Restart-Monotone Strategy Recovery](docs/active/2026-08-16-five-t1-restart-monotone-recovery/plan.md)
hit its explicit Gate 4 stop condition. Oliver approved the
[selected successor](docs/active/2026-08-16-five-t1-restart-monotone-recovery/successor-plan.md)
after reviewing the follow-up evidence. Do not resume unstarted gates from the
old plan, raise the solver/WASM cap, or describe the five-T1 priced-base
strategy as recovered.

The native source checkpoint is `705c25c` on
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

Next run Witness A once and priced Witness B once for Gate 2 with all declared
caps and prices unchanged. If B certifies, skip Gate 3. If it still stops,
choose exactly one compact/interned/streamed pair-representation change from
the new owning payload and actual refined-class boundary. Do not infer a count
cap from raw pair count, rebuild WASM yet, or start later action-semantics work.
Preserve the current tree and checkpoints.
