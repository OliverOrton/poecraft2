# Session Handoff

**Status: B1 implementation is in progress, but B1 acceptance is paused for
Oliver's two-T1 evidence re-pin decision. Do not proceed to B2.**

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

The B1 working tree is intentionally uncommitted while the gate is paused. It
contains ABI v2 bounded-result vocabulary, an atomic output incumbent,
bounded-policy compilation, exact native/compiled evaluation and occupancy
foundations, WASM/TypeScript propagation, targeted tests, stable contract
documentation, and rebuilt WASM artifacts. It does not contain B2 product
presentation, the B3 corpus, or any later chunk.

Targeted evidence completed on this working tree:

- native engine build passed;
- `poecraft_engine_tests.exe --solver-solve-only` passed 386 checks with zero
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
`7b11b34`, before the constructive-search commits. Oliver clarified that the
57,182 / 738,139 result is not a B1 regression. B1 acceptance is blocked only
on his decision whether to re-pin that structural evidence. Do not rerun the
slow case to reconfirm its timing.

The runtime is nevertheless a performance regression: constructive
renewal/progressive-fracture synthesis runs from scratch every focused round.
Oliver authorized retaining the synthesized fallback witness in the atomic
incumbent across rounds strictly for upper-bound/output use. Search guidance
from a stale incumbent remains forbidden. The refresh rule is: synthesize
when absent; reuse while goal, economy, action vocabulary, referenced
row/operator provenance, executable dependencies, and properness remain
valid; monotonic graph extension only revalidates row ownership and stamps the
new output bundle's graph identity. Refresh on dependency change or validation
failure, not on a mere focused round, graph extension, or lower-bound update.
Direct/partial executable upper policies can still replace it atomically when
cheaper.

All future acceptance runs must be detached with a hard 15-minute watchdog.
On expiry, kill the process tree, retain telemetry, and treat the deadline as
the result. Post-fix the exact two-T1 oracle is expected to take about five
minutes. Iterate only on fast gates, then run that deadlined oracle once at the
end.

Resume at the paused B1 gate:

1. obtain Oliver's evidence re-pin decision; do not accept B1 or begin B2
   before it;
2. implement retained constructive fallback-witness reuse locally on this
   branch without copying, merging, rebasing, or cherry-picking from
   `codex/exact-search-design`;
3. use fast targeted gates while iterating; and
4. at the end of B1, run the exact two-T1 oracle once under the detached
   15-minute watchdog and compare it to Oliver's selected structural pin.

Generated benchmark goals in later phases remain natural T1 modifiers only;
bench modifiers cannot satisfy goal slots, while legal priced bench actions
remain admitted for metamods, blocking, setup, and cleanup.

Follow the repository testing cadence: use narrow tests only when needed during
B1-B5, then run the complete affected native/WASM/web and 10,000-run
verification gates once at B6. Oliver owns rendered UI review.
