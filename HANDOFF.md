# Session Handoff

**Status: selected implementation boundary at exact-evaluator attribution and
behavior-keyed product-local Fracture sharing.**

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

The next work is Gate 0 evaluator subphase timing and diagnostic telemetry,
followed by Gate 1 compiler sharing of product-local Fracture regions keyed by
complete executable behavior. The accepted 1,813-node four-goal graph has 767
Fracture routers but only seven route behaviors; this reduction may directly
shrink the evaluator carrier. Rerun the priced witness only at the Gate 2
decision point. Choose compact/interned/streamed pair work only if that run
still stops, and base the choice on the newly measured owning payload and
actual refined-class boundary. Preserve the current tree and checkpoints.
