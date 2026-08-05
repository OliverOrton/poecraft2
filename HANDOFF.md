# Session Handoff

**Status: Goal-Gated Semantic Policy Routing is active at Gate 4.**

Active plan:
[Goal-Gated Semantic Policy Routing](docs/active/goal-gated-semantic-policy-routing.md).

Starting commit: `cf67fd982e649193821424a0d8b85ea614027cf3`.

Current branch: `codex/goal-gated-semantic-policy-routing`.

## Objective

Make normal Calculator solves request the existing zero-goal-progress reforge
scope, record that scope as structured non-executable strategy provenance, and
compile solved policies by final executable region rather than irrelevant
junk-carrier identity. The simple goal-or-selected-reforge loop must be proved
from the solved policy and existing renewal machinery.

## Current boundary

Gate 0 is complete in its separate local commit. The supplied condition is
retained byte-for-byte with its required hash; U/J/M/S and A/B are frozen;
focused compile tests pass; and old versus complete compiler-memory telemetry
is observational and leaves strategy hashes unchanged. Point C remains
intentionally unfilled until the new compiler is measured.

Gate 1 is complete in its separate local commit. Normal Calculator solves now
always send `goal_progress_gated_reforges: true`, including without gap
targets. The native/WASM option remains optional and defaults unrestricted.
Compiled strategies carry optional non-executable `solver_policy_scope`
metadata, preserved by the TypeScript model, cloning, persistence, and
comparison path. Focused web tests and native compile/solve suites pass.

Gate 2 is complete in its separate local commit. U now proves a complete
fixed-policy closure directly from the exact solved policy and compiles to the
existing 4-node/4-edge loop at 1,205 JSON bytes and zero junk predicates. Its
policy value and hashes are unchanged; exact evaluation reconciles and 10,000
simulations have zero failures/off-policy. Bounded witnessed renewal remains
4/4. Stale signatures, conflicting carriers, and observation-owned behavior
fall through to the general compiler.

Gate 3 is complete in its separate local commit. Generic quotient and strict
policy routers now classify final executable operation/continuation regions,
minimize only against different regions or off-policy, and emit one root route
per final region. Strict DAG partitions stop when one region remains and use
the existing quotient minimizer with exact-state fallback. Multi-operation
tests preserve Transmute and Alteration plus the explicit off-policy default;
structured observation-owned routing remains unchanged. Focused compile and
solve suites pass, and the evidence records the generic before/after and the
protected body-armour B-to-C result.

Gate 4 is in progress:

- make audited complete compiler-owned bytes the enforced cap value and the
  public `peak_owned_bytes` value;
- retain `previously_accounted_peak_owned_bytes` and
  `complete_peak_owned_bytes` as separate telemetry columns;
- propagate the corrected contract through retained compiled artifacts and
  policy assertions; and
- keep accepted cases under their existing caps without raising limits.

## Preserved boundaries

- No mechanic, price, action-filter, Bellman, V3, abstraction, quotient, cap,
  or unrestricted engine-default changes.
- Partial-progress outcomes remain exact; ordinary, Essence, Harvest, and
  Fossil alternatives remain discoverable.
- Unknown, conflicting, state-local, observation-owned, or unproved routing
  keeps the existing off-policy/bounded fallback.
- No cap increase, push, merge, visual review, five-goal watchdog,
  rare-renewal reconciliation, checkpoint/replay, or general solver scaling.
- Final changed-strategy verification uses exactly 10,000 simulations. The
  full `scripts/test.ps1` pipeline runs exactly once at final acceptance.

## Handoff discipline

Update this file at every stopping point. Each Gate 0-4 boundary receives its
own local commit, all ending with the active agent co-author line. On final
completion, archive the plan/evidence/report, update stable docs, clear this
file to no active boundary, and leave the tree clean.
