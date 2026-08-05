# Completion Report

Parent: [Goal-Gated Semantic Policy Routing](README.md)

## Outcome

Calculator now opts into `goal_progress_gated_reforges: true` on every normal
solve, with or without gap targets. Generated strategies carry optional
`solver_policy_scope` provenance. Native and WASM callers that omit the option
remain unrestricted.

The compiler classifies generic strict carriers by their final executable
operation and continuation before minimizing predicates. A proved uniform
destructive-renewal policy emits the existing four-node/four-edge
goal-or-repeat graph; incompatible observation domains, conflicting carriers,
state-local choices, and unproved closures use the general compiler.

## Attribution

### Gating before compiler changes

On the frozen body-armour A/B case, gating removes zero-goal-progress salvage
routes from the retry basin while leaving genuine partial-progress carriers
exact. Nodes fall 18 to 14, edges 51 to 31, JSON 4,766,346 to 723,637 bytes,
conditions 4,761,052 to 719,918 bytes, junk predicates 31,448 to 4,666, and
compile wall 53.7456 to 8.8035 ms. Historic/complete compiler memory falls
14,652,036/14,655,286 to 2,559,848/2,562,026 bytes. The restricted policy is a
different MDP, so its value changes from 6.8745265564 to 8.3411819866; this is
gating attribution, not compiler optimization.

### Region routing after gating

The body-armour B/C route is structured and observation-owned, so its 14
nodes, 31 edges, and 719,918 condition bytes remain unchanged; the 64-byte JSON
increase is only `solver_policy_scope` metadata. The generic strict regression
does expose region routing directly: condition bytes fall 4,808 to 3,076
(36.02%) and JSON 7,368 to 4,775 bytes (35.19%) while Transmute, Alteration,
and the off-policy default remain distinct.

J (`reliability-selected-gloves-10k`) qualifies for the certified compact path:
5 nodes/5 edges become 4/4, JSON falls 38,457 to 1,181 bytes (96.93%), and
condition bytes fall 37,286 to 141 (99.62%). Compile wall falls 1.8628 to
0.6026 ms and exact evaluation 159.8041 to 143.3282 ms in the authoritative
portfolio run. Its exact value remains 0.3684 and all 10,000 simulations
succeed. This reduction belongs to the Gate 2 compact proof, not the Gate 3
generic minimizer.

## Why the old 435 KiB condition existed

The old compiler classified behavioral quotient classes before final emitted
operation regions were known. Its exact fallback therefore serialized a DNF
over 55 irrelevant junk-carrier identities to distinguish kernel classes that
ultimately selected the same operation and continuation. Final-region routing
removes only distinctions with no executable consequence.

## Preserved partial progress and alternatives

Absent and present-below-tier misses have zero goal progress and may enter the
existing retry basin. Any carrier satisfying a proper subset of a multi-slot
goal remains an exact state with its complete filtered action envelope.
Focused V1/V3 checks retain ordinary reforges, forced Essence,
targeted/exceptional Harvest, and positive, zero-weight, additive, and forced
Fossil alternatives with identical target maps, probabilities, and selected
semantic actions.

## Certification and refusal

The compact loop requires one identical legal state-independent destructive
renewal on every reachable non-goal carrier, identical non-empty exact gated
kernel signatures and hashes, unit-mass closed successors, goal ownership of
all exits, positive success probability, properness, and evaluated-cost/upper
agreement. A retained bounded witness, when present, must agree.

It is refused for stale or conflicting witnesses, differing operations or
kernels, state-local/observed recipes, open closure, or a reduced exact carrier
that cannot support the operation-and-goal observation contract. The latter
keeps the influenced Essence reliability case on its exact five-node general
route at value 24.14 rather than publishing an unsupported compact fallback.

Multi-operation regions compare complete operation and continuation recipes.
Structured Unveil, Fracture, Imprint, protected-repeat, temporary-bench,
gated-basin, and other observation-owned/state-local paths remain independent;
unknown carriers retain the explicit off-policy or bounded Restart default.

## Memory and portfolio

Accepted compact U reports 1,206 historic and 1,817 corrected bytes; J reports
1,185 and 1,748; the protected body-armour route reports 2,559,976 and
2,562,154. `peak_owned_bytes` and cap enforcement use the corrected column.
No cap was raised and no compiled portfolio case hit a compiler cap.

Across the 38 IDs compiled in both the historical and final 48-case reports,
nodes fall 405 to 403, edges 924 to 922, JSON 25,151,385 to 25,078,873 bytes,
and condition bytes 25,037,770 to 24,963,216. Aggregate compile wall changes
391.5099 to 394.9302 ms (+0.87%) and exact evaluation 27,967.5464 to
28,020.0654 ms (+0.19%), both ordinary run noise. One historical case no
longer compiles because the preceding versioned reforge-accounting milestone
exhausts its unchanged logical work budget during publication; this is not a
routing or compiler-memory cap regression.

The final portfolio exact-evaluates and runs 38 compiled policies for exactly
380,000 simulations: 348,278 successes, 31,722 existing action-limit failures,
and zero off-policy failures. The only expectation failures are the same two
excluded rare-renewal exact-cost reconciliation cases.

## Compatibility statement

No policy value changes for identical solve options, and no mechanic, action
filter, price, Bellman comparison, V3 result, cap, state abstraction, quotient
proof, or engine-wide unrestricted default changes. Final commands and counts
are retained in Gate 5 evidence.

## Final acceptance

- Native compiler: 791 checks, zero failures.
- Native solver: 98,308 checks, zero failures.
- Targeted gated V1/V3: 5,355 checks, zero failures.
- Release WASM rebuild: passed.
- Focused worker/Calculator/model regressions: passed.
- `npm test` and `npx tsc --noEmit`: passed.
- `scripts/test.ps1`: run exactly once and passed, including 3,003,170 native
  checks plus the complete web and Python pipeline.
