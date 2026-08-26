# Gate 5 — Pattern-database and Proof-owner Refactor

**Status:** passed on 2026-08-25. This gate changes ownership and
observability only; it does not add a proof value or enable a pruning
consumer.

## Typed ownership boundary

`ProofPatternManager` now owns the existing universal cover, clean MDP,
carrier MDP, terminal-debt floor, strict-clean pattern, and operator lower.
Every registered pattern declares its finite projection, covered action
shapes, local fallback, immediate-price authority, optimistic successor
authority, solution form, residual/convergence state, provenance, and selected
owner count. Public completion lowers are composed only as the maximum of
independently admissible `ProofLowerValue` contributions. Invalid available
components preserve the existing zero fallback.

Ordering and executable-carrier planner projections remain non-convertible to
proof values. No planner estimate, scheduling score, or newly introduced
pattern value can enter the lower bound.

The former monolithic bounds source is split by proof responsibility:

- `solver_solve_bounds.cpp`: universal cover and clean MDP;
- `solver_solve_carrier_pattern.cpp`: carrier MDP, carrier-local eligibility,
  terminal debt, and maximum composition;
- `solver_solve_strict_pattern.cpp`: strict-clean pattern; and
- `solver_solve_operator_proof.cpp`: operator and carrier Bellman lowers.

`solver_eval.cpp` is unchanged.

## Inequality and telemetry controls

A dedicated proof-pattern test route exhausts a finite three-state Bellman
fixture over every integer value assignment in `[0,4]` for every registered
pattern owner. It also checks complete contract metadata, maximum/tie owner
selection, and zero-fallback behavior. The focused route passed 196 checks
with zero failures.

Native telemetry reports the manager contract and observed solution state.
On the owner fixed-work control, the clean MDP converged in 231 sweeps with
residual `6.9469763275265e-11`; the carrier MDP converged in 24 sweeps with
zero residual. Universal cover, terminal debt, and operator lower report their
closed one-pass/exact composition contracts. The unavailable strict-clean
specialization remains explicitly unconverged with no selected ownership in
that Eldritch solve.

## Fixed-work parity

Gate 5 was compared directly with the post-fallback Gate 4 reports:

| Control | Lower | Verified upper | Graph | Result classification | Published strategy SHA-256 |
| --- | ---: | ---: | ---: | --- | --- |
| fixed four-of-five | 3.47245 | 59,810.9537769745 | 51 / 149 | `refused_state_cap` | `12e9b2dd308b9a7994fb906764d0f430603cf8d14e860074927580554ef4beb4` |
| owner four-to-five | 3.47245 | 2,698.87479601436 | 215 / 563 | `bounded_feasible` | `2062ec8b3400fd5cb54538ce359b22821e681dfa11049c4cab5a8f696eac5db5` |

Both controls are bit-for-bit equal to Gate 4 in lower, verified upper,
compiled node/edge count, strategy JSON byte count, core policy hash, graph
identity, root action, solver status, and termination classification. The
published strategy hashes also reproduce Gate 1/Gate 2 authority. Independent
exact evaluation remained the publication boundary; sampled simulation was
not run.

## Evidence files and cadence

Raw reports and emitted strategies are retained outside tracked documentation
at `build/performance/gate5-proof-pattern-manager/`:

- `gate5-fixed.json`;
- `gate5-owner.json`;
- `conquest-lamellar-allflame-partial-five-capped-diagnostic.strategy.json`;
  and
- `conquest-lamellar-allflame-partial-five-last-mile-current.strategy.json`.

The release native engine built successfully, including all split translation
units and the test route. Only the narrow proof-pattern fixture route was run
at this gate. No WASM build, web suite, rendered review, sampled verification,
or full repository acceptance pipeline was run; those remain governed by the
plan's later gates and final cadence.
