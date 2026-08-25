# Condition-Efficient Strategy Compilation Final Result

**Status: complete and accepted.**

The compiler now uses typed canonical composite conditions, proves structural
partition disjointness, coalesces priority-safe same-target routes, stages
refined observation routes through the same authority, and builds both exact-
policy and refined-parent decision DAGs through one mode-explicit engine.

## Representation result

| Control | Nodes | Edges before -> after | Condition bytes before -> after | JSON bytes before -> after | Compile ms before -> after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Witness A product | 184 | 666 -> 613 | 415,270 -> 398,187 | 482,233 -> 460,885 | 4.80 -> 4.23 |
| priced Witness B | 92 | 338 -> 308 | 116,972 -> 107,696 | 150,813 -> 139,225 | 2.85 -> 1.60 |
| exact four-T1 | 292 | 1,549 -> 815 | 4,594,437 -> 4,587,281 | 4,737,473 -> 4,670,987 | 671.06 -> 66.05 |

All 140 measured same-target groups and the full 817-edge proof-safe ceiling
are removed. The dense four-T1 graph loses 47.4% of its edges and its native
compiler phase is about 10.2 times faster. Nodes do not regress. Typed
condition/provenance accounting raises the four-T1 compiler peak from 23.76 MB
to 24.86 MB, far below its configured limit.

Condition bytes remain dominant in the four-T1 output because distinct inline
`observation_signature` leaves repeat their complete requirement. Current v1
has no exact shared-reference form. This is the measured next representation
boundary; it was not hidden behind an unsafe boolean rewrite.

## Correctness result

Solver transition and policy hashes are unchanged on every frozen control.
Independent exact evaluation remains:

- Witness A: 624,800.9519118543, success one, off-policy zero;
- priced Witness B: 16,226,566.773294946, success one, off-policy zero; and
- four-T1: 3,745.73093400839, success one, off-policy zero.

Witness B remains honestly bounded. Its zero lower bound, stored
37,279,651.842345364 versus exact 16,226,566.773294946 cost mismatch, carrier
5983 coarse-mapping failure, and non-selection of Fracture are unchanged. The
four-T1 policy still selects Fracture at 3.99695198371769 expected applied uses.

## Native/WASM and verification

The exact four-T1 graph completed 10,000 Simulator runs on both native and
release WASM with the fixture's pinned seed. Both reports have 10,000
successes, zero failures, zero off-policy failures, sampled mean
3,738.95388111991, and `verification_passed = true`.

The native/WASM comparison passes 96/96 checks. Both emit 292 nodes, 815 edges,
4,670,987 JSON bytes, and exact cost 3,745.73093400839. Release WASM's maximum
slice is 7,700.80 ms against this case's explicit 20-second diagnostic limit;
the separately deferred 50/250 ms responsiveness work was not relabeled as
complete. The final WASM binary is 5,565,744 bytes with SHA-256
`b3482836895dea7f65b8557456035fcd702adbf7d25d7108e69cc78f6ce1d3e6`.

## Acceptance

- native build passed;
- compiler 834, evaluator 18,065, refinement 362, solver 96,120, and core
  2,861,286 focused checks passed;
- release WASM build passed;
- TypeScript checking and the complete non-visual web suite passed;
- `git diff --check` passed; and
- the single final `powershell -File scripts/test.ps1` pipeline passed,
  including 3,463,907 native engine checks.

Acceptance also corrected two stale golden fixtures: solver telemetry now
asserts the new condition census/route density, and the web corpus inventory
recognizes all three checked five-natural-T1 cases.
