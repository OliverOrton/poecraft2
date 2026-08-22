# Handoff

**Status: no implementation boundary is active.** Temporary blocker tier
canonicalization completed on 2026-08-22. Oliver must select the next chunk
before implementation resumes. The strongest measured successor is exact
carrier-local row equivalence across goal-slot-labelled temporary-blocker
choices; it remains unimplemented.

## Current checkpoint

- Branch: `main`
- Local `origin/main` is at release checkpoint `c95e6e1`; this run issued no
  push. All new checkpoints remain local.
- Implementation checkpoint: `a8deff0`
- Parent release checkpoint: `c95e6e1`
- Completed milestone plan:
  [docs/active/2026-08-22-temporary-blocker-tier-canonicalization/plan.md](docs/active/2026-08-22-temporary-blocker-tier-canonicalization/plan.md)
- Result:
  [docs/active/2026-08-22-temporary-blocker-tier-canonicalization/result.md](docs/active/2026-08-22-temporary-blocker-tier-canonicalization/result.md)
- Parent milestone:
  [docs/active/2026-08-21-persistent-quotient-session/plan.md](docs/active/2026-08-21-persistent-quotient-session/plan.md)
- Parent result:
  [docs/active/2026-08-21-persistent-quotient-session/result.md](docs/active/2026-08-21-persistent-quotient-session/result.md)
- Parent compact evidence:
  [docs/active/2026-08-21-persistent-quotient-session/evidence/native-primary-summary.json](docs/active/2026-08-21-persistent-quotient-session/evidence/native-primary-summary.json)

## What completed in this boundary

For one exact carrier-local temporary-blocker effect group, priced solves now
retain only the strictly cheapest blocker resource variant before planner
admission. Equal prices retain the first authoritative variant. Unpriced
callers still retain every differently resourced variant because they have no
economy with which to prove dominance.

The focused regression proves the selected tier changes when prices reverse,
the discarded tier does not reach a decision or operator, equal-price ties are
stable, and the unpriced path retains both variants. Native build, focused
solver test, and the WASM rebuild passed. The full acceptance pipeline and
primary were deliberately not run because this boundary did not change the
admitted Bellman rows or requalify a compiled strategy.

The stored four-T1 envelope contained 4,779 temporary-bench candidate
decisions; 2,442 were redundant tier variants previously collapsed only at the
late price-selection boundary. This change removes those variants before
planner admission. The same envelope still admits 521 temporary-bench Bellman
rows, so this is a planner-size cleanup rather than the row/proof reduction.

## Possible next owner: goal-slot row equivalence

The stored four-T1 envelope has 45 temporary-bench action identities for only
12 blocker/follow-up behaviours. Its 521 rows contain 248 unique transition
templates and 273 exact template repeats. At the retained root witness, 45
temporary rows fall into 12 state/effect groups; every row in each group has
identical lower value, upper value, and status, leaving 33 duplicates beyond
the first.

A safe next chunk should canonicalize only rows proven equivalent after
carrier mapping: identical transition kernel, immediate active-economy cost,
and stopping semantics. It must preserve any goal-slot distinction that can
change whether the strategy stops or continues. A stronger later quality
owner may deliberately stop on any relevant goal progress, but that is a
semantic policy change and should not be smuggled into equivalence cleanup.

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
