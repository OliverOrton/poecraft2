# Handoff

**Status: temporary blocker tier canonicalization is active.** Oliver selected
the narrow successor on 2026-08-22: for one exact carrier-local blocker effect,
choose the cheapest legal tier under the active economy before fixed-option,
operator, Bellman-row, or proof construction. Preserve all differently
resourced variants for unpriced callers. The broader goal-slot-equivalence
finding is a separate successor and is not part of this boundary.

## Current checkpoint

- Branch: `main`
- Local `origin/main` is at release checkpoint `c95e6e1`; this run issued no
  push. The documentation finalization commits remain local.
- Retained implementation checkpoint: `bb29378`
- Release checkpoint: `c95e6e1`
- Active milestone plan:
  [docs/active/2026-08-22-temporary-blocker-tier-canonicalization/plan.md](docs/active/2026-08-22-temporary-blocker-tier-canonicalization/plan.md)
- Completed parent milestone:
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

## Release qualification

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
- Release WASM primary: one session, zero restarts, four insertions / 138
  states, unchanged upper `3745.7295960574743`, unchanged 6,963 open
  alternatives, and no unsupported lower or upper increase.
- Release primary first strict upper: 63,208.70 ms; largest worker slice:
  2,049.82 ms in the pre-refinement coarse focused-expansion round.
- Release primary live owned at stop: 366,916,901 bytes; solver-owned quotient
  telemetry: 315,716,514 bytes; WASM heap growth: 500,498,432 bytes.
- Release WASM build, `npx tsc --noEmit`, complete web tests, and 28/28 engine
  smoke checks: pass.
- `powershell -File scripts/test.ps1`: pass, including 3,464,468 native engine
  checks with zero failures.

The release primary reached the five-minute watchdog and produced no final
strategy, so it was not simulated. Its full local report is
`build/solver-diagnostics/persistent-quotient-session/wasm-primary-final.json`.

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
ownership, and aliasing before choosing compaction or lifetime work. Candidate
directions are safe cache release between carrier subproofs, compact/shared
selected raw-row payloads, and removal of duplicated strict-versus-quotient
transition state.
Do not make another cap-only adjustment, weaken carrier coverage, trim
actions, infer identity from hashes, or add another unbounded replay cache
without a newly selected and evidenced boundary.

Five-T1 recovery, Imprint state 928, compiler/router work, mechanics, prices,
and action admission remain separate and unchanged.
