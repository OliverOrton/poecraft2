# Gate 3 — Cooperative Scheduler Qualification and Fallback

**Decision:** the cooperative scheduler implementation is retained as typed,
observable infrastructure, but its behavior-changing product profile is
disabled. The plan's explicit fallback is active because no measured profile
qualified both the clean target and retained controls.

The action-envelope ledger remains complete observational authority. The
legacy completed-pair view remains scheduling authority until a later gate can
replace it without changing the accepted product boundary.

## Implemented infrastructure

- `AnytimeScheduler` owns typed `legacy_fairness`, `executable_upper`,
  `high_progress`, `exact_closure`, and reserved `proof_directed` lanes.
- The versioned profile owns lane quotas, refinement batch, incumbent
  checkpoint, starvation threshold, and material-improvement threshold.
- Telemetry exposes profile identity, offers, service, waits, yields,
  improvements, starvation, current wait, and maximum wait for every lane.
- Carrier scores retain exact goal subset, side-capacity obstruction,
  `blocked_mask`, occupancy, fracture, useful protection, and stable semantic
  identity. Action scores retain preservation, destruction/cleanup, immediate
  reach, and stable operator identity. Neither type converts to proof values.
- The scheduler has direct unit coverage for weighted tickets, strict yield,
  wait/starvation accounting, and improvement telemetry.
- Fixed-work ledger identities and caller/action scope remain complete.

## Rejected cooperative profiles

All runs use the Gate 0 corpus identity and skip sampled verification. Every
published upper below was independently exact-evaluated.

| Profile or control | Proper upper | Graph | Wall ms | Max slice ms | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| best clean cooperative profile | 1,562,686.25661352 | 289 / 797 | 70,896.7 | 4,360.0 | misses required target by 603.10464663 (0.0386%) |
| 128-row automatic-locality profile | 1,562,686.25661352 | 213 / 637 | 70,523.3 | 4,341.3 | same upper; rejected with 131-dispatch fairness wait |
| fixed four-of-five | 59,810.9537769745 | 51 / 149 | 6,522.9 | 4,320.8 | exact upper retained |
| owner four-to-five before fallback | 14,048,002.5685606 | 5 / 5 | 41,236.1 | 4,493.1 | hard last-mile regression |
| non-armour 10-second control | 9,087,149,347.98672 | 97 / 334 | 57,612.6 | 13,428.8 | proper/no watchdog, but weaker than baseline |
| tri-elemental Bow | 79,273.3250308337 | 83 / 239 | 41,445.2 | 2,661.8 | exact parity |
| bounded Warlord | 307.556312036793 | 21 / 45 | 1,578.8 | 239.5 | proper bounded parity; lower 212.3 |

The historical 87k strategy remains observational evidence only. Its current
exact replay is deferred to the final evidence package alongside the required
strategy verification, so this gate does not misstate it as current product
authority.

## Applied fallback

The fallback restores the previously proven carrier-local automatic drain,
operator-major continuation producer, state-index legacy tie break, geometric
incumbent checkpoint, and completed-pair scheduling view. Its formerly
anonymous `128`, `16`, four-wave, and weighting constants are now named by
`solver_anytime_warm_start_compat_v1` and serialized in telemetry. The typed
ledger and cooperative scheduler remain observational and report
`matched_control_profile_unqualified`.

| Current fallback control | Lower | Verified upper | Graph | Wall ms | Max slice ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| clean zero-to-five | 36.4286171890906 | 14,454,067.4260706 | 514 / 1,788 | 73,205.3 | 4,323.5 |
| fixed four-of-five | 3.47245 | 59,810.9537769745 | 51 / 149 | 41,256.6 | 4,645.2 |
| owner four-to-five | 3.47245 | 2,698.87479601436 | 215 / 563 | 21,613.1 | 4,947.8 |

Bounds, graph census, properness, exact evaluation, and result classification
match Gate 0/Gate 2 authority. Clean and owner wall times improve slightly.
Maximum-step samples differ by less than 75 ms from the single Gate 0 samples;
the fallback restores the same unsliced legacy work unit, so this is recorded
as runtime variance rather than a responsiveness claim.

## Evidence files

Raw diagnostic reports are retained outside the tracked documentation tree at
`build/performance/gate3-scheduler-experiments/`. Principal reports:

- `gate3-clean-preservation-first.json`
- `gate3-clean-automatic-locality-128.json`
- `gate3-control-four-of-five.json`
- `gate3-control-owner-four-to-five.json`
- `gate3-control-non-armour-bow.json`
- `gate3-control-tri-elemental-bow.json`
- `gate3-control-warlord.json`
- `gate3-fallback-clean-final.json`
- `gate3-fallback-fixed-work.json`
- `gate3-fallback-owner-final.json`

Only native narrow diagnostics and native rebuilds were run. No routine suite,
sampled simulation, WASM build, web suite, rendered review, or full acceptance
pipeline was run at this intermediate gate.
