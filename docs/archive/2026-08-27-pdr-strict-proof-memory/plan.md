# PDR Strict-Proof Memory Attribution And Repair

**Status: stopped at Gate 1 on 2026-08-27.** The existing coarse checkpoint
proved insufficient for the open incremental scheduler. See the
[result](result.md).

Parent: [Documentation archive](../README.md)

## Fixed Witness

Use only
`conquest-lamellar-allflame-clean-4-pdr-product8` from the dedicated
`fixtures/solver-exact-same-side/v1/gate4-manifest.json` corpus unless a small
native unit fixture is required to prove a local invariant. The product scope
is unchanged: Calculator product profile, generated Imprint programs off,
economic Restart off, goal-progress-gated reforges on, exact junk-free
terminal success, 50,000,000 logical reforge work, and 1 GiB solver-owned
memory.

The matched starting boundary is one strict frontier insertion, two completed
alternative rows, 3,507,568 logical strict reforge work, 846,846,750 retained
proof/quotient bytes, and a `max_solver_owned_bytes` stop. The independently
evaluated bounded upper is `7866.432124027084`; the certified lower is
`21.772459401271156`.

## Gates

1. Save one current-source coarse checkpoint and replay that exact PDR request
   in a separate process. Confirm graph reuse and deterministic coarse graph,
   incumbent, strict-stop, and exact-evaluation identities. If the checkpoint
   cannot be saved at the named resource stop, repair checkpoint lifecycle
   only; do not weaken its completeness checks.
2. Attribute retained bytes at the first frontier and the second carrier
   generation to concrete proof-store, quotient, kernel/row, dependency,
   carrier-state, and incidental-capacity owners. Report logical sizes and
   allocated capacities separately where they differ. Instrumentation must be
   behavior-neutral and bounded in compact telemetry.
3. Explain the measured amplification: identify the action/operator families,
   carrier classes, rows, and dependency generations responsible for retained
   growth, and determine whether memory is live proof authority, duplicate
   exact identity, or reclaimable historical generation data.
4. Implement the narrowest proved repair supported by Gate 3: exact reuse,
   earlier exact retirement/reclamation, a cheaper equivalent representation,
   or a successor-complete admissible rejection. Do not substitute a generic
   cap increase, unproved carrier merge, or previously falsified coarse-lower
   transfer.
5. Replay the same checkpoint and compare against the starting witness. Accept
   only if values, bounds, action scope, policy/graph identities where closure
   permits them, and independent exact evaluation remain truthful while
   retained proof/quotient memory materially falls or the case advances to a
   later named exactness boundary. Run focused native tests and only the
   downstream acceptance required by the files changed.

## Stop Conditions

- Stop with a precise handoff if the measured owner requires a new mechanic
  ruling, an unproved state equivalence, or a first-strict-partition checkpoint
  rather than the existing coarse format.
- Do not run a broad benchmark matrix, five-goal tuning, rendered browser
  review, or repeated cap-only continuations.
- Do not claim exactness from the bounded incumbent or restricted coarse value.

## Outcome

Gate 1 demonstrated that the named witness requires scheduler state outside
the shipped coarse format. The first stop condition fired. Gates 2 through 5
were not entered and no source repair was accepted.
