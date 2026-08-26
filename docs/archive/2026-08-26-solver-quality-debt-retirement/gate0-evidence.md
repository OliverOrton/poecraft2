# Gate 0 — Calculator-profile quality ladder

Recorded 2026-08-26 from plan checkpoint `b5f3cfb` plus the Gate 0 owned-byte
accounting repair. The canonical compiled artifact identity was
`af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5`.
The native benchmark executable used by both matrices had SHA-256
`de883a48212733e417962e82b601bfaebcb9cca6b7152d428e7c7f6c8c26b5ec`.

## Reproduction

Generate the versioned cases:

```powershell
py -3 tools/ingest/generate_solver_quality_ladder.py
```

Run the Calculator worker profile, whose adaptive native slices are capped at
eight work items when the Calculator supplies no `chunkSize`:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
py -3 tools/ingest/benchmark_solver_corpus.py --root . `
  --executable build/engine/poecraft_solver_benchmark.exe `
  --artifact data/compiled/current `
  --corpus fixtures/solver-quality-ladder/v1/manifest.json `
  --output build/performance/solver-quality-gate0-product8-ledger-fixed `
  --max-workers 2 --memory-budget-bytes 2147483648 `
  --watchdog-ceiling-seconds 120
```

Run the otherwise-matching retained native qualification slice:

```powershell
py -3 tools/ingest/benchmark_solver_corpus.py --root . `
  --executable build/engine/poecraft_solver_benchmark.exe `
  --artifact data/compiled/current `
  --corpus fixtures/solver-quality-ladder/v1/qualification-1024/manifest.json `
  --output build/performance/solver-quality-gate0-qualification1024-ledger-fixed `
  --max-workers 2 --memory-budget-bytes 2147483648 `
  --watchdog-ceiling-seconds 120
```

Both use the Allflame economy, exact terminal success, generated Imprint
programs off, voluntary economic Restart off, goal-progress-gated reforges,
and high-impact executable uppers. Every published finite policy was evaluated
exactly; sampled verification is intentionally deferred by Gate 0.

## Product-profile result

Costs are chaos-equivalent. `watchdog` rows are censored observations, not
solver exactness failures.

| Base/start | Goal | Wall | Stop | Lower | Exact published upper |
|---|---:|---:|---|---:|---:|
| Conquest clean | 1 | 0.4 s | numerical stability | 0.520018 | 62.0878 |
| Conquest clean | 2 | 6.0 s | numerical stability | 0 | 307.811 |
| Conquest clean | 3 | 31.7 s | requested finish | 79.4055 | 2,722.04 |
| Conquest clean | 4 | 64.7 s | requested finish | 21.7725 | 8,501.69 |
| Conquest clean | 5 | 67.3 s | requested finish | 36.4885 | 14,454,067.43 |
| Conquest partial 3 | 4 | 60.3 s | watchdog | 21.7725 | 5,154.69 observed, not finalized |
| Conquest partial 3 | 5 | 46.8 s | requested finish | 36.4885 | 80,720.79 |
| Conquest partial 4 | 5 | 50.9 s | requested finish | 36.4885 | 7,896,721.25 |
| Conquest fractured partial 4 | 5 | 31.1 s | requested finish | 36.4286 | 2,698.87 |
| Spine Bow clean | 1 | 0.6 s | numerical stability | 1.79935 | 66.6079 |
| Spine Bow clean | 2 | 2.4 s | numerical stability | 89.9426 | 1,828.65 |
| Spine Bow clean | 3 | 8.2 s | numerical stability | 212.320 | 11,376.03 |
| Spine Bow clean | 4 | 88.4 s | state cap | 212.386 | 223,349.00 |
| Spine Bow clean | 5 | 79.2 s | state cap | 212.386 | 17,073,119,024.23 |
| Amethyst Ring clean | 1 | 0.7 s | numerical stability | 1.87855 | 74.7921 |
| Amethyst Ring clean | 2 | 7.5 s | memory cap | 10.1698 | none |
| Amethyst Ring clean | 3 | 12.8 s | memory cap | 85.5405 | none |
| Amethyst Ring clean | 4 | 23.5 s | memory cap | 201.855 | 3,725,358,919.50 |

The ladder therefore separates the current product failures:

- **no incumbent:** Ring two- and three-goal solves stop at `max_owned_bytes`
  before producing a policy;
- **resource stop:** Bow four/five reach the 200,000-state cap, while all Ring
  cases beyond one goal encounter the owned-byte cap;
- **proof weakness:** clean two-goal Conquest publishes a zero lower, and all
  hard five-goal/partial cases retain tiny independently global floors relative
  to their executable uppers;
- **valuation/refinement mismatch:** direct certification reports
  `cost_mismatch` on Conquest clean two/four/five and partial three/four to
  five; and
- **failed strict promotion:** clean five, both ordinary partial-to-five cases,
  and the fractured owner reach the misleading
  `proof envelope lost its selected quotient state` exception. The fractured
  owner is the shortest deterministic witness and is selected for Gate 1.

The Conquest clean five policy is proper, cost-complete, zero-off-policy, and
exactly evaluated at 14,454,067.43. It is therefore a real but very poor
strategy, not merely an unverified estimate. Its final strategy uses Chaos,
Annul, Exalt, one Eldritch Ember, and one Eldritch Chaos; no Harvest action is
selected despite Harvest candidates being admitted.

## Eight versus 1,024 work items

Every case that finalized under both profiles produced bit-identical lower,
published upper, exact-evaluator cost, and stop classification. This includes
clean five (14,454,067.43), fractured four-to-five (2,698.87), ordinary
three-to-five (80,720.79), ordinary four-to-five (7,896,721.25), Ring four,
and both Bow cases.

The 1,024-item Conquest clean-four control was externally censored at 92.1
seconds with a 5,218.04 in-progress upper, whereas product-eight finalized at
8,501.69. That is not evidence that 1,024 selected a better published policy:
the 5,218.04 value had not passed final exact evaluation. It does reproduce the
product's visible upper-change/finalization discontinuity and must be reported
as candidate versus verified authority, not as a monotone verified incumbent.

Conclusion: work-slice drift affects responsiveness, watchdog interaction, and
reproducibility, but did not cause the five-goal strategy-quality cliff in this
matrix. Gate 6 still owns the profile unification.

## Gate 0 accounting defect

The first matrix exposed a deterministic internal abort on Ring cases:
`incremental SolveWork selected-owned-byte ledger undercounted`. The retained
automatic-candidate samples contain bounded diagnostic strings whose
capacities can change across move/copy/replacement. Their cached nested-byte
subtotal was copied as though those capacities were stable.

Gate 0 now reconciles the actual retained sample capacities after mutation and
recomputes the subtotal when quotient work is copied. A direct copy/capacity
regression was added. The focused native solve suite passed with 100,146 checks
and zero failures. After the repair, the same Ring cases reach truthful
`max_owned_bytes` classifications rather than aborting.

## Gate 1 boundary

The selected witness is
`conquest-lamellar-allflame-fractured-4-to-5-product8` because direct
certification completes at 2,698.87 but strict lifting fails after 31 seconds.
Source inspection found that selected-first refinement walks
`selected_rows_by_state` before checking the quotient Bellman result's status.
The next diagnostic rerun moves that check to the truthful boundary; values,
selected actions, hashes, caps, and publication fallback remain unchanged.
