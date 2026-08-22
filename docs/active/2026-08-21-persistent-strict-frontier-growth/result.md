# Persistent Strict Frontier Growth Result

**Status: stopped precisely on 2026-08-21; no source change retained.**

The narrow carrier-row replay proposed by this boundary was sound after its
identity guard was corrected, but it did not improve the real four-T1 solve.
The experiment was removed rather than moving the stop to memory or retaining
complexity without a measured benefit.

## What Was Tested

The trial retained each completed exact carrier/action result—including a
deterministic absent row—across competitive frontier restarts. Its key embedded
the full collision-free exact source and full alternative-action identities.
Successful rows stored exact successor identities plus strict-context locator
representatives. On replay, every representative was re-interned into the new
strict child and required to reconstruct the same exact target before the
current pass repeated canonicalization, projection, coverage, partition,
Bellman, proof-generation, and Q comparison.

An initial diagnostic compared a policy-collapsed exact identity with its raw
strict locator and correctly failed when two contract-equivalent locators
shared the collapsed identity. That guard was corrected to use `ExactState`
identity and target rebind validation before the checked measurement. This was
a diagnostic correction, not a relaxation of collision checking.

## Checked Primary

The corrected native Allflame four-natural-T1 Conquest Lamellar run reached
the five-minute watchdog:

| Measurement | Accepted baseline | Carrier-row replay |
| --- | ---: | ---: |
| Solve wall | 300,102.42 ms | 300,460.67 ms |
| First verified strict upper | 38,943.05 ms | 42,064.56 ms |
| Final live upper | 3745.7295960574743 | 3745.7295960574743 |
| Upper increases | 0 | 0 |
| Largest public step | 1,484.77 ms | 1,490.83 ms |
| Maximum finalization work item | 78,509 | 78,489 |
| Solve steps | 81,423 | 80,841 |
| Native peak owned estimate | 98,661,450 bytes | 686,355,442 bytes |
| Native live owned after abandon | 41,777,961 bytes | 41,777,961 bytes |

The trial observed strict passes near 5,820, 5,924, and 5,951 states. It still
had global lower zero, did not finish, and emitted no final strategy. The
20-work-item difference at the stop is 0.03% and is not material. Time to the
first verified strict upper regressed by 8.0%, estimated peak ownership grew
to 6.96 times baseline, and the largest public step was unchanged.

The evidence says completed mechanic-row calculation is not the controlling
cost. Rebuilding selected closure and repeating generation-dependent
canonicalization, carrier projection, partition, Bellman, coverage, and proof
work remain dominant. Caching more generation products would create invalid
proof authority; caching only exact rows retains a large payload without
avoiding that dominant work.

Compact measurements are recorded in
[native-primary-summary.json](evidence/native-primary-summary.json). Full
trial reports remain derived local artifacts under
`build/solver-diagnostics/persistent-strict-frontier-growth/`.

## Final Tree And Qualification

All replay source and test-hook changes were removed. The final source is the
accepted verified-interim-upper checkpoint behavior.

- Native build: pass after removal.
- Refinement focus: 362 checks, 0 failures.
- Quotient proof/partition/Bellman focus: 381 checks, 0 failures.
- Policy-refinement focus: 969 checks, 0 failures.
- Solver suite: 96,174 checks, 0 failures.
- Release WASM, TypeScript, web tests, the full repository pipeline, and
  10,000 primary simulations: not run. No engine change remained, and the
  primary produced no final strategy.

## Next Precise Owner

The next implementation must extend one live strict generation in place when
a competitive successor appears. It needs stable locator insertion, localized
raw-row discovery, carrier-coverage growth, split-only partition updates,
Bellman dependency updates, and revocation/reproof of exactly the obligations
whose source or target coverage changed. It must not reuse a verdict from an
older Q/partition generation or rebuild the nearly complete strict pass.

State-928 Imprint grammar and five-T1 recovery remain separate. No
implementation boundary is active until Oliver selects the next chunk.
