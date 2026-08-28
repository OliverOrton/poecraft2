# Solver Research Architecture Audits

**Status: archived read-only research input from 2026-08-27.** These reports
were prepared against `769c3deb1a2a2913c228c4135c764271f662bef9`. They are
evidence and design proposals, not implementation authority or current
sequencing authority.

Parent: [Documentation archive](../README.md)

## Reports

- [Archaeology audit](archaeology-audit.md) — measured historical lineage,
  changed premises, and experiments that should remain closed.
- [Verified executable options audit](executable-options-audit.md) — an exact
  graph-fragment architecture for bounded executable policies.
- [Native Solver Lab architecture audit](native-solver-lab-architecture-audit.md) —
  persistent experiments, subprocess supervision, and scheduler-continuation
  replay design.
- [Retention–Capacity Abstract SSP audit](retention-capacity-abstract-ssp-audit.md) —
  a proof-only action-specific lower-bound portfolio proposal.
- [Learned-guidance audit](learned-guidance-audit.md) — fail-open scheduling,
  resource prediction, data, and model-safety boundaries.
- [Integrated synthesis](integrated-synthesis.md) — the other session's
  reconciliation and suggested dependency order.
- [Long-horizon implementation notes](implementation-session-notes.md) — raw
  follow-up advice about a focused Native Solver Lab implementation run.

## Current Interpretation

The following conclusions are accepted as useful planning input:

- extend the existing native corpus runner and benchmark rather than creating
  a second solver backend;
- keep the engine authoritative for mechanics, action legality, probabilities,
  proof, compilation, and exact evaluation;
- implement subgoals later as verified executable graph fragments, never as a
  coarse stochastic upper;
- keep retention-aware abstractions proof-only, small, action-complete, and
  tied to a measured pruning or retirement consumer;
- keep learned systems fail-open and ordering/proposal-only; and
- do not use the completed-coarse checkpoint as a live PDR scheduler snapshot.

Several recommendations are deliberately not adopted as immediate gates:

- scheduler-aware replay is required for identical-prefix continuation and
  running pause/resume, but not for behavior-neutral telemetry gathered from a
  fresh ordinary solve;
- the full Lab does not need to precede all policy-quality research;
- the RCASSP report's `460678.970156889` PDR upper is not supported by the
  repository. The current archived witness remains `7866.432124027084` upper
  and `21.772459401271156` lower; and
- the reports' broad mechanic-question list must first be resolved through
  current engine contracts. Oliver is asked only about genuine remaining
  ambiguity.

The selected first plan is maintained separately under [Active work](../../active/README.md).
