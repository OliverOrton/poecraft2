# Gate 1 Certification Result And Gate 4 Stop

**Status: stopped at the selected plan's Gate 4 pair-discovery boundary.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](../README.md)

Source checkpoint: `705c25c` (`Expose the five-T1 certification boundary`).
This evidence was produced natively on 2026-08-16 from the checked Allflame
fixtures. The release WASM was not rebuilt and the full repository acceptance
pipeline was not run.

## What Changed

- Witnesses A and B are checked cases. Their priced action lists prove that
  `base = 5` adds only `restart`: 27 actions become 28.
- Bounded policy compilation now has explicit product-safe-Restart and
  certification-fail-closed modes.
- Candidate assertions independently evaluate the fail-closed graph. An
  executable cost-mismatch candidate is still eligible by its evaluated cost.
  Its retained product graph is accepted only after a structural comparison
  proves the pair differs at policy-router default targets alone.
- Observation propagation reports nodes, equal-authority groups, rounds,
  canonical requirement count, payload percentiles, projected/estimated
  owned bytes, retained bytes, and time. Telemetry collection uses no
  graph-sized temporary percentile or uniqueness containers.
- The observation preflight charges the graph, ordered index, grouping
  vectors/signatures, two simultaneously live dense requirement copies, and
  merge scratch. The actual retained estimate is checked after the fixed
  point. No cap was raised.
- Certification failures now distinguish observation memory, pair discovery,
  component memory, compile size/memory, route coverage, pricing, unsupported
  actions, time/round caps, and solver/exact cost mismatch.

## Witness A - Restart-Free Reference

Command case: `conquest-lamellar-allflame-five-natural-t1-product`.

| Evidence | Result |
| --- | ---: |
| public status | `refused_unsupported_action` / bounded policy |
| lower / evaluated upper | `0 / 624800.9519118543` |
| product graph | 184 nodes / 666 edges / 482,233 bytes |
| product defaults | 170 `product_safe_restart` |
| certification defaults | 170 `certification_fail_closed` |
| pair structural check | passed |
| external product evaluation | matched; success 1; off-policy 0 |
| total / solve wall time | 5,501.82 / 3,399.92 ms |
| largest solve step | 224.30 ms |
| product SHA-256 | `f12a2cb13137e69d7b107015da9d417026a4b01accf5cb7206da18d315b2ee62` |

The selected candidate remained `verified_retained`. The internal fail-closed
assertion is executable, proper, cost-complete, and zero-off-policy; the
external evaluator independently matched the retained product graph at the
same cost. The previous 624,800.9519 candidate therefore does not take a
reachable fail-closed route default.

Its product-graph observation pass used 184 nodes/groups, 15 rounds, and seven
canonical requirements. The conservative projected peak was 373,912 bytes,
the post-fixed-point owned estimate was 321,056 bytes, and retained assignment
storage was 114,280 bytes.

Three earlier warmed focused runs during instrumentation measured 5,638.34,
5,699.79, and 5,713.18 ms with identical value and termination. The final
5,501.82 ms run shows no timing regression. Because pairing intentionally
changes the previously returned fail-closed graph into the product graph,
this is not claimed as a byte-for-byte behavior-neutral Gate 0 closure.

## Witness B - Priced Base

Command case:
`conquest-lamellar-allflame-five-natural-t1-priced-base-product`.

The 2,015-node/4,123-edge, 757-region fail-closed certificate now passes the
observation stage. Its 499 policy-router defaults all target `offpolicy`.
Observation evidence is:

| Evidence | Result |
| --- | ---: |
| nodes / equal-authority groups | 2,015 / 2,015 |
| rounds / canonical requirements | 10 / 6 |
| direct payload p50 / p95 / max | 40 / 40 / 80 bytes |
| propagated payload p50 / p95 / max | 280 / 320 / 320 bytes |
| projected / estimated peak | 4,313,004 / 2,905,660 bytes |
| retained assignments | 836,280 bytes |
| observation wall time | 24.5 ms |

The next exact stage discovers 8,395,474 state/action pairs over 35,837
states, 544 rows, and 8,396,650 transitions. It owns 1,178,801,916 bytes
against a 1,050,982,663-byte evaluator budget before component construction;
268,692,800 bytes are row payload and only 594,480 bytes are retained
observation requirements. The stable failure classification is
`exact_eval_pair_discovery_memory_cap` with `path=pre_component` and
`probe=steady_state`.

The candidate is therefore not materialized. Publication remains the
independently evaluated six-node/seven-edge Chaos renewal at
37,279,857.73995944 chaos, SHA-256
`7a36219a6226ec5242fc4c59b5ec80788676aebe0a11db054e8f7582ea4b28d6`.
Total native time was 13,128.40 ms, and the pair-discovery/finalization path
still produced a 2,636.42 ms public step.

## Gate Disposition

- Gate 1's Witness A certification boundary is sound and the retained public
  artifact is again the safe product graph. The complete planned Gate 1
  synthetic/WASM matrix was not run after the later stop.
- Gate 4's observation-accounting correction is sound for both frozen graphs,
  but the priced-base graph reaches a materially broader pair-discovery
  representation boundary under the unchanged cap.
- Stop conditions 5, 6, and 7 are present: broader pair-discovery work is
  required, the largest step exceeds 250 ms, and Witness B still publishes
  the renewal fallback above the one-million-chaos materiality ceiling.
- Gates 2, 3, and 5-8 were not started. In particular, Restart staging,
  high-impact automatic admission, publication/progress monotonicity, release
  WASM qualification, and final acceptance remain undone.

The next implementation must be a newly selected pair-discovery memory and
cooperativity plan. It should attribute the 8.4-million-pair map/vector and
268.7 MB row payload before choosing interning, compact keys, streaming, or a
different exact quotient. This milestone must not resume by raising the cap.

## Focused Checks

- `powershell -File scripts/build.ps1` - passed.
- `poecraft_engine_tests.exe --solver-compile-only` - 815 checks, zero
  failures.
- `poecraft_engine_tests.exe --solver-eval-only` - 16,801 checks, zero
  failures.
- `poecraft_engine_tests.exe --solver-solve-only` - 96,082 checks, zero
  failures.
- Both frozen native benchmark cases completed with exact evaluation of the
  graph that was actually published.
- `git diff --check` - passed before the source checkpoint.

Local raw reports are
`build/qualification/gate1-paired-final-five-t1-a.json` and
`build/qualification/gate4-pair-discovery-boundary-final.json`. They are
diagnostic build outputs, not checked artifacts.
