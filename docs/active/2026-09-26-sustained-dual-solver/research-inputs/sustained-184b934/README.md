# Sustained dual-solver programme: target support and selective completion

**Reviewed repository:** `OliverOrton/poecraft2`  
**Pinned main:** `184b934396e9aba7d009f8dcbad3f488ee71dbfd`  
**Parent:** `adb521064cd2d7b8d1dc67b6aebb258bdb2283f6`  
**Status:** proposed implementation programme; no runtime change or new native measurement is contained in this packet.

## Decision

Execute P0–P9 as one sustained, checkpointed programme. Finish Current's previously unimplemented target-neutral discovery profile. Then implement one selective, progress-preserving completion family in a shared native owner, and connect it to both Finder's search and Current's executable-upper service. Qualify same-target economics separately from the clean/coverage target contrast. Preserve the current default clean solver, existing exact controls and independent graph checking.

This is deliberately larger than an audit followed by one trial. Finding the already-listed proof dependencies is not completion of P1/P2. A negative first economic candidate does not cancel the remaining engineering or the other solver's work. A correctness failure can block its dependent stages; independent safe work proceeds. The detailed hard-stop conditions are in [execution protocol](EXECUTION_PROTOCOL.md).

No predicted speedup or crafting-cost reduction is promised. Success has distinct engineering, semantic-support, fixed-budget economic, proof, and product levels.

## Read in this order

1. [Executor prompt](CODEX_PROMPT.md), [main review](REVIEW.md), and [master plan](MASTER_PLAN.md).
2. For P1–P3, [Current profile design](CURRENT_PROFILE.md) and [mathematics](MATHEMATICS.md).
3. For P4–P6, [shared completion design](SHARED_COMPLETION.md), [Finder search](FINDER_SEARCH.md), and [Current candidate service](CURRENT_CANDIDATES.md).
4. [Experiments](EXPERIMENTS.md), [validation](VALIDATION.md), [execution protocol](EXECUTION_PROTOCOL.md), and [documentation maintenance](DOC_MAINTENANCE.md).
5. [Data request](DATA_REQUEST.md), [source index](SOURCES.md), and [remaining roadmap](NEXT_DECISIONS.md) as needed.

The source index and evidence files distinguish inspected source, recorded native evidence, conditional mathematics and proposals. Older user handoffs are motivation, not current implementation evidence.

## Deliverables that must exist at the end

- A real Current `SolveWork` diagnostic profile with no clean-target positive-proof leakage, native L/R operation, and bounded-only reporting; or a concrete reduced native counterexample to an actual unresolved semantic dependency, not the generic previous refusal.
- Preserved ordinary Current clean behavior and its separate exact-closure capability.
- A complete selective-retention candidate family using current native action/program contracts, not a hardcoded crafting recipe or a reset renamed as preservation.
- Finder genuinely generating and checking that family; a real Current caller using the same producer and checked-artifact contract, without running Finder secretly.
- A fixed-budget comparison which does not mix different targets, candidate budgets, action scopes or runtime transports.
- Source-matched native/WASM qualification, per-lane capability status, updated canonical mathematics, and a usable checkpoint handoff.

The exact public names proposed in this packet are provisional and explicitly NEW. Resolve them through the existing CLI/API owners before use. There is no authorized public default-goal migration, global solver rewrite, neural model, GPU port, concurrent portfolio or unrestricted incumbent-import feature.

## What the packet tests itself

`tests/test_contracts.py` contains exact-rational mathematical examples and specification checks. It does not import native game code or prove implementation correctness. Run it with ordinary Python; its machine-readable result is under `evidence/`. The manifest and `verify_packet.py` validate this package only.
