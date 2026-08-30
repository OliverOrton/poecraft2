# Handoff

**Status: one implementation boundary is active.**

## Active Boundary

[Carrier-Ladder Resumable Joint-Policy Continuation v1](docs/active/2026-08-30-carrier-ladder-resumable-joint-policy-continuation-v1/README.md)
uses the ordinary-interleaving engine at
`b690cad1e376baa5b267603f5cb619c9596a4f94` through clean evidence/WASM child
`a270bd35902546aac09ba41d6187f4f15e1bafa7`.

The saved winner occupies no Imprint or replacement-base action. Independent
exact evaluation under the pinned current economy remains
`85408.64362148794` Chaos with success probability one and zero off-policy
mass. A fresh fixed 180-second no-Imprint run independently produced
`85408.46200624379` Chaos.

## Current Implementation Boundary

Implement at most one snapshot-pure resumable ordinary joint-policy candidate.
When its exact selected-policy traversal names a missing continuation, request
normal exact service, yield to ordinary interleaving, then resume the same
candidate cursor and fixed prefix. Do not retain carrier ownership, drain an
automatic frontier, or reconstruct the complete global candidate after every
serviced state.

Phase 1 located the discarded state in `try_install_reachable_incumbent`: a
missing state is handed to ordinary exact refinement, but stack-local selected
rows, observation routing, reachability, and traversal cursor are destroyed by
`restore`, so the next checkpoint reconstructs the complete attempt. Phase 2
now retains those candidate-local concepts in an internal snapshot-pure state
machine. Its focused two-resume fixture passes 51 checks with zero failures.

Next connect the object only to the existing benchmark-private diagnostic,
retain one candidate from an actual ordinary missing-prefix attempt, and emit
bounded identity/resume/yield/row/work/byte lineage. Do not run PDR until the
native diagnostic itself can demonstrate a second wait/resume without a new
capture. Then run one PDR causal witness; clean-five still follows only if PDR
passes.

## Retained Repository State

- The production selection/scheduler path remains behaviorally unchanged from
  `b690cad`; the retained source addition is an unconnected internal contract
  plus focused fixture. All rejected fixed-window and sticky-witness changes
  exist only in ignored evidence.
- The user-requested rebuilt
  `bindings/wasm/dist/poecraft_engine.wasm` is the only retained binary change
  at this handoff. No test result is claimed for it; Oliver owns frontend
  testing.
- The exact PDR reference is `7852.71432971444` Chaos and strategy SHA-256
  `f1b6c20001bd29b75346176852b4fc94b00d68167e5ffb005ed83bfaf01aa61e`.
  Clean-five hard gates are `98220` Chaos and `10000` expected actions.
- Work is local-only. Do not push.
- Protected untracked `0` is user state. It was not read, modified, staged,
  moved, cleaned, or deleted. Preserve it exactly.
- Reproduce the current focused checkpoint with
  `build/engine/poecraft_engine_tests.exe --solver-joint-policy-continuation-only`.
  Last result: 51 checks, zero failures. Broad tests, long solves, WASM rebuild,
  and Simulator are unrun for this checkpoint.
