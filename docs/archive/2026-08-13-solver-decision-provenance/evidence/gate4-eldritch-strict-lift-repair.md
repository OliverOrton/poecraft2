# Gate 4 Eldritch Strict-Lift Repair

**Status: complete on 2026-08-13.**

Parent: [Solver Decision Provenance And Result-Truth Hardening](../plan.md)

## Why Gate 4 reopened

Gate 3 had no remaining failing witness, so Gate 4 initially closed without a
repair. The first proportional Gate 5 native semantic run then produced the
required new witness: the frozen automatic Eldritch Exalt case returned
`bounded_feasible / refused_resource_cap` instead of its accepted exact result.
The primary, Warlord, and Imprint controls still passed.

The failed result retained a proper independently evaluated upper at
`0.018630169563331064`, but correctly published lower zero because strict
closure had failed. It did not make an unearned exactness claim.

## Localization

Two related failures were captured while replaying the exact same case:

1. a sub-tolerance strict policy change reached streamed strict compilation,
   whose sidecar replaced its materialized equivalent-carrier coverage with
   streamed coverage and then correctly refused the omitted represented
   member; and
2. another replay of the same sub-tolerance change requested an already-known
   competitive quotient frontier and refused because the frontier did not
   grow.

The strict row argmin was not the defect. The policy-change gates in local
strict reoptimization and competitive alternative publication had also been
made strict. Those are convergence/stability boundaries: treating a
representably smaller value below their named numerical tolerances as a policy
mutation can repeatedly perturb quotient construction even though objective
selection remains strict.

## Repair

- Strict objective selection remains unchanged: every representably cheaper
  finite row wins the canonical argmin.
- Local strict reoptimization again requires improvement beyond its existing
  absolute-plus-relative reconciliation tolerance.
- Competitive alternative publication again requires improvement beyond its
  existing `1e-12` stability tolerance.
- Streamed compile routing now takes the canonical union of proof-streamed
  strict members and semantically equivalent carriers discovered while
  materializing the representative. It populates routing, policy, and values
  for that complete union instead of overwriting one authority with the other.

A focused unit regression covers canonical equivalent-member union. The
refinement and quotient-proof suites pass with 302 and 357 checks respectively.

## Post-fix witness

The repaired native Eldritch replay closes exact in 89.673 seconds:

| Field | Frozen Gate 8 | Repaired build |
| --- | --- | --- |
| Result / termination | exact / `exact_closed` | exact / `exact_closed` |
| Lower / upper / evaluated cost | `0.018630169563331064` | same |
| Strict lift | complete | complete |
| Exact evaluation | matched; success probability `0.99999999999999911`; zero off-policy mass | same |
| Strategy SHA-256 | `c85cc8ff9e63629775e6e544f07f6ac4936d8525c58b10acba1759989d6bf229` | same |

The current strategy is byte-identical to the frozen strategy, so its existing
10,000-run verification remains authoritative and is not repeated merely to
recreate Gate 8. Ignored raw reports and logs are under
`build/solver-decision-provenance/gate5/`.

Gate 5 must restart from a release WASM rebuild because native source changed
after the first candidate module was built.
