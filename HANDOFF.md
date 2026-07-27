# Session Handoff

**Status: active implementation boundary.** Oliver selected
[Streaming Broad-Lower Fold Falsification](docs/active/plan.md) on
2026-07-27. Execute Gates 0 through 4 in order on
`codex/streaming-broad-lower-fold`.

## Current Boundary

Test one hypothesis in shadow mode before creating a solver architecture:

```text
Can c(s,a) + E[H_coarse(X)] be computed before successor interning,
inside the existing 11M work cap, strongly enough to avoid immediately
materializing the ordinary broad row?
```

The first implementation is measurement-only. It observes fully constructed
final `AbstractState` values immediately before `intern_state`, evaluates only
the graph-independent coarse optimistic lower, retains no successor support,
and leaves production Bellman behavior, counters, caps, policies, and
termination unchanged.

Gate 2 rejects the direction if the fold is incomplete, nonselective, or
immediately followed by the same exact materialization at worse combined work.
A raw broad-action lower increase is not success.

Exactly one Gate 3 path follows:

- a complete pass may integrate one immutable lower-only scalar record, with
  no detached table or promotion; or
- any failure restores measurement-only source, rejects further broad-kernel
  work, and pivots to versioned constructive/fallback properness-proof reuse.

## Source And Repository State

Local `main` was fast-forwarded to the completed anytime benchmark closure
`e27b45e` before this branch was created. That milestone supplies durable
incomplete trajectories, v2 experiment identity, corpus roles, and the fresh
smoke baseline. Nothing was pushed.

The current-solver path remains CPU-native. GPU use remains relevant only to
future ML work. No mechanic, action scope, cap, ABI, data, economy, binding,
web, or visual change is selected.

## Acceptance Boundary

Pin selected smoke/full/deep cases and identities before the first candidate
run. Use fixed production caps, one worker, existing watchdog/survivor
handling, deterministic work counters, and machine/compiler disclosure.
Do not run the exact natural two-T1 oracle.

Run the appropriate affected acceptance once after the conditional Gate 3
result. Rebuild WASM only if retained native behavior reaches the browser path.
Run exact policy evaluation and the standing 10,000 simulations only if a
retained candidate changes or newly qualifies a policy.

Commits remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
