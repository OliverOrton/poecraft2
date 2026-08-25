# Condition-Efficient Compilation Gate 0

**Status: complete; the measured same-target opportunity is material and the
implementation gates are selected.**

The frozen native compiler reproduced the dense routing Oliver observed. This
was measured from persisted strategy JSON before changing route construction.

| Control | Nodes | Edges | JSON bytes | Condition edges | Unique conditions | Repeated occurrences | Repeated bytes | Same-target groups | Grouped edges | Proof-safe edge saving |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| priced-base five-T1 Witness B | 92 | 338 | 150,813 | 248 | 52 | 196 | 57,220 | 30 | 60 | 30 |
| restart-free five-T1 Witness A product graph | 184 | 666 | 482,233 | 484 | 73 | 411 | 249,386 | 53 | 106 | 53 |
| exact four-T1 control | 292 | 1,549 | 4,737,473 | 1,259 | 753 | 506 | 1,270,674 | 57 | 791 | 734 |

Every counted group consists of two or more sibling route edges with the same
destination. The compiler generated those siblings from distinct values of
one selected structural feature, so the value predicates are mutually
exclusive. Replacing a group with one `any` of its complete old conditions
therefore preserves first-match behavior and the old product or certification
default.

The exact four-T1 result is the strongest signal: 791 conditioned sibling
edges converge within only 57 target groups. The current compiler pays for 734
extra graph edges before considering condition factoring. This is not merely
a visual-layout problem.

## Frozen behavior

- Witness B remains bounded feasible at independently evaluated cost
  `16226566.773294946`, success one, and zero off-policy mass. The run took
  17.18 seconds, including 4.08 seconds of exact graph evaluation.
- The four-T1 control remains exact and converged. The run completed in 95.80
  seconds.
- Restart-free Witness A retains its previously proved policy result. The
  product benchmark reports the already-known unsupported-action refusal while
  still emitting the 184-node diagnostic product graph used for this census.
- The focused compiler suite passed 815 checks after adding behavior-neutral,
  cap-accounted census telemetry.

## Predeclared targets

1. Remove all 140 reproduced same-target sibling groups without changing their
   accepted domain, destination, or default.
2. Realize at least the exact 817-edge proof-safe ceiling across the three
   frozen graphs (30 + 53 + 734), before any additional DAG or factoring gain.
3. Preserve exact graph evaluation values, terminal mass, action/material
   accounting, defaults, and off-policy mass.
4. Do not regress nodes, edges, condition bytes, or total JSON bytes on a
   frozen graph without an explicit recorded reason.

The implementation is therefore authorized to proceed through typed condition
construction and priority-safe sibling coalescing. The continuation-aware DAG
and further factoring remain measurement-gated after coalescing.
