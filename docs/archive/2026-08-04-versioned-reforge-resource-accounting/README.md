# Versioned Reforge Resource Accounting And V3 Production Qualification

**Status: completed and archived on 2026-08-04.** The solver now caps every
reforge evaluator against one stable V1-equivalent logical envelope, reports
implementation effort separately, and uses exact V3 for production strict
rows after native, release-WASM, memory, latency, reliability, and hard-fallback
qualification.

Parent: [Documentation archive](../README.md)

- [Archived plan](plan.md)
- [Engineering report](report.md)
- [Historical interpretation audit](audit.md)
- [Gate 5 qualification evidence](../../../fixtures/solver-reliability/v1/evidence/reforge-resource-accounting-v3-qualification-gate5.json)
- [Gate 6 release evidence](../../../fixtures/solver-reliability/v1/evidence/reforge-resource-accounting-v3-qualification-gate6.json)

## Outcome

- `max_reforge_work` now consumes `logical_work_v1` regardless of evaluator.
  Raw V1 receives its historical envelope; V1/V2/V3 implementation effort and
  physical components remain separately versioned observations.
- Nested automatic/comparison contexts receive the parent's remaining logical
  budget before execution. Executed child work is retained on refusal and an
  interrupted row cannot publish.
- Coarse reforge rows remain raw V1. Strict selected and competitive
  alternative rows use exact V3 generically across ordinary, Essence, Harvest,
  and Fossil actions. Native diagnostics retain V1 rollback and V2 research.
- V3 improved the frozen native binding row by 27.25%, preserved aggregate
  eligible performance, and stayed within every 10% memory/latency limit.
  Release WASM preserved hashes/bounds and cancellation headroom.
- The one 100M diagnostic preserved 40 selected rows and 6,903,840 transitions
  without a partition. The one hard 20M run preserved the independent
  four-node fallback, exact evaluation, and 10,000 zero-off-policy simulations.
- The 48-case portfolio retains the two known rare-renewal misses and no new
  V3 failure. Selected closure, alternative closure, rare-renewal numerical
  reconciliation, and five-goal finalization remain unresolved.

No follow-on implementation boundary is selected automatically. Oliver must
choose the next chunk.
