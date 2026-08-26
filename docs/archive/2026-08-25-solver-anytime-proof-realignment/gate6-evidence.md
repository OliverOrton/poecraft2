# Gate 6 — Stronger Carrier/action Proof Patterns

**Status:** passed on 2026-08-25. The retained values publish only during
finalization. They do not schedule, prune, admit, or materialize work; Gate 7
owns the first consumer.

## Retained proof patterns

`ProofPatternManager` now owns three additional independent patterns:

- `identity_clean_mdp` fixes the exact source influence, Eldritch,
  protection, and Fracture identity, then reuses the rarity × exact goal mask
  × prefix/suffix occupancy relaxation. Exact identity-changing successors
  fall back locally. Its probability authority clears explicit sides before
  pool construction, excludes already-satisfied goal groups, and grants free
  cleanup/perfect preservation; those are favorable grants rather than
  mechanics claims.
- `bounded_gain_mdp` is a compact rarity × satisfied-goal-count debt
  projection. It permits the most favorable source mask, perfect
  preservation, and probability upper bounds, but permits at most one newly
  satisfied goal per affix draw. It replaces the unexplained 32-lift carrier
  cap with a residual-aware monotone solve and a defensive 4,096-sweep
  ceiling. A ceiling hit remains an interrupted subsolution and cannot report
  convergence.
- `envelope_bellman` audits every admitted, materialized start-state ordinary
  and automatic row after policy certification. It composes exact immediate
  cost with independently admissible successor patterns, eliminates self
  mass, treats a proved all-self row as nonproductive, and takes the complete
  operator minimum. Missing row coverage falls back to the independently
  global state pattern rather than strengthening by assertion.

The envelope also retains per-action clean/carrier floors and a scratch
`CalcContext` for counterexample-guided diagnosis. Scratch refinement does not
intern states or rows in the main solve. The minimizing row, coverage census,
fallback reason, optimistic grant, residual, contribution, and compact
refinement trace are reported.

An optimistic authored-program automaton now computes the cheapest complete
authorized runtime path, thereby charging every unconditional primitive and a
no-stronger minimum conditional suffix. It is deliberately not a pruning
consumer at this gate; the established first-step lower remains active until
Gate 7.

## Rejected experiments and preserved boundary

Two measured experiments did not qualify and are not retained:

- a compatible guaranteed-Harvest probability partition did not tighten the
  controlling lower; and
- strict rare/Eldritch extension had no complete automatic-option coverage on
  the anytime path.

The original independent Harvest upper remains in force. The strict pattern's
Eldritch guard remains explicit with fallback reason
`eldritch_option_coverage_unavailable`. No mechanic rule, evaluator outcome,
automatic option, or exact-evaluation boundary changed.

## Proof qualification

The dedicated native proof route now passes 302 checks with zero failures.
It exhausts the finite three-state Bellman fixture for every registered
pattern and separately checks ordinary/automatic row inequalities, exact goal
zero, nonnegative debt, price monotonicity, action-set monotonicity, maximum
composition, unknown local fallback, and invalid-claim zero fallback.

Every qualified product start had complete materialized-row coverage:

| Control | Admitted rows | Finite/proved rows | Minimizing operator |
| --- | ---: | ---: | --- |
| clean zero→five | 5 | 5 | `scour` |
| owner fractured four→five | 10 | 10 | Eldritch suffix Exalt side option |

The release WASM artifact was rebuilt. Native and WASM certified lowers are
bit-equal on both controls. The owner upper is bit-equal; the clean upper
differs by `6.52e-8`, within the case's exact-evaluation tolerance. Both WASM
exact evaluations matched. A pre-existing WASM benchmark-harness mismatch
classified `requested_bounded_finish` as unnamed; the harness now mirrors the
native named-stop contract, and the owner control reran with all expectations
passing. No web test suite was run at this gate.

## Numerical and policy qualification

| Control | Gate 5/prechange lower | Gate 6 lower | Evaluated upper | Native graph | Native wall ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| clean zero→five | 36.4286171890906 | 36.4885317287664 | 14,454,067.42607058 | 514 / 1,788 | 73,687.384 |
| owner fractured four→five | 3.47245 | 36.4286171891044 | 2,698.87479601436 | 215 / 563 | 22,112.086 |

Both independently evaluated uppers, result classifications, compiled graph
sizes, and strategy byte counts match their retained controls. The clean wall
increase over the 73,166.569 ms prechange run is 0.71%; its solver-owned memory
estimate decreased from 315,856,764 to 314,557,422 bytes. The owner remains in
the same proportional wall/memory class. Search trajectory and policy remain
unchanged because the stronger envelope is installed only after search and
certification.

## Evidence and cadence

Raw qualified reports and all rejected intermediate experiments are retained
under the ignored `build/performance/gate6-proof-pattern-experiments/`
directory. The final qualified reports are:

- `gate6-qualified-native-clean.json`;
- `gate6-qualified-native-owner.json`;
- `gate6-qualified-wasm-clean.json`; and
- `gate6-qualified-wasm-owner-fixed.json`.

The release native and WASM engines built successfully. Only the narrow proof
route and targeted native/WASM controls ran. Sampled verification, the
nonvisual web suite, TypeScript suite, rendered review, and full repository
acceptance pipeline remain deferred to their planned final cadence.
