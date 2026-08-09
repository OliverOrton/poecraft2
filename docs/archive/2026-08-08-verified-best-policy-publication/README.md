# Verified Best Executable Strategy Publication

**Status: implementation and acceptance complete; archived 2026-08-08.**

Parent: [Documentation archive](../README.md)

The solver now publishes the cheapest independently evaluated executable final
strategy JSON found during a solve. Direct, strict-refinement, and renewal
candidates all pass through the same final-graph evaluation and bound
normalization contract. A source-estimate mismatch blocks optimality but no
longer discards an otherwise proper, completely priced, zero-off-policy graph.

Fishing Rod recovers its retained renewal and publishes bounded at
`16.482913436819231`. The `60546`, `a64d3`, and `c4c4` cases publish cheaper
verified direct graphs instead of a more expensive renewal or an unverified
strict artifact. Ring and Amulet remain unavailable because their retained
artifacts cannot be independently evaluated under the unchanged caps.

The 49-case native and release-WASM portfolios agree on every compared
publication and exact-evaluation semantic check. Both publish 45 policies,
and every published graph independently converges with complete cost, eventual
success probability one, and zero off-policy mass. The required Runic
Gauntlets and Dire Pelt controls each completed 10,000 simulator runs in both
runtimes without an off-policy failure.

Artifacts:

- [completed plan](plan.md)
- [completion report](report.md)
- [tracked deterministic evidence](../../../fixtures/solver-reliability/v1/evidence/verified-best-policy-publication.json)

This milestone changes no crafting mechanic, price, goal, action scope,
Bellman comparison, state abstraction, strategy vocabulary, resource cap,
reconciliation tolerance, watchdog, or simulator action limit. Rendered UI
review was intentionally excluded.
