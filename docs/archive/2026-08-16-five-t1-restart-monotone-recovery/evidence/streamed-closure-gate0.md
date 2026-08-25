# Streamed Closure Gate 0 - Representation Census

Date: 2026-08-17

## Result

The behavior-neutral census identifies deterministic router pairs, not another
pair container or the calculator state table, as the owner of the stopped raw
discovery frontier. Gate 1 therefore selects **exact online deterministic
routing**. Derived transition routing remains unimplemented.

This decision follows the prior-work audit in the selected plan. In
particular, it does not retry reconstruct-then-merge, open-graph aggregation,
another pair-index rewrite, or whole-run checkpoint/replay.

## Checked 10-Million Prefix

Witness B stopped at the unchanged transition cap with:

| Census | Count |
|---|---:|
| Raw pairs | 9,998,209 |
| Expanded pairs | 3,966 |
| Pending pairs | 9,994,243 |
| Router pairs | 9,987,873 |
| Operation pairs | 10,335 |
| Expanded operation pairs | 3,965 |
| Expanded router pairs | 0 |
| Retained rows | 728 |
| Shared-row pair reuses | 3,238 |
| Retained transitions | 9,974,257 |
| Transitions carrying the existing compiler-policy route | 12,126 |
| Transition policy states equal to target-pair states | 9,974,257 |
| Transition policy states different from target-pair states | 0 |
| Calculator states | 35,837 |

Router pairs are 99.90% of all raw pairs in the stopped prefix. Almost the
entire pending frontier was produced by only 3,965 expanded operation pairs;
the evaluator had not expanded a router pair before the transition cap fired.
The existing single-root `policy_route_*` compression covers only 12,126 of
the retained transitions, so merely compacting its metadata cannot address
the observed frontier.

The measured owners were:

| Owner | Bytes |
|---|---:|
| Calculator | 68,408,968 |
| Segmented pair carrier | 240,263,168 |
| Segmented pair links | 40,050,688 |
| Row payload | 239,382,200 |
| Evaluator live estimate | 599,143,284 |
| Evaluator peak | 600,881,884 |
| Evaluator budget | 1,050,981,759 |

The census adds scalar counters only. Relative to the preceding checked run,
the evaluator peak changed from 600,881,764 to 600,881,884 bytes. Witness A
remained independently exact at 624,800.951911854 with success one and zero
off-policy mass. Witness B retained the same transition-cap classification
and the same published Chaos fallback at 37,279,857.73995944.

The refreshed native largest steps were 319.21 ms for Witness A and 3,416.10
ms for Witness B. Those are not treated as proof of a runtime regression or
improvement: the census does no second row traversal, total Witness A solve
time was 4.78 seconds, and only one refreshed timing sample was taken. They
are retained as the Gate 1 comparison baseline.

## Proof Boundary For Gate 1

Gate 1 may skip only non-operation routers whose selected edge is a pure
function of the retained exact item state and which do not observe a modifier
offer. It must:

- intern a collision-safe exact trace of every skipped router and edge;
- retain no-matching-edge and terminal identities;
- account node occupancy, edge traversal, and top classes from solved flow;
- leave deterministic router cycles in the raw graph; and
- preserve raw/reference, single-step, route, flow, and operation/material
  accounting controls.

The census does not prove that routing alone closes the full graph. Gate 2
will make that decision at the checked cap and, only if measured headroom
justifies it, one higher probe.

## Checks

- native build: passed
- focused evaluator suite: 16,857 checks, zero failures
- focused solver suite: 96,113 checks, zero failures
- Witness A: exact value/success/off-policy unchanged
- Witness B: stopped at the unchanged 10-million transition cap
- `git diff --check`: passed

No release-WASM build, web suite, parent successor Gate 4-8 work, or full
acceptance pipeline was run.
