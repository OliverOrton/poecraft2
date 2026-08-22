# Handoff

**Status: conditional release qualification is active.** Oliver reviewed the
persistent quotient session's 419,316,840-byte conservative native peak on
2026-08-22 and replaced the original 150 MB milestone ceiling with 512 MiB
(536,870,912 bytes). Gate 6 therefore passes. This does not change an engine
resource cap or proof behavior.

## Current checkpoint

- Branch: `main`
- Upstream remains at `65d4d76`; nothing was pushed.
- Retained implementation checkpoint: `bb29378`
- Milestone plan:
  [docs/active/2026-08-21-persistent-quotient-session/plan.md](docs/active/2026-08-21-persistent-quotient-session/plan.md)
- Result:
  [docs/active/2026-08-21-persistent-quotient-session/result.md](docs/active/2026-08-21-persistent-quotient-session/result.md)
- Compact evidence:
  [docs/active/2026-08-21-persistent-quotient-session/evidence/native-primary-summary.json](docs/active/2026-08-21-persistent-quotient-session/evidence/native-primary-summary.json)

## What is retained

Production strict refinement now owns one durable quotient session across
competitive frontier growth. The strict oracle, selected closure, split-only
partition, Bellman graph, proof store, alternative obligations, published
rows, and independently evaluated incumbent survive in-place carrier
insertions. Source/target splits invalidate dependent proof authority through
stable generations and reverse indexes; unchanged cells and payloads remain
live. Alternative proof and final-policy verification remain cooperative.

The retained code spans the local checkpoints `18e4640` through `bb29378`.
Do not reset, squash away, or replace this architecture with the rejected
cross-generation carrier-row cache. A narrower within-obligation absolute-row
cache was also measured and removed after only nine hits.

## Gate 6 result

The final native Allflame four-natural-T1 Conquest Lamellar run reached the
300-second watchdog with:

- upper `3745.7295960574743`, lower `0`, and no upper increase;
- first verified strict upper at 36,462.28 ms;
- one strict-session construction and zero production full restarts;
- four in-place frontier insertions containing 138 new states;
- 17,334 cells retained, 126 cells superseded, 126 source splits, 126 target
  splits, 6,576 reverse invalidations, and 28,802 row reprojections;
- 6,963 open alternatives versus the 17,584 parent baseline, a 60.40%
  reduction;
- largest public step 1,194.53 ms; and
- 419,316,840 native peak owned bytes, with 363,854,278 attributed to the
  live quotient/oracle/proof session.

Every native acceptance condition passed under the owner-adjusted 512 MiB
ceiling. The primary produced no final strategy, so it was not simulated.

## Qualification already run

- `powershell -File scripts/build.ps1`: pass.
- Quotient partition/Bellman/proof: 614 checks, 0 failures.
- Core refinement: 362 checks, 0 failures.
- Policy refinement: 1,234 checks, 0 failures.
- Solver suite: 96,439 checks, 0 failures.
- Automatic Eldritch product control: converged at
  `0.018630169563331064`, 10,000/10,000 successful simulations.
- Warlord product control: converged at `224.1238588972487`,
  10,000/10,000 successful simulations.
- Final primary derived report:
  `build/solver-diagnostics/persistent-quotient-session/native-primary-final-128.json`.

Release WASM, TypeScript/web acceptance, the release-WASM five-minute primary,
and, if those pass, the one full `scripts/test.ps1` acceptance run are now the
active conditional release boundary.

## Later memory owner

If Oliver selects a successor, begin with behavior-neutral ownership
telemetry that separates:

1. live strict `CalcContext` states, transition/option/reforge caches, and
   operator/session payload;
2. adapter `known_`, exact-policy, recipe, and carrier maps;
3. persistent selected raw closure and published row metadata;
4. Bellman sparse rows and projected transitions; and
5. proof payload, coverage, dependency, and alternative-obligation storage.

The named proof categories total about 134 MB; the unsplit oracle/adapter
remainder is about 230 MB. Establish its actual subowners, simultaneous
ownership, and aliasing before choosing compaction or lifetime work. Candidate directions are
safe cache release between carrier subproofs, compact/shared selected raw-row
payloads, and removal of duplicated strict-versus-quotient transition state.
Do not raise the memory cap, weaken carrier coverage, trim actions, infer
identity from hashes, or add another unbounded replay cache.

Five-T1 recovery, Imprint state 928, compiler/router work, mechanics, prices,
and action admission remain separate and unchanged.
