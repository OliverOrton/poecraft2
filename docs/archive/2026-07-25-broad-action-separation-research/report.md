# Broad-Action Separation And Renewal Research Report

**Status: archived negative result from Gates 0–5 on 2026-07-25.**

Parent: [Archive record](README.md)

Plan: [Broad-action separation and renewal research](plan.md)

Boundary: [HANDOFF](../../../HANDOFF.md)

## Decision

Reject broad-action separation plus pre-expansion renewal as the next
production solver architecture.

The research found a valid reusable primitive: an exact fixed-policy reforge
success evaluator can sum goal mass without interning every failed successor,
and a repeat-until-success destructive renewal then has expected cost `c / p`.
That primitive produced a finite executable-policy candidate on every hard
case under a diagnostic 100,000,000 reforge-work allowance.

It does not solve the selected product failure under current contracts.
Immediate-price envelopes exclude zero broad actions even after the incumbent
exists. Chaos remains the cheapest unresolved lower term, so exact separation
still has to materialize its kernel and reproduces the 200,000-state wall.
The fixed-policy evaluator fits the current 11,000,000 reforge-work allowance
on the two four-mod cases, but not on either three-mod case. The smallest exact
compaction preserves the measured probabilities yet still needs 14,815,748
and 48,409,673 diagnostic work on the two three-mod cases.

No candidate therefore materially avoids the measured failure across the
four-case acceptance boundary. No prototype source or generated artifact was
retained.

## Gate Results

| Gate | Result |
| --- | --- |
| 0 — boundary and baseline | Passed at documentation commit `9fef501`; source boundary `bc9ee23`. |
| 1 — descriptor and impossibility analysis | Passed the proof analysis; rejected immediate-price separation as a material-avoidance mechanism. |
| 2 — pre-expansion renewal upper | Proved and measured exact finite candidates on all four cases under 100M diagnostic work; two exceed the product's 11M work cap. |
| 3 — separation with an incumbent | Rejected: zero actions are excluded by the cheap envelope, and Chaos still forces the first 200,000-state kernel. |
| 4 — compact-kernel feasibility | Fixed-policy-only compaction is exact but insufficient; general Bellman compaction is not justified by the observation contract. No production architecture selected. |
| 5 — closure | Diagnostic source restored exactly; documentation-only acceptance and archive completed. |

## Gate 0 — Ownership And Proof Map

The investigation mapped these existing owners:

| Concern | Existing owner |
| --- | --- |
| primitive registry order and action identity | `engine/src/solver_registry.cpp` and registry construction reached from `solver_calc.cpp` |
| ordinary reforge distribution and frontier work | `engine/src/solver_reforge.cpp` |
| abstract-state interning and exact observations | `engine/src/solver_calc.cpp` and `solver_internal.hpp` |
| start preparation and ordinary row scheduling | `engine/src/solver_solve.cpp` and `solver_solve_expand.cpp` |
| constructive renewal fallback | `engine/src/solver_solve_constructive.cpp` |
| focused bounds and final certification | `engine/src/solver_solve_focused.cpp`, `solver_solve_bellman.cpp`, and `solver_solve_finish.cpp` |
| fixed-option/renewal compilation | `engine/src/solver_compile.cpp` and the strategy compiler sources |
| diagnostic accounting | `engine/src/solver_solve_telemetry.cpp`, `solver_solve_types.hpp`, and `solver_internal.hpp` |

The standing proof obligations were:

- every unresolved legal action remains in the proof-bearing lower minimum
  through an admissible lower envelope;
- only an exact executable policy may supply `U`;
- a repeat-until-success renewal must return to a carrier with the same
  behavior for the repeated action after every failure; and
- a compact Bellman kernel must preserve every observation available to every
  admitted continuation action, not only goal success.

The reused baseline had each hard case expand one state, discover exactly
200,000 states in its first ordinary broad reforge, and stop with `L=0` and no
finite `U`. Reforge frontier work was 10,145,608 for full-three, 8,153,574
for deep-three, and 6,120,150 for each four-mod case.

## Gate 1 — What Separation Can Prove

Chaos is the first ordinary broad operator on all four traces. For any legal
unresolved ordinary action `a` at carrier `s`, the cheapest envelope available
without successor generation is its mandatory immediate current price:

`ell(a, s) = price(a)`.

Every execution pays that price once and all continuation costs are
nonnegative, so `ell(a, s) <= Q(a, s)`. While `U` is infinite, this envelope
cannot exclude any action. After Gate 2, the smallest hard-case incumbent is
`575,497.52262412792`, while the largest single primitive price in the pinned
economy is `14,619`. Cheap envelopes therefore exclude zero operators on all
four cases.

An unresolved action cannot be removed merely because another action has a
finite executable upper. Its hidden exact continuation could lie anywhere
above its lower envelope and below the incumbent. Removing it would replace
the minimum over all legal actions with a restricted-action minimum and could
raise `L` above the true optimum. In these traces, Chaos retains lower term
`1`; lifting the start lower bound requires its exact row. The prior baseline
then proves that materialization reaches the same 200,000-state wall.

The existing 200,000-successor observation study reinforces the materialization
limit, but a 2026-07-27 quotient audit corrected its interpretation. All four
graphs were incomplete, so exact quotient refinement did not run: the working
count remained at the 200,000 strict identities. The recorded 199,981,
199,967, 199,983, and 199,976 counters are incomplete shadow splits against
literal observed row payloads, not completed exact non-equivalence proofs.
Generalized separation can reorder work, but the available cheap descriptor
still cannot avoid the first huge kernel or produce a stronger certified lower
bound.

## Gate 2 — Exact Fixed Renewal

The isolated native diagnostic evaluated one destructive reforge before
ordinary expansion. It reused the exact roll frontier but accumulated only
total final mass and goal final mass. Failed final items were not interned.
For an action whose failures wipe back to the same preserved carrier, repeated
attempts satisfy:

`V = c + (1 - p)V`,

and therefore:

`V = c / p`.

The existing exact reforge-kernel signature supplies the preserved-base and
action-identity check. The existing fixed primitive destructive-renewal
strategy contract can represent the repeat witness; the diagnostic did not
publish or compile a new product policy.

The selected deterministic research candidate was the cheapest admitted
goal-targeted Harvest reforge that had positive exact success mass. A 100M
diagnostic allowance was used to distinguish feasibility from the current
11M product cap:

| Case | Action | Exact success `p` | Candidate `c / p` | Diagnostic work | Solve-setup time |
| --- | --- | ---: | ---: | ---: | ---: |
| full three | `harvest_reforge:lightning` | `9.9438685754566162e-07` | `1,918,267.5087923806` | 72,104,156 | 1,395.726 ms |
| deep three | `harvest_reforge:fire` | `2.7689085310637481e-06` | `575,497.52262412792` | 14,815,748 | 224.628 ms |
| full four | `harvest_reforge:attack` | `1.2367619689693022e-08` | `193,266,777.27582425` | 6,789,419 | 114.185 ms |
| deep four | `harvest_reforge:fire` | `9.099152523570356e-09` | `175,126,199.48640418` | 2,698,559 | 50.964 ms |

Default Chaos also produced exact candidates for deep-three, full-four, and
deep-four, but full-three exhausted the 100M diagnostic work allowance.
Targeted Harvest made all four finite under that research allowance. Sampled
Fossil alternatives either had zero success mass on deep-three or consumed
similarly excessive work on full-three; none repaired the 11M-cap miss.

The evaluator retains no failed successor states after it returns. The raw
solver-owned memory observation is taken after subsequent ordinary expansion
and therefore is not a valid isolated transient-peak measurement; no
fixed-renewal peak-memory claim is made. This instrumentation limitation does
not affect the rejection, which already follows from exact work counts.

## Gate 3 — Incumbent Separation

Combining the exact renewal candidates with the Gate 1 envelope changes
neither the proof-bearing minimum nor first materialization:

| Quantity | Portfolio result |
| --- | --- |
| finite exact renewal candidates | 4 / 4 under 100M diagnostic allowance |
| actions excluded by immediate-price envelopes | 0 |
| lowest unresolved lower term | Chaos price `1` |
| first exact broad kernel still required | Chaos |
| result of that materialization | 200,000 discovered states after one expansion |

The incumbent is useful as an executable upper candidate but not selective
enough to avoid the carrier wall. Reporting the targeted-renewal value as if
it solved a restricted action set would not certify the complete product
envelope.

## Gate 4 — Compaction Boundary

Goal/failure aggregation is exact for the fixed repeat policy because the
policy never observes a failed item's detailed mods before retrying the same
destructive action. During that calculation only, junk choices in the same
exclusion-group family with the same later-roll goal-block mask can be summed.
Bellman continuation cannot use this compaction: admitted follow-up actions
can distinguish mod identity, groups, sides, tags, counts, and legality, and
the cap-stopped observation study supplied no completed quotient result across
the 200,000 successors. The fixed-policy result itself does not establish
Bellman-state equivalence.

The fixed-policy compaction preserved every measured probability. It reduced
only full-three work materially:

| Case | Uncompacted work | Compacted work | Probability comparison | Fits 11M |
| --- | ---: | ---: | --- | --- |
| full three | 72,104,156 | 48,409,673 | absolute delta `3.07e-20` | no |
| deep three | 14,815,748 | 14,815,748 | identical | no |
| full four | 6,789,419 | 6,789,419 | identical | yes |
| deep four | 2,698,559 | 2,698,559 | identical | yes |

The compact evaluator is a valid fixed-policy research primitive, not a
compact Bellman kernel. Separation plus renewal still fails the three-mod
acceptance cases, while full Bellman compaction would weaken the exact
observation contract. Gate 4 therefore selects no production architecture.

## Evidence

Raw evidence is under `build/broad-action-separation-research/`. The pinned
manifest, compiled artifact, economy, and cases are inherited from the prior
accepted natural-T1 research; the prohibited exact natural two-T1 oracle did
not run.

Representative commands, each wrapped by the detached watchdog, were:

```text
powershell -File scripts/build.ps1

POECRAFT_DIAGNOSTIC_RENEWAL_ACTION=<action>
py -3 tools/ingest/benchmark_solver_corpus.py
  --root .
  --executable build/engine/poecraft_solver_benchmark.exe
  --artifact data/compiled/current
  --corpus build/gap-directed-natural-t1-research/gate2/inputs/reforge-100m/manifest.json
  --case <case-id>
  --output build/broad-action-separation-research/<gate>/<run>
  --max-workers 1
  --no-exact-evaluation
```

Key SHA-256 records:

- uncompacted five-case portfolio ledger:
  `55bf9f2729b7ed60f43d496836ef44b723421c87df990f9f5c17e7031fa9c28a`;
- uncompacted portfolio watchdog:
  `d41ff153b71670448d240d4cda551ab6710303e8448a00a192695e3b3e76d8b4`;
- full-three targeted Harvest ledger:
  `af222b4a6e804bcc7422969bc8756eef90bf2d64bd320a790ac9977bd85c9480`;
- compact-evaluator build watchdog:
  `a3127207303ee2d8e096d89d82d8f1cb8aed69797c6ca9f01f2e65e289715429`;
- compact full-three ledger/watchdog:
  `f41fb5574ee469923b976df565a8df8aa1bee27c54b2019dae544dafdfb1e537` /
  `9ae033a018a87f1770c3a9f4ba81daaf9d537055279a65d71618cf590a16f362`;
- compact deep-three ledger/watchdog:
  `e91ec9a01c8595a4d43e190ab67fd1790ae5c52f72e44dec75a10623541aa55d` /
  `b1bf283b0055ef814584e8953166423fb9e3182ba8a3cd510d57b4a72f059a46`;
- compact full-four ledger/watchdog:
  `278a54e23ec25c1ffe88b138b701b70a6e47904ed0d234c1ae0f8f7a44a1636a` /
  `c90650e82ec8a865ae995774ce0edabe578287c5f54eca3b360b1d736bb51ec3`;
  and
- compact deep-four ledger/watchdog:
  `5a66bb32e909a402d92d74e2734e589616e9c2a41ad22ffdffddcb6c0ad70e29` /
  `1742550db334ce7082861387553b9396d916c22673a1c6cd06be82a960fa0831`.

Every listed benchmark watchdog completed without timeout or survivor. The
successful diagnostic builds also completed within their detached 900-second
watchdogs. The final link audit checked 884 local Markdown targets with zero
missing targets; its watchdog SHA-256 is
`8d395240f7ea824f07203243afe6c72c73b8606ab6ece0612c2283b2691d6634`.

## Acceptance Boundary

The prototypes changed native solver internals only during measurement and
were restored exactly before closure. Consequently:

- no engine source, C ABI, strategy vocabulary, binding, WASM, web, data,
  economy, corpus, cap, mechanic, or generated artifact changed;
- a rebuilt WASM module and cross-layer runtime suite would validate no
  retained behavior and were not run;
- final acceptance is source-delta verification, whitespace validation,
  documentation link integrity, and a clean tracked tree; and
- the exact natural two-T1 oracle was not run.

## Next Decision

Do not implement the measured separation-plus-renewal design as the general
fix. The fixed-policy success evaluator may be reconsidered only inside a
fresh production plan with its own memory telemetry, deterministic candidate
selection, witness publication, and all-four-case 11M work gate.

For the same hard cases, the remaining exact blocker is a broad-kernel
representation that preserves the complete continuation-action observation
contract while avoiding successor enumeration. This milestone found no such
representation. Another continuation should begin with a new proof or a
materially narrower product objective, not with cap increases, immediate-price
separation, goal/failure Bellman aggregation, or landing the diagnostic
prototype.
