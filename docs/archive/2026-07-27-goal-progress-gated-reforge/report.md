# Goal-Progress-Gated Reforge Report

**Status: final implementation and acceptance record.**

Parent: [Milestone archive](README.md)

## Result

The milestone retains an opt-in solver flag and leaves the unrestricted exact
solver as the default. For every primitive destructive reforge row in gated
mode:

1. all goal-satisfying mass reaches one terminal exit;
2. all zero-satisfied-goal mass reaches one virtual retry basin for the exact
   preserved boundary;
3. every partial-progress outcome remains its complete exact state; and
4. raw probability/resource mass is neither deleted nor renormalized.

A retry basin can choose only a legal destructive reforge whose kernel ignores
the discarded affixes. Annul, Exalt, Bench, protection, and other salvage are
unavailable there. Retained partial states keep the complete ordinary action
envelope. The compiler emits an explicit retry route when a closed restricted
policy exists and refuses to broaden an unrepresentable policy.

The only enumeration short-circuits are monotone proofs: an already satisfied
goal cannot become unsatisfied during the remaining roll, or a zero-progress
branch has no remaining eligible positive-weight goal roll. Terminal, retry,
and partial mass plus short-circuit counts and a canonical kernel-bits hash are
published in telemetry.

## Scope

`exact_within_zero_progress_reroll_restriction` means exact for the restricted
executable policy class. It does not mean globally optimal over excluded
zero-progress salvage routes. Fixed-option kernels retain their existing
explicit-program semantics. Omitting the flag retains
`globally_optimal_unrestricted`.

## Frozen results

Both cases used the committed product action envelope, prices, artifact,
generator-config hash, and caps: 200,000 discovered states, 25,000 expanded
states, 300,000 rows, 3,000,000 transitions, 3,000,000 reforge work, and
256 MiB selected solver-owned memory.

| Case | First-row exits | Partial states | Terminal / retry / partial mass | Kernel hash | Final stop |
| --- | ---: | ---: | --- | --- | --- |
| full-four | 134,477 | 134,475 | `1.6572365216439631e-08` / `0.91351213364415884` / `0.086487849783493706` | `54c479dfcd7d14f3` | 134,508 discovered; 3,000,000 reforge work |
| deep-four | 123,697 | 123,695 | `5.38535873115097e-09` / `0.91208724024359367` / `0.087912754371051213` | `5cfd88b724a36b01` | 123,728 discovered; 3,000,000 reforge work |

Committed mass sums are `1.0000000000000178` and
`1.0000000000000036`, respectively. The first row is therefore below the
200,000-state requirement in both cases without dropping mass. Two repetitions
per case reproduced every deterministic state/work/mass/hash field.

Neither case reached Bellman optimization or a policy. After completing the
first Chaos row and other cheap root rows, a later exact reforge request
exhausted `max_reforge_work`. Raising caps or merging partials by goal count
was not attempted.

**2026-07-28 correction:** the raw reports have `expanded_states = 1`, so the
later request above is a competing broad reforge on the root, not a row from
the retained partial-state graph. The first Chaos row uses 2,807,580 of the
3,000,000 reforge-work units; the next root Fossil request consumes the
remainder. Root broad-action competition is the immediate measured wall.
Partial-state admission is later and unmeasured.

Unrestricted controls remained in global scope and reproduced their incomplete
first-row control hashes: transition `566809fa56c7e2a4`, policy
`0cfb7e9c1000a4da`.

## Verification

- Native release build completed with GCC 14.2.0. Its only diagnostic was the
  existing optimizer warning in `prepare_goal_cover_cost`.
- Native acceptance passed 500,825 checks with zero failures.
- The small restricted oracle proves grouped parity against the unrestricted
  row, raw probability conservation, deterministic kernel identity, basin
  action restriction, and full partial-state action availability.
- The toy restricted solve compiles, evaluates exactly at the native value,
  and succeeds in 10,000 seeded simulator runs.
- The frozen cases had no policy, so no frozen 10,000-run verification was
  applicable.
- Release WASM was rebuilt. Web tests and TypeScript typecheck passed.

Wall measurements on the sequential repetitions were 565.20/556.01 ms solve
for full-four and 421.90/431.89 ms for deep-four. They are machine/compiler
bound and do not survive a hardware or compiler change. Deterministic work,
mass, and hashes are the portable evidence.

## Identity

- Starting source: `f843a9d`.
- Plan commit: `1c59493`.
- Benchmark executable SHA-256:
  `083fa6b62a89eac6fe9e53177bc6440fe65d67eecca5043abd563f5405c80a33`.
- Product portfolio manifest SHA-256:
  `a4f080e680499b95d2b6b5e635c4bc9fd6678a82f4ce698ea8c6882b37fb3efd`.
- Artifact manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Natural-T1 generator config SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Compiler/machine: GCC 14.2.0, Windows build 26200, Intel i7-13700K,
  16 cores / 24 logical processors, 68,396,957,696 physical bytes.

Raw reports remain under
`build/goal-progress-gated-reforge/final-evidence/`; their hashes and complete
portable fields are pinned in the tracked summary.

## Consequence

The zero-progress composition wall is removed for the two frozen cases.
Corrected root-level telemetry selects competing broad root actions as the
next measured exact-search wall. Exact partial-progress states remain large
and may justify a later bounded Pareto design after the solver actually begins
expanding them; they are not the current measured owner.
