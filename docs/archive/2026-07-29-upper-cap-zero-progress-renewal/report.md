# Upper-Cap Sensitivity And Zero-Progress Renewal Audit

**Status: executable-upper qualification not established; no new canonical
live-renewal merge retained.**

## Decision

The higher-cap experiment produced real executable-upper movement, so the
preceding 200,000-state zero-movement result was cap-sensitive. The movement
was nevertheless microscopic: after 2,197 seconds and 387,556 discovered
states, the root upper had fallen by `4.0763404965` from
`60341416.98784247`, a `0.00000675546%` reduction. The frozen 10%
qualification was missed by about six orders of magnitude. The preserved
long-run partial does not contain terminal action classification, so it cannot
establish a strict completed Fossil/Harvest admission and none is claimed.

No additional zero-progress canonicalization is sound under the current
goal-relevant action envelope. The exact audit found the shared renewal
signature the proposal anticipated, but it also found legal Annul, Exalt,
Fracture, Harvest Augment, Scour, and state-local automatic candidates that
observe or exploit the ordinary carriers. The existing gated retry basin is
the only state that passes both exact renewal and non-renewal-observer checks.
It remains same-carrier renewal, not Restart.

Retained source is diagnostic and default-disabled:

- the recovered progress-preserving state/action scheduler and focused
  executable-upper pass behind a native-benchmark-only flag;
- finalization-only cap, contribution, fanout, and zero-progress audit
  telemetry;
- explicit incumbent policy-reachability capture, which prevents unrelated
  discovered states from being published as policy reachable; and
- a benchmark CLI discovered-state override whose output records the
  effective cap.

No product default, public option constant, binding protocol, strategy
vocabulary, compiled policy, or unrestricted solver behavior changed.

## Recovery And 200,000-State Reproduction

The tracked recovered patch has SHA-256
`3bc21c3fc5cf5cf1b91607cb6c89102eaf7ffd651ed94c602b585bd7143f3501`.
The final-binary reproduction matched the rejected candidate:

| Field | Result |
| --- | ---: |
| Root lower / upper | `432.40685295343258 / 60341416.98784247` |
| Discovered / expanded / frontier | `200000 / 160 / 199840` |
| Focused-upper requested / started / proper / rejected | `5 / 5 / 5 / 0` |
| Q rounds / selected states | `2 / 144` |
| Rows reconsidered / upper-policy updates | `2946 / 0` |
| Rows / transitions | `2228 / 842726` |
| Reforge work | `16746695` |
| Unique kernels / carrier reuse | `14 / 1203` |
| Admitted / non-improving / unresolved / unevaluated | `0 / 0 / 1057 / 2815` |
| Completed rows recomputed | `0` |
| Live / peak owned bytes | `83936943 / 134297160` |
| Total wall ms | `12408.6214` |
| Cap | `max_discovered_states` |
| Transition / policy hash | `d4346e90f923332c / 8b2a568f3c9cfd35` |

The benchmark output records
`input.run_overrides.max_discovered_states = 200000`; its cap check passes.

## Cap Sensitivity

The first recovered 400,000- and 600,000-state attempts both reached 313,642
discovered / 6,688 expanded states, 57,268 rows, 3,160,605 transitions, and
49,443,214 reforge work. Each completed 60 proper focused-upper passes before
the harness rejected an incumbent that incorrectly marked unrelated
discovered states reachable. Those are harness failures, not state-cap
measurements.

The reachability repair records only states reached by the selected policy
walk and its executable exits. The corrected 400,000-cap run passed the old
failure and produced:

| Field | Right-censored snapshot |
| --- | ---: |
| Elapsed wall ms | `2197242.1795` |
| Root lower / upper | `432.40685295343258 / 60341412.911501974` |
| Absolute / percentage upper reduction | `4.0763404965 / 0.00000675546%` |
| Discovered / expanded | `387556 / 19360` |
| Rows / transitions | `118664 / 3521080` |
| Reforge work | `49443214` |
| Live / peak owned bytes | `340631496 / 380246517` |
| Completed-row recomputation | not finalized in partial; `0` in completed 200k and initial 313,642-state snapshots |
| Transition / policy hash | not finalized in partial |
| Terminal classification | owner-approved time-censored stop |

This snapshot is not described as a 400,000-state cap hit. Oliver explicitly
allowed avoiding hours in the slow tail, so the corrected 600,000 run was not
repeated. The earlier 600,000 request remains the identical 313,642-state
harness failure above. An 800,000 run was not justified.

The observed scaling answers the primary question: more states do eventually
move the executable upper, but strict carrier-by-carrier materialization buys
roughly four currency units after more than half an hour while leaving every
finalized 200,000-state qualifying interval overlapping. The right-censored
snapshot is useful movement evidence, not a terminal admission certificate.

## Root Successor Coverage At 200,000 States

Six completed root Fossil/Harvest rows contributed 569,976 successors and
probability mass `5.999999999999596`. All mass had non-Restart continuations:
`5.999999758046422` used the Chaos fallback, `0` used Restart or a local exact
row, and `2.4195317358e-7` used another fallback.

| Rank | Probability mass | Upper-Q contribution | Contribution fraction |
| ---: | ---: | ---: | ---: |
| 32 | `5.352635556205077` | `322985614.08092278` | `89.2105962%` |
| 128 | `5.369417485794238` | `323998259.49212271` | `89.4902950%` |
| 512 | `5.411376098921418` | `326530101.6630612` | `90.1896053%` |
| 2048 | `5.495569539500234` | `331610453.16866767` | `91.5928294%` |

The top 32 successors already dominate contribution, yet their selected
continuations remain almost entirely the same high-cost Chaos renewal. This
is why strict carrier expansion adds substantial policy-evaluation cost
without a comparable upper reduction.

The completed root row intervals were:

| Action | Lower Q | Upper Q | Upper Q above root incumbent |
| --- | ---: | ---: | ---: |
| Lucent fossil | `440.6346504848839` | `60341420.46413261` | `3.47629014` |
| Harvest Attack | `434.7225946216303` | `60341418.63180959` | `1.64396712` |
| Harvest Cold | `445.9290532830591` | `60341428.5593793` | `11.57153683` |
| Harvest Elemental | `479.1076560067963` | `60341463.11912154` | `46.13127907` |
| Harvest Mana | `520.6537143967446` | `60341500.47141495` | `83.48357248` |
| Harvest Physical | `445.9307369070396` | `60341429.2781554` | `12.29031293` |

## Zero-Progress Audit

The final-binary 200,000-state audit classified:

- 1,031 zero-progress states: 1,030 ordinary and one existing retry basin;
- 1,031 live-renewable, zero certified dead, and zero without legal renewal;
- 1,028 with explicit affixes and none with fracture, crafted/metamod,
  influence, corruption, Eldritch, veiled, or persistent setup in this
  measured subset;
- 1,030 ordinary states observed by retained non-renewal behavior;
- two exact combined renewal signatures; and
- one safe state, the existing retry basin, with zero additional
  canonicalizable states.

The main signature
`f7daf74de711fd2a` grouped 1,030 states, including 1,028 explicit-affix
carriers and the retry basin. Exact renewal equality therefore exists, but it
is insufficient for solver-state equality. Observer counts were Annul 1,028;
Exalt 1,029; Fracture 909; each of three Harvest Augments 1,029; Scour 1,029;
and state-local automatic candidates 1,030.

Discovery fanout explains the scale:

| Family | Rows | Successor entries | Maximum row fanout |
| --- | ---: | ---: | ---: |
| Chaos | `160` | `21516320` | `134477` |
| Harvest reforge | `880` | `77896720` | `134400` |
| Fossil | `176` | `15330064` | `93306` |
| Add/remove | `694` | `10652` | `68` |
| Fracture | `158` | `706` | `6` |
| Restart | `160` | `160` | `1` |

The large counts are observed evaluator fanout, not retained transition
entries. The audit is finalization-only and excluded from solve decisions,
hashes, caps, and owned-memory accounting.

## Renewal And Restart Controls

Native controls cover all twelve requested boundaries:

1. discarded-junk carriers retain distinct strict IDs but share the exact
   Chaos renewal signature and retry representative;
2. analytic renewal value is `1 / p` even with a one-billion base price;
3. a fractured goal-dead carrier selects mandatory Restart;
4. the existing price-flip case keeps optional economic Restart distinct;
5. goal-progress carriers never enter the retry basin;
6. fractured, crafted/metamod/protected, influenced, corrupted, and Eldritch
   identities remain distinct;
7. gated and uncollapsed probabilities/signatures remain exact;
8. a legal crafted-mod cleanup observer blocks canonicalization;
9. completed rows report zero recomputation;
10. repeated hashes remain deterministic;
11. every published reachable non-goal state has an executable action; and
12. unrestricted outcomes and solving remain unchanged.

The corrected incumbent mask also proves an unrelated pre-interned observer
state is not policy reachable.

## Interpretation

The next credible boundary is a feature-conditioned, compositional
prefix/suffix executable upper that can value the small contribution-dominant
set without materializing one policy row per strict carrier. The audit does
not authorize a goal-count merge, a renewal-signature-only merge, or a
Restart substitution.

## Acceptance

The final acceptance completed:

- the native fallback build passed;
- the complete native suite passed 502,270 checks with zero failures;
- standard and natural-T1 manifests validated 12 and 146 cases;
- release WASM rebuilt to a 2,561,916-byte module with SHA-256
  `7169a8126ca5a7cc76e6336e52376edca2b6f8b62ff5a1b28b625a62a0e53e53`;
- the complete non-visual web suite passed; and
- `npx tsc --noEmit` passed.

The first complete native invocation reported four assertions from the new
explicit-action audit control. The audit had been unnecessarily gated on
incremental-action generation. Removing that diagnostic-only guard changed
no solve or product behavior; the second complete invocation passed.

Release WASM and non-visual web gates were run because diagnostic engine
behavior remains compiled in, even though no public/product path enables it.
No rendered or screenshot review was run.
