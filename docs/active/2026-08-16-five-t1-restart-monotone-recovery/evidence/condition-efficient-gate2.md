# Condition-Efficient Compilation Gate 2

**Status: complete; every reproduced same-target group is removed with exact
behavior preserved.**

The route compiler now has two explicit priority-safe coalescing proofs:

1. siblings from different values of one structural feature are mutually
   exclusive and may be grouped by destination even when non-adjacent; and
2. an adjacent run of equal destinations may always become one `any` without
   a disjointness claim, because no different-target priority crosses the run.

The second rule matters for refined observation routers. Their signatures may
overlap when they choose the same semantic operation, but the old order is
irrelevant within one consecutive equal-target run. Interleaved overlapping
targets remain separate.

| Control | Nodes | Edges before | Edges after | Condition edges after | JSON before | JSON after | Exact cost |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| priced-base five-T1 Witness B | 92 | 338 | 308 | 218 | 150,813 | 139,225 | 16,226,566.773294946 |
| restart-free five-T1 Witness A product graph | 184 | 666 | 613 | 431 | 482,233 | 460,885 | 624,800.9519118543 |
| exact four-T1 control | 292 | 1,549 | 815 | 525 | 4,737,473 | 4,670,987 | 3,745.73093400839 |

All 140 measured same-target groups are gone and the exact predeclared
817-edge ceiling is realized. Success remains one and off-policy mass remains
zero in every independently evaluated graph. Product/certification defaults,
action/material accounting, and solver result classifications are unchanged.

The dense four-T1 compiler phase fell from 671.06 ms to 69.25 ms. Its exact
evaluator phase fell from 3,773.48 ms to 3,707.05 ms. Compiler peak ownership
rose from 23.76 MB to 24.86 MB because the typed immutable condition and route
provenance are fully cap-accounted; that increase is bounded and immaterial to
the configured cap.

Conditions still own 4,587,281 bytes of the four-T1 graph. Inspection shows
the dominant leaves are large `observation_signature` predicates that repeat
their complete observation requirement in the public inline v1 schema. Their
signatures differ, so exact route sharing and current-vocabulary factoring do
not remove the payload. Eliminating that repetition requires the already
deferred versioned condition-reference or observation-decision representation,
not an unsafe string rewrite.
