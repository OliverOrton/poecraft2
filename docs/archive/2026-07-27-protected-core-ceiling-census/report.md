# Protected-Core Compression Ceiling Census Report

**Status: final evidence and decision record.**

Parent: [Milestone archive](README.md)

## Result

Cleanup is rejected as the next pre-bound scaling architecture.

The census deliberately granted cleanup more power than any real route could
have: opposite-side or non-goal affix contents disappeared instantly, for no
currency, with no failure exit, legality condition, or continuation proof.
The projected keys were full collision-checked `AbstractState` values, but
they were diagnostic signatures only.

The raw all-state ceiling was large:

| Case | Mix | Observed | Prefix core | Suffix core | Goal core |
| --- | --- | ---: | ---: | ---: | ---: |
| full three | SSS | 199,952 | 893 / `223.9x` | 3,601 / `55.5x` | 27 / `7,405.6x` |
| deep three | PSS | 199,960 | 221 / `904.8x` | 3,251 / `61.5x` | 27 / `7,405.9x` |
| full four | PPPS | 199,969 | 276 / `724.5x` | 817 / `244.8x` | 54 / `3,703.1x` |
| deep four | PPSS | 199,969 | 276 / `724.5x` | 817 / `244.8x` | 54 / `3,703.1x` |

Those ratios do not describe cleanup's usable population. Oliver's motivating
case requires meaningful protected-side goal progress. The broadest generous
population counted every nonterminal state with at least two satisfied goals
on either one side, even when that side was not yet complete:

| Case | Two-plus protected-side states | Fraction of observed prefix | Protected side complete |
| --- | ---: | ---: | ---: |
| full three | 1,683 | `0.842%` | 0 |
| deep three | 275 | `0.138%` | 275 |
| full four | 11,352 | `5.677%` | 136 |
| deep four | 7,666 | `3.834%` | 7,666 |

Even if a cleanup implementation deleted every state in that generous
population without adding one replacement state, the best case would recover
only 11,352 entries. That is at most 5.68% of the existing cap, while all four
cases stop during their first broad row before the kernel is complete. A real
stochastic or deterministic cleanup route would recover less because it has
costs, failures, preconditions, and retained exits.

The current goal contract also accepts extra non-goal modifiers. Cleanup is
therefore not required to satisfy these goals; it would need to earn its place
through an exact continuation/source-elimination argument. The measured
population is too small to justify building that route now.

## Separate positive signal

The very large all-state prefix/suffix ratios are still useful. They show that
the observed state explosion contains substantial opposite-side identity.
They do **not** show that those states are behaviorally equivalent under the
admitted continuation actions. Earlier exact all-action evidence found no
merge across the complete product envelope, and this census intentionally
erased affix-derived flags that some actions observe.

If Oliver later selects the broader direction, the next question is not
"which cleanup craft should we add?" It is whether an action-local
factorization or source-elimination certificate can prove that a protected
core owns an exact subset of continuations while preserving every exit needed
by the remaining actions. A new plan would need:

1. a complete, action-by-action observation contract for the candidate
   continuation subset;
2. collision-checked complete-kernel or otherwise formally bounded coverage,
   rather than extrapolation from this cap-censored prefix;
3. counterexamples for actions, flags, count conditions, prices, and finish
   requirements that distinguish erased contents; and
4. a proof that the factorization reduces actual interned states before any
   implementation may affect Bellman bounds or pruning.

No cleanup primitive or mechanic ruling is selected by this result.

## Frozen identity

- Source commit:
  `c9591789154190aa3bf18a0d0e0b93e619107427`.
- Measurement patch Git blob SHA-1:
  `c6d03ddfdedf9dff84f8d8b4c49c4c26e9646067`.
- Measurement executable SHA-256:
  `b804d3483e884339aeb24f576ecf8bf402c2fd8b94e12f74521510e8afa9fd61`.
- Portfolio manifest SHA-256:
  `a4f080e680499b95d2b6b5e635c4bc9fd6678a82f4ce698ea8c6882b37fb3efd`.
- Artifact-manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Natural-T1 generator-config SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Economy content SHA-256:
  `9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.
- Compiler/machine: GCC 14.2.0 on Windows 11 Pro, Intel Core i7-13700K,
  24 logical processors.

All four cases used the unchanged 200,000-state, 1,215,000-row,
10,000,000-transition, 11,000,000-reforge-work, and 1 GiB
selected-owned-memory limits.

Wall time is machine/compiler-bound. Observed reforge work was 10,143,446,
8,152,092, 6,119,280, and 6,119,280 respectively. Census construction took
264.7 to 388.1 ms and an estimated 1.71 to 2.26 MiB of peak temporary storage.

## Verification and restoration

The measurement build passed 120,461 focused native calculation checks with
zero failures. All four final reports completed with `refused_state_cap`;
there were no watchdog timeouts or surviving processes.

Against the prior frozen streaming-fold baseline, every case matched:

- status and cap classification;
- discovered, expanded, and goal-state counts;
- state/action rows, transitions, and outcome entries;
- reforge frontier work; and
- transition and policy hashes.

The tracked
[evidence summary](../../../fixtures/solver-natural-t1/v1/evidence/protected-core-ceiling-summary.json)
pins the per-case counts, hashes, fan-in maxima, costs, and case identities.
Raw reports remain local under
`build/protected-core-ceiling-census/final-runs/`.

All measurement-only engine and native-test source was restored. The final
repository change is documentation and tracked evidence only, so no WASM,
binding, artifact, database, or web acceptance was required.
