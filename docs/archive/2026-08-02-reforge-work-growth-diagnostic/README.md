# Exact Reforge-Work Growth Diagnostic

**Status: completed on 2026-08-02; eager exact candidate certification is an
approximately linear pre-partition grind.**

Parent: [Documentation archive](../README.md)

The frozen two-goal case was measured at its preserved 20M point, a fresh 50M
Run A, and a conditional 100M Run B. The coarse solve was exactly independent
of the configured cap. Exact work grew from 2 to 17 to 40 completed kernels,
with exactly 172,596 additional transitions per marginal kernel in both
segments, while quotient classes, partition rounds, certificates, and
published policies stayed zero.

Peak native-owned memory remained exactly `375,483,167` bytes. Both fresh
runs stopped solely on `max_reforge_work`, well inside the 1 GiB and
900-second boundaries, with zero reference-adapter calls. The 200M run was not
needed: 50M to 100M already established an approximately constant/slightly
rising per-row slope rather than falling cost.

- [Archived diagnostic plan](plan.md)
- [Full slope and projection report](report.md)
- [Tracked diagnostic evidence](../../../fixtures/solver-reliability/v1/evidence/reforge-work-growth-diagnostic.json)
- [Completed selected follow-on](../2026-08-02-competitive-lazy-alternative-certification/README.md)

No product cap, source, fixture, action filter, scheduling default, ABI, WASM,
strategy format, or frontend behavior changed. Nothing was pushed or merged,
and no visual review or full test pipeline was performed.
