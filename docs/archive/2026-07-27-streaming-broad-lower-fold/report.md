# Streaming Broad-Lower Fold Report

**Status: final evidence and decision record.**

Parent: [Milestone archive](README.md)

## Result

The fixed measurement-only candidate did not establish a publishable
`c + E[H_coarse(X)]` scalar on any selected case. The four hard cases entered
the existing goal-cover preparation while calculating the shadow comparison
term, reached ordinary graph/resource limits before the standalone fold, and
reported no eligible/completed fold or bound. The smoke diagnostic terminated
abnormally with Windows status `0xC0000005`.

The no-tuning rule applied after the first candidate result. The candidate was
not repaired or rerun, its measurement-only solver changes were restored, and
no detached kernel, scalar record, promotion path, or new broad-kernel
representation was retained.

This rejects the candidate and further broad-kernel work in this milestone. It
does not prove that every conceivable streaming expectation is mathematically
impossible: the fixed implementation failed before measuring computability,
selectivity, or refinement pressure. That distinction is intentional.

Gate 3B retained versioned successful properness-proof reuse. It removes
duplicate validation after an executable constructive fallback exists, while
leaving the hard pre-bound failure unchanged.

## Frozen Identity

- Gate 0 source: `5400255db05b207b8c1a1bc081b1514a5accec5c`.
- Shadow source: `699c4671d7f01cced87e237b3e6966ed61fc8986`.
- Retained implementation source: `8da701502e86962cf051731e157891ff4d731c61`.
- Baseline executable SHA-256:
  `f6e4990d25eb903409caa14606b7b020b788a365f5266846bc5753e6ca2ea27d`.
- Retained executable SHA-256:
  `8134b78a90f0b4e10b5888dbaadd857e95f24b08f5a87c6fb59ac75ee8122744`.
- Artifact-manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Natural-T1 generator-config SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Economy content SHA-256:
  `9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.
- Compiler/machine: GCC 14.2.0 on Windows 11, Intel64 Family 6 Model 183,
  24 logical CPUs.

All runs used one worker and fixed per-case watchdogs. Wall time is
machine/compiler-bound; deterministic work, graph counts, bounds, and hashes
are the portable comparison.

## Gate 2 Falsification

The fixed five-case ledger SHA-256 is
`979be757cf6105053ee134258792d36ea994c77c40c1271a9ff8dc0d50a1338d`.
The four hard cases returned `refused_state_cap`; the smoke crashed. No case
published a fold. Because computability failed at the implementation boundary,
selectivity and refinement pressure were not scored.

The useful general benchmark repair was retained: if a native solve step
throws while the solver still owns a valid in-progress snapshot, the harness
cooperatively abandons and queries telemetry before destroying the handle.
The final 11M portfolio produced five reports, no timeout, and no survivor.
The smoke report now preserves an `abandoned` expansion snapshot at exactly
11,000,000 reforge work. Its ledger SHA-256 is
`7d95f60dbec05a173f4517431f5c0cdbaf6b38b8345d31e4fe1bd7fc763e2dcc`.

## Retained Properness-Proof Reuse

A successful fallback validation is retained only inside the current solve.
The key contains:

- proof implementation version;
- exact immutable fallback-policy ownership and semantic policy identity;
- goal and economy identities;
- the complete action-vocabulary prefix present at validation;
- transition-graph owner plus the immutable row/pricing prefix;
- successor, probability, choice, and choice-option prefix identities; and
- the immutable mechanics/session owner.

Focused graph and action-vocabulary growth may append after the stored prefix.
Any replacement or mutation of a dependency misses the cache and executes the
full existing validation. Only successful validations are remembered. The
cache cannot guide search, change a bound, construct a policy, or survive a
solve lifetime.

On the 30M Dire Pelt owner, both variants used the same case SHA-256
`da358284785064726aa0bcd89de5843c5200d0664b1b94834075eea1eb8dffc5`
and fixed caps: 200,000 states, 25,000 expanded states, 300,000 rows,
3,000,000 transitions, 30,000,000 reforge work, and 256 MiB selected memory.

| Measurement | Baseline | Reuse |
| --- | ---: | ---: |
| solve wall | 36,391.471 ms | 28,268.875 ms |
| fallback validation | 8,902.739 ms | 557.833 ms |
| start-properness checks | 17 | 1 |
| successful proof-cache hits | 0 | 16 |
| proof identity checks | 0 | 17 in 30.456 ms |
| discovered / expanded states | 95,118 / 2,301 | identical |
| rows / transitions | 61,661 / 873,813 | identical |
| reforge work | 18,349,624 | identical |
| lower / upper | 93.7458734898226 / 10209.562183559785 | identical |
| transition / policy hash | `7bb62e2e3c3b6650` / `ce20fd1456065d18` | identical |
| termination | `refused_resource_cap` | identical |

The measured solve reduction is 8,122.596 ms, or 22.32%. Validation itself
saved 8,344.906 ms. The remaining difference is ordinary wall-time noise and
changed profiling overhead, not different deterministic work.

All other paired cases also retained status, termination, bounds, state/row/
transition/work counts, and transition/policy hashes. Their small wall-time
movements are not claimed as improvements.

## Acceptance

- Native build completed.
- Focused solve suite: 518 checks, 0 failures.
- Final 30M paired benchmark: five reports per variant, no timeout or survivor.
- Final 11M portfolio: five reports, no timeout or survivor.
- Release WASM rebuilt; worker smoke passed 27/27.
- Full non-visual web test command passed.
- `npx tsc --noEmit` passed.
- No exact natural two-T1 oracle, compiled-policy evaluation, or 10,000-run
  simulation was required because no policy changed or newly qualified.

Release WASM SHA-256:

- module: `4746fc86ad9ca4e942d55f1348512554a6b879fd9a4832a9e7feb98b632d4c54`;
- binary: `a529d4b1ea74a5f19e1d5bc06ee3d425302b3c19ee3718241b438bbec82ddada`.
