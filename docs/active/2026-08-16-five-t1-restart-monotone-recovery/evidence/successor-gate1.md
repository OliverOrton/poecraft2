# Successor Gate 1 - Behaviour-Keyed Product Fracture Sharing

**Status: passed.**

Source checkpoint: the Gate 1 checkpoint on `codex/solver-goal-realignment`.
This evidence was produced natively on 2026-08-16. Release WASM and the full
repository acceptance pipeline were not run.

## Implementation

Product-local Fracture states now enter policy-region compression through a
canonical length-delimited signature of their complete emitted behaviour:

- operation JSON;
- accounting-role JSON;
- the acceptable unfractured-goal hit condition; and
- the common product-Fracture retry/default continuation.

The route condition is constructed by the same helper used in the signature,
so the key cannot silently drift from emitted execution. Uniform annotations
retain the existing shared-region behaviour; non-uniform expected-cost
annotations remain non-authoritative and are omitted.

## Focused Oracle

The product-Fracture native oracle selects Fracture in three reachable states
with two distinct acceptable-hit behaviours. Compilation emits exactly two
Fracture operation nodes and two route nodes. The compiled graph evaluates as
proper and cost-complete with success 1, failure 0, action-not-applied 0, and
no-matching-edge 0. Its exact cost and every expected operation-consumption
entry match the independently compiled strict lift.

Different acceptable-hit masks remain separate. The retry destination is
common by construction within one product graph, and accounting roles and the
operation descriptor are explicit signature components rather than inferred
from an action index.

## Frozen Four-Goal Acceptance

Case: `conquest-lamellar-allflame-four-natural-t1`.

| Evidence | Before | Gate 1 |
| --- | ---: | ---: |
| Fracture-selected states | 767 | 767 |
| Fracture route/operation behaviours | 767 | 7 |
| compiled nodes | 1,813 | 292 |
| compiled edges | 3,832 | 1,549 |
| strategy JSON bytes | 5,205,249 | 4,737,473 |
| exact cost | 3,745.73093400839 | 3,745.73093400839 |
| success / off-policy mass | 1 / 0 | 1 / 0 |

The node reduction is 83.9%, the edge reduction is 59.6%, and serialized size
falls 9.0%. Conditions, rather than duplicated Fracture nodes, now dominate
the artifact size. The current native run spent 670.25 ms in compilation and
4,185.65 ms in independent exact evaluation; compilation speed is not the
qualification claim. Total solve time was 114.84 seconds, and the known
pre-Gate-4 cooperative-step obligation remains open at 3,826.75 ms.

The report is the untracked diagnostic output
`build/qualification/successor-gate1-four-goal.json`.

## Focused Checks

- `powershell -File scripts/build.ps1` - passed.
- solver compiler suite - 815 checks, zero failures.
- solver solve suite - 96,111 checks, zero failures.
- frozen four-goal native case - converged, expectation met, independent exact
  evaluation matched.
- `git diff --check` - passed before the checkpoint.

Gate 2 is now authorized: run Witness A once and priced Witness B once with
the Gate 0 evaluator telemetry and this compiler representation.
