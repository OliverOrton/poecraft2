# Proof-carrying quotient scale fixture

This directory freezes the Gate 0 natural five-goal scale input for the
proof-carrying quotient refinement milestone. The generator is intentionally
restricted to one explicit Runic Gauntlets case so regeneration is
deterministic and cannot perturb the existing 49-case reliability portfolio.

The selected case is `natural-t1-scale-five-d432b26dfce2`: item level 86,
ordinary rare start, two prefix and three suffix natural T1 goals, the frozen
Mirage economy, and the existing goal-relevant product action envelope. Native
feasibility reports `natural_reforge_witness`, five eligible slots, pool count
139, pool weight 115,000, and no slot or group conflict.

Its qualification limits remain 1,073,741,824 solver-owned bytes and 900
seconds. Compilation and independent exact evaluation are required for a
published policy; verification uses 10,000 simulator runs. A resource refusal
leaves five-goal scale unqualified but does not by itself invalidate a
successful core quotient qualification.

Regenerate from the repository root with:

```powershell
$env:PYTHONPATH='tools/ingest;bindings/python'
py -3 tools/ingest/generate_natural_t1_corpus.py `
  fixtures/solver-reliability/v1/qualification/proof-carrying-quotient/generator-config.json
```

Gate 0 provenance and the fixture hash are recorded in
[`proof-carrying-quotient-gate0.json`](../../evidence/proof-carrying-quotient-gate0.json).
