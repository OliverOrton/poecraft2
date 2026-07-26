# Session Handoff

**Status: Oliver accepted the gap-directed natural-T1 research and selected
the [exact automatic-action constraint-generation milestone](docs/active/exact-automatic-action-constraint-generation.md)
on 2026-07-25. Gates 0–5 execute in order on branch
`codex/exact-automatic-action-constraint-generation` from source boundary
`49ce94c5bcee077a8e3cbddb1ab9749f00cf8de5`.**

The accepted research plan and report are preserved in the
[dated archive](docs/archive/2026-07-25-gap-directed-natural-t1-research/README.md).
Its raw evidence remains under `build/gap-directed-natural-t1-research/`.

## Current Objective

Separate complete, lightweight state-local automatic-action descriptors from
expensive exact local-kernel construction. Each unresolved descriptor must
retain a cheap admissible lower envelope. Exact separation must materialize any
action that can improve the incumbent or violate the Bellman certificate.

The implementation must preserve:

- `L <= J_pi <= U`;
- complete action identity and eventual exact answers;
- proper executable policy witnesses;
- deterministic results and hashes;
- repricing, goal, carrier, and action-vocabulary invalidation; and
- honest resource stops and resumability.

A restricted-action value is search guidance only. It cannot be reported as a
certified lower bound while unresolved descriptors remain. Generic zero is
safe but fails the utility gate. Heuristic-only admission is forbidden.

## Gate 0 Boundary

The clean accepted source boundary is commit
`49ce94c5bcee077a8e3cbddb1ab9749f00cf8de5`. The current branch is
`codex/exact-automatic-action-constraint-generation`. The documentation
boundary archives the accepted research and creates the selected active plan.

The first implementation step is proof and ownership mapping, not production
behavior:

1. descriptor synthesis and deterministic identity;
2. exact local option-kernel construction and resource ownership;
3. incumbent acquisition and solve-phase scheduling;
4. lower-bound and Bellman-certificate ownership;
5. price and vocabulary invalidation; and
6. native/WASM telemetry and acceptance ownership.

No Path of Exile mechanic, action scope, economy price, product default,
public cap, public ABI, compiler, UI, or tracked corpus change is authorized.
The exact natural two-T1 oracle must not run.

## Evidence Contract

All builds, benchmarks, evaluations, simulations, and tests use a detached
900-second watchdog with process-tree termination and a survivor check. New raw
work belongs under
`build/exact-automatic-action-constraint-generation/`. Reuse the accepted
five-case portfolio and hashes before solving.

Native portfolio acceptance uses one warmup and three equal-cap measured
repetitions with one hard-case worker. The smoke baseline first finite upper is
20,048.350 ms. At the 100M reforge control, the other four cases each reached
200,000 discovered states during their first carrier with `L=0` and no finite
upper.

Do not run routine suites between gates. Run the appropriate complete
acceptance once in Gate 5. Engine solver source changes require rebuilding the
WASM module. Compiled-strategy verification, if required, uses 10,000 runs.
Oliver owns rendered and visual review.

## Stop Conditions

Stop and preserve a clean negative result if:

- no useful cheap admissible descriptor envelope can be proved;
- exact separation cannot keep unresolved actions in the proof-bearing bound;
- qualification does not materially avoid the measured exact kernels/states;
  or
- integration would require an unselected mechanic, action-scope, public-cap,
  ABI, compiler, UI, corpus, or economy change.

Keep this file current at every gate. Commits are local-only and end with:

`Co-authored-by: Codex <codex@openai.com>`
