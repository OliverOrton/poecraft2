# Action-Local Side Factorization Falsification

**Status: complete (2026-07-27).** Simple count-conditioned prefix/suffix
convolution was rejected for the exact destructive-reforge evaluator.

Parent: [Archive](../README.md)

- [Final report](report.md)
- [Completed plan](plan.md)
- [Tracked evidence summary](../../../fixtures/solver-scaling/v1/evidence/action-local-side-factorization-summary.json)

The exact synthetic Chaos table had rectangular prefix/suffix support but a
non-zero probability minor (`4.7521644443241428e-6`) inside the fixed
two-prefix/two-suffix count cell. Sequential draws use the combined remaining
pool weight, so outcome identity on either side changes later selection odds
on the other.

Conditioning on final remaining prefix/suffix weights restored rank one but
required 48 marginal identities to encode 41 joint outcomes. The candidate
therefore failed before a real-data or production shadow evaluator.

Fracture remains an exact preserved-base boundary variable. The retained
native regression pins that a fractured goal satisfies its slot and unrelated
fractured junk does not invalidate the current permissive goal predicate.
