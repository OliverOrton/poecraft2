# Session Handoff

**Status: bounded executable-policy results are selected; B1 implementation
has not started.**

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

The current engine already computes focused lower/upper bounds and retains
partial executable upper-policy state internally. On a capped stop it restores
lower-mode state for finalization, and the Calculator compiles only when
`converged` is true. B1 must therefore change the policy/result handoff, not
the Bellman objective or mechanic model.

Resume at B1:

1. bump the public ABI for bounded summary/progress fields and independent
   absolute/relative gap targets;
2. create one atomic incumbent containing value, policy rows, Unveil
   preferences, frontier fallback, and quotient provenance;
3. stitch partial upper rows to the executable Restart/anchor fallback and
   compile that bounded policy;
4. stop after a complete focused round when either configured gap target is
   met, without changing exact numerical semantics; and
5. validate `L <= J_pi <= U` with native and compiled exact evaluation while
   retaining occupancy data for later action accounting.

No implementation code, ABI edit, WASM rebuild, web change, corpus generator,
or acceptance run has begun on this branch. Generated benchmark goals in later
phases are natural T1 modifiers only; bench modifiers cannot satisfy goal
slots, while legal priced bench actions remain admitted for metamods, blocking,
setup, and cleanup.

Follow the repository testing cadence: use narrow tests only when needed during
B1-B5, then run the complete affected native/WASM/web and 10,000-run
verification gates once at B6. Oliver owns rendered UI review.
