# Finite Imprint-Program Closure Result

**Status: stopped precisely (2026-08-22).**

Parent: [plan](plan.md)

Source checkpoint: `f8c5932` (production source restored)

## Result

Two collision-checked pre-evaluation dominance proofs were implemented and
measured, then removed. Both were sound: an exact destructive-reforge
signature shared by every positive-mass prefix carrier identifies one complete
terminal kernel, and an already evaluated word with that signature plus
componentwise-no-greater primitive counts dominates the longer word under
every nonnegative economy.

The narrow version removed disposable prefixes when the terminal renewal was
also legal at the checkpoint. The generalized version reused any shorter
setup-dependent word that already owned the same reforge signature, including
`Fossil > Chaos` dominating `Fossil > Chaos > Chaos`. The dedicated native
Imprint suite passed with 60 checks and zero failures under both versions.

Neither proof closed or usefully relocated the real boundary. Each consumed
the same 256-program allowance, exposed much larger previously unscheduled
exact kernels, retained the same certified lower and verified upper, and
increased solve wall time. No production source or test change was therefore
retained.

## Measurements

| Measurement | `f8c5932` | Entry-only proof | Any-shorter-word proof |
| --- | ---: | ---: | ---: |
| Solve + finalization wall | 47.789 s | 73.422 s | 107.747 s |
| Total case wall, verification skipped | n/a | 74.372 s | 108.693 s |
| Programs evaluated | 256 | 256 | 256 |
| Programs pruned | 169 | 385 | 461 |
| Exact action-state evaluations | 182,778 | 1,271,518 | 2,072,977 |
| Outcomes merged | 2,570,418 | 891,712,766 | 2,141,217,729 |
| Maximum frontier size | 61 | 61 | 61 |
| Maximum evaluated depth | 3 | 3 | 3 |
| Certified lower | 21.772459401332767 | same | same |
| Verified executable upper | 3759.9763122101763 | same | same |
| Termination | `max_imprint_program_work` | same | same |

The open frontier moved as follows:

1. baseline: `Annul > Augment > Fossil`;
2. entry-only proof: `Fossil > Chaos > Chaos`; and
3. generalized proof: `Fossil > Harvest Reforge Defences > Fracture`.

The final witness is not an obvious cycle or erased prefix. It is a
mechanically distinct setup, targeted reforge, and persistent Fracture attempt.
Flat word pruning therefore spends its fixed budget on increasingly expensive
novel kernels rather than proving the grammar closed.

Evidence:

- [entry-only diagnostic](primary-native-diagnostic.json)
- [generalized diagnostic](primary-native-diagnostic-v2.json)

## Conclusion

Another local dominance predicate, scalar price ordering, or cap increase is
not the next owner. Prior 512/1,024-cap and scalar-order trials already failed
to close this carrier, and the new measurements show why: the work unit counts
programs while surviving programs vary from thousands to billions of merged
outcomes.

The exact research successor is a first-class kernel/label search with a work
contract that accounts for support-processing effort, but it must be designed
against the genuinely novel Fossil/Harvest/Fracture witness rather than merely
deduplicating repeated renewals.

The nearer product-quality successor is different: keep the unresolved
Imprint obligation honest and bounded while allowing independent automatic
families and executable-upper scheduling to continue. Today one local Imprint
resource deferral stops the whole solve and can starve Harvest, Essence,
Fossil, and downstream refinement even though a verified incumbent already
exists. That continuation must never promote the result to exact until the
deferred obligation closes.

## Acceptance and omissions

- Dedicated native Imprint suite: 60 checks, zero failures for each measured
  proof version.
- Two checked native four-T1 diagnostics completed under the unchanged
  five-minute watchdog and stopped at the unchanged resource cap.
- Production engine and test source was restored byte-for-byte to `f8c5932`.
- Release WASM, web tests, Warlord, 10,000-run verification, and the full
  repository pipeline were not run because the plan's native stop condition
  fired and no behavior change was retained.
