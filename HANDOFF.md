# Session Handoff

**Status: blocked at the B1 acceptance gate. No implementation chunk has
passed; do not proceed to B2. The final permitted exact two-T1 oracle attempt
hit the binding 15-minute watchdog at 55,850 / 57,182 expanded states.**

Oliver selected the
[bounded policy results and benchmarking plan](docs/active/bounded-policy-and-benchmarking.md)
on 2026-07-22. Work starts from clean `main` commit `60500ef` on
`codex/bounded-policy-contract`.

The preceding exact constructive-policy milestone is preserved in its
[dated archive](docs/archive/2026-07-22-exact-constructive-policy-search/README.md).
It established generic destructive-renewal and progressive-fracture
incumbents for an empty rare Dire Pelt requiring three naturally rolled T1
modifiers, proving

`261.05161071365512 <= V*(start) <= 4104.7066630770487`

under all 23 priced actions and unchanged production caps. This is an
executable feasibility bracket, not a near-optimal result: `U / L` is about
`15.7237`. Exact closure is no longer the selected next boundary.

The exact-search architecture review and Candidate A/C source prototypes are
preserved only as local review evidence on `codex/exact-search-design` at
`273831f`. They are not ancestors of, merged into, or adopted by the current
branch.

The B1 working tree is intentionally uncommitted while the gate is blocked. It
contains ABI v2 bounded-result vocabulary, an atomic output incumbent,
bounded-policy compilation, exact native/compiled evaluation and occupancy
foundations, WASM/TypeScript propagation, targeted tests, stable contract
documentation, and rebuilt WASM artifacts. It does not contain B2 product
presentation, the B3 corpus, or any later chunk.

Targeted evidence completed on this working tree:

- native engine build passed;
- `poecraft_engine_tests.exe --solver-solve-only` passed 391 checks with zero
  failures;
- the earlier ABI/API narrow run passed 45,717 checks with zero failures;
- `npx tsc --noEmit` passed; and
- `scripts/build-wasm.ps1` completed after the ABI change.

Two exact two-T1 reports under `build/diagnostics/` agree on the true
`60500ef`-lineage result:

- exact value `230.26738656962243`;
- 57,182 expanded states, 738,139 state-action rows, and 1,165,840
  transitions;
- transition hash `ad4fc4865f2872e9` and policy hash `f797e61b00a127a7`;
- compiled graph 6,391 nodes / 9,607 edges; and
- total wall times 4,306,314.89 ms and 4,558,421.93 ms.

The repository pin of 57,233 expanded states / 903,935 rows was generated at
`7b11b34`, before the constructive-search commits. Oliver approved re-pinning
active evidence to the 57,182 / 738,139 lineage result. Do not rerun the slow
case to reconfirm its timing.

The runtime was nevertheless a performance regression: constructive
renewal/progressive-fracture synthesis runs from scratch every focused round.
Oliver authorized retaining the synthesized fallback witness in the atomic
incumbent across rounds strictly for upper-bound/output use. Search guidance
from a stale incumbent remains forbidden. The implemented refresh rule is:
synthesize when absent; reuse while goal, economy, the complete action prefix
present at synthesis, referenced rows/operators, executable dependencies, and
properness remain valid. Monotonic graph and lazy vocabulary extension do not
invalidate the witness. Refresh on an existing dependency change or
validation failure, not on a mere focused round, graph/vocabulary extension,
or lower-bound update. Direct/partial executable upper policies can still
replace it atomically when cheaper.

That retention path is now implemented in the B1 working tree. The fast
progressive-fracture regression crosses multiple focused rounds, observes
synthesis followed by reuse, observes zero refreshes, and the solver-only
suite passes 391 checks with zero failures.

The first deadlined oracle attempt hit the 900-second watchdog at 34,273
expanded states and was killed with no survivor. A 2,000-state real-product
diagnostic then isolated whole-vocabulary identity invalidation: lazy operator
appends caused 13 syntheses / 7 reuses / 10 refreshes and 100.6 seconds. The
fixed rule retains one immutable shared witness while its complete synthesis
prefix and referenced rows/operators remain stable; monotonic graph and
vocabulary extension do not invalidate it. The same diagnostic now records 3
initial syntheses / 17 reuses / 0 refreshes and finishes in 27.1 seconds. WASM
was rebuilt after this native change.

The next deadlined oracle attempt still expired at 51,533 expanded states.
Diagnosis isolated another B1-only repeated cost: every improving atomic
incumbent eagerly derived policy and Unveil-preference vectors across about
116,000 states. Those deterministic fields are now materialized once from the
captured same-round values/rows/frontier/fallback only when a bounded incumbent
is returned. The 2,000-state real-product gate remains 3 syntheses / 17 reuses
/ 0 refreshes, while focused-round time falls from 7.3 seconds to 1.6 seconds
and total time from 27.1 seconds to 22.3 seconds. Its bounded policy still
materializes during finalization; the solver-only suite remains 391/0, and
WASM was rebuilt before the final oracle attempt.

That final attempt also hit the 900-second watchdog. It reached 55,850 of the
approved 57,182 expanded states, then was killed with exit 124; no benchmark
process survived. This is the exact stopping point. The performance fix is
substantial but insufficient for the binding deadline, so B1 acceptance did
not pass and this run must stop without further retries or B2 work.

All future acceptance runs must be detached with a hard 15-minute watchdog.
On expiry, kill the process tree, retain telemetry, and treat the deadline as
the result. Post-fix the exact two-T1 oracle is expected to take about five
minutes. Iterate only on fast gates, then run that deadlined oracle once at the
end.

Resume only after Oliver directs a new response to the blocked B1 gate:

1. do not change the 15-minute deadline or reinterpret the failed gate;
2. use the retained timeout logs and fast 2,000-state reports instead of
   rerunning known-slow configurations;
3. if Oliver authorizes more performance work, diagnose the remaining late
   closure cost with fast bounded evidence before one newly authorized
   watchdog run; and
4. do not begin B2 until B1 passes in full.

Generated benchmark goals in later phases remain natural T1 modifiers only;
bench modifiers cannot satisfy goal slots, while legal priced bench actions
remain admitted for metamods, blocking, setup, and cleanup.

Follow the repository testing cadence: use narrow tests only when needed during
B1-B5, then run the complete affected native/WASM/web and 10,000-run
verification gates once at B6. Oliver owns rendered UI review.

Chunks completed: none. B1 implementation is present only as an uncommitted,
narrow-test-green working checkpoint; its acceptance gate is blocked by the
watchdog. Still awaiting Oliver later in the plan are rendered UI review,
corpus-strata sanity review, and acceptance of the eventual B6 results.
