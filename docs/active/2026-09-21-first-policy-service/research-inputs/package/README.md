# Integrity first, then bounded first-policy service

**Read-only review and implementation proposal — 21 September 2026**  
**Repository:** OliverOrton/poecraft2  
**Reviewed main:** `a48f055ccc07751807c334386f1fd25c6b4717de`  
**Previous reviewed main:** `0f376f4249789e0bbe4680494d8c01609c84a765`

## Decision

Preserve the completed validation, retained-pool ownership, cooperative setup, full cancellation and fixed-graph compatibility work. **The completed Windows run changes the next priority:** three native suites fail, including an internal `L <= J_pi <= U` exception. Select **T0–T4** in [the milestone plan](02_MILESTONES.md), with [T0 integrity repair](08_INTEGRITY_GATE.md) mandatory before compiler performance work. Resolve actual correctness/capability failures and legitimate phase-test updates, then attribute and repair the remaining first-policy blocking span. Do not restart S0–S4 or turn this into a whole-solver cleanup campaign.

The current 362–373 ms ordinary step is a real failed gate, but “ladder scheduling → compilation” is not its causal diagnosis. That interval can contain policy materialization/copying, first-use runtime compilation, graph emission, graph parsing, evaluator construction and a batch of evaluation work. The plan separates these before selecting a treatment. The implementation is conditional; no native speedup, wrong answer, or root cause is asserted by this packet.

The next **capability** question after this bounded consolidation is the Conquest-four discovery/service gap, not another attempt to prove that its saved cheaper graph runs in WASM. That same-graph question was answered by S1. The deferred programme and advancement gates are in [07](07_RESEARCH_QUEUE_AND_DOCS.md).

## Reading route

The executor starts with [CODEX_PROMPT.md](CODEX_PROMPT.md), [02_MILESTONES.md](02_MILESTONES.md), and the mandatory [08_INTEGRITY_GATE.md](08_INTEGRITY_GATE.md). Use [01](01_MAIN_REVIEW.md) for changes and remaining risks; [03](03_ATTRIBUTION_AND_EXPERIMENTS.md) for the causal experiment; [04](04_SERVICE_AND_OWNERSHIP_DESIGN.md) for an implementation boundary; [05](05_MATHEMATICS_AND_PRIMARY_RESEARCH.md) for the arguments and counterexamples; [06](06_VALIDATION.md) for actual existing commands. Do not reread every historical archive merely because a link exists.

| File | Purpose |
|---|---|
| [01_MAIN_REVIEW.md](01_MAIN_REVIEW.md) | Current-main evidence, new source findings, and what not to repeat |
| [02_MILESTONES.md](02_MILESTONES.md) | Selected work, gates, stop rules and closeout |
| [03_ATTRIBUTION_AND_EXPERIMENTS.md](03_ATTRIBUTION_AND_EXPERIMENTS.md) | Cold/warm, stage, work, code-size and ledger attribution |
| [04_SERVICE_AND_OWNERSHIP_DESIGN.md](04_SERVICE_AND_OWNERSHIP_DESIGN.md) | Existing-owner design and conditional treatment branches |
| [05_MATHEMATICS_AND_PRIMARY_RESEARCH.md](05_MATHEMATICS_AND_PRIMARY_RESEARCH.md) | Prefix preservation, paired graphs, latency composition, memory accounting |
| [06_VALIDATION.md](06_VALIDATION.md) | Focused regression matrix and proportionate runtime qualification |
| [07_RESEARCH_QUEUE_AND_DOCS.md](07_RESEARCH_QUEUE_AND_DOCS.md) | Explicit next capability/proof questions and canonical documentation updates |
| [08_INTEGRITY_GATE.md](08_INTEGRITY_GATE.md) | Actual current CI failures, focused reproduction and authority-preserving repairs |
| [SOURCES.md](SOURCES.md) | Pinned repository sources and primary external research |
| [evidence/baseline.json](evidence/baseline.json) | Selected recorded measurements, not new runs |
| [evidence/ci_observations.json](evidence/ci_observations.json) | Current hosted observation, independently of the historical handoff |
| [checks/check_arguments.py](checks/check_arguments.py) | Executed abstract counterexamples and arithmetic; not native qualification |

## Evidence and scope

**Recorded** means an existing repository receipt. **Source-confirmed** means inspected at the reviewed pin. **Derived** means arithmetic or an argument with stated premises. **Hypothesis/proposed** means still to test. These labels are not interchangeable.

This packet contains plans and review evidence, not the frozen production dataset, native binaries, raw local `out/` reports or a modified engine. The executor must resolve their actual availability and identity. Search was used only for navigation; all relied-on implementation excerpts were explicitly fetched at the current pin. Repository access was read-only through GitHub. The review did not run native/WASM benchmarks or simulations. Offline checks and package verification are reported separately.

## Packet checks

The included abstract script ran **23 tests, all passing**. These are synthetic compiler/service/authority and timing-arithmetic checks, not native engine qualification. Run `python checks/verify_package.py` to verify inventory, hashes, structured files and local documentation links. No new native/WASM execution, strategy discovery or Simulator campaign was performed in this review. The review did read the completed hosted job; that existing run is not an execution performed by the reviewer.
