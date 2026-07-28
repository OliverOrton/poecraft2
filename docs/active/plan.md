# Certified Root-Action Feasibility

**Status: active, falsification-first solver research.**

Owner: Oliver

Branch: `codex/certified-root-action-feasibility`

Starting source: `0fe0673` (`main`)

## Decision To Make

Determine whether the current exact executable-upper and admissible-lower
machinery can certify the next executable craft before a hard natural-T1 solve
materializes its first cap-failing broad row.

This is a materially narrower objective than certifying `V(start)`. It is not
assumed to be easy. The preceding broad-action study already showed that a
finite renewal upper plus immediate-price operator lowers does not separate
the hard starts: Chaos retained lower term `1` against incumbents from
`575,497` to `193,266,777`. This pass must measure the actual root-action
intervals before adding a public result type.

The current constructive state certificate is a proof primitive, not the
proposed product:

- it is disabled during focused search;
- it requires one complete retained row whose non-self successors already
  have constructive uppers;
- it compares one planner operator with the remaining operator lowers only
  while unevaluated operators remain to prune;
- it records a diagnostic witness but no durable root-action result or
  termination; and
- it does not group different planner programs by the concrete first craft
  the user would execute.

## Gate 0 - Shipping Contract

### Certified object

A **root action key** is the complete first executable primitive craft request,
including every parameter that changes execution. Planner operators and fixed
programs that begin with the same request belong to the same root-action
class. An operator with no exact first-action projection blocks certification;
it is not silently discarded.

For every admitted, legal, and price-complete root planner operator `o`, retain:

- an admissible action-local lower `L_o <= Q*(start, o)`; and
- when available, a certified executable policy upper `U_o >= Q*(start, o)`.

For a root-action class `A`:

```text
L_A = min L_o over operators in A
U_A = min U_o over executable witnesses in A
```

Class `A*` is certified only when `U_A*` is finite and:

```text
U_A* + comparison_tolerance < min L_B over every B != A*
```

An unavailable, invalid, or negative-infinite competing lower prevents the
certificate. Positive infinity is allowed only for a proved impossible
operator or class. Ties and tolerance-overlapping intervals produce no unique
root-action certificate.

### Meaning

The certificate proves that an optimal policy in the solver's disclosed
action/economy/goal scope begins with the reported executable craft. It does
not prove the exact optimal value, the uniqueness of the downstream policy,
or the exactness of the incumbent's later actions.

Missing prices, unsupported legal actions, or a caller-selected restricted
action envelope must remain explicit. They cannot produce an unqualified
product claim. A certificate is price-, goal-, item-, action-vocabulary-, and
solver-version-specific and is never reused across a changed identity.

### Required witness

A durable result must contain:

- certified/unavailable status and a distinct termination when an opt-in solve
  stops on the certificate;
- concrete root action key and display identity;
- selected planner-operator witness and executable incumbent provenance;
- `U_A*`, the minimum competing `L_B`, strict margin, and comparison
  tolerance;
- every competing root-action class lower, with the operator attaining it;
- admitted/skipped/unpriced/unsupported action-scope status;
- wall time and deterministic work at first certification; and
- enough identity to reproduce the exact case, economy, artifact, executable,
  compiler, and machine.

Wall time is machine/compiler-bound. Deterministic work is the portable
comparison.

### What counts as shipped

The feature is shipped only if the native result, C ABI, WASM facade, typed
worker/client path, and non-visual Calculator result all preserve the contract
above; opt-in root-action stopping returns the certificate without labelling
the value or downstream policy exact; exact-mode behavior remains unchanged;
and affected native plus downstream acceptance passes.

The hard-case objective is stronger and reported separately. It is achieved
only if all four frozen hard representatives certify before materializing
their known cap-failing first Chaos row under unchanged product caps. A partial
result may still ship as an honest capability, but it must not be described as
solving the general natural-T1 wall.

## Frozen Portfolio

Use:

- the small constructive bench certificate oracle;
- `natural-t1-smoke-dire-pelt-three`;
- `natural-t1-full-three-24920b3b28de`;
- `natural-t1-deep-three-low-probability-af4719c816f3`;
- `natural-t1-full-four-47d8b909aa88`; and
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

Pin source, case hashes, artifact manifest, natural-T1 generator-config hash,
economy hash, executable hash, compiler, machine, and unchanged caps. Do not
run the expensive natural two-T1 exact oracle.

## Gate 1 - Shadow Root-Interval Audit

Add the smallest internal, measurement-only observation needed to evaluate the
Gate 0 class intervals:

1. enumerate the complete admitted root operator envelope and exact
   first-action projection before broad-row evaluation;
2. observe every finite executable incumbent and the existing admissible
   operator lower at that moment;
3. group by root action and record the full interval table, the selected
   witness if any, the lowest blocking class, and why each unavailable bound
   is unavailable;
4. record whether the observation occurred before, during, or after first
   broad-row materialization and the deterministic work already spent; and
5. change no Bellman value, row order, cap, policy, or termination.

Run the frozen portfolio once after a smoke oracle proves the diagnostic
faithfully reports the current constructive certificate.

Exit paths:

- **qualified:** at least one previously refused hard case obtains a strict
  root-action certificate before its cap-failing Chaos row, with no restricted
  or incomplete action scope; proceed to Gate 2;
- **rejected:** no hard case qualifies, or every apparent certificate depends
  on an incomplete action envelope, post-cap evidence, or a fully materialized
  broad row; restore the probe and close with the measured blocking intervals.

A finite incumbent, a larger global lower, an action ranking, or a stable
guess is not a Gate 1 pass.

## Gate 2 - Internal Certificate And Stop

Enter only after Gate 1 qualifies.

1. Promote the root-action class construction and interval comparison into a
   price-scoped internal proof object.
2. Re-evaluate it only at bounded events that can improve a class upper or
   lower.
3. Preserve every competing admitted operator in its class lower.
4. Add an opt-in stop condition; do not change exact-mode defaults.
5. Retain the executable incumbent used for `U_A*` so a bounded policy remains
   available when it is already valid, while keeping root-action exactness
   separate from downstream-policy exactness.
6. Prove parity with the certificate disabled and prove ties, missing lowers,
   repricing, changed goals/items, fixed programs, and same-first-action
   operators cannot yield a stale or false certificate.

Exit: a native stepped solve can return a typed, reproducible root-action
certificate before exact value closure without weakening any existing result.

## Gate 3 - Public And Product Delivery

Enter only after the internal proof passes Gate 2 and materially improves at
least one refused frozen case.

1. Extend the versioned solver result/options contract with opt-in
   root-action stopping, typed certificate fields, and a distinct termination.
2. Update the C facade, WASM JSON, TypeScript protocol, worker/client, and
   Calculator result model. The frontend presents native evidence and gains no
   mechanic or proof authority.
3. Label the result as a verified next action. Keep exact value, bounded
   policy, and verified-next-action claims visibly distinct.
4. Preserve cancellation, stepped progress, partial-trajectory snapshots, and
   compile-policy eligibility.

Oliver owns rendered review. No screenshot or visual acceptance runs unless he
explicitly requests them.

## Gate 4 - Acceptance, Evidence, And Handoff

- Run the affected native solver suite once after implementation.
- If Gate 3 changes the browser-visible native contract, rebuild release WASM
  and run the affected non-visual WASM/web tests and TypeScript check.
- Run 10,000 compiled-strategy simulations only if the retained change creates
  or changes a compiled policy.
- Compare enabled/disabled exact controls and require identical exact
  value/policy/transition hashes when no early root stop is requested.
- Run the frozen portfolio under unchanged caps and report deterministic work,
  wall time, first-certificate point, interval margin, termination, failures,
  and whether each known first Chaos row was avoided.
- Restore unqualified measurement source, retain tracked evidence, update
  stable solver/evidence/roadmap docs, archive the plan, clear `HANDOFF.md`,
  and commit locally with the required co-author line.

## Stop Rules

Stop without public integration if:

- no finite executable root-class upper exists before the first cap failure;
- the cheapest competing class lower still overlaps the best executable
  upper;
- exact first-action projection for the admitted planner vocabulary is not
  well-defined;
- certification arrives only after the work it was meant to avoid;
- the result depends on raising caps, deleting actions, treating missing
  prices as impossible, or trusting a learned/heuristic estimate as a bound;
  or
- the only gain is a diagnostic label on an already exact result.

## Out Of Scope

- Raising state, transition, memory, or reforge-work caps.
- Another state quotient, cleanup equivalence, side factorization, or compact
  broad-kernel representation.
- Mechanics, goal, or condition changes.
- GPU work or ML guidance.
- A new approximate objective.
- Full-value certification improvements that do not advance root-action
  certification.
