# Exact Automatic-Action Constraint Generation Report

**Status: archived negative result from Gates 0–5 on 2026-07-25.**

Parent: [Archive record](README.md)

Plan: [Exact automatic-action constraint generation](plan.md)

Boundary: [HANDOFF](../../../HANDOFF.md)

## Decision

Reject automatic-action-only constraint generation as the next production
solver architecture.

The prototype establishes that complete lightweight descriptors can avoid
eager temporary-bench kernel construction without weakening the lower-bound
contract. That intervention is nevertheless insufficient: all four hard
portfolio cases immediately transfer the 200,000-state first-carrier failure
to an ordinary broad reforge. Shipping the automatic-only change would move
the bottleneck without producing a finite executable policy or useful
certified-gap progress.

No prototype source or generated artifact was retained.

## Gate Result

| Gate | Result |
| --- | --- |
| 0 — boundary and ownership | Passed at documentation commit `72c216e`; source boundary `49ce94c`. |
| 1 — descriptor envelope proof | Passed exactness; the useful-enough architecture question advanced to qualification. |
| 2 — isolated separation qualification | Failed: complete temporary-bench deferral exposed an equally terminal ordinary-reforge state explosion. |
| 3 — exact solver integration | Not entered under the Gate 2 stop rule. |
| 4 — native portfolio acceptance | Not entered; the diagnostic portfolio rejected the candidate before acceptance repetitions. |
| 5 — cross-layer acceptance and handoff | Completed as a clean negative result. Source was restored; documentation-only checks replaced irrelevant native/WASM acceptance. |

## Ownership Map

The implementation study mapped the cross-cutting ownership before changing
behavior:

| Concern | Existing owner |
| --- | --- |
| automatic descriptor synthesis and deterministic planner identity | `engine/src/solver_options.cpp`, especially `CalcContext::admit_state_local_automatic_candidates` |
| temporary-bench effect classes and exact option kernels | `engine/src/solver_options.cpp`, including `initialize_temporary_bench_effect_classes` and `CalcContext::option_kernel` |
| per-state admission, pricing, and row insertion | `engine/src/solver_solve_expand.cpp`, beginning at `prepare_state_expansion` |
| exact reforge distribution/work ownership | `engine/src/solver_calc.cpp` and primitive evaluation reached from expansion |
| constructive finite-upper acquisition | `engine/src/solver_solve_constructive.cpp` |
| focused lower/upper phase scheduling | `engine/src/solver_solve_focused.cpp` and `engine/src/solver_solve_bellman.cpp` |
| final convergence and certified-gap publication | `engine/src/solver_solve_finish.cpp` |
| repricing invalidation | `CalcContext::reset_solve_telemetry` in `engine/src/solver_calc.cpp`, which clears price-scoped state-local admission and its graph |
| diagnostic schema and accounting | `engine/src/solver_solve_telemetry.cpp` with state in `solver_internal.hpp` and `solver_solve_types.hpp` |
| downstream native/WASM behavior | shared engine solver source; a retained change would require the standing native and rebuilt-WASM acceptance |

The proposed invariant was: every legal action is represented at each carrier
by either an exact Bellman row or an unresolved complete descriptor; the
proof-bearing lower term is the minimum over exact rows and every unresolved
admissible envelope; upper policies may use exact executable rows only; and a
certificate is impossible until every potentially violating descriptor has
been separated or materialized.

## Admissible Envelope

For a legal temporary-bench-repeat descriptor `d` on carrier `s`, define:

`ell(d, s) = price(mandatory setup primitive of d)`.

Every exact program represented by the descriptor executes that setup
primitive once before any outer exit or retry. Prices and all later exact
cost-to-go terms are nonnegative. Omitting the follow-up, cleanup, retry, and
continuation costs therefore gives:

`ell(d, s) <= Q_exact(d, s)`.

The claim is independent of success probability and self-loop structure.
Envelope values are derived from the current price table rather than retained
across repricing. Descriptor identity remains a deterministic function of the
carrier, goal, action vocabulary, setup/follow-up/cleanup operations, and
retry policy; changes to any of those inputs require fresh enumeration.

An initially tested completion-aware envelope was also admissible, but it
eagerly invoked the same broad reforge preparation work that separation was
meant to avoid. It failed the cheapness requirement and was discarded. The
mandatory-first-price envelope was the only qualified lower term.

## Isolated Prototype

The production-shaped diagnostic:

- enumerated complete temporary-bench descriptors without exact kernels;
- installed lower-only placeholder rows using the admissible envelope;
- excluded unresolved descriptors from upper-policy construction;
- retained unresolved envelopes in lower-bound accounting; and
- could replace a selected placeholder in place with one exact isolated local
  kernel.

This proved the scheduling boundary can express unresolved exact actions
honestly. It did not prove that the boundary is sufficient for the current
product cases.

## Qualification Measurements

The one-case 100M control deferred all 225 temporary-bench descriptors and
built zero exact temporary-bench templates. The next ordinary reforge then
discovered 200,000 states from one expansion, performed 10,145,608 units of
reforge frontier work, and stopped with `L=0` and no finite `U`.

The same result held across the four hard cases:

| Case | Automatic considered / deferred | Temporary-bench candidates / exact templates | Expanded / discovered | Reforge frontier work | Bounds |
| --- | ---: | ---: | ---: | ---: | --- |
| full three | 226 / 225 | 225 / 0 | 1 / 200,000 | 10,145,608 | `0 / none` |
| deep three | 211 / 210 | 210 / 0 | 1 / 200,000 | 8,153,574 | `0 / none` |
| full four | 213 / 212 | 212 / 0 | 1 / 200,000 | 6,120,150 | `0 / none` |
| deep four | 220 / 219 | 219 / 0 | 1 / 200,000 | 6,120,150 | `0 / none` |

Every hard case stopped explicitly at `max_discovered_states`. The smoke case
exceeded its existing 60-second case watchdog and was terminated with no
survivor. Because the hard cases had already failed the predeclared
qualification exit, no warmup plus three-repetition acceptance campaign was
run.

## Evidence

Raw evidence is under
`build/exact-automatic-action-constraint-generation/`.

Key SHA-256 records:

- Gate 1 second build watchdog:
  `327f357c80bdb15cc2bcf71c4fd0466928ac943c33f204ce93b9a47c8302c8a9`;
- Gate 1 full-three 100M ledger:
  `162dfe8720fabb045ea56861de402b5331065ceca617ab26751f3cded59bdb2f`;
- Gate 2 portfolio ledger:
  `aac7a4ae55e08ef795276d1a72944b1c73a991097e7a44661f644c7292dd331b`;
  and
- outer Gate 2 watchdog:
  `6296301cb080b8f14a7c890795e5dc43a18a4d077291d7a2e2797439b9f16d11`;
  and
- final documentation link-audit watchdog:
  `4874fb34cac4f983061cdd493cddac6665e873b1c9606da94eeaa58aa8a411c7`.

Both native builds completed inside the detached 900-second watchdog. The
portfolio watchdog returned the runner's expected nonzero aggregate failure
status after 65.143 seconds, did not itself time out, and recorded no
survivor.

## Acceptance Boundary

The prototype modified native solver internals only during qualification.
Those changes were restored exactly before closure. Consequently:

- no engine source, C ABI, strategy vocabulary, data, binding, WASM, web, or
  generated artifact changed;
- a WASM rebuild and native/cross-layer suites would not validate any retained
  behavior and were not run;
- the prohibited exact natural two-T1 oracle was not run; and
- final acceptance is documentation link integrity, whitespace validation,
  source-delta verification, and a clean tracked tree.

The final link audit checked 871 local Markdown targets with zero missing
targets, completed inside the detached watchdog, and recorded no survivor.

## Recommended Next Boundary

Do not resume by merely landing the automatic descriptor machinery. The next
solver chunk, if Oliver selects it, should first research a broader exact
action-separation architecture that covers both automatic programs and
ordinary broad stochastic kernels. It must be coupled to constructive
upper-policy acquisition: the cheap immediate-price lower envelope alone
cannot safely choose which enormous ordinary kernel to materialize before a
finite incumbent exists.

Two plausible design axes require fresh proof and measurement:

1. generalized exact action descriptors and separation across broad ordinary
   reforge operators, with unresolved terms retained in the certified lower
   bound; and
2. a compact exact outcome/kernel representation that avoids enumerating the
   200,000 strict successors of the first broad reforge.

This archive does not select either axis or authorize implementation.
