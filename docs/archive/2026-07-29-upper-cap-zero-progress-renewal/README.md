# Upper-Cap Sensitivity And Zero-Progress Renewal Audit

**Status: completed measurement; executable-upper qualification not
established and no additional live-renewal canonicalization proved sound.**

Parent: [Documentation index](../../README.md)

- [Frozen plan](plan.md)
- [Final report](report.md)
- [Recovered candidate source](recovered-candidate.diff)
- [Tracked evidence](../../../fixtures/solver-natural-t1/v1/evidence/upper-cap-zero-progress-renewal-summary.json)

## Outcome

The exact recovered 200,000-state candidate reproduced its prior bounds,
work, lifecycle counts, and hashes. A corrected long run then reached 387,556
discovered states before Oliver stopped the increasingly slow tail after
2,197 seconds. It reduced the 60.3-million upper by only `4.07634`, or
`0.00000676%`. The preserved partial snapshot does not contain terminal
action classification, so no strict qualifying admission is claimed.

The zero-progress audit found 1,031 states. Although 1,030 shared one exact
renewal signature, 1,030 ordinary carriers were observable by retained
non-renewal actions. Only the existing gated retry-basin state passed the full
renewal-and-observer contract. No broader state merge was applied.

The recovered scheduler and audit remain a native-benchmark-only diagnostic
behind an unpublished run-local flag. Product defaults, the public ABI,
bindings, strategy vocabulary, compiled policies, and unrestricted solving
remain unchanged.
